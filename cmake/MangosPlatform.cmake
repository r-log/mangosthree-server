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
# Compiler definitions and options, applied to everything below this directory.
#
# This file used to end with:
#
#     set_property(DIRECTORY
#         PROPERTY COMPILE_DEFINITIONS ${DEFAULT_COMPILE_DEFS}
#         PROPERTY COMPILE_OPTIONS     ${DEFAULT_COMPILE_OPTS})
#
# set_property takes exactly one property. CMake reads the *last* PROPERTY
# keyword as the name and folds everything before it into the value list, so
# COMPILE_DEFINITIONS was never set at all and the definitions were appended to
# COMPILE_OPTIONS instead. They still reached the compiler -- /D and -D happen to
# be valid options too -- which is why nobody noticed, but the definitions
# property was dead and the spelling was locked to one compiler's flag syntax.
#
# add_compile_definitions() / add_compile_options() are the modern spellings and
# cannot be combined by accident: definitions are written without any /D or -D,
# and CMake emits the right prefix per compiler.
# =============================================================================

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PLATFORM 64)
else()
    set(PLATFORM 32)
endif()

if(XCODE)
    if(PLATFORM EQUAL 32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
        set(CMAKE_OSX_ARCHITECTURES ARM32)
    elseif(PLATFORM EQUAL 32)
        set(CMAKE_OSX_ARCHITECTURES i386)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
        set(CMAKE_OSX_ARCHITECTURES ARM64)
    else()
        set(CMAKE_OSX_ARCHITECTURES x86_64)
    endif()
endif()

# -----------------------------------------------------------------------------
# Definitions
# -----------------------------------------------------------------------------
if(WIN32)
    # NOMINMAX is not optional. Without it <windows.h> defines min and max as
    # function-like macros, which then mangle every std::min / std::max in a
    # translation unit that transitively reaches a Windows header -- the failure
    # reads as "C2589: '(': illegal token on right side of '::'" and points at
    # perfectly correct code. The tree used to paper over this with #undef min /
    # #undef max buried at the bottom of Common.h; suppressing the macros at the
    # source is both smaller and reliable.
    add_compile_definitions(
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

if(MSVC)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )
    # Dropped from the historical list:
    #   _WIN64                     - the compiler defines it itself on x64.
    #   _SCL_SECURE_NO_WARNINGS    - the checked-iterator warnings it silenced
    #                                were removed from the toolset years ago.
endif()

if(MINGW)
    add_compile_definitions(
        WINVER=0x0600
        _WIN32_WINNT=0x0600
    )
    if(PLATFORM EQUAL 32)
        add_compile_definitions(HAVE_SSE2 __SSE2__)
    endif()
endif()

# -----------------------------------------------------------------------------
# Options
#
# WHICH WARNINGS ARE RAISED DOES NOT DEPEND ON THE BUILD TYPE. A warning is the
# compiler reporting a defect it can already see; whether the binary is
# optimised has nothing to do with whether the defect is there. Only the flags
# that genuinely differ between a build you debug and a build you ship -- debug
# info, optimisation-specific codegen -- are gated below.
#
# It was not always so, and the cost was visible. -Wall and -Wextra were gated
# on Debug while this tree forces Release unless -DDEBUG=ON, and CI builds
# Release -- so the configuration everyone actually compiled raised only the
# compiler's default set, on two of the three toolchains, while MSVC raised /W4
# in all of them. What that hid, found the day the gate came off: a DEBUG_LOG
# with a %u and no argument, four `if (this)` checks the optimiser is entitled
# to delete, three comparisons between unrelated enums, a dangling reference,
# and a dozen GUIDs and timestamps truncated by %u in log lines.
#
# The suppressions are likewise unconditional. A warning class that is noise is
# noise in every build type; one that was silenced in Release and left to shout
# in Debug is how a developer learns to scroll past the output.
# -----------------------------------------------------------------------------
if(MSVC)
    add_compile_options(
        /MP                                     # parallel compilation
        /W4
        /bigobj                                 # section limit, not a debug aid
        $<$<EQUAL:${PLATFORM},32>:/arch:SSE2>
        $<$<CONFIG:Release>:/Gw>                # whole-program global data opt
        $<$<CONFIG:Release>:/GF>                # string pooling

        # Warning suppressions. Kept as-is rather than trimmed blind: each would
        # need a build to prove it is no longer raised, and a silent regression
        # here is a wall of noise rather than a compile error.
        /wd4018 /wd4100 /wd4101 /wd4127 /wd4131 /wd4189 /wd4244 /wd4245
        /wd4267 /wd4302 /wd4305 /wd4311 /wd4389 /wd4456 /wd4458 /wd4581
        /wd4589 /wd4701 /wd4702 /wd4703 /wd4706 /wd4840 /wd4996
    )
    # /GS- (no buffer security check) is deliberately gone. It traded a
    # documented mitigation against stack-buffer overruns for a few percent, in
    # a server that parses hostile network input for a living.

    # A PDB for the shipped build, for the reason given above the GCC/Clang
    # block. /DEBUG makes the linker emit it; /OPT:REF and /OPT:ICF put back the
    # dead-code and identical-COMDAT folding that /DEBUG otherwise turns off, so
    # this buys symbols without giving up the optimisation.
    add_compile_options($<$<CONFIG:Release>:/Zi>)
    add_link_options($<$<CONFIG:Release>:/DEBUG>
                     $<$<CONFIG:Release>:/OPT:REF>
                     $<$<CONFIG:Release>:/OPT:ICF>)
endif()

# The set both GCC and Clang raise, so the two toolchains cannot disagree about
# what this tree considers a defect.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Winit-self

        # Symbols in the shipped build. A server that crashes at 200 players
        # crashes in Release, and a core dump without symbols says only that it
        # happened. This does not de-optimise anything -- -O stays whatever the
        # build type sets -- it only stops the information being discarded, at
        # the price of a larger binary on disk. Strip at packaging time if that
        # matters; you cannot recover it afterwards.
        $<$<CONFIG:Release>:-g>

        # Frame pointers, for the same reason: without them a backtrace out of
        # an optimised build is guesswork, and a profiler cannot unwind at all.
        $<$<CONFIG:Release>:-fno-omit-frame-pointer>
    )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Winvalid-pch

        # Parameter-passing ABI note on ARM, not actionable and not a defect:
        # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=77728
        -Wno-psabi

        # Macro definitions in the debug info, so a debugger can expand the
        # tree's many object-field macros. Costs only file size.
        $<$<CONFIG:Debug>:-g3>
    )
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "i386")
        add_compile_options(-msse2 -mfpmath=sse)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -Woverloaded-virtual

        # Default-on in Clang, and each is a whole class this tree has not
        # cleared. They are suppressed rather than fixed for now; removing one
        # is a cleanup with a build behind it, not an edit here.
        -Wno-c++11-narrowing
        -Wno-inconsistent-missing-override
        -Wno-switch
    )
    # -Wno-deprecated-register dropped: the `register` keyword was removed in
    # C++17, which this tree now requires, so the warning cannot be raised.
