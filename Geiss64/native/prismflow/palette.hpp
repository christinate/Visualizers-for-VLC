// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#pragma once
#include "background.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
namespace prismflow {
using Palette=std::vector<float>;
using ExpandedPalette=std::array<float,1024>;
using FloatColor=std::array<float,3>;
struct PaletteDefinition {Palette rgba;std::string name;Color background;int32_t index=-1;};
// Owned CPU values only. RGB is finite, nonnegative and <=65504; alpha must
// be finite. Palettes contain 1..4096 colors. Input text is bounded to 1 MiB.
Palette deserializePalette(std::string_view text);
std::string serializePalette(std::span<const float> rgba);
Color samplePalette(std::span<const float> rgba,double t);
ExpandedPalette expandPalette(std::span<const float> rgba);
Color companionColor(std::span<const float> rgba,const RandomFunction& random,std::stop_token stop={});
FloatColor hdrToSdr(double t,const Color& rgb,double maximum,bool experiment=false);
// Headroom is SDR-relative, in [1,65504]. The native tone mapper keeps zero
// luminance finite and bounds the white fade for extreme custom palettes.
FloatColor reduceHdrHeadroom(double headroom,double t,const Color& rgb,double maximum,bool experiment=false);
class PaletteCatalog {
    std::vector<std::string> text_;
public:
    PaletteCatalog();
    size_t size()const{return text_.size();}
    std::string_view text(size_t index)const{return text_.at(index);}
    PaletteDefinition saved(size_t index,const RandomFunction& random,std::stop_token stop={})const;
    PaletteDefinition generate(const RandomFunction& random,std::stop_token stop={})const;
    int32_t add(std::span<const float> rgba);
    std::string saveBackground(int32_t index,const Color& background);
};
}
