#pragma once

#include <TFT_eSPI.h>

#include "GraphicsSurface.h"

namespace ui {

static_assert(static_cast<uint8_t>(TextDatum::TopLeft) == TL_DATUM,
              "Text datum mapping must match TFT_eSPI");
static_assert(static_cast<uint8_t>(TextDatum::TopCenter) == TC_DATUM,
              "Text datum mapping must match TFT_eSPI");
static_assert(static_cast<uint8_t>(TextDatum::TopRight) == TR_DATUM,
              "Text datum mapping must match TFT_eSPI");
static_assert(static_cast<uint8_t>(TextDatum::MiddleLeft) == ML_DATUM,
              "Text datum mapping must match TFT_eSPI");
static_assert(static_cast<uint8_t>(TextDatum::MiddleCenter) == MC_DATUM,
              "Text datum mapping must match TFT_eSPI");
static_assert(static_cast<uint8_t>(TextDatum::MiddleRight) == MR_DATUM,
              "Text datum mapping must match TFT_eSPI");
static_assert(static_cast<uint8_t>(TextDatum::BottomLeft) == BL_DATUM,
              "Text datum mapping must match TFT_eSPI");
static_assert(static_cast<uint8_t>(TextDatum::BottomCenter) == BC_DATUM,
              "Text datum mapping must match TFT_eSPI");
static_assert(static_cast<uint8_t>(TextDatum::BottomRight) == BR_DATUM,
              "Text datum mapping must match TFT_eSPI");

class TftCanvas {
 public:
  explicit TftCanvas(TFT_eSprite& canvas) : canvas_(canvas) {}

  void clear(DisplayColor color) { canvas_.fillSprite(color); }
  void fillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                DisplayColor color) {
    canvas_.fillRect(x, y, width, height, color);
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t width, int16_t height,
                     int16_t radius, DisplayColor color) {
    canvas_.fillRoundRect(x, y, width, height, radius, color);
  }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    int16_t x2, int16_t y2, DisplayColor color) {
    canvas_.fillTriangle(x0, y0, x1, y1, x2, y2, color);
  }
  void setTextColor(DisplayColor foreground, DisplayColor background) {
    canvas_.setTextColor(foreground, background);
  }
  void setTextDatum(TextDatum datum) {
    canvas_.setTextDatum(static_cast<uint8_t>(datum));
  }
  void setTextSize(uint8_t size) { canvas_.setTextSize(size); }
  int16_t drawString(const char* text, int32_t x, int32_t y,
                     uint8_t fontFace) {
    return canvas_.drawString(text, x, y, fontFace);
  }
  int16_t textWidth(const char* text, uint8_t fontFace) {
    return canvas_.textWidth(text, fontFace);
  }
  void present() { canvas_.pushSprite(0, 0); }
  void present(int32_t x, int32_t y, int32_t sourceX, int32_t sourceY,
               int32_t width, int32_t height) {
    canvas_.pushSprite(x, y, sourceX, sourceY, width, height);
  }

 private:
  TFT_eSprite& canvas_;
};

}  // namespace ui
