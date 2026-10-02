# Decoupling D4h: a character manager's packets and tables stay in the manager. ReputationMgr
# (src/game/entities/player/social/ReputationMgr.*) and AchievementMgr
# (src/game/WorldHandlers/AchievementMgr.*) own their state privately -- no friend, and no public
# accessor that hands it out mutable -- and today they alone build their packets and edit a live
# character's rows (design/2026-09-26-decoupling-d4-facts.md section 5, "Bypass audit": no
# bypass). Nothing in the compiler stops the next handler from sending SMSG_SET_FACTION_STANDING
# itself or writing `character_reputation` behind the manager's back. This gate does.
#
# It is a table. Each ROW is one owner concern:
#
#   state_row(<row> OWNER <class> PACKETS <opcode>... TABLES <table>... TYPES <type>...)
#       PACKETS: the server packets the owner builds (measured from src/proto/Opcodes.h);
#       TABLES:  the character tables that hold its rows;
#       TYPES:   the owner class and every type its public const accessors hand out by pointer
#                or reference -- what a const_cast would have to name to reach the state.
#   state_allow(<row> <file> WHY "<reason>" NAMES <name>...)
#       one file allowed to spell some of the row's names: the reason in one line, and exactly
#       the names it spells today. The owner's own files are allowances like any other; the
#       rest are whole-character or realm-level tools (a delete, the login holder, the character
#       dump, a start-up cleanup) and tests or harness code that observe a packet (classify it
#       by opcode, build nothing).
#   state_definition_site(<file> WHY "<reason>")
#       a file that DEFINES opcodes rather than building packets. It may spell any row's
#       PACKETS, never its TABLES.
#
# Every C/C++ source under src/ is read -- the extensions in SCAN_EXTENSIONS, src/modules/SD3
# included, the third-party trees in SCAN_EXCLUDED left out -- and each of these fails the gate
# with the file and the line:
#   - a row's name spelled in a file that has no allowance for that name. The match is a whole
#     word in the raw text: comments are NOT exempt (a comment naming the packet in a handler is
#     exactly where a bypass starts) and neither are string literals (a table name only ever
#     appears inside an SQL literal);
#   - a const_cast whose target type names one of a row's TYPES, anywhere, the owner included;
# and, so that the lists only ever shrink (the ratchet), each of these fails as well:
#   - an allowance whose file no longer spells one of its names ("allowance no longer needed:
#     remove it"), and a definition site that spells no row packet at all;
#   - an allowed file or definition site that does not exist, or that the scan does not read;
#   - a row name that no scanned file spells: it guards nothing (a typo, or a packet or table
#     that is gone -- remove it from the row).
# The table functions check the table itself before anything is scanned: a name on one row
# only, a packet shaped like an opcode and a table like a table, every allowed name on its
# row, a file allowed once per row, a reason on every entry.
#
# ADDING A ROW (the D4k managers: currency, honor, glyphs, cooldowns, runes, pets, social):
#   1. state_row(): PACKETS are the SMSG_ names in src/proto/Opcodes.h that the manager builds
#      (grep them; a family member someone else builds by design is left out, or gets a row of
#      its own under its real owner); TABLES are the character tables the manager loads and
#      saves; TYPES are the manager class plus what its public const accessors return by
#      pointer or reference.
#   2. One state_allow() per file that spells a name today, the manager's own files first,
#      each with its reason (whole-character delete, login holder SELECT, character dump,
#      realm-wide cleanup, a test or harness that observes the packet, ...) and exactly the
#      names it spells. Run the gate: every spelling not yet allowed is printed as file:line, so
#      the list is complete when it passes.
#   3. The definition sites are shared by every row and need no change.
#   Then say in the PR which spellings stay and why, and which moved into the manager.
#
# KNOWN MISSES, stated rather than chased. The rules are text, so a name is seen only where it
# is spelled whole:
#   - a name assembled by the preprocessor -- token pasting (`SMSG_SET_ ## FACTION_STANDING`),
#     a macro defined elsewhere that expands to it, or the name split by a backslash-newline --
#     or by string concatenation ("character_" "reputation", std::string("character_") + x, a
#     `%s` filled with a fragment). Searched for D4h: no string literal under src/ ends in
#     `character_`, none puts a `%s` beside a table fragment, and the only literals spelling a
#     bare `reputation` or `achievement` are chat command names; the rows' tables are always
#     spelled whole.
#   - a numeric opcode value (`WorldPacket data(0x0126, 17)`). Today the rows' values appear only
#     in the definition sites and in one ReputationMgr.cpp comment (0x4216); CharacterCache.cpp's
#     0x0126 is an unrelated table value, not an opcode.
#   - a packet built by a generic helper that takes the opcode as a runtime value. Found:
#     `.debug send opcode` (ChatHandler::HandleDebugSendOpcodeCommand, DebugCommands.cpp) reads a
#     number from opcode.txt and sends a packet under it, so a GM can put any row packet on the
#     wire without its name; `.debug recv` does the same with a client packet. The rest
#     -- WorldPacket(uint16) and Initialize(uint16) themselves, the proto and loadtest framing
#     (PacketCodec.cpp, Framing.cpp), motion's EncodeRegistry, Player::AddSpellMod and
#     `.debug spellmods` (a choice between two spelled spell-mod opcodes) -- are reached by no
#     row name. Relays (SendDirectMessage, the broadcasts, the stream plumbing) forward a packet
#     the owner already built; they are not builders.
#   - a table reached through a helper that takes the table name at runtime. Three exist on the
#     character database. CharacterDatabaseCleaner::CheckUnique (the call in
#     CleanCharacterAchievementProgress) and PlayerDump's table walker (its dumpTables list) are
#     handed row tables, spelled where they are handed over, which is where the gate sees them;
#     MapPersistentStateManager::_DelHelper (the instance cleanup) is handed only instance tables.
#   - a C-style cast `(FactionState*)state` strips const as surely as const_cast does, and is not
#     matched: an unnamed parameter `(FactionState*)` in a declaration reads the same. Nor is a
#     const_cast whose target type itself holds parentheses (a function-pointer type), since the
#     target is read up to the cast's '>(' and never across a parenthesis. There are no casts of
#     any kind to a row type today.
#   - work done through the owner's public methods (SetReputation, UpdateAchievementCriteria,
#     ...) is what the owner is for, not a bypass; who calls them is not this gate's question.
# Run standalone (-P), this script sees none of the top-level project's policies. The project
# requires CMake >= 3.18, so that is also the floor here.
cmake_minimum_required(VERSION 3.18)

include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(StateOwnership)

# ---------------------------------------------------------------------------------------------
# The table's three entry points. They append to ROWS / ALLOWANCES / DEFINITION_SITES in the
# caller's scope, and check each entry as it is made.

set(WORD_CHARS "A-Za-z0-9_")

# The key every per-file flag is stored under: the path in hex. It has to be injective --
# string(MAKE_C_IDENTIFIER) maps '/', '.' and '_' alike to '_', so src/game/Tools_PlayerDump.h
# would inherit src/game/Tools/PlayerDump.h's allowance and quiet its ratchet -- and it has no
# '_', so `<id>_<name>` cannot collide either. string(HEX) is CMake 3.18, the floor.
function(file_id FILE_REL OUT_VAR)
    string(HEX "${FILE_REL}" ID)
    set(${OUT_VAR} "${ID}" PARENT_SCOPE)
endfunction()

