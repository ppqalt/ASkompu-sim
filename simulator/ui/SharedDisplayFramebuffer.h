#pragma once

#include <cstdint>
#include <vector>

#include "core/DisplayModel.h"

namespace simulator::desktop {

struct NativeDisplayFramebuffer {
  uint16_t width = 0;
  uint16_t height = 0;
  bool panelInverted = false;
  std::vector<uint16_t> pixels;
};

bool sharedDisplayRendererAvailable();
bool sharedDisplayRendererSupports(core::Screen screen);
NativeDisplayFramebuffer renderSharedDisplay(const core::DisplayModel& model,
                                             uint16_t width, uint16_t height);

}  // namespace simulator::desktop
