// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "presentation.hpp"
#include <SDL.h>
#include <SDL_metal.h>
#include <nlohmann/json.hpp>
namespace visualizers::gpu {
DisplayPresentation metalDisplayPresentation(SDL_Window*,SDL_MetalView,bool requested);
void configureMetalPresentationLayer(SDL_MetalView,bool linear);
nlohmann::json metalPresentationLayerInfo(SDL_Window*,SDL_MetalView);
}
