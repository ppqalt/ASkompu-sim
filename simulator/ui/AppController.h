#pragma once

#include <deque>
#include <string>
#include "engine/SimulatorEngine.h"

namespace simulator::desktop {
struct LogEntry {
  uint64_t observedAtUs;
  std::string text;
  bool applicationEvent = false;
  size_t repositoryIndex = 0;
};

class AppController {
 public:
  static constexpr uint64_t stepMicroseconds = 100000;  // Askel = 0,1 s.
  static constexpr uint64_t maximumHostFrameNs = 250000000;  // Enintään 250 ms.
  static constexpr size_t logCapacity = 512;
  explicit AppController(const InitialState& initial = InitialState{});
  const SimulatorEngine& engine() const { return engine_; }
  bool paused() const { return paused_; }
  unsigned multiplier() const { return multiplier_; }
  bool resetPending() const { return resetPending_; }
  const std::deque<LogEntry>& log() const { return log_; }
  uint64_t discardedHostNanoseconds() const { return discardedHostNs_; }

  void hostFrame(uint64_t elapsedNanoseconds);
  void pause();
  void resume();
  void setMultiplier(unsigned multiplier);
  void step();
  void setSpeed(double kmh);
  void stopVehicle();
  void setReverse(bool reverse);
  void press(core::ButtonId button);
  void longPress(core::ButtonId button);
  void holdReset(core::ButtonId button, bool down);
  void requestReset();
  void cancelReset();
  void confirmReset();

 private:
  void append(std::string text, bool applicationEvent = false, size_t index = 0);
  void observe();
  void reset();
  SimulatorEngine engine_;
  bool paused_ = true;
  bool dirty_ = false;
  bool resetPending_ = false;
  bool resumeAfterCancel_ = false;
  unsigned multiplier_ = 1;
  uint64_t remainderNs_ = 0;
  uint64_t discardedHostNs_ = 0;
  bool stallReported_ = false;
  size_t nextEvent_ = 0;
  domain::CompetitionState lastCompetition_ = domain::CompetitionState::IDLE;
  std::deque<LogEntry> log_;
};
}  // namespace simulator::desktop
