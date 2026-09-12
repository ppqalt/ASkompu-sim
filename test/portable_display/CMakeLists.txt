cmake_minimum_required(VERSION 3.16)
project(ASkompuPortableDisplayTests LANGUAGES CXX)

enable_testing()

add_executable(display_renderer_golden_tests
  FramebufferCanvas.cpp
  DisplayRendererGoldenTests.cpp)
target_compile_features(display_renderer_golden_tests PRIVATE cxx_std_17)
target_include_directories(display_renderer_golden_tests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
target_compile_definitions(display_renderer_golden_tests PRIVATE
  ASKOMPU_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/goldens")

if(MSVC)
  target_compile_options(display_renderer_golden_tests PRIVATE /W4 /WX)
else()
  target_compile_options(display_renderer_golden_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME display_renderer_goldens COMMAND display_renderer_golden_tests)
