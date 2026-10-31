#include "gba_texture_replacement.h"
#include <cstring>
namespace gba {
uint64_t TextureReplacementPack::fingerprint(const uint8_t* tile,const uint8_t* pal,bool bpp8,bool obj) {
    uint64_t hash=14695981039346656037ull;
    auto put=[&](uint8_t b){hash=(hash^b)*1099511628211ull;};
    put(1); put(obj?1:0); // version and namespace; independent of VRAM address
    for(unsigned i=0;i<64;++i){
        const unsigned p=bpp8?tile[i]:((tile[i/2]>>((i&1)*4))&15);
        // Zero means transparent; use bit15 as an opacity marker in the key.
        const unsigned c=p ? ((pal[p*2]|(unsigned(pal[p*2+1])<<8))&0x7fff)|0x8000 : 0;
        put(c&255); put(c>>8);
    }
    return hash;
}
std::array<uint8_t,192> TextureReplacementPack::native_rgb(const uint8_t* tile,const uint8_t* pal,bool bpp8){
    std::array<uint8_t,192> out{};
    for(unsigned i=0;i<64;++i){
        unsigned p=bpp8?tile[i]:((tile[i/2]>>((i&1)*4))&15);
        unsigned c=p?(pal[p*2]|(unsigned(pal[p*2+1])<<8)):0;
        for(unsigned j=0;j<3;++j){unsigned v=(c>>(5*j))&31;out[i*3+j]=(v<<3)|(v>>2);}
    } return out;
}
bool TextureReplacementPack::add(uint64_t key,HdTile image){
    if(image.size<16 || image.size>128 || image.size%8 || image.rgba.size()!=size_t(image.size)*image.size*4 || images_.count(key) || images_.size()>=kMaxTiles || bytes_+image.rgba.size()>kBudget) return false;
    bytes_+=image.rgba.size(); images_.emplace(key,std::move(image)); cache_.clear(); return true;
}
const HdTile* TextureReplacementPack::lookup(const uint8_t* tile,const uint8_t* pal,bool bpp8,bool obj){
    if(!enabled || (images_.empty() && !capture))return nullptr;
    // Pointer pair cannot be packed losslessly: hash into a bounded cache,
    // and verify full source bytes on every hit. A collision just costs a hash.
    uintptr_t slot=(reinterpret_cast<uintptr_t>(tile)>>5) ^ (reinterpret_cast<uintptr_t>(pal)<<8) ^ (uintptr_t(bpp8)<<2) ^ uintptr_t(obj);
    if(cache_.size()>=8192 && !cache_.count(slot))cache_.clear();
    auto& entry=cache_[slot];
    size_t n=bpp8?64:32, pn=bpp8?512:32;
    if(!entry.valid || entry.bpp8!=bpp8 || entry.obj!=obj || std::memcmp(tile,entry.tile.data(),n) || std::memcmp(pal,entry.palette.data(),pn)){
        auto key=fingerprint(tile,pal,bpp8,obj);
        entry.key=key;
        auto it=images_.find(key);
        entry.image=it==images_.end()?nullptr:&it->second;
        std::memcpy(entry.tile.data(),tile,n);std::memcpy(entry.palette.data(),pal,pn);entry.valid=true;entry.bpp8=bpp8;entry.obj=obj;
    }
    if(capture && captured_.size()<1024 && !captured_.count(entry.key))
        captured_.emplace(entry.key,Captured{native_rgb(tile,pal,bpp8),obj});
    if(entry.image)++hits; else ++misses;
    return entry.image;
}
}
