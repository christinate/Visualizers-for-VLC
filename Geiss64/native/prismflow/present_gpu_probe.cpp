// SPDX-License-Identifier: GPL-2.0-or-later
#include "gpu_present.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;using namespace prismflow;using json=nlohmann::json;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>static void rejects(F&& fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid presentation input accepted");}
template<class T>static std::vector<T> read(const fs::path& folder,const std::string& name){
    require(fs::path(name).filename()==fs::path(name),"Invalid presentation fixture filename");const auto file=folder/name;const auto size=fs::file_size(file);require(size<=16*1024*1024&&size%sizeof(T)==0,"Invalid presentation fixture size");
    std::vector<T> values(size/sizeof(T));std::ifstream input(file,std::ios::binary);input.read(reinterpret_cast<char*>(values.data()),std::streamsize(size));require(bool(input),"Cannot read presentation fixture");return values;
}
template<class T>static void dump(const fs::path& file,const std::vector<T>& values){
    std::ofstream stream(file,std::ios::binary);stream.write(reinterpret_cast<const char*>(values.data()),std::streamsize(values.size()*sizeof(T)));
    require(bool(stream),"Cannot write diagnostic presentation output");
}
static void diagnoseTitle(GpuContext& context,const fs::path& assets,const fs::path& output,
        const PresentationFrame& frame,const wgpu::TextureView& title){
    require(uint64_t(frame.width)*frame.height<=1024*1024,"Title diagnostic exceeds its pixel budget");
    require(fs::file_size(assets/"present.wgsl")<=4*1024*1024,"Diagnostic shader exceeds its source budget");
    std::ifstream source(assets/"present.wgsl",std::ios::binary);std::string code((std::istreambuf_iterator<char>(source)),{});
    const auto fragment=code.find("@fragment");require(fragment!=std::string::npos,"Missing presentation fragment shader");code.resize(fragment);
    code+=R"(
@fragment fn mask_main(@builtin(position) p:vec4f)->@location(0) vec4f {
 let uv=(p.xy-vec2f(u.title_left_px,u.title_top_px)*u.oversample.y)/
   max(vec2f(u.title_width_px,u.title_height_px)*u.oversample.y,vec2f(1));
 return vec4f(uv,overlay_title(p.xy),u.title_intensity);
})";
    auto shader=context.shader(code);auto renderer=visualizers::gpu::pipeline(context,shader,"vs_main","mask_main",wgpu::TextureFormat::RGBA32Float);
    auto uniforms=gpuBuffer(context,256,wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst);
    const auto data=GpuPresentation::uniformData(frame);context.queue.WriteBuffer(uniforms,0,data.data(),sizeof(data));
    wgpu::SamplerDescriptor descriptor{};descriptor.minFilter=descriptor.magFilter=wgpu::FilterMode::Linear;descriptor.mipmapFilter=wgpu::MipmapFilterMode::Nearest;
    const auto sampler=context.device.CreateSampler(&descriptor);
    const std::array entries={bindBuffer(1,uniforms),bindTexture(3,title),bindSampler(4,sampler)};
    const auto bindings=gpuBindings(context,renderer.GetBindGroupLayout(0),entries);
    auto target=gpuTexture(context,frame.width,frame.height,wgpu::TextureFormat::RGBA32Float,wgpu::TextureUsage::RenderAttachment|wgpu::TextureUsage::CopySrc);
    visualizers::gpu::render(context,renderer,target.CreateView(),bindings);
    const uint32_t row=(frame.width*16+255)&~255u;
    auto buffer=gpuBuffer(context,uint64_t(row)*frame.height,wgpu::BufferUsage::CopySrc|wgpu::BufferUsage::CopyDst);
    auto encoder=context.device.CreateCommandEncoder();wgpu::TexelCopyTextureInfo from{};from.texture=target;
    wgpu::TexelCopyBufferInfo to{};to.buffer=buffer;to.layout.bytesPerRow=row;to.layout.rowsPerImage=frame.height;
    const wgpu::Extent3D extent{frame.width,frame.height,1};encoder.CopyTextureToBuffer(&from,&to,&extent);
    const auto commands=encoder.Finish();context.queue.Submit(1,&commands);const auto bytes=readGpuBuffer(context,buffer);
    std::vector<uint8_t> packed(size_t(frame.width)*frame.height*16);
    for(uint32_t y=0;y<frame.height;++y)std::copy_n(bytes.data()+size_t(y)*row,frame.width*16,packed.data()+size_t(y)*frame.width*16);
    dump(output/"title-uv-alpha.f32le",packed);
}
static PresentationFrame frameData(const json& item){
    PresentationFrame f;f.width=item.at("width");f.height=item.at("height");f.oversample=item.at("oversample");f.background=item.at("background").get<Color>();
    f.layer1Density=item.at("shader").at("layer1_density");f.layer2Brightness=item.at("shader").at("layer2_brightness");const auto& c=item.at("calibration");f.calibrating=c.at("calibrating_hdr_headroom").get<int>()!=0;f.selectedSquare=c.at("selected_square_index");f.calibrationWidth=c.at("width_frac");
    f.swatchCount=item.at("swatches");if(!item.at("swatch_background").is_null())f.swatchBackground=item.at("swatch_background").get<Color>();
    if(!item.at("title").is_null()){const auto& r=item.at("title");f.title=PresentationTitle{r.at("left_px"),r.at("top_px"),r.at("width_px"),r.at("height_px"),r.at("intensity")};}return f;
}
int main(int argc,char** argv){fs::path assets,fixtures,output="prismflow-presentation";json report;size_t current=0;
    try{
        static_assert(std::endian::native==std::endian::little);bool exact=false,dumpOutput=false,titleDiagnostic=false;wgpu::BackendType backend=wgpu::BackendType::Undefined;
        for(int n=1;n<argc;++n){const std::string option=argv[n];if(option=="--assets"&&n+1<argc)assets=argv[++n];else if(option=="--fixtures"&&n+1<argc)fixtures=argv[++n];else if(option=="--output"&&n+1<argc)output=argv[++n];else if(option=="--exact")exact=true;
            else if(option=="--dump-output")dumpOutput=true;
            else if(option=="--diagnose-title"){titleDiagnostic=true;dumpOutput=true;}
            else if(option=="--backend"&&n+1<argc){const std::string value=argv[++n];if(value=="d3d12")backend=wgpu::BackendType::D3D12;else if(value=="metal")backend=wgpu::BackendType::Metal;else if(value=="vulkan")backend=wgpu::BackendType::Vulkan;else throw std::runtime_error("Unknown presentation backend");}else throw std::runtime_error("Unknown presentation argument");}
        require(!assets.empty()&&!fixtures.empty(),"Expected presentation assets and fixtures");require(fs::file_size(fixtures)<1024*1024,"Presentation manifest exceeds bound");std::ifstream manifest(fixtures);const auto fixture=json::parse(manifest);require(!fixture.at("frames").empty()&&fixture.at("frames").size()<=128,"Invalid presentation fixture count");
        fs::create_directories(output);GpuContext context(backend);GpuAssets bank(context,assets);GpuPresentation presentation(context,bank);report=context.info();report["reference_adapter"]=fixture.at("adapter");report["frames"]=json::array();
        const auto folder=fixtures.parent_path();const auto titleBytes=read<uint8_t>(folder,fixture.at("title_file"));require(titleBytes.size()==16*8*4,"Invalid synthetic title size");auto title=gpuTexture(context,16,8,wgpu::TextureFormat::RGBA8Unorm,wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::CopyDst);const auto titleView=title.CreateView();
        {wgpu::TexelCopyTextureInfo target{};target.texture=title;wgpu::TexelCopyBufferLayout layout{};layout.bytesPerRow=64;const wgpu::Extent3D extent{16,8,1};context.queue.WriteTexture(&target,titleBytes.data(),titleBytes.size(),&layout,&extent);}
        uint64_t packed=0,uniforms=0,components=0,different=0;double globalMax=0,totalError=0;
        for(const auto& item:fixture.at("frames")){
            const auto f=frameData(item);const auto data=GpuPresentation::uniformData(f);const auto ui=GpuPresentation::uiUniformData(f);for(size_t n=0;n<data.size();++n){require(data[n]==item.at("uniforms").at(n).get<float>(),"Presentation uniforms differ from original");++uniforms;}
            if(titleDiagnostic&&f.title)diagnoseTitle(context,assets,output,f,titleView);
            if(f.swatchCount)for(size_t n=0;n<ui.size();++n){require(ui[n]==item.at("ui_uniforms").at(n).get<float>(),"Palette overlay uniforms differ from original");++uniforms;}
            const auto palette=read<float>(folder,item.at("palette_file"));presentation.uploadPalette(palette);const auto expectedPacked=read<uint8_t>(folder,item.at("packed_file"));require(readByteTexture(context,presentation.paletteTexture())==expectedPacked,"Packed palette bytes differ from original upload");packed+=expectedPacked.size();
            const auto format=textureFormat(item.at("format"));auto input=gpuTexture(context,item.at("index_width"),item.at("index_height"),wgpu::TextureFormat::RGBA16Float,wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::CopyDst);uploadHalfTexture(context,input,read<uint16_t>(folder,item.at("input_file")));
            const auto target=gpuTexture(context,f.width,f.height,format,wgpu::TextureUsage::RenderAttachment|wgpu::TextureUsage::CopySrc);presentation.prepare(input,f,format,titleView);const auto encoder=context.device.CreateCommandEncoder();
            wgpu::RenderPassColorAttachment color{};color.view=target.CreateView();color.loadOp=wgpu::LoadOp::Clear;color.storeOp=wgpu::StoreOp::Store;color.clearValue={0,0,0,1};wgpu::RenderPassDescriptor descriptor{};descriptor.colorAttachmentCount=1;descriptor.colorAttachments=&color;
            auto pass=encoder.BeginRenderPass(&descriptor);presentation.draw(pass);presentation.drawPalette(pass);pass.End();const auto commands=encoder.Finish();context.queue.Submit(1,&commands);
            double maxError=0,sum=0;uint64_t changed=0,count=0;
            if(format==wgpu::TextureFormat::RGBA16Float){const auto actual=readHalfTexture(context,target),expected=read<uint16_t>(folder,item.at("file"));require(actual.size()==expected.size(),"Presentation output size differs");count=actual.size();
                if(dumpOutput)dump(output/("actual-"+item.at("file").get<std::string>()),actual);
                for(size_t n=0;n<actual.size();++n){const double a=visualizers::gpu::decodeHalf(actual[n]),b=visualizers::gpu::decodeHalf(expected[n]);require(std::isfinite(a)&&std::isfinite(b),"Presentation contains nonfinite channels");const double e=std::abs(a-b);maxError=std::max(maxError,e);sum+=e;changed+=actual[n]!=expected[n];}}
            else{const auto actual=readByteTexture(context,target),expected=read<uint8_t>(folder,item.at("file"));require(actual.size()==expected.size(),"Presentation byte output size differs");count=actual.size();
                if(dumpOutput)dump(output/("actual-"+item.at("file").get<std::string>()),actual);
                for(size_t n=0;n<actual.size();++n){const double e=std::abs(double(actual[n])-expected[n])/255.;maxError=std::max(maxError,e);sum+=e;changed+=actual[n]!=expected[n];}}
            const double mean=sum/count;report["frames"].push_back({{"scene",item.at("scene")},{"format",item.at("format")},{"components",count},{"differing_components",changed},{"maximum_absolute_error",maxError},{"mean_absolute_error",mean}});
            require(maxError<=.004&&mean<=.0002&&(!exact||!changed),"Presentation/reference numerical limits exceeded");globalMax=std::max(globalMax,maxError);totalError+=sum;components+=count;different+=changed;++current;
        }
        // Boundary guards are independent of image tolerances.
        auto bad=frameData(fixture.at("frames").at(0));bad.oversample=0;rejects([&]{GpuPresentation::uniformData(bad);});bad.oversample=INFINITY;rejects([&]{GpuPresentation::uniformData(bad);});
        bad=frameData(fixture.at("frames").at(0));bad.headroom=.5;rejects([&]{GpuPresentation::uniformData(bad);});bad=frameData(fixture.at("frames").at(0));bad.calibrating=true;bad.calibrationWidth=0;rejects([&]{GpuPresentation::uniformData(bad);});
        auto invalid=std::vector<float>(1024,0);invalid[7]=NAN;rejects([&]{presentation.uploadPalette(invalid);});rejects([&]{presentation.uploadPalette(std::span<const float>{});});
        auto input=gpuTexture(context,8,8,wgpu::TextureFormat::RGBA16Float,wgpu::TextureUsage::TextureBinding);bad=frameData(fixture.at("frames").at(0));rejects([&]{presentation.prepare(input,bad,wgpu::TextureFormat::RGBA16Float);});
        bad.width=bad.height=8;bad.title=PresentationTitle{0,0,8,8,1};rejects([&]{presentation.prepare(input,bad,wgpu::TextureFormat::RGBA16Float);});bad.title.reset();rejects([&]{presentation.prepare(input,bad,wgpu::TextureFormat::RG16Float);});context.checkErrors();
        report["result"]="pass";report["cases"]=current;report["exact_required"]=exact;report["packed_palette_bytes_exact"]=packed;report["uniform_components_exact"]=uniforms;report["pixel_components_compared"]=components;report["differing_components"]=different;report["maximum_absolute_error"]=globalMax;report["mean_absolute_error"]=totalError/components;report["invalid_inputs"]="pass";
        report["scope"]="Offscreen palette/supersampling/title-mask/calibration/swatch presentation only; no physical HDR or bokeh/font-rasterizer claim";std::ofstream(output/"result.json")<<report.dump(2)<<'\n';auto summary=report;summary.erase("frames");std::cout<<summary.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){report["result"]="fail";report["case"]=current;report["error"]=error.what();std::error_code ignored;fs::create_directories(output,ignored);std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;}
}
