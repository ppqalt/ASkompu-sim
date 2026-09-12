#pragma once

#include <cstdio>
#include <cstring>

#include "GraphicsSurface.h"
#include "core/DisplayModel.h"

namespace ui {

struct TransferRegion {
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
};

struct MenuTransferPlan {
  TransferRegion regions[2]{};
  uint8_t count = 0;
};

#if defined(_MSC_VER)
#define ASKOMPU_RENDER_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define ASKOMPU_RENDER_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define ASKOMPU_RENDER_ALWAYS_INLINE inline
#endif

// Surface is deliberately a template parameter so embedded drawing calls can
// inline directly into TFT_eSprite without a virtual dispatch layer.
template <typename Surface>
class DisplayRenderer {
 public:
  ASKOMPU_RENDER_ALWAYS_INLINE static void renderMenu(
      Surface& surface, uint16_t width, uint16_t height,
      DisplayColor background, const core::MenuDisplayModel& model,
      DisplayColor textColor, domain::MenuFontSize menuFontSize) {
    const bool large = width >= 480;
    const uint8_t textScale = menuTextScale(menuFontSize);
    const int16_t top = large ? 56 : 30;
    const int16_t rowHeight =
        (height - top - 8) / core::MENU_VISIBLE_ROWS;
    surface.setTextDatum(TextDatum::TopCenter);
    surface.setTextColor(textColor, background);
    surface.setTextSize(textScale);
    surface.drawString(model.title, width / 2, 5, 2);
    for (uint8_t row = 0; row < model.visibleRowCount; ++row) {
      const bool selected = row == model.selectedVisibleRow;
      surface.fillRect(5, top + row * rowHeight, width - 10, rowHeight - 2,
                       background);
      if (selected) {
        surface.fillRoundRect(5, top + row * rowHeight + 5, 5,
                              rowHeight - 12, 2, textColor);
      }
      surface.setTextDatum(TextDatum::MiddleLeft);
      surface.setTextColor(textColor, background);
      surface.setTextSize(textScale);
      surface.drawString(model.rows[row].label, 18,
                         top + row * rowHeight + rowHeight / 2, 2);
      if (!model.rows[row].enabled) {
        surface.setTextDatum(TextDatum::MiddleRight);
        surface.drawString("--", width - 13,
                           top + row * rowHeight + rowHeight / 2, 2);
      }
    }
    if (model.scrollOffset > 0) {
      const int16_t x = width - 10;
      surface.fillTriangle(x, top + 2, x - 6, top + 10, x + 6, top + 10,
                           textColor);
    }
    if (model.scrollOffset + model.visibleRowCount < model.totalRows) {
      const int16_t x = width - 10;
      const int16_t y = height - 4;
      surface.fillTriangle(x, y, x - 6, y - 8, x + 6, y - 8, textColor);
    }
  }

  static MenuTransferPlan menuTransferPlan(
      uint16_t width, uint16_t height, bool fullRefresh, bool titleChanged,
      bool rowsChanged, uint8_t oldSelectedRow, uint8_t newSelectedRow) {
    MenuTransferPlan plan;
    if (fullRefresh || titleChanged) {
      plan.regions[0] = {0, 0, static_cast<int16_t>(width),
                         static_cast<int16_t>(height)};
      plan.count = 1;
      return plan;
    }
    const int16_t top = width >= 480 ? 56 : 30;
    if (rowsChanged) {
      plan.regions[0] = {0, top, static_cast<int16_t>(width),
                         static_cast<int16_t>(height - top)};
      plan.count = 1;
      return plan;
    }
    if (oldSelectedRow != newSelectedRow) {
      const int16_t rowHeight =
          (height - top - 8) / core::MENU_VISIBLE_ROWS;
      plan.regions[0] = {
          0, static_cast<int16_t>(top + oldSelectedRow * rowHeight), 12,
          rowHeight};
      plan.regions[1] = {
          0, static_cast<int16_t>(top + newSelectedRow * rowHeight), 12,
          rowHeight};
      plan.count = 2;
    }
    return plan;
  }

  ASKOMPU_RENDER_ALWAYS_INLINE static void drawFooter(
      Surface& surface, uint16_t width, uint16_t height,
      DisplayColor background, const char* left, const char* upDown,
      const char* right, DisplayColor color, bool showLabels,
      domain::MenuFontSize menuFontSize) {
    if (!showLabels) return;
    surface.setTextColor(color, background);
    const int16_t y = height - 6;
    drawFooterLabel(surface, left, TextDatum::BottomLeft, 4, y, menuFontSize);
    drawFooterLabel(surface, upDown, TextDatum::BottomCenter, width / 2, y,
                    menuFontSize);
    drawFooterLabel(surface, right, TextDatum::BottomRight, width - 4, y,
                    menuFontSize);
  }

  ASKOMPU_RENDER_ALWAYS_INLINE static void renderTimeEntry(
      Surface& surface, uint16_t width, uint16_t height,
      DisplayColor background, const core::TimeEntryDisplayModel& model,
      DisplayColor textColor, bool showLabels,
      domain::MenuFontSize menuFontSize) {
    char value[20];
    if (model.activeField == core::TimeField::Hour)
      std::snprintf(value, sizeof(value), "[%02u]:%02u", model.hour,
                    model.minute);
    else
      std::snprintf(value, sizeof(value), "%02u:[%02u]", model.hour,
                    model.minute);
    const int16_t centerX = width / 2;
    const int16_t valueY = height * 46 / 100;
    const uint8_t valueScale = width >= 480 ? 4 : 3;
    surface.setTextDatum(TextDatum::TopCenter);
    surface.setTextColor(textColor, background);
    surface.drawString(model.startup ? "ASETA KELLONAIKA"
                                     : "MUUTA KELLONAIKA",
                       centerX, 12, 2);
    surface.setTextSize(valueScale);
    surface.setTextDatum(TextDatum::MiddleCenter);
    surface.setTextColor(textColor, background);
    surface.drawString(value, centerX, valueY, 2);
    drawFooter(surface, width, height, background, "", "YLOS/ALAS: MUUTA",
               "OIKEA: JATKA", textColor, showLabels, menuFontSize);
  }

 private:
  ASKOMPU_RENDER_ALWAYS_INLINE static uint8_t menuTextScale(
      domain::MenuFontSize size) {
    return static_cast<uint8_t>(size) + 1U;
  }

  static const char* footerLabel(const char* text) {
    if (!text) return "";
    const char* separator = std::strchr(text, ':');
    if (!separator) return text;
    ++separator;
    while (*separator == ' ') ++separator;
    return separator;
  }

  ASKOMPU_RENDER_ALWAYS_INLINE static void drawFooterLabel(
      Surface& surface, const char* text, TextDatum datum, int16_t x, int16_t y,
      domain::MenuFontSize menuFontSize) {
    text = footerLabel(text);
    if (!text[0]) return;
    surface.setTextSize(menuTextScale(menuFontSize));
    surface.setTextDatum(datum);
    surface.drawString(text, x, y, 1);
  }
};

#undef ASKOMPU_RENDER_ALWAYS_INLINE

}  // namespace ui
