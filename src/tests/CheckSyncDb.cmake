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
    src/game/persistence/characters/CharacterTransaction.h
    src/game/persistence/characters/CharacterTransaction.cpp
    src/game/persistence/characters/pets/PetRepository.h
    src/game/persistence/characters/pets/PetRepository.cpp
    src/game/persistence/characters/social/GroupRepository.h
    src/game/persistence/characters/social/GroupRepository.cpp
    src/game/Object/ItemPersistence.cpp
    src/game/persistence/characters/inventory/ItemRepository.h
    src/game/persistence/characters/inventory/ItemRepository.cpp
    src/game/persistence/characters/mail/MailRepository.h
    src/game/persistence/characters/mail/MailRepository.cpp
    src/game/persistence/characters/social/CalendarRepository.h
    src/game/persistence/characters/social/CalendarRepository.cpp
    src/game/persistence/characters/social/TicketRepository.h
    src/game/persistence/characters/social/TicketRepository.cpp
    src/game/persistence/characters/economy/AuctionRepository.h
    src/game/persistence/characters/economy/AuctionRepository.cpp
    src/game/persistence/login/AccountRepository.h
    src/game/persistence/login/AccountRepository.cpp
    src/game/persistence/characters/social/GuildRepository.h
    src/game/persistence/characters/social/GuildRepository.cpp
    src/game/persistence/characters/social/GuildBankRepository.h
    src/game/persistence/characters/social/GuildBankRepository.cpp
    src/game/persistence/characters/social/PetitionRepository.h
    src/game/persistence/characters/social/PetitionRepository.cpp
    src/game/persistence/characters/CharacterRepository.h
    src/game/persistence/characters/CharacterRepository.cpp
    src/game/persistence/characters/CharacterDeleteRepository.h
    src/game/persistence/characters/CharacterDeleteRepository.cpp
    src/game/persistence/characters/CharacterSaveRepository.h
    src/game/persistence/characters/CharacterSaveRepository.cpp
    src/game/persistence/characters/maps/InstanceBindRepository.h
    src/game/persistence/characters/maps/InstanceBindRepository.cpp
    src/game/persistence/characters/achievements/AchievementRepository.h
    src/game/persistence/characters/achievements/AchievementRepository.cpp
    src/game/persistence/world/achievements/AchievementDataRepository.h
    src/game/persistence/world/achievements/AchievementDataRepository.cpp
    src/game/persistence/world/spawns/SpawnRepository.h
    src/game/persistence/world/spawns/SpawnRepository.cpp
    src/game/persistence/characters/maps/CorpseRepository.h
    src/game/persistence/characters/maps/CorpseRepository.cpp
    src/game/persistence/characters/maps/InstanceRepository.h
    src/game/persistence/characters/maps/InstanceRepository.cpp
    src/game/persistence/characters/account/AccountDataRepository.h
    src/game/persistence/characters/account/AccountDataRepository.cpp
    src/game/persistence/login/RealmRepository.h
    src/game/persistence/login/RealmRepository.cpp
    src/game/persistence/characters/quests/QuestTrackerRepository.h
    src/game/persistence/characters/quests/QuestTrackerRepository.cpp
    src/game/persistence/characters/maps/InstanceDataRepository.h
    src/game/persistence/characters/maps/InstanceDataRepository.cpp
    src/game/persistence/characters/pvp/BattleGroundStatsRepository.h
    src/game/persistence/characters/pvp/BattleGroundStatsRepository.cpp
    src/game/persistence/characters/SavedVariablesRepository.h
    src/game/persistence/characters/SavedVariablesRepository.cpp
    src/game/persistence/world/spells/SpellDataRepository.h
    src/game/persistence/world/spells/SpellDataRepository.cpp
    src/game/persistence/world/ai/CreatureEventAIRepository.h
    src/game/persistence/world/ai/CreatureEventAIRepository.cpp
    src/game/persistence/world/economy/LootDataRepository.h
    src/game/persistence/world/economy/LootDataRepository.cpp
    src/game/persistence/world/economy/SkillDataRepository.h
    src/game/persistence/world/economy/SkillDataRepository.cpp
    src/game/persistence/world/maps/MapDataRepository.h
    src/game/persistence/world/maps/MapDataRepository.cpp
    src/game/persistence/world/pvp/BattleGroundDataRepository.h
    src/game/persistence/world/pvp/BattleGroundDataRepository.cpp
    src/game/spells/data/SpellMgr.cpp
    src/game/spells/data/SpellMgrAreas.cpp
    src/game/spells/data/SpellMgrChains.cpp
    src/game/spells/data/SpellMgrData.cpp
    src/game/spells/data/SpellMgrPet.cpp
    src/game/spells/data/SpellMgrProc.cpp
    src/game/Object/CreatureEventAIMgr.cpp
    src/game/Object/LootMgr.cpp
    src/game/Object/ItemEnchantmentMgr.cpp
    src/game/WorldHandlers/SkillDiscovery.cpp
    src/game/WorldHandlers/SkillExtraItems.cpp
    src/game/WorldHandlers/Transports.cpp
    src/game/WorldHandlers/Weather.cpp
    src/game/BattleGround/BattleGroundMgrUtil.cpp
    src/game/WorldHandlers/NPCHandler.cpp
    src/game/spells/pipeline/SpellChecks.cpp
    src/game/Object/Guild.cpp
    src/game/Object/GuildRank.cpp
    src/game/entities/player/pvp/PlayerBattleGround.cpp
    src/game/WorldHandlers/CalendarHandler.cpp
    src/game/Object/Calendar.cpp
    src/game/WorldHandlers/Mail.cpp
    src/game/WorldHandlers/MailHandler.cpp
    src/game/WorldHandlers/SpellHandler.cpp
    src/game/Object/ArenaTeam.cpp
    src/game/persistence/characters/social/ArenaTeamRepository.h
    src/game/persistence/characters/social/ArenaTeamRepository.cpp
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
    src/game/WorldHandlers/MiscHandler.cpp
    src/game/entities/player/social/SocialMgr.cpp
    src/game/Object/GMTicketMgr.cpp
    src/game/Object/AuctionHouseMgr.cpp
    src/game/WorldHandlers/AccountMgr.cpp
    src/game/ChatCommands/AccountCommands.cpp
    src/game/entities/player/quests/QuestStatusMgr.h
    src/game/entities/player/quests/QuestStatusMgr.cpp
    src/game/persistence/characters/quests/QuestStatusRepository.h
    src/game/persistence/characters/quests/QuestStatusRepository.cpp
    src/game/entities/player/talents/TalentMgr.h
    src/game/entities/player/talents/TalentMgr.cpp
    src/game/persistence/characters/talents/TalentRepository.h
    src/game/persistence/characters/talents/TalentRepository.cpp
    src/game/entities/player/inventory/InventoryMgr.h
    src/game/entities/player/inventory/InventoryMgr.cpp
    src/game/entities/player/spells/RuneMgr.h
    src/game/entities/player/spells/RuneMgr.cpp
    src/game/spells/SpellCooldownMgr.h
    src/game/spells/SpellCooldownMgr.cpp
    src/game/persistence/characters/spells/SpellCooldownRepository.h
    src/game/persistence/characters/spells/SpellCooldownRepository.cpp
    src/game/entities/player/talents/GlyphMgr.h
    src/game/entities/player/talents/GlyphMgr.cpp
    src/game/persistence/characters/talents/GlyphRepository.h
    src/game/persistence/characters/talents/GlyphRepository.cpp
    src/game/entities/player/pets/PetMgr.h
    src/game/entities/player/pets/PetMgr.cpp
    src/game/entities/player/social/SocialList.h
    src/game/entities/player/social/SocialList.cpp
    src/game/persistence/characters/social/SocialRepository.h
    src/game/persistence/characters/social/SocialRepository.cpp
    src/game/entities/player/inventory/CurrencyMgr.h
    src/game/entities/player/inventory/CurrencyMgr.cpp
    src/game/persistence/characters/inventory/CurrencyRepository.h
    src/game/persistence/characters/inventory/CurrencyRepository.cpp
    src/game/entities/player/social/ReputationMgr.h
    src/game/entities/player/social/ReputationMgr.cpp
    src/game/persistence/characters/social/ReputationRepository.h
    src/game/persistence/characters/social/ReputationRepository.cpp
    src/game/entities/player/pvp/HonorMgr.h
    src/game/entities/player/pvp/HonorMgr.cpp
    src/game/session/handlers/entities/PetHandlers.cpp
    src/game/session/handlers/social/ContactHandlers.cpp
    src/game/session/handlers/social/PetitionHandlers.cpp
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

