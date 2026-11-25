// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#include "gpu_present.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace prismflow {
namespace {
float scalar(double v){if(!std::isfinite(v)||std::abs(v)>std::numeric_limits<float>::max())throw std::invalid_argument("Nonfinite or excessive presentation value");return float(v);}
void validate(const PresentationFrame& frame){
    if(!frame.width||!frame.height||uint64_t(frame.width)*frame.height>16777216||!std::isfinite(frame.oversample)||frame.oversample<=0||frame.oversample>8||frame.swatchCount>4096||!std::isfinite(frame.headroom)||frame.headroom<1||frame.headroom>65504)
        throw std::invalid_argument("Invalid presentation dimensions/scale/headroom/swatch count");
}
}
GpuPresentation::GpuPresentation(GpuContext& context,GpuAssets& assets):context_(context),assets_(assets){
    uniforms_=gpuBuffer(context_,256,wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst);uiUniforms_=gpuBuffer(context_,64,wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst);
    constexpr auto usage=wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::CopyDst|wgpu::TextureUsage::CopySrc;
    palette_=gpuTexture(context_,256,1,wgpu::TextureFormat::RGBA8Unorm,usage);paletteView_=palette_.CreateView();
    transparent_=gpuTexture(context_,1,1,wgpu::TextureFormat::RGBA8Unorm,usage);transparentView_=transparent_.CreateView();
    wgpu::SamplerDescriptor sampler{};sampler.minFilter=sampler.magFilter=wgpu::FilterMode::Linear;sampler.mipmapFilter=wgpu::MipmapFilterMode::Nearest;titleSampler_=context_.device.CreateSampler(&sampler);
    // Dawn zero-initializes new textures, providing a transparent no-title view.
    context_.checkErrors();
}
std::array<uint8_t,1024> GpuPresentation::packPalette(std::span<const float> rgba){
    if(rgba.size()!=1024)throw std::invalid_argument("Presentation palette must contain 256 RGBA entries");std::array<uint8_t,1024> packed;
    for(size_t n=0;n<rgba.size();++n){if(!std::isfinite(rgba[n]))throw std::invalid_argument("Presentation palette must be finite");const double value=std::sqrt(std::clamp(double(rgba[n])*(1./3),0.,1.))*255;packed[n]=uint8_t(value+.5);}return packed;
}
void GpuPresentation::uploadPalette(std::span<const float> rgba){
    const auto bytes=packPalette(rgba);wgpu::TexelCopyTextureInfo target{};target.texture=palette_;wgpu::TexelCopyBufferLayout layout{};layout.bytesPerRow=1024;layout.rowsPerImage=1;const wgpu::Extent3D extent{256,1,1};
    context_.queue.WriteTexture(&target,bytes.data(),bytes.size(),&layout,&extent);
}
std::array<float,24> GpuPresentation::uniformData(const PresentationFrame& f){
    validate(f);std::array<float,24> data{};data[0]=scalar(f.oversample);data[1]=scalar(1./f.oversample);
    data[6]=float(f.width);data[7]=float(f.height);if(f.title){const auto& r=*f.title;if(r.width<0||r.height<0||r.intensity<0)throw std::invalid_argument("Invalid presentation title rectangle");
        data[4]=scalar(r.left);data[5]=scalar(r.top);data[6]=scalar(r.width);data[7]=scalar(r.height);data[10]=scalar(r.intensity);}
    data[8]=float(f.width);data[9]=float(f.height);data[12]=f.calibrating?1.f:0.f;data[13]=scalar(f.selectedSquare);data[14]=scalar(f.calibrationWidth);
    if(f.calibrating&&(f.selectedSquare<0||f.selectedSquare>13||f.calibrationWidth<=0||f.calibrationWidth>1||double(f.width)*f.calibrationWidth<14))throw std::invalid_argument("Invalid headroom calibration geometry");
    data[18]=scalar(f.layer1Density);data[19]=scalar(f.layer2Brightness);for(unsigned n=0;n<3;++n)data[20+n]=scalar(f.background[n]);data[23]=scalar(f.headroom);return data;
}
std::array<float,12> GpuPresentation::uiUniformData(const PresentationFrame& f){
    validate(f);std::array<float,12> data{float(f.width),float(f.height),float(f.swatchCount*32),32,8,8,float(f.swatchCount),-1,-1,-1,0,0};
    if(f.swatchBackground)for(unsigned n=0;n<3;++n)data[7+n]=scalar((*f.swatchBackground)[n]);return data;
}
void GpuPresentation::prepare(const wgpu::Texture& indices,const PresentationFrame& frame,wgpu::TextureFormat format,const wgpu::TextureView& title){
    const auto data=uniformData(frame);const auto ui=uiUniformData(frame);if(!indices||indices.GetFormat()!=wgpu::TextureFormat::RGBA16Float)throw std::invalid_argument("Presentation requires RGBA16F feedback");
    const float scale=float(frame.oversample);const auto samples=uint32_t(std::max(1.f,std::floor(scale+.5f)));
    const double lastX=std::floor(float(frame.width-1)*scale)+samples,lastY=std::floor(float(frame.height-1)*scale)+samples;
    if(lastX>indices.GetWidth()||lastY>indices.GetHeight())throw std::invalid_argument("Presentation supersamples exceed feedback dimensions");
    if(frame.title&&frame.title->intensity>0&&!title)throw std::invalid_argument("Active presentation title has no texture");
    auto pipeline=assets_.render("present",format);auto uiPipeline=assets_.render("palette_overlay",format);const auto input=indices.CreateView();
    const std::array entries={bindTexture(0,input),bindBuffer(1,uniforms_),bindTexture(2,paletteView_),bindTexture(3,title?title:transparentView_),bindSampler(4,titleSampler_)};
    auto bindings=gpuBindings(context_,pipeline.GetBindGroupLayout(0),entries);const std::array uiEntries={bindTexture(0,paletteView_),bindBuffer(1,uiUniforms_)};
    auto uiBindings=gpuBindings(context_,uiPipeline.GetBindGroupLayout(0),uiEntries);context_.checkErrors();
    context_.queue.WriteBuffer(uniforms_,0,data.data(),sizeof(data));context_.queue.WriteBuffer(uiUniforms_,0,ui.data(),sizeof(ui));
    pipeline_=std::move(pipeline);uiPipeline_=std::move(uiPipeline);bindings_=std::move(bindings);uiBindings_=std::move(uiBindings);swatches_=frame.swatchCount;prepared_=true;
}
void GpuPresentation::draw(const wgpu::RenderPassEncoder& pass)const{if(!prepared_)throw std::logic_error("Presentation needs a prepared frame");pass.SetPipeline(pipeline_);pass.SetBindGroup(0,bindings_);pass.Draw(3);}
void GpuPresentation::drawPalette(const wgpu::RenderPassEncoder& pass)const{if(!prepared_)throw std::logic_error("Palette overlay needs a prepared frame");if(swatches_){pass.SetPipeline(uiPipeline_);pass.SetBindGroup(0,uiBindings_);pass.Draw(6);}}
}
