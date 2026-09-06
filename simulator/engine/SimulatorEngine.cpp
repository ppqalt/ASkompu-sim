#include "SimulatorEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "SimTimeSource.h"
#include "WheelPulseGenerator.h"
#include "domain/CalibrationSetting.h"

namespace simulator {

struct SimulatorEngine::Runtime {
  explicit Runtime(const InitialState& initial)
      : clock(time),
        application(clock, initial.millimetersPerPulse, initial.zeroSpeedTimeoutUs),
        savesSucceed(initial.savesSucceed) {
    application.setCompetitionSettings(initial.competitionSettings);
    application.setInitialDisplaySettings(initial.displaySettings);
    application.setInitialDebugDisplaySettings(initial.debugDisplaySettings);
    application.setInitialTextColor(initial.textColor);
    if (!initial.routeOrder.segments.empty())
      application.setInitialRouteOrder(initial.routeOrder, initial.routeActive);
    wheel.setCalibration(application.millimetersPerPulse());
  }
  SimTimeSource time;
  core::SoftwareClock clock;
  core::ApplicationCore application;
  WheelPulseGenerator wheel;
  bool reverse = false;
  bool savesSucceed;
  uint64_t pulses = 0;
  uint64_t previousPulseUs = 0;
  uint64_t lastPulseUs = 0;
};

Command Command::speed(uint32_t milliKmh) {
  Command command;
  command.kind = Kind::Speed;
  command.speedMilliKmh = milliKmh;
  return command;
}

Command Command::reverseState(bool active) {
  Command command;
  command.kind = Kind::Reverse;
  command.reverse = active;
  return command;
}

Command Command::buttonEvent(core::ButtonId id, core::ButtonEventType type) {
  Command command;
  command.kind = Kind::Button;
  command.button = id;
  command.eventType = type;
  return command;
}

Command Command::click(core::ButtonId id) {
  Command command = buttonEvent(id, core::ButtonEventType::Press);
  command.kind = Kind::Click;
  return command;
}

SimulatorEngine::SimulatorEngine(const InitialState& initial) : initial_(initial) {
  if (!domain::calibration::isValid(initial_.millimetersPerPulse))
    throw std::invalid_argument("Kalibroinnin on oltava välillä 1–100000 mm/pulssi");
  if (initial_.zeroSpeedTimeoutUs == 0)
    throw std::invalid_argument("Nopeuden nollausviiveen on oltava positiivinen");
  if ((!initial_.routeOrder.segments.empty() || initial_.routeActive) &&
      domain::validateRouteOrder(initial_.routeOrder) !=
          domain::RouteOrderValidationError::NONE)
    throw std::invalid_argument("Ajomääräys ei ole kelvollinen");
  reset();
}

SimulatorEngine::~SimulatorEngine() = default;

void SimulatorEngine::reset() {
  // Uudelleenluonti poistaa myös ytimen piilotetun valikko-, pito- ja lokitilan.
  auto fresh = std::make_unique<Runtime>(initial_);
  runtime_ = std::move(fresh);
  scheduled_.clear();
}

void SimulatorEngine::advanceMicroseconds(uint64_t durationUs) {
  auto& state = *runtime_;
  if (durationUs > std::numeric_limits<uint64_t>::max() - state.time.microseconds())
    throw std::overflow_error("Simuloidun ajan enimmäisarvo ylittyy");
  const uint64_t target = state.time.microseconds() + durationUs;
  while (state.time.microseconds() < target) {
    const uint64_t now = state.time.microseconds();
    const uint64_t untilTick = 1000 - now % 1000;
    uint64_t delta = std::min(target - now, untilTick);
    delta = std::min(delta, state.wheel.microsecondsUntilPulse());
    if (!scheduled_.empty()) delta = std::min(delta, scheduled_.begin()->first - now);

    const bool pulse = state.wheel.advanceMicroseconds(delta);
    state.time.advanceMicroseconds(delta);
    if (pulse) {
      state.previousPulseUs = state.lastPulseUs;
      state.lastPulseUs = state.time.microseconds();
      ++state.pulses;
      state.application.handleDistancePulses(
          {1, static_cast<uint32_t>(state.previousPulseUs),
           static_cast<uint32_t>(state.lastPulseUs),
           static_cast<uint32_t>(state.time.microseconds()), state.reverse});
    }
    // Kutsun mielivaltainen loppuaika ei lisää tick-kutsua. Näin pilkkominen
    // ei muuta AT:n, MITTIS-näyttöpidon tai muiden ajastimien käyttäytymistä.
    if (pulse || delta == untilTick) settle();
    if (!scheduled_.empty() && scheduled_.begin()->first == state.time.microseconds()) {
      auto commands = std::move(scheduled_.begin()->second);
      scheduled_.erase(scheduled_.begin());
      for (const auto& command : commands) execute(command);
    }
  }
}

void SimulatorEngine::advanceMilliseconds(uint64_t durationMs) {
  if (durationMs > std::numeric_limits<uint64_t>::max() / 1000)
    throw std::overflow_error("Simuloidun ajan enimmäisarvo ylittyy");
  advanceMicroseconds(durationMs * 1000);
}

void SimulatorEngine::step() { advanceMilliseconds(1); }

void SimulatorEngine::setSpeedKmh(double speed) {
  if (!std::isfinite(speed) || speed < 0 || speed > 200)
    throw std::invalid_argument("Nopeuden on oltava välillä 0–200 km/h");
  setSpeedMilliKmh(static_cast<uint32_t>(std::floor(speed * 1000.0 + 0.5)));
}

void SimulatorEngine::setSpeedMilliKmh(uint32_t speed) { execute(Command::speed(speed)); }
void SimulatorEngine::stop() { setSpeedMilliKmh(0); }
void SimulatorEngine::setReverse(bool active) { execute(Command::reverseState(active)); }
void SimulatorEngine::press(core::ButtonId id) { execute(Command::click(id)); }
void SimulatorEngine::buttonEvent(core::ButtonId id, core::ButtonEventType type) {
  execute(Command::buttonEvent(id, type));
}

void SimulatorEngine::validate(const Command& command) const {
  switch (command.kind) {
    case Command::Kind::Speed:
      if (command.speedMilliKmh > WheelPulseGenerator::maximumSpeedMilliKmh)
        throw std::invalid_argument("Nopeuden on oltava välillä 0–200 km/h");
      break;
    case Command::Kind::Reverse:
      break;
    case Command::Kind::Button:
    case Command::Kind::Click:
      if (static_cast<unsigned>(command.button) >
              static_cast<unsigned>(core::ButtonId::FootReset) ||
          static_cast<unsigned>(command.eventType) >
              static_cast<unsigned>(core::ButtonEventType::LongRepeat))
        throw std::invalid_argument("Painiketapahtuma ei ole kelvollinen");
      break;
    default:
      throw std::invalid_argument("Simulaattorikomento ei ole kelvollinen");
  }
}

void SimulatorEngine::execute(const Command& command) {
  validate(command);
  auto& state = *runtime_;
  const auto nowMs = state.time.monotonicMilliseconds();
  switch (command.kind) {
    case Command::Kind::Speed:
      state.wheel.setSpeedMilliKmh(command.speedMilliKmh);
      break;
    case Command::Kind::Reverse:
      state.reverse = command.reverse;
      state.application.handleReverseSignal({state.reverse, nowMs});
      break;
    case Command::Kind::Button:
      state.application.handleButton({command.button, command.eventType, nowMs});
      break;
    case Command::Kind::Click:
      state.application.handleButton({command.button, core::ButtonEventType::Press, nowMs});
      state.application.handleButton({command.button, core::ButtonEventType::Release, nowMs});
      break;
  }
  settle();
}

void SimulatorEngine::schedule(uint64_t atUs, const Command& command) {
  validate(command);
  if (atUs <= monotonicMicroseconds())
    throw std::invalid_argument("Ajastetun komennon ajan on oltava tulevaisuudessa");
  scheduled_[atUs].push_back(command);
}

void SimulatorEngine::setSavesSucceed(bool succeed) { runtime_->savesSucceed = succeed; }

void SimulatorEngine::settle() {
  runtime_->application.tick(static_cast<uint32_t>(monotonicMicroseconds()));
  serviceSaves();
  runtime_->wheel.setCalibration(runtime_->application.millimetersPerPulse());
}

void SimulatorEngine::serviceSaves() {
  auto& app = runtime_->application;
  const bool saved = runtime_->savesSucceed;
  uint32_t calibration = 0;
  if (app.takeCalibrationSaveRequest(calibration)) app.completeCalibrationSave(saved);
  domain::TextColor color;
  if (app.takeTextColorSaveRequest(color)) app.completeTextColorSave(saved);
  domain::DebugDisplaySettings debug;
  if (app.takeDebugDisplaySettingsSaveRequest(debug)) app.completeDebugDisplaySettingsSave(saved);
  domain::DisplaySettings display;
  if (app.takeDisplaySettingsSaveRequest(display)) app.completeDisplaySettingsSave(saved);
  const domain::RouteOrder* order = nullptr;
  if (app.takeRouteOrderSaveRequest(order)) app.completeRouteOrderSave(saved);
  if (app.takeRouteOrderCompletionRequest()) app.completeRouteOrderCompletion(saved);
}

uint64_t SimulatorEngine::monotonicMicroseconds() const { return runtime_->time.microseconds(); }
uint32_t SimulatorEngine::targetSpeedMilliKmh() const { return runtime_->wheel.speedMilliKmh(); }
double SimulatorEngine::targetSpeedKmh() const { return targetSpeedMilliKmh() / 1000.0; }
bool SimulatorEngine::reverseActive() const { return runtime_->reverse; }
uint64_t SimulatorEngine::generatedPulseCount() const { return runtime_->pulses; }
uint64_t SimulatorEngine::lastPulseMicroseconds() const { return runtime_->lastPulseUs; }
uint64_t SimulatorEngine::previousPulseMicroseconds() const { return runtime_->previousPulseUs; }
const core::SoftwareClock& SimulatorEngine::clock() const { return runtime_->clock; }
const core::ApplicationCore& SimulatorEngine::application() const { return runtime_->application; }
core::DisplayModel SimulatorEngine::displayModel() const { return application().displayModel(); }

}  // namespace simulator
