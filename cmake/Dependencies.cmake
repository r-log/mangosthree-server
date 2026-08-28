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

# =============================================================================
# Which vendored dependencies this tree builds.
#
# dep/ holds plain copies of the third-party libraries the core uses (see
# dep/README.md for versions and origins); it has no CMakeLists of its own.
# This file is the single place that lists what is built and when, so a
# library that is not named here is simply never configured.
#
# StormLib is built for the extractor tools whenever BUILD_TOOLS is on: the
# extractor is what produces the tiles the server reads, not an optional extra.
# =============================================================================

set(MANGOS_DEP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dep")

if(NOT EXISTS "${MANGOS_DEP_DIR}")
    message(FATAL_ERROR "dep/ is missing: the checkout is incomplete.")
endif()

if(NOT TARGET "ZLIB::ZLIB")
    add_subdirectory(${MANGOS_DEP_DIR}/zlib dep/zlib)
endif()

if(NOT TARGET "BZip2::BZip2")
    add_subdirectory(${MANGOS_DEP_DIR}/bzip2 dep/bzip2)
endif()

add_subdirectory(${MANGOS_DEP_DIR}/utf8cpp dep/utf8cpp)

# Recast and Detour are needed by the server's pathfinder AND by the baker's navmesh
# stage, and the baker always builds, so this is not gated either.
add_subdirectory(${MANGOS_DEP_DIR}/recastnavigation dep/recastnavigation)

# The MPQ reader is not gated: mangos-extractor is what produces the tiles the server
# reads, so it is always built rather than being an optional extra that a fresh clone
# can leave out and then have nothing to run on.
add_subdirectory(${MANGOS_DEP_DIR}/StormLib dep/StormLib)
