// ROM-free verification of native GBA OBJ identity and background metadata.
// g++ -std=c++17 -Isrc/gba -Isrc/runtime tests/mod_runtime/hd_obj_attribution_test.cpp -o /tmp/objtest && /tmp/objtest
#include "../../src/runtime/hd_obj_attribution.h"
#include "../../src/runtime/hd_obj_probe.h"  // compile-time API compatibility
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
using namespace gbarecomp::hd_obj_attribution;
static void put16(std::uint8_t* p,unsigned v) {
    p[0]=static_cast<std::uint8_t>(v);
    p[1]=static_cast<std::uint8_t>(v>>8);
}
int main() {
    std::vector<std::uint8_t> vram(kVramBytes,0),pal(kPaletteBytes,0);
    std::vector<std::uint8_t> oam(1024,0),io(1024,0);
    // Real GBA 16x16, 4bpp OBJ in 1D tile mode, palette bank 4.
    const unsigned dispcnt=0x1040u; // enable OBJ, 1D
    put16(oam.data(),20);                // Y=20, regular OBJ
    put16(oam.data()+2,0x4000u|30u);  // 16x16, X=30
    put16(oam.data()+4,0x4000u|0x0800u|7u); // bank4, priority2, tile7
    const unsigned base=0x10000u;
    for(unsigned i=0;i<4*32;++i)vram[base+7*32+i]=static_cast<std::uint8_t>(i);
    for(unsigned i=0;i<32;++i)pal[512+4*32+i]=static_cast<std::uint8_t>(i);
    Obj original{};
    assert(decode(oam.data(),vram.data(),pal.data(),dispcnt,0,original));
    assert(original.x==30 && original.y==20);
    assert(original.width==16 && original.height==16);
    assert(original.tile==7 && original.priority==2);
    assert(original.palette_bank==4 && !original.color256);
    const auto h=original.tile_hash,p=original.palette_hash;
    vram[base+7*32+44]^=1;
    Obj changed{};
    assert(decode(oam.data(),vram.data(),pal.data(),dispcnt,0,changed));
    assert(changed.tile_hash!=h && changed.palette_hash==p);
    pal[512+4*32+4]^=1;
    assert(decode(oam.data(),vram.data(),pal.data(),dispcnt,0,changed));
    assert(changed.palette_hash!=p);
    // A 2D-mapped sprite accesses different tile rows from 1D mode.
    assert(decode(oam.data(),vram.data(),pal.data(),0x1000u,0,changed));
    assert(changed.tile_hash!=original.tile_hash);
    // Hide, OBJ window, and invalid shape must not claim visible artwork.
    put16(oam.data(),20|0x0200u);
    assert(!decode(oam.data(),vram.data(),pal.data(),dispcnt,0,changed));
    put16(oam.data(),20|0x0800u);
    assert(!decode(oam.data(),vram.data(),pal.data(),dispcnt,0,changed));
    put16(oam.data(),20|0xc000u);
    assert(!decode(oam.data(),vram.data(),pal.data(),dispcnt,0,changed));
    // Affine double-size bounding box without increasing source tile count.
    put16(oam.data(),20|0x0300u);
    assert(decode(oam.data(),vram.data(),pal.data(),dispcnt,0,changed));
    assert(changed.affine && changed.width==32 && changed.height==32);
    // Signed/wrapped placement uses the same decode as native PPU.
    put16(oam.data()+2,0x4000u|0x1f0u);
    put16(oam.data(),248u);
    assert(decode(oam.data(),vram.data(),pal.data(),dispcnt,0,changed));
    assert(changed.x==-16 && changed.y==-8);
    // Bitmap modes reserve tile indices 0..511; >=512 is permitted.
    put16(oam.data(),20u);
    put16(oam.data()+2,0x4000u|30u);
    assert(!decode(oam.data(),vram.data(),pal.data(),0x1003u,0,changed));
    put16(oam.data()+4,0x4000u|512u);
    assert(decode(oam.data(),vram.data(),pal.data(),0x1003u,0,changed));
    assert(!decode(oam.data(),vram.data(),pal.data(),0x0000u,0,changed));
    assert(!decode(oam.data(),vram.data(),pal.data(),0x1000u,128,changed));

    // BG control + live scroll + screen-block fingerprints.
    put16(io.data()+0x0a,0x0104u); // BG1, screen block 1
    put16(io.data()+0x14,0x0025u);
    put16(io.data()+0x16,0x0047u);
    vram[2048]=42;
    auto bg=decode_bg(io.data(),vram.data(),1);
    assert(bg.index==1 && bg.control==0x0104 && bg.hofs==37 && bg.vofs==71);
    const auto before=bg.map_hash;
    vram[2048]^=1;
    assert(decode_bg(io.data(),vram.data(),1).map_hash!=before);
    std::puts("HD attribution: native OBJ mapping, palette, priority, modes, BG hashes OK");
}
