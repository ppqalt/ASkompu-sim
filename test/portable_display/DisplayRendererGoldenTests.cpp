#include "FramebufferCanvas.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ui/DisplayRenderer.h"

namespace {

using ui::DisplayColor;
using ui::TextDatum;
using ui::test::FramebufferCanvas;

struct GoldenCase {
  const char* name;
  uint16_t width;
  uint16_t height;
  DisplayColor background;
  DisplayColor foreground;
  core::TimeEntryDisplayModel model;
};

struct MenuGoldenCase {
  const char* name;
  uint16_t width;
  uint16_t height;
  DisplayColor background;
  DisplayColor foreground;
  core::MenuDisplayModel model;
};

struct RecordedOperation {
  std::string name;
  std::string text;
  int32_t first;
  int32_t second;
  int32_t third;
  int32_t fourth = 0;
  int32_t fifth = 0;
  int32_t sixth = 0;
};

class RecordingSurface {
 public:
  void setTextColor(DisplayColor foreground, DisplayColor background) {
    operations.push_back(
        {"color", "", static_cast<int32_t>(foreground),
         static_cast<int32_t>(background), 0});
  }
  void setTextDatum(TextDatum datum) {
    operations.push_back(
        {"datum", "", static_cast<int32_t>(datum), 0, 0});
  }
  void setTextSize(uint8_t size) {
    operations.push_back({"size", "", size, 0, 0});
  }
  int16_t drawString(const char* text, int32_t x, int32_t y,
                     uint8_t fontFace) {
    operations.push_back({"text", text, x, y, fontFace});
    return 0;
  }
  void fillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                DisplayColor color) {
    operations.push_back({"rect", "", x, y, width, height, color});
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t width, int16_t height,
                     int16_t radius, DisplayColor color) {
    operations.push_back(
        {"roundRect", "", x, y, width, height, radius, color});
  }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    int16_t x2, int16_t y2, DisplayColor) {
    operations.push_back({"triangle", "", x0, y0, x1, y1, x2, y2});
  }

  std::vector<RecordedOperation> operations;
};

[[noreturn]] void fail(const std::string& message) {
  throw std::runtime_error(message);
}

void expect(bool condition, const std::string& message) {
  if (!condition) fail(message);
}

std::vector<DisplayColor> render(const GoldenCase& goldenCase) {
  FramebufferCanvas canvas(goldenCase.width, goldenCase.height);
  canvas.clear(goldenCase.background);
  ui::DisplayRenderer<FramebufferCanvas>::renderTimeEntry(
      canvas, goldenCase.width, goldenCase.height, goldenCase.background,
      goldenCase.model, goldenCase.foreground, true,
      domain::MenuFontSize::MEDIUM);
  return canvas.pixels();
}

std::vector<DisplayColor> render(const MenuGoldenCase& goldenCase) {
  FramebufferCanvas canvas(goldenCase.width, goldenCase.height);
  canvas.clear(goldenCase.background);
  ui::DisplayRenderer<FramebufferCanvas>::renderMenu(
      canvas, goldenCase.width, goldenCase.height, goldenCase.background,
      goldenCase.model, goldenCase.foreground, domain::MenuFontSize::MEDIUM);
  return canvas.pixels();
}

void writeRgb565(const std::filesystem::path& path,
                 const std::vector<DisplayColor>& pixels) {
  std::ofstream output(path, std::ios::binary);
  if (!output) fail("Cannot write " + path.string());
  for (DisplayColor pixel : pixels) {
    output.put(static_cast<char>(pixel >> 8));
    output.put(static_cast<char>(pixel & 0xFF));
  }
}

std::vector<DisplayColor> readRgb565(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) fail("Cannot read golden " + path.string());
  std::vector<DisplayColor> pixels;
  char high = 0;
  char low = 0;
  while (input.get(high)) {
    if (!input.get(low)) fail("Golden has an odd byte count: " + path.string());
    pixels.push_back(static_cast<DisplayColor>(
        static_cast<uint16_t>(static_cast<uint8_t>(high)) << 8 |
        static_cast<uint8_t>(low)));
  }
  return pixels;
}

