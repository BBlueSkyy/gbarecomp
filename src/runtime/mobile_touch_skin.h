// Mobile action skin for Dragon Ball: Advanced Adventure.
// Decorative host overlay only; the native KEYINPUT touch ownership lives in
// host_window.cpp.  No sprites are decoded/uploaded on each frame.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "host_overlay.h"
#include "mobile_reference_icons.h"
#include "mobile_game_sprite_masks.h"

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

// Compact, monochrome smoked glass, matching the owner's final reference.
// Ready feedback remains charge-dependent but silver instead of neon blue.
// All primitives are drawn by the existing host overlay (no guest work).
inline void bezel(HostOverlay& o, Circle c, Color, Color, bool pressed,
                  bool special, bool ready, float phase) {
    const float r=c.r*(pressed?.975f:1.f);
    o.fill_circle(c.x,c.y,r,
        pressed?Color{111,115,120,118}:Color{92,96,102,84},D);
    o.stroke_circle(c.x,c.y,r*.985f,std::max(1.2f,r*.035f),
        pressed?Color{255,255,255,228}:Color{226,229,233,167},D);
    // Tiny white top glint, not a stacked chrome/neon ring.
    o.arc(c.x,c.y,r*.915f,std::max(1.f,r*.017f),
        .82f,.96f,{242,243,245,70},D);
    if(special && ready) {
        const float beat=.5f+.5f*std::sin(phase);
        o.stroke_circle(c.x,c.y,r*1.045f,r*.022f,
            {241,244,246,static_cast<std::uint8_t>(118+42*beat)},D);
    }
}

// Two cupped hands facing a Kamehameha sphere: one above and one
// below, taken from the pose in the supplied reference, not a generic beam.
// The cuffs and fingers are visible at small mobile button sizes.
inline void ki_icon(HostOverlay& o, Circle b) {
    const Color rim{236,237,239,219}, shade{153,156,160,213};
    const float r=b.r;
    // Soft mono energy core, with no persistent cyan/neon halo.
    orb(o,b,+.23f,0.f,.36f,{230,232,236,80});
    orb(o,b,+.25f,0.f,.25f,{247,248,249,232});
    auto hand=[&](float sign) {
        // Three bent fingers curve inward to the energy ball.
        seg(o,b,-.78f,sign*.48f,-.60f,sign*.53f,.09f,shade);
        seg(o,b,-.60f,sign*.53f,-.36f,sign*.46f,.10f,rim);
        seg(o,b,-.36f,sign*.46f,-.21f,sign*.31f,.075f,rim);
        seg(o,b,-.21f,sign*.31f,-.34f,sign*.21f,.063f,rim);
        seg(o,b,-.53f,sign*.36f,-.32f,sign*.34f,.071f,rim);
        seg(o,b,-.32f,sign*.34f,-.15f,sign*.18f,.052f,rim);
        seg(o,b,-.57f,sign*.21f,-.38f,sign*.15f,.074f,rim);
        seg(o,b,-.38f,sign*.15f,-.22f,sign*.09f,.054f,rim);
        // Wrist/sleeve: gray like every other button.
        seg(o,b,-.82f,sign*.48f,-.65f,sign*.32f,.115f,shade);
        seg(o,b,-.71f,sign*.52f,-.58f,sign*.33f,.045f,rim);
    };
    hand(-1.f);
    hand(1.f);
}

// Icon traced from the user-supplied reference, rather than a few rectangles.
inline void fist_icon(HostOverlay& o, Circle b) {
    mobile_reference::draw_fist(o,b.x,b.y,b.r);
}

inline void staff_icon(HostOverlay& o, Circle b) {
    // Goku's actual overhead staff pose, traced from the supplied GBA frame.
    mobile_sprite::draw(o,mobile_sprite::pole,b.x,b.y,b.r,
                        {232,232,232,246},{160,160,160,237});
    mobile_sprite::draw(o,mobile_sprite::pole_dark,b.x,b.y,b.r,
                        {56,56,56,249},{49,49,49,246});
}
inline void jump_icon(HostOverlay& o, Circle b) {
    // Airborne Goku from the user-supplied frame, grayscale not a glyph.
    mobile_sprite::draw(o,mobile_sprite::jump,b.x,b.y,b.r,
                        {236,236,236,246},{163,163,163,240});
    mobile_sprite::draw(o,mobile_sprite::jump_dark,b.x,b.y,b.r,
                        {49,49,49,250},{57,57,57,246});
}

