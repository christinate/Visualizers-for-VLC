// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "image_pixels.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace visualizers {
struct TextRasterOptions {
    // Logical pixels, 0.5..1024; padding 0..256; supersample 1..8.
    std::string family="sans-serif";double fontPixels=18,paddingPixels=4;
    uint32_t supersample=2,weight=700;
};
struct TextRasterMetrics {
    // Outward-rounded ink bounds relative to the baseline. Draw at
    // (padding + left, padding + ascent), including negative left bearings.
    double left=0,right=0,ascent=0,descent=0,advance=0;
    uint32_t padding=0,supersample=1;std::string primaryFamily;
};
struct RasterizedText {RgbaImage image;TextRasterMetrics metrics;};
// CPU-only, owned values inside the host. These STL types are not a plugin ABI.
// Call on the UI/render thread after NativeUiSession on Linux. The font backend
// performs shaping/fallback; no browser/runtime reference bitmap is consulted.
RasterizedText rasterizeText(std::string_view utf8,const TextRasterOptions& options={});
namespace raster_detail {
inline constexpr uint32_t maximumDimension=8192,maximumPixels=16777216;
std::string normalizeText(std::string_view utf8);
std::string fontFamily(std::string_view utf8);
void validate(const TextRasterOptions&);
RasterizedText allocate(const TextRasterOptions&,const TextRasterMetrics& raw);
void whiteAlpha(RgbaImage&,const uint8_t* alpha,uint32_t stride);
}
}
