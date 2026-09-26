# Decoupling D3 (server #77): the god headers do not reach the layers they forward-declare.
# For each rule "header|forbidden,forbidden,..." the transitive includes of the header are
# walked: an include resolves relative to the including file first, then by basename against
# every directory under the four roots this gate globs -- src/game, src/shared, src/proto and
# src/motion -- the same resolution the counter tool (src/tests/tools/include_reach.py) uses.
# The tool indexes two more roots (src/mangosd, src/realmd) for its own basename fallback, but
# neither root defines a header this gate's rules or the game target can reach, so the two
# resolutions agree on every measured number; the tool's wider index is stricter than the
# compiler and so cannot be bypassed by an include the compiler would not find. The walk must
# not touch a forbidden header. Object/Unit.h has one more rule: nothing under src/motion but
# Mobility.h.
# The PCH is never read: with it on, every translation unit sees the world and this check
# would mean nothing.
# Run standalone (-P), this script sees none of the top-level project's policies: without this,
# CMP0057 stays OLD and IN_LIST is parsed as plain text instead of the if() operator it is used
# as below. The project requires CMake >= 3.18, so that is also the floor here.
cmake_minimum_required(VERSION 3.18)

set(GAME_DIR "${SOURCE_ROOT}/src/game")
set(MOTION_DIR "${SOURCE_ROOT}/src/motion")
get_filename_component(MOTION_DIR "${MOTION_DIR}" REALPATH)

set(INCLUDE_DIRS "")
foreach(TOP IN ITEMS "${GAME_DIR}" "${SOURCE_ROOT}/src/shared" "${SOURCE_ROOT}/src/proto" "${MOTION_DIR}")
    list(APPEND INCLUDE_DIRS "${TOP}")
    file(GLOB_RECURSE SUBDIRS LIST_DIRECTORIES true "${TOP}/*")
    foreach(ENTRY IN LISTS SUBDIRS)
        if(IS_DIRECTORY "${ENTRY}")
            list(APPEND INCLUDE_DIRS "${ENTRY}")
        endif()
    endforeach()
endforeach()

# Each entry: "<header relative to src/game>|<forbidden suffix>,<forbidden suffix>,..."
# Commas, not semicolons: a semicolon inside a quoted argument splits the CMake list.
# A forbidden suffix matches any reached path that ends with "/<suffix>".
set(REACH_RULES
    "Object/Player.h|Object/GMTicketMgr.h,Object/Bag.h,Server/DBCStores.h,WorldHandlers/NPCHandler.h,WorldHandlers/Chat.h,Server/WorldSession.h,BattleGround/BattleGround.h,WorldHandlers/Group.h,Object/Pet.h,WorldHandlers/Map.h,WorldHandlers/AchievementMgr.h,Object/CinematicFlyover.h,WorldHandlers/ScriptMgr.h,Database/DatabaseEnv.h"
    "Object/Unit.h|MotionGenerators/MotionMaster.h,motion/State.h,Object/Player.h,Server/WorldSession.h,proto/WorldPacket.h,WorldHandlers/Path.h"
    # Decoupling D5b (server #132): the combat leaves are arithmetic over values. A
    # combat header that reaches the object layer has stopped being one, and the
    # golden vectors in src/tests/CombatGoldenVectors.h could no longer be checked by
    # a test that links nothing.
    "combat/ArmorReduction.h|Object/Unit.h,Object/Object.h,Object/Player.h,Object/Creature.h,WorldHandlers/SpellAuras.h,Server/WorldSession.h,Server/DBCStores.h,Server/DBCStructure.h"
    "combat/MeleeChances.h|Object/Unit.h,Object/Object.h,Object/Player.h,Object/Creature.h,WorldHandlers/SpellAuras.h,Server/WorldSession.h,Server/DBCStores.h,Server/DBCStructure.h"
    "combat/SpellBonus.h|Object/Unit.h,Object/Object.h,Object/Player.h,Object/Creature.h,WorldHandlers/SpellAuras.h,Server/WorldSession.h,Server/DBCStores.h,Server/DBCStructure.h"
    "combat/WeaponDamage.h|Object/Unit.h,Object/Object.h,Object/Player.h,Object/Creature.h,WorldHandlers/SpellAuras.h,Server/WorldSession.h,Server/DBCStores.h,Server/DBCStructure.h"
    # Decoupling D5d (server #136): the aura container is Unit's storage, and Unit.h includes
    # it. It stores Aura* and SpellAuraHolder* without ever dereferencing one, so it must
    # forward-declare both; the moment it reaches SpellAuras.h or the object layer the
    # sentinel-pointer test in src/tests/AuraContainerTest.cpp stops being possible, and
    # Unit.h's own include graph has grown a cycle.
    "spells/AuraContainer.h|Object/Unit.h,Object/Object.h,WorldHandlers/SpellAuras.h,Server/DBCStructure.h,Server/WorldSession.h"
    # Decoupling D4a: a character manager is state plus parameters. QuestStatusMgr.h reaching
    # the character, the unit, the session or the object manager would put the templates and
    # the owner back in reach, and src/tests/QuestStatusMgrTest.cpp builds it from nothing.
    "entities/player/QuestStatusMgr.h|Object/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h"
    # The .cpp as well: CheckManagerIsolation.cmake is a text gate, and the one thing it cannot
    # see -- the name spelled around its patterns -- only compiles against the complete type,
    # which the .cpp could otherwise include without the header noticing.
    # Decoupling D4b: and no global the manager could reach for instead of a parameter -- the
    # world (sWorld: game time, config) and the object registries that replaced the old
    # ObjectAccessor.h (sPlayerRegistry, ObjectLookup, sCorpseManager).
    "entities/player/QuestStatusMgr.cpp|Object/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,Object/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h")
