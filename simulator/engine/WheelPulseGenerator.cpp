#include "WheelPulseGenerator.h"

#include <limits>
#include <stdexcept>

namespace simulator {
namespace {
constexpr uint64_t movementDivisor = 3600000;
}

void WheelPulseGenerator::setSpeedMilliKmh(uint32_t speed) {
  if (speed > maximumSpeedMilliKmh)
    throw std::invalid_argument("Nopeuden on oltava välillä 0–200 km/h");
  speed_ = speed;
}

void WheelPulseGenerator::setCalibration(uint32_t millimetersPerPulse) {
  if (millimetersPerPulse == 0)
    throw std::invalid_argument("Kalibroinnin on oltava positiivinen");
  if (millimetersPerPulse == calibration_) return;
  // Säilytetään pyörän kierroksen osuus. Jaettu kertolasku välttää ylivuodon
  // myös tuotantoytimen mahdollisella uint32_t-kokoisella MITTIS-kertoimella.
  phase_ = (phase_ / calibration_) * millimetersPerPulse +
           (phase_ % calibration_) * millimetersPerPulse / calibration_;
  calibration_ = millimetersPerPulse;
}

uint64_t WheelPulseGenerator::microsecondsUntilPulse() const {
  if (speed_ == 0) return std::numeric_limits<uint64_t>::max();
  const uint64_t remaining = calibration_ * movementDivisor - phase_;
  return remaining / speed_ + (remaining % speed_ != 0 ? 1 : 0);
}

bool WheelPulseGenerator::advanceMicroseconds(uint64_t durationUs) {
  if (durationUs > 1000 || durationUs > microsecondsUntilPulse())
    throw std::invalid_argument("Pulssiaskeleen sallittu pituus ylittyy");
  phase_ += durationUs * speed_;
  const uint64_t threshold = calibration_ * movementDivisor;
  if (phase_ < threshold) return false;
  phase_ -= threshold;
  return true;
}

}  // namespace simulator
