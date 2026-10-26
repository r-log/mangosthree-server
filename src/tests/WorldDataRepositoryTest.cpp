/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/// The world data repositories on the global WorldDatabase backed by fakes: every startup load runs
/// on the calling thread, in call order, with the SQL text the loaders sent.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/world/DisableDataRepository.h"
#include "persistence/world/ai/CreatureEventAIRepository.h"
#include "persistence/world/creatures/CreatureDataRepository.h"
#include "persistence/world/economy/LootDataRepository.h"
#include "persistence/world/economy/SkillDataRepository.h"
#include "persistence/world/events/GameEventDataRepository.h"
#include "persistence/world/items/ItemDataRepository.h"
#include "persistence/world/locales/LocaleDataRepository.h"
#include "persistence/world/maps/DungeonDataRepository.h"
#include "persistence/world/maps/MapDataRepository.h"
#include "persistence/world/npcs/NpcDataRepository.h"
#include "persistence/world/players/PlayerDataRepository.h"
#include "persistence/world/pools/PoolDataRepository.h"
#include "persistence/world/pvp/BattleGroundDataRepository.h"
#include "persistence/world/quests/QuestDataRepository.h"
#include "persistence/world/reputation/ReputationDataRepository.h"
#include "persistence/world/spawns/SpawnRepository.h"
#include "persistence/world/spells/SpellDataRepository.h"
#include "persistence/world/waypoints/WaypointRepository.h"

#include <string>
#include <vector>

namespace
{
    /// The fakes on the global world database.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        Db() : query(WorldDatabase), async(WorldDatabase),
            attached(WorldDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
        }
    };

    void CheckLines(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got == want)
        {
            return;
        }
        std::string text = "lines differ (case line " + std::to_string(line) + "): got [";
        for (size_t i = 0; i < got.size(); ++i)
        {
            text += (i ? " | " : "") + got[i];
        }
        text += "] want [";
        for (size_t i = 0; i < want.size(); ++i)
        {
            text += (i ? " | " : "") + want[i];
        }
        testing::ReportFailure(__FILE__, line, text + "]");
    }
}

// The spell, event AI, loot, tradeskill, transport, weather and battleground loads read on the
// calling thread and queue nothing.
TEST(WorldDataRepository_StartupReads)
{
    Db db;
    SpellDataRepository::QueryUsedSpells("spell_check");
    SpellDataRepository::QuerySpellAreas();
    SpellDataRepository::QuerySpellChains();
    SpellDataRepository::QueryLearnSpells();
    SpellDataRepository::QueryTargetPositions();
    SpellDataRepository::QueryBonuses();
    SpellDataRepository::QueryElixirs();
    SpellDataRepository::QueryThreats();
    SpellDataRepository::QueryPetAuras();
    SpellDataRepository::QueryProcEvents();
    SpellDataRepository::QueryProcItemEnchants();
    CreatureEventAIRepository::QuerySummons();
    CreatureEventAIRepository::QueryEvents();
    LootDataRepository::QueryLootTemplate("creature_loot_template");
    LootDataRepository::QueryItemEnchantments();
    SkillDataRepository::QueryDiscoveries();
    SkillDataRepository::QueryExtraItems();
    MapDataRepository::QueryTransports();
    MapDataRepository::QueryTransportGameObjects();
    MapDataRepository::QueryWeather();
    BattleGroundDataRepository::QueryTemplates();
    BattleGroundDataRepository::QueryBattleMasters();
    BattleGroundDataRepository::QueryEventIndexes();
    CheckLines(db.query.executed, {
        "SELECT `spellid`,`SpellFamilyName`,`SpellFamilyMaskA`,`SpellFamilyMaskB`,`SpellIcon`,`SpellVisual`,`SpellCategory`,`EffectType`,`EffectAura`,`EffectIdx`,`Name`,`Code` FROM `spell_check`",
        "SELECT `spell`, `area`, `quest_start`, `quest_start_active`, `quest_end`, `condition_id`, `aura_spell`, `racemask`, `gender`, `autocast` FROM `spell_area`",
        "SELECT `spell_id`, `prev_spell`, `first_spell`, `rank`, `req_spell` FROM `spell_chain`",
        "SELECT `entry`, `SpellID`, `Active` FROM `spell_learn_spell`",
        "SELECT `id`, `target_map`, `target_position_x`, `target_position_y`, `target_position_z`, `target_orientation` FROM `spell_target_position`",
        "SELECT entry, direct_bonus,         `dot_bonus`, `ap_bonus`, `ap_dot_bonus` FROM `spell_bonus_data`",
        "SELECT `entry`, `mask` FROM `spell_elixir`",
        "SELECT `entry`, `Threat`, `multiplier`, `ap_bonus` FROM `spell_threat`",
        "SELECT `spell`, `effectId`, `pet`, `aura` FROM `spell_pet_auras`",
        "SELECT `entry`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMaskA0`, `SpellFamilyMaskA1`, `SpellFamilyMaskA2`, `SpellFamilyMaskB0`, `SpellFamilyMaskB1`, `SpellFamilyMaskB2`, `SpellFamilyMaskC0`, `SpellFamilyMaskC1`, `SpellFamilyMaskC2`, `procFlags`, `procEx`, `ppmRate`, `CustomChance`, `Cooldown` FROM `spell_proc_event`",
        "SELECT `entry`, `ppmRate` FROM `spell_proc_item_enchant`",
        "SELECT `id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs` FROM `creature_ai_summons`",
        "SELECT `id`, `creature_id`, `event_type`, `event_inverse_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `action1_type`, `action1_param1`, `action1_param2`, `action1_param3`, `action2_type`, `action2_param1`, `action2_param2`, `action2_param3`, `action3_type`, `action3_param1`, `action3_param2`, `action3_param3` FROM `creature_ai_scripts`",
        "SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount`, `condition_id` FROM `creature_loot_template`",
        "SELECT `entry`, `ench`, `chance` FROM `item_enchantment_template`",
        "SELECT `spellId`, `reqSpell`, `reqSkillValue`, `chance` FROM `skill_discovery_template`",
        "SELECT `spellId`, `requiredSpecialization`, `additionalCreateChance`, `additionalMaxNum` FROM `skill_extra_item_template`",
        "SELECT `entry`, `name`, `period` FROM `transports`",
        "SELECT `gameobject`.`guid`,`gameobject`.`id`,`transports`.`name` FROM `gameobject`,`transports` WHERE `gameobject`.`id` = `transports`.`entry`",
        "SELECT `zone`, `spring_rain_chance`, `spring_snow_chance`, `spring_storm_chance`, `summer_rain_chance`, `summer_snow_chance`, `summer_storm_chance`, `fall_rain_chance`, `fall_snow_chance`, `fall_storm_chance`, `winter_rain_chance`, `winter_snow_chance`, `winter_storm_chance` FROM `game_weather`",
        "SELECT `id`, `MinPlayersPerTeam`,`MaxPlayersPerTeam`,`AllianceStartLoc`,`AllianceStartO`,`HordeStartLoc`,`HordeStartO`, `StartMaxDist` FROM `battleground_template`",
        "SELECT `entry`,`bg_template` FROM `battlemaster_entry`",
        "SELECT `data`.`typ`, `data`.`guid1`, `data`.`ev1` AS `ev1`, `data`.`ev2` AS ev2, `data`.`map` AS m, `data`.`guid2`, `description`.`map`, `description`.`event1`, `description`.`event2`, `description`.`description` FROM (SELECT '1' AS typ, `a`.`guid` AS `guid1`, `a`.`event1` AS ev1, `a`.`event2` AS ev2, `b`.`map` AS map, `b`.`guid` AS guid2 FROM `gameobject_battleground` AS a LEFT OUTER JOIN `gameobject` AS b ON `a`.`guid` = `b`.`guid` UNION SELECT '2' AS typ, `a`.`guid` AS guid1, `a`.`event1` AS ev1, `a`.`event2` AS ev2, `b`.`map` AS map, `b`.`guid` AS guid2 FROM `creature_battleground` AS a LEFT OUTER JOIN `creature` AS b ON `a`.`guid` = `b`.`guid` ) data RIGHT OUTER JOIN `battleground_events` AS `description` ON `data`.`map` = `description`.`map` AND `data`.`ev1` = `description`.`event1` AND `data`.`ev2` = `description`.`event2` UNION SELECT `data`.`typ`, `data`.`guid1`, `data`.`ev1`, `data`.`ev2`, `data`.`map`, `data`.`guid2`, `description`.`map`, `description`.`event1`, `description`.`event2`, `description`.`description` FROM (SELECT '1' AS typ, `a`.`guid` AS guid1, `a`.`event1` AS ev1, `a`.`event2` AS ev2, `b`.`map` AS map, `b`.`guid` AS guid2 FROM `gameobject_battleground` AS a LEFT OUTER JOIN `gameobject` AS b ON `a`.`guid` = `b`.`guid` UNION SELECT '2' AS typ, `a`.`guid` AS guid1, `a`.`event1` AS ev1, `a`.`event2` AS ev2, `b`.`map` AS map, `b`.`guid` AS guid2 FROM `creature_battleground` AS a LEFT OUTER JOIN `creature` AS b ON `a`.`guid` = `b`.`guid` ) data LEFT OUTER JOIN `battleground_events` AS `description` ON `data`.`map` = `description`.`map` AND `data`.`ev1` = `description`.`event1` AND `data`.`ev2` = `description`.`event2` ORDER BY `m`, `ev1`, `ev2`",
    }, __LINE__);
    CHECK_EQ(db.async.executed.size(), size_t(0));
}

