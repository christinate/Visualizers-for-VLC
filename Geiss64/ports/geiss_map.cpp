// Portable restoration of the bilinear warp in Geiss 4.30 (BSD-3-Clause).
#include "Proc_map.h"
#include <cstdint>
#include <cstring>
void Process_Map(void* source, void* destination) {
    const auto started = clock();
    auto src = static_cast<const uint8_t*>(source);
    auto dst = static_cast<uint8_t*>(destination);
    ptrdiff_t offset = 0;
    const int stride = FXW * (iDispBits == 8 ? 1 : 4);
    const int channels = iDispBits == 8 ? 1 : 3;
    const int step = iDispBits == 8 ? 1 : 4;
    const ptrdiff_t length = ptrdiff_t(stride) * FXH;
    for (int n = 0; n < FX_YCUT_NUM_LINES; ++n) {
        const auto weights = DATA_FX + FX_YCUT_xFXW_x8 + ptrdiff_t(n) * 8;
        int32_t delta;
        memcpy(&delta, weights + 4, sizeof(delta));
        offset += delta;
        const auto out = ptrdiff_t(FX_YCUT_xFXW + n) * step;
        if (offset < 0 || offset + stride + step + channels > length) {
            memset(dst + out, 0, step);
            continue;
        }
        for (int c = 0; c < channels; ++c) {
            dst[out+c] = uint8_t((src[offset+c]*weights[0] + src[offset+step+c]*weights[1]
                + src[offset+stride+c]*weights[2] + src[offset+stride+step+c]*weights[3]) >> 8);
        }
        if (step == 4) dst[out+3] = 255;
    }
    core_clock_time += clock() - started;
}
