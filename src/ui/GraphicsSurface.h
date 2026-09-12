#pragma once

#include <cstdint>

namespace ui {

using DisplayColor = uint16_t;

// DisplayRenderer surfaces use structural typing. Menu rendering additionally
// requires fillRoundRect(x, y, width, height, radius, color); no runtime base
// class or virtual dispatch is involved.

// Values intentionally match TFT_eSPI's datum constants. A surface is a
// statically-bound template parameter; this is not a virtual interface.
enum class TextDatum : uint8_t {
  TopLeft = 0,
  TopCenter = 1,
  TopRight = 2,
  MiddleLeft = 3,
  MiddleCenter = 4,
  MiddleRight = 5,
  BottomLeft = 6,
  BottomCenter = 7,
  BottomRight = 8,
};

}  // namespace ui
