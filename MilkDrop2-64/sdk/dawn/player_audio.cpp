// SPDX-License-Identifier: GPL-2.0-or-later
#include "player_audio.hpp"
#include "shared_audio.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/event.h>
#else
#include <poll.h>
#include <sys/syscall.h>
#endif
#endif

namespace visualizers {
uint64_t playerMilliseconds()noexcept{
#ifdef _WIN32
    return GetTickCount64();
#else
    return suite::milliseconds();
#endif
}
namespace {
constexpr uint64_t playerTimeoutMs=250;
bool fresh(const suite::AudioRing& ring,uint64_t now)noexcept{
    return ring.magic==suite::audioMagic&&ring.version==1&&ring.channels==2&&ring.rate>=8000&&ring.rate<=768000&&
        now>=ring.updatedMs&&now-ring.updatedMs<=playerTimeoutMs;
}
void readWindowEnding(const suite::AudioRing& ring,std::span<float,4096> output,uint64_t end)noexcept{
    auto clean=[](float value){return std::isfinite(value)?std::clamp(value,-32.f,32.f):0.f;};
    const uint64_t historyAge=ring.written-end;
    std::fill(output.begin(),output.end(),0.f);
    for(unsigned n=0;n<2048;++n){const double age=(2047-n)*ring.rate/44100.;const auto whole=historyAge+uint64_t(age);const float fraction=float(age-uint64_t(age));
        if(ring.written>whole&&whole<suite::ringFrames){const auto position=ring.written-1-whole;
            for(unsigned channel=0;channel<2;++channel){const auto recent=clean(ring.samples[position%suite::ringFrames][channel]);
                // The newest exact sample needs no earlier neighbor. During
                // startup, pad unavailable history with silence.
                const auto previous=position&&whole+1<suite::ringFrames?clean(ring.samples[(position-1)%suite::ringFrames][channel]):0.f;
                output[n*2+channel]=recent*(1-fraction)+previous*fraction;
            }
        }
    }
}
}
bool readPlayerWindow(const suite::AudioRing& ring,std::span<float,4096> output,uint64_t now)noexcept{
    if(!fresh(ring,now)){std::fill(output.begin(),output.end(),0.f);return false;}
    readWindowEnding(ring,output,ring.written);
    return true;
}
BeatState PlayerBeatHistory::idle(uint64_t now)const{
    return beats_.state(now>deliveredMs_?double(now-deliveredMs_)*.001:0.);
}
BeatState PlayerBeatHistory::advance(const suite::AudioRing& ring,uint64_t now){
    if(!fresh(ring,now)||!ring.written)return idle(now);
    const auto hop=std::max<uint64_t>(1,uint64_t(ring.rate)*512/44100);
    if(ring.rate!=rate_||ring.written<observedEnd_){
        // Keep the count monotonic when the writer restarts or the format
        // changes. Start with the current window, without replaying old beats.
        rate_=ring.rate;
        analyzedEnd_=ring.written>=hop?ring.written-hop:0;
        beats_.restart(audioTime_);
    }else if(ring.written!=observedEnd_&&ring.updatedMs>deliveredMs_&&ring.updatedMs-deliveredMs_>playerTimeoutMs){
        // A paused or stalled source must not bring back its last beat pulse.
        beats_.restart(audioTime_);
    }
    observedEnd_=ring.written;
    if(ring.written>suite::ringFrames){
        const auto required=std::min<uint64_t>(suite::ringFrames,
            (uint64_t(rate_)*2048+44099)/44100);
        const auto firstUsable=ring.written-suite::ringFrames+required;
        const auto skipTo=firstUsable>=hop?firstUsable-hop:0;
        if(analyzedEnd_<skipTo){
            audioTime_+=double(skipTo-analyzedEnd_)/rate_;
            analyzedEnd_=skipTo;
        }
    }
    auto value=idle(now);
    const uint64_t before=value.count;
    std::array<float,4096> samples;
    while(ring.written-analyzedEnd_>=hop){
        analyzedEnd_+=hop;
        audioTime_+=double(hop)/rate_;
        readWindowEnding(ring,samples,analyzedEnd_);
        value=beats_.update(estimateBass(samples),audioTime_);
        deliveredMs_=ring.updatedMs;
    }
    value.detected=value.count!=before;
    return value;
}
struct PlayerAudio::Impl {
    const uint32_t pid;
    std::mutex mutex;
    std::array<float,4096> latest{};
    uint64_t updatedMs=0, publishedMs=0, returnedBeats=0;
    BeatState latestBeat;
    std::exception_ptr failure;
    std::atomic_bool stopped=false;
    std::thread reader;

    explicit Impl(uint32_t value):pid(value){
        if(!pid||pid>uint32_t(std::numeric_limits<int32_t>::max()))throw std::runtime_error("Invalid player process ID");
        reader=std::thread([this]{poll();});
    }
    ~Impl(){stopped.store(true,std::memory_order_relaxed);reader.join();}

