
if(NOT BUILDDIR)
  # Workaround for funny MSVC behaviour - this segment is only used when using cmake gui
  set(BUILDDIR ${CMAKE_BINARY_DIR})
endif()

if(WITHOUT_GIT)
  set(rev_date                "1970-01-01 00:00:00 +0000" )
  set(rev_hash                "unknown"                   )
  set(rev_branch              "Archived"                  )

  # No valid git commit date, use compiled date
  string(TIMESTAMP rev_date_fallback            "%Y-%m-%d %H:%M:%S" UTC)

else()
  if(GIT_EXECUTABLE)
    # Create a revision-string that we can use
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" describe --long --match init --dirty=+ --abbrev=12 --always
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE rev_info
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    # And grab the commits timestamp
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" show -s --format=%ci
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE rev_date
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    # Also retrieve branch name
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE rev_branch
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

  endif()

  # Last minute check - ensure that we have a proper revision
  # If everything above fails (means the user has erased the git revision control directory or removed the origin/HEAD tag)
  if(NOT rev_info)
    # No valid ways available to find/set the revision/hash, so let's force some defaults
    message(STATUS "
    Could not find a proper repository signature (hash) - you may need to pull tags with git fetch -t
    Continuing anyway - note that the versionstring will be set to \"unknown 1970-01-01 00:00:00 (Archived)\"")
    set(rev_date              "1970-01-01 00:00:00 +0000" )
    set(rev_hash              "unknown"                   )
    set(rev_branch            "Archived"                  )

    # No valid git commit date, use compiled date
    string(TIMESTAMP rev_date_fallback            "%Y-%m-%d %H:%M:%S" UTC)
  else()
    # We have valid date from git commit, use it
    set(rev_date_fallback           ${rev_date}           )

    # Extract information required to build a proper versionstring
    string(REGEX REPLACE init-|[0-9]+-g "" rev_hash           ${rev_info}           )
  endif()
endif()

# For package / copyright information we always need proper date
string(REGEX MATCH "([0-9]+)-([0-9]+)-([0-9]+)" rev_date_fallback_match ${rev_date_fallback})
set(rev_year  ${CMAKE_MATCH_1})
set(rev_month ${CMAKE_MATCH_2})
set(rev_day   ${CMAKE_MATCH_3})

# --- Writing the header ------------------------------------------------------
#
# Two callers, two phases, one template.
#
# At CONFIGURE time this file is include()d for its side effect: the rev_* values
# above, which the build summary prints and which src/shared/CMakeLists.txt
# substitutes into BuildInfo.h along with everything else CMake knows.
#
# At BUILD time the genrev target runs this same file with `cmake -P`, because a
# new commit must change the reported hash without anyone re-running configure.
# Script mode has no cache, so the values it cannot derive arrive as -D; the
# versions it pulls in itself, from the one file that declares them.
if(CMAKE_SCRIPT_MODE_FILE)
    include("${CMAKE_SOURCE_DIR}/cmake/MangosVersions.cmake")

    configure_file(
        "${CMAKE_SOURCE_DIR}/src/shared/BuildInfo.h.in"
        "${BUILDDIR}/src/shared/BuildInfo.h"
        @ONLY
    )
endif()
