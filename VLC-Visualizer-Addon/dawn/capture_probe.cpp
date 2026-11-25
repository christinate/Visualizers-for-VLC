// SPDX-License-Identifier: GPL-2.0-or-later
#include "audio_capture.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>

int main(int argc,char** argv){
    try{
        std::string input,output;double seconds=2;bool list=false;
        for(int n=1;n<argc;++n){std::string a=argv[n];if(a=="--list")list=true;
            else if(a=="--input"&&n+1<argc)input=argv[++n];else if(a=="--seconds"&&n+1<argc)seconds=std::stod(argv[++n]);
            else if(a=="--output"&&n+1<argc)output=argv[++n];else throw std::runtime_error("Unknown capture probe argument");}
        if(!std::isfinite(seconds)||seconds<=0||seconds>60)throw std::runtime_error("Capture duration must be in (0,60]");
        visualizers::AudioCapture capture;nlohmann::json report;
        report["devices"]=nlohmann::json::array();for(const auto& device:capture.devices())
            report["devices"].push_back({{"id",device.id},{"label",device.label},{"default",device.isDefault}});
        if(!list){
            if(input.empty())throw std::runtime_error("Specify --input explicitly to open a recording device, or use --list");
            input=capture.resolveInput(input);
            visualizers::requestCapturePermission();const auto permissionStart=visualizers::audioMilliseconds();
            if(visualizers::capturePermission()==visualizers::CapturePermission::Pending)
                std::cerr<<visualizers::capturePermissionMessage(visualizers::CapturePermission::Pending)<<'\n';
            while(visualizers::capturePermission()==visualizers::CapturePermission::Pending){
                if(visualizers::audioMilliseconds()-permissionStart>180000)throw std::runtime_error("Microphone permission is still pending; allow access and retry this check");
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            capture.start(input);const auto start=visualizers::audioMilliseconds();std::array<float,4096> audio{};double maximum=0;
            while(double(visualizers::audioMilliseconds()-start)<seconds*1000){
                capture.window(audio);double squares=0;for(float value:audio){if(!std::isfinite(value))throw std::runtime_error("Capture produced nonfinite PCM");squares+=double(value)*value;}
                maximum=std::max(maximum,std::sqrt(squares/4096));std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            const auto status=capture.status();report["capture"]={{"backend",status.backend},{"device",status.device},{"running",status.running},
                {"frames",status.frames},{"dropped_frames",status.dropped},{"native_rate",status.nativeRate},{"native_channels",status.nativeChannels},
                {"output_rate",44100},{"output_channels",2},{"format","float32"},{"maximum_rms",maximum}};
            if(!status.running||!status.frames)throw std::runtime_error("Recording device did not deliver frames");capture.stop();
        }
        report["result"]="pass";report["audio_saved"]=false;
        if(!output.empty())std::ofstream(output)<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){std::cerr<<nlohmann::json({{"result","fail"},{"error",error.what()}}).dump(2)<<'\n';return 1;}
}
