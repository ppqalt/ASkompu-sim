#include "AppController.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <utility>
#include "Finnish.h"
#include "ui/RouteOrderFormatting.h"

namespace simulator::desktop {
AppController::AppController(const InitialState& initial) : engine_(initial) {
  append("Simulaattori valmis · aseta ASkompun kellonaika");
}
std::vector<std::pair<std::string, std::string>> AppController::diagnostics() const {
  const auto& app = engine_.application();
  const auto& competition = app.competition();
  std::vector<std::pair<std::string, std::string>> rows;
  rows.emplace_back("Näyttö", screenName(app.screen()));
  if (app.screen() == core::Screen::Menu) {
    const auto model = displayModel();
    rows.emplace_back("Valikko", model.menu.title ? model.menu.title : "");
  }
  rows.emplace_back("Pulssimäärä", std::to_string(engine_.generatedPulseCount()));
  rows.emplace_back("Kilpailu", competitionName(competition.state()));
  const auto* route = app.currentRouteOrder();
  rows.emplace_back("Ajomääräys", route ? std::to_string(route->segments.size()) + " pisteväliä" : "Ei ajomääräystä");
  if (const auto* current = competition.currentSegment()) {
    char text[24];
    ui::formatDriveSegmentRange(text, sizeof(text), *current);
    rows.emplace_back("Reittipiste", text);
  }
  rows.emplace_back("Kilpailumatka", distanceText(competition.physicalDistanceMillimeters()));
  rows.emplace_back("Kokonaispisteet", std::to_string(competition.totalPoints()));
  rows.emplace_back("Kellon käyntiaika", durationText(engine_.clock().elapsedSinceSetMilliseconds() * 1000));
  rows.emplace_back("Ohitettu ruutuviive", std::to_string(discardedHostNs_ / 1000000) + " ms");
  return rows;
}
const domain::EventRecord* AppController::applicationEvent(size_t index) const {
  return engine_.application().eventRepository().at(index);
}
size_t AppController::applicationEventCount() const {
  return engine_.application().eventRepository().count();
}
uint32_t AppController::millimetersPerPulse() const {
  return engine_.application().millimetersPerPulse();
}
void AppController::append(std::string text, bool applicationEvent, size_t index) {
  if (log_.size() == logCapacity) log_.pop_front();
  log_.push_back({engine_.monotonicMicroseconds(), std::move(text), applicationEvent, index});
}
void AppController::observe() {
  const auto& app = engine_.application();
  const auto& events = app.eventRepository();
  for (; nextEvent_ < events.count(); ++nextEvent_)
    append(eventDescription(*events.at(nextEvent_)), true, nextEvent_);
  if (lastCompetition_ != app.competition().state()) {
    lastCompetition_ = app.competition().state();
    append(competitionName(lastCompetition_));
  }
}
void AppController::hostFrame(uint64_t elapsedNs) {
  if (paused_ || resetPending_) return;
  const uint64_t accepted = std::min(elapsedNs, maximumHostFrameNs);
  if (elapsedNs > accepted) {
    const uint64_t discarded = elapsedNs - accepted;
    discardedHostNs_ += std::min(discarded, std::numeric_limits<uint64_t>::max() - discardedHostNs_);
    if (!stallReported_) append("Varoitus: pitkä ruutuviive rajattiin 250 millisekuntiin");
    stallReported_ = true;
  } else {
    stallReported_ = false;
  }
  const uint64_t ns = accepted * multiplier_ + remainderNs_;
  engine_.advanceMicroseconds(ns / 1000);
  remainderNs_ = ns % 1000;
  observe();
}
void AppController::pause() {
  if (!paused_) { paused_ = true; append("Simulointi keskeytetty"); }
}
void AppController::resume() {
  if (resetPending_) return;
  if (paused_) { paused_ = false; append("Simulointi jatkuu"); }
}
void AppController::setMultiplier(unsigned multiplier) {
  if (multiplier != 1 && multiplier != 10 && multiplier != 100)
    throw std::invalid_argument("Simulointinopeuden on oltava 1×, 10× tai 100×");
  if (multiplier_ == multiplier) return;
  multiplier_ = multiplier;
  append("Simulointinopeus " + std::to_string(multiplier) + "×");
}
void AppController::step() {
  if (resetPending_) return;
  pause();
  engine_.advanceMicroseconds(stepMicroseconds);
  observe();
  append("Askel · 0,1 s");
}
void AppController::setSpeed(double kmh) {
  if (resetPending_) return;
  if (!std::isfinite(kmh) || kmh < 0 || kmh > 200)
    throw std::invalid_argument("Nopeuden on oltava välillä 0–200 km/h");
  if (engine_.targetSpeedMilliKmh() == static_cast<uint32_t>(std::floor(kmh * 1000 + 0.5))) return;
  engine_.setSpeedKmh(kmh);
  dirty_ = true;
  char text[64];
  std::snprintf(text, sizeof(text), "Nopeus %.1f km/h", engine_.targetSpeedKmh());
  append(text);
  observe();
}
void AppController::stopVehicle() { setSpeed(0); }
void AppController::setReverse(bool reverse) {
  if (resetPending_ || engine_.reverseActive() == reverse) return;
  engine_.setReverse(reverse);
  dirty_ = true;
  observe();
}
void AppController::press(core::ButtonId button) {
  if (resetPending_) return;
  append(std::string("Painike · ") + buttonName(button));
  engine_.press(button);
  dirty_ = true;
  observe();
}
void AppController::longPress(core::ButtonId button) {
  if (resetPending_) return;
  // Tuotanto kuluttaa valmiin pitkän painalluksen; ei ylimääräistä Point-vapautusta.
  engine_.buttonEvent(button, core::ButtonEventType::LongStart);
  dirty_ = true;
  append(std::string("Pitkä painallus · ") + buttonName(button));
  observe();
}
void AppController::holdReset(core::ButtonId button, bool down) {
  if (resetPending_) return;
  if (button != core::ButtonId::Trip1Reset && button != core::ButtonId::Trip2Reset &&
      button != core::ButtonId::FootReset)
    throw std::invalid_argument("Pito on käytettävissä vain nollauspainikkeille");
  engine_.buttonEvent(button, down ? core::ButtonEventType::Press : core::ButtonEventType::Release);
  dirty_ = true;
  append(std::string(down ? "Nollauspainike painettu · " : "Nollauspainike vapautettu · ") + buttonName(button));
  observe();
}
void AppController::requestReset() {
  if (resetPending_) return;
  if (!dirty_ && engine_.monotonicMicroseconds() == 0) { reset(); return; }
  resumeAfterCancel_ = !paused_;
  pause();
  resetPending_ = true;
}
void AppController::cancelReset() {
  if (!resetPending_) return;
  resetPending_ = false;
  if (resumeAfterCancel_) resume();
}
void AppController::confirmReset() {
  if (resetPending_) reset();
}
void AppController::reset() {
  engine_.reset();
  paused_ = true; dirty_ = false; resetPending_ = false; resumeAfterCancel_ = false;
  multiplier_ = 1; remainderNs_ = 0; discardedHostNs_ = 0; stallReported_ = false;
  nextEvent_ = 0; lastCompetition_ = domain::CompetitionState::IDLE;
  log_.clear();
  append("Aloitettu alusta · aseta ASkompun kellonaika");
}
}  // namespace simulator::desktop
