#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace simulator::desktop {

struct ParsedSpeed {
  bool valid = false;
  double kmh = 0;
  const char* error = "";
};

// Parse the entire draft, independently of the system's numeric locale.
// Finnish decimal commas and decimal points are both accepted.
ParsedSpeed parseSpeedInput(const std::string& text);

class SpeedInput {
 public:
  char* text() { return draft_.data(); }
  const char* text() const { return draft_.data(); }
  size_t capacity() const { return draft_.size(); }
  bool dirty() const { return dirty_; }
  const char* error() const { return error_; }

  void sync(double currentKmh, bool editing);
  void edited();
  ParsedSpeed submit();
  void accept(double currentKmh);

 private:
  std::array<char, 64> draft_{};
  bool dirty_ = false;
  const char* error_ = "";
};

}  // namespace simulator::desktop
