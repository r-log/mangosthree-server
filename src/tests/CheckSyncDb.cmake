# Decoupling D7: a file that has been converted off synchronous database calls stays
# converted. For every path in CONVERTED_FILES, a line that calls one of the blocking
# Database entry points on CharacterDatabase/WorldDatabase/LoginDatabase fails this gate,
# naming the file and the line -- unless that exact line is in the file's allow list.
#
# This gate covers DIRECT calls in converted files. It cannot see an indirect chain (a
# handler that calls a helper that queries), and it is not meant to: the runtime counter
# (TickGuard, `.server database`) is what measures those, because only a run knows which
# chains the tick actually walks. The two together are the proof; neither alone is.
#
# CONVERTED_FILES was empty in D7a, which converted nothing -- the gate was built there so
# the conversion PRs had somewhere to append. The self-tests below run before the scan, so
# the gate is known to work whether or not it has anything to guard.
#
# Run standalone (-P), this script sees none of the top-level project's policies. The
# project requires CMake >= 3.18, so that is also the floor here.
cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SyncDb: -DSOURCE_ROOT=<repo root> is required")
endif()

# Repo-relative paths. Later PRs append one line per converted file.
set(CONVERTED_FILES
    src/game/WorldHandlers/PetitionsHandler.cpp             # decoupling D7b
    src/game/Object/PlayerDbLookup.cpp                      # decoupling D7c
    src/game/Object/ObjectMgr.cpp                           # decoupling D7c
    src/game/WorldHandlers/CharacterHandler.cpp             # decoupling D7d
    src/game/WorldHandlers/CharacterHandlerCustomize.cpp    # decoupling D7d
    src/game/Object/Player.cpp                              # decoupling D7d
    src/game/Object/PetDatabase.cpp                         # decoupling D7e
    src/game/Object/PetSpells.cpp                           # decoupling D7e
    src/game/WorldHandlers/NPCHandler.cpp                   # decoupling D7e
    src/game/WorldHandlers/SpellChecks.cpp                  # decoupling D7e
    src/game/Object/Guild.cpp                               # decoupling D7f
    src/game/Object/GuildRank.cpp                           # decoupling D7f
    src/game/Object/PlayerBattleGround.cpp                  # decoupling D7f
    src/game/WorldHandlers/CalendarHandler.cpp              # decoupling D7g
    src/game/Object/Calendar.cpp                            # decoupling D7g
    src/game/WorldHandlers/Mail.cpp                         # decoupling D7g
    src/game/WorldHandlers/MailHandler.cpp                  # decoupling D7g
    src/game/WorldHandlers/SpellHandler.cpp                 # decoupling D7g
    src/game/Object/ArenaTeam.cpp                           # decoupling D7g
    src/game/Object/GuildBank.cpp                           # decoupling D7g
)

# Per file, the exact lines (trimmed) that are allowed to keep a direct call -- a startup
# path in a file that is otherwise converted, say. Named ALLOW_<file name with every
# non-alphanumeric character replaced by _>. Empty in D7a.
#
# Write the semicolons in the line text as \; -- an unescaped one would split the entry
# into two list elements, neither of which matches anything. The self-test below covers
# exactly that spelling, so a mistake here fails the gate rather than quietly allowing
# nothing:
#
#   set(ALLOW_PetitionsHandler_cpp
#       "CharacterDatabase.escape_string(name)\;")

