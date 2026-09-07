#pragma once

#include <map>
#include <string>
#include "imgui.h"
#include "AppController.h"
#include "CoreVersionView.h"

namespace simulator::desktop {
struct HitRegion { ImVec2 minimum, maximum; };
class SimulatorView {
 public:
  SimulatorView(AppController& controller, ImFont* displayFont,
      std::string coreHome = {}, std::string coreUpstream = {})
      : controller_(controller), displayFont_(displayFont), coreVersions_(std::move(coreHome),std::move(coreUpstream)) {}
  void render();
  // Käynnistystesti käyttää oikeiden ohjainten sijainteja, ei kiinteitä pikseleitä.
  const std::map<std::string, HitRegion>& hitRegions() const { return hitRegions_; }
  bool restartCompleted() const { return coreVersions_.closeRequested(); }
  const std::string& corePhase() const { return coreVersions_.phase(); }
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
  CoreVersionView coreVersions_;
  bool corePage_ = false;
  bool helpOpen_ = false;
  bool followLog_ = true;
  bool showApplicationOnly_ = false;
  float speedInput_ = 0;
  std::map<std::string, HitRegion> hitRegions_;
};
void applyDesktopStyle(float scale);
}  // namespace simulator::desktop
