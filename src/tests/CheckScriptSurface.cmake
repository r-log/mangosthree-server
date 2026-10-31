cmake_minimum_required(VERSION 3.18)
include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(ScriptSurface)
get_filename_component(SOURCE_ROOT "${SOURCE_ROOT}" ABSOLUTE)

set(SCRIPT_SURFACE_TOOL "${SOURCE_ROOT}/src/tests/tools/script_surface.py")
if(NOT EXISTS "${SCRIPT_SURFACE_TOOL}")
    message(FATAL_ERROR "ScriptSurface: ${SCRIPT_SURFACE_TOOL} does not exist (moved?)")
endif()
if(NOT SCRIPT_SURFACE_PYTHON)
    message(FATAL_ERROR "ScriptSurface: no Python 3 interpreter was found; src/tests/tools/script_surface.py needs one, "
        "and a gate that cannot run must not pass.")
endif()
if(NOT SYMBOL_FORMAT OR NOT SYMBOL_TOOL OR NOT EXISTS "${SYMBOL_TOOL}")
    message(FATAL_ERROR "ScriptSurface: no symbol tool was found (nm, or dumpbin beside the MSVC linker; got "
        "'${SYMBOL_FORMAT}' '${SYMBOL_TOOL}'), and a symbol check that cannot read symbols must not pass.")
endif()

set(SCRIPT_SURFACE_ARGS --root "${SOURCE_ROOT}" --check --symbols "${SYMBOL_FORMAT}" "${SYMBOL_TOOL}")
string(REPLACE "|" ";" CORE_LIB_LIST "${CORE_LIBS}")
foreach(LIB IN LISTS CORE_LIB_LIST)
    list(APPEND SCRIPT_SURFACE_ARGS --core-lib "${LIB}")
endforeach()
string(REPLACE "|" ";" CONTENT_OBJECT_LIST "${CONTENT_OBJECTS}")
foreach(OBJ IN LISTS CONTENT_OBJECT_LIST)
    list(APPEND SCRIPT_SURFACE_ARGS --content-object "${OBJ}")
endforeach()

execute_process(
    COMMAND "${SCRIPT_SURFACE_PYTHON}" "${SCRIPT_SURFACE_TOOL}" ${SCRIPT_SURFACE_ARGS}
    OUTPUT_VARIABLE SCRIPT_SURFACE_OUTPUT
    ERROR_VARIABLE SCRIPT_SURFACE_ERROR
    RESULT_VARIABLE SCRIPT_SURFACE_RESULT)
string(STRIP "${SCRIPT_SURFACE_OUTPUT}" SCRIPT_SURFACE_OUTPUT)
if(NOT SCRIPT_SURFACE_RESULT EQUAL 0)
    message(FATAL_ERROR "CheckScriptSurface failed (design/architecture.md sections 4 and 7):\n"
        "${SCRIPT_SURFACE_OUTPUT}\n${SCRIPT_SURFACE_ERROR}\n"
        "Run it locally: python src/tests/tools/script_surface.py --check (the rules: --help; the fixtures: "
        "--self-test).\n"
        "What each failure above asks for:\n"
        "  - a new script or a surface header including a header outside the surface: reach the core through "
        "the surface; a verb the surface lacks lands as its family, with tests.\n"
        "  - a content object referring to a core symbol: the script declared a core function by hand; call the "
        "surface instead.\n"
        "  - a file outside src/scripts/legacy/ including a legacy header: only the legacy library and "
        "src/mangosd/LegacyScripts.cpp include it.\n"
        "  - no surface header, or no symbol read from a core library: the tree or the build is not where the "
        "gate looks; fix the path, do not skip the gate.")
endif()
message(STATUS "${SCRIPT_SURFACE_OUTPUT}")
