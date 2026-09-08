function(askompu_validate_core)
  # Script-mode callers can supply native Windows separators or path aliases.
  file(REAL_PATH "${ASKOMPU_ROOT}" ASKOMPU_ROOT)
  file(REAL_PATH "${ASKOMPU_CORE_ROOT}" ASKOMPU_CORE_ROOT)
  if(NOT EXISTS "${ASKOMPU_CORE_ROOT}/src/core/ApplicationCore.cpp")
    message(FATAL_ERROR "Valitusta ASkompu-lähdehakemistosta puuttuu tuotantoydin")
  endif()
  if(NOT ASKOMPU_CORE_ROOT STREQUAL ASKOMPU_ROOT OR ASKOMPU_CORE_REVISION)
    string(LENGTH "${ASKOMPU_CORE_REVISION}" sha_length)
    if(NOT sha_length EQUAL 40 OR NOT ASKOMPU_CORE_REVISION MATCHES "^[0-9a-f]+$")
      message(FATAL_ERROR "Erillinen ASkompu-ydin tarvitsee täyden lukitun commit-SHA:n")
    endif()
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --show-toplevel
      WORKING_DIRECTORY "${ASKOMPU_CORE_ROOT}" OUTPUT_VARIABLE git_root
      OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    file(REAL_PATH "${git_root}" git_root)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
      WORKING_DIRECTORY "${ASKOMPU_CORE_ROOT}" OUTPUT_VARIABLE actual
      OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=normal
      WORKING_DIRECTORY "${ASKOMPU_CORE_ROOT}" OUTPUT_VARIABLE dirty COMMAND_ERROR_IS_FATAL ANY)
    if(NOT git_root STREQUAL ASKOMPU_CORE_ROOT OR NOT actual STREQUAL ASKOMPU_CORE_REVISION OR dirty)
      message(FATAL_ERROR "Valittu ASkompu-ydin ei vastaa lukittua puhdasta committia; rakennus estetty")
    endif()
  endif()
endfunction()
