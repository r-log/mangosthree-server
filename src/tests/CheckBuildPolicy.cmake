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

file(READ "${SOURCE_ROOT}/src/CMakeLists.txt" SRC_CMAKE)
file(READ "${SOURCE_ROOT}/.github/workflows/core_linux_build.yml" LINUX_CI)
file(READ "${SOURCE_ROOT}/.github/workflows/core_windows_build.yml" WINDOWS_CI)


string(FIND "${SRC_CMAKE}" "Upstream realmd passes the *file*" POSITION)
if(NOT POSITION EQUAL -1)
  message(FATAL_ERROR "Obsolete external realmd VersionInfo workaround remains")
endif()

foreach(CI_TEXT IN ITEMS "${LINUX_CI}" "${WINDOWS_CI}")
  foreach(REQUIRED_TEXT "-DWITH_TESTS=1" "ctest --test-dir")
    string(FIND "${CI_TEXT}" "${REQUIRED_TEXT}" POSITION)
    if(POSITION EQUAL -1)
      message(FATAL_ERROR "CI does not run tests: ${REQUIRED_TEXT}")
    endif()
  endforeach()
endforeach()

