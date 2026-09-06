#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "CalibrationConfig.h"
#include "DemoConfig.h"
#include "core/ApplicationCore.h"

namespace simulator {

struct InitialState {
  uint32_t millimetersPerPulse = CalibrationConfig::DEFAULT_MILLIMETERS_PER_PULSE;
  uint32_t zeroSpeedTimeoutUs = DemoConfig::zeroSpeedTimeoutUs;
  // Tyhjä reitti tarkoittaa, ettei tallennettua ajomääräystä ole.
  domain::RouteOrder routeOrder;
  bool routeActive = false;
  domain::CompetitionSettings competitionSettings;
  domain::DisplaySettings displaySettings;
  domain::DebugDisplaySettings debugDisplaySettings{};
  domain::TextColor textColor = domain::TextColor::WHITE;
  bool savesSucceed = true;
};

// Toistettavat arvokomennot: ei isäntäkelloa, säikeitä tai takaisinkutsuja.
struct Command {
  enum class Kind { Speed, Reverse, Button, Click };
  Kind kind = Kind::Speed;
  uint32_t speedMilliKmh = 0;
  bool reverse = false;
  core::ButtonId button = core::ButtonId::Point;
  core::ButtonEventType eventType = core::ButtonEventType::Press;

  static Command speed(uint32_t milliKmh);
  static Command reverseState(bool active);
  static Command buttonEvent(core::ButtonId id, core::ButtonEventType type);
  static Command click(core::ButtonId id);
};

class SimulatorEngine {
 public:
  explicit SimulatorEngine(const InitialState& initial = InitialState{});
  ~SimulatorEngine();
  SimulatorEngine(const SimulatorEngine&) = delete;
  SimulatorEngine& operator=(const SimulatorEngine&) = delete;

  void reset();
  void advanceMicroseconds(uint64_t durationUs);
  void advanceMilliseconds(uint64_t durationMs);
  void step();  // 1 ms simuloitua aikaa.
  void setSpeedKmh(double speed);
  void setSpeedMilliKmh(uint32_t speed);
  void stop();
  void setReverse(bool active);
  void press(core::ButtonId id);  // Semanttinen lyhyt painallus ja vapautus.
  void buttonEvent(core::ButtonId id, core::ButtonEventType type);
  void execute(const Command& command);
  // Vain tulevat ajat; saman aikaleiman komennot suoritetaan lisäysjärjestyksessä.
  void schedule(uint64_t atUs, const Command& command);
  void setSavesSucceed(bool succeed);

  uint64_t monotonicMicroseconds() const;
  uint32_t targetSpeedMilliKmh() const;
  double targetSpeedKmh() const;
  bool reverseActive() const;
  uint64_t generatedPulseCount() const;
  uint64_t lastPulseMicroseconds() const;
  uint64_t previousPulseMicroseconds() const;
  const core::SoftwareClock& clock() const;
  const core::ApplicationCore& application() const;
  core::DisplayModel displayModel() const;

 private:
  struct Runtime;
  void validate(const Command& command) const;
  void settle();
  void serviceSaves();

  InitialState initial_;
  std::unique_ptr<Runtime> runtime_;
  std::map<uint64_t, std::vector<Command>> scheduled_;
};

}  // namespace simulator
