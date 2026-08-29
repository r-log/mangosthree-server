# The Montgomery kernel has one assembly source, MontgomeryAsm.S (GAS); MSVC assembles
# MontgomeryAsm.asm, generated from it by src/tests/tools/gas2masm.py. This regenerates
# the twin and compares it with the committed file, so an edit to one cannot go unnoticed
# in the other. Python is required on Linux (the CI runners have it); on Windows the
# check is skipped with a notice when no interpreter is found.
# An interpreter that actually runs: on Windows a `python3` may be the Store's
# app-execution alias, which exits non-zero without a word.
set(PYTHON_FOR_TWIN "")
foreach(candidate IN ITEMS python3 python py)
    find_program(PYTHON_CANDIDATE_${candidate} NAMES ${candidate})
    if(PYTHON_CANDIDATE_${candidate})
        execute_process(COMMAND "${PYTHON_CANDIDATE_${candidate}}" --version
                        RESULT_VARIABLE probe OUTPUT_QUIET ERROR_QUIET)
        if(probe EQUAL 0)
            set(PYTHON_FOR_TWIN "${PYTHON_CANDIDATE_${candidate}}")
            break()
        endif()
    endif()
endforeach()
if(NOT PYTHON_FOR_TWIN)
    if(CMAKE_HOST_WIN32)
        message(STATUS "CheckAsmTwin: no working Python interpreter found; the twin is checked on the Linux jobs")
        return()
    endif()
    message(FATAL_ERROR "CheckAsmTwin: python3 is required to regenerate MontgomeryAsm.asm")
endif()

set(GAS_SOURCE "${SOURCE_ROOT}/src/shared/Crypto/MontgomeryAsm.S")
set(MASM_TWIN "${SOURCE_ROOT}/src/shared/Crypto/MontgomeryAsm.asm")
set(TRANSLATOR "${SOURCE_ROOT}/src/tests/tools/gas2masm.py")

execute_process(
    COMMAND "${PYTHON_FOR_TWIN}" "${TRANSLATOR}" "${GAS_SOURCE}"
    OUTPUT_VARIABLE GENERATED
    ERROR_VARIABLE TRANSLATOR_ERROR
    RESULT_VARIABLE TRANSLATOR_RESULT)
if(NOT TRANSLATOR_RESULT EQUAL 0)
    message(FATAL_ERROR "CheckAsmTwin: gas2masm.py failed: ${TRANSLATOR_ERROR}")
endif()

file(READ "${MASM_TWIN}" COMMITTED)
string(REPLACE "\r\n" "\n" GENERATED "${GENERATED}")
string(REPLACE "\r\n" "\n" COMMITTED "${COMMITTED}")
if(NOT GENERATED STREQUAL COMMITTED)
    message(FATAL_ERROR
        "CheckAsmTwin: src/shared/Crypto/MontgomeryAsm.asm is not what gas2masm.py generates from MontgomeryAsm.S. "
        "Regenerate it: python3 src/tests/tools/gas2masm.py src/shared/Crypto/MontgomeryAsm.S > src/shared/Crypto/MontgomeryAsm.asm")
endif()
message(STATUS "CheckAsmTwin: MontgomeryAsm.asm is current")
