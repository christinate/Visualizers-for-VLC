// SPDX-License-Identifier: GPL-2.0-or-later
// Native Prism Flow module; derived renderer notices are retained with its assets.
#include "scene_dawn.hpp"
#include "visualizer_dawn_api.h"
#include "visualizer_dawn_control.h"
#include "diagnostic_text.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>

namespace {
void error(VisNativeError* out,const char* message)noexcept{
    if(!out||out->size<sizeof(*out)||!out->utf8||!out->capacity)return;
    visualizers::copyDiagnostic(out->utf8,out->capacity,message);
}
prismflow::SceneOptions settings(const VisNativeCreateInfo& info){
    prismflow::SceneOptions result;result.linear=(info.flags&VIS_NATIVE_LINEAR_OUTPUT)!=0;
    if(info.flags&~VIS_NATIVE_LINEAR_OUTPUT)throw std::runtime_error("Unsupported native output flags");
    const char* text=info.settings_json_utf8?info.settings_json_utf8:"{}";
    size_t size=0;while(size<65536&&text[size])++size;
    if(size>=65536)throw std::runtime_error("Prism Flow settings exceed the size limit");
    const auto values=nlohmann::json::parse(text,text+size,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){
        if(depth>32)throw std::runtime_error("Prism Flow settings nesting exceeds its limit");return true;
    });
    if(!values.is_object())throw std::runtime_error("Prism Flow settings must be a JSON object");
    const double ss=values.value("supersampling",1.),peak=values.value("peak",1000.),seed=values.value("seed",double(result.seed));
    if(!std::isfinite(ss)||(ss!=1&&ss!=2)||!std::isfinite(peak)||peak<80||peak>10000||
       !std::isfinite(seed)||seed<0||seed>4294967295.||seed!=std::floor(seed))throw std::runtime_error("Prism Flow settings are outside their supported ranges");
    result.supersampling=uint32_t(ss);result.seed=values.contains("seed")?uint32_t(seed):std::random_device{}();result.headroom=result.linear?std::max(1.,peak/203.):1.;return result;
}
std::filesystem::path shaders(const char* assets){
    if(!assets)throw std::runtime_error("Prism Flow assets directory is missing");
    const std::string encoded(assets);const auto root=std::filesystem::path(std::u8string(encoded.begin(),encoded.end()));
    if(!root.is_absolute()||!std::filesystem::is_regular_file(root/"manifest.json"))throw std::runtime_error("Prism Flow shader manifest is missing or invalid");
    return root;
}
struct State {
    prismflow::SceneOptions configuration;
    visualizers::gpu::Context context;
    prismflow::DawnScene scene;
    visualizers::MusicFrame audio;
    State(const VisNativeCreateInfo& info):configuration(settings(info)),
        context(visualizers::gpu::Context::BorrowedGpu{info.gpu.instance,info.gpu.adapter,info.gpu.device,info.gpu.queue,
            info.gpu.future_userdata,info.gpu.track_future,info.gpu.forget_future}),
        scene(context,shaders(info.assets_utf8),info.width,info.height,configuration){}
};
void* create(const VisNativeCreateInfo* info,VisNativeError* out)noexcept{
    try{
        if(!info||info->size<sizeof(*info)||info->api_version!=VIS_NATIVE_API_VERSION||info->gpu.size<sizeof(VisNativeGpu)||
           info->gpu.api_version!=VIS_NATIVE_API_VERSION||!info->gpu.dawn_revision_utf8||!info->gpu.track_future||!info->gpu.forget_future||
           std::strcmp(info->gpu.dawn_revision_utf8,VIS_NATIVE_DAWN_REVISION))throw std::runtime_error("Incompatible native Prism Flow create information");
        return new State(*info);
    }catch(const std::exception& value){error(out,value.what());}catch(...){error(out,"Unknown Prism Flow initialization failure");}
    return nullptr;
}
void destroy(void* value)noexcept{delete static_cast<State*>(value);}
int32_t resize(void* value,uint32_t width,uint32_t height,VisNativeError* out)noexcept{
    try{if(!value)throw std::runtime_error("Prism Flow state is missing");static_cast<State*>(value)->scene.resize(width,height);return 0;}
    catch(const std::exception& failure){error(out,failure.what());}catch(...){error(out,"Unknown Prism Flow resize failure");}return -1;
}
int32_t render(void* value,const VisNativeFrame* frame,VisNativeError* out)noexcept{
    try{
        if(!value||!frame||frame->size<sizeof(*frame)||frame->api_version!=VIS_NATIVE_API_VERSION||!frame->wave||!frame->spectrum||
           frame->wave_count!=2048||frame->spectrum_count!=1024||frame->sample_rate!=44100||
           !std::isfinite(frame->time_seconds)||frame->time_seconds<0||!std::isfinite(frame->dt_seconds)||frame->dt_seconds<0||frame->dt_seconds>1||
           !std::isfinite(frame->rms)||frame->rms<0||!std::isfinite(frame->beat_envelope)||!std::isfinite(frame->visual_energy)||frame->visual_energy<0||frame->visual_energy>256||frame->rms>16)throw std::runtime_error("Invalid native Prism Flow audio frame");
        for(unsigned n=0;n<2048;++n)if((!std::isfinite(frame->wave[n])||std::abs(frame->wave[n])>16))throw std::runtime_error("Nonfinite Prism Flow waveform sample");
        for(unsigned n=0;n<1024;++n)if((!std::isfinite(frame->spectrum[n])||std::abs(frame->spectrum[n])>65504))throw std::runtime_error("Nonfinite Prism Flow spectrum sample");
        auto& state=*static_cast<State*>(value);auto& audio=state.audio;
        std::copy_n(frame->wave,2048,audio.wave.begin());std::copy_n(frame->spectrum,1024,audio.spectrum.begin());
        audio.rms=frame->rms;audio.beatEnvelope=frame->beat_envelope;audio.beatCount=frame->beat_count;audio.beat=frame->beat!=0;
        audio.bass=frame->bass;audio.visualEnergy=frame->visual_energy;
        state.scene.render(audio,frame->time_seconds,frame->dt_seconds);return 0;
    }catch(const std::exception& failure){error(out,failure.what());}catch(...){error(out,"Unknown Prism Flow render failure");}return -1;
}
int32_t image(void* value,VisNativeImage* out,VisNativeError* failure)noexcept{
    try{
        if(!value||!out||out->size<sizeof(*out))throw std::runtime_error("Invalid Prism Flow output information");
        const auto& state=*static_cast<State*>(value);const auto& texture=state.scene.texture();
        out->width=texture.GetWidth();out->height=texture.GetHeight();out->texture=texture.Get();out->format=WGPUTextureFormat_RGBA16Float;
        out->encoding=state.configuration.linear?VIS_NATIVE_OUTPUT_LINEAR:VIS_NATIVE_OUTPUT_SRGB;out->reserved=0;return 0;
    }catch(const std::exception& value){error(failure,value.what());}catch(...){error(failure,"Unknown Prism Flow output failure");}return -1;
}
int32_t key(void* value,uint32_t code,uint32_t modifiers,uint32_t down,VisNativeError* out)noexcept{
    try{if(!value)throw std::runtime_error("Prism Flow state is missing");static_cast<State*>(value)->scene.key(code,modifiers,down!=0);return 0;}
    catch(const std::exception& failure){error(out,failure.what());}catch(...){error(out,"Unknown Prism Flow key failure");}return -1;
}
std::string commandText(const VisNativeCommand& command){
    if(!command.utf8||!command.bytes||command.bytes>32768)throw std::invalid_argument("Invalid native image path size");std::string result(command.utf8,command.bytes);
    for(size_t i=0;i<result.size();){const uint8_t first=uint8_t(result[i]);size_t n=1;uint32_t code=first,minimum=0;
        if(first>=0xc2&&first<=0xdf){n=2;code=first&31;minimum=0x80;}else if(first>=0xe0&&first<=0xef){n=3;code=first&15;minimum=0x800;}else if(first>=0xf0&&first<=0xf4){n=4;code=first&7;minimum=0x10000;}else if(first>=0x80)throw std::invalid_argument("Image path is not UTF-8");
        if(i+n>result.size())throw std::invalid_argument("Image path has truncated UTF-8");for(size_t j=1;j<n;++j){const uint8_t c=uint8_t(result[i+j]);if((c&0xc0)!=0x80)throw std::invalid_argument("Image path is not UTF-8");code=(code<<6)|(c&63);}
        if(!code||code<minimum||code>0x10ffff||(code>=0xd800&&code<=0xdfff))throw std::invalid_argument("Image path contains NUL or invalid UTF-8");i+=n;
    }return result;
}
int32_t submit(void* value,const VisNativeCommand* command,VisNativeError* out)noexcept{
    try{
        if(!value||!command||command->size<sizeof(*command)||command->flags)throw std::runtime_error("Invalid native image command");auto& scene=static_cast<State*>(value)->scene;
        if(command->type==VIS_NATIVE_COMMAND_IMPORT_IMAGE){const auto text=commandText(*command);scene.importImage(std::filesystem::path(std::u8string(text.begin(),text.end())),command->token);}
        else if(command->type==VIS_NATIVE_COMMAND_CANCEL_IMAGE){if(command->bytes)throw std::invalid_argument("Image cancellation has no payload");scene.cancelImageImport(command->token);}
        else throw std::runtime_error("Unsupported native image command");return 0;
    }catch(const std::exception& failure){error(out,failure.what());}catch(...){error(out,"Unknown native image command failure");}return -1;
}
int32_t poll(void* value,VisNativeCommandStatus* status,VisNativeError* out)noexcept{
    try{
        if(!value||!status||status->size<sizeof(*status))throw std::runtime_error("Invalid native image status");const auto& image=static_cast<State*>(value)->scene.imageImportStatus();
        status->type=VIS_NATIVE_COMMAND_IMPORT_IMAGE;status->token=image.token;status->reserved=0;
        switch(image.state){case prismflow::ImageImportState::Idle:status->state=VIS_NATIVE_COMMAND_IDLE;break;case prismflow::ImageImportState::Pending:status->state=VIS_NATIVE_COMMAND_PENDING;break;case prismflow::ImageImportState::Applied:status->state=VIS_NATIVE_COMMAND_APPLIED;break;case prismflow::ImageImportState::Failed:status->state=VIS_NATIVE_COMMAND_FAILED;break;case prismflow::ImageImportState::Cancelled:status->state=VIS_NATIVE_COMMAND_CANCELLED;break;}
        error(out,image.error.c_str());return 0;
    }catch(const std::exception& failure){error(out,failure.what());}catch(...){error(out,"Unknown native image status failure");}return -1;
}
const VisNativeApi api={sizeof(VisNativeApi),VIS_NATIVE_API_VERSION,"geiss","Prism Flow",VIS_NATIVE_DAWN_REVISION,
    &wgpuGetProcAddress,create,destroy,resize,render,image,key};
const VisNativeControlApi controls={sizeof(VisNativeControlApi),VIS_NATIVE_CONTROL_VERSION,VIS_NATIVE_CONTROL_IMAGE_IMPORT,submit,poll};
}
#ifdef _WIN32
#define VIS_EXPORT __declspec(dllexport)
#else
#define VIS_EXPORT __attribute__((visibility("default")))
#endif
extern "C" VIS_EXPORT const VisNativeApi* VisualizerGetNativeApi(uint32_t version)noexcept{
    return version==VIS_NATIVE_API_VERSION?&api:nullptr;
}
extern "C" VIS_EXPORT const VisNativeControlApi* VisualizerGetNativeControlApi(uint32_t version)noexcept{return version==VIS_NATIVE_CONTROL_VERSION?&controls:nullptr;}