set(ALLOW_GuildRepository_cpp
    "return CharacterDatabase.Query(\"SELECT `guild`.`guildid`,`guild`.`name`,`leaderguid`,`EmblemStyle`,`EmblemColor`,`BorderStyle`,`BorderColor`,\""
    "return CharacterDatabase.Query(\"SELECT `guildid`,`rid`,`rname`,`rights`,`BankMoneyPerDay` FROM `guild_rank` ORDER BY `guildid` ASC, `rid` ASC\")\;"
    "return CharacterDatabase.Query(\"SELECT `guildid`,`guild_member`.`guid`,`rank`,`pnote`,`offnote`,`BankResetTimeMoney`,`BankRemMoney`,\""
    "return CharacterDatabase.Query(\"SELECT `guildid`,`TabId`,`rid`,`gbright`,`SlotPerDay` FROM `guild_bank_right` ORDER BY `guildid` ASC, `TabId` ASC\")\;"
    "return CharacterDatabase.PQuery(\"SELECT `LogGuid`, `EventType`, `PlayerGuid1`, `PlayerGuid2`, `NewRank`, `TimeStamp` FROM `guild_eventlog` WHERE `guildid`=%u ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u\", guildId, maxRecords)\;")

set(ALLOW_CalendarRepository_cpp
    "return CharacterDatabase.Query(\"SELECT `eventId`, `creatorGuid`, `guildId`, `type`, `flags`, `dungeonId`, `eventTime`, `title`, `description` FROM `calendar_events` ORDER BY `eventId`\")\;"
    "return CharacterDatabase.Query(\"SELECT `inviteId`, `eventId`, `inviteeGuid`, `senderGuid`, `status`, `lastUpdateTime`, `rank` FROM `calendar_invites` ORDER BY `inviteId`\")\;"
    "CharacterDatabase.DirectExecute(\"TRUNCATE TABLE calendar_events\")\;"
    "CharacterDatabase.DirectExecute(\"TRUNCATE TABLE calendar_invites\")\;")

