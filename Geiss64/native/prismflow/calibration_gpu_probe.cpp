// SPDX-License-Identifier: GPL-2.0-or-later
#include "gpu_present.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
using namespace prismflow;
using json=nlohmann::json;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){fs::path assets,output;json report;uint64_t channels=0,exact=0;float maximumError=0;
    try{
        wgpu::BackendType backend=wgpu::BackendType::Undefined;
        for(int n=1;n<argc;++n){const std::string option=argv[n];
            if(option=="--assets"&&n+1<argc)assets=argv[++n];
            else if(option=="--output"&&n+1<argc)output=argv[++n];
            else if(option=="--backend"&&n+1<argc){const std::string value=argv[++n];
                if(value=="d3d12")backend=wgpu::BackendType::D3D12;
                else if(value=="vulkan")backend=wgpu::BackendType::Vulkan;
                else if(value=="metal")backend=wgpu::BackendType::Metal;
                else throw std::runtime_error("Unknown calibration backend");
            }else throw std::runtime_error("Unknown calibration argument");
        }
        require(!assets.empty()&&!output.empty(),"Expected --assets and --output");
        GpuContext context(backend);GpuAssets bank(context,assets);GpuPresentation presenter(context,bank);report=context.info();
        presenter.uploadPalette(std::vector<float>(1024,0));
        constexpr std::array<float,14> levels={1,1.1f,1.2f,1.3f,1.4f,1.5f,1.65f,1.8f,2,2.2f,2.4f,2.6f,2.8f,3};
        struct Geometry {uint32_t width,height;double fraction;};
        unsigned cases=0;
        for(const auto& geometry:std::array<Geometry,4>{{{448,96,1},{896,192,.85},{512,200,.5},{320,128,.85}}}){
            const auto input=gpuTexture(context,geometry.width,geometry.height,wgpu::TextureFormat::RGBA16Float,wgpu::TextureUsage::TextureBinding);
            const auto target=gpuTexture(context,geometry.width,geometry.height,wgpu::TextureFormat::RGBA16Float,wgpu::TextureUsage::RenderAttachment|wgpu::TextureUsage::CopySrc);
            const auto size=std::floor(float(geometry.width)*float(geometry.fraction)*(1.f/14.f));
            const float left=float(geometry.width)*.5f-size*7,top=float(geometry.height)*.5f-size*.5f;
            require(size>=16,"Calibration fixture squares are too small");
            for(unsigned selected=0;selected<levels.size();++selected){
                PresentationFrame frame;frame.width=geometry.width;frame.height=geometry.height;
                frame.calibrating=true;frame.calibrationWidth=geometry.fraction;frame.selectedSquare=selected;
                presenter.prepare(input,frame,wgpu::TextureFormat::RGBA16Float);
                const auto encoder=context.device.CreateCommandEncoder();wgpu::RenderPassColorAttachment color{};
                color.view=target.CreateView();color.loadOp=wgpu::LoadOp::Clear;color.storeOp=wgpu::StoreOp::Store;
                wgpu::RenderPassDescriptor descriptor{};descriptor.colorAttachmentCount=1;descriptor.colorAttachments=&color;
                auto pass=encoder.BeginRenderPass(&descriptor);presenter.draw(pass);pass.End();
                const auto commands=encoder.Finish();context.queue.Submit(1,&commands);
                const auto pixels=readHalfTexture(context,target);
                auto sample=[&](unsigned square,float fractionX,const std::array<float,4>& expected,const char* type){
                    const auto x=uint32_t(std::floor(left+square*size+size*fractionX));
                    const auto y=uint32_t(std::floor(top+size*.5f));
                    for(unsigned c=0;c<4;++c){const auto actual=pixels[(uint64_t(y)*geometry.width+x)*4+c];
                        const auto wanted=visualizers::gpu::encodeHalf(expected[c]);const float value=visualizers::gpu::decodeHalf(actual);
                        // Decimal levels/subtraction may differ by one half-float
                        // step between GPU conversions. Binary-exact endpoints
                        // and the selection border must still match exactly.
                        const bool exactRequired=expected[c]==0||expected[c]==1.5f||expected[c]==3||expected[c]==2.75f;
                        const float tolerance=exactRequired?0:std::max(
                            visualizers::gpu::decodeHalf(wanted+1)-visualizers::gpu::decodeHalf(wanted),
                            visualizers::gpu::decodeHalf(wanted)-visualizers::gpu::decodeHalf(wanted-1));
                        const auto difference=std::abs(value-expected[c]);maximumError=std::max(maximumError,difference);
                        if(!std::isfinite(value)||difference>tolerance){
                            report["mismatch"]={{"width",geometry.width},{"height",geometry.height},{"selected_square",selected},
                                {"square",square},{"sample",type},{"channel",c},{"expected",expected[c]},
                                {"actual",value}};
                            throw std::runtime_error("Calibration channel differs from its declared brightness or selection border");
                        }++channels;exact+=actual==wanted;
                    }
                };
                for(unsigned square=0;square<levels.size();++square){
                    const auto value=levels[square],circle=value-.25f;
                    sample(square,.1f,{value,value,value,value},"background");
                    sample(square,.5f,{circle,circle,circle,circle},"contrast circle");
                }
                sample(selected,0,{1.5f,0,1.5f,0},"selected border");++cases;
            }
        }
        context.checkErrors();report["result"]="pass";report["cases"]=cases;report["channels_checked"]=channels;report["channels_exact"]=exact;
        report["maximum_absolute_error"]=maximumError;report["limits"]="One half-float step for decimal arithmetic; exact endpoints 3.0/2.75 and selection border";
        report["declared_levels"]=levels;report["last_square_background"]=3;report["last_square_circle"]=2.75;
        report["scope"]="All 14 calibration levels, contrast circles and selected borders at four geometries in raw RGBA16F; no physical HDR claim";
    }catch(const std::exception& error){report["result"]="fail";report["error"]=error.what();report["channels_before_failure"]=channels;}
    if(!output.empty()){fs::create_directories(output);std::ofstream(output/"result.json")<<report.dump(2)<<'\n';}
    std::cout<<report.dump(2)<<'\n';return report.value("result","")=="pass"?0:1;
}
