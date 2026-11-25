// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace prismflow {
std::vector<float> blurWave(std::span<const float>,uint32_t radius);
std::vector<float> resampleWave(std::span<const float>,uint32_t outputCount);
class WaveAligner {
    uint32_t original_=0,aligned_=0;
    std::vector<float> previous_;
public:
    std::vector<float> align(std::span<const float>,double fraction);
    void reset(){original_=aligned_=0;previous_.clear();}
};
}
