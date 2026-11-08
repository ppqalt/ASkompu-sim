cmake_minimum_required(VERSION 3.25)
# CMP0091 on NEW ennen ensimmäistä project()-kutsua. Sama CRT kaikille kohteille.
option(ASKOMPU_STATIC_MSVC_RUNTIME "Linkitä MSVC-ajonaikaiskirjasto staattisesti" ON)
if(ASKOMPU_STATIC_MSVC_RUNTIME)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
project(ASkompuSimulator VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_compile_options(/utf-8)
endif()

get_filename_component(ASKOMPU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(ASKOMPU_CORE_ROOT "${ASKOMPU_ROOT}" CACHE PATH "Valitun ASkompu-ytimen lähdehakemisto")
set(ASKOMPU_CORE_REVISION "" CACHE STRING "Erillisen ytimen lukittu täysi Git-SHA")
set(ASKOMPU_CORE_SNAPSHOT "" CACHE FILEPATH "Paikallisen lähdekopion varmennettu manifesti")
file(REAL_PATH "${ASKOMPU_CORE_ROOT}" ASKOMPU_CORE_ROOT)
include(cmake/ValidateCore.cmake)
askompu_validate_core()
set(ASKOMPU_DISPLAY_PROFILE "480x320" CACHE STRING "ASkompun näyttöprofiili")
set_property(CACHE ASKOMPU_DISPLAY_PROFILE PROPERTY STRINGS "320x170" "480x320")
if(ASKOMPU_DISPLAY_PROFILE STREQUAL "320x170")
  set(ASKOMPU_DISPLAY_WIDTH 320)
  set(ASKOMPU_DISPLAY_HEIGHT 170)
elseif(ASKOMPU_DISPLAY_PROFILE STREQUAL "480x320")
  set(ASKOMPU_DISPLAY_WIDTH 480)
  set(ASKOMPU_DISPLAY_HEIGHT 320)
else()
  message(FATAL_ERROR "ASKOMPU_DISPLAY_PROFILE on oltava 320x170 tai 480x320")
endif()
add_library(askompu_core STATIC
  ${ASKOMPU_CORE_ROOT}/src/core/ApplicationCore.cpp
  ${ASKOMPU_CORE_ROOT}/src/core/Clock.cpp
  ${ASKOMPU_CORE_ROOT}/src/domain/CalibrationSetting.cpp
  ${ASKOMPU_CORE_ROOT}/src/domain/CompetitionEngine.cpp
  ${ASKOMPU_CORE_ROOT}/src/domain/CompetitionRecords.cpp
  ${ASKOMPU_CORE_ROOT}/src/domain/EventRepository.cpp
  ${ASKOMPU_CORE_ROOT}/src/domain/MotionMath.cpp
  ${ASKOMPU_CORE_ROOT}/src/domain/RouteOrder.cpp
  ${ASKOMPU_CORE_ROOT}/src/domain/SpeedCalculator.cpp
  ${ASKOMPU_CORE_ROOT}/src/domain/TripCounter.cpp
  ${ASKOMPU_CORE_ROOT}/src/route/RouteOrderCodec.cpp
  ${ASKOMPU_CORE_ROOT}/src/route/RouteOrderEditor.cpp
)
target_include_directories(askompu_core PUBLIC
  ${ASKOMPU_CORE_ROOT}/src ${ASKOMPU_CORE_ROOT}/include)
if(EXISTS "${ASKOMPU_CORE_ROOT}/src/ui/TripFormatting.h")
  target_compile_definitions(askompu_core PUBLIC ASKOMPU_HAS_UPSTREAM_012_UI=1)
else()
  target_compile_definitions(askompu_core PUBLIC ASKOMPU_HAS_UPSTREAM_012_UI=0)
endif()

# REAL comes entirely from the selected ASkompu tree. No firmware graphics are
# copied into this desktop branch; older cores retain the original preview.
set(ASKOMPU_HAS_SHARED_DISPLAY_RENDERER 0)
if(EXISTS "${ASKOMPU_CORE_ROOT}/src/ui/DisplayRenderer.h" AND
   EXISTS "${ASKOMPU_CORE_ROOT}/test/portable_display/FramebufferCanvas.h" AND
   EXISTS "${ASKOMPU_CORE_ROOT}/test/portable_display/FramebufferCanvas.cpp")
  file(STRINGS "${ASKOMPU_CORE_ROOT}/src/ui/DisplayRenderer.h" renderer_api REGEX "renderScreen")
  if(renderer_api)
    set(ASKOMPU_HAS_SHARED_DISPLAY_RENDERER 1)
  endif()
endif()
add_library(askompu_shared_display STATIC ui/SharedDisplayFramebuffer.cpp)
target_link_libraries(askompu_shared_display PUBLIC askompu_core)
target_include_directories(askompu_shared_display PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(askompu_shared_display PUBLIC
  ASKOMPU_HAS_SHARED_DISPLAY_RENDERER=${ASKOMPU_HAS_SHARED_DISPLAY_RENDERER})
if(ASKOMPU_HAS_SHARED_DISPLAY_RENDERER)
  target_sources(askompu_shared_display PRIVATE
    "${ASKOMPU_CORE_ROOT}/test/portable_display/FramebufferCanvas.cpp"
    "${ASKOMPU_CORE_ROOT}/src/ui/DisplayFormatting.cpp"
    "${ASKOMPU_CORE_ROOT}/src/ui/DisplayLayout.cpp"
    "${ASKOMPU_CORE_ROOT}/src/ui/RouteOrderFormatting.cpp")
  target_include_directories(askompu_shared_display PRIVATE
    "${ASKOMPU_CORE_ROOT}/test/portable_display")
endif()

add_library(askompu_simulator STATIC
  engine/SimulatorEngine.cpp engine/WheelPulseGenerator.cpp)
target_include_directories(askompu_simulator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(askompu_simulator PUBLIC askompu_core)
target_compile_features(askompu_simulator PUBLIC cxx_std_17)
if(MSVC)
  target_compile_options(askompu_simulator PRIVATE /W4)
else()
  target_compile_options(askompu_simulator PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
add_library(askompu_desktop_controller STATIC ui/AppController.cpp ui/Finnish.cpp
  ui/FolderBrowser.cpp ui/CoreBuildInfo.cpp ui/SpeedInput.cpp
  ${ASKOMPU_CORE_ROOT}/src/ui/RouteOrderFormatting.cpp)
target_link_libraries(askompu_desktop_controller PUBLIC askompu_simulator)
target_include_directories(askompu_desktop_controller PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/compat")

option(ASKOMPU_BUILD_GUI "Rakenna graafinen ASkompu-simulaattori" OFF)
if(ASKOMPU_BUILD_GUI)
  include(cmake/DesktopDependencies.cmake)
  add_executable(askompu-simulaattori
    ui/Main.cpp ui/SimulatorView.cpp ui/CoreVersionView.cpp ui/CoreVersionSmokeCheck.cpp ui/DeviceDisplay.cpp ui/GuiSmokeCheck.cpp
    ${ASKOMPU_CORE_ROOT}/src/ui/DisplayLayout.cpp)
  target_include_directories(askompu-simulaattori PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
  if(WIN32)
    if(NOT TARGET SDL2::SDL2main)
      message(FATAL_ERROR "Windows-rakennus tarvitsee SDL2::SDL2main-kohteen")
    endif()
    set_target_properties(askompu-simulaattori PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(askompu-simulaattori PRIVATE SDL2::SDL2main)
  else()
    target_compile_definitions(askompu-simulaattori PRIVATE SDL_MAIN_HANDLED)
  endif()
  target_link_libraries(askompu-simulaattori PRIVATE askompu_desktop_controller askompu_imgui askompu_shared_display)
  target_compile_definitions(askompu-simulaattori PRIVATE
    ASKOMPU_DISPLAY_WIDTH=${ASKOMPU_DISPLAY_WIDTH}
    ASKOMPU_DISPLAY_HEIGHT=${ASKOMPU_DISPLAY_HEIGHT})
  include(cmake/BuildInformation.cmake)
  include(cmake/Packaging.cmake)
endif()

if(BUILD_TESTING)
  add_executable(controller_tests tests/AppControllerTests.cpp)
  target_link_libraries(controller_tests PRIVATE askompu_desktop_controller)
  add_test(NAME kayttoliittyman_ohjaus COMMAND controller_tests)
  add_executable(desktop_workflow_tests tests/DesktopWorkflowTests.cpp)
  target_link_libraries(desktop_workflow_tests PRIVATE askompu_desktop_controller)
  add_test(NAME paikallinen_kayttopolku COMMAND desktop_workflow_tests)
  add_executable(speed_input_tests tests/SpeedInputTests.cpp)
  target_link_libraries(speed_input_tests PRIVATE askompu_desktop_controller)
  add_test(NAME nopeussyotto COMMAND speed_input_tests)
  add_executable(simulator_tests tests/SimulatorEngineTests.cpp)
  target_link_libraries(simulator_tests PRIVATE askompu_simulator)
  add_test(NAME simulaattori COMMAND simulator_tests)
  add_executable(shared_display_tests tests/SharedDisplayFramebufferTests.cpp)
  target_link_libraries(shared_display_tests PRIVATE askompu_shared_display)
  add_test(NAME yhteinen_nayttorenderer COMMAND shared_display_tests)

  # Valinnainen: alkuperäiset Unity-testit sellaisinaan, ilman PlatformIO-muutoksia.
  set(ASKOMPU_UNITY_SOURCE_DIR "" CACHE PATH "Unity-kirjaston src-hakemisto")
  option(ASKOMPU_FETCH_UNITY "Hae lukittu Unity nykyisten host-testien ajamiseen" OFF)
  if(ASKOMPU_FETCH_UNITY AND NOT ASKOMPU_UNITY_SOURCE_DIR)
    include(FetchContent)
    FetchContent_Declare(unity_sources
      URL https://codeload.github.com/ThrowTheSwitch/Unity/tar.gz/refs/tags/v2.6.1
      URL_HASH SHA256=b41a66d45a6b99758fb3202ace6178177014d52fc524bf1f72687d93e9867292
      SOURCE_SUBDIR askompu-no-subproject)
    FetchContent_MakeAvailable(unity_sources)
    set(ASKOMPU_UNITY_SOURCE_DIR "${unity_sources_SOURCE_DIR}/src")
  endif()
  if(ASKOMPU_UNITY_SOURCE_DIR)
    if(NOT EXISTS "${ASKOMPU_UNITY_SOURCE_DIR}/unity.c" OR
       NOT EXISTS "${ASKOMPU_UNITY_SOURCE_DIR}/unity.h")
      message(FATAL_ERROR "Unity-hakemistosta puuttuu unity.c tai unity.h")
    endif()
    add_library(askompu_unity STATIC "${ASKOMPU_UNITY_SOURCE_DIR}/unity.c")
    target_include_directories(askompu_unity PUBLIC "${ASKOMPU_UNITY_SOURCE_DIR}")
    target_compile_definitions(askompu_unity PUBLIC UNITY_SUPPORT_64)
    add_library(askompu_host_adapters STATIC
      ${ASKOMPU_CORE_ROOT}/src/input/ButtonInterpreter.cpp
      ${ASKOMPU_CORE_ROOT}/src/ui/DisplayLayout.cpp
      ${ASKOMPU_CORE_ROOT}/src/ui/RouteOrderFormatting.cpp)
    target_link_libraries(askompu_host_adapters PUBLIC askompu_core)
    foreach(suite motion_math competition competition_records display_layout board_profiles)
      add_executable(existing_${suite} "${ASKOMPU_CORE_ROOT}/test/test_${suite}/test_main.cpp")
      target_compile_definitions(existing_${suite} PRIVATE ASKOMPU_BOARD_LILYGO_MAIN=1)
      target_link_libraries(existing_${suite} PRIVATE askompu_host_adapters askompu_unity)
      add_test(NAME nykyinen_${suite} COMMAND existing_${suite})
    endforeach()
  endif()
endif()

# Varoitukset vain omalle desktop-koodille, ei upstreamille tai riippuvuuksille.
foreach(target askompu_desktop_controller askompu-simulaattori controller_tests)
  if(TARGET ${target})
    if(MSVC)
      target_compile_options(${target} PRIVATE /W4)
      target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    else()
      target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
  endif()
endforeach()