void writePpm(const std::filesystem::path& path,
              const std::vector<DisplayColor>& pixels, uint16_t width,
              uint16_t height) {
  std::ofstream output(path, std::ios::binary);
  if (!output) fail("Cannot write " + path.string());
  output << "P6\n" << width << ' ' << height << "\n255\n";
  for (DisplayColor pixel : pixels) {
    const uint8_t red5 = static_cast<uint8_t>((pixel >> 11) & 0x1F);
    const uint8_t green6 = static_cast<uint8_t>((pixel >> 5) & 0x3F);
    const uint8_t blue5 = static_cast<uint8_t>(pixel & 0x1F);
    output.put(static_cast<char>((red5 << 3) | (red5 >> 2)));
    output.put(static_cast<char>((green6 << 2) | (green6 >> 4)));
    output.put(static_cast<char>((blue5 << 3) | (blue5 >> 2)));
  }
}

void writeDiffPpm(const std::filesystem::path& path,
                  const std::vector<DisplayColor>& expected,
                  const std::vector<DisplayColor>& actual, uint16_t width,
                  uint16_t height) {
  std::ofstream output(path, std::ios::binary);
  if (!output) fail("Cannot write " + path.string());
  output << "P6\n" << width << ' ' << height << "\n255\n";
  for (size_t index = 0; index < actual.size(); ++index) {
    if (index >= expected.size() || expected[index] != actual[index]) {
      output.put(static_cast<char>(255));
      output.put(static_cast<char>(0));
      output.put(static_cast<char>(255));
    } else {
      const uint8_t level = actual[index] == 0 ? 0 : 48;
      output.put(static_cast<char>(level));
      output.put(static_cast<char>(level));
      output.put(static_cast<char>(level));
    }
  }
}

void verifyOperationContract(uint16_t width, uint16_t height,
                             int16_t expectedValueY,
                             uint8_t expectedValueScale) {
  RecordingSurface surface;
  const core::TimeEntryDisplayModel model{
      7, 5, core::TimeField::Hour, true};
  ui::DisplayRenderer<RecordingSurface>::renderTimeEntry(
      surface, width, height, 0x0123, model, 0x4567, true,
      domain::MenuFontSize::MEDIUM);
  const auto& operations = surface.operations;
  expect(operations.size() == 14, "Unexpected rendering operation count");
  expect(operations[0].name == "datum" && operations[0].first == 1,
         "Title datum changed");
  expect(operations[1].name == "color" && operations[1].first == 0x4567 &&
             operations[1].second == 0x0123,
         "Title colors changed");
  expect(operations[2].name == "text" &&
             operations[2].text == "ASETA KELLONAIKA" &&
             operations[2].first == width / 2 && operations[2].second == 12 &&
             operations[2].third == 2,
         "Title draw operation changed");
  expect(operations[3].name == "size" &&
             operations[3].first == expectedValueScale,
         "Value scale changed");
  expect(operations[4].name == "datum" && operations[4].first == 4,
         "Value datum changed");
  expect(operations[6].name == "text" && operations[6].text == "[07]:05" &&
             operations[6].first == width / 2 &&
             operations[6].second == expectedValueY &&
             operations[6].third == 2,
         "Value draw operation changed");
  expect(operations[10].name == "text" && operations[10].text == "MUUTA" &&
             operations[10].first == width / 2 &&
             operations[10].second == height - 6,
         "Center footer draw operation changed");
  expect(operations[12].name == "datum" && operations[12].first == 8,
         "Right footer datum changed");
  expect(operations[13].name == "text" && operations[13].text == "JATKA" &&
             operations[13].first == width - 4 &&
             operations[13].second == height - 6,
         "Right footer draw operation changed");
}

