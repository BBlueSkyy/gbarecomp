#include "gba_texture_replacement.h"
#include <cassert>
#include <array>
#include <iostream>
int main() {
    gba::TextureReplacementPack pack;
    std::array<uint8_t,64> tile{};
    std::array<uint8_t,512> pal{};
    tile.fill(0x11); pal[2]=31;
    const auto key=gba::TextureReplacementPack::fingerprint(tile.data(),pal.data(),false,false);
    gba::HdTile art; art.size=24; art.rgba.assign(24*24*4,255);
    assert(pack.add(key,std::move(art)));
    assert(pack.lookup(tile.data(),pal.data(),false,false));
    assert(!pack.lookup(tile.data(),pal.data(),false,true));
    pal[2]=30; assert(!pack.lookup(tile.data(),pal.data(),false,false));
    pal[2]=31; assert(pack.lookup(tile.data(),pal.data(),false,false));
    tile[0]=0x12; assert(!pack.lookup(tile.data(),pal.data(),false,false));
    tile[0]=0x11; assert(pack.lookup(tile.data(),pal.data(),false,false));
    // Capture can be enabled after ordinary rendering filled the cache.
    assert(pack.captured().empty()); pack.capture=true;
    assert(pack.lookup(tile.data(),pal.data(),false,false));
    assert(pack.captured().size()==1);
    pack.enabled=false; assert(!pack.lookup(tile.data(),pal.data(),false,false));
    pack.enabled=true;
    gba::HdTile invalid; invalid.size=23; invalid.rgba.assign(23*23*4,255);
    assert(!pack.add(key+1,std::move(invalid)));
    std::cout << "pack content/palette/namespace/mutation/disable/dimensions passed\n";
}
