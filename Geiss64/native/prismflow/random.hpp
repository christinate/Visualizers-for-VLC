// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include <bit>
#include <cmath>
#include <cstdint>
namespace prismflow {
class Mulberry32 {
    uint32_t state_;
public:
    explicit Mulberry32(uint32_t seed):state_(seed){}
    double operator()(){
        state_+=0x6d2b79f5u;uint32_t t=state_;
        t=(t^(t>>15))*(t|1u);t^=t+(t^(t>>7))*(t|61u);
        return double(t^(t>>14))/4294967296.;
    }
};
inline int32_t jsInt32(double value){
    if(!std::isfinite(value)||value==0)return 0;
    double wrapped=std::fmod(std::trunc(value),4294967296.);if(wrapped<0)wrapped+=4294967296.;
    return std::bit_cast<int32_t>(uint32_t(wrapped));
}
inline double jsBitOr(double a,double b){return double(jsInt32(a)|jsInt32(b));}
inline double jsBitAnd(double a,double b){return double(jsInt32(a)&jsInt32(b));}
}
