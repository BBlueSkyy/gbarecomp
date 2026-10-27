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
    int bright_blue = 0, pink_punch_rim = 0, mint_jump_rim = 0;
    int icon_spans = 0;
    int dpad_idle_keys = 0, dpad_pressed_keys = 0;
    int dpad_glyph_lines = 0, dpad_outline_lines = 0, dpad_center_circles = 0;
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
    void line(float,float,float,float,float,OverlayColor c,OverlaySpace) override {
        ++lines;
        if(c.r == 200 && c.g == 205 && c.b == 212) ++dpad_glyph_lines;
        if(c.r == 174 && c.g == 180 && c.b == 187) ++dpad_outline_lines;
    }
    void polyline(const OverlayPoint*,std::size_t,float,OverlayColor,OverlaySpace) override {}
    void fill_rect(float,float,float,float,float,OverlayColor c,OverlaySpace) override {
        ++rounded;
        if(c.r == 70 && c.g == 75 && c.b == 82 && c.a == 32) ++dpad_idle_keys;
        if(c.r == 104 && c.g == 111 && c.b == 121 && c.a == 95) ++dpad_pressed_keys;
        if ((c.r == 255 && c.g == 227 && c.b == 174) ||
            (c.r == 255 && c.g == 181 && c.b == 117) ||
            (c.r == 224 && c.g == 255 && c.b == 247) ||
            (c.r == 213 && c.g == 255 && c.b == 238))
            ++icon_spans;
    }
    void stroke_rect(float,float,float,float,float,float,OverlayColor,OverlaySpace) override {}
    void fill_circle(float x,float y,float,OverlayColor,OverlaySpace) override {
        ++circles;
        if(x == 240.f && y == 690.f) ++dpad_center_circles;
    }
    void stroke_circle(float,float,float,float,OverlayColor c,OverlaySpace) override {
        if(c.r == 65 && c.g == 194 && c.b == 255) ++bright_blue;
        if(c.r == 255 && c.g == 187 && c.b == 207) ++pink_punch_rim;
        if(c.r == 218 && c.g == 255 && c.b == 247) ++mint_jump_rim;
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
    Recorder idle, ready, pressed, up_pressed;
    mobile_skin::draw(idle,layout,0,false,1234);
    mobile_skin::draw(ready,layout,0,true,1234);
    mobile_skin::draw(pressed,layout,(1u<<0)|(1u<<8),true,1634);
    mobile_skin::draw(up_pressed,layout,1u<<6,false,1234);
    assert(idle.circles > 5 && idle.rounded > 200 && idle.lines > 15);
    assert(idle.icon_spans > 150); // traced fist + leaping figure, not generic glyphs
    assert(idle.dpad_idle_keys == 4 && idle.dpad_pressed_keys == 0);
    assert(idle.dpad_glyph_lines == 20 && idle.dpad_outline_lines == 48);
    assert(idle.dpad_center_circles == 0); // no analog stick / enclosing circle
    assert(up_pressed.dpad_idle_keys == 3 && up_pressed.dpad_pressed_keys == 1);
    assert(idle.bright_blue == 0);
    assert(idle.pink_punch_rim == 1 && idle.mint_jump_rim == 1);
    assert(ready.pink_punch_rim == 1 && ready.mint_jump_rim == 1);
    assert(ready.bright_blue == 2); // both Power Pole and Ki only
    assert(pressed.bright_blue == 2);
    std::puts("mobile action HUD: four grey outlined D-pad keys, traced action icons and charge rings OK");
}