# Decoupling D7c. PlayerDbLookup.cpp has NO allow list: all six of its lookups read the
# character cache now, so nothing in it may block again.
#
# ObjectMgr.cpp's five offline lookups (GetPlayerGuidByName, GetPlayerNameByGUID,
# GetPlayerTeamByGUID, GetPlayerAccountIdByGUID, GetPlayerAccountIdByPlayerName) read the
# same cache and likewise have NO entry here -- if one of them ever queries again, this
# gate names the line. What is allowed is the rest of the file, which is start-up work:
# every line below is inside a Load*/Pack*/SetHighestGuids that World::SetInitialWorldSettings
# calls once, before the tick exists.
#
# D7c's two declared exceptions -- ObjectMgr::ReturnOrDeleteOldMails' mail and mail-items
# reads, which run on the mail timer inside World::Update as well as at start-up -- are
# GONE: D7f stages both into one holder and does the returns and deletes in a continuation,
# so this list is now start-up work and nothing else.
set(ALLOW_ObjectMgr_cpp
    # --- ObjectMgr::LoadQuestAreaTriggers / LoadTavernAreaTriggers ---
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `entry`, `quest` FROM `quest_relations` WHERE `actor` = %d\", QA_AREATRIGGER)\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id` FROM `areatrigger_tavern`\")\;"
    # --- ObjectMgr::PackGroupIds ---
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `groupId` FROM `groups`\")\;"
    # --- ObjectMgr::SetHighestGuids ---
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
    # --- ObjectMgr::LoadExplorationBaseXP / LoadPetNames / LoadPetNumber / LoadCorpses ---
    "QueryResult* result = WorldDatabase.Query(\"SELECT `level`,`basexp` FROM `exploration_basexp`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `word`,`entry`,`half` FROM `pet_name_generation`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `character_pet`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `corpse`.`guid`, `player`, `corpse`.`position_x`, `corpse`.`position_y`, `corpse`.`position_z`, `corpse`.`orientation`, `corpse`.`map`, \""
    # --- ObjectMgr::LoadPointsOfInterest / LoadQuestPOI / LoadMailLevelRewards / LoadHotfixData ---
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `x`, `y`, `icon`, `flags`, `data`, `icon_name` FROM `points_of_interest`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `questId`, `poiId`, `objIndex`, `mapId`, `mapAreaId`, `floorId`, `unk3`, `unk4` FROM `quest_poi`\")\;"
    "QueryResult* points = WorldDatabase.Query(\"SELECT `questId`, `poiId`, `x`, `y` FROM `quest_poi_points`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `level`, `raceMask`, `mailTemplateId`, `senderEntry` FROM `mail_level_reward`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT entry, type, UNIX_TIMESTAMP(hotfixDate) FROM hotfix_data\")\;")

# Decoupling D7d. CharacterHandler.cpp and CharacterHandlerCustomize.cpp have NO allow list:
# character create, delete, customize, rename and declined names all read through holders or
# async queries now, and all five escapes are gone, so nothing in either file may block again.
#
# Player.cpp has NO allow list either, as of D7f. D7d left it exactly one line -- the
# synchronous Player::RemovePetitionsAndSigns(ObjectGuid), which existed only for
# Guild::AddMember -- and D7f replaced that overload with Player::QueueRemovePetitionsAndSigns,
# which stages the same statement asynchronously. Everything else Player.cpp used to block on
# was already converted: DeleteFromDB's five reads (group, COD mail, those mails' items, pets,
# friends) are staged into one holder by StageDeleteReads; DeleteOldCharacters' guid list is an
# AsyncPQuery; and Player::Customize's `playerBytes2` read is staged by the customize handler
# next to its own.

# Decoupling D7e. FOUR files, and NONE of them has an allow list -- the whole pet subsystem's
# reads are gone:
#
#   PetDatabase.cpp   -- LoadPetFromDB's five `character_pet` branches, its declined-name read
#                        and SavePetToDB's free-slot scan all read PlayerPetCache.
#   PetSpells.cpp     -- _LoadAuras / _LoadSpells / _LoadSpellCooldowns and both of
#                        resetTalentsForAllPetsOf's reads likewise.
#   NPCHandler.cpp    -- the five stable-handler reads (the stable list, the set-pet-slot
#                        source and displaced pets, unstable, swap). The file had no other
#                        blocking call.
#   SpellChecks.cpp   -- the Tame Beast roster COUNT. Its only blocking call.
#
# The pet WRITES stay where they are: they are queued (PExecute / prepared statements), which
# this gate does not match, and each one now updates the cache beside itself. If a read ever
# comes back to any of these four files, the gate names the line.

