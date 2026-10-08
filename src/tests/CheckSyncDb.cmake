# Checks that no file in CONVERTED_FILES makes a blocking database call its ALLOW_ list does not hold, and
# that only ADMIN_SCOPE_FILES construct a TickGuard::AdminScope; reads those files and the sources under src/.
cmake_minimum_required(VERSION 3.18)

include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(SyncDb)

set(CONVERTED_FILES
    src/game/WorldHandlers/PetitionsHandler.cpp
    src/game/entities/player/persistence/PlayerDbLookup.cpp
    src/game/Object/ObjectMgr.cpp
    src/game/WorldHandlers/CharacterHandler.cpp
    src/game/WorldHandlers/CharacterHandlerCustomize.cpp
    src/game/entities/player/Player.cpp
    src/game/Object/PetDatabase.cpp
    src/game/Object/PetSpells.cpp
    src/game/WorldHandlers/NPCHandler.cpp
    src/game/WorldHandlers/SpellChecks.cpp
    src/game/Object/Guild.cpp
    src/game/Object/GuildRank.cpp
    src/game/entities/player/pvp/PlayerBattleGround.cpp
    src/game/WorldHandlers/CalendarHandler.cpp
    src/game/Object/Calendar.cpp
    src/game/WorldHandlers/Mail.cpp
    src/game/WorldHandlers/MailHandler.cpp
    src/game/WorldHandlers/SpellHandler.cpp
    src/game/Object/ArenaTeam.cpp
    src/game/Object/GuildBank.cpp
    src/game/WorldHandlers/Chat.cpp
    src/game/Harness/Harness.cpp
    src/game/WorldHandlers/Map.cpp
    src/game/WorldHandlers/InstanceData.cpp
    src/game/WorldHandlers/InstanceDataCache.cpp
    src/game/WorldHandlers/MapPersistentStateMgr.cpp
    src/game/BattleGround/BattleGroundReward.cpp
    src/game/BattleGround/BattleGroundMgr.cpp
    src/game/WorldHandlers/PetHandler.cpp
    src/game/WorldHandlers/MiscHandlerSocial.cpp
    src/game/WorldHandlers/MiscHandler.cpp
    src/game/entities/player/social/SocialMgr.cpp
    src/game/Object/GMTicketMgr.cpp
    src/game/Object/AuctionHouseMgr.cpp
    src/game/WorldHandlers/AccountMgr.cpp
    src/game/ChatCommands/AccountCommands.cpp
    src/game/entities/player/quests/QuestStatusMgr.h
    src/game/entities/player/quests/QuestStatusMgr.cpp
    src/game/entities/player/talents/TalentMgr.h
    src/game/entities/player/talents/TalentMgr.cpp
    src/game/entities/player/inventory/InventoryMgr.h
    src/game/entities/player/inventory/InventoryMgr.cpp
    src/game/entities/player/spells/RuneMgr.h
    src/game/entities/player/spells/RuneMgr.cpp
    src/game/spells/SpellCooldownMgr.h
    src/game/spells/SpellCooldownMgr.cpp
    src/game/entities/player/talents/GlyphMgr.h
    src/game/entities/player/talents/GlyphMgr.cpp
    src/game/entities/player/pets/PetMgr.h
    src/game/entities/player/pets/PetMgr.cpp
    src/game/entities/player/social/SocialList.h
    src/game/entities/player/social/SocialList.cpp
    src/game/entities/player/inventory/CurrencyMgr.h
    src/game/entities/player/inventory/CurrencyMgr.cpp
    src/game/entities/player/social/ReputationMgr.h
    src/game/entities/player/social/ReputationMgr.cpp
    src/game/entities/player/pvp/HonorMgr.h
    src/game/entities/player/pvp/HonorMgr.cpp
)

set(ADMIN_SCOPE_FILES
    src/game/WorldHandlers/Chat.cpp
    src/tests/TickGuardTest.cpp)