core::MenuDisplayModel menuModel(const char* first, const char* second,
                                 const char* third, uint8_t selectedRow,
                                 uint8_t scrollOffset, uint8_t totalRows,
                                 bool thirdEnabled = true) {
  core::MenuDisplayModel model{};
  model.title = "PAAVALIKKO";
  model.rows[0] = {first, true};
  model.rows[1] = {second, true};
  model.rows[2] = {third, thirdEnabled};
  model.visibleRowCount = 3;
  model.selectedVisibleRow = selectedRow;
  model.selectedIndex = static_cast<uint8_t>(scrollOffset + selectedRow);
  model.totalRows = totalRows;
  model.scrollOffset = scrollOffset;
  return model;
}

void expectRegion(const ui::TransferRegion& region, int16_t x, int16_t y,
                  int16_t width, int16_t height, const char* message) {
  expect(region.x == x && region.y == y && region.width == width &&
             region.height == height,
         message);
}

void verifyMenuTransferPlans(uint16_t width, uint16_t height, int16_t top,
                             int16_t rowHeight) {
  using Renderer = ui::DisplayRenderer<RecordingSurface>;
  auto plan = Renderer::menuTransferPlan(width, height, true, false, false, 0, 0);
  expect(plan.count == 1, "Initial menu transfer count changed");
  expectRegion(plan.regions[0], 0, 0, width, height,
               "Initial menu transfer changed");

  plan = Renderer::menuTransferPlan(width, height, false, true, false, 0, 0);
  expect(plan.count == 1, "Menu title transfer count changed");
  expectRegion(plan.regions[0], 0, 0, width, height,
               "Menu title transfer changed");

  plan = Renderer::menuTransferPlan(width, height, false, false, true, 0, 0);
  expect(plan.count == 1, "Menu scroll transfer count changed");
  expectRegion(plan.regions[0], 0, top, width, height - top,
               "Menu scroll transfer changed");

  plan = Renderer::menuTransferPlan(width, height, false, false, false, 0, 2);
  expect(plan.count == 2, "Selection-only transfer count changed");
  expectRegion(plan.regions[0], 0, top, 12, rowHeight,
               "Old selection transfer changed");
  expectRegion(plan.regions[1], 0, top + 2 * rowHeight, 12, rowHeight,
               "New selection transfer changed");

  plan = Renderer::menuTransferPlan(width, height, false, false, false, 1, 1);
  expect(plan.count == 0, "Unchanged menu unexpectedly transfers pixels");
}

void verifySelectionPixelDifferences(uint16_t width, uint16_t height,
                                     DisplayColor background,
                                     DisplayColor foreground, int16_t top,
                                     int16_t rowHeight) {
  const MenuGoldenCase first{
      "selection-first", width, height, background, foreground,
      menuModel("AJOMAARAYS", "KELLO", "KERROIN", 0, 0, 7)};
  const MenuGoldenCase third{
      "selection-third", width, height, background, foreground,
      menuModel("AJOMAARAYS", "KELLO", "KERROIN", 2, 0, 7)};
  const std::vector<DisplayColor> before = render(first);
  const std::vector<DisplayColor> after = render(third);
  size_t changedPixels = 0;
  for (size_t index = 0; index < before.size(); ++index) {
    if (before[index] == after[index]) continue;
    const int16_t x = static_cast<int16_t>(index % width);
    const int16_t y = static_cast<int16_t>(index / width);
    const bool inOldRegion = x < 12 && y >= top && y < top + rowHeight;
    const int16_t newTop = top + 2 * rowHeight;
    const bool inNewRegion = x < 12 && y >= newTop && y < newTop + rowHeight;
    expect(inOldRegion || inNewRegion,
           "Selection-only rendering changed pixels outside dirty regions");
    ++changedPixels;
  }
  expect(changedPixels > 0, "Selection-only rendering changed no pixels");
}

