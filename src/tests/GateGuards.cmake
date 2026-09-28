# Decoupling D4l: the two guards every gate in src/tests shares. A text gate that reads nothing
# finds nothing, and a gate that finds nothing passes -- so a gate pointed at the wrong tree was
# a green gate. The D4h review ran CheckManagerIsolation.cmake with a relative SOURCE_ROOT: its
# globs listed no file and it passed. Each gate includes this file and calls:
#   - gate_require_source_root(<gate>) before it reads anything: SOURCE_ROOT must be set, be an
#     absolute path, and hold a src/ directory. A relative root is refused even where it would
#     happen to work: file(GLOB) and file(RELATIVE_PATH) treat it differently, which is how the
#     hole was made.
#   - gate_require_scanned(<gate> <count> <what>) after each scan whose result may not be empty:
#     zero files of a kind the gate expects (sources, headers, listed files, rules) is an error.
# The self-test below runs on every include, so each gate proves the guard before it trusts it.
# Run standalone (-P), a gate sees none of the top-level project's policies; include() gives this
# file a policy scope of its own, so the floor set here does not leak into the gate.
cmake_minimum_required(VERSION 3.18)
include_guard(GLOBAL)

# Why ROOT cannot be a gate's SOURCE_ROOT, or "" when it can.
function(gate_source_root_problem ROOT OUT_VAR)
    if("${ROOT}" STREQUAL "")
        set(PROBLEM "is not set")
    elseif(NOT IS_ABSOLUTE "${ROOT}")
        set(PROBLEM "'${ROOT}' is a relative path")
    elseif(NOT IS_DIRECTORY "${ROOT}/src")
        set(PROBLEM "'${ROOT}' has no src/ directory (it does not exist, or it is not the repo root)")
    else()
        set(PROBLEM "")
    endif()
    set(${OUT_VAR} "${PROBLEM}" PARENT_SCOPE)
endfunction()

function(gate_require_source_root GATE)
    gate_source_root_problem("${SOURCE_ROOT}" PROBLEM)
    if(PROBLEM)
        message(FATAL_ERROR "${GATE}: SOURCE_ROOT ${PROBLEM}. Pass -DSOURCE_ROOT=<the absolute path of the "
            "repo root>; a gate given the wrong root scans nothing and would pass.")
    endif()
endfunction()

# TRUE when a scan's count means it read nothing: zero, or no count at all.
function(gate_scanned_nothing COUNT OUT_VAR)
    if("${COUNT}" STREQUAL "" OR "${COUNT}" EQUAL 0)
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(gate_require_scanned GATE COUNT WHAT)
    gate_scanned_nothing("${COUNT}" NOTHING)
    if(NOTHING)
        message(FATAL_ERROR "${GATE}: found no ${WHAT} under ${SOURCE_ROOT} -- a gate that scans nothing "
            "passes nothing. Fix the path (moved or renamed?) instead of letting the gate go inert.")
    endif()
endfunction()

# Self-test: the guard refuses an unset, a relative and a nonexistent root and accepts this tree;
# a count of zero or none is an empty scan, a count of one is not.
get_filename_component(GATE_GUARDS_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
foreach(GATE_GUARDS_CASE IN ITEMS "unset|" "relative|src/.." "relative, one level|server"
        "nonexistent|${GATE_GUARDS_REPO_ROOT}/no-such-root" "not the repo root|${GATE_GUARDS_REPO_ROOT}/src/tests")
    string(FIND "${GATE_GUARDS_CASE}" "|" GATE_GUARDS_BAR)
    string(SUBSTRING "${GATE_GUARDS_CASE}" 0 ${GATE_GUARDS_BAR} GATE_GUARDS_LABEL)
    math(EXPR GATE_GUARDS_BAR "${GATE_GUARDS_BAR} + 1")
    string(SUBSTRING "${GATE_GUARDS_CASE}" ${GATE_GUARDS_BAR} -1 GATE_GUARDS_ROOT)
    gate_source_root_problem("${GATE_GUARDS_ROOT}" GATE_GUARDS_PROBLEM)
    if(NOT GATE_GUARDS_PROBLEM)
        message(FATAL_ERROR "GateGuards self-test failed (${GATE_GUARDS_LABEL}): '${GATE_GUARDS_ROOT}' was accepted")
    endif()
endforeach()
gate_source_root_problem("${GATE_GUARDS_REPO_ROOT}" GATE_GUARDS_PROBLEM)
if(GATE_GUARDS_PROBLEM)
    message(FATAL_ERROR "GateGuards self-test failed (this tree): ${GATE_GUARDS_PROBLEM}")
endif()
foreach(GATE_GUARDS_CASE IN ITEMS "0|TRUE" "|TRUE" "1|FALSE" "1671|FALSE")
    string(REPLACE "|" ";" GATE_GUARDS_CASE "${GATE_GUARDS_CASE}")
    list(GET GATE_GUARDS_CASE 0 GATE_GUARDS_COUNT)
    list(GET GATE_GUARDS_CASE 1 GATE_GUARDS_EXPECTED)
    gate_scanned_nothing("${GATE_GUARDS_COUNT}" GATE_GUARDS_GOT)
    if(NOT GATE_GUARDS_GOT STREQUAL GATE_GUARDS_EXPECTED)
        message(FATAL_ERROR "GateGuards self-test failed (a count of '${GATE_GUARDS_COUNT}'): "
            "empty scan ${GATE_GUARDS_GOT}, expected ${GATE_GUARDS_EXPECTED}")
    endif()
endforeach()
unset(GATE_GUARDS_CASE)
unset(GATE_GUARDS_BAR)
unset(GATE_GUARDS_LABEL)
unset(GATE_GUARDS_ROOT)
unset(GATE_GUARDS_PROBLEM)
unset(GATE_GUARDS_COUNT)
unset(GATE_GUARDS_EXPECTED)
unset(GATE_GUARDS_GOT)
unset(GATE_GUARDS_REPO_ROOT)
