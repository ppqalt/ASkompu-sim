cmake_minimum_required(VERSION 3.25)
# CMP0091 on NEW ennen ensimmäistä project()-kutsua. Sama CRT kaikille kohteille.
option(ASKOMPU_STATIC_MSVC_RUNTIME "Linkitä MSVC-ajonaikaiskirjasto staattisesti" ON)
if(ASKOMPU_STATIC_MSVC_RUNTIME)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
project(ASkompuSimulator VERSION 0.2.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_compile_options(/utf-8)
endif()

set(ASKOMPU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(askompu_core STATIC
  ${ASKOMPU_ROOT}/src/core/ApplicationCore.cpp
  ${ASKOMPU_ROOT}/src/core/Clock.cpp
  ${ASKOMPU_ROOT}/src/domain/CalibrationSetting.cpp
  ${ASKOMPU_ROOT}/src/domain/CompetitionEngine.cpp
  ${ASKOMPU_ROOT}/src/domain/CompetitionRecords.cpp
  ${ASKOMPU_ROOT}/src/domain/EventRepository.cpp
  ${ASKOMPU_ROOT}/src/domain/MotionMath.cpp
  ${ASKOMPU_ROOT}/src/domain/RouteOrder.cpp
  ${ASKOMPU_ROOT}/src/domain/SpeedCalculator.cpp
  ${ASKOMPU_ROOT}/src/domain/TripCounter.cpp
  ${ASKOMPU_ROOT}/src/route/RouteOrderCodec.cpp
  ${ASKOMPU_ROOT}/src/route/RouteOrderEditor.cpp
)
target_include_directories(askompu_core PUBLIC
  ${ASKOMPU_ROOT}/src ${ASKOMPU_ROOT}/include)

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
  ${ASKOMPU_ROOT}/src/ui/RouteOrderFormatting.cpp)
target_link_libraries(askompu_desktop_controller PUBLIC askompu_simulator)

option(ASKOMPU_BUILD_GUI "Rakenna graafinen ASkompu-simulaattori" OFF)
if(ASKOMPU_BUILD_GUI)
  include(cmake/DesktopDependencies.cmake)
  add_executable(askompu-simulaattori
    ui/Main.cpp ui/SimulatorView.cpp ui/DeviceDisplay.cpp ui/GuiSmokeCheck.cpp
    ${ASKOMPU_ROOT}/src/ui/DisplayLayout.cpp)
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
  target_link_libraries(askompu-simulaattori PRIVATE askompu_desktop_controller askompu_imgui)
  include(cmake/BuildInformation.cmake)
  include(cmake/Packaging.cmake)
endif()

if(BUILD_TESTING)
  add_executable(controller_tests tests/AppControllerTests.cpp)
  target_link_libraries(controller_tests PRIVATE askompu_desktop_controller)
  add_test(NAME kayttoliittyman_ohjaus COMMAND controller_tests)
  add_executable(simulator_tests tests/SimulatorEngineTests.cpp)
  target_link_libraries(simulator_tests PRIVATE askompu_simulator)
  add_test(NAME simulaattori COMMAND simulator_tests)

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
      ${ASKOMPU_ROOT}/src/input/ButtonInterpreter.cpp
      ${ASKOMPU_ROOT}/src/ui/DisplayLayout.cpp
      ${ASKOMPU_ROOT}/src/ui/RouteOrderFormatting.cpp)
    target_link_libraries(askompu_host_adapters PUBLIC askompu_core)
    foreach(suite motion_math competition competition_records display_layout board_profiles)
      add_executable(existing_${suite} "${ASKOMPU_ROOT}/test/test_${suite}/test_main.cpp")
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
