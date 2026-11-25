// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <vector>
namespace visualizers {
// Owned CPU image; row-major, straight-alpha RGBA8, RGB encoded as sRGB.
// These C++ values stay inside the host, not across a plugin ABI.
struct RgbaImage {uint32_t width=0,height=0;std::vector<uint8_t> rgba;};
}
