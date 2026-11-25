// SPDX-License-Identifier: GPL-2.0-or-later
#include "window.hpp"
#ifdef __APPLE__
#include "presentation_mac.hpp"
#endif
#include <SDL_syswm.h>
#ifdef None
#undef None
#endif
#ifdef Status
#undef Status
#endif
#ifdef Success
#undef Success
#endif
#include <algorithm>
#include <stdexcept>
#ifdef _WIN32
#include <dxgi1_6.h>
#include <wrl/client.h>
#endif

namespace visualizers::gpu {
namespace {
void sdlCheck(bool failed){if(failed)throw std::runtime_error(SDL_GetError());}
#ifdef _WIN32
SDL_SysWMinfo windowsInfo(SDL_Window* window){
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    sdlCheck(SDL_GetWindowWMInfo(window,&info)!=SDL_TRUE);
    return info;
}
#endif
}

NativeWindow::NativeWindow(const char* title,int width,int height,bool background){
    if(width<64||height<64||width>4096||height>4096)throw std::runtime_error("Invalid native window dimensions");
    SDL_SetMainReady();
#ifdef __APPLE__
    if(background)SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP,"1");
    SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES,"0");
#endif
    sdlCheck(SDL_InitSubSystem(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS)!=0);
    initialized_=true;
    Uint32 flags=SDL_WINDOW_BORDERLESS|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_SHOWN;
#ifdef __APPLE__
    flags|=SDL_WINDOW_METAL;
#elif !defined(_WIN32)
    flags|=SDL_WINDOW_VULKAN;
#endif
    window_=SDL_CreateWindow(title,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,width,height,flags);
    if(!window_){const std::string error=SDL_GetError();SDL_QuitSubSystem(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS);initialized_=false;throw std::runtime_error(error);}
}
NativeWindow::~NativeWindow(){
#ifdef __APPLE__
    if(metalView_)SDL_Metal_DestroyView(metalView_);
#endif
    if(window_)SDL_DestroyWindow(window_);
    if(initialized_)SDL_QuitSubSystem(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS);
}
wgpu::Surface NativeWindow::createSurface(const wgpu::Instance& instance){
    wgpu::SurfaceDescriptor descriptor{};
#ifdef __APPLE__
    metalView_=SDL_Metal_CreateView(window_);sdlCheck(!metalView_);
    wgpu::SurfaceSourceMetalLayer source{};source.layer=SDL_Metal_GetLayer(metalView_);
    descriptor.nextInChain=&source;return instance.CreateSurface(&descriptor);
#else
#ifdef _WIN32
    const auto info=windowsInfo(window_);
    wgpu::SurfaceSourceWindowsHWND source{};source.hinstance=info.info.win.hinstance;source.hwnd=info.info.win.window;
    descriptor.nextInChain=&source;return instance.CreateSurface(&descriptor);
#else
    SDL_SysWMinfo info{};SDL_VERSION(&info.version);sdlCheck(SDL_GetWindowWMInfo(window_,&info)!=SDL_TRUE);
#if defined(SDL_VIDEO_DRIVER_X11)
    if(info.subsystem==SDL_SYSWM_X11){
        wgpu::SurfaceSourceXlibWindow source{};source.display=info.info.x11.display;source.window=info.info.x11.window;
        descriptor.nextInChain=&source;return instance.CreateSurface(&descriptor);
    }
#endif
#if defined(SDL_VIDEO_DRIVER_WAYLAND)
    if(info.subsystem==SDL_SYSWM_WAYLAND){
        wgpu::SurfaceSourceWaylandSurface source{};source.display=info.info.wl.display;source.surface=info.info.wl.surface;
        descriptor.nextInChain=&source;return instance.CreateSurface(&descriptor);
    }
#endif
    throw std::runtime_error("SDL display driver has no supported native Dawn surface");
#endif
#endif
}
#if defined(__linux__)
wgpu::Surface NativeWindow::createWaylandExtendedSurface(const wgpu::Instance& instance)const{
#if defined(SDL_VIDEO_DRIVER_WAYLAND)
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if(SDL_GetWindowWMInfo(window_,&info)!=SDL_TRUE||info.subsystem!=SDL_SYSWM_WAYLAND)return {};
    wgpu::SurfaceColorManagement color{};
    color.colorSpace=wgpu::PredefinedColorSpace::SRGBLinear;
    color.toneMappingMode=wgpu::ToneMappingMode::Extended;
    wgpu::SurfaceSourceWaylandSurface source{};
    source.display=info.info.wl.display;
    source.surface=info.info.wl.surface;
    source.nextInChain=&color;
    wgpu::SurfaceDescriptor descriptor{};
    descriptor.nextInChain=&source;
    return instance.CreateSurface(&descriptor);
#else
    (void)instance;
    return {};
#endif
}
#endif
std::array<unsigned,2> NativeWindow::size()const{
    if(SDL_GetWindowFlags(window_)&SDL_WINDOW_MINIMIZED)return {0,0};
#ifdef _WIN32
    const auto hwnd=windowsInfo(window_).info.win.window;
    if(IsIconic(hwnd))return {0,0};
    RECT client{};
    if(!GetClientRect(hwnd,&client))throw std::runtime_error("Cannot query the visualizer client size");
    return {unsigned(std::max(0L,client.right-client.left)), unsigned(std::max(0L,client.bottom-client.top))};
#else
    int width=0,height=0;SDL_GetWindowSizeInPixels(window_,&width,&height);
    return {unsigned(std::max(0,width)),unsigned(std::max(0,height))};
#endif
}

