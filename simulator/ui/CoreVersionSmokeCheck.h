#pragma once
#include <SDL.h>
#include <string>
#include "SimulatorView.h"
namespace simulator::desktop {
class CoreVersionSmokeCheck {
 public:
  CoreVersionSmokeCheck(std::string directory,bool build):directory_(std::move(directory)),build_(build){}
  void beforeFrame(SimulatorView& view,SDL_Window* window);
  void afterRender(SDL_Renderer* renderer);
  bool finished() const{return finished_;}
 private:
  bool click(SimulatorView& view,const char* title);
  void advance(){++step_;clickPhase_=0;settle_=0;}
  std::string directory_,capture_;
  bool build_=false,finished_=false;
  unsigned step_=0,clickPhase_=0,settle_=0;
  double started_=0;
};
}
