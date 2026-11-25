// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace visualizers {
using SettingsJson=nlohmann::json;
enum class SettingType { Boolean, Integer, Number, Choice, Ini };
struct SettingChoice {std::string label;SettingsJson value;};
struct SettingField {
    std::string key,label;SettingType type;double minimum=0,maximum=0;
    std::vector<SettingChoice> choices;
};
std::vector<SettingField> settingsFields(const std::string& visualizerId,bool sceneEditor=false);
bool hasSceneIni(const std::string& id);
SettingsJson normalizeSettings(const SettingsJson&);
void validateSettingsPatch(const std::vector<SettingField>&,const SettingsJson&);
double settingNumber(const std::string& text);
std::filesystem::path defaultSettingsProfile();
struct SettingsSnapshot {SettingsJson values;bool importedIni=false;};
class SettingsStore {
    std::filesystem::path profile_;std::string id_;
public:
    SettingsStore(std::filesystem::path profile,std::string visualizerId);
    SettingsSnapshot load()const;
    std::string sceneOverrides()const;
    std::string sceneEditor(const std::filesystem::path& assets)const;
    unsigned advanceObjectsStart()const;
    // Merge only changed keys into the latest document under a per-ID lock.
    // Unknown plugin fields survive; malformed documents are never overwritten.
    SettingsSnapshot savePatch(const SettingsJson& changed)const;
};
}
