# CheckCaseLabels: the switch-aware spell-ID case-label ratchet of decoupling D11
# (design/2026-09-28-unit-reopening.md, section 3(c)'s proof table, row (b)). The per-spell-ID
# switches inside the spell handlers move, one family per PR, into the spell handler registry
# (src/game/spells/handlers/). Every `case` label whose innermost enclosing switch is keyed on a
# spell id is counted per file under src/game, outside the registry's directory, and the counts
# are listed in src/tests/case_labels.txt: a file whose count grows, or that the list does not
# hold, fails (a new label), and so does a file whose count fell below its line (a stale baseline:
# the PR that moves a family lowers its line in the same PR). A scan that reads nothing fails too.
#
# The work is src/tests/tools/case_labels.py --check (the lexer and its definition are in its
# docstring); its fixtures run as the separate ctest case_labels_selftest. This script fails
# without a Python 3 interpreter: a gate that cannot run must not pass.
# Run standalone (-P), this script sees none of the top-level project's policies. The project
# requires CMake >= 3.18, so that is also the floor here.
cmake_minimum_required(VERSION 3.18)
include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(CaseLabels)
get_filename_component(SOURCE_ROOT "${SOURCE_ROOT}" ABSOLUTE)

set(CASE_LABELS_TOOL "${SOURCE_ROOT}/src/tests/tools/case_labels.py")
set(CASE_LABELS_BASELINE "${SOURCE_ROOT}/src/tests/case_labels.txt")
foreach(REQUIRED IN ITEMS "${CASE_LABELS_TOOL}" "${CASE_LABELS_BASELINE}")
    if(NOT EXISTS "${REQUIRED}")
        message(FATAL_ERROR "CaseLabels: ${REQUIRED} does not exist (moved?)")
    endif()
endforeach()

# The interpreter: the one CMake found for the other Python tests (CMakeLists passes
# -DCASE_LABELS_PYTHON=${Python3_EXECUTABLE}); when none was passed (a standalone -P run, or CMake
# found none), the first working python3, python or py. A passed interpreter that does not run
# Python 3 fails, and so does finding none.
function(case_labels_python_works EXE OUT_VAR)
    execute_process(COMMAND "${EXE}" -c "import sys; sys.exit(0 if sys.version_info >= (3, 6) else 1)"
        RESULT_VARIABLE PROBE OUTPUT_QUIET ERROR_QUIET)
    if(PROBE EQUAL 0)
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

if(CASE_LABELS_PYTHON)
    case_labels_python_works("${CASE_LABELS_PYTHON}" WORKS)
    if(NOT WORKS)
        message(FATAL_ERROR "CaseLabels: CASE_LABELS_PYTHON '${CASE_LABELS_PYTHON}' does not run Python 3.6 or later.")
    endif()
else()
    set(CASE_LABELS_PYTHON "")
    foreach(CANDIDATE IN ITEMS python3 python py)
        find_program(CASE_LABELS_PYTHON_${CANDIDATE} NAMES ${CANDIDATE})
        if(CASE_LABELS_PYTHON_${CANDIDATE})
            case_labels_python_works("${CASE_LABELS_PYTHON_${CANDIDATE}}" WORKS)
            if(WORKS)
                set(CASE_LABELS_PYTHON "${CASE_LABELS_PYTHON_${CANDIDATE}}")
                break()
            endif()
        endif()
    endforeach()
    if(NOT CASE_LABELS_PYTHON)
        message(FATAL_ERROR "CaseLabels: no Python 3 interpreter (CASE_LABELS_PYTHON, python3, python or py) was "
            "found; src/tests/tools/case_labels.py needs one, and a gate that cannot run must not pass.")
    endif()
endif()

execute_process(
    COMMAND "${CASE_LABELS_PYTHON}" "${CASE_LABELS_TOOL}" --root "${SOURCE_ROOT}" --baseline "${CASE_LABELS_BASELINE}" --check
    OUTPUT_VARIABLE CASE_LABELS_OUTPUT
    ERROR_VARIABLE CASE_LABELS_ERROR
    RESULT_VARIABLE CASE_LABELS_RESULT)
string(STRIP "${CASE_LABELS_OUTPUT}" CASE_LABELS_OUTPUT)
if(NOT CASE_LABELS_RESULT EQUAL 0)
    message(FATAL_ERROR "CheckCaseLabels failed (decoupling D11, ${CASE_LABELS_PYTHON}):\n"
        "${CASE_LABELS_OUTPUT}\n${CASE_LABELS_ERROR}\n"
        "What each failure above asks for:\n"
        "  - a new spell-ID label: register the case as a handler in the spell handler registry "
        "(src/game/spells/handlers/) instead of adding a label; never raise src/tests/case_labels.txt.\n"
        "  - a stale baseline: labels moved into the registry; lower the file's line in this PR "
        "(case_labels.py --generate).\n"
        "  - a malformed or duplicate baseline line: one '<path> <count>' per file.")
endif()
message(STATUS "${CASE_LABELS_OUTPUT}")
