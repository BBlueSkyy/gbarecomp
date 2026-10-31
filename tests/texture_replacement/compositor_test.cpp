#include "gba_high_resolution.h"
#include "gba_texture_replacement.h"
#include <array>
#include <cassert>
#include <iostream>
int main(){
 std::array<uint8_t,0x60> io{};std::array<uint8_t,96*1024> vram{};std::array<uint8_t,1024> pal{};
 gba::TextureReplacementPack pack;gba::HdTile tile;tile.size=24;tile.rgba.assign(24*24*4,255);
 // Adjacent HD subpixels must differ inside one native pixel.
 tile.rgba[0]=255;tile.rgba[1]=0;tile.rgba[2]=0;
 tile.rgba[4]=0;tile.rgba[5]=255;tile.rgba[6]=0;
 std::array<uint8_t,32> native{};native.fill(0x11);pal[2]=31;
 auto key=gba::TextureReplacementPack::fingerprint(native.data(),pal.data(),false,false);
 assert(pack.add(key,std::move(tile)));
 gba::GbaHighResolutionFrame hd(3);hd.configure(240,160);hd.set_replacements(&pack);
 for(int y=0;y<160;++y){assert(hd.begin_line(y,0,0,0,io.data(),vram.data(),pal.data(),nullptr,false));
   hd.submit(0,31,1,0,false,false,{pack.lookup(native.data(),pal.data(),false,false),0,0,false,false});
   hd.resolve();}
 hd.latch();auto* pixels=hd.pixels();assert(pixels);assert(pixels[0]==255&&pixels[1]==0);assert(pixels[3]==0&&pixels[4]==255);
 pack.enabled=false;assert(!hd.begin_line(0,0,0,0,io.data(),vram.data(),pal.data(),nullptr,false));
 std::cout<<"HD compositor subpixels and disabled fallback passed\n";
}
