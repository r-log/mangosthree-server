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

# The crypto is the tree's own (src/shared/Crypto). Two exact greps keep it that way:
#
#   1. Nothing in the repository names the library the tree used to sit on -- not a
#      package list, a workflow, a comment, a link target -- except one sentence in
#      README.md about the MySQL client shipping its own copy for itself. The pattern is
#      deliberately unanchored so a package name, a target name or a DLL name all match.
#      dep/ (vendored, its own business; only its CMake lists are read) and the
#      historical ChangeLog are excluded.
#
#   2. No source under src/ uses that library's symbol families -- the prefixes its
#      API used, and its big-number type name. A wrapper that quietly reached back for
#      one would be caught at its first identifier. src/modules (a submodule) is
#      excluded.
#
# Both greps must come back empty; this script prints every offender it finds.

set(WORD_PATTERN "openssl|libssl|libcrypto|legacy[.]dll")
set(README_SENTENCE "the openssl libraries it brings for itself")
set(SYMBOL_PATTERN "(^|[^A-Za-z0-9_])((OSSL|EVP|BN|OPENSSL|SHA1|MD5|HMAC|RAND)_[A-Za-z]|BIGNUM([^A-Za-z0-9_]|$))")

file(GLOB_RECURSE ALL_FILES LIST_DIRECTORIES false RELATIVE "${SOURCE_ROOT}" "${SOURCE_ROOT}/*")

set(WORD_OFFENDERS)
set(SYMBOL_OFFENDERS)
set(SCANNED 0)

foreach(REL IN LISTS ALL_FILES)
  # Excluded: version control, any build tree inside the checkout, the compiler cache,
  # the historical change log, this script (it spells the patterns), and vendored code
  # -- except the CMake lists under dep/, which are where a vendored crypto target would
  # have to be declared to reach a binary of ours.
  if(REL MATCHES "^[.]git/" OR REL MATCHES "^[.]ccache/" OR REL MATCHES "^_?build[^/]*/"
     OR REL STREQUAL "extra/doc/ChangeLog.md"
     OR REL STREQUAL "src/tests/CheckCryptoIsOurs.cmake")
    continue()
  endif()
  if(REL MATCHES "^dep/" AND NOT REL MATCHES "^dep/([^/]+/)?CMakeLists[.]txt$")
    continue()
  endif()
  # Only text is worth reading; binaries (images, archives) cannot name a library.
  if(REL MATCHES "[.](png|jpg|jpeg|gif|ico|bmp|zip|gz|7z|mpq|dbc|wdt|adt|m2|wmo|db2|pdf|bin)$")
    continue()
  endif()

  file(READ "${SOURCE_ROOT}/${REL}" CONTENT)
  math(EXPR SCANNED "${SCANNED} + 1")
  string(TOLOWER "${CONTENT}" LOWER)

  if(LOWER MATCHES "${WORD_PATTERN}")
    if(REL STREQUAL "README.md")
      # Every matching line of README.md must be the one permitted sentence.
      string(REGEX MATCHALL "[^\n]*(${WORD_PATTERN})[^\n]*" LINES "${LOWER}")
      foreach(LINE IN LISTS LINES)
        if(NOT LINE MATCHES "${README_SENTENCE}")
          list(APPEND WORD_OFFENDERS "${REL}: ${LINE}")
        endif()
      endforeach()
    else()
      list(APPEND WORD_OFFENDERS "${REL}")
    endif()
  endif()

  if(REL MATCHES "^src/" AND NOT REL MATCHES "^src/modules/")
    if(CONTENT MATCHES "${SYMBOL_PATTERN}")
      string(REGEX MATCH "${SYMBOL_PATTERN}[A-Za-z0-9_]*" HIT "${CONTENT}")
      list(APPEND SYMBOL_OFFENDERS "${REL}: ${HIT}")
    endif()
  endif()
endforeach()

if(SCANNED LESS 100)
  message(FATAL_ERROR "crypto_is_ours: scanned only ${SCANNED} files under ${SOURCE_ROOT}; the walk is broken")
endif()

set(FAILED FALSE)
if(WORD_OFFENDERS)
  set(FAILED TRUE)
  message(STATUS "Files naming the removed crypto library (allowed: one sentence in README.md):")
  foreach(OFFENDER IN LISTS WORD_OFFENDERS)
    message(STATUS "  ${OFFENDER}")
  endforeach()
endif()
if(SYMBOL_OFFENDERS)
  set(FAILED TRUE)
  message(STATUS "Sources under src/ using the removed library's symbol families:")
  foreach(OFFENDER IN LISTS SYMBOL_OFFENDERS)
    message(STATUS "  ${OFFENDER}")
  endforeach()
endif()
if(FAILED)
  message(FATAL_ERROR "crypto_is_ours: the tree still refers to the removed crypto library (see above)")
endif()
message(STATUS "crypto_is_ours: ${SCANNED} files scanned, nothing of the removed library remains")
