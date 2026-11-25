// SPDX-License-Identifier: MIT
#pragma once
#include "scene_ini.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace terrain {

inline std::string readSceneFile(const std::filesystem::path& file, size_t maximum, bool optional = false) {
    if (optional && !std::filesystem::exists(file)) return {};
    if (!std::filesystem::is_regular_file(file) || std::filesystem::file_size(file) > maximum)
        throw std::runtime_error("Invalid scene file: " + file.filename().string());
    std::ifstream input(file, std::ios::binary);
    std::string text(maximum + 1, '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (input.bad() || !input.eof() || static_cast<size_t>(input.gcount()) > maximum)
        throw std::runtime_error("Cannot read scene file: " + file.filename().string());
    text.resize(static_cast<size_t>(input.gcount()));
    return text;
}

using suite::SceneIni;

struct BeatTrigger {
    uint64_t count = 0;
    double at = -1;
    bool level = false;

    bool update(double time, uint64_t next, bool beat) {
        if (next < count || time < at) {
            count = 0;
            at = -1;
            level = false;
        }
        const bool hit = next > count || (beat && !level);
        count = next;
        level = beat;
        if (hit) at = time;
        return hit;
    }

    float remaining(double time, double duration) const {
        if (at < 0 || time < at || duration <= 0 || time - at >= duration) return 0;
        return static_cast<float>(1 - (time - at) / duration);
    }
};

// Manual changes restart the automatic rotation and use a short transition.
class ModeCycle {
public:
    struct Frame { unsigned from, to; double blend; unsigned current; };
private:
    double lastTime_ = 0, started_ = 0;
    double fraction_ = 0, heldFraction_ = 0, badgeAt_ = -1;
    unsigned offset_ = 0, from_ = 0;
    unsigned held_ = 0;
    bool pending_ = false, changing_ = false, toggle_ = false, locked_ = false;
    bool displayed_ = false;
    Frame previous_{};
public:
    void next() { pending_ = true; }
    void toggleLock() { toggle_ = !toggle_; }
    float badge(double time) const {
        const double age = time - badgeAt_;
        if (badgeAt_ < 0 || age < 0 || age >= 2) return 0;
        const float alpha = static_cast<float>(std::min(1., (2-age)/.65));
        return locked_ ? alpha : -alpha;
    }
    Frame frame(double time, double interval, double transition, unsigned count, bool animate = true,
                unsigned longerMode = 0, double extraSeconds = 0) {
        if (time < lastTime_) {
            started_ = time; offset_ = 0; changing_ = false;
            displayed_ = false; badgeAt_ = -1;
        }
        lastTime_ = time;
        const auto eased = [](double t) { return t*t*(3-2*t); };
        const auto duration = [&](unsigned mode) { return interval+(mode==longerMode?extraSeconds:0); };
        if (toggle_) {
            toggle_ = false;
            locked_ = !locked_;
            badgeAt_ = time;
            if (locked_) {
                held_ = displayed_?(previous_.blend>=.5?previous_.to:previous_.from):offset_%count;
                heldFraction_ = displayed_&&previous_.blend==0&&!changing_?fraction_:0;
            } else {
                offset_ = held_;
                started_ = time-heldFraction_*duration(held_);
            }
            changing_ = false;
        }
        displayed_ = true;
        if (locked_) {
            pending_ = false;
            return previous_ = {held_, held_, 0, held_};
        }
        double elapsed = std::fmod(time - started_, interval*count+extraSeconds);
        unsigned current = offset_%count;
        while(elapsed>=duration(current)){
            elapsed-=duration(current);
            current=(current+1)%count;
        }
        if (pending_) {
            pending_ = false;
            from_ = current;
            offset_ = (current + 1) % count;
            started_ = time;
            current = offset_;
            changing_ = animate;
            elapsed = 0;
        }
        constexpr double manualSeconds = .7;
        if (changing_ && time - started_ < manualSeconds)
            return previous_ = {from_, current, eased((time - started_) / manualSeconds),current};
        changing_ = false;
        fraction_ = elapsed/duration(current);
        const double fade=transition*interval;
        const double amount=fade>0?std::clamp((elapsed-duration(current)+fade)/fade,0.,1.):0.;
        return previous_ = {current, (current + 1) % count, eased(amount),current};
    }
    void startAt(unsigned index) { offset_ = index; }
};

} // namespace terrain
