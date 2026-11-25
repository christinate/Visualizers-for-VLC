// SPDX-License-Identifier: GPL-2.0-or-later
#include "player_audio.hpp"
#include "shared_audio.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
#include <thread>

using namespace visualizers;namespace fs=std::filesystem;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static uint32_t ownPid(){
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return uint32_t(getpid());
#endif
}

static void checkBeatHistory(suite::AudioRing& ring){
    ring={};
    constexpr unsigned rate=44100, callbackFrames=4096, count=16;
    constexpr uint64_t startMs=1000;
    std::array<float,callbackFrames*2> chunk{};
    std::array<float,4096> latest{};
    PlayerBeatHistory history;
    MusicAnalysis displayAnalysis;
    BeatState beat;
    uint64_t written=0;
    unsigned pulses=0;
    for(unsigned frame=0;frame<=14*19;++frame){
        const double seconds=double(frame)/19;
        while(written+callbackFrames<=seconds*rate){
            for(unsigned n=0;n<callbackFrames;++n){
                const double t=double(written+n)/rate;
                const double position=t-4;
                const int onset=position<0?-1:int(position/.5);
                const double phase=position-onset*.5;
                double sample=.038*std::sin(2*std::numbers::pi*659.25*t)+.027*std::sin(2*std::numbers::pi*987.77*t);
                if(onset>=0&&onset<int(count)&&phase<.115){
                    const double kick=.76*(onset%4==0?1:.8)*std::sin(2*std::numbers::pi*94*phase);
                    sample+=kick*std::min(1.,phase*240)*std::exp(-26*phase);
                }
                chunk[n*2]=chunk[n*2+1]=float(std::round(sample*32767)/32768);
            }
            written+=callbackFrames;
            suite::write(ring,chunk.data(),callbackFrames,2,rate,startMs+written*1000/rate);
        }
        const auto now=startMs+frame*1000/19;
        readPlayerWindow(ring,latest,now);
        const auto& display=displayAnalysis.update(latest,seconds);
        require(std::abs(estimateBass(latest)-display.bass)<1e-7,"Beat-only analysis disagreed with the displayed spectrum");
        beat=history.advance(ring,now);
        if(beat.detected)++pulses;
        const auto reread=history.advance(ring,now);
        require(!reread.detected&&reread.count==beat.count,"Repeated player window detected the same beat twice");
    }
    require(beat.count==count&&pulses==count,"Burst-delivered PCM lost a scheduled beat");
    const auto idle=history.idle(startMs+15000);
    require(!idle.detected&&idle.count==count&&idle.envelope<beat.envelope,"Idle player retained its beat pulse");
    chunk.fill(0);
    suite::write(ring,chunk.data(),callbackFrames,2,rate,startMs+15000);
    beat=history.advance(ring,startMs+15000);
    require(!beat.detected&&beat.count==count&&beat.envelope==0,"Resuming the player revived its last beat pulse");
    ring={};
    suite::write(ring,chunk.data(),callbackFrames,2,48000,startMs+15000);
    beat=history.advance(ring,startMs+15000);
    require(!beat.detected&&beat.count==count&&beat.envelope==0,"New player format replayed old beats");
    for(unsigned block=0;block<10;++block){
        suite::write(ring,chunk.data(),callbackFrames,2,48000,startMs+15800+block*90);
    }
    beat=history.advance(ring,startMs+16610);
    require(!beat.detected&&beat.count==count,"Ring overrun analyzed overwritten player samples");
}

static void checkIndependentReader(PlayerAudio& reader,suite::SharedAudio& writer){
    constexpr unsigned callbackFrames=2205, rate=44100;
    std::array<float,callbackFrames*2> pcm;
    // The producer advances past a full ring while the window reader stays idle.
    // The two early pulses must still reach the next rendered frame.
    for(unsigned packet=0;packet<24;++packet){
        for(unsigned n=0;n<callbackFrames;++n){
            const double seconds=double(packet*callbackFrames+n)/rate;
            const double phase=seconds-(seconds<.45?.10:.50);
            const double value=phase>=0&&phase<.105?.62*std::sin(2*std::numbers::pi*94*phase)*std::exp(-22*phase):0;
            pcm[n*2]=pcm[n*2+1]=float(value);
        }
        unsigned attempts=0;
        while(!writer.lock()){
            require(++attempts<30,"Generated beat writer could not acquire the IPC lock");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        suite::write(*writer.ring,pcm.data(),callbackFrames,2,rate,playerMilliseconds());
        writer.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(22));
    }
    std::array<float,4096> window;
    BeatState beat;
    require(reader.window(window,&beat)&&beat.detected&&beat.count==2,"Paused rendering lost retained beat events");
    require(reader.window(window,&beat)&&!beat.detected&&beat.count==2,"Repeated rendering replayed pending beat events");
}