function(state_row ROW)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "OWNER" "PACKETS;TABLES;TYPES")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "StateOwnership: state_row(${ROW}): unexpected arguments '${ARG_UNPARSED_ARGUMENTS}'")
    endif()
    if(NOT ROW MATCHES "^[a-z][a-z0-9_]*$")
        message(FATAL_ERROR "StateOwnership: state_row(${ROW}): a row id is lower-case letters, digits and '_'")
    endif()
    if(ROW IN_LIST ROWS)
        message(FATAL_ERROR "StateOwnership: state_row(${ROW}): the row is declared twice")
    endif()
    if(NOT ARG_OWNER)
        message(FATAL_ERROR "StateOwnership: state_row(${ROW}): OWNER <class> is required")
    endif()
    if(NOT ARG_PACKETS AND NOT ARG_TABLES)
        message(FATAL_ERROR "StateOwnership: state_row(${ROW}): a row owns PACKETS, TABLES or both")
    endif()
    if(NOT ARG_OWNER IN_LIST ARG_TYPES)
        message(FATAL_ERROR "StateOwnership: state_row(${ROW}): the owner class ${ARG_OWNER} belongs in TYPES")
    endif()
    foreach(NAME IN LISTS ARG_PACKETS)
        if(NOT NAME MATCHES "^(SMSG|CMSG|MSG)_[A-Z0-9_]+$")
            message(FATAL_ERROR "StateOwnership: state_row(${ROW}): '${NAME}' in PACKETS is not an opcode name")
        endif()
    endforeach()
    foreach(NAME IN LISTS ARG_TABLES)
        if(NOT NAME MATCHES "^[a-z][a-z0-9_]*$")
            message(FATAL_ERROR "StateOwnership: state_row(${ROW}): '${NAME}' in TABLES is not a table name")
        endif()
    endforeach()
    foreach(NAME IN LISTS ARG_PACKETS ARG_TABLES)
        if(DEFINED NAME_ROW_${NAME})
            message(FATAL_ERROR "StateOwnership: state_row(${ROW}): '${NAME}' is already on row '${NAME_ROW_${NAME}}' (or twice on this one)")
        endif()
        set(NAME_ROW_${NAME} "${ROW}")
        set(NAME_ROW_${NAME} "${ROW}" PARENT_SCOPE)
    endforeach()
    foreach(TYPE IN LISTS ARG_TYPES)
        if(NOT TYPE MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
            message(FATAL_ERROR "StateOwnership: state_row(${ROW}): '${TYPE}' in TYPES is not a type name")
        endif()
        if(DEFINED TYPE_ROW_${TYPE})
            message(FATAL_ERROR "StateOwnership: state_row(${ROW}): type '${TYPE}' is already on row '${TYPE_ROW_${TYPE}}' (or twice on this one)")
        endif()
        set(TYPE_ROW_${TYPE} "${ROW}")
        set(TYPE_ROW_${TYPE} "${ROW}" PARENT_SCOPE)
    endforeach()
    set(ROWS ${ROWS} ${ROW} PARENT_SCOPE)
    set(ALL_PACKETS ${ALL_PACKETS} ${ARG_PACKETS} PARENT_SCOPE)
    set(ALL_NAMES ${ALL_NAMES} ${ARG_PACKETS} ${ARG_TABLES} PARENT_SCOPE)
    set(ALL_TYPES ${ALL_TYPES} ${ARG_TYPES} PARENT_SCOPE)
    set(ROW_${ROW}_OWNER "${ARG_OWNER}" PARENT_SCOPE)
    set(ROW_${ROW}_FILES "" PARENT_SCOPE)
endfunction()

function(state_allow ROW FILE_REL)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "" "WHY" "NAMES")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "StateOwnership: state_allow(${ROW} ${FILE_REL}): unexpected arguments '${ARG_UNPARSED_ARGUMENTS}'")
    endif()
    if(NOT ROW IN_LIST ROWS)
        message(FATAL_ERROR "StateOwnership: state_allow(${ROW} ${FILE_REL}): no row '${ROW}' (declare it with state_row first)")
    endif()
    if(NOT FILE_REL MATCHES "^src/[^\\\\]+$")
        message(FATAL_ERROR "StateOwnership: state_allow(${ROW} ${FILE_REL}): the file is a path under src/, relative to the repo root, with '/'")
    endif()
    if(FILE_REL IN_LIST ROW_${ROW}_FILES)
        message(FATAL_ERROR "StateOwnership: state_allow(${ROW} ${FILE_REL}): the file is already allowed on this row (one entry per file)")
    endif()
    if("${ARG_WHY}" STREQUAL "")
        message(FATAL_ERROR "StateOwnership: state_allow(${ROW} ${FILE_REL}): WHY \"<reason>\" is required")
    endif()
    if(NOT ARG_NAMES)
        message(FATAL_ERROR "StateOwnership: state_allow(${ROW} ${FILE_REL}): NAMES <name>... is required")
    endif()
    file_id("${FILE_REL}" FILE_ID)
    foreach(NAME IN LISTS ARG_NAMES)
        if(NOT "${NAME_ROW_${NAME}}" STREQUAL "${ROW}")
            message(FATAL_ERROR "StateOwnership: state_allow(${ROW} ${FILE_REL}): '${NAME}' is not a name of row '${ROW}'")
        endif()
        if(ALLOWED_${FILE_ID}_${NAME})
            message(FATAL_ERROR "StateOwnership: state_allow(${ROW} ${FILE_REL}): '${NAME}' is listed twice")
        endif()
        set(ALLOWED_${FILE_ID}_${NAME} ON)
        set(ALLOWED_${FILE_ID}_${NAME} ON PARENT_SCOPE)
    endforeach()
    list(LENGTH ALLOWANCES INDEX)
    set(ALLOWANCES ${ALLOWANCES} ${INDEX} PARENT_SCOPE)
    set(ALLOW_${INDEX}_ROW "${ROW}" PARENT_SCOPE)
    set(ALLOW_${INDEX}_FILE "${FILE_REL}" PARENT_SCOPE)
    set(ALLOW_${INDEX}_WHY "${ARG_WHY}" PARENT_SCOPE)
    set(ALLOW_${INDEX}_NAMES ${ARG_NAMES} PARENT_SCOPE)
    set(ROW_${ROW}_FILES ${ROW_${ROW}_FILES} "${FILE_REL}" PARENT_SCOPE)
endfunction()

