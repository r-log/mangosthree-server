cmake_minimum_required(VERSION 3.18)
include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(MovementWriters)
get_filename_component(SOURCE_ROOT "${SOURCE_ROOT}" ABSOLUTE)

set(MOVEMENT_WRITERS_TOOL "${SOURCE_ROOT}/src/tests/tools/movement_writers.py")
if(NOT EXISTS "${MOVEMENT_WRITERS_TOOL}")
    message(FATAL_ERROR "MovementWriters: ${MOVEMENT_WRITERS_TOOL} does not exist (moved?)")
endif()

function(movement_writers_python_works EXE OUT_VAR)
    execute_process(COMMAND "${EXE}" -c "import sys; sys.exit(0 if sys.version_info >= (3, 6) else 1)"
        RESULT_VARIABLE PROBE OUTPUT_QUIET ERROR_QUIET)
    if(PROBE EQUAL 0)
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

if(MOVEMENT_WRITERS_PYTHON)
    movement_writers_python_works("${MOVEMENT_WRITERS_PYTHON}" WORKS)
    if(NOT WORKS)
        message(FATAL_ERROR "MovementWriters: MOVEMENT_WRITERS_PYTHON '${MOVEMENT_WRITERS_PYTHON}' does not run Python 3.6 or later.")
    endif()
else()
    set(MOVEMENT_WRITERS_PYTHON "")
    foreach(CANDIDATE IN ITEMS python3 python py)
        find_program(MOVEMENT_WRITERS_PYTHON_${CANDIDATE} NAMES ${CANDIDATE})
        if(MOVEMENT_WRITERS_PYTHON_${CANDIDATE})
            movement_writers_python_works("${MOVEMENT_WRITERS_PYTHON_${CANDIDATE}}" WORKS)
            if(WORKS)
                set(MOVEMENT_WRITERS_PYTHON "${MOVEMENT_WRITERS_PYTHON_${CANDIDATE}}")
                break()
            endif()
        endif()
    endforeach()
    if(NOT MOVEMENT_WRITERS_PYTHON)
        message(FATAL_ERROR "MovementWriters: no Python 3 interpreter (MOVEMENT_WRITERS_PYTHON, python3, python or py) was "
            "found; src/tests/tools/movement_writers.py needs one, and a gate that cannot run must not pass.")
    endif()
endif()

execute_process(
    COMMAND "${MOVEMENT_WRITERS_PYTHON}" "${MOVEMENT_WRITERS_TOOL}" --root "${SOURCE_ROOT}" --check
    OUTPUT_VARIABLE MOVEMENT_WRITERS_OUTPUT
    ERROR_VARIABLE MOVEMENT_WRITERS_ERROR
    RESULT_VARIABLE MOVEMENT_WRITERS_RESULT)
string(STRIP "${MOVEMENT_WRITERS_OUTPUT}" MOVEMENT_WRITERS_OUTPUT)
if(NOT MOVEMENT_WRITERS_RESULT EQUAL 0)
    message(FATAL_ERROR "CheckMovementWriters failed (${MOVEMENT_WRITERS_PYTHON}):\n"
        "${MOVEMENT_WRITERS_OUTPUT}\n${MOVEMENT_WRITERS_ERROR}\n"
        "What each failure above asks for:\n"
        "  - a file names m_movementInfo: read the record through Unit::ClientStatus() or Unit::WireStatus(), "
        "write it through an owner operation (AcceptClientStatus, ResetClientWord, ClearClientFall, "
        "SetTransportStatus, ClearTransportStatus).\n"
        "  - an operation called from a file not on its row: route the write through the code that owns it, or add "
        "the file to the row in src/tests/tools/movement_writers.py with the change that needs it.\n"
        "  - a stale row: a listed file no longer calls the operation; remove it from the row.\n"
        "  - a friend or a const_cast on MovementInfo: remove it; the record has one owner.")
endif()
message(STATUS "${MOVEMENT_WRITERS_OUTPUT}")
