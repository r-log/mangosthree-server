cmake_minimum_required(VERSION 3.18)
include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(RawRand)
get_filename_component(SOURCE_ROOT "${SOURCE_ROOT}" ABSOLUTE)

set(RAW_RAND_TOOL "${SOURCE_ROOT}/src/tests/tools/raw_rand.py")
if(NOT EXISTS "${RAW_RAND_TOOL}")
    message(FATAL_ERROR "RawRand: ${RAW_RAND_TOOL} does not exist (moved?)")
endif()

function(raw_rand_python_works EXE OUT_VAR)
    execute_process(COMMAND "${EXE}" -c "import sys; sys.exit(0 if sys.version_info >= (3, 6) else 1)"
        RESULT_VARIABLE PROBE OUTPUT_QUIET ERROR_QUIET)
    if(PROBE EQUAL 0)
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

if(RAW_RAND_PYTHON)
    raw_rand_python_works("${RAW_RAND_PYTHON}" WORKS)
    if(NOT WORKS)
        message(FATAL_ERROR "RawRand: RAW_RAND_PYTHON '${RAW_RAND_PYTHON}' does not run Python 3.6 or later.")
    endif()
else()
    set(RAW_RAND_PYTHON "")
    foreach(CANDIDATE IN ITEMS python3 python py)
        find_program(RAW_RAND_PYTHON_${CANDIDATE} NAMES ${CANDIDATE})
        if(RAW_RAND_PYTHON_${CANDIDATE})
            raw_rand_python_works("${RAW_RAND_PYTHON_${CANDIDATE}}" WORKS)
            if(WORKS)
                set(RAW_RAND_PYTHON "${RAW_RAND_PYTHON_${CANDIDATE}}")
                break()
            endif()
        endif()
    endforeach()
    if(NOT RAW_RAND_PYTHON)
        message(FATAL_ERROR "RawRand: no Python 3 interpreter (RAW_RAND_PYTHON, python3, python or py) was "
            "found; src/tests/tools/raw_rand.py needs one, and a gate that cannot run must not pass.")
    endif()
endif()

execute_process(
    COMMAND "${RAW_RAND_PYTHON}" "${RAW_RAND_TOOL}" --root "${SOURCE_ROOT}" --check
    OUTPUT_VARIABLE RAW_RAND_OUTPUT
    ERROR_VARIABLE RAW_RAND_ERROR
    RESULT_VARIABLE RAW_RAND_RESULT)
string(STRIP "${RAW_RAND_OUTPUT}" RAW_RAND_OUTPUT)
if(NOT RAW_RAND_RESULT EQUAL 0)
    message(FATAL_ERROR "CheckRawRand failed (${RAW_RAND_PYTHON}):\n"
        "${RAW_RAND_OUTPUT}\n${RAW_RAND_ERROR}\n"
        "What each failure above asks for:\n"
        "  - a rand() draw: call the seeded RNG instead (urand(0, n - 1) for rand() % n, rand32() for a whole word, "
        "rand_norm() for a fraction in [0, 1)); the harness seeds it, the C library's rand() it never seeds.\n"
        "  - an srand( call: remove it; the one seed left is World.cpp's, for SD3's scripts.")
endif()
message(STATUS "${RAW_RAND_OUTPUT}")
