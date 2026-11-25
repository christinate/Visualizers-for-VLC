// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "image_pixels.hpp"
#include <array>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>
namespace visualizers {
struct DecodedImage {
    RgbaImage image;uint32_t sourceWidth=0,sourceHeight=0;
    bool sourceProfileReported=false;std::string backend;
};
// Local file, first frame, EXIF orientation, embedded-profile -> sRGB,
// premultiplied-alpha downsampling, then straight-alpha RGBA8. No upscaling.
// Codecs are OS/system provided; unsupported/corrupt files report an error.
// 64 MiB compressed; source <=32768 per axis and <=64 million pixels;
// output longest side 1..512. Cancellation is checked around codec calls;
// callers must also reject superseded results before publication.
DecodedImage decodeImage(const std::filesystem::path&,uint32_t longestSide=128,std::stop_token stop={});
namespace image_detail {
struct GifFrame {uint32_t canvasWidth,canvasHeight,left,top,width,height;};
std::optional<GifFrame> gifFirstFrame(const std::filesystem::path&,std::stop_token);
void validateFile(const std::filesystem::path&,uint32_t,std::stop_token);
std::array<uint32_t,2> dimensions(uint32_t,uint32_t,uint32_t);
void cancelled(std::stop_token);
void unpremultiply(RgbaImage&);
}
}
