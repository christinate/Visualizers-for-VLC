// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace visualizers {
struct BeatState {
    uint64_t count=0;
    double envelope=0;
    bool detected=false;
};

class BeatDetector {
    double average_=.001,lastBeat_=-10,lastTime_=0,envelope_=0;
    uint64_t beats_=0;
public:
    BeatState update(double bass,double time);
    BeatState state(double silenceSeconds=0)const;
    void restart(double time);
};

double estimateBass(std::span<const float,4096> stereo);

struct MusicFrame {
    std::array<float,2048> wave{};
    std::array<float,1024> spectrum{};
    double rms=0;
    double visualEnergy=0;
    double bass=0;
    double beatEnvelope=0;
    uint64_t beatCount=0;
    bool beat=false;
};

// Consumes the same 2048-frame, stereo float, 44.1 kHz window as the preserved
// JavaScript reference. Capture/IPC conversion occurs outside this analyzer.
// Arrays are reused; update performs no allocation and acquires no lock.
class MusicAnalysis {
    MusicFrame result_{};
    std::array<float,2048> real_{},imag_{};
    BeatDetector beats_;
public:
    const MusicFrame& update(std::span<const float> stereo,double time,const BeatState* playerBeat=nullptr);
};
}
