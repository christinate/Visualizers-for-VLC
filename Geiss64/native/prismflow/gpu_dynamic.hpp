// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include "gpu_support.hpp"
#include "motion.hpp"
namespace prismflow {
struct DynamicMapState {
    int32_t mode=0;
    std::array<float,kDynamicMotionParamCount> parameters{};
    wgpu::Buffer uniforms;
    wgpu::BindGroup bindings;
    wgpu::RenderPipeline pipeline;
};
struct DynamicMapDraw {const DynamicMapState* state;wgpu::TextureView target;};
class GpuDynamicMaps {
    struct Noise {
        wgpu::Texture texture;wgpu::TextureView view;wgpu::Buffer seed;
        wgpu::ComputePipeline pipeline;wgpu::BindGroup bindings;
        uint32_t width,height,depth;
    };
    GpuContext& context_;GpuAssets& assets_;
    std::array<Noise,3> noise_;
    wgpu::Texture spectrum_;wgpu::TextureView spectrumView_;
    wgpu::Sampler noiseSampler_,spectrumSampler_;
    std::array<uint16_t,512> spectrumUpload_{};
    uint32_t seed_;
    Noise createNoise(uint32_t width,uint32_t height,uint32_t depth,bool volume);
    void encodeNoise(const wgpu::CommandEncoder&,const Noise&,uint32_t seed);
public:
    GpuDynamicMaps(GpuContext&,GpuAssets&,uint32_t seed);
    DynamicMapState create(int32_t mode,std::span<const float> parameters);
    void uploadSpectrum(std::span<const float> values);
    // Call once per engine frame. Empty draws leave the dynamic noise alone;
    // duplicate state/view pairs are rendered once. Each instance owns uniforms.
    void encode(const wgpu::CommandEncoder&,std::span<const DynamicMapDraw>,double time);
    const wgpu::Texture& staticNoise3d()const{return noise_[0].texture;}
    const wgpu::Texture& staticNoise2d()const{return noise_[1].texture;}
    const wgpu::Texture& dynamicNoise2d()const{return noise_[2].texture;}
    const wgpu::Texture& spectrumTexture()const{return spectrum_;}
    uint32_t noiseSeed()const{return seed_;}
    static std::array<float,24> uniformData(const DynamicMapState&,double time);
};
}
