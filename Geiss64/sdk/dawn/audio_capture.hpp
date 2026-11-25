// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace visualizers {
// One callback producer, one render-thread consumer. Unread data is never
// overwritten: overflow drops incoming frames, avoiding data races on floats.
class AudioFifo {
    static constexpr uint64_t capacity_=16384;
    std::array<float,capacity_*2> data_{};
    std::array<float,4096> history_{};
    alignas(64) std::atomic<uint64_t> written_{0};
    alignas(64) std::atomic<uint64_t> consumed_{0};
    std::atomic<uint64_t> dropped_{0},publishedAt_{0},received_{0};
    unsigned historyHead_=0,historyFrames_=0;
    uint64_t lastStamp_=0;
public:
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    // reset requires the capture callback to be stopped.
    void reset()noexcept;
    void push(const float* stereo,uint32_t frames,uint64_t nowMs)noexcept;
    unsigned window(std::span<float,4096> stereo,uint64_t nowMs)noexcept;
    // Streaming consumer: copy new frames, once, into an even-sized buffer.
    // Only the first returned frames are valid. Stale input is discarded.
    // Like window(), this must run on the one consumer thread.
    unsigned drain(std::span<float> stereo,uint64_t nowMs)noexcept;
    uint64_t received()const noexcept{return received_.load(std::memory_order_relaxed);}
    uint64_t dropped()const noexcept{return dropped_.load(std::memory_order_relaxed);}
};

struct InputDevice {std::string id,label;bool isDefault=false;};
std::string resolveAudioInput(const std::vector<InputDevice>&,const std::string& idOrExactLabel);
struct CaptureStatus {
    bool running=false;
    uint64_t frames=0,dropped=0;
    unsigned nativeRate=0,nativeChannels=0;
    std::string backend,device;
};
enum class CapturePermission { Granted, NotDetermined, Pending, Denied, Restricted, MissingUsageDescription };
// Querying never opens a device or asks for permission. Request on the UI
// thread only after the user selected recording input; poll without blocking
// the visualizer's event loop. Permission is enforced again by start().
CapturePermission capturePermission();
void requestCapturePermission();
const char* capturePermissionMessage(CapturePermission)noexcept;
// The context/device addresses remain stable until uninit. No recording data
// is written to disk; output is normalized float stereo at 44.1 kHz.
class AudioCapture {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    AudioCapture();
    ~AudioCapture();
    AudioCapture(const AudioCapture&)=delete;
    AudioCapture& operator=(const AudioCapture&)=delete;
    std::vector<InputDevice> devices();
    std::string resolveInput(const std::string& idOrExactLabel);
    void start(const std::string& idOrExactLabel="default");
    void stop()noexcept;
    unsigned window(std::span<float,4096> stereo);
    unsigned drain(std::span<float> stereo);
    CaptureStatus status()const;
};
uint64_t audioMilliseconds()noexcept;
}