// ObjectMgr's world loads read on the calling thread and queue nothing; the class level stats name
// one health and damage column pair per expansion.
TEST(WorldDataRepository_ObjectMgrReads)
{
    Db db;
    CreatureDataRepository::QueryClassLevelStats(1);
    CreatureDataRepository::QueryModelRaces();
    CreatureDataRepository::QueryPetNameParts();
    CreatureDataRepository::QueryPetLevelStats();
    SpawnRepository::QueryCreatureSpawns();
    SpawnRepository::QueryGameObjectSpawns();
    ItemDataRepository::QueryItemLocales();
    ItemDataRepository::QueryItemConverts();
    ItemDataRepository::QueryItemExpireConverts();
    ItemDataRepository::QueryItemRequiredTargets();
    ItemDataRepository::QueryHotfixes();
    LocaleDataRepository::QueryCreatureLocales();
    LocaleDataRepository::QueryGossipMenuOptionLocales();
    LocaleDataRepository::QueryPointOfInterestLocales();
    LocaleDataRepository::QueryPageTextLocales();
    LocaleDataRepository::QueryNpcTextLocales();
    LocaleDataRepository::QueryGameObjectLocales();
    PlayerDataRepository::QueryCreateInfo();
    PlayerDataRepository::QueryCreateItems();
    PlayerDataRepository::QueryCreateSpells();
    PlayerDataRepository::QueryCreateActions();
    PlayerDataRepository::QueryLevelStats();
    PlayerDataRepository::QueryXpForLevel();
    PlayerDataRepository::QueryExplorationXp();
    PlayerDataRepository::QueryLevelRewardMails();
    PlayerDataRepository::QueryReservedNames();
    QuestDataRepository::QueryQuests();
    QuestDataRepository::QueryQuestLocales();
    QuestDataRepository::QueryQuestRelations(3, 2);
    QuestDataRepository::QueryActorQuestRelations(3);
    QuestDataRepository::QueryQuestPois();
    QuestDataRepository::QueryQuestPoiPoints();
    NpcDataRepository::QueryGossipMenus();
    NpcDataRepository::QueryGossipMenuOptions();
    NpcDataRepository::QueryNpcTexts();
    NpcDataRepository::QueryTrainerSpells("npc_trainer");
    NpcDataRepository::QueryVendorItems("npc_vendor");
    NpcDataRepository::QuerySpellClickSpells();
    NpcDataRepository::QueryPointsOfInterest();
    MapDataRepository::QueryTeles();
    MapDataRepository::QueryGraveyardZones();
    MapDataRepository::QueryAreaTriggerTeleports();
    MapDataRepository::QueryTavernAreaTriggers();
    DungeonDataRepository::QueryInstanceEncounters();
    DungeonDataRepository::QueryDungeonFinderRequirements();
    DungeonDataRepository::QueryDungeonFinderRewards();
    DungeonDataRepository::QueryDungeonFinderItemRewards();
    DungeonDataRepository::QueryDungeonEntrances();
    ReputationDataRepository::QueryRewardRates();
    ReputationDataRepository::QueryOnKillReputations();
    ReputationDataRepository::QuerySpillovers();
    SkillDataRepository::QueryFishingBaseLevels();
    CheckLines(db.query.executed, {
        "SELECT `Class`, `Level`, `BaseMana`, `BaseMeleeAttackPower`, `BaseRangedAttackPower`, `BaseArmor`, `BaseHealthExp0`, `BaseDamageExp0`, `BaseHealthExp1`, `BaseDamageExp1` FROM `creature_template_classlevelstats` ORDER BY `Class`, `Level`",
        "SELECT `modelid`, `racemask`, `creature_entry`, `modelid_racial` FROM `creature_model_race`",
        "SELECT `word`,`entry`,`half` FROM `pet_name_generation`",
        "SELECT `creature_entry`, `level`, `hp`, `mana`, `str`, `agi`, `sta`, `inte`, `spi`, `armor` FROM `pet_levelstats`",
        "SELECT `creature`.`guid`, `creature`.`id`, `map`, `modelid`,`equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `spawndist`, `currentwaypoint`,`curhealth`, `curmana`, `DeathState`, `MovementType`, `spawnMask`, `phaseMask`, `event`,`pool_creature`.`pool_entry`, `pool_creature_template`.`pool_entry` FROM `creature` LEFT OUTER JOIN `game_event_creature` ON `creature`.`guid` = `game_event_creature`.`guid` LEFT OUTER JOIN `pool_creature` ON `creature`.`guid` = `pool_creature`.`guid` LEFT OUTER JOIN `pool_creature_template` ON `creature`.`id` = `pool_creature_template`.`id`",
        "SELECT `gameobject`.`guid`, `gameobject`.`id`, `gameobject`.`map`, `gameobject`.`position_x`, `gameobject`.`position_y`, `gameobject`.`position_z`, `gameobject`.`orientation`, `gameobject`.`rotation0`, `gameobject`.`rotation1`, `gameobject`.`rotation2`, `gameobject`.`rotation3`, `gameobject`.`spawntimesecs`, `gameobject`.`animprogress`, `gameobject`.`state`, `gameobject`.`spawnMask`, `gameobject`.`phaseMask`, `game_event_gameobject`.`event`, `pool_gameobject`.`pool_entry`, `pool_gameobject_template`.`pool_entry` FROM `gameobject` LEFT OUTER JOIN `game_event_gameobject` ON `gameobject`.`guid` = `game_event_gameobject`.`guid` LEFT OUTER JOIN `pool_gameobject` ON `gameobject`.`guid` = `pool_gameobject`.`guid` LEFT OUTER JOIN `pool_gameobject_template` ON `gameobject`.`id` = `pool_gameobject_template`.`id`",
        "SELECT `entry`,`name_loc1`,`description_loc1`,`name_loc2`,`description_loc2`,`name_loc3`,`description_loc3`,`name_loc4`,`description_loc4`,`name_loc5`,`description_loc5`,`name_loc6`,`description_loc6`,`name_loc7`,`description_loc7`,`name_loc8`,`description_loc8`,`name_loc9`,`description_loc9`,`name_loc10`,`description_loc10`,`name_loc11`,`description_loc11` FROM `locales_item`",
        "SELECT `entry`,`item` FROM `item_convert`",
        "SELECT `entry`,`item` FROM `item_expire_convert`",
        "SELECT `entry`,`type`,`targetEntry` FROM `item_required_target`",
        "SELECT entry, type, UNIX_TIMESTAMP(hotfixDate) FROM hotfix_data",
        "SELECT `entry`,`name_loc1`,`subname_loc1`,`name_loc2`,`subname_loc2`,`name_loc3`,`subname_loc3`,`name_loc4`,`subname_loc4`,`name_loc5`,`subname_loc5`,`name_loc6`,`subname_loc6`,`name_loc7`,`subname_loc7`,`name_loc8`,`subname_loc8`,`name_loc9`,`subname_loc9`,`name_loc10`,`subname_loc10`,`name_loc11`,`subname_loc11` FROM `locales_creature`",
        "SELECT `menu_id`,`id`,`option_text_loc1`,`box_text_loc1`,`option_text_loc2`,`box_text_loc2`,`option_text_loc3`,`box_text_loc3`,`option_text_loc4`,`box_text_loc4`,`option_text_loc5`,`box_text_loc5`,`option_text_loc6`,`box_text_loc6`,`option_text_loc7`,`box_text_loc7`,`option_text_loc8`,`box_text_loc8`,`option_text_loc9`,`box_text_loc9`,`option_text_loc10`,`box_text_loc10`,`option_text_loc11`,`box_text_loc11` FROM `locales_gossip_menu_option`",
        "SELECT `entry`,`icon_name_loc1`,`icon_name_loc2`,`icon_name_loc3`,`icon_name_loc4`,`icon_name_loc5`,`icon_name_loc6`,`icon_name_loc7`,`icon_name_loc8`,`icon_name_loc9`,`icon_name_loc10`,`icon_name_loc11` FROM `locales_points_of_interest`",
        "SELECT `entry`,`text_loc1`,`text_loc2`,`text_loc3`,`text_loc4`,`text_loc5`,`text_loc6`,`text_loc7`,`text_loc8`,`text_loc9`,`text_loc10`,`text_loc11` FROM `locales_page_text`",
        "SELECT `entry`,`Text0_0_loc1`,`Text0_1_loc1`,`Text1_0_loc1`,`Text1_1_loc1`,`Text2_0_loc1`,`Text2_1_loc1`,`Text3_0_loc1`,`Text3_1_loc1`,`Text4_0_loc1`,`Text4_1_loc1`,`Text5_0_loc1`,`Text5_1_loc1`,`Text6_0_loc1`,`Text6_1_loc1`,`Text7_0_loc1`,`Text7_1_loc1`,`Text0_0_loc2`,`Text0_1_loc2`,`Text1_0_loc2`,`Text1_1_loc2`,`Text2_0_loc2`,`Text2_1_loc2`,`Text3_0_loc2`,`Text3_1_loc2`,`Text4_0_loc2`,`Text4_1_loc2`,`Text5_0_loc2`,`Text5_1_loc2`,`Text6_0_loc2`,`Text6_1_loc2`,`Text7_0_loc2`,`Text7_1_loc2`,`Text0_0_loc3`,`Text0_1_loc3`,`Text1_0_loc3`,`Text1_1_loc3`,`Text2_0_loc3`,`Text2_1_loc3`,`Text3_0_loc3`,`Text3_1_loc3`,`Text4_0_loc3`,`Text4_1_loc3`,`Text5_0_loc3`,`Text5_1_loc3`,`Text6_0_loc3`,`Text6_1_loc3`,`Text7_0_loc3`,`Text7_1_loc3`,`Text0_0_loc4`,`Text0_1_loc4`,`Text1_0_loc4`,`Text1_1_loc4`,`Text2_0_loc4`,`Text2_1_loc4`,`Text3_0_loc4`,`Text3_1_loc4`,`Text4_0_loc4`,`Text4_1_loc4`,`Text5_0_loc4`,`Text5_1_loc4`,`Text6_0_loc4`,`Text6_1_loc4`,`Text7_0_loc4`,`Text7_1_loc4`,`Text0_0_loc5`,`Text0_1_loc5`,`Text1_0_loc5`,`Text1_1_loc5`,`Text2_0_loc5`,`Text2_1_loc5`,`Text3_0_loc5`,`Text3_1_loc5`,`Text4_0_loc5`,`Text4_1_loc5`,`Text5_0_loc5`,`Text5_1_loc5`,`Text6_0_loc5`,`Text6_1_loc5`,`Text7_0_loc5`,`Text7_1_loc5`,`Text0_0_loc6`,`Text0_1_loc6`,`Text1_0_loc6`,`Text1_1_loc6`,`Text2_0_loc6`,`Text2_1_loc6`,`Text3_0_loc6`,`Text3_1_loc6`,`Text4_0_loc6`,`Text4_1_loc6`,`Text5_0_loc6`,`Text5_1_loc6`,`Text6_0_loc6`,`Text6_1_loc6`,`Text7_0_loc6`,`Text7_1_loc6`,`Text0_0_loc7`,`Text0_1_loc7`,`Text1_0_loc7`,`Text1_1_loc7`,`Text2_0_loc7`,`Text2_1_loc7`,`Text3_0_loc7`,`Text3_1_loc7`,`Text4_0_loc7`,`Text4_1_loc7`,`Text5_0_loc7`,`Text5_1_loc7`,`Text6_0_loc7`,`Text6_1_loc7`,`Text7_0_loc7`,`Text7_1_loc7`, `Text0_0_loc8`,`Text0_1_loc8`,`Text1_0_loc8`,`Text1_1_loc8`,`Text2_0_loc8`,`Text2_1_loc8`,`Text3_0_loc8`,`Text3_1_loc8`,`Text4_0_loc8`,`Text4_1_loc8`,`Text5_0_loc8`,`Text5_1_loc8`,`Text6_0_loc8`,`Text6_1_loc8`,`Text7_0_loc8`,`Text7_1_loc8`,`Text0_0_loc9`,`Text0_1_loc9`,`Text1_0_loc9`,`Text1_1_loc9`,`Text2_0_loc9`,`Text2_1_loc9`,`Text3_0_loc9`,`Text3_1_loc9`,`Text4_0_loc9`,`Text4_1_loc9`,`Text5_0_loc9`,`Text5_1_loc9`,`Text6_0_loc9`,`Text6_1_loc9`,`Text7_0_loc9`,`Text7_1_loc9`,`Text0_0_loc10`,`Text0_1_loc10`,`Text1_0_loc10`,`Text1_1_loc10`,`Text2_0_loc10`,`Text2_1_loc10`,`Text3_0_loc10`,`Text3_1_loc10`,`Text4_0_loc10`,`Text4_1_loc10`,`Text5_0_loc10`,`Text5_1_loc10`,`Text6_0_loc10`,`Text6_1_loc10`,`Text7_0_loc10`,`Text7_1_loc10`,`Text0_0_loc11`,`Text0_1_loc11`,`Text1_0_loc11`,`Text1_1_loc11`,`Text2_0_loc11`,`Text2_1_loc11`,`Text3_0_loc11`,`Text3_1_loc11`,`Text4_0_loc11`,`Text4_1_loc11`,`Text5_0_loc11`,`Text5_1_loc11`,`Text6_0_loc11`,`Text6_1_loc11`,`Text7_0_loc11`,`Text7_1_loc11`  FROM `locales_npc_text`",
        "SELECT `entry`,`name_loc1`,`name_loc2`,`name_loc3`,`name_loc4`,`name_loc5`,`name_loc6`,`name_loc7`,`name_loc8`,`name_loc9`,`name_loc10`,`name_loc11`,`castbarcaption_loc1`,`castbarcaption_loc2`,`castbarcaption_loc3`,`castbarcaption_loc4`,`castbarcaption_loc5`,`castbarcaption_loc6`,`castbarcaption_loc7`,`castbarcaption_loc8`,`castbarcaption_loc9`,`castbarcaption_loc10`,`castbarcaption_loc11` FROM `locales_gameobject`",
        "SELECT `race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`, `phaseMap` FROM `playercreateinfo`",
        "SELECT `race`, `class`, `itemid`, `amount` FROM `playercreateinfo_item`",
        "SELECT `race`, `class`, `Spell` FROM `playercreateinfo_spell`",
        "SELECT `race`, `class`, `button`, `action`, `type` FROM `playercreateinfo_action`",
        "SELECT `race`, `class`, `level`, `str`, `agi`, `sta`, `inte`, `spi` FROM `player_levelstats`",
        "SELECT `lvl`, `xp_for_next_level` FROM `player_xp_for_level`",
        "SELECT `level`,`basexp` FROM `exploration_basexp`",
        "SELECT `level`, `raceMask`, `mailTemplateId`, `senderEntry` FROM `mail_level_reward`",
        "SELECT `name` FROM `reserved_name`",
        "SELECT `entry`, `Method`, `ZoneOrSort`, `MinLevel`, `QuestLevel`, `Type`, `RequiredClasses`, `RequiredRaces`, `RequiredSkill`, `RequiredSkillValue`,`RepObjectiveFaction`, `RepObjectiveValue`, `RequiredMinRepFaction`, `RequiredMinRepValue`, `RequiredMaxRepFaction`, `RequiredMaxRepValue`, `SuggestedPlayers`, `LimitTime`,`QuestFlags`, `SpecialFlags`, `CharTitleId`, `PlayersSlain`, `BonusTalents`, `PortraitGiver`, `PortraitTurnIn`, `PrevQuestId`, `NextQuestId`, `ExclusiveGroup`, `NextQuestInChain`,`RewXPId`, `SrcItemId`, `SrcItemCount`, `SrcSpell`,`Title`, `Details`, `Objectives`, `OfferRewardText`, `RequestItemsText`, `EndText`, `CompletedText`, `PortraitGiverName`, `PortraitGiverText`, `PortraitTurnInName`, `PortraitTurnInText`,`ObjectiveText1`, `ObjectiveText2`, `ObjectiveText3`, `ObjectiveText4`,`ReqItemId1`, `ReqItemId2`, `ReqItemId3`, `ReqItemId4`, `ReqItemId5`, `ReqItemId6`, `ReqItemCount1`, `ReqItemCount2`, `ReqItemCount3`, `ReqItemCount4`, `ReqItemCount5`, `ReqItemCount6`,`ReqSourceId1`, `ReqSourceId2`, `ReqSourceId3`, `ReqSourceId4`, `ReqSourceCount1`, `ReqSourceCount2`, `ReqSourceCount3`, `ReqSourceCount4`,`ReqCreatureOrGOId1`, `ReqCreatureOrGOId2`, `ReqCreatureOrGOId3`, `ReqCreatureOrGOId4`, `ReqCreatureOrGOCount1`, `ReqCreatureOrGOCount2`, `ReqCreatureOrGOCount3`, `ReqCreatureOrGOCount4`,`ReqCurrencyId1`, `ReqCurrencyId2`, `ReqCurrencyId3`, `ReqCurrencyId4`,`ReqCurrencyCount1`, `ReqCurrencyCount2`, `ReqCurrencyCount3`, `ReqCurrencyCount4`,`ReqSpellCast1`, `ReqSpellCast2`, `ReqSpellCast3`, `ReqSpellCast4`, `ReqSpellLearned`,`RewChoiceItemId1`, `RewChoiceItemId2`, `RewChoiceItemId3`, `RewChoiceItemId4`, `RewChoiceItemId5`, `RewChoiceItemId6`,`RewChoiceItemCount1`, `RewChoiceItemCount2`, `RewChoiceItemCount3`, `RewChoiceItemCount4`, `RewChoiceItemCount5`, `RewChoiceItemCount6`,`RewItemId1`, `RewItemId2`, `RewItemId3`, `RewItemId4`, `RewItemCount1`, `RewItemCount2`, `RewItemCount3`, `RewItemCount4`,`RewCurrencyId1`, `RewCurrencyId2`, `RewCurrencyId3`, `RewCurrencyId4`,`RewCurrencyCount1`, `RewCurrencyCount2`, `RewCurrencyCount3`, `RewCurrencyCount4`,`RewSkill`, `RewSkillValue`,`RewRepFaction1`, `RewRepFaction2`, `RewRepFaction3`, `RewRepFaction4`, `RewRepFaction5`,`RewRepValueId1`, `RewRepValueId2`, `RewRepValueId3`, `RewRepValueId4`, `RewRepValueId5`,`RewRepValue1`, `RewRepValue2`, `RewRepValue3`, `RewRepValue4`, `RewRepValue5`,`RewHonorAddition`, `RewHonorMultiplier`, `RewOrReqMoney`, `RewMoneyMaxLevel`, `RewSpell`, `RewSpellCast`,`RewMailTemplateId`, `RewMailDelaySecs`, `PointMapId`, `PointX`, `PointY`, `PointOpt`,`DetailsEmote1`, `DetailsEmote2`, `DetailsEmote3`, `DetailsEmote4`, `DetailsEmoteDelay1`, `DetailsEmoteDelay2`, `DetailsEmoteDelay3`, `DetailsEmoteDelay4`,`IncompleteEmote`, `CompleteEmote`, `OfferRewardEmote1`, `OfferRewardEmote2`, `OfferRewardEmote3`, `OfferRewardEmote4`,`OfferRewardEmoteDelay1`, `OfferRewardEmoteDelay2`, `OfferRewardEmoteDelay3`, `OfferRewardEmoteDelay4`,`SoundAccept`, `SoundTurnIn`, `StartScript`, `CompleteScript` FROM `quest_template`",
        "SELECT `entry`,`Title_loc1`,`Details_loc1`,`Objectives_loc1`,`OfferRewardText_loc1`,`RequestItemsText_loc1`,`EndText_loc1`,`CompletedText_loc1`,`ObjectiveText1_loc1`,`ObjectiveText2_loc1`,`ObjectiveText3_loc1`,`ObjectiveText4_loc1`,`PortraitGiverName_loc1`,`PortraitGiverText_loc1`,`PortraitTurnInName_loc1`,`PortraitTurnInText_loc1`,`Title_loc2`,`Details_loc2`,`Objectives_loc2`,`OfferRewardText_loc2`,`RequestItemsText_loc2`,`EndText_loc2`,`CompletedText_loc2`,`ObjectiveText1_loc2`,`ObjectiveText2_loc2`,`ObjectiveText3_loc2`,`ObjectiveText4_loc2`,`PortraitGiverName_loc2`,`PortraitGiverText_loc2`,`PortraitTurnInName_loc2`,`PortraitTurnInText_loc2`,`Title_loc3`,`Details_loc3`,`Objectives_loc3`,`OfferRewardText_loc3`,`RequestItemsText_loc3`,`EndText_loc3`,`CompletedText_loc3`,`ObjectiveText1_loc3`,`ObjectiveText2_loc3`,`ObjectiveText3_loc3`,`ObjectiveText4_loc3`,`PortraitGiverName_loc3`,`PortraitGiverText_loc3`,`PortraitTurnInName_loc3`,`PortraitTurnInText_loc3`,`Title_loc4`,`Details_loc4`,`Objectives_loc4`,`OfferRewardText_loc4`,`RequestItemsText_loc4`,`EndText_loc4`,`CompletedText_loc4`,`ObjectiveText1_loc4`,`ObjectiveText2_loc4`,`ObjectiveText3_loc4`,`ObjectiveText4_loc4`,`PortraitGiverName_loc4`,`PortraitGiverText_loc4`,`PortraitTurnInName_loc4`,`PortraitTurnInText_loc4`,`Title_loc5`,`Details_loc5`,`Objectives_loc5`,`OfferRewardText_loc5`,`RequestItemsText_loc5`,`EndText_loc5`,`CompletedText_loc5`,`ObjectiveText1_loc5`,`ObjectiveText2_loc5`,`ObjectiveText3_loc5`,`ObjectiveText4_loc5`,`PortraitGiverName_loc5`,`PortraitGiverText_loc5`,`PortraitTurnInName_loc5`,`PortraitTurnInText_loc5`,`Title_loc6`,`Details_loc6`,`Objectives_loc6`,`OfferRewardText_loc6`,`RequestItemsText_loc6`,`EndText_loc6`,`CompletedText_loc6`,`ObjectiveText1_loc6`,`ObjectiveText2_loc6`,`ObjectiveText3_loc6`,`ObjectiveText4_loc6`,`PortraitGiverName_loc6`,`PortraitGiverText_loc6`,`PortraitTurnInName_loc6`,`PortraitTurnInText_loc6`,`Title_loc7`,`Details_loc7`,`Objectives_loc7`,`OfferRewardText_loc7`,`RequestItemsText_loc7`,`EndText_loc7`,`CompletedText_loc7`,`ObjectiveText1_loc7`,`ObjectiveText2_loc7`,`ObjectiveText3_loc7`,`ObjectiveText4_loc7`,`PortraitGiverName_loc7`,`PortraitGiverText_loc7`,`PortraitTurnInName_loc7`,`PortraitTurnInText_loc7`,`Title_loc8`,`Details_loc8`,`Objectives_loc8`,`OfferRewardText_loc8`,`RequestItemsText_loc8`,`EndText_loc8`,`CompletedText_loc8`,`ObjectiveText1_loc8`,`ObjectiveText2_loc8`,`ObjectiveText3_loc8`,`ObjectiveText4_loc8`,`PortraitGiverName_loc8`,`PortraitGiverText_loc8`,`PortraitTurnInName_loc8`,`PortraitTurnInText_loc8`,`Title_loc9`,`Details_loc9`,`Objectives_loc9`,`OfferRewardText_loc9`,`RequestItemsText_loc9`,`EndText_loc9`,`CompletedText_loc9`,`ObjectiveText1_loc9`,`ObjectiveText2_loc9`,`ObjectiveText3_loc9`,`ObjectiveText4_loc9`,`PortraitGiverName_loc9`,`PortraitGiverText_loc9`,`PortraitTurnInName_loc9`,`PortraitTurnInText_loc9`,`Title_loc10`,`Details_loc10`,`Objectives_loc10`,`OfferRewardText_loc10`,`RequestItemsText_loc10`,`EndText_loc10`,`CompletedText_loc10`,`ObjectiveText1_loc10`,`ObjectiveText2_loc10`,`ObjectiveText3_loc10`,`ObjectiveText4_loc10`,`PortraitGiverName_loc10`,`PortraitGiverText_loc10`,`PortraitTurnInName_loc10`,`PortraitTurnInText_loc10`,`Title_loc11`,`Details_loc11`,`Objectives_loc11`,`OfferRewardText_loc11`,`RequestItemsText_loc11`,`EndText_loc11`,`CompletedText_loc11`,`ObjectiveText1_loc11`,`ObjectiveText2_loc11`,`ObjectiveText3_loc11`,`ObjectiveText4_loc11`,`PortraitGiverName_loc11`,`PortraitGiverText_loc11`,`PortraitTurnInName_loc11`,`PortraitTurnInText_loc11` FROM `locales_quest`",
        "SELECT `entry`, `quest` FROM `quest_relations` WHERE `actor` = 3 AND `role` = 2",
        "SELECT `entry`, `quest` FROM `quest_relations` WHERE `actor` = 3",
        "SELECT `questId`, `poiId`, `objIndex`, `mapId`, `mapAreaId`, `floorId`, `unk3`, `unk4` FROM `quest_poi`",
        "SELECT `questId`, `poiId`, `x`, `y` FROM `quest_poi_points`",
        "SELECT `entry`, `text_id`, `script_id`, `condition_id` FROM `gossip_menu`",
        "SELECT `menu_id`, `id`, `option_icon`, `option_text`, `option_id`, `npc_option_npcflag`, `action_menu_id`, `action_poi_id`, `action_script_id`, `box_coded`, `box_money`, `box_text`, `condition_id` FROM `gossip_menu_option` ORDER BY `menu_id`, `id`",
        "SELECT * FROM `npc_text`",
        "SELECT `entry`, `spell`,`spellcost`,`reqskill`,`reqskillvalue`,`reqlevel` FROM `npc_trainer`",
        "SELECT `entry`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `condition_id` FROM `npc_vendor`",
        "SELECT `npc_entry`, `spell_id`, `quest_start`, `quest_start_active`, `quest_end`, `cast_flags`, `condition_id` FROM `npc_spellclick_spells`",
        "SELECT `entry`, `x`, `y`, `icon`, `flags`, `data`, `icon_name` FROM `points_of_interest`",
        "SELECT `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, `name` FROM `game_tele`",
        "SELECT `id`,`ghost_zone`,`faction` FROM `game_graveyard_zone`",
        "SELECT `id`, `required_level`, `required_item`, `required_item2`, `heroic_key`, `heroic_key2`, `required_quest_done`, `required_quest_done_heroic`, `target_map`, `target_position_x`, `target_position_y`, `target_position_z`, `target_orientation` FROM `areatrigger_teleport`",
        "SELECT `id` FROM `areatrigger_tavern`",
        "SELECT `entry`, `creditType`, `creditEntry`, `lastEncounterDungeon` FROM `instance_encounters`",
        "SELECT `mapId`, `difficulty`, `min_item_level`, `item`, `item_2`, `alliance_quest`, `horde_quest`, `achievement`, `quest_incomplete_text` FROM `dungeonfinder_requirements`",
        "SELECT `id`, `level`, `base_xp_reward`, `base_monetary_reward` FROM `dungeonfinder_rewards`",
        "SELECT `id`, `min_level`, `max_level`, `item_reward`, `item_amount`, `dungeon_type` FROM `dungeonfinder_item_rewards`",
        "SELECT `dungeonId`, `position_x`, `position_y`, `position_z`, `orientation` FROM `lfg_dungeon_entrances`",
        "SELECT `faction`, `quest_rate`, `creature_rate`, `spell_rate` FROM `reputation_reward_rate`",
        "SELECT `creature_id`, `RewOnKillRepFaction1`, `RewOnKillRepFaction2`,`IsTeamAward1`, `MaxStanding1`, `RewOnKillRepValue1`, `IsTeamAward2`, `MaxStanding2`, `RewOnKillRepValue2`, `TeamDependent` FROM `creature_onkill_reputation`",
        "SELECT `faction`, `faction1`, `rate_1`, `rank_1`, `faction2`, `rate_2`, `rank_2`, `faction3`, `rate_3`, `rank_3`, `faction4`, `rate_4`, `rank_4`, `faction5`, `rate_5`, `rank_5` FROM `reputation_spillover_template`",
        "SELECT `entry`,`skill` FROM `skill_fishing_base_level`",
    }, __LINE__);
    CHECK_EQ(db.async.executed.size(), size_t(0));
}

