#include "FramebufferCanvas.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {

// Exact font tables from TFT_eSPI 2.5.43. These source files are kept
// byte-for-byte identical under tft_espi_2_5_43/ with the upstream license.
#define PROGMEM
#include "tft_espi_2_5_43/glcdfont.c"
#include "tft_espi_2_5_43/Font16.c"
#undef PROGMEM

constexpr uint8_t FONT_2_HEIGHT = 16;
constexpr uint8_t FONT_2_BASELINE = 13;
static_assert(FONT_2_HEIGHT == 16 && FONT_2_BASELINE == 13,
              "TFT_eSPI 2.5.43 Font 2 metadata changed");

}  // namespace

namespace ui::test {

FramebufferCanvas::FramebufferCanvas(uint16_t width, uint16_t height)
    : width_(width), height_(height), pixels_(width * height) {
  if (width == 0 || height == 0)
    throw std::invalid_argument("Framebuffer dimensions must be non-zero");
}

void FramebufferCanvas::clear(DisplayColor color) {
  std::fill(pixels_.begin(), pixels_.end(), color);
}

void FramebufferCanvas::fillRect(int16_t x, int16_t y, int16_t rectWidth,
                                 int16_t rectHeight, DisplayColor color) {
  if (rectWidth <= 0 || rectHeight <= 0) return;
  const int32_t left = std::max<int32_t>(0, x);
  const int32_t top = std::max<int32_t>(0, y);
  const int32_t right = std::min<int32_t>(width_, x + rectWidth);
  const int32_t bottom = std::min<int32_t>(height_, y + rectHeight);
  for (int32_t row = top; row < bottom; ++row) {
    auto first = pixels_.begin() + row * width_ + left;
    std::fill(first, first + (right - left), color);
  }
}

void FramebufferCanvas::fillCircleHelper(int16_t x0, int16_t y0,
                                         int16_t radius, uint8_t corners,
                                         int16_t delta, DisplayColor color) {
  int32_t f = 1 - radius;
  int32_t deltaX = 1;
  int32_t deltaY = -radius - radius;
  int32_t y = 0;
  ++delta;
  while (y < radius) {
    if (f >= 0) {
      if (corners & 0x1)
        fillRect(x0 - y, y0 + radius, y + y + delta, 1, color);
      if (corners & 0x2)
        fillRect(x0 - y, y0 - radius, y + y + delta, 1, color);
      --radius;
      deltaY += 2;
      f += deltaY;
    }
    ++y;
    deltaX += 2;
    f += deltaX;
    if (corners & 0x1)
      fillRect(x0 - radius, y0 + y, radius + radius + delta, 1, color);
    if (corners & 0x2)
      fillRect(x0 - radius, y0 - y, radius + radius + delta, 1, color);
  }
}

void FramebufferCanvas::fillRoundRect(int16_t x, int16_t y,
                                      int16_t rectWidth, int16_t rectHeight,
                                      int16_t radius, DisplayColor color) {
  fillRect(x, y + radius, rectWidth, rectHeight - radius - radius, color);
  fillCircleHelper(x + radius, y + rectHeight - radius - 1, radius, 1,
                   rectWidth - radius - radius - 1, color);
  fillCircleHelper(x + radius, y + radius, radius, 2,
                   rectWidth - radius - radius - 1, color);
}

void FramebufferCanvas::fillTriangle(int16_t x0, int16_t y0, int16_t x1,
                                     int16_t y1, int16_t x2, int16_t y2,
                                     DisplayColor color) {
  if (y0 > y1) {
    std::swap(y0, y1);
    std::swap(x0, x1);
  }
  if (y1 > y2) {
    std::swap(y2, y1);
    std::swap(x2, x1);
  }
  if (y0 > y1) {
    std::swap(y0, y1);
    std::swap(x0, x1);
  }
  if (y0 == y2) {
    const int16_t left = std::min({x0, x1, x2});
    const int16_t right = std::max({x0, x1, x2});
    fillRect(left, y0, right - left + 1, 1, color);
    return;
  }

  const int32_t dx01 = x1 - x0;
  const int32_t dy01 = y1 - y0;
  const int32_t dx02 = x2 - x0;
  const int32_t dy02 = y2 - y0;
  const int32_t dx12 = x2 - x1;
  const int32_t dy12 = y2 - y1;
  int32_t accumulatorA = 0;
  int32_t accumulatorB = 0;
  int32_t y = y0;
  const int32_t last = y1 == y2 ? y1 : y1 - 1;
  for (; y <= last; ++y) {
    int32_t a = x0 + accumulatorA / dy01;
    int32_t b = x0 + accumulatorB / dy02;
    accumulatorA += dx01;
    accumulatorB += dx02;
    if (a > b) std::swap(a, b);
    fillRect(static_cast<int16_t>(a), static_cast<int16_t>(y),
             static_cast<int16_t>(b - a + 1), 1, color);
  }
  accumulatorA = dx12 * (y - y1);
  accumulatorB = dx02 * (y - y0);
  for (; y <= y2; ++y) {
    int32_t a = x1 + accumulatorA / dy12;
    int32_t b = x0 + accumulatorB / dy02;
    accumulatorA += dx12;
    accumulatorB += dx02;
    if (a > b) std::swap(a, b);
    fillRect(static_cast<int16_t>(a), static_cast<int16_t>(y),
             static_cast<int16_t>(b - a + 1), 1, color);
  }
}

void FramebufferCanvas::setTextColor(DisplayColor foreground,
                                     DisplayColor background) {
  textColor_ = foreground;
  textBackground_ = background;
}

void FramebufferCanvas::setTextDatum(TextDatum datum) { textDatum_ = datum; }

void FramebufferCanvas::setTextSize(uint8_t size) {
  textSize_ = size == 0 ? 1 : size;
}

int16_t FramebufferCanvas::textWidth(const char* text, uint8_t fontFace) const {
  int32_t width = 0;
  if (fontFace == 1) {
    while (*text++) width += 6;
  } else if (fontFace == 2) {
    while (*text) {
      const uint8_t character = static_cast<uint8_t>(*text++);
      const uint8_t index = character >= 32 && character < 128
                                ? static_cast<uint8_t>(character - 32)
                                : 0;
      width += widtbl_f16[index];
    }
  } else {
    throw std::invalid_argument("FramebufferCanvas supports TFT fonts 1 and 2");
  }
  return static_cast<int16_t>(width * textSize_);
}

int16_t FramebufferCanvas::fontHeight(uint8_t fontFace) const {
  if (fontFace == 1) return static_cast<int16_t>(8 * textSize_);
  if (fontFace == 2)
    return static_cast<int16_t>(FONT_2_HEIGHT * textSize_);
  throw std::invalid_argument("FramebufferCanvas supports TFT fonts 1 and 2");
}

int16_t FramebufferCanvas::drawString(const char* text, int32_t x, int32_t y,
                                      uint8_t fontFace) {
  const int16_t stringWidth = textWidth(text, fontFace);
  const int16_t stringHeight = fontHeight(fontFace);
  switch (textDatum_) {
    case TextDatum::TopCenter: x -= stringWidth / 2; break;
    case TextDatum::TopRight: x -= stringWidth; break;
    case TextDatum::MiddleLeft: y -= stringHeight / 2; break;
    case TextDatum::MiddleCenter:
      x -= stringWidth / 2;
      y -= stringHeight / 2;
      break;
    case TextDatum::MiddleRight:
      x -= stringWidth;
      y -= stringHeight / 2;
      break;
    case TextDatum::BottomLeft: y -= stringHeight; break;
    case TextDatum::BottomCenter:
      x -= stringWidth / 2;
      y -= stringHeight;
      break;
    case TextDatum::BottomRight:
      x -= stringWidth;
      y -= stringHeight;
      break;
    case TextDatum::TopLeft: break;
  }

  int16_t drawnWidth = 0;
  while (*text) {
    const uint8_t character = static_cast<uint8_t>(*text++);
    drawnWidth = static_cast<int16_t>(
        drawnWidth + drawCharacter(character, x + drawnWidth, y, fontFace));
  }
  return drawnWidth;
}

int16_t FramebufferCanvas::drawCharacter(uint8_t character, int32_t x,
                                         int32_t y, uint8_t fontFace) {
  if (fontFace == 1) {
    if (character < 32) return 0;
    drawGlcdCharacter(character, x, y);
    return static_cast<int16_t>(6 * textSize_);
  }
  if (fontFace == 2) {
    if (character < 32 || character > 127) return 0;
    drawFont2Character(character, x, y);
    return static_cast<int16_t>(widtbl_f16[character - 32] * textSize_);
  }
  throw std::invalid_argument("FramebufferCanvas supports TFT fonts 1 and 2");
}

void FramebufferCanvas::drawGlcdCharacter(uint8_t character, int32_t x,
                                          int32_t y) {
  for (uint8_t column = 0; column < 6; ++column) {
    uint8_t bits = column == 5 ? 0 : font[character * 5 + column];
    for (uint8_t row = 0; row < 8; ++row) {
      const DisplayColor color =
          bits & 1U ? textColor_ : textBackground_;
      fillRect(static_cast<int16_t>(x + column * textSize_),
               static_cast<int16_t>(y + row * textSize_), textSize_, textSize_,
               color);
      bits >>= 1U;
    }
  }
}

void FramebufferCanvas::drawFont2Character(uint8_t character, int32_t x,
                                           int32_t y) {
  const uint8_t index = static_cast<uint8_t>(character - 32);
  const int32_t glyphWidth = widtbl_f16[index];
  const int32_t bytesPerRow = (glyphWidth + 6) / 8;
  const uint8_t* glyph = chrtbl_f16[index];
  for (int32_t row = 0; row < FONT_2_HEIGHT; ++row) {
    fillRect(static_cast<int16_t>(x),
             static_cast<int16_t>(y + row * textSize_),
             static_cast<int16_t>(glyphWidth * textSize_), textSize_,
             textBackground_);
    for (int32_t byteIndex = 0; byteIndex < bytesPerRow; ++byteIndex) {
      const uint8_t bits = glyph[row * bytesPerRow + byteIndex];
      for (uint8_t bit = 0; bit < 8; ++bit) {
        if (bits & static_cast<uint8_t>(0x80U >> bit)) {
          fillRect(static_cast<int16_t>(
                       x + (byteIndex * 8 + bit) * textSize_),
                   static_cast<int16_t>(y + row * textSize_), textSize_,
                   textSize_, textColor_);
        }
      }
    }
  }
}

}  // namespace ui::test
