# Decoupling D4l: no two include directories of a target offer a header under the same spelling.
# The compiler resolves `#include "X.h"` against a target's include directories in order and
# takes the first hit, so a new header whose name another include directory already has is not
# an error anywhere -- whichever directory comes first wins, silently. src/game lists 28 include
# directories, and since decoupling D4j entities/player/combat and entities/player/spells sit
# beside the src/game/combat and src/game/spells roots: nothing is shadowed today, and this gate
# keeps it that way.
#
# For each target below, the include directories are read from its CMakeLists.txt (every
# target_include_directories() call for it) plus those of the targets it inherits them from
# (game's are PUBLIC). Every header under each directory, recursively, is listed by the path it
# would be included by from that directory: "Unit.h" from src/game/Object, "Object/Unit.h" from
# src/game. A spelling that names two different files is a collision. The basenames are the
# spellings of the files directly in a directory; the deeper spellings catch the D4j shape too
# ("combat/X.h" from both src/game and src/game/entities/player). Spellings are compared in
# lower case, so "Foo.h" and "foo.h" collide: the Windows and macOS file systems ignore case,
# and the gate must fail the same way on the case-sensitive Linux CI runners.
#
# Stated limits: only the targets below are read, and of them only the directories their own
# CMakeLists name -- the directories that come with a linked library (shared, proto, motion) are
# not added; a directory made at configure time (${CMAKE_CURRENT_BINARY_DIR}) has no source to
# list and is skipped by name. A real collision that must exist goes on ALLOWED_COLLISIONS with
# its reason, and an entry that no longer matches a collision fails the gate.
# The parser reads only these CMakeLists and only the literal `target_include_directories(<name>`
# form: a call spelled in another letter case, a target named through a variable, a `#` inside an
# argument, a bracket comment, `include_directories()`, a call in another file and the includer's
# own directory are not seen. None of those occurs today.
# Run standalone (-P), this script sees none of the top-level project's policies. The project
# requires CMake >= 3.18, so that is also the floor here.
cmake_minimum_required(VERSION 3.18)
include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(IncludeCollisions)
# One spelling of the root (a trailing slash, backslashes, another letter case on Windows) so the
# parser self-test compares like with like.
get_filename_component(SOURCE_ROOT "${SOURCE_ROOT}" ABSOLUTE)

# "<target>|<its CMakeLists.txt, relative to SOURCE_ROOT>|<targets it inherits include directories from>"
set(COLLISION_TARGETS
    "game|src/game/CMakeLists.txt|"
    "mangos_tests|src/tests/CMakeLists.txt|game"
    "mangosd|src/mangosd/CMakeLists.txt|game"
    "mangosscript|src/modules/SD3/CMakeLists.txt|game")

set(HEADER_EXTENSIONS h hh hpp hxx inl inc ipp)

# "<spelling, lower case>|<why it must exist>". Empty: there is no collision today.
set(ALLOWED_COLLISIONS "")

# The include directories one target_include_directories() text gives TARGET, as absolute paths.
# CMAKELISTS_DIR is the directory that text lives in (its ${CMAKE_CURRENT_SOURCE_DIR}, and the
# base of a relative entry). Skipped ${CMAKE_CURRENT_BINARY_DIR} entries are counted in
# SKIPPED_VAR; any other variable or generator expression is not understood and fails.
function(parse_include_dirs TEXT TARGET CMAKELISTS_DIR OUT_VAR SKIPPED_VAR)
    string(REGEX REPLACE "#[^\n]*" "" TEXT "${TEXT}")
    string(REGEX MATCHALL "target_include_directories[ \t\r\n]*\\([ \t\r\n]*${TARGET}[ \t\r\n][^)]*\\)"
        BLOCKS "${TEXT}")
    set(DIRS "")
    set(SKIPPED 0)
    foreach(BLOCK IN LISTS BLOCKS)
        string(REGEX REPLACE "^target_include_directories[ \t\r\n]*\\([ \t\r\n]*${TARGET}" "" BODY "${BLOCK}")
        string(REGEX REPLACE "\\)$" "" BODY "${BODY}")
        string(REGEX MATCHALL "[^ \t\r\n]+" TOKENS "${BODY}")
        foreach(TOKEN IN LISTS TOKENS)
            if(TOKEN MATCHES "^(PUBLIC|PRIVATE|INTERFACE|SYSTEM|BEFORE|AFTER)$")
                continue()
            endif()
            string(FIND "${TOKEN}" "\${CMAKE_CURRENT_BINARY_DIR}" AT)
            if(NOT AT EQUAL -1)
                math(EXPR SKIPPED "${SKIPPED} + 1")
                continue()
            endif()
            string(REPLACE "\${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKELISTS_DIR}" DIR "${TOKEN}")
            string(REPLACE "\${CMAKE_SOURCE_DIR}" "${SOURCE_ROOT}" DIR "${DIR}")
            if(DIR MATCHES "\\$[{<]")
                message(FATAL_ERROR "IncludeCollisions: ${TARGET}'s include directory '${TOKEN}' is not understood; "
                    "teach src/tests/CheckIncludeCollisions.cmake to resolve it")
            endif()
            if(NOT IS_ABSOLUTE "${DIR}")
                set(DIR "${CMAKELISTS_DIR}/${DIR}")
            endif()
            get_filename_component(DIR "${DIR}" ABSOLUTE)
            if(NOT IS_DIRECTORY "${DIR}")
                message(FATAL_ERROR
                    "IncludeCollisions: ${TARGET}'s include directory '${TOKEN}' does not exist (${DIR})")
            endif()
            list(APPEND DIRS "${DIR}")
        endforeach()
    endforeach()
    set(${OUT_VAR} "${DIRS}" PARENT_SCOPE)
    set(${SKIPPED_VAR} "${SKIPPED}" PARENT_SCOPE)