// A GM's vendor, teleport and graveyard writes are queued in order; a currency's negated entry is
// written signed, and the teleport's name is escaped on the calling thread first.
TEST(WorldDataRepository_GmWrites)
{
    Db db;
    TickGuard::ResetViolations();
    CHECK(MapDataRepository::InsertTele(9, 1.5f, -2.25f, 3.0f, 0.5f, 530, "Shattrath"));
    {
        TickGuard::Scope scope;
        NpcDataRepository::InsertVendorItem(5, uint32(-int32(3)), 1, 2, 3);
        NpcDataRepository::DeleteVendorItem(5, 7);
        MapDataRepository::DeleteTele("Shattrath");
        MapDataRepository::InsertGraveyardZone(1, 2, 469);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(WorldDatabase.GetDelayQueueDepth(), size_t(5));
    }
    WorldDatabase.ExecuteQueuedForTest();
    CheckLines(db.async.executed, {
        "INSERT INTO `game_tele` (`id`,`position_x`,`position_y`,`position_z`,`orientation`,`map`,`name`) VALUES (9,1.500000,-2.250000,3.000000,0.500000,530,'Shattrath')",
        "INSERT INTO `npc_vendor` (`entry`,`item`,`maxcount`,`incrtime`,`extendedcost`) VALUES('5','-3','1','2','3')",
        "DELETE FROM `npc_vendor` WHERE `entry`='5' AND `item`='7'",
        "DELETE FROM `game_tele` WHERE `name` = 'Shattrath'",
        "INSERT INTO `game_graveyard_zone` (`id`,`ghost_zone`,`faction`) VALUES ('1', '2','469')",
    }, __LINE__);
}
// The game event, waypoint, pool, linking and disable loads read on the calling thread and queue
// nothing.
TEST(WorldDataRepository_ManagerReads)
{
    Db db;
    GameEventDataRepository::QueryHighestEventEntry();
    GameEventDataRepository::QueryEvents();
    GameEventDataRepository::QueryEventCreatures();
    GameEventDataRepository::QueryEventGameObjects();
    GameEventDataRepository::QueryEventCreatureData();
    GameEventDataRepository::QueryEventQuests();
    GameEventDataRepository::QueryEventMails();
    WaypointRepository::QueryPathPointCounts();
    WaypointRepository::QueryPathPoints();
    WaypointRepository::QueryTemplatePathPointCounts();
    WaypointRepository::QueryTemplatePathPoints();
    SpawnRepository::QueryCreatureEntryAndMap(42);
    SpawnRepository::QueryCreatureGuidsOnMap(1234, 530);
    PoolDataRepository::QueryHighestPoolEntry();
    PoolDataRepository::QueryPools();
    PoolDataRepository::QueryPoolCreatures();
    PoolDataRepository::QueryPoolCreatureTemplates();
    PoolDataRepository::QueryPoolGameObjects();
    PoolDataRepository::QueryPoolGameObjectTemplates();
    PoolDataRepository::QueryPoolPools();
    CreatureDataRepository::QueryLinkingTemplates();
    CreatureDataRepository::QueryLinkings();
    DisableDataRepository::QueryDisables();
    CheckLines(db.query.executed, {
        "SELECT MAX(`entry`) FROM `game_event`",
        "SELECT `entry`,UNIX_TIMESTAMP(`start_time`),UNIX_TIMESTAMP(`end_time`),`occurence`,`length`,`holiday`,`description` FROM `game_event`",
        "SELECT `creature`.`guid`, `game_event_creature`.`event` FROM `creature` JOIN `game_event_creature` ON `creature`.`guid` = `game_event_creature`.`guid`",
        "SELECT `gameobject`.`guid`, `game_event_gameobject`.`event` FROM `gameobject` JOIN `game_event_gameobject` ON `gameobject`.`guid`=`game_event_gameobject`.`guid`",
        "SELECT `creature`.`guid`, `game_event_creature_data`.`event`, `game_event_creature_data`.`modelid`,`game_event_creature_data`.`equipment_id`, `game_event_creature_data`.`entry_id`, `game_event_creature_data`.`spell_start`, `game_event_creature_data`.`spell_end` FROM `creature` JOIN `game_event_creature_data` ON `creature`.`guid`=`game_event_creature_data`.`guid`",
        "SELECT `quest`, `event` FROM `game_event_quest`",
        "SELECT `event`, `raceMask`, `quest`, `mailTemplateId`, `senderEntry` FROM `game_event_mail`",
        "SELECT `id`, COUNT(`point`) FROM `creature_movement` GROUP BY `id`",
        "SELECT `id`, `point`, `position_x`, `position_y`, `position_z`, `waittime`, `script_id`,`textid1`, `textid2`, `textid3`, `textid4`, `textid5`, `emote`, `spell`, `orientation`, `model1`, `model2` FROM `creature_movement`",
        "SELECT `entry`, COUNT(`point`) FROM `creature_movement_template` GROUP BY `entry`",
        "SELECT `entry`, `point`, `position_x`, `position_y`, `position_z`, `waittime`, `script_id`,`textid1`, `textid2`, `textid3`, `textid4`, `textid5`, `emote`, `spell`, `orientation`, `model1`, `model2` FROM `creature_movement_template`",
        "SELECT `id`, `map` FROM `creature` WHERE `guid` = '42'",
        "SELECT `guid` FROM `creature` WHERE `id`=1234 AND `map`=530 LIMIT 2",
        "SELECT MAX(`entry`) FROM `pool_template`",
        "SELECT `entry`, `max_limit`, `description` FROM `pool_template`",
        "SELECT `guid`, `pool_entry`, `chance` FROM `pool_creature`",
        "SELECT `guid`, `pool_entry`, `chance`, `pool_creature_template`.`id` FROM `pool_creature_template` LEFT JOIN `creature` ON `creature`.`id` = `pool_creature_template`.`id`",
        "SELECT `guid`, `pool_entry`, `chance` FROM `pool_gameobject`",
        "SELECT `guid`, `pool_entry`, `chance`, `pool_gameobject_template`.`id` FROM `pool_gameobject_template` LEFT JOIN `gameobject` ON `gameobject`.`id` = `pool_gameobject_template`.`id`",
        "SELECT `pool_id`, `mother_pool`, `chance` FROM `pool_pool`",
        "SELECT `entry`, `map`, `master_entry`, `flag`, `search_range` FROM `creature_linking_template`",
        "SELECT `guid`, `master_guid`, `flag` FROM `creature_linking`",
        "SELECT `sourceType`, `entry`, `flags`, `data` FROM `disables`",
    }, __LINE__);
    CHECK_EQ(db.async.executed.size(), size_t(0));
}

