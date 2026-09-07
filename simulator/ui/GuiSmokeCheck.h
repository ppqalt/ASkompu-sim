#pragma once
#include <functional>
#include <string>
#include <vector>
#include <SDL.h>
#include "SimulatorView.h"

namespace simulator::desktop {
// Valinnainen näkyvä käyttöliittymän integraatiotarkistus; ei pikselivertailuja.
class GuiSmokeCheck {
 public:
  explicit GuiSmokeCheck(std::string outputDirectory);
  void beforeFrame(SimulatorView& view, AppController& controller, SDL_Window* window);
  void afterRender(SDL_Renderer* renderer);
  bool finished() const { return finished_; }
 private:
  struct Action {
    std::string button;
    std::function<void(AppController&)> verify;
    ImGuiKey key = ImGuiKey_None;
    bool control = false;
    std::string text{};
  };
  std::vector<Action> actions_;
  std::string outputDirectory_;
  size_t action_ = 0;
  unsigned frame_ = 0;
  unsigned phase_ = 0;
  bool finished_ = false;
  int snapshot_ = 0;
};
}  // namespace simulator::desktop
