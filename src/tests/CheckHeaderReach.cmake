# Checks that no file in REACH_RULES reaches a header its rule forbids through its includes, and that
# Object/Unit.h reaches only Mobility.h in src/motion; reads src/game, src/shared, src/proto and src/motion.
cmake_minimum_required(VERSION 3.18)
include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(HeaderReach)

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

set(REACH_RULES
    "entities/player/Player.h|Object/GMTicketMgr.h,Object/Bag.h,Server/DBCStores.h,WorldHandlers/NPCHandler.h,WorldHandlers/Chat.h,Server/WorldSession.h,BattleGround/BattleGround.h,WorldHandlers/Group.h,Object/Pet.h,WorldHandlers/Map.h,WorldHandlers/AchievementMgr.h,Object/CinematicFlyover.h,WorldHandlers/ScriptMgr.h,Database/DatabaseEnv.h"
    "Object/Unit.h|MotionGenerators/MotionMaster.h,motion/State.h,entities/player/Player.h,Server/WorldSession.h,proto/WorldPacket.h,WorldHandlers/Path.h"
    "combat/ArmorReduction.h|Object/Unit.h,Object/Object.h,entities/player/Player.h,Object/Creature.h,WorldHandlers/SpellAuras.h,Server/WorldSession.h,Server/DBCStores.h,Server/DBCStructure.h"
    "combat/MeleeChances.h|Object/Unit.h,Object/Object.h,entities/player/Player.h,Object/Creature.h,WorldHandlers/SpellAuras.h,Server/WorldSession.h,Server/DBCStores.h,Server/DBCStructure.h"
    "combat/SpellBonus.h|Object/Unit.h,Object/Object.h,entities/player/Player.h,Object/Creature.h,WorldHandlers/SpellAuras.h,Server/WorldSession.h,Server/DBCStores.h,Server/DBCStructure.h"
    "combat/WeaponDamage.h|Object/Unit.h,Object/Object.h,entities/player/Player.h,Object/Creature.h,WorldHandlers/SpellAuras.h,Server/WorldSession.h,Server/DBCStores.h,Server/DBCStructure.h"
    "spells/AuraContainer.h|Object/Unit.h,Object/Object.h,WorldHandlers/SpellAuras.h,Server/DBCStructure.h,Server/WorldSession.h"
    "entities/player/quests/QuestStatusMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h"
    "entities/player/quests/QuestStatusMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h"
    "entities/player/quests/QuestCompletePacket.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/QuestDef.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    "entities/player/quests/QuestCompletePacket.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/QuestDef.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    "entities/player/quests/QuestRewardRules.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/QuestDef.h,WorldHandlers/World.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    "entities/player/quests/QuestRewardRules.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    "entities/player/talents/TalentMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    "entities/player/talents/TalentMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h"
    "entities/player/inventory/InventoryMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,Object/Bag.h,Object/Item.h"
    "entities/player/inventory/InventoryMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    "entities/player/spells/RuneMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h,proto/WorldPacket.h"
    "entities/player/spells/RuneMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,Server/DBCStores.h,Database/DatabaseEnv.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h"
    "entities/player/ManagerPacketSink.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,proto/WorldPacket.h"
    "entities/player/PlayerClientFacts.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,proto/WorldPacket.h"
    "entities/GroupUpdateFacts.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,proto/WorldPacket.h,WorldHandlers/Group.h"
    "spells/SpellCooldownMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h,proto/WorldPacket.h,WorldHandlers/Spell.h"
    "spells/SpellCooldownMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h,WorldHandlers/Spell.h,proto/WorldPacket.h,proto/Opcodes.h"
    "spells/SpellModMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h,proto/WorldPacket.h,WorldHandlers/Spell.h"
    "spells/SpellModMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h,WorldHandlers/Spell.h,proto/WorldPacket.h,proto/Opcodes.h"
    "entities/player/talents/GlyphMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h,proto/WorldPacket.h,WorldHandlers/Spell.h"
    "entities/player/talents/GlyphMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h,WorldHandlers/Spell.h,proto/WorldPacket.h"
    "entities/player/pets/PetMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,Object/Pet.h,proto/WorldPacket.h"
    "entities/player/pets/PetMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,Server/DBCStores.h,Database/DatabaseEnv.h,Object/Pet.h"
    "entities/player/social/SocialList.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,proto/WorldPacket.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,entities/player/social/SocialMgr.h"
    "entities/player/social/SocialList.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,Server/DBCStores.h,entities/player/social/SocialMgr.h"
    "entities/player/inventory/CurrencyMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,Server/DBCStores.h,Database/DatabaseEnv.h,WorldHandlers/SpellAuras.h,WorldHandlers/AchievementMgr.h,proto/WorldPacket.h"
    "entities/player/inventory/CurrencyMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,WorldHandlers/SpellAuras.h,WorldHandlers/AchievementMgr.h"
    "entities/player/social/ReputationMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,WorldHandlers/AchievementMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,proto/WorldPacket.h"
    "entities/player/social/ReputationMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,WorldHandlers/AchievementMgr.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h"
    "entities/player/pvp/HonorMgr.h|entities/player/Player.h,Object/Unit.h,Object/Creature.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,WorldHandlers/SpellAuras.h,WorldHandlers/AchievementMgr.h,entities/player/inventory/CurrencyMgr.h,BattleGround/BattleGround.h,Object/Formulas.h,Server/DBCStores.h,Database/DatabaseEnv.h,proto/WorldPacket.h"
    "entities/player/pvp/HonorMgr.cpp|entities/player/Player.h,Object/Unit.h,Object/Creature.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,WorldHandlers/SpellAuras.h,WorldHandlers/AchievementMgr.h,entities/player/inventory/CurrencyMgr.h,BattleGround/BattleGround.h,Object/Formulas.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    "data/MailLevelRewardStore.h|ObjectMgr.h,entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,WorldHandlers/World.h,Database/DatabaseEnv.h,proto/WorldPacket.h"
    "data/ExplorationBaseXpStore.h|ObjectMgr.h,entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,WorldHandlers/World.h,Database/DatabaseEnv.h,proto/WorldPacket.h"
    "data/FishingBaseSkillStore.h|ObjectMgr.h,entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,WorldHandlers/World.h,Database/DatabaseEnv.h,proto/WorldPacket.h"
    "data/PointOfInterestStore.h|ObjectMgr.h,entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,WorldHandlers/World.h,Database/DatabaseEnv.h,proto/WorldPacket.h"
    "data/QuestPOIStore.h|ObjectMgr.h,entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,WorldHandlers/World.h,Database/DatabaseEnv.h,proto/WorldPacket.h"
    "data/DungeonFinderStore.h|ObjectMgr.h,entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,WorldHandlers/World.h,Database/DatabaseEnv.h,proto/WorldPacket.h"
    "data/LfgDungeonEntranceStore.h|ObjectMgr.h,entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,WorldHandlers/World.h,Database/DatabaseEnv.h,proto/WorldPacket.h"
    "session/handlers/combat/CombatHandlers.h|Server/WorldSession.h,entities/player/Player.h,Object/Unit.h,proto/WorldPacket.h,ObjectMgr.h,WorldHandlers/World.h"
    "session/handlers/economy/AuctionHandlers.h|Server/WorldSession.h,entities/player/Player.h,Object/Unit.h,proto/WorldPacket.h,ObjectMgr.h,WorldHandlers/World.h,Object/AuctionHouseMgr.h"
    "session/handlers/economy/LootHandlers.h|Server/WorldSession.h,entities/player/Player.h,Object/Unit.h,proto/WorldPacket.h,ObjectMgr.h,WorldHandlers/World.h"
    "session/handlers/economy/TradeHandlers.h|Server/WorldSession.h,entities/player/Player.h,Object/Unit.h,proto/WorldPacket.h,ObjectMgr.h,WorldHandlers/World.h"
    "session/handlers/economy/VendorHandlers.h|Server/WorldSession.h,entities/player/Player.h,Object/Unit.h,proto/WorldPacket.h,ObjectMgr.h,WorldHandlers/World.h"
    "session/handlers/entities/EnchantHandlers.h|Server/WorldSession.h,entities/player/Player.h,Object/Unit.h,proto/WorldPacket.h,ObjectMgr.h,WorldHandlers/World.h"
    "session/handlers/entities/SkillHandlers.h|Server/WorldSession.h,entities/player/Player.h,Object/Unit.h,proto/WorldPacket.h,ObjectMgr.h,WorldHandlers/World.h"
    "session/handlers/pvp/PvpHandlers.h|Server/WorldSession.h,entities/player/Player.h,Object/Unit.h,proto/WorldPacket.h,ObjectMgr.h,WorldHandlers/World.h,BattleGround/BattleGround.h,BattleGround/BattleGroundMgr.h,Object/ArenaTeam.h")
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

