// SPDX-License-Identifier: GPL-2.0-or-later
#include "context.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <array>
#include <algorithm>
#include <cmath>
#include <numbers>
#ifdef VIS_HAVE_TERRAIN
#include "scene_dawn.hpp"
#endif
namespace fs=std::filesystem;using namespace visualizers::gpu;using json=nlohmann::json;
static constexpr const char* shader=R"(
@vertex fn vertex(@builtin(vertex_index) index:u32)->@builtin(position) vec4f {
 let points=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));return vec4f(points[index],0,1);
}
@fragment fn fragment()->@location(0) vec4f {return vec4f(.5,.25,2,1);}
)";
static uint64_t checksum(const std::vector<uint16_t>& pixels){uint64_t value=1469598103934665603ull;for(auto pixel:pixels){value^=pixel;value*=1099511628211ull;}return value;}
static void preview(const fs::path& file,const std::vector<uint16_t>& pixels,unsigned width,unsigned height){
    std::ofstream out(file,std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(size_t n=0;n<pixels.size();n+=4)for(unsigned channel=0;channel<3;++channel){const float decoded=decodeHalf(pixels[n+channel]);const char value=std::isfinite(decoded)?char(std::lround(std::clamp(decoded,0.f,1.f)*255)):char(channel==1?0:255);out.write(&value,1);}
}
static json auditPixels(const std::vector<uint16_t>& pixels,unsigned width,unsigned height,bool linear=false){
    std::array<float,4> low,high;low.fill(INFINITY);high.fill(-INFINITY);
    unsigned nonfinite=0,white=0,badAlpha=0,outOfRange=0;json examples=json::array();
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        const size_t offset=(size_t(y)*width+x)*4;bool bad=false,bright=true;
        for(unsigned channel=0;channel<4;++channel){
            const float value=decodeHalf(pixels[offset+channel]);
            if(!std::isfinite(value)){++nonfinite;bad=true;}
            else {low[channel]=std::min(low[channel],value);high[channel]=std::max(high[channel],value);}
            if(channel<3&&std::isfinite(value)&&(value<0||(!linear&&value>1))){++outOfRange;bad=true;}
            if(channel<3&&(!std::isfinite(value)||value<.98f))bright=false;
        }
        if(pixels[offset+3]!=0x3c00){++badAlpha;bad=true;}
        if(bright){++white;if(!linear)bad=true;}
        if(bad&&examples.size()<32)examples.push_back({{"x",x},{"y",y},{"rgba16_bits",{pixels[offset],pixels[offset+1],pixels[offset+2],pixels[offset+3]}}});
    }
    return {{"nonfinite_channels",nonfinite},{"white_pixels",white},{"nonopaque_pixels",badAlpha},{"out_of_range_channels",outOfRange},
            {"minimum",low},{"maximum",high},{"examples",examples},{"checksum",checksum(pixels)}};
}
static void raw(const fs::path& file,const std::vector<uint16_t>& pixels){
    // Preserve GPU bits; an 8-bit preview can conceal NaNs and infinities.
    std::ofstream out(file,std::ios::binary);for(uint16_t value:pixels){const char bytes[]={char(value&255),char(value>>8)};out.write(bytes,2);}
}
int main(int argc,char** argv){
    fs::path output="dawn-probe",terrainShader;wgpu::BackendType backend=wgpu::BackendType::Undefined;bool software=false,terrainAudit=false;
    try{
        for(int n=1;n<argc;++n){std::string argument=argv[n];
            if(argument=="--output-dir"&&n+1<argc)output=argv[++n];
            else if(argument=="--terrain-shader"&&n+1<argc)terrainShader=argv[++n];
            else if(argument=="--software")software=true;
            else if(argument=="--terrain-audit")terrainAudit=true;
            else if(argument=="--backend"&&n+1<argc){std::string value=argv[++n];if(value=="d3d12")backend=wgpu::BackendType::D3D12;else if(value=="metal")backend=wgpu::BackendType::Metal;else if(value=="vulkan")backend=wgpu::BackendType::Vulkan;else if(value=="opengl")backend=wgpu::BackendType::OpenGL;else throw std::runtime_error("Unknown backend");}
            else throw std::runtime_error("Unknown argument: "+argument);
        }
        if(terrainAudit&&terrainShader.empty())throw std::runtime_error("--terrain-audit requires --terrain-shader");
        fs::create_directories(output);Context context(backend,software);
        auto report=context.info();report["result"]="pass";report["runtime"]="native Dawn; no browser or Node";
        auto module=context.shader(shader);auto graphics=pipeline(context,module,"vertex","fragment");
        auto texture=context.target(64,64);render(context,graphics,texture.CreateView());
        auto pixels=context.readRgba16(texture,64,64);std::array<uint16_t,4> expected={0x3800,0x3400,0x4000,0x3c00};
        for(size_t n=0;n<pixels.size();++n)if(pixels[n]!=expected[n%4])throw std::runtime_error("RGBA16F readback did not preserve the expected pixel value");
        if(encodeHalf(1.9999f)!=0x4000||decodeHalf(0x4000)!=2.f)throw std::runtime_error("Half-float conversion regression");
        report["rgba16f"]={{"pixels_checked",4096},{"channels",{.5,.25,2.,1.}},{"above_one_preserved",true}};
        if(!terrainShader.empty()){
#ifdef VIS_HAVE_TERRAIN
            if(fs::file_size(terrainShader)>4*1024*1024)throw std::runtime_error("Terrain shader exceeds the prototype size limit");
            std::ifstream input(terrainShader,std::ios::binary);std::string source((std::istreambuf_iterator<char>(input)),{});
            constexpr unsigned width=192,height=128;terrain::DawnScene scene(context,source,width,height);
            std::vector<uint16_t> history(256*128*4);
            auto fill=[&](float energy){for(unsigned y=0;y<128;++y)for(unsigned x=0;x<256;++x){const size_t n=(y*256+x)*4;float wave=std::sin(float(x)*.09f+float(y)*.04f)*energy;history[n]=encodeHalf(wave);history[n+1]=encodeHalf(energy*(.5f+.4f*std::sin(float(y)*.17f)));history[n+2]=encodeHalf(std::abs(wave));history[n+3]=encodeHalf(energy*.2f);}scene.setHistory(history,.5f/128);};
            fill(.7f);json modes=json::array();uint64_t audible=0;
            for(unsigned mode=0;mode<4;++mode){const double time=30+60*mode;auto actual=scene.draw(time,.8f,.2f,8);if(actual!=mode)throw std::runtime_error("Terrain camera mode mismatch");
                auto frame=context.readRgba16(scene.texture(),width,height);float low=1e9f,high=-1e9f;
                for(size_t n=0;n<frame.size();n+=4){const float value=decodeHalf(frame[n]);if(!std::isfinite(value))throw std::runtime_error("Nonfinite Terrain pixel");low=std::min(low,value);high=std::max(high,value);}
                if(high-low<.02f)throw std::runtime_error("Terrain output lacks expected image variation");
                if(!mode)audible=checksum(frame);preview(output/("terrain-"+std::to_string(mode)+".ppm"),frame,width,height);
                modes.push_back({{"mode",mode},{"checksum",checksum(frame)},{"red_range",{low,high}}});
            }
            fill(0);scene.draw(30,0,0,8);auto quiet=context.readRgba16(scene.texture(),width,height);
            if(checksum(quiet)==audible)throw std::runtime_error("Terrain did not respond to changed waveform history");
            report["terrain"]={{"wgsl","existing Terrain source"},{"cameras",modes},{"waveform_changes_image",true}};
            if(terrainAudit){
                constexpr unsigned auditWidth=192,auditHeight=108;
                terrain::DawnScene animated(context,source,auditWidth,auditHeight);visualizers::MusicAnalysis analysis;
                std::array<float,4096> pcm{};json frames=json::array();unsigned failures=0;
                for(unsigned index=0;index<128;++index){
                    const bool linear=index>=96;if(index==96)animated.setOutput(true,1000);
                    const double time=double(index)/20.;
                    for(unsigned n=0;n<2048;++n){const double t=time+n/44100.,pulse=std::exp(-std::fmod(t,.5)*20),amplitude=.08+.7*pulse;
                        pcm[n*2]=float(amplitude*std::sin(t*std::numbers::pi*2*110));pcm[n*2+1]=float(amplitude*std::sin(t*std::numbers::pi*2*165));}
                    const auto& audio=analysis.update(pcm,time);const auto camera=animated.update(audio,time*180);
                    const auto image=context.readRgba16(animated.texture(),auditWidth,auditHeight);
                    auto audit=auditPixels(image,auditWidth,auditHeight,linear);audit["index"]=index;audit["time"]=time*180;audit["camera"]=camera;audit["output"]=linear?"linear HDR intermediate":"SDR";
                    const bool bad=audit["nonfinite_channels"]!=0||audit["out_of_range_channels"]!=0||audit["nonopaque_pixels"]!=0;
                    if(bad)++failures;
                    if(index==0||index==95||index==127||(bad&&failures<=8)){
                        const auto name="audit-"+std::to_string(index);raw(output/(name+".rgba16le"),image);preview(output/(name+".ppm"),image,auditWidth,auditHeight);
                    }
                    frames.push_back(std::move(audit));
                }
                report["terrain_pixel_audit"]={{"stage","RGBA16F before presentation/compositor/remoting"},{"width",auditWidth},{"height",auditHeight},
                    {"frames",frames},{"failed_frames",failures},{"rgba16_endianness","little"}};
                if(failures)report["result"]="fail";
            }
#else
            throw std::runtime_error("Terrain repository was not available when configuring this probe");
#endif
        }
        std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';return report["result"]=="pass"?0:1;
    }catch(const std::exception& error){
        json report={{"result","fail"},{"error",error.what()}};
        std::error_code ignored;fs::create_directories(output,ignored);std::ofstream(output/"result.json")<<report.dump(2)<<'\n';std::cerr<<report.dump(2)<<'\n';return 1;
    }
}
