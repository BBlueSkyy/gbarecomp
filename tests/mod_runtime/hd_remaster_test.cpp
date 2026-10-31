// ROM-free stage-1 HD presentation policy checks. No SDL or game assets.
// g++ -std=c++17 -Isrc/runtime tests/mod_runtime/hd_remaster_test.cpp -o /tmp/hdremaster && /tmp/hdremaster
#include "../../src/runtime/hd_remaster.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
using namespace gbarecomp::hd_remaster;

int main() {
    std::vector<std::uint8_t> frame(240*160*3,0);
    Region r{"panel.bmp",14,20,24,12,0};
    for(int y=r.y;y<r.y+r.h;++y)
        for(int x=r.x;x<r.x+r.w;++x) {
            const auto off=(y*240+x)*3;
            frame[off]=std::uint8_t(x);
            frame[off+1]=std::uint8_t(y);
            frame[off+2]=0x55;
        }
    r.fingerprint=fingerprint_rgb888(frame.data(),240,160,r);
    assert(r.fingerprint!=0 && matches(frame.data(),240,160,r));
    assert(!matches(frame.data(),480,320,r));
    frame[(21*240+19)*3]^=0x01;
    assert(!matches(frame.data(),240,160,r)); // fail closed
    assert(safe_image_name("goku-hud.bmp"));
    assert(!safe_image_name("../other.bmp"));
    assert(!safe_image_name("/data/evil.bmp"));
    assert(!safe_image_name("C:\\evil.bmp"));
    assert(!safe_image_name("panel.PNG"));

    auto parsed=[&](const std::string& json, bool ok) {
        std::vector<Region> regions;
        std::string err;
        const bool status=parse(json,regions,err);
        assert(status==ok);
        return regions;
    };
    const std::string base=R"({"version":1,"regions":[
       {"image":"panel.bmp","x":14,"y":20,"w":24,"h":12,
        "fingerprint":"abcdef0123456789"}]})";
    const auto regs=parsed(base,true);
    assert(regs.size()==1 && regs[0].x==14);
    parsed(R"({"version":1,"regions":[
       {"image":"../escape.bmp","x":14,"y":20,"w":24,"h":12,
        "fingerprint":"abcdef0123456789"}]})",false);
    parsed(R"({"version":1,"regions":[
       {"image":"panel.bmp","x":220,"y":150,"w":30,"h":12,
        "fingerprint":"abcdef0123456789"}]})",false);
    parsed(R"({"version":1,"regions":[
       {"image":"panel.bmp","x":14,"y":20,"w":24.5,"h":12,
        "fingerprint":"abcdef0123456789"}]})",false);
    parsed(R"({"version":1,"regions":[
       {"image":"panel.bmp","x":14,"y":20,"w":24,"h":12,
        "fingerprint":"invalid"}]})",false);
    parsed(R"({"version":1,"regions":[
       {"image":"panel.bmp","x":0,"y":0,"w":100,"h":100,"fingerprint":"abcdef0123456789"},
       {"image":"duplicate.bmp","x":50,"y":50,"w":50,"h":50,"fingerprint":"abcdef0123456789"}]})",false);
    parsed(R"({"version":2,"regions":[]})",false);
    parsed(R"({"version":1,"regions":[
       {"image":"panel.bmp","x":0,"y":0,"w":240,"h":160,
        "fingerprint":"abcdef0123456789"}]})",false); // budget
    std::puts("hd_remaster: validated immutable pixel signature, geometry and manifest fail-closed");
}