void verifyMenuOperationContract(uint16_t width, uint16_t height, int16_t top,
                                 int16_t rowHeight) {
  RecordingSurface surface;
  const core::MenuDisplayModel model = menuModel(
      "KERROIN", "PISTEET JA LOKI", "PAINIKEASETUKSET", 0, 2, 7, false);
  ui::DisplayRenderer<RecordingSurface>::renderMenu(
      surface, width, height, 0x0123, model, 0x4567,
      domain::MenuFontSize::MEDIUM);
  const auto& operations = surface.operations;
  expect(operations.size() == 24, "Unexpected menu operation count");
  expect(operations[0].name == "datum" && operations[0].first == 1,
         "Menu title datum changed");
  expect(operations[3].name == "text" &&
             operations[3].text == "PAAVALIKKO" &&
             operations[3].first == width / 2 && operations[3].second == 5 &&
             operations[3].third == 2,
         "Menu title draw changed");
  expect(operations[4].name == "rect" && operations[4].first == 5 &&
             operations[4].second == top &&
             operations[4].third == width - 10 &&
             operations[4].fourth == rowHeight - 2,
         "Menu row clear changed");
  expect(operations[5].name == "roundRect" && operations[5].first == 5 &&
             operations[5].second == top + 5 && operations[5].third == 5 &&
             operations[5].fourth == rowHeight - 12 &&
             operations[5].fifth == 2 && operations[5].sixth == 0x4567,
         "Menu selection marker changed");
  expect(operations[14].name == "text" &&
             operations[14].text == "PISTEET JA LOKI" &&
             operations[14].first == 18 &&
             operations[14].second == top + rowHeight + rowHeight / 2,
         "Long menu label placement changed");
  expect(operations[20].name == "datum" && operations[20].first == 5 &&
             operations[21].name == "text" && operations[21].text == "--" &&
             operations[21].first == width - 13,
         "Disabled menu row marker changed");
  expect(operations[22].name == "triangle" &&
             operations[22].first == width - 10 &&
             operations[22].second == top + 2,
         "Upper scroll indicator changed");
  expect(operations[23].name == "triangle" &&
             operations[23].first == width - 10 &&
             operations[23].second == height - 4,
         "Lower scroll indicator changed");
}

void verifyFramebufferSemantics() {
  FramebufferCanvas canvas(320, 170);
  canvas.setTextSize(1);
  expect(canvas.textWidth("ASETA KELLONAIKA", 2) == 120,
         "TFT font 2 title width changed");
  expect(canvas.textWidth("MUUTA", 1) == 30,
         "TFT GLCD footer width changed");
  canvas.setTextSize(3);
  expect(canvas.textWidth("[07]:05", 2) == 129,
         "Scaled TFT font 2 value width changed");
}

