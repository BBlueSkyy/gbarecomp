#include "gba_ppu.h"
#include <array>
#include <vector>
#include <cassert>
#include <cstring>
#include <iostream>
int main(){
 std::array<uint8_t,96*1024> vram{};std::array<uint8_t,1024> pal{},oam{};std::array<uint8_t,0x60> io{};
 auto u16=[](auto& bytes,unsigned off,unsigned value){bytes[off]=value;bytes[off+1]=value>>8;};
 u16(io,8,0x1f00);std::memset(vram.data(),0x11,32);std::memset(vram.data()+0x10000,0x11,32);
 u16(pal,2,31);u16(pal,0x202,31<<10);for(unsigned i=0;i<128;++i)u16(oam,i*8,0x200);
 gba::TextureReplacementPack pack;
 auto add=[&](bool obj){gba::HdTile im;im.size=24;im.rgba.assign(24*24*4,255);
  for(unsigned y=0;y<24;++y)for(unsigned x=0;x<24;++x){auto* p=im.rgba.data()+(y*24+x)*4;p[0]=obj?255:0;p[1]=(x%3)*127;p[2]=0;}
  assert(pack.add(gba::TextureReplacementPack::fingerprint(vram.data()+(obj?0x10000:0),pal.data()+(obj?0x200:0),false,obj),std::move(im)));};add(false);add(true);
 gba::GbaPpu ppu;ppu.set_texture_replacements(&pack);ppu.set_internal_resolution(3);
 auto draw=[&](unsigned display){for(unsigned y=0;y<160;++y)ppu.render_scanline(y,display,io.data(),vram.data(),oam.data(),pal.data());ppu.mark_framebuffer_latched();return ppu.high_resolution_framebuffer();};
 auto* p=draw(0x100);assert(p&&p[0]==0&&p[1]==0&&p[3]==0&&p[4]>=120&&p[4]<=132);
 std::vector<uint8_t> original(ppu.latched_framebuffer(),ppu.latched_framebuffer()+240*160*3);
 pack.enabled=false;p=draw(0x100);assert(!std::memcmp(original.data(),ppu.latched_framebuffer(),original.size()));assert(p[0]==255&&p[4]==0);
 pack.enabled=true;u16(oam,0,0);u16(oam,2,0);p=draw(0x1100);assert(p[0]==255&&p[1]==0&&p[4]>=120&&p[4]<=132);
 u16(oam,2,0x1000);p=draw(0x1100);assert(p[0]==255&&p[1]==255&&p[4]>=120&&p[4]<=132); // flipped HD subpixel order 2,1,0
 u16(oam,0,0x400);u16(oam,2,0);u16(io,0x50,0x100);u16(io,0x52,0x808);p=draw(0x1100);assert(p[0]>=120&&p[0]<=132&&p[1]==0);
 u16(oam,0,0x200);u16(io,0x50,0x81);u16(io,0x54,8);p=draw(0x100);assert(p[0]>=120&&p[0]<=132&&p[1]>=120);
 // Last 8bpp OBJ index exposes a partial tile: replacement must fall back
 // without reading 32 bytes beyond VRAM, even if the current pixel is valid.
 u16(io,0x50,0);u16(oam,0,0x2000);u16(oam,2,0);u16(oam,4,1023);
 std::memset(vram.data()+96*1024-32,1,32);draw(0x1100);
 std::cout<<"real PPU: native unchanged, HD BG/OBJ, priority, flip, alpha and brightness passed\n";
}