set(ALLOW_ObjectMgr_cpp
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `entry`, `quest` FROM `quest_relations` WHERE `actor` = %d\", QA_AREATRIGGER)\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id` FROM `areatrigger_tavern`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `groupId` FROM `groups`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`guid`) FROM `characters`\")\;"
    "result = WorldDatabase.Query(\"SELECT MAX(`guid`) FROM `creature`\")\;"
    "result = CharacterDatabase.Query(\"SELECT MAX(`guid`) FROM `item_instance`\")\;"
    "result = CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `instance`\")\;"
    "result = WorldDatabase.Query(\"SELECT MAX(`guid`) FROM `gameobject`\")\;"
    "result = CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `auction`\")\;"
    "result = CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `mail`\")\;"
    "result = CharacterDatabase.Query(\"SELECT MAX(`guid`) FROM `corpse`\")\;"
    "result = CharacterDatabase.Query(\"SELECT MAX(`arenateamid`) FROM `arena_team`\")\;"
    "result = CharacterDatabase.Query(\"SELECT MAX(`setguid`) FROM `character_equipmentsets`\")\;"
    "result = CharacterDatabase.Query(\"SELECT MAX(`guildid`) FROM `guild`\")\;"
    "result = CharacterDatabase.Query(\"SELECT MAX(`groupId`) FROM `groups`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `level`,`basexp` FROM `exploration_basexp`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `word`,`entry`,`half` FROM `pet_name_generation`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `character_pet`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `corpse`.`guid`, `player`, `corpse`.`position_x`, `corpse`.`position_y`, `corpse`.`position_z`, `corpse`.`orientation`, `corpse`.`map`, \""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `x`, `y`, `icon`, `flags`, `data`, `icon_name` FROM `points_of_interest`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `questId`, `poiId`, `objIndex`, `mapId`, `mapAreaId`, `floorId`, `unk3`, `unk4` FROM `quest_poi`\")\;"
    "QueryResult* points = WorldDatabase.Query(\"SELECT `questId`, `poiId`, `x`, `y` FROM `quest_poi_points`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `level`, `raceMask`, `mailTemplateId`, `senderEntry` FROM `mail_level_reward`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT entry, type, UNIX_TIMESTAMP(hotfixDate) FROM hotfix_data\")\;")

set(ALLOW_Guild_cpp
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `LogGuid`, `EventType`, `PlayerGuid1`, `PlayerGuid2`, `NewRank`, `TimeStamp` FROM `guild_eventlog` WHERE `guildid`=%u ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u\", m_Id, GUILD_EVENTLOG_MAX_RECORDS)\;")

set(ALLOW_Calendar_cpp
    "QueryResult* eventsQuery = CharacterDatabase.Query(\"SELECT `eventId`, `creatorGuid`, `guildId`, `type`, `flags`, `dungeonId`, `eventTime`, `title`, `description` FROM `calendar_events` ORDER BY `eventId`\")\;"
    "QueryResult* invitesQuery = CharacterDatabase.Query(\"SELECT `inviteId`, `eventId`, `inviteeGuid`, `senderGuid`, `status`, `lastUpdateTime`, `rank` FROM `calendar_invites` ORDER BY `inviteId`\")\;"
    "CharacterDatabase.DirectExecute(\"TRUNCATE TABLE calendar_events\")\;"
    "CharacterDatabase.DirectExecute(\"TRUNCATE TABLE calendar_invites\")\;")

set(ALLOW_GuildBank_cpp
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `TabId`, `TabName`, `TabIcon`, `TabText` FROM `guild_bank_tab` WHERE `guildid`='%u' ORDER BY `TabId`\", m_Id)\;"
    "result = CharacterDatabase.PQuery(\"SELECT `data`, `text`, `TabId`, `SlotId`, `item_guid`, `item_entry` FROM `guild_bank_item` JOIN `item_instance` ON `item_guid` = `guid` WHERE `guildid`='%u' ORDER BY `TabId`\", m_Id)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `LogGuid`, `EventType`, `PlayerGuid`, `ItemOrMoney`, `ItemStackCount`, `DestTabId`, `TimeStamp` FROM `guild_bank_eventlog` WHERE `guildid`='%u' AND `TabId`='%u' ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u\", m_Id, tabId, GUILD_BANK_MAX_LOGS)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `LogGuid`, `EventType`, `PlayerGuid`, `ItemOrMoney`, `ItemStackCount`, `DestTabId`, `TimeStamp` FROM `guild_bank_eventlog` WHERE `guildid`='%u' AND `TabId`='%u' ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u\", m_Id, GUILD_BANK_MONEY_LOGS_TAB, GUILD_BANK_MAX_LOGS)\;")

