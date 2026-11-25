// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "music_analysis.hpp"
#include <cstdint>
#include <memory>
#include <span>

namespace suite {struct AudioRing;}
namespace visualizers {
// Player IPC timestamps use the platform's shared monotonic clock, rather
// than an implementation-defined std::chrono epoch.
uint64_t playerMilliseconds()noexcept;
bool readPlayerWindow(const suite::AudioRing&,std::span<float,4096>,uint64_t nowMs)noexcept;
class PlayerBeatHistory {
    BeatDetector beats_;
    uint64_t analyzedEnd_=0, observedEnd_=0, deliveredMs_=0;
    uint32_t rate_=0;
    double audioTime_=0;
public:
    BeatState advance(const suite::AudioRing&,uint64_t nowMs);
    BeatState idle(uint64_t nowMs)const;
};
class PlayerAudio {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    explicit PlayerAudio(uint32_t pid);
    ~PlayerAudio();
    PlayerAudio(const PlayerAudio&)=delete;
    bool window(std::span<float,4096>,BeatState* beat=nullptr);
};
// Hold a kernel reference/watch for this particular process, so PID reuse
// cannot attach an existing renderer to a later, unrelated player.
class PlayerLifetime {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    explicit PlayerLifetime(uint32_t pid);
    ~PlayerLifetime();
    PlayerLifetime(const PlayerLifetime&)=delete;
    bool exited();
};
}