endfunction()

# Every header under DIR as "<spelling from DIR>|<path relative to SOURCE_ROOT>".
function(dir_spellings DIR OUT_VAR)
    file(RELATIVE_PATH DIR_REL "${SOURCE_ROOT}" "${DIR}")
    file(GLOB_RECURSE FILES LIST_DIRECTORIES false RELATIVE "${DIR}" "${DIR}/*")
    set(ENTRIES "")
    foreach(REL IN LISTS FILES)
        string(TOLOWER "${REL}" LOWER)
        if(NOT LOWER MATCHES "\\.([^./]+)$")
            continue()
        endif()
        if(NOT CMAKE_MATCH_1 IN_LIST HEADER_EXTENSIONS)
            continue()
        endif()
        list(APPEND ENTRIES "${REL}|${DIR_REL}/${REL}")
    endforeach()
    set(${OUT_VAR} "${ENTRIES}" PARENT_SCOPE)
endfunction()

# The spellings in ENTRIES_VAR ("<spelling>|<file>" entries) that name two or more different files,
# compared in lower case, as "<spelling>: <file>, <file>".
function(find_collisions ENTRIES_VAR OUT_VAR)
    set(KEYS "")
    foreach(ENTRY IN LISTS ${ENTRIES_VAR})
        string(FIND "${ENTRY}" "|" BAR)
        string(SUBSTRING "${ENTRY}" 0 ${BAR} SPELLING)
        math(EXPR BAR "${BAR} + 1")
        string(SUBSTRING "${ENTRY}" ${BAR} -1 FILE_PATH)
        string(TOLOWER "${SPELLING}" KEY)
        string(HEX "${KEY}" ID)
        if(NOT DEFINED SEEN_${ID})
            set(SEEN_${ID} "${FILE_PATH}")
            list(APPEND KEYS "${KEY}")
        elseif(NOT FILE_PATH IN_LIST SEEN_${ID})
            list(APPEND SEEN_${ID} "${FILE_PATH}")
        endif()
    endforeach()
    set(COLLISIONS "")
    foreach(KEY IN LISTS KEYS)
        string(HEX "${KEY}" ID)
        list(LENGTH SEEN_${ID} COUNT)
        if(COUNT GREATER 1)
            string(REPLACE ";" ", " FILES "${SEEN_${ID}}")
            list(APPEND COLLISIONS "${KEY}: ${FILES}")
        endif()
    endforeach()
    set(${OUT_VAR} "${COLLISIONS}" PARENT_SCOPE)
endfunction()

# COLLISIONS_VAR minus the allowed spellings (REMAINING_VAR), and the allowed spellings that
# match no collision (STALE_VAR).
function(apply_allowlist COLLISIONS_VAR ALLOWED_VAR REMAINING_VAR STALE_VAR)
    set(REMAINING "")
    set(USED "")
    foreach(COLLISION IN LISTS ${COLLISIONS_VAR})
        string(FIND "${COLLISION}" ":" COLON)
        string(SUBSTRING "${COLLISION}" 0 ${COLON} KEY)
        set(ALLOWED FALSE)
        foreach(ALLOW IN LISTS ${ALLOWED_VAR})
            string(FIND "${ALLOW}" "|" BAR)
            string(SUBSTRING "${ALLOW}" 0 ${BAR} ALLOW_KEY)
            string(TOLOWER "${ALLOW_KEY}" ALLOW_KEY)
            if(ALLOW_KEY STREQUAL KEY)
                set(ALLOWED TRUE)
                list(APPEND USED "${ALLOW_KEY}")
            endif()
        endforeach()
        if(NOT ALLOWED)
            list(APPEND REMAINING "${COLLISION}")
        endif()
    endforeach()
    set(STALE "")
    foreach(ALLOW IN LISTS ${ALLOWED_VAR})
        string(FIND "${ALLOW}" "|" BAR)
        string(SUBSTRING "${ALLOW}" 0 ${BAR} ALLOW_KEY)
        string(TOLOWER "${ALLOW_KEY}" ALLOW_KEY)
        if(NOT ALLOW_KEY IN_LIST USED)
            list(APPEND STALE "${ALLOW_KEY}")
        endif()
    endforeach()
    set(${REMAINING_VAR} "${REMAINING}" PARENT_SCOPE)
    set(${STALE_VAR} "${STALE}" PARENT_SCOPE)