set(ALLOW_GuildBankRepository_cpp
    "return CharacterDatabase.PQuery(\"SELECT `TabId`, `TabName`, `TabIcon`, `TabText` FROM `guild_bank_tab` WHERE `guildid`='%u' ORDER BY `TabId`\", guildId)\;"
    "return CharacterDatabase.PQuery(\"SELECT `data`, `text`, `TabId`, `SlotId`, `item_guid`, `item_entry` FROM `guild_bank_item` JOIN `item_instance` ON `item_guid` = `guid` WHERE `guildid`='%u' ORDER BY `TabId`\", guildId)\;"
    "return CharacterDatabase.PQuery(\"SELECT `LogGuid`, `EventType`, `PlayerGuid`, `ItemOrMoney`, `ItemStackCount`, `DestTabId`, `TimeStamp` FROM `guild_bank_eventlog` WHERE `guildid`='%u' AND `TabId`='%u' ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u\", guildId, tabId, maxLogs)\;")

set(ALLOW_Chat_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `command_text`,`security`,`help_text` FROM `command`\")\;")

set(ALLOW_InstanceDataRepository_cpp
    "return CharacterDatabase.Query(\"SELECT `id`, `map`, `data` FROM `instance`\")\;"
    "return CharacterDatabase.Query(\"SELECT `map`, `data` FROM `world`\")\;")

