// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include "gpu_feedback.hpp"
namespace prismflow {
class GpuDots {
    GpuContext& context_;wgpu::RenderPipeline pipeline_;wgpu::BindGroup bindings_;
    wgpu::Buffer uniforms_,corners_,instances_;uint32_t capacity_=0,count_=0;bool sized_=false;
public:
    GpuDots(GpuContext&,GpuAssets&,uint32_t capacity=32768);
    void dimensions(uint32_t width,uint32_t height);
    uint32_t upload(std::span<const float> xySizeAlpha);
    void draw(const wgpu::CommandEncoder&,const wgpu::TextureView&)const;
    void drawInstances(const wgpu::CommandEncoder&,const wgpu::TextureView&,const wgpu::Buffer&,uint32_t count)const;
    uint32_t count()const{return count_;}
};
struct WarpDotFrame {double time=0,strength=0,pointSize=1;};
class GpuWarpDots {
public:static constexpr uint32_t count=1024;
private:
    GpuContext& context_;wgpu::ComputePipeline pipeline_;wgpu::BindGroup bindings_;
    wgpu::Buffer positions_,instances_,controls_,boundUniforms_;wgpu::Sampler boundSampler_;
    std::array<wgpu::TextureView,8> boundViews_;
    uint32_t seed_=0,frame_=0,width_=0,height_=0;bool initialized_=false,visible_=false;
public:
    GpuWarpDots(GpuContext&,GpuAssets&,uint32_t seed);
    void encode(const wgpu::CommandEncoder&,const GpuFeedback&,const WarpDotFrame&);
    void draw(const wgpu::CommandEncoder&,const wgpu::TextureView&,const GpuDots&)const;
    const wgpu::Buffer& positions()const{return positions_;}const wgpu::Buffer& instances()const{return instances_;}
    const wgpu::Buffer& controls()const{return controls_;}
    uint32_t frame()const{return frame_;}
};
}
