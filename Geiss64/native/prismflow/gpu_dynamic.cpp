// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#include "gpu_dynamic.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace prismflow {
GpuDynamicMaps::Noise GpuDynamicMaps::createNoise(uint32_t width,uint32_t height,uint32_t depth,bool volume){
    Noise result;result.width=width;result.height=height;result.depth=depth;
    result.texture=gpuTexture(context_,width,height,wgpu::TextureFormat::RGBA16Float,
        wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::StorageBinding|wgpu::TextureUsage::CopySrc,depth,
        volume?wgpu::TextureDimension::e3D:wgpu::TextureDimension::e2D);
    wgpu::TextureViewDescriptor view{};view.dimension=volume?wgpu::TextureViewDimension::e3D:wgpu::TextureViewDimension::e2D;result.view=result.texture.CreateView(&view);
    result.seed=gpuBuffer(context_,16,wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst);
    result.pipeline=assets_.compute(volume?"noise_3d":"noise_2d");
    const std::array entries={bindTexture(0,result.view),bindBuffer(1,result.seed)};
    result.bindings=gpuBindings(context_,result.pipeline.GetBindGroupLayout(0),entries);return result;
}
void GpuDynamicMaps::encodeNoise(const wgpu::CommandEncoder& encoder,const Noise& noise,uint32_t seed){
    const std::array<uint32_t,4> data={seed,0,0,0};context_.queue.WriteBuffer(noise.seed,0,data.data(),sizeof(data));
    auto pass=encoder.BeginComputePass();pass.SetPipeline(noise.pipeline);pass.SetBindGroup(0,noise.bindings);
    pass.DispatchWorkgroups((noise.width+7)/8,(noise.height+7)/8,noise.depth);pass.End();
}
GpuDynamicMaps::GpuDynamicMaps(GpuContext& context,GpuAssets& assets,uint32_t seed):context_(context),assets_(assets),seed_(seed){
    noise_[0]=createNoise(64,64,16,true);noise_[1]=createNoise(256,256,1,false);noise_[2]=createNoise(64,64,1,false);
    wgpu::SamplerDescriptor noiseSampler{};noiseSampler.addressModeU=noiseSampler.addressModeV=noiseSampler.addressModeW=wgpu::AddressMode::Repeat;
    noiseSampler.minFilter=noiseSampler.magFilter=wgpu::FilterMode::Linear;noiseSampler.mipmapFilter=wgpu::MipmapFilterMode::Nearest;noiseSampler_=context_.device.CreateSampler(&noiseSampler);
    wgpu::SamplerDescriptor spectrumSampler{};spectrumSampler.minFilter=spectrumSampler.magFilter=wgpu::FilterMode::Linear;spectrumSampler_=context_.device.CreateSampler(&spectrumSampler);
    spectrum_=gpuTexture(context_,512,1,wgpu::TextureFormat::R16Float,wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::CopyDst|wgpu::TextureUsage::CopySrc);
    spectrumView_=spectrum_.CreateView();uploadHalfTexture(context_,spectrum_,spectrumUpload_);
    auto encoder=context_.device.CreateCommandEncoder();encodeNoise(encoder,noise_[0],seed_^0xa511e9b3u);encodeNoise(encoder,noise_[1],seed_^0x63d83595u);encodeNoise(encoder,noise_[2],seed_);
    const auto commands=encoder.Finish();context_.queue.Submit(1,&commands);context_.checkErrors();
}
DynamicMapState GpuDynamicMaps::create(int32_t mode,std::span<const float> parameters){
    if(mode!=35&&mode!=37&&mode!=38&&mode!=39)throw std::invalid_argument("Unknown dynamic Prism Flow mode");
    if(parameters.size()!=kDynamicMotionParamCount)throw std::invalid_argument("Dynamic Prism Flow maps require 16 parameters");
    for(const auto value:parameters)if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("Dynamic Prism Flow parameters must be finite in [0,1]");
    DynamicMapState state;state.mode=mode;std::copy(parameters.begin(),parameters.end(),state.parameters.begin());
    state.pipeline=assets_.render("dynamic_"+std::to_string(mode),wgpu::TextureFormat::RG16Float);state.uniforms=gpuBuffer(context_,24*sizeof(float),wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst);
    const std::array entries={bindBuffer(0,state.uniforms),bindTexture(1,noise_[0].view),bindTexture(2,noise_[1].view),bindTexture(3,noise_[2].view),bindSampler(4,noiseSampler_),bindTexture(5,spectrumView_),bindSampler(6,spectrumSampler_)};
    state.bindings=gpuBindings(context_,assets_.dynamicGroup(),entries);context_.checkErrors();return state;
}
void GpuDynamicMaps::uploadSpectrum(std::span<const float> values){
    if(values.size()!=spectrumUpload_.size())throw std::invalid_argument("Dynamic Prism Flow requires 512 audio spectrum values");
    for(size_t n=0;n<values.size();++n){const float value=std::isfinite(values[n])?std::clamp(values[n],-65504.f,65504.f):0.f;spectrumUpload_[n]=visualizers::gpu::encodeHalf(value);}
    uploadHalfTexture(context_,spectrum_,spectrumUpload_);
}
std::array<float,24> GpuDynamicMaps::uniformData(const DynamicMapState& state,double time){
    if(!std::isfinite(time)||std::abs(time)>std::numeric_limits<float>::max())throw std::invalid_argument("Invalid dynamic Prism Flow time");
    std::array<float,24> values{};std::copy(state.parameters.begin(),state.parameters.end(),values.begin());
    values[16]=float(time);values[17]=values[18]=kWarpMapSize;values[19]=64;values[20]=16;values[21]=256;values[22]=64;return values;
}
void GpuDynamicMaps::encode(const wgpu::CommandEncoder& encoder,std::span<const DynamicMapDraw> draws,double time){
    if(draws.size()>20)throw std::invalid_argument("Too many dynamic Prism Flow map draws");if(draws.empty())return;
    // Validate before modifying the frame/noise seed or recording commands.
    for(const auto& draw:draws){if(!draw.state||!draw.target||!draw.state->bindings||!draw.state->pipeline)throw std::invalid_argument("Incomplete dynamic Prism Flow draw");uniformData(*draw.state,time);}
    encodeNoise(encoder,noise_[2],++seed_);
    for(size_t i=0;i<draws.size();++i){const auto& draw=draws[i];bool duplicate=false;
        for(size_t n=0;n<i;++n)if(draws[n].state==draw.state&&draws[n].target.Get()==draw.target.Get()){duplicate=true;break;}if(duplicate)continue;
        const auto data=uniformData(*draw.state,time);context_.queue.WriteBuffer(draw.state->uniforms,0,data.data(),sizeof(data));
        wgpu::RenderPassColorAttachment color{};color.view=draw.target;color.loadOp=wgpu::LoadOp::Clear;color.storeOp=wgpu::StoreOp::Store;color.clearValue={0,0,0,0};
        wgpu::RenderPassDescriptor descriptor{};descriptor.colorAttachmentCount=1;descriptor.colorAttachments=&color;
        auto pass=encoder.BeginRenderPass(&descriptor);pass.SetPipeline(draw.state->pipeline);pass.SetBindGroup(0,draw.state->bindings);pass.Draw(3);pass.End();
    }
}
}
