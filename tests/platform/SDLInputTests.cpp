// Exercise the real SDL event adapter with synthetic events, without a display.
#include "platform/SDLDesktopApp.cpp"
#include <stdexcept>
using namespace glyph;
int main(){try {
 runtime::RuntimeShell shell(std::filesystem::path(GLYPH_SOURCE_DIR)/"examples/arcade/comet-courier/game.glyph");
 platform::SDLState sdl;bool running=true;
 const auto check=[](bool v,const char* why){if(!v)throw std::runtime_error(why);};
 const auto key=[&](SDL_Keycode code,bool down){SDL_Event e{};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.keysym.sym=code;platform::consumeEvent(sdl,shell,e,running);};
 const auto axis=[&]{return shell.input().axis(shell.host().vm().interner().intern(":move-x"));};
 key(SDLK_a,true);check(axis()==-1,"left key moves left");
 key(SDLK_d,true);check(axis()==0,"opposite held keys cancel");
 key(SDLK_a,false);check(axis()==1,"releasing left preserves held right");
 key(SDLK_d,false);key(SDLK_a,true);key(SDLK_LEFT,true);key(SDLK_a,false);
 check(axis()==-1,"releasing WASD alias preserves held arrow");
 key(SDLK_LEFT,false);key(SDLK_SPACE,true);
 SDL_Event mouse{};mouse.type=SDL_MOUSEBUTTONUP;mouse.button.x=100;mouse.button.y=200;platform::consumeEvent(sdl,shell,mouse,running);
 check(shell.input().held(shell.host().vm().interner().intern(":tap")),"mouse release preserves keyboard confirm");
 key(SDLK_SPACE,false);key(SDLK_ESCAPE,true);
 check(running && shell.paused(),"Escape pauses without quitting");
 key(SDLK_RETURN,true);check(!shell.paused(),"Enter resumes pause menu");
 SDL_Event focus{};focus.type=SDL_WINDOWEVENT;focus.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
 platform::consumeEvent(sdl,shell,focus,running);check(shell.paused(),"losing focus pauses");
 focus.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;platform::consumeEvent(sdl,shell,focus,running);
 key(SDLK_RETURN,true);check(!shell.paused(),"regaining focus permits deliberate resume");
 SDL_Event controller{};controller.type=SDL_CONTROLLERAXISMOTION;controller.caxis.axis=SDL_CONTROLLER_AXIS_LEFTX;controller.caxis.value=2000;
 platform::consumeEvent(sdl,shell,controller,running);check(axis()==0,"controller dead zone prevents drift");
 controller.caxis.value=32767;platform::consumeEvent(sdl,shell,controller,running);check(axis()==1,"controller stick drives action axis");
 platform::setLifecyclePaused(sdl,shell,true);platform::setLifecyclePaused(sdl,shell,false);
 check(shell.paused(),"return from background waits for deliberate resume");
 key(SDLK_q,true);check(!running,"Quit is available in pause menu");
 // Use a real logical renderer to exercise normalized, simultaneous touch contacts.
 SDL_SetMainReady();SDL_setenv("SDL_VIDEODRIVER","dummy",1);
 check(SDL_Init(SDL_INIT_VIDEO)==0,"dummy video initializes");
 sdl.window=SDL_CreateWindow("input test",0,0,480,360,SDL_WINDOW_HIDDEN);
 sdl.renderer=SDL_CreateRenderer(sdl.window,-1,SDL_RENDERER_SOFTWARE);
 check(sdl.renderer!=nullptr,"logical renderer initializes");
 SDL_RenderSetLogicalSize(sdl.renderer,480,360);
 runtime::RuntimeShell ski(std::filesystem::path(GLYPH_SOURCE_DIR)/"examples/arcade/ski-slalom/game.glyph");
 platform::clearControls(sdl,ski);sdl.userPaused=false;sdl.backgrounded=false;
 const auto touch=[&](Uint32 type,SDL_FingerID finger,float x,float y){SDL_Event e{};e.type=type;e.tfinger.fingerId=finger;e.tfinger.x=x/480;e.tfinger.y=y/360;platform::consumeEvent(sdl,ski,e,running);};
 touch(SDL_FINGERDOWN,1,240,290);ski.tick(game::GameHost::fixedDt);
 auto phaseId=ski.host().vm().interner().intern(":phase");
 check(ski.host().state().map->at(phaseId).id==ski.host().vm().interner().intern(":play"),"touch Start is not intercepted by steering pad");
 touch(SDL_FINGERUP,1,240,290);ski.tick(game::GameHost::fixedDt);
 touch(SDL_FINGERDOWN,2,100,240);touch(SDL_FINGERMOTION,2,150,280);
 touch(SDL_FINGERDOWN,3,400,270);ski.tick(game::GameHost::fixedDt);
 check(ski.input().axis(ski.host().vm().interner().intern(":move-x"))==1,"touch steering survives a second contact");
 check(ski.host().state().map->at(ski.host().vm().interner().intern(":air")).number>0,"second finger jumps while steering");
 touch(SDL_FINGERUP,3,400,270);
 check(ski.input().axis(ski.host().vm().interner().intern(":move-x"))==1,"releasing jump preserves steering");
 platform::setLifecyclePaused(sdl,ski,true);
 check(ski.input().axis(ski.host().vm().interner().intern(":move-x"))==0 && !sdl.steeringFinger,"background cancels touch contacts");
 SDL_DestroyRenderer(sdl.renderer);SDL_DestroyWindow(sdl.window);SDL_Quit();
 std::cout<<"SDL input adapter tests passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
