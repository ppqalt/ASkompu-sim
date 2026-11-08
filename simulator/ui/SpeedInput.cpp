#include "SpeedInput.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <locale>
#include <sstream>

namespace simulator::desktop {

ParsedSpeed parseSpeedInput(const std::string& text) {
  const auto first = text.find_first_not_of(" \t\r\n\v\f");
  if (first == std::string::npos)
    return {false, 0, "Kirjoita nopeus (0–200 km/h)."};
  const auto last = text.find_last_not_of(" \t\r\n\v\f");
  std::string normalized = text.substr(first, last - first + 1);
  std::replace(normalized.begin(), normalized.end(), ',', '.');
  std::istringstream input(normalized);
  input.imbue(std::locale::classic());
  double kmh = 0;
  input >> std::noskipws >> kmh;
  if (input.fail() || !input.eof() || !std::isfinite(kmh))
    return {false, 0, "Anna kelvollinen numero, esimerkiksi 72,5."};
  if (kmh < 0 || kmh > 200)
    return {false, 0, "Nopeuden pitää olla välillä 0–200 km/h."};
  return {true, kmh, ""};
}

void SpeedInput::sync(double currentKmh, bool editing) {
  if (!dirty_ && !editing) accept(currentKmh);
}

void SpeedInput::edited() {
  dirty_ = true;
  error_ = "";
}

ParsedSpeed SpeedInput::submit() {
  const auto result = parseSpeedInput(draft_.data());
  error_ = result.error;
  return result;
}

void SpeedInput::accept(double currentKmh) {
  // The production pulse generator keeps three decimal places. Retain those
  // instead of silently rounding an unchanged input on the next submission.
  std::snprintf(draft_.data(), draft_.size(), "%.3f", currentKmh);
  size_t length = std::strlen(draft_.data());
  while (length > 2 && draft_[length - 1] == '0' &&
         draft_[length - 2] != '.' && draft_[length - 2] != ',')
    draft_[--length] = '\0';
  dirty_ = false;
  error_ = "";
}

}  // namespace simulator::desktop
