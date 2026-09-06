cmake_minimum_required(VERSION 3.16)
project(ASkompuSimulator VERSION 0.1.0 LANGUAGES C CXX)

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
add_library(askompu_desktop_controller STATIC ui/AppController.cpp ui/Finnish.cpp)
target_link_libraries(askompu_desktop_controller PUBLIC askompu_simulator)

option(ASKOMPU_BUILD_GUI "Rakenna graafinen ASkompu-simulaattori" OFF)
if(ASKOMPU_BUILD_GUI)
  include(cmake/DesktopDependencies.cmake)
  add_executable(askompu-simulaattori
    ui/Main.cpp ui/SimulatorView.cpp ui/DeviceDisplay.cpp ui/GuiSmokeCheck.cpp
    ${ASKOMPU_ROOT}/src/ui/DisplayLayout.cpp
    ${ASKOMPU_ROOT}/src/ui/RouteOrderFormatting.cpp)
  target_include_directories(askompu-simulaattori PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
  target_compile_definitions(askompu-simulaattori PRIVATE SDL_MAIN_HANDLED)
  target_link_libraries(askompu-simulaattori PRIVATE askompu_desktop_controller askompu_imgui)
  install(TARGETS askompu-simulaattori RUNTIME DESTINATION bin)
  install(FILES "${imgui_SOURCE_DIR}/LICENSE.txt"
    DESTINATION share/askompu-simulaattori RENAME ImGui-LICENSE.txt)
  install(FILES assets/Roboto-LICENSE.txt THIRD_PARTY.md DESTINATION share/askompu-simulaattori)
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
