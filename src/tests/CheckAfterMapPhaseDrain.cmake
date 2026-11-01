# Checks that World::Update in src/game/app/World.cpp calls ApplyAfterMapPhasePosts() after sMapMgr.Update.
include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(AfterMapPhaseDrain)

set(WORLD_CPP "${SOURCE_ROOT}/src/game/app/World.cpp")
if(NOT EXISTS "${WORLD_CPP}")
    message(FATAL_ERROR "after-phase drain: the world's source is missing: ${WORLD_CPP}")
endif()

file(READ "${WORLD_CPP}" WORLD_SOURCE)
string(REPLACE "\r" "" WORLD_SOURCE "${WORLD_SOURCE}")
string(REPLACE ";" "<semicolon>" WORLD_SOURCE "${WORLD_SOURCE}")
string(REPLACE "[" "<open>" WORLD_SOURCE "${WORLD_SOURCE}")
string(REPLACE "]" "<close>" WORLD_SOURCE "${WORLD_SOURCE}")
string(REPLACE "\n" ";" WORLD_LINES "${WORLD_SOURCE}")

set(HEAD "void World::Update(uint32 diff)")
set(MAP_UPDATE "sMapMgr.Update(diff)<semicolon>")
set(HEADS 0)
set(IN_UPDATE OFF)
set(MAP_UPDATES 0)
set(AWAIT_NEXT OFF)
set(NEXT "")
set(AWAIT_SECOND OFF)
set(SECOND "")
set(LINE_NO 0)

foreach(LINE IN LISTS WORLD_LINES)
    math(EXPR LINE_NO "${LINE_NO} + 1")

    if("${LINE}" STREQUAL "${HEAD}")
        math(EXPR HEADS "${HEADS} + 1")
        set(IN_UPDATE ON)
        set(HEAD_LINE ${LINE_NO})
        continue()
    endif()

    if(NOT IN_UPDATE)
        continue()
    endif()

    if("${LINE}" STREQUAL "}")
        set(IN_UPDATE OFF)
        continue()
    endif()

    string(STRIP "${LINE}" CODE)
    if("${CODE}" STREQUAL "" OR "${CODE}" MATCHES "^//")
        continue()
    endif()

    if(AWAIT_SECOND)
        set(SECOND "${CODE}")
        set(SECOND_LINE ${LINE_NO})
        set(AWAIT_SECOND OFF)
    endif()

    if(AWAIT_NEXT)
        set(NEXT "${CODE}")
        set(NEXT_LINE ${LINE_NO})
        set(AWAIT_NEXT OFF)
        set(AWAIT_SECOND ON)
    endif()

    if("${CODE}" STREQUAL "${MAP_UPDATE}")
        math(EXPR MAP_UPDATES "${MAP_UPDATES} + 1")
        set(MAP_UPDATE_LINE ${LINE_NO})
        set(AWAIT_NEXT ON)
    endif()
endforeach()

if(NOT HEADS EQUAL 1)
    message(FATAL_ERROR
        "after-phase drain: World.cpp holds '${HEAD}' ${HEADS} time(s), not once.\n"
        "The gate reads the statement after the map update in that one definition; if the\n"
        "signature changed, update the gate together with it.")
endif()

if(NOT MAP_UPDATES EQUAL 1)
    message(FATAL_ERROR
        "after-phase drain: World::Update (World.cpp:${HEAD_LINE}) holds 'sMapMgr.Update(diff);'\n"
        "${MAP_UPDATES} time(s), not once. The drain belongs right after the one map update.")
endif()

string(REPLACE "<semicolon>" ";" NEXT "${NEXT}")
string(REPLACE "<open>" "[" NEXT "${NEXT}")
string(REPLACE "<close>" "]" NEXT "${NEXT}")

if(NOT "${NEXT}" STREQUAL "ApplyAfterMapPhasePosts();")
    message(FATAL_ERROR
        "after-phase drain: the statement after 'sMapMgr.Update(diff);' (World.cpp:${MAP_UPDATE_LINE}) is\n"
        "'${NEXT}' (World.cpp:${NEXT_LINE}), not 'ApplyAfterMapPhasePosts();'. The posts the map\n"
        "threads made during the map phase are applied on the world thread once every map update\n"
        "has returned, before anything else in the world tick runs.")
endif()

string(REPLACE "<semicolon>" ";" SECOND "${SECOND}")

if(NOT "${SECOND}" STREQUAL "ApplyOffMapAuraUpdates();")
    message(FATAL_ERROR
        "after-phase drain: the statement after 'ApplyAfterMapPhasePosts();' (World.cpp:${NEXT_LINE}) is\n"
        "'${SECOND}' (World.cpp:${SECOND_LINE}), not 'ApplyOffMapAuraUpdates();'. The aura holder updates\n"
        "the map threads left for the world thread run once every map update has returned, right\n"
        "after the after-phase posts.")
endif()

message(STATUS "after-phase drain: World::Update applies the after-phase posts, then the off-map aura updates, right after sMapMgr.Update(diff)")
