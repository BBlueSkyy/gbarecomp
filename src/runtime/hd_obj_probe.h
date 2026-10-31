// hd_obj_probe.h — opt-in, capped, read-only PPU raster attribution.
//
// Emitted lines contain OAM attributes and tile/palette *hashes* only.
// This does NOT reconstruct copyrighted sprite sheets or render replacements.
// Caller must require a local user-created capture.flag before instantiation.
#pragma once
#include "gba_ppu.h"
#include "hd_obj_attribution.h"
#include <cstdio>
#include <cstdint>
#include <string>
#include <utility>

namespace gbarecomp {
class HdObjProbe final : public gba::GbaPpu::PresentationObserver {
public:
    explicit HdObjProbe(const std::string& path) : path_(path) {
        file_=std::fopen(path.c_str(),"wb");
        if(file_) {
            std::fprintf(file_,"{\"format\":\"gba-hd-attribution-v1\","
                "\"note\":\"hashes and OAM/BG metadata only; no graphics data\"}\n");
            std::fflush(file_);
            std::fprintf(stderr,"hd_obj_probe: capture enabled -> %s\n",path.c_str());
        }
    }
    ~HdObjProbe() override {
        if(file_) {
            std::fflush(file_);
            std::fclose(file_);
        }
    }
    HdObjProbe(const HdObjProbe&)=delete;
    HdObjProbe& operator=(const HdObjProbe&)=delete;
    bool active() const noexcept {return file_!=nullptr;}

    void scanline(const gba::GbaPpu& ppu,std::uint32_t y,std::uint16_t dispcnt,
                  const std::uint8_t* io,const std::uint8_t* vram,
                  const std::uint8_t* oam,const std::uint8_t* pal) override {
        // No instrumentation at all until explicitly armed by an app-private
        // capture.flag. One bounded snapshot every 30 frames. No guest writes.
        if(!file_ || done_ || y!=0 || !io || !vram || !oam || !pal) return;
        const auto frame=ppu.frame_count();
        if(frame%30u!=0u) return;
        ++samples_;
        std::fprintf(file_,"{\"frame\":%llu,\"dispcnt\":%u,\"bg\":[",
            static_cast<unsigned long long>(frame),static_cast<unsigned>(dispcnt));
        for(int i=0;i<4;++i) {
            auto bg=hd_obj_attribution::decode_bg(io,vram,i);
            std::fprintf(file_,"%s[%d,%d,%d,%d,\"%016llx\"]",
                         i?",":"",bg.index,bg.control,bg.hofs,bg.vofs,
                         static_cast<unsigned long long>(bg.map_hash));
        }
        std::fprintf(file_,"],\"objs\":[");
        unsigned count=0;
        for(int i=0;i<128;++i) {
            hd_obj_attribution::Obj ob{};
            if(!hd_obj_attribution::decode(oam,vram,pal,dispcnt,i,ob))continue;
            // Captures OAM index, attributes, signed native position,
            // real GBA shape/dimensions and hashes of referenced OBJ tiles.
            // Never copies or logs the tile bytes or palette colors.
            std::fprintf(file_,"%s[%d,%u,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,"
                              "\"%016llx\",\"%016llx\"]",
                count?",":"",ob.oam_index,static_cast<unsigned>(ob.attr0),
                static_cast<unsigned>(ob.attr1),static_cast<unsigned>(ob.attr2),
                ob.x,ob.y,ob.width,ob.height,ob.tile,ob.priority,
                ob.palette_bank,ob.mode,
                static_cast<unsigned long long>(ob.tile_hash),
                static_cast<unsigned long long>(ob.palette_hash));
            ++count;
        }
        std::fprintf(file_,"]}\n");
        std::fflush(file_); // survive Android background/kills
        if(samples_>=120) {
            done_=true;
            std::fprintf(stderr,"hd_obj_probe: completed %u bounded samples -> %s\n",
                         samples_,path_.c_str());
        }
    }
    void frame_ready() override {}
    void restored() noexcept override {}

private:
    std::string path_;
    std::FILE* file_=nullptr;
    unsigned samples_=0;
    bool done_=false;
};
} // namespace gbarecomp