set(MOTION_ONLY_HEADER "Object/Unit.h")
set(MOTION_ALLOWED "Mobility.h")

function(resolve_include INCLUDE FROM_DIR OUT_VAR)
    set(CANDIDATE "${FROM_DIR}/${INCLUDE}")
    if(EXISTS "${CANDIDATE}" AND NOT IS_DIRECTORY "${CANDIDATE}")
        get_filename_component(CANDIDATE "${CANDIDATE}" REALPATH)
        set(${OUT_VAR} "${CANDIDATE}" PARENT_SCOPE)
        return()
    endif()
    get_filename_component(BASENAME "${INCLUDE}" NAME)
    foreach(DIR IN LISTS INCLUDE_DIRS)
        foreach(NAME IN ITEMS "${INCLUDE}" "${BASENAME}")
            set(CANDIDATE "${DIR}/${NAME}")
            if(EXISTS "${CANDIDATE}" AND NOT IS_DIRECTORY "${CANDIDATE}")
                get_filename_component(CANDIDATE "${CANDIDATE}" REALPATH)
                set(${OUT_VAR} "${CANDIDATE}" PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endforeach()
    set(${OUT_VAR} "" PARENT_SCOPE)
endfunction()

function(walk_includes START OUT_VAR)
    set(QUEUE "${START}")
    set(SEEN "")
    while(QUEUE)
        list(POP_FRONT QUEUE CURRENT)
        file(STRINGS "${CURRENT}" LINES REGEX "^[ \t]*#[ \t]*include")
        get_filename_component(CURRENT_DIR "${CURRENT}" DIRECTORY)
        foreach(LINE IN LISTS LINES)
            string(REGEX MATCH "[\"<]([^\">]+)[\">]" _ "${LINE}")
            set(INCLUDE "${CMAKE_MATCH_1}")
            if(NOT INCLUDE)
                continue()
            endif()
            resolve_include("${INCLUDE}" "${CURRENT_DIR}" RESOLVED)
            if(RESOLVED AND NOT RESOLVED IN_LIST SEEN)
                list(APPEND SEEN "${RESOLVED}")
                list(APPEND QUEUE "${RESOLVED}")
            endif()
        endforeach()
    endwhile()
    set(${OUT_VAR} "${SEEN}" PARENT_SCOPE)
endfunction()

# Parser self-test (decoupling D3, F1): a rule with three forbidden entries must yield three.
string(REPLACE "|" ";" SELF_TEST "x.h|a.h,b.h,c.h")
list(GET SELF_TEST 1 SELF_TEST_FORBIDDEN)
string(REPLACE "," ";" SELF_TEST_FORBIDDEN "${SELF_TEST_FORBIDDEN}")
list(LENGTH SELF_TEST_FORBIDDEN SELF_TEST_COUNT)
if(NOT SELF_TEST_COUNT EQUAL 3)
    message(FATAL_ERROR "Header reach: the rule parser split 'a.h,b.h,c.h' into ${SELF_TEST_COUNT} entries, not 3")
endif()

# Decoupling D4b: a forbidden entry must name a file that exists under the four roots. A rule
# that forbids a header the tree does not have can never fire -- the D4b brief named
# ObjectAccessor.h, which this tree split into PlayerRegistry.h, ObjectLookup.h and
# CorpseManager.h long ago -- so a typo or a deleted header fails here instead of passing inert.
set(TREE_FILES "")
foreach(TOP IN ITEMS "${GAME_DIR}" "${SOURCE_ROOT}/src/shared" "${SOURCE_ROOT}/src/proto" "${MOTION_DIR}")
    file(GLOB_RECURSE TOP_FILES LIST_DIRECTORIES false "${TOP}/*")
    list(APPEND TREE_FILES ${TOP_FILES})
endforeach()

function(forbidden_exists SUFFIX OUT_VAR)
    string(REPLACE "." "\\." SUFFIX_RE "${SUFFIX}")
    set(MATCHES ${TREE_FILES})
    list(FILTER MATCHES INCLUDE REGEX "(^|/)${SUFFIX_RE}$")
    if(MATCHES)
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

forbidden_exists("Object/Player.h" SELF_TEST_FOUND)
forbidden_exists("Object/NoSuchHeader.h" SELF_TEST_MISSING)
forbidden_exists("bject/Player.h" SELF_TEST_PARTIAL)
if(NOT SELF_TEST_FOUND OR SELF_TEST_MISSING OR SELF_TEST_PARTIAL)
    message(FATAL_ERROR "Header reach: the forbidden-entry existence check is broken "
        "(Object/Player.h ${SELF_TEST_FOUND}, Object/NoSuchHeader.h ${SELF_TEST_MISSING}, bject/Player.h ${SELF_TEST_PARTIAL})")
endif()

set(INERT "")
foreach(RULE IN LISTS REACH_RULES)
    string(REPLACE "|" ";" PARTS "${RULE}")
    list(GET PARTS 0 HEADER)
    list(LENGTH PARTS PART_COUNT)
    if(PART_COUNT GREATER 1)
        list(GET PARTS 1 FORBIDDEN)
        string(REPLACE "," ";" FORBIDDEN "${FORBIDDEN}")
        foreach(SUFFIX IN LISTS FORBIDDEN)
            if(NOT SUFFIX)
                continue()
            endif()
            forbidden_exists("${SUFFIX}" FOUND)
            if(NOT FOUND)
                list(APPEND INERT "${HEADER} forbids ${SUFFIX}")
            endif()
        endforeach()
    endif()
endforeach()
if(INERT)
    string(REPLACE ";" "\n  " REPORT "${INERT}")
    message(FATAL_ERROR
        "Header reach: a rule forbids a header that does not exist in the tree, so it can never fire:\n  ${REPORT}\n"
        "Name the header by its current path (renamed or split?).")
endif()

set(VIOLATIONS "")
foreach(RULE IN LISTS REACH_RULES)
    string(REPLACE "|" ";" PARTS "${RULE}")
    list(GET PARTS 0 HEADER)
    list(LENGTH PARTS PART_COUNT)
    set(FORBIDDEN "")
    if(PART_COUNT GREATER 1)
        list(GET PARTS 1 FORBIDDEN)
        string(REPLACE "," ";" FORBIDDEN "${FORBIDDEN}")
    endif()
    get_filename_component(START "${GAME_DIR}/${HEADER}" REALPATH)
    if(NOT EXISTS "${START}")
        message(FATAL_ERROR "Header reach: ${HEADER} does not exist")
    endif()
    walk_includes("${START}" REACHED)
    list(LENGTH REACHED REACHED_COUNT)
    message(STATUS "header reach: ${HEADER} -> ${REACHED_COUNT} headers")
    foreach(SUFFIX IN LISTS FORBIDDEN)
        if(NOT SUFFIX)
            continue()
        endif()
        string(REPLACE "." "\\." SUFFIX_RE "${SUFFIX}")
        foreach(PATH IN LISTS REACHED)
            string(REGEX MATCH "(^|/)${SUFFIX_RE}$" HIT "${PATH}")
            if(HIT)
                list(APPEND VIOLATIONS "${HEADER} reaches ${SUFFIX} (${PATH})")
            endif()
        endforeach()
    endforeach()
    if(HEADER STREQUAL MOTION_ONLY_HEADER)
        foreach(PATH IN LISTS REACHED)
            string(FIND "${PATH}" "${MOTION_DIR}/" AT)
            if(AT EQUAL 0)
                get_filename_component(NAME "${PATH}" NAME)
                if(NOT NAME IN_LIST MOTION_ALLOWED)
                    list(APPEND VIOLATIONS "${HEADER} reaches the movement kernel header ${NAME}; only ${MOTION_ALLOWED} is allowed")
                endif()
            endif()
        endforeach()
    endif()
endforeach()

if(VIOLATIONS)
    string(REPLACE ";" "\n  " REPORT "${VIOLATIONS}")
    message(FATAL_ERROR
        "A god header reaches a layer it must only forward-declare (decoupling D3, server #77):\n  ${REPORT}\n"
        "Forward-declare the type and move the include to the .cpp that needs it.")
endif()
