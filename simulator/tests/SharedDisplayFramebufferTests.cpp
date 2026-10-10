#include "ui/SharedDisplayFramebuffer.h"
#include "ui/DisplayModelRenderState.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool result, const char* message) {
  if (!result) throw std::runtime_error(message);
}

template<class Exception, class Operation>
void requireThrows(Operation operation, const char* message) {
  try { operation(); }
  catch (const Exception&) { return; }
  throw std::runtime_error(message);
}
}  // namespace

int main() {
  try {
    using namespace simulator::desktop;
    core::DisplayModel model{};
    model.screen = core::Screen::StartupTimeEntry;
    model.timeEntry = {13, 42, core::TimeField::Hour, true};
    model.textColor = domain::TextColor::WHITE;
    model.backlightPercent = 100;
    requireThrows<std::invalid_argument>(
        [&] { renderSharedDisplay(model, 400, 240); },
        "Unsupported native resolution accepted");
    require(!sharedDisplayRendererSupports(static_cast<core::Screen>(255)),
            "Unknown screen advertised as supported");

    for (uint16_t width : {uint16_t{320}, uint16_t{480}}) {
      const uint16_t height = width == 320 ? 170 : 320;
      if (!sharedDisplayRendererAvailable()) {
        require(!sharedDisplayRendererSupports(model.screen),
                "Old core advertised a shared renderer");
        requireThrows<std::runtime_error>(
            [&] { renderSharedDisplay(model, width, height); },
            "Old core silently returned fake shared pixels");
        continue;
      }
      const auto first = renderSharedDisplay(model, width, height);
      const auto second = renderSharedDisplay(model, width, height);
      require(first.width == width && first.height == height,
              "Framebuffer resolution differs from native profile");
      require(first.pixels.size() == static_cast<size_t>(width) * height,
              "Framebuffer has invalid pixel count");
      require(first.pixels == second.pixels,
              "Identical models do not produce deterministic pixels");
      require(first.panelInverted == (width == 480),
              "Display profile inversion changed");
      auto changedTime = model;
      ++changedTime.timeEntry.hour;
      require(!sameRenderedDisplayState(model, changedTime),
              "Changed time value leaves stale cached pixels");
      require(renderSharedDisplay(changedTime, width, height).pixels != first.pixels,
              "Selected renderer did not receive the changed time model");
    }

    const core::Screen screens[] = {
        core::Screen::StartupTimeEntry, core::Screen::BasicView,
        core::Screen::Menu, core::Screen::TimeEdit,
        core::Screen::CalibrationEdit, core::Screen::MittisProposal,
        core::Screen::JatResult, core::Screen::StartTimeEdit,
        core::Screen::OverrideMenu, core::Screen::OverrideEdit,
        core::Screen::ResultView, core::Screen::OrderAccessPrompt,
        core::Screen::OrderEdit, core::Screen::Diagnostics};
    for (auto screen : screens) {
      model.screen = screen;
      require(sameRenderedDisplayState(model, model),
              "Unchanged model invalidates cache");
      if (!sharedDisplayRendererAvailable())
        require(!sharedDisplayRendererSupports(screen),
                "Old core incorrectly supports a shared screen");
    }

    model.screen = core::Screen::Menu;
    model.menu.visibleRowCount = 1;
    model.menu.title = "PÄÄVALIKKO";
    model.menu.rows[0] = {"AJOMÄÄRÄYS", true};
    auto changed = model;
    changed.menu.rows[0].label = "NÄYTTÖASETUKSET";
    require(!sameRenderedDisplayState(model, changed),
            "Changed Finnish menu text leaves stale cached pixels");
    changed = model;
    changed.menu.rows[0].enabled = false;
    require(!sameRenderedDisplayState(model, changed),
            "Disabled row leaves stale cached pixels");
    changed = model;
    changed.menu.selectedVisibleRow = 1;
    require(!sameRenderedDisplayState(model, changed),
            "Selected row leaves stale cached pixels");
    std::cout << "Shared display bridge and old-core fallback checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
