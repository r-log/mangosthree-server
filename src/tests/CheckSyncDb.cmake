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
    src/game/entities/player/persistence/PlayerDbLookup.cpp # decoupling D7c
    src/game/Object/ObjectMgr.cpp                           # decoupling D7c
    src/game/WorldHandlers/CharacterHandler.cpp             # decoupling D7d
    src/game/WorldHandlers/CharacterHandlerCustomize.cpp    # decoupling D7d
    src/game/entities/player/Player.cpp                     # decoupling D7d
    src/game/Object/PetDatabase.cpp                         # decoupling D7e
    src/game/Object/PetSpells.cpp                           # decoupling D7e
    src/game/WorldHandlers/NPCHandler.cpp                   # decoupling D7e
    src/game/WorldHandlers/SpellChecks.cpp                  # decoupling D7e
    src/game/Object/Guild.cpp                               # decoupling D7f
    src/game/Object/GuildRank.cpp                           # decoupling D7f
    src/game/entities/player/pvp/PlayerBattleGround.cpp     # decoupling D7f
    src/game/WorldHandlers/CalendarHandler.cpp              # decoupling D7g
    src/game/Object/Calendar.cpp                            # decoupling D7g
    src/game/WorldHandlers/Mail.cpp                         # decoupling D7g
    src/game/WorldHandlers/MailHandler.cpp                  # decoupling D7g
    src/game/WorldHandlers/SpellHandler.cpp                 # decoupling D7g
    src/game/Object/ArenaTeam.cpp                           # decoupling D7g
    src/game/Object/GuildBank.cpp                           # decoupling D7g
    src/game/WorldHandlers/Chat.cpp                         # decoupling D7h
    src/game/Harness/Harness.cpp                            # decoupling D7h
    src/game/WorldHandlers/Map.cpp                          # decoupling D7i
    src/game/WorldHandlers/InstanceData.cpp                 # decoupling D7i
    src/game/WorldHandlers/InstanceDataCache.cpp            # decoupling D7i
    src/game/WorldHandlers/MapPersistentStateMgr.cpp        # decoupling D7i
    src/game/BattleGround/BattleGroundReward.cpp            # decoupling D7i
    src/game/BattleGround/BattleGroundMgr.cpp               # decoupling D7i
    src/game/WorldHandlers/PetHandler.cpp                   # decoupling D7i
    src/game/WorldHandlers/MiscHandlerSocial.cpp            # decoupling D7i
    src/game/WorldHandlers/MiscHandler.cpp                  # decoupling D7i
    src/game/entities/player/social/SocialMgr.cpp           # decoupling D7i
    src/game/Object/GMTicketMgr.cpp                         # decoupling D7i
    src/game/Object/AuctionHouseMgr.cpp                     # decoupling D7i
    src/game/WorldHandlers/AccountMgr.cpp                   # decoupling D7i
    src/game/ChatCommands/AccountCommands.cpp               # decoupling D7i
    src/game/entities/player/quests/QuestStatusMgr.h        # decoupling D4a
    src/game/entities/player/quests/QuestStatusMgr.cpp      # decoupling D4a
    src/game/entities/player/talents/TalentMgr.h            # decoupling D4c
    src/game/entities/player/talents/TalentMgr.cpp          # decoupling D4c
)

# The files that may construct a TickGuard::AdminScope (decoupling D7h), and nothing else
# under src/. The scope suppresses the strict-mode assert and moves the acquisitions it
# covers into a counter of their own, so a second one in the server would be a hole that no
# runtime counter could ever show. Checked below, after the file scan, over the whole of
# src/ -- a named list would only ever find what somebody remembered to list.
#
#   src/game/WorldHandlers/Chat.cpp -- ChatHandler::ExecuteCommand, around a `.reload <table>`
#                                      handler. THE site: the reload dispatcher.
#   src/tests/TickGuardTest.cpp     -- the cases that prove the scope counts apart, nests,
#                                      and does nothing when it is not entered. Named here
#                                      rather than skipping the whole of src/tests/, so a
#                                      scope opened in some other test still fails this.
set(ADMIN_SCOPE_FILES
    src/game/WorldHandlers/Chat.cpp
    src/tests/TickGuardTest.cpp)

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

