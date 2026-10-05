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

# Each entry: "<header relative to src/game>|<forbidden suffix>,<forbidden suffix>,..."
# Commas, not semicolons: a semicolon inside a quoted argument splits the CMake list.
# A forbidden suffix matches any reached path that ends with "/<suffix>".
# Decoupling D4j: the character's files live under entities/player/, grouped by concern, so the
# rules name Player.h, PlayerRegistry.h and the two managers there.
set(REACH_RULES
    "entities/player/Player.h|Object/GMTicketMgr.h,Object/Bag.h,Server/DBCStores.h,WorldHandlers/NPCHandler.h,WorldHandlers/Chat.h,Server/WorldSession.h,BattleGround/BattleGround.h,WorldHandlers/Group.h,Object/Pet.h,WorldHandlers/Map.h,WorldHandlers/AchievementMgr.h,Object/CinematicFlyover.h,WorldHandlers/ScriptMgr.h,Database/DatabaseEnv.h"
    "Object/Unit.h|MotionGenerators/MotionMaster.h,motion/State.h,entities/player/Player.h,Server/WorldSession.h,proto/WorldPacket.h,WorldHandlers/Path.h"
    # Decoupling D5b (server #132): the combat leaves are arithmetic over values. A
    # combat header that reaches the object layer has stopped being one, and the
    # golden vectors in src/tests/CombatGoldenVectors.h could no longer be checked by
    # a test that links nothing.
    "combat/ArmorReduction.h|Object/Unit.h,Object/Object.h,entities/player/Player.h,Object/Creature.h,WorldHandlers/SpellAuras.h,Server/WorldSession.h,Server/DBCStores.h,Server/DBCStructure.h"
    "combat/MeleeChances.h|Object/Unit.h,Object/Object.h,entities/player/Player.h,Object/Creature.h,WorldHandlers/SpellAuras.h,Server/WorldSession.h,Server/DBCStores.h,Server/DBCStructure.h"
    "combat/SpellBonus.h|Object/Unit.h,Object/Object.h,entities/player/Player.h,Object/Creature.h,WorldHandlers/SpellAuras.h,Server/WorldSession.h,Server/DBCStores.h,Server/DBCStructure.h"
    "combat/WeaponDamage.h|Object/Unit.h,Object/Object.h,entities/player/Player.h,Object/Creature.h,WorldHandlers/SpellAuras.h,Server/WorldSession.h,Server/DBCStores.h,Server/DBCStructure.h"
    # Decoupling D5d (server #136): the aura container is Unit's storage, and Unit.h includes
    # it. It stores Aura* and SpellAuraHolder* without ever dereferencing one, so it must
    # forward-declare both; the moment it reaches SpellAuras.h or the object layer the
    # sentinel-pointer test in src/tests/AuraContainerTest.cpp stops being possible, and
    # Unit.h's own include graph has grown a cycle.
    "spells/AuraContainer.h|Object/Unit.h,Object/Object.h,WorldHandlers/SpellAuras.h,Server/DBCStructure.h,Server/WorldSession.h"
    # Decoupling D4a: a character manager is state plus parameters. QuestStatusMgr.h reaching
    # the character, the unit, the session or the object manager would put the templates and
    # the owner back in reach, and src/tests/QuestStatusMgrTest.cpp builds it from nothing.
    "entities/player/quests/QuestStatusMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h"
    # The .cpp as well: CheckManagerIsolation.cmake is a text gate, and the one thing it cannot
    # see -- the name spelled around its patterns -- only compiles against the complete type,
    # which the .cpp could otherwise include without the header noticing.
    # Decoupling D4b: and no global the manager could reach for instead of a parameter -- the
    # world (sWorld: game time, config) and the object registries that replaced the old
    # ObjectAccessor.h (sPlayerRegistry, ObjectLookup, sCorpseManager).
    # Decoupling D4c: and the map manager (sMapMgr), which no manager needs either.
    "entities/player/quests/QuestStatusMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h"
    # Q-2: the quest-complete packet builder takes plain values, so src/tests/QuestCompletePacketTest.cpp
    # pins its bytes with no character and no quest template. The same two rules as the managers,
    # and it reaches no quest template (WorldHandlers/QuestDef.h), no DBC store and no database either.
    "entities/player/quests/QuestCompletePacket.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/QuestDef.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    "entities/player/quests/QuestCompletePacket.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/QuestDef.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    # Decoupling D4f: the quest reward's arithmetic and template reads, as pure functions over plain
    # values, so src/tests/QuestRewardTest.cpp pins each rule over a table. The world's rates are
    # parameters the owner reads, so neither file may reach the world (WorldHandlers/World.h), and
    # neither touches a DBC store or the database. The header forward-declares the quest template
    # (WorldHandlers/QuestDef.h stays out of its reach, closure 2); the .cpp reads the template's
    # reward-choice slots and so includes it.
    "entities/player/quests/QuestRewardRules.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/QuestDef.h,WorldHandlers/World.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    "entities/player/quests/QuestRewardRules.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    # Decoupling D4c: the talent manager, the same two rules. The header also stays inside
    # entities/player/Player.h's own rule above (Player.h includes it), so it may reach
    # neither the DBC stores nor the database. Its .cpp reads the talent DBC stores
    # (Server/DBCStores.h) and nothing else global: the game time, the talent rate and the
    # quest-reward bonus are parameters, and src/tests/TalentMgrTest.cpp seeds the stores.
    "entities/player/talents/TalentMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    "entities/player/talents/TalentMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h"
    # Decoupling D4e1: the inventory manager, the same two rules. The header stays inside
    # entities/player/Player.h's rule (Player.h includes it), so it may reach neither the DBC
    # stores, the database nor Bag.h. Its .cpp reads items and bags (Object/Bag.h, Object/Item.h)
    # and nothing global at all: no DBC store and no database either, which the talent manager's
    # .cpp needs and this one does not.
    # Decoupling D4e2: InventoryResult, the storage checks' verdict (an unscoped enum, which cannot
    # be forward-declared), moved from Object/Item.h to Object/ItemPrototype.h, so the header
    # includes ItemPrototype.h and still forward-declares Item (closure 7, not Item.h's 36). The
    # .cpp includes Server/DBCStructure.h for the limit-category row's fields, but the row comes
    # from a lookup the owner passes in: the store itself (Server/DBCStores.h) stays out of reach.
    # Decoupling D4e3: the header may not reach Object/Item.h either, so its closure (7) cannot
    # silently go back to the 37 that an include of Item.h gave it.
    "entities/player/inventory/InventoryMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,Object/Bag.h,Object/Item.h"
    "entities/player/inventory/InventoryMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,Server/DBCStores.h,Database/DatabaseEnv.h"
    # Decoupling D4k: the rune manager, the same two rules, plus the spell layer. A slot's convert
    # aura is an opaque identity -- stored, compared, handed back, never dereferenced -- so neither
    # file may reach WorldHandlers/SpellAuras.h (the Aura class) or Object/SpellMgr.h (what a spell
    # is); the facts about the aura are read by the owner. The header forward-declares WorldPacket
    # for its PacketSink and may not reach proto/WorldPacket.h (closure 3); the .cpp builds the
    # three rune packets, so it includes WorldPacket.h and Opcodes.h, which reach none of these.
    "entities/player/spells/RuneMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h,proto/WorldPacket.h"
    "entities/player/spells/RuneMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,Server/DBCStores.h,Database/DatabaseEnv.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h"
    # Decoupling D4k: the managers' shared packet sink type. Every manager header and the owner
    # include it, so it stays a bare std::function over a forward-declared WorldPacket.
    "entities/player/ManagerPacketSink.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,proto/WorldPacket.h"
    "entities/player/PlayerClientFacts.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,proto/WorldPacket.h"
    "spells/SpellCooldownMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h,proto/WorldPacket.h,WorldHandlers/Spell.h"
    "spells/SpellCooldownMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h,WorldHandlers/Spell.h,proto/WorldPacket.h,proto/Opcodes.h"
    # Decoupling D4k: the glyph manager, the rune manager's two rules plus the cast
    # (WorldHandlers/Spell.h): the glyph's spell is cast and its auras removed by callbacks the owner
    # builds, so the manager never sees a spell, an aura or a unit. It builds no packet, so neither
    # file may reach proto/WorldPacket.h. The header stays inside entities/player/Player.h's rule
    # (Player.h includes it) and forward-declares Field. The .cpp, like the talent manager's, reads
    # the glyph slot and glyph property DBC stores (Server/DBCStores.h) and loads and saves through
    # the character database (Database/DatabaseEnv.h); src/tests/GlyphMgrTest.cpp seeds the stores.
    # As for the cooldown manager, the WorldHandlers/Spell.h entries are belt-and-braces (Spell.h
    # includes Object/Unit.h and entities/player/Player.h itself, so those entries fire first);
    # Object/SpellMgr.h reaches neither, so its entry does real work.
    "entities/player/talents/GlyphMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h,proto/WorldPacket.h,WorldHandlers/Spell.h"
    "entities/player/talents/GlyphMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,WorldHandlers/SpellAuras.h,Object/SpellMgr.h,WorldHandlers/Spell.h,proto/WorldPacket.h"
    # Decoupling D4k: the pet manager, the rune manager's two rules plus the pet (Object/Pet.h): the
    # live pet is found, read, unsummoned and loaded by the owner (its facts come in as values, the
    # unsummon and the load as callbacks), so neither file may reach the pet creature. The header
    # includes ManagerPacketSink.h (WorldPacket forward-declared) and the pet cache
    # (pets/PlayerPetCache.h, which reaches only SharedDefines.h), and stays inside
    # entities/player/Player.h's rule (Player.h includes it). The .cpp builds the pet spells packet,
    # so it includes WorldPacket.h, Opcodes.h and ObjectGuid.h, which reach none of these; it
    # touches no DBC store and no database, so it may reach neither.
    "entities/player/pets/PetMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,Object/Pet.h,proto/WorldPacket.h"
    "entities/player/pets/PetMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,Server/DBCStores.h,Database/DatabaseEnv.h,Object/Pet.h"
    # Decoupling D4k: the friend and ignore list (PlayerSocial), the online-visibility verdict and
    # the two social packet builders, the rune manager's two rules plus the global that drives them
    # (entities/player/social/SocialMgr.h): the list never finds another character, so it reaches
    # neither the registry, the object manager nor the global over every list; what it needs of the
    # other characters comes in through a fill callback. The header forward-declares WorldPacket
    # and Field, may not reach proto/WorldPacket.h or the database (closure: Define.h,
    # ServerDefines.h, SharedDefines.h, ObjectGuid.h, ManagerPacketSink.h and what they include).
    # The .cpp builds the two packets (WorldPacket.h, Opcodes.h) and writes the rows through the
    # character database (Database/DatabaseEnv.h); src/tests/SocialMgrTest.cpp checks both.
    "entities/player/social/SocialList.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,proto/WorldPacket.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,entities/player/social/SocialMgr.h"
    "entities/player/social/SocialList.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,Server/DBCStores.h,entities/player/social/SocialMgr.h"
    # Decoupling D4k: the currency manager, the rune manager's two rules plus the achievement
    # manager (WorldHandlers/AchievementMgr.h): a gain's achievement update, the owner's two
    # currency quest checks and the packets reach the owner through callbacks, the currency-gain
    # aura multiplier and the in-world-and-not-loading check are read callbacks, and the conquest
    # week cap comes in as a value, so neither file may reach the world (WorldHandlers/World.h: the
    # config), the auras or the achievements. The header includes ManagerPacketSink.h (WorldPacket
    # forward-declared) and DBCEnums.h (the achievement criteria type, enums only), forward-declares
    # Field, ObjectGuid and CurrencyTypesEntry, and stays inside entities/player/Player.h's rule
    # (Player.h includes it). The .cpp, like the talent manager's, reads the currency types DBC
    # store (Server/DBCStores.h) and loads and saves through the character database
    # (Database/DatabaseEnv.h); src/tests/CurrencyMgrTest.cpp seeds the store.
    "entities/player/inventory/CurrencyMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,Server/DBCStores.h,Database/DatabaseEnv.h,WorldHandlers/SpellAuras.h,WorldHandlers/AchievementMgr.h,proto/WorldPacket.h"
    "entities/player/inventory/CurrencyMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,WorldHandlers/SpellAuras.h,WorldHandlers/AchievementMgr.h"
    # Decoupling D4k: the reputation manager, the rune manager's two rules plus the world
    # (WorldHandlers/World.h) and the achievements (WorldHandlers/AchievementMgr.h): a changed
    # faction's quest check and achievement updates are callbacks the owner builds, and the
    # spillover template (the object manager's) and a faction's team list come in through read
    # callbacks, so neither file reaches the object manager, the achievements or the session. The
    # header defines RepSpilloverTemplate (ObjectMgr.h includes it), forward-declares Field, and
    # stays inside entities/player/Player.h's rule (Player.h includes it); it may reach neither
    # the DBC stores, the database nor proto/WorldPacket.h. The .cpp builds the five packets
    # (WorldPacket.h, Opcodes.h), reads the faction store (Server/DBCStores.h) and saves through
    # the character database (Database/DatabaseEnv.h); src/tests/ReputationMgrTest.cpp seeds the
    # store and checks both.
    "entities/player/social/ReputationMgr.h|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,WorldHandlers/AchievementMgr.h,Server/DBCStores.h,Database/DatabaseEnv.h,proto/WorldPacket.h"
    "entities/player/social/ReputationMgr.cpp|entities/player/Player.h,Object/Unit.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,WorldHandlers/AchievementMgr.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h"
    # Decoupling D4k: the honor manager. The owner's and the victim's facts, the realm's type and
    # honor rate come in as values, the clock and the honor draw as read callbacks, and the kill
    # fields, the achievement updates, the packet and the honor currency change go out through
    # callbacks, so neither file may reach the character or the unit (nor the creature, whose
    # racial-leader flag the owner reads), the session, the world (WorldHandlers/World.h: the config
    # and the realm type), the auras, the achievements, the currency manager, the battlegrounds (the
    # inactive spell id) or the grey-level formula (Object/Formulas.h, which reaches the world). The
    # header includes SharedDefines.h (Team), DBCEnums.h (the criteria type), ObjectGuid.h (the
    # victim's guid, by value) and ManagerPacketSink.h (WorldPacket forward-declared), and stays
    # inside entities/player/Player.h's rule (Player.h includes it). The .cpp builds the packet
    # (WorldPacket.h, Opcodes.h) and touches no DBC store and no database.
    "entities/player/pvp/HonorMgr.h|entities/player/Player.h,Object/Unit.h,Object/Creature.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,WorldHandlers/SpellAuras.h,WorldHandlers/AchievementMgr.h,entities/player/inventory/CurrencyMgr.h,BattleGround/BattleGround.h,Object/Formulas.h,Server/DBCStores.h,Database/DatabaseEnv.h,proto/WorldPacket.h"
    "entities/player/pvp/HonorMgr.cpp|entities/player/Player.h,Object/Unit.h,Object/Creature.h,Server/WorldSession.h,ObjectMgr.h,WorldHandlers/World.h,entities/player/PlayerRegistry.h,Object/ObjectLookup.h,Object/CorpseManager.h,WorldHandlers/MapManager.h,WorldHandlers/SpellAuras.h,WorldHandlers/AchievementMgr.h,entities/player/inventory/CurrencyMgr.h,BattleGround/BattleGround.h,Object/Formulas.h,Server/DBCStores.h,Database/DatabaseEnv.h")
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
        "A god header reaches a layer it must only forward-declare (decoupling D3, server #77):\n  ${REPORT}\n"
        "Forward-declare the type and move the include to the .cpp that needs it.")
endif()
