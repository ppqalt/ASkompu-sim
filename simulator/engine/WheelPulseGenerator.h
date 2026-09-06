#pragma once

#include <cstdint>

namespace simulator {

// Nopeus: 0,001 km/h. Vaihe: 1 / 3 600 000 millimetriä.
// Simulaattori tuottaa hyväksytyt pulssit; sähköisiä häiriöitä ei mallinneta.
class WheelPulseGenerator {
 public:
  static constexpr uint32_t maximumSpeedMilliKmh = 200000;
  void setSpeedMilliKmh(uint32_t speed);
  void setCalibration(uint32_t millimetersPerPulse);
  uint32_t speedMilliKmh() const { return speed_; }
  uint64_t microsecondsUntilPulse() const;
  // Kutsuja etenee enintään seuraavaan pulssiin, enintään 1000 µs kerralla.
  bool advanceMicroseconds(uint64_t durationUs);

 private:
  uint32_t speed_ = 0;
  uint32_t calibration_ = 1000;
  uint64_t phase_ = 0;
};

}  // namespace simulator
