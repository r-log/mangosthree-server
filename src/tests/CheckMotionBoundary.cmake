include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(MotionBoundary)

set(KERNEL_DIR "${SOURCE_ROOT}/src/motion")
set(MOTION_DIRS "${KERNEL_DIR}")

set(MOTION_SOURCES "")
foreach(MOTION_DIR IN LISTS MOTION_DIRS)
    if(NOT IS_DIRECTORY "${MOTION_DIR}")
        message(FATAL_ERROR "Motion boundary missing: ${MOTION_DIR}")
    endif()
    file(GLOB_RECURSE DIR_SOURCES
        "${MOTION_DIR}/*.h" "${MOTION_DIR}/*.hpp"
        "${MOTION_DIR}/*.cpp" "${MOTION_DIR}/*.cc")
    list(APPEND MOTION_SOURCES ${DIR_SOURCES})
endforeach()
list(LENGTH MOTION_SOURCES MOTION_SOURCE_COUNT)
gate_require_scanned(MotionBoundary "${MOTION_SOURCE_COUNT}" "sources in the motion layer")

set(FORBIDDEN_PATTERNS
    "#[ \t]*include[ \t]*[\"<](Database/|World\\.h|WorldSession\\.h|AddonHandler\\.h|Warden)"
    "#[ \t]*include[ \t]*[\"<](Unit\\.h|Player\\.h|Creature\\.h|SharedDefines\\.h|Object/|WorldHandlers/|Server/|ChatCommands/|Maps/|MotionGenerators/)"
    "(^|[^A-Za-z0-9_])(WorldSession|sWorld|LoginDatabase|CharacterDatabase|WorldDatabase|sAddOnHandler|Warden)([^A-Za-z0-9_]|$)")
set(KERNEL_PATTERNS
    "(^|[^A-Za-z0-9_])sLog([^A-Za-z0-9_]|$)")

set(VIOLATIONS "")

foreach(FILE_PATH IN LISTS MOTION_SOURCES)
    file(STRINGS "${FILE_PATH}" RAW_LINES)

    set(IN_BLOCK OFF)
    set(CODE_ONLY "")

    foreach(LINE IN LISTS RAW_LINES)
        if(IN_BLOCK)
            string(FIND "${LINE}" "*/" CLOSE_AT)
            if(CLOSE_AT EQUAL -1)
                continue()
            endif()
            math(EXPR CLOSE_AT "${CLOSE_AT} + 2")
            string(SUBSTRING "${LINE}" ${CLOSE_AT} -1 LINE)
            set(IN_BLOCK OFF)
        endif()

        string(REGEX REPLACE "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/" " " LINE "${LINE}")

        string(FIND "${LINE}" "/*" OPEN_AT)
        if(NOT OPEN_AT EQUAL -1)
            string(SUBSTRING "${LINE}" 0 ${OPEN_AT} LINE)
            set(IN_BLOCK ON)
        endif()

        string(REGEX REPLACE "//.*$" "" LINE "${LINE}")

        string(APPEND CODE_ONLY "${LINE}\n")
    endforeach()

    set(PATTERNS ${FORBIDDEN_PATTERNS})
    string(FIND "${FILE_PATH}" "${KERNEL_DIR}/" IN_KERNEL)
    string(FIND "${FILE_PATH}" "${KERNEL_DIR}/movement/" IN_MOVEMENT)
    if(IN_KERNEL EQUAL 0 AND NOT IN_MOVEMENT EQUAL 0)
        list(APPEND PATTERNS ${KERNEL_PATTERNS})
    endif()

    foreach(PATTERN IN LISTS PATTERNS)
        if(CODE_ONLY MATCHES "${PATTERN}")
            list(APPEND VIOLATIONS "${FILE_PATH}: ${CMAKE_MATCH_0}")
        endif()
    endforeach()
endforeach()

if(VIOLATIONS)
    string(REPLACE ";" "\n  " VIOLATIONS "${VIOLATIONS}")
    message(FATAL_ERROR "Forbidden game dependency in the motion library:\n  ${VIOLATIONS}")
endif()
