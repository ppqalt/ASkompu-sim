#pragma once

#include <map>
#include <string>
#include "imgui.h"
#include "AppController.h"
#include "CoreVersionView.h"
#include "DeviceDisplay.h"
#include "SpeedInput.h"

namespace simulator::desktop {
struct HitRegion { ImVec2 minimum, maximum; };
class SimulatorView {
 public:
  SimulatorView(AppController& controller, ImFont* displayFont, SDL_Renderer* renderer,
      std::string coreHome = {}, std::string coreUpstream = {}, std::string localCore = {})
      : controller_(controller), displayFont_(displayFont), deviceDisplay_(renderer),
        coreVersions_(std::move(coreHome),std::move(coreUpstream),std::move(localCore)) {}
  void render();
  // Käynnistystesti käyttää oikeiden ohjainten sijainteja, ei kiinteitä pikseleitä.
  const std::map<std::string, HitRegion>& hitRegions() const { return hitRegions_; }
  bool restartCompleted() const { return coreVersions_.closeRequested(); }
  const std::string& corePhase() const { return coreVersions_.phase(); }
  const std::string& coreOperation() const { return coreVersions_.operation(); }
  bool localSourceReady() const { return coreVersions_.localSourceReady(); }
  const std::string& localSourcePath() const { return coreVersions_.localSourcePath(); }
  bool folderSourceReady() const { return coreVersions_.folderSourceReady(); }
  DeviceRendererMode rendererMode() const { return rendererMode_; }
  const char* speedInputError() const { return speedInput_.error(); }
  bool helpVisible() const { return helpVisible_; }
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
  DeviceDisplay deviceDisplay_;
  DeviceRendererMode rendererMode_ = DeviceRendererMode::Legacy;
  CoreVersionView coreVersions_;
  bool corePage_ = false;
  bool helpOpen_ = false;
  bool helpVisible_ = false;
  bool followLog_ = true;
  bool showApplicationOnly_ = false;
  SpeedInput speedInput_;
  std::map<std::string, HitRegion> hitRegions_;
};
void applyDesktopStyle(float scale);
}  // namespace simulator::desktop
