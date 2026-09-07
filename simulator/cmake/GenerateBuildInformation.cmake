cmake_minimum_required(VERSION 3.25)
file(REAL_PATH "${ASKOMPU_SOURCE_ROOT}" source_root)
set(ASKOMPU_ROOT "${source_root}")
if(NOT ASKOMPU_CORE_ROOT)
  set(ASKOMPU_CORE_ROOT "${source_root}")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/ValidateCore.cmake")
askompu_validate_core()
set(ASKOMPU_REVISION "tuntematon")
set(ASKOMPU_UPSTREAM_BASE "tuntematon")
set(ASKOMPU_WORKTREE "ei Git-tietoja")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --show-toplevel
    WORKING_DIRECTORY "${source_root}" OUTPUT_VARIABLE git_root
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  # Arkisto voi sijaita jonkin muun Git-repon sisällä: älä lainaa sen tunnistetta.
  if(git_root)
    file(REAL_PATH "${git_root}" git_root)
  endif()
  if(git_root STREQUAL source_root)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
      WORKING_DIRECTORY "${source_root}" OUTPUT_VARIABLE revision
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE result)
    if(result EQUAL 0)
      set(ASKOMPU_REVISION "${revision}")
      execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=normal
        WORKING_DIRECTORY "${source_root}" OUTPUT_VARIABLE changes ERROR_QUIET)
      if(changes)
        set(ASKOMPU_WORKTREE "sisältää paikallisia muutoksia")
      else()
        set(ASKOMPU_WORKTREE "puhdas")
      endif()
      execute_process(COMMAND "${GIT_EXECUTABLE}" merge-base HEAD refs/remotes/upstream/main
        WORKING_DIRECTORY "${source_root}" OUTPUT_VARIABLE base
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE result)
      if(result EQUAL 0)
        set(ASKOMPU_UPSTREAM_BASE "${base}")
      endif()
    endif()
  endif()
endif()

# Lähdepaketti säilyttää simulaattorin tunnisteen ilman alkuperäistä Git-työpuuta.
if(ASKOMPU_REVISION STREQUAL "tuntematon" AND EXISTS "${source_root}/simulator/SOURCE-ORIGIN.txt")
  file(STRINGS "${source_root}/simulator/SOURCE-ORIGIN.txt" origin LIMIT_COUNT 2)
  list(GET origin 0 ASKOMPU_REVISION)
  list(GET origin 1 ASKOMPU_WORKTREE)
endif()
set(ASKOMPU_ACTUAL_CORE_REVISION "tuntematon")
set(ASKOMPU_CORE_DATE "tuntematon")
set(ASKOMPU_CORE_TAG "")
set(ASKOMPU_CORE_WORKTREE "ei Git-tietoja")
if(GIT_FOUND)
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --show-toplevel
    WORKING_DIRECTORY "${ASKOMPU_CORE_ROOT}" OUTPUT_VARIABLE core_git_root
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(core_git_root)
    file(REAL_PATH "${core_git_root}" core_git_root)
  endif()
  if(core_git_root STREQUAL ASKOMPU_CORE_ROOT)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD WORKING_DIRECTORY "${ASKOMPU_CORE_ROOT}"
      OUTPUT_VARIABLE ASKOMPU_ACTUAL_CORE_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" show -s --format=%cI HEAD WORKING_DIRECTORY "${ASKOMPU_CORE_ROOT}"
      OUTPUT_VARIABLE ASKOMPU_CORE_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags --exact-match HEAD WORKING_DIRECTORY "${ASKOMPU_CORE_ROOT}"
      OUTPUT_VARIABLE ASKOMPU_CORE_TAG OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain WORKING_DIRECTORY "${ASKOMPU_CORE_ROOT}"
      OUTPUT_VARIABLE core_dirty)
    if(core_dirty)
      set(ASKOMPU_CORE_WORKTREE "paikallisia muutoksia")
    else()
      set(ASKOMPU_CORE_WORKTREE "puhdas")
    endif()
  endif()
endif()
# Git-tagissa sallitaan myös C++-merkkijonon erikoismerkkejä.
string(REPLACE "\\" "\\\\" ASKOMPU_CORE_TAG "${ASKOMPU_CORE_TAG}")
string(REPLACE "\"" "\\\"" ASKOMPU_CORE_TAG "${ASKOMPU_CORE_TAG}")

# Tunnistaa myös paikalliset muutokset ja .git-metatiedottoman lähdearkiston.
function(source_fingerprint output root)
  file(GLOB_RECURSE paths LIST_DIRECTORIES FALSE RELATIVE "${root}" ${ARGN})
  list(SORT paths)
  set(manifest "")
  foreach(path IN LISTS paths)
    file(SHA256 "${root}/${path}" digest)
    string(APPEND manifest "${path}:${digest}\n")
  endforeach()
  string(SHA256 fingerprint "${manifest}")
  set(${output} "${fingerprint}" PARENT_SCOPE)
endfunction()
source_fingerprint(ASKOMPU_CORE_FINGERPRINT "${ASKOMPU_CORE_ROOT}" "${ASKOMPU_CORE_ROOT}/src/*" "${ASKOMPU_CORE_ROOT}/include/*")
source_fingerprint(ASKOMPU_SIMULATOR_FINGERPRINT "${source_root}"
  "${source_root}/simulator/engine/*" "${source_root}/simulator/ui/*"
  "${source_root}/simulator/cmake/*" "${source_root}/simulator/assets/*"
  "${source_root}/simulator/compat/*" "${source_root}/simulator/tools/*"
  "${source_root}/simulator/CMakeLists.txt" "${source_root}/simulator/CMakePresets.json")
file(MAKE_DIRECTORY "${ASKOMPU_OUTPUT_DIR}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/BuildInformation.h.in"
  "${ASKOMPU_OUTPUT_DIR}/BuildInformation.h" @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/BUILDINFO.txt.in"
  "${ASKOMPU_OUTPUT_DIR}/BUILDINFO-${ASKOMPU_CONFIGURATION}.txt" @ONLY)
file(WRITE "${ASKOMPU_OUTPUT_DIR}/SOURCE-ORIGIN.txt" "${ASKOMPU_REVISION}\n${ASKOMPU_WORKTREE}\n")
