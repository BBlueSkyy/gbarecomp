// Mobile action skin for Dragon Ball: Advanced Adventure.
// Decorative host overlay only; the native KEYINPUT touch ownership lives in
// host_window.cpp.  No sprites are decoded/uploaded on each frame.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "host_overlay.h"
#include "mobile_reference_icons.h"

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
        // Charged glow is dynamic. Non-special punch/jump buttons keep
        // a faint red/green enamel rim as in the approved mobile mockup.
        const bool fist = !special && top.r > top.g;
        if (special) {
            o.stroke_circle(c.x,c.y,r*1.097f,r*.042f,
                            {55,135,222,78},D);
        } else {
            // Punch and jump have their own always-on *static* illuminated
            // enamel rings in the user's references. They are separate
            // from the blue, charge-dependent Power Pole / Ki pulses.
            const Color aura = fist ? Color{255,70,113,65} :
                                     Color{111,238,222,60};
            const Color rim = fist ? Color{255,187,207,157} :
                                    Color{218,255,247,155};
            o.stroke_circle(c.x,c.y,r*1.156f,r*.031f,aura,D);
            o.stroke_circle(c.x,c.y,r*1.096f,r*.056f,rim,D);
            o.stroke_circle(c.x,c.y,r*1.058f,r*.017f,
                    fist ? Color{255,116,141,181} :
                           Color{159,255,228,163},D);
        }
    }
    // Dark metal frame, blue silver rim and glossy translucent body.
    o.fill_circle(c.x,c.y,r*1.030f,{22,28,34,76},D);
    o.stroke_circle(c.x,c.y,r*1.019f,r*.031f,
                    {207,220,233,133},D);
    o.fill_circle(c.x,c.y,r*.976f,
                  pressed ? Color{93,117,141,148} :
                            Color{bottom.r,bottom.g,bottom.b,A(bottom.a*.40f)},D);
    o.fill_circle(c.x,c.y-r*.065f,r*.850f,
                  {top.r,top.g,top.b,A(top.a*.52f)},D);
    o.fill_circle(c.x,c.y+r*.115f,r*.742f,
                  {bottom.r,bottom.g,bottom.b,A(bottom.a*.36f)},D);
    // Reflected highlight at the top-left and low-key shadow opposite.
    o.arc(c.x,c.y,r*.911f,r*.043f,.80f,1.10f,
          {250,254,255,143},D);
    o.arc(c.x,c.y,r*.864f,r*.024f,.16f,.50f,
          {173,198,214,85},D);
    o.arc(c.x,c.y,r*.955f,r*.022f,.35f,.62f,
          {5,13,27,65},D);
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

