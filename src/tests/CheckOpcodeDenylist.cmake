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

# 47 opcode names whose value disagrees with every independent 4.3.4 source and
# which nothing currently builds or dispatches. They are inert TODAY. Adopting
# one without correcting its value ships a silent no-op -- an SMSG the client
# discards, or a CMSG handler that never runs.
#
# Both directions are checked. Checking construction alone is the exact mistake
# that hid the calendar's request-side breakage: the server never builds a CMSG,
# so a wrong CMSG value is invisible to a construction-only test.

set(DENYLIST_FILE "${CMAKE_CURRENT_LIST_DIR}/opcode_denylist.txt")
if(NOT EXISTS "${DENYLIST_FILE}")
    message(FATAL_ERROR "Opcode denylist missing: ${DENYLIST_FILE}")
endif()

file(STRINGS "${DENYLIST_FILE}" DENY_LINES REGEX "^(SMSG|CMSG|MSG)_[A-Z0-9_]+")
set(DENY_NAMES "")
foreach(LINE IN LISTS DENY_LINES)
    if(LINE MATCHES "^((SMSG|CMSG|MSG)_[A-Z0-9_]+)")
        list(APPEND DENY_NAMES "${CMAKE_MATCH_1}")
    endif()
endforeach()

list(LENGTH DENY_NAMES DENY_COUNT)
if(DENY_COUNT EQUAL 0)
    message(FATAL_ERROR "Opcode denylist parsed to zero names -- gate is inert")
endif()

file(GLOB_RECURSE GAME_SOURCES "${SOURCE_ROOT}/src/game/*.cpp")
set(VIOLATIONS "")

foreach(FILE_PATH IN LISTS GAME_SOURCES)
    file(STRINGS "${FILE_PATH}" RAW_LINES)
    set(LINE_NO 0)
    foreach(LINE IN LISTS RAW_LINES)
        math(EXPR LINE_NO "${LINE_NO} + 1")
        string(STRIP "${LINE}" TRIMMED)
        if(TRIMMED MATCHES "^(//|\\*|/\\*)")
            continue()
        endif()
        foreach(NAME IN LISTS DENY_NAMES)
            # Construction, either direction: building a packet with a wrong
            # value sends something the client discards.
            if(TRIMMED MATCHES "(WorldPacket[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\\(|Initialize[ \t]*\\(|SetOpcode[ \t]*\\()[ \t]*${NAME}[ \t]*[,)]")
                list(APPEND VIOLATIONS "${FILE_PATH}:${LINE_NO}: ${NAME} (constructed)")
            endif()
            # Dispatch, CMSG/MSG only. An SMSG OPCODE(...) entry is
            # STATUS_NEVER + Handle_ServerSide -- a table-completeness
            # placeholder, never a dispatch target, because the server never
            # receives an SMSG. Eleven denylisted SMSG names already have such
            # entries; flagging those would fail the gate on an unmodified tree.
            # Only a CMSG/MSG registration at a wrong value produces the
            # dead-handler failure this gate exists to prevent.
            if(NAME MATCHES "^(CMSG|MSG)_"
               AND TRIMMED MATCHES "^OPCODE[ \t]*\\([ \t]*${NAME}[ \t]*,")
                list(APPEND VIOLATIONS "${FILE_PATH}:${LINE_NO}: ${NAME} (dispatched)")
            endif()
        endforeach()
    endforeach()
endforeach()

if(VIOLATIONS)
    string(REPLACE ";" "\n  " PRETTY "${VIOLATIONS}")
    message(FATAL_ERROR
        "Known-wrong opcode value adopted:\n  ${PRETTY}\n\n"
        "These names carry values the 4.3.4 client does not agree with. Using one\n"
        "ships a silent no-op. Correct the value in src/proto/Opcodes.h from the\n"
        "'correct' column of src/tests/opcode_denylist.txt, then remove the name\n"
        "from that file in the same commit.\n"
        "See OPCODE_CORRECTION_DESIGN_2026-08-31.md.")
endif()

message(STATUS "opcode denylist: ${DENY_COUNT} names, no adoptions")
