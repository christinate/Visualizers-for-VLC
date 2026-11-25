// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include "gpu_dynamic.hpp"
#include "preset.hpp"
namespace prismflow {
struct FeedbackFrame {
    std::array<double,kGpuWarpSlots> strengths{};
    double shiftX=0,shiftY=0,darkening=0,time=0,oversample=1;
    ShaderParams shader;
    double diffusionColorization=0; // Retained legacy uniform; unused by WGSL.
    uint32_t noiseX=0,noiseY=0;
};
class GpuFeedback {
public:
    static constexpr size_t cacheSize=20;
    enum class Phase {Idle,Prepared,Warped};
private:
    struct CacheEntry {
        uint64_t identity=0,lastUsed=0;int32_t mode=0,seed=0;
        wgpu::Texture texture;wgpu::TextureView view;
        std::optional<DynamicMapState> dynamic;
    };
    GpuContext& context_;GpuAssets& assets_;GpuDynamicMaps& dynamicMaps_;
    uint32_t width_=0,height_=0,ping_=0,frame_=0;uint64_t cacheClock_=0;
    std::array<CacheEntry,cacheSize> cache_;
    std::array<int32_t,kGpuWarpSlots> slots_;
    std::array<wgpu::TextureView,kGpuWarpSlots> slotViews_;
    std::array<wgpu::Texture,2> feedback_;
    std::array<wgpu::TextureView,2> feedbackViews_;
    std::array<wgpu::Texture,4> noise_;
    std::array<wgpu::TextureView,4> noiseViews_;
    wgpu::Sampler warpSampler_,indexSampler_;
    wgpu::Buffer uniforms_,exposure_;
    wgpu::RenderPipeline warpPipeline_;wgpu::ComputePipeline exposurePipeline_;
    std::array<std::array<wgpu::BindGroup,4>,2> warpGroups_;
    std::array<wgpu::BindGroup,2> exposureGroups_;
    wgpu::CommandEncoder encoder_;Phase phase_=Phase::Idle;uint32_t priorPing_=0;
    bool bindingsDirty_=true;
    void requireIdle()const;
    void rebuildBindings();
    void replaceFeedback(uint32_t width,uint32_t height);
public:
    GpuFeedback(GpuContext&,GpuAssets&,GpuDynamicMaps&,uint32_t width,uint32_t height,const RandomFunction&);
    ~GpuFeedback(){abortFrame();}
    GpuFeedback(const GpuFeedback&)=delete;GpuFeedback& operator=(const GpuFeedback&)=delete;
    void bindMap(size_t slot,const MotionMap&);
    void uploadIndices(std::span<const uint16_t> rgba);
    void uploadIndices(std::span<const uint8_t> monochrome);
    void fillIndices(const RandomFunction&);
    void resize(uint32_t width,uint32_t height,const RandomFunction&);
    void uploadSpectrum(std::span<const float> values){requireIdle();dynamicMaps_.uploadSpectrum(values);}
    static std::array<float,36> uniformData(uint32_t width,uint32_t height,const FeedbackFrame&);
    const wgpu::CommandEncoder& beginFrame(const FeedbackFrame&);
    void encodeWarp();
    void submit();
    void abortFrame()noexcept;
    Phase phase()const{return phase_;}
    uint32_t width()const{return width_;}uint32_t height()const{return height_;}
    uint32_t ping()const{return ping_;}uint32_t frame()const{return frame_;}
    const wgpu::Texture& texture()const{return feedback_[ping_];}
    const wgpu::TextureView& view()const{return feedbackViews_[ping_];}
    const auto& mapViews()const{return slotViews_;}
    const wgpu::Sampler& mapSampler()const{return warpSampler_;}
    const wgpu::Buffer& uniformBuffer()const{return uniforms_;}
    const wgpu::Buffer& exposureBuffer()const{return exposure_;}
    const auto& cacheSlots()const{return slots_;}
    size_t populatedMaps()const;
    size_t dynamicInstances()const;
    const wgpu::Texture& slotTexture(size_t slot)const;
    const wgpu::Texture& noiseTexture(size_t index)const{return noise_.at(index);}
};
}
