// Action icon masks traced from the two reference images supplied by the
// Dragon Ball Android port owner. 64x64, 1bpp, no external runtime assets.
// Keeps the exact silhouette proportions (four-knuckle fist, spiky-haired
// airborne fighter and rising arrow) rather than drawing placeholder glyphs.
#pragma once
#include <cstdint>
#include "host_overlay.h"

namespace gbarecomp::mobile_reference {
using C = OverlayColor;
constexpr auto D = OverlaySpace::Drawable;

static constexpr std::uint64_t fist[64] = {
 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
 0x0000000000010000ULL, 0x00000001f01fe000ULL, 0x00000007fc7ff000ULL, 0x0000000ffffff800ULL,
 0x0000001ffffff800ULL, 0x0000003ffffffc00ULL, 0x00003ffffffffc00ULL, 0x0000fffffffffc00ULL,
 0x0001fffffffffe00ULL, 0x0003fffffffffe00ULL, 0x0003fffffffffe00ULL, 0x0007fffffffffe00ULL,
 0x007ffffffffffe00ULL, 0x01fffffffffffe00ULL, 0x03ffffffff7ffe00ULL, 0x07ffffffff7ffe00ULL,
 0x07ffffffff3ffe00ULL, 0x07ffffffff3ffe00ULL, 0x07ffffefff3ffe00ULL, 0x0fffffefff3ffe00ULL,
 0x07ffffefff3ffe00ULL, 0x07ffffe7ff3ffe00ULL, 0x07ffffe7ffbffe00ULL, 0x07feffe7ffbffe00ULL,
 0x07feffe7ffbffe00ULL, 0x07fefff7ffbffe00ULL, 0x07fe7ff7ffbffe00ULL, 0x03fe7ff7ffbffc00ULL,
 0x03ff7ff7ffbffc80ULL, 0x03ff7ff3ff9ffc80ULL, 0x03ff7ff3ff9ffcc0ULL, 0x03ff3ff3ff9ff9c0ULL,
 0x01ff3ffbff8ff1c0ULL, 0x01ff3ff9ff87c3c0ULL, 0x01ff9ff9ff0007e0ULL, 0x00ff9ff8fe001fe0ULL,
 0x007f8ff83007ffe0ULL, 0x003f87e000ffffc0ULL, 0x000e000003ffffc0ULL, 0x0000000007ffffc0ULL,
 0x000000000fffff80ULL, 0x000000000fffff80ULL, 0x000000000fffff00ULL, 0x000000000ffffe00ULL,
 0x000000000ffffc00ULL, 0x000000000ffff800ULL, 0x0000000007ffe000ULL, 0x0000000000000000ULL,
 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL
};
static constexpr std::uint64_t jump[64] = {
 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
 0x000000000c000000ULL, 0x0000000018000000ULL, 0x0000000038000000ULL, 0x0000000078070000ULL,
 0x00000000f8fc0000ULL, 0x00000000f3f80000ULL, 0x00000001fff00000ULL, 0x0000000fffe00000ULL,
 0x0000001fffe00000ULL, 0x0000003ffffc0000ULL, 0x0000007fffff0000ULL, 0x0000007ffffe0000ULL,
 0x00001ffffff00000ULL, 0x000007fffff00000ULL, 0x000003fffff00000ULL, 0x000001fffff80080ULL,
 0x00001ffffff80180ULL, 0x000007ffffc00380ULL, 0x000001ffffc00780ULL, 0x00000fffffc00780ULL,
 0x000000ffffc00380ULL, 0x000000ffffc0e000ULL, 0x0000007fffc1f000ULL, 0x0000001fff81f000ULL,
 0x00000007ff07e000ULL, 0x00000007fe0f8000ULL, 0x000001ffffff0000ULL, 0x00000ffffffe0000ULL,
 0x00001ffffff80000ULL, 0x00007fffffe00000ULL, 0x0000fffffe000000ULL, 0x0001fffffe000000ULL,
 0x0007fffffe000000ULL, 0x00fff8fffe000000ULL, 0x01ffe0fffc078000ULL, 0x01ff00fffc7fe000ULL,
 0x01fc00ffffffe000ULL, 0x00fc00fffffff000ULL, 0x0078007ffffff000ULL, 0x0000007ffffff000ULL,
 0x000000fffffff000ULL, 0x000001fffffff000ULL, 0x000003ffffffe000ULL, 0x000003ffffefe000ULL,
 0x000007ffff0fc000ULL, 0x000007fff00fc000ULL, 0x00000fff8007c000ULL, 0x00000fff0007fc00ULL,
 0x00001ffe000ffc00ULL, 0x00001ffc000ffc00ULL, 0x00003ff800070000ULL, 0x00007ff800000000ULL,
 0x0001fff000000000ULL, 0x0003ffe000000000ULL, 0x0007ffe000000000ULL, 0x000fffc000000000ULL,
 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL
};

// Scan horizontal runs: 1 rectangle per contiguous run, *not* one
// rectangle per icon pixel. Zero allocations, compact .rodata, no decoding,
// no texture upload, no dependency on PNG/SDL_image.
inline void draw_mask(HostOverlay& ov, const std::uint64_t (&mask)[64],
                      float cx, float cy, float radius, C base, C shade) {
    const float step = radius * 1.72f / 64.0f;
    const float left = cx - 32.0f * step;
    const float top = cy - 32.0f * step;
    for (unsigned y = 0; y < 64; ++y) {
        const auto row = mask[y];
        if (!row) continue;
        // A little warm shading in the bottom third gives the punch depth
        // while the jump remains almost white.
        const auto color = y >= 43 ? shade : base;
        unsigned x = 0;
        while (x < 64) {
            if ((row & (std::uint64_t(1) << (63u - x))) == 0) {
                ++x;
                continue;
            }
            const unsigned start = x;
            while (x < 64 && (row & (std::uint64_t(1) << (63u - x))))
                ++x;
            ov.fill_rect(left + start * step, top + y * step,
                         (x - start) * step + 0.05f, step + 0.05f,
                         0.0f, color, D);
        }
    }
}
inline void draw_fist(HostOverlay& ov, float x, float y, float r) {
    draw_mask(ov, fist, x, y, r, {255,227,174,250}, {255,181,117,250});
}
inline void draw_jump(HostOverlay& ov, float x, float y, float r) {
    draw_mask(ov, jump, x, y, r, {224,255,247,250}, {213,255,238,250});
}
} // namespace gbarecomp::mobile_reference
