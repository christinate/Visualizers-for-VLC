// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
// Numeric port of the preserved motion.js; no JavaScript runtime.
// Random-index updates are sequenced explicitly before dependent expressions.
#include "motion.hpp"
#include "random.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <atomic>
#include <limits>
namespace prismflow {
uint64_t nextMotionMapIdentity(){
    static std::atomic<uint64_t> next{1};auto value=next.load(std::memory_order_relaxed);
    for(;;){if(value==std::numeric_limits<uint64_t>::max())throw std::overflow_error("Prism Flow map identity space exhausted");
        if(next.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;}
}
namespace {
constexpr double kSwirlFreq=1.05;
constexpr double WarpMapRandomNumberCount=256,WarpMapRandomNumberMask=255;
struct Vec2 {double x,y;};
double lerp(double a,double b,double t){return a*(1-t)+b*t;}
double smoothstep(double x){return x*x*(3-2*x);}
double LogInterp(double lo,double hi,double t){return lo*std::pow(hi/lo,t);}
double LinearInterp(double lo,double hi,double t){return lo+(hi-lo)*t;}
Vec2 ApplyRot2D(double x,double y,double rot){const double c=std::cos(rot),s=std::sin(rot);return {x*c-y*s,x*s+y*c};}
void AdjustCenter(double cx,double cy,WarpPreferences& p){
    p.flat_cx_lo+=cx;p.flat_cx_hi+=cx;p.flat_cy_lo+=cy;p.flat_cy_hi+=cy;
    p.circ_cx_lo+=cx;p.circ_cx_hi+=cx;p.circ_cy_lo+=cy;p.circ_cy_hi+=cy;p.motion_cx=cx;p.motion_cy=cy;
}
size_t checkedIndex(double value,size_t size){
    if(!std::isfinite(value)||value<0||value>=double(size)||value!=std::floor(value))throw std::out_of_range("Prism Flow motion array index");return size_t(value);
}
std::vector<float> floatArray(double size){
    if(!std::isfinite(size)||size<0||size>kWarpMapSize*kWarpMapSize*2||size!=std::floor(size))throw std::out_of_range("Prism Flow motion array budget");return std::vector<float>(size_t(size));
}
void checkCancelled(std::stop_token stop){if(stop.stop_requested())throw std::runtime_error("Prism Flow motion generation cancelled");}
const std::array<MotionMode,40> modes={{{0, 0.41}, {1, 2.43}, {2, 0.62}, {3, (1.12 * kSwirlFreq)}, {4, (0.61 * kSwirlFreq)}, {5, (0.42 * kSwirlFreq)}, {6, (0.43 * kSwirlFreq)}, {7, (0.94 * kSwirlFreq)}, {8, 1.49}, {9, 1.23}, {10, 0.15}, {11, 1.82}, {12, 0.02}, {13, 0.22}, {14, 1.79}, {15, 0.06}, {16, 2.33}, {17, 3.28}, {18, 1.1}, {19, 0.29}, {20, 0.06}, {21, 0.49}, {22, 0.12}, {23, 0.17}, {24, 0.13}, {25, 0.08}, {26, 0.19}, {27, 0.31}, {28, 0.17}, {29, 0.17}, {30, 0.36}, {31, 0.35}, {32, 0.13}, {33, 0.32}, {34, 0.17}, {35, 1.5}, {36, 0.2}, {37, (1.33 * kSwirlFreq)}, {38, (0.27 * kSwirlFreq)}, {39, 1e-06}}};
const ModePairs SuperGoodModePairs={{
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
}};
const ModePairs GoodModePairs={{
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
}};
const ModePairs BadModePairs={{
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
}};
const ModePairs ForbiddenModePairs={{
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0}},
    {{0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 1, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0}},
    {{1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
}};
}
std::span<const MotionMode,40> motionModes(){return modes;}
const ModePairs& modePairs(ModeRelation relation){
    switch(relation){
        case ModeRelation::SuperGood:return SuperGoodModePairs;
        case ModeRelation::Good:return GoodModePairs;
        case ModeRelation::Bad:return BadModePairs;
        case ModeRelation::Forbidden:return ForbiddenModePairs;
    }throw std::invalid_argument("Unknown motion relation");
}
MotionMap buildWarpMap(int32_t new_mode,int32_t seed,std::stop_token stop){
    checkCancelled(stop);
    if(new_mode < -1 || (new_mode>41 && new_mode!=43 && new_mode!=44 && new_mode!=98 && new_mode!=99)) throw std::invalid_argument("Unknown Prism Flow motion mode");
    auto random = Mulberry32(uint32_t(seed));
    const double W = kWarpMapSize;
    const double H = kWarpMapSize;
    const double kFollowMotionNone = 0.0;
    const double kFollowMotionLow = 0.1;
    const double kFollowMotionMed = 0.5;
    const double kFollowMotionHigh = 1.0;
    auto warp_prefs = WarpPreferences{.wave_prefs_weight = 1.0, .use_motion_metadata_to_choose_wave_type = 1.0, .wave_flat_prob = 1.0, .wave_circ_prob = 0.25, .wave_flat_plus_circ_prob = 0.01, .flat_angle_lo = 0.0, .flat_angle_hi = 3.141592, .flat_angle_bias_toward_horizontal_angles = 1.7, .flat_scale = 1.0, .flat_is_stereo_chance = 0.18, .flat_stereo_sep_lo = 0.43, .flat_stereo_sep_hi = 0.65, .flat_stereo_amplitude_scale = 1.0, .flat_cx_lo = (-0.12), .flat_cx_hi = 0.12, .flat_cy_lo = (-0.12), .flat_cy_hi = 0.12, .circ_rad_lo = 0.65, .circ_rad_hi = 1.2, .circ_scale = 1.0, .circ_cx_lo = (-0.05), .circ_cx_hi = 0.05, .circ_cy_lo = (-0.05), .circ_cy_hi = 0.05, .motion_cx = 0.0, .motion_cy = 0.0, .use_motion_center_as_wave_center_prob = 0.93, .use_motion_center_as_wave_center_power = 0.4, .radial_beat_dots_prob = 0.57, .random_beat_dots_prob = 0.01, .fading_dots_prob = 0.07, .grid_dots_prob = 0.005, .chaser_prob = 0.37, .dotty_chaser_prob = 0.05, .stars_voting_weight = 1.0, .stars_prob = 0.55, .stars_population_min = 1.0, .stars_population_max = 1.0, .stars_opacity_min = 0.3, .stars_opacity_max = 1.0, .stars_follow_motion_min = kFollowMotionMed, .stars_follow_motion_max = kFollowMotionHigh, .stars_follow_motion_pow = 1.0};
    const double n = (W * H);
    bool is_dynamic = false;
    auto src_dxy = floatArray((n * 2.0));
    const double aspect = ((W * 1.0) / H);
    double x0 = (-1.0);
    double y0 = (-1.0);
    double x1 = 1.0;
    double y1 = 1.0;
    if ((aspect > 1.0))
    {
        y0 /= aspect;
        y1 /= aspect;
    }
    else
    {
        x0 *= aspect;
        x1 *= aspect;
    }
    const double inv_W = (1.0 / W);
    const double inv_H = (1.0 / H);
    const double x0x1 = (x1 - x0);
    const double y0y1 = (y1 - y0);
    const double inv_x0x1 = (1.0 / x0x1);
    const double inv_y0y1 = (1.0 / y0y1);
    auto randoms = floatArray(WarpMapRandomNumberCount);
    for (double i = 0.0; (i < WarpMapRandomNumberCount); i += 1.0)
    {
        randoms.at(checkedIndex(i, randoms.size())) = random();
    }
    double rand_idx = 0.0;
    double write_offset = 0.0;
    if ((new_mode < 0.0))
    {
    }
    else
    if (new_mode == 4.0)
    {
        warp_prefs.stars_prob *= 0.5;
        warp_prefs.stars_follow_motion_min = kFollowMotionNone;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_pow = 3.0;
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = 0.0;
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = 0.0;
            }
        }
        const double ordered_3 = rand_idx;
        rand_idx += 1.0;
        const double N = jsBitOr(((2.0) + ((34.0) * double(randoms.at(checkedIndex(ordered_3, randoms.size()))))), 0.0);
        for (double n = 0.0; (n < N); n += 1.0)
        {
            const double ordered_4 = rand_idx;
            rand_idx += 1.0;
            const double cx = ((double(randoms.at(checkedIndex(ordered_4, randoms.size()))) - 0.5) * x0x1);
            const double ordered_5 = rand_idx;
            rand_idx += 1.0;
            const double cy = ((double(randoms.at(checkedIndex(ordered_5, randoms.size()))) - 0.5) * y0y1);
            const double ordered_8 = rand_idx;
            rand_idx += 1.0;
            const double rad = ((0.3) + (((0.5) * double(randoms.at(checkedIndex(ordered_8, randoms.size())))) * std::min(1.0, (10.0 / N))));
            const double ordered_11 = rand_idx;
            rand_idx += 1.0;
            const auto ordered_12 = ((0.2) + ((0.5) * double(randoms.at(checkedIndex(ordered_11, randoms.size())))));
            const double ordered_13 = rand_idx;
            rand_idx += 1.0;
            const double str = (((ordered_12 * ((double(randoms.at(checkedIndex(ordered_13, randoms.size()))) > 0.5) ? 1.0 : (-1.0))) * 0.1) * std::pow(std::min(1.0, (3.0 / N)), 0.7));
            const double dx0 = jsBitOr(std::max(0.0, std::min(W, ((((cx - rad) * inv_x0x1) + 0.5) * W))), 0.0);
            const double dy0 = jsBitOr(std::max(0.0, std::min(H, ((((cy - rad) * inv_y0y1) + 0.5) * H))), 0.0);
            const double dx1 = jsBitOr(std::max(0.0, std::min(W, ((((cx + rad) * inv_x0x1) + 0.5) * W))), 0.0);
            const double dy1 = jsBitOr(std::max(0.0, std::min(H, ((((cy + rad) * inv_y0y1) + 0.5) * H))), 0.0);
            for (double dy = dy0; (dy < dy1); dy += 1.0)
            {
                checkCancelled(stop);
                double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
                write_offset = (((dy * W) + dx0) * 2.0);
                for (double dx = dx0; (dx < dx1); dx += 1.0, write_offset += 2.0)
                {
                    double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                    double rad_sq = ((((fdx - cx) * (fdx - cx)) + ((fdy - cy) * (fdy - cy))) * (1.0 / (rad * rad)));
                    if ((rad_sq < 1.0))
                    {
                        double r = std::pow(rad_sq, 0.333);
                        double sdx = fdx;
                        double sdy = fdy;
                        r = smoothstep(r);
                        double zx = (fdx - cx);
                        double zy = (fdy - cy);
                        double rot = ((1.0 - r) * str);
                        double zx2 = ((zx * std::cos(rot)) - (zy * std::sin(rot)));
                        double zy2 = ((zx * std::sin(rot)) + (zy * std::cos(rot)));
                        sdx = (cx + zx2);
                        sdy = (cy + zy2);
                        src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) += (sdx - fdx);
                        src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) += (sdy - fdy);
                    }
                }
            }
        }
    }
    else
    if (new_mode == 5.0)
    {
        warp_prefs.stars_prob *= 0.5;
        warp_prefs.stars_follow_motion_min = kFollowMotionNone;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_pow = 3.0;
        const double ordered_15 = rand_idx;
        rand_idx += 1.0;
        const double N = ((0.5) + (double(randoms.at(checkedIndex(ordered_15, randoms.size()))) * 3.5));
        const double ordered_16 = rand_idx;
        rand_idx += 1.0;
        const double offset_x = (double(randoms.at(checkedIndex(ordered_16, randoms.size()))) * (1.0 / N));
        const double ordered_17 = rand_idx;
        rand_idx += 1.0;
        const double offset_y = (double(randoms.at(checkedIndex(ordered_17, randoms.size()))) * (1.0 / N));
        const double ordered_20 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_21 = ((0.3) * double(randoms.at(checkedIndex(ordered_20, randoms.size()))));
        const double ordered_22 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_23 = ((0.1) + (ordered_21 * double(randoms.at(checkedIndex(ordered_22, randoms.size())))));
        const double ordered_24 = rand_idx;
        rand_idx += 1.0;
        const double str = ((ordered_23 * ((double(randoms.at(checkedIndex(ordered_24, randoms.size()))) > 0.5) ? 1.0 : (-1.0))) * 0.25);
        const double ordered_25 = rand_idx;
        rand_idx += 1.0;
        const double rot0 = (double(randoms.at(checkedIndex(ordered_25, randoms.size()))) * 6.28);
        const double ordered_26 = rand_idx;
        rand_idx += 1.0;
        const double variety = (double(randoms.at(checkedIndex(ordered_26, randoms.size()))) > 0.5);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            fdy += offset_y;
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                fdx += offset_x;
                auto p = ApplyRot2D(fdx, fdy, rot0);
                double fx = p.x;
                double fy = p.y;
                double nx = std::floor((fx * N));
                double ny = std::floor((fy * N));
                double k = jsBitOr(((nx * 3.0) + (ny * 7.0)), 0.0);
                double att = (variety ? ((double(randoms.at(checkedIndex(jsBitAnd(k, WarpMapRandomNumberMask), randoms.size()))) * 2.0) - 1.0) : 1.0);
                double cx = ((nx + 0.5) * (1.0 / N));
                double cy = ((ny + 0.5) * (1.0 / N));
                double rad_sq = ((((fx - cx) * (fx - cx)) + ((fy - cy) * (fy - cy))) * ((N * N) * 4.0));
                double r = std::pow(rad_sq, 0.333);
                double sdx = fx;
                double sdy = fy;
                if ((r < 1.0))
                {
                    r = smoothstep(r);
                    double zx = (fx - cx);
                    double zy = (fy - cy);
                    double rot = ((1.0 - r) * str);
                    double zx2 = ((zx * std::cos((rot * att))) - (zy * std::sin((rot * att))));
                    double zy2 = ((zx * std::sin((rot * att))) + (zy * std::cos((rot * att))));
                    sdx = (cx + zx2);
                    sdy = (cy + zy2);
                }
                sdx -= fx;
                sdy -= fy;
                auto p2 = ApplyRot2D(sdx, sdy, (-rot0));
                sdx = p2.x;
                sdy = p2.y;
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = sdx;
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = sdy;
            }
        }
    }
    else
    if (new_mode == 6.0)
    {
        warp_prefs.stars_prob *= 0.5;
        warp_prefs.stars_follow_motion_min = kFollowMotionNone;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_pow = 3.0;
        const double ordered_28 = rand_idx;
        rand_idx += 1.0;
        const double N = ((2.0) + (double(randoms.at(checkedIndex(ordered_28, randoms.size()))) * 8.0));
        const double ordered_29 = rand_idx;
        rand_idx += 1.0;
        const double offset_x = (double(randoms.at(checkedIndex(ordered_29, randoms.size()))) * (1.0 / N));
        const double ordered_30 = rand_idx;
        rand_idx += 1.0;
        const double offset_y = (double(randoms.at(checkedIndex(ordered_30, randoms.size()))) * (1.0 / N));
        const double scale = 1.0;
        const double ordered_33 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_34 = ((0.002) + ((0.028) * double(randoms.at(checkedIndex(ordered_33, randoms.size())))));
        const double ordered_35 = rand_idx;
        rand_idx += 1.0;
        const double rot = ((ordered_34 * ((double(randoms.at(checkedIndex(ordered_35, randoms.size()))) < 0.5) ? 1.0 : (-1.0))) * std::min(1.0, (4.0 / N)));
        const double ordered_38 = rand_idx;
        rand_idx += 1.0;
        const double power = ((0.25) + ((0.25) * double(randoms.at(checkedIndex(ordered_38, randoms.size())))));
        const double ordered_41 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_42 = ((double(randoms.at(checkedIndex(ordered_41, randoms.size()))) * 3.0) - 1.0);
        const auto ordered_43 = std::min((1.0), ordered_42);
        const double variety = std::max((0.0), ordered_43);
        const double ordered_44 = rand_idx;
        rand_idx += 1.0;
        const double rot0 = (double(randoms.at(checkedIndex(ordered_44, randoms.size()))) * 6.28);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double orig_fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double orig_fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                auto p = ApplyRot2D(orig_fdx, orig_fdy, rot0);
                double fdx = p.x;
                double fdy = p.y;
                fdx += offset_x;
                fdx = ((fdx * N) * 0.5);
                const double nx = std::floor(fdx);
                fdx -= nx;
                fdx = ((fdx * 2.0) - 1.0);
                fdy += offset_y;
                fdy = ((fdy * N) * 0.5);
                const double ny = std::floor(fdy);
                fdy -= ny;
                fdy = ((fdy * 2.0) - 1.0);
                const double k = jsBitOr(((nx * 3.0) + (ny * 7.0)), 0.0);
                const double tx = fdx;
                const double ty = fdy;
                double rad = std::sqrt(((tx * tx) + (ty * ty)));
                rad = std::pow(rad, power);
                const double rot2 = (rot * std::max(0.0, (1.0 - rad)));
                const double rot3 = (rot2 * lerp(1.0, ((double(randoms.at(checkedIndex(jsBitAnd(k, WarpMapRandomNumberMask), randoms.size()))) * 2.0) - 1.0), variety));
                double sdx = (((fdx * std::cos(rot3)) - (fdy * std::sin(rot3))) * scale);
                double sdy = (((fdx * std::sin(rot3)) + (fdy * std::cos(rot3))) * scale);
                sdx -= fdx;
                sdy -= fdy;
                auto p2 = ApplyRot2D(sdx, sdy, (-rot0));
                sdx = p2.x;
                sdy = p2.y;
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = sdx;
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = sdy;
            }
        }
    }
    else
    if (new_mode == 10.0)
    {
        warp_prefs.stars_follow_motion_min = kFollowMotionNone;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        const double ordered_47 = rand_idx;
        rand_idx += 1.0;
        double str = ((0.4) + ((1.4) * double(randoms.at(checkedIndex(ordered_47, randoms.size())))));
        const double ordered_48 = rand_idx;
        rand_idx += 1.0;
        if ((double(randoms.at(checkedIndex(ordered_48, randoms.size()))) < 0.3))
        {
            str = (str * (-0.25));
        }
        const double ordered_50 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_51 = (double(randoms.at(checkedIndex(ordered_50, randoms.size()))) * 6.0);
        const double fins = ((3.0) + std::floor(ordered_51));
        const double ordered_52 = rand_idx;
        rand_idx += 1.0;
        const double cx = (((double(randoms.at(checkedIndex(ordered_52, randoms.size()))) * 2.0) - 1.0) * 0.12);
        const double ordered_53 = rand_idx;
        rand_idx += 1.0;
        const double cy = (((double(randoms.at(checkedIndex(ordered_53, randoms.size()))) * 2.0) - 1.0) * 0.12);
        AdjustCenter(cx, cy, warp_prefs);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double tx = (fdx - cx);
                const double ty = (fdy - cy);
                const double ang = std::atan2(tx, ty);
                const double scale = (0.987 + (0.01 * std::cos((ang * fins))));
                double sdx = (fdx * scale);
                double sdy = (fdy * scale);
                const double rad = std::sqrt(((tx * tx) + (ty * ty)));
                const double att = std::max(0.0, std::min(1.0, ((rad - 0.05) * 4.0)));
                sdx = (fdx + (((sdx - fdx) * att) * str));
                sdy = (fdy + (((sdy - fdy) * att) * str));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 9.0)
    {
        warp_prefs.circ_scale = 4.0;
        warp_prefs.stars_follow_motion_min = kFollowMotionNone;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        const double ordered_54 = rand_idx;
        rand_idx += 1.0;
        const double cx = (((double(randoms.at(checkedIndex(ordered_54, randoms.size()))) * 2.0) - 1.0) * 0.25);
        const double ordered_55 = rand_idx;
        rand_idx += 1.0;
        const double cy = (((double(randoms.at(checkedIndex(ordered_55, randoms.size()))) * 2.0) - 1.0) * 0.25);
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_58 = rand_idx;
        rand_idx += 1.0;
        const double freq = ((6.0) + ((18.0) * double(randoms.at(checkedIndex(ordered_58, randoms.size())))));
        const double ordered_59 = rand_idx;
        rand_idx += 1.0;
        const double phase = (double(randoms.at(checkedIndex(ordered_59, randoms.size()))) * 6.28);
        const double ordered_62 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_63 = ((((6e-05) + ((0.00011) * double(randoms.at(checkedIndex(ordered_62, randoms.size()))))) / freq) * 100.0);
        const double ordered_64 = rand_idx;
        rand_idx += 1.0;
        const double mag = ((ordered_63 * ((double(randoms.at(checkedIndex(ordered_64, randoms.size()))) < 0.5) ? 1.0 : (-1.0))) * 2.0);
        const double ordered_65 = rand_idx;
        rand_idx += 1.0;
        const double bias = ((double(randoms.at(checkedIndex(ordered_65, randoms.size()))) * 2.0) - 1.0);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double tx = (fdx - cx);
                const double ty = (fdy - cy);
                const double rad = std::sqrt(((tx * tx) + (ty * ty)));
                const double scale = (1.0 + (mag * (std::cos(((rad * freq) + phase)) + bias)));
                double sdx = (fdx * scale);
                double sdy = (fdy * scale);
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 8.0)
    {
        warp_prefs.circ_scale = 4.0;
        warp_prefs.stars_follow_motion_min = kFollowMotionNone;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_pow = 3.0;
        const double ordered_66 = rand_idx;
        rand_idx += 1.0;
        const double cx = (((double(randoms.at(checkedIndex(ordered_66, randoms.size()))) * 2.0) - 1.0) * 0.45);
        const double ordered_67 = rand_idx;
        rand_idx += 1.0;
        const double cy = (((double(randoms.at(checkedIndex(ordered_67, randoms.size()))) * 2.0) - 1.0) * 0.45);
        warp_prefs.motion_cx = cx;
        warp_prefs.motion_cy = cy;
        const double ordered_70 = rand_idx;
        rand_idx += 1.0;
        const double rad_freq = ((5.0) + ((19.0) * double(randoms.at(checkedIndex(ordered_70, randoms.size())))));
        const double ordered_73 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_74 = ((0.004) * double(randoms.at(checkedIndex(ordered_73, randoms.size()))));
        const double ordered_75 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_76 = ((0.001) + (ordered_74 * double(randoms.at(checkedIndex(ordered_75, randoms.size())))));
        const double ordered_77 = rand_idx;
        rand_idx += 1.0;
        const double rot_str = ((((ordered_76 * ((double(randoms.at(checkedIndex(ordered_77, randoms.size()))) < 0.5) ? (-1.0) : 1.0)) * 0.21) * std::pow((13.0 / rad_freq), 0.6)) * 4.0);
        const double rot_bias = ((double(randoms.at(checkedIndex(rand_idx, randoms.size()))) * 2.0) - 1.0);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double tx = (fdx - cx);
                const double ty = (fdy - cy);
                const double rad = std::sqrt(((tx * tx) + (ty * ty)));
                const double rot = ((std::cos((rad * rad_freq)) + rot_bias) * rot_str);
                double sdx = (((tx * std::cos(rot)) - (ty * std::sin(rot))) + cx);
                double sdy = (((tx * std::sin(rot)) + (ty * std::cos(rot))) + cy);
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 2.0)
    {
        warp_prefs.wave_prefs_weight *= 5.0;
        const double ordered_81 = rand_idx;
        rand_idx += 1.0;
        double str = ((0.027) * ((0.1) + ((0.9) * double(randoms.at(checkedIndex(ordered_81, randoms.size()))))));
        const double ordered_82 = rand_idx;
        rand_idx += 1.0;
        double angle = (double(randoms.at(checkedIndex(ordered_82, randoms.size()))) * 6.28);
        const double cos_angle = std::cos(angle);
        const double sin_angle = std::sin(angle);
        const double ordered_83 = rand_idx;
        rand_idx += 1.0;
        const double rot = (((double(randoms.at(checkedIndex(ordered_83, randoms.size()))) * 2.0) - 1.0) * 0.0015);
        const double cos_rot = std::cos(rot);
        const double sin_rot = std::sin(rot);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                double rx = ((fdx * cos_angle) - (fdy * sin_angle));
                double ry = ((fdx * sin_angle) + (fdy * cos_angle));
                double sdx = fdx;
                double sdy = fdy;
                double zoom = (1.0 - ((1.0 / ((-ry) + 1.4)) * str));
                sdx *= zoom;
                sdy *= zoom;
                rx = ((sdx * cos_rot) - (sdy * sin_rot));
                ry = ((sdx * sin_rot) + (sdy * cos_rot));
                sdx = rx;
                sdy = ry;
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 15.0)
    {
        warp_prefs.wave_prefs_weight *= 10.0;
        warp_prefs.circ_rad_lo = 0.9;
        warp_prefs.circ_rad_hi = 1.5;
        const double ordered_86 = rand_idx;
        rand_idx += 1.0;
        double str = ((0.75) + ((1.5) * double(randoms.at(checkedIndex(ordered_86, randoms.size())))));
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                double sdx = fdx;
                double sdy = fdy;
                double rad = std::sqrt(((fdx * fdx) + (fdy * fdy)));
                double scale = (0.97 + (0.1 * rad));
                scale = std::pow(scale, 4.0);
                sdx *= scale;
                sdy *= scale;
                sdx = (fdx + ((sdx - fdx) * (-str)));
                sdy = (fdy + ((sdy - fdy) * (-str)));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 0.0)
    {
        warp_prefs.wave_prefs_weight *= 10.0;
        const double ordered_89 = rand_idx;
        rand_idx += 1.0;
        double zoom = ((0.004) + ((0.016) * double(randoms.at(checkedIndex(ordered_89, randoms.size())))));
        const double ordered_90 = rand_idx;
        rand_idx += 1.0;
        if ((double(randoms.at(checkedIndex(ordered_90, randoms.size()))) < 0.1))
        {
            zoom *= (-0.25);
        }
        const double ordered_91 = rand_idx;
        rand_idx += 1.0;
        const double rot = (((double(randoms.at(checkedIndex(ordered_91, randoms.size()))) * 2.0) - 1.0) * 0.0015);
        const double cos_rot = std::cos(rot);
        const double sin_rot = std::sin(rot);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                double sdx = (fdx + ((0.0 - fdx) * zoom));
                double sdy = (fdy + ((0.0 - fdy) * zoom));
                double rx = ((sdx * cos_rot) - (sdy * sin_rot));
                double ry = ((sdx * sin_rot) + (sdy * cos_rot));
                sdx = rx;
                sdy = ry;
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 1.0)
    {
        warp_prefs.wave_prefs_weight *= 10.0;
        const double ordered_94 = rand_idx;
        rand_idx += 1.0;
        double zoom = ((((0.4) + ((2.7) * double(randoms.at(checkedIndex(ordered_94, randoms.size()))))) * 0.03) * 1.8);
        const double ordered_95 = rand_idx;
        rand_idx += 1.0;
        if ((double(randoms.at(checkedIndex(ordered_95, randoms.size()))) < 0.01))
        {
            zoom *= (-0.4);
        }
        const double ordered_96 = rand_idx;
        rand_idx += 1.0;
        const double rot = (((double(randoms.at(checkedIndex(ordered_96, randoms.size()))) * 2.0) - 1.0) * 0.0015);
        const double cos_rot = std::cos(rot);
        const double sin_rot = std::sin(rot);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                double r = ((fdx * fdx) + (fdy * fdy));
                double sdx = (fdx + (((0.0 - fdx) * r) * zoom));
                double sdy = (fdy + (((0.0 - fdy) * r) * zoom));
                double rx = ((sdx * cos_rot) - (sdy * sin_rot));
                double ry = ((sdx * sin_rot) + (sdy * cos_rot));
                sdx = rx;
                sdy = ry;
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 16.0)
    {
        warp_prefs.wave_prefs_weight *= 0.01;
        warp_prefs.stars_follow_motion_min = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        const double ordered_99 = rand_idx;
        rand_idx += 1.0;
        double rot1 = ((0.002) + ((0.008) * double(randoms.at(checkedIndex(ordered_99, randoms.size())))));
        const double ordered_100 = rand_idx;
        rand_idx += 1.0;
        if ((double(randoms.at(checkedIndex(ordered_100, randoms.size()))) < 0.5))
        rot1 *= (-1.0);
        const double ordered_103 = rand_idx;
        rand_idx += 1.0;
        double rot2 = (((0.002) + ((0.008) * double(randoms.at(checkedIndex(ordered_103, randoms.size()))))) * 2.0);
        const double ordered_104 = rand_idx;
        rand_idx += 1.0;
        if ((double(randoms.at(checkedIndex(ordered_104, randoms.size()))) < 0.5))
        {
            rot2 *= (-1.0);
        }
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double rad = std::sqrt(((fdx * fdx) + (fdy * fdy)));
                const double rot = (rot1 + ((rot2 - rot1) * rad));
                double sdx = ((fdx * std::cos(rot)) - (fdy * std::sin(rot)));
                double sdy = ((fdx * std::sin(rot)) + (fdy * std::cos(rot)));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 17.0)
    {
        warp_prefs.wave_prefs_weight *= 10.0;
        const double speed_min = 0.015;
        const double speed_max = 0.16;
        const double speed_pow = 2.0;
        const double ordered_105 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_106 = double(randoms.at(checkedIndex(ordered_105, randoms.size())));
        const double t = std::pow(ordered_106, (3.0));
        const auto ordered_108 = speed_min;
        const auto ordered_109 = speed_max;
        const double ordered_110 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_111 = double(randoms.at(checkedIndex(ordered_110, randoms.size())));
        const auto ordered_112 = speed_pow;
        const auto ordered_113 = std::pow(ordered_111, ordered_112);
        const double speed = LinearInterp(ordered_108, ordered_109, ordered_113);
        const double ordered_114 = rand_idx;
        rand_idx += 1.0;
        const double rot = (((double(randoms.at(checkedIndex(ordered_114, randoms.size()))) * 2.0) - 1.0) * 0.0015);
        const double cos_rot = std::cos(rot);
        const double sin_rot = std::sin(rot);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                double orig_rad = std::sqrt((((fdx * fdx) + (fdy * fdy)) + 1e-05));
                double inv_rad = (1.0 / orig_rad);
                double nx = (fdx * inv_rad);
                double ny = (fdy * inv_rad);
                double dist = inv_rad;
                dist += speed;
                double rad = (1.0 / dist);
                double sdx = (fdx + (nx * (rad - orig_rad)));
                double sdy = (fdy + (ny * (rad - orig_rad)));
                double rx = ((sdx * cos_rot) - (sdy * sin_rot));
                double ry = ((sdx * sin_rot) + (sdy * cos_rot));
                sdx = rx;
                sdy = ry;
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 12.0)
    {
        warp_prefs.wave_prefs_weight *= 10.0;
        warp_prefs.stars_follow_motion_min = kFollowMotionLow;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        const double ordered_117 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_118 = double(randoms.at(checkedIndex(ordered_117, randoms.size())));
        const double power = LinearInterp((1.03), (1.1), ordered_118);
        const double ordered_119 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_120 = double(randoms.at(checkedIndex(ordered_119, randoms.size())));
        const double ordered_121 = rand_idx;
        rand_idx += 1.0;
        const double t = ((ordered_120 + double(randoms.at(checkedIndex(ordered_121, randoms.size())))) * 0.5);
        const double scale = LogInterp(1.5, 4.0, t);
        const double inv_scale = (1.0 / scale);
        const double ordered_124 = rand_idx;
        rand_idx += 1.0;
        const double str = ((0.1) + ((0.4) * double(randoms.at(checkedIndex(ordered_124, randoms.size())))));
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                double orig_rad = std::sqrt((((fdx * fdx) + (fdy * fdy)) + 1e-05));
                double rad = (std::pow((orig_rad * scale), power) * inv_scale);
                double inv_rad = (1.0 / orig_rad);
                double nx = (fdx * inv_rad);
                double ny = (fdy * inv_rad);
                double sdx = (fdx + ((nx * (rad - orig_rad)) * str));
                double sdy = (fdy + ((ny * (rad - orig_rad)) * str));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 20.0)
    {
        warp_prefs.wave_prefs_weight *= 10.0;
        warp_prefs.stars_follow_motion_min = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        const double ordered_127 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_128 = double(randoms.at(checkedIndex(ordered_127, randoms.size())));
        const auto ordered_129 = LinearInterp((0.003), (0.01), ordered_128);
        const double ordered_130 = rand_idx;
        rand_idx += 1.0;
        const double speed = (ordered_129 * ((double(randoms.at(checkedIndex(ordered_130, randoms.size()))) > 0.5) ? 1.0 : (-1.0)));
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double rad = std::sqrt(((fdx * fdx) + (fdy * fdy)));
                double nx = fdy;
                double ny = (-fdx);
                double nx_sign = ((nx < 0.0) ? (-1.0) : 1.0);
                double ny_sign = ((ny < 0.0) ? (-1.0) : 1.0);
                nx *= (nx * nx_sign);
                ny *= (ny * ny_sign);
                const double norm_scale = (1.0 / std::sqrt((((nx * nx) + (ny * ny)) + 1e-05)));
                nx *= norm_scale;
                ny *= norm_scale;
                double sdx = (fdx + ((nx * speed) * rad));
                double sdy = (fdy + ((ny * speed) * rad));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 30.0)
    {
        const double ordered_131 = rand_idx;
        rand_idx += 1.0;
        const double cx = (((double(randoms.at(checkedIndex(ordered_131, randoms.size()))) * 2.0) - 1.0) * 0.08);
        const double ordered_132 = rand_idx;
        rand_idx += 1.0;
        const double cy = (((double(randoms.at(checkedIndex(ordered_132, randoms.size()))) * 2.0) - 1.0) * 0.08);
        AdjustCenter(cx, cy, warp_prefs);
        warp_prefs.stars_follow_motion_min = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        const double ordered_135 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_136 = double(randoms.at(checkedIndex(ordered_135, randoms.size())));
        const auto ordered_137 = LinearInterp((0.003), (0.01), ordered_136);
        const double ordered_138 = rand_idx;
        rand_idx += 1.0;
        const double str = (ordered_137 * ((double(randoms.at(checkedIndex(ordered_138, randoms.size()))) > 0.5) ? 1.0 : (-1.0)));
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double tx = (fdx - cx);
                const double ty = (fdy - cy);
                const double rad = std::sqrt(((tx * tx) + (ty * ty)));
                const double rot = (rad * str);
                const auto rotated = ApplyRot2D(tx, ty, rot);
                double sdx = (cx + rotated.x);
                double sdy = (cy + rotated.y);
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 34.0)
    {
        warp_prefs.motion_cx = 0.0;
        warp_prefs.motion_cy = 0.0;
        warp_prefs.fading_dots_prob *= 1.8;
        warp_prefs.wave_prefs_weight *= 10.0;
        const double ordered_141 = rand_idx;
        rand_idx += 1.0;
        double r1 = ((0.2) + ((0.4) * double(randoms.at(checkedIndex(ordered_141, randoms.size())))));
        const double ordered_144 = rand_idx;
        rand_idx += 1.0;
        double r2 = ((0.25) + ((0.45) * double(randoms.at(checkedIndex(ordered_144, randoms.size())))));
        const double ordered_147 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_148 = double(randoms.at(checkedIndex(ordered_147, randoms.size())));
        const auto ordered_150 = ((0.3) + ((0.7) * std::pow(ordered_148, (0.5))));
        const double ordered_151 = rand_idx;
        rand_idx += 1.0;
        double rot = (ordered_150 * ((double(randoms.at(checkedIndex(ordered_151, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double ordered_153 = rand_idx;
        rand_idx += 1.0;
        double inward_speed = ((0.03) + (double(randoms.at(checkedIndex(ordered_153, randoms.size()))) * 0.3));
        const double ordered_156 = rand_idx;
        rand_idx += 1.0;
        double fall_accel = ((0.05) + ((0.42) * double(randoms.at(checkedIndex(ordered_156, randoms.size())))));
        const double ordered_159 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_160 = ((10.0) + ((20.0) * double(randoms.at(checkedIndex(ordered_159, randoms.size())))));
        const double ordered_162 = rand_idx;
        rand_idx += 1.0;
        double elev_degrees = (ordered_160 + ((20.0) * double(randoms.at(checkedIndex(ordered_162, randoms.size())))));
        const auto stats = fillWormholeMap(src_dxy, uint32_t(W), uint32_t(H), WormholeParameters{.r1 = r1, .r2 = r2, .rotation = rot, .inward_speed = inward_speed, .fall_acceleration = fall_accel, .elevation_degrees = elev_degrees}, stop);
    }
    else
    if (new_mode == 35.0)
    {
        is_dynamic = true;
        warp_prefs.fading_dots_prob = 0.8;
        warp_prefs.wave_prefs_weight = 10.0;
        warp_prefs.use_motion_metadata_to_choose_wave_type = 0.0;
        warp_prefs.wave_flat_prob = 0.0;
        warp_prefs.wave_circ_prob = 1.0;
        warp_prefs.wave_flat_plus_circ_prob = 0.0;
    }
    else
    if (new_mode == 36.0)
    {
        const double ordered_165 = rand_idx;
        rand_idx += 1.0;
        const double str = ((0.2) + ((0.8) * double(randoms.at(checkedIndex(ordered_165, randoms.size())))));
        const double ordered_167 = rand_idx;
        rand_idx += 1.0;
        const double rad_scale = ((4.0) + (double(randoms.at(checkedIndex(ordered_167, randoms.size()))) * 10.0));
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double rad = std::sqrt(((fdx * fdx) + (fdy * fdy)));
                const double band = jsBitOr(std::floor((rad * rad_scale)), 0.0);
                const double rot = ((((double(randoms.at(checkedIndex((16.0 + band), randoms.size()))) * 2.0) - 1.0) * 0.02) * str);
                const auto rotated = ApplyRot2D(fdx, fdy, rot);
                double sdx = rotated.x;
                double sdy = rotated.y;
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 37.0)
    {
        is_dynamic = true;
        warp_prefs.stars_follow_motion_min = kFollowMotionLow;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_pow = 3.0;
    }
    else
    if (new_mode == 38.0)
    {
        is_dynamic = true;
    }
    else
    if (new_mode == 39.0)
    {
        is_dynamic = true;
    }
    else
    if (new_mode == 22.0)
    {
        warp_prefs.wave_prefs_weight *= 10.0;
        warp_prefs.stars_follow_motion_min = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        const double ordered_168 = rand_idx;
        rand_idx += 1.0;
        const double theta = (double(randoms.at(checkedIndex(ordered_168, randoms.size()))) * 3.14);
        const double cos_theta = std::cos(theta);
        const double sin_theta = std::sin(theta);
        const double cos_minus_theta = std::cos((-theta));
        const double sin_minus_theta = std::sin((-theta));
        const double ordered_171 = rand_idx;
        rand_idx += 1.0;
        const double dz = (((0.015) + ((0.015) * double(randoms.at(checkedIndex(ordered_171, randoms.size()))))) * 1.0);
        const double ordered_174 = rand_idx;
        rand_idx += 1.0;
        const double plane_y = ((1.01) + ((0.5) * double(randoms.at(checkedIndex(ordered_174, randoms.size())))));
        const double ordered_175 = rand_idx;
        rand_idx += 1.0;
        const double wave_offset = (((double(randoms.at(checkedIndex(ordered_175, randoms.size()))) * 2.0) - 1.0) * 0.6);
        const double cx = (wave_offset * std::cos(((-theta) + (std::numbers::pi / 2.0))));
        const double cy = (wave_offset * std::sin(((-theta) + (std::numbers::pi / 2.0))));
        const double ordered_176 = rand_idx;
        rand_idx += 1.0;
        if ((double(randoms.at(checkedIndex(ordered_176, randoms.size()))) < 0.99))
        {
            warp_prefs.flat_angle_lo = ((-theta) + (3.1415927 * 2.0));
            warp_prefs.flat_angle_hi = ((-theta) + (3.1415927 * 2.0));
            warp_prefs.flat_cx_lo = cx;
            warp_prefs.flat_cx_hi = cx;
            warp_prefs.flat_cy_lo = cy;
            warp_prefs.flat_cy_hi = cy;
            warp_prefs.flat_angle_bias_toward_horizontal_angles = 0.0;
            warp_prefs.flat_stereo_sep_lo *= 0.5;
            warp_prefs.flat_stereo_sep_hi *= 0.7;
            warp_prefs.flat_stereo_amplitude_scale *= 0.6;
            warp_prefs.circ_cx_lo = cx;
            warp_prefs.circ_cx_hi = cx;
            warp_prefs.circ_cy_lo = cy;
            warp_prefs.circ_cy_hi = cy;
        }
        warp_prefs.motion_cx = cx;
        warp_prefs.motion_cy = cy;
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                double rrx = (fdx - cx);
                double rry = (fdy - cy);
                double rx = ((rrx * cos_theta) - (rry * sin_theta));
                double ry = ((rrx * sin_theta) + (rry * cos_theta));
                double sx = rx;
                double sy = ry;
                double sz = 1.0;
                double vx = sx;
                double vy = sy;
                double vz = sz;
                double v_norm_scale = (1.0 / std::sqrt((((vx * vx) + (vy * vy)) + (vz * vz))));
                vx *= v_norm_scale;
                vy *= v_norm_scale;
                vz *= v_norm_scale;
                const double which_plane_y = (plane_y * ((ry < 0.0) ? (-1.0) : 1.0));
                double t = (which_plane_y / vy);
                double ix = (vx * t);
                double iy = (vy * t);
                double iz = (vz * t);
                iz += dz;
                vx = ix;
                vy = iy;
                vz = iz;
                v_norm_scale = (1.0 / std::sqrt((((vx * vx) + (vy * vy)) + (vz * vz))));
                vx *= v_norm_scale;
                vy *= v_norm_scale;
                vz *= v_norm_scale;
                t = (1.0 / vz);
                double fx = (vx * t);
                double fy = (vy * t);
                double sdx = ((fx * cos_minus_theta) - (fy * sin_minus_theta));
                double sdy = ((fx * sin_minus_theta) + (fy * cos_minus_theta));
                sdx += cx;
                sdy += cy;
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 3.0)
    {
        warp_prefs.stars_prob *= 0.5;
        warp_prefs.stars_follow_motion_min = kFollowMotionNone;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_pow = 3.0;
        const double N = 15.0;
        const double ordered_179 = rand_idx;
        rand_idx += 1.0;
        const double str = (((0.4) + ((0.6) * double(randoms.at(checkedIndex(ordered_179, randoms.size()))))) * 0.75);
        auto theta = floatArray(N);
        auto cos_theta = floatArray(N);
        auto sin_theta = floatArray(N);
        auto freq = floatArray(N);
        auto amp = floatArray(N);
        const double ordered_180 = rand_idx;
        rand_idx += 1.0;
        const double base_theta = double(randoms.at(checkedIndex(ordered_180, randoms.size())));
        double theta_sum = 0.0;
        for (double i = 0.0; (i < N); i += 1.0)
        {
            theta.at(checkedIndex(i, theta.size())) = ((base_theta + (i / N)) * 6.28);
            cos_theta.at(checkedIndex(i, cos_theta.size())) = std::cos(double(theta.at(checkedIndex(i, theta.size()))));
            sin_theta.at(checkedIndex(i, sin_theta.size())) = std::sin(double(theta.at(checkedIndex(i, theta.size()))));
            const double ordered_181 = rand_idx;
            rand_idx += 1.0;
            amp.at(checkedIndex(i, amp.size())) = ((((double(randoms.at(checkedIndex(ordered_181, randoms.size()))) * str) * 0.002) * 0.5) * 1.3);
            const double ordered_182 = rand_idx;
            rand_idx += 1.0;
            freq.at(checkedIndex(i, freq.size())) = ((double(randoms.at(checkedIndex(ordered_182, randoms.size()))) * 12.0) * 2.0);
            theta_sum += double(theta.at(checkedIndex(i, theta.size())));
        }
        const double cos_undo = std::cos((-theta_sum));
        const double sin_undo = std::sin((-theta_sum));
        const double SCALE = 4.0;
        const double W2 = jsBitOr(((W / SCALE) + 1.0), 0.0);
        const double H2 = jsBitOr(((H / SCALE) + 1.0), 0.0);
        auto temp_x = floatArray((W2 * H2));
        auto temp_y = floatArray((W2 * H2));
        for (double dy = 0.0; (dy < H2); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H2)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W2); dx += 1.0)
            {
                double fdx = (((dx * (2.0 / W2)) - 1.0) * (x0x1 * 0.5));
                double x = fdx;
                double y = fdy;
                for (double i = 0.0; (i < N); i += 1.0)
                {
                    double x2 = ((x * double(cos_theta.at(checkedIndex(i, cos_theta.size())))) - (y * double(sin_theta.at(checkedIndex(i, sin_theta.size())))));
                    double y2 = ((x * double(sin_theta.at(checkedIndex(i, sin_theta.size())))) + (y * double(cos_theta.at(checkedIndex(i, cos_theta.size())))));
                    x = x2;
                    y = y2;
                    x += (std::cos((y * double(freq.at(checkedIndex(i, freq.size()))))) * double(amp.at(checkedIndex(i, amp.size()))));
                }
                double sdx = ((x * cos_undo) - (y * sin_undo));
                double sdy = ((x * sin_undo) + (y * cos_undo));
                temp_x.at(checkedIndex(((dy * W2) + dx), temp_x.size())) = (sdx - fdx);
                temp_y.at(checkedIndex(((dy * W2) + dx), temp_y.size())) = (sdy - fdy);
            }
        }
        write_offset = 0.0;
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fsy = ((dy * (H2 - 1.0)) / H);
            const double sy = std::floor(fsy);
            const double fy = (fsy - sy);
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fsx = ((dx * (W2 - 1.0)) / W);
                const double sx = std::floor(fsx);
                const double fx = (fsx - sx);
                const double o1 = (((sy + 0.0) * W2) + (sx + 0.0));
                const double o2 = (((sy + 0.0) * W2) + (sx + 1.0));
                const double o3 = (((sy + 1.0) * W2) + (sx + 0.0));
                const double o4 = (((sy + 1.0) * W2) + (sx + 1.0));
                const double w1 = ((1.0 - fy) * (1.0 - fx));
                const double w2 = ((1.0 - fy) * fx);
                const double w3 = (fy * (1.0 - fx));
                const double w4 = (fy * fx);
                const double vx = ((((double(temp_x.at(checkedIndex(o1, temp_x.size()))) * w1) + (double(temp_x.at(checkedIndex(o2, temp_x.size()))) * w2)) + (double(temp_x.at(checkedIndex(o3, temp_x.size()))) * w3)) + (double(temp_x.at(checkedIndex(o4, temp_x.size()))) * w4));
                const double vy = ((((double(temp_y.at(checkedIndex(o1, temp_y.size()))) * w1) + (double(temp_y.at(checkedIndex(o2, temp_y.size()))) * w2)) + (double(temp_y.at(checkedIndex(o3, temp_y.size()))) * w3)) + (double(temp_y.at(checkedIndex(o4, temp_y.size()))) * w4));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = vx;
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = vy;
            }
        }
    }
    else
    if (new_mode == 7.0)
    {
        warp_prefs.stars_prob *= 0.5;
        warp_prefs.stars_follow_motion_min = kFollowMotionNone;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_pow = 3.0;
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = 0.0;
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = 0.0;
            }
        }
        const double ordered_185 = rand_idx;
        rand_idx += 1.0;
        const double N = jsBitOr(((2.0) + ((240.0) * double(randoms.at(checkedIndex(ordered_185, randoms.size()))))), 0.0);
        const double str_big = 0.03;
        const double ordered_188 = rand_idx;
        rand_idx += 1.0;
        const double str_lil = ((0.03) + ((0.05) * double(randoms.at(checkedIndex(ordered_188, randoms.size())))));
        for (double n = 0.0; (n < N); n += 1.0)
        {
            const double t = (n / (N - 1.0));
            const double ordered_189 = rand_idx;
            rand_idx += 1.0;
            const double cx = ((double(randoms.at(checkedIndex(ordered_189, randoms.size()))) - 0.5) * x0x1);
            const double ordered_190 = rand_idx;
            rand_idx += 1.0;
            const double cy = ((double(randoms.at(checkedIndex(ordered_190, randoms.size()))) - 0.5) * y0y1);
            const double rad = (0.8 - (0.75 * std::pow(t, 0.25)));
            const double ordered_193 = rand_idx;
            rand_idx += 1.0;
            const auto ordered_194 = ((0.2) + ((0.3) * double(randoms.at(checkedIndex(ordered_193, randoms.size())))));
            const double ordered_195 = rand_idx;
            rand_idx += 1.0;
            const double str = ((ordered_194 * ((double(randoms.at(checkedIndex(ordered_195, randoms.size()))) > 0.5) ? 1.0 : (-1.0))) * (str_big + ((str_lil - str_big) * t)));
            const double dx0 = jsBitOr(std::max(0.0, std::min(W, ((((cx - rad) * inv_x0x1) + 0.5) * W))), 0.0);
            const double dy0 = jsBitOr(std::max(0.0, std::min(H, ((((cy - rad) * inv_y0y1) + 0.5) * H))), 0.0);
            const double dx1 = jsBitOr(std::max(0.0, std::min(W, ((((cx + rad) * inv_x0x1) + 0.5) * W))), 0.0);
            const double dy1 = jsBitOr(std::max(0.0, std::min(H, ((((cy + rad) * inv_y0y1) + 0.5) * H))), 0.0);
            for (double dy = dy0; (dy < dy1); dy += 1.0)
            {
                checkCancelled(stop);
                double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
                write_offset = (((dy * W) + dx0) * 2.0);
                for (double dx = dx0; (dx < dx1); dx += 1.0, write_offset += 2.0)
                {
                    double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                    double rad_sq = ((((fdx - cx) * (fdx - cx)) + ((fdy - cy) * (fdy - cy))) * (1.0 / (rad * rad)));
                    if ((rad_sq < 1.0))
                    {
                        double r = std::pow(rad_sq, 0.333);
                        double sdx = fdx;
                        double sdy = fdy;
                        r = smoothstep(r);
                        double zx = (fdx - cx);
                        double zy = (fdy - cy);
                        double rot = ((1.0 - r) * str);
                        double zx2 = ((zx * std::cos(rot)) - (zy * std::sin(rot)));
                        double zy2 = ((zx * std::sin(rot)) + (zy * std::cos(rot)));
                        sdx = (cx + zx2);
                        sdy = (cy + zy2);
                        src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) += (sdx - fdx);
                        src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) += (sdy - fdy);
                    }
                }
            }
            if ((rand_idx > (double(randoms.size()) - 8.0)))
            {
                rand_idx = jsBitOr((double(randoms.at(checkedIndex(rand_idx, randoms.size()))) * 43.0), 0.0);
            }
        }
    }
    else
    if (new_mode == 18.0)
    {
        const auto ordered_196 = (-0.1);
        const double ordered_198 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_196 + ((0.2) * double(randoms.at(checkedIndex(ordered_198, randoms.size())))));
        const auto ordered_199 = (-0.1);
        const double ordered_201 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_199 + ((0.2) * double(randoms.at(checkedIndex(ordered_201, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_204 = rand_idx;
        rand_idx += 1.0;
        const double f1 = ((0.98) + ((0.02) * double(randoms.at(checkedIndex(ordered_204, randoms.size())))));
        const double ordered_207 = rand_idx;
        rand_idx += 1.0;
        const double f2 = (((0.0006) + ((0.0005) * double(randoms.at(checkedIndex(ordered_207, randoms.size()))))) * 80.0);
        const double ordered_211 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_212 = ((0.15) * ((0.01) + ((0.01) * double(randoms.at(checkedIndex(ordered_211, randoms.size()))))));
        const double ordered_213 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_212 * ((double(randoms.at(checkedIndex(ordered_213, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double r = (std::sqrt(((dx2 * dx2) + (dy2 * dy2))) * f2);
                const auto ordered_214 = (f1 - r);
                const double ordered_215 = rand_idx;
                rand_idx += 1.0;
                const double scale = (ordered_214 + (double(randoms.at(checkedIndex(ordered_215, randoms.size()))) * 0.01));
                rand_idx = std::fmod(rand_idx, (double(randoms.size()) - 3.0));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 44.0)
    {
        const auto ordered_216 = (-0.1);
        const double ordered_218 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_216 + ((0.2) * double(randoms.at(checkedIndex(ordered_218, randoms.size())))));
        const auto ordered_219 = (-0.1);
        const double ordered_221 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_219 + ((0.2) * double(randoms.at(checkedIndex(ordered_221, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_225 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_226 = ((0.15) * ((0.01) + ((0.015) * double(randoms.at(checkedIndex(ordered_225, randoms.size()))))));
        const double ordered_227 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_226 * ((double(randoms.at(checkedIndex(ordered_227, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double scale = (0.9875 - (dy2 * 0.03));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 13.0)
    {
        const auto ordered_228 = (-0.1);
        const double ordered_230 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_228 + ((0.2) * double(randoms.at(checkedIndex(ordered_230, randoms.size())))));
        const auto ordered_231 = (-0.1);
        const double ordered_233 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_231 + ((0.2) * double(randoms.at(checkedIndex(ordered_233, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_237 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_238 = ((0.15) * ((0.007) + ((0.02) * double(randoms.at(checkedIndex(ordered_237, randoms.size()))))));
        const double ordered_239 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_238 * ((double(randoms.at(checkedIndex(ordered_239, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double r = std::sqrt(((dx2 * dx2) + (dy2 * dy2)));
                const double scale = (0.975 + (r * 0.028));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 21.0)
    {
        const auto ordered_240 = (-0.1);
        const double ordered_242 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_240 + ((0.2) * double(randoms.at(checkedIndex(ordered_242, randoms.size())))));
        const auto ordered_243 = (-0.1);
        const double ordered_245 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_243 + ((0.2) * double(randoms.at(checkedIndex(ordered_245, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_248 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_249 = ((0.05) + ((0.05) * double(randoms.at(checkedIndex(ordered_248, randoms.size())))));
        const double ordered_251 = rand_idx;
        rand_idx += 1.0;
        const double f1 = ((ordered_249 + ((0.07) * double(randoms.at(checkedIndex(ordered_251, randoms.size()))))) * 0.25);
        const double ordered_254 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_255 = ((0.9975) - ((0.0025) * double(randoms.at(checkedIndex(ordered_254, randoms.size())))));
        const double ordered_257 = rand_idx;
        rand_idx += 1.0;
        const double f2 = (ordered_255 - ((0.005) * double(randoms.at(checkedIndex(ordered_257, randoms.size())))));
        const bool sqrtRadius = true;
        const double ordered_261 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_262 = ((0.15) * ((0.01) + ((0.03) * double(randoms.at(checkedIndex(ordered_261, randoms.size()))))));
        const double ordered_263 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_262 * ((double(randoms.at(checkedIndex(ordered_263, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double r = (std::sqrt(((dx2 * dx2) + (dy2 * dy2))) * 1.6);
                const double perspective = (sqrtRadius ? std::sqrt(r) : (r * 1.7));
                const double scale = (f2 - (f1 * perspective));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 11.0)
    {
        warp_prefs.stars_follow_motion_min = kFollowMotionNone;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_pow = 4.0;
        const auto ordered_264 = (-0.06);
        const double ordered_266 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_264 + ((0.12) * double(randoms.at(checkedIndex(ordered_266, randoms.size())))));
        const auto ordered_267 = (-0.06);
        const double ordered_269 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_267 + ((0.12) * double(randoms.at(checkedIndex(ordered_269, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_272 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_273 = ((1.4) + ((0.2) * double(randoms.at(checkedIndex(ordered_272, randoms.size())))));
        const double ordered_275 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_276 = double(randoms.at(checkedIndex(ordered_275, randoms.size())));
        const double f1 = (ordered_273 + ((2.7) * std::pow(ordered_276, (6.0))));
        const double ordered_279 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_280 = ((0.0075) * double(randoms.at(checkedIndex(ordered_279, randoms.size()))));
        const double ordered_281 = rand_idx;
        rand_idx += 1.0;
        const double turn = ((ordered_280 * ((double(randoms.at(checkedIndex(ordered_281, randoms.size()))) < 0.5) ? 1.0 : (-1.0))) * 0.5);
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        const double damping = 1.0;
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double r = (std::sqrt(((dx2 * dx2) + (dy2 * dy2))) * 320.0);
                const double scale = (0.98 + (0.017 * std::sin((std::sqrt(r) * f1))));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = ((sdx - fdx) * damping);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = ((sdy - fdy) * damping);
            }
        }
    }
    else
    if (new_mode == 40.0)
    {
        const auto ordered_282 = (-0.1);
        const double ordered_284 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_282 + ((0.2) * double(randoms.at(checkedIndex(ordered_284, randoms.size())))));
        const auto ordered_285 = (-0.1);
        const double ordered_287 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_285 + ((0.2) * double(randoms.at(checkedIndex(ordered_287, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_290 = rand_idx;
        rand_idx += 1.0;
        const double scale2 = ((0.95) + ((0.0625) * double(randoms.at(checkedIndex(ordered_290, randoms.size())))));
        const double ordered_293 = rand_idx;
        rand_idx += 1.0;
        const double f1 = ((0.995) + ((0.0025) * double(randoms.at(checkedIndex(ordered_293, randoms.size())))));
        const double ordered_296 = rand_idx;
        rand_idx += 1.0;
        const double f2 = (((0.0009) + ((0.0012) * double(randoms.at(checkedIndex(ordered_296, randoms.size()))))) * 80.0);
        const double ordered_300 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_301 = ((0.15) * ((0.01) + ((0.03) * double(randoms.at(checkedIndex(ordered_300, randoms.size()))))));
        const double ordered_302 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_301 * ((double(randoms.at(checkedIndex(ordered_302, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double r = (std::sqrt(((dx2 * dx2) + (dy2 * dy2))) * f2);
                const double scale = ((jsBitAnd(dx, 1.0) == jsBitAnd(dy, 1.0)) ? (f1 - r) : scale2);
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 41.0)
    {
        const auto ordered_303 = (-0.1);
        const double ordered_305 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_303 + ((0.2) * double(randoms.at(checkedIndex(ordered_305, randoms.size())))));
        const double cy = 0.0;
        AdjustCenter(cx, cy, warp_prefs);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double scaleX = (1.0075 + (0.00375 * (fdy + 1.0)));
                const double sdx = (cx + (dx2 * scaleX));
                const double sdy = ((-1.0) + ((fdy + 1.0) * 1.01));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 26.0)
    {
        warp_prefs.stars_follow_motion_min = kFollowMotionNone;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_pow = 0.333;
        const auto ordered_306 = (-0.1);
        const double ordered_308 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_306 + ((0.2) * double(randoms.at(checkedIndex(ordered_308, randoms.size())))));
        const auto ordered_309 = (-0.1);
        const double ordered_311 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_309 + ((0.2) * double(randoms.at(checkedIndex(ordered_311, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_314 = rand_idx;
        rand_idx += 1.0;
        const double baseScale = ((1.008) + ((0.008) * double(randoms.at(checkedIndex(ordered_314, randoms.size())))));
        const double scale1 = (1.0 + (((baseScale * 0.99) - 1.0) * 0.25));
        const double ordered_318 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_319 = ((0.15) * ((0.04) + ((0.14) * double(randoms.at(checkedIndex(ordered_318, randoms.size()))))));
        const double ordered_320 = rand_idx;
        rand_idx += 1.0;
        const double baseTurn = (ordered_319 * ((double(randoms.at(checkedIndex(ordered_320, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double turn1 = (baseTurn * (-0.6));
        const double c1 = std::cos(turn1);
        const double s1 = std::sin(turn1);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double scale = scale1;
                const double c = c1;
                const double s = s1;
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 43.0)
    {
        const auto ordered_321 = (-0.1);
        const double ordered_323 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_321 + ((0.2) * double(randoms.at(checkedIndex(ordered_323, randoms.size())))));
        const double cy = 0.0;
        AdjustCenter(cx, cy, warp_prefs);
        const double sqrtConversion = (1.0 / std::sqrt(320.0));
        const double centerThreshold = (0.5 / 320.0);
        const double offset = (0.9 / 320.0);
        warp_prefs.flat_angle_lo = ((3.1415927 * 0.5) - 0.1);
        warp_prefs.flat_angle_hi = ((3.1415927 * 0.5) + 0.1);
        warp_prefs.flat_is_stereo_chance = 0.0;
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double sourceX = ((dx2 < (-centerThreshold)) ? ((cx - (std::sqrt((-dx2)) * sqrtConversion)) + offset) : ((dx2 > centerThreshold) ? ((cx + (std::sqrt(dx2) * sqrtConversion)) - offset) : cx));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = ((sourceX - fdx) * 0.25);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = 0.0;
            }
        }
    }
    else
    if (new_mode == 27.0)
    {
        const auto ordered_324 = (-0.1);
        const double ordered_326 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_324 + ((0.2) * double(randoms.at(checkedIndex(ordered_326, randoms.size())))));
        const auto ordered_327 = (-0.1);
        const double ordered_329 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_327 + ((0.2) * double(randoms.at(checkedIndex(ordered_329, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        warp_prefs.stars_follow_motion_min = kFollowMotionNone;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_pow = 3.0;
        const double ordered_332 = rand_idx;
        rand_idx += 1.0;
        const double f1 = ((0.92) + ((0.16) * double(randoms.at(checkedIndex(ordered_332, randoms.size())))));
        const double ordered_336 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_337 = ((0.15) * ((0.007) + ((0.02) * double(randoms.at(checkedIndex(ordered_336, randoms.size()))))));
        const double ordered_338 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_337 * ((double(randoms.at(checkedIndex(ordered_338, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double r = (std::sqrt(((dx2 * dx2) + (dy2 * dy2))) * 320.0);
                const double scale = (1.0 + ((0.01 - ((r * std::sqrt(r)) * 8.75e-06)) * f1));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 28.0)
    {
        const auto ordered_339 = (-0.1);
        const double ordered_341 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_339 + ((0.2) * double(randoms.at(checkedIndex(ordered_341, randoms.size())))));
        const auto ordered_342 = (-0.1);
        const double ordered_344 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_342 + ((0.2) * double(randoms.at(checkedIndex(ordered_344, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_348 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_349 = ((0.15) * ((0.007) + ((0.02) * double(randoms.at(checkedIndex(ordered_348, randoms.size()))))));
        const double ordered_350 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_349 * ((double(randoms.at(checkedIndex(ordered_350, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double phase = (dy2 * 4.5);
                const double scale = (0.975 + (0.05 * std::cos(phase)));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 29.0)
    {
        const auto ordered_351 = (-0.1);
        const double ordered_353 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_351 + ((0.2) * double(randoms.at(checkedIndex(ordered_353, randoms.size())))));
        const auto ordered_354 = (-0.1);
        const double ordered_356 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_354 + ((0.2) * double(randoms.at(checkedIndex(ordered_356, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_359 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_360 = ((5.0) * double(randoms.at(checkedIndex(ordered_359, randoms.size()))));
        const double petals = ((2.0) + std::floor(ordered_360));
        const double ordered_363 = rand_idx;
        rand_idx += 1.0;
        const double f2 = ((0.988) + ((0.01) * double(randoms.at(checkedIndex(ordered_363, randoms.size())))));
        const double ordered_366 = rand_idx;
        rand_idx += 1.0;
        const double f3 = (((0.0125) + ((0.0125) * double(randoms.at(checkedIndex(ordered_366, randoms.size()))))) * 0.25);
        const double ordered_369 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_370 = ((0.04) * double(randoms.at(checkedIndex(ordered_369, randoms.size()))));
        const double ordered_372 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_373 = ((0.15) * (ordered_370 + ((0.045) * double(randoms.at(checkedIndex(ordered_372, randoms.size()))))));
        const double ordered_374 = rand_idx;
        rand_idx += 1.0;
        const double turn = ((ordered_373 * ((double(randoms.at(checkedIndex(ordered_374, randoms.size()))) < 0.5) ? 1.0 : (-1.0))) * 0.5);
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double angle = std::atan2(dy2, dx2);
                const double scale = (f2 + (f3 * std::sin((angle * petals))));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 14.0)
    {
        const auto ordered_375 = (-0.1);
        const double ordered_377 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_375 + ((0.2) * double(randoms.at(checkedIndex(ordered_377, randoms.size())))));
        const auto ordered_378 = (-0.1);
        const double ordered_380 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_378 + ((0.2) * double(randoms.at(checkedIndex(ordered_380, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        warp_prefs.stars_follow_motion_min = kFollowMotionNone;
        warp_prefs.stars_follow_motion_max = kFollowMotionHigh;
        warp_prefs.stars_follow_motion_pow = 3.0;
        const double ordered_384 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_385 = ((0.15) * ((0.007) + ((0.02) * double(randoms.at(checkedIndex(ordered_384, randoms.size()))))));
        const double ordered_386 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_385 * ((double(randoms.at(checkedIndex(ordered_386, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double r2 = ((dx2 * dx2) + (dy2 * dy2));
                const double scale = std::max(0.375, (1.0125 - (r2 * 0.576)));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 31.0)
    {
        const auto ordered_387 = (-0.1);
        const double ordered_389 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_387 + ((0.2) * double(randoms.at(checkedIndex(ordered_389, randoms.size())))));
        const auto ordered_390 = (-0.1);
        const double ordered_392 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_390 + ((0.2) * double(randoms.at(checkedIndex(ordered_392, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_396 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_397 = ((0.15) * ((0.007) + ((0.02) * double(randoms.at(checkedIndex(ordered_396, randoms.size()))))));
        const double ordered_398 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_397 * ((double(randoms.at(checkedIndex(ordered_398, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double scale = (0.9925 - ((dy2 * dy2) * 0.1));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 32.0)
    {
        const auto ordered_399 = (-0.1);
        const double ordered_401 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_399 + ((0.2) * double(randoms.at(checkedIndex(ordered_401, randoms.size())))));
        const auto ordered_402 = (-0.1);
        const double ordered_404 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_402 + ((0.2) * double(randoms.at(checkedIndex(ordered_404, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_408 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_409 = ((0.15) * ((0.007) + ((0.02) * double(randoms.at(checkedIndex(ordered_408, randoms.size()))))));
        const double ordered_410 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_409 * ((double(randoms.at(checkedIndex(ordered_410, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double scale = (0.9925 - ((dx2 * dx2) * 0.1));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 33.0)
    {
        const auto ordered_411 = (-0.1);
        const double ordered_413 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_411 + ((0.2) * double(randoms.at(checkedIndex(ordered_413, randoms.size())))));
        const auto ordered_414 = (-0.1);
        const double ordered_416 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_414 + ((0.2) * double(randoms.at(checkedIndex(ordered_416, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_420 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_421 = ((0.15) * ((0.007) + ((0.02) * double(randoms.at(checkedIndex(ordered_420, randoms.size()))))));
        const double ordered_422 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_421 * ((double(randoms.at(checkedIndex(ordered_422, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double r = std::sqrt(((dx2 * dx2) + (dy2 * dy2)));
                const double scale = (1.01 - (r * 0.0625));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 23.0)
    {
        const auto ordered_423 = (-0.1);
        const double ordered_425 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_423 + ((0.2) * double(randoms.at(checkedIndex(ordered_425, randoms.size())))));
        const auto ordered_426 = (-0.1);
        const double ordered_428 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_426 + ((0.2) * double(randoms.at(checkedIndex(ordered_428, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_432 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_433 = ((0.15) * ((0.007) + ((0.02) * double(randoms.at(checkedIndex(ordered_432, randoms.size()))))));
        const double ordered_434 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_433 * ((double(randoms.at(checkedIndex(ordered_434, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double scale = (1.0375 - (std::sqrt((dy2 + 1.4)) * 0.05));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 24.0)
    {
        const auto ordered_435 = (-0.1);
        const double ordered_437 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_435 + ((0.2) * double(randoms.at(checkedIndex(ordered_437, randoms.size())))));
        const auto ordered_438 = (-0.1);
        const double ordered_440 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_438 + ((0.2) * double(randoms.at(checkedIndex(ordered_440, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_444 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_445 = ((0.15) * ((0.007) + ((0.02) * double(randoms.at(checkedIndex(ordered_444, randoms.size()))))));
        const double ordered_446 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_445 * ((double(randoms.at(checkedIndex(ordered_446, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double r = std::sqrt(((dx2 * dx2) + (dy2 * dy2)));
                const double ring = std::floor((r * 10.0));
                const double scale = (0.9875 - (ring * 0.01));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 25.0)
    {
        const auto ordered_447 = (-0.1);
        const double ordered_449 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_447 + ((0.2) * double(randoms.at(checkedIndex(ordered_449, randoms.size())))));
        const auto ordered_450 = (-0.1);
        const double ordered_452 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_450 + ((0.2) * double(randoms.at(checkedIndex(ordered_452, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_456 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_457 = ((0.15) * ((0.007) + ((0.02) * double(randoms.at(checkedIndex(ordered_456, randoms.size()))))));
        const double ordered_458 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_457 * ((double(randoms.at(checkedIndex(ordered_458, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double r = std::sqrt(((dx2 * dx2) + (dy2 * dy2)));
                const double ring = std::fmod(std::floor((r * 20.0)), 4.0);
                const double scale = (0.9875 - (ring * 0.03));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (new_mode == 19.0)
    {
        const auto ordered_459 = (-0.1);
        const double ordered_461 = rand_idx;
        rand_idx += 1.0;
        const double cx = (ordered_459 + ((0.2) * double(randoms.at(checkedIndex(ordered_461, randoms.size())))));
        const auto ordered_462 = (-0.1);
        const double ordered_464 = rand_idx;
        rand_idx += 1.0;
        const double cy = (ordered_462 + ((0.2) * double(randoms.at(checkedIndex(ordered_464, randoms.size())))));
        AdjustCenter(cx, cy, warp_prefs);
        const double ordered_468 = rand_idx;
        rand_idx += 1.0;
        const auto ordered_469 = ((0.15) * ((0.007) + ((0.02) * double(randoms.at(checkedIndex(ordered_468, randoms.size()))))));
        const double ordered_470 = rand_idx;
        rand_idx += 1.0;
        const double turn = (ordered_469 * ((double(randoms.at(checkedIndex(ordered_470, randoms.size()))) < 0.5) ? 1.0 : (-1.0)));
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            const double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                const double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double dx2 = (fdx - cx);
                const double dy2 = (fdy - cy);
                const double r = std::sqrt(((dx2 * dx2) + (dy2 * dy2)));
                const double scale = (1.0 - ((0.25 * r) / (3.0 + r)));
                const double sdx = (cx + (scale * ((dx2 * c) - (dy2 * s))));
                const double sdy = (cy + (scale * ((dx2 * s) + (dy2 * c))));
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    else
    if (((new_mode == 98.0) || (new_mode == 99.0)))
    {
        warp_prefs.wave_prefs_weight = 1e-05;
        warp_prefs.flat_angle_lo = 0.0;
        warp_prefs.flat_angle_hi = 0.0;
        warp_prefs.flat_is_stereo_chance = 0.0;
        warp_prefs.flat_cx_lo = (-0.1);
        warp_prefs.flat_cx_hi = 0.1;
        warp_prefs.flat_cy_lo = (-0.2);
        warp_prefs.flat_cy_hi = 0.2;
        warp_prefs.use_motion_center_as_wave_center_prob = 0.0;
    }
    else
    if (new_mode == 17.0)
    {
        const double ordered_471 = rand_idx;
        rand_idx += 1.0;
        const double cx = (((double(randoms.at(checkedIndex(ordered_471, randoms.size()))) * 2.0) - 1.0) * 0.08);
        const double ordered_472 = rand_idx;
        rand_idx += 1.0;
        const double cy = (((double(randoms.at(checkedIndex(ordered_472, randoms.size()))) * 2.0) - 1.0) * 0.08);
        AdjustCenter(cx, cy, warp_prefs);
        for (double dy = 0.0; (dy < H); dy += 1.0)
        {
            checkCancelled(stop);
            double fdy = (((dy * (2.0 / H)) - 1.0) * (y0y1 * 0.5));
            for (double dx = 0.0; (dx < W); dx += 1.0, write_offset += 2.0)
            {
                double fdx = (((dx * (2.0 / W)) - 1.0) * (x0x1 * 0.5));
                const double tx = (fdx - cx);
                const double ty = (fdy - cy);
                const double rad1 = std::sqrt(((tx * tx) + (ty * ty)));
                const double inv_rad1 = (1.0 / (rad1 + 0.0001));
                const double nx = (tx * inv_rad1);
                const double ny = (ty * inv_rad1);
                const double dist1 = inv_rad1;
                const double dist2 = (dist1 + 0.01);
                const double rad2 = ((1.0 / dist2) - 0.0001);
                double tx2 = (rad2 * nx);
                double ty2 = (rad2 * ny);
                double sdx = (cx + tx2);
                double sdy = (cy + ty2);
                src_dxy.at(checkedIndex((write_offset + 0.0), src_dxy.size())) = (sdx - fdx);
                src_dxy.at(checkedIndex((write_offset + 1.0), src_dxy.size())) = (sdy - fdy);
            }
        }
    }
    auto dynamic_motion_params = std::vector<float>{};
    if (is_dynamic)
    {
        src_dxy = std::vector<float>{};
        dynamic_motion_params = floatArray(kDynamicMotionParamCount);
        for (double i = 0.0; (i < kDynamicMotionParamCount); i += 1.0)
        {
            const double ordered_473 = rand_idx;
            rand_idx += 1.0;
            dynamic_motion_params.at(checkedIndex(i, dynamic_motion_params.size())) = double(randoms.at(checkedIndex(ordered_473, randoms.size())));
        }
    }
    if ((rand_idx > WarpMapRandomNumberCount))
    {
        throw std::runtime_error("Prism Flow motion random bank exhausted");
    }
    const double ordered_474 = rand_idx;
    rand_idx += 1.0;
    double breathing_str = (double(randoms.at(checkedIndex(ordered_474, randoms.size()))) * 0.4);
    const double ordered_475 = rand_idx;
    rand_idx += 1.0;
    double breathing_phase = (double(randoms.at(checkedIndex(ordered_475, randoms.size()))) * 6.28);
    const double ordered_478 = rand_idx;
    rand_idx += 1.0;
    const auto ordered_479 = ((double(randoms.at(checkedIndex(ordered_478, randoms.size()))) * 2.0) - 1.0);
    double breathing_freq = ((0.5) * std::pow((1.92), ordered_479));
    for (float value : src_dxy) if (!std::isfinite(value)) throw std::runtime_error("Nonfinite Prism Flow motion map");
    return {new_mode, uint32_t(W), uint32_t(H), warp_prefs, std::move(src_dxy), std::move(dynamic_motion_params), seed, breathing_str, breathing_phase, breathing_freq};
}
}