endfunction()

# Self-test.
function(expect_collisions LABEL ENTRIES EXPECTED)
    set(SELF_TEST_ENTRIES ${ENTRIES})
    find_collisions(SELF_TEST_ENTRIES GOT)
    if(NOT "${GOT}" STREQUAL "${EXPECTED}")
        message(FATAL_ERROR "IncludeCollisions self-test failed (${LABEL}): got '${GOT}', expected '${EXPECTED}'")
    endif()
endfunction()

expect_collisions("one basename in two include directories" "foo.h|a/foo.h;foo.h|b/foo.h" "foo.h: a/foo.h, b/foo.h")
expect_collisions("the same basename in another letter case" "Foo.h|a/Foo.h;foo.h|b/foo.h" "foo.h: a/Foo.h, b/foo.h")
expect_collisions("a subpath under two roots (the D4j shape)"
    "combat/x.h|src/game/combat/x.h;combat/x.h|src/game/entities/player/combat/x.h"
    "combat/x.h: src/game/combat/x.h, src/game/entities/player/combat/x.h")
expect_collisions("three directories" "a.h|x/a.h;a.h|y/a.h;a.h|z/a.h" "a.h: x/a.h, y/a.h, z/a.h")
expect_collisions("the same file twice is not a collision" "foo.h|a/foo.h;foo.h|a/foo.h" "")
expect_collisions("different names" "foo.h|a/foo.h;bar.h|b/bar.h;a/foo.h|c/a/foo.h" "")

set(SELF_TEST_COLLISIONS "foo.h: a/foo.h, b/foo.h" "bar.h: a/bar.h, b/bar.h")
set(SELF_TEST_ALLOWED "Foo.h|a reason" "gone.h|a stale reason")
apply_allowlist(SELF_TEST_COLLISIONS SELF_TEST_ALLOWED SELF_TEST_REMAINING SELF_TEST_STALE)
if(NOT "${SELF_TEST_REMAINING}" STREQUAL "bar.h: a/bar.h, b/bar.h" OR NOT "${SELF_TEST_STALE}" STREQUAL "gone.h")
    message(FATAL_ERROR "IncludeCollisions self-test failed (allowlist): remaining '${SELF_TEST_REMAINING}', "
        "stale '${SELF_TEST_STALE}'")
endif()

set(SELF_TEST_GAME "${SOURCE_ROOT}/src/game")
string(CONCAT SELF_TEST_TEXT "x()\ntarget_include_directories(game\n    PUBLIC # a comment ) with a paren\n"
    "        \${CMAKE_CURRENT_SOURCE_DIR}\n        Object\n        \${CMAKE_CURRENT_SOURCE_DIR}/combat\n"
    "        \${CMAKE_CURRENT_BINARY_DIR}\n)\ntarget_include_directories(gamex PRIVATE Server)\n"
    "target_include_directories(game PRIVATE \${CMAKE_SOURCE_DIR}/src/game/spells)\n")
parse_include_dirs("${SELF_TEST_TEXT}" game "${SELF_TEST_GAME}" SELF_TEST_DIRS SELF_TEST_SKIPPED)
set(SELF_TEST_EXPECTED "${SELF_TEST_GAME};${SELF_TEST_GAME}/Object;${SELF_TEST_GAME}/combat;${SELF_TEST_GAME}/spells")
if(NOT "${SELF_TEST_DIRS}" STREQUAL "${SELF_TEST_EXPECTED}" OR NOT SELF_TEST_SKIPPED EQUAL 1)
    message(FATAL_ERROR "IncludeCollisions self-test failed (parser): got '${SELF_TEST_DIRS}' "
        "(${SELF_TEST_SKIPPED} skipped), expected '${SELF_TEST_EXPECTED}' (1 skipped)")
endif()

