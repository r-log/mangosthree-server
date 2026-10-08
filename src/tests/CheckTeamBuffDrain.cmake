# Checks that Map::Update in src/game/WorldHandlers/Map.cpp opens its MapPhase::Scope and then calls ApplyTeamBuffs().
include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(TeamBuffDrain)

set(MAP_CPP "${SOURCE_ROOT}/src/game/WorldHandlers/Map.cpp")
if(NOT EXISTS "${MAP_CPP}")
    message(FATAL_ERROR "team buff drain: the map's source is missing: ${MAP_CPP}")
endif()

file(READ "${MAP_CPP}" MAP_SOURCE)
string(REPLACE "\r" "" MAP_SOURCE "${MAP_SOURCE}")
string(REPLACE ";" "<semicolon>" MAP_SOURCE "${MAP_SOURCE}")
string(REPLACE "[" "<open>" MAP_SOURCE "${MAP_SOURCE}")
string(REPLACE "]" "<close>" MAP_SOURCE "${MAP_SOURCE}")
string(REPLACE "\n" ";" MAP_LINES "${MAP_SOURCE}")

set(HEAD "void Map::Update(const uint32& t_diff)")
set(HEADS 0)
set(IN_UPDATE OFF)
set(CODE_LINES "")
set(LINE_NO 0)

foreach(LINE IN LISTS MAP_LINES)
    math(EXPR LINE_NO "${LINE_NO} + 1")
    string(STRIP "${LINE}" CODE)

    if("${CODE}" STREQUAL "${HEAD}")
        math(EXPR HEADS "${HEADS} + 1")
        set(IN_UPDATE ON)
        set(HEAD_LINE ${LINE_NO})
        continue()
    endif()

    if(NOT IN_UPDATE)
        continue()
    endif()

    if("${CODE}" STREQUAL "" OR "${CODE}" STREQUAL "{" OR "${CODE}" MATCHES "^//")
        continue()
    endif()

    list(LENGTH CODE_LINES CODE_COUNT)
    if(CODE_COUNT LESS 2)
        list(APPEND CODE_LINES "${CODE}")
        list(APPEND CODE_LINE_NOS ${LINE_NO})
    else()
        set(IN_UPDATE OFF)
    endif()
endforeach()

if(NOT HEADS EQUAL 1)
    message(FATAL_ERROR
        "team buff drain: Map.cpp holds '${HEAD}' ${HEADS} time(s), not once.\n"
        "The gate reads the first two statements of that one definition; if the signature\n"
        "changed, update the gate together with it.")
endif()

list(LENGTH CODE_LINES CODE_COUNT)
if(CODE_COUNT LESS 2)
    message(FATAL_ERROR
        "team buff drain: Map::Update (Map.cpp:${HEAD_LINE}) ends before two statements.")
endif()

list(GET CODE_LINES 0 FIRST)
list(GET CODE_LINES 1 SECOND)
list(GET CODE_LINE_NOS 0 FIRST_NO)
list(GET CODE_LINE_NOS 1 SECOND_NO)
string(REPLACE "<semicolon>" ";" FIRST "${FIRST}")
string(REPLACE "<semicolon>" ";" SECOND "${SECOND}")
string(REPLACE "<open>" "[" FIRST "${FIRST}")
string(REPLACE "<close>" "]" FIRST "${FIRST}")
string(REPLACE "<open>" "[" SECOND "${SECOND}")
string(REPLACE "<close>" "]" SECOND "${SECOND}")

if(NOT "${FIRST}" STREQUAL "MapPhase::Scope phase(this);")
    message(FATAL_ERROR
        "team buff drain: the first statement of Map::Update is '${FIRST}' (Map.cpp:${FIRST_NO}),\n"
        "not 'MapPhase::Scope phase(this);'. The map must own itself before it applies the team\n"
        "buffs posted to it, so the scope comes first and the drain right after it.")
endif()

if(NOT "${SECOND}" STREQUAL "ApplyTeamBuffs();")
    message(FATAL_ERROR
        "team buff drain: the statement after Map::Update's MapPhase::Scope is '${SECOND}'\n"
        "(Map.cpp:${SECOND_NO}), not 'ApplyTeamBuffs();'. A team buff posted by another map's\n"
        "thread must land before this map's session drain, player updates and object updates,\n"
        "so the drain is the first thing the map does once it owns itself.")
endif()

message(STATUS "team buff drain: Map::Update applies the posted team buffs right after its MapPhase::Scope")
