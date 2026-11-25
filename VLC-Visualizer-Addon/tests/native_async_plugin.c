// SPDX-License-Identifier: GPL-2.0-or-later
// Deliberately leaves a one-shot callback pending on failed initialization.
// The host must keep this module loaded until GPU-instance cancellation.
#include "visualizer_dawn_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
struct Pending {WGPUBuffer buffer;WGPUFuture future;void* owner;void (*forget)(void*,WGPUFuture);char marker[4096];};
static void completed(WGPUMapAsyncStatus status,WGPUStringView message,void* first,void* second){
    struct Pending* state=(struct Pending*)first;
    (void)message;(void)second;
    state->forget(state->owner,state->future);
    if(status==WGPUMapAsyncStatus_Success)wgpuBufferUnmap(state->buffer);
    wgpuBufferRelease(state->buffer);
    FILE* output=fopen(state->marker,"wb");if(output){fprintf(output,"callback completed before module unload: %u\n",(unsigned)status);fclose(output);}
    free(state);
}
static void* create(const VisNativeCreateInfo* info,VisNativeError* error){
    struct Pending* state=(struct Pending*)calloc(1,sizeof(struct Pending));
    if(!state)return NULL;
    state->owner=info->gpu.future_userdata;state->forget=info->gpu.forget_future;
    if(snprintf(state->marker,sizeof(state->marker),"%s/callback-completed.txt",info->assets_utf8)<0){free(state);return NULL;}
    WGPUBufferDescriptor descriptor=WGPU_BUFFER_DESCRIPTOR_INIT;descriptor.size=16;descriptor.usage=WGPUBufferUsage_MapRead|WGPUBufferUsage_CopyDst;
    state->buffer=wgpuDeviceCreateBuffer(info->gpu.device,&descriptor);
    WGPUBufferMapCallbackInfo callback=WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callback.mode=WGPUCallbackMode_WaitAnyOnly;callback.callback=completed;callback.userdata1=state;
    state->future=wgpuBufferMapAsync(state->buffer,WGPUMapMode_Read,0,16,callback);
    if(info->gpu.track_future(state->owner,state->future)!=0){
        WGPUFutureWaitInfo wait=WGPU_FUTURE_WAIT_INFO_INIT;wait.future=state->future;
        wgpuInstanceWaitAny(info->gpu.instance,1,&wait,UINT64_MAX);
    }
    if(error&&error->utf8&&error->capacity){const char* message="Intentional deferred-callback initialization failure";
        size_t n=strlen(message);if(n>=error->capacity)n=error->capacity-1;memcpy(error->utf8,message,n);error->utf8[n]=0;}
    return NULL;
}
static void destroy(void* state){(void)state;}
static int32_t resize(void* state,uint32_t w,uint32_t h,VisNativeError* error){(void)state;(void)w;(void)h;(void)error;return -1;}
static int32_t render(void* state,const VisNativeFrame* frame,VisNativeError* error){(void)state;(void)frame;(void)error;return -1;}
static int32_t image(void* state,VisNativeImage* result,VisNativeError* error){(void)state;(void)result;(void)error;return -1;}
static int32_t key(void* state,uint32_t code,uint32_t modifiers,uint32_t down,VisNativeError* error){(void)state;(void)code;(void)modifiers;(void)down;(void)error;return -1;}
static const VisNativeApi api={sizeof(VisNativeApi),VIS_NATIVE_API_VERSION,"async-probe","Async lifetime fixture",VIS_NATIVE_DAWN_REVISION,
    wgpuGetProcAddress,create,destroy,resize,render,image,key};
EXPORT const VisNativeApi* VisualizerGetNativeApi(uint32_t version){return version==VIS_NATIVE_API_VERSION?&api:NULL;}
