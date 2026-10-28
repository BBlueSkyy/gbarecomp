// ROM-free smoke tests of compact Dragon Ball neutral-grey touch artwork.
// c++ -std=c++20 -Isrc/runtime tests/runtime/mobile_action_skin_test.cpp -o /tmp/skin-test
// /tmp/skin-test
#include "../../src/runtime/mobile_touch_skin.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace gbarecomp;

struct Recorder final : HostOverlay {
    int circles=0, filled=0, lines=0, grey_dpad=0, grey_rims=0;
    int charged_rims=0, neutral_violations=0, character_spans=0;
    int center_circles=0;
    static bool neutral(OverlayColor c) {
        const int r=c.r,g=c.g,b=c.b;
        return std::abs(r-g)<=8 && std::abs(g-b)<=8;
    }
    void record(OverlayColor c) {
        if(!neutral(c))++neutral_violations;
    }
    int drawable_width() const override{return 1920;}
    int drawable_height() const override{return 1080;}
    PresentationLayout game_rect() const override{return {};}
    int view_width() const override{return 240;}
    int view_height() const override{return 160;}
    float drawable_px_per_mm() const override{return 10.f;}
    Insets safe_insets() const override{return {};}
    std::uint32_t host_ms() const override{return 1234;}
    void to_drawable(float x,float y,OverlaySpace,float* a,float* b) const override {*a=x;*b=y;}
    void line(float,float,float,float,float,OverlayColor c,OverlaySpace) override{++lines;record(c);}
    void polyline(const OverlayPoint*,std::size_t,float,OverlayColor c,OverlaySpace) override{record(c);}
    void fill_rect(float,float,float,float,float,OverlayColor c,OverlaySpace) override{
        ++filled;record(c);
        if((c.r==236&&c.g==236&&c.b==236)||
           (c.r==232&&c.g==232&&c.b==232)||
           (c.r==163&&c.g==163&&c.b==163)||
           (c.r==160&&c.g==160&&c.b==160))
            ++character_spans;
    }
    void stroke_rect(float,float,float,float,float,float,OverlayColor c,OverlaySpace) override {record(c);}
    void fill_circle(float x,float y,float,OverlayColor c,OverlaySpace) override{
        ++circles;record(c);
        if(c.r==92&&c.g==96&&c.b==102)++grey_dpad;
        if(x==240.f&&y==690.f)++center_circles;
    }
    void stroke_circle(float,float,float,float,OverlayColor c,OverlaySpace) override{
        record(c);++grey_rims;
        if(c.r==241&&c.g==244&&c.b==246)++charged_rims;
    }
    void arc(float,float,float,float,float,float,OverlayColor c,OverlaySpace) override {record(c);}
    bool text_supported() const override{return false;}
    void text(float,float,float,OverlayColor c,const char*,OverlayAlign,OverlaySpace) override {record(c);}
    float text_width(const char*,float,OverlaySpace) const override {return 0.f;}
};
int main(){
    const mobile_skin::Layout l{
        {240.f,690.f,115.f},{1570.f,680.f,58.f},
        {1720.f,480.f,65.f},{1720.f,230.f,65.f},
        {1540.f,720.f,59.f},10.f
    };
    Recorder idle, charged, pressed;
    mobile_skin::draw(idle,l,0,false,1234);
    mobile_skin::draw(charged,l,0,true,1234);
    mobile_skin::draw(pressed,l,(1u<<0)|(1u<<8),true,1634);
    assert(idle.circles>=10); // independent 4-way D-pad, 4 actions, energy
    assert(idle.grey_dpad>=4); // circle keys are light neutral grey
    assert(idle.center_circles==0); // no central D-pad disc or analog
    assert(idle.character_spans>30); // actual grayscale jump + staff masks
    assert(idle.filled>75); // filled directional triangles + image spans
    assert(idle.lines>15); // articulated two-hand Ki, not a generic dot
    assert(idle.charged_rims==0);
    assert(charged.charged_rims==2); // one each for R/Ki and L/Pole
    assert(pressed.charged_rims==2);
    assert(idle.neutral_violations==0);
    assert(charged.neutral_violations==0);
    assert(pressed.neutral_violations==0);
    Recorder nav;
    mobile_skin::navigation(nav,25.f,900.f,95.f,53.f,false,false);
    mobile_skin::navigation(nav,900.f,900.f,130.f,53.f,true,true);
    assert(nav.filled>20 && nav.lines>5 && nav.neutral_violations==0);
    std::puts("compact Dragon Ball HUD: all grey, 4 circle D-pad keys, two-hand Ki, Goku sprites, BACK/PLAY OK");
}