    void poll(){
        try {
            std::unique_ptr<suite::SharedAudio> source;
            suite::AudioRing snapshot{};
            PlayerBeatHistory history;
            uint64_t nextOpen=0;
            while(!stopped.load(std::memory_order_relaxed)){
                const auto now=playerMilliseconds();
                if((!source||!source->ring)&&now>=nextOpen){
                    source=std::make_unique<suite::SharedAudio>(pid,false);
                    nextOpen=now+playerTimeoutMs;
                }
                if(source&&source->ring&&source->lock()){
                    const auto& ring=*source->ring;
                    const bool changed=ring.written!=snapshot.written||ring.updatedMs!=snapshot.updatedMs||
                        ring.rate!=snapshot.rate||ring.channels!=snapshot.channels||ring.version!=snapshot.version||ring.magic!=snapshot.magic;
                    if(changed)std::memcpy(&snapshot,&ring,sizeof(snapshot));
                    source->unlock();
                    if(changed){
                        // Shader compilation can hold up presentation longer than the shared ring retains audio.
                        // Consume the bridge independently and only publish after releasing VLC's lock.
                        const auto copiedAt=playerMilliseconds();
                        const auto beat=history.advance(snapshot,copiedAt);
                        std::array<float,4096> pcm;
                        const bool valid=readPlayerWindow(snapshot,pcm,copiedAt);
                        std::lock_guard lock(mutex);
                        latest=pcm;
                        latestBeat=beat;
                        updatedMs=valid?snapshot.updatedMs:0;
                        publishedMs=copiedAt;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(12));
            }
        } catch(...){
            std::lock_guard lock(mutex);
            failure=std::current_exception();
        }
    }
};
PlayerAudio::PlayerAudio(uint32_t pid):impl_(std::make_unique<Impl>(pid)){}
PlayerAudio::~PlayerAudio()=default;
bool PlayerAudio::window(std::span<float,4096> output,BeatState* beat){
    auto& state=*impl_;
    std::lock_guard lock(state.mutex);
    if(state.failure)std::rethrow_exception(state.failure);
    const auto now=playerMilliseconds();
    const bool valid=state.updatedMs&&now>=state.updatedMs&&now-state.updatedMs<=playerTimeoutMs;
    if(valid)std::copy(state.latest.begin(),state.latest.end(),output.begin());
    else std::fill(output.begin(),output.end(),0.f);
    if(beat){
        const auto age=now>=state.publishedMs?now-state.publishedMs:0;
        *beat={state.latestBeat.count,state.latestBeat.envelope*std::exp(-double(age)*.004),
            valid&&state.latestBeat.count!=state.returnedBeats};
        state.returnedBeats=state.latestBeat.count;
    }
    return valid;
}
struct PlayerLifetime::Impl {
#ifdef _WIN32
    HANDLE handle=nullptr;
    ~Impl(){if(handle)CloseHandle(handle);}
#else
    int handle=-1;
    ~Impl(){if(handle>=0)close(handle);}
#endif
    bool ended=false;
};
PlayerLifetime::PlayerLifetime(uint32_t pid):impl_(std::make_unique<Impl>()){
    if(!pid||pid>uint32_t(std::numeric_limits<int32_t>::max()))throw std::runtime_error("Invalid player process ID");
#ifdef _WIN32
    impl_->handle=OpenProcess(SYNCHRONIZE,FALSE,pid);
    if(!impl_->handle){if(GetLastError()==ERROR_INVALID_PARAMETER){impl_->ended=true;return;}throw std::runtime_error("Cannot watch player process exit");}
#elif defined(__APPLE__)
    impl_->handle=kqueue();if(impl_->handle<0)throw std::runtime_error("Cannot create player process watch");
    if(fcntl(impl_->handle,F_SETFD,FD_CLOEXEC)<0)throw std::runtime_error("Cannot protect player process watch");
    struct kevent change;EV_SET(&change,pid,EVFILT_PROC,EV_ADD|EV_ENABLE|EV_CLEAR,NOTE_EXIT,0,nullptr);
    if(kevent(impl_->handle,&change,1,nullptr,0,nullptr)<0){if(errno==ESRCH){impl_->ended=true;return;}throw std::runtime_error("Cannot watch player process exit");}
#else
    impl_->handle=int(syscall(SYS_pidfd_open,pid,0));
    if(impl_->handle<0){if(errno==ESRCH){impl_->ended=true;return;}throw std::runtime_error("Cannot watch player process exit (pidfd is required)");}
#endif
}
PlayerLifetime::~PlayerLifetime()=default;
bool PlayerLifetime::exited(){
    auto& state=*impl_;if(state.ended)return true;
#ifdef _WIN32
    const auto result=WaitForSingleObject(state.handle,0);if(result==WAIT_FAILED)throw std::runtime_error("Cannot poll player process exit");state.ended=result==WAIT_OBJECT_0;
#elif defined(__APPLE__)
    struct kevent event{};const timespec immediate{};const auto count=kevent(state.handle,nullptr,0,&event,1,&immediate);
    if(count<0){if(errno==EINTR)return false;throw std::runtime_error("Cannot poll player process exit");}
    if(count&&event.flags&EV_ERROR)throw std::runtime_error("Player process watch reported an error");state.ended=count&&(event.fflags&NOTE_EXIT);
#else
    pollfd event{state.handle,POLLIN,0};const auto count=poll(&event,1,0);
    if(count<0){if(errno==EINTR)return false;throw std::runtime_error("Cannot poll player process exit");}
    if(event.revents&POLLNVAL)throw std::runtime_error("Player process watch is invalid");state.ended=count&&(event.revents&(POLLIN|POLLHUP|POLLERR));
#endif
    return state.ended;
}
}