set(ALLOW_Chat_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `command_text`,`security`,`help_text` FROM `command`\")\;")

set(ALLOW_InstanceDataCache_cpp
    "if (QueryResult* result = CharacterDatabase.Query(\"SELECT `id`, `map`, `data` FROM `instance`\"))"
    "if (QueryResult* result = CharacterDatabase.Query(\"SELECT `map`, `data` FROM `world`\"))")

set(ALLOW_MapPersistentStateMgr_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `id`, `map`, `difficulty`, `resettime` FROM `instance` WHERE `resettime` > 0\")\;"
    "result = CharacterDatabase.Query(\"SELECT MAX(`respawntime`), `instance` FROM `creature_respawn` WHERE `instance` > 0 GROUP BY `instance`\")\;"
    "CharacterDatabase.DirectPExecute(\"UPDATE `instance` SET `resettime` = '\" UI64FMTD \"' WHERE `id` = '%u'\", uint64(resettime), instance)\;"
    "result = CharacterDatabase.Query(\"SELECT `mapid`, `difficulty`, `resettime` FROM `instance_reset`\")\;"
    "CharacterDatabase.DirectPExecute(\"DELETE FROM `instance_reset` WHERE `mapid` = '%u' AND `difficulty` = '%u'\", mapid, difficulty)\;"
    "CharacterDatabase.DirectPExecute(\"UPDATE `instance_reset` SET `resettime` = '\" UI64FMTD \"' WHERE `mapid` = '%u' AND `difficulty` = '%u'\", newresettime, mapid, difficulty)\;"
    "CharacterDatabase.DirectPExecute(\"INSERT INTO `instance_reset` VALUES ('%u','%u','\" UI64FMTD \"')\", mapid, difficulty, (uint64)t)\;"
    "CharacterDatabase.DirectPExecute(\"UPDATE `instance_reset` SET `resettime` = '\" UI64FMTD \"' WHERE mapid = '%u' AND difficulty= '%u'\", (uint64)t, mapid, difficulty)\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `id` FROM `instance`\")\;"
    "CharacterDatabase.DirectExecute(\"DELETE FROM `creature_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `creature_respawn` LEFT JOIN `instance` ON `instance` = `id`\")\;"
    "CharacterDatabase.DirectExecute(\"DELETE FROM `gameobject_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `gameobject_respawn` LEFT JOIN `instance` ON `instance` = `id`\")\;")

set(ALLOW_BattleGroundMgr_cpp
    "if (QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `pvpstats_battlegrounds`\"))"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `MinPlayersPerTeam`,`MaxPlayersPerTeam`,`AllianceStartLoc`,`AllianceStartO`,`HordeStartLoc`,`HordeStartO`, `StartMaxDist` FROM `battleground_template`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `NextArenaPointDistributionTime` FROM `saved_variables`\")\;")

set(ALLOW_GMTicketMgr_cpp
    "if (QueryResult* highest = CharacterDatabase.Query(\"SELECT MAX(`ticket_id`) FROM `character_ticket`\"))"
    "if (QueryResult* next = CharacterDatabase.Query(\"SELECT `AUTO_INCREMENT` FROM `information_schema`.`TABLES` WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'character_ticket'\"))"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guid`, `ticket_text`, `response_text`, UNIX_TIMESTAMP(`ticket_lastchange`), `ticket_id` FROM `character_ticket` WHERE `resolved` = 0 ORDER BY `ticket_id` ASC\")\;")

set(ALLOW_AuctionHouseMgr_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `data`,`text`,`itemguid`,`item_template` FROM `auction` JOIN `item_instance` ON `itemguid` = `guid`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT COUNT(*) FROM `auction`\")\;"
    "result = CharacterDatabase.Query(\"SELECT `id`,`houseid`,`itemguid`,`item_template`,`item_count`,`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`moneyTime`,`buyguid`,`lastbid`,`startbid`,`deposit` FROM `auction`\")\;")

