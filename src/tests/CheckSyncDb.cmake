# Checks that no file in CONVERTED_FILES makes a blocking database call its ALLOW_ list does not hold, and
# that only ADMIN_SCOPE_FILES construct a TickGuard::AdminScope; reads those files and the sources under src/.
cmake_minimum_required(VERSION 3.18)

include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(SyncDb)

set(CONVERTED_FILES
    src/game/session/handlers/social/PetitionsHandler.cpp
    src/game/entities/player/persistence/PlayerDbLookup.cpp
    src/game/data/world/ObjectMgr.cpp
    src/game/session/handlers/entities/CharacterHandler.cpp
    src/game/session/handlers/entities/CharacterCustomizeHandlers.cpp
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
    src/game/persistence/DatabaseLifecycle.h
    src/game/persistence/DatabaseLifecycle.cpp
    src/game/persistence/world/DbVersionRepository.h
    src/game/persistence/world/DbVersionRepository.cpp
    src/game/persistence/world/BroadcastDataRepository.h
    src/game/persistence/world/BroadcastDataRepository.cpp
    src/game/app/World.cpp
    src/game/app/WorldConfig.cpp
    src/game/AuctionHouseBot/AuctionHouseBot.cpp
    src/game/persistence/world/CommandDataRepository.h
    src/game/persistence/world/CommandDataRepository.cpp
    src/game/persistence/world/ScriptDataRepository.h
    src/game/persistence/world/ScriptDataRepository.cpp
    src/game/persistence/DatabaseStatus.h
    src/game/persistence/DatabaseStatus.cpp
    src/game/ChatCommands/CreatureCommands.cpp
    src/game/ChatCommands/GameObjectCommands.cpp
    src/game/ChatCommands/TeleportationAndPositionCommands.cpp
    src/game/ChatCommands/ServerCommands.cpp
    src/game/ChatCommands/GMCommands.cpp
    src/game/ChatCommands/WaypointCommands.cpp
    src/game/ChatCommands/CommandMgr.cpp
    src/game/dbscripts/ScriptMgrBinding.cpp
    src/game/dbscripts/ScriptMgrDbScripts.cpp
    src/game/ChatCommands/ListCommands.cpp
    src/game/ChatCommands/LookupCommands.cpp
    src/game/ChatCommands/PlayerCommands.cpp
    src/game/ChatCommands/PlayerMiscCommands.cpp
    src/game/ChatCommands/PlayerStateCommands.cpp
    src/game/persistence/login/BanRepository.h
    src/game/persistence/login/BanRepository.cpp
    src/game/ChatCommands/BanAndKickCommands.cpp
    src/game/ChatCommands/CommunicationCommands.cpp
    src/game/persistence/world/events/GameEventDataRepository.h
    src/game/persistence/world/events/GameEventDataRepository.cpp
    src/game/persistence/characters/events/GameEventStatusRepository.h
    src/game/persistence/characters/events/GameEventStatusRepository.cpp
    src/game/persistence/world/waypoints/WaypointRepository.h
    src/game/persistence/world/waypoints/WaypointRepository.cpp
    src/game/persistence/world/pools/PoolDataRepository.h
    src/game/persistence/world/pools/PoolDataRepository.cpp
    src/game/persistence/world/DisableDataRepository.h
    src/game/persistence/world/DisableDataRepository.cpp
    src/game/data/world/GameEventMgr.cpp
    src/game/data/world/WaypointManager.cpp
    src/game/data/world/PoolManager.cpp
    src/game/data/world/CreatureLinkingMgr.cpp
    src/game/data/world/DisableMgr.cpp
    src/game/persistence/world/creatures/CreatureDataRepository.h
    src/game/persistence/world/creatures/CreatureDataRepository.cpp
    src/game/persistence/world/items/ItemDataRepository.h
    src/game/persistence/world/items/ItemDataRepository.cpp
    src/game/persistence/world/locales/LocaleDataRepository.h
    src/game/persistence/world/locales/LocaleDataRepository.cpp
    src/game/persistence/world/maps/DungeonDataRepository.h
    src/game/persistence/world/maps/DungeonDataRepository.cpp
    src/game/persistence/world/npcs/NpcDataRepository.h
    src/game/persistence/world/npcs/NpcDataRepository.cpp
    src/game/persistence/world/players/PlayerDataRepository.h
    src/game/persistence/world/players/PlayerDataRepository.cpp
    src/game/persistence/world/quests/QuestDataRepository.h
    src/game/persistence/world/quests/QuestDataRepository.cpp
    src/game/persistence/world/reputation/ReputationDataRepository.h
    src/game/persistence/world/reputation/ReputationDataRepository.cpp
    src/game/data/world/ObjectMgrAreaTrigger.cpp
    src/game/data/world/ObjectMgrCreatures.cpp
    src/game/data/world/ObjectMgrDungeonFinder.cpp
    src/game/data/world/ObjectMgrGameObjects.cpp
    src/game/data/world/ObjectMgrGameTele.cpp
    src/game/data/world/ObjectMgrGossipMenu.cpp
    src/game/data/world/ObjectMgrGraveyard.cpp
    src/game/data/world/ObjectMgrInstanceData.cpp
    src/game/social/ArenaTeamRegistry.cpp
    src/game/social/GroupRegistry.cpp
    src/game/data/world/ObjectMgrItems.cpp
    src/game/data/world/ObjectMgrLocales.cpp
    src/game/data/world/ObjectMgrNameCheck.cpp
    src/game/data/world/ObjectMgrPlayerInfo.cpp
    src/game/data/world/ObjectMgrQuestRelations.cpp
    src/game/data/world/ObjectMgrQuests.cpp
    src/game/data/world/ObjectMgrReputation.cpp
    src/game/data/world/ObjectMgrSpellData.cpp
    src/game/data/world/ObjectMgrText.cpp
    src/game/data/world/ObjectMgrTrainerVendor.cpp
    src/game/data/CharacterCache.cpp
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
    src/game/data/world/ItemEnchantmentMgr.cpp
    src/game/WorldHandlers/SkillDiscovery.cpp
    src/game/WorldHandlers/SkillExtraItems.cpp
    src/game/WorldHandlers/Transports.cpp
    src/game/WorldHandlers/Weather.cpp
    src/game/BattleGround/BattleGroundMgrUtil.cpp
    src/game/session/handlers/entities/NPCHandler.cpp
    src/game/spells/pipeline/SpellChecks.cpp
    src/game/Object/Guild.cpp
    src/game/Object/GuildRank.cpp
    src/game/entities/player/pvp/PlayerBattleGround.cpp
    src/game/session/handlers/social/CalendarHandler.cpp
    src/game/Object/Calendar.cpp
    src/game/WorldHandlers/Mail.cpp
    src/game/session/handlers/social/MailHandlers.cpp
    src/game/session/handlers/spells/SpellHandlers.cpp
    src/game/Object/ArenaTeam.cpp
    src/game/persistence/characters/social/ArenaTeamRepository.h
    src/game/persistence/characters/social/ArenaTeamRepository.cpp
    src/game/Object/GuildBank.cpp
    src/game/ChatCommands/Chat.cpp
    src/game/Harness/Harness.cpp
    src/game/WorldHandlers/Map.cpp
    src/game/WorldHandlers/InstanceData.cpp
    src/game/WorldHandlers/InstanceDataCache.cpp
    src/game/WorldHandlers/MapPersistentStateMgr.cpp
    src/game/BattleGround/BattleGroundReward.cpp
    src/game/BattleGround/BattleGroundMgr.cpp
    src/game/session/handlers/entities/PetHandler.cpp
    src/game/session/handlers/maps/MapHandlers.cpp
    src/game/session/handlers/entities/DeathHandlers.cpp
    src/game/session/handlers/entities/PlayerHandlers.cpp
    src/game/session/handlers/social/WhoHandlers.cpp
    src/game/session/handlers/social/TicketHandlers.cpp
    src/game/session/handlers/pvp/PvpHandlers.cpp
    src/game/session/handlers/connection/ClientHandlers.cpp
    src/game/entities/player/social/SocialMgr.cpp
    src/game/Object/GMTicketMgr.cpp
    src/game/Object/AuctionHouseMgr.cpp
    src/game/session/AccountMgr.cpp
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
    src/game/session/handlers/entities/NpcHandlers.cpp
    src/game/session/handlers/social/CalendarHandlers.cpp
    src/game/session/handlers/entities/CharacterHandlers.cpp
)

