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

/// ReputationDataRepository's row reads on the global WorldDatabase backed by fakes: every column
/// of a canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/reputation/ReputationDataRepository.h"

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

        Db() : query(WorldDatabase), async(WorldDatabase), attached(WorldDatabase, &query, &async, &results)
        {
        }
    };
}

// Each column of a `reputation_reward_rate` row lands in its member, and the rows come back in
// order.
TEST(ReputationDataRepository_RewardRates)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001.5", "1002.5", "1003.5"},
        FakeRow{"2000", "2001.5", "2002.5", "2003.5"},
    });
    std::vector<ReputationRewardRateRow> rows = ReputationDataRepository::QueryRewardRates();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].faction, 1000u);
    CHECK_EQ(rows[0].questRate, 1001.5f);
    CHECK_EQ(rows[0].creatureRate, 1002.5f);
    CHECK_EQ(rows[0].spellRate, 1003.5f);
    CHECK_EQ(rows[1].faction, 2000u);
}

// Each column of a `creature_onkill_reputation` row lands in its member, and the rows come back in
// order.
TEST(ReputationDataRepository_OnKillReputations)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1", "1004", "-1005", "1", "1007", "-1008", "109"},
        FakeRow{"2000", "2001", "2002", "1", "2004", "-2005", "1", "2007", "-2008", "209"},
    });
    std::vector<OnKillReputationRow> rows = ReputationDataRepository::QueryOnKillReputations();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].creatureId, 1000u);
    CHECK_EQ(rows[0].rewOnKillRepFaction1, 1001u);
    CHECK_EQ(rows[0].rewOnKillRepFaction2, 1002u);
    CHECK_EQ(rows[0].isTeamAward1, true);
    CHECK_EQ(rows[0].maxStanding1, 1004u);
    CHECK_EQ(rows[0].rewOnKillRepValue1, -1005);
    CHECK_EQ(rows[0].isTeamAward2, true);
    CHECK_EQ(rows[0].maxStanding2, 1007u);
    CHECK_EQ(rows[0].rewOnKillRepValue2, -1008);
    CHECK_EQ(rows[0].teamDependent, uint8(109));
    CHECK_EQ(rows[1].creatureId, 2000u);
}

// Each column of a `reputation_spillover_template` row lands in its member, and the rows come back
// in order.
TEST(ReputationDataRepository_Spillovers)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002.5", "1003", "1004", "1005.5", "1006", "1007", "1008.5",
            "1009", "1010", "1011.5", "1012", "1013", "1014.5", "1015"},
        FakeRow{"2000", "2001", "2002.5", "2003", "2004", "2005.5", "2006", "2007", "2008.5",
            "2009", "2010", "2011.5", "2012", "2013", "2014.5", "2015"},
    });
    std::vector<ReputationSpilloverRow> rows = ReputationDataRepository::QuerySpillovers();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].faction, 1000u);
    CHECK_EQ(rows[0].faction1, 1001u);
    CHECK_EQ(rows[0].rate1, 1002.5f);
    CHECK_EQ(rows[0].rank1, 1003u);
    CHECK_EQ(rows[0].faction2, 1004u);
    CHECK_EQ(rows[0].rate2, 1005.5f);
    CHECK_EQ(rows[0].rank2, 1006u);
    CHECK_EQ(rows[0].faction3, 1007u);
    CHECK_EQ(rows[0].rate3, 1008.5f);
    CHECK_EQ(rows[0].rank3, 1009u);
    CHECK_EQ(rows[0].faction4, 1010u);
    CHECK_EQ(rows[0].rate4, 1011.5f);
    CHECK_EQ(rows[0].rank4, 1012u);
    CHECK_EQ(rows[0].faction5, 1013u);
    CHECK_EQ(rows[0].rate5, 1014.5f);
    CHECK_EQ(rows[0].rank5, 1015u);
    CHECK_EQ(rows[1].faction, 2000u);
}
