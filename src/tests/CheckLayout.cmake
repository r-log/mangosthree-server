cmake_minimum_required(VERSION 3.18)
include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(Layout)
get_filename_component(SOURCE_ROOT "${SOURCE_ROOT}" ABSOLUTE)

set(LAYOUT_TOOL "${SOURCE_ROOT}/src/tests/tools/layout_gate.py")
set(LAYOUT_ALLOW "${SOURCE_ROOT}/src/tests/layout_allow.txt")
foreach(REQUIRED IN ITEMS "${LAYOUT_TOOL}" "${LAYOUT_ALLOW}")
    if(NOT EXISTS "${REQUIRED}")
        message(FATAL_ERROR "Layout: ${REQUIRED} does not exist (moved?)")
    endif()
endforeach()

function(layout_python_works EXE OUT_VAR)
    execute_process(COMMAND "${EXE}" -c "import sys; sys.exit(0 if sys.version_info >= (3, 6) else 1)"
        RESULT_VARIABLE PROBE OUTPUT_QUIET ERROR_QUIET)
    if(PROBE EQUAL 0)
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

if(LAYOUT_PYTHON)
    layout_python_works("${LAYOUT_PYTHON}" WORKS)
    if(NOT WORKS)
        message(FATAL_ERROR "Layout: LAYOUT_PYTHON '${LAYOUT_PYTHON}' does not run Python 3.6 or later.")
    endif()
else()
    set(LAYOUT_PYTHON "")
    foreach(CANDIDATE IN ITEMS python3 python py)
        find_program(LAYOUT_PYTHON_${CANDIDATE} NAMES ${CANDIDATE})
        if(LAYOUT_PYTHON_${CANDIDATE})
            layout_python_works("${LAYOUT_PYTHON_${CANDIDATE}}" WORKS)
            if(WORKS)
                set(LAYOUT_PYTHON "${LAYOUT_PYTHON_${CANDIDATE}}")
                break()
            endif()
        endif()
    endforeach()
    if(NOT LAYOUT_PYTHON)
        message(FATAL_ERROR "Layout: no Python 3 interpreter (LAYOUT_PYTHON, python3, python or py) was found; "
            "src/tests/tools/layout_gate.py needs one, and a gate that cannot run must not pass.")
    endif()
endif()

execute_process(
    COMMAND "${LAYOUT_PYTHON}" "${LAYOUT_TOOL}" --root "${SOURCE_ROOT}" --allow "${LAYOUT_ALLOW}" --check
    OUTPUT_VARIABLE LAYOUT_OUTPUT
    ERROR_VARIABLE LAYOUT_ERROR
    RESULT_VARIABLE LAYOUT_RESULT)
string(STRIP "${LAYOUT_OUTPUT}" LAYOUT_OUTPUT)
if(NOT LAYOUT_RESULT EQUAL 0)
    message(FATAL_ERROR "CheckLayout failed (design/architecture.md sections 1 and 4, ${LAYOUT_PYTHON}):\n"
        "${LAYOUT_OUTPUT}\n${LAYOUT_ERROR}\n"
        "Run it locally: python src/tests/tools/layout_gate.py --check (the rules: --help; the fixtures: "
        "--self-test).\n"
        "What each failure above asks for:\n"
        "  - a new edge against the rule, into a gated seam, or sideways between two domain directories: "
        "remove the include (forward-declare, move it to the .cpp, or move the code to its layer); "
        "never add it to src/tests/layout_allow.txt. A sideways edge is keyed per includer directory, "
        "includer peer and header: a new file of a listed peer in a listed directory including a listed "
        "header is not new.\n"
        "  - moved: an against-the-rule or seam include went from one includer file to another of the same "
        "layer pair and the header's count for that pair did not grow: replace the old line with the new one it "
        "names (or --generate).\n"
        "  - an allow-list line whose edge is gone or allowed now: delete the line (the PR that removes an "
        "edge removes its line; --generate rewrites the list).\n"
        "  - a file no RULES row classifies: give its directory a layer in layout_gate.py's RULES and in the "
        "page's section 1.\n"
        "  - a quoted header the tree does not have: fix the spelling, or add the header (third-party ones "
        "live under dep/).\n"
        "  - an ambiguous spelling (two or more files end with it, none next to the includer): include it by "
        "a longer path, or relative to the includer, that names one file.\n"
        "  - a malformed or duplicate allow-list line: fix it to one '<includer> -> <header>' per edge, or "
        "delete the copy.")
endif()
message(STATUS "${LAYOUT_OUTPUT}")
