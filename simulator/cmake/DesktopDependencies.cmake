include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

option(ASKOMPU_USE_SYSTEM_SDL "Käytä järjestelmään asennettua SDL2-kirjastoa" OFF)
if(ASKOMPU_USE_SYSTEM_SDL)
  find_package(SDL2 2.0.18 REQUIRED CONFIG)
  set(ASKOMPU_SDL_TARGET SDL2::SDL2)
else()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
  set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
  set(SDL_FORCE_STATIC_VCRT ${ASKOMPU_STATIC_MSVC_RUNTIME} CACHE BOOL "" FORCE)
  FetchContent_Declare(sdl
    URL https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-2.32.10
    URL_HASH SHA256=03f9d7c191a837525c9cda6406af2f2e48be02b5e7eb03d949cc9f1e9ca41c8b)
  FetchContent_MakeAvailable(sdl)
  set(ASKOMPU_SDL_TARGET SDL2::SDL2-static)
endif()

FetchContent_Declare(imgui
  URL https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.91.9b
  URL_HASH SHA256=8e1bbc76c71d74fef2fb85db7e7ca8eba13d6a86623c54992b60162db554ffdb)
FetchContent_MakeAvailable(imgui)
add_library(askompu_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp)
target_include_directories(askompu_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(askompu_imgui PUBLIC ${ASKOMPU_SDL_TARGET})

# Fontti sisällytetään ohjelmaan: käyttö ei riipu työhakemistosta tai koneen fonteista.
file(READ "${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf" ASKOMPU_FONT_HEX HEX)
string(REGEX REPLACE "(..)" "0x\\1,\n" ASKOMPU_FONT_BYTES "${ASKOMPU_FONT_HEX}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_LIST_DIR}/EmbeddedFont.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedFont.h" @ONLY)
