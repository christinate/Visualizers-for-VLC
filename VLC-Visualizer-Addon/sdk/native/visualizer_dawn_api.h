// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef VLC_VISUALIZER_DAWN_API_H
#define VLC_VISUALIZER_DAWN_API_H
#include <stdint.h>
#include <webgpu/webgpu.h>

#define VIS_NATIVE_API_VERSION 4u
#define VIS_NATIVE_DAWN_REVISION "7daf3b0bee2c73621001ab15b256b5a84fb0b45f"
#define VIS_NATIVE_LINEAR_OUTPUT 1u
#define VIS_NATIVE_OUTPUT_SRGB 1u
#define VIS_NATIVE_OUTPUT_LINEAR 2u

#ifdef __cplusplus
extern "C" {
#endif

// UTF-8 errors use caller-owned storage. Capacity includes the terminating NUL.
// Callbacks must not throw exceptions across this C interface.
typedef struct VisNativeError {
    uint32_t size,capacity;
    char* utf8;
} VisNativeError;

// Handles are borrowed. A plugin may retain them with WebGPU AddRef/Release,
// but must never Destroy the host's device/instance or replace its callbacks.
typedef struct VisNativeGpu {
    uint32_t size,api_version;
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    const char* dawn_revision_utf8;
    void* future_userdata;
    int32_t (*track_future)(void*,WGPUFuture);
    void (*forget_future)(void*,WGPUFuture);
} VisNativeGpu;

typedef struct VisNativeCreateInfo {
    uint32_t size,api_version,width,height;
    uint32_t flags,reserved;
    VisNativeGpu gpu;
    const char* assets_utf8;
    const char* settings_json_utf8;
} VisNativeCreateInfo;

// Arrays are borrowed for the render callback only. Wave is 2048 mono float
// samples; spectrum is 1024 float bins, analyzed from 44.1 kHz stereo PCM.
typedef struct VisNativeFrame {
    uint32_t size,api_version;
    double time_seconds,dt_seconds;
    const float* wave;
    uint32_t wave_count,sample_rate;
    const float* spectrum;
    uint32_t spectrum_count,beat;
    double rms,visual_energy,bass,beat_envelope;
    uint64_t beat_count;
} VisNativeFrame;

// The texture is borrowed until resize/destroy. A host retaining it for
// presentation must AddRef and later Release it; neither side frees memory
// with the other module's allocator. The texture belongs to the shared device.
typedef struct VisNativeImage {
    uint32_t size,width,height,encoding;
    WGPUTextureFormat format;
    uint32_t reserved;
    WGPUTexture texture;
} VisNativeImage;

typedef struct VisNativeApi {
    uint32_t size,api_version;
    const char* id_utf8;
    const char* name_utf8;
    const char* dawn_revision_utf8;
    WGPUProcGetProcAddress runtime_get_proc_address;
    void* (*create)(const VisNativeCreateInfo*,VisNativeError*);
    void (*destroy)(void*);
    int32_t (*resize)(void*,uint32_t,uint32_t,VisNativeError*);
    int32_t (*render)(void*,const VisNativeFrame*,VisNativeError*);
    int32_t (*image)(void*,VisNativeImage*,VisNativeError*);
    int32_t (*key)(void*,uint32_t,uint32_t,uint32_t,VisNativeError*);
} VisNativeApi;

// Export exactly this C symbol. Return NULL for an unsupported API version.
typedef const VisNativeApi* (*VisualizerGetNativeApiFn)(uint32_t);

// Optional VisualizerCreateWithStartupProgressV1 export. It has the same
// ownership and render-thread rules as create(). The hook is borrowed only
// during creation. Return 0 from the hook to cancel initialization.
typedef int32_t (*VisNativeStartupProgressFn)(void*);
typedef void* (*VisualizerCreateWithStartupProgressV1Fn)(
    const VisNativeCreateInfo*, VisNativeError*, VisNativeStartupProgressFn, void*);

// All callbacks run on one host render thread, except internal plugin workers
// which must be joined by destroy(). The host keeps the module loaded through
// renderer destruction AND GPU instance teardown/callback cancellation.
// Plugins must track each WaitAnyOnly future until completion; track_future
// returns 0 on success. On capacity failure, finish the new future before
// returning. Other callback modes are reserved to the host. Tracking permits
// cleanup even when initialization fails before returning renderer state.
#ifdef __cplusplus
}
#endif
#endif