set(ALLOW_InstanceRepository_cpp
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT %s FROM %s %s\", fields, table, szQueryTail)\;"
    "CharacterDatabase.escape_string(fieldValue)\;"
    "CharacterDatabase.DirectExecute(\"DELETE FROM `creature_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())\")\;"
    "CharacterDatabase.DirectExecute(\"DELETE FROM `gameobject_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())\")\;"
    "return CharacterDatabase.Query(\"SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `creature_respawn` LEFT JOIN `instance` ON `instance` = `id`\")\;"
    "return CharacterDatabase.Query(\"SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `gameobject_respawn` LEFT JOIN `instance` ON `instance` = `id`\")\;"
    "CharacterDatabase.DirectPExecute(\"UPDATE `instance` SET `resettime` = '\" UI64FMTD \"' WHERE `id` = '%u'\", resetTime, instanceId)\;"
    "return CharacterDatabase.Query(\"SELECT `id`, `map`, `difficulty`, `resettime` FROM `instance` WHERE `resettime` > 0\")\;"
    "return CharacterDatabase.Query(\"SELECT MAX(`respawntime`), `instance` FROM `creature_respawn` WHERE `instance` > 0 GROUP BY `instance`\")\;"
    "return CharacterDatabase.Query(\"SELECT `id` FROM `instance`\")\;"
    "return CharacterDatabase.Query(\"SELECT `mapid`, `difficulty`, `resettime` FROM `instance_reset`\")\;"
    "CharacterDatabase.DirectPExecute(\"UPDATE `instance_reset` SET `resettime` = '\" UI64FMTD \"' WHERE `mapid` = '%u' AND `difficulty` = '%u'\", resetTime, map, difficulty)\;"
    "CharacterDatabase.DirectPExecute(\"UPDATE `instance_reset` SET `resettime` = '\" UI64FMTD \"' WHERE mapid = '%u' AND difficulty= '%u'\", resetTime, map, difficulty)\;"
    "CharacterDatabase.DirectPExecute(\"INSERT INTO `instance_reset` VALUES ('%u','%u','\" UI64FMTD \"')\", map, difficulty, resetTime)\;"
    "CharacterDatabase.DirectPExecute(\"DELETE FROM `instance_reset` WHERE `mapid` = '%u' AND `difficulty` = '%u'\", map, difficulty)\;")

set(ALLOW_SpellDataRepository_cpp
    "return WorldDatabase.PQuery(\"SELECT `spellid`,`SpellFamilyName`,`SpellFamilyMaskA`,`SpellFamilyMaskB`,`SpellIcon`,`SpellVisual`,`SpellCategory`,`EffectType`,`EffectAura`,`EffectIdx`,`Name`,`Code` FROM `%s`\", table)\;"
    "return WorldDatabase.Query(\"SELECT `spell`, `area`, `quest_start`, `quest_start_active`, `quest_end`, `condition_id`, `aura_spell`, `racemask`, `gender`, `autocast` FROM `spell_area`\")\;"
    "return WorldDatabase.Query(\"SELECT `spell_id`, `prev_spell`, `first_spell`, `rank`, `req_spell` FROM `spell_chain`\")\;"
    "return WorldDatabase.Query(\"SELECT `entry`, `SpellID`, `Active` FROM `spell_learn_spell`\")\;"
    "return WorldDatabase.Query(\"SELECT `id`, `target_map`, `target_position_x`, `target_position_y`, `target_position_z`, `target_orientation` FROM `spell_target_position`\")\;"
    "return WorldDatabase.Query(\"SELECT entry, direct_bonus,         `dot_bonus`, `ap_bonus`, `ap_dot_bonus` FROM `spell_bonus_data`\")\;"
    "return WorldDatabase.Query(\"SELECT `entry`, `mask` FROM `spell_elixir`\")\;"
    "return WorldDatabase.Query(\"SELECT `entry`, `Threat`, `multiplier`, `ap_bonus` FROM `spell_threat`\")\;"
    "return WorldDatabase.Query(\"SELECT `spell`, `effectId`, `pet`, `aura` FROM `spell_pet_auras`\")\;"
    "return WorldDatabase.Query(\"SELECT `entry`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMaskA0`, `SpellFamilyMaskA1`, `SpellFamilyMaskA2`, `SpellFamilyMaskB0`, `SpellFamilyMaskB1`, `SpellFamilyMaskB2`, `SpellFamilyMaskC0`, `SpellFamilyMaskC1`, `SpellFamilyMaskC2`, `procFlags`, `procEx`, `ppmRate`, `CustomChance`, `Cooldown` FROM `spell_proc_event`\")\;"
    "return WorldDatabase.Query(\"SELECT `entry`, `ppmRate` FROM `spell_proc_item_enchant`\")\;")

