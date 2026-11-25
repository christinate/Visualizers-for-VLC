#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define VISUALIZER_API_VERSION 2u
// API 2 uses an OpenGL 3.3+ context owned by the host. All callbacks execute
// on that context's main thread. No OS window handles or C++ objects cross ABI.
typedef struct VisualizerCreateInfo {
    uint32_t size,api_version;
    int32_t width,height;
    const char* assets_utf8;
} VisualizerCreateInfo;
typedef struct VisualizerAudio {
    uint32_t size,frames,sample_rate,channels;
    const float* interleaved;
    double time_seconds;
} VisualizerAudio;
typedef struct VisualizerApi {
    uint32_t size,api_version;
    const char* name_utf8;
    void* (*create)(const VisualizerCreateInfo*);
    void (*destroy)(void*);
    void (*resize)(void*,int32_t,int32_t);
    int (*render)(void*,const VisualizerAudio*);
    void (*key)(void*,int32_t,int32_t);
    int (*key_state)(void*);
} VisualizerApi;
typedef const VisualizerApi* (*VisualizerGetApiFn)(uint32_t);
#ifdef __cplusplus
}
#endif