function(state_definition_site FILE_REL)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "WHY" "")
    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "StateOwnership: state_definition_site(${FILE_REL}): unexpected arguments '${ARG_UNPARSED_ARGUMENTS}'")
    endif()
    if(NOT FILE_REL MATCHES "^src/[^\\\\]+$")
        message(FATAL_ERROR "StateOwnership: state_definition_site(${FILE_REL}): the file is a path under src/, relative to the repo root, with '/'")
    endif()
    if(FILE_REL IN_LIST DEFINITION_SITES)
        message(FATAL_ERROR "StateOwnership: state_definition_site(${FILE_REL}): the file is listed twice")
    endif()
    if("${ARG_WHY}" STREQUAL "")
        message(FATAL_ERROR "StateOwnership: state_definition_site(${FILE_REL}): WHY \"<reason>\" is required")
    endif()
    file_id("${FILE_REL}" FILE_ID)
    set(DEFINITION_SITES ${DEFINITION_SITES} "${FILE_REL}" PARENT_SCOPE)
    set(DEFINITION_${FILE_ID} ON PARENT_SCOPE)
    set(DEFINITION_${FILE_ID}_WHY "${ARG_WHY}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------------------------
# THE TABLE. Measured on master 071325aad (decoupling D4h); the rune row on 4ebfddc10, the
# cooldown row on a63c61953, the glyph row on cc63db97b, the pet row on 157d99a5c, the social row on
# b20d94a87, the currency row on 6f20649ea and the honor row on 9fb598e4e (decoupling D4k); the
# reputation row's allowances re-measured on ad8931f9c when ReputationMgr became Player-less
# (decoupling D4k). The reasons name functions and lists, never line numbers or counts: those go
# stale with the next edit, and the gate prints the lines.

# Every SMSG_ name in Opcodes.h containing FACTION, REPUTATION or FORCED_REACTIONS, less the two
# that are not reputation state: SMSG_CHAT_WRONG_FACTION (the chat code's "wrong faction" error,
# WorldSession::SendWrongFactionNotice) and SMSG_CHAR_FACTION_CHANGE (the faction-change reply
# at the character screen, built by nobody). SMSG_SET_FACTION_NOT_VISIBLE is built by nobody
# either, and is kept: when it is built, ReputationMgr builds it. The client also knows the guild
# reputation packets (SMSG_GUILD_REPUTATION_*, in OpcodeSlots.inc), which Opcodes.h does not
# define; if one is ever defined, the guild code owns it -- leave it off this row.
state_row(reputation
    OWNER   ReputationMgr
    PACKETS SMSG_INITIALIZE_FACTIONS        # ReputationMgr::SendInitialReputations
            SMSG_SET_FACTION_VISIBLE        # ReputationMgr::SendVisible
            SMSG_SET_FACTION_STANDING       # ReputationMgr::SendState
            SMSG_SET_FORCED_REACTIONS       # ReputationMgr::SendForceReactions
            SMSG_SET_FACTION_ATWAR          # ReputationMgr::SendAtWar
            SMSG_SET_FACTION_NOT_VISIBLE    # built by nobody today
    TABLES  character_reputation
    TYPES   ReputationMgr
            FactionState                    # GetState()
            FactionStateList                # GetStateList()
            ReputationRank)                 # GetForcedRankIfAny()

state_allow(reputation src/game/entities/player/social/ReputationMgr.cpp
    WHY "the owner: builds its packets and saves the rows (the login rows come in one at a time, LoadRow)"
    NAMES SMSG_INITIALIZE_FACTIONS SMSG_SET_FACTION_VISIBLE SMSG_SET_FACTION_STANDING
          SMSG_SET_FORCED_REACTIONS SMSG_SET_FACTION_ATWAR character_reputation)
state_allow(reputation src/game/entities/player/Player.cpp
    WHY "whole-character delete (Player::DeleteFromDB)"
    NAMES character_reputation)
state_allow(reputation src/game/WorldHandlers/CharacterHandler.cpp
    WHY "login holder SELECT (PLAYER_LOGIN_QUERY_LOADREPUTATION), handed to Player::_LoadReputations"
    NAMES character_reputation)
state_allow(reputation src/game/Tools/PlayerDump.cpp
    WHY "character dump: the dumped-table list and its doc comment"
    NAMES character_reputation)
state_allow(reputation src/game/Tools/PlayerDump.h
    WHY "character dump: the table-type doc comment"
    NAMES character_reputation)
state_allow(reputation src/game/Harness/Trace.cpp
    WHY "a test or harness that observes the packet (classifies it by opcode, builds nothing)"
    NAMES SMSG_SET_FACTION_STANDING SMSG_SET_FACTION_VISIBLE)
state_allow(reputation src/game/Harness/ScenariosQuest.cpp
    WHY "a test or harness that observes the packet (classifies it by opcode, builds nothing)"
    NAMES SMSG_SET_FACTION_STANDING)
state_allow(reputation src/tests/ReputationMgrTest.cpp
    WHY "the manager's own test: checks the opcodes it builds and the statements it saves (builds nothing)"
    NAMES SMSG_INITIALIZE_FACTIONS SMSG_SET_FACTION_VISIBLE SMSG_SET_FACTION_STANDING
          SMSG_SET_FORCED_REACTIONS SMSG_SET_FACTION_ATWAR character_reputation)
state_allow(reputation src/tests/HarnessTest.cpp
    WHY "a test or harness that observes the packet (classifies it by opcode, builds nothing)"
    NAMES SMSG_SET_FACTION_STANDING SMSG_SET_FACTION_VISIBLE)

# Every SMSG_ name in Opcodes.h containing ACHIEVEMENT or CRITERIA; the character's
# AchievementMgr builds all of them, so nothing is left out. The client also knows the guild
# family (SMSG_GUILD_ACHIEVEMENT_*, SMSG_GUILD_CRITERIA_*, SMSG_ALL_GUILD_ACHIEVEMENTS, in
# OpcodeSlots.inc), which Opcodes.h does not define; if one is ever defined, the guild code owns
# it -- leave it off this row, or give it a row of its own under the guild's owner.
state_row(achievement
    OWNER   AchievementMgr
    PACKETS SMSG_ACHIEVEMENT_EARNED              # AchievementMgr::SendAchievementEarned
            SMSG_CRITERIA_UPDATE                 # AchievementMgr::SendCriteriaUpdate
            SMSG_RESPOND_INSPECT_ACHIEVEMENTS    # AchievementMgr::SendRespondInspectAchievements
            SMSG_ALL_ACHIEVEMENT_DATA            # AchievementMgr::SendAllAchievementData
            SMSG_SERVER_FIRST_ACHIEVEMENT        # AchievementMgr::SendAchievementEarned
            SMSG_CRITERIA_DELETED                # AchievementMgr::Reset, ::SetCriteriaProgress
            SMSG_ACHIEVEMENT_DELETED             # AchievementMgr::Reset, ::IncompletedAchievement
    TABLES  character_achievement
            character_achievement_progress
    TYPES   AchievementMgr
            CompletedAchievementData             # GetCompleteData()
            CompletedAchievementMap)             # GetCompletedAchievements()

state_allow(achievement src/game/WorldHandlers/AchievementMgr.cpp
    WHY "the owner: builds its packets, loads, saves and deletes the rows"
    NAMES SMSG_ACHIEVEMENT_EARNED SMSG_CRITERIA_UPDATE SMSG_RESPOND_INSPECT_ACHIEVEMENTS
          SMSG_ALL_ACHIEVEMENT_DATA SMSG_SERVER_FIRST_ACHIEVEMENT SMSG_CRITERIA_DELETED
          SMSG_ACHIEVEMENT_DELETED character_achievement character_achievement_progress)
state_allow(achievement src/game/WorldHandlers/AchievementMgr.h
    WHY "realm-wide cleanup: AchievementGlobalMgr::CleanupOrphanedCriteriaProgress's doc comment, in the owner's header"
    NAMES character_achievement_progress)
state_allow(achievement src/game/entities/player/Player.cpp
    WHY "whole-character delete (Player::DeleteFromDB)"
    NAMES character_achievement character_achievement_progress)
state_allow(achievement src/game/WorldHandlers/CharacterHandler.cpp
    WHY "login holder SELECTs (PLAYER_LOGIN_QUERY_LOADACHIEVEMENTS / LOADCRITERIAPROGRESS), handed to AchievementMgr::LoadFromDB"
    NAMES character_achievement character_achievement_progress)
state_allow(achievement src/game/WorldHandlers/AchievementGlobalMgr.cpp
    WHY "realm-wide start-up cleanup (LoadCompletedAchievements, CleanupOrphanedCriteriaProgress)"
    NAMES character_achievement character_achievement_progress)
state_allow(achievement src/game/Tools/CharacterDatabaseCleaner.cpp
    WHY "realm-wide cleanup (CleanCharacterAchievementProgress's CheckUnique)"
    NAMES character_achievement_progress)
state_allow(achievement src/game/Tools/PlayerDump.cpp
    WHY "character dump: the dumped-table list"
    NAMES character_achievement character_achievement_progress)
state_allow(achievement src/game/Tools/PlayerDump.h
    WHY "character dump: the table-type doc comment"
    NAMES character_achievement character_achievement_progress)
state_allow(achievement src/game/Harness/Trace.h
    WHY "a test or harness that observes the packet (classifies it by opcode, builds nothing)"
    NAMES SMSG_CRITERIA_UPDATE SMSG_ACHIEVEMENT_EARNED)
state_allow(achievement src/game/Harness/Trace.cpp
    WHY "a test or harness that observes the packet (classifies it by opcode, builds nothing)"
    NAMES SMSG_CRITERIA_UPDATE SMSG_ACHIEVEMENT_EARNED)
state_allow(achievement src/tests/HarnessTest.cpp
    WHY "a test or harness that observes the packet (classifies it by opcode, builds nothing)"
    NAMES SMSG_CRITERIA_UPDATE SMSG_ACHIEVEMENT_EARNED)
state_allow(achievement src/game/Harness/Recorder.cpp
    WHY "a test or harness that observes the packet (classifies it by opcode, builds nothing)"
    NAMES SMSG_CRITERIA_UPDATE)

# Decoupling D4k. Every SMSG_ name in Opcodes.h containing RUNE; RuneMgr builds all three. The
# rune state is runtime-only (Init() rebuilds it, nothing saves it), so the row owns no table.
# Left out: CMSG_SET_RUNE_COUNT and CMSG_SET_RUNE_COOLDOWN (client packets, no handler: the
# opcode table lists them commented out), and SMSG_RUNE_REGEN_DEBUG, which only the client knows
# (OpcodeSlots.inc) and Opcodes.h does not define; if it is ever defined, RuneMgr builds it --
# add it here. TYPES is the owner alone: its const accessors hand out values, and the
# `Aura const*` of GetRuneConvertAura() is an opaque identity of an aura the spell code owns, not
# rune state (a const_cast of it is the aura system's business, e.g. the removal the owner does
# in spells/PlayerRune.cpp).
state_row(rune
    OWNER   RuneMgr
    PACKETS SMSG_CONVERT_RUNE                    # RuneMgr::ConvertRune
            SMSG_RESYNC_RUNES                    # RuneMgr::ResyncRunes
            SMSG_ADD_RUNE_POWER                  # RuneMgr::AddRunePower
    TYPES   RuneMgr)

state_allow(rune src/game/entities/player/spells/RuneMgr.cpp
    WHY "the owner: builds its packets and hands them to the owner's session sink"
    NAMES SMSG_CONVERT_RUNE SMSG_RESYNC_RUNES SMSG_ADD_RUNE_POWER)
state_allow(rune src/tests/RuneMgrTest.cpp
    WHY "unit test that observes the packets through a capturing sink (checks each opcode and its bytes, builds nothing)"
    NAMES SMSG_CONVERT_RUNE SMSG_RESYNC_RUNES SMSG_ADD_RUNE_POWER)

state_row(cooldown
    OWNER   SpellCooldownMgr
    PACKETS SMSG_COOLDOWN_EVENT
            SMSG_CLEAR_COOLDOWNS
    TABLES  character_spell_cooldown
    TYPES   SpellCooldownMgr
            SpellCooldowns                       # GetSpellCooldownMap()
            SpellCooldown)                       # its elements

state_allow(cooldown src/game/spells/SpellCooldownMgr.cpp
    WHY "the owner: loads the rows one at a time and saves them; its cooldown event and clear of every cooldown leave as facts"
    NAMES character_spell_cooldown)
state_allow(cooldown src/game/session/packets/spells/CooldownPackets.cpp
    WHY "the session's builders of the owner's two facts: the cooldown event and the clear of every cooldown"
    NAMES SMSG_COOLDOWN_EVENT SMSG_CLEAR_COOLDOWNS)
state_allow(cooldown src/game/entities/player/Player.cpp
    WHY "the one-spell clear for the character or its pet (Player::SendClearCooldown), and the whole-character delete (Player::DeleteFromDB)"
    NAMES SMSG_CLEAR_COOLDOWNS character_spell_cooldown)
state_allow(cooldown src/game/WorldHandlers/CharacterHandler.cpp
    WHY "login holder SELECT (PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS), handed to the owner's per-row load"
    NAMES character_spell_cooldown)
state_allow(cooldown src/game/Tools/PlayerDump.cpp
    WHY "character dump: the dumped-table list"
    NAMES character_spell_cooldown)
state_allow(cooldown src/game/Tools/PlayerDump.h
    WHY "character dump: the table-type doc comment"
    NAMES character_spell_cooldown)
state_allow(cooldown src/tests/SpellCooldownMgrTest.cpp
    WHY "unit test that observes the save's statements through a fake connection (checks each statement, builds nothing)"
    NAMES character_spell_cooldown)
state_allow(cooldown src/tests/CooldownPacketsTest.cpp
    WHY "unit test of the session's cooldown builders and installed callbacks: checks each opcode and byte they produce, builds nothing itself"
    NAMES SMSG_COOLDOWN_EVENT SMSG_CLEAR_COOLDOWNS)
state_allow(cooldown src/game/Harness/Trace.h
    WHY "harness recorder (decoupling D11 PR 2): the decoders' doc comments name the packets they read"
    NAMES SMSG_COOLDOWN_EVENT SMSG_CLEAR_COOLDOWNS)
state_allow(cooldown src/game/Harness/Trace.cpp
    WHY "harness recorder (decoupling D11 PR 2): classifies the packets by opcode and decodes their bytes into a TRACE line, builds nothing"
    NAMES SMSG_COOLDOWN_EVENT SMSG_CLEAR_COOLDOWNS)
state_allow(cooldown src/game/Harness/ScenariosSpell.cpp
    WHY "harness spell scenarios (decoupling D11 PR 2): read the recorded packets by opcode for their categories, build nothing"
    NAMES SMSG_COOLDOWN_EVENT SMSG_CLEAR_COOLDOWNS)
state_allow(cooldown src/tests/HarnessTest.cpp
    WHY "unit test of the harness decoders: hand-builds the packets' bytes as their writers lay them out and checks the decoded record, sends nothing"
    NAMES SMSG_COOLDOWN_EVENT SMSG_CLEAR_COOLDOWNS)

# Decoupling D4k. The glyph state's table. Opcodes.h defines no SMSG_ name containing GLYPH, and
# GlyphMgr builds no packet: the client sees glyphs through the owner's update fields and the
# talent info packet (SMSG_TALENT_UPDATE, the talent code's). Left out: CMSG_SET_GLYPH_SLOT,
# CMSG_SET_GLYPH and CMSG_REMOVE_GLYPH, client packets whose handlers the opcode table lists
# commented out (a handler reads a client packet, it does not build one). TYPES is the owner
# alone: its public const accessor hands out a glyph id by value.
state_row(glyph
    OWNER   GlyphMgr
    TABLES  character_glyphs
    TYPES   GlyphMgr)

state_allow(glyph src/game/entities/player/talents/GlyphMgr.cpp
    WHY "the owner: loads the rows one at a time (the row's columns in a comment, the invalid-row DELETEs) and saves them"
    NAMES character_glyphs)
state_allow(glyph src/game/entities/player/talents/GlyphMgr.h
    WHY "the owner's header: the save states' and the save's doc comments name the table"
    NAMES character_glyphs)
state_allow(glyph src/game/entities/player/Player.cpp
    WHY "whole-character delete (Player::DeleteFromDB)"
    NAMES character_glyphs)
state_allow(glyph src/game/WorldHandlers/CharacterHandler.cpp
    WHY "login holder SELECT (PLAYER_LOGIN_QUERY_LOADGLYPHS), handed to the owner's per-row load"
    NAMES character_glyphs)
state_allow(glyph src/game/Tools/PlayerDump.cpp
    WHY "character dump: the dumped-table list"
    NAMES character_glyphs)
state_allow(glyph src/game/Tools/PlayerDump.h
    WHY "character dump: the table-type doc comment"
    NAMES character_glyphs)
state_allow(glyph src/tests/GlyphMgrTest.cpp
    WHY "unit test that observes the load's DELETEs and the save's statements through a fake connection (checks each statement, runs nothing itself)"
    NAMES character_glyphs)

# Decoupling D4k. The one packet PetMgr builds: SMSG_PET_SPELLS with an empty guid, which clears
# the client's pet action bar (PetMgr::RemoveActionBar). The same opcode's full forms -- the
# spell bars of the pet, of a possessed unit and of a charmed unit -- are the owner's
# (Player::PetSpellInitialize, PossessSpellInitialize and CharmSpellInitialize read the live pet
# or charm), hence the PlayerPet.cpp allowance. Every other SMSG_ name in Opcodes.h containing
# PET is left out, since PetMgr builds none of them: SMSG_PET_GUIDS is the owner's
# (SendPetGUIDs); the name query and name-invalid, cast-failed, tame-failure, mode, action
# feedback and sound, dismiss sound, unlearn-confirm and learned/removed-spell packets are built
# by the pet code, the pet and spell handlers, the unit, the vehicle and the owner's other files;
# SMSG_PETGODMODE, SMSG_PET_BROKEN, SMSG_PET_RENAMEABLE and SMSG_PET_UPDATE_COMBO_POINTS are built
# by nobody; the petition names (SMSG_PETITION_*, SMSG_TURN_IN_PETITION_RESULTS, SMSG_OFFER_PETITION_ERROR)
# are the petition code's and Player.cpp's sign/turn-in results; MSG_LIST_STABLED_PETS and
# SMSG_STABLE_RESULT are the stable handlers'. NO TABLE: the stable-slot count is a column of the
# character row, which the owner's persistence files load and save, and the five pet tables are
# written by the pet code (the pet's save and delete, the stable and pet handlers) and mirrored by
# PlayerPetCache (decoupling D7e), which writes none of them; a row for them belongs to their
# writer, not to this manager. TYPES: the owner, and the pet cache its GetPetCache() const hands
# out by reference.
state_row(pet
    OWNER   PetMgr
    PACKETS SMSG_PET_SPELLS                      # PetMgr::RemoveActionBar
    TYPES   PetMgr
            PlayerPetCache)                      # GetPetCache() const

state_allow(pet src/game/entities/player/pets/PetMgr.cpp
    WHY "the owner: builds the empty-guid pet spells packet for the owner's session sink"
    NAMES SMSG_PET_SPELLS)
state_allow(pet src/game/entities/player/pets/PlayerPet.cpp
    WHY "the owner's full spell bars of its pet, possessed and charmed unit (Player::PetSpellInitialize, PossessSpellInitialize, CharmSpellInitialize)"
    NAMES SMSG_PET_SPELLS)
state_allow(pet src/tests/PetMgrTest.cpp
    WHY "unit test that observes the packet through a capturing sink (checks its opcode and bytes, builds nothing)"
    NAMES SMSG_PET_SPELLS)
state_allow(pet src/game/Harness/Trace.h
    WHY "harness recorder (decoupling D11 PR 2): the decoder's doc comment names the packet it reads"
    NAMES SMSG_PET_SPELLS)
state_allow(pet src/game/Harness/Trace.cpp
    WHY "harness recorder (decoupling D11 PR 2): classifies the packet by opcode and decodes its bytes into a TRACE line, builds nothing"
    NAMES SMSG_PET_SPELLS)
state_allow(pet src/tests/HarnessTest.cpp
    WHY "unit test of the harness decoders: hand-builds the packet's bytes as its writers lay them out and checks the decoded record, sends nothing"
    NAMES SMSG_PET_SPELLS)

# Decoupling D4k. The friend and ignore list. PlayerSocial builds the contact list and the friend
# status packet (the free builder beside it in SocialList.cpp); the global SocialMgr only sends them.
# Every other SMSG_ name in Opcodes.h containing FRIEND, CONTACT, SOCIAL, IGNORE or MUTE is left out:
# SMSG_REFER_A_FRIEND_EXPIRED and SMSG_REFER_A_FRIEND_FAILURE (Recruit-A-Friend),
# SMSG_IGNORE_REQUIREMENTS_CHEAT and SMSG_IGNORE_DIMINISHING_RETURNS_CHEAT (cheat toggles) and
# SMSG_VOICE_SET_TALKER_MUTED (voice chat) are none of this list's state and are built by nobody
# (only the definition sites spell them). The CMSG_ names are client packets (their handlers read,
# they build nothing). TYPES is the owner alone: its public accessors hand out values, and the
# character's GetSocial() const hands out the owner itself.
state_row(social
    OWNER   PlayerSocial
    PACKETS SMSG_CONTACT_LIST                    # PlayerSocial::SendSocialList
            SMSG_FRIEND_STATUS                   # MakeFriendStatusPacket / BuildFriendStatusPacket
    TABLES  character_social
    TYPES   PlayerSocial)

state_allow(social src/game/entities/player/social/SocialList.cpp
    WHY "the owner: builds its packets for the sink or the caller, writes the rows (add, remove, note)"
    NAMES SMSG_CONTACT_LIST SMSG_FRIEND_STATUS character_social)
state_allow(social src/game/entities/player/social/SocialList.h
    WHY "the owner's header: the class's and the builders' doc comments name the packets"
    NAMES SMSG_CONTACT_LIST SMSG_FRIEND_STATUS)
state_allow(social src/game/entities/player/Player.cpp
    WHY "whole-character delete (Player::DeleteFromDB: the read of who lists the character, and the DELETE)"
    NAMES character_social)
state_allow(social src/game/WorldHandlers/CharacterHandler.cpp
    WHY "login holder SELECT (PLAYER_LOGIN_QUERY_LOADSOCIALLIST), handed to SocialMgr::LoadFromDB's per-row load"
    NAMES character_social)
state_allow(social src/game/WorldHandlers/CalendarHandler.cpp
    WHY "an offline invitee's ignore flag for the inviter (the calendar invite's read, and its comments): the invitee's list is not loaded"
    NAMES character_social)
state_allow(social src/game/WorldHandlers/MiscHandlerSocial.cpp
    WHY "the social handlers' debug log lines name the packet SocialMgr::SendFriendStatus sent (a relay, builds nothing)"
    NAMES SMSG_FRIEND_STATUS)
state_allow(social src/tests/CalendarMailAsyncTest.cpp
    WHY "unit test that observes the calendar's read through a fake connection (checks the statement, runs nothing itself)"
    NAMES character_social)
state_allow(social src/tests/CharacterOpsAsyncTest.cpp
    WHY "unit test that observes the delete's read through a fake connection (checks the statement, runs nothing itself)"
    NAMES character_social)
state_allow(social src/tests/SocialMgrTest.cpp
    WHY "unit test that observes the packets through a capturing sink and the statements through a fake connection (checks each opcode, byte and statement, builds nothing)"
    NAMES SMSG_CONTACT_LIST SMSG_FRIEND_STATUS character_social)
# Decoupling D4k. The four currency packets CurrencyMgr builds. Every other name in Opcodes.h
# containing CURRENC is left out: SMSG_LOOT_CURRENCY_REMOVED is the loot code's (the owner's
# SendNotifyLootItemRemoved, a loot slot's removal); CMSG_LOOT_CURRENCY and CMSG_SET_CURRENCY_FLAGS
# are client packets (the loot and customize handlers read them; the second ends in the manager's
# SetFlags); CMSG_CHEAT_SET_HONOR_CURRENCY and CMSG_CHEAT_SET_ARENA_CURRENCY are client packets no
# handler reads. TABLES: the currency rows the manager loads one at a time and saves. TYPES is the
# owner alone: its public const accessors hand out counts and caps by value.
state_row(currency
    OWNER   CurrencyMgr
    PACKETS SMSG_SET_CURRENCY                    # CurrencyMgr::ModifyCount
            SMSG_SET_CURRENCY_WEEK_LIMIT         # CurrencyMgr::SendWeekCap
            SMSG_SEND_CURRENCIES                 # CurrencyMgr::SendAll
            SMSG_WEEKLY_RESET_CURRENCIES         # CurrencyMgr::ResetWeekCounts
    TABLES  character_currencies
    TYPES   CurrencyMgr)

state_allow(currency src/game/entities/player/inventory/CurrencyMgr.cpp
    WHY "the owner: builds its packets for the owner's session sink, loads the rows one at a time (the row's columns in a comment, the invalid-row DELETE) and saves them"
    NAMES SMSG_SET_CURRENCY SMSG_SET_CURRENCY_WEEK_LIMIT SMSG_SEND_CURRENCIES SMSG_WEEKLY_RESET_CURRENCIES character_currencies)
state_allow(currency src/game/entities/player/inventory/CurrencyMgr.h
    WHY "the owner's header: the flags' and the sinks' doc comments name the table and the packet"
    NAMES SMSG_SET_CURRENCY character_currencies)
state_allow(currency src/game/entities/player/Player.cpp
    WHY "whole-character delete (Player::DeleteFromDB)"
    NAMES character_currencies)
state_allow(currency src/game/WorldHandlers/CharacterHandler.cpp
    WHY "login holder SELECT (PLAYER_LOGIN_QUERY_LOADCURRENCIES), handed to the owner's per-row load"
    NAMES character_currencies)
state_allow(currency src/game/WorldHandlers/World.cpp
    WHY "realm-wide weekly reset (World::ResetCurrencyWeekCounts): every character's week count in one UPDATE, then each online character's manager"
    NAMES character_currencies)
state_allow(currency src/game/Harness/Trace.cpp
    WHY "harness recorder rule table: classifies the packet by opcode (hashes it), builds nothing"
    NAMES SMSG_SET_CURRENCY)
state_allow(currency src/game/Harness/ScenariosQuest.cpp
    WHY "harness scenario 925 counts the recorded packets by opcode, builds nothing"
    NAMES SMSG_SET_CURRENCY)
state_allow(currency src/tests/HarnessTest.cpp
    WHY "unit test of the recorder's rule table (checks the opcode's rule, builds nothing)"
    NAMES SMSG_SET_CURRENCY)
state_allow(currency src/tests/CurrencyMgrTest.cpp
    WHY "unit test that observes the packets through a capturing sink and the load's DELETE and the save's statements through a fake connection (checks each opcode, byte and statement, builds nothing)"
    NAMES SMSG_SET_CURRENCY SMSG_SET_CURRENCY_WEEK_LIMIT SMSG_SEND_CURRENCIES SMSG_WEEKLY_RESET_CURRENCIES character_currencies)
# Decoupling D4k. The one packet HonorMgr builds. The other server names in Opcodes.h containing
# PVP, HONOR or KILL are left out, each built by its own owner or by nobody: SMSG_INSPECT_HONOR_STATS
# (the inspect handler, MiscHandler.cpp), SMSG_PVP_LOG_DATA and SMSG_PVP_OPTIONS_ENABLED (the
# battleground manager and handler), SMSG_PVP_REWARDS (the owner's SendPvPRewards,
# PlayerBattleGround.cpp), SMSG_PARTYKILLLOG (the unit's kill, Unit.cpp), SMSG_SPELLINSTAKILLLOG
# and SMSG_QUESTUPDATE_ADD_KILL (the spell and quest code); SMSG_PVP_QUEUE_STATS,
# SMSG_REMOVED_FROM_PVP_QUEUE, SMSG_REPORT_PVP_AFK_RESULT and SMSG_QUESTUPDATE_ADD_PVP_KILL are built
# by nobody. No TABLES: the kill counts are update fields saved with the character row. TYPES is the
# owner alone: it has no public accessor.
state_row(honor
    OWNER   HonorMgr
    PACKETS SMSG_PVP_CREDIT                      # HonorMgr::Reward
    TYPES   HonorMgr)

state_allow(honor src/game/entities/player/pvp/HonorMgr.cpp
    WHY "the owner: builds the kill's honor packet for the owner's session sink"
    NAMES SMSG_PVP_CREDIT)
state_allow(honor src/game/entities/player/pvp/HonorMgr.h
    WHY "the owner's header: the class, the sink and Reward's doc comments name the packet"
    NAMES SMSG_PVP_CREDIT)
state_allow(honor src/tests/HonorMgrTest.cpp
    WHY "unit test that observes the packet through a capturing sink (checks the opcode and each byte, builds nothing)"
    NAMES SMSG_PVP_CREDIT)

# Shared by every row: the files that define opcodes. Each is checked to still spell at least
# one row packet.
state_definition_site(src/proto/Opcodes.h
    WHY "opcode definition: each packet's name and value")
state_definition_site(src/game/Server/OpcodeTable.cpp
    WHY "opcode table: each packet registered as a server-side opcode")
state_definition_site(src/proto/OpcodeSlots.inc
    WHY "generated client stream-slot table: each line's comment names its opcode, in the client's spelling")
state_definition_site(src/tests/OpcodeValuesTest.cpp
    WHY "opcode value test: pins corrected opcode values")

# ---------------------------------------------------------------------------------------------
# What is scanned: every file under src/ with one of these extensions -- the C and C++ sources
# and headers, the .inl/.inc fragments they include, the configure-time .in templates, and the
# assembly -- except under SCAN_EXCLUDED. Everything else under src/ is not code the server is
# built from: CMake scripts (this gate spells every name), .txt, .md, .py and .sh tools, the
# golden .log captures.
set(SCAN_EXTENSIONS c cc cpp cxx h hh hpp hxx inl inc ipp tpp in s S asm)
set(SCAN_EXCLUDED
    src/tests/oracle/s2n-bignum/    # third-party: AWS s2n-bignum, vendored as the crypto test oracle
)

# Every ';' in a scanned text becomes this byte first, so no rule has to spell ';' -- which
# would split the CMake lists the rules and their matches are kept in.
string(ASCII 1 SEMI)

# The 1-based line and the label (CMAKE_MATCH_<LABEL_GROUP>) of every match of RE in TEXT, as
# "<line>|<label>" entries. RE's first group is the leading word boundary (^|[^A-Za-z0-9_]).
# The first occurrence of a match's text is where the leftmost match starts (an earlier copy
# would have matched first). The walk resumes after the whole match. Every match ends on a
# non-word character -- a name on its trailing boundary (TEXT ends in a newline, so never on
# `$`), a cast on its '(' -- so the rest always follows a non-word character, and the `^`
# branch is a true word boundary there.
function(find_hits TEXT RE LABEL_GROUP OUT_VAR)
    set(FOUND "")
    set(REST "${TEXT}")
    set(LINE 1)
    while(TRUE)
        string(REGEX MATCH "${RE}" MATCH "${REST}")
        if("${MATCH}" STREQUAL "")
            break()
        endif()
        set(LABEL "${CMAKE_MATCH_${LABEL_GROUP}}")
        string(LENGTH "${CMAKE_MATCH_1}" LEAD)
        string(FIND "${REST}" "${MATCH}" AT)
        math(EXPR START "${AT} + ${LEAD}")
        string(SUBSTRING "${REST}" 0 ${START} BEFORE)
        string(REGEX REPLACE "[^\n]+" "" NEWLINES "${BEFORE}")
        string(LENGTH "${NEWLINES}" NEWLINE_COUNT)
        math(EXPR LINE "${LINE} + ${NEWLINE_COUNT}")
        list(APPEND FOUND "${LINE}|${LABEL}")
        # REST moves past the match; the lines the match itself spans (a const_cast written over
        # two lines) are counted on the way.
        string(LENGTH "${MATCH}" MATCH_LENGTH)
        math(EXPR SPAN "${MATCH_LENGTH} - ${LEAD}")
        string(SUBSTRING "${REST}" ${START} ${SPAN} SPANNED)
        string(REGEX REPLACE "[^\n]+" "" NEWLINES "${SPANNED}")
        string(LENGTH "${NEWLINES}" NEWLINE_COUNT)
        math(EXPR LINE "${LINE} + ${NEWLINE_COUNT}")
        math(EXPR NEXT "${AT} + ${MATCH_LENGTH}")
        string(SUBSTRING "${REST}" ${NEXT} -1 REST)
    endwhile()
    set(${OUT_VAR} "${FOUND}" PARENT_SCOPE)
endfunction()

# The name rule's regex for one name, and the const_cast rule's for a list of types. The label
# is group 2 of the first (the name) and group 3 of the second (the type named). The cast is
# matched whole -- `const_cast`, '<', a target naming the type, the closing '>' and the '(' that
# applies it -- and its target never crosses '(', ')', ';', '{' or '}'. So a cast to something
# else cannot borrow a type named later in the text, and prose that says "const_cast<>" and
# names a type on the next line is not a cast.
function(name_regex NAME OUT_VAR)
    set(${OUT_VAR} "(^|[^${WORD_CHARS}])(${NAME})([^${WORD_CHARS}]|$)" PARENT_SCOPE)
endfunction()

function(cast_regex TYPES OUT_VAR)
    string(REPLACE ";" "|" ALTERNATION "${TYPES}")
    set(${OUT_VAR}
        "(^|[^${WORD_CHARS}])const_cast[ \t\r\n]*<([^(){}${SEMI}]*[^${WORD_CHARS}])?(${ALTERNATION})([^${WORD_CHARS}][^(){}${SEMI}]*)?>[ \t\r\n]*\\("
        PARENT_SCOPE)
endfunction()

# The name hits ("<line>|<name>") of the names in NAMES, and the const_cast hits
# ("<line>|<type>") of the types in TYPES, in one text. A string(FIND) sieve first, so a file
# spelling none of them costs one pass per name and no regex.
function(scan_text TEXT NAMES TYPES NAME_HITS_VAR CAST_HITS_VAR)
    string(REPLACE ";" "${SEMI}" TEXT "${TEXT}")
    # A trailing newline, so every match ends on a real character and the walk's resume
    # arithmetic never meets the end of the text.
    string(APPEND TEXT "\n")
    set(NAME_HITS "")
    foreach(NAME IN LISTS NAMES)
        string(FIND "${TEXT}" "${NAME}" AT)
        if(AT GREATER -1)
            name_regex("${NAME}" RE)
            find_hits("${TEXT}" "${RE}" 2 HITS)
            list(APPEND NAME_HITS ${HITS})
        endif()
    endforeach()
    set(CAST_HITS "")
    string(FIND "${TEXT}" "const_cast" AT)
    if(AT GREATER -1)
        set(PRESENT "")
        foreach(TYPE IN LISTS TYPES)
            string(FIND "${TEXT}" "${TYPE}" AT)
            if(AT GREATER -1)
                list(APPEND PRESENT "${TYPE}")
            endif()
        endforeach()
        if(PRESENT)
            cast_regex("${PRESENT}" RE)
            find_hits("${TEXT}" "${RE}" 3 CAST_HITS)
        endif()
    endif()
    set(${NAME_HITS_VAR} "${NAME_HITS}" PARENT_SCOPE)
    set(${CAST_HITS_VAR} "${CAST_HITS}" PARENT_SCOPE)
endfunction()

# A file's text and whether it exists. In the self-test (SELFTEST_MODE) both come from
# SELFTEST_TEXT_<file id> and nothing is read from disk.
function(read_source FILE_REL OUT_VAR)
    file_id("${FILE_REL}" FILE_ID)
    if(SELFTEST_MODE)
        set(${OUT_VAR} "${SELFTEST_TEXT_${FILE_ID}}" PARENT_SCOPE)
    else()
        file(READ "${SOURCE_ROOT}/${FILE_REL}" TEXT)
        set(${OUT_VAR} "${TEXT}" PARENT_SCOPE)
    endif()
endfunction()

function(source_exists FILE_REL OUT_VAR)
    file_id("${FILE_REL}" FILE_ID)
    if(SELFTEST_MODE)
        if(DEFINED SELFTEST_TEXT_${FILE_ID})
            set(${OUT_VAR} ON PARENT_SCOPE)
        else()
            set(${OUT_VAR} OFF PARENT_SCOPE)
        endif()
    elseif(EXISTS "${SOURCE_ROOT}/${FILE_REL}" AND NOT IS_DIRECTORY "${SOURCE_ROOT}/${FILE_REL}")
        set(${OUT_VAR} ON PARENT_SCOPE)
    else()
        set(${OUT_VAR} OFF PARENT_SCOPE)
    endif()
endfunction()

# Every rule, over the files in the list SCANNED_VAR names (paths relative to SOURCE_ROOT).
# The failures come back as "<file>[:<line>]: <what>" entries.
function(evaluate SCANNED_VAR FAILURES_VAR)
    set(FAILURES "")

    # The table's own files exist and are read by the scan. One that is not is reported once,
    # here, and left out of the ratchet below.
    set(UNUSABLE "")
    foreach(INDEX IN LISTS ALLOWANCES)
        set(FILE_REL "${ALLOW_${INDEX}_FILE}")
        source_exists("${FILE_REL}" PRESENT)
        if(NOT PRESENT)
            list(APPEND FAILURES "${FILE_REL}: allowed on row '${ALLOW_${INDEX}_ROW}' but does not exist (renamed, deleted, or its letter case differs from the file on disk?) -- move the allowance to the file's path, or remove it")
            list(APPEND UNUSABLE "${FILE_REL}")
        elseif(NOT FILE_REL IN_LIST ${SCANNED_VAR})
            list(APPEND FAILURES "${FILE_REL}: allowed on row '${ALLOW_${INDEX}_ROW}' but not a file this gate scans: its letter case differs from the file on disk, or it is outside SCAN_EXTENSIONS / inside SCAN_EXCLUDED")
            list(APPEND UNUSABLE "${FILE_REL}")
        endif()
    endforeach()
    foreach(FILE_REL IN LISTS DEFINITION_SITES)
        source_exists("${FILE_REL}" PRESENT)
        if(NOT PRESENT)
            list(APPEND FAILURES "${FILE_REL}: a definition site that does not exist (renamed, deleted, or its letter case differs from the file on disk?) -- move it to the file's path, or remove it")
            list(APPEND UNUSABLE "${FILE_REL}")
        elseif(NOT FILE_REL IN_LIST ${SCANNED_VAR})
            list(APPEND FAILURES "${FILE_REL}: a definition site that this gate does not scan: its letter case differs from the file on disk, or it is outside SCAN_EXTENSIONS / inside SCAN_EXCLUDED")
            list(APPEND UNUSABLE "${FILE_REL}")
        endif()
    endforeach()

    # The scan.
    foreach(FILE_REL IN LISTS ${SCANNED_VAR})
        read_source("${FILE_REL}" TEXT)
        scan_text("${TEXT}" "${ALL_NAMES}" "${ALL_TYPES}" NAME_HITS CAST_HITS)
        file_id("${FILE_REL}" FILE_ID)
        foreach(HIT IN LISTS NAME_HITS)
            string(REPLACE "|" ";" PARTS "${HIT}")
            list(GET PARTS 0 LINE)
            list(GET PARTS 1 NAME)
            set(SEEN_${NAME} ON)
            set(ROW "${NAME_ROW_${NAME}}")
            if(ALLOWED_${FILE_ID}_${NAME})
                set(SPELLED_${FILE_ID}_${NAME} ON)
            elseif(DEFINITION_${FILE_ID} AND NAME IN_LIST ALL_PACKETS)
                set(SPELLED_${FILE_ID} ON)
            else()
                list(APPEND FAILURES
                    "${FILE_REL}:${LINE}: ${NAME} belongs to ${ROW_${ROW}_OWNER} (row '${ROW}') and this file has no allowance for it")
            endif()
        endforeach()
        foreach(HIT IN LISTS CAST_HITS)
            string(REPLACE "|" ";" PARTS "${HIT}")
            list(GET PARTS 0 LINE)
            list(GET PARTS 1 TYPE)
            set(ROW "${TYPE_ROW_${TYPE}}")
            list(APPEND FAILURES
                "${FILE_REL}:${LINE}: a const_cast to ${TYPE} (row '${ROW}') -- ${ROW_${ROW}_OWNER}'s state changes through its own methods")
        endforeach()
    endforeach()

    # The ratchet: every allowance is still needed, name by name, and every definition site
    # still defines something a row owns.
    foreach(INDEX IN LISTS ALLOWANCES)
        set(FILE_REL "${ALLOW_${INDEX}_FILE}")
        if(FILE_REL IN_LIST UNUSABLE)
            continue()
        endif()
        file_id("${FILE_REL}" FILE_ID)
        foreach(NAME IN LISTS ALLOW_${INDEX}_NAMES)
            if(NOT SPELLED_${FILE_ID}_${NAME})
                list(APPEND FAILURES
                    "${FILE_REL}: allowed to spell ${NAME} (row '${ALLOW_${INDEX}_ROW}': ${ALLOW_${INDEX}_WHY}) but no longer does -- allowance no longer needed: remove it")
            endif()
        endforeach()
    endforeach()
    foreach(FILE_REL IN LISTS DEFINITION_SITES)
        if(FILE_REL IN_LIST UNUSABLE)
            continue()
        endif()
        file_id("${FILE_REL}" FILE_ID)
        if(NOT SPELLED_${FILE_ID})
            list(APPEND FAILURES
                "${FILE_REL}: a definition site (${DEFINITION_${FILE_ID}_WHY}) that spells no row packet -- allowance no longer needed: remove it")
        endif()
    endforeach()
    foreach(NAME IN LISTS ALL_NAMES)
        if(NOT SEEN_${NAME})
            list(APPEND FAILURES
                "${NAME} (row '${NAME_ROW_${NAME}}'): spelled in no scanned file, so it guards nothing -- a typo, or gone: remove it from the row")
        endif()
    endforeach()

    set(${FAILURES_VAR} "${FAILURES}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------------------------
# Self-test, before the real scan. A broken regex or a broken rule fails the gate here, with
# FATAL_ERROR -- it never gets to pass the scan below quietly.

# The hits of one text, as "<line>:<label>" joined by ',' (names first, then casts).
function(expect_scan LABEL TEXT NAMES TYPES EXPECTED)
    scan_text("${TEXT}" "${NAMES}" "${TYPES}" NAME_HITS CAST_HITS)
    set(GOT "")
    foreach(HIT IN LISTS NAME_HITS CAST_HITS)
        string(REPLACE "|" ":" HIT "${HIT}")
        list(APPEND GOT "${HIT}")
    endforeach()
    string(REPLACE ";" "," GOT "${GOT}")
    if(NOT "${GOT}" STREQUAL "${EXPECTED}")
        message(FATAL_ERROR
            "StateOwnership self-test failed (${LABEL}): got '${GOT}', expected '${EXPECTED}'")
    endif()
endfunction()

set(T_NAMES SMSG_SET_FACTION_STANDING character_achievement character_achievement_progress)
set(T_TYPES ReputationMgr FactionState FactionStateList)

# The name rule: a whole word, in code, comments and literals alike.
expect_scan("a packet built in a handler" "WorldPacket data(SMSG_SET_FACTION_STANDING, 17);" "${T_NAMES}" "" "1:SMSG_SET_FACTION_STANDING")
expect_scan("a packet named in a comment" "// then send SMSG_SET_FACTION_STANDING" "${T_NAMES}" "" "1:SMSG_SET_FACTION_STANDING")
expect_scan("a table inside an SQL literal" "PExecute(\"DELETE FROM `character_achievement` WHERE `guid` = %u\", g);" "${T_NAMES}" "" "1:character_achievement")
expect_scan("a table in a block comment" "/* rows of character_achievement */ int x;" "${T_NAMES}" "" "1:character_achievement")
expect_scan("the name alone" "SMSG_SET_FACTION_STANDING" "${T_NAMES}" "" "1:SMSG_SET_FACTION_STANDING")
expect_scan("a longer name is another name" "SMSG_SET_FACTION_STANDING_EXTRA XSMSG_SET_FACTION_STANDING" "${T_NAMES}" "" "")
expect_scan("a longer table is another table" "`character_achievement_progress`" "${T_NAMES}" "" "1:character_achievement_progress")
expect_scan("prose in another case is fine" "the character achievement table; smsg_set_faction_standing" "${T_NAMES}" "" "")
expect_scan("the line numbers, and a name at the start of a line"
    "int a;\nSMSG_SET_FACTION_STANDING;\n\n  x(`character_achievement`);\r\ny(SMSG_SET_FACTION_STANDING)" "${T_NAMES}" ""
    "2:SMSG_SET_FACTION_STANDING,5:SMSG_SET_FACTION_STANDING,4:character_achievement")
expect_scan("a name ending its line, then the next line" "SMSG_SET_FACTION_STANDING\nSMSG_SET_FACTION_STANDING" "${T_NAMES}" ""
    "1:SMSG_SET_FACTION_STANDING,2:SMSG_SET_FACTION_STANDING")
expect_scan("two on one line, and adjacent" "a(SMSG_SET_FACTION_STANDING,SMSG_SET_FACTION_STANDING)\n\nSMSG_SET_FACTION_STANDING" "${T_NAMES}" ""
    "1:SMSG_SET_FACTION_STANDING,1:SMSG_SET_FACTION_STANDING,3:SMSG_SET_FACTION_STANDING")
expect_scan("brackets, backslashes and semicolons do not split the text"
    "a[0] = '\\\\'; b\\\n[; c\nd(character_achievement);" "${T_NAMES}" "" "3:character_achievement")
# The const_cast rule.
expect_scan("const_cast to the manager" "const_cast<ReputationMgr&>(p->GetReputationMgr())" "" "${T_TYPES}" "1:ReputationMgr")
expect_scan("const_cast with spaces and a pointer" "x = const_cast < FactionState * >(s);" "" "${T_TYPES}" "1:FactionState")
expect_scan("const_cast to what an accessor hands out" "const_cast<FactionStateList&>(mgr.GetStateList())" "" "${T_TYPES}" "1:FactionStateList")
expect_scan("const_cast over two lines" "int a;\nauto& m = const_cast<\n    ReputationMgr&>(r);" "" "${T_TYPES}" "2:ReputationMgr")
expect_scan("const_cast to a template of the type" "const_cast<std::map<uint32, FactionState>&>(m)" "" "${T_TYPES}" "1:FactionState")
expect_scan("const_cast in a comment still counts" "// const_cast<ReputationMgr*>(this)" "" "${T_TYPES}" "1:ReputationMgr")
expect_scan("const_cast to another type is fine" "const_cast<Player*>(p); ReputationMgr& r = x;" "" "${T_TYPES}" "")
expect_scan("const_cast to a longer name is fine" "const_cast<ReputationMgrView&>(v)" "" "${T_TYPES}" "")
expect_scan("the target stops at the paren" "const_cast<Foo*>(bar(ReputationMgr))" "" "${T_TYPES}" "")
expect_scan("another identifier ending in const_cast is not the keyword" "my_const_cast<ReputationMgr>(x)" "" "${T_TYPES}" "")
expect_scan("static_cast is not const_cast" "static_cast<ReputationMgr const*>(p)" "" "${T_TYPES}" "")
expect_scan("two casts" "const_cast<FactionState*>(a);\nconst_cast<ReputationMgr&>(b);" "" "${T_TYPES}" "1:FactionState,2:ReputationMgr")
expect_scan("a cast over two lines, then another" "const_cast<\n    ReputationMgr&>(r);\nconst_cast<FactionState*>(s);" "" "${T_TYPES}"
    "1:ReputationMgr,3:FactionState")
expect_scan("a cast inside a cast" "const_cast<ReputationMgr*>(const_cast<FactionState*>(s))" "" "${T_TYPES}" "1:ReputationMgr,1:FactionState")
expect_scan("prose saying const_cast<> above a type is not a cast"
    "// no const_cast<> needed here: the forced\n// ReputationMgr is only read" "" "${T_TYPES}" "")
expect_scan("a const_cast< in prose stops at ';' (the ';' masking)"
    "// no const_cast<> here;\nFactionState* f = const_cast<Other*>(p);" "" "${T_TYPES}" "")

# The rules and the ratchet, on a made-up table and made-up files. The function scope keeps
# the made-up table away from the real one. Each argument pair after the expectation is a file
# and its text; a file written '!<path>' exists but is not scanned.
function(expect_failures LABEL EXPECTED_COUNT EXPECTED_TEXT)
    set(SELFTEST_MODE ON)
    set(ROWS "")
    set(ALLOWANCES "")
    set(DEFINITION_SITES "")
    set(ALL_NAMES "")
    set(ALL_PACKETS "")
    set(ALL_TYPES "")
    state_row(t OWNER TestMgr
        PACKETS SMSG_TEST_BUILT SMSG_TEST_UNBUILT
        TABLES character_test
        TYPES TestMgr TestState)
    state_allow(t src/t/TestMgr.cpp WHY "the owner" NAMES SMSG_TEST_BUILT character_test)
    state_allow(t src/t/Delete.cpp WHY "whole-character delete" NAMES character_test)
    state_definition_site(src/t/Opcodes.h WHY "definitions")
    set(SCANNED "")
    set(PAIRS ${ARGN})
    list(LENGTH PAIRS PAIR_LENGTH)
    if(PAIR_LENGTH GREATER 0)
        math(EXPR LAST "${PAIR_LENGTH} - 1")
        foreach(I RANGE 0 ${LAST} 2)
            math(EXPR J "${I} + 1")
            list(GET PAIRS ${I} FILE_REL)
            list(GET PAIRS ${J} TEXT)
            string(REPLACE "," ";" TEXT "${TEXT}")
            if(FILE_REL MATCHES "^!(.*)$")
                set(FILE_REL "${CMAKE_MATCH_1}")
            else()
                list(APPEND SCANNED "${FILE_REL}")
            endif()
            file_id("${FILE_REL}" FILE_ID)
            set(SELFTEST_TEXT_${FILE_ID} "${TEXT}")
        endforeach()
    endif()
    evaluate(SCANNED FAILURES)
    list(LENGTH FAILURES GOT)
    string(REPLACE ";" "\n    " REPORT "${FAILURES}")
    if(NOT GOT EQUAL EXPECTED_COUNT)
        message(FATAL_ERROR
            "StateOwnership self-test failed (${LABEL}): ${GOT} failure(s), expected ${EXPECTED_COUNT}:\n    ${REPORT}")
    endif()
    if(NOT "${EXPECTED_TEXT}" STREQUAL "" AND NOT "${FAILURES}" MATCHES "${EXPECTED_TEXT}")
        message(FATAL_ERROR
            "StateOwnership self-test failed (${LABEL}): no failure matches '${EXPECTED_TEXT}':\n    ${REPORT}")
    endif()
endfunction()

# Commas in the texts below stand for semicolons (the pairs travel as one CMake list).
set(T_OWNER  "void TestMgr::Send() { WorldPacket d(SMSG_TEST_BUILT),\nSave(\"`character_test`\") }")
set(T_DELETE "PExecute(\"DELETE FROM `character_test` WHERE guid = %u\", g),")
set(T_DEFS   "SMSG_TEST_BUILT = 0x1,\nSMSG_TEST_UNBUILT = 0x2,")

expect_failures("the clean table" 0 ""
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Delete.cpp "${T_DELETE}" src/t/Opcodes.h "${T_DEFS}"
    src/t/Handler.cpp "void Handle() { mgr.Send(), }")
expect_failures("a row packet in a handler" 1 "src/t/Handler.cpp:2: SMSG_TEST_BUILT belongs to TestMgr .row 't'. and this file has no allowance for it"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Delete.cpp "${T_DELETE}" src/t/Opcodes.h "${T_DEFS}"
    src/t/Handler.cpp "void Handle()\n{ WorldPacket d(SMSG_TEST_BUILT), }")
expect_failures("a row table in a comment of an unlisted file" 1 "src/t/Handler.cpp:1: character_test"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Delete.cpp "${T_DELETE}" src/t/Opcodes.h "${T_DEFS}"
    src/t/Handler.cpp "// writes character_test directly")
expect_failures("an allowance covers its own names only" 1 "src/t/Delete.cpp:1: SMSG_TEST_UNBUILT"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Delete.cpp "SMSG_TEST_UNBUILT ${T_DELETE}" src/t/Opcodes.h "${T_DEFS}")
expect_failures("a definition site may not spell a table" 1 "src/t/Opcodes.h:3: character_test"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Delete.cpp "${T_DELETE}" src/t/Opcodes.h "${T_DEFS}\n// character_test")
expect_failures("a const_cast to the owner, in the owner" 1 "src/t/TestMgr.cpp:3: a const_cast to TestMgr"
    src/t/TestMgr.cpp "${T_OWNER}\nconst_cast<TestMgr*>(this)," src/t/Delete.cpp "${T_DELETE}" src/t/Opcodes.h "${T_DEFS}")
expect_failures("a const_cast to a state type, anywhere" 1 "src/t/Handler.cpp:1: a const_cast to TestState"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Delete.cpp "${T_DELETE}" src/t/Opcodes.h "${T_DEFS}"
    src/t/Handler.cpp "const_cast<TestState&>(mgr.GetState()).x = 1,")
expect_failures("the ratchet: an allowance no longer needed" 1 "src/t/Delete.cpp: allowed to spell character_test .*allowance no longer needed: remove it"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Delete.cpp "mgr.DeleteFromDB(g)," src/t/Opcodes.h "${T_DEFS}")
expect_failures("the ratchet, name by name" 1 "src/t/TestMgr.cpp: allowed to spell character_test"
    src/t/TestMgr.cpp "WorldPacket d(SMSG_TEST_BUILT)," src/t/Delete.cpp "${T_DELETE}" src/t/Opcodes.h "${T_DEFS}")
expect_failures("the ratchet: a definition site that defines nothing a row owns" 2 "src/t/Opcodes.h: a definition site .definitions. that spells no row packet"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Delete.cpp "${T_DELETE}" src/t/Opcodes.h "SMSG_OTHER = 0x3,")
expect_failures("an allowed file that does not exist" 1 "src/t/Delete.cpp: allowed on row 't' but does not exist"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Opcodes.h "${T_DEFS}")
expect_failures("a definition site that does not exist" 2 "src/t/Opcodes.h: a definition site that does not exist"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Delete.cpp "${T_DELETE}")
expect_failures("an allowed file the scan does not read" 1 "src/t/Delete.cpp: allowed on row 't' but not a file this gate scans"
    src/t/TestMgr.cpp "${T_OWNER}" !src/t/Delete.cpp "${T_DELETE}" src/t/Opcodes.h "${T_DEFS}")
expect_failures("a row name spelled nowhere" 1 "SMSG_TEST_UNBUILT .row 't'.: spelled in no scanned file"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Delete.cpp "${T_DELETE}" src/t/Opcodes.h "SMSG_TEST_BUILT = 0x1,")
# Decoupling D4h fix round 1: the file keys are injective. string(MAKE_C_IDENTIFIER) gave these
# two paths the ids of src/t/TestMgr.cpp and src/t/Delete.cpp -- an unlisted file inheriting an
# allowance, and keeping the listed file's ratchet quiet.
expect_failures("a path differing only in separators is another file" 1 "src/t_TestMgr.cpp:1: character_test"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Delete.cpp "${T_DELETE}" src/t/Opcodes.h "${T_DEFS}"
    src/t_TestMgr.cpp "// character_test")
expect_failures("... and does not keep the other's allowance alive" 2 "src/t/Delete.cpp: allowed to spell character_test .*no longer needed"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/Delete.cpp "mgr.DeleteFromDB(g)," src/t/Opcodes.h "${T_DEFS}"
    src/t_Delete.cpp "${T_DELETE}")
expect_failures("an allowed file in another letter case" 2 "src/t/Delete.cpp: allowed on row 't' but does not exist .renamed, deleted, or its letter case differs"
    src/t/TestMgr.cpp "${T_OWNER}" src/t/delete.cpp "${T_DELETE}" src/t/Opcodes.h "${T_DEFS}")

# ---------------------------------------------------------------------------------------------
# The real scan.

list(LENGTH ROWS ROW_COUNT)
list(LENGTH ALL_NAMES NAME_COUNT)
list(LENGTH ALLOWANCES ALLOWANCE_COUNT)
list(LENGTH DEFINITION_SITES DEFINITION_COUNT)
if(ROW_COUNT EQUAL 0)
    message(FATAL_ERROR "StateOwnership: the table has no rows -- the gate would check nothing")
endif()

file(GLOB_RECURSE PRESENT_FILES LIST_DIRECTORIES false RELATIVE "${SOURCE_ROOT}" "${SOURCE_ROOT}/src/*")
set(SCANNED "")
foreach(FILE_REL IN LISTS PRESENT_FILES)
    if(NOT FILE_REL MATCHES "\\.([^./]+)$")
        continue()
    endif()
    if(NOT CMAKE_MATCH_1 IN_LIST SCAN_EXTENSIONS)
        continue()
    endif()
    set(EXCLUDED OFF)
    foreach(PREFIX IN LISTS SCAN_EXCLUDED)
        string(FIND "${FILE_REL}" "${PREFIX}" AT)
        if(AT EQUAL 0)
            set(EXCLUDED ON)
        endif()
    endforeach()
    if(NOT EXCLUDED)
        list(APPEND SCANNED "${FILE_REL}")
    endif()
endforeach()
list(LENGTH SCANNED SCANNED_COUNT)
if(SCANNED_COUNT EQUAL 0)
    message(FATAL_ERROR "StateOwnership: no source under ${SOURCE_ROOT}/src -- the gate would check nothing")
endif()

set(SELFTEST_MODE OFF)
evaluate(SCANNED FAILURES)

if(FAILURES)
    list(LENGTH FAILURES FAILURE_COUNT)
    string(REPLACE ";" "\n  " REPORT "${FAILURES}")
    message(FATAL_ERROR
        "A character manager's packets or tables are spelled outside the manager, or the table in "
        "src/tests/CheckStateOwnership.cmake is out of date (decoupling D4h) -- ${FAILURE_COUNT} failure(s):\n  ${REPORT}\n"
        "Build the packet or touch the rows through the owner's own methods. Only a whole-character or "
        "realm-level tool (a delete, the login holder, the character dump, a start-up cleanup) or a test "
        "or harness that observes the packet (classifies it by opcode, builds nothing) gets an allowance: "
        "add a state_allow() with its reason and the names it spells, and say why in the PR. "
        "An allowance that is no longer needed is removed, never kept.")
endif()

message(STATUS "state ownership: ${ROW_COUNT} rows, ${NAME_COUNT} names, ${ALLOWANCE_COUNT} allowances and "
    "${DEFINITION_COUNT} definition sites all still needed, no const_cast to a row type, "
    "${SCANNED_COUNT} files scanned, self-test OK")
