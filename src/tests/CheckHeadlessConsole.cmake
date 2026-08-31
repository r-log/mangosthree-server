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

# The console may only stop the world on end-of-input when stdin is a terminal.
#
# Reading EOF as "the operator asked to shut down" is right at a terminal and
# wrong everywhere else. A service manager gives the daemon /dev/null, which
# reports EOF on the first read, so an unguarded World::StopNow() there ends the
# world about a second after start-up completes -- on every headless host, which
# is every real deployment.
#
# It is checked here, as source shape, because it cannot be checked by running
# anything. The failure needs a booted world server, and a booted world server
# needs three populated databases and several gigabytes of extracted client data
# that CI does not have. Nor does the failure announce itself once it happens:
# the shutdown is orderly and the process exits 0, so systemd's Restart=on-failure
# stays quiet and the journal holds no error to grep for. What is observable from
# outside is the world port closing, which reads as a firewall fault and sends
# the investigation somewhere else entirely.
#
# Windows never had the bug: Master.cpp declines to start a console in service
# mode. Nothing covered POSIX.

set(CLI_SERVICE "${SOURCE_ROOT}/src/mangosd/CliService.cpp")
if(NOT EXISTS "${CLI_SERVICE}")
    message(FATAL_ERROR "Console reader missing: ${CLI_SERVICE}")
endif()

file(STRINGS "${CLI_SERVICE}" CLI_LINES)

set(SAW_EOF OFF)
set(GUARD_DISTANCE 99)          # lines since the last terminal check
set(STOP_CALLS 0)
set(UNGUARDED "")
set(LINE_NO 0)

foreach(LINE IN LISTS CLI_LINES)
    math(EXPR LINE_NO "${LINE_NO} + 1")

    # Comments describe the rule; they must not be able to satisfy it.
    string(REGEX REPLACE "//.*$" "" CODE "${LINE}")

    if(CODE MATCHES "feof[ \t]*\\([ \t]*stdin[ \t]*\\)")
        set(SAW_EOF ON)
    endif()

    if(CODE MATCHES "StdinIsTerminal[ \t]*\\(")
        set(GUARD_DISTANCE 0)
    else()
        math(EXPR GUARD_DISTANCE "${GUARD_DISTANCE} + 1")
    endif()

    if(CODE MATCHES "World::StopNow[ \t]*\\(")
        math(EXPR STOP_CALLS "${STOP_CALLS} + 1")
        # The guard opens the block the call sits in, so it is a handful of
        # lines above it, never below and never far.
        if(GUARD_DISTANCE GREATER 6)
            list(APPEND UNGUARDED "${LINE_NO}")
        endif()
    endif()
endforeach()

if(NOT SAW_EOF)
    message(FATAL_ERROR
        "CliService.cpp no longer tests feof(stdin).\n"
        "If the console stopped reading stdin this gate is obsolete; if it still\n"
        "reads stdin, the end-of-input case has gone unhandled.")
endif()

if(STOP_CALLS EQUAL 0)
    message(FATAL_ERROR
        "CliService.cpp no longer calls World::StopNow().\n"
        "Ctrl-D at a terminal must still stop the server. Update this gate only\n"
        "together with whatever replaced that call.")
endif()

if(UNGUARDED)
    string(REPLACE ";" ", " PRETTY "${UNGUARDED}")
    message(FATAL_ERROR
        "Unguarded World::StopNow() in CliService.cpp at line(s): ${PRETTY}\n\n"
        "End of input on stdin may only stop the world when stdin is a terminal,\n"
        "so the call has to sit inside an if (StdinIsTerminal()) block. Without\n"
        "it every headless host -- systemd, Docker, any service manager that\n"
        "passes /dev/null -- shuts the world down a second after it starts, and\n"
        "does it cleanly enough that nothing reports an error.")
endif()

message(STATUS "headless console: World::StopNow guarded by a terminal check")
