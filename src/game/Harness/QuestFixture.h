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

#ifndef MANGOS_HARNESS_QUEST_FIXTURE_H
#define MANGOS_HARNESS_QUEST_FIXTURE_H

#include "Platform/Define.h"
#include "SharedDefines.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class Quest;

/**
 * The quest family's guard (decoupling D4f0, design note §1-§2): the fingerprint that turns a
 * server owner's edit of a quest the family runs into a named INVALID, and the refusals that keep
 * every branch that would write to the character database out of the run. All of it is read from
 * the loaded world data and the configuration, BEFORE the scenario makes any server call and
 * before it spawns its player -- the spawn's own Player::Create moves achievement criteria too.
 */
namespace Harness
{
    /// How a scenario uses its quest. The pre-checks read everything else from the quest itself.
    struct QuestPlan
    {
        uint32 quest = 0;
        uint8  classId = CLASS_WARRIOR;     ///< the human's class SpawnPlayer builds
        uint32 level = 0;                   ///< the level the quest is rewarded at; 0 = the class's created level
        uint32 giverEntry = 0;              ///< the spawned creature giver; 0 = the player is the giver
        uint32 choice = 0;                  ///< the reward index RewardQuest is handed
        uint32 rewards = 1;                 ///< how many times the scenario rewards the quest
        std::map<uint32, uint32> kills;     ///< creature entry -> the credits the scenario supplies
        std::map<uint32, uint32> items;     ///< objective items the scenario stores itself -> count
        std::set<uint32> spells;            ///< spells the scenario teaches beyond the quest's own
        std::set<uint32> casts;             ///< spells cast beyond the quest's own
        std::set<uint32> skills;            ///< skills moved beyond the quest's own
        uint64 seededMoney = 0;             ///< money the scenario hands the player itself
    };

    /// Every objective slot and every reward field of a quest, by the column's name, as the loaded
    /// template holds it: ReqItemId/Count 1-6, ReqSourceId/Count 1-4, ReqCreatureOrGOId/Count and
    /// ReqSpellCast 1-4, ReqCurrencyId/Count 1-4, RepObjectiveFaction/Value, ReqSpellLearned,
    /// Method and SpecialFlags (the loader's derived bits included), and every reward column.
    typedef std::vector<std::pair<std::string, int64> > QuestFields;
    QuestFields ReadQuestFields(Quest const* quest);

    /// "" when every field equals `expected`, a field `expected` does not name counting as 0 --
    /// so every zero is compared too. Otherwise the first difference, as
    /// "quest 52: RewOrReqMoney 300, expected 250".
    std::string CompareQuestFields(uint32 questId, QuestFields const& actual, std::map<std::string, int64> const& expected);

    /// What the guard found. `refusal` is empty when the scenario may run.
    struct QuestPreCheck
    {
        std::string refusal;
        uint32 fields = 0;          ///< fields compared
        uint32 startLevel = 0;
        uint32 xpBound = 0;         ///< the most XP the scenario's rewards can give
        uint32 levelBound = 0;      ///< the highest level that XP can reach
        uint32 mailTrees = 0;       ///< achievements with a mail reward, each walked
    };

    /**
     * The refusals, in order, the first failure winning:
     *  1. the template exists and matches its fingerprint;
     *  2. QuestTracker.Enable is off (AddQuest and CompleteQuest write the tracker otherwise);
     *  3. no reward mail template, no DB start or complete script, not timed;
     *  4. the creature giver's template exists and binds no script;
     *  5. no level-mail row between the start level and the highest level the rewards' XP can
     *     reach (GiveXP's own arithmetic, XPValue at its full multiplier);
     *  6. the achievement closure (F1): no achievement with a mail reward -- nor any that its
     *     completion chains to -- can complete from what the scenario and the spawn's Create move.
     */
    QuestPreCheck CheckQuestPlan(QuestPlan const& plan, std::map<std::string, int64> const& expected);

    /// True for every achievement criteria type the closure in CheckQuestPlan judges. The closure
    /// reads any other type as unreachable, which is sound only while the run never fires it:
    /// AchievementMgr::GetCriteriaProgressMaxCounter answers 0 for a type outside its switch, so
    /// such a criteria completes on its first progress. A scenario therefore checks, at its
    /// verdict, that every criteria-update packet it recorded names a criteria of a modelled type
    /// (UnmodelledCriteriaTypes), and reports a BUG naming the type otherwise.
    bool ClosureModelsCriteriaType(uint32 type);

    /// The criteria types, by id, among `criteriaIds` that ClosureModelsCriteriaType does not
    /// model ("type <t> (criteria <id>)"), and in `types` every distinct type seen.
    std::vector<std::string> UnmodelledCriteriaTypes(std::vector<uint32> const& criteriaIds, std::set<uint32>& types);
}

#endif
