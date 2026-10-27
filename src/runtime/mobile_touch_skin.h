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
    const float r=c.r*(pressed?0.95f:1.0f);
    const float blue=0.5f+0.5f*std::sin(phase);
    if(special && ready) {
        o.fill_circle(c.x,c.y,r*1.21f,
                      {17,100,255,static_cast<uint8_t>(14+16*blue)},D);
        o.stroke_circle(c.x,c.y,r*1.19f,r*0.060f,
                        {16,110,255,static_cast<uint8_t>(95+55*blue)},D);
        o.stroke_circle(c.x,c.y,r*1.125f,r*0.071f,
                        {80,200,255,static_cast<uint8_t>(130+75*blue)},D);
    } else {
        o.stroke_circle(c.x,c.y,r*1.095f,r*0.045f,
                        special ? Color{27,122,230,109}:Color{233,241,255,70},D);
    }
    o.fill_circle(c.x,c.y,r*1.035f,{13,26,48,231},D);
    o.stroke_circle(c.x,c.y,r*1.028f,r*0.035f,{185,215,244,173},D);
    o.fill_circle(c.x,c.y,r*0.963f,
                  pressed ? Color{47,100,143,230}:bottom,D);
    // Simulated glossy enamel and curved internal rim: three low-alpha layers.
    o.fill_circle(c.x,c.y-r*0.045f,r*0.845f,top,D);
    o.fill_circle(c.x,c.y+r*0.135f,r*0.72f,bottom,D);
    o.arc(c.x,c.y,r*0.90f,r*0.035f,0.82f,1.15f,
          {227,246,255,158},D);
    o.arc(c.x,c.y,r*0.87f,r*0.025f,0.20f,0.49f,
          {106,163,213,108},D);
    if(special && ready)
        o.stroke_circle(c.x,c.y,r*0.983f,r*0.039f,
                        {159,234,255,static_cast<uint8_t>(185+58*blue)},D);
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
inline void dpad(HostOverlay& o, Circle d, uint16_t held) {
    const float x=d.x,y=d.y,r=d.r;
    const float a=r*.41f;
    // Wide glass ring, less opaque than the previous dark cross.
    o.stroke_circle(x,y,r*1.32f,r*.028f,{120,189,250,77},D);
    o.stroke_circle(x,y,r*1.23f,r*.026f,{177,212,255,66},D);
    o.stroke_circle(x,y,r*1.13f,r*.028f,{171,205,247,67},D);
    const struct Item {int bit;float cx,cy;} cells[]={
        {6,x,y-r*.68f},{7,x,y+r*.68f},
        {5,x-r*.68f,y},{4,x+r*.68f,y}
    };
    for(const auto& q:cells) {
        const bool down=(held&(1u<<q.bit))!=0;
        const Color frame=down?Color{94,210,255,230}:Color{196,220,247,190};
        o.fill_rect(q.cx-a,q.cy-a,2*a,2*a,a*.34f,
                    down?Color{31,90,155,178}:Color{18,35,59,144},D);
        o.stroke_rect(q.cx-a,q.cy-a,2*a,2*a,a*.34f,
                      r*.036f,frame,D);
        o.fill_rect(q.cx-a*.83f,q.cy-a*.83f,1.66f*a,1.66f*a,
                    a*.30f,down?Color{83,164,211,107}:Color{192,216,239,45},D);
        const float v=r*.14f;
        const Color arrow{242,249,255,244};
        if(q.bit==6 || q.bit==7) {
            const float dir=q.bit==6?-1.f:1.f;
            line(o,q.cx-v,q.cy-dir*v*.3f,q.cx,q.cy+dir*v*.65f,
                 r*.066f,arrow);
            line(o,q.cx,q.cy+dir*v*.65f,q.cx+v,q.cy-dir*v*.3f,
                 r*.066f,arrow);
        } else {
            const float dir=q.bit==5?-1.f:1.f;
            line(o,q.cx-dir*v*.3f,q.cy-v,q.cx+dir*v*.65f,q.cy,
                 r*.066f,arrow);
            line(o,q.cx+dir*v*.65f,q.cy,q.cx-dir*v*.3f,q.cy+v,
                 r*.066f,arrow);
        }
    }
    o.fill_circle(x,y,r*.215f,{67,84,110,155},D);
    o.stroke_circle(x,y,r*.205f,r*.035f,{183,207,234,102},D);
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