string(REPLACE "|" ";" SELF_TEST "x.h|a.h,b.h,c.h")
list(GET SELF_TEST 1 SELF_TEST_FORBIDDEN)
string(REPLACE "," ";" SELF_TEST_FORBIDDEN "${SELF_TEST_FORBIDDEN}")
list(LENGTH SELF_TEST_FORBIDDEN SELF_TEST_COUNT)
if(NOT SELF_TEST_COUNT EQUAL 3)
    message(FATAL_ERROR "Header reach: the rule parser split 'a.h,b.h,c.h' into ${SELF_TEST_COUNT} entries, not 3")
endif()

set(TREE_FILES "")
foreach(TOP IN ITEMS "${GAME_DIR}" "${SOURCE_ROOT}/src/shared" "${SOURCE_ROOT}/src/proto" "${MOTION_DIR}")
    file(GLOB_RECURSE TOP_FILES LIST_DIRECTORIES false "${TOP}/*")
    list(APPEND TREE_FILES ${TOP_FILES})
endforeach()
list(LENGTH TREE_FILES TREE_FILE_COUNT)
gate_require_scanned(HeaderReach "${TREE_FILE_COUNT}" "files under src/game, src/shared, src/proto and src/motion")

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

forbidden_exists("entities/player/Player.h" SELF_TEST_FOUND)
forbidden_exists("Object/NoSuchHeader.h" SELF_TEST_MISSING)
forbidden_exists("ntities/player/Player.h" SELF_TEST_PARTIAL)
if(NOT SELF_TEST_FOUND OR SELF_TEST_MISSING OR SELF_TEST_PARTIAL)
    message(FATAL_ERROR "Header reach: the forbidden-entry existence check is broken "
        "(entities/player/Player.h ${SELF_TEST_FOUND}, Object/NoSuchHeader.h ${SELF_TEST_MISSING}, ntities/player/Player.h ${SELF_TEST_PARTIAL})")
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
        "A god header reaches a layer it must only forward-declare:\n  ${REPORT}\n"
        "Forward-declare the type and move the include to the .cpp that needs it.")
endif()