std::array<int,4> NativeWindow::bounds()const{
#ifdef _WIN32
    // An OS resize can update the HWND before SDL updates its windowed size.
    // Read and restore the same native rectangle to preserve DPI and frame offsets.
    RECT rect{};
    if(!GetWindowRect(windowsInfo(window_).info.win.window,&rect))
        throw std::runtime_error("Cannot query the visualizer window bounds");
    return {int(rect.left),int(rect.top),int(rect.right-rect.left),int(rect.bottom-rect.top)};
#else
    std::array<int,4> value{};
    SDL_GetWindowPosition(window_,&value[0],&value[1]);
    SDL_GetWindowSize(window_,&value[2],&value[3]);
    return value;
#endif
}
void NativeWindow::validatePresentation(const nlohmann::json& adapterInfo)const{
#if !defined(_WIN32) && !defined(__APPLE__) && defined(SDL_VIDEO_DRIVER_X11)
    // NVK can expose an offscreen-capable adapter even when Xorg lacks the
    // DRI3 extension required by its window-system integration. Check before
    // Configure: that failure otherwise closes the X connection in Mesa.
    if(adapterInfo.value("software",false)||adapterInfo.value("backend",std::string())!="Vulkan"||
       adapterInfo.value("description",std::string()).find("NVK")==std::string::npos)return;
    SDL_SysWMinfo info{};SDL_VERSION(&info.version);sdlCheck(SDL_GetWindowWMInfo(window_,&info)!=SDL_TRUE);
    if(info.subsystem!=SDL_SYSWM_X11)return;
    void* xlib=SDL_LoadObject("libX11.so.6");
    if(!xlib)throw std::runtime_error("Cannot inspect X11 presentation support");
    using Query=int(*)(Display*,const char*,int*,int*,int*);
    auto query=reinterpret_cast<Query>(SDL_LoadFunction(xlib,"XQueryExtension"));
    int opcode=0,event=0,error=0;const bool supported=query&&query(info.info.x11.display,"DRI3",&opcode,&event,&error);
    SDL_UnloadObject(xlib);
    if(!supported)throw std::runtime_error("X11 DRI3 is unavailable for the NVK Vulkan driver. Use Classic or a configured software adapter.");
#else
    (void)adapterInfo;
#endif
}
DisplayPresentation NativeWindow::displayPresentation(const nlohmann::json& adapterInfo)const{
#ifdef _WIN32
    if(adapterInfo.value("backend",std::string())!="D3D12")return {false,false,"Windows HDR requires the native D3D12 backend"};
    SDL_SysWMinfo info{};SDL_VERSION(&info.version);
    if(SDL_GetWindowWMInfo(window_,&info)!=SDL_TRUE)return {false,true,"Cannot identify the visualizer display"};
    const auto monitor=MonitorFromWindow(info.info.win.window,MONITOR_DEFAULTTONEAREST);
    using Microsoft::WRL::ComPtr;ComPtr<IDXGIFactory1> factory;
    if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return {false,true,"Cannot query Windows display capabilities"};
    for(UINT a=0;;++a){
        ComPtr<IDXGIAdapter1> adapter;const auto adapterResult=factory->EnumAdapters1(a,&adapter);
        if(adapterResult==DXGI_ERROR_NOT_FOUND)break;
        if(FAILED(adapterResult))return {false,true,"Cannot enumerate Windows display adapters"};
        for(UINT n=0;;++n){
            ComPtr<IDXGIOutput> output;const auto result=adapter->EnumOutputs(n,&output);
            if(result==DXGI_ERROR_NOT_FOUND)break;
            if(FAILED(result))return {false,true,"Cannot enumerate Windows displays"};
            DXGI_OUTPUT_DESC desc{};if(FAILED(output->GetDesc(&desc))||desc.Monitor!=monitor)continue;
            ComPtr<IDXGIOutput6> advanced;DXGI_OUTPUT_DESC1 current{};
            if(FAILED(output.As(&advanced))||FAILED(advanced->GetDesc1(&current)))return {false,true,"Windows advanced-color display information is unavailable"};
            const bool enabled=current.ColorSpace==DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
            return {enabled,true,enabled?"Windows reports HDR enabled on this display":"Windows reports SDR on this display; enable HDR in Windows to use it"};
        }
    }
    return {false,true,"The visualizer display was not found in the active Windows outputs"};
#elif defined(__APPLE__)
    if(adapterInfo.value("backend",std::string())!="Metal")return {false,false,"Mac EDR requires the native Metal backend"};
    return metalDisplayPresentation(window_,metalView_,hdrIntent_);
#else
    (void)adapterInfo;
    return {false,false,"The pinned Dawn Vulkan surface supports sRGB presentation only"};
#endif
}
void NativeWindow::prepareHdrPresentation(bool requested){hdrIntent_=requested;}
void NativeWindow::configurePresentationLayer(bool linear){
#ifdef __APPLE__
    configureMetalPresentationLayer(metalView_,linear);
#else
    (void)linear;
#endif
}
nlohmann::json NativeWindow::presentationLayerInfo()const{
#ifdef __APPLE__
    return metalPresentationLayerInfo(window_,metalView_);
#else
    return nlohmann::json::object();
#endif
}
void NativeWindow::toggleFullscreen(){
    if(!fullscreen_){
        saved_=bounds();
        sdlCheck(SDL_SetWindowFullscreen(window_,SDL_WINDOW_FULLSCREEN_DESKTOP)!=0);fullscreen_=true;
    }else{
        sdlCheck(SDL_SetWindowFullscreen(window_,0)!=0);fullscreen_=false;
#ifdef _WIN32
        if(!SetWindowPos(windowsInfo(window_).info.win.window,nullptr,saved_[0],saved_[1],saved_[2],saved_[3],
                         SWP_NOZORDER|SWP_NOACTIVATE))
            throw std::runtime_error("Cannot restore the visualizer window bounds");
#else
        SDL_SetWindowPosition(window_,saved_[0],saved_[1]);SDL_SetWindowSize(window_,saved_[2],saved_[3]);
#endif
    }
}
void NativeWindow::resize(int width,int height){
    if(width<64||height<64||width>4096||height>4096)throw std::runtime_error("Invalid native window dimensions");
    SDL_SetWindowSize(window_,width,height);
}

