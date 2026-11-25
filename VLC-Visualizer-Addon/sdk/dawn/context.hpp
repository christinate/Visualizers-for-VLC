// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <webgpu/webgpu_cpp.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <functional>
#include <array>
namespace visualizers::gpu {
class ShaderCache;
struct StartupPulse {
    int32_t (*callback)(void*)=nullptr;
    void* user=nullptr;
    explicit operator bool()const{return callback!=nullptr;}
};
std::string string(wgpu::StringView value);
struct Diagnostics {
    std::mutex mutex;
    std::vector<std::string> errors;
    void add(const std::string& error);
    void check();
};
// Prototype objects remain inside one executable. This is not the public plugin ABI.
class Context {
    std::shared_ptr<Diagnostics> diagnostics_=std::make_shared<Diagnostics>();
    bool ownsDevice_=true;
    struct ModuleHold {std::shared_ptr<void> value;void (*pin)(void*)=nullptr;};
    std::vector<ModuleHold> modules_;
    std::array<WGPUFuture,128> futures_{};
    void* futureUserdata_=nullptr;
    int32_t (*trackService_)(void*,WGPUFuture)=nullptr;
    void (*forgetService_)(void*,WGPUFuture)=nullptr;
    bool closing_=false;
    StartupPulse startup_{};
#if defined(__linux__)
    bool waylandColorManagement_=false;
#endif
public:
    wgpu::Instance instance;
    wgpu::Surface surface;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;
    using SurfaceFactory=std::function<wgpu::Surface(const wgpu::Instance&)>;
    explicit Context(wgpu::BackendType backend=wgpu::BackendType::Undefined,bool software=false,
                     const SurfaceFactory& surfaceFactory={},ShaderCache* cache=nullptr);
    struct BorrowedGpu {
        WGPUInstance instance;WGPUAdapter adapter;WGPUDevice device;WGPUQueue queue;
        void* future_userdata=nullptr;
        int32_t (*track_future)(void*,WGPUFuture)=nullptr;
        void (*forget_future)(void*,WGPUFuture)=nullptr;
        StartupPulse startup{};
    };
    explicit Context(const BorrowedGpu&);
    ~Context();
    Context(const Context&)=delete;
    Context& operator=(const Context&)=delete;
    void wait(wgpu::Future future,const char* operation);
    bool hasStartupPulse()const{return bool(startup_);}
    void setStartupPulse(StartupPulse pulse){startup_=pulse;}
    void pulse();
    nlohmann::json info()const;
    wgpu::ShaderModule shader(const std::string& source);
    wgpu::Texture target(unsigned width,unsigned height);
    std::vector<uint16_t> readRgba16(const wgpu::Texture& texture,unsigned width,unsigned height);
    void checkErrors(){diagnostics_->check();}
#if defined(__linux__)
    bool waylandColorManagement()const{return waylandColorManagement_;}
#endif
    void keepModule(const std::shared_ptr<void>& module,void (*pin)(void*)=nullptr);
    int32_t trackFuture(WGPUFuture)noexcept;
    void forgetFuture(WGPUFuture)noexcept;
};
wgpu::RenderPipeline pipeline(Context& context,const wgpu::ShaderModule& shader,
    const char* vertex,const char* fragment,wgpu::TextureFormat format=wgpu::TextureFormat::RGBA16Float);
void render(Context& context,const wgpu::RenderPipeline& pipeline,const wgpu::TextureView& target,
    const wgpu::BindGroup& bindings=nullptr);
float decodeHalf(uint16_t value);
uint16_t encodeHalf(float value);
}
