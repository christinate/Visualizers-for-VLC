// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "context.hpp"
#include "music_analysis.hpp"
#include "visualizer_dawn_api.h"
#include "visualizer_dawn_control.h"
#include <filesystem>
#include <optional>
#include <string_view>

namespace visualizers::gpu {
class PluginInstance;
class PluginLibrary {
    struct State;
    std::shared_ptr<State> state_;
    friend class PluginInstance;
public:
    explicit PluginLibrary(const std::filesystem::path&,const std::string& expectedId={});
    const VisNativeApi& api()const;
    std::unique_ptr<PluginInstance> create(Context&,unsigned width,unsigned height,
        const std::filesystem::path& assets,const std::string& settings="{}",uint32_t flags=0,
        StartupPulse startup={})const;
};
class PluginInstance {
    std::shared_ptr<PluginLibrary::State> library_;
    void* renderer_=nullptr;
    PluginInstance(std::shared_ptr<PluginLibrary::State>,void*);
    friend class PluginLibrary;
public:
    struct CommandStatus {uint32_t type=0,state=0;uint64_t token=0;std::string message;};
    ~PluginInstance();
    PluginInstance(const PluginInstance&)=delete;
    PluginInstance& operator=(const PluginInstance&)=delete;
    void resize(unsigned width,unsigned height);
    void render(const visualizers::MusicFrame&,double time,double dt);
    wgpu::Texture image(uint32_t* encoding=nullptr)const;
    void key(uint32_t key,uint32_t modifiers=0,bool down=true);
    uint64_t controlCapabilities()const;
    bool submitCommand(uint32_t type,std::string_view utf8={},uint64_t token=0);
    std::optional<CommandStatus> commandStatus()const;
};
}
