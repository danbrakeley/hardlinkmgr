# Writes BuildInfo.h with APP_VERSION and APP_BUILD_DATE. Invoked as a
# build-time (not configure-time) step -- see the "hlm_build_info" custom
# target in the top-level CMakeLists.txt -- so both values reflect the
# actual build, not just the last `cmake --preset` configure: the build date
# would otherwise go stale across incremental builds, and the version
# wouldn't pick up new commits/tags or a newly-dirtied working tree.
#
# Expects on the command line (via -D):
#   DST              - path to the header to write
#   SRC_DIR          - repo root, to run git commands against
#   FALLBACK_VERSION - CMakeLists.txt's project() VERSION, used verbatim
#                       when no "vMAJOR.MINOR.PATCH" tag is reachable from
#                       HEAD (e.g. a shallow clone with no tags fetched)
#
# APP_VERSION is derived from the most recent "vMAJOR.MINOR.PATCH"-style git
# tag reachable from HEAD (README's release process is the source of these
# tags): the "v" is stripped, then "-{short_hash}" is appended if HEAD is
# not exactly that tag, then "-dev" is appended if the working tree has
# uncommitted changes (staged or not).

string(TIMESTAMP HLM_BUILD_DATE "%Y-%m-%d %H:%M UTC" UTC)

set(HLM_VERSION "${FALLBACK_VERSION}")

find_program(HLM_GIT_EXECUTABLE git)
if(HLM_GIT_EXECUTABLE)
  execute_process(
    COMMAND "${HLM_GIT_EXECUTABLE}" describe --tags --match "v[0-9]*.[0-9]*.[0-9]*" --abbrev=0
    WORKING_DIRECTORY "${SRC_DIR}"
    OUTPUT_VARIABLE HLM_LAST_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE HLM_DESCRIBE_RESULT)

  if(HLM_DESCRIBE_RESULT EQUAL 0 AND HLM_LAST_TAG MATCHES "^v(.+)$")
    set(HLM_VERSION "${CMAKE_MATCH_1}")

    execute_process(
      COMMAND "${HLM_GIT_EXECUTABLE}" rev-list "${HLM_LAST_TAG}..HEAD" --count
      WORKING_DIRECTORY "${SRC_DIR}"
      OUTPUT_VARIABLE HLM_COMMITS_SINCE_TAG
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)

    if(NOT HLM_COMMITS_SINCE_TAG STREQUAL "0")
      execute_process(
        COMMAND "${HLM_GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${SRC_DIR}"
        OUTPUT_VARIABLE HLM_SHORT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
      if(HLM_SHORT_HASH)
        set(HLM_VERSION "${HLM_VERSION}-${HLM_SHORT_HASH}")
      endif()
    endif()
  endif()

  execute_process(
    COMMAND "${HLM_GIT_EXECUTABLE}" status --porcelain
    WORKING_DIRECTORY "${SRC_DIR}"
    OUTPUT_VARIABLE HLM_GIT_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(NOT HLM_GIT_STATUS STREQUAL "")
    set(HLM_VERSION "${HLM_VERSION}-dev")
  endif()
endif()

get_filename_component(HLM_BUILD_INFO_DIR "${DST}" DIRECTORY)
file(MAKE_DIRECTORY "${HLM_BUILD_INFO_DIR}")
file(WRITE "${DST}"
  "#pragma once\n"
  "#define APP_VERSION \"${HLM_VERSION}\"\n"
  "#define APP_BUILD_DATE \"${HLM_BUILD_DATE}\"\n")
