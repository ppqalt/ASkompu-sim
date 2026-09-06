#include "DisplayView.h"

#include <inttypes.h>
#include <cstdio>
#include <cstring>

#include "BoardConfig.h"
#include "DisplayLayout.h"
#include "DisplayFormatting.h"
#include "RouteOrderFormatting.h"

namespace ui {
namespace {

// The verified ILI9488 module needs panel inversion for clean RGB levels.
// Compensate for that inversion in the framebuffer so the physical colors
// still match the user-facing setting and the physical background stays black.
#ifdef ASKOMPU_BOARD_ILI9488_MAIN
constexpr uint16_t DISPLAY_BACKGROUND_COLOR = TFT_WHITE;
constexpr uint16_t DISPLAY_RED = TFT_CYAN;
constexpr uint16_t DISPLAY_GREEN = TFT_MAGENTA;
constexpr uint16_t DISPLAY_WHITE = TFT_BLACK;
#else
constexpr uint16_t DISPLAY_BACKGROUND_COLOR = TFT_BLACK;
constexpr uint16_t DISPLAY_RED = TFT_RED;
constexpr uint16_t DISPLAY_GREEN = TFT_GREEN;
constexpr uint16_t DISPLAY_WHITE = TFT_WHITE;
#endif

const char* eventTypeName(domain::DomainEventType type) {
  switch (type) {
    case domain::DomainEventType::NORMAL_POINT: return "VAIHTOPISTE";
    case domain::DomainEventType::POINT_UNDO: return "PISTE PERUTTU";
    case domain::DomainEventType::AT: return "AT";
    case domain::DomainEventType::AT_CANCELLED: return "AT PERUTTU";
    case domain::DomainEventType::JAT: return "JAT";
    case domain::DomainEventType::FINISH: return "MAALI";
    case domain::DomainEventType::START_TIME_PROPOSED: return "LAHTOEHDOTUS";
    case domain::DomainEventType::START_TIME_ACCEPTED: return "LAHTO HYV.";
    case domain::DomainEventType::START_TIME_CORRECTED: return "LAHTO KORJ.";
    case domain::DomainEventType::MITTIS_PROPOSED: return "MITTIS EHD.";
    case domain::DomainEventType::MITTIS_ACCEPTED: return "MITTIS HYV.";
    case domain::DomainEventType::MITTIS_REJECTED: return "MITTIS HYL.";
    case domain::DomainEventType::ADDITIONAL_ORDER: return "LISAMAARAYS";
    case domain::DomainEventType::ROAD_BREAK: return "TIEKATKO";
    case domain::DomainEventType::REVERSE_CHANGED: return "PERUUTUS";
    case domain::DomainEventType::TRIP_RESET: return "TRIP NOLLAUS";
    case domain::DomainEventType::MANUAL_SUBTRACTION_CHANGED:
      return "MIINUSTUS";
    case domain::DomainEventType::TIME_ADJUSTMENT:
      return "AIKA+-";
  }
  return "TAPAHTUMA";
}

uint32_t textFingerprint(const char* text) {
  uint32_t hash = 2166136261UL;
  if (!text) return hash;
  while (*text) {
    hash ^= static_cast<uint8_t>(*text++);
    hash *= 16777619UL;
  }
  return hash;
}

uint32_t menuRowsFingerprint(const core::MenuDisplayModel& model) {
  uint32_t hash = 2166136261UL;
  hash = (hash ^ model.visibleRowCount) * 16777619UL;
  hash = (hash ^ model.scrollOffset) * 16777619UL;
  hash = (hash ^ model.totalRows) * 16777619UL;
  for (uint8_t row = 0; row < model.visibleRowCount; ++row) {
    hash ^= textFingerprint(model.rows[row].label);
    hash *= 16777619UL;
    hash = (hash ^ model.rows[row].enabled) * 16777619UL;
  }
  return hash;
}

uint8_t menuTextScale(domain::MenuFontSize size) {
  return static_cast<uint8_t>(size) + 1U;
}

const char* footerLabel(const char* text) {
  if (!text) return "";
  const char* separator = std::strchr(text, ':');
  if (!separator) return text;
  ++separator;
  while (*separator == ' ') ++separator;
  return separator;
}

bool sameClock(const core::ClockTime& first, const core::ClockTime& second) {
  return first.hour == second.hour && first.minute == second.minute &&
         first.second == second.second;
}

bool sameSegment(const domain::SegmentDefinition& first,
                 const domain::SegmentDefinition& second) {
  return first.segmentIndex == second.segmentIndex &&
         first.startPointIndex == second.startPointIndex &&
         first.endPointIndex == second.endPointIndex &&
         first.segmentType == second.segmentType && first.value == second.value &&
         first.hasMittisDuration == second.hasMittisDuration &&
         first.mittisDurationSeconds == second.mittisDurationSeconds &&
         first.pointTypeAtEnd == second.pointTypeAtEnd &&
         first.hasJatType == second.hasJatType &&
         first.jatType == second.jatType &&
         first.hasJatOffsetMinutes == second.hasJatOffsetMinutes &&
         first.jatOffsetMinutes == second.jatOffsetMinutes;
}

bool currentSegmentChanged(const core::CompetitionDisplayModel& first,
                           const core::CompetitionDisplayModel& second) {
  return first.hasCurrentSegment != second.hasCurrentSegment ||
         (first.hasCurrentSegment &&
          !sameSegment(first.currentSegment, second.currentSegment));
}

bool nextSegmentChanged(const core::CompetitionDisplayModel& first,
                        const core::CompetitionDisplayModel& second) {
  return first.timeAdjustmentEditing != second.timeAdjustmentEditing ||
         first.editedTimeAdjustmentSeconds !=
             second.editedTimeAdjustmentSeconds ||
         first.hasNextSegment != second.hasNextSegment ||
         (first.hasNextSegment &&
          !sameSegment(first.nextSegment, second.nextSegment));
}

bool deltaChanged(const core::DisplayModel& first,
                  const core::DisplayModel& second) {
  return first.competition.state != second.competition.state ||
         first.competition.deltaSeconds != second.competition.deltaSeconds ||
         first.competition.deltaFrozen != second.competition.deltaFrozen ||
         first.atOverlay.visible != second.atOverlay.visible ||
         (second.atOverlay.visible &&
          (first.atOverlay.clockTime.hour != second.atOverlay.clockTime.hour ||
           first.atOverlay.clockTime.minute != second.atOverlay.clockTime.minute ||
           first.atOverlay.clockTime.second != second.atOverlay.clockTime.second));
}

WidgetRect deltaTransferRect(const DisplayLayout& layout) {
  const int16_t height = layout.height >= 300 ? 104 : layout.delta.height;
  return {layout.delta.x,
          static_cast<int16_t>(layout.delta.y +
                               (layout.delta.height - height) / 2),
          layout.delta.width, height};
}

void clearRect(TFT_eSprite& canvas, const WidgetRect& rect) {
  canvas.fillRect(rect.x, rect.y, rect.width, rect.height,
                  DISPLAY_BACKGROUND_COLOR);
}

void pushRect(TFT_eSprite& canvas, const WidgetRect& rect) {
  canvas.pushSprite(rect.x, rect.y, rect.x, rect.y, rect.width, rect.height);
}

}  // namespace

DisplayView::DisplayView() : canvas_(&display_) {}

bool DisplayView::begin() {
#ifdef ASKOMPU_BOARD_ILI9488_MAIN
  // Drive the external module's active-HIGH backlight before initialization.
  setBacklight(true);
#endif
  display_.init();
  display_.setRotation(BoardConfig::DISPLAY_ROTATION);
  // Re-assert the configured level after TFT_eSPI initialization as well.
  setBacklight(true);
  canvas_.setColorDepth(16);
  if (BoardConfig::USE_PSRAM_SPRITE)
    canvas_.setAttribute(PSRAM_ENABLE, true);
  spriteReady_ = canvas_.createSprite(BoardConfig::DISPLAY_WIDTH,
                                      BoardConfig::DISPLAY_HEIGHT) != nullptr;
  if (!spriteReady_) {
    display_.fillScreen(DISPLAY_BACKGROUND_COLOR);
    display_.setTextColor(DISPLAY_RED, DISPLAY_BACKGROUND_COLOR);
    display_.setTextDatum(MC_DATUM);
    display_.drawString("DISPLAY BUFFER ERROR", BoardConfig::DISPLAY_WIDTH / 2,
                        BoardConfig::DISPLAY_HEIGHT / 2, 2);
    return false;
  }
  canvas_.fillSprite(DISPLAY_BACKGROUND_COLOR);
  canvas_.pushSprite(0, 0);
  return true;
}

void DisplayView::setBacklight(bool enabled) {
  backlightEnabled_ = enabled;
#if defined(TFT_BL)
  pinMode(TFT_BL, OUTPUT);
  if (enabled) {
    setBacklightPercent(backlightPercent_);
  } else {
    analogWrite(TFT_BL, TFT_BACKLIGHT_ON == HIGH ? 0 : 255);
  }
#endif
}

void DisplayView::setBacklightPercent(uint8_t percent) {
#if defined(TFT_BL)
  if (percent < domain::MIN_BACKLIGHT_PERCENT)
    percent = domain::MIN_BACKLIGHT_PERCENT;
  if (percent > domain::MAX_BACKLIGHT_PERCENT)
    percent = domain::MAX_BACKLIGHT_PERCENT;
  backlightPercent_ = percent;
  const uint8_t duty = static_cast<uint8_t>(
      (static_cast<uint16_t>(percent) * 255U + 50U) / 100U);
  pinMode(TFT_BL, OUTPUT);
  if (backlightEnabled_)
    analogWrite(TFT_BL, TFT_BACKLIGHT_ON == HIGH ? duty : 255U - duty);
  else
    analogWrite(TFT_BL, TFT_BACKLIGHT_ON == HIGH ? 0 : 255);
#else
  (void)percent;
#endif
}

uint16_t DisplayView::width() const { return BoardConfig::DISPLAY_WIDTH; }
uint16_t DisplayView::height() const { return BoardConfig::DISPLAY_HEIGHT; }

void DisplayView::drawFooter(const char* left, const char* upDown,
                             const char* right, uint16_t color) {
  if (!showLabels_) return;
  canvas_.setTextColor(color, DISPLAY_BACKGROUND_COLOR);
  const int16_t y = BoardConfig::DISPLAY_HEIGHT - 6;
  const auto drawLabel = [&](const char* text, uint8_t datum, int16_t x) {
    text = footerLabel(text);
    if (!text[0]) return;
    canvas_.setTextSize(menuTextScale(menuFontSize_));
    canvas_.setTextDatum(datum);
    canvas_.drawString(text, x, y, 1);
  };
  drawLabel(left, BL_DATUM, 4);
  drawLabel(upDown, BC_DATUM, BoardConfig::DISPLAY_WIDTH / 2);
  drawLabel(right, BR_DATUM, BoardConfig::DISPLAY_WIDTH - 4);
}

void DisplayView::render(const core::DisplayModel& model) {
  if (!spriteReady_) return;
  if (!hasRendered_ || backlightPercent_ != model.backlightPercent)
    setBacklightPercent(model.backlightPercent);
  switch (model.textColor) {
    case domain::TextColor::RED:
      textColor_ = DISPLAY_RED;
      break;
    case domain::TextColor::GREEN:
      textColor_ = DISPLAY_GREEN;
      break;
    case domain::TextColor::WHITE:
    default:
      textColor_ = DISPLAY_WHITE;
      break;
  }
  const bool sameScreen = hasRendered_ && renderedScreen_ == model.screen;
  const bool colorChanged = hasRendered_ &&
                            lastModel_.textColor != model.textColor;
  const bool labelsChanged = hasRendered_ &&
                             lastModel_.showLabels != model.showLabels;
  const bool fontChanged = hasRendered_ &&
                           lastModel_.menuFontSize != model.menuFontSize;
  showLabels_ = model.showLabels;
  menuFontSize_ = model.menuFontSize;
  if (sameScreen && !colorChanged && !labelsChanged && !fontChanged &&
      model.screen == core::Screen::BasicView &&
      lastModel_.finishResult.visible == model.finishResult.visible &&
      lastModel_.competition.manualSubtractActive ==
          model.competition.manualSubtractActive &&
      !model.finishResult.visible) {
    const DisplayLayout layout =
        layoutFor(BoardConfig::DISPLAY_WIDTH, BoardConfig::DISPLAY_HEIGHT);
    const bool tripChanged =
        lastModel_.tripDisplayMode != model.tripDisplayMode ||
        lastModel_.trip1.distanceMillimeters != model.trip1.distanceMillimeters ||
        lastModel_.trip2.distanceMillimeters != model.trip2.distanceMillimeters;
    if (tripChanged)
      canvas_.fillRect(layout.trip1.x, layout.trip1.y, layout.trip1.width,
                       layout.trip1.height, DISPLAY_BACKGROUND_COLOR);
    if (!sameClock(lastModel_.clock, model.clock))
      canvas_.fillRect(layout.clock.x, layout.clock.y, layout.clock.width,
                       layout.clock.height, DISPLAY_BACKGROUND_COLOR);
    if (currentSegmentChanged(lastModel_.competition, model.competition))
      clearRect(canvas_, layout.currentSegment);
    if (deltaChanged(lastModel_, model))
      clearRect(canvas_, deltaTransferRect(layout));
    if (nextSegmentChanged(lastModel_.competition, model.competition))
      clearRect(canvas_, layout.nextSegment);
    if (lastModel_.showSpeed != model.showSpeed ||
        lastModel_.speedKmh != model.speedKmh ||
        lastModel_.competition.undoPromptVisible !=
            model.competition.undoPromptVisible)
      clearRect(canvas_, layout.debugSpeed);
  } else {
    canvas_.fillSprite(DISPLAY_BACKGROUND_COLOR);
  }
  canvas_.setTextSize(1);
  switch (model.screen) {
    case core::Screen::StartupTimeEntry:
    case core::Screen::TimeEdit:
      showTimeEntry(model.timeEntry);
      break;
    case core::Screen::BasicView:
      showBasicView(model);
      break;
    case core::Screen::Menu:
      showMenu(model.menu);
      break;
    case core::Screen::CalibrationEdit:
      showCalibration(model.calibration);
      break;
    case core::Screen::MittisProposal:
      showMittis(model.mittis);
      break;
    case core::Screen::JatResult:
      showJatResult(model.jatResult);
      break;
    case core::Screen::StartTimeEdit:
      showStartTimeEdit(model.startTimeEdit);
      break;
    case core::Screen::OverrideMenu:
      showOverrideMenu(model.overrideMenu);
      break;
    case core::Screen::OverrideEdit:
      showOverrideEdit(model.overrideEdit);
      break;
    case core::Screen::ResultView:
      showResultView(model.resultView);
      break;
    case core::Screen::OrderAccessPrompt:
      showOrderAccess(model.orderAccess);
      break;
    case core::Screen::OrderEdit:
      showOrder(model.order);
      break;
    case core::Screen::Diagnostics:
      showDiagnostics(model.diagnostics);
      break;
  }
  if (!sameScreen || colorChanged || labelsChanged || fontChanged) {
    canvas_.pushSprite(0, 0);
  } else if (model.screen == core::Screen::Menu) {
    const uint32_t titleFingerprint = textFingerprint(model.menu.title);
    const uint32_t rowsFingerprint = menuRowsFingerprint(model.menu);
    if (titleFingerprint != menuTitleFingerprint_) {
      canvas_.pushSprite(0, 0);
    } else if (rowsFingerprint != menuRowsFingerprint_) {
      const int16_t top = BoardConfig::DISPLAY_WIDTH >= 480 ? 56 : 30;
      canvas_.pushSprite(0, top, 0, top, BoardConfig::DISPLAY_WIDTH,
                         BoardConfig::DISPLAY_HEIGHT - top);
    } else if (lastModel_.menu.selectedVisibleRow !=
               model.menu.selectedVisibleRow) {
      const int16_t top = BoardConfig::DISPLAY_WIDTH >= 480 ? 56 : 30;
      const int16_t rowHeight =
          (BoardConfig::DISPLAY_HEIGHT - top - 8) /
          core::MENU_VISIBLE_ROWS;
      const int16_t oldY =
          top + lastModel_.menu.selectedVisibleRow * rowHeight;
      const int16_t newY = top + model.menu.selectedVisibleRow * rowHeight;
      canvas_.pushSprite(0, oldY, 0, oldY, 12, rowHeight);
      canvas_.pushSprite(0, newY, 0, newY, 12, rowHeight);
    }
    menuTitleFingerprint_ = titleFingerprint;
    menuRowsFingerprint_ = rowsFingerprint;
  } else if (model.screen == core::Screen::BasicView) {
    const DisplayLayout layout =
        layoutFor(BoardConfig::DISPLAY_WIDTH, BoardConfig::DISPLAY_HEIGHT);
    if (lastModel_.finishResult.visible != model.finishResult.visible ||
        lastModel_.competition.manualSubtractActive !=
            model.competition.manualSubtractActive) {
      canvas_.pushSprite(0, 0);
    } else if (model.finishResult.visible) {
      // The finish values are frozen; no periodic transfer is necessary.
    } else {
      const bool tripChanged =
          lastModel_.tripDisplayMode != model.tripDisplayMode ||
          lastModel_.trip1.distanceMillimeters !=
              model.trip1.distanceMillimeters ||
          lastModel_.trip2.distanceMillimeters !=
              model.trip2.distanceMillimeters;
      if (tripChanged) {
        canvas_.pushSprite(layout.trip1.x, layout.trip1.y, layout.trip1.x,
                           layout.trip1.y, layout.trip1.width,
                           layout.trip1.height);
      }
      if (!sameClock(lastModel_.clock, model.clock)) {
        canvas_.pushSprite(layout.clock.x, layout.clock.y, layout.clock.x,
                           layout.clock.y, layout.clock.width,
                           layout.clock.height);
      }
      if (currentSegmentChanged(lastModel_.competition, model.competition))
        pushRect(canvas_, layout.currentSegment);
      if (deltaChanged(lastModel_, model))
        pushRect(canvas_, deltaTransferRect(layout));
      if (nextSegmentChanged(lastModel_.competition, model.competition))
        pushRect(canvas_, layout.nextSegment);
      if (lastModel_.showSpeed != model.showSpeed ||
          (model.showSpeed && lastModel_.speedKmh != model.speedKmh) ||
          lastModel_.competition.undoPromptVisible !=
              model.competition.undoPromptVisible)
        pushRect(canvas_, layout.debugSpeed);
    }
  } else if (model.screen == core::Screen::StartupTimeEntry ||
             model.screen == core::Screen::TimeEdit) {
    const int16_t top = BoardConfig::DISPLAY_WIDTH >= 480 ? 80 : 45;
    const int16_t bottom = BoardConfig::DISPLAY_WIDTH >= 480 ? 210 : 125;
    canvas_.pushSprite(0, top, 0, top, BoardConfig::DISPLAY_WIDTH,
                       bottom - top);
  } else if (model.screen == core::Screen::OrderEdit) {
    if (lastModel_.order.editor.phase != model.order.editor.phase ||
        lastModel_.order.editor.enteringMittisTime !=
            model.order.editor.enteringMittisTime) {
      canvas_.pushSprite(0, 0);
    } else {
      const int16_t top = BoardConfig::DISPLAY_WIDTH >= 480 ? 80 : 40;
      const int16_t bottom = BoardConfig::DISPLAY_HEIGHT - 34;
      canvas_.pushSprite(0, top, 0, top, BoardConfig::DISPLAY_WIDTH,
                         bottom - top);
    }
  } else if (model.screen == core::Screen::Diagnostics) {
    canvas_.pushSprite(0, 0);
  } else {
    const int16_t height = BoardConfig::DISPLAY_HEIGHT > 190
                               ? 190
                               : BoardConfig::DISPLAY_HEIGHT;
    canvas_.pushSprite(0, 0, 0, 0, BoardConfig::DISPLAY_WIDTH, height);
  }
  renderedScreen_ = model.screen;
  lastModel_ = model;
  if (model.screen == core::Screen::Menu) {
    menuTitleFingerprint_ = textFingerprint(model.menu.title);
    menuRowsFingerprint_ = menuRowsFingerprint(model.menu);
  }
  hasRendered_ = true;
}

void DisplayView::showResultView(const core::ResultViewDisplayModel& model) {
  char line1[48]{};
  char line2[48]{};
  const char* title = "TULOKSET";
  if (model.type == core::ResultViewType::StageResults) {
    title = "JAKSOJEN PISTEET";
    if (model.hasStageResult) {
      std::snprintf(line1, sizeof(line1), "JAKSO %u   %" PRIu64,
                    model.stageResult.stageIndex + 1,
                    model.stageResult.points);
      std::snprintf(line2, sizeof(line2), "YHTEENSA %" PRIu64,
                    model.totalPoints);
    } else {
      std::snprintf(line1, sizeof(line1), "EI TULOKSIA");
    }
  } else if (model.type == core::ResultViewType::TotalPoints) {
    title = "KOKONAISPISTEET";
    std::snprintf(line1, sizeof(line1), "%" PRIu64, model.totalPoints);
  } else {
    title = "TAPAHTUMAT";
    if (model.hasEvent) {
      const domain::EventRecord& event = model.event;
      std::snprintf(line1, sizeof(line1), "%s%s",
                    eventTypeName(event.eventType),
                    event.cancelled ? " [PERUTTU]" : "");
      std::snprintf(line2, sizeof(line2), "%02u:%02u:%02u  J%u V%u",
                    event.clockTime.hour, event.clockTime.minute,
                    event.clockTime.second, event.stageIndex + 1,
                    event.segmentIndex + 1);
    } else {
      std::snprintf(line1, sizeof(line1), "EI TAPAHTUMIA");
    }
  }
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(title, BoardConfig::DISPLAY_WIDTH / 2, 8, 2);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextSize(2);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(line1, BoardConfig::DISPLAY_WIDTH / 2, 72, 2);
  if (model.type != core::ResultViewType::StageResults)
    canvas_.setTextSize(1);
  canvas_.drawString(line2, BoardConfig::DISPLAY_WIDTH / 2, 112, 2);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  if (model.itemCount > 1) {
    char position[20];
    std::snprintf(position, sizeof(position), "%u/%u",
                  model.selectedIndex + 1, model.itemCount);
    canvas_.drawString(position, BoardConfig::DISPLAY_WIDTH / 2, 139, 1);
  }
  drawFooter("VASEN: PALAA", "YLOS/ALAS: SELAA", "", textColor_);
}

void DisplayView::showOverrideMenu(
    const core::OverrideMenuDisplayModel& model) {
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString("KILPAILUMUUTOS", BoardConfig::DISPLAY_WIDTH / 2, 10, 2);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextSize(2);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(
      model.selectedAction == core::OverrideMenuAction::AdditionalOrder
          ? "LISAMAARAYS"
          : "TIEKATKO",
      BoardConfig::DISPLAY_WIDTH / 2, 78, 2);
  drawFooter("VASEN: PERU", "YLOS/ALAS: VAIHDA", "OIKEA: AVAA",
             textColor_);
}

void DisplayView::showOverrideEdit(
    const core::OverrideEditDisplayModel& model) {
  char value[48];
  const char* title = model.action == core::OverrideMenuAction::RoadBreak
                          ? "TIEKATKO"
                          : "LISAMAARAYS";
  if (model.phase == core::OverrideEditPhase::StartSegment) {
    std::snprintf(value, sizeof(value), "ALKU %u-%u",
                  model.startSegmentIndex, model.startSegmentIndex + 1);
  } else if (model.phase == core::OverrideEditPhase::Duration) {
    std::snprintf(value, sizeof(value), "AIKA %u:%02u",
                  model.durationSeconds / 60, model.durationSeconds % 60);
  } else {
    std::snprintf(value, sizeof(value), "LOPPU PISTE %u",
                  model.endPointIndex);
  }
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(title, BoardConfig::DISPLAY_WIDTH / 2, 10, 2);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextSize(2);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(value, BoardConfig::DISPLAY_WIDTH / 2, 76, 2);
  canvas_.setTextSize(1);
  if (model.invalid) {
    canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
    canvas_.drawString("VALINTA EI KELPAA", BoardConfig::DISPLAY_WIDTH / 2,
                       122, 1);
  }
  drawFooter("VASEN: PALAA", "YLOS/ALAS: MUUTA", "OIKEA: JATKA",
             textColor_);
}

void DisplayView::showJatResult(const core::JatResultDisplayModel& model) {
  char arrival[12];
  char delta[24];
  std::snprintf(arrival, sizeof(arrival), "%02u:%02u:%02u",
                model.arrivalClockTime.hour, model.arrivalClockTime.minute,
                model.arrivalClockTime.second);
  formatDelta(delta, sizeof(delta), model.finalDeltaSeconds);
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString("JAT TULOAIKA", BoardConfig::DISPLAY_WIDTH / 2, 8, 2);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextSize(3);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(arrival, BoardConfig::DISPLAY_WIDTH / 2, 65, 2);
  canvas_.setTextSize(2);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(delta, BoardConfig::DISPLAY_WIDTH / 2, 118, 2);
  drawFooter("", "", "OIKEA: JATKA", textColor_);
}

void DisplayView::showStartTimeEdit(
    const core::StartTimeEditDisplayModel& model) {
  char value[12];
  std::snprintf(value, sizeof(value), "%02u:%02u:00",
                model.proposedClockTime.hour, model.proposedClockTime.minute);
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString("SEURAAVA LAHTO", BoardConfig::DISPLAY_WIDTH / 2, 10, 2);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextSize(3);
  canvas_.setTextColor(model.valueVisible ? textColor_
                                          : DISPLAY_BACKGROUND_COLOR,
                       DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(value, BoardConfig::DISPLAY_WIDTH / 2, 75, 2);
  drawFooter("", "YLOS/ALAS: +/-1 MIN",
             model.acceptsWithPoint ? "PISTE: HYVAKSY" : "OIKEA: HYVAKSY",
             textColor_);
}

void DisplayView::showMittis(const core::MittisDisplayModel& model) {
  char valueText[48];
  if (model.valid)
    std::snprintf(valueText, sizeof(valueText), "%lu -> %lu",
                  static_cast<unsigned long>(model.oldMillimetersPerPulse),
                  static_cast<unsigned long>(model.proposedMillimetersPerPulse));
  else
    std::snprintf(valueText, sizeof(valueText), "MITTAUS EI KELPAA");
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString("MITTIS KERROIN", BoardConfig::DISPLAY_WIDTH / 2, 10, 2);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextSize(2);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(valueText, BoardConfig::DISPLAY_WIDTH / 2, 72, 2);
  canvas_.setTextSize(1);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(model.saveFailed ? "TALLENNUSVIRHE - VANHA KERROIN"
                                      : (model.valid ? "VAIHDA?" : "EHDOTUS EI KELPAA"),
                     BoardConfig::DISPLAY_WIDTH / 2, 116, 1);
  drawFooter(model.valid ? "VASEN: HYLKAA" : "VASEN: POISTU", "",
             model.valid ? "OIKEA: HYVAKSY" : "", textColor_);
}

void DisplayView::showOrderAccess(const core::OrderAccessDisplayModel& model) {
  const bool edit = model.selectedAction == core::OrderAccessAction::Edit;
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString("AJOMAARAYS", BoardConfig::DISPLAY_WIDTH / 2, 10, 2);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextSize(2);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(edit ? "MUOKKAA AJOMAARAYS" : "UUSI AJOMAARAYS",
                     BoardConfig::DISPLAY_WIDTH / 2, 78, 2);
  drawFooter("VASEN: PALAA", "YLOS/ALAS: VAIHDA", "OIKEA: HYVAKSY",
             textColor_);
}

void DisplayView::showBasicView(const core::DisplayModel& model) {
  if (model.finishResult.visible) {
    showFinishResult(model.finishResult);
    return;
  }
  char clockText[12];
  char speedText[18];
  char trip1Text[24];
  char trip2Text[24];
  char deltaText[24];
  char atText[12];
  char currentRange[16]{};
  char currentValue[16]{};
  const char* currentLabel = "";
  char nextRange[16]{};
  char nextValue[16]{};
  const char* nextLabel = "";
  std::snprintf(clockText, sizeof(clockText), "%02u:%02u:%02u",
                model.clock.hour, model.clock.minute, model.clock.second);
  std::snprintf(speedText, sizeof(speedText), "%.0f km/h", model.speedKmh);
  formatTrip(trip1Text, sizeof(trip1Text), model.trip1.distanceMillimeters);
  formatTrip(trip2Text, sizeof(trip2Text), model.trip2.distanceMillimeters);
  formatDelta(deltaText, sizeof(deltaText), model.competition.deltaSeconds);
  std::snprintf(atText, sizeof(atText), "%02u:%02u:%02u",
                model.atOverlay.clockTime.hour, model.atOverlay.clockTime.minute,
                model.atOverlay.clockTime.second);
  if (model.competition.hasCurrentSegment) {
    formatDriveSegmentRange(currentRange, sizeof(currentRange),
                            model.competition.currentSegment);
    formatSegmentValue(currentValue, sizeof(currentValue),
                       model.competition.currentSegment);
    currentLabel = driveSegmentLabel(model.competition.currentSegment);
  }
  if (model.competition.hasNextSegment) {
    formatDriveSegmentRange(nextRange, sizeof(nextRange),
                            model.competition.nextSegment);
    formatSegmentValue(nextValue, sizeof(nextValue),
                       model.competition.nextSegment);
    nextLabel = driveSegmentLabel(model.competition.nextSegment);
  }
  if (model.competition.timeAdjustmentEditing) {
    nextRange[0] = '\0';
    std::snprintf(nextValue, sizeof(nextValue), "%+ld s",
                  static_cast<long>(
                      model.competition.editedTimeAdjustmentSeconds));
    nextLabel = "AIKA+-";
  }

  const DisplayLayout layout =
      layoutFor(BoardConfig::DISPLAY_WIDTH, BoardConfig::DISPLAY_HEIGHT);
  const auto centerX = [](const WidgetRect& rect) {
    return static_cast<int16_t>(rect.x + rect.width / 2);
  };
  const auto centerY = [](const WidgetRect& rect) {
    return static_cast<int16_t>(rect.y + rect.height / 2);
  };

  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  if (model.tripDisplayMode == domain::TripDisplayMode::BOTH) {
    const int16_t gap = 4;
    const int16_t leftWidth = (layout.trip1.width - gap) / 2;
    const WidgetRect leftTrip = {layout.trip1.x, layout.trip1.y, leftWidth,
                                 layout.trip1.height};
    const WidgetRect rightTrip = {
        static_cast<int16_t>(layout.trip1.x + leftWidth + gap), layout.trip1.y,
        static_cast<int16_t>(layout.trip1.width - leftWidth - gap),
        layout.trip1.height};
    const uint8_t bothScale =
        layout.topFont.value > 1 ? layout.topFont.value - 1 : 1;
    canvas_.setTextSize(bothScale);
    canvas_.drawString(trip1Text, centerX(leftTrip), centerY(leftTrip),
                       layout.topFont.face);
    canvas_.drawString(trip2Text, centerX(rightTrip), centerY(rightTrip),
                       layout.topFont.face);
  } else {
    canvas_.setTextSize(layout.topFont.value);
    canvas_.drawString(
        model.tripDisplayMode == domain::TripDisplayMode::TRIP_2 ? trip2Text
                                                                 : trip1Text,
        centerX(layout.trip1), centerY(layout.trip1), layout.topFont.face);
  }
  canvas_.setTextSize(layout.clockFont.value);
  canvas_.drawString(clockText, centerX(layout.clock), centerY(layout.clock),
                     layout.clockFont.face);

  if (model.competition.state != domain::CompetitionState::IDLE) {
    canvas_.setTextDatum(MC_DATUM);
    canvas_.setTextSize(layout.deltaFont.value);
    canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
    canvas_.drawString(model.atOverlay.visible ? atText : deltaText,
                       centerX(layout.delta), centerY(layout.delta),
                       layout.deltaFont.face);

    canvas_.setTextSize(layout.segmentFont.value);
    canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
    if (currentLabel[0])
      canvas_.drawString(currentLabel, centerX(layout.currentSegment),
                         layout.currentSegment.y + 10,
                         layout.segmentFont.face);
    if (nextLabel[0])
      canvas_.drawString(nextLabel, centerX(layout.nextSegment),
                         layout.nextSegment.y + 10,
                         layout.segmentFont.face);
    canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
    canvas_.drawString(currentRange, centerX(layout.currentSegment),
                       layout.currentSegment.y + layout.currentSegment.height / 3,
                       layout.segmentFont.face);
    canvas_.drawString(nextRange, centerX(layout.nextSegment),
                       layout.nextSegment.y + layout.nextSegment.height / 3,
                       layout.segmentFont.face);

    canvas_.setTextSize(layout.segmentValueFont.value);
    canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
    canvas_.drawString(currentValue, centerX(layout.currentSegment),
                       layout.currentSegment.y +
                           (layout.currentSegment.height * 2) / 3,
                       layout.segmentValueFont.face);
    canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
    canvas_.drawString(nextValue, centerX(layout.nextSegment),
                       layout.nextSegment.y +
                           (layout.nextSegment.height * 2) / 3,
                       layout.segmentValueFont.face);
  }

  if (model.showSpeed) {
    canvas_.setTextDatum(ML_DATUM);
    canvas_.setTextSize(layout.debugFont.value);
    canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
    canvas_.drawString(speedText, layout.debugSpeed.x + 4,
                       centerY(layout.debugSpeed), layout.debugFont.face);
  }
  if (model.competition.undoPromptVisible) {
    canvas_.setTextDatum(MR_DATUM);
    canvas_.setTextSize(layout.debugFont.value);
    canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
    canvas_.drawString("<- = PERU", layout.debugSpeed.right() - 4,
                       centerY(layout.debugSpeed), layout.debugFont.face);
  }
  if (model.competition.manualSubtractActive) {
    const int16_t bannerHeight =
        static_cast<int16_t>(BoardConfig::DISPLAY_HEIGHT * 28 / 100);
    const int16_t bannerY =
        static_cast<int16_t>(BoardConfig::DISPLAY_HEIGHT - bannerHeight);
    canvas_.fillRect(0, bannerY, BoardConfig::DISPLAY_WIDTH, bannerHeight,
                     DISPLAY_RED);
    canvas_.setTextDatum(MC_DATUM);
    canvas_.setTextColor(DISPLAY_WHITE, DISPLAY_RED);
    canvas_.setTextSize(BoardConfig::DISPLAY_WIDTH >= 480 ? 4 : 3);
    canvas_.drawString("MIINUSTUS", BoardConfig::DISPLAY_WIDTH / 2,
                       bannerY + bannerHeight / 2, 2);
  }
}

void DisplayView::showFinishResult(
    const core::FinishResultDisplayModel& model) {
  char clockText[12];
  char pointsText[32];
  std::snprintf(clockText, sizeof(clockText), "%02u:%02u:%02u",
                model.finishClockTime.hour, model.finishClockTime.minute,
                model.finishClockTime.second);
  std::snprintf(pointsText, sizeof(pointsText), "YHTEENSA %" PRIu64,
                model.totalPoints);
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.setTextSize(BoardConfig::DISPLAY_WIDTH >= 480 ? 2 : 1);
  canvas_.drawString("MAALI", BoardConfig::DISPLAY_WIDTH / 2, 8, 2);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextSize(BoardConfig::DISPLAY_WIDTH >= 480 ? 4 : 3);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(clockText, BoardConfig::DISPLAY_WIDTH / 2,
                     BoardConfig::DISPLAY_HEIGHT * 38 / 100, 2);
  canvas_.setTextSize(BoardConfig::DISPLAY_WIDTH >= 480 ? 3 : 2);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(pointsText, BoardConfig::DISPLAY_WIDTH / 2,
                     BoardConfig::DISPLAY_HEIGHT * 65 / 100, 2);
  drawFooter("VASEN: PALAA", "", "OIKEA: PALAA", textColor_);
}

void DisplayView::showTimeEntry(const core::TimeEntryDisplayModel& model) {
  char value[20];
  if (model.activeField == core::TimeField::Hour)
    std::snprintf(value, sizeof(value), "[%02u]:%02u", model.hour,
                  model.minute);
  else
    std::snprintf(value, sizeof(value), "%02u:[%02u]", model.hour,
                  model.minute);
  const int16_t centerX = BoardConfig::DISPLAY_WIDTH / 2;
  const int16_t valueY = BoardConfig::DISPLAY_HEIGHT * 46 / 100;
  const uint8_t valueScale = BoardConfig::DISPLAY_WIDTH >= 480 ? 4 : 3;
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(model.startup ? "ASETA KELLONAIKA" : "MUUTA KELLONAIKA",
                     centerX, 12, 2);
  canvas_.setTextSize(valueScale);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(value, centerX, valueY, 2);
  drawFooter("", "YLOS/ALAS: MUUTA", "OIKEA: JATKA", textColor_);
}

void DisplayView::showMenu(const core::MenuDisplayModel& model) {
  const bool large = BoardConfig::DISPLAY_WIDTH >= 480;
  const uint8_t textScale = menuTextScale(menuFontSize_);
  const int16_t top = large ? 56 : 30;
  const int16_t rowHeight =
      (BoardConfig::DISPLAY_HEIGHT - top - 8) / core::MENU_VISIBLE_ROWS;
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.setTextSize(textScale);
  canvas_.drawString(model.title, BoardConfig::DISPLAY_WIDTH / 2, 5, 2);
  for (uint8_t row = 0; row < model.visibleRowCount; ++row) {
    const bool selected = row == model.selectedVisibleRow;
    const uint16_t background = DISPLAY_BACKGROUND_COLOR;
    canvas_.fillRect(5, top + row * rowHeight,
                     BoardConfig::DISPLAY_WIDTH - 10, rowHeight - 2,
                     background);
    if (selected) {
      canvas_.fillRect(5, top + row * rowHeight + 5, 5, rowHeight - 12,
                       textColor_);
    }
    canvas_.setTextDatum(ML_DATUM);
    canvas_.setTextColor(textColor_, background);
    canvas_.setTextSize(textScale);
    canvas_.drawString(model.rows[row].label, 18,
                       top + row * rowHeight + rowHeight / 2, 2);
    if (!model.rows[row].enabled) {
      canvas_.setTextDatum(MR_DATUM);
      canvas_.drawString("--", BoardConfig::DISPLAY_WIDTH - 13,
                         top + row * rowHeight + rowHeight / 2, 2);
    }
  }
  if (model.scrollOffset > 0) {
    const int16_t x = BoardConfig::DISPLAY_WIDTH - 10;
    canvas_.fillTriangle(x, top + 2, x - 6, top + 10, x + 6, top + 10,
                         textColor_);
  }
  if (model.scrollOffset + model.visibleRowCount < model.totalRows) {
    const int16_t x = BoardConfig::DISPLAY_WIDTH - 10;
    const int16_t y = BoardConfig::DISPLAY_HEIGHT - 4;
    canvas_.fillTriangle(x, y, x - 6, y - 8, x + 6, y - 8, textColor_);
  }
}

void DisplayView::showCalibration(
    const core::CalibrationDisplayModel& model) {
  char valueText[32];
  std::snprintf(valueText, sizeof(valueText), "%lu",
                static_cast<unsigned long>(model.editedMillimetersPerPulse));
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString("MITTARIKERROIN", BoardConfig::DISPLAY_WIDTH / 2, 10, 2);
  canvas_.setTextDatum(MC_DATUM);
  canvas_.setTextSize(2);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(valueText, BoardConfig::DISPLAY_WIDTH / 2, 75, 2);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  if (model.saveFailed)
    canvas_.drawString("TALLENNUSVIRHE", BoardConfig::DISPLAY_WIDTH / 2,
                       118, 2);
  drawFooter("VASEN: PERU", "YLOS/ALAS: +/-1", "OIKEA: OK", textColor_);
}

void DisplayView::showOrder(const core::OrderDisplayModel& model) {
  const route::RouteOrderEditorView& editor = model.editor;
  const char* title = routeOrderEditorTitle(editor);
  const char* primary = "";
  char value[48]{};
  switch (editor.phase) {
    case route::EditorPhase::COMPETITION_TYPE:
      primary = competitionTypeLabel(editor.competitionType);
      break;
    case route::EditorPhase::START_TIME:
      if (editor.startTimeField == 0)
        std::snprintf(value, sizeof(value), "[%02u]:%02u", editor.startHour,
                      editor.startMinute);
      else
        std::snprintf(value, sizeof(value), "%02u:[%02u]", editor.startHour,
                      editor.startMinute);
      primary = value;
      break;
    case route::EditorPhase::SEGMENT_TYPE:
      title = editor.editingExisting ? "MUOKKAA PISTEVALIA" : "MAARAYSTYYPPI";
      primary = editor.segmentType == domain::SegmentType::TIME
                    ? "AIKA"
                    : (editor.segmentType == domain::SegmentType::SPEED
                           ? "NOPEUS"
                           : "MITTIS");
      break;
    case route::EditorPhase::VALUE:
      title = editor.segmentType == domain::SegmentType::TIME
                  ? "AIKA"
                  : (editor.segmentType == domain::SegmentType::SPEED
                         ? "NOPEUS"
                         : (editor.enteringMittisTime ? "MITTIS AIKA"
                                                      : "MITTIS METRIA"));
      formatEditableRouteOrderValue(value, sizeof(value), editor);
      primary = value;
      break;
    case route::EditorPhase::CONTINUATION: {
      static const char* ACTIONS[] = {"SEURAAVA", "JAT", "MAALI"};
      title = "JATKOTOIMINTO";
      primary = ACTIONS[editor.continuation];
      break;
    }
    case route::EditorPhase::JAT_TYPE:
      title = "JAT-TYYPPI";
      switch (editor.jatType) {
        case domain::JatType::MANNED_JAT: primary = "MANNED JAT"; break;
        case domain::JatType::EMIT_JAT_OFFSET: primary = "JAT+AIKA"; break;
        case domain::JatType::EMIT_MLA: primary = "JAT+MLA"; break;
        case domain::JatType::EMIT_ULA: primary = "JAT+ULA"; break;
      }
      break;
    case route::EditorPhase::JAT_OFFSET:
      title = "JAT LISAAIKA";
      std::snprintf(value, sizeof(value), "%+d min", editor.jatOffsetMinutes);
      primary = value;
      break;
    case route::EditorPhase::BROWSE:
      title = "AJOMAARAYS";
      if (model.showsStartTime) {
        std::snprintf(value, sizeof(value), "LAHTO %02u:%02u",
                      editor.startHour, editor.startMinute);
        primary = value;
      } else if (model.hasSelectedSegment) {
        const domain::SegmentDefinition& segment = model.selectedSegment;
        char startPoint[8];
        char endPoint[8];
        if (segment.startPointIndex == 0)
          std::snprintf(startPoint, sizeof(startPoint), "L");
        else if (segment.segmentType == domain::SegmentType::MITTIS)
          std::snprintf(value, sizeof(value), "%s-%s MITTIS %lum %lu:%02lu",
                        startPoint, endPoint,
                        static_cast<unsigned long>(segment.value),
                        static_cast<unsigned long>(
                            segment.mittisDurationSeconds / 60),
                        static_cast<unsigned long>(
                            segment.mittisDurationSeconds % 60));
        else
          std::snprintf(startPoint, sizeof(startPoint), "%u",
                        segment.startPointIndex);
        if (segment.pointTypeAtEnd == domain::PointType::FINISH_M)
          std::snprintf(endPoint, sizeof(endPoint), "M");
        else
          std::snprintf(endPoint, sizeof(endPoint), "%u",
                        segment.endPointIndex);
        const char* type = segment.segmentType == domain::SegmentType::TIME
                               ? "AIKA"
                               : (segment.segmentType == domain::SegmentType::SPEED
                                      ? "NOPEUS"
                                      : "MITTIS");
        if (segment.segmentType == domain::SegmentType::TIME)
          std::snprintf(value, sizeof(value), "%s-%s %s %lu:%02lu",
                        startPoint, endPoint, type,
                        static_cast<unsigned long>(segment.value / 60),
                        static_cast<unsigned long>(segment.value % 60));
        else
          std::snprintf(value, sizeof(value), "%s-%s %s %lu",
                        startPoint, endPoint, type,
                        static_cast<unsigned long>(segment.value));
        primary = value;
      }
      break;
    case route::EditorPhase::CANCEL_PROMPT:
      title = "KESKEYTETAANKO?";
      primary = "VASEN EI   OIKEA KYLLA";
      break;
    case route::EditorPhase::SAVE_PENDING:
      title = "TALLENNETAAN";
      primary = "ODOTA";
      break;
  }
  canvas_.setTextDatum(TC_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.setTextSize(BoardConfig::DISPLAY_WIDTH >= 480 ? 2 : 1);
  canvas_.drawString(title, BoardConfig::DISPLAY_WIDTH / 2, 10, 2);
  canvas_.setTextDatum(MC_DATUM);
  uint8_t primaryScale = BoardConfig::DISPLAY_WIDTH >= 480 ? 3 : 2;
  canvas_.setTextSize(primaryScale);
  while (primaryScale > 1 &&
         canvas_.textWidth(primary, 2) > BoardConfig::DISPLAY_WIDTH - 16) {
    canvas_.setTextSize(--primaryScale);
  }
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.drawString(primary, BoardConfig::DISPLAY_WIDTH / 2,
                     BoardConfig::DISPLAY_HEIGHT * 46 / 100, 2);
  canvas_.setTextSize(BoardConfig::DISPLAY_WIDTH >= 480 ? 2 : 1);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  if (editor.saveFailed) {
    drawFooter("", "TALLENNUSVIRHE", "", textColor_);
  } else if (editor.phase == route::EditorPhase::BROWSE) {
    drawFooter("VASEN: PALAA", "YLOS/ALAS: SELAA", "OIKEA: MUOKKAA",
               textColor_);
  } else if (editor.phase == route::EditorPhase::CANCEL_PROMPT) {
    drawFooter("VASEN: EI", "", "OIKEA: KYLLA", textColor_);
  } else if (editor.phase != route::EditorPhase::SAVE_PENDING) {
    drawFooter("VASEN: TAKAISIN", "YLOS/ALAS: MUUTA", "OIKEA: JATKA",
               textColor_);
  }
}

void DisplayView::showDiagnostics(
    const core::DiagnosticsDisplayModel& model) {
  char line[48];
  const bool large = BoardConfig::DISPLAY_WIDTH >= 480;
  const uint8_t scale = large ? 2 : 1;
  const int16_t lineHeight = large ? 25 : 14;
  int16_t y = large ? 42 : 20;
  canvas_.setTextDatum(TL_DATUM);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.setTextSize(large ? 2 : 1);
  canvas_.drawString("DIAGNOSTIIKKA", 4, 2, 2);
  canvas_.setTextColor(textColor_, DISPLAY_BACKGROUND_COLOR);
  canvas_.setTextSize(scale);
  std::snprintf(line, sizeof(line), "L:%u U:%u D:%u R:%u P:%u",
                model.buttonPressed[0], model.buttonPressed[1],
                model.buttonPressed[2], model.buttonPressed[3],
                model.buttonPressed[5]);
  canvas_.drawString(line, 4, y, 1);
  y += lineHeight;
  std::snprintf(line, sizeof(line), "LAST %u/%u  STATE %u",
                model.lastButtonId, model.lastButtonEventType,
                static_cast<unsigned>(model.currentScreen));
  canvas_.drawString(line, 4, y, 1);
  y += lineHeight;
  std::snprintf(line, sizeof(line), "PULSSIT %" PRIu64 "  T1 %" PRIu64,
                model.totalPulseCount, model.trip1PulseCount);
  canvas_.drawString(line, 4, y, 1);
  y += lineHeight;
  std::snprintf(line, sizeof(line), "AT:%u T2:%u REV:%u P/A:%u/%u",
                model.buttonPressed[6], model.buttonPressed[7],
                model.reverseActive, model.lastPointEventType,
                model.lastAtEventType);
  canvas_.drawString(line, 4, y, 1);
  y += lineHeight;
  std::snprintf(line, sizeof(line), "T1 %" PRId64 " mm", model.trip1DistanceMillimeters);
  canvas_.drawString(line, 4, y, 1);
  y += lineHeight;
  std::snprintf(line, sizeof(line), "T2 %" PRId64 " mm", model.trip2DistanceMillimeters);
  canvas_.drawString(line, 4, y, 1);
  y += lineHeight;
  std::snprintf(line, sizeof(line), "K %lu  V %.1f km/h",
                static_cast<unsigned long>(model.millimetersPerPulse),
                model.speedKmh);
  canvas_.drawString(line, 4, y, 1);
  y += lineHeight;
  std::snprintf(line, sizeof(line), "PULSSI-IKA %lu ms  ZERO %u",
                static_cast<unsigned long>(model.lastPulseAgeMilliseconds),
                model.speedZeroTimedOut);
  canvas_.drawString(line, 4, y, 1);
  y += lineHeight;
  std::snprintf(line, sizeof(line), "KELLO %02u:%02u:%02u  SET %u",
                model.clock.hour, model.clock.minute, model.clock.second,
                model.clockSet);
  canvas_.drawString(line, 4, y, 1);
  y += lineHeight;
  std::snprintf(line, sizeof(line), "KULUNUT %" PRIu64 " ms",
                model.clockElapsedMilliseconds);
  canvas_.drawString(line, 4, y, 1);
  drawFooter("VASEN: PALAA", "", "", textColor_);
}

}  // namespace ui
