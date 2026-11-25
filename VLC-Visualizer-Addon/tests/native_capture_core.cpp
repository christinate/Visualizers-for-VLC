// SPDX-License-Identifier: GPL-2.0-or-later
#include "audio_capture.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    try{
        using visualizers::resolveAudioInput;
        const std::vector<visualizers::InputDevice> devices={{"stable:a","Same label",true},{"stable:b","Same label",false},{"stable:c","Entrée 音声",false}};
        require(resolveAudioInput(devices,"DeFaUlT")=="default","Default recording alias failed");
        require(resolveAudioInput(devices,"stable:b")=="stable:b","Duplicate names changed an explicit device ID");
        require(resolveAudioInput(devices,"Entrée 音声")=="stable:c","Exact Unicode input label failed");
        for(const auto& token:std::vector<std::string>{"Same label","missing", "",std::string("bad\0token",9)}){
            bool rejected=false;try{resolveAudioInput(devices,token);}catch(const std::runtime_error&){rejected=true;}
            require(rejected,"Ambiguous/missing/invalid input was accepted");
        }
        auto fifo=std::make_unique<visualizers::AudioFifo>();std::array<float,4096> samples{},window{};
        samples.fill(.1234567f);fifo->push(samples.data(),2048,1000);require(fifo->window(window,1000)==2048,"Initial FIFO frame count");
        require(window==samples,"Float PCM was quantized or reordered");
        fifo->window(window,1300);require(std::all_of(window.begin(),window.end(),[](float x){return x==0;}),"Stale capture did not become silence");
        fifo->reset();samples[0]=std::numeric_limits<float>::quiet_NaN();samples[1]=std::numeric_limits<float>::infinity();samples[2]=2;samples[3]=-2;
        samples[4]=std::numeric_limits<float>::max();samples[5]=-std::numeric_limits<float>::max();
        fifo->push(samples.data(),2048,1000);fifo->window(window,1000);
        require(window[0]==0&&window[1]==0&&window[2]==2&&window[3]==-2&&window[4]==32&&window[5]==-32,"Capture sanitization or clipping failed");
        fifo->reset();std::vector<float> large(40000*2,.5f);fifo->push(large.data(),40000,1000);
        require(fifo->dropped()==40000-16384&&fifo->window(window,1000)==16384,"FIFO capacity was exceeded");
        // A modal native picker can take more than 2,048 frames between pumps.
        // Streaming export must not repeat overlapping history or lose the
        // beginning of a queued block merely because history is smaller.
        fifo->reset();std::vector<float> sequence(12000*2);
        for(unsigned n=0;n<12000;++n){sequence[n*2]=float(n)/16384;sequence[n*2+1]=-sequence[n*2];}
        fifo->push(sequence.data(),12000,1000);
        std::array<float,667> chunk;chunk.fill(99);
        require(fifo->drain(chunk,1000)==0,"Odd streaming buffer consumed PCM");
        unsigned total=0;
        while(const auto count=fifo->drain(std::span(chunk).first(666),1000)){
            require(count<=333&&chunk[666]==99,"Streaming read exceeded its buffer");
            for(unsigned n=0;n<count*2;++n)require(chunk[n]==sequence[total*2+n],"Streaming read lost/repeated/reordered float PCM");
            total+=count;
        }
        require(total==12000&&fifo->dropped()==0,"Streaming read retained a hidden history-size limit");
        fifo->window(window,1000);require(std::equal(window.begin(),window.end(),sequence.end()-4096),"Streaming/history tail disagrees");
        fifo->push(sequence.data(),10,1000);require(fifo->drain(std::span(chunk).first(666),1300)==0,"Stale streaming PCM was republished as fresh");
        fifo->window(window,1300);require(std::all_of(window.begin(),window.end(),[](float x){return x==0;}),"Stale streaming discard retained old history");
        fifo->push(sequence.data(),5,1400);require(fifo->drain(std::span(chunk).first(666),1400)==5,"Streaming read did not recover after stale input");
        for(unsigned n=0;n<10;++n)require(chunk[n]==sequence[n],"Streaming recovery changed PCM");
        fifo->reset();std::atomic<bool> finished=false;std::exception_ptr failure;
        std::thread writer([&]{std::array<float,128> block{};for(unsigned n=0;n<20000;++n){
            for(unsigned frame=0;frame<64;++frame){float value=float((n*64+frame)%16384)/16384;block[frame*2]=value;block[frame*2+1]=-value;}
            fifo->push(block.data(),64,1000);if(n%8==0)std::this_thread::yield();}finished.store(true,std::memory_order_release);});
        try{
            do{fifo->window(window,1000);for(unsigned n=0;n<2048;++n)require(window[n*2]==-window[n*2+1],"Concurrent capture tore a stereo frame");}
            while(!finished.load(std::memory_order_acquire));
        }catch(...){failure=std::current_exception();}
        writer.join();if(failure)std::rethrow_exception(failure);
        require(fifo->received()==20000ull*64,"64-bit capture frame count differs");
        fifo->reset();finished=false;std::atomic<bool> stop=false;std::atomic<uint64_t> read=0;
        constexpr uint64_t streamingFrames=640000;
        std::thread streaming([&]{std::array<float,128> block{};
            for(uint64_t offset=0;offset<streamingFrames&&!stop.load(std::memory_order_acquire);offset+=64){
                while(offset-read.load(std::memory_order_acquire)>=8192&&!stop.load(std::memory_order_acquire))std::this_thread::yield();
                if(stop.load(std::memory_order_acquire))break;
                for(unsigned n=0;n<64;++n){block[n*2]=float((offset+n)%16384)/16384;block[n*2+1]=-block[n*2];}
                fifo->push(block.data(),64,1000);
            }finished.store(true,std::memory_order_release);
        });
        uint64_t received=0;
        try{
            while(received<streamingFrames){auto count=fifo->drain(std::span(chunk).first(666),1000);
                if(!count&&finished.load(std::memory_order_acquire)){count=fifo->drain(std::span(chunk).first(666),1000);require(count||received==streamingFrames,"Streaming producer finished with missing frames");}
                for(unsigned n=0;n<count;++n){const auto expected=float((received+n)%16384)/16384;
                    require(chunk[n*2]==expected&&chunk[n*2+1]==-expected,"Concurrent streaming PCM tore/repeated/reordered frames");}
                received+=count;read.store(received,std::memory_order_release);
                if(!count)std::this_thread::yield();
            }
        }catch(...){failure=std::current_exception();stop.store(true,std::memory_order_release);}
        streaming.join();if(failure)std::rethrow_exception(failure);
        require(fifo->received()==streamingFrames&&fifo->dropped()==0,"Concurrent streaming dropped bounded queued PCM");
        std::cout<<"{\"result\":\"pass\",\"threaded_frames\":1280000,\"streamed_frames\":640000,\"capacity_frames\":16384,\"format\":\"float32 stereo\"}\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
