#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>

#include "core/Clock.h"

namespace simulator {

class SimTimeSource final : public core::TimeSource {
 public:
  uint64_t microseconds() const { return nowUs_; }
  uint32_t monotonicMilliseconds() const override {
    return static_cast<uint32_t>(nowUs_ / 1000);
  }
  void advanceMicroseconds(uint64_t durationUs) {
    if (durationUs > std::numeric_limits<uint64_t>::max() - nowUs_)
      throw std::overflow_error("Simuloidun ajan enimmäisarvo ylittyy");
    nowUs_ += durationUs;
  }

 private:
  uint64_t nowUs_ = 0;
};

}  // namespace simulator
