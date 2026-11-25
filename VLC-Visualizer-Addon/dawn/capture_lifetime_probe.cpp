// SPDX-License-Identifier: GPL-2.0-or-later
#include "audio_capture.hpp"
#include "resource_metrics.hpp"
#include <chrono>
#include <iostream>
#include <thread>

using namespace visualizers;using json=nlohmann::json;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){json report;std::filesystem::path output;
    try{
        std::string input;for(int n=1;n<argc;++n){const std::string argument=argv[n];if(argument=="--record-input"&&n+1<argc)input=argv[++n];else if(argument=="--output"&&n+1<argc)output=argv[++n];else throw std::runtime_error("Unknown capture lifetime argument");}
        uint64_t recorded=0;json samples=json::array();
        for(unsigned cycle=0;cycle<40;++cycle){
            {
                AudioCapture capture;capture.devices();bool failed=false;
                try{capture.start("missing-native-lifetime-fixture-input");}catch(const std::exception& error){failed=std::string(error.what()).find("missing")!=std::string::npos;}
                require(failed&&!capture.status().running,"Missing-input initialization did not release recording state");
                if(!input.empty()){
                    capture.start(input);std::array<float,4096> pcm{};const auto start=audioMilliseconds();
                    do{capture.window(pcm);std::this_thread::sleep_for(std::chrono::milliseconds(10));}while(capture.status().frames<512&&audioMilliseconds()-start<1000);
                    const auto active=capture.status();require(active.running&&active.frames>=512,"Repeated capture did not deliver PCM");recorded+=active.frames;
                    capture.stop();require(!capture.status().running&&capture.status().frames==0,"Stopped recording retained a running device or FIFO history");
                }
                capture.stop();
            }
            if(cycle==7||cycle==23||cycle==39){auto metrics=testing::resourceMetrics();metrics["completed_cycles"]=cycle+1;samples.push_back(metrics);}
        }
        // Some OS audio services retain a small global/threadpool cache after
        // first use. Compare the two equal post-warmup batches, not cold start.
        const uint64_t first=samples.at(1).at("handles_or_fds"),last=samples.at(2).at("handles_or_fds");
        require(last<=first+4,"Repeated capture contexts retained process handles/file descriptors");
        report={{"result","pass"},{"cycles",40},{"recording",!input.empty()},{"recorded_frames",recorded},{"audio_saved",false},{"samples",samples}};
        if(!output.empty())std::ofstream(output)<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report={{"result","fail"},{"error",error.what()}};if(!output.empty())std::ofstream(output)<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
