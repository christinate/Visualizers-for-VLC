// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include "palette.hpp"
#include <optional>
namespace prismflow {
struct ImagePaletteOptions {
    uint32_t sampleSize=128,clusterCount=12,iterations=10;
    double highlightLuminance=.85;
    std::optional<double> smoothness;
};
using ImageColors=std::array<Color,8>;
// Small, already color-managed, unpremultiplied sRGB RGBA8 image. The native
// decoder downsamples first; this function owns no decoder/GUI/GPU state.
// Each dimension is 1..512. Input remains caller-owned for the synchronous call.
// Native callers may cancel without publishing a partially generated palette.
ImageColors paletteFromImageRgba(uint32_t width,uint32_t height,std::span<const uint8_t>,
    const RandomFunction&,const ImagePaletteOptions& options={},std::stop_token stop={});
Palette imagePaletteRgba(const ImageColors&);
void validateImagePaletteOptions(const ImagePaletteOptions&);
}
