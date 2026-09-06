#pragma once

#include "imgui.h"
#include "core/DisplayModel.h"

namespace simulator::desktop {
// Piirtosovitin: vain näyttömalli sisään, ei pääsyä simulaattorin mutaatioihin.
void drawDeviceDisplay(const core::DisplayModel& model, ImVec2 origin,
                       ImVec2 size, ImFont* font);
}  // namespace simulator::desktop