set(ADMIN_SCOPE_FILES
    src/game/ChatCommands/Chat.cpp
    src/tests/TickGuardTest.cpp)

set(ALLOW_GuildRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guild`.`guildid`,`guild`.`name`,`leaderguid`,`EmblemStyle`,`EmblemColor`,`BorderStyle`,`BorderColor`,\""
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guildid`,`rid`,`rname`,`rights`,`BankMoneyPerDay` FROM `guild_rank` ORDER BY `guildid` ASC, `rid` ASC\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guildid`,`guild_member`.`guid`,`rank`,`pnote`,`offnote`,`BankResetTimeMoney`,`BankRemMoney`,\""
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guildid`,`TabId`,`rid`,`gbright`,`SlotPerDay` FROM `guild_bank_right` ORDER BY `guildid` ASC, `TabId` ASC\")\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `LogGuid`, `EventType`, `PlayerGuid1`, `PlayerGuid2`, `NewRank`, `TimeStamp` FROM `guild_eventlog` WHERE `guildid`=%u ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u\", guildId, maxRecords)\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`guildid`) FROM `guild`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guid`, `guildid`, `rank` FROM `guild_member`\")\;")

set(ALLOW_CalendarRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `eventId`, `creatorGuid`, `guildId`, `type`, `flags`, `dungeonId`, `eventTime`, `title`, `description` FROM `calendar_events` ORDER BY `eventId`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `inviteId`, `eventId`, `inviteeGuid`, `senderGuid`, `status`, `lastUpdateTime`, `rank` FROM `calendar_invites` ORDER BY `inviteId`\")\;"
    "CharacterDatabase.DirectExecute(\"TRUNCATE TABLE calendar_events\")\;"
    "CharacterDatabase.DirectExecute(\"TRUNCATE TABLE calendar_invites\")\;")