# Decoupling D7h. Chat.cpp and Harness.cpp, the D7a baseline's last two tick acquisitions:
#
#   Chat.cpp     -- the `command` table's security and help overrides used to be read
#                   lazily, inside getCommandTable(), so the first chat or console command
#                   of a process paid a blocking SELECT on the world thread. The read is
#                   ChatHandler::LoadCommandTable() now, called by
#                   World::SetInitialWorldSettings before the tick exists and by
#                   `.reload command` under TickGuard::AdminScope. ONE allowed line, that
#                   loader's own query; the lazy branch is gone, so nothing else in this
#                   file may block.
#   Harness.cpp  -- Runner::Start's `SELECT COUNT(*) FROM characters WHERE guid BETWEEN`,
#                   the reserved-guid-block check for a queue holding a player scenario, is
#                   an AsyncPQuery whose continuation (Runner::OnGuidBlockChecked) makes the
#                   same three refusals, with the same text, before anything starts. It was
#                   the file's only blocking call, so NO allow list.
set(ALLOW_Chat_cpp
    # --- start-up (World::SetInitialWorldSettings) and `.reload command`: ChatHandler::LoadCommandTable ---
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `command_text`,`security`,`help_text` FROM `command`\")\;")

# Decoupling D7i. The player-reachable residual and the one tick-autonomous site:
#
#   Map.cpp                     -- Map::CreateInstanceData's two `SELECT data FROM
#                                  instance/world`, one per map created (every continent at
#                                  start-up, every transport deck, every dungeon entered),
#                                  read InstanceDataCache. NO allow list.
#   InstanceData.cpp            -- InstanceData::SaveToDB's escape is bound and its two
#                                  UPDATEs mirror into that cache. Its only call. NO allow list.
#   InstanceDataCache.cpp       -- the cache itself. TWO allowed lines, its own start-up reads.
#   MapPersistentStateMgr.cpp   -- DungeonResetScheduler::Update's DirectPExecute is queued
#                                  (the one site in the residual that needed no human at all),
#                                  and DungeonPersistentState::SaveToDB's escape is a bound
#                                  INSERT that mirrors into the cache. THIRTEEN allowed lines,
#                                  all start-up: LoadResetTimes, PackInstances and the two
#                                  respawn loaders.
#   BattleGroundReward.cpp      -- BattleGround::EndBattleGround's `SELECT MAX(id) FROM
#                                  pvpstats_battlegrounds` is an in-memory counter. NO allow list.
#   BattleGroundMgr.cpp         -- where that counter is primed. THREE allowed lines, all
#                                  start-up (the new one, the template loader, the arena-point
#                                  distribution time).
#   PetHandler.cpp              -- HandlePetRename's six escapes are bound. NO allow list.
#   MiscHandlerSocial.cpp       -- HandleAddFriendOpcode / HandleAddIgnoreOpcode resolve the
#                                  name through CharacterCache instead of escaping it for a
#                                  `characters` read. NO allow list.
#   MiscHandler.cpp             -- HandleBugOpcode's two escapes are bound; HandleWhoisOpcode's
#                                  `account` read is a continuation (it is SEC_ADMINISTRATOR
#                                  work, but an OPCODE, so no AdminScope can reach it). NO allow list.
#   SocialMgr.cpp               -- PlayerSocial::SetFriendNote's escape is bound. Its only
#                                  blocking call, so NO allow list.
#   GMTicketMgr.cpp             -- SaveSurveyData, SetText, SetResponseText and Create lose
#                                  four escapes, and Create loses its DirectPExecute AND the
#                                  SELECT that read the new id back: the id is a counter now.
#                                  THREE allowed lines, all LoadGMTickets'.
#   AuctionHouseMgr.cpp         -- SendAuctionWonMail's `sAccountMgr.GetSecurity()` for an
#                                  OFFLINE bidder is a continuation that writes the gm.log
#                                  line when the answer arrives. THREE allowed lines, the
#                                  start-up auction loaders.
#   AccountMgr.cpp              -- CheckPassword is gone; `.account password` goes through
#                                  QueueChangePasswordChecked, which reads `username` and
#                                  `sha_pass_hash` once, asynchronously, and does the whole
#                                  verify-and-change in the callback. NINE allowed lines: the
#                                  GM/console entry points (DeleteAccount, ChangeUsername,
#                                  GetId, GetSecurity, GetName, GetCharactersCount), which
#                                  ChatHandler::ExecuteCommand's AdminScope covers.
#   AccountCommands.cpp         -- the command above. TWO allowed lines, `.account onlinelist`
#                                  and `.account characters`, both SEC_ADMINISTRATOR.
set(ALLOW_InstanceDataCache_cpp
    # --- start-up: InstanceDataCache::LoadFromDB, from World::SetInitialWorldSettings ---
    "if (QueryResult* result = CharacterDatabase.Query(\"SELECT `id`, `map`, `data` FROM `instance`\"))"
    "if (QueryResult* result = CharacterDatabase.Query(\"SELECT `map`, `data` FROM `world`\"))")