# Decoupling D7f. The guild-creation chain:
#
#   Guild.cpp          -- Guild::Create's three escapes (name, info, MOTD) and Guild::AddMember's
#                         two (the member's notes) are prepared statements with the strings bound;
#                         AddMember's offline `SELECT name,level,class,zone,account` reads the
#                         character cache; and the four note/text setters (SetPNOTE, SetOFFNOTE,
#                         SetMOTD, SetGINFO), which the guild handlers reach from inside the tick,
#                         lost their escapes the same way.
#   GuildRank.cpp      -- Guild::CreateRank (five times per guild created, through
#                         CreateDefaultGuildRanks) and Guild::SetRankName likewise. NO allow list.
#   PlayerBattleGround -- Player::LeaveAllArenaTeams' `arena_team_member` lookup reads the
#                         cache's three arena slots. It was the file's ONLY blocking call, so
#                         NO allow list.
#
# Guild.cpp has exactly ONE allowed line: Guild::LoadGuildEventLogFromDB, which GuildMgr::LoadGuilds
# calls once per guild at start-up (World::SetInitialWorldSettings) and nothing else calls at all.
# Its sibling repair in Guild::LoadRanksFromDB is also start-up-only and was converted anyway --
# it is the same INSERT as CreateRank's, and there was no reason to let one path keep an escape
# the other had lost.
set(ALLOW_Guild_cpp
    # --- start-up: Guild::LoadGuildEventLogFromDB, from GuildMgr::LoadGuilds ---
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `LogGuid`, `EventType`, `PlayerGuid1`, `PlayerGuid2`, `NewRank`, `TimeStamp` FROM `guild_eventlog` WHERE `guildid`=%u ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u\", m_Id, GUILD_EVENTLOG_MAX_RECORDS)\;")

# Decoupling D7g. The calendar, mail and wrapped-item residual the D7a inventory named, plus
# the arena twins of D7f's two guild sites and the guild bank's escapes:
#
#   CalendarHandler.cpp -- HandleCalendarEventInvite's escape and its `SELECT guid,race FROM
#                          characters WHERE name` read the character cache; the invitee's
#                          `character_social` ignore flag, which nothing caches, is a
#                          continuation; HandleCalendarUpdateEvent's two escapes became
#                          CalendarMgr::WriteEventUpdateToDB's bound UPDATE. NO allow list.
#   Calendar.cpp        -- CalendarMgr::AddEvent's two escapes became WriteEventToDB's bound
#                          INSERT. FOUR allowed lines, all inside LoadCalendarsFromDB.
#   Mail.cpp            -- MailDraft::SendMailTo's subject and body are bound. That is the
#                          whole file's blocking surface, and it is the one place every mail
#                          in the server is written, so quest rewards, auctions, the mass
#                          mailer, the calendar and the GM commands all lost it at once.
#                          NO allow list.
#   MailHandler.cpp     -- HandleSendMail's offline `SELECT COUNT(*) FROM mail` is a
#                          continuation. Its only blocking call, so NO allow list.
#   SpellHandler.cpp    -- HandleOpenItemOpcode's `character_gifts` read is a continuation.
#                          Its only blocking call, so NO allow list.
#   ArenaTeam.cpp       -- ArenaTeam::Create's name escape is bound and AddMember's
#                          `SELECT name,class FROM characters` reads the cache: the twins of
#                          Guild::Create and Guild::AddMember (D7f). The file had no other
#                          blocking call -- its start-up loaders are handed their results by
#                          ObjectMgr::LoadArenaTeams -- so NO allow list.
#   GuildBank.cpp       -- Guild::SetGuildBankTabInfo's two escapes and SetGuildBankTabText's
#                          one are bound. FOUR allowed lines, the start-up bank reads.
#
# Calendar.cpp's four are CalendarMgr::LoadCalendarsFromDB's, which World::SetInitialWorldSettings
# calls once before the tick exists; the two TRUNCATEs are its repair for a half-empty pair of
# tables and run from the same function.
set(ALLOW_Calendar_cpp
    # --- start-up: CalendarMgr::LoadCalendarsFromDB, from World::SetInitialWorldSettings ---
    "QueryResult* eventsQuery = CharacterDatabase.Query(\"SELECT `eventId`, `creatorGuid`, `guildId`, `type`, `flags`, `dungeonId`, `eventTime`, `title`, `description` FROM `calendar_events` ORDER BY `eventId`\")\;"
    "QueryResult* invitesQuery = CharacterDatabase.Query(\"SELECT `inviteId`, `eventId`, `inviteeGuid`, `senderGuid`, `status`, `lastUpdateTime`, `rank` FROM `calendar_invites` ORDER BY `inviteId`\")\;"
    "CharacterDatabase.DirectExecute(\"TRUNCATE TABLE calendar_events\")\;"
    "CharacterDatabase.DirectExecute(\"TRUNCATE TABLE calendar_invites\")\;")

