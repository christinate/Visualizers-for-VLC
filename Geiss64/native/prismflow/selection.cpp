// SPDX-License-Identifier: Apache-2.0
// Derived from Geiss HDR, Copyright (c) 2026 Ryan Geiss.
// Native C++ port: Copyright (c) 2026 VLC Visualizers Contributors.
#include "motion.hpp"
#include <cmath>
#include <stdexcept>

namespace prismflow {
int32_t pickRandomWarpMap(std::span<const int32_t> otherModes,double unitRandom){
    if(!std::isfinite(unitRandom)||unitRandom<0||unitRandom>=1||otherModes.size()>kMaxActiveWarps)
        throw std::invalid_argument("Invalid Prism Flow motion selection request");
    for(auto mode:otherModes)if(mode<0||mode>=40)throw std::invalid_argument("Unknown companion motion mode");
    const auto modes=motionModes();std::array<float,40> adjusted{};double weightSum=0;
    const auto& superGood=modePairs(ModeRelation::SuperGood);const auto& good=modePairs(ModeRelation::Good);
    const auto& bad=modePairs(ModeRelation::Bad);const auto& forbidden=modePairs(ModeRelation::Forbidden);
    for(unsigned i=0;i<modes.size();++i){
        double weight=modes[i].weight;
        for(auto other:otherModes){
            if(modes[i].mode==other)weight*=.00001;
            if(superGood[i][other])weight*=kSuperGoodModePairWeightScale;
            if(good[i][other])weight*=kGoodModePairWeightScale;
            if(bad[i][other])weight*=kBadModePairWeightScale;
            if(forbidden[i][other])weight*=kForbiddenModePairWeightScale;
        }
        // Preserve the reference's double total and float32 cumulative weights.
        adjusted[i]=float(weight);weightSum+=weight;
    }
    const double target=unitRandom*weightSum;double sum=0;
    for(unsigned i=0;i<modes.size();++i){sum+=double(adjusted[i]);if(sum>=target)return modes[i].mode;}
    return modes.back().mode;
}
}
