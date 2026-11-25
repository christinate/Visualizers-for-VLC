// SPDX-License-Identifier: GPL-2.0-or-later
#include "plugin.hpp"
#include "resource_metrics.hpp"
#include <dawn/native/DawnNative.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <type_traits>

using namespace visualizers::gpu;using json=nlohmann::json;namespace fs=std::filesystem;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static uint64_t used(Context& context){return dawn::native::ComputeEstimatedMemoryUsageInfo(context.device.Get()).totalUsage;}
static void settle(Context& context){
    auto done=std::make_shared<bool>(false);context.wait(context.queue.OnSubmittedWorkDone(wgpu::CallbackMode::WaitAnyOnly,[done](wgpu::QueueWorkDoneStatus status,wgpu::StringView){*done=status==wgpu::QueueWorkDoneStatus::Success;}),"Lifetime test completion");
    require(*done,"Lifetime test GPU work did not complete");
    for(unsigned n=0;n<100;++n){
        // Vulkan's trim can submit deferred deletions. Advance completed queue
        // serials as well; polling the trim flag alone cannot retire that work.
        dawn::native::DeviceTick(context.device.Get());
        if(!dawn::native::ReduceMemoryUsage(context.device.Get()))return;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("Lifetime test GPU resources did not settle");
}
int main(int argc,char** argv){json report;fs::path output="resource-check";
    try{
        static_assert(std::is_trivially_copyable_v<dawn::native::MemoryUsageInfo>);
        if(argc!=4)throw std::runtime_error("Expected native plugin, assets and output directory");output=argv[3];fs::create_directories(output);
        const std::string large(65536,'x');require(string(wgpu::StringView(large.data(),large.size())).size()==4096,"GPU diagnostic copy exceeded its bound");
        const auto unicode=std::string(4095,'x')+"音声";const auto clipped=string(wgpu::StringView(unicode.data(),unicode.size()));require(clipped.size()==4095,"GPU diagnostic truncation split UTF-8");
        Diagnostics diagnostics;for(unsigned n=0;n<100;++n)diagnostics.add(large);require(diagnostics.errors.size()==32&&diagnostics.errors.front().size()==4096,"GPU error history exceeded its bound");
        Context context;report["adapter"]=context.info();settle(context);const auto baseline=used(context);
        {
            auto retained=context.target(64,64);require(used(context)>=baseline+32768,"GPU accounting positive control did not detect a retained texture");
        }
        settle(context);require(used(context)==baseline,"Positive-control texture remained live after release");
        PluginLibrary library(argv[1],"terrain");json samples=json::array();uint64_t steadyBaseline=0;
        const auto initialTextures=dawn::native::ComputeEstimatedMemoryUsageInfo(context.device.Get()).texturesUsage;
        for(unsigned cycle=0;cycle<40;++cycle){
            bool failed=false;try{library.create(context,0,64,fs::absolute(argv[2]));}catch(const std::exception& error){failed=std::string(error.what()).find("dimensions")!=std::string::npos;}
            require(failed,"Partial renderer initialization did not reject zero dimensions");
            {
                auto renderer=library.create(context,64,64,fs::absolute(argv[2]));visualizers::MusicFrame audio;audio.rms=.1;
                for(unsigned frame=0;frame<3;++frame){renderer->resize(frame==1?96:64,64);renderer->render(audio,cycle+frame*.04,1./60);auto image=renderer->image();const unsigned width=image.GetWidth();const auto pixels=context.readRgba16(image,width,64);require(pixels.size()==size_t(width)*64*4,"Lifetime readback was incomplete");}
            }
            settle(context);const auto usage=dawn::native::ComputeEstimatedMemoryUsageInfo(context.device.Get());const auto live=usage.totalUsage;
            // Internal staging buffers can shrink after the first submitted
            // frames. Establish the repeatable baseline after eight full cycles;
            // textures must still return to their original count every cycle.
            if(cycle==7)steadyBaseline=live;
            if(usage.texturesUsage>initialTextures||(cycle>=8&&live>steadyBaseline)){report["retained"]={{"cycle",cycle},{"baseline",steadyBaseline},{"live",live},{"textures",usage.texturesUsage},{"buffers",usage.buffersUsage}};throw std::runtime_error("Renderer initialization/resize/destruction retained GPU texture or buffer bytes");}
            if(cycle==7||cycle==23||cycle==39){auto sample=visualizers::testing::resourceMetrics();sample["completed_cycles"]=cycle+1;sample["live_gpu_bytes"]=live;samples.push_back(sample);}
        }
        // Fill the actual asynchronous callback registry. An additional future
        // must be rejected, then completed before its callback state is freed.
        std::array<wgpu::Future,129> futures{};auto completed=std::make_shared<unsigned>(0);
        for(unsigned n=0;n<futures.size();++n){futures[n]=context.queue.OnSubmittedWorkDone(wgpu::CallbackMode::WaitAnyOnly,[completed](wgpu::QueueWorkDoneStatus,wgpu::StringView){++*completed;});const auto status=context.trackFuture(WGPUFuture{futures[n].id});require((n<128&&status==0)||(n==128&&status!=0),"Native callback registry exceeded or failed its 128-future quota");}
        require(context.instance.WaitAny(futures.back(),1'000'000'000)==wgpu::WaitStatus::Success,"Overflow future cleanup failed");
        for(unsigned n=0;n<128;++n)context.wait(futures[n],"Future registry cleanup");require(*completed==129,"Native callback registry did not deliver every completion");
        settle(context);require(used(context)<=steadyBaseline,"Callback registry cleanup retained GPU resources");
        report["result"]="pass";report["cycles"]=40;report["partial_initialization_failures"]=40;report["cold_live_gpu_bytes"]=baseline;report["baseline_live_gpu_bytes"]=steadyBaseline;report["gpu_accounting_positive_control_bytes"]=32768;report["callback_completions"]=*completed;report["samples"]=samples;
        std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report["result"]="fail";report["error"]=error.what();std::error_code ignored;fs::create_directories(output,ignored);std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