Presenter::Presenter(Context& context,NativeWindow& window,bool hdrRequested,bool linearSupported,std::function<DisplayPresentation()> displayQuery,bool vsync):context_(context),window_(window),surface_(context.surface),adapterInfo_(context.info()),displayQuery_(std::move(displayQuery)),hdrRequested_(hdrRequested),linearSupported_(linearSupported),vsyncRequested_(vsync){
    if(!surface_)throw std::runtime_error("Native surface is unavailable");
    wgpu::SurfaceCapabilities caps{};
    if(surface_.GetCapabilities(context.adapter,&caps)!=wgpu::Status::Success)throw std::runtime_error("Cannot query native surface capabilities");
    // Terrain already encodes SDR output. Avoid a second sRGB transfer here.
    for(size_t n=0;n<caps.formatCount;++n){auto value=caps.formats[n];
        if(value==wgpu::TextureFormat::RGBA16Float)float16Surface_=true;
        if(sdrFormat_==wgpu::TextureFormat::Undefined&&(value==wgpu::TextureFormat::BGRA8Unorm||value==wgpu::TextureFormat::RGBA8Unorm))sdrFormat_=value;
    }
    if(sdrFormat_==wgpu::TextureFormat::Undefined)throw std::runtime_error("No compatible SDR presentation format");
#if defined(__linux__)
    // The patched Dawn runtime exposes float16 here only when the Vulkan
    // WSI reports the matching extended-linear sRGB pair for this surface.
    wgpu::SurfaceCapabilities extendedCaps{};
    if(!displayQuery_&&hdrRequested_&&linearSupported_&&context.waylandColorManagement()&&
            adapterInfo_.value("backend",std::string())=="Vulkan"){
        auto extended=window_.createWaylandExtendedSurface(context.instance);
        if(extended&&extended.GetCapabilities(context.adapter,&extendedCaps)==wgpu::Status::Success&&
                extendedCaps.formatCount==1&&extendedCaps.formats[0]==wgpu::TextureFormat::RGBA16Float){
            surface_=std::move(extended);
            float16Surface_=true;
            waylandExtended_=true;
        }else if(extended){
            waylandFallback_="The Wayland surface does not support float16 extended-linear sRGB";
        }
    }
#endif
    window_.prepareHdrPresentation(hdrRequested_&&linearSupported_&&float16Surface_);
    if(!displayQuery_)displayQuery_=[this]{
#if defined(__linux__)
        if(waylandExtended_)return DisplayPresentation{false,true,"Wayland compositor color management",false,1,1,true};
        if(!waylandFallback_.empty())return DisplayPresentation{false,false,waylandFallback_};
#endif
        return window_.displayPresentation(adapterInfo_);
    };
    display_=displayQuery_();choice_=choosePresentation(hdrRequested_,display_,float16Surface_,sdrFormat_,linearSupported_);
    linearSource_=choice_.format==wgpu::TextureFormat::RGBA16Float;config_.format=choice_.format;
    config_.device=context.device;config_.usage=wgpu::TextureUsage::RenderAttachment;
    config_.presentMode=wgpu::PresentMode::Fifo;config_.alphaMode=wgpu::CompositeAlphaMode::Auto;
    const auto windowedMode=[](const wgpu::SurfaceCapabilities& modes){
        for(auto mode:{wgpu::PresentMode::Immediate,wgpu::PresentMode::Mailbox})
            for(size_t n=0;n<modes.presentModeCount;++n)
                if(modes.presentModes[n]==mode)return mode;
        return wgpu::PresentMode::Fifo;
    };
    windowedPresentMode_=windowedMode(caps);
#if defined(__linux__)
    sdrWindowedPresentMode_=windowedPresentMode_;
    if(waylandExtended_)windowedPresentMode_=windowedMode(extendedCaps);
#endif
    config_.presentMode=desiredPresentMode();
    auto module=context.shader(presentationShader);pipeline_=pipeline(context,module,"vertex","fragment",config_.format);
    wgpu::BufferDescriptor uniforms{};uniforms.size=16;uniforms.usage=wgpu::BufferUsage::Uniform|wgpu::BufferUsage::CopyDst;
    outputUniforms_=context.device.CreateBuffer(&uniforms);
    struct Output {uint32_t mode;float whiteScale;float reserved[2];};
    const Output value{presentationOutputMode(choice_,linearSource_),presentationWhiteScale(display_),{}};context.queue.WriteBuffer(outputUniforms_,0,&value,sizeof(value));
    wgpu::SamplerDescriptor sampler{};sampler.magFilter=wgpu::FilterMode::Linear;sampler.minFilter=wgpu::FilterMode::Linear;
    sampler_=context.device.CreateSampler(&sampler);context.checkErrors();
}
wgpu::PresentMode Presenter::desiredPresentMode()const{
    return vsyncRequested_&&window_.fullscreen()?wgpu::PresentMode::Fifo:windowedPresentMode_;
}
nlohmann::json Presenter::presentationInfo()const{
    nlohmann::json result={{"hdr_requested",hdrRequested_},{"hdr_active",choice_.hdr},{"hdr_experimental",true},
        {"display_hdr_enabled",display_.compositorManaged?nlohmann::json(nullptr):nlohmann::json(display_.hdrEnabled)},
        {"native_hdr_transport",display_.linearTransport},
        {"vsync_requested",vsyncRequested_},{"fullscreen",window_.fullscreen()},
        {"vsync_active",vsyncRequested_&&window_.fullscreen()&&config_.presentMode==wgpu::PresentMode::Fifo},
        {"fifo_required",windowedPresentMode_==wgpu::PresentMode::Fifo},
        {"present_mode",config_.presentMode==wgpu::PresentMode::Fifo?"fifo":config_.presentMode==wgpu::PresentMode::Mailbox?"mailbox":"immediate"},
        {"surface_float16",float16Surface_},{"source_linear",linearSource_},{"plugin_linear_supported",linearSupported_},{"reason",choice_.reason},
        {"format",choice_.format==wgpu::TextureFormat::RGBA16Float?"rgba16float":sdrFormat_==wgpu::TextureFormat::BGRA8Unorm?"bgra8unorm":"rgba8unorm"},
        {"color_space",choice_.format==wgpu::TextureFormat::RGBA16Float?(display_.edr?"extended linear sRGB / Metal EDR":"linear scRGB / Rec.709"):"SDR encoded RGB"},
        {"output_mode",presentationOutputMode(choice_,linearSource_)},{"white_scale",presentationWhiteScale(display_)},{"reference_white_nits",203}};
    result.update(window_.presentationLayerInfo());return result;
}
void Presenter::updatePresentation(){
    if(!hdrRequested_)return;
    const auto now=SDL_GetTicks64();if(now<nextDisplayCheck_)return;nextDisplayCheck_=now+1000;
    display_=displayQuery_();
    // A plugin's output encoding is fixed at creation. An SDR-started session
    // stays SDR until reopened; a linear session can safely fall back on moves
    // or Windows HDR changes without resetting presets or capture.
    auto next=choosePresentation(hdrRequested_&&linearSource_,display_,float16Surface_,sdrFormat_,linearSupported_);
    if(hdrRequested_&&!linearSource_){const auto available=choosePresentation(true,display_,float16Surface_,sdrFormat_,linearSupported_);
        next.reason=available.format==wgpu::TextureFormat::RGBA16Float?"HDR is now available; reopen this visualizer to enable it":available.reason;}
    if(next.format!=config_.format){
        drain();if(configured_)surface_.Unconfigure();configured_=false;
        config_.format=next.format;pipeline_=pipeline(context_,context_.shader(presentationShader),"vertex","fragment",config_.format);
        imageBindings_=nullptr;source_=nullptr;
    }
    // EDR headroom can change without changing the float16 surface format.
    struct Output {uint32_t mode;float whiteScale;float reserved[2];};
    const Output value{presentationOutputMode(next,linearSource_),presentationWhiteScale(display_),{}};
    context_.queue.WriteBuffer(outputUniforms_,0,&value,sizeof(value));
    choice_=std::move(next);
}
Presenter::~Presenter(){if(configured_)surface_.Unconfigure();}
void Presenter::poll(){
    while(!pending_.empty()){
        auto& front=pending_.front();const auto status=context_.instance.WaitAny(front.future,0);
        if(status==wgpu::WaitStatus::TimedOut)break;
        if(status!=wgpu::WaitStatus::Success||!front.state->done||front.state->status!=wgpu::QueueWorkDoneStatus::Success)
            throw std::runtime_error("Native GPU submission failed or device was lost");
        context_.forgetFuture(WGPUFuture{front.future.id});
        pending_.pop_front();
    }
    context_.checkErrors();
}
bool Presenter::ready(){poll();return pending_.size()<2;}
#if defined(__linux__)
bool Presenter::configureWaylandSurface(){
    context_.device.PushErrorScope(wgpu::ErrorFilter::Internal);
    context_.device.PushErrorScope(wgpu::ErrorFilter::Validation);
    surface_.Configure(&config_);
    bool accepted=true;
    for(int index=0;index<2;++index){
        auto succeeded=std::make_shared<bool>(false);
        context_.wait(context_.device.PopErrorScope(wgpu::CallbackMode::WaitAnyOnly,
            [succeeded](wgpu::PopErrorScopeStatus status,wgpu::ErrorType error,wgpu::StringView){
                *succeeded=status==wgpu::PopErrorScopeStatus::Success&&error==wgpu::ErrorType::NoError;
            }),"Wayland presentation configuration");
        accepted=accepted&&*succeeded;
    }
    return accepted;
}
void Presenter::fallBackFromWaylandHdr(const char* reason){
    drain();
    surface_.Unconfigure();
    surface_=context_.surface;
    configured_=false;
    waylandExtended_=false;
    waylandFallback_=reason;
    windowedPresentMode_=sdrWindowedPresentMode_;
    config_.presentMode=desiredPresentMode();
    consecutiveSurfaceFailures_=0;
    nextDisplayCheck_=0;
    updatePresentation();
}
#endif
void Presenter::resize(unsigned width,unsigned height){
    poll();
    updatePresentation();
    if(!width||!height){if(configured_)surface_.Unconfigure();configured_=false;return;}
    if(width>16384||height>16384)throw std::runtime_error("Native surface exceeds the supported size");
    const auto presentMode=desiredPresentMode();
    if(configured_&&width==config_.width&&height==config_.height&&config_.presentMode==presentMode)return;
    if(configured_&&config_.presentMode!=presentMode){drain();surface_.Unconfigure();configured_=false;}
    config_.presentMode=presentMode;
    config_.width=width;config_.height=height;
#if defined(__linux__)
    if(waylandExtended_&&!configureWaylandSurface())fallBackFromWaylandHdr("The compositor declined extended-linear sRGB presentation");
    if(!waylandExtended_)surface_.Configure(&config_);
#else
    surface_.Configure(&config_);
#endif
    configured_=true;++configurations_;
    window_.configurePresentationLayer(config_.format==wgpu::TextureFormat::RGBA16Float);
    context_.checkErrors();
}
bool Presenter::present(const wgpu::Texture& source){
    if(!configured_||!ready())return false;
    wgpu::SurfaceTexture acquired{};surface_.GetCurrentTexture(&acquired);
    switch(acquired.status){
        case wgpu::SurfaceGetCurrentTextureStatus::Timeout:trackSubmission();return false;
        case wgpu::SurfaceGetCurrentTextureStatus::Outdated:
        case wgpu::SurfaceGetCurrentTextureStatus::Lost:
            acquired.texture=nullptr;
            if(++consecutiveSurfaceFailures_>3){
#if defined(__linux__)
                if(waylandExtended_){
                    fallBackFromWaylandHdr("The compositor lost its extended-linear sRGB surface");
                    return false;
                }
#endif
                throw std::runtime_error("Native presentation surface was repeatedly lost; reopen the visualizer");
            }
            surface_.Unconfigure();configured_=false;trackSubmission();return false;
        case wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal:
        case wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal:consecutiveSurfaceFailures_=0;break;
        default:throw std::runtime_error("Cannot acquire a native presentation texture");
    }
    if(!acquired.texture)throw std::runtime_error("Native presentation texture is empty");
    if(source_.Get()!=source.Get()){
        std::array<wgpu::BindGroupEntry,3> entries{};
        entries[0].binding=0;entries[0].textureView=source.CreateView();entries[1].binding=1;entries[1].sampler=sampler_;
        entries[2].binding=2;entries[2].buffer=outputUniforms_;entries[2].size=16;
        wgpu::BindGroupDescriptor group{};group.layout=pipeline_.GetBindGroupLayout(0);group.entryCount=entries.size();group.entries=entries.data();
        imageBindings_=context_.device.CreateBindGroup(&group);source_=source;
    }
    render(context_,pipeline_,acquired.texture.CreateView(),imageBindings_);
    surface_.Present();acquired.texture=nullptr;
    trackSubmission();context_.checkErrors();return true;
}
void Presenter::trackSubmission(){
    auto state=std::make_shared<Completion>();
    auto future=context_.queue.OnSubmittedWorkDone(wgpu::CallbackMode::WaitAnyOnly,
        [state](wgpu::QueueWorkDoneStatus status,wgpu::StringView){state->status=status;state->done=true;});
    if(context_.trackFuture(WGPUFuture{future.id})!=0)context_.wait(future,"Native GPU future overflow cleanup");
    pending_.push_back({future,state});maxPending_=std::max(maxPending_,unsigned(pending_.size()));
}
void Presenter::drain(){
    while(!pending_.empty()){
        context_.wait(pending_.front().future,"Native GPU shutdown");
        auto state=pending_.front().state;pending_.pop_front();
        if(!state->done||state->status!=wgpu::QueueWorkDoneStatus::Success)throw std::runtime_error("Native GPU did not finish successfully");
    }
    context_.checkErrors();
}
}
