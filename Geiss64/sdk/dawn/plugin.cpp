// SPDX-License-Identifier: GPL-2.0-or-later
#include "plugin.hpp"
#include <array>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace visualizers::gpu {
namespace {
struct Error {
    std::array<char,4096> text{};
    VisNativeError value{sizeof(VisNativeError),uint32_t(text.size()),text.data()};
    std::string message(const char* fallback)const{
        const auto end=std::find(text.begin(),text.end(),'\0');
        return end==text.begin()?fallback:std::string(text.begin(),end);
    }
};
std::string bounded(const char* value,size_t limit,const char* field){
    if(!value)throw std::runtime_error(std::string("Missing native plugin ")+field);
    for(size_t n=0;n<limit;++n)if(!value[n])return std::string(value,n);
    throw std::runtime_error(std::string("Oversized native plugin ")+field);
}
}
struct PluginLibrary::State {
#ifdef _WIN32
    HMODULE handle=nullptr;
#else
    void* handle=nullptr;
#endif
    const VisNativeApi* api=nullptr;
    const VisNativeControlApi* controls=nullptr;
    VisualizerCreateWithStartupProgressV1Fn createWithStartup=nullptr;
    bool pinned=false;
    ~State(){
        if(pinned)return;
#ifdef _WIN32
        if(handle)FreeLibrary(handle);
#else
        if(handle)dlclose(handle);
#endif
    }
};
PluginLibrary::PluginLibrary(const std::filesystem::path& path,const std::string& expectedId){
    const auto absolute=std::filesystem::canonical(path);
    if(!std::filesystem::is_regular_file(absolute)||std::filesystem::file_size(absolute)>128*1024*1024)
        throw std::runtime_error("Native plugin file is invalid");
    auto state=std::make_shared<State>();
#ifdef _WIN32
    state->handle=LoadLibraryExW(absolute.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!state->handle)throw std::runtime_error("Cannot load native plugin (Windows error "+std::to_string(GetLastError())+")");
    auto query=reinterpret_cast<VisualizerGetNativeApiFn>(GetProcAddress(state->handle,"VisualizerGetNativeApi"));
    auto controlQuery=reinterpret_cast<VisualizerGetNativeControlApiFn>(GetProcAddress(state->handle,"VisualizerGetNativeControlApi"));
    state->createWithStartup=reinterpret_cast<VisualizerCreateWithStartupProgressV1Fn>(GetProcAddress(state->handle,"VisualizerCreateWithStartupProgressV1"));
#else
    state->handle=dlopen(absolute.c_str(),RTLD_NOW|RTLD_LOCAL);
    if(!state->handle)throw std::runtime_error(std::string("Cannot load native plugin: ")+dlerror());
    auto query=reinterpret_cast<VisualizerGetNativeApiFn>(dlsym(state->handle,"VisualizerGetNativeApi"));
    auto controlQuery=reinterpret_cast<VisualizerGetNativeControlApiFn>(dlsym(state->handle,"VisualizerGetNativeControlApi"));
    state->createWithStartup=reinterpret_cast<VisualizerCreateWithStartupProgressV1Fn>(dlsym(state->handle,"VisualizerCreateWithStartupProgressV1"));
#endif
    if(!query)throw std::runtime_error("Plugin does not export VisualizerGetNativeApi");
    state->api=query(VIS_NATIVE_API_VERSION);
    const auto* api=state->api;
    if(!api||api->size<sizeof(VisNativeApi)||api->api_version!=VIS_NATIVE_API_VERSION)
        throw std::runtime_error("Native plugin API version/table is incompatible");
    if(bounded(api->dawn_revision_utf8,64,"Dawn revision")!=VIS_NATIVE_DAWN_REVISION)
        throw std::runtime_error("Native plugin Dawn revision is incompatible");
    const WGPUStringView identityName={"wgpuCreateInstance",WGPU_STRLEN};
    // Windows import stubs can have a different address in each module.
    // Compare the canonical implementation returned by the shared runtime.
    const auto identity=wgpuGetProcAddress(identityName);
    if(!identity||!api->runtime_get_proc_address||api->runtime_get_proc_address(identityName)!=identity)
        throw std::runtime_error("Native plugin loaded a different Dawn runtime");
    const auto id=bounded(api->id_utf8,65,"ID");
    if(id.empty()||id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_")!=std::string::npos||(!expectedId.empty()&&id!=expectedId))
        throw std::runtime_error("Native plugin ID does not match its package");
    if(bounded(api->name_utf8,257,"name").empty()||!api->create||!api->destroy||!api->resize||!api->render||!api->image||!api->key)
        throw std::runtime_error("Native plugin callback table is incomplete");
    if(controlQuery){state->controls=controlQuery(VIS_NATIVE_CONTROL_VERSION);if(state->controls){const auto* controls=state->controls;
        if(controls->size<sizeof(VisNativeControlApi)||controls->version!=VIS_NATIVE_CONTROL_VERSION||!controls->submit||!controls->poll)
            throw std::runtime_error("Native plugin optional control table is incompatible");
    }}
    state_=std::move(state);
}
const VisNativeApi& PluginLibrary::api()const{return *state_->api;}
std::unique_ptr<PluginInstance> PluginLibrary::create(Context& context,unsigned width,unsigned height,
        const std::filesystem::path& assets,const std::string& settings,uint32_t flags,StartupPulse startup)const{
    const auto encoded=std::filesystem::canonical(assets).u8string();const std::string utf8(encoded.begin(),encoded.end());
    VisNativeCreateInfo info{};info.size=sizeof(info);info.api_version=VIS_NATIVE_API_VERSION;info.width=width;info.height=height;info.flags=flags;
    info.gpu={sizeof(VisNativeGpu),VIS_NATIVE_API_VERSION,context.instance.Get(),context.adapter.Get(),context.device.Get(),context.queue.Get(),VIS_NATIVE_DAWN_REVISION,
        &context,[](void* value,WGPUFuture future)->int32_t{return static_cast<Context*>(value)->trackFuture(future);},
        [](void* value,WGPUFuture future){static_cast<Context*>(value)->forgetFuture(future);}};
    info.assets_utf8=utf8.c_str();info.settings_json_utf8=settings.c_str();
    // Even failed initialization may leave a cancelled callback to deliver.
    context.keepModule(state_,[](void* value){static_cast<State*>(value)->pinned=true;});
    Error error;
    void* renderer=startup&&state_->createWithStartup
        ?state_->createWithStartup(&info,&error.value,startup.callback,startup.user)
        :state_->api->create(&info,&error.value);
    if(!renderer)throw std::runtime_error(error.message("Native plugin initialization failed"));
    try{return std::unique_ptr<PluginInstance>(new PluginInstance(state_,renderer));}
    catch(...){state_->api->destroy(renderer);throw;}
}
PluginInstance::PluginInstance(std::shared_ptr<PluginLibrary::State> library,void* renderer):library_(std::move(library)),renderer_(renderer){}
PluginInstance::~PluginInstance(){if(renderer_)library_->api->destroy(renderer_);}
void PluginInstance::resize(unsigned width,unsigned height){
    Error error;if(library_->api->resize(renderer_,width,height,&error.value))throw std::runtime_error(error.message("Native plugin resize failed"));
}
void PluginInstance::render(const visualizers::MusicFrame& audio,double time,double dt){
    VisNativeFrame frame{};frame.size=sizeof(frame);frame.api_version=VIS_NATIVE_API_VERSION;frame.time_seconds=time;frame.dt_seconds=dt;
    frame.wave=audio.wave.data();frame.wave_count=audio.wave.size();frame.sample_rate=44100;frame.spectrum=audio.spectrum.data();frame.spectrum_count=audio.spectrum.size();
    frame.beat=audio.beat;frame.rms=audio.rms;frame.visual_energy=audio.visualEnergy;frame.bass=audio.bass;frame.beat_envelope=audio.beatEnvelope;frame.beat_count=audio.beatCount;
    Error error;if(library_->api->render(renderer_,&frame,&error.value))throw std::runtime_error(error.message("Native plugin render failed"));
}
wgpu::Texture PluginInstance::image(uint32_t* encoding)const{
    Error error;VisNativeImage image{};image.size=sizeof(image);
    if(library_->api->image(renderer_,&image,&error.value))throw std::runtime_error(error.message("Native plugin output failed"));
    if(image.size<sizeof(image)||!image.texture||!image.width||!image.height||uint64_t(image.width)*image.height>16'777'216||
       image.format!=WGPUTextureFormat_RGBA16Float||(image.encoding!=VIS_NATIVE_OUTPUT_SRGB&&image.encoding!=VIS_NATIVE_OUTPUT_LINEAR))
        throw std::runtime_error("Native plugin output metadata is invalid");
    wgpu::Texture texture(image.texture); // AddRef a borrowed texture for host use.
    if(texture.GetWidth()!=image.width||texture.GetHeight()!=image.height||texture.GetFormat()!=wgpu::TextureFormat::RGBA16Float)
        throw std::runtime_error("Native plugin output does not match its texture");
    if(encoding)*encoding=image.encoding;return texture;
}
void PluginInstance::key(uint32_t key,uint32_t modifiers,bool down){
    Error error;if(library_->api->key(renderer_,key,modifiers,down,&error.value))throw std::runtime_error(error.message("Native plugin key handling failed"));
}
uint64_t PluginInstance::controlCapabilities()const{return library_->controls?library_->controls->capabilities:0;}
bool PluginInstance::submitCommand(uint32_t type,std::string_view payload,uint64_t token){
    if(!library_->controls||!(controlCapabilities()&VIS_NATIVE_CONTROL_IMAGE_IMPORT)||(type!=VIS_NATIVE_COMMAND_IMPORT_IMAGE&&type!=VIS_NATIVE_COMMAND_CANCEL_IMAGE))return false;
    if(payload.size()>32768)throw std::invalid_argument("Native image command exceeds the UTF-8 byte limit");
    VisNativeCommand command{sizeof(command),type,0,uint32_t(payload.size()),payload.data(),token};Error error;
    if(library_->controls->submit(renderer_,&command,&error.value))throw std::runtime_error(error.message("Native image command failed"));return true;
}
std::optional<PluginInstance::CommandStatus> PluginInstance::commandStatus()const{
    if(!library_->controls||!(controlCapabilities()&VIS_NATIVE_CONTROL_IMAGE_IMPORT))return {};VisNativeCommandStatus status{};status.size=sizeof(status);Error error;
    if(library_->controls->poll(renderer_,&status,&error.value))throw std::runtime_error(error.message("Native image status failed"));
    if(status.size<sizeof(status)||status.reserved||status.type!=VIS_NATIVE_COMMAND_IMPORT_IMAGE||status.state>VIS_NATIVE_COMMAND_CANCELLED)throw std::runtime_error("Native image status metadata is invalid");
    return CommandStatus{status.type,status.state,status.token,error.message("")};
}
}
