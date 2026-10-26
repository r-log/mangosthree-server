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
#include "persistence/world/ai/CreatureEventAIRepository.h"
#include "persistence/world/economy/LootDataRepository.h"
#include "persistence/world/economy/SkillDataRepository.h"
#include "persistence/world/maps/MapDataRepository.h"
#include "persistence/world/pvp/BattleGroundDataRepository.h"
#include "persistence/world/spells/SpellDataRepository.h"

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
    delete SpellDataRepository::QueryUsedSpells("spell_check");
    delete SpellDataRepository::QuerySpellAreas();
    delete SpellDataRepository::QuerySpellChains();
    delete SpellDataRepository::QueryLearnSpells();
    delete SpellDataRepository::QueryTargetPositions();
    delete SpellDataRepository::QueryBonuses();
    delete SpellDataRepository::QueryElixirs();
    delete SpellDataRepository::QueryThreats();
    delete SpellDataRepository::QueryPetAuras();
    delete SpellDataRepository::QueryProcEvents();
    delete SpellDataRepository::QueryProcItemEnchants();
    delete CreatureEventAIRepository::QuerySummons();
    delete CreatureEventAIRepository::QueryEvents();
    delete LootDataRepository::QueryLootTemplate("creature_loot_template");
    delete LootDataRepository::QueryItemEnchantments();
    delete SkillDataRepository::QueryDiscoveries();
    delete SkillDataRepository::QueryExtraItems();
    delete MapDataRepository::QueryTransports();
    delete MapDataRepository::QueryTransportGameObjects();
    delete MapDataRepository::QueryWeather();
    delete BattleGroundDataRepository::QueryTemplates();
    delete BattleGroundDataRepository::QueryBattleMasters();
    delete BattleGroundDataRepository::QueryEventIndexes();
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