# GuildBank.cpp's four are Guild::LoadGuildBankFromDB's two and Guild::LoadGuildBankEventLogFromDB's
# two, which GuildMgr::LoadGuilds calls once per guild at start-up and nothing else calls at all.
set(ALLOW_GuildBank_cpp
    # --- start-up: Guild::LoadGuildBankFromDB, from GuildMgr::LoadGuilds ---
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `TabId`, `TabName`, `TabIcon`, `TabText` FROM `guild_bank_tab` WHERE `guildid`='%u' ORDER BY `TabId`\", m_Id)\;"
    "result = CharacterDatabase.PQuery(\"SELECT `data`, `text`, `TabId`, `SlotId`, `item_guid`, `item_entry` FROM `guild_bank_item` JOIN `item_instance` ON `item_guid` = `guid` WHERE `guildid`='%u' ORDER BY `TabId`\", m_Id)\;"
    # --- start-up: Guild::LoadGuildBankEventLogFromDB, from GuildMgr::LoadGuilds ---
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `LogGuid`, `EventType`, `PlayerGuid`, `ItemOrMoney`, `ItemStackCount`, `DestTabId`, `TimeStamp` FROM `guild_bank_eventlog` WHERE `guildid`='%u' AND `TabId`='%u' ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u\", m_Id, tabId, GUILD_BANK_MAX_LOGS)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `LogGuid`, `EventType`, `PlayerGuid`, `ItemOrMoney`, `ItemStackCount`, `DestTabId`, `TimeStamp` FROM `guild_bank_eventlog` WHERE `guildid`='%u' AND `TabId`='%u' ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u\", m_Id, GUILD_BANK_MONEY_LOGS_TAB, GUILD_BANK_MAX_LOGS)\;")

set(SYNC_DB_RE "(CharacterDatabase|WorldDatabase|LoginDatabase)[ \t]*\\.[ \t]*(P?Query|QueryNamed|PQueryNamed|DirectExecute|DirectPExecute|DirectExecuteStmt|Ping|CommitTransactionChecked|escape_string)[ \t]*\\(")

# Self-test: the regex is exercised against a positive and a negative string for every
# spelling it has to recognise, and the allow-line rule is exercised both ways, before the
# scan runs. A broken regex fails here, with FATAL_ERROR -- it never gets the chance to
# silently pass the real scan below.
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

# Whether a line calls a blocking entry point that its file is not allowed to keep. The
# allowed lines are compared as exact (trimmed) text, so an allowance covers the one line
# it was written for and not a second call bolted onto it.
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

# The allow rule, both ways, on a file key that exists only for this self-test. Note the
# \; -- that is the spelling a real entry has to use.
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

# The real scan.
set(VIOLATIONS "")
list(LENGTH CONVERTED_FILES CONVERTED_COUNT)
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
        "The no-sync-DB-in-the-tick seam (decoupling D7) is broken in ${VIOLATION_COUNT} place(s):\n  ${REPORT}\n"
        "A converted file asks the database asynchronously (AsyncQuery/AsyncPQuery/\n"
        "DelayQueryHolder) or defers the write (Execute/PExecute). If one of these lines\n"
        "genuinely runs outside the tick, add its exact text to that file's allow list in\n"
        "src/tests/CheckSyncDb.cmake and say in the PR why.")
endif()

message(STATUS "sync db: ${CONVERTED_COUNT} converted file(s) clean, self-test OK")
