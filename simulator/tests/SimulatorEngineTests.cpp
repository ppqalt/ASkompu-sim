#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "engine/SimTimeSource.h"
#include "engine/SimulatorEngine.h"
#include "engine/WheelPulseGenerator.h"
#include "route/RouteOrderCodec.h"

namespace {
using simulator::Command;
using simulator::InitialState;
using simulator::SimulatorEngine;
using core::ButtonId;
using core::ButtonEventType;

void require(bool ok, int line) {
  if (!ok) throw std::runtime_error("Tarkistus epäonnistui rivillä " + std::to_string(line));
}
#define CHECK(condition) require((condition), __LINE__)

template <typename Exception, typename F>
void rejects(F action) {
  bool rejected = false;
  try { action(); } catch (const Exception&) { rejected = true; }
  CHECK(rejected);
}

void acceptTime(SimulatorEngine& engine, unsigned hour = 0, unsigned minute = 0) {
  for (unsigned i = 0; i < hour; ++i) engine.press(ButtonId::Up);
  engine.press(ButtonId::Right);
  for (unsigned i = 0; i < minute; ++i) engine.press(ButtonId::Up);
  engine.press(ButtonId::Right);
}

domain::SegmentDefinition segment(unsigned index, domain::SegmentType type,
                                  uint32_t value, domain::PointType end) {
  domain::SegmentDefinition result;
  result.segmentIndex = static_cast<uint16_t>(index);
  result.startPointIndex = static_cast<uint16_t>(index);
  result.endPointIndex = static_cast<uint16_t>(index + 1);
  result.segmentType = type;
  result.value = value;
  result.pointTypeAtEnd = end;
  return result;
}

InitialState routeState(domain::SegmentType first = domain::SegmentType::SPEED) {
  InitialState initial;
  initial.routeActive = true;
  initial.routeOrder.competitionType = domain::CompetitionType::NON_EMIT;
  initial.routeOrder.segments = {
      segment(0, first, first == domain::SegmentType::MITTIS ? 1000 : 36,
              domain::PointType::NORMAL),
      segment(1, domain::SegmentType::TIME, 20, domain::PointType::FINISH_M)};
  if (first == domain::SegmentType::MITTIS) {
    initial.routeOrder.segments[0].hasMittisDuration = true;
    initial.routeOrder.segments[0].mittisDurationSeconds = 100;
  }
  return initial;
}

void openCalibration(SimulatorEngine& engine) {
  engine.press(ButtonId::Down);
  engine.press(ButtonId::Down);
  engine.press(ButtonId::Down);
  engine.press(ButtonId::Right);
  CHECK(engine.application().screen() == core::Screen::CalibrationEdit);
}

// Kenttäkohtainen vertailu: ei muistitäytettä, osoitteita eikä raakaa memcmp:tä.
struct Snapshot {
  std::ostringstream out;
  Snapshot() { out << std::setprecision(std::numeric_limits<double>::max_digits10); }
  template <typename T> void value(T v) { out << +v << ';'; }
  template <typename T> void enumeration(T v) { value(static_cast<unsigned>(v)); }
  void text(const char* v) { out << (v ? v : "") << ';'; }
  template <typename T> void clock(T v) { value(v.hour); value(v.minute); value(v.second); }
  void stage(const domain::StageResult& v) {
    value(v.stageIndex); enumeration(v.endPointType); clock(v.endClockTime);
    value(v.finalDeltaSeconds); value(v.lateSeconds); value(v.earlySeconds); value(v.points);
  }
  void segment(const domain::SegmentDefinition& v) {
    value(v.segmentIndex); value(v.startPointIndex); value(v.endPointIndex);
    enumeration(v.segmentType); value(v.value); value(v.hasMittisDuration);
    value(v.mittisDurationSeconds); enumeration(v.pointTypeAtEnd); value(v.hasJatType);
    enumeration(v.jatType); value(v.hasJatOffsetMinutes); value(v.jatOffsetMinutes);
  }
  void overrideValue(const domain::SegmentOverride& v) {
    enumeration(v.overrideType); value(v.startSegmentIndex); value(v.endPointIndex);
    value(v.replacementDurationSeconds); clock(v.acceptedClockTime);
    value(v.originalDefinitions.size());
    for (const auto& s : v.originalDefinitions) segment(s);
  }
  void event(const domain::EventRecord& v) {
    value(v.eventId); enumeration(v.eventType); clock(v.clockTime); value(v.monotonicTimeMs);
    value(v.stageIndex); value(v.segmentIndex); value(v.competitionTimeMs);
    value(v.tIdealMs); value(v.deltaMs); value(v.physicalDistanceMillimeters);
    value(v.stageDistanceMillimeters); value(v.segmentDistanceMillimeters);
    value(v.trip1DistanceMillimeters); value(v.trip2DistanceMillimeters);
    value(v.speedKmhMilli); value(v.reverseActive); value(v.manualSubtractActive);
    value(v.cancelled);
    const auto& p = v.payload;
    value(p.referencedEventId); value(p.hasStageResult); stage(p.stageResult);
    value(p.hasJatType); enumeration(p.jatType); clock(p.arrivalClockTime);
    value(p.finalDeltaMs); clock(p.proposedStartClockTime); clock(p.acceptedStartClockTime);
    value(p.oldCalibration); value(p.proposedCalibration); enumeration(p.tripChannel);
    value(p.reverseActive); value(p.manualSubtractActive); value(p.timeAdjustmentSeconds);
    value(p.hasOverride); overrideValue(p.segmentOverride);
  }
};

std::string snapshot(const SimulatorEngine& e) {
  Snapshot s;
  s.value(e.monotonicMicroseconds()); s.value(e.targetSpeedMilliKmh());
  s.value(e.reverseActive()); s.value(e.generatedPulseCount());
  s.value(e.lastPulseMicroseconds()); s.value(e.previousPulseMicroseconds());
  s.value(e.clock().isSet()); s.value(e.clock().elapsedSinceSetMilliseconds());
  const auto& a = e.application();
  s.value(a.trip1DistanceMillimeters()); s.value(a.trip2DistanceMillimeters());
  s.value(a.totalPulseCount()); s.value(a.millimetersPerPulse());
  s.value(a.manualSubtractActive()); s.enumeration(a.menuPage());
  s.value(a.menuSelectedIndex()); s.value(a.menuScrollOffset());
  const auto& c = a.competition();
  s.enumeration(c.state()); s.value(c.currentSegmentIndex()); s.value(c.stageIndex());
  s.value(c.realTimeMs()); s.value(c.idealTimeMs()); s.value(c.deltaMs());
  s.value(c.physicalDistanceMillimeters()); s.value(c.stageDistanceMillimeters());
  s.value(c.segmentDistanceMillimeters()); s.value(c.reverseActive());
  s.value(c.deltaFrozen()); s.value(c.totalPoints()); s.value(c.finalDeltaMs());
  s.clock(c.arrivalClockTime()); s.clock(c.proposedStartClockTime());
  s.enumeration(c.pendingJatType()); s.value(c.stageDistanceEnabled());
  s.value(c.correctingStartTime());
  s.value(c.mittisProposal().pending);
  s.value(c.activeOverride() != nullptr);
  if (c.activeOverride()) s.overrideValue(*c.activeOverride());
  s.value(c.stageResults().size());
  for (const auto& result : c.stageResults()) s.stage(result);
  s.value(a.hasRouteOrder());
  if (a.currentRouteOrder()) {
    std::vector<uint8_t> bytes;
    CHECK(route::RouteOrderCodec::encode(*a.currentRouteOrder(), bytes));
    for (auto b : bytes) s.value(b);
  }
  s.value(a.eventRepository().count());
  for (size_t i = 0; i < a.eventRepository().count(); ++i) s.event(*a.eventRepository().at(i));
  const auto m = e.displayModel();
  s.enumeration(m.screen); s.enumeration(m.textColor); s.value(m.backlightPercent);
  s.value(m.showLabels); s.enumeration(m.menuFontSize); s.enumeration(m.tripDisplayMode);
  s.clock(m.clock); s.value(m.speedKmh); s.value(m.showSpeed);
  s.value(m.trip1.distanceMillimeters); s.value(m.trip1.visible);
  s.value(m.trip2.distanceMillimeters); s.value(m.trip2.visible);
  s.value(m.timeEntry.hour); s.value(m.timeEntry.minute);
  s.enumeration(m.timeEntry.activeField); s.value(m.timeEntry.startup);
  s.value(m.calibration.editedMillimetersPerPulse); s.value(m.calibration.saveFailed);
  s.value(m.mittis.oldMillimetersPerPulse); s.value(m.mittis.proposedMillimetersPerPulse);
  s.value(m.mittis.measuredDistanceMillimeters); s.value(m.mittis.referenceDistanceMeters);
  s.value(m.mittis.valid); s.value(m.mittis.saveFailed);
  s.value(m.atOverlay.visible); s.clock(m.atOverlay.clockTime);
  s.value(m.atOverlay.waitingForStop); s.value(m.atOverlay.travelledDistanceMillimeters);
  s.value(m.atOverlay.targetDistanceMeters); s.clock(m.jatResult.arrivalClockTime);
  s.value(m.jatResult.finalDeltaSeconds); s.value(m.finishResult.visible);
  s.clock(m.finishResult.finishClockTime); s.value(m.finishResult.totalPoints);
  s.clock(m.startTimeEdit.proposedClockTime); s.enumeration(m.startTimeEdit.jatType);
  s.value(m.startTimeEdit.valueVisible); s.value(m.startTimeEdit.acceptsWithPoint);
  s.enumeration(m.overrideMenu.selectedAction); s.enumeration(m.overrideEdit.action);
  s.enumeration(m.overrideEdit.phase); s.value(m.overrideEdit.startSegmentIndex);
  s.value(m.overrideEdit.endPointIndex); s.value(m.overrideEdit.maximumEndPointIndex);
  s.value(m.overrideEdit.durationSeconds); s.value(m.overrideEdit.invalid);
  s.enumeration(m.resultView.type); s.value(m.resultView.selectedIndex);
  s.value(m.resultView.itemCount); s.value(m.resultView.hasStageResult);
  s.stage(m.resultView.stageResult); s.value(m.resultView.totalPoints);
  s.value(m.resultView.hasEvent); s.event(m.resultView.event);
  s.enumeration(m.orderAccess.selectedAction);
  s.segment(m.order.selectedSegment); s.value(m.order.hasSelectedSegment);
  s.value(m.order.showsStartTime);
  const auto& editor = m.order.editor;
  s.enumeration(editor.phase); s.value(editor.editingExisting); s.value(editor.saveFailed);
  s.enumeration(editor.competitionType); s.value(editor.startHour); s.value(editor.startMinute);
  s.value(editor.startTimeField); s.enumeration(editor.segmentType); s.value(editor.value);
  s.value(editor.enteringMittisTime); s.value(editor.digitCursor); s.value(editor.continuation);
  s.enumeration(editor.jatType); s.value(editor.jatOffsetMinutes);
  s.value(editor.selectedSegment); s.value(editor.segmentCount);
  s.enumeration(m.competition.state); s.value(m.competition.deltaSeconds);
  s.value(m.competition.deltaFrozen); s.segment(m.competition.currentSegment);
  s.value(m.competition.hasCurrentSegment); s.segment(m.competition.nextSegment);
  s.value(m.competition.hasNextSegment); s.value(m.competition.undoPromptVisible);
  s.value(m.competition.manualSubtractActive); s.value(m.competition.timeAdjustmentEditing);
  s.value(m.competition.editedTimeAdjustmentSeconds);
  s.text(m.menu.title); s.value(m.menu.visibleRowCount); s.value(m.menu.selectedVisibleRow);
  s.value(m.menu.selectedIndex); s.value(m.menu.totalRows); s.value(m.menu.scrollOffset);
  for (const auto& row : m.menu.rows) { s.text(row.label); s.value(row.enabled); }
  const auto& d = m.diagnostics;
  s.enumeration(d.currentScreen);
  for (bool pressed : d.buttonPressed) s.value(pressed);
  s.value(d.lastButtonId); s.value(d.lastButtonEventType); s.value(d.totalPulseCount);
  s.value(d.trip1PulseCount); s.value(d.trip2PulseCount);
  s.value(d.trip1DistanceMillimeters); s.value(d.trip2DistanceMillimeters);
  s.value(d.millimetersPerPulse); s.value(d.speedKmh); s.value(d.lastPulseAgeMilliseconds);
  s.value(d.speedZeroTimedOut); s.value(d.clockSet); s.clock(d.clock);
  s.value(d.clockElapsedMilliseconds); s.value(d.lastPointEventType);
  s.value(d.lastAtEventType); s.value(d.reverseActive);
  return s.out.str();
}

void testTime() {
  simulator::SimTimeSource source;
  core::SoftwareClock clock(source);
  CHECK(source.microseconds() == 0);
  source.advanceMicroseconds(999);
  CHECK(source.monotonicMilliseconds() == 0);
  source.advanceMicroseconds(1);
  CHECK(source.monotonicMilliseconds() == 1);
  clock.set(23, 59, 59);
  source.advanceMicroseconds(1000000);
  CHECK(clock.now().hour == 0 && clock.now().minute == 0 && clock.now().second == 0);
  CHECK(clock.elapsedSinceSetMilliseconds() == 1000);
  // uint32_t-millis-rajapinnan kierto, kelloa palvellaan ennen täyttä kierrosta.
  source.advanceMicroseconds((uint64_t{1} << 32) * 1000 - 2000000);
  CHECK(clock.elapsedSinceSetMilliseconds() == (uint64_t{1} << 32) - 1000);
  source.advanceMicroseconds(2000000);
  CHECK(clock.elapsedSinceSetMilliseconds() == (uint64_t{1} << 32) + 1000);
  CHECK(source.monotonicMilliseconds() == 1001);
}

void testClockAndStopped() {
  SimulatorEngine e;
  e.advanceMilliseconds(2500);
  CHECK(!e.clock().isSet());
  acceptTime(e, 12, 34);
  e.advanceMilliseconds(60000);
  CHECK(e.monotonicMicroseconds() == 62500000);
  CHECK(e.clock().elapsedSinceSetMilliseconds() == 60000);
  CHECK(e.displayModel().clock.hour == 12 && e.displayModel().clock.minute == 35);
  CHECK(e.generatedPulseCount() == 0 && e.application().trip1DistanceMillimeters() == 0);
  CHECK(e.displayModel().speedKmh == 0);
}

void checkInterval(uint32_t speed, uint64_t interval) {
  SimulatorEngine e;
  e.setSpeedMilliKmh(speed);
  e.advanceMicroseconds(interval - 1);
  CHECK(e.generatedPulseCount() == 0);
  e.advanceMicroseconds(1);
  CHECK(e.generatedPulseCount() == 1 && e.lastPulseMicroseconds() == interval);
  e.advanceMicroseconds(interval);
  CHECK(e.generatedPulseCount() == 2);
  CHECK(e.lastPulseMicroseconds() - e.previousPulseMicroseconds() == interval);
  CHECK(std::abs(e.displayModel().speedKmh - speed / 1000.0) < 0.001);
}

void testCalibrations() {
  for (uint32_t calibration : {1U, 997U, 1000U, 100000U}) {
    InitialState initial;
    initial.millimetersPerPulse = calibration;
    SimulatorEngine e(initial);
    e.setSpeedKmh(36);
    e.advanceMilliseconds(1000);
    CHECK(e.generatedPulseCount() == 10000 / calibration);
    CHECK(e.application().trip1DistanceMillimeters() ==
          static_cast<int64_t>(e.generatedPulseCount() * calibration));
  }
}

void testRemainderAndStop() {
  SimulatorEngine e;
  e.setSpeedKmh(36);
  e.advanceMicroseconds(50000);
  e.stop();
  e.advanceMilliseconds(1000);
  CHECK(e.generatedPulseCount() == 0);
  e.setSpeedKmh(60);
  e.advanceMicroseconds(29999);
  CHECK(e.generatedPulseCount() == 0);
  e.advanceMicroseconds(1);
  CHECK(e.generatedPulseCount() == 1);
  CHECK(e.lastPulseMicroseconds() == 1080000);
  e.advanceMilliseconds(200);
  e.stop();
  e.advanceMilliseconds(1000);
  CHECK(e.displayModel().speedKmh == 0);
}

void testLongRun() {
  InitialState initial;
  initial.millimetersPerPulse = 997;
  SimulatorEngine e(initial);
  acceptTime(e);
  e.setSpeedMilliKmh(73123);
  const uint64_t duration = 3600123456ULL;
  e.advanceMicroseconds(duration);
  const uint64_t expected = duration * 73123ULL / (997ULL * 3600000ULL);
  CHECK(e.generatedPulseCount() == expected);
  CHECK(e.application().trip1DistanceMillimeters() == static_cast<int64_t>(expected * 997));
  const uint64_t idealLastUs = (expected * 997ULL * 3600000ULL + 73122) / 73123;
  CHECK(e.lastPulseMicroseconds() == idealLastUs);
  CHECK(e.clock().elapsedSinceSetMilliseconds() == duration / 1000);
}

void testReverse() {
  SimulatorEngine e(routeState());
  acceptTime(e);
  e.setSpeedKmh(36);
  e.advanceMilliseconds(1000);
  e.setReverse(true);
  e.advanceMilliseconds(1200);
  CHECK(e.application().trip1DistanceMillimeters() == -2000);
  CHECK(e.application().trip2DistanceMillimeters() == -2000);
  CHECK(e.application().competition().physicalDistanceMillimeters() == -2000);
  CHECK(e.application().competition().reverseActive());
  e.stop(); e.setReverse(false); e.setSpeedKmh(36); e.advanceMilliseconds(500);
  CHECK(e.application().trip1DistanceMillimeters() == 3000);
  CHECK(e.generatedPulseCount() == 27);
  const auto& events = e.application().eventRepository();
  CHECK(events.at(events.count() - 1)->eventType == domain::DomainEventType::REVERSE_CHANGED);
  CHECK(!events.at(events.count() - 1)->payload.reverseActive);
}

void testButtonsAndDisplay() {
  InitialState initial;
  initial.displaySettings.externalResetTarget = domain::TripResetTarget::TRIP_2;
  SimulatorEngine e(initial);
  e.press(ButtonId::Up);
  CHECK(e.displayModel().timeEntry.hour == 1);
  e.press(ButtonId::Right); e.press(ButtonId::Down); e.press(ButtonId::Right);
  CHECK(e.displayModel().screen == core::Screen::BasicView);
  CHECK(e.clock().now().hour == 1 && e.clock().now().minute == 59);
  e.setSpeedKmh(36); e.advanceMilliseconds(1000);
  CHECK(e.displayModel().trip1.distanceMillimeters == 10000);
  e.press(ButtonId::Trip2Reset);
  CHECK(e.application().trip2DistanceMillimeters() == 0);
  CHECK(e.application().trip1DistanceMillimeters() == 10000);
  e.buttonEvent(ButtonId::Trip1Reset, ButtonEventType::Press);
  e.advanceMilliseconds(1000);
  CHECK(e.application().trip1DistanceMillimeters() == 0);
  e.buttonEvent(ButtonId::Trip1Reset, ButtonEventType::Release);
  e.advanceMilliseconds(100);
  CHECK(e.application().trip1DistanceMillimeters() == 1000);
  e.press(ButtonId::FootReset);
  CHECK(e.application().trip1DistanceMillimeters() == 1000);
  CHECK(e.application().trip2DistanceMillimeters() == 0);
  e.press(ButtonId::Down);
  CHECK(e.displayModel().screen == core::Screen::Menu);
  CHECK(std::string(e.displayModel().menu.rows[0].label) == "AJOMAARAYS");
  e.buttonEvent(ButtonId::Left, ButtonEventType::LongStart);
  CHECK(e.application().screen() == core::Screen::BasicView);
  e.press(ButtonId::At);
  CHECK(e.displayModel().atOverlay.visible);
  e.press(ButtonId::At);
  CHECK(!e.displayModel().atOverlay.visible);
  CHECK(e.application().eventRepository().at(e.application().eventRepository().count()-1)
            ->eventType == domain::DomainEventType::AT_CANCELLED);
}

void testCalibrationSave() {
  SimulatorEngine e;
  acceptTime(e);
  e.setSpeedKmh(36); e.advanceMicroseconds(50000);
  openCalibration(e);
  e.press(ButtonId::Up);
  CHECK(e.application().millimetersPerPulse() == 1000);
  e.press(ButtonId::Right);
  CHECK(e.application().millimetersPerPulse() == 1001);
  // Puoli kierrosta säilyy: seuraava puolikas kestää nyt 50050 µs.
  e.advanceMicroseconds(50049); CHECK(e.generatedPulseCount() == 0);
  e.advanceMicroseconds(1); CHECK(e.generatedPulseCount() == 1);
  CHECK(e.application().trip1DistanceMillimeters() == 1001);
  e.press(ButtonId::Left);
  openCalibration(e); e.press(ButtonId::Up);
  e.setSavesSucceed(false); e.press(ButtonId::Right);
  CHECK(e.displayModel().calibration.saveFailed);
  CHECK(e.application().millimetersPerPulse() == 1001);
  e.setSavesSucceed(true); e.press(ButtonId::Right);
  CHECK(e.application().millimetersPerPulse() == 1002);
  CHECK(e.application().trip1DistanceMillimeters() == 1001);
}

void testMittis() {
  SimulatorEngine e(routeState(domain::SegmentType::MITTIS));
  acceptTime(e);
  e.setSpeedKmh(36); e.advanceMilliseconds(9999);
  CHECK(e.displayModel().trip1.distanceMillimeters == 0);
  CHECK(e.application().trip1DistanceMillimeters() == 99000);
  e.advanceMilliseconds(1);
  CHECK(e.displayModel().trip1.distanceMillimeters == 100000);
  e.advanceMilliseconds(40000);
  e.press(ButtonId::Point);
  CHECK(e.application().screen() == core::Screen::MittisProposal);
  CHECK(e.displayModel().mittis.proposedMillimetersPerPulse == 2000);
  e.setSavesSucceed(false); e.press(ButtonId::Right);
  CHECK(e.displayModel().mittis.saveFailed && e.application().millimetersPerPulse() == 1000);
  e.setSavesSucceed(true); e.press(ButtonId::Right);
  CHECK(e.application().millimetersPerPulse() == 2000);
  CHECK(e.application().competition().currentSegmentIndex() == 1);
  e.advanceMilliseconds(200);
  CHECK(e.generatedPulseCount() == 501);
  CHECK(e.application().trip1DistanceMillimeters() == 502000);
}

void testJatFinishAndScore() {
  auto initial = routeState(domain::SegmentType::TIME);
  auto& first = initial.routeOrder.segments[0];
  first.value = 10; first.pointTypeAtEnd = domain::PointType::JAT;
  first.hasJatType = true; first.jatType = domain::JatType::MANNED_JAT;
  first.hasJatOffsetMinutes = true;
  SimulatorEngine e(initial);
  acceptTime(e);
  e.advanceMilliseconds(12000); e.press(ButtonId::Point);
  CHECK(e.application().screen() == core::Screen::JatResult);
  CHECK(e.application().competition().totalPoints() == 2);
  e.advanceMilliseconds(4999);
  CHECK(e.application().screen() == core::Screen::JatResult);
  e.step(); CHECK(e.application().screen() == core::Screen::StartTimeEdit);
  e.press(ButtonId::Right);
  CHECK(e.application().competition().state() == domain::CompetitionState::WAIT_START);
  e.advanceMilliseconds(43000);
  CHECK(e.application().competition().state() == domain::CompetitionState::RUNNING);
  e.advanceMilliseconds(20000); e.press(ButtonId::Point);
  CHECK(e.displayModel().finishResult.visible);
  CHECK(e.displayModel().finishResult.totalPoints == 2);
  CHECK(e.application().competition().stageResults().size() == 2);
  const auto points = e.application().competition().totalPoints();
  e.setSpeedKmh(36); e.advanceMilliseconds(1000);
  CHECK(e.application().competition().totalPoints() == points);
  CHECK(e.application().competition().segmentDistanceMillimeters() == 0);
}

void addTimeline(SimulatorEngine& e) {
  acceptTime(e);
  e.schedule(1, Command::speed(73123));
  e.schedule(99999, Command::click(ButtonId::At));
  e.schedule(2112345, Command::reverseState(true));
  e.schedule(2112345, Command::speed(36000));
  e.schedule(3000000, Command::reverseState(false));
  e.schedule(3123456, Command::click(ButtonId::Point));
  e.schedule(3456789, Command::click(ButtonId::Left));
  e.schedule(4000000, Command::speed(0));
  e.schedule(4000001, Command::click(ButtonId::At));
  e.schedule(5500001, Command::speed(60000));
  e.schedule(7000000, Command::click(ButtonId::Trip2Reset));
  e.schedule(9000000, Command::click(ButtonId::Point));
  e.schedule(12000000, Command::click(ButtonId::Point));
}

void testReplayAndPartition() {
  SimulatorEngine large(routeState()), small(routeState()), repeated(routeState());
  addTimeline(large); addTimeline(small); addTimeline(repeated);
  large.advanceMicroseconds(12555555);
  repeated.advanceMicroseconds(12555555);
  uint32_t random = 7;
  while (small.monotonicMicroseconds() < 12555555) {
    random = random * 1664525U + 1013904223U;
    const uint64_t step = 1 + random % 17003;
    small.advanceMicroseconds(std::min(step, 12555555 - small.monotonicMicroseconds()));
    (void)small.displayModel();  // Lukutiheys ei muuta lopputulosta.
  }
  CHECK(snapshot(large) == snapshot(small));
  CHECK(snapshot(large) == snapshot(repeated));
}

void testSameTimestamp() {
  SimulatorEngine e(routeState());
  acceptTime(e); e.setSpeedKmh(36);
  e.schedule(100000, Command::reverseState(true));
  e.schedule(100000, Command::click(ButtonId::At));
  e.schedule(100000, Command::click(ButtonId::Trip1Reset));
  e.schedule(100000, Command::reverseState(false));
  e.advanceMicroseconds(100000);
  const auto& events = e.application().eventRepository();
  const size_t n = events.count();
  CHECK(events.at(n-4)->eventType == domain::DomainEventType::REVERSE_CHANGED);
  CHECK(events.at(n-4)->trip1DistanceMillimeters == 1000);
  CHECK(events.at(n-3)->eventType == domain::DomainEventType::AT);
  CHECK(events.at(n-3)->reverseActive);
  CHECK(events.at(n-2)->eventType == domain::DomainEventType::TRIP_RESET);
  CHECK(events.at(n-1)->eventType == domain::DomainEventType::REVERSE_CHANGED);
  CHECK(e.application().trip1DistanceMillimeters() == 0);
  CHECK(e.application().trip2DistanceMillimeters() == 1000);
  SimulatorEngine direct(routeState());
  acceptTime(direct); direct.setSpeedKmh(36); direct.advanceMicroseconds(100000);
  direct.setReverse(true); direct.press(ButtonId::At);
  direct.press(ButtonId::Trip1Reset); direct.setReverse(false);
  CHECK(snapshot(e) == snapshot(direct));
}

void testReset() {
  const auto initial = routeState();
  SimulatorEngine e(initial), fresh(initial);
  addTimeline(e); e.advanceMilliseconds(8000);
  e.buttonEvent(ButtonId::FootReset, ButtonEventType::Press);
  e.setSavesSucceed(false); e.reset();
  CHECK(snapshot(e) == snapshot(fresh));
  CHECK(!e.clock().isSet());
  e.advanceMilliseconds(20000); fresh.advanceMilliseconds(20000);
  CHECK(snapshot(e) == snapshot(fresh));
  acceptTime(e); acceptTime(fresh);
  e.setSpeedKmh(36); fresh.setSpeedKmh(36);
  e.advanceMilliseconds(1000); fresh.advanceMilliseconds(1000);
  CHECK(snapshot(e) == snapshot(fresh));
  openCalibration(e); e.press(ButtonId::Up); e.press(ButtonId::Right);
  CHECK(e.application().millimetersPerPulse() == 1001);
  e.reset(); CHECK(e.application().millimetersPerPulse() == 1000);
}

void testInvalidInput() {
  SimulatorEngine e;
  const auto before = snapshot(e);
  for (double speed : {-1.0, 201.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()})
    rejects<std::invalid_argument>([&] { e.setSpeedKmh(speed); });
  rejects<std::invalid_argument>([&] { e.schedule(0, Command::speed(1000)); });
  rejects<std::invalid_argument>([&] { e.schedule(1000, Command::speed(200001)); });
  rejects<std::invalid_argument>([&] { e.press(static_cast<ButtonId>(255)); });
  rejects<std::overflow_error>([&] { e.advanceMilliseconds(std::numeric_limits<uint64_t>::max()); });
  CHECK(snapshot(e) == before);
  e.step();
  rejects<std::invalid_argument>([&] { e.schedule(999, Command::speed(1000)); });
  rejects<std::overflow_error>([&] { e.advanceMicroseconds(std::numeric_limits<uint64_t>::max()); });
  CHECK(e.monotonicMicroseconds() == 1000);
  InitialState bad;
  bad.millimetersPerPulse = 0;
  rejects<std::invalid_argument>([&] { SimulatorEngine invalid(bad); });
  bad = InitialState{}; bad.routeActive = true;
  rejects<std::invalid_argument>([&] { SimulatorEngine invalid(bad); });
  simulator::SimTimeSource source;
  source.advanceMicroseconds(std::numeric_limits<uint64_t>::max());
  rejects<std::overflow_error>([&] { source.advanceMicroseconds(1); });
  CHECK(source.microseconds() == std::numeric_limits<uint64_t>::max());
}

void testMicrosRollover() {
  SimulatorEngine e;
  acceptTime(e);
  e.advanceMicroseconds((uint64_t{1} << 32) - 150000);
  e.setSpeedKmh(36); e.advanceMicroseconds(300000);
  CHECK(e.generatedPulseCount() == 3);
  CHECK(e.lastPulseMicroseconds() > std::numeric_limits<uint32_t>::max());
  CHECK(e.lastPulseMicroseconds() - e.previousPulseMicroseconds() == 100000);
  CHECK(e.displayModel().speedKmh == 36);
  CHECK(e.clock().elapsedSinceSetMilliseconds() == ((uint64_t{1} << 32) + 150000) / 1000);
  e.stop(); e.advanceMilliseconds(1000);
  // Viimeinen pulssi ei osu ms-ruudukkoon: aikakatkaisu näkyy seuraavalla tickillä.
  CHECK(e.displayModel().speedKmh == 36);
  e.advanceMicroseconds(1000 - e.monotonicMicroseconds() % 1000);
  CHECK(e.displayModel().speedKmh == 0);
}

void testManualSubtractAndPointUndo() {
  SimulatorEngine e(routeState());
  acceptTime(e); e.setSpeedKmh(36); e.advanceMilliseconds(1000);
  e.press(ButtonId::Left); e.advanceMilliseconds(200);
  CHECK(e.application().trip1DistanceMillimeters() == 8000);
  CHECK(e.application().competition().segmentDistanceMillimeters() == 8000);
  CHECK(e.displayModel().competition.manualSubtractActive);
  e.press(ButtonId::Point);
  CHECK(e.application().competition().currentSegmentIndex() == 1);
  CHECK(!e.application().manualSubtractActive());
  e.press(ButtonId::Left);
  CHECK(e.application().competition().currentSegmentIndex() == 0);
  CHECK(e.application().competition().segmentDistanceMillimeters() == 8000);
}

void testDirectCoreOracle() {
  // Sama syöte suoraan tuotantoytimeen; odotettu pulssijono on käsin määritelty.
  simulator::SimTimeSource time;
  core::SoftwareClock clock(time);
  core::ApplicationCore app(clock, 1000, 1000000);
  const auto initial = routeState();
  app.setInitialRouteOrder(initial.routeOrder, true);
  SimulatorEngine e(initial);
  acceptTime(e);
  for (unsigned i = 0; i < 2; ++i) {
    app.handleButton({ButtonId::Right, ButtonEventType::Press, 0});
    app.handleButton({ButtonId::Right, ButtonEventType::Release, 0});
    app.tick(0);
  }
  e.setSpeedKmh(36); app.tick(0);
  for (uint32_t ms = 1; ms <= 1000; ++ms) {
    time.advanceMicroseconds(1000);
    if (ms % 100 == 0)
      app.handleDistancePulses({1, (ms - 100) * 1000, ms * 1000, ms * 1000, false});
    app.tick(ms * 1000);
  }
  e.advanceMilliseconds(1000);
  e.press(ButtonId::Point);
  app.handleButton({ButtonId::Point, ButtonEventType::Press, 1000});
  app.handleButton({ButtonId::Point, ButtonEventType::Release, 1000});
  app.tick(1000000);
  CHECK(app.trip1DistanceMillimeters() == e.application().trip1DistanceMillimeters());
  CHECK(app.competition().deltaMs() == e.application().competition().deltaMs());
  CHECK(app.displayModel().competition.deltaSeconds == e.displayModel().competition.deltaSeconds);
  CHECK(app.eventRepository().count() == e.application().eventRepository().count());
  Snapshot actual, expected;
  for (size_t i = 0; i < app.eventRepository().count(); ++i) {
    actual.event(*e.application().eventRepository().at(i));
    expected.event(*app.eventRepository().at(i));
  }
  CHECK(actual.out.str() == expected.out.str());
}

void testExtremeWheelCalibration() {
  simulator::WheelPulseGenerator wheel;
  wheel.setSpeedMilliKmh(200000);
  wheel.setCalibration(std::numeric_limits<uint32_t>::max());
  for (unsigned i = 0; i < 100; ++i) CHECK(!wheel.advanceMicroseconds(1000));
  wheel.setCalibration(std::numeric_limits<uint32_t>::max() - 1);
  CHECK(wheel.microsecondsUntilPulse() > 0);
  wheel.setCalibration(1);
  CHECK(wheel.microsecondsUntilPulse() <= 18);
  rejects<std::invalid_argument>([&] { wheel.setCalibration(0); });
  rejects<std::invalid_argument>([&] { wheel.advanceMicroseconds(1001); });
}

void testAtTimingPartition() {
  SimulatorEngine large, small;
  for (auto* e : {&large, &small}) {
    acceptTime(*e);
    e->schedule(1234, Command::click(ButtonId::At));
    e->schedule(1001234, Command::speed(36000));
    e->schedule(1501234, Command::reverseState(true));
  }
  large.advanceMicroseconds(2001234);
  for (unsigned i = 0; i < 2001234; ++i) small.advanceMicroseconds(1);
  CHECK(!large.displayModel().atOverlay.visible);
  CHECK(large.displayModel().atOverlay.travelledDistanceMillimeters == 10000);
  CHECK(snapshot(large) == snapshot(small));
}

void testRouteAndSettingsSaves() {
  SimulatorEngine e;
  acceptTime(e);
  e.press(ButtonId::Down); e.press(ButtonId::Right); e.press(ButtonId::Right);
  e.press(ButtonId::Right); e.press(ButtonId::Up); e.press(ButtonId::Right);
  // TIME 00:01, sitten MAALI.
  for (unsigned i = 0; i < 4; ++i) e.press(ButtonId::Right);
  e.press(ButtonId::Up); e.press(ButtonId::Right);
  e.press(ButtonId::Down); e.press(ButtonId::Down); e.press(ButtonId::Right);
  CHECK(e.application().hasRouteOrder());
  CHECK(e.application().currentRouteOrder()->startMinute == 1);
  CHECK(e.application().competition().state() == domain::CompetitionState::WAIT_START);
  e.advanceMilliseconds(60000);
  CHECK(e.application().competition().state() == domain::CompetitionState::RUNNING);
  e.advanceMilliseconds(1000); e.press(ButtonId::Point);
  CHECK(e.displayModel().finishResult.visible);
  CHECK(e.displayModel().finishResult.totalPoints == 0);
  e.press(ButtonId::Left);
  // NÄYTTÖASETUKSET > KIRKKAUS, tuotantoytimen tallennuskuittaus.
  e.press(ButtonId::Down);
  for (unsigned i = 0; i < 4; ++i) e.press(ButtonId::Down);
  e.press(ButtonId::Right); e.press(ButtonId::Right);
  e.press(ButtonId::Up); e.press(ButtonId::Right);
  CHECK(e.displayModel().backlightPercent == 70);
  // Tekstin väri > PUNAINEN.
  e.press(ButtonId::Down); e.press(ButtonId::Right);
  e.press(ButtonId::Down); e.press(ButtonId::Right);
  CHECK(e.displayModel().textColor == domain::TextColor::RED);
  e.reset();
  CHECK(!e.application().hasRouteOrder());
  CHECK(e.displayModel().backlightPercent == 60);
  CHECK(e.displayModel().textColor == domain::TextColor::WHITE);
}

void testInitialRouteAndResetTargets() {
  auto initial = routeState();
  initial.routeActive = false;
  SimulatorEngine inactive(initial);
  acceptTime(inactive);
  CHECK(inactive.application().hasRouteOrder());
  CHECK(inactive.application().competition().state() == domain::CompetitionState::IDLE);
  inactive.setSpeedKmh(36); inactive.advanceMilliseconds(1000);
  // Tuotannon oletus: molemmat ulkoiset nollaukset kohdistuvat Trip 1:een.
  inactive.press(ButtonId::Trip2Reset);
  CHECK(inactive.application().trip1DistanceMillimeters() == 0);
  CHECK(inactive.application().trip2DistanceMillimeters() == 10000);
  inactive.advanceMilliseconds(100);
  inactive.press(ButtonId::FootReset);
  CHECK(inactive.application().trip1DistanceMillimeters() == 0);
  CHECK(inactive.application().trip2DistanceMillimeters() == 11000);
}
}  // namespace

