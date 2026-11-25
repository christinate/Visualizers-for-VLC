// SPDX-License-Identifier: GPL-2.0-or-later
#include "visualizer_dawn_api.h"
#include "visualizer_dawn_control.h"
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
#ifndef BAD_PLUGIN_CASE
#define BAD_PLUGIN_CASE 0
#endif
#if BAD_PLUGIN_CASE == 3
extern "C" EXPORT int DeliberatelyDifferentEntry(void){return 1;}
#else
namespace {
WGPUProc foreignRuntime(WGPUStringView){return nullptr;}
void* create(const VisNativeCreateInfo*,VisNativeError*){static int state=0;return BAD_PLUGIN_CASE==7?&state:nullptr;}
void destroy(void*){}
int32_t resize(void*,uint32_t,uint32_t,VisNativeError*){return 0;}
int32_t render(void*,const VisNativeFrame*,VisNativeError*){return 0;}
int32_t image(void*,VisNativeImage*,VisNativeError*){return 0;}
int32_t key(void*,uint32_t,uint32_t,uint32_t,VisNativeError*){return 0;}
const VisNativeApi api={
    BAD_PLUGIN_CASE==2?8u:uint32_t(sizeof(VisNativeApi)),VIS_NATIVE_API_VERSION,"bad-plugin","Bad plugin fixture",
    BAD_PLUGIN_CASE==0?"wrong-revision":VIS_NATIVE_DAWN_REVISION,
    BAD_PLUGIN_CASE==1?&foreignRuntime:&wgpuGetProcAddress,create,destroy,resize,render,image,key};
}
extern "C" EXPORT const VisNativeApi* VisualizerGetNativeApi(uint32_t version){return version==VIS_NATIVE_API_VERSION?&api:nullptr;}
#if BAD_PLUGIN_CASE >= 4
namespace {
int32_t submit(void*,const VisNativeCommand*,VisNativeError*){return 0;}
int32_t poll(void*,VisNativeCommandStatus*,VisNativeError*){return BAD_PLUGIN_CASE==7?-1:0;}
const VisNativeControlApi controls={BAD_PLUGIN_CASE==4?8u:uint32_t(sizeof(VisNativeControlApi)),BAD_PLUGIN_CASE==5?999u:VIS_NATIVE_CONTROL_VERSION,
    BAD_PLUGIN_CASE==7?2ull:VIS_NATIVE_CONTROL_IMAGE_IMPORT,BAD_PLUGIN_CASE==6?nullptr:submit,poll};
}
extern "C" EXPORT const VisNativeControlApi* VisualizerGetNativeControlApi(uint32_t version){return version==VIS_NATIVE_CONTROL_VERSION?&controls:nullptr;}
#endif
#endif
