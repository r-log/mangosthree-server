cmake_minimum_required(VERSION 3.18)

foreach(VAR IN ITEMS TESTS_EXE CASE EXPECT)
    if("${${VAR}}" STREQUAL "")
        message(FATAL_ERROR "AbortCase: pass -D${VAR}=<value>")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "MANGOS_TESTS_ABORT_CASE=${CASE}" "${TESTS_EXE}" -only "${CASE}"
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE OUT
    ERROR_VARIABLE ERR)

if("${RESULT}" STREQUAL "0")
    message(FATAL_ERROR "AbortCase: ${CASE} exited 0: the assertion on '${EXPECT}' did not end it.\n${OUT}${ERR}")
endif()

string(FIND "${ERR}" "Error: Assertion in " AT_ASSERT)
string(FIND "${ERR}" " failed: ${EXPECT}" AT_EXPECT)
if(AT_ASSERT EQUAL -1 OR AT_EXPECT EQUAL -1)
    message(FATAL_ERROR "AbortCase: ${CASE} ended (${RESULT}) without the assertion on '${EXPECT}'.\n${OUT}${ERR}")
endif()

message(STATUS "abort case: ${CASE} ended on the assertion on '${EXPECT}' (${RESULT})")
