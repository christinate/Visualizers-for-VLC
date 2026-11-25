// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include "gpu_present.hpp"
namespace prismflow {
struct TextTiming {double start=-2,end=-1,power=1;};
struct TextRect {bool enabled=false;PresentationTitle title;};
// Uploads a native-rasterized RGBA8 mask and retains logical size/timing.
// Font rasterization is a separate CPU service, not approximated by this pass.
class GpuText {
    GpuContext& context_;wgpu::RenderPipeline pipeline_;wgpu::Buffer uniforms_;
    wgpu::Sampler sampler_;wgpu::Texture mask_,dummy_;wgpu::TextureView maskView_,dummyView_;wgpu::BindGroup bindings_;
    bool has_=false,enabled_=false;double width_=0,height_=0,centerX_=0,centerY_=0,intensity_=1;
    std::optional<TextTiming> timing_=TextTiming{};
public:
    GpuText(GpuContext&,GpuAssets&);
    GpuText(const GpuText&)=delete;GpuText& operator=(const GpuText&)=delete;
    void uploadMask(uint32_t width,uint32_t height,uint32_t supersample,std::span<const uint8_t> rgba);
    void clear();void hide(){enabled_=false;}
    void showTimed(double centerX,double centerY,double intensity,const TextTiming&);
    void showImmediate(double centerX,double centerY,double intensity);
    double intensity(double time)const;
    TextRect rect(double time)const;
    wgpu::TextureView presenterView()const{return enabled_&&has_?maskView_:dummyView_;}
    const wgpu::Sampler& sampler()const{return sampler_;}
    const wgpu::Texture& maskTexture()const{return mask_;}
    const wgpu::TextureView& maskView()const{return maskView_;}
    bool hasMask()const{return has_;}
    static std::array<float,8> uniformData(uint32_t dstWidth,uint32_t dstHeight,double centerX,double centerY,double intensity,double logicalWidth,double logicalHeight);
    // One stamp per submitted frame; does not require presenter visibility.
    void draw(const wgpu::CommandEncoder&,const wgpu::TextureView&,uint32_t width,uint32_t height,double centerX,double centerY,double intensity=1);
};
}
