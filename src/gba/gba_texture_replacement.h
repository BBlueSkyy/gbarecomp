#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
namespace gba {
struct HdTile { unsigned size=0; std::vector<uint8_t> rgba; };
struct HdTileSample {
    const HdTile* tile=nullptr;
    uint8_t x=0,y=0;
    bool flip_x=false,flip_y=false;
};
// Presentation-owned. Nothing in this cache is guest state or serialized.
class TextureReplacementPack {
public:
    static constexpr size_t kBudget=64*1024*1024;
    static constexpr unsigned kMaxTiles=16384;
    bool enabled=true;
    bool capture=false;
    struct Captured { std::array<uint8_t,192> rgb; bool obj; };
    const auto& captured() const { return captured_; }
    bool add(uint64_t key,HdTile image);
    const HdTile* lookup(const uint8_t* tile,const uint8_t* palette,bool bpp8,bool obj);
    static uint64_t fingerprint(const uint8_t* tile,const uint8_t* palette,bool bpp8,bool obj);
    static std::array<uint8_t,192> native_rgb(const uint8_t* tile,const uint8_t* palette,bool bpp8);
    size_t bytes() const {return bytes_;}
    size_t size() const {return images_.size();}
    uint64_t hits=0,misses=0;
private:
    struct Cache {
        std::array<uint8_t,64> tile{};
        std::array<uint8_t,512> palette{};
        const HdTile* image=nullptr;
        uint64_t key=0;
        bool valid=false, bpp8=false, obj=false;
    };
    // One entry per VRAM tile address/format/namespace/palette bank; bounded.
    // Palette/content are rechecked, including after snapshot restores.
    std::unordered_map<uintptr_t,Cache> cache_;
    std::unordered_map<uint64_t,HdTile> images_;
    std::unordered_map<uint64_t,Captured> captured_;
    size_t bytes_=0;
};
}