set(ALLOW_GuildBankRepository_cpp
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `TabId`, `TabName`, `TabIcon`, `TabText` FROM `guild_bank_tab` WHERE `guildid`='%u' ORDER BY `TabId`\", guildId)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `data`, `text`, `TabId`, `SlotId`, `item_guid`, `item_entry` FROM `guild_bank_item` JOIN `item_instance` ON `item_guid` = `guid` WHERE `guildid`='%u' ORDER BY `TabId`\", guildId)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `LogGuid`, `EventType`, `PlayerGuid`, `ItemOrMoney`, `ItemStackCount`, `DestTabId`, `TimeStamp` FROM `guild_bank_eventlog` WHERE `guildid`='%u' AND `TabId`='%u' ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u\", guildId, tabId, maxLogs)\;")

set(ALLOW_InstanceDataRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `id`, `map`, `data` FROM `instance`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `map`, `data` FROM `world`\")\;")

set(ALLOW_InstanceRepository_cpp
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT %s FROM %s %s\", fields, table, szQueryTail)\;"
    "CharacterDatabase.escape_string(fieldValue)\;"
    "CharacterDatabase.DirectExecute(\"DELETE FROM `creature_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())\")\;"
    "CharacterDatabase.DirectExecute(\"DELETE FROM `gameobject_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `creature_respawn` LEFT JOIN `instance` ON `instance` = `id`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `gameobject_respawn` LEFT JOIN `instance` ON `instance` = `id`\")\;"
    "CharacterDatabase.DirectPExecute(\"UPDATE `instance` SET `resettime` = '\" UI64FMTD \"' WHERE `id` = '%u'\", resetTime, instanceId)\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `id`, `map`, `difficulty`, `resettime` FROM `instance` WHERE `resettime` > 0\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`respawntime`), `instance` FROM `creature_respawn` WHERE `instance` > 0 GROUP BY `instance`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `id` FROM `instance`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `mapid`, `difficulty`, `resettime` FROM `instance_reset`\")\;"
    "CharacterDatabase.DirectPExecute(\"UPDATE `instance_reset` SET `resettime` = '\" UI64FMTD \"' WHERE `mapid` = '%u' AND `difficulty` = '%u'\", resetTime, map, difficulty)\;"
    "CharacterDatabase.DirectPExecute(\"UPDATE `instance_reset` SET `resettime` = '\" UI64FMTD \"' WHERE mapid = '%u' AND difficulty= '%u'\", resetTime, map, difficulty)\;"
    "CharacterDatabase.DirectPExecute(\"INSERT INTO `instance_reset` VALUES ('%u','%u','\" UI64FMTD \"')\", map, difficulty, resetTime)\;"
    "CharacterDatabase.DirectPExecute(\"DELETE FROM `instance_reset` WHERE `mapid` = '%u' AND `difficulty` = '%u'\", map, difficulty)\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `instance`\")\;")

set(ALLOW_SpellDataRepository_cpp
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `spellid`,`SpellFamilyName`,`SpellFamilyMaskA`,`SpellFamilyMaskB`,`SpellIcon`,`SpellVisual`,`SpellCategory`,`EffectType`,`EffectAura`,`EffectIdx`,`Name`,`Code` FROM `%s`\", table)\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `spell`, `area`, `quest_start`, `quest_start_active`, `quest_end`, `condition_id`, `aura_spell`, `racemask`, `gender`, `autocast` FROM `spell_area`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `spell_id`, `prev_spell`, `first_spell`, `rank`, `req_spell` FROM `spell_chain`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `SpellID`, `Active` FROM `spell_learn_spell`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `target_map`, `target_position_x`, `target_position_y`, `target_position_z`, `target_orientation` FROM `spell_target_position`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT entry, direct_bonus,         `dot_bonus`, `ap_bonus`, `ap_dot_bonus` FROM `spell_bonus_data`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `mask` FROM `spell_elixir`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `Threat`, `multiplier`, `ap_bonus` FROM `spell_threat`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `spell`, `effectId`, `pet`, `aura` FROM `spell_pet_auras`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMaskA0`, `SpellFamilyMaskA1`, `SpellFamilyMaskA2`, `SpellFamilyMaskB0`, `SpellFamilyMaskB1`, `SpellFamilyMaskB2`, `SpellFamilyMaskC0`, `SpellFamilyMaskC1`, `SpellFamilyMaskC2`, `procFlags`, `procEx`, `ppmRate`, `CustomChance`, `Cooldown` FROM `spell_proc_event`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `ppmRate` FROM `spell_proc_item_enchant`\")\;")

