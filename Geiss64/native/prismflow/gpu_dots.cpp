// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#include "gpu_dots.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
namespace prismflow {
namespace {
float scalar(double v){if(!std::isfinite(v)||std::abs(v)>std::numeric_limits<float>::max())throw std::invalid_argument("Invalid warp-dot scalar");return float(v);}
}
GpuDots::GpuDots(GpuContext& context,GpuAssets& assets,uint32_t capacity):context_(context),capacity_(capacity){
    if(!capacity||capacity>32768)throw std::invalid_argument("Dot capacity must be in [1,32768]");pipeline_=assets.render("dots",wgpu::TextureFormat::RGBA16Float);
    uniforms_=gpuBuffer(context_,16,wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst);corners_=gpuBuffer(context_,32,wgpu::BufferUsage::Vertex|wgpu::BufferUsage::CopyDst);
    instances_=gpuBuffer(context_,uint64_t(capacity)*16,wgpu::BufferUsage::Vertex|wgpu::BufferUsage::CopyDst);
    constexpr std::array<float,8> corners{-1,-1,1,-1,-1,1,1,1};context_.queue.WriteBuffer(corners_,0,corners.data(),sizeof(corners));const std::array entries={bindBuffer(0,uniforms_)};
    bindings_=gpuBindings(context_,pipeline_.GetBindGroupLayout(0),entries);context_.checkErrors();
}
void GpuDots::dimensions(uint32_t width,uint32_t height){if(!width||!height||uint64_t(width)*height>16777216)throw std::invalid_argument("Invalid dot target dimensions");const std::array<float,4> data={float(width),float(height),0,0};context_.queue.WriteBuffer(uniforms_,0,data.data(),sizeof(data));sized_=true;}
uint32_t GpuDots::upload(std::span<const float> data){
    if(data.size()%4)throw std::invalid_argument("Dot instances require four components per point");const auto used=uint32_t(std::min<size_t>(data.size()/4,capacity_));
    for(float value:data.first(size_t(used)*4))if(!std::isfinite(value))throw std::invalid_argument("Dot instances must be finite");if(used)context_.queue.WriteBuffer(instances_,0,data.data(),size_t(used)*16);count_=used;return used;
}
void GpuDots::drawInstances(const wgpu::CommandEncoder& encoder,const wgpu::TextureView& target,const wgpu::Buffer& instances,uint32_t count)const{
    if(!count)return;if(!sized_)throw std::logic_error("Dot target dimensions have not been set");if(!encoder||!target||!instances||count>32768||instances.GetSize()<uint64_t(count)*16)throw std::invalid_argument("Invalid dot draw resources/count");
    wgpu::RenderPassColorAttachment color{};color.view=target;color.loadOp=wgpu::LoadOp::Load;color.storeOp=wgpu::StoreOp::Store;wgpu::RenderPassDescriptor descriptor{};descriptor.colorAttachmentCount=1;descriptor.colorAttachments=&color;
    auto pass=encoder.BeginRenderPass(&descriptor);pass.SetPipeline(pipeline_);pass.SetBindGroup(0,bindings_);pass.SetVertexBuffer(0,corners_);pass.SetVertexBuffer(1,instances);pass.Draw(4,count);pass.End();
}
void GpuDots::draw(const wgpu::CommandEncoder& encoder,const wgpu::TextureView& target)const{drawInstances(encoder,target,instances_,count_);}
GpuWarpDots::GpuWarpDots(GpuContext& context,GpuAssets& assets,uint32_t seed):context_(context),seed_(seed){
    pipeline_=assets.compute("warp_dots");positions_=gpuBuffer(context_,count*8,wgpu::BufferUsage::Storage|wgpu::BufferUsage::CopySrc);
    instances_=gpuBuffer(context_,count*16,wgpu::BufferUsage::Storage|wgpu::BufferUsage::Vertex|wgpu::BufferUsage::CopySrc);
    controls_=gpuBuffer(context_,32,wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst|wgpu::BufferUsage::CopySrc);context_.checkErrors();
}
void GpuWarpDots::encode(const wgpu::CommandEncoder& encoder,const GpuFeedback& feedback,const WarpDotFrame& frame){
    const float time=scalar(frame.time),strength=scalar(frame.strength),size=scalar(frame.pointSize);if(frame.pointSize<0)throw std::invalid_argument("Dot size must be nonnegative");
    visible_=frame.strength>.001;if(!visible_)return;
    if(!encoder||feedback.phase()!=GpuFeedback::Phase::Prepared)throw std::logic_error("Warp-dot advection requires a prepared feedback frame");
    bool dirty=!bindings_||boundUniforms_.Get()!=feedback.uniformBuffer().Get()||boundSampler_.Get()!=feedback.mapSampler().Get();
    for(unsigned n=0;n<8;++n)dirty=dirty||boundViews_[n].Get()!=feedback.mapViews()[n].Get();
    if(dirty){std::array<wgpu::BindGroupEntry,13> entries;entries[0]=bindBuffer(0,positions_);entries[1]=bindBuffer(1,instances_);entries[2]=bindBuffer(2,controls_);entries[3]=bindBuffer(3,feedback.uniformBuffer());
        for(unsigned n=0;n<8;++n)entries[n+4]=bindTexture(n+4,feedback.mapViews()[n]);entries[12]=bindSampler(12,feedback.mapSampler());
        bindings_=gpuBindings(context_,pipeline_.GetBindGroupLayout(0),entries);boundViews_=feedback.mapViews();boundUniforms_=feedback.uniformBuffer();boundSampler_=feedback.mapSampler();}
    const std::array<float,6> scalars={time,strength,size,initialized_?0.f:1.f,initialized_?float(double(feedback.width())/width_):1.f,initialized_?float(double(feedback.height())/height_):1.f};
    std::array<uint32_t,8> data{};for(unsigned n=0;n<6;++n)data[n]=std::bit_cast<uint32_t>(scalars[n]);data[6]=seed_+frame_;context_.queue.WriteBuffer(controls_,0,data.data(),sizeof(data));
    auto pass=encoder.BeginComputePass();pass.SetPipeline(pipeline_);pass.SetBindGroup(0,bindings_);pass.DispatchWorkgroups(16);pass.End();++frame_;initialized_=true;width_=feedback.width();height_=feedback.height();
}
void GpuWarpDots::draw(const wgpu::CommandEncoder& encoder,const wgpu::TextureView& target,const GpuDots& dots)const{if(visible_&&initialized_)dots.drawInstances(encoder,target,instances_,count);}
}
