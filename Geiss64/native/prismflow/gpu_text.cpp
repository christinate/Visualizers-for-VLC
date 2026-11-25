// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#include "gpu_text.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace prismflow {
namespace {
float scalar(double v){if(!std::isfinite(v)||std::abs(v)>std::numeric_limits<float>::max())throw std::invalid_argument("Invalid text overlay scalar");return float(v);}
}
GpuText::GpuText(GpuContext& context,GpuAssets& assets):context_(context){
    pipeline_=assets.render("text_overlay",wgpu::TextureFormat::RGBA16Float);uniforms_=gpuBuffer(context_,32,wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst);
    wgpu::SamplerDescriptor sampler{};sampler.minFilter=sampler.magFilter=wgpu::FilterMode::Linear;sampler.mipmapFilter=wgpu::MipmapFilterMode::Nearest;sampler_=context_.device.CreateSampler(&sampler);
    dummy_=gpuTexture(context_,1,1,wgpu::TextureFormat::RGBA8Unorm,wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::CopyDst);dummyView_=dummy_.CreateView();context_.checkErrors();
}
void GpuText::uploadMask(uint32_t width,uint32_t height,uint32_t ss,std::span<const uint8_t> rgba){
    if(!ss||ss>8||!width||!height||uint64_t(width)*height>16777216||rgba.size()!=uint64_t(width)*height*4)throw std::invalid_argument("Invalid text mask dimensions/scale/byte count");
    auto texture=mask_;auto view=maskView_;auto group=bindings_;
    if(!texture||texture.GetWidth()!=width||texture.GetHeight()!=height){texture=gpuTexture(context_,width,height,wgpu::TextureFormat::RGBA8Unorm,wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::CopyDst|wgpu::TextureUsage::CopySrc);view=texture.CreateView();const std::array e={bindTexture(0,view),bindSampler(1,sampler_),bindBuffer(2,uniforms_)};group=gpuBindings(context_,pipeline_.GetBindGroupLayout(0),e);}
    wgpu::TexelCopyTextureInfo target{};target.texture=texture;wgpu::TexelCopyBufferLayout layout{};layout.bytesPerRow=width*4;layout.rowsPerImage=height;const wgpu::Extent3D extent{width,height,1};context_.queue.WriteTexture(&target,rgba.data(),rgba.size(),&layout,&extent);context_.checkErrors();
    mask_=std::move(texture);maskView_=std::move(view);bindings_=std::move(group);width_=double(width)/ss;height_=double(height)/ss;has_=true;
}
void GpuText::clear(){has_=false;width_=height_=0;enabled_=false;}
void GpuText::showTimed(double x,double y,double strength,const TextTiming& timing){
    if(!has_){enabled_=false;return;}scalar(x);scalar(y);scalar(strength);if(!std::isfinite(timing.start)||!std::isfinite(timing.end)||!std::isfinite(timing.power)||timing.end<=timing.start||timing.power<0)throw std::invalid_argument("Invalid text overlay timing");
    centerX_=x;centerY_=y;intensity_=strength;timing_=timing;enabled_=true;
}
void GpuText::showImmediate(double x,double y,double strength){if(!has_){enabled_=false;return;}scalar(x);scalar(y);scalar(strength);centerX_=x;centerY_=y;intensity_=strength;timing_.reset();enabled_=true;}
double GpuText::intensity(double time)const{
    if(!std::isfinite(time))throw std::invalid_argument("Text overlay time must be finite");if(!timing_)return intensity_;if(timing_->start<0)return 0;
    const double t=std::clamp((time-timing_->start)/(timing_->end-timing_->start),0.,1.);return intensity_*std::pow(t,timing_->power);
}
TextRect GpuText::rect(double time)const{const double w=has_?width_:0,h=has_?height_:0;return {enabled_&&has_,{centerX_-w*.5,centerY_-h*.5,w,h,intensity(time)}};}
std::array<float,8> GpuText::uniformData(uint32_t width,uint32_t height,double x,double y,double strength,double w,double h){
    if(!width||!height||uint64_t(width)*height>16777216||!std::isfinite(w)||!std::isfinite(h)||w<0||h<0)throw std::invalid_argument("Invalid text stamp dimensions");return {scalar(x-w*.5),scalar(y-h*.5),scalar(w),scalar(h),float(width),float(height),scalar(strength),0};
}
void GpuText::draw(const wgpu::CommandEncoder& encoder,const wgpu::TextureView& target,uint32_t width,uint32_t height,double x,double y,double strength){
    if(!has_)return;if(!encoder||!target)throw std::invalid_argument("Text stamp requires valid command/target handles");const auto data=uniformData(width,height,x,y,strength,width_,height_);context_.queue.WriteBuffer(uniforms_,0,data.data(),sizeof(data));
    wgpu::RenderPassColorAttachment color{};color.view=target;color.loadOp=wgpu::LoadOp::Load;color.storeOp=wgpu::StoreOp::Store;wgpu::RenderPassDescriptor descriptor{};descriptor.colorAttachmentCount=1;descriptor.colorAttachments=&color;auto pass=encoder.BeginRenderPass(&descriptor);pass.SetPipeline(pipeline_);pass.SetBindGroup(0,bindings_);pass.Draw(6);pass.End();
}
}