set(ALLOW_MapPersistentStateMgr_cpp
    # --- start-up: DungeonResetScheduler::LoadResetTimes, from MapPersistentStateManager::LoadCreatureRespawnTimes' caller chain ---
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `id`, `map`, `difficulty`, `resettime` FROM `instance` WHERE `resettime` > 0\")\;"
    "result = CharacterDatabase.Query(\"SELECT MAX(`respawntime`), `instance` FROM `creature_respawn` WHERE `instance` > 0 GROUP BY `instance`\")\;"
    "CharacterDatabase.DirectPExecute(\"UPDATE `instance` SET `resettime` = '\" UI64FMTD \"' WHERE `id` = '%u'\", uint64(resettime), instance)\;"
    "result = CharacterDatabase.Query(\"SELECT `mapid`, `difficulty`, `resettime` FROM `instance_reset`\")\;"
    "CharacterDatabase.DirectPExecute(\"DELETE FROM `instance_reset` WHERE `mapid` = '%u' AND `difficulty` = '%u'\", mapid, difficulty)\;"
    "CharacterDatabase.DirectPExecute(\"UPDATE `instance_reset` SET `resettime` = '\" UI64FMTD \"' WHERE `mapid` = '%u' AND `difficulty` = '%u'\", newresettime, mapid, difficulty)\;"
    "CharacterDatabase.DirectPExecute(\"INSERT INTO `instance_reset` VALUES ('%u','%u','\" UI64FMTD \"')\", mapid, difficulty, (uint64)t)\;"
    "CharacterDatabase.DirectPExecute(\"UPDATE `instance_reset` SET `resettime` = '\" UI64FMTD \"' WHERE mapid = '%u' AND difficulty= '%u'\", (uint64)t, mapid, difficulty)\;"
    # --- start-up: MapPersistentStateManager::PackInstances ---
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `id` FROM `instance`\")\;"
    # --- start-up: MapPersistentStateManager::LoadCreatureRespawnTimes / LoadGameobjectRespawnTimes ---
    "CharacterDatabase.DirectExecute(\"DELETE FROM `creature_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `creature_respawn` LEFT JOIN `instance` ON `instance` = `id`\")\;"
    "CharacterDatabase.DirectExecute(\"DELETE FROM `gameobject_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `gameobject_respawn` LEFT JOIN `instance` ON `instance` = `id`\")\;")

set(ALLOW_BattleGroundMgr_cpp
    # --- start-up: BattleGroundMgr::LoadHighestPvPStatsId, from World::SetInitialWorldSettings ---
    "if (QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `pvpstats_battlegrounds`\"))"
    # --- start-up: BattleGroundMgr::CreateInitialBattleGrounds / InitAutomaticArenaPointDistribution ---
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `MinPlayersPerTeam`,`MaxPlayersPerTeam`,`AllianceStartLoc`,`AllianceStartO`,`HordeStartLoc`,`HordeStartO`, `StartMaxDist` FROM `battleground_template`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `NextArenaPointDistributionTime` FROM `saved_variables`\")\;")

# GMTicketMgr::LoadGMTickets' own SELECT was written across several lines, which would have
# forced a loose allowance (the bare `QueryResult* result = CharacterDatabase.Query(`); fix
# round 1 put it on one line so every entry here is an exact statement. The middle one is
# the ticket counter's AUTO_INCREMENT seed.
set(ALLOW_GMTicketMgr_cpp
    # --- start-up: GMTicketMgr::LoadGMTickets, from World::SetInitialWorldSettings ---
    "if (QueryResult* highest = CharacterDatabase.Query(\"SELECT MAX(`ticket_id`) FROM `character_ticket`\"))"
    "if (QueryResult* next = CharacterDatabase.Query(\"SELECT `AUTO_INCREMENT` FROM `information_schema`.`TABLES` WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'character_ticket'\"))"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guid`, `ticket_text`, `response_text`, UNIX_TIMESTAMP(`ticket_lastchange`), `ticket_id` FROM `character_ticket` WHERE `resolved` = 0 ORDER BY `ticket_id` ASC\")\;")

set(ALLOW_AuctionHouseMgr_cpp
    # --- start-up: AuctionHouseMgr::LoadAuctionItems / LoadAuctions ---
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `data`,`text`,`itemguid`,`item_template` FROM `auction` JOIN `item_instance` ON `itemguid` = `guid`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT COUNT(*) FROM `auction`\")\;"
    "result = CharacterDatabase.Query(\"SELECT `id`,`houseid`,`itemguid`,`item_template`,`item_count`,`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`moneyTime`,`buyguid`,`lastbid`,`startbid`,`deposit` FROM `auction`\")\;")

