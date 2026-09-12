# Golden framebuffer format

Each `.rgb565` file is a native-resolution, row-major framebuffer. Every pixel
is stored as two bytes, most significant byte first. The dimensions and render
inputs are defined in `DisplayRendererGoldenTests.cpp`.

These files were generated from the portable implementation of the exact
TFT_eSPI 2.5.43 font 1 and font 2 tables and checked visually as PPM images.
