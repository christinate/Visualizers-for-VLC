// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "context.hpp"
#include "presentation.hpp"
#include <array>
#include <deque>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_metal.h>

namespace visualizers::gpu {
class NativeWindow {
    SDL_Window* window_=nullptr;
    SDL_MetalView metalView_=nullptr;
    bool initialized_=false;
    bool fullscreen_=false;
    bool hdrIntent_=false;
    std::array<int,4> saved_{};
public:
    NativeWindow(const char* title,int width,int height,bool background=false);
    ~NativeWindow();
    NativeWindow(const NativeWindow&)=delete;
    NativeWindow& operator=(const NativeWindow&)=delete;
    wgpu::Surface createSurface(const wgpu::Instance& instance);
#if defined(__linux__)
    wgpu::Surface createWaylandExtendedSurface(const wgpu::Instance& instance)const;
#endif
    void validatePresentation(const nlohmann::json& adapterInfo)const;
    DisplayPresentation displayPresentation(const nlohmann::json& adapterInfo)const;
    void prepareHdrPresentation(bool requested);
    void configurePresentationLayer(bool linear);
    nlohmann::json presentationLayerInfo()const;
    std::array<unsigned,2> size()const;
    std::array<int,4> bounds()const;
    void toggleFullscreen();
    bool fullscreen()const{return fullscreen_;}
    SDL_Window* handle()const{return window_;}
    void resize(int width,int height);
};

// Acquired surface textures never escape present(). GPU completion callbacks
// own their state and are polled without blocking the native UI event loop.
class Presenter {
    struct Completion {bool done=false;wgpu::QueueWorkDoneStatus status=wgpu::QueueWorkDoneStatus::Error;};
    struct Pending {wgpu::Future future;std::shared_ptr<Completion> state;};
    Context& context_;
    NativeWindow& window_;
    wgpu::Surface surface_;
    nlohmann::json adapterInfo_;
    DisplayPresentation display_;
    std::function<DisplayPresentation()> displayQuery_;
    PresentationChoice choice_;
    wgpu::TextureFormat sdrFormat_=wgpu::TextureFormat::Undefined;
    bool hdrRequested_=false,float16Surface_=false,linearSource_=false,linearSupported_=true;
    bool vsyncRequested_=false;
    wgpu::PresentMode windowedPresentMode_=wgpu::PresentMode::Fifo;
    uint64_t nextDisplayCheck_=0;
    wgpu::Buffer outputUniforms_;
    wgpu::RenderPipeline pipeline_;
    wgpu::Sampler sampler_;
    wgpu::Texture source_;
    wgpu::BindGroup imageBindings_;
    wgpu::SurfaceConfiguration config_{};
    std::deque<Pending> pending_;
    unsigned maxPending_=0;
    unsigned configurations_=0;
    unsigned consecutiveSurfaceFailures_=0;
    bool configured_=false;
#if defined(__linux__)
    bool waylandExtended_=false;
    std::string waylandFallback_;
    wgpu::PresentMode sdrWindowedPresentMode_=wgpu::PresentMode::Fifo;
    bool configureWaylandSurface();
    void fallBackFromWaylandHdr(const char* reason);
#endif
    void poll();
    void trackSubmission();
    void updatePresentation();
    wgpu::PresentMode desiredPresentMode()const;
public:
    Presenter(Context& context,NativeWindow& window,bool hdrRequested=false,bool linearSupported=true,
        std::function<DisplayPresentation()> displayQuery={},bool vsync=false);
    ~Presenter();
    Presenter(const Presenter&)=delete;
    bool ready();
    void resize(unsigned width,unsigned height);
    bool present(const wgpu::Texture& source);
    void drain();
    unsigned maximumFramesInFlight()const{return maxPending_;}
    unsigned configurations()const{return configurations_;}
    bool linearSource()const{return linearSource_;}
    nlohmann::json presentationInfo()const;
};
}
