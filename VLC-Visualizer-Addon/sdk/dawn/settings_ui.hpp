// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "settings.hpp"
#include <functional>
#include <optional>
namespace visualizers {
struct SettingsAutomation {bool cancel=false;SettingsJson edits=SettingsJson::object();};
// Return changed fields only. A cancelled dialog never modifies storage.
std::optional<SettingsJson> editNativeSettings(const SettingsJson& values,const std::string& id,
    const std::string& title,const std::function<bool()>& keepOpen,
    const SettingsAutomation* automation=nullptr,const std::string& notice="");
inline const char* settingsNote(){return "Settings apply the next time the visualizer opens. HDR is Experimental and falls back to SDR when unavailable.";}
}