int main(int argc,char** argv){try{
    if(argc==4&&(std::string(argv[1])=="--writer"||std::string(argv[1])=="--hold")){
        const bool write=std::string(argv[1])=="--writer";const fs::path ready=argv[2],stop=argv[3];
        std::unique_ptr<suite::SharedAudio> shared;if(write){shared=std::make_unique<suite::SharedAudio>(ownPid(),true);require(shared->ring,"Cannot create generated-PCM fixture mapping");}
        fs::create_directories(ready.parent_path());std::ofstream(ready)<<nlohmann::json{{"pid",ownPid()},{"generated_pcm",write}}.dump();
        uint64_t frames=0;const auto started=playerMilliseconds();std::array<float,2048> pcm{};
        while(!fs::exists(stop)&&playerMilliseconds()-started<60000){
            if(shared&&shared->lock()){
                for(unsigned n=0;n<1024;++n){const double t=double(frames+n)/48000.;pcm[n*2]=float(.375*std::sin(t*6.283185307179586*220));pcm[n*2+1]=float(.625*std::sin(t*6.283185307179586*330));}
                suite::write(*shared->ring,pcm.data(),1024,2,48000,playerMilliseconds());shared->unlock();frames+=1024;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return 0;
    }
    require(argc==1,"Expected no arguments, or --writer/--hold ready.json stop-file");
    auto ring=std::make_unique<suite::AudioRing>();ring->magic=suite::audioMagic;ring->version=1;ring->channels=2;ring->rate=44100;ring->updatedMs=1000;ring->written=suite::ringFrames*2+3;
    std::array<float,4096> output{};
    for(unsigned n=0;n<suite::ringFrames;++n){ring->samples[n][0]=float(n%127)/127;ring->samples[n][1]=-ring->samples[n][0];}
    require(readPlayerWindow(*ring,output,1000),"Valid player ring was rejected");
    for(unsigned n=0;n<2048;++n){const auto position=(ring->written-2048+n)%suite::ringFrames;require(output[n*2]==ring->samples[position][0]&&output[n*2+1]==ring->samples[position][1],"Float IPC read quantized, wrapped or reordered PCM");}
    ring->rate=48000;require(readPlayerWindow(*ring,output,1000),"48 kHz player audio was rejected");
    for(unsigned n=0;n<2048;++n){const double age=(2047-n)*48000./44100.;const auto whole=uint64_t(age),position=ring->written-1-whole;const float fraction=float(age-whole);
        const float expected=ring->samples[position%suite::ringFrames][0]*(1-fraction)+ring->samples[(position-1)%suite::ringFrames][0]*fraction;
        require(output[n*2]==expected&&output[n*2+1]==-expected,"Player sample-rate conversion differed");}
    ring->rate=44100;const auto last=(ring->written-1)%suite::ringFrames;ring->samples[last][0]=std::numeric_limits<float>::quiet_NaN();ring->samples[last][1]=std::numeric_limits<float>::infinity();readPlayerWindow(*ring,output,1000);require(output[4094]==0&&output[4095]==0,"Player IPC retained nonfinite PCM");
    ring->samples[last][0]=2.125f;ring->samples[last][1]=-99;readPlayerWindow(*ring,output,1000);require(output[4094]==2.125f&&output[4095]==-32,"Player float PCM lost precision/headroom or exceeded its safety bound");
    auto silence=[&](uint64_t now){require(!readPlayerWindow(*ring,output,now),"Invalid/stale player ring was accepted");for(float value:output)require(value==0,"Rejected player audio retained stale samples");};
    silence(999);silence(1251);ring->rate=0;silence(1000);ring->rate=std::numeric_limits<uint32_t>::max();silence(1000);ring->rate=44100;ring->channels=1;silence(1000);ring->channels=2;ring->version=999;silence(1000);ring->version=1;
    ring->written=1;ring->samples[0][0]=.1234567f;ring->samples[0][1]=-.1234567f;readPlayerWindow(*ring,output,1000);require(output[4094]==.1234567f&&output[4095]==-.1234567f&&output[4092]==0,"Short player history padding is wrong");
    checkBeatHistory(*ring);
    PlayerLifetime lifetime(ownPid());require(!lifetime.exited(),"Current process was reported exited");
    PlayerAudio reader(ownPid());require(!reader.window(output),"Missing player bridge was reported active");
    // Give the background reader a chance to see the initially absent bridge.
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    suite::SharedAudio writer(ownPid(),true);require(writer.ring&&writer.lock(),"Cannot create local player IPC fixture");
    *writer.ring={};const std::array<float,4> source={.1234567f,-.1234567f,.3333333f,-.3333333f};suite::write(*writer.ring,source.data(),2,2,44100,playerMilliseconds());writer.unlock();
    bool connected=false;
    for(unsigned n=0;n<60&&!connected;++n){
        connected=reader.window(output);
        if(!connected){
            // A real player keeps publishing while the reader retries. A
            // one-shot test packet can expire before the next open attempt.
            if(writer.lock()){
                suite::write(*writer.ring,source.data(),2,2,44100,playerMilliseconds());
                writer.unlock();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    require(connected&&output[4094]==source[2],"Late player bridge was not discovered");
    checkIndependentReader(reader,writer);
    std::cout<<"Player PCM: float/wrap/resampling, nonfinite/stale/invalid/short history, successive beats, independent polling, late IPC and process watch pass\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