set(ALLOW_CreatureEventAIRepository_cpp
    "return WorldDatabase.Query(\"SELECT `id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs` FROM `creature_ai_summons`\")\;"
    "return WorldDatabase.Query(\"SELECT `id`, `creature_id`, `event_type`, `event_inverse_phase_mask`, `event_chance`, `event_flags`, \"")

set(ALLOW_LootDataRepository_cpp
    "return WorldDatabase.PQuery(\"SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount`, `condition_id` FROM `%s`\", table)\;"
    "return WorldDatabase.Query(\"SELECT `entry`, `ench`, `chance` FROM `item_enchantment_template`\")\;")

set(ALLOW_SkillDataRepository_cpp
    "return WorldDatabase.Query(\"SELECT `spellId`, `reqSpell`, `reqSkillValue`, `chance` FROM `skill_discovery_template`\")\;"
    "return WorldDatabase.Query(\"SELECT `spellId`, `requiredSpecialization`, `additionalCreateChance`, `additionalMaxNum` FROM `skill_extra_item_template`\")\;")

set(ALLOW_MapDataRepository_cpp
    "return WorldDatabase.Query(\"SELECT `entry`, `name`, `period` FROM `transports`\")\;"
    "return WorldDatabase.Query(\"SELECT `gameobject`.`guid`,`gameobject`.`id`,`transports`.`name` FROM `gameobject`,`transports` WHERE `gameobject`.`id` = `transports`.`entry`\")\;"
    "return WorldDatabase.Query(\"SELECT `zone`, `spring_rain_chance`, `spring_snow_chance`, `spring_storm_chance`, `summer_rain_chance`, `summer_snow_chance`, `summer_storm_chance`, `fall_rain_chance`, `fall_snow_chance`, `fall_storm_chance`, `winter_rain_chance`, `winter_snow_chance`, `winter_storm_chance` FROM `game_weather`\")\;")

set(ALLOW_BattleGroundDataRepository_cpp
    "return WorldDatabase.Query(\"SELECT `id`, `MinPlayersPerTeam`,`MaxPlayersPerTeam`,`AllianceStartLoc`,`AllianceStartO`,`HordeStartLoc`,`HordeStartO`, `StartMaxDist` FROM `battleground_template`\")\;"
    "return WorldDatabase.Query(\"SELECT `entry`,`bg_template` FROM `battlemaster_entry`\")\;"
    "return WorldDatabase.Query(\"SELECT `data`.`typ`, `data`.`guid1`, `data`.`ev1` AS `ev1`, `data`.`ev2` AS ev2, `data`.`map` AS m, `data`.`guid2`, `description`.`map`, \"")

set(ALLOW_BattleGroundStatsRepository_cpp
    "return CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `pvpstats_battlegrounds`\")\;")

set(ALLOW_SavedVariablesRepository_cpp
    "return CharacterDatabase.Query(\"SELECT `NextArenaPointDistributionTime` FROM `saved_variables`\")\;")

