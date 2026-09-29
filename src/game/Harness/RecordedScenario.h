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

#ifndef MANGOS_HARNESS_RECORDED_SCENARIO_H
#define MANGOS_HARNESS_RECORDED_SCENARIO_H

#include "Scenario.h"
#include "Recorder.h"

#include <set>
#include <string>
#include <vector>

class Player;

namespace Harness
{
    struct QuestPlan;

    /**
     * What every recorded scenario shares, whichever family it belongs to (the quest family,
     * decoupling D4f0; the spell family, decoupling D11): the category list -- so the all-INVALID
     * verdict and the real one are built from one list -- and the two categories every recorded
     * scenario prints besides its own, noPersistence and digest. The family's recorder is the
     * scenario's own member; Rec() hands it to the shared categories.
     */
    class RecordedScenario : public Scenario
    {
    public:
        RecordedScenario(char const* name, int order, std::vector<char const*> const& categories)
            : Scenario(name, order), m_categories(categories) {}
        bool UsesPlayer() const override { return true; }

    protected:
        /// The scenario's recorder.
        virtual Recorder const& Rec() const = 0;

        /// Every category of the scenario INVALID(why): the taxi family's pattern, so a refusal
        /// keeps the category set stable.
        std::string Invalid(std::string const& why) const;

        /// "<category>=<value> | ..." in the list's order; a value count that does not match the
        /// list is the scenario's own bug, and says so in every category.
        std::string Compose(std::vector<std::string> const& values) const;

        /// The completed achievements, by id.
        static std::set<uint32> Achievements(Player* p);

        /**
         * noPersistence (the D4f0 note's §5): no mail sent, no mail-rewarded achievement
         * completed since the spawn, no mail level crossed since `levelFrom`, and the achievement
         * closure checked against what the run really fired (the D4f0-1 task review's M-2): every
         * criteria update the recorder saw names a criteria whose type the closure models, or the
         * pre-check proved nothing about it. `noReachLevel`: the scenario set its level with the
         * `.reset level` sequence and its rewards cannot cross a level, which is exactly when the
         * closure reads the run as reaching no REACH_LEVEL criteria (QuestFixture.cpp) -- so one
         * firing is a BUG too. `dealsDamage`: the plan's QuestPlan::dealsDamage, false for every
         * quest plan; the closure reads DAMAGE_DONE and HIGHEST_HIT_DEALT as reached exactly when
         * it is set, so one of them firing while it is not is a BUG (the backstop those two types
         * had while the closure did not model them). `spellPlan`: a spell scenario's plan (decoupling
         * D11 PR 2), whose takesDamage and heals the closure reads the same way for the
         * damage-received and healing types, and whose exploredAreas it matches against each fired
         * EXPLORE_AREA criteria's overlay (UnexploredAreaCriteria), a BUG for any it does not name;
         * NULL -- every quest scenario and 930 -- reads them all as false and empty, so one of those
         * types firing there is a BUG, as it was while the closure did not model them.
         */
        std::string NoPersistence(Player* p, std::set<uint32> const& achievementsAtSpawn, uint32 levelFrom, bool noReachLevel,
                                  bool dealsDamage, QuestPlan const* spellPlan = NULL) const;

        /// The digest category: FNV-1a over every digested TRACE line, from the first step on.
        std::string DigestValue(char const* from = "the accept") const;

        /// The `.reset level` sequence (PlayerMiscCommands.cpp, HandleResetLevelCommand) to `level`
        /// instead of the start level: the level-scaled item mods off, SetLevel, InitRunes,
        /// InitStatsForLevel(true), the taxi nodes, glyphs and talents for the level, XP 0, the mods
        /// back on. It reaches neither GiveLevel's REACH_LEVEL criteria nor its level mail. The
        /// command's sCharacterCache.UpdateLevel is left out: the cache holds no harness guid, so
        /// it does nothing (CharacterCache.cpp), and the harness keeps out of global state. Moved
        /// here verbatim from ScenariosQuest.cpp (decoupling D11 PR 2) so the spell family runs the
        /// same sequence.
        static void SetLevelAsResetDoes(Player* p, uint32 level);

    private:
        std::vector<char const*>   m_categories;
    };
}

#endif
