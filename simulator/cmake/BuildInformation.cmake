# Päivitetään joka rakennuksessa, myös ilman uudelleenkonfigurointia.
add_custom_target(askompu_build_information
  COMMAND "${CMAKE_COMMAND}"
    "-DASKOMPU_SOURCE_ROOT=${ASKOMPU_ROOT}"
    "-DASKOMPU_CORE_ROOT=${ASKOMPU_CORE_ROOT}"
    "-DASKOMPU_CORE_REVISION=${ASKOMPU_CORE_REVISION}"
    "-DASKOMPU_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/generated"
    "-DASKOMPU_VERSION=${PROJECT_VERSION}"
    "-DASKOMPU_CONFIGURATION=$<CONFIG>"
    -P "${CMAKE_CURRENT_LIST_DIR}/GenerateBuildInformation.cmake"
  BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/generated/BuildInformation.h"
  VERBATIM)
add_dependencies(askompu-simulaattori askompu_build_information)
include("${CMAKE_CURRENT_LIST_DIR}/SourceBundle.cmake")
target_compile_definitions(askompu-simulaattori PRIVATE
  "ASKOMPU_BUILD_CONFIGURATION=\"$<CONFIG>\"")
