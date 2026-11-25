// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#pragma once
#include "motion.hpp"
#include "palette.hpp"
#include <optional>
namespace prismflow {
struct MotionParams {
    double blended_net_motion=0,blended_net_zoom_motion=0,blended_in_or_out_motion=0;
    double blended_net_clockwise_motion=0,blended_cw_or_ccw_motion=0;
    double blended_motion_cx=0,blended_motion_cy=0;
};
struct WaveParams {
    double wave_flat_str=0,wave_flat_angle=0,wave_flat_scale=1,wave_flat_is_stereo=0;
    double wave_flat_stereo_sep=.6,wave_flat_stereo_amplitude_scale=1,wave_flat_cx=0,wave_flat_cy=0;
    double wave_circ_str=0,wave_circ_rad=1,wave_circ_scale=1,wave_circ_angle=0,wave_circ_cx=0,wave_circ_cy=0;
};
struct DotParams {
    double radial_beat_dots=0,random_beat_dots=0,fading_dots=0,grid_dots=0;
    double chaser_1=0,chaser_2=0,dotty_chaser=0,stars_opacity=0,stars_population=0;
};
struct ShaderParams {
    double rx_diff_grey_scott_str=0,rx_diff_grey_scott_str2=0,rx_diff_barkley_str=0;
    double frequency_injector_str=0,frequency_injector_freq_scale=0,painterly_str=0,sharpen_str=0,noise_str=0;
    double layer2_warp_scale=0,layer1_density=0,layer2_brightness=0,layer2_motion_scale=0;
};
struct Warp {
    int32_t mode=0,rand_seed=0;double strength=1;
    MotionMap map{};
    // An artistic choice, separate from regenerable pixel storage. Keeping it
    // here preserves it when an old static map is released and rebuilt.
    std::optional<double> starsFollowMotion;
};
class Preset {
public:
    uint64_t number=0;double t1=0,t2=0;
    std::array<Warp,kMaxActiveWarps> warps;
    bool mapsReady=false;int64_t rebuildId=-1;
    PaletteDefinition originalPalette;
    ExpandedPalette expandedPalette{};
    MotionParams motion;
    WaveParams wave;
    DotParams dots;
    ShaderParams shader;

    static Preset generate(uint64_t number,double t1,double t2,const PaletteCatalog&,
        const RandomFunction&,int32_t overrideMode=-1,std::stop_token stop={});
    void randomizeMotion(const RandomFunction&,int32_t overrideMode=-1,std::stop_token stop={});
    void extractMotionMetadata(const RandomFunction&,std::stop_token stop={});
    WarpPreferences blendedPreferences(bool applyWavePreferenceWeights)const;
    void randomizeWave(const RandomFunction&);
    void randomizeDots(const RandomFunction&);
    void randomizeShader(const RandomFunction&);
    void randomizePalette(const PaletteCatalog&,const RandomFunction&,std::stop_token stop={});
    void updateStarsFollowMotion(const RandomFunction&,bool forceAll);
    void setWarpSlot(size_t slot,int32_t mode,const RandomFunction&,std::stop_token stop={});
    void releaseMaps();
    void rebuildMaps(bool fromWorker=false,std::stop_token stop={});
    size_t mapBytes()const;
};
}
