#include "ui/SpeedInput.h"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
void edit(simulator::desktop::SpeedInput& input, const char* text) {
  std::snprintf(input.text(), input.capacity(), "%s", text);
  input.edited();
}
}

int main() {
  try {
    using namespace simulator::desktop;
    const std::pair<const char*, double> valid[] = {
        {"0", 0}, {"200", 200}, {"72.5", 72.5}, {"72,5", 72.5},
        {" 73,123 ", 73.123}, {"+36", 36}, {"1e2", 100}, {".5", .5}};
    for (const auto& example : valid) {
      const auto result = parseSpeedInput(example.first);
      require(result.valid && std::abs(result.kmh - example.second) < 1e-9,
              "Valid decimal speed rejected or changed");
    }
    const char* invalid[] = {
        "", "   ", "nan", "NaN", "inf", "-inf", "Infinity", "1e999",
        "-1", "200.0001", "30foo", "12.4.5", "12,4.5", "--1", "1/2"};
    for (const auto* example : invalid) {
      const auto result = parseSpeedInput(example);
      require(!result.valid && *result.error, "Invalid speed silently accepted");
    }

    SpeedInput input;
    input.accept(36);
    edit(input, "72,5");
    input.sync(36, false);
    require(input.dirty() && std::string(input.text()) == "72,5",
            "Clicking an Apply button loses the unsubmitted draft");
    require(input.submit().valid && input.submit().kmh == 72.5,
            "Apply did not preserve the user's draft");
    input.accept(72.5);
    require(!input.dirty() && !*input.error(), "Accepted speed remains in an error state");

    edit(input, "nan");
    require(!input.submit().valid && *input.error(), "NaN draft has no useful error");
    input.sync(72.5, false);
    require(input.dirty() && std::string(input.text()) == "nan" && *input.error(),
            "Rejected draft or error is discarded before the user can correct it");
    edit(input, "73.123");
    require(!*input.error() && input.submit().valid, "Corrected draft stays rejected");
    input.accept(73.123);
    require(std::string(input.text()) == "73.123", "Input loses production speed precision");
    input.sync(60, false);
    require(std::string(input.text()) == "60.0", "Clean input does not follow preset speed changes");
    input.sync(120, true);
    require(std::string(input.text()) == "60.0", "Editing input changes under the user's cursor");

    std::cout << "Speed parsing, rejection and draft-preservation checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
