// hd_remaster.h — opt-in, scene-verified HD *presentation* replacement.
//
// Stage 1 deliberately accepts ONLY exact-match fixed rectangles, suitable
// for a verified, static HUD panel. This is NOT an OBJ compositor: animated
// sprites and scrolling backgrounds need authoritative per-layer identities.
// The policy is host-only; guest PPU, snapshots and clock are not touched.
#pragma once
#include "mini_json.h"
#include <array>
#include <cstddef>
#include <utility>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace gbarecomp::hd_remaster {
struct Region {
    std::string image;             // safe file name, same directory as manifest
    int x=0, y=0, w=0, h=0;       // native GBA pixels (source AND destination)
    std::uint64_t fingerprint=0;  // FNV-1a of raw RGB888, unfiltered
};
inline bool safe_image_name(const std::string& name) {
    // Never open absolute paths, traversal or subdirectories from a pack.
    if(name.empty() || name.size()>96 || name[0]=='.') return false;
    if(name.size()<5 || name.substr(name.size()-4)!=".bmp") return false;
    for(char ch: name)
        if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||
             (ch>='0'&&ch<='9')||ch=='_'||ch=='-'||ch=='.')) return false;
    return name.find("..")==std::string::npos;
}
inline bool bounded_integer(const json::Value* v,int min,int max,int& out) {
    if(!v || !v->is_number() || !std::isfinite(v->number) ||
       v->number < min || v->number > max ||
       std::floor(v->number)!=v->number) return false;
    out=static_cast<int>(v->number);return true;
}
inline bool decode_fingerprint(std::string_view s,std::uint64_t& out) {
    if(s.size()!=16) return false;
    std::uint64_t v=0;
    for(char c:s) {
        unsigned d=0;
        if(c>='0'&&c<='9')d=static_cast<unsigned>(c-'0');
        else if(c>='a'&&c<='f')d=static_cast<unsigned>(c-'a'+10);
        else if(c>='A'&&c<='F')d=static_cast<unsigned>(c-'A'+10);
        else return false;
        v=(v<<4)|d;
    }
    if(v==0) return false; // reserved invalid sentinel
    out=v;return true;
}
inline bool parse(std::string_view contents, std::vector<Region>& result,
                  std::string& error) {
    result.clear();
    if(contents.size()>16384) {error="manifest too large";return false;}
    json::Value root;
    if(!json::parse(contents,root,&error) || !root.is_object()) {
        if(error.empty())error="expected JSON object";return false;
    }
    int version=0;
    if(!bounded_integer(root.get("version"),1,1,version)) {
        error="unsupported HD pack version";return false;
    }
    const auto* arr=root.get("regions");
    if(!arr || !arr->is_array() || arr->array.size()>8) {
        error="regions must be an array of up to eight entries";return false;
    }
    int verified_pixels=0;
    for(const auto& entry:arr->array) {
        if(!entry.is_object()) {error="region must be object";return false;}
        Region r;
        const auto* image=entry.get("image");
        const auto* hash=entry.get("fingerprint");
        if(!image || !image->is_string() || !safe_image_name(image->string) ||
           !hash || !hash->is_string() ||
           !decode_fingerprint(hash->string,r.fingerprint)) {
            error="invalid image filename or fingerprint";return false;
        }
        r.image=image->string;
        if(!bounded_integer(entry.get("x"),0,239,r.x) ||
           !bounded_integer(entry.get("y"),0,159,r.y) ||
           !bounded_integer(entry.get("w"),1,240,r.w) ||
           !bounded_integer(entry.get("h"),1,160,r.h) ||
           r.x+r.w>240 || r.y+r.h>160) {
            error="region outside native 240x160 GBA screen";return false;
        }
        verified_pixels+=r.w*r.h;
        if(verified_pixels>16384) {
            error="fingerprint budget exceeded";return false;
        }
        for(const auto& prev:result) {
            if(!(r.x+r.w<=prev.x || prev.x+prev.w<=r.x ||
                 r.y+r.h<=prev.y || prev.y+prev.h<=r.y)) {
                error="overlapping HD regions are not supported";return false;
            }
        }
        result.push_back(std::move(r));
    }
    return true;
}
inline std::uint64_t fingerprint_rgb888(const std::uint8_t* rgb,
         int frame_w,int frame_h,const Region& r) {
    // Exact source resolution only; never reinterpret enhanced or compressed
    // textures. A mismatch returns zero (no overlay).
    if(!rgb || frame_w!=240 || frame_h!=160 ||
       r.x<0 || r.y<0 || r.w<=0 || r.h<=0 ||
       r.x+r.w>frame_w || r.y+r.h>frame_h) return 0;
    std::uint64_t hash=UINT64_C(14695981039346656037);
    for(int y=r.y;y<r.y+r.h;++y) {
        const std::uint8_t* p=rgb+(static_cast<std::size_t>(y)*frame_w+r.x)*3;
        for(int i=0;i<r.w*3;++i) {
            hash^=p[i];
            hash*=UINT64_C(1099511628211);
        }
    }
    return hash;
}
inline bool matches(const std::uint8_t* rgb,int w,int h,const Region& r) {
    return rgb && w==240 && h==160 && r.fingerprint!=0 &&
           fingerprint_rgb888(rgb,w,h,r)==r.fingerprint;
}
} // namespace gbarecomp::hd_remaster
