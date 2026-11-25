// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <webgpu/webgpu_cpp.h>
#include <filesystem>
#include <string>
#include <mutex>

namespace visualizers::gpu {
class ShaderCache {
    std::filesystem::path root_, directory_;
    std::string buildKey_;
    mutable std::mutex mutex_;
    unsigned loads_=0, stores_=0;
    static size_t load(size_t,const uint8_t*,size_t,uint8_t*,ShaderCache*) noexcept;
    static void store(size_t,const uint8_t*,size_t,const uint8_t*,ShaderCache*) noexcept;
public:
    explicit ShaderCache(const std::filesystem::path& profile,const std::filesystem::path& module,
                         const std::filesystem::path& shader);
    void attach(WGPUDawnCacheDeviceDescriptor&,const std::string& adapter);
    bool ready()const;
    void complete()noexcept;
    unsigned hits()const;
    unsigned writes()const;
};
}
