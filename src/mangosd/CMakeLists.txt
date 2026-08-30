# SPDX-License-Identifier: GPL-3.0-or-later
#
#/**
# * MaNGOS is a full featured server for World of Warcraft, supporting
# * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
# *
# * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
# *
# * This program is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program; if not, write to the Free Software
# * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# *
# * World of Warcraft, and all World of Warcraft or Warcraft art, images,
# * and lore are copyrighted by Blizzard Entertainment, Inc.
# */

#Main Files
set(SRC_GRP_MAIN
  AntiFreezeService.cpp
  AntiFreezeService.h
  CliService.cpp
  CliService.h
  Master.cpp
  Master.h
  Service.h
  mangosd.cpp
)
source_group("Main" FILES ${SRC_GRP_MAIN})

add_executable(mangosd
    ${SRC_GRP_MAIN}
    $<$<BOOL:${WIN32}>:${CMAKE_SOURCE_DIR}/cmake/win/VersionInfo.rc>
)

# The three strings that name THIS executable. Everything else the resource
# needs comes from the generated BuildInfo.h, which is shared; these are not,
# so they are handed to the resource compiler per target instead of being
# baked into a per-target copy of a generated header.
if(WIN32)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/cmake/win/VersionInfo.rc
        TARGET_DIRECTORY mangosd
        PROPERTIES COMPILE_DEFINITIONS
            "PRODUCT_ICON=\"${CMAKE_CURRENT_SOURCE_DIR}/mangosd.ico\";PRODUCT_BUNDLE=\"mangosd.exe\\0\";PRODUCT_FILE_DESCRIPTION=\"${MANGOS_PKG} World Server\\0\"")
endif()

target_include_directories(mangosd
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(mangosd
    PRIVATE
        game
        proto
        Threads::Threads
)

install(
    TARGETS mangosd
    DESTINATION ${BIN_DIR}
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mangosd.conf.dist.in ${CMAKE_CURRENT_BINARY_DIR}/mangosd.conf.dist)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/mangosd.conf.dist
    DESTINATION ${CONF_INSTALL_DIR}
)

if(WIN32 AND MSVC)
    install(
        FILES $<TARGET_PDB_FILE:mangosd>
        DESTINATION ${BIN_DIR}
        OPTIONAL
        )
endif()
