#include "CoreVersionSmokeCheck.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace simulator::desktop {
bool CoreVersionSmokeCheck::click(SimulatorView& view,const char* title){
  auto& io=ImGui::GetIO();const auto it=view.hitRegions().find(title);
  if(it==view.hitRegions().end())return false;
  const auto r=it->second;
  if(r.minimum.y<15||r.maximum.y>io.DisplaySize.y-15){
    io.AddMouseButtonEvent(0,false);clickPhase_=0;settle_=0;
    io.AddMousePosEvent(8,io.DisplaySize.y*.5f);io.AddMouseWheelEvent(0,r.minimum.y<15?3.f:-3.f);return false;
  }
  io.AddMousePosEvent((r.minimum.x+r.maximum.x)/2,(r.minimum.y+r.maximum.y)/2);
  if(clickPhase_==0){io.AddMouseButtonEvent(0,true);clickPhase_=1;return false;}
  if(clickPhase_==1){io.AddMouseButtonEvent(0,false);clickPhase_=2;return false;}
  return true;
}
void CoreVersionSmokeCheck::beforeFrame(SimulatorView& view,SDL_Window* window){
  if(!started_)started_=ImGui::GetTime();
  if(ImGui::GetTime()-started_>(build_?1800:90))throw std::runtime_error("Ytimen versionvalitsimen GUI-testi ylitti aikarajan");
  if(++settle_<5)return;
  if(settle_==5)std::cout<<"Ytimen GUI-tarkistus: vaihe "<<step_<<std::endl;
  switch(step_){
    case 0:if(click(view,"Ytimen versio"))advance();break;
    case 1:if(view.corePhase()=="done"){capture_="ytimen-tyhja.bmp";advance();}else if(view.corePhase()=="failed")throw std::runtime_error("Versionvalitsimen työkalutarkistus epäonnistui");break;
    case 2:
      if(!view.hitRegions().count("Myöhemmin")||click(view,"Myöhemmin"))advance();
      break;
    case 3:
      if(!view.hitRegions().count("Näytä rakennus")||click(view,"Näytä rakennus")){capture_="ytimen-aloitustila.bmp";advance();}
      break;
    case 4:if(click(view,"Päivitä versiolista"))advance();break;
    case 5:if(view.corePhase()=="done"){capture_="ytimen-versiot.bmp";advance();}else if(view.corePhase()=="failed")throw std::runtime_error("Versionvalitsimen versiolistan haku epäonnistui");break;
    case 6:SDL_SetWindowSize(window,780,660);advance();break;
    case 7:capture_="ytimen-versiot-pieni.bmp";advance();break;
    case 8:
      if(click(view,"Tarkka commit")){
        if(view.hitRegions().count("Ratkaise SHA"))advance();
        else{clickPhase_=0;settle_=0;}
      }
      break;
    case 9:if(click(view,"Ratkaise SHA"))advance();break;
    case 10:if(view.corePhase()=="failed"){capture_="ytimen-versiot-virhe.bmp";advance();}break;
    case 11:
      if(!build_){finished_=true;std::cout<<"Hyväksytty: ytimen versionvalitsin, versiolista, pieni ikkuna ja virhetila\n";}
      else{SDL_SetWindowSize(window,1380,960);advance();}break;
    case 12:if(click(view,"Rakenna valittu ydin"))advance();break;
    case 13:if(view.corePhase()=="building"){capture_="ytimen-rakennus.bmp";advance();}else if(view.corePhase()=="failed")throw std::runtime_error("Valitsimen oikea rakennus epäonnistui ennen peruutustestiä");break;
    case 14:if(click(view,"Peruuta toiminto"))advance();break;
    case 15:if(view.corePhase()=="cancelled")advance();break;
    case 16:if(click(view,"Rakenna valittu ydin"))advance();break;
    case 17:if(view.corePhase()=="ready"){capture_="ytimen-rakennus-valmis.bmp";advance();}else if(view.corePhase()=="failed")throw std::runtime_error("Valitsimen oikea rakennus tai testit epäonnistuivat");break;
    case 18:if(click(view,"Käynnistä uudelleen"))advance();break;
    case 19:if(click(view,"Aloita uusi sessio"))advance();break;
    case 20:if(view.restartCompleted()){finished_=true;std::cout<<"Hyväksytty: ytimen GUI-rakennus, peruutus, testit ja oikea uudelleenkäynnistys\n";}else if(view.corePhase()=="failed")throw std::runtime_error("Uuden simulaattorin käynnistys epäonnistui");break;
  }
}
void CoreVersionSmokeCheck::afterRender(SDL_Renderer* renderer){
  if(capture_.empty()||directory_.empty())return;
  int w=0,h=0;SDL_GetRendererOutputSize(renderer,&w,&h);
  auto* surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
  if(!surface)throw std::runtime_error("Versionvalitsimen tarkistuskuvaa ei voitu luoda");
  const auto path=std::filesystem::u8path(directory_)/capture_;
  const bool ok=SDL_RenderReadPixels(renderer,nullptr,surface->format->format,surface->pixels,surface->pitch)==0&&SDL_SaveBMP(surface,path.u8string().c_str())==0;
  SDL_FreeSurface(surface);capture_.clear();
  if(!ok)throw std::runtime_error("Versionvalitsimen tarkistuskuvaa ei voitu tallentaa");
}
}