// Four independent round keys as in the approved compact reference.
// No central disc, shared background, analog ring or extra SELECT.
inline void dpad(HostOverlay& o, Circle d, uint16_t held) {
    const float x=d.x,y=d.y,r=d.r;
    const float cr=r*.325f, offset=r*.76f;
    const struct Item {int bit,dir;float cx,cy;} cells[]={
        {6,0,x,y-offset}, {7,1,x,y+offset},
        {5,2,x-offset,y}, {4,3,x+offset,y}
    };
    for(const auto& cell:cells) {
        const bool down=(held&(1u<<cell.bit))!=0;
        const Color fill=down?Color{111,115,120,119}:Color{92,96,102,84};
        const Color edge=down?Color{255,255,255,228}:Color{226,229,233,167};
        const Color ink=down?Color{255,255,255,255}:Color{244,245,247,224};
        o.fill_circle(cell.cx,cell.cy,cr,fill,D);
        o.stroke_circle(cell.cx,cell.cy,cr,std::max(1.1f,r*.021f),edge,D);
        // Filled triangle pointing up/down/left/right, composed of 12 spans
        // without font or texture lookups.
        for(int row=0;row<12;row++) {
            const float t=(row+.5f)/12.f;
            float xa=0.f, ya=0.f, wa=0.f, ha=0.f;
            const float size=cr*.92f;
            if(cell.dir==0 || cell.dir==1) {
                const float spread=(cell.dir==0?t:1.f-t)*size*.8f;
                xa=cell.cx-spread*.5f;
                ya=cell.cy-size*.47f+t*size*.94f;
                wa=spread;ha=size*.94f/12.f+.3f;
            }else {
                const float spread=(cell.dir==2?t:1.f-t)*size*.8f;
                xa=cell.cx-size*.47f+t*size*.94f;
                ya=cell.cy-spread*.5f;
                wa=size*.94f/12.f+.3f;ha=spread;
            }
            if(wa>0.f&&ha>0.f)o.fill_rect(xa,ya,wa,ha,0.f,ink,D);
        }
    }
}

// Small neutral-grey navigation controls, matching the four D-pad keys.
// Vector glyphs work without a host font (including SDL's fallback renderer).
inline void navigation(HostOverlay& o,float x,float y,float w,float h,
                       bool play,bool down) {
    const float r=h*.5f, cy=y+r, left=x+r, right=x+w-r;
    const Color fill=down?Color{111,115,120,119}:Color{92,96,102,84};
    const Color edge=down?Color{255,255,255,228}:Color{226,229,233,167};
    const Color ink=down?Color{249,250,252,228}:Color{235,238,243,210};
    const float t=std::max(1.4f,h*.055f), cx=x+w*.5f;
    // Fixed spans avoid SDL fill_circle's heap-backed tessellation per frame.
    for(int i=0;i<12;++i){const float dy=(float(i)+.5f)/6.f-1.f;
        const float cap=r*std::sqrt(std::max(0.f,1.f-dy*dy));
        o.fill_rect(left-cap,y+h*float(i)/12.f,right-left+2.f*cap,h/12.f+.5f,0,fill,D);
    }
    line(o,left,y,right,y,t,edge);line(o,left,y+h,right,y+h,t,edge);
    o.arc(left,cy,r,t,.5f,1.f,edge,D);o.arc(right,cy,r,t,0.f,.5f,edge,D);
    if(play) {
        // Filled right-pointing triangle, bounded spans; no font allocation.
        for(int i=0;i<12;++i){const float dx=float(i)/12.f;
            const float half=h*.25f*(1.f-dx);
            o.fill_rect(cx-h*.16f+dx*h*.42f,cy-half,h*.035f+1.f,2.f*half,0,ink,D);}
    } else {
        // Hooked return arrow, deliberately distinct from Android system Back.
        line(o,cx+h*.22f,cy-h*.16f,cx+h*.22f,cy+h*.15f,t,ink);
        line(o,cx+h*.22f,cy+h*.15f,cx-h*.22f,cy+h*.15f,t,ink);
        line(o,cx-h*.22f,cy+h*.15f,cx-h*.04f,cy-h*.03f,t,ink);
        line(o,cx-h*.22f,cy+h*.15f,cx-h*.04f,cy+h*.33f,t,ink);
    }
}

inline void draw(HostOverlay& o,const Layout& l,uint16_t held,
                 bool ready,std::uint32_t host_ms) {
    dpad(o,l.dpad,held);
    const float phase=static_cast<float>(host_ms)*.0065f;
    bezel(o,l.ki,{7,57,135,170},{7,24,72,228},
          (held&(1u<<8))!=0,true,ready,phase);
    ki_icon(o,l.ki);
    bezel(o,l.fist,{175,37,45,159},{93,8,22,232},
          (held&(1u<<1))!=0,false,false,phase);
    fist_icon(o,l.fist);
    bezel(o,l.pole,{75,44,23,154},{32,20,13,230},
          (held&(1u<<9))!=0,true,ready,phase);
    staff_icon(o,l.pole);
    bezel(o,l.jump,{30,117,83,160},{7,62,48,228},
          (held&(1u<<0))!=0,false,false,phase);
    jump_icon(o,l.jump);
}
} // namespace gbarecomp::mobile_skin