set(ALLOW_CreatureEventAIRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs` FROM `creature_ai_summons`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `creature_id`, `event_type`, `event_inverse_phase_mask`, `event_chance`, `event_flags`, \"")

set(ALLOW_LootDataRepository_cpp
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount`, `condition_id` FROM `%s`\", table)\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `ench`, `chance` FROM `item_enchantment_template`\")\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `item` FROM `creature_loot_template` UNION \"")

set(ALLOW_SkillDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `spellId`, `reqSpell`, `reqSkillValue`, `chance` FROM `skill_discovery_template`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `spellId`, `requiredSpecialization`, `additionalCreateChance`, `additionalMaxNum` FROM `skill_extra_item_template`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,`skill` FROM `skill_fishing_base_level`\")\;")

set(ALLOW_MapDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `name`, `period` FROM `transports`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `gameobject`.`guid`,`gameobject`.`id`,`transports`.`name` FROM `gameobject`,`transports` WHERE `gameobject`.`id` = `transports`.`entry`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `zone`, `spring_rain_chance`, `spring_snow_chance`, `spring_storm_chance`, `summer_rain_chance`, `summer_snow_chance`, `summer_storm_chance`, `fall_rain_chance`, `fall_snow_chance`, `fall_storm_chance`, `winter_rain_chance`, `winter_snow_chance`, `winter_storm_chance` FROM `game_weather`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, `name` FROM `game_tele`\")\;"
    "WorldDatabase.escape_string(safeName)\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`,`ghost_zone`,`faction` FROM `game_graveyard_zone`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `required_level`, `required_item`, `required_item2`, `heroic_key`, `heroic_key2`, `required_quest_done`, `required_quest_done_heroic`, `target_map`, `target_position_x`, `target_position_y`, `target_position_z`, `target_orientation` FROM `areatrigger_teleport`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id` FROM `areatrigger_tavern`\")\;")

set(ALLOW_BattleGroundDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `MinPlayersPerTeam`,`MaxPlayersPerTeam`,`AllianceStartLoc`,`AllianceStartO`,`HordeStartLoc`,`HordeStartO`, `StartMaxDist` FROM `battleground_template`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,`bg_template` FROM `battlemaster_entry`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `data`.`typ`, `data`.`guid1`, `data`.`ev1` AS `ev1`, `data`.`ev2` AS ev2, `data`.`map` AS m, `data`.`guid2`, `description`.`map`, \"")

set(ALLOW_BattleGroundStatsRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `pvpstats_battlegrounds`\")\;")

set(ALLOW_SavedVariablesRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `NextArenaPointDistributionTime` FROM `saved_variables`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `NextWeeklyQuestResetTime` FROM `saved_variables`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `NextDailyQuestResetTime` FROM `saved_variables`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `NextMonthlyQuestResetTime` FROM `saved_variables`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `NextCurrenciesResetTime` FROM `saved_variables`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `NextRandomBGResetTime` FROM `saved_variables`\")\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `cleaning_flags` FROM `saved_variables`\")\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `last_cleaned_dbc_build` FROM `saved_variables`\")\;")

set(ALLOW_TicketRepository_cpp
    "if (QueryResult* highest = CharacterDatabase.Query(\"SELECT MAX(`ticket_id`) FROM `character_ticket`\"))"
    "if (QueryResult* next = CharacterDatabase.Query(\"SELECT `AUTO_INCREMENT` FROM `information_schema`.`TABLES` WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'character_ticket'\"))"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guid`, `ticket_text`, `response_text`, UNIX_TIMESTAMP(`ticket_lastchange`), `ticket_id` FROM `character_ticket` WHERE `resolved` = 0 ORDER BY `ticket_id` ASC\")\;")

set(ALLOW_AuctionRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `data`,`text`,`itemguid`,`item_template` FROM `auction` JOIN `item_instance` ON `itemguid` = `guid`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT COUNT(*) FROM `auction`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `id`,`houseid`,`itemguid`,`item_template`,`item_count`,`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`moneyTime`,`buyguid`,`lastbid`,`startbid`,`deposit` FROM `auction`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `auction`\")\;")

set(ALLOW_AchievementRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `achievement` FROM `character_achievement` GROUP BY `achievement`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT DISTINCT `criteria` FROM `character_achievement_progress`\")\;")

set(ALLOW_AchievementDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `criteria_id`, `type`, `value1`, `value2` FROM `achievement_criteria_requirement`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `gender`, `title_A`, `title_H`, `item`, `sender`, `subject`, `text` FROM `achievement_reward`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,`gender`,\"")

set(ALLOW_AccountDataRepository_cpp
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `type`, `time`, `data` FROM `account_data` WHERE `account` = '%u'\", accountId)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `tut0`,`tut1`,`tut2`,`tut3`,`tut4`,`tut5`,`tut6`,`tut7` FROM `character_tutorial` WHERE `account` = '%u'\", accountId)\;")

