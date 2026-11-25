// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef VLC_VISUALIZER_DAWN_CONTROL_H
#define VLC_VISUALIZER_DAWN_CONTROL_H
#include "visualizer_dawn_api.h"
#define VIS_NATIVE_CONTROL_VERSION 1u
#define VIS_NATIVE_CONTROL_IMAGE_IMPORT 1ull
#define VIS_NATIVE_COMMAND_IMPORT_IMAGE 1u
#define VIS_NATIVE_COMMAND_CANCEL_IMAGE 2u
#define VIS_NATIVE_COMMAND_IDLE 0u
#define VIS_NATIVE_COMMAND_PENDING 1u
#define VIS_NATIVE_COMMAND_APPLIED 2u
#define VIS_NATIVE_COMMAND_FAILED 3u
#define VIS_NATIVE_COMMAND_CANCELLED 4u
#ifdef __cplusplus
extern "C" {
#endif
// Optional extension to API 4; the base table/layout remains unchanged.
// The host borrows utf8[0..bytes) for submit only. Copy before returning if
// work is asynchronous. Import accepts an absolute local image filename,
// <=32768 UTF-8 bytes, no NUL. A nonzero token identifies the request.
// Cancel uses no payload; token 0 means current, otherwise cancel only a
// matching current import. New imports supersede earlier imports.
typedef struct VisNativeCommand {
    uint32_t size,type,flags,bytes;
    const char* utf8;
    uint64_t token;
} VisNativeCommand;
typedef struct VisNativeCommandStatus {
    uint32_t size,type,state,reserved;
    uint64_t token;
} VisNativeCommandStatus;
typedef struct VisNativeControlApi {
    uint32_t size,version;
    uint64_t capabilities;
    int32_t (*submit)(void*,const VisNativeCommand*,VisNativeError*);
    // Nonblocking latest status; error buffer may contain a recoverable image
    // failure even when poll returns 0. APPLIED means render-thread commit.
    int32_t (*poll)(void*,VisNativeCommandStatus*,VisNativeError*);
} VisNativeControlApi;
typedef const VisNativeControlApi* (*VisualizerGetNativeControlApiFn)(uint32_t);
// Export VisualizerGetNativeControlApi optionally; NULL means unsupported
// version. A module without the export keeps working through base API 4.
#ifdef __cplusplus
}
#endif
#endif
