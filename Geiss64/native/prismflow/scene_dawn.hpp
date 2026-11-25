// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include "frame_cpu.hpp"
#include "gpu_bokeh.hpp"
#include "gpu_dots.hpp"
#include "gpu_text.hpp"
#include "music_analysis.hpp"
#include "random.hpp"
#include "image_import.hpp"
#include <filesystem>
namespace prismflow {
struct SceneOptions {uint32_t supersampling=1,seed=0x31415926;bool linear=false;double headroom=1;};
enum class ImageImportState {Idle,Pending,Applied,Failed,Cancelled};
struct ImageImportStatus {ImageImportState state=ImageImportState::Idle;uint64_t token=0;std::string error;};
// Private native implementation, called by one UI/render thread. The shared
// GPU context must outlive it. No C++ value here crosses the public plugin ABI.
class DawnScene {
    GpuContext& context_;SceneOptions options_;Mulberry32 rng_;uint64_t randomCalls_=0;RandomFunction random_;uint32_t dynamicSeed_,warpSeed_;
    GpuAssets assets_;GpuDynamicMaps dynamic_;GpuDots dots_;GpuWarpDots warpDots_;GpuBokeh bokeh_;GpuFeedback feedback_;GpuPresentation presentation_;GpuText text_;
    wgpu::Texture color_,output_;wgpu::TextureView colorView_,outputView_;wgpu::RenderPipeline convert_;wgpu::BindGroup conversion_;
    PresetController presets_;FramePlanner planner_;FramePlan plan_;FrameRequests requests_;
    std::array<float,512> spectrum_{};std::vector<BokehSpawn> spawns_;
    uint32_t width_=0,height_=0,supersampling_=1;uint64_t frames_=0;
    double presetTime_=1000;bool locked_=false,frozen_=false,paletteLocked_=false,pendingBurn_=false;
    double titleX_=0,titleY_=0;std::string embed_;
    std::unique_ptr<ImagePaletteWorker> images_;ImageImportStatus imageStatus_;
    uint32_t scaleFor(uint32_t width,uint32_t height)const;
    void targets(uint32_t width,uint32_t height);
    void unusedSeedDraws();
public:
    DawnScene(GpuContext&,const std::filesystem::path& shaders,uint32_t width,uint32_t height,const SceneOptions& options={});
    ~DawnScene();
    DawnScene(const DawnScene&)=delete;DawnScene& operator=(const DawnScene&)=delete;
    void resize(uint32_t width,uint32_t height);
    // The optional delayed-statistic input is for deterministic local oracle
    // replay; normal module rendering always uses its asynchronous GPU cache.
    void render(const visualizers::MusicFrame&,double time,double dt,std::optional<double> referenceStarLifetime={});
    void key(uint32_t code,uint32_t modifiers=0,bool down=true);
    void embed(std::string text);
    void importImage(std::filesystem::path,uint64_t token);
    void cancelImageImport(uint64_t token=0);
    const ImageImportStatus& imageImportStatus()const{return imageStatus_;}
    void palette(PaletteDefinition value){requests_.overwrite=std::move(value);}
    void frozen(bool value){frozen_=value;}
    void paletteLocked(bool value){paletteLocked_=value;}
    const wgpu::Texture& texture()const{return output_;}
    const wgpu::Texture& colorTexture()const{return color_;}
    const FramePlan& framePlan()const{return plan_;}
    const PresetController& controller()const{return presets_;}
    const GpuFeedback& feedback()const{return feedback_;}
    const GpuBokeh& bokeh()const{return bokeh_;}
    uint32_t supersampling()const{return supersampling_;}
    uint64_t frames()const{return frames_;}
    double presetTime()const{return presetTime_;}
    uint64_t randomCalls()const{return randomCalls_;}
};
}
