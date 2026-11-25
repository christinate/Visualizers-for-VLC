// SPDX-License-Identifier: MIT
#include "scene_dawn.hpp"
#if defined(VIS_BEACH) || defined(VIS_OBJECTS) || defined(VIS_UNDERSEA) || defined(VIS_CORALCANYON)
#include "environment.hpp"
#endif
#include "visualizer_dawn_api.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

namespace {
#if defined(VIS_BEACH)
constexpr char Identifier[]="beach", Name[]="Beach";
using Motion=terrain::BeachMotion;
#elif defined(VIS_OBJECTS)
constexpr char Identifier[]="objects", Name[]="Objects";
using Motion=terrain::ObjectsMotion;
#elif defined(VIS_CORALCANYON)
constexpr char Identifier[]="coralcanyon", Name[]="CoralCanyon";
using Motion=terrain::UnderSeaMotion;
#elif defined(VIS_UNDERSEA)
constexpr char Identifier[]="undersea", Name[]="UnderSea";
using Motion=terrain::UnderSeaMotion;
#else
constexpr char Identifier[]="terrain", Name[]="Rhythm Terrain";
#endif

#if defined(VIS_CORALCANYON)
constexpr auto ShaderName="undersea";
#else
constexpr auto ShaderName=Identifier;
#endif

void error(VisNativeError* out,const char* message)noexcept{
    if(!out||out->size<sizeof(*out)||!out->utf8||!out->capacity)return;
    const size_t limit=std::min<size_t>(out->capacity-1,4095);size_t count=0;
    while(count<limit&&message[count])++count;
    std::memcpy(out->utf8,message,count);out->utf8[count]=0;
}
struct Settings {float cameraSeconds=60,peak=1000;uint32_t colorBeats=4;bool linear=false;};
Settings settings(const nlohmann::json& values,const VisNativeCreateInfo& info){
    Settings result;result.linear=(info.flags&VIS_NATIVE_LINEAR_OUTPUT)!=0;
    if(info.flags&~VIS_NATIVE_LINEAR_OUTPUT)throw std::runtime_error("Unsupported native output flags");
    const double camera=values.value("cameraSeconds",60.),color=values.value("colorBeats",4.),peak=values.value("peak",1000.);
    if(!std::isfinite(camera)||camera<30||camera>180||!std::isfinite(color)||color<1||color>32||color!=std::floor(color)||
       !std::isfinite(peak)||peak<80||peak>10000)throw std::runtime_error("Terrain settings are outside their supported ranges");
    result.cameraSeconds=float(camera);result.colorBeats=uint32_t(color);result.peak=float(peak);return result;
}
std::filesystem::path assetPath(const char* assets){
    if(!assets)throw std::runtime_error("Terrain assets directory is missing");
    const std::string encoded(assets);const auto root=std::filesystem::path(std::u8string(encoded.begin(),encoded.end()));
    if(!root.is_absolute())throw std::runtime_error("Scene assets directory must be absolute");
    return root;
}
nlohmann::json settingsJson(const VisNativeCreateInfo& info){
    const char* text=info.settings_json_utf8?info.settings_json_utf8:"{}";
    size_t size=0;while(size<262144&&text[size])++size;
    if(size>=262144)throw std::runtime_error("Scene settings exceed the size limit");
    auto values=nlohmann::json::parse(text,text+size);
    if(!values.is_object())throw std::runtime_error("Scene settings must be a JSON object");
    return values;
}
struct State {
    Settings configuration;
    visualizers::gpu::Context context;
#if defined(VIS_BEACH) || defined(VIS_OBJECTS) || defined(VIS_UNDERSEA) || defined(VIS_CORALCANYON)
    Motion motion;
    terrain::EnvironmentTarget scene;
    terrain::SceneUniforms uniforms{};
#else
    terrain::DawnScene scene;
#endif
    visualizers::MusicFrame audio;
    State(const VisNativeCreateInfo& info,const std::filesystem::path& assets,const nlohmann::json& values,
          visualizers::gpu::StartupPulse startup={}):configuration(settings(values,info)),
        context(visualizers::gpu::Context::BorrowedGpu{info.gpu.instance,info.gpu.adapter,info.gpu.device,info.gpu.queue,
            info.gpu.future_userdata,info.gpu.track_future,info.gpu.forget_future,startup}),
#if defined(VIS_OBJECTS)
        motion(ini(assets,values),values.value("objectStartIndex",0u)),
#elif defined(VIS_CORALCANYON)
        motion(ini(assets,values),true),
#elif defined(VIS_BEACH) || defined(VIS_UNDERSEA)
        motion(ini(assets,values)),
#endif
        scene(context,terrain::readSceneFile(assets/(std::string(ShaderName)+".wgsl"),4*1024*1024),info.width,info.height){
        context.setStartupPulse({});
#if defined(VIS_BEACH) || defined(VIS_OBJECTS) || defined(VIS_UNDERSEA) || defined(VIS_CORALCANYON)
        uniforms.output[1]=configuration.linear?1.f:0.f;
        uniforms.output[2]=configuration.peak/203.f;
#else
        scene.setMotion(terrain::TerrainMotion::fromIni(ini(assets,values)));
        scene.setOutput(configuration.linear,configuration.peak);
#endif
    }
    static terrain::SceneIni ini(const std::filesystem::path& assets,const nlohmann::json& values){
        terrain::SceneIni ini;
        ini.apply(terrain::readSceneFile(assets/(std::string(Identifier)+".ini"),32768,true));
        ini.apply(values.value("sceneIni",std::string{}));
        return ini;
    }
    void update(double time,double dt){
#if defined(VIS_BEACH) || defined(VIS_OBJECTS) || defined(VIS_UNDERSEA) || defined(VIS_CORALCANYON)
        motion.update(audio,time,dt,uniforms);
        scene.draw(uniforms);
#else
        (void)dt;
        scene.update(audio,time,configuration.cameraSeconds,configuration.colorBeats);
#endif
    }
    void nextMode(){
#if defined(VIS_BEACH) || defined(VIS_OBJECTS) || defined(VIS_UNDERSEA) || defined(VIS_CORALCANYON)
        motion.nextMode();
#else
        scene.nextMode();
#endif
    }
    void toggleModeLock(){
#if defined(VIS_BEACH) || defined(VIS_OBJECTS) || defined(VIS_UNDERSEA) || defined(VIS_CORALCANYON)
        motion.toggleModeLock();
#else
        scene.toggleModeLock();
#endif
    }
};
void* createScene(const VisNativeCreateInfo* info,VisNativeError* out,visualizers::gpu::StartupPulse startup)noexcept{
    try{
        if(!info||info->size<sizeof(*info)||info->api_version!=VIS_NATIVE_API_VERSION||info->gpu.size<sizeof(VisNativeGpu)||
           info->gpu.api_version!=VIS_NATIVE_API_VERSION||!info->gpu.dawn_revision_utf8||!info->gpu.track_future||!info->gpu.forget_future||
           std::strcmp(info->gpu.dawn_revision_utf8,VIS_NATIVE_DAWN_REVISION))throw std::runtime_error("Incompatible native scene create information");
        return new State(*info,assetPath(info->assets_utf8),settingsJson(*info),startup);
    }catch(const std::exception& value){error(out,value.what());}catch(...){error(out,"Unknown scene initialization failure");}
    return nullptr;
}
void* create(const VisNativeCreateInfo* info,VisNativeError* out)noexcept{return createScene(info,out,{});}
void destroy(void* value)noexcept{delete static_cast<State*>(value);}
int32_t resize(void* value,uint32_t width,uint32_t height,VisNativeError* out)noexcept{
    try{if(!value)throw std::runtime_error("Scene state is missing");static_cast<State*>(value)->scene.resize(width,height);return 0;}
    catch(const std::exception& failure){error(out,failure.what());}catch(...){error(out,"Unknown scene resize failure");}return -1;
}
int32_t render(void* value,const VisNativeFrame* frame,VisNativeError* out)noexcept{
    try{
        if(!value||!frame||frame->size<sizeof(*frame)||frame->api_version!=VIS_NATIVE_API_VERSION||!frame->wave||!frame->spectrum||
           frame->wave_count!=2048||frame->spectrum_count!=1024||frame->sample_rate!=44100||
           !std::isfinite(frame->time_seconds)||frame->time_seconds<0||!std::isfinite(frame->dt_seconds)||frame->dt_seconds<0||frame->dt_seconds>1||
           !std::isfinite(frame->rms)||frame->rms<0||!std::isfinite(frame->beat_envelope))throw std::runtime_error("Invalid native scene audio frame");
        for(unsigned n=0;n<2048;++n)if(!std::isfinite(frame->wave[n]))throw std::runtime_error("Nonfinite scene waveform sample");
        for(unsigned n=0;n<1024;++n)if(!std::isfinite(frame->spectrum[n]))throw std::runtime_error("Nonfinite scene spectrum sample");
        auto& state=*static_cast<State*>(value);auto& audio=state.audio;
        std::copy_n(frame->wave,2048,audio.wave.begin());std::copy_n(frame->spectrum,1024,audio.spectrum.begin());
        audio.rms=frame->rms;audio.beatEnvelope=frame->beat_envelope;audio.beatCount=frame->beat_count;audio.beat=frame->beat!=0;
        audio.bass=frame->bass;audio.visualEnergy=frame->visual_energy;
        state.update(frame->time_seconds,frame->dt_seconds);return 0;
    }catch(const std::exception& failure){error(out,failure.what());}catch(...){error(out,"Unknown scene render failure");}return -1;
}
int32_t image(void* value,VisNativeImage* out,VisNativeError* failure)noexcept{
    try{
        if(!value||!out||out->size<sizeof(*out))throw std::runtime_error("Invalid scene output information");
        const auto& state=*static_cast<State*>(value);const auto& texture=state.scene.texture();
        out->width=texture.GetWidth();out->height=texture.GetHeight();out->texture=texture.Get();out->format=WGPUTextureFormat_RGBA16Float;
        out->encoding=state.configuration.linear?VIS_NATIVE_OUTPUT_LINEAR:VIS_NATIVE_OUTPUT_SRGB;out->reserved=0;return 0;
    }catch(const std::exception& value){error(failure,value.what());}catch(...){error(failure,"Unknown scene output failure");}return -1;
}
int32_t key(void* value,uint32_t code,uint32_t modifiers,uint32_t down,VisNativeError* out)noexcept{
    try{
        if(!value)throw std::runtime_error("Scene state is missing");
        if(down&&!modifiers){
            auto& state=*static_cast<State*>(value);
            if(code==' ')state.nextMode();
            else if(code=='l'||code=='L')state.toggleModeLock();
#if defined(VIS_UNDERSEA) || defined(VIS_CORALCANYON)
            else if(code=='c'||code=='C')state.motion.nextCamera();
#elif defined(VIS_BEACH)
            else if(code=='d'||code=='D')state.motion.previewDolphin();
#endif
        }
        return 0;
    }
    catch(const std::exception& failure){error(out,failure.what());}catch(...){error(out,"Unknown scene key failure");}return -1;
}
const VisNativeApi api={sizeof(VisNativeApi),VIS_NATIVE_API_VERSION,Identifier,Name,VIS_NATIVE_DAWN_REVISION,
    &wgpuGetProcAddress,create,destroy,resize,render,image,key};
}
#ifdef _WIN32
#define VIS_EXPORT __declspec(dllexport)
#else
#define VIS_EXPORT __attribute__((visibility("default")))
#endif
extern "C" VIS_EXPORT const VisNativeApi* VisualizerGetNativeApi(uint32_t version)noexcept{
    return version==VIS_NATIVE_API_VERSION?&api:nullptr;
}
#ifdef VIS_BEACH
extern "C" VIS_EXPORT void* VisualizerCreateWithStartupProgressV1(const VisNativeCreateInfo* info,
        VisNativeError* out,VisNativeStartupProgressFn callback,void* user)noexcept{
    return createScene(info,out,{callback,user});
}
#endif