set(ALLOW_AccountRepository_cpp
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT 1 FROM `account` WHERE `id`='%u'\", accountId)\;"
    "LoginDatabase.escape_string(safe_new_uname)\;"
    "LoginDatabase.escape_string(safeName)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id` FROM `account` WHERE `username` = '%s'\", safeName.c_str())\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `gmlevel` FROM `account` WHERE `id` = '%u'\", accountId)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `username` FROM `account` WHERE `id` = '%u'\", accountId)\;"
    "LoginDatabase.escape_string(safeAccount)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT \""
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT 1 FROM `account_banned` WHERE `id` = %u AND `active` = 1 \""
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `active_realm_id` = %u\", realmId)\;"
    "LoginDatabase.escape_string(email)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `email` \" _LIKE_ \" \" _CONCAT3_(\"'%%'\", \"'%s'\", \"'%%'\"), email.c_str())\;"
    "LoginDatabase.escape_string(ip)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `last_ip` \" _LIKE_ \" \" _CONCAT3_(\"'%%'\", \"'%s'\", \"'%%'\"), ip.c_str())\;"
    "LoginDatabase.escape_string(account)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `username` \" _LIKE_ \" \" _CONCAT3_(\"'%%'\", \"'%s'\", \"'%%'\"), account.c_str())\;"
    "LoginDatabase.escape_string(ip)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id`,`username` FROM `account` WHERE `last_ip` \" _LIKE_ \" \" _CONCAT3_(\"'%%'\", \"'%s'\", \"'%%'\"), ip.c_str())\;"
    "LoginDatabase.escape_string(account)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id`,`username` FROM `account` WHERE `username` \" _LIKE_ \" \" _CONCAT3_(\"'%%'\", \"'%s'\", \"'%%'\"), account.c_str())\;"
    "LoginDatabase.escape_string(email)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id`,`username` FROM `account` WHERE `email` \" _LIKE_ \" \" _CONCAT3_(\"'%%'\", \"'%s'\", \"'%%'\"), email.c_str())\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `username`,`gmlevel`,`last_ip`,`last_login` FROM `account` WHERE `id` = '%u'\", accountId)\;"
    "QueryResult* result = LoginDatabase.Query(\"SELECT `username`,`gmlevel` FROM `account` WHERE `gmlevel` > 0\")\;")

set(ALLOW_CharacterRepository_cpp
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `guid` FROM `characters` WHERE `account`='%u'\", accountId)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'\", accountId)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `position_x`,`position_y`,`position_z`,`orientation`,`map`,`taxi_path` FROM `characters` WHERE `guid` = '%u'\", guid)\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`guid`) FROM `characters`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guid`, `account`, `name`, `race`, `class`, `level`, `zone`, `map`, `position_x`, `position_y`, `position_z` FROM `characters` ORDER BY `guid`\")\;"
    "LoginDatabase.escape_string(filter)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `account` FROM `characters` WHERE `name` \" _LIKE_ \" \" _CONCAT3_(\"'%%'\", \"'%s'\", \"'%%'\"), filter.c_str())\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `guid`, `name`, `race`, `class`, `level` FROM `characters` WHERE `account` = %u\", accountId)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `guid`, `deleteInfos_Name`, `deleteInfos_Account`, `deleteDate` FROM `characters` WHERE `deleteDate` IS NOT NULL AND `guid` = %u\", guid)\;"
    "CharacterDatabase.escape_string(name)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `guid`, `deleteInfos_Name`, `deleteInfos_Account`, `deleteDate` FROM `characters` WHERE `deleteDate` IS NOT NULL AND `deleteInfos_Name` \" _LIKE_ \" \" _CONCAT3_(\"'%%'\", \"'%s'\", \"'%%'\"), name.c_str())\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `guid`, `deleteInfos_Name`, `deleteInfos_Account`, `deleteDate` FROM `characters` WHERE `deleteDate` IS NOT NULL\")\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `totaltime`, `level`, `money`, `account` FROM `characters` WHERE `guid` = '%u'\", guid)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `account` FROM `characters` WHERE `name` = '%s'\", name.c_str())\;")

set(ALLOW_PetRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `character_pet`\")\;")

set(ALLOW_GroupRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `groupId` FROM `groups`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`groupId`) FROM `groups`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `mainTank`, `mainAssistant`, `lootMethod`, `looterGuid`, `lootThreshold`, `icon1`, `icon2`, `icon3`, `icon4`, `icon5`, `icon6`, `icon7`, `icon8`, `groupType`, `difficulty`, `raiddifficulty`, `leaderGuid`, `groupId` FROM `groups`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `memberGuid`, `assistant`, `subgroup`, `groupId`, `roles` FROM `group_member` ORDER BY `groupId`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `group_instance`.`leaderGuid`, `map`, `instance`, `permanent`, `instance`.`difficulty`, `resettime`, \"")

