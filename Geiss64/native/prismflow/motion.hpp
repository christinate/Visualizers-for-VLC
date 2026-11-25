// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#pragma once
#include "wormhole.hpp"
#include <vector>
#include <array>
#include <span>
namespace prismflow {
inline constexpr uint32_t kWarpMapSize=512,kDynamicMotionParamCount=16;
inline constexpr uint32_t kMaxActiveWarps=4,kGpuWarpSlots=8,kNoiseTexSize=256,kNoiseTexCount=4;
struct WarpPreferences {
    double wave_prefs_weight;
    double use_motion_metadata_to_choose_wave_type;
    double wave_flat_prob;
    double wave_circ_prob;
    double wave_flat_plus_circ_prob;
    double flat_angle_lo;
    double flat_angle_hi;
    double flat_angle_bias_toward_horizontal_angles;
    double flat_scale;
    double flat_is_stereo_chance;
    double flat_stereo_sep_lo;
    double flat_stereo_sep_hi;
    double flat_stereo_amplitude_scale;
    double flat_cx_lo;
    double flat_cx_hi;
    double flat_cy_lo;
    double flat_cy_hi;
    double circ_rad_lo;
    double circ_rad_hi;
    double circ_scale;
    double circ_cx_lo;
    double circ_cx_hi;
    double circ_cy_lo;
    double circ_cy_hi;
    double motion_cx;
    double motion_cy;
    double use_motion_center_as_wave_center_prob;
    double use_motion_center_as_wave_center_power;
    double radial_beat_dots_prob;
    double random_beat_dots_prob;
    double fading_dots_prob;
    double grid_dots_prob;
    double chaser_prob;
    double dotty_chaser_prob;
    double stars_voting_weight;
    double stars_prob;
    double stars_population_min;
    double stars_population_max;
    double stars_opacity_min;
    double stars_opacity_max;
    double stars_follow_motion_min;
    double stars_follow_motion_max;
    double stars_follow_motion_pow;
};
struct PreferenceField {const char* name;double WarpPreferences::*member;};
inline constexpr std::array<PreferenceField,43> preferenceFields={{
    {"wave_prefs_weight", &WarpPreferences::wave_prefs_weight},
    {"use_motion_metadata_to_choose_wave_type", &WarpPreferences::use_motion_metadata_to_choose_wave_type},
    {"wave_flat_prob", &WarpPreferences::wave_flat_prob},
    {"wave_circ_prob", &WarpPreferences::wave_circ_prob},
    {"wave_flat_plus_circ_prob", &WarpPreferences::wave_flat_plus_circ_prob},
    {"flat_angle_lo", &WarpPreferences::flat_angle_lo},
    {"flat_angle_hi", &WarpPreferences::flat_angle_hi},
    {"flat_angle_bias_toward_horizontal_angles", &WarpPreferences::flat_angle_bias_toward_horizontal_angles},
    {"flat_scale", &WarpPreferences::flat_scale},
    {"flat_is_stereo_chance", &WarpPreferences::flat_is_stereo_chance},
    {"flat_stereo_sep_lo", &WarpPreferences::flat_stereo_sep_lo},
    {"flat_stereo_sep_hi", &WarpPreferences::flat_stereo_sep_hi},
    {"flat_stereo_amplitude_scale", &WarpPreferences::flat_stereo_amplitude_scale},
    {"flat_cx_lo", &WarpPreferences::flat_cx_lo},
    {"flat_cx_hi", &WarpPreferences::flat_cx_hi},
    {"flat_cy_lo", &WarpPreferences::flat_cy_lo},
    {"flat_cy_hi", &WarpPreferences::flat_cy_hi},
    {"circ_rad_lo", &WarpPreferences::circ_rad_lo},
    {"circ_rad_hi", &WarpPreferences::circ_rad_hi},
    {"circ_scale", &WarpPreferences::circ_scale},
    {"circ_cx_lo", &WarpPreferences::circ_cx_lo},
    {"circ_cx_hi", &WarpPreferences::circ_cx_hi},
    {"circ_cy_lo", &WarpPreferences::circ_cy_lo},
    {"circ_cy_hi", &WarpPreferences::circ_cy_hi},
    {"motion_cx", &WarpPreferences::motion_cx},
    {"motion_cy", &WarpPreferences::motion_cy},
    {"use_motion_center_as_wave_center_prob", &WarpPreferences::use_motion_center_as_wave_center_prob},
    {"use_motion_center_as_wave_center_power", &WarpPreferences::use_motion_center_as_wave_center_power},
    {"radial_beat_dots_prob", &WarpPreferences::radial_beat_dots_prob},
    {"random_beat_dots_prob", &WarpPreferences::random_beat_dots_prob},
    {"fading_dots_prob", &WarpPreferences::fading_dots_prob},
    {"grid_dots_prob", &WarpPreferences::grid_dots_prob},
    {"chaser_prob", &WarpPreferences::chaser_prob},
    {"dotty_chaser_prob", &WarpPreferences::dotty_chaser_prob},
    {"stars_voting_weight", &WarpPreferences::stars_voting_weight},
    {"stars_prob", &WarpPreferences::stars_prob},
    {"stars_population_min", &WarpPreferences::stars_population_min},
    {"stars_population_max", &WarpPreferences::stars_population_max},
    {"stars_opacity_min", &WarpPreferences::stars_opacity_min},
    {"stars_opacity_max", &WarpPreferences::stars_opacity_max},
    {"stars_follow_motion_min", &WarpPreferences::stars_follow_motion_min},
    {"stars_follow_motion_max", &WarpPreferences::stars_follow_motion_max},
    {"stars_follow_motion_pow", &WarpPreferences::stars_follow_motion_pow},
}};
// Copies retain identity because generated pixel/descriptor data is immutable
// while cached. Building/rebuilding a map creates a new checked identity.
uint64_t nextMotionMapIdentity();
struct MotionMap {
    int32_t mode;uint32_t width,height;
    WarpPreferences preferences;
    std::vector<float> displacement,dynamic;
    int32_t seed;double breathingStrength,breathingPhase,breathingFrequency;
    uint64_t identity=nextMotionMapIdentity();
    bool isDynamic()const{return !dynamic.empty();}
};
struct MotionMode {int32_t mode;double weight;};
std::span<const MotionMode,40> motionModes();
enum class ModeRelation {SuperGood,Good,Bad,Forbidden};
using ModePairs=std::array<std::array<int32_t,40>,40>;
const ModePairs& modePairs(ModeRelation);
inline constexpr double kSuperGoodModePairWeightScale=4,kGoodModePairWeightScale=2.15,
    kBadModePairWeightScale=1/2.15,kForbiddenModePairWeightScale=.00001;
MotionMap buildWarpMap(int32_t mode,int32_t seed,std::stop_token stop={});
int32_t pickRandomWarpMap(std::span<const int32_t> companions,double unitRandom);
}
