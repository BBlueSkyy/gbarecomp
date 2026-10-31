// hd_obj_attribution.h — ROM-free OBJ and BG identity probes for HD remaster.
// Pure read-only decoder. No VRAM writes, no per-pixel PPU hooks, no art copies.
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>

namespace gbarecomp::hd_obj_attribution {
constexpr std::size_t kVramBytes = 96u * 1024u;
constexpr std::size_t kPaletteBytes = 1024u;
constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);
inline std::uint16_t u16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0]) |
           static_cast<std::uint16_t>(p[1] << 8);
}
inline std::uint64_t hash_bytes(const std::uint8_t* p, std::size_t n,
                                std::uint64_t h = kFnvOffset) noexcept {
    if (!p) return 0;
    for (std::size_t i=0;i<n;++i) {
        h ^= p[i];
        h *= kFnvPrime;
    }
    return h;
}
struct Obj {
    int oam_index=0, x=0, y=0, width=0, height=0;
    int tile=0, priority=0, palette_bank=0, mode=0;
    bool affine=false, color256=false, mosaic=false;
    std::uint16_t attr0=0, attr1=0, attr2=0;
    std::uint64_t tile_hash=0, palette_hash=0;
};
constexpr int kSizes[3][4][2] = {
    {{8,8},{16,16},{32,32},{64,64}},
    {{16,8},{32,8},{32,16},{64,32}},
    {{8,16},{8,32},{16,32},{32,64}}
};
inline bool decode(const std::uint8_t* oam, const std::uint8_t* vram,
                   const std::uint8_t* pal, std::uint16_t dispcnt, int index,
                   Obj& out) noexcept {
    if(!oam || !vram || !pal || index<0 || index>=128 ||
       (dispcnt & 0x1000u)==0) return false; // OBJ display disabled
    const auto* e=oam+index*8;
    const std::uint16_t a0=u16(e),a1=u16(e+2),a2=u16(e+4);
    const bool affine=(a0&0x0100u)!=0;
    if(!affine && (a0&0x0200u)) return false; // hidden OBJ
    const int shape=(a0>>14)&3, size=(a1>>14)&3;
    const int mode=(a0>>10)&3;
    if(shape==3 || mode>=2) return false; // OBJ-window stencil is not visible artwork
    const int width=kSizes[shape][size][0],height=kSizes[shape][size][1];
    const int raw_x=a1&0x1ff,raw_y=a0&0xff;
    const bool color256=(a0&0x2000u)!=0;
    const unsigned base=0x10000u; // same native PPU OBJ address interpretation
    const bool mapping1d=(dispcnt&0x0040u)!=0;
    const unsigned tile=a2&0x3ffu;
    if((dispcnt&7u)>=3u && tile<512u) return false; // bitmap modes reserve lower OBJ tiles
    std::uint64_t h=kFnvOffset;
    const unsigned tiles_w=static_cast<unsigned>(width/8);
    for(unsigned ty=0;ty<static_cast<unsigned>(height/8);++ty) {
        for(unsigned tx=0;tx<tiles_w;++tx) {
            const unsigned tile_index=tile+
                (mapping1d?(ty*tiles_w+tx)*(color256?2u:1u):
                            ty*32u+tx*(color256?2u:1u));
            const unsigned off=base+tile_index*32u;
            const unsigned len=color256?64u:32u;
            // Invalid tile references are not authenticated sprite identities.
            if(off>=kVramBytes || len>kVramBytes-off) return false;
            h=hash_bytes(vram+off,len,h);
        }
    }
    out={};
    out.oam_index=index;
    out.x=raw_x>=256?raw_x-512:raw_x;
    out.y=raw_y>=160?raw_y-256:raw_y;
    out.width=width*(affine && (a0&0x0200u)?2:1);
    out.height=height*(affine && (a0&0x0200u)?2:1);
    out.tile=static_cast<int>(tile);
    out.priority=(a2>>10)&3;
    out.palette_bank=(a2>>12)&15;
    out.mode=mode;
    out.affine=affine;
    out.color256=color256;
    out.mosaic=(a0&0x1000u)!=0;
    out.attr0=a0;out.attr1=a1;out.attr2=a2;
    out.tile_hash=h;
    const auto pal_off=512u+(color256?0u:static_cast<unsigned>(out.palette_bank)*32u);
    out.palette_hash=hash_bytes(pal+pal_off,color256?512u:32u);
    return true;
}
struct Bg {
    int index=0,control=0,hofs=0,vofs=0;
    std::uint64_t map_hash=0;
};
inline Bg decode_bg(const std::uint8_t* io,const std::uint8_t* vram,int index) noexcept {
    Bg out{};
    if(!io||!vram||index<0||index>=4)return out;
    out.index=index;
    out.control=u16(io+0x08u+index*2u);
    out.hofs=u16(io+0x10u+index*4u)&511;
    out.vofs=u16(io+0x12u+index*4u)&511;
    const unsigned screen_base=static_cast<unsigned>((out.control>>8)&31u)*2048u;
    if(screen_base+2048u<=kVramBytes)
        out.map_hash=hash_bytes(vram+screen_base,2048u);
    return out;
}
} // namespace gbarecomp::hd_obj_attribution
