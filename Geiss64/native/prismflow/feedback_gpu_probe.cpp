// SPDX-License-Identifier: GPL-2.0-or-later
#include "gpu_feedback.hpp"
#include "gpu_dots.hpp"
#include "random.hpp"
#include "resource_metrics.hpp"
#include <dawn/native/DawnNative.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
namespace fs=std::filesystem;using namespace prismflow;using json=nlohmann::json;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>static void rejects(F&& fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid feedback request accepted");}
template<class T>static std::vector<T> read(const fs::path& folder,const std::string& name){
    require(fs::path(name).filename()==fs::path(name),"Invalid fixture filename");const auto file=folder/name;const auto size=fs::file_size(file);require(size<=16*1024*1024&&size%sizeof(T)==0,"Invalid fixture byte count");
    std::vector<T> values(size/sizeof(T));std::ifstream input(file,std::ios::binary);input.read(reinterpret_cast<char*>(values.data()),std::streamsize(size));require(bool(input),"Cannot read feedback fixture");return values;
}
static uint64_t checksum(std::span<const uint16_t> values){uint64_t hash=1469598103934665603ull;for(auto value:values){hash^=value;hash*=1099511628211ull;}return hash;}
static void settle(GpuContext& context){
    auto done=std::make_shared<bool>(false);context.wait(context.queue.OnSubmittedWorkDone(wgpu::CallbackMode::WaitAnyOnly,[done](wgpu::QueueWorkDoneStatus status,wgpu::StringView){*done=status==wgpu::QueueWorkDoneStatus::Success;}),"Feedback lifetime completion");require(*done,"Feedback work did not finish");
    for(unsigned n=0;n<100;++n){dawn::native::DeviceTick(context.device.Get());if(!dawn::native::ReduceMemoryUsage(context.device.Get()))return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("Feedback resources did not settle");
}
static FeedbackFrame frameData(const json& item,const RandomFunction& random){
    FeedbackFrame frame;frame.strengths=item.at("strengths").get<std::array<double,8>>();frame.shiftX=item.at("shift_x");frame.shiftY=item.at("shift_y");frame.darkening=item.at("darkening");frame.time=item.at("time");frame.oversample=item.at("oversample");
    frame.noiseX=uint32_t(random()*255);frame.noiseY=uint32_t(random()*255);require(frame.noiseX==item.at("noise_x")&&frame.noiseY==item.at("noise_y"),"Frame random-noise consumption differs");
    const auto& p=item.at("shader");
#define F(field) frame.shader.field=p.value(#field,0.);
    F(rx_diff_grey_scott_str) F(rx_diff_grey_scott_str2) F(rx_diff_barkley_str) F(frequency_injector_str) F(frequency_injector_freq_scale) F(painterly_str) F(sharpen_str) F(noise_str) F(layer2_warp_scale) F(layer1_density) F(layer2_brightness) F(layer2_motion_scale)
#undef F
    frame.diffusionColorization=p.value("diffusion_colorization",0.);return frame;
}
static json compare(GpuContext& context,GpuAssets& bank,const json& fixture,const fs::path& folder,std::vector<MotionMap>& maps,const fs::path& output,bool exact){
    Mulberry32 generator(fixture.at("init_seed"));for(unsigned n=0;n<fixture.at("init_rng_skip").get<unsigned>();++n)generator();auto random=[&]{return generator();};
    GpuDynamicMaps dynamic(context,bank,fixture.at("dynamic_seed"));GpuFeedback feedback(context,bank,dynamic,fixture.at("initial_width"),fixture.at("initial_height"),random);
    std::unique_ptr<GpuDots> dots;std::unique_ptr<GpuWarpDots> warpDots;if(fixture.contains("warp_dot_seed")){dots=std::make_unique<GpuDots>(context,bank);warpDots=std::make_unique<GpuWarpDots>(context,bank,fixture.at("warp_dot_seed"));}
    uint64_t dotComponents=0,dotControls=0,dotBits=0;double maxDotError=0;
    uint64_t gaussian=0,uniforms=0,components=0,differing=0;double maxError=0,sumError=0,maxExposure=0;json results=json::array();
    for(unsigned n=0;n<4;++n){const auto expected=read<uint8_t>(folder,fixture.at("noise_files").at(n));const auto actual=readByteTexture(context,feedback.noiseTexture(n));require(actual==expected,"Gaussian noise bytes differ from the preserved reference");gaussian+=actual.size();}
    feedback.uploadIndices(read<uint16_t>(folder,fixture.at("initial_file")));
    for(const auto& item:fixture.at("frames")){
        if(!item.at("resize").is_null())feedback.resize(item.at("resize").at("width"),item.at("resize").at("height"),random);
        for(size_t slot=0;slot<8;++slot)feedback.bindMap(slot,maps.at(item.at("bindings").at(slot).get<size_t>()));
        require(feedback.cacheSlots()==item.at("cache_slots").get<std::array<int32_t,8>>(),"Feedback cache slot assignment differs");
        feedback.uploadSpectrum(item.at("spectrum").get<std::vector<float>>());const auto frame=frameData(item,random);const auto data=GpuFeedback::uniformData(feedback.width(),feedback.height(),frame);
        for(size_t n=0;n<data.size();++n){require(data[n]==item.at("uniforms").at(n).get<float>(),"Feedback uniform differs from reference");++uniforms;}
        const auto encoder=feedback.beginFrame(frame);if(warpDots){const auto& p=item.at("dots");warpDots->encode(encoder,feedback,WarpDotFrame{p.at("true_time"),p.at("strength"),p.at("point_size")});}
        feedback.encodeWarp();if(dots){dots->dimensions(feedback.width(),feedback.height());const auto points=item.at("waveform").get<std::vector<float>>();dots->upload(points);dots->draw(encoder,feedback.view());warpDots->draw(encoder,feedback.view(),*dots);}
        feedback.submit();require(feedback.frame()==item.at("frame").get<uint32_t>()+1,"Feedback frame count differs");
        if(warpDots){for(const auto& [field,buffer]:std::array<std::pair<const char*,wgpu::Buffer>,2>{{{"dots_positions",warpDots->positions()},{"dots_instances",warpDots->instances()}}}){
            const auto raw=readGpuBuffer(context,buffer);const auto expected=read<float>(folder,item.at(field));require(raw.size()==expected.size()*sizeof(float),"Warp-dot state size differs");
            for(size_t n=0;n<expected.size();++n){float value;std::memcpy(&value,raw.data()+n*4,4);const double difference=std::abs(double(value)-expected[n]);const bool changed=std::bit_cast<uint32_t>(value)!=std::bit_cast<uint32_t>(expected[n]);
                if(!std::isfinite(value)||difference>.0002+std::abs(expected[n])*1e-6||(exact&&changed)){json detail={{"frame",item.at("frame")},{"field",field},{"component",n},{"actual",value},{"expected",expected[n]},{"absolute_error",difference},{"limit",.0002+std::abs(expected[n])*1e-6}};std::ofstream(output/"particle-state-detail.json")<<detail.dump(2)<<'\n';throw std::runtime_error("Warp-dot GPU state differs from reference");}
                maxDotError=std::max(maxDotError,difference);dotBits+=changed;++dotComponents;}}
            const auto expected=read<uint8_t>(folder,item.at("dots_controls"));require(readGpuBuffer(context,warpDots->controls())==expected,"Warp-dot scalar controls differ");dotControls+=expected.size();}
        const auto actual=readHalfTexture(context,feedback.texture()),expected=read<uint16_t>(folder,item.at("file"));require(actual.size()==expected.size(),"Feedback dimensions differ");
        double error=0,mean=0;uint64_t changed=0;for(size_t n=0;n<actual.size();++n){const double value=visualizers::gpu::decodeHalf(actual[n]),reference=visualizers::gpu::decodeHalf(expected[n]);
            require(std::isfinite(value)&&std::isfinite(reference)&&value>=0&&(dots||value<=1),"Feedback contains invalid channel values");const double difference=std::abs(value-reference);error=std::max(error,difference);mean+=difference;if(actual[n]!=expected[n])++changed;}
        std::array<float,4> exposure{};const auto bytes=readGpuBuffer(context,feedback.exposureBuffer());require(bytes.size()==sizeof(exposure),"Exposure buffer size differs");std::memcpy(exposure.data(),bytes.data(),sizeof(exposure));double exposureError=0;
        for(unsigned n=0;n<4;++n)exposureError=std::max(exposureError,std::abs(double(exposure[n])-item.at("exposure").at(n).get<double>()));
        mean/=actual.size();maxError=std::max(maxError,error);sumError+=mean*actual.size();maxExposure=std::max(maxExposure,exposureError);components+=actual.size();differing+=changed;
        results.push_back({{"frame",item.at("frame")},{"maximum_absolute_error",error},{"mean_absolute_error",mean},{"differing_half_components",changed},{"exposure_error",exposureError},{"checksum",checksum(actual)}});
        if(error>.002||mean>.0002||exposureError>.000001||(exact&&(changed||exposureError!=0))){json failure={{"frames",results},{"result","fail"}};std::ofstream(output/"comparison-detail.json")<<failure.dump(2)<<'\n';throw std::runtime_error("Feedback/reference numerical limits exceeded; inspect comparison-detail.json");}
    }
    for(const auto& item:fixture.at("cache_trace")){auto fresh=maps[0];fresh.identity=nextMotionMapIdentity();const auto before=feedback.cacheSlots();const size_t slot=item.at("slot");feedback.bindMap(slot,fresh);
        require(feedback.cacheSlots()==item.at("slots").get<std::array<int32_t,8>>(),"Protected LRU cache eviction differs from reference");const auto selected=feedback.cacheSlots()[slot];require(std::find(before.begin(),before.end(),selected)==before.end(),"Warp cache evicted a bound slot");require(feedback.populatedMaps()<=20,"Warp cache exceeded its capacity");}
    require(feedback.populatedMaps()==20,"Cache pressure did not populate all twenty entries");
    const auto before=checksum(readHalfTexture(context,feedback.texture()));FeedbackFrame frame;frame.strengths[0]=1;frame.shader.layer2_motion_scale=1;
    feedback.beginFrame(frame);rejects([&]{feedback.bindMap(0,maps[0]);});rejects([&]{feedback.resize(10,10,random);});rejects([&]{feedback.beginFrame(frame);});feedback.abortFrame();
    require(checksum(readHalfTexture(context,feedback.texture()))==before,"Abandoned prepared frame changed the image");feedback.beginFrame(frame);feedback.encodeWarp();feedback.abortFrame();require(checksum(readHalfTexture(context,feedback.texture()))==before,"Abandoned encoded frame advanced the image");
    rejects([&]{feedback.submit();});rejects([&]{feedback.encodeWarp();});rejects([&]{feedback.bindMap(8,maps[0]);});rejects([&]{feedback.uploadIndices(std::span<const uint16_t>{});});
    auto invalid=frame;invalid.time=INFINITY;rejects([&]{feedback.beginFrame(invalid);});auto missing=maps[0];missing.displacement.clear();rejects([&]{feedback.bindMap(0,missing);});
    auto duplicate=maps[0];require(duplicate.identity==maps[0].identity,"Immutable map copy lost its identity");const auto distinct=buildWarpMap(maps[0].mode,maps[0].seed);require(distinct.identity!=maps[0].identity,"Independent map generation reused an identity");
    if(dots){GpuDots capacityProbe(context,bank,4);std::array<float,20> points{};require(capacityProbe.upload(points)==4,"Dot uploads did not retain capacity truncation");points[0]=NAN;rejects([&]{capacityProbe.upload(points);});rejects([&]{capacityProbe.upload(std::span(points).first(3));});rejects([&]{capacityProbe.dimensions(0,1);});require(capacityProbe.upload({})==0&&capacityProbe.count()==0,"Empty upload retained dot count");}
    return {{"frames",results},{"exact_required",exact},{"gaussian_bytes_exact",gaussian},{"uniform_components_exact",uniforms},{"feedback_components_compared",components},{"differing_half_components",differing},{"maximum_absolute_error",maxError},{"mean_absolute_error",sumError/components},{"maximum_exposure_error",maxExposure},{"warp_dot_components_compared",dotComponents},{"warp_dot_differing_bits",dotBits},{"warp_dot_control_bytes_exact",dotControls},{"maximum_warp_dot_state_error",maxDotError},{"cache_eviction_steps",fixture.at("cache_trace").size()},{"cache_and_frame_state_checks","pass"}};
}
static json lifetimes(GpuContext& context,GpuAssets& bank,const MotionMap& map,unsigned cycles){
    settle(context);const auto cold=dawn::native::ComputeEstimatedMemoryUsageInfo(context.device.Get());uint64_t steady=0;json samples=json::array();
    {const auto retained=context.target(64,64);require(dawn::native::ComputeEstimatedMemoryUsageInfo(context.device.Get()).texturesUsage>=cold.texturesUsage+32768,"Texture accounting control missed a retained texture");}settle(context);
    for(unsigned cycle=0;cycle<cycles;++cycle){
        {Mulberry32 generator(cycle);auto random=[&]{return generator();};GpuDynamicMaps dynamic(context,bank,cycle);
            bool rejected=false;try{GpuFeedback partial(context,bank,dynamic,32,24,[]{return INFINITY;});}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Partial feedback initialization did not reject invalid noise");
            GpuFeedback feedback(context,bank,dynamic,32,24,random);feedback.bindMap(0,map);feedback.fillIndices(random);FeedbackFrame frame;frame.strengths[0]=1;frame.shader.layer2_motion_scale=1;
            GpuDots dots(context,bank,8);GpuWarpDots warpDots(context,bank,cycle);const std::array<float,8> points{10,10,1,.1f,15,15,4,.3f};dots.upload(points);
            for(unsigned n=0;n<3;++n){if(n==1)feedback.resize(40,32,random);const auto encoder=feedback.beginFrame(frame);warpDots.encode(encoder,feedback,WarpDotFrame{double(n)*.3,.5,2});feedback.encodeWarp();dots.dimensions(feedback.width(),feedback.height());dots.draw(encoder,feedback.view());warpDots.draw(encoder,feedback.view(),dots);if(n==0&&cycle%2==0)feedback.abortFrame();else feedback.submit();}
            const auto pixels=readHalfTexture(context,feedback.texture());for(auto value:pixels)require(std::isfinite(visualizers::gpu::decodeHalf(value)),"Lifetime feedback output became nonfinite");}
        settle(context);const auto used=dawn::native::ComputeEstimatedMemoryUsageInfo(context.device.Get());if(cycle==7)steady=used.totalUsage;
        if(used.texturesUsage>cold.texturesUsage||(cycle>=8&&used.totalUsage>steady))throw std::runtime_error("Feedback lifetime cycles retained GPU textures or buffers");
        if(cycle==7||cycle==23||cycle+1==cycles){auto sample=visualizers::testing::resourceMetrics();sample["completed_cycles"]=cycle+1;sample["live_gpu_bytes"]=used.totalUsage;sample["live_texture_bytes"]=used.texturesUsage;samples.push_back(std::move(sample));}
    }
    return {{"cycles",cycles},{"partial_initialization_failures",cycles},{"cold_live_gpu_bytes",cold.totalUsage},{"steady_live_gpu_bytes",steady},{"samples",samples}};
}
int main(int argc,char** argv){fs::path assets,fixtures,output="prismflow-feedback";json report;std::string current;
    try{
        static_assert(std::endian::native==std::endian::little);wgpu::BackendType backend=wgpu::BackendType::Undefined;unsigned cycles=0;bool exact=false,lifetimesOnly=false;
        for(int n=1;n<argc;++n){const std::string option=argv[n];if(option=="--assets"&&n+1<argc)assets=argv[++n];else if(option=="--fixtures"&&n+1<argc)fixtures=argv[++n];else if(option=="--output"&&n+1<argc)output=argv[++n];else if(option=="--exact")exact=true;
            else if(option=="--lifetimes-only")lifetimesOnly=true;
            else if(option=="--cycles"&&n+1<argc){const auto count=std::stoul(argv[++n]);require(count<=40,"At most forty lifetime cycles are supported");cycles=unsigned(count);}
            else if(option=="--backend"&&n+1<argc){const std::string value=argv[++n];if(value=="d3d12")backend=wgpu::BackendType::D3D12;else if(value=="metal")backend=wgpu::BackendType::Metal;else if(value=="vulkan")backend=wgpu::BackendType::Vulkan;else throw std::runtime_error("Unknown feedback backend");}else throw std::runtime_error("Unknown feedback probe argument");}
        require(!assets.empty()&&cycles<=40,"Expected assets and at most 40 cycles");
        if(lifetimesOnly){
            require(cycles>0&&fixtures.empty()&&!exact,"Lifetime-only mode requires cycles and does not accept reference/exact options");
            fs::create_directories(output);GpuContext context(backend);GpuAssets bank(context,assets);report=context.info();current="feedback lifetimes";
            report["lifetimes"]=lifetimes(context,bank,buildWarpMap(0,11),cycles);report["comparison"]="not run: explicit lifetime-only mode";
            report["result"]="pass";report["scope"]="Bounded resource/state checks only; no reference-image comparison";
            std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return 0;
        }
        require(!fixtures.empty(),"Expected feedback fixtures unless --lifetimes-only is selected");require(fs::file_size(fixtures)<1024*1024,"Feedback manifest exceeds bound");std::ifstream input(fixtures);const auto fixture=json::parse(input);require(fixture.at("maps").size()<=64&&fixture.at("frames").size()<=128,"Feedback fixture count exceeds bound");
        require(!fixture.at("maps").empty()&&!fixture.at("frames").empty(),"Feedback fixture lists must be nonempty");
        fs::create_directories(output);GpuContext context(backend);GpuAssets bank(context,assets);report=context.info();std::vector<MotionMap> maps;uint64_t mapComponents=0;double maxMapError=0;
        current="CPU map inputs";for(const auto& entry:fixture.at("maps")){auto map=buildWarpMap(entry.at("mode"),entry.at("seed"));const auto reference=read<float>(fixtures.parent_path(),entry.at("file"));const auto& actual=map.isDynamic()?map.dynamic:map.displacement;require(actual.size()==reference.size(),"CPU map input shape differs");
            for(size_t n=0;n<actual.size();++n){const double difference=std::abs(double(actual[n])-reference[n]);maxMapError=std::max(maxMapError,difference);require(std::isfinite(actual[n])&&difference<=2e-7+std::abs(reference[n])*1e-6,"CPU map input differs from reference");++mapComponents;}maps.push_back(std::move(map));}
        current="feedback comparison";report["reference_adapter"]=fixture.at("adapter");report["comparison"]=compare(context,bank,fixture,fixtures.parent_path(),maps,output,exact);report["map_components_compared"]=mapComponents;report["maximum_map_input_error"]=maxMapError;
        if(cycles){current="feedback lifetimes";report["lifetimes"]=lifetimes(context,bank,maps[0],cycles);}report["result"]="pass";report["scope"]=fixture.contains("warp_dot_seed")?"Feedback plus explicit point drawing and warp-dot advection/state; bokeh, text and full scene remain open":"Feedback/exposure/map plumbing only; particles, text and full-frame parity remain open";
        std::ofstream(output/"result.json")<<report.dump(2)<<'\n';auto summary=report;summary["comparison"].erase("frames");std::cout<<summary.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report["result"]="fail";report["case"]=current;report["error"]=error.what();std::error_code ignored;fs::create_directories(output,ignored);std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
