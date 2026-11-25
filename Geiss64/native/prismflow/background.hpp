// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
// Retain NOTICE.txt, LICENSE.txt and OUTPUTS.txt in this directory.
#pragma once
#include <cstdint>
#include <array>
#include <functional>
#include <stop_token>
#include <vector>
namespace prismflow {
using Color=std::array<double,3>;
using RandomFunction=std::function<double()>;
struct BackgroundOptions {
    double adventurousness=.5,maxLightnessDifference=18,mixingFraction=.15;
};
struct BackgroundCandidate {
    Color rgb;
    double score,preference,foreground,harmony,blend,lightnessDifference,probability;
};
std::vector<BackgroundCandidate> backgroundCandidates(const Color& foreground,
    const BackgroundOptions& options={},std::stop_token stop={});
Color pickBackgroundColor(const Color& foreground,const BackgroundOptions& options,
    const RandomFunction& random,std::stop_token stop={});
}
