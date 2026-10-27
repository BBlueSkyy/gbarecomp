// Mobile action skin for Dragon Ball: Advanced Adventure.
// Decorative host overlay only; the native KEYINPUT touch ownership lives in
// host_window.cpp.  No sprites are decoded/uploaded on each frame.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "host_overlay.h"

namespace gbarecomp::mobile_skin {

struct Circle { float x, y, r; };
struct Layout {
    Circle dpad, jump, fist, ki, pole;
    float mm = 1.0f;
};
using Color = OverlayColor;
constexpr auto D = OverlaySpace::Drawable;

inline void line(HostOverlay& o, float x1, float y1, float x2, float y2,
                 float thickness, Color c) {
    o.line(x1, y1, x2, y2, thickness, c, D);
}
inline void round(HostOverlay& o, Circle b, float dx, float dy,
                  float w, float h, float rad, Color c) {
    o.fill_rect(b.x + dx*b.r, b.y + dy*b.r, w*b.r, h*b.r, rad*b.r, c, D);
}
inline void orb(HostOverlay& o, Circle c, float x, float y, float r, Color color) {
    o.fill_circle(c.x+x*c.r,c.y+y*c.r,r*c.r,color,D);
}
inline void seg(HostOverlay& o, Circle c, float x, float y, float xx, float yy,
                float t, Color color) {
    line(o,c.x+x*c.r,c.y+y*c.r,c.x+xx*c.r,c.y+yy*c.r,t*c.r,color);
}

// The two blues are intentionally independent from the RGB monitor/lens.
// Active special: blue bloom + gently breathing outline; not-ready special:
// colored decorative rim, visibly weaker, with no flashing animation.
inline void bezel(HostOverlay& o, Circle c, Color top, Color bottom, bool pressed,
                  bool special, bool ready, float phase) {
    // Three glass/enamel layers + thin chrome rims. Cheap host-side strokes,
    // never a fullscreen blur/postprocess shader.
    const float r=c.r*(pressed?0.955f:1.0f);
    const float beat = 0.5f + 0.5f * std::sin(phase);
    const auto A = [](float f) -> uint8_t {
        return static_cast<uint8_t>(std::clamp(static_cast<int>(f),0,255));
    };
    if (special && ready) {
        // Four concentric translucent rims produce a luminous blue halo,
        // rather than the single barely visible line in the original HUD.
        o.stroke_circle(c.x,c.y,r*1.245f,r*.040f,
                        {13,106,255,A(42+32*beat)},D);
        o.stroke_circle(c.x,c.y,r*1.186f,r*.052f,
                        {17,135,255,A(102+60*beat)},D);
        o.stroke_circle(c.x,c.y,r*1.130f,r*.048f,
                        {65,194,255,A(143+75*beat)},D);
        o.stroke_circle(c.x,c.y,r*1.080f,r*.035f,
                        {175,248,255,A(175+75*beat)},D);
    } else {
        o.stroke_circle(c.x,c.y,r*1.097f,r*.042f,
                        special ? Color{55,135,222,80} :
                                  Color{215,229,250,68},D);
    }
    // Dark metal frame, blue silver rim and glossy translucent body.
    o.fill_circle(c.x,c.y,r*1.030f,{5,18,32,233},D);
    o.stroke_circle(c.x,c.y,r*1.019f,r*.031f,
                    {182,213,245,192},D);
    o.fill_circle(c.x,c.y,r*.976f,
                  pressed ? Color{51,103,143,232} : bottom,D);
    o.fill_circle(c.x,c.y-r*.065f,r*.850f,
                  {top.r,top.g,top.b,A(top.a*.85f)},D);
    o.fill_circle(c.x,c.y+r*.115f,r*.742f,
                  {bottom.r,bottom.g,bottom.b,A(bottom.a*.70f)},D);
    // Reflected highlight at the top-left and low-key shadow opposite.
    o.arc(c.x,c.y,r*.911f,r*.043f,.80f,1.10f,
          {250,254,255,209},D);
    o.arc(c.x,c.y,r*.864f,r*.024f,.16f,.50f,
          {135,196,244,115},D);
    o.arc(c.x,c.y,r*.955f,r*.022f,.35f,.62f,
          {5,13,27,171},D);
    if (special && ready)
        o.stroke_circle(c.x,c.y,r*.980f,r*.030f,
                        {182,251,255,A(200+55*beat)},D);
}

inline void ki_icon(HostOverlay& o, Circle b) {
    // White-hot core and cyan flares travelling from right to left.
    orb(o,b,-0.16f,0.00f,0.39f,{26,115,255,112});
    orb(o,b,-0.20f,0.01f,0.32f,{63,200,255,233});
    orb(o,b,-0.24f,0.00f,0.23f,{245,254,255,255});
    const Color glow{57,185,255,190}, light{172,247,255,246};
    for(int k=-3;k<=3;k++) {
        const float v=k*0.105f;
        seg(o,b,0.03f,v*0.8f,0.60f-0.065f*std::abs(k),v*1.55f,
            0.053f,glow);
    }
    seg(o,b,-.33f,-.19f,-.02f,-.39f,.080f,light);
    seg(o,b,-.29f,+.22f,+.03f,+.39f,.075f,light);
    seg(o,b,+.08f,-.03f,+.48f,-.08f,.055f,light);
    // Subtle left-side crescent, visually distinctive from a plain dot.
    o.arc(b.x-.20f*b.r,b.y,b.r*.39f,b.r*.055f,.56f,.94f,light,D);
}

inline void fist_icon(HostOverlay& o, Circle b) {
    const Color shadow{84,30,18,236}, skin{255,195,132,254},
                highlight{255,232,175,251};
    // Wrist and folded thumb, outlined knuckles, lighter top facets.
    round(o,b,-.16f,+.12f,.43f,.38f,.13f,shadow);
    round(o,b,-.12f,+.12f,.34f,.31f,.11f,skin);
    round(o,b,-.43f,-.17f,.85f,.46f,.16f,shadow);
    round(o,b,-.38f,-.20f,.75f,.43f,.15f,skin);
    for(int n=0;n<4;n++) {
        const float x=-.32f+n*.205f;
        round(o,b,x,-.30f,.185f,.23f,.10f,shadow);
        round(o,b,x+.012f,-.285f,.155f,.20f,.09f,highlight);
    }
    round(o,b,-.30f,+.04f,.29f,.22f,.08f,{227,135,84,252});
    seg(o,b,-.10f,.08f,.27f,.08f,.025f,{151,78,48,185});
}

inline void staff_icon(HostOverlay& o, Circle b) {
    // Black edged, warm golden Power Pole on a diagonal.
    const Color outline{41,12,4,253}, brass{254,172,50,255};
    seg(o,b,-.51f,.47f,.50f,-.49f,.195f,outline);
    seg(o,b,-.48f,.42f,.47f,-.46f,.143f,{143,44,10,250});
    seg(o,b,-.47f,.41f,.47f,-.46f,.094f,brass);
    seg(o,b,-.45f,.35f,.45f,-.49f,.041f,{255,236,150,255});
    orb(o,b,-.49f,.46f,.115f,{103,35,14,235});
    orb(o,b,+.51f,-.49f,.105f,{246,155,43,251});
    // Pole cross-bands to read as a staff rather than a slanted line.
    for(int i=0;i<3;i++) {
        const float t=-.30f+i*.30f;
        seg(o,b,t-.055f,-t+.08f,t+.056f,-t-.055f,
            .032f,{116,32,9,220});
    }
}
inline void jump_icon(HostOverlay& o, Circle b) {
    const Color fig{233,255,243,252}, shade{120,224,173,208};
    // Stylised martial artist with spiky hair and dynamically bent legs.
    orb(o,b,-.22f,-.29f,.157f,fig);
    const float spikes[][4]={
        {-.34f,-.40f,-.43f,-.65f},{-.20f,-.42f,-.22f,-.67f},
        {-.12f,-.40f,+.01f,-.60f},{-.36f,-.30f,-.53f,-.32f}};
    for(const auto& a:spikes) seg(o,b,a[0],a[1],a[2],a[3],.085f,fig);
    seg(o,b,-.17f,-.17f,+.04f,+.14f,.23f,shade);
    seg(o,b,-.16f,-.18f,+.08f,+.14f,.165f,fig);
    // Raised fist; leg in the air and a trailing leg below.
    seg(o,b,-.12f,-.09f,+.23f,-.39f,.11f,fig);
    orb(o,b,+.26f,-.42f,.094f,fig);
    seg(o,b,+.06f,+.10f,+.34f,+.28f,.13f,fig);
    seg(o,b,+.34f,+.28f,+.52f,+.11f,.115f,fig);
    seg(o,b,+.02f,+.09f,-.22f,+.47f,.14f,fig);
    seg(o,b,-.22f,+.47f,-.45f,+.55f,.11f,fig);
    // Thick upward arrow at the right, like the approved image.
    seg(o,b,+.54f,+.15f,+.54f,-.43f,.105f,fig);
    seg(o,b,+.54f,-.43f,+.37f,-.24f,.090f,fig);
    seg(o,b,+.54f,-.43f,+.71f,-.24f,.090f,fig);
}
// Small filled triangle made from thin rectangles; no ImGui-only primitive,
// no per-frame heap allocations, and works in both SDL and ImGui backends.
inline void arrow(HostOverlay& o,float x,float y,float r,int dir) {
    const Color white{249,253,255,245};
    for(int i=0;i<8;i++) {
        const float u=(i+.5f)/8.f;
        const float width=r*(.020f+.132f*u);
        const float longaxis=r*(-.205f+.44f*u);
        const float thickness=r*.063f;
        if(dir==0) // up
            o.fill_rect(x-width,y+longaxis,2*width,thickness,0,white,D);
        else if(dir==1) // down
            o.fill_rect(x-width,y-longaxis,2*width,thickness,0,white,D);
        else if(dir==2) // left
            o.fill_rect(x+longaxis,y-width,thickness,2*width,0,white,D);
        else
            o.fill_rect(x-longaxis,y-width,thickness,2*width,0,white,D);
    }
}
inline void dpad(HostOverlay& o, Circle d, uint16_t held) {
    const float x=d.x,y=d.y,r=d.r;
    const float a=r*.405f;
    // Airy translucent blue ring like the approved touch overlay.
    o.fill_circle(x,y,r*1.28f,{38,111,175,16},D);
    o.stroke_circle(x,y,r*1.337f,r*.034f,{54,152,248,105},D);
    o.stroke_circle(x,y,r*1.294f,r*.020f,{184,232,255,115},D);
    o.stroke_circle(x,y,r*1.225f,r*.021f,{111,197,252,78},D);
    o.arc(x,y,r*1.325f,r*.029f,.79f,1.13f,
          {218,248,255,168},D);

    const struct Item {int bit;int direction;float cx,cy;} cells[] = {
        {6,0,x,y-r*.67f},{7,1,x,y+r*.67f},
        {5,2,x-r*.67f,y},{4,3,x+r*.67f,y}
    };
    for(const auto& q:cells) {
        const bool down=(held&(1u<<q.bit))!=0;
        const Color rim=down?Color{129,236,255,245}:Color{211,240,255,224};
        // More transparent than the previous opaque navy squares: the GBA
        // scenery remains visible behind all four directional controls.
        o.fill_rect(q.cx-a,q.cy-a,2*a,2*a,a*.38f,
                    down?Color{46,158,223,133}:
                         Color{118,170,205,68},D);
        o.stroke_rect(q.cx-a,q.cy-a,2*a,2*a,a*.38f,
                      r*.050f,rim,D);
        o.stroke_rect(q.cx-a*.915f,q.cy-a*.915f,1.83f*a,1.83f*a,
                      a*.35f,r*.021f,{144,210,249,122},D);
        o.fill_rect(q.cx-a*.84f,q.cy-a*.82f,1.68f*a,.34f*a,
                    a*.22f,{240,252,255,static_cast<uint8_t>(down?98:63)},D);
        arrow(o,q.cx,q.cy,r,q.direction);
    }
    o.fill_circle(x,y,r*.220f,{50,92,132,138},D);
    o.stroke_circle(x,y,r*.217f,r*.045f,{209,241,255,165},D);
    o.fill_circle(x-r*.045f,y-r*.045f,r*.105f,{232,248,255,42},D);
}

inline void draw(HostOverlay& o,const Layout& l,uint16_t held,
                 bool ready,std::uint32_t host_ms) {
    dpad(o,l.dpad,held);
    const float phase=static_cast<float>(host_ms)*.0065f;
    bezel(o,l.ki,{7,57,135,170},{7,24,72,228},
          (held&(1u<<8))!=0,true,ready,phase);
    ki_icon(o,l.ki);
    bezel(o,l.fist,{114,27,31,139},{60,6,13,227},
          (held&(1u<<1))!=0,false,false,phase);
    fist_icon(o,l.fist);
    bezel(o,l.pole,{75,44,23,154},{32,20,13,230},
          (held&(1u<<9))!=0,true,ready,phase);
    staff_icon(o,l.pole);
    bezel(o,l.jump,{14,93,65,149},{6,47,37,224},
          (held&(1u<<0))!=0,false,false,phase);
    jump_icon(o,l.jump);
}
} // namespace gbarecomp::mobile_skin