// Icon traced from the user-supplied reference, rather than a few rectangles.
inline void fist_icon(HostOverlay& o, Circle b) {
    mobile_reference::draw_fist(o,b.x,b.y,b.r);
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
// Real jumping silhouette and arrow contours from the selected reference.
inline void jump_icon(HostOverlay& o, Circle b) {
    mobile_reference::draw_jump(o,b.x-b.r*.070f,b.y,b.r*.97f);
    // The selected green reference has a substantial filled upward arrow,
    // separate from the character silhouette. The source crop did not
    // preserve its entire outer tip, so reconstruct it as solid spans.
    constexpr OverlayColor c{226,255,247,252};
    const float r=b.r, cx=b.x+r*.57f, tip=b.y-r*.51f;
    for (int i=0;i<10;i++) {
        const float t=(i+.5f)/10.f;
        const float hw=r*(.018f+.245f*t);
        o.fill_rect(cx-hw,tip+r*.37f*t,2.f*hw,r*.043f,
                    0.f,c,D);
    }
    o.fill_rect(cx-r*.093f,tip+r*.345f,r*.186f,r*.39f,
                0.f,c,D);
}

// User-approved discrete D-pad: four independently outlined, rounded
// translucent-grey keys with five-sided, unfilled directional glyphs.
// No analog stick, center circle, enclosing ring, blur, or blue neon.
inline void rounded_key_outline(HostOverlay& o, float x, float y,
                               float half, float corner, float width, Color c) {
    const float x0=x-half, x1=x+half, y0=y-half, y1=y+half;
    line(o,x0+corner,y0,x1-corner,y0,width,c);
    line(o,x1,y0+corner,x1,y1-corner,width,c);
    line(o,x1-corner,y1,x0+corner,y1,width,c);
    line(o,x0,y1-corner,x0,y0+corner,width,c);
    constexpr float pi=3.14159265358979323846f;
    const struct Corner { float x,y,start; } corners[]={
        {x1-corner,y0+corner,-pi*.5f},
        {x1-corner,y1-corner,0.f},
        {x0+corner,y1-corner,pi*.5f},
        {x0+corner,y0+corner,pi}
    };
    // Two short segments per corner: visually rounded even on the SDL
    // renderer fallback where stroke_rect ignores its rounding argument.
    for(const auto& p:corners) {
        float px=p.x+corner*std::cos(p.start);
        float py=p.y+corner*std::sin(p.start);
        for(int k=1;k<=2;++k) {
            const float t=p.start+(pi*.5f)*(float(k)/2.f);
            const float nx=p.x+corner*std::cos(t);
            const float ny=p.y+corner*std::sin(t);
            line(o,px,py,nx,ny,width,c);
            px=nx; py=ny;
        }
    }
}
inline void outlined_dpad_arrow(HostOverlay& o,float x,float y,
                                float half,int direction,Color color,float width) {
    // The source icon is a hollow 'house-shaped' arrow with straight
    // shoulders and a squared base, not a filled triangle/chevron.
    constexpr float vx[5]={0.f,-.65f,-.65f,.65f,.65f};
    constexpr float vy[5]={-.78f,-.08f,.65f,.65f,-.08f};
    float xx[5],yy[5];
    for(int i=0;i<5;i++) {
        float dx=vx[i],dy=vy[i];
        if(direction==1) {dx=-vx[i];dy=-vy[i];}          // down
        else if(direction==2) {dx=vy[i];dy=-vx[i];}     // left
        else if(direction==3) {dx=-vy[i];dy=vx[i];}     // right
        xx[i]=x+dx*half*.78f;
        yy[i]=y+dy*half*.78f;
    }
    for(int i=0;i<5;i++) {
        const int j=(i+1)%5;
        line(o,xx[i],yy[i],xx[j],yy[j],width,color);
    }
}
inline void dpad(HostOverlay& o, Circle d, uint16_t held) {
    const float x=d.x,y=d.y,r=d.r;
    const float half=r*.34f, offset=r*.72f;
    const float edge=std::max(1.4f,r*.026f);
    const float glyph=std::max(1.5f,r*.034f);
    const struct Item {int bit,dir;float cx,cy;} cells[]={
        {6,0,x,y-offset}, {7,1,x,y+offset},
        {5,2,x-offset,y}, {4,3,x+offset,y}
    };
    for(const auto& cell:cells) {
        const bool down=(held&(1u<<cell.bit))!=0;
        // Deliberately neutral grey, partially transparent over the game.
        // The fill is nearly invisible at idle; only its outlines define
        // the control. Highlight just the touched direction.
        o.fill_rect(cell.cx-half,cell.cy-half,2.f*half,2.f*half,
                    r*.090f,down?Color{104,111,121,95}:
                                  Color{70,75,82,32},D);
        rounded_key_outline(o,cell.cx,cell.cy,half,r*.090f,edge,
                            down?Color{230,233,238,198}:
                                 Color{174,180,187,118});
        outlined_dpad_arrow(o,cell.cx,cell.cy,half,cell.dir,
                            down?Color{249,250,252,228}:
                                 Color{200,205,212,147},glyph);
    }
}

// Small neutral-grey navigation controls, matching the four D-pad keys.
// Vector glyphs work without a host font (including SDL's fallback renderer).
inline void navigation(HostOverlay& o,float x,float y,float w,float h,
                       bool play,bool down) {
    const float r=h*.5f, cy=y+r, left=x+r, right=x+w-r;
    const Color fill=down?Color{104,111,121,95}:Color{70,75,82,32};
    const Color edge=down?Color{230,233,238,198}:Color{174,180,187,118};
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
