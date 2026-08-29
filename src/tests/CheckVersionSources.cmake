# SPDX-License-Identifier: GPL-3.0-or-later
#
# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

# cmake/MangosVersions.cmake is the single source of every declared version.
# This is what keeps it single.
#
# The failure it exists to prevent is not a build error -- it is a version that
# has been copied, and where bumping the copy in the obvious place changes
# nothing observable. That is how the auction-house bot ended up with three
# config versions, two of them agreeing with each other and neither of them the
# one this tree declared.

# file(RELATIVE_PATH) below refuses a relative base, and it is only reached once
# a violation has been found -- so a relative -DSOURCE_ROOT would turn every
# report into a crash and every clean run into a pass.
get_filename_component(SOURCE_ROOT "${SOURCE_ROOT}" ABSOLUTE)

set(VERSIONS_FILE "${SOURCE_ROOT}/cmake/MangosVersions.cmake")

if(NOT EXISTS "${VERSIONS_FILE}")
    message(FATAL_ERROR "Version source missing: ${VERSIONS_FILE}")
endif()

set(VIOLATIONS "")

# --- Every config template takes its version by substitution -----------------
#
# A literal here is a number that cannot be bumped from the versions file, and
# whose disagreement with the C++ shows up only as a warning in an operator's
# log.

file(GLOB_RECURSE CONF_TEMPLATES "${SOURCE_ROOT}/src/*.conf.dist.in")

foreach(TEMPLATE IN LISTS CONF_TEMPLATES)
    file(STRINGS "${TEMPLATE}" CONF_LINES REGEX "^[ \t]*ConfVersion[ \t]*=")

    if(NOT CONF_LINES)
        file(RELATIVE_PATH SHORT "${SOURCE_ROOT}" "${TEMPLATE}")
        list(APPEND VIOLATIONS "${SHORT}: no ConfVersion line")
        continue()
    endif()

    foreach(LINE IN LISTS CONF_LINES)
        if(NOT LINE MATCHES "@[A-Za-z0-9_]+@")
            file(RELATIVE_PATH SHORT "${SOURCE_ROOT}" "${TEMPLATE}")
            list(APPEND VIOLATIONS
                 "${SHORT}: ConfVersion is a literal (${LINE}); use the @MANGOS_..._VER@ that cmake/MangosVersions.cmake declares")
        endif()
    endforeach()
endforeach()

# --- Nothing re-declares what BuildInfo.h generates ---------------------------

set(GENERATED_MACROS
    MANGOSD_CONFIG_VERSION
    REALMD_CONFIG_VERSION
    AHBOT_CONFIG_VERSION
    AUCTIONHOUSEBOT_CONF_VERSION
    EXPECTED_MANGOSD_CLIENT_BUILD
    EXPECTED_MANGOSD_CLIENT_VERSION
    PROJECT_REVISION_NR
)

file(GLOB_RECURSE SOURCES "${SOURCE_ROOT}/src/*.h" "${SOURCE_ROOT}/src/*.cpp")

foreach(FILE_PATH IN LISTS SOURCES)
    # BuildInfo.h.in is where these are supposed to be defined.
    if(FILE_PATH MATCHES "BuildInfo\\.h")
        continue()
    endif()

    foreach(MACRO_NAME IN LISTS GENERATED_MACROS)
        file(STRINGS "${FILE_PATH}" HITS REGEX "^[ \t]*#[ \t]*define[ \t]+${MACRO_NAME}[ \t]")
        if(HITS)
            file(RELATIVE_PATH SHORT "${SOURCE_ROOT}" "${FILE_PATH}")
            list(APPEND VIOLATIONS
                 "${SHORT}: re-defines ${MACRO_NAME}; it is generated from cmake/MangosVersions.cmake into BuildInfo.h")
        endif()
    endforeach()
endforeach()

# --- The template carries no literals of its own ------------------------------
#
# It used to hold the database versions as plain strings, so a schema bump meant
# editing a file whose other values are all substitutions -- and which reads as
# generated, so nobody looked.

file(STRINGS "${SOURCE_ROOT}/src/shared/BuildInfo.h.in" REVISION_LINES
     REGEX "^[ \t]*#[ \t]*define[ \t]+(PROJECT_REVISION_NR|(REALMD|CHAR|WORLD)_DB_)")

foreach(LINE IN LISTS REVISION_LINES)
    if(NOT LINE MATCHES "@[A-Za-z0-9_]+@")
        list(APPEND VIOLATIONS
             "src/shared/BuildInfo.h.in: literal version (${LINE}); declare it in cmake/MangosVersions.cmake")
    endif()
endforeach()

# --- No header includes the generated one ------------------------------------
#
# BuildInfo.h is rewritten whenever any declared value changes. Anything that
# includes it is rebuilt with it, and anything that includes THAT is rebuilt
# too -- so it reaching a widely included header turns a config-version bump
# into a full rebuild of the server. It has happened: it once reached
# SharedDefines.h, which 81 files include directly and most of the game reaches
# transitively.
#
# A .cpp may include it: those are leaves, and it carries install paths, config
# file names and default ports that a .cpp legitimately wants. A header may not.
# Versions go through GitRevision either way.

file(GLOB_RECURSE ALL_HEADERS "${SOURCE_ROOT}/src/*.h")

foreach(FILE_PATH IN LISTS ALL_HEADERS)
    if(FILE_PATH MATCHES "BuildInfo\\.h")
        continue()
    endif()

    file(STRINGS "${FILE_PATH}" HITS
         REGEX "^[ \t]*#[ \t]*include[ \t]*[\"<][^\">]*BuildInfo\\.h[\">]")

    if(HITS)
        file(RELATIVE_PATH SHORT "${SOURCE_ROOT}" "${FILE_PATH}")
        list(APPEND VIOLATIONS
             "${SHORT}: a HEADER includes BuildInfo.h, so everything that includes it rebuilds on a version bump")
    endif()
endforeach()

if(VIOLATIONS)
    list(REMOVE_DUPLICATES VIOLATIONS)
    string(REPLACE ";" "\n  " PRETTY "${VIOLATIONS}")
    message(FATAL_ERROR
        "Version declarations have drifted from cmake/MangosVersions.cmake:\n  ${PRETTY}\n")
endif()

message(STATUS "Version sources: single-sourced from cmake/MangosVersions.cmake")
