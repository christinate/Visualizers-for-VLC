// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include "scheduler.hpp"
#include "waveform.hpp"
namespace prismflow {
template<class T>struct ParameterField {const char* name;double T::*member;};
#define PRISM_FIELD(T,x) ParameterField<T>{#x,&T::x}
inline constexpr std::array motionFields={PRISM_FIELD(MotionParams,blended_net_motion),PRISM_FIELD(MotionParams,blended_net_zoom_motion),PRISM_FIELD(MotionParams,blended_in_or_out_motion),PRISM_FIELD(MotionParams,blended_net_clockwise_motion),PRISM_FIELD(MotionParams,blended_cw_or_ccw_motion),PRISM_FIELD(MotionParams,blended_motion_cx),PRISM_FIELD(MotionParams,blended_motion_cy)};
inline constexpr std::array waveFields={PRISM_FIELD(WaveParams,wave_flat_str),PRISM_FIELD(WaveParams,wave_flat_angle),PRISM_FIELD(WaveParams,wave_flat_scale),PRISM_FIELD(WaveParams,wave_flat_is_stereo),PRISM_FIELD(WaveParams,wave_flat_stereo_sep),PRISM_FIELD(WaveParams,wave_flat_stereo_amplitude_scale),PRISM_FIELD(WaveParams,wave_flat_cx),PRISM_FIELD(WaveParams,wave_flat_cy),PRISM_FIELD(WaveParams,wave_circ_str),PRISM_FIELD(WaveParams,wave_circ_rad),PRISM_FIELD(WaveParams,wave_circ_scale),PRISM_FIELD(WaveParams,wave_circ_angle),PRISM_FIELD(WaveParams,wave_circ_cx),PRISM_FIELD(WaveParams,wave_circ_cy)};
inline constexpr std::array dotFields={PRISM_FIELD(DotParams,radial_beat_dots),PRISM_FIELD(DotParams,random_beat_dots),PRISM_FIELD(DotParams,fading_dots),PRISM_FIELD(DotParams,grid_dots),PRISM_FIELD(DotParams,chaser_1),PRISM_FIELD(DotParams,chaser_2),PRISM_FIELD(DotParams,dotty_chaser),PRISM_FIELD(DotParams,stars_opacity),PRISM_FIELD(DotParams,stars_population)};
inline constexpr std::array shaderFields={PRISM_FIELD(ShaderParams,rx_diff_grey_scott_str),PRISM_FIELD(ShaderParams,rx_diff_grey_scott_str2),PRISM_FIELD(ShaderParams,rx_diff_barkley_str),PRISM_FIELD(ShaderParams,frequency_injector_str),PRISM_FIELD(ShaderParams,frequency_injector_freq_scale),PRISM_FIELD(ShaderParams,painterly_str),PRISM_FIELD(ShaderParams,sharpen_str),PRISM_FIELD(ShaderParams,noise_str),PRISM_FIELD(ShaderParams,layer2_warp_scale),PRISM_FIELD(ShaderParams,layer1_density),PRISM_FIELD(ShaderParams,layer2_brightness),PRISM_FIELD(ShaderParams,layer2_motion_scale)};
#undef PRISM_FIELD
struct FrameAudio {std::span<const float> wave;double rms=0,visualEnergy=0;};
struct FrameInput {
    uint32_t width=1,height=1,waveSmoothing=1;
    double shiftTime=1000,time=0,fps=60,motionScale=1,brightness=1,headroom=1,darkening=1,alignFraction=1,beat=0;
    bool frozen=false,paletteLocked=false,experiment=false;
    double averageStarLifetime=4;
    uint32_t embedUnits=0; // UTF-16 units, matching the reference's text.length.
};
struct StarSpawn {double x,y,z,z2,palettePosition,fadeIn,hold,fadeOut;bool background;};
struct FramePlan {
    struct Title {double x,y;uint32_t fontPixels;};std::optional<Title> title;
    // Borrowed maps remain valid only until the history is edited/pruned.
    std::array<const Warp*,8> warps{};std::array<double,8> strengths{},follow{};size_t warpCount=0;
    ExpandedPalette palette{};Color background{};
    WaveParams wave;DotParams dots;ShaderParams shader;MotionParams motion;
    double shiftX=0,shiftY=0,pointSize=1,amplitude=1,autoDarkening=0,darkening=0;
    uint32_t noiseX=0,noiseY=0;
    std::vector<float> points;std::vector<StarSpawn> stars;
};
class FramePlanner {
    WaveAligner aligner_;double energy_=0,titleX_=-1,titleY_=-1;std::optional<ExpandedPalette> frozenPalette_;
    void wavePoints(FramePlan&,const FrameInput&,const FrameAudio&);
    void decorations(FramePlan&,const FrameInput&,const RandomFunction&);
public:
    FramePlan build(const PresetHistory&,const ActivePresets&,const FrameInput&,const FrameAudio&,const RandomFunction&,FramePlan reuse={});
    double smoothedEnergy()const{return energy_;}
    void resetWave(){aligner_.reset();}
    void titleBurned(){titleX_=titleY_=-1;}
};
struct FrameRequests {
    bool motion=false,palette=false,wave=false,dots=false,shader=false;
    bool grid=false,fading=false,chasers=false,dotty=false,randomBeat=false,radialBeat=false;
    double paletteScale=1;std::optional<PaletteDefinition> overwrite;
};
void applyFrameRequests(PresetController&,const ActivePresets&,const FrameRequests&,const RandomFunction&);
}
