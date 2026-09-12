# Display renderer migration baseline

This baseline was recorded from commit `f7f78e2` before introducing the shared
display renderer. It documents the behavior that the first migration milestone
must preserve.

## Supported display profiles

| Profile | Logical size | Sprite storage | Baseline static RAM | Baseline flash |
| --- | ---: | --- | ---: | ---: |
| `lilygo-main` | 320 x 170 | Internal RAM, RGB565 | 24,660 bytes | 364,117 bytes |
| `esp32s3-ili9488-main` | 480 x 320 | PSRAM, RGB565 | 23,500 bytes | 380,901 bytes |

Both baseline builds use one 16-bit `TFT_eSprite`. The LilyGo profile does not
enable `PSRAM_ENABLE`; the ILI9488 profile does. Sprite creation sizes are
320 x 170 and 480 x 320 respectively. No second framebuffer exists.

The baseline build artifacts were copied outside the repository to
`/tmp/askompu-renderer-baseline/<profile>/` for local before/after comparison.

## Time-entry screen baseline

`StartupTimeEntry` and `TimeEdit` share `DisplayView::showTimeEntry`.

| Element | 320 x 170 | 480 x 320 |
| --- | ---: | ---: |
| Horizontal center | 160 | 240 |
| Title position | center, y = 12 | center, y = 12 |
| Title datum/font | top-center, face 2, scale 1 | top-center, face 2, scale 1 |
| Value position | center, y = 78 | center, y = 147 |
| Value datum/font | middle-center, face 2, scale 3 | middle-center, face 2, scale 4 |
| Footer baseline | y = 164 | y = 314 |

The title is `ASETA KELLONAIKA` for startup entry and `MUUTA KELLONAIKA`
otherwise. The value is `[%02u]:%02u` for the hour field and `%02u:[%02u]`
for the minute field. The footer uses `YLOS/ALAS: MUUTA` and `OIKEA: JATKA`;
when labels are shown the existing footer logic draws only `MUUTA` and `JATKA`.

Entering either screen, changing color, changing label visibility, or changing
menu font size transfers the full sprite:

- 320 x 170: 54,400 pixels, 108,800 RGB565 bytes
- 480 x 320: 153,600 pixels, 307,200 RGB565 bytes

Subsequent renders on the same time-entry screen transfer exactly one cropped
region, even if the model value did not change:

- 320 x 170: `(0, 45, 320, 80)`, 25,600 pixels, 51,200 RGB565 bytes
- 480 x 320: `(0, 80, 480, 130)`, 62,400 pixels, 124,800 RGB565 bytes

The sprite is cleared in full before drawing this screen. Rendering and transfer
timings were not measured because no instrumented physical target was available.

## Menu baseline

The menu uses three visible rows. For 320 x 170 the content starts at y = 30
and each row is 44 pixels high. For 480 x 320 it starts at y = 56 and each row
is 85 pixels high. The selection marker is a 5-pixel-wide vertical bar at x = 5
with a 5-pixel top inset and a 7-pixel bottom inset.

Entering the menu or changing its title, text color, label visibility, or menu
font size transfers the full sprite. A row-content or scroll-offset change
transfers `(0, 30, 320, 140)` or `(0, 56, 480, 264)`. A selection-only change
transfers two narrow regions: `(0, oldY, 12, rowHeight)` and
`(0, newY, 12, rowHeight)`. The possible row y values are 30, 74 and 118 for
320 x 170, and 56, 141 and 226 for 480 x 320.
