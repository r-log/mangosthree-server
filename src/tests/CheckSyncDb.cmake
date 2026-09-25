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
# The two exceptions are named as such: ObjectMgr::ReturnOrDeleteOldMails runs at start-up
# (World.cpp, serverUp=false) AND on the mail timer inside World::Update, so its two reads
# are a declared residual of D7c, not a start-up path. Converting them is a later PR's job;
# they are allowed here so that the five lookups can be gated at all.
set(ALLOW_ObjectMgr_cpp
    # --- residual: ObjectMgr::ReturnOrDeleteOldMails, also called from World::Update ---
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `id`,`messageType`,`sender`,`receiver`,`has_items`,`expire_time`,`cod`,`checked`,`mailTemplateId` FROM `mail` WHERE `expire_time` < '\" UI64FMTD \"'\", (uint64)basetime)\;"
    "QueryResult* resultItems = CharacterDatabase.PQuery(\"SELECT `item_guid`,`item_template` FROM `mail_items` WHERE `mail_id`='%u'\", m->messageID)\;"
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
# Player.cpp has exactly ONE allowed line, and it is not this PR's:
#
#   Player::RemovePetitionsAndSigns(ObjectGuid) -- the SYNCHRONOUS overload, which exists only
#   for Guild::AddMember (src/game/Object/Guild.cpp), still called from inside the tick. That
#   chain is PR D7f's (the D7b report's turn-in residual); D7d converted the character-delete
#   caller by giving the function a second overload that takes the rows out of the delete
#   holder, and left this one alone rather than reaching into D7f's scope. When D7f converts
#   Guild::AddMember, this overload and this line go with it.
#
# Everything else Player.cpp used to block on is converted: DeleteFromDB's five reads (group,
# COD mail, those mails' items, pets, friends) are staged into one holder by StageDeleteReads;
# DeleteOldCharacters' guid list is an AsyncPQuery; and Player::Customize's `playerBytes2` read
# is staged by the customize handler next to its own.
set(ALLOW_Player_cpp
    # --- residual: Guild::AddMember's caller, PR D7f ---
    "CharacterDatabase.PQuery(\"SELECT `ownerguid`,`petitionguid` FROM `petition_sign` WHERE `playerguid` = '%u'\", guid.GetCounter()))\;")

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