# The real check.
set(ALL_DIRS "")
set(ALL_COLLISIONS "")
set(TARGET_REPORT "")
foreach(ROW IN LISTS COLLISION_TARGETS)
    string(REPLACE "|" ";" ROW "${ROW}")
    list(GET ROW 0 TARGET)
    list(GET ROW 1 CMAKELISTS_REL)
    list(GET ROW 2 INHERITS)
    set(CMAKELISTS "${SOURCE_ROOT}/${CMAKELISTS_REL}")
    if(NOT EXISTS "${CMAKELISTS}")
        message(FATAL_ERROR "IncludeCollisions: ${CMAKELISTS_REL} does not exist (moved?)")
    endif()
    file(READ "${CMAKELISTS}" TEXT)
    get_filename_component(CMAKELISTS_DIR "${CMAKELISTS}" DIRECTORY)
    parse_include_dirs("${TEXT}" "${TARGET}" "${CMAKELISTS_DIR}" OWN_DIRS SKIPPED)
    list(LENGTH OWN_DIRS OWN_COUNT)
    if(NOT INHERITS)
        gate_require_scanned(IncludeCollisions "${OWN_COUNT}" "include directories of ${TARGET} in ${CMAKELISTS_REL}")
    endif()
    set(DIRS_${TARGET} ${DIRS_${INHERITS}} ${OWN_DIRS})
    list(REMOVE_DUPLICATES DIRS_${TARGET})
    set(ENTRIES "")
    foreach(DIR IN LISTS DIRS_${TARGET})
        string(HEX "${DIR}" DIR_ID)
        if(NOT DEFINED SPELLINGS_${DIR_ID})
            dir_spellings("${DIR}" SPELLINGS_${DIR_ID})
            list(APPEND ALL_DIRS "${DIR}")
        endif()
        list(APPEND ENTRIES ${SPELLINGS_${DIR_ID}})
    endforeach()
    list(LENGTH DIRS_${TARGET} DIR_COUNT)
    list(LENGTH ENTRIES ENTRY_COUNT)
    gate_require_scanned(IncludeCollisions "${ENTRY_COUNT}" "headers in ${TARGET}'s include directories")
    find_collisions(ENTRIES COLLISIONS)
    foreach(COLLISION IN LISTS COLLISIONS)
        list(APPEND ALL_COLLISIONS "${COLLISION}")
    endforeach()
    list(APPEND TARGET_REPORT "${TARGET} ${DIR_COUNT} dirs (${SKIPPED} generated skipped), ${ENTRY_COUNT} spellings")
endforeach()
list(REMOVE_DUPLICATES ALL_COLLISIONS)

foreach(DIR IN LISTS ALL_DIRS)
    string(HEX "${DIR}" DIR_ID)
    set(DIRECT 0)
    foreach(ENTRY IN LISTS SPELLINGS_${DIR_ID})
        string(FIND "${ENTRY}" "|" BAR)
        string(SUBSTRING "${ENTRY}" 0 ${BAR} SPELLING)
        string(FIND "${SPELLING}" "/" SLASH)
        if(SLASH EQUAL -1)
            math(EXPR DIRECT "${DIRECT} + 1")
        endif()
    endforeach()
    list(LENGTH SPELLINGS_${DIR_ID} SPELLING_COUNT)
    file(RELATIVE_PATH DIR_REL "${SOURCE_ROOT}" "${DIR}")
    message(STATUS "include collisions: ${DIR_REL}/ -> ${DIRECT} header basenames, ${SPELLING_COUNT} spellings")
endforeach()

apply_allowlist(ALL_COLLISIONS ALLOWED_COLLISIONS REMAINING STALE)
if(REMAINING OR STALE)
    set(REMAINING_REPORT "(none)")
    set(STALE_REPORT "(none)")
    if(REMAINING)
        string(REPLACE ";" "\n  " REMAINING_REPORT "${REMAINING}")
    endif()
    if(STALE)
        string(REPLACE ";" "\n  " STALE_REPORT "${STALE}")
    endif()
    message(FATAL_ERROR
        "Two include directories of one target offer a header under the same spelling (decoupling D4l); "
        "the compiler takes whichever directory comes first, silently:\n  ${REMAINING_REPORT}\n"
        "ALLOWED_COLLISIONS entries that match no collision any more (remove them):\n  ${STALE_REPORT}\n"
        "Rename the new header, or include it by a path no other include directory offers.")
endif()

list(LENGTH ALL_DIRS ALL_DIR_COUNT)
list(LENGTH ALLOWED_COLLISIONS ALLOWED_COUNT)
string(REPLACE ";" "; " TARGET_REPORT "${TARGET_REPORT}")
message(STATUS "include collisions: none but the ${ALLOWED_COUNT} allowed over ${ALL_DIR_COUNT} include directories "
    "(${TARGET_REPORT}), self-test OK")
