#pragma once

#include <cstdint>
#include <vector>

#include "ui/GraphicsSurface.h"

namespace ui::test {

// Host-test-only RGB565 drawing surface. It mirrors the TFT_eSprite operations
// currently needed by the migrated DisplayRenderer paths.
class FramebufferCanvas {
 public:
  FramebufferCanvas(uint16_t width, uint16_t height);

  uint16_t width() const { return width_; }
  uint16_t height() const { return height_; }
  const std::vector<DisplayColor>& pixels() const { return pixels_; }

  void clear(DisplayColor color);
  void fillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                DisplayColor color);
  void fillRoundRect(int16_t x, int16_t y, int16_t width, int16_t height,
                     int16_t radius, DisplayColor color);
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    int16_t x2, int16_t y2, DisplayColor color);
  void setTextColor(DisplayColor foreground, DisplayColor background);
  void setTextDatum(TextDatum datum);
  void setTextSize(uint8_t size);
  int16_t drawString(const char* text, int32_t x, int32_t y,
                     uint8_t fontFace);
  int16_t textWidth(const char* text, uint8_t fontFace) const;

 private:
  void fillCircleHelper(int16_t x, int16_t y, int16_t radius,
                        uint8_t corners, int16_t delta, DisplayColor color);
  int16_t drawCharacter(uint8_t character, int32_t x, int32_t y,
                        uint8_t fontFace);
  void drawGlcdCharacter(uint8_t character, int32_t x, int32_t y);
  void drawFont2Character(uint8_t character, int32_t x, int32_t y);
  int16_t fontHeight(uint8_t fontFace) const;

  uint16_t width_;
  uint16_t height_;
  std::vector<DisplayColor> pixels_;
  DisplayColor textColor_ = 0xFFFF;
  DisplayColor textBackground_ = 0x0000;
  TextDatum textDatum_ = TextDatum::TopLeft;
  uint8_t textSize_ = 1;
};

}  // namespace ui::test
