#pragma once

#include <map>
#include <string>
#include "imgui.h"
#include "AppController.h"

namespace simulator::desktop {
struct HitRegion { ImVec2 minimum, maximum; };
class SimulatorView {
 public:
  SimulatorView(AppController& controller, ImFont* displayFont)
      : controller_(controller), displayFont_(displayFont) {}
  void render();
  // Käynnistystesti käyttää oikeiden ohjainten sijainteja, ei kiinteitä pikseleitä.
  const std::map<std::string, HitRegion>& hitRegions() const { return hitRegions_; }
 private:
  bool button(const char* title, ImVec2 size = {0,0}, bool accent = false);
  void remember(const char* title);
  void transport();
  void instrument();
  void vehicle();
  void controls();
  void telemetry();
  void eventLog();
  void dialogs();
  void shortcuts();
  AppController& controller_;
  ImFont* displayFont_;
  bool helpOpen_ = false;
  bool followLog_ = true;
  bool showApplicationOnly_ = false;
  float speedInput_ = 0;
  std::map<std::string, HitRegion> hitRegions_;
};
void applyDesktopStyle(float scale);
}  // namespace simulator::desktop