set(ALLOW_AccountMgr_cpp
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT 1 FROM `account` WHERE `id`='%u'\", accid)\;"
    "result = CharacterDatabase.PQuery(\"SELECT `guid` FROM `characters` WHERE `account`='%u'\", accid)\;"
    "LoginDatabase.escape_string(safe_new_uname)\;"
    "LoginDatabase.escape_string(username)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id` FROM `account` WHERE `username` = '%s'\", username.c_str())\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `gmlevel` FROM `account` WHERE `id` = '%u'\", acc_id)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `username` FROM `account` WHERE `id` = '%u'\", acc_id)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'\", acc_id)\;")

set(ALLOW_AccountCommands_cpp
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `active_realm_id` = %u\", realmID)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `guid`, `name`, `race`, `class`, `level` FROM `characters` WHERE `account` = %u\", account_id)\;")

set(SYNC_DB_RE "(CharacterDatabase|WorldDatabase|LoginDatabase)[ \t]*\\.[ \t]*(P?Query|QueryNamed|PQueryNamed|DirectExecute|DirectPExecute|DirectExecuteStmt|Ping|CommitTransactionChecked|escape_string)[ \t]*\\(")

set(ADMIN_SCOPE_RE "TickGuard::AdminScope[ \t]+[A-Za-z_][A-Za-z_0-9]*[ \t]*\\(")

function(assert_regex LABEL LINE PATTERN EXPECT_MATCH)
    if(LINE MATCHES "${PATTERN}")
        set(GOT ON)
    else()
        set(GOT OFF)
    endif()
    if(NOT GOT STREQUAL EXPECT_MATCH)
        message(FATAL_ERROR
            "SyncDb self-test failed (${LABEL}): '${PATTERN}' against '${LINE}' "
            "expected match=${EXPECT_MATCH}, got ${GOT}")
    endif()
endfunction()

assert_regex("Query" "QueryResult* r = CharacterDatabase.Query(\"SELECT 1\");" "${SYNC_DB_RE}" ON)
assert_regex("PQuery" "QueryResult* r = CharacterDatabase.PQuery(\"SELECT %u\", 1);" "${SYNC_DB_RE}" ON)
assert_regex("QueryNamed" "WorldDatabase.QueryNamed(\"SELECT 1\");" "${SYNC_DB_RE}" ON)
assert_regex("PQueryNamed" "WorldDatabase.PQueryNamed(\"SELECT %u\", 1);" "${SYNC_DB_RE}" ON)
assert_regex("DirectExecute" "LoginDatabase.DirectExecute(\"SET NAMES utf8\");" "${SYNC_DB_RE}" ON)
assert_regex("DirectPExecute" "LoginDatabase.DirectPExecute(\"UPDATE x SET y = %u\", 1);" "${SYNC_DB_RE}" ON)
assert_regex("DirectExecuteStmt" "CharacterDatabase.DirectExecuteStmt(id, params);" "${SYNC_DB_RE}" ON)
assert_regex("Ping" "WorldDatabase.Ping();" "${SYNC_DB_RE}" ON)
assert_regex("CommitTransactionChecked" "if (!CharacterDatabase.CommitTransactionChecked())" "${SYNC_DB_RE}" ON)
assert_regex("escape_string" "CharacterDatabase.escape_string(name);" "${SYNC_DB_RE}" ON)
assert_regex("whitespace around the dot and the paren" "CharacterDatabase . PQuery (\"SELECT 1\");" "${SYNC_DB_RE}" ON)