template <typename Case>
void compareGolden(const Case& goldenCase, bool updateGoldens) {
  const std::filesystem::path goldenPath =
      std::filesystem::path(ASKOMPU_GOLDEN_DIR) /
      (std::string(goldenCase.name) + ".rgb565");
  const std::vector<DisplayColor> actual = render(goldenCase);
  const size_t expectedPixelCount =
      static_cast<size_t>(goldenCase.width) * goldenCase.height;
  expect(actual.size() == expectedPixelCount,
         std::string(goldenCase.name) + " produced a wrong framebuffer size");
  expect(render(goldenCase) == actual,
         std::string(goldenCase.name) + " is not deterministic");

  size_t foregroundPixels = 0;
  for (DisplayColor pixel : actual) {
    expect(pixel == goldenCase.background || pixel == goldenCase.foreground,
           std::string(goldenCase.name) + " emitted an unexpected RGB565 color");
    if (pixel == goldenCase.foreground) ++foregroundPixels;
  }
  expect(foregroundPixels > 0,
         std::string(goldenCase.name) + " did not draw foreground pixels");

  if (updateGoldens) {
    writeRgb565(goldenPath, actual);
    std::cout << "Updated " << goldenPath << '\n';
    return;
  }

  const std::vector<DisplayColor> expected = readRgb565(goldenPath);
  if (expected == actual) return;

  const std::filesystem::path failureDirectory =
      std::filesystem::current_path() / "golden-failures";
  std::filesystem::create_directories(failureDirectory);
  const std::filesystem::path base = failureDirectory / goldenCase.name;
  writeRgb565(base.string() + ".actual.rgb565", actual);
  writePpm(base.string() + ".actual.ppm", actual, goldenCase.width,
           goldenCase.height);
  writeDiffPpm(base.string() + ".diff.ppm", expected, actual,
               goldenCase.width, goldenCase.height);

  size_t differences = 0;
  std::ostringstream details;
  for (size_t index = 0; index < actual.size(); ++index) {
    if (index >= expected.size() || expected[index] != actual[index]) {
      if (differences < 12) {
        details << " (" << index % goldenCase.width << ','
                << index / goldenCase.width << ": expected 0x" << std::hex
                << std::setw(4) << std::setfill('0')
                << (index < expected.size() ? expected[index] : 0)
                << ", actual 0x" << std::setw(4) << actual[index] << std::dec
                << ')';
      }
      ++differences;
    }
  }
  fail(std::string(goldenCase.name) + " differs at " +
       std::to_string(differences) + " pixels." + details.str() +
       " Inspection files: " + failureDirectory.string());
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const bool updateGoldens =
        argc == 2 && std::string(argv[1]) == "--update-goldens";
    if (argc > 2 || (argc == 2 && !updateGoldens))
      fail("Usage: display_renderer_golden_tests [--update-goldens]");

    verifyOperationContract(320, 170, 78, 3);
    verifyOperationContract(480, 320, 147, 4);
    verifyMenuOperationContract(320, 170, 30, 44);
    verifyMenuOperationContract(480, 320, 56, 85);
    verifyMenuTransferPlans(320, 170, 30, 44);
    verifyMenuTransferPlans(480, 320, 56, 85);
    verifySelectionPixelDifferences(320, 170, 0x0000, 0xFFFF, 30, 44);
    verifySelectionPixelDifferences(480, 320, 0xFFFF, 0x0000, 56, 85);
    verifyFramebufferSemantics();

    const GoldenCase cases[] = {
        {"time-entry-320x170-startup-white", 320, 170, 0x0000, 0xFFFF,
         {7, 5, core::TimeField::Hour, true}},
        {"time-entry-320x170-edit-red", 320, 170, 0x0000, 0xF800,
         {23, 59, core::TimeField::Minute, false}},
        {"time-entry-480x320-startup-panel-white", 480, 320, 0xFFFF, 0x0000,
         {0, 0, core::TimeField::Hour, true}},
        {"time-entry-480x320-edit-panel-green", 480, 320, 0xFFFF, 0xF81F,
         {12, 34, core::TimeField::Minute, false}},
    };
    for (const GoldenCase& goldenCase : cases)
      compareGolden(goldenCase, updateGoldens);

    const MenuGoldenCase menuCases[] = {
        {"menu-320x170-initial-first-selected", 320, 170, 0x0000, 0xFFFF,
         menuModel("AJOMAARAYS", "KELLO", "KERROIN", 0, 0, 7)},
        {"menu-320x170-selection-row-three", 320, 170, 0x0000, 0xFFFF,
         menuModel("AJOMAARAYS", "KELLO", "KERROIN", 2, 0, 7)},
        {"menu-320x170-scrolled-long-labels", 320, 170, 0x0000, 0xFFFF,
         menuModel("KERROIN", "PISTEET JA LOKI", "PAINIKEASETUKSET", 1, 2,
                   7, false)},
        {"menu-480x320-initial-first-selected", 480, 320, 0xFFFF, 0x0000,
         menuModel("AJOMAARAYS", "KELLO", "KERROIN", 0, 0, 7)},
        {"menu-480x320-selection-row-three", 480, 320, 0xFFFF, 0x0000,
         menuModel("AJOMAARAYS", "KELLO", "KERROIN", 2, 0, 7)},
        {"menu-480x320-scrolled-long-labels", 480, 320, 0xFFFF, 0x0000,
         menuModel("KERROIN", "PISTEET JA LOKI", "PAINIKEASETUKSET", 1, 2,
                   7, false)},
    };
    for (const MenuGoldenCase& goldenCase : menuCases)
      compareGolden(goldenCase, updateGoldens);

    std::cout << "Portable display contract and golden tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAILED: " << error.what() << '\n';
    return 1;
  }
}
