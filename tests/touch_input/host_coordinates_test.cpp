// Real SDL event watch + HostWindow + TouchHub. No guest ROM or BIOS required.
#include "host_window.h"
#include "touch_input.h"
#include <SDL.h>
#include <array>
#include <cmath>
#include <cstdio>
int main(){
 SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);SDL_setenv("SDL_RENDER_DRIVER","software",1);
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
 std::printf("host touch coordinates: %u real SDL edges map once, landscape/portrait PASS\n",checks);win.close();
}