# AccountMgr's nine are GM or console work, every one of them reached through
# ChatHandler::ExecuteCommand and therefore inside the AdminScope D7i widened. The first
# entry is one line of text that appears in TWO functions (DeleteAccount and ChangeUsername
# open with the same existence check), which is what an allow list compared as exact text
# does; both are administrative.
set(ALLOW_AccountMgr_cpp
    # --- AccountMgr::DeleteAccount (`.account delete`) and AccountMgr::ChangeUsername ---
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT 1 FROM `account` WHERE `id`='%u'\", accid)\;"
    "result = CharacterDatabase.PQuery(\"SELECT `guid` FROM `characters` WHERE `account`='%u'\", accid)\;"
    "LoginDatabase.escape_string(safe_new_uname)\;"
    # --- AccountMgr::GetId (`.ban account`, `.account onlinelist`, the account arg extractor) ---
    "LoginDatabase.escape_string(username)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id` FROM `account` WHERE `username` = '%s'\", username.c_str())\;"
    # --- AccountMgr::GetSecurity (ChatHandler::HasLowerSecurity) ---
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `gmlevel` FROM `account` WHERE `id` = '%u'\", acc_id)\;"
    # --- AccountMgr::GetName (`.pinfo`, `.baninfo`, `.character deleted restore`) ---
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `username` FROM `account` WHERE `id` = '%u'\", acc_id)\;"
    # --- AccountMgr::GetCharactersCount (`.character deleted restore`, `.pdump load`) ---
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'\", acc_id)\;")

set(ALLOW_AccountCommands_cpp
    # --- `.account onlinelist` and `.account characters`, both SEC_ADMINISTRATOR ---
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `active_realm_id` = %u\", realmID)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `guid`, `name`, `race`, `class`, `level` FROM `characters` WHERE `account` = %u\", account_id)\;")

# Decoupling D4a. QuestStatusMgr.h and QuestStatusMgr.cpp have NO allow list. The quest status
# saves moved there from PlayerSave.cpp (INSERT/UPDATE `character_queststatus`, DELETE+INSERT
# `character_queststatus_weekly` / `_monthly`) and they stay queued prepared statements
# (SqlStatement Execute/PExecute), run from Player::SaveToDB. A blocking call in either file
# is exactly what strict tick mode would abort on.
#
# Decoupling D4c. TalentMgr.h and TalentMgr.cpp have NO allow list either. The talent save moved
# there from PlayerSave.cpp (DELETE+INSERT `character_talent`, queued prepared statements run
# from Player::SaveToDB), and so did the six DELETEs of the talent load's row checks
# (CharacterDatabase.PExecute, queued once async writes are on), run from the login.

set(SYNC_DB_RE "(CharacterDatabase|WorldDatabase|LoginDatabase)[ \t]*\\.[ \t]*(P?Query|QueryNamed|PQueryNamed|DirectExecute|DirectPExecute|DirectExecuteStmt|Ping|CommitTransactionChecked|escape_string)[ \t]*\\(")

# A CONSTRUCTION of a TickGuard::AdminScope -- the type name followed by a variable name and
# an open paren. Prose that merely names the type ("under TickGuard::AdminScope", "opens a
# TickGuard::AdminScope, and") cannot match it, which is what lets the comments explaining
# the rule live next to the rule.
set(ADMIN_SCOPE_RE "TickGuard::AdminScope[ \t]+[A-Za-z_][A-Za-z_0-9]*[ \t]*\\(")

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

assert_regex("an AdminScope declaration" "    TickGuard::AdminScope administrativeReload(parentCommand && strcmp(parentCommand->Name, \"reload\") == 0);" "${ADMIN_SCOPE_RE}" ON)
assert_regex("an AdminScope declaration with no space before the paren" "TickGuard::AdminScope s(true);" "${ADMIN_SCOPE_RE}" ON)
assert_regex("prose naming the type is not a second site" " * line) and do not assert under TickGuard::AdminScope." "${ADMIN_SCOPE_RE}" OFF)
assert_regex("prose naming the type before a comma is not a second site" "// the ONE site that opens a TickGuard::AdminScope, and the gate says so" "${ADMIN_SCOPE_RE}" OFF)
assert_regex("the declaration in the guard's own header is not a use" "        explicit AdminScope(bool enter);" "${ADMIN_SCOPE_RE}" OFF)
assert_regex("a block comment in front of a real scope does not hide it" "/* enter */ TickGuard::AdminScope sneaky(true);" "${ADMIN_SCOPE_RE}" ON)

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

# The AdminScope single-site rule (decoupling D7h). The whole of src/ is scanned, not a
# named list, because the point is to catch a scope opened somewhere nobody thought to
# look.
file(GLOB_RECURSE ADMIN_SCOPE_SOURCES
    "${SOURCE_ROOT}/src/*.cpp" "${SOURCE_ROOT}/src/*.h" "${SOURCE_ROOT}/src/*.hpp")
set(ADMIN_SCOPE_SITES "")
foreach(FILE_PATH IN LISTS ADMIN_SCOPE_SOURCES)
    file(STRINGS "${FILE_PATH}" LINES REGEX "TickGuard::AdminScope")
    foreach(LINE IN LISTS LINES)
        string(STRIP "${LINE}" TRIMMED)
        # A commented-out declaration is not a scope: the compiler never sees it. Only the
        # two shapes that make the WHOLE line a comment are skipped -- `//` and a doc
        # comment's continuation `*`. A block-comment OPENER is deliberately NOT skipped:
        # `/* enter */ TickGuard::AdminScope sneaky(true);` is real code with a comment in
        # front of it, and skipping the line would hide it. Prose that opens a block comment
        # and names the type still cannot match ADMIN_SCOPE_RE, which needs the type followed
        # by an identifier and an open paren.
        if(TRIMMED MATCHES "^(//|\\*)")
            continue()
        endif()
        if(LINE MATCHES "${ADMIN_SCOPE_RE}")
            file(RELATIVE_PATH REL_FILE "${SOURCE_ROOT}" "${FILE_PATH}")
            list(APPEND ADMIN_SCOPE_SITES "${REL_FILE}")
        endif()
    endforeach()
endforeach()

# By file, not by line: the test constructs three, and counting them would make the gate
# fail the next time a case is added rather than the next time a scope escapes.
list(REMOVE_DUPLICATES ADMIN_SCOPE_SITES)
list(SORT ADMIN_SCOPE_SITES)
set(ADMIN_SCOPE_EXPECTED ${ADMIN_SCOPE_FILES})
list(SORT ADMIN_SCOPE_EXPECTED)

if(NOT "${ADMIN_SCOPE_SITES}" STREQUAL "${ADMIN_SCOPE_EXPECTED}")
    string(REPLACE ";" "\n  " ADMIN_SCOPE_REPORT "${ADMIN_SCOPE_SITES}")
    string(REPLACE ";" "\n  " ADMIN_SCOPE_WANTED "${ADMIN_SCOPE_EXPECTED}")
    message(FATAL_ERROR
        "TickGuard::AdminScope is constructed somewhere it may not be (decoupling D7h).\n"
        "Expected, and only:\n  ${ADMIN_SCOPE_WANTED}\n"
        "Found:\n  ${ADMIN_SCOPE_REPORT}\n"
        "The scope suppresses the MANGOS_STRICT_TICK assert and moves the acquisitions it\n"
        "covers into a counter of their own, so a second one in the server hides tick work\n"
        "from both this gate and `.server database`. If a new administrative family really\n"
        "needs one, say so in the PR and add its file to ADMIN_SCOPE_FILES, deliberately.")
endif()

list(LENGTH ADMIN_SCOPE_SOURCES ADMIN_SCOPE_SCANNED)
message(STATUS "sync db: ${CONVERTED_COUNT} converted file(s) clean, AdminScope in the 2 named file(s) only (${ADMIN_SCOPE_SCANNED} file(s) scanned), self-test OK")
