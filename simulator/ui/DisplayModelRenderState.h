#pragma once

#include <cstring>

#include "core/DisplayModel.h"

namespace simulator::desktop {
namespace detail {

inline bool sameText(const char* a, const char* b) {
  if (a == b) return true;
  return a && b && std::strcmp(a, b) == 0;
}
inline bool sameClock(const core::ClockTime& a, const core::ClockTime& b) {
  return a.hour == b.hour && a.minute == b.minute && a.second == b.second;
}
inline bool sameEventClock(const domain::EventClockTime& a,
                           const domain::EventClockTime& b) {
  return a.hour == b.hour && a.minute == b.minute && a.second == b.second;
}
inline bool sameSegment(const domain::SegmentDefinition& a,
                        const domain::SegmentDefinition& b) {
  return a.segmentIndex == b.segmentIndex &&
         a.startPointIndex == b.startPointIndex &&
         a.endPointIndex == b.endPointIndex && a.segmentType == b.segmentType &&
         a.value == b.value && a.hasMittisDuration == b.hasMittisDuration &&
         a.mittisDurationSeconds == b.mittisDurationSeconds &&
         a.pointTypeAtEnd == b.pointTypeAtEnd && a.hasJatType == b.hasJatType &&
         a.jatType == b.jatType &&
         a.hasJatOffsetMinutes == b.hasJatOffsetMinutes &&
         a.jatOffsetMinutes == b.jatOffsetMinutes;
}
inline bool sameEditor(const route::RouteOrderEditorView& a,
                       const route::RouteOrderEditorView& b) {
  return a.phase == b.phase && a.editingExisting == b.editingExisting &&
         a.saveFailed == b.saveFailed &&
         a.competitionType == b.competitionType && a.startHour == b.startHour &&
         a.startMinute == b.startMinute &&
         a.startTimeField == b.startTimeField &&
         a.segmentType == b.segmentType && a.value == b.value &&
         a.enteringMittisTime == b.enteringMittisTime &&
         a.digitCursor == b.digitCursor && a.continuation == b.continuation &&
         a.jatType == b.jatType && a.jatOffsetMinutes == b.jatOffsetMinutes &&
         a.selectedSegment == b.selectedSegment &&
         a.segmentCount == b.segmentCount;
}

}  // namespace detail

// Compares exactly the state that can affect pixels for the active REAL screen.
// This avoids hashing padding or pointer values and keeps old-core fallback
// independent from the shared renderer cache.
inline bool sameRenderedDisplayState(const core::DisplayModel& a,
                                     const core::DisplayModel& b) {
  if (a.screen != b.screen || a.textColor != b.textColor ||
      a.showLabels != b.showLabels || a.menuFontSize != b.menuFontSize)
    return false;

  switch (a.screen) {
    case core::Screen::StartupTimeEntry:
    case core::Screen::TimeEdit:
      return a.timeEntry.hour == b.timeEntry.hour &&
             a.timeEntry.minute == b.timeEntry.minute &&
             a.timeEntry.activeField == b.timeEntry.activeField &&
             a.timeEntry.startup == b.timeEntry.startup;
    case core::Screen::Menu:
      if (!detail::sameText(a.menu.title, b.menu.title) ||
          a.menu.visibleRowCount != b.menu.visibleRowCount ||
          a.menu.selectedVisibleRow != b.menu.selectedVisibleRow ||
          a.menu.selectedIndex != b.menu.selectedIndex ||
          a.menu.totalRows != b.menu.totalRows ||
          a.menu.scrollOffset != b.menu.scrollOffset)
        return false;
      for (uint8_t row = 0; row < a.menu.visibleRowCount; ++row)
        if (!detail::sameText(a.menu.rows[row].label, b.menu.rows[row].label) ||
            a.menu.rows[row].enabled != b.menu.rows[row].enabled)
          return false;
      return true;
    case core::Screen::BasicView:
      return detail::sameClock(a.clock, b.clock) && a.speedKmh == b.speedKmh &&
             a.showSpeed == b.showSpeed &&
             a.tripDisplayMode == b.tripDisplayMode &&
             a.trip1.distanceMillimeters == b.trip1.distanceMillimeters &&
             a.trip2.distanceMillimeters == b.trip2.distanceMillimeters &&
             a.competition.state == b.competition.state &&
             a.competition.deltaSeconds == b.competition.deltaSeconds &&
             a.competition.hasCurrentSegment == b.competition.hasCurrentSegment &&
             (!a.competition.hasCurrentSegment ||
              detail::sameSegment(a.competition.currentSegment,
                                  b.competition.currentSegment)) &&
             a.competition.hasNextSegment == b.competition.hasNextSegment &&
             (!a.competition.hasNextSegment ||
              detail::sameSegment(a.competition.nextSegment,
                                  b.competition.nextSegment)) &&
             a.competition.undoPromptVisible ==
                 b.competition.undoPromptVisible &&
             a.competition.manualSubtractActive ==
                 b.competition.manualSubtractActive &&
             a.competition.timeAdjustmentEditing ==
                 b.competition.timeAdjustmentEditing &&
             a.competition.editedTimeAdjustmentSeconds ==
                 b.competition.editedTimeAdjustmentSeconds &&
             a.atOverlay.visible == b.atOverlay.visible &&
             (!a.atOverlay.visible ||
              detail::sameEventClock(a.atOverlay.clockTime,
                                     b.atOverlay.clockTime)) &&
             a.finishResult.visible == b.finishResult.visible &&
             (!a.finishResult.visible ||
              (detail::sameEventClock(a.finishResult.finishClockTime,
                                      b.finishResult.finishClockTime) &&
               a.finishResult.totalPoints == b.finishResult.totalPoints));
    case core::Screen::CalibrationEdit:
      return a.calibration.editedMillimetersPerPulse ==
                 b.calibration.editedMillimetersPerPulse &&
             a.calibration.saveFailed == b.calibration.saveFailed;
    case core::Screen::MittisProposal:
      return a.mittis.oldMillimetersPerPulse == b.mittis.oldMillimetersPerPulse &&
             a.mittis.proposedMillimetersPerPulse ==
                 b.mittis.proposedMillimetersPerPulse &&
             a.mittis.valid == b.mittis.valid &&
             a.mittis.saveFailed == b.mittis.saveFailed;
    case core::Screen::JatResult:
      return detail::sameEventClock(a.jatResult.arrivalClockTime,
                                    b.jatResult.arrivalClockTime) &&
             a.jatResult.finalDeltaSeconds == b.jatResult.finalDeltaSeconds;
    case core::Screen::StartTimeEdit:
      return detail::sameEventClock(a.startTimeEdit.proposedClockTime,
                                    b.startTimeEdit.proposedClockTime) &&
             a.startTimeEdit.jatType == b.startTimeEdit.jatType &&
             a.startTimeEdit.valueVisible == b.startTimeEdit.valueVisible &&
             a.startTimeEdit.acceptsWithPoint == b.startTimeEdit.acceptsWithPoint;
    case core::Screen::OverrideMenu:
      return a.overrideMenu.selectedAction == b.overrideMenu.selectedAction;
    case core::Screen::OverrideEdit:
      return a.overrideEdit.action == b.overrideEdit.action &&
             a.overrideEdit.phase == b.overrideEdit.phase &&
             a.overrideEdit.startSegmentIndex == b.overrideEdit.startSegmentIndex &&
             a.overrideEdit.endPointIndex == b.overrideEdit.endPointIndex &&
             a.overrideEdit.durationSeconds == b.overrideEdit.durationSeconds &&
             a.overrideEdit.invalid == b.overrideEdit.invalid;
    case core::Screen::ResultView:
      return a.resultView.type == b.resultView.type &&
             a.resultView.selectedIndex == b.resultView.selectedIndex &&
             a.resultView.itemCount == b.resultView.itemCount &&
             a.resultView.hasStageResult == b.resultView.hasStageResult &&
             (!a.resultView.hasStageResult ||
              (a.resultView.stageResult.stageIndex ==
                   b.resultView.stageResult.stageIndex &&
               a.resultView.stageResult.points == b.resultView.stageResult.points)) &&
             a.resultView.totalPoints == b.resultView.totalPoints &&
             a.resultView.hasEvent == b.resultView.hasEvent &&
             (!a.resultView.hasEvent ||
              (a.resultView.event.eventType == b.resultView.event.eventType &&
               detail::sameEventClock(a.resultView.event.clockTime,
                                      b.resultView.event.clockTime) &&
               a.resultView.event.stageIndex == b.resultView.event.stageIndex &&
               a.resultView.event.segmentIndex == b.resultView.event.segmentIndex &&
               a.resultView.event.cancelled == b.resultView.event.cancelled));
    case core::Screen::OrderAccessPrompt:
      return a.orderAccess.selectedAction == b.orderAccess.selectedAction;
    case core::Screen::OrderEdit:
      return detail::sameEditor(a.order.editor, b.order.editor) &&
             a.order.hasSelectedSegment == b.order.hasSelectedSegment &&
             a.order.showsStartTime == b.order.showsStartTime &&
             (!a.order.hasSelectedSegment ||
              detail::sameSegment(a.order.selectedSegment,
                                  b.order.selectedSegment));
    case core::Screen::Diagnostics:
      if (a.diagnostics.currentScreen != b.diagnostics.currentScreen ||
          a.diagnostics.lastButtonId != b.diagnostics.lastButtonId ||
          a.diagnostics.lastButtonEventType !=
              b.diagnostics.lastButtonEventType ||
          a.diagnostics.totalPulseCount != b.diagnostics.totalPulseCount ||
          a.diagnostics.trip1PulseCount != b.diagnostics.trip1PulseCount ||
          a.diagnostics.trip2PulseCount != b.diagnostics.trip2PulseCount ||
          a.diagnostics.trip1DistanceMillimeters !=
              b.diagnostics.trip1DistanceMillimeters ||
          a.diagnostics.trip2DistanceMillimeters !=
              b.diagnostics.trip2DistanceMillimeters ||
          a.diagnostics.millimetersPerPulse !=
              b.diagnostics.millimetersPerPulse ||
          a.diagnostics.speedKmh != b.diagnostics.speedKmh ||
          a.diagnostics.lastPulseAgeMilliseconds !=
              b.diagnostics.lastPulseAgeMilliseconds ||
          a.diagnostics.speedZeroTimedOut !=
              b.diagnostics.speedZeroTimedOut ||
          a.diagnostics.clockSet != b.diagnostics.clockSet ||
          !detail::sameClock(a.diagnostics.clock, b.diagnostics.clock) ||
          a.diagnostics.reverseActive != b.diagnostics.reverseActive)
        return false;
      for (size_t button = 0; button < 8; ++button)
        if (a.diagnostics.buttonPressed[button] !=
            b.diagnostics.buttonPressed[button])
          return false;
      return true;
  }
  return false;
}

}  // namespace simulator::desktop