assert_regex("an async query is not a synchronous one" "CharacterDatabase.AsyncPQuery(cb, \"SELECT %u\", 1);" "${SYNC_DB_RE}" OFF)
assert_regex("a queued execute is not a synchronous one" "CharacterDatabase.PExecute(\"UPDATE x SET y = %u\", 1);" "${SYNC_DB_RE}" OFF)
assert_regex("a held query holder is not a synchronous one" "CharacterDatabase.DelayQueryHolder(cb, holder);" "${SYNC_DB_RE}" OFF)
assert_regex("another object's Query is not one of the three" "sObjectMgr.Query(\"SELECT 1\");" "${SYNC_DB_RE}" OFF)
assert_regex("a mention without a call is not a call" "// CharacterDatabase.PQuery is what this replaced" "${SYNC_DB_RE}" OFF)

assert_regex("an AdminScope declaration" "    TickGuard::AdminScope administrativeReload(parentCommand && strcmp(parentCommand->Name, \"reload\") == 0);" "${ADMIN_SCOPE_RE}" ON)
assert_regex("an AdminScope declaration with no space before the paren" "TickGuard::AdminScope s(true);" "${ADMIN_SCOPE_RE}" ON)
assert_regex("prose naming the type is not a second site" " * line) and do not assert under TickGuard::AdminScope." "${ADMIN_SCOPE_RE}" OFF)
assert_regex("prose naming the type before a comma is not a second site" "// the ONE site that opens a TickGuard::AdminScope, and the gate says so" "${ADMIN_SCOPE_RE}" OFF)
assert_regex("the declaration in the guard's own header is not a use" "        explicit AdminScope(bool enter);" "${ADMIN_SCOPE_RE}" OFF)
assert_regex("a block comment in front of a real scope does not hide it" "/* enter */ TickGuard::AdminScope sneaky(true);" "${ADMIN_SCOPE_RE}" ON)

function(sync_db_line_violates FILE_KEY LINE OUT_VAR)
    set(RESULT OFF)
    if(LINE MATCHES "${SYNC_DB_RE}")
        set(RESULT ON)
        string(STRIP "${LINE}" TRIMMED)
        foreach(ALLOWED IN LISTS ALLOW_${FILE_KEY})
            string(STRIP "${ALLOWED}" ALLOWED_TRIMMED)
            if(TRIMMED STREQUAL ALLOWED_TRIMMED)
                set(RESULT OFF)
            endif()
        endforeach()
    endif()
    set(${OUT_VAR} "${RESULT}" PARENT_SCOPE)
endfunction()

set(ALLOW_SelfTest_cpp "CharacterDatabase.escape_string(name)\;")
sync_db_line_violates("SelfTest_cpp" "    CharacterDatabase.escape_string(name);" ALLOWED_VIOLATES)
if(ALLOWED_VIOLATES)
    message(FATAL_ERROR
        "SyncDb self-test failed (an allowed line): expected no violation, got one")
endif()
sync_db_line_violates("SelfTest_cpp" "    CharacterDatabase.escape_string(other);" NEARMISS_VIOLATES)
if(NOT NEARMISS_VIOLATES)
    message(FATAL_ERROR
        "SyncDb self-test failed (a line the allowance does not cover): "
        "expected a violation, got none")
endif()
sync_db_line_violates("NoAllowList_cpp" "    CharacterDatabase.escape_string(name);" UNALLOWED_VIOLATES)
if(NOT UNALLOWED_VIOLATES)
    message(FATAL_ERROR
        "SyncDb self-test failed (a file with no allow list): expected a violation, got none")
endif()
unset(ALLOW_SelfTest_cpp)

set(VIOLATIONS "")
list(LENGTH CONVERTED_FILES CONVERTED_COUNT)
gate_require_scanned(SyncDb "${CONVERTED_COUNT}" "files in CONVERTED_FILES")
foreach(REL_PATH IN LISTS CONVERTED_FILES)
    set(FILE_PATH "${SOURCE_ROOT}/${REL_PATH}")
    if(NOT EXISTS "${FILE_PATH}")
        message(FATAL_ERROR "SyncDb: ${FILE_PATH} does not exist")
    endif()

    get_filename_component(FILE_NAME "${FILE_PATH}" NAME)
    string(REGEX REPLACE "[^A-Za-z0-9]" "_" FILE_KEY "${FILE_NAME}")

    file(STRINGS "${FILE_PATH}" LINES)
    set(LINE_NO 0)
    foreach(LINE IN LISTS LINES)
        math(EXPR LINE_NO "${LINE_NO} + 1")

        sync_db_line_violates("${FILE_KEY}" "${LINE}" LINE_VIOLATES)
        if(LINE_VIOLATES)
            list(APPEND VIOLATIONS
                "${REL_PATH}:${LINE_NO}: a synchronous database call in a converted file")
        endif()
    endforeach()
