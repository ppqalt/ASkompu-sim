# Toimitetaan versionvalitsimen tarvitsema simulaattorilähde; tuotantoydin haetaan
# erikseen valittuun SHA:han. Paketti ei riipu julkaisemattoman forkin fetchistä.
add_custom_target(askompu_source_bundle
  COMMAND "${CMAKE_COMMAND}"
    "-DSIMULATOR_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DBUNDLE_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_LIST_DIR}/GenerateSourceBundle.cmake"
  VERBATIM)
add_dependencies(askompu_source_bundle askompu_build_information)
add_dependencies(askompu-simulaattori askompu_source_bundle)