set(ALLOW_ItemRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`guid`) FROM `item_instance`\")\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT COUNT(`item_template`) FROM `character_inventory` WHERE `item_template`='%u'\", itemId)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT COUNT(`item_template`) FROM `mail_items` WHERE `item_template`='%u'\", itemId)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT COUNT(`item_template`) FROM `auction` WHERE `item_template`='%u'\", itemId)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT COUNT(`item_entry`) FROM `guild_bank_item` WHERE `item_entry`='%u'\", itemId)\;"
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `ci`.`item`, `cibag`.`slot` AS bag, `ci`.`slot`, `ci`.`guid`, `characters`.`account`,`characters`.`name` \""
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT `mail_items`.`item_guid`, `mail`.`sender`, `mail`.`receiver`, `char_s`.`account`, `char_s`.`name`, `char_r`.`account`, `char_r`.`name` \""
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT  `auction`.`itemguid`, `auction`.`itemowner`, `characters`.`account`, `characters`.`name` \""
    "QueryResult* result = CharacterDatabase.PQuery(\"SELECT gi.item_guid, gi.guildid, guild.name \"")

set(ALLOW_MailRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`id`) FROM `mail`\")\;")

set(ALLOW_CharacterSaveRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`setguid`) FROM `character_equipmentsets`\")\;")

set(ALLOW_SpawnRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT MAX(`guid`) FROM `creature`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT MAX(`guid`) FROM `gameobject`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `creature`.`guid`, `creature`.`id`, `map`, `modelid`,\""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `gameobject`.`guid`, `gameobject`.`id`, `gameobject`.`map`, `gameobject`.`position_x`, \""
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `id`, `map` FROM `creature` WHERE `guid` = '%u'\", guid)\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `guid` FROM `creature` WHERE `id`=%u AND `map`=%u LIMIT 2\", entry, mapId)\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT COUNT(`guid`) FROM `gameobject` WHERE `id`='%u'\", entry)\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `guid`, `position_x`, `position_y`, `position_z`, `map`, (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) AS order_ FROM `gameobject` WHERE `id` = '%u' ORDER BY `order_` ASC LIMIT %u\","
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `guid`, `position_x`, `position_y`, `position_z`, `map` FROM `gameobject` WHERE `id` = '%u' LIMIT %u\","
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT COUNT(`guid`) FROM `creature` WHERE `id`='%u'\", entry)\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `guid`, `position_x`, `position_y`, `position_z`, `map`, (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) AS order_ FROM `creature` WHERE `id` = '%u' ORDER BY `order_` ASC LIMIT %u\","
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `guid`, `position_x`, `position_y`, `position_z`, `map` FROM `creature` WHERE `id` = '%u' LIMIT %u\","
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `map`, \""
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) AS order_ FROM `gameobject` WHERE `map` = '%i' AND `id` = '%u' ORDER BY order_ ASC LIMIT 1\","
    "WorldDatabase.escape_string(name)\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - %f, 2) + POW(`position_y` - %f, 2) + POW(`position_z` - %f, 2)) AS order_ \""
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `gameobject`.`guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, \""
    "WorldDatabase.escape_string(name)\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `guid` FROM `creature`, `creature_template` WHERE `creature`.`id` = `creature_template`.`entry` AND `creature_template`.`name` \" _LIKE_ \" \" _CONCAT3_(\"'%%'\", \"'%s'\", \"'%%'\"), name.c_str())\;"
    "WorldDatabase.escape_string(name)\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `guid` FROM `gameobject`, `gameobject_template` WHERE `gameobject`.`id` = `gameobject_template`.`entry` AND `gameobject_template`.`name `\" _LIKE_ \" \" _CONCAT3_(\"'%%'\", \"'%s'\", \"'%%'\"), name.c_str())\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT COUNT(`id`) FROM `creature` WHERE `id` = %u\", entry)\;")

set(ALLOW_CorpseRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`guid`) FROM `corpse`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `corpse`.`guid`, `player`, `corpse`.`position_x`, `corpse`.`position_y`, `corpse`.`position_z`, `corpse`.`orientation`, `corpse`.`map`, \"")

set(ALLOW_ArenaTeamRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT MAX(`arenateamid`) FROM `arena_team`\")\;"
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `arena_team`.`arenateamid`,`name`,`captainguid`,`type`,`BackgroundColor`,`EmblemStyle`,\""
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `arenateamid`,`member`.`guid`,`played_week`,`wons_week`,`played_season`,`wons_season`,`personal_rating`,`name`,`class` \""
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `arena_team_member`.`guid`, `arena_team_member`.`arenateamid`, `arena_team`.`type` \"")

set(ALLOW_CreatureDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(queryStr.c_str())\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `modelid`, `racemask`, `creature_entry`, `modelid_racial` FROM `creature_model_race`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `word`,`entry`,`half` FROM `pet_name_generation`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `creature_entry`, `level`, `hp`, `mana`, `str`, `agi`, `sta`, `inte`, `spi`, `armor` FROM `pet_levelstats`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `map`, `master_entry`, `flag`, `search_range` FROM `creature_linking_template`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `guid`, `master_guid`, `flag` FROM `creature_linking`\")\;")

set(ALLOW_ItemDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,\""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,`item` FROM `item_convert`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,`item` FROM `item_expire_convert`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,`type`,`targetEntry` FROM `item_required_target`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT entry, type, UNIX_TIMESTAMP(hotfixDate) FROM hotfix_data\")\;"
    "WorldDatabase.escape_string(itemName)\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `entry` FROM `item_template` WHERE `name` = '%s'\", itemName.c_str())\;")

set(ALLOW_LocaleDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,\""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `menu_id`,`id`,\""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,\""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,\""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,\""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,\"")

set(ALLOW_DungeonDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `creditType`, `creditEntry`, `lastEncounterDungeon` FROM `instance_encounters`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `mapId`, `difficulty`, `min_item_level`, `item`, `item_2`, `alliance_quest`, `horde_quest`, `achievement`, `quest_incomplete_text` FROM `dungeonfinder_requirements`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `level`, `base_xp_reward`, `base_monetary_reward` FROM `dungeonfinder_rewards`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `min_level`, `max_level`, `item_reward`, `item_amount`, `dungeon_type` FROM `dungeonfinder_item_rewards`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `dungeonId`, `position_x`, `position_y`, `position_z`, `orientation` FROM `lfg_dungeon_entrances`\")\;")

set(ALLOW_NpcDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `text_id`, `script_id`, \""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `menu_id`, `id`, `option_icon`, `option_text`, `option_id`, `npc_option_npcflag`, \""
    "QueryResult* result = WorldDatabase.Query(\"SELECT * FROM `npc_text`\")\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `entry`, `spell`,`spellcost`,`reqskill`,`reqskillvalue`,`reqlevel` FROM `%s`\", table)\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `entry`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `condition_id` FROM `%s`\", table)\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `npc_entry`, `spell_id`, `quest_start`, `quest_start_active`, `quest_end`, `cast_flags`, `condition_id` FROM `npc_spellclick_spells`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `x`, `y`, `icon`, `flags`, `data`, `icon_name` FROM `points_of_interest`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT DISTINCT `item` FROM `npc_vendor`\")\;")

set(ALLOW_PlayerDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`, `phaseMap` FROM `playercreateinfo`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `race`, `class`, `itemid`, `amount` FROM `playercreateinfo_item`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `race`, `class`, `Spell` FROM `playercreateinfo_spell`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `race`, `class`, `button`, `action`, `type` FROM `playercreateinfo_action`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `race`, `class`, `level`, `str`, `agi`, `sta`, `inte`, `spi` FROM `player_levelstats`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `lvl`, `xp_for_next_level` FROM `player_xp_for_level`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `level`,`basexp` FROM `exploration_basexp`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `level`, `raceMask`, `mailTemplateId`, `senderEntry` FROM `mail_level_reward`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `name` FROM `reserved_name`\")\;")

