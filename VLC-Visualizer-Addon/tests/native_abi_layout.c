// SPDX-License-Identifier: GPL-2.0-or-later
#include "visualizer_dawn_api.h"
#include "visualizer_dawn_control.h"
#include <stddef.h>
_Static_assert(sizeof(void*)==8,"Native renderers require 64-bit targets");
_Static_assert(sizeof(VisNativeGpu)==72,"GPU ABI layout changed");
_Static_assert(sizeof(VisNativeCreateInfo)==112,"Create ABI layout changed");
_Static_assert(sizeof(VisNativeFrame)==96,"Audio ABI layout changed");
_Static_assert(sizeof(VisNativeImage)==32,"Image ABI layout changed");
_Static_assert(sizeof(VisNativeApi)==88,"Callback ABI layout changed");
_Static_assert(sizeof(VisNativeCommand)==32,"Control command C ABI layout changed");
_Static_assert(sizeof(VisNativeCommandStatus)==24,"Control status C ABI layout changed");
_Static_assert(sizeof(VisNativeControlApi)==32,"Optional control table C ABI layout changed");
uint64_t native_abi_c_layout(void){
    return sizeof(VisNativeCreateInfo)*1000000ull+sizeof(VisNativeFrame)*1000ull+offsetof(VisNativeApi,render);
}
