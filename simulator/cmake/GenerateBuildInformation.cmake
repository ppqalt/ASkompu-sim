cmake_minimum_required(VERSION 3.25)
file(REAL_PATH "${ASKOMPU_SOURCE_ROOT}" source_root)
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

# Tunnistaa myös paikalliset muutokset ja .git-metatiedottoman lähdearkiston.
function(source_fingerprint output)
  file(GLOB_RECURSE paths LIST_DIRECTORIES FALSE RELATIVE "${source_root}" ${ARGN})
  list(SORT paths)
  set(manifest "")
  foreach(path IN LISTS paths)
    file(SHA256 "${source_root}/${path}" digest)
    string(APPEND manifest "${path}:${digest}\n")
  endforeach()
  string(SHA256 fingerprint "${manifest}")
  set(${output} "${fingerprint}" PARENT_SCOPE)
endfunction()
source_fingerprint(ASKOMPU_CORE_FINGERPRINT "${source_root}/src/*" "${source_root}/include/*")
source_fingerprint(ASKOMPU_SIMULATOR_FINGERPRINT
  "${source_root}/simulator/engine/*" "${source_root}/simulator/ui/*"
  "${source_root}/simulator/cmake/*" "${source_root}/simulator/assets/*"
  "${source_root}/simulator/CMakeLists.txt" "${source_root}/simulator/CMakePresets.json")
file(MAKE_DIRECTORY "${ASKOMPU_OUTPUT_DIR}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/BuildInformation.h.in"
  "${ASKOMPU_OUTPUT_DIR}/BuildInformation.h" @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/BUILDINFO.txt.in"
  "${ASKOMPU_OUTPUT_DIR}/BUILDINFO-${ASKOMPU_CONFIGURATION}.txt" @ONLY)
