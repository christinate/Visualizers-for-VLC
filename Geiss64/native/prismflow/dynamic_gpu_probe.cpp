// SPDX-License-Identifier: GPL-2.0-or-later
#include "gpu_dynamic.hpp"
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
namespace fs=std::filesystem;using namespace prismflow;using json=nlohmann::json;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>static void rejects(F&& fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid dynamic GPU request accepted");}
static uint64_t checksum(std::span<const uint16_t> values){uint64_t hash=1469598103934665603ull;for(auto value:values){hash^=value;hash*=1099511628211ull;}return hash;}
static void raw(const fs::path& file,const std::vector<uint16_t>& values){std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(values.data()),std::streamsize(values.size()*2));}
// Instrument the unchanged integer noise function with full-precision storage.
// This separates hash arithmetic from the backend's float16 storage rounding.
class NoiseOracle {
    GpuContext& context_;wgpu::ComputePipeline pipeline_;
public:
    NoiseOracle(GpuContext& context,const fs::path& assets):context_(context){
        std::ifstream file(assets/"noise_2d.wgsl",std::ios::binary);std::string source((std::istreambuf_iterator<char>(file)),{});
        const std::string before="texture_storage_2d<rgba16float, write>",after="texture_storage_2d<rgba32float, write>";const auto index=source.find(before);require(index!=std::string::npos,"Noise storage instrumentation marker missing");source.replace(index,before.size(),after);
        const auto module=context_.shader(source);wgpu::ComputePipelineDescriptor pipeline{};pipeline.compute.module=module;pipeline.compute.entryPoint="cs_main";pipeline_=context_.device.CreateComputePipeline(&pipeline);
    }
    std::vector<float> values(uint32_t seed,size_t pixels){
        require(pixels&&pixels%64==0&&pixels<=65536,"Unexpected noise oracle dimensions");const uint32_t width=64,height=uint32_t(pixels/64);
        const auto texture=gpuTexture(context_,width,height,wgpu::TextureFormat::RGBA32Float,wgpu::TextureUsage::StorageBinding|wgpu::TextureUsage::CopySrc);
        const auto uniform=gpuBuffer(context_,16,wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst);const std::array<uint32_t,4> data={seed,0,0,0};context_.queue.WriteBuffer(uniform,0,data.data(),sizeof(data));
        const std::array entries={bindTexture(0,texture.CreateView()),bindBuffer(1,uniform)};const auto group=gpuBindings(context_,pipeline_.GetBindGroupLayout(0),entries);
        auto encoder=context_.device.CreateCommandEncoder();auto pass=encoder.BeginComputePass();pass.SetPipeline(pipeline_);pass.SetBindGroup(0,group);pass.DispatchWorkgroups(8,(height+7)/8);pass.End();
        const auto buffer=gpuBuffer(context_,pixels*16,wgpu::BufferUsage::CopyDst|wgpu::BufferUsage::MapRead);
        wgpu::TexelCopyTextureInfo from{};from.texture=texture;wgpu::TexelCopyBufferInfo to{};to.buffer=buffer;to.layout.bytesPerRow=width*16;to.layout.rowsPerImage=height;
        const wgpu::Extent3D size{width,height,1};encoder.CopyTextureToBuffer(&from,&to,&size);const auto commands=encoder.Finish();context_.queue.Submit(1,&commands);
        auto error=std::make_shared<std::string>();context_.wait(buffer.MapAsync(wgpu::MapMode::Read,0,pixels*16,wgpu::CallbackMode::WaitAnyOnly,[error](wgpu::MapAsyncStatus status,wgpu::StringView message){if(status!=wgpu::MapAsyncStatus::Success)*error=visualizers::gpu::string(message);}),"Full-precision noise diagnostics");
        require(error->empty(),"Could not read full-precision noise diagnostics");const auto* mapped=static_cast<const float*>(buffer.GetConstMappedRange());std::vector<float> result(mapped,mapped+pixels*4);buffer.Unmap();context_.checkErrors();return result;
    }
};
int main(int argc,char** argv){fs::path assets,fixtures,output="prismflow-dynamic-gpu";std::string current;json report;
    try{
        static_assert(std::endian::native==std::endian::little);wgpu::BackendType backend=wgpu::BackendType::Undefined;
        for(int n=1;n<argc;++n){const std::string option=argv[n];
            if(option=="--assets"&&n+1<argc)assets=argv[++n];else if(option=="--fixtures"&&n+1<argc)fixtures=argv[++n];else if(option=="--output"&&n+1<argc)output=argv[++n];
            else if(option=="--backend"&&n+1<argc){const std::string name=argv[++n];if(name=="d3d12")backend=wgpu::BackendType::D3D12;else if(name=="metal")backend=wgpu::BackendType::Metal;else if(name=="vulkan")backend=wgpu::BackendType::Vulkan;else throw std::runtime_error("Unknown backend");}
            else throw std::runtime_error("Unknown dynamic-probe argument");}
        require(!assets.empty()&&!fixtures.empty(),"Expected --assets and --fixtures");require(fs::file_size(fixtures)<1024*1024,"Dynamic fixture exceeds bound");std::ifstream input(fixtures);const auto fixture=json::parse(input);
        fs::create_directories(output);GpuContext context(backend);GpuAssets bank(context,assets);GpuDynamicMaps maps(context,bank,fixture.at("initial_seed"));report=context.info();
        uint64_t noiseComponents=0,uniformComponents=0,finiteComponents=0;json noiseResults=json::array(),frames=json::array();NoiseOracle oracle(context,assets);
        auto noiseCheck=[&](const std::string& name,const wgpu::Texture& texture){current=name;const json* expected=nullptr;for(const auto& entry:fixture.at("noise"))if(entry.at("name").get<std::string>()==name)expected=&entry;
            require(expected,"Missing noise fixture");const auto values=readHalfTexture(context,texture);const auto file=fixtures.parent_path()/expected->at("file").get<std::string>();require(fs::file_size(file)==values.size()*2,"Noise fixture size differs");
            std::vector<uint16_t> reference(values.size());std::ifstream binary(file,std::ios::binary);binary.read(reinterpret_cast<char*>(reference.data()),std::streamsize(reference.size()*2));require(bool(binary),"Cannot read noise fixture");
            const auto floatFile=fixtures.parent_path()/expected->at("float_file").get<std::string>();require(fs::file_size(floatFile)==values.size()*4,"Full-precision noise fixture size differs");
            std::vector<float> floats(values.size());std::ifstream floatInput(floatFile,std::ios::binary);floatInput.read(reinterpret_cast<char*>(floats.data()),std::streamsize(floats.size()*4));require(bool(floatInput),"Cannot read full-precision noise fixture");
            const auto gpuFloats=oracle.values(expected->at("seed"),values.size()/4);require(gpuFloats==floats,"Full-precision GPU noise hash differs from integer reference");
            uint64_t nearestOnly=0,truncatedOnly=0;
            for(size_t n=0;n<values.size();++n){const auto nearest=reference[n];const auto truncated=uint16_t(nearest-(visualizers::gpu::decodeHalf(nearest)>floats[n]?1:0));
                const bool matchesNearest=values[n]==nearest,matchesTruncated=values[n]==truncated;
                if(!matchesNearest&&!matchesTruncated){report["component"]=n;report["native_bits"]=values[n];report["nearest_bits"]=nearest;report["truncated_bits"]=truncated;throw std::runtime_error("Noise pixel does not match verified full-precision storage conversion");}
                if(matchesNearest&&!matchesTruncated)++nearestOnly;if(matchesTruncated&&!matchesNearest)++truncatedOnly;
            }
            require(!nearestOnly||!truncatedOnly,"Noise storage rounding is inconsistent across one texture");
            noiseComponents+=values.size();noiseResults.push_back({{"name",name},{"components",values.size()},{"checksum",checksum(values)},{"float32_hash_exact",true},{"storage_rounding",truncatedOnly?"toward_zero":"nearest_even"},{"nearest_only",nearestOnly},{"truncated_only",truncatedOnly}});
        };
        noiseCheck("static_3d",maps.staticNoise3d());noiseCheck("static_2d",maps.staticNoise2d());noiseCheck("dynamic_0",maps.dynamicNoise2d());
        current="spectrum upload";std::array<float,512> spectrum{};for(size_t n=0;n<spectrum.size();++n){const auto& value=fixture.at("spectrum").at("input").at(n);
            if(value.is_string()){const auto text=value.get<std::string>();if(text=="nan")spectrum[n]=std::numeric_limits<float>::quiet_NaN();else if(text=="infinity")spectrum[n]=INFINITY;else if(text=="-infinity")spectrum[n]=-INFINITY;else if(text=="-zero")spectrum[n]=-0.f;else throw std::runtime_error("Unknown spectrum input tag");}else spectrum[n]=value.get<float>();}
        maps.uploadSpectrum(spectrum);const auto uploaded=readHalfTexture(context,maps.spectrumTexture());for(size_t n=0;n<uploaded.size();++n)if(uploaded[n]!=fixture.at("spectrum").at("expected").at(n).get<uint16_t>()){
            report["component"]=n;report["input"]=fixture.at("spectrum").at("input").at(n);report["native_bits"]=uploaded[n];report["expected_bits"]=fixture.at("spectrum").at("expected").at(n);throw std::runtime_error("Spectrum half upload differs from reference");}
        current="dynamic map uniforms";std::array<DynamicMapState,4> states;std::array<wgpu::Texture,4> textures;std::array<DynamicMapDraw,5> draws;
        for(size_t i=0;i<states.size();++i){const auto& item=fixture.at("cases").at(i);states[i]=maps.create(item.at("mode"),item.at("parameters").get<std::vector<float>>());
            for(const auto& reference:item.at("uniforms")){const auto data=GpuDynamicMaps::uniformData(states[i],reference.at("time"));require(data.size()==reference.at("data").size(),"Dynamic uniform count differs");for(size_t n=0;n<data.size();++n){require(data[n]==reference.at("data").at(n).get<float>(),"Dynamic uniform field differs");++uniformComponents;}}
            textures[i]=gpuTexture(context,kWarpMapSize,kWarpMapSize,wgpu::TextureFormat::RG16Float,wgpu::TextureUsage::RenderAttachment|wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::CopySrc);draws[i]={&states[i],textures[i].CreateView()};}
        draws[4]=draws[0];std::array<std::vector<uint16_t>,4> quiet,audible,animated;
        for(unsigned frame=0;frame<4;++frame){current="dynamic frame "+std::to_string(frame);
            if(frame==0)spectrum.fill(0);else for(size_t n=0;n<spectrum.size();++n)spectrum[n]=float(.02+.8*std::pow(std::sin(n*.021),2));maps.uploadSpectrum(spectrum);
            const double time=frame<2?0:7.25;auto encoder=context.device.CreateCommandEncoder();maps.encode(encoder,draws,time);const auto commands=encoder.Finish();context.queue.Submit(1,&commands);
            require(maps.noiseSeed()==fixture.at("initial_seed").get<uint32_t>()+frame+1,"Dynamic noise advanced more than once per frame");noiseCheck("dynamic_"+std::to_string(frame+1),maps.dynamicNoise2d());
            for(size_t i=0;i<states.size();++i){auto values=readHalfTexture(context,textures[i]);float low=INFINITY,high=-INFINITY;
                for(auto bits:values){const float value=visualizers::gpu::decodeHalf(bits);require(std::isfinite(value),"Nonfinite dynamic motion pixel");low=std::min(low,value);high=std::max(high,value);++finiteComponents;}
                if(frame==0)quiet[i]=values;else if(frame==1)audible[i]=values;else if(frame==2)animated[i]=values;else require(values==animated[i],"A repeated time/spectrum frame changed a static-noise motion");
                if(frame==0||frame==2)raw(output/("mode-"+std::to_string(states[i].mode)+"-frame-"+std::to_string(frame)+".rg16le"),values);
                frames.push_back({{"frame",frame},{"mode",states[i].mode},{"checksum",checksum(values)},{"minimum",low},{"maximum",high}});
            }
        }
        for(size_t i=0;i<3;++i){require(quiet[i]==audible[i],"Non-audio motion changed with spectrum only");require(audible[i]!=animated[i],"Time-dependent motion did not animate");}
        require(quiet[3]!=audible[3]&&audible[3]==animated[3],"Spectrum motion did not follow audio independently of time");
        current="empty frame and copy layouts";const auto seed=maps.noiseSeed();auto empty=context.device.CreateCommandEncoder();maps.encode(empty,{},0);const auto emptyCommands=empty.Finish();context.queue.Submit(1,&emptyCommands);require(maps.noiseSeed()==seed,"Empty dynamic frame changed noise");
        {
            GpuDynamicMaps wrap(context,bank,0xffffffffu);const auto state=wrap.create(35,states[0].parameters);
            const std::array batch={DynamicMapDraw{&state,textures[0].CreateView()}};auto encoder=context.device.CreateCommandEncoder();wrap.encode(encoder,batch,0);const auto commands=encoder.Finish();context.queue.Submit(1,&commands);
            require(wrap.noiseSeed()==0,"Dynamic noise seed did not wrap as uint32");noiseCheck("wrap_dynamic_0",wrap.dynamicNoise2d());
        }
        for(const auto format:{wgpu::TextureFormat::R16Float,wgpu::TextureFormat::RG16Float,wgpu::TextureFormat::RGBA16Float}){
            const unsigned channels=format==wgpu::TextureFormat::R16Float?1:format==wgpu::TextureFormat::RG16Float?2:4;const unsigned depth=channels==4?2:1;
            auto texture=gpuTexture(context,13,7,format,wgpu::TextureUsage::CopyDst|wgpu::TextureUsage::CopySrc,depth,depth>1?wgpu::TextureDimension::e3D:wgpu::TextureDimension::e2D);
            std::vector<uint16_t> values(13*7*channels*depth);for(size_t n=0;n<values.size();++n)values[n]=visualizers::gpu::encodeHalf(float(n%31)*.0625f);uploadHalfTexture(context,texture,values);require(readHalfTexture(context,texture)==values,"Padded row/layer half-texture copy changed bits");}
        current="invalid requests";rejects([&]{maps.create(42,states[0].parameters);});rejects([&]{maps.create(35,std::span<const float>{});});rejects([&]{maps.uploadSpectrum({});});
        auto bad=states[0].parameters;bad[0]=INFINITY;rejects([&]{maps.create(35,bad);});rejects([&]{GpuDynamicMaps::uniformData(states[0],std::numeric_limits<double>::infinity());});
        auto invalidEncoder=context.device.CreateCommandEncoder();const std::array<DynamicMapDraw,21> tooMany{};
        rejects([&]{maps.encode(invalidEncoder,tooMany,0);});rejects([&]{maps.encode(invalidEncoder,std::span(draws).first(1),INFINITY);});require(maps.noiseSeed()==seed,"Rejected dynamic draw modified the seed");
        rejects([&]{bank.compute("warp");});rejects([&]{bank.render("warp",wgpu::TextureFormat::R16Float);});rejects([&]{bank.compute("../noise_2d");});
        rejects([&]{gpuBuffer(context,3,wgpu::BufferUsage::Uniform);});rejects([&]{gpuTexture(context,0,1,wgpu::TextureFormat::RGBA16Float,wgpu::TextureUsage::CopyDst);});
        context.checkErrors();report["result"]="pass";report["noise_hash_components_exact"]=noiseComponents;report["noise_storage_components_verified"]=noiseComponents;report["noise_storage_rounding"]=noiseResults.at(0).at("storage_rounding");report["uniform_components_exact"]=uniformComponents;report["spectrum_components_exact"]=uploaded.size();report["finite_motion_components_checked"]=finiteComponents;
        report["time_audio_and_repeat_checks"]="pass";report["duplicate_empty_and_wrapping_frame_checks"]="pass";report["padded_copy_and_invalid_request_checks"]="pass";report["noise"]=std::move(noiseResults);report["frames"]=std::move(frames);
        report["scope"]="Native procedural noise and four dynamic motion passes; full reference image parity remains open";
        std::ofstream(output/"result.json")<<report.dump(2)<<'\n';auto summary=report;summary.erase("noise");summary.erase("frames");std::cout<<summary.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report["result"]="fail";report["case"]=current;report["error"]=error.what();std::error_code ignored;fs::create_directories(output,ignored);std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
