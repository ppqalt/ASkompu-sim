#include "GuiSmokeCheck.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace simulator::desktop {
namespace {
void require(bool ok) {if(!ok)throw std::runtime_error("Graafisen käyttöliittymän tarkistus epäonnistui");}
}
GuiSmokeCheck::GuiSmokeCheck(std::string directory):outputDirectory_(std::move(directory)) {
  const auto none=[](AppController&){};
  const auto screen=[](core::Screen s){return [s](AppController& c){require(c.engine().displayModel().screen==s);};};
  actions_={
    {"Nuoli oikealle",none,ImGuiKey_RightArrow},{"OIKEA",screen(core::Screen::BasicView)},
    {"Nopeuden syöttö",none},
    {"Valitse nopeuskenttä",none,ImGuiKey_A,true},
    {"Kirjoita nopeus",none,ImGuiKey_None,false,"72.5"},
    {"Nuoli syöttökentässä",screen(core::Screen::BasicView),ImGuiKey_DownArrow},
    {"Hyväksy nopeus",[](AppController& c){require(c.engine().targetSpeedKmh()==72.5);},ImGuiKey_Enter},
    {"36",[](AppController& c){require(c.engine().targetSpeedKmh()==36);}},
    {"Askel",[](AppController& c){require(c.engine().generatedPulseCount()==1&&c.engine().monotonicMicroseconds()==100000);}},
    {"Peruutus",[](AppController& c){require(c.engine().reverseActive());}},
    {"Askel",[](AppController& c){require(c.engine().application().trip1DistanceMillimeters()==0);}},
    {"Peruutus",[](AppController& c){require(!c.engine().reverseActive());}},
    {"Jatka",[](AppController& c){require(!c.paused());}},
    {"10×",[](AppController& c){require(c.multiplier()==10);}},
    {"100×",[](AppController& c){require(c.multiplier()==100);}},
    {"Keskeytä",[](AppController& c){require(c.paused()&&c.engine().generatedPulseCount()>2);}},
    {"1×",[](AppController& c){require(c.multiplier()==1);}},
    {"ALAS",screen(core::Screen::Menu)},
    {"OIKEA",screen(core::Screen::OrderEdit)},
    {"OIKEA",none},{"OIKEA",none},{"OIKEA",none},
    {"OIKEA",none},{"OIKEA",none},{"OIKEA",none},{"OIKEA",none},
    {"YLÖS",none},{"OIKEA",none},{"ALAS",none},{"ALAS",none},
    {"OIKEA",[](AppController& c){require(c.engine().application().hasRouteOrder());}},
    {"AT",[](AppController& c){require(c.engine().displayModel().atOverlay.visible);}},
    {"AT",[](AppController& c){require(!c.engine().displayModel().atOverlay.visible);}},
    {"PISTE",[](AppController& c){require(c.engine().displayModel().finishResult.visible);}},
    {"Aloita alusta",[](AppController& c){require(c.resetPending());}},
    {"Peruuta",[](AppController& c){require(!c.resetPending()&&c.engine().application().hasRouteOrder());}},
    {"Aloita alusta",[](AppController& c){require(c.resetPending());}},
    {"Aloita alusta##vahvista",[](AppController& c){require(c.engine().monotonicMicroseconds()==0&&c.engine().generatedPulseCount()==0&&c.paused());}}
  };
}
void GuiSmokeCheck::beforeFrame(SimulatorView& view,AppController& c,SDL_Window* window) {
  ++frame_;
  auto& io=ImGui::GetIO();
  if(frame_<6)return;
  if(action_==actions_.size()) {
    if(phase_==0){SDL_SetWindowSize(window,780,660);phase_=1;frame_=0;return;}
    if(phase_==1&&frame_>6){snapshot_=2;phase_=2;return;}
    if(phase_==2) {
      // Anna ImGui-tapahtumajonon ja vierityksen päivittyä ennen uutta osumaa.
      if(frame_%3!=0)return;
      const auto it=view.hitRegions().find("36");
      if(it==view.hitRegions().end())throw std::runtime_error("Pienen ikkunan nopeusohjainta ei löytynyt");
      const auto& r=it->second;
      if(r.minimum.y<40||r.maximum.y>io.DisplaySize.y-20) {
        require(frame_<120);
        io.AddMousePosEvent(io.DisplaySize.x*.25f,io.DisplaySize.y*.45f);
        io.AddMouseWheelEvent(0,-3);
        return;
      }
      io.AddMousePosEvent((r.minimum.x+r.maximum.x)/2,(r.minimum.y+r.maximum.y)/2);
      io.AddMouseButtonEvent(0,true);phase_=3;return;
    }
    if(phase_==3) {
      const auto& r=view.hitRegions().at("36");
      io.AddMousePosEvent((r.minimum.x+r.maximum.x)/2,(r.minimum.y+r.maximum.y)/2);
      io.AddMouseButtonEvent(0,false);phase_=4;return;
    }
    if(phase_==4) {
      require(c.engine().targetSpeedKmh()==36&&c.engine().monotonicMicroseconds()==0);
      finished_=true;
      std::cout<<"Hyväksytty: graafinen käynnistys, ohjaimet, ajomääräys, reset sekä pienen ikkunan vieritys ja nopeussyöte\n";
    }
    return;
  }
  const auto& action=actions_[action_];
  const bool mouse=action.key==ImGuiKey_None&&action.text.empty();
  if(phase_<2&&mouse) {
    const auto it=view.hitRegions().find(actions_[action_].button);
    if(it==view.hitRegions().end())throw std::runtime_error("Ohjainta ei löytynyt: "+actions_[action_].button);
    const auto& r=it->second;
    io.AddMousePosEvent((r.minimum.x+r.maximum.x)/2,(r.minimum.y+r.maximum.y)/2);
  }
  if(phase_==0) {
    if(mouse)io.AddMouseButtonEvent(0,true);
    else if(!action.text.empty())io.AddInputCharactersUTF8(action.text.c_str());
    else {
      if(action.control)io.AddKeyEvent(ImGuiMod_Ctrl,true);
      io.AddKeyEvent(action.key,true);
    }
    phase_=1;
  } else if(phase_==1) {
    if(mouse)io.AddMouseButtonEvent(0,false);
    else if(action.key!=ImGuiKey_None){io.AddKeyEvent(action.key,false);if(action.control)io.AddKeyEvent(ImGuiMod_Ctrl,false);}
    phase_=2;
  }
  else if(phase_==2) {
    actions_[action_].verify(c);
    std::cout<<"Hyväksytty ohjain: "<<actions_[action_].button<<'\n';
    if(action_>0&&action.button=="AT"&&actions_[action_-1].button=="AT")snapshot_=1;
    ++action_;phase_=0;
  }
}
void GuiSmokeCheck::afterRender(SDL_Renderer* renderer) {
  if(snapshot_==0||outputDirectory_.empty())return;
  int width=0,height=0;SDL_GetRendererOutputSize(renderer,&width,&height);
  SDL_Surface* surface=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_ARGB8888);
  if(!surface)throw std::runtime_error("Tarkistuskuvan muistia ei voitu varata");
  const int read=SDL_RenderReadPixels(renderer,nullptr,surface->format->format,surface->pixels,surface->pitch);
  const auto path=std::filesystem::path(outputDirectory_)/(snapshot_==1?"simulaattori-ajo.bmp":"simulaattori-pieni.bmp");
  const int saved=read==0?SDL_SaveBMP(surface,path.string().c_str()):-1;
  SDL_FreeSurface(surface);
  if(saved!=0)throw std::runtime_error("Tarkistuskuvan tallennus epäonnistui");
  snapshot_=0;
}
}  // namespace simulator::desktop
