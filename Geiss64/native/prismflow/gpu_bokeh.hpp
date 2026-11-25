// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include "gpu_feedback.hpp"
#include "palette.hpp"
#include <limits>
#include <optional>
namespace prismflow {
struct BokehSpawn {
    double x=0,y=0,z=.5;std::optional<double> z2;
    bool useBackground=false;double palettePosition=.5,fadeIn=.25,hold=2,fadeOut=1;
};
struct BokehFrame {double time=0,focus=.5,opacity=1;uint32_t width=1,height=1;};
struct BokehStats {uint32_t alive=0,samples=0;std::optional<double> mean;std::string error;};
class GpuBokeh {
    struct Readback;
    GpuContext& context_;GpuAssets& assets_;
    wgpu::Buffer particles_,spawns_,alive_,events_,history_,snapshot_,cursor_,update_,draw_,palette_;
    wgpu::ComputePipeline updatePipeline_,deathPipeline_,statsPipeline_;
    wgpu::BindGroup updateGroup_,deathGroup_,statsGroup_;
    std::array<wgpu::RenderPipeline,3> drawPipelines_;
    std::array<wgpu::BindGroup,3> drawGroups_;
    std::array<wgpu::TextureView,8> boundViews_;wgpu::Sampler boundSampler_;wgpu::Buffer boundUniforms_;
    std::vector<std::array<double,12>> pending_;
    std::vector<float> spawnData_; // Sized once; keep the 192 KiB upload off the stack.
    std::array<float,8> follow_{1,1,1,1,1,1,1,1};
    BokehFrame frame_;BokehStats stats_;bool initialized_=false;int drawIndex_=-1;
    uint64_t generation_=0,statsRequests_=0;double lastRequest_=-std::numeric_limits<double>::infinity();
    std::shared_ptr<Readback> readback_;
    void pollStats();
public:
    static constexpr uint32_t capacity=4096,eventCount=8192,historySize=1024;
    GpuBokeh(GpuContext&,GpuAssets&);
    ~GpuBokeh();
    GpuBokeh(const GpuBokeh&)=delete;GpuBokeh& operator=(const GpuBokeh&)=delete;
    uint32_t add(std::span<const BokehSpawn>,uint32_t simulationWidth,uint32_t simulationHeight,double birthTime);
    void frame(const BokehFrame&);
    void follow(std::span<const double>);
    void palette(std::span<const float>);
    void seedHistory(double maximumLifetime);
    void encode(const wgpu::CommandEncoder&,const GpuFeedback&);
    void prepareDraw(wgpu::TextureFormat,const Color& background);
    void draw(const wgpu::RenderPassEncoder&)const;
    // Cached result; polls ready work without waiting and requests at most one
    // new snapshot per 30 ms. Use a monotonic clock, between submitted frames.
    BokehStats stats(double monotonicMilliseconds);
    bool statsPending()const;
    uint64_t historyGeneration()const{return generation_;}
    uint64_t statsRequests()const{return statsRequests_;}
    size_t pendingSpawns()const{return pending_.size();}
    // Diagnostic buffers. Production frame encoding never maps them.
    const wgpu::Buffer& particles()const{return particles_;}
    const wgpu::Buffer& history()const{return history_;}
    const wgpu::Buffer& events()const{return events_;}
    const wgpu::Buffer& alive()const{return alive_;}
    const wgpu::Buffer& updateControls()const{return update_;}
    const wgpu::Buffer& drawControls()const{return draw_;}
    const wgpu::Buffer& spawnBuffer()const{return spawns_;}
};
}