endif()

if(MSVC)
    set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD ON)
endif()

#
# ADD_CXX_PCH(TARGET_NAME PRECOMPILED_HEADER PRECOMPILED_SOURCE)
#
# Sets a precompiled header for a given target
#
# TARGET_NAME - Name of the target. Only valid after add_library or add_executable
# PRECOMPILED_HEADER - Header file to precompile
# PRECOMPILED_SOURCE - MSVC specific source to do the actual precompilation. Ignored on other platforms
#

include(CheckCXXCompilerFlag)

# Clang re-instantiates the PCH's templates in EVERY translation unit that loads it, which for
# a header set this size hands most of the saving straight back. This bakes the instantiations
# into the PCH once instead. GCC and MSVC already do the equivalent. (Clang >= 11.)
check_cxx_compiler_flag("-fpch-instantiate-templates" HAVE_FPCH_INSTANTIATE_TEMPLATES)

# ADD_CXX_PCH(TARGET_NAME PRECOMPILED_HEADER [PRECOMPILED_SOURCE])
#
# PRECOMPILED_SOURCE is accepted for backwards compatibility with the old hand-rolled MSVC
# /Yc scheme and ignored -- target_precompile_headers generates its own translation unit.
function(ADD_CXX_PCH TARGET_NAME PRECOMPILED_HEADER)
	if(NOT TARGET ${TARGET_NAME})
		message(FATAL_ERROR "ADD_CXX_PCH: '${TARGET_NAME}' is not a target.")
	endif()

	# Guard, because the failure this catches is invisible. ADD_CXX_PCH(<target> ${<var>}) with
	# the variable unset collapses to ADD_CXX_PCH(<target>), which used to reach
	# target_precompile_headers with an EMPTY header list -- a silent no-op that left a whole
	# target without the precompiled header its sources textually include anyway. Fail loudly.
	if(NOT PRECOMPILED_HEADER)
		message(FATAL_ERROR "ADD_CXX_PCH(${TARGET_NAME}): no precompiled header given.")
	endif()

	get_filename_component(_pch "${PRECOMPILED_HEADER}" ABSOLUTE
		BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

	if(NOT EXISTS "${_pch}")
		message(FATAL_ERROR "ADD_CXX_PCH(${TARGET_NAME}): header '${_pch}' does not exist.")
	endif()

	target_precompile_headers(${TARGET_NAME} PRIVATE "${_pch}")

	if(HAVE_FPCH_INSTANTIATE_TEMPLATES)
		target_compile_options(${TARGET_NAME} PRIVATE -fpch-instantiate-templates)
	endif()

	# Clang records the mtime of every header in the PCH and refuses the PCH when they differ
	# ("file has been modified since the precompiled header was built"). A compiler cache
	# restores the PCH into a fresh checkout, where every mtime is new, so the PCH is built
	# without timestamps: its validity is then a matter of content, which is also what lets
	# ccache hash it as an input of every unit that uses it. GCC and MSVC have no such check.
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		target_compile_options(${TARGET_NAME} PRIVATE -Xclang -fno-pch-timestamp)
	endif()
endfunction()
