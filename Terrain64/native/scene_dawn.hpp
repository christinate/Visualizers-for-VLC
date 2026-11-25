// SPDX-License-Identifier: MIT
#pragma once
#include "context.hpp"
#include "music_analysis.hpp"
#include "scene_settings.hpp"
#include <array>
#include <span>
#include <cmath>
namespace terrain {
inline constexpr std::array<const char*,8> CameraNames = {
    "flyover", "underside", "sidewall", "tunnel", "ridge", "survey", "skimmer", "reverse_bank",
};

struct TerrainMotion {
    std::array<bool,8> cameras{true,true,true,true,false,false,false,false};
    std::array<bool,8> heightViews{true,false,false,false,true,true,true,false};
    std::array<bool,8> widthViews{false,false,true,false,false,false,false,true};
    bool heightEnabled = false;
    bool widthEnabled = false;
    float heightPeak = 2, widthPeak = 2, heightSeconds = .5f, widthSeconds = .5f;
    float cameraSeconds = 0, speed = 3;
    float clearance = 3;
    double transition = .15;
    unsigned colorBeats = 0;

    static TerrainMotion fromIni(const SceneIni&);
};

// Native controller workbench. The host/context must outlive this renderer.
class DawnScene {
    visualizers::gpu::Context& context_;
    wgpu::RenderPipeline pipeline_;
    wgpu::Texture history_,target_;
    wgpu::TextureView targetView_;
    wgpu::Buffer uniform_;
    wgpu::BindGroup bindings_;
    unsigned width_,height_;
    float head_=0;
    std::array<float,28> data_{};
    std::array<uint16_t,1024> rowData_{};
    unsigned row_=0;
    double lastHistory_=-1,hue_=0;
    uint64_t lastBeat_=0;
    TerrainMotion motion_;
    BeatTrigger beat_;
    ModeCycle cameras_;
    bool linearOutput_=false;
    float peakNits_=1000;
public:
    DawnScene(visualizers::gpu::Context&,const std::string& shader,unsigned width,unsigned height);
    void setHistory(std::span<const uint16_t> data,float head);
    unsigned draw(double time,float beatEnvelope,float rms,uint64_t beatCount,
        float cameraSeconds=60,unsigned colorBeats=4,float hue=-1);
    unsigned update(const visualizers::MusicFrame&,double time,float cameraSeconds=60,unsigned colorBeats=4);
    void resize(unsigned width,unsigned height);
    void setOutput(bool linear,float peakNits);
    void setMotion(TerrainMotion motion){motion_=motion;}
    void nextColor(){hue_=std::fmod(hue_+.137,1.);}
    void nextMode(){cameras_.next();}
    void toggleModeLock(){cameras_.toggleLock();}
    const std::array<float,28>& uniforms()const{return data_;}
    const std::array<uint16_t,1024>& historyRow()const{return rowData_;}
    unsigned nextHistoryRow()const{return row_;}
    const wgpu::Texture& texture()const{return target_;}
};
}
