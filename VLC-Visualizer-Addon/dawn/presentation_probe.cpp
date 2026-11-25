// SPDX-License-Identifier: GPL-2.0-or-later
#include "context.hpp"
#include "presentation.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <filesystem>
using namespace visualizers::gpu;
using json=nlohmann::json;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
    json report;std::filesystem::path output;
    try{
        bool software=false;
        for(int n=1;n<argc;++n){const std::string option=argv[n];
            if(option=="--output"&&n+1<argc)output=argv[++n];
            else if(option=="--software")software=true;
            else throw std::runtime_error("Unknown presentation probe argument");
        }
        constexpr auto sdr=wgpu::TextureFormat::BGRA8Unorm;
        struct Case {bool requested,enabled,transport,float16,expected;};
        unsigned policyCases=0;
        for(const auto& test:std::array<Case,9>{{
            {false,false,false,false,false},{false,true,true,true,false},
            {true,false,false,false,false},{true,false,true,true,false},
            {true,true,false,true,false},{true,true,true,false,false},
            {true,true,true,true,true},{true,false,true,false,false},
            {true,true,false,false,false}}}){
            const auto choice=choosePresentation(test.requested,{test.enabled,test.transport,"Unavailable in fixture"},test.float16,sdr);
            require(choice.hdr==test.expected,"HDR capability policy selected the wrong mode");
            require(choice.format==(test.expected?wgpu::TextureFormat::RGBA16Float:sdr),"HDR policy selected the wrong surface format");
            require(!choice.reason.empty(),"Presentation choice has no explanation");++policyCases;
        }
        for(bool requested:{false,true}){
            const auto choice=choosePresentation(requested,{true,true,"Fixture HDR"},true,sdr,false);
            require(!choice.hdr&&choice.format==sdr,"SDR-only plugin did not fall back on an HDR display");++policyCases;
        }
        for(bool requested:{false,true})for(bool transport:{false,true})
        for(bool float16:{false,true})for(bool linearRenderer:{false,true}){
            const DisplayPresentation display{false,transport,"Fixture Wayland",false,1,1,true};
            const auto choice=choosePresentation(requested,display,float16,sdr,linearRenderer);
            const bool expected=requested&&transport&&float16&&linearRenderer;
            require(choice.hdr==expected,"Wayland HDR policy selected the wrong mode");
            require(choice.format==(expected?wgpu::TextureFormat::RGBA16Float:sdr),
                    "Wayland HDR policy selected the wrong surface format");
            require(presentationWhiteScale(display)==scRgbWhiteScale,
                    "Wayland HDR did not use the scRGB white scale");
            ++policyCases;
        }
        for(bool requested:{false,true})for(bool enabled:{false,true})
        for(bool transport:{false,true})for(bool float16:{false,true}){
            DisplayPresentation display{enabled,transport,"Fixture EDR",true,enabled?4.f:1.f,4.f};
            const auto choice=choosePresentation(requested,display,float16,sdr);
            const bool linear=requested&&transport&&float16;
            require(choice.hdr==(linear&&enabled),"EDR active state ignored current headroom");
            require(choice.format==(linear?wgpu::TextureFormat::RGBA16Float:sdr),"EDR headroom-one fallback lost its linear surface");
            require(presentationOutputMode(choice,linear)==(linear?(enabled?1u:3u):0u),"EDR selected the wrong output transfer");
            require(presentationWhiteScale(display)==1.f,"EDR used Windows' absolute scRGB white scale");++policyCases;
        }
        for(bool requested:{false,true}){
            const auto choice=choosePresentation(requested,{true,true,"Fixture EDR",true,4,4},true,sdr,false);
            require(!choice.hdr&&choice.format==sdr,"SDR-only plugin enabled EDR");++policyCases;
        }
        Context context(wgpu::BackendType::Undefined,software);report=context.info();
        const auto shader=context.shader(presentationShader);
        const auto renderer=pipeline(context,shader,"vertex","fragment");
        wgpu::TextureDescriptor sourceDescriptor{};sourceDescriptor.size={1,1,1};sourceDescriptor.format=wgpu::TextureFormat::RGBA16Float;
        sourceDescriptor.usage=wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::CopyDst;
        auto source=context.device.CreateTexture(&sourceDescriptor);auto target=context.target(8,8);
        wgpu::SamplerDescriptor samplerDescriptor{};samplerDescriptor.minFilter=samplerDescriptor.magFilter=wgpu::FilterMode::Linear;
        auto sampler=context.device.CreateSampler(&samplerDescriptor);
        wgpu::BufferDescriptor bufferDescriptor{};bufferDescriptor.size=16;bufferDescriptor.usage=wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst;
        const auto buffer=context.device.CreateBuffer(&bufferDescriptor);
        std::array<wgpu::BindGroupEntry,3> entries{};
        entries[0].binding=0;entries[0].textureView=source.CreateView();entries[1].binding=1;entries[1].sampler=sampler;
        entries[2].binding=2;entries[2].buffer=buffer;entries[2].size=16;
        wgpu::BindGroupDescriptor group{};group.layout=renderer.GetBindGroupLayout(0);group.entryCount=entries.size();group.entries=entries.data();
        const auto bindings=context.device.CreateBindGroup(&group);
        uint64_t components=0;double maximumError=0;float maximumHdr=0;
        for(uint32_t mode=0;mode<5;++mode)for(float sample:std::array<float,10>{-65504,-.25f,0,.03125f,.25f,.5f,1,2,8,65504}){
            const std::array<uint16_t,4> pixel={encodeHalf(sample),encodeHalf(sample),encodeHalf(sample),encodeHalf(1)};
            wgpu::TexelCopyTextureInfo destination{};destination.texture=source;
            wgpu::TexelCopyBufferLayout layout{};layout.bytesPerRow=8;layout.rowsPerImage=1;const wgpu::Extent3D extent{1,1,1};
            context.queue.WriteTexture(&destination,pixel.data(),sizeof(pixel),&layout,&extent);
            struct Output {uint32_t mode;float whiteScale;float reserved[2];};const Output uniform{mode==4?1u:mode,mode==4?1.f:scRgbWhiteScale,{}};
            context.queue.WriteBuffer(buffer,0,&uniform,sizeof(uniform));render(context,renderer,target.CreateView(),bindings);
            const auto actual=context.readRgba16(target,8,8);
            const auto nonnegative=std::max(0.f,sample);
            const double expected=mode==0||mode==4?sample:mode==1?std::clamp(sample*scRgbWhiteScale,-65504.f,65504.f):
                mode==3?nonnegative/(1+nonnegative):std::pow(nonnegative/(1+nonnegative),1./2.2);
            for(size_t n=0;n<actual.size();++n){const float value=decodeHalf(actual[n]);
                const double wanted=n%4==3?1:expected;const auto error=std::abs(value-wanted);
                require(std::isfinite(value)&&error<=std::max(.0006,std::abs(wanted)*.001),"Presentation GPU conversion exceeded fixed numerical limits");
                if(mode==0)require(actual[n]==pixel[n%4],"SDR copy changed source channel bits");
                if(mode==4)require(actual[n]==pixel[n%4],"EDR reference-white conversion changed source channel bits");
                if(mode==1&&n%4!=3)maximumHdr=std::max(maximumHdr,value);
                maximumError=std::max(maximumError,error);++components;
            }
        }
        require(maximumHdr>20,"HDR conversion clipped extended-range values");context.checkErrors();
        report["result"]="pass";report["capability_policy_cases"]=policyCases;report["gpu_conversion_cases"]=50;
        report["components"]=components;report["maximum_absolute_error"]=maximumError;report["maximum_hdr_channel"]=maximumHdr;
        report["sdr_copy"]="exact half-float channel bits";
        report["scope"]="Capability decisions and offscreen SDR/scRGB/EDR/fallback shader conversions; no physical HDR display claim";
    }catch(const std::exception& error){report["result"]="fail";report["error"]=error.what();}
    if(!output.empty()){std::filesystem::create_directories(output.parent_path());std::ofstream(output)<<report.dump(2)<<'\n';}
    std::cout<<report.dump(2)<<'\n';return report.value("result","")=="pass"?0:1;
}
