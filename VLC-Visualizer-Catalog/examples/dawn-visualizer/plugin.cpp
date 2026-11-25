// SPDX-License-Identifier: MIT
#include "visualizer_dawn_api.h"
#include <webgpu/webgpu_cpp.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#if defined(_WIN32) && !defined(_M_X64) && !defined(__x86_64__)
#error This example currently targets Windows AMD64
#elif defined(__APPLE__) && !defined(__arm64__) && !defined(__aarch64__)
#error This example currently targets macOS arm64
#elif defined(__linux__) && !defined(__x86_64__)
#error This example currently targets Linux x86_64
#endif

namespace {
// Messages here are fixed ASCII. Production modules accepting external text
// should truncate only at complete UTF-8 scalar boundaries.
void error(VisNativeError* out,const char* message)noexcept{
    if(!out||out->size<sizeof(*out)||!out->utf8||!out->capacity)return;
    const auto bytes=std::min<size_t>({std::strlen(message),out->capacity-1,4095});
    std::memcpy(out->utf8,message,bytes);out->utf8[bytes]=0;
}
struct State {
    wgpu::Device device;
    wgpu::Queue queue;
    wgpu::Texture texture;
    wgpu::TextureView view;
    unsigned color=0;
    explicit State(const VisNativeCreateInfo& info){
        // Acquire takes ownership of a reference. Retain borrowed host handles
        // first; destructors release only our references and never Destroy them.
        wgpuDeviceAddRef(info.gpu.device);device=wgpu::Device::Acquire(info.gpu.device);
        wgpuQueueAddRef(info.gpu.queue);queue=wgpu::Queue::Acquire(info.gpu.queue);
        resize(info.width,info.height);
    }
    void resize(uint32_t width,uint32_t height){
        wgpu::Limits limits{};
        if(!width||!height||width>4096||height>4096||uint64_t(width)*height>16777216||
           device.GetLimits(&limits)!=wgpu::Status::Success||width>limits.maxTextureDimension2D||height>limits.maxTextureDimension2D)
            throw std::runtime_error("Invalid dimensions");
        if(texture&&texture.GetWidth()==width&&texture.GetHeight()==height)return;
        wgpu::TextureDescriptor descriptor{};descriptor.size={width,height,1};
        descriptor.dimension=wgpu::TextureDimension::e2D;descriptor.format=wgpu::TextureFormat::RGBA16Float;
        descriptor.usage=wgpu::TextureUsage::RenderAttachment|wgpu::TextureUsage::TextureBinding|wgpu::TextureUsage::CopySrc;
        auto replacement=device.CreateTexture(&descriptor);if(!replacement)throw std::runtime_error("Cannot create image");
        auto replacementView=replacement.CreateView();if(!replacementView)throw std::runtime_error("Cannot create image view");
        view=std::move(replacementView);texture=std::move(replacement);
    }
    void render(const VisNativeFrame& frame){
        const auto hue=(frame.beat_count/4+color)%3;
        const double brightness=.05+.8*std::clamp(frame.rms*2.,0.,1.);
        wgpu::RenderPassColorAttachment attachment{};attachment.view=view;
        attachment.loadOp=wgpu::LoadOp::Clear;attachment.storeOp=wgpu::StoreOp::Store;
        attachment.clearValue={hue==0?brightness:.02,hue==1?brightness:.02,hue==2?brightness:.02,1};
        wgpu::RenderPassDescriptor descriptor{};descriptor.colorAttachmentCount=1;descriptor.colorAttachments=&attachment;
        auto encoder=device.CreateCommandEncoder();auto pass=encoder.BeginRenderPass(&descriptor);pass.End();
        const auto commands=encoder.Finish();queue.Submit(1,&commands);
        // No asynchronous callbacks/readback are created by this example.
        // Host presentation follows on the same queue and bounds frames in flight.
    }
};
void* create(const VisNativeCreateInfo* info,VisNativeError* out)noexcept{
    if(!info||info->size<sizeof(*info)||info->api_version!=4||info->gpu.size<sizeof(VisNativeGpu)||info->gpu.api_version!=4||
       !info->gpu.device||!info->gpu.queue||!info->gpu.dawn_revision_utf8||std::strcmp(info->gpu.dawn_revision_utf8,VIS_NATIVE_DAWN_REVISION)||info->flags){
        error(out,"Audio Color needs API 4, the pinned shared runtime and SDR output");return nullptr;
    }
    try{return new State(*info);}catch(...){error(out,"Audio Color initialization failed");return nullptr;}
}
void destroy(void* state)noexcept{delete static_cast<State*>(state);}
int32_t resize(void* state,uint32_t width,uint32_t height,VisNativeError* out)noexcept{
    if(!state){error(out,"Audio Color state is missing");return -1;}
    try{static_cast<State*>(state)->resize(width,height);return 0;}catch(...){error(out,"Audio Color resize failed or dimensions exceed the limit");return -1;}
}
int32_t render(void* state,const VisNativeFrame* frame,VisNativeError* out)noexcept{
    if(!state||!frame||frame->size<sizeof(*frame)||frame->api_version!=4||frame->sample_rate!=44100||
       !std::isfinite(frame->time_seconds)||frame->time_seconds<0||!std::isfinite(frame->dt_seconds)||frame->dt_seconds<0||frame->dt_seconds>1||
       !std::isfinite(frame->rms)||frame->rms<0){error(out,"Invalid Audio Color frame");return -1;}
    try{static_cast<State*>(state)->render(*frame);return 0;}catch(...){error(out,"Audio Color rendering failed");return -1;}
}
int32_t image(void* state,VisNativeImage* out,VisNativeError* failure)noexcept{
    if(!state||!out||out->size<sizeof(*out)){error(failure,"Invalid Audio Color image request");return -1;}
    const auto& texture=static_cast<State*>(state)->texture;
    out->width=texture.GetWidth();out->height=texture.GetHeight();out->format=WGPUTextureFormat_RGBA16Float;
    out->texture=texture.Get();out->encoding=VIS_NATIVE_OUTPUT_SRGB;out->reserved=0;return 0;
}
int32_t key(void* state,uint32_t code,uint32_t modifiers,uint32_t down,VisNativeError* out)noexcept{
    if(!state){error(out,"Audio Color state is missing");return -1;}
    if(down&&code==32&&!modifiers)static_cast<State*>(state)->color=(static_cast<State*>(state)->color+1)%3;return 0;
}
const VisNativeApi api={sizeof(VisNativeApi),VIS_NATIVE_API_VERSION,"audio_color","Audio Color Example",VIS_NATIVE_DAWN_REVISION,
    &wgpuGetProcAddress,create,destroy,resize,render,image,key};
}
#ifdef _WIN32
#define EXAMPLE_EXPORT __declspec(dllexport)
#else
#define EXAMPLE_EXPORT __attribute__((visibility("default")))
#endif
extern "C" EXAMPLE_EXPORT const VisNativeApi* VisualizerGetNativeApi(uint32_t version)noexcept{
    return version==VIS_NATIVE_API_VERSION?&api:nullptr;
}