set(ALLOW_TicketRepository_cpp
    "if (QueryResult* highest = CharacterDatabase.Query(\"SELECT MAX(`ticket_id`) FROM `character_ticket`\"))"
    "if (QueryResult* next = CharacterDatabase.Query(\"SELECT `AUTO_INCREMENT` FROM `information_schema`.`TABLES` WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'character_ticket'\"))"
    "return CharacterDatabase.Query(\"SELECT `guid`, `ticket_text`, `response_text`, UNIX_TIMESTAMP(`ticket_lastchange`), `ticket_id` FROM `character_ticket` WHERE `resolved` = 0 ORDER BY `ticket_id` ASC\")\;")

set(ALLOW_AuctionRepository_cpp
    "return CharacterDatabase.Query(\"SELECT `data`,`text`,`itemguid`,`item_template` FROM `auction` JOIN `item_instance` ON `itemguid` = `guid`\")\;"
    "return CharacterDatabase.Query(\"SELECT COUNT(*) FROM `auction`\")\;"
    "return CharacterDatabase.Query(\"SELECT `id`,`houseid`,`itemguid`,`item_template`,`item_count`,`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`moneyTime`,`buyguid`,`lastbid`,`startbid`,`deposit` FROM `auction`\")\;")

set(ALLOW_AchievementRepository_cpp
    "return CharacterDatabase.Query(\"SELECT `achievement` FROM `character_achievement` GROUP BY `achievement`\")\;"
    "return CharacterDatabase.Query(\"SELECT DISTINCT `criteria` FROM `character_achievement_progress`\")\;")

set(ALLOW_AchievementDataRepository_cpp
    "return WorldDatabase.Query(\"SELECT `criteria_id`, `type`, `value1`, `value2` FROM `achievement_criteria_requirement`\")\;"
    "return WorldDatabase.Query(\"SELECT `entry`, `gender`, `title_A`, `title_H`, `item`, `sender`, `subject`, `text` FROM `achievement_reward`\")\;"
    "return WorldDatabase.Query(\"SELECT `entry`,`gender`,\"")

set(ALLOW_AccountDataRepository_cpp
    "return CharacterDatabase.PQuery(\"SELECT `type`, `time`, `data` FROM `account_data` WHERE `account` = '%u'\", accountId)\;"
    "return CharacterDatabase.PQuery(\"SELECT `tut0`,`tut1`,`tut2`,`tut3`,`tut4`,`tut5`,`tut6`,`tut7` FROM `character_tutorial` WHERE `account` = '%u'\", accountId)\;")

set(ALLOW_AccountRepository_cpp
    "return LoginDatabase.PQuery(\"SELECT 1 FROM `account` WHERE `id`='%u'\", accountId)\;"
    "LoginDatabase.escape_string(safe_new_uname)\;"
    "LoginDatabase.escape_string(safeName)\;"
    "return LoginDatabase.PQuery(\"SELECT `id` FROM `account` WHERE `username` = '%s'\", safeName.c_str())\;"
    "return LoginDatabase.PQuery(\"SELECT `gmlevel` FROM `account` WHERE `id` = '%u'\", accountId)\;"
    "return LoginDatabase.PQuery(\"SELECT `username` FROM `account` WHERE `id` = '%u'\", accountId)\;"
    "LoginDatabase.escape_string(safeAccount)\;"
    "return LoginDatabase.PQuery(\"SELECT \""
    "return LoginDatabase.PQuery(\"SELECT 1 FROM `account_banned` WHERE `id` = %u AND `active` = 1 \"")

set(ALLOW_CharacterRepository_cpp
    "return CharacterDatabase.PQuery(\"SELECT `guid` FROM `characters` WHERE `account`='%u'\", accountId)\;"
    "return CharacterDatabase.PQuery(\"SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'\", accountId)\;"
    "return CharacterDatabase.PQuery(\"SELECT `position_x`,`position_y`,`position_z`,`orientation`,`map`,`taxi_path` FROM `characters` WHERE `guid` = '%u'\", guid)\;")

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
