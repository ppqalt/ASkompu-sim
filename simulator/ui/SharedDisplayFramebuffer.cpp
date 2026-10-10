#include "SharedDisplayFramebuffer.h"

#include <stdexcept>

#if ASKOMPU_HAS_SHARED_DISPLAY_RENDERER
#include "ui/DisplayRenderer.h"
#include "FramebufferCanvas.h"
#endif

namespace simulator::desktop {
namespace {
#if ASKOMPU_HAS_SHARED_DISPLAY_RENDERER
uint16_t textColor(const core::DisplayModel& model, bool inverted) {
  const uint16_t color = model.textColor == domain::TextColor::RED ? 0xF800 :
      model.textColor == domain::TextColor::GREEN ? 0x07E0 : 0xFFFF;
  return inverted ? static_cast<uint16_t>(color ^ 0xFFFF) : color;
}
#endif
}  // namespace

bool sharedDisplayRendererAvailable() {
#if ASKOMPU_HAS_SHARED_DISPLAY_RENDERER
  return true;
#else
  return false;
#endif
}

bool sharedDisplayRendererSupports(core::Screen screen) {
  if (!sharedDisplayRendererAvailable()) return false;
  switch (screen) {
    case core::Screen::StartupTimeEntry:
    case core::Screen::BasicView:
    case core::Screen::Menu:
    case core::Screen::TimeEdit:
    case core::Screen::CalibrationEdit:
    case core::Screen::MittisProposal:
    case core::Screen::JatResult:
    case core::Screen::StartTimeEdit:
    case core::Screen::OverrideMenu:
    case core::Screen::OverrideEdit:
    case core::Screen::ResultView:
    case core::Screen::OrderAccessPrompt:
    case core::Screen::OrderEdit:
    case core::Screen::Diagnostics:
      return true;
  }
  return false;
}

NativeDisplayFramebuffer renderSharedDisplay(const core::DisplayModel& model,
                                             uint16_t width, uint16_t height) {
  if (!((width == 320 && height == 170) ||
        (width == 480 && height == 320)))
    throw std::invalid_argument("Unsupported ASkompu display profile");
  if (!sharedDisplayRendererSupports(model.screen))
    throw std::runtime_error("Selected ASkompu core has no shared renderer for this screen");
#if ASKOMPU_HAS_SHARED_DISPLAY_RENDERER
  // Match the existing physical display profiles, including ILI9488 panel
  // inversion. Presentation converts these raw sprite pixels to visible RGB.
  const bool inverted = width == 480;
  const uint16_t background = inverted ? 0xFFFF : 0x0000;
  ui::test::FramebufferCanvas canvas(width, height);
  canvas.clear(background);
  ui::DisplayRenderer<ui::test::FramebufferCanvas>::renderScreen(
      canvas, width, height, background, model, textColor(model, inverted));
  return {width, height, inverted, canvas.pixels()};
#else
  return {};  // The unsupported-core guard above always throws.
#endif
}

}  // namespace simulator::desktop