set(ALLOW_QuestDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `Method`, `ZoneOrSort`, `MinLevel`, `QuestLevel`, `Type`, `RequiredClasses`, `RequiredRaces`, `RequiredSkill`, `RequiredSkillValue`,\""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,\""
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `entry`, `quest` FROM `quest_relations` WHERE `actor` = %d AND `role` = %d\", actor, role)\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `entry`, `quest` FROM `quest_relations` WHERE `actor` = %d\", actor)\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `questId`, `poiId`, `objIndex`, `mapId`, `mapAreaId`, `floorId`, `unk3`, `unk4` FROM `quest_poi`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `questId`, `poiId`, `x`, `y` FROM `quest_poi_points`\")\;")

set(ALLOW_ReputationDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `faction`, `quest_rate`, `creature_rate`, `spell_rate` FROM `reputation_reward_rate`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `creature_id`, `RewOnKillRepFaction1`, `RewOnKillRepFaction2`,\""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `faction`, `faction1`, `rate_1`, `rank_1`, `faction2`, `rate_2`, `rank_2`, `faction3`, `rate_3`, `rank_3`, `faction4`, `rate_4`, `rank_4`, `faction5`, `rate_5`, `rank_5` FROM `reputation_spillover_template`\")\;")

set(ALLOW_GameEventDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT MAX(`entry`) FROM `game_event`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`,UNIX_TIMESTAMP(`start_time`),UNIX_TIMESTAMP(`end_time`),`occurence`,`length`,`holiday`,`description` FROM `game_event`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `creature`.`guid`, `game_event_creature`.`event` \""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `gameobject`.`guid`, `game_event_gameobject`.`event` \""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `creature`.`guid`, `game_event_creature_data`.`event`, `game_event_creature_data`.`modelid`,\""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `quest`, `event` FROM `game_event_quest`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `event`, `raceMask`, `quest`, `mailTemplateId`, `senderEntry` FROM `game_event_mail`\")\;")

set(ALLOW_GameEventStatusRepository_cpp
    "QueryResult* result = CharacterDatabase.Query(\"SELECT `event` FROM `game_event_status`\")\;")

set(ALLOW_WaypointRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, COUNT(`point`) FROM `creature_movement` GROUP BY `id`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `point`, `position_x`, `position_y`, `position_z`, `waittime`, `script_id`,\""
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, COUNT(`point`) FROM `creature_movement_template` GROUP BY `entry`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `point`, `position_x`, `position_y`, `position_z`, `waittime`, `script_id`,\"")

set(ALLOW_PoolDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT MAX(`entry`) FROM `pool_template`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `entry`, `max_limit`, `description` FROM `pool_template`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `guid`, `pool_entry`, `chance` FROM `pool_creature`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `guid`, `pool_entry`, `chance`, `pool_creature_template`.`id` FROM `pool_creature_template` LEFT JOIN `creature` ON `creature`.`id` = `pool_creature_template`.`id`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `guid`, `pool_entry`, `chance` FROM `pool_gameobject`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `guid`, `pool_entry`, `chance`, `pool_gameobject_template`.`id` FROM `pool_gameobject_template` LEFT JOIN `gameobject` ON `gameobject`.`id` = `pool_gameobject_template`.`id`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT `pool_id`, `mother_pool`, `chance` FROM `pool_pool`\")\;")

set(ALLOW_DisableDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `sourceType`, `entry`, `flags`, `data` FROM `disables`\")\;")

set(ALLOW_BanRepository_cpp
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `account`.`username` FROM `account`,`account_banned` WHERE `account_banned`.`id`='%u' AND `account_banned`.`id`=`account`.`id`\", accountId)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `bandate`,`unbandate`,`bannedby`,`banreason` FROM `account_banned` WHERE `id` = %u ORDER BY `unbandate`\", accountId)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT FROM_UNIXTIME(`bandate`), `unbandate`-`bandate`, `active`, `unbandate`,`banreason`,`bannedby` FROM `account_banned` WHERE `id` = '%u' ORDER BY `bandate` ASC\", accountId)\;"
    "LoginDatabase.escape_string(ip)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `ip`, FROM_UNIXTIME(`bandate`), FROM_UNIXTIME(`unbandate`), `unbandate`-UNIX_TIMESTAMP(), `banreason`,`bannedby`,`unbandate`-`bandate` FROM `ip_banned` WHERE `ip` = '%s'\", ip.c_str())\;"
    "LoginDatabase.escape_string(filter)\;"
    "return ReadRows<ActiveIpBanRow>(LoginDatabase.Query(\"SELECT `ip`,`bandate`,`unbandate`,`bannedby`,`banreason` FROM `ip_banned`\""
    "return ReadRows<ActiveIpBanRow>(LoginDatabase.PQuery(\"SELECT `ip`,`bandate`,`unbandate`,`bannedby`,`banreason` FROM `ip_banned`\""
    "LoginDatabase.escape_string(filter)\;"
    "return ReadRows<BanListAccountRow>(LoginDatabase.Query(\"SELECT `account`.`id`, `username` FROM `account`, `account_banned`\""
    "return ReadRows<BanListAccountRow>(LoginDatabase.PQuery(\"SELECT `account`.`id`, `username` FROM `account`, `account_banned`\""
    "LoginDatabase.escape_string(text)\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id` FROM `account` WHERE `last_ip` = '%s'\", ip.c_str())\;"
    "QueryResult* result = LoginDatabase.PQuery(\"SELECT `id` FROM `account` WHERE `username` = '%s'\", name.c_str())\;")

