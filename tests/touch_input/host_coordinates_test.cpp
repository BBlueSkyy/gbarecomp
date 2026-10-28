// Real SDL event watch + HostWindow + TouchHub. No guest ROM or BIOS required.
#include "host_window.h"
#include "touch_input.h"
#include <SDL.h>
#include <array>
#include <cmath>
#include <cstdio>
int main(){
 SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);SDL_setenv("SDL_RENDER_DRIVER","software",1);
 SDL_setenv("GBARECOMP_TOUCH_CONTROLS","1",1);SDL_setenv("GBARECOMP_TOUCH_DPI","406.4",1);
 gbarecomp::HostWindow win;if(!win.open(4,240,160,"touch coordinates",nullptr,false,false))return 1;
 auto& hub=gbarecomp::TouchHub::instance();hub.reset_for_tests();win.configure_touch(true,gbarecomp::kTouchClaimTap|gbarecomp::kTouchClaimLongPress,0,false);
 std::array<uint8_t,240*160*3> frame{};auto* window=SDL_GetWindowFromID(1);if(!window)return 2;unsigned checks=0;
 for(auto size:{std::pair{2520,1080},std::pair{1920,1080},std::pair{1080,2400}}){
  SDL_SetWindowSize(window,size.first,size.second);win.present(frame.data());const auto p=hub.presentation();
  for(auto point:{std::pair{100.0f,60.0f},std::pair{165.0f,110.0f},std::pair{120.0f,68.0f}}){
   float dx,dy;gbarecomp::logical_point_to_presentation(p.layout,240,160,point.first,point.second,&dx,&dy);
   for(auto type:{SDL_FINGERDOWN,SDL_FINGERUP}){SDL_Event e{};e.type=type;e.tfinger.type=type;e.tfinger.touchId=17;e.tfinger.fingerId=19;e.tfinger.x=dx/p.drawable_width;e.tfinger.y=dy/p.drawable_height;if(SDL_PushEvent(&e)!=1)return 3;}
   win.pump();const auto batch=hub.drain(0,gbarecomp::touch_clock_ms());if(batch.events.size()!=2)return 4;
   for(auto e:batch.events){++checks;if(!e.in_view||std::fabs(e.view_x-point.first)>0.05f||std::fabs(e.view_y-point.second)>0.05f){std::fprintf(stderr,"touch mismatch %dx%d expected %.2f,%.2f actual %.2f,%.2f\n",size.first,size.second,point.first,point.second,e.view_x,e.view_y);return 5;}}
  }
 }
 // Mobile navigation Down+Up between guest frames must survive via TouchHub;
 // neither the removed SELECT nor level-only START may swallow these taps.
 win.configure_touch(true,gbarecomp::kTouchClaimTap|gbarecomp::kTouchClaimLongPress,1,false);
 win.configure_mobile_action_hud(true,nullptr);SDL_SetWindowSize(window,2520,1080);win.present(frame.data());
 const auto p=hub.presentation();const auto mm=win.px_per_mm();
 for(int side:{-1,1}){
  for(auto type:{SDL_FINGERDOWN,SDL_FINGERUP}){SDL_Event e{};e.type=type;e.tfinger.type=type;e.tfinger.touchId=17;e.tfinger.fingerId=19;e.tfinger.x=(p.drawable_width*.5f+side*9*mm)/p.drawable_width;e.tfinger.y=(p.drawable_height-7*mm)/p.drawable_height;SDL_PushEvent(&e);}
  const auto out=win.pump();const auto batch=hub.drain(0,gbarecomp::touch_clock_ms());
  if(batch.events.size()==2){
   for(auto e:batch.events)if(e.control!=(side<0?gbarecomp::TouchControl::Back:gbarecomp::TouchControl::Start))return 7;
  }
  hub.advance(gbarecomp::touch_clock_ms()+1000);if(!hub.take_host_actions().empty())return 8;
  if(batch.events.size()!=2 || out.keyinput!=0x3ff){std::fprintf(stderr,"short mobile navigation tap swallowed (%d): %zu events, key=%x\n",side,batch.events.size(),out.keyinput);return 6;}
 }
 // Removing SELECT must not leave a padded zero-sized rectangle at (0,0).
 {SDL_Event e{};e.type=e.tfinger.type=SDL_FINGERDOWN;e.tfinger.touchId=17;e.tfinger.fingerId=77;e.tfinger.x=0;e.tfinger.y=0;SDL_PushEvent(&e);if(win.pump().keyinput!=0x3ff)return 9;e.type=e.tfinger.type=SDL_FINGERUP;SDL_PushEvent(&e);win.pump();hub.drain(0,gbarecomp::touch_clock_ms());}
 // Compact portrait with a high physical density: a short lower band must
 // not collapse jump, punch and Ki into the same centre (1080x1200,16px/mm).
 SDL_SetWindowSize(window,1080,1200);win.present(frame.data());
 {SDL_Event e{};e.type=e.tfinger.type=SDL_FINGERDOWN;e.tfinger.touchId=17;e.tfinger.fingerId=76;e.tfinger.x=912.f/1080;e.tfinger.y=1032.f/1200;SDL_PushEvent(&e);const auto k=win.pump().keyinput;if(k!=0x3fe){std::fprintf(stderr,"portrait jump also asserts other actions: %x\n",k);return 11;}e.type=e.tfinger.type=SDL_FINGERUP;SDL_PushEvent(&e);win.pump();}
 SDL_SetWindowSize(window,2520,1080);
 // Other games keep physical virtual SELECT exactly as before.
 win.configure_mobile_action_hud(false,nullptr);win.present(frame.data());
 {SDL_Event e{};e.type=e.tfinger.type=SDL_FINGERDOWN;e.tfinger.touchId=17;e.tfinger.fingerId=78;e.tfinger.x=(1260-8.5f*mm)/2520;e.tfinger.y=(1080-6.25f*mm)/1080;SDL_PushEvent(&e);if(win.pump().keyinput!=0x3fb)return 10;e.type=e.tfinger.type=SDL_FINGERUP;SDL_PushEvent(&e);win.pump();}
 std::printf("host touch coordinates: %u real SDL edges map once, landscape/portrait PASS\n",checks);win.close();
}
