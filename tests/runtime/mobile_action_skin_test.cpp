// ROM-free visual smoke test for the title opt-in Android action skin.
// Compile on a desktop without SDL/Android/ROM:
//   c++ -std=c++20 -Isrc/runtime tests/runtime/mobile_action_skin_test.cpp -o /tmp/skin-test
//   /tmp/skin-test
#include "../../src/runtime/mobile_touch_skin.h"
#include <cassert>
#include <cstdint>
#include <cstdio>

using namespace gbarecomp;
struct Recorder final : HostOverlay {
    int circles = 0, rounded = 0, lines = 0;
    int bright_blue = 0, blue_ready = 0;
    int drawable_width() const override { return 1920; }
    int drawable_height() const override { return 1080; }
    PresentationLayout game_rect() const override { return {}; }
    int view_width() const override { return 240; }
    int view_height() const override { return 160; }
    float drawable_px_per_mm() const override { return 10.0f; }
    Insets safe_insets() const override { return {}; }
    std::uint32_t host_ms() const override { return 1234; }
    void to_drawable(float x,float y,OverlaySpace,float* a,float* b) const override {
        *a=x; *b=y;
    }
    void line(float,float,float,float,float,OverlayColor,OverlaySpace) override { ++lines; }
    void polyline(const OverlayPoint*,std::size_t,float,OverlayColor,OverlaySpace) override {}
    void fill_rect(float,float,float,float,float,OverlayColor,OverlaySpace) override { ++rounded; }
    void stroke_rect(float,float,float,float,float,float,OverlayColor,OverlaySpace) override {}
    void fill_circle(float,float,float,OverlayColor,OverlaySpace) override { ++circles; }
    void stroke_circle(float,float,float,float,OverlayColor c,OverlaySpace) override {
        if(c.r == 80 && c.g == 200 && c.b == 255) ++bright_blue;
    }
    void arc(float,float,float,float,float,float,OverlayColor,OverlaySpace) override {}
    bool text_supported() const override { return false; }
    void text(float,float,float,OverlayColor,const char*,OverlayAlign,OverlaySpace) override {}
    float text_width(const char*,float,OverlaySpace) const override { return 0; }
};
int main() {
    const mobile_skin::Layout layout{
        {240.f,690.f,115.f},{1570.f,680.f,58.f},
        {1720.f,480.f,65.f},{1720.f,230.f,65.f},
        {1720.f,720.f,65.f}, 10.f
    };
    Recorder idle, ready, pressed;
    mobile_skin::draw(idle,layout,0,false,1234);
    mobile_skin::draw(ready,layout,0,true,1234);
    mobile_skin::draw(pressed,layout,(1u<<0)|(1u<<8),true,1634);
    assert(idle.circles > 5 && idle.rounded > 4 && idle.lines > 15);
    assert(idle.bright_blue == 0);
    assert(ready.bright_blue == 2); // both Power Pole and Ki only
    assert(pressed.bright_blue == 2);
    std::puts("mobile action HUD: 4 icons, idle/ready outlines, D-pad geometry OK");
}