set(ALLOW_CommandDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `id`, `command_text`,`security`,`help_text` FROM `command`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT \"")

set(ALLOW_ScriptDataRepository_cpp
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `type`, `bind`, `ScriptName`, `data` FROM `script_binding`\")\;"
    "QueryResult* result = WorldDatabase.Query(\"SELECT DISTINCT(`ScriptName`) FROM `script_binding`\")\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `id`, `delay`, `command`, `datalong`, `datalong2`, `buddy_entry`, `search_radius`, `data_flags`, `dataint`, `dataint2`, `dataint3`, `dataint4`, `x`, `y`, `z`, `o` FROM `db_scripts` WHERE `script_type` = %d ORDER BY `script_guid` ASC\", type)\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT COUNT(`entry`) FROM `script_waypoint` GROUP BY `entry`\")\;"
    "QueryResult* result = WorldDatabase.PQuery(\"SELECT `entry`, `pointid`, `location_x`, `location_y`, `location_z`, `waittime` FROM `script_waypoint` ORDER BY `pointid`\")\;")

set(ALLOW_DbVersionRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `version`, `structure`, `content` FROM `db_version` ORDER BY `version` DESC, `structure` DESC, `content` DESC LIMIT 1\")\;")

set(ALLOW_BroadcastDataRepository_cpp
    "QueryResult* result = WorldDatabase.Query(\"SELECT `autobroadcast`.`id`, `autobroadcast`.`content`,`autobroadcast`.`ratio` FROM `autobroadcast`\")\;")

set(ALLOW_RealmRepository_cpp
    "LoginDatabase.escape_string(builds)\;"
    "LoginDatabase.DirectPExecute(\"UPDATE `realmlist` SET `realmflags` = %u, `population` = 0, \""
    "LoginDatabase.DirectPExecute(\"UPDATE `realmlist` SET `realmflags` = `realmflags` | %u WHERE `id` = '%u'\",")

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
