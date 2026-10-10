#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <SDL.h>
#include "imgui.h"
#include "core/DisplayModel.h"

namespace simulator::desktop {
enum class DeviceRendererMode { Shared, Legacy };
enum class DisplayScaling { Nearest, Linear };

// Host presentation only. Shared pixels come from the selected ASkompu core;
// old cores retain the original simulator display as a compatibility fallback.
class DeviceDisplay {
 public:
  explicit DeviceDisplay(SDL_Renderer* renderer) : renderer_(renderer) {}
  ~DeviceDisplay();
  DeviceDisplay(const DeviceDisplay&) = delete;
  DeviceDisplay& operator=(const DeviceDisplay&) = delete;

  DeviceRendererMode draw(const core::DisplayModel& model, ImVec2 origin,
                          ImVec2 size, ImFont* font, uint16_t nativeWidth,
                          uint16_t nativeHeight);
  void setScaling(DisplayScaling scaling);
  DisplayScaling scaling() const { return scaling_; }

 private:
  void updateSharedTexture(const core::DisplayModel& model,
                           uint16_t nativeWidth, uint16_t nativeHeight);
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
  uint16_t textureWidth_ = 0;
  uint16_t textureHeight_ = 0;
  DisplayScaling scaling_ = DisplayScaling::Nearest;
  core::DisplayModel cachedModel_{};
  bool hasCachedModel_ = false;
  std::string cachedMenuTitle_;
  std::string cachedMenuLabels_[core::MENU_VISIBLE_ROWS];
  std::vector<uint8_t> rgbaPixels_;
};

const char* rendererModeName(DeviceRendererMode mode);
}  // namespace simulator::desktop
