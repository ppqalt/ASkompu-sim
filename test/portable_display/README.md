# Portable display golden tests

These host-only tests render the shared `DisplayRenderer` into a native-size
RGB565 framebuffer. They are intentionally independent of SDL, ImGui and the
embedded PlatformIO targets.

The current rasterizer implements only TFT_eSPI fonts 1 and 2 because those are
the fonts used by the migrated time-entry screen and its footer. The exact font
tables are copied byte-for-byte from the firmware's locked TFT_eSPI 2.5.43
dependency under `tft_espi_2_5_43/`. That directory includes TFT_eSPI's complete
license and attribution text.

Build and run on a normal Linux host:

```sh
cmake -S test/portable_display -B /tmp/askompu-portable-display
cmake --build /tmp/askompu-portable-display
ctest --test-dir /tmp/askompu-portable-display --output-on-failure
```

Golden buffers use big-endian bytes for each logical RGB565 pixel, in row-major
order. To intentionally regenerate them after reviewing a renderer change:

```sh
/tmp/askompu-portable-display/display_renderer_golden_tests --update-goldens
```

On a mismatch, the test writes the actual RGB565 buffer, a viewable PPM image,
and a difference PPM into `golden-failures/` below the process working directory.
It also prints the first differing pixel coordinates and values.
