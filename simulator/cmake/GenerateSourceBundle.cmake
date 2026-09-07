cmake_minimum_required(VERSION 3.25)
set(stage "${BUNDLE_OUTPUT}/source-bundle-stage")
file(REMOVE_RECURSE "${stage}")
file(MAKE_DIRECTORY "${stage}/simulator" "${BUNDLE_OUTPUT}/tools")
foreach(directory engine ui compat cmake assets docs tools release tests)
  file(COPY "${SIMULATOR_SOURCE}/${directory}" DESTINATION "${stage}/simulator"
    PATTERN "__pycache__" EXCLUDE PATTERN "*.pyc" EXCLUDE)
endforeach()
file(COPY "${SIMULATOR_SOURCE}/CMakeLists.txt" "${SIMULATOR_SOURCE}/CMakePresets.json"
  "${SIMULATOR_SOURCE}/README.md" "${SIMULATOR_SOURCE}/THIRD_PARTY.md" DESTINATION "${stage}/simulator")
file(COPY "${SIMULATOR_SOURCE}/../LICENSE" DESTINATION "${stage}")
file(COPY "${BUNDLE_OUTPUT}/generated/SOURCE-ORIGIN.txt" DESTINATION "${stage}/simulator")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${BUNDLE_OUTPUT}/simulator-source.zip"
  --format=zip -- simulator LICENSE WORKING_DIRECTORY "${stage}" COMMAND_ERROR_IS_FATAL ANY)
configure_file("${SIMULATOR_SOURCE}/tools/core_versions.py" "${BUNDLE_OUTPUT}/tools/core_versions.py" COPYONLY)