endforeach()

if(VIOLATIONS)
    string(REPLACE ";" "\n  " REPORT "${VIOLATIONS}")
    list(LENGTH VIOLATIONS VIOLATION_COUNT)
    message(FATAL_ERROR
        "The no-sync-DB-in-the-tick seam is broken in ${VIOLATION_COUNT} place(s):\n  ${REPORT}\n"
        "A converted file asks the database asynchronously (AsyncQuery/AsyncPQuery/\n"
        "DelayQueryHolder) or defers the write (Execute/PExecute). If one of these lines\n"
        "genuinely runs outside the tick, add its exact text to that file's allow list in\n"
        "src/tests/CheckSyncDb.cmake and say in the PR why.")
endif()

file(GLOB_RECURSE ADMIN_SCOPE_SOURCES
    "${SOURCE_ROOT}/src/*.cpp" "${SOURCE_ROOT}/src/*.h" "${SOURCE_ROOT}/src/*.hpp")
list(LENGTH ADMIN_SCOPE_SOURCES ADMIN_SCOPE_SCANNED)
gate_require_scanned(SyncDb "${ADMIN_SCOPE_SCANNED}" "sources")
set(ADMIN_SCOPE_SITES "")
foreach(FILE_PATH IN LISTS ADMIN_SCOPE_SOURCES)
    file(STRINGS "${FILE_PATH}" LINES REGEX "TickGuard::AdminScope")
    foreach(LINE IN LISTS LINES)
        string(STRIP "${LINE}" TRIMMED)
        if(TRIMMED MATCHES "^(//|\\*)")
            continue()
        endif()
        if(LINE MATCHES "${ADMIN_SCOPE_RE}")
            file(RELATIVE_PATH REL_FILE "${SOURCE_ROOT}" "${FILE_PATH}")
            list(APPEND ADMIN_SCOPE_SITES "${REL_FILE}")
        endif()
    endforeach()
endforeach()

list(REMOVE_DUPLICATES ADMIN_SCOPE_SITES)
list(SORT ADMIN_SCOPE_SITES)
set(ADMIN_SCOPE_EXPECTED ${ADMIN_SCOPE_FILES})
list(SORT ADMIN_SCOPE_EXPECTED)

if(NOT "${ADMIN_SCOPE_SITES}" STREQUAL "${ADMIN_SCOPE_EXPECTED}")
    string(REPLACE ";" "\n  " ADMIN_SCOPE_REPORT "${ADMIN_SCOPE_SITES}")
    string(REPLACE ";" "\n  " ADMIN_SCOPE_WANTED "${ADMIN_SCOPE_EXPECTED}")
    message(FATAL_ERROR
        "TickGuard::AdminScope is constructed somewhere it may not be.\n"
        "Expected, and only:\n  ${ADMIN_SCOPE_WANTED}\n"
        "Found:\n  ${ADMIN_SCOPE_REPORT}\n"
        "The scope suppresses the MANGOS_STRICT_TICK assert and moves the acquisitions it\n"
        "covers into a counter of their own, so a second one in the server hides tick work\n"
        "from both this gate and `.server database`. If a new administrative family really\n"
        "needs one, say so in the PR and add its file to ADMIN_SCOPE_FILES, deliberately.")
endif()

message(STATUS "sync db: ${CONVERTED_COUNT} converted file(s) clean, AdminScope in the 2 named file(s) only (${ADMIN_SCOPE_SCANNED} file(s) scanned), self-test OK")