int main() {
  const std::vector<std::pair<const char*, std::function<void()>>> tests = {
      {"Simuloitu aika, vuorokausi ja millisekuntilaskurin kierto", testTime},
      {"Kellon alustus ja pysähtynyt ajoneuvo", testClockAndStopped},
      {"36 km/h: pulssiväli 100000 µs", [] { checkInterval(36000, 100000); }},
      {"60 km/h: pulssiväli 60000 µs", [] { checkInterval(60000, 60000); }},
      {"Pulssimäärät eri kalibroinneilla", testCalibrations},
      {"Nopeuden muutos, pysähdys ja osapulssin säilyminen", testRemainderAndStop},
      {"Tunnin ajo ilman kumuloituvaa pyöristysvirhettä", testLongRun},
      {"Peruutus tuotantoytimen kautta", testReverse},
      {"Painikkeet, pidetty nollaus, AT ja näyttömalli", testButtonsAndDisplay},
      {"Kalibroinnin tallennus, virhe ja pyörän vaihe", testCalibrationSave},
      {"MITTIS, näyttöpito ja hyväksytty kalibrointi", testMittis},
      {"JAT, uusi lähtö, maali ja tuotantopisteytys", testJatFinishAndScore},
      {"Toisto sekä suurten ja pienten aika-askelten vastaavuus", testReplayAndPartition},
      {"Samanaikaisten tapahtumien järjestys", testSameTimestamp},
      {"Aloita alusta palauttaa koko alkutilan", testReset},
      {"Virheelliset syötteet ja ajan ylivuoto", testInvalidInput},
      {"Mikrosekuntilaskurin kierto ja nopeuden nollaus", testMicrosRollover},
      {"Käsin miinustus ja reittipisteen peruminen", testManualSubtractAndPointUndo},
      {"Vertailu suoraan tuotantoytimeen", testDirectCoreOracle},
      {"Pulssigeneraattorin suuret kalibrointiarvot", testExtremeWheelCalibration},
      {"AT-ajastimet yhden mikrosekunnin askelilla", testAtTimingPartition},
      {"Ajomääräyksen ja näyttöasetusten tallennus muistissa", testRouteAndSettingsSaves},
      {"Passiivinen alkureitti ja tuotannon nollauskohteet", testInitialRouteAndResetTargets},
  };
  unsigned failed = 0;
  for (const auto& test : tests) {
    try { test.second(); std::cout << "Hyväksytty: " << test.first << '\n'; }
    catch (const std::exception& error) {
      ++failed;
      std::cerr << "Virhe: " << test.first << ": " << error.what() << '\n';
    }
  }
  std::cout << tests.size() << " testiä, " << failed << " epäonnistui\n";
  return failed == 0 ? 0 : 1;
}