// The waypoint load's coordinate fixes and a GM's path edits are queued in order, the edits naming
// the table and key column they are handed.
TEST(WorldDataRepository_WaypointWrites)
{
    Db db;
    TickGuard::ResetViolations();
    {
        TickGuard::Scope scope;
        WaypointRepository::FixPathPoint(1.5f, -2.25f, 3.0f, 42, 7);
        WaypointRepository::FixTemplatePathPoint(1.5f, -2.25f, 1234, 7);
        WaypointRepository::ShiftPoint("creature_movement", "id", 42, 6);
        WaypointRepository::InsertPoint("creature_movement", "id", 42, 3, 1.5f, -2.25f, 3.0f);
        WaypointRepository::DeletePoint("creature_movement_template", "entry", 1234, 3);
        WaypointRepository::DeletePath(42);
        WaypointRepository::SetPointPosition("creature_movement", 1.5f, -2.25f, 3.0f, "id", 42, 3);
        WaypointRepository::SetPointWaittime("creature_movement", 5000, "id", 42, 3);
        WaypointRepository::SetPointOrientation("creature_movement", 0.5f, "id", 42, 3);
        WaypointRepository::SetPointScriptId("creature_movement", 77, "id", 42, 3);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(WorldDatabase.GetDelayQueueDepth(), size_t(10));
    }
    WorldDatabase.ExecuteQueuedForTest();
    CheckLines(db.async.executed, {
        "UPDATE `creature_movement` SET `position_x` = '1.500000', `position_y` = '-2.250000', `position_z` = '3.000000' WHERE `id` = '42' AND `point` = '7'",
        "UPDATE `creature_movement_template` SET `position_x` = '1.500000', `position_y` = '-2.250000' WHERE `entry` = 1234 AND `point` = 7",
        "UPDATE `creature_movement` SET `point`=`point`+1 WHERE `id`=42 AND `point`=6",
        "INSERT INTO `creature_movement` (`id`,`point`,`position_x`,`position_y`,`position_z`,`orientation`) VALUES (42,3, 1.500000,-2.250000,3.000000, 100)",
        "DELETE FROM `creature_movement_template` WHERE `entry`=1234 AND `point`=3",
        "DELETE FROM `creature_movement` WHERE `id`=42",
        "UPDATE `creature_movement` SET `position_x`=1.500000, `position_y`=-2.250000, `position_z`=3.000000 WHERE `id`=42 AND `point`=3",
        "UPDATE `creature_movement` SET `waittime`=5000 WHERE `id`=42 AND `point`=3",
        "UPDATE `creature_movement` SET `orientation`=0.500000 WHERE `id`=42 AND `point`=3",
        "UPDATE `creature_movement` SET `script_id`=77 WHERE `id`=42 AND `point`=3",
    }, __LINE__);
}