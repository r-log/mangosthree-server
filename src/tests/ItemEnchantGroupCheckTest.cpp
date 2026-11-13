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

/// The random enchantment group check on a fake `item_enchantment_template`. Every member of a group
/// is looked up in its DBC store, so the result never depends on a roll: a group whose chances sum
/// below a hundred is valid, a member the store lacks is listed, and a group with no rows is missing.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "ItemEnchantGroupCheck.h"
#include "ItemEnchantmentMgr.h"

#include <vector>

namespace
{
    uint32 const kPropertyGroup = 990401;
    uint32 const kBrokenPropertyGroup = 990402;
    uint32 const kSuffixGroup = 990403;
    uint32 const kBrokenSuffixGroup = 990404;
    uint32 const kNoSuchGroup = 990405;

    uint32 const kPropertyA = 990411;
    uint32 const kPropertyB = 990412;
    uint32 const kSuffixA = 990421;
    uint32 const kUnknownEnchant = 990499;

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

    /// Loads `rows` as the `item_enchantment_template` table.
    void LoadEnchantments(FakeRows rows)
    {
        Db db;
        db.query.Answer("SELECT", std::move(rows));
        LoadRandomEnchantmentsTable();
    }

    /// Two random properties and one random suffix in the DBC stores, and four groups in the
    /// enchantment table, for the scope; all of it is dropped at its end.
    class SeededGroups
    {
        public:
            SeededGroups()
            {
                m_propertyA.ID = kPropertyA;
                m_propertyB.ID = kPropertyB;
                m_suffixA.ID = kSuffixA;
                sItemRandomPropertiesStore.SetEntry(kPropertyA, &m_propertyA);
                sItemRandomPropertiesStore.SetEntry(kPropertyB, &m_propertyB);
                sItemRandomSuffixStore.SetEntry(kSuffixA, &m_suffixA);

                LoadEnchantments(FakeRows{
                    FakeRow{"990401", "990411", "30"},
                    FakeRow{"990401", "990412", "20"},
                    FakeRow{"990402", "990411", "50"},
                    FakeRow{"990402", "990499", "50"},
                    FakeRow{"-990403", "990421", "10"},
                    FakeRow{"-990404", "990421", "60"},
                    FakeRow{"-990404", "990411", "40"},
                });
            }

            ~SeededGroups()
            {
                LoadEnchantments(FakeRows{});
                sItemRandomSuffixStore.SetEntry(kSuffixA, NULL);
                sItemRandomPropertiesStore.SetEntry(kPropertyB, NULL);
                sItemRandomPropertiesStore.SetEntry(kPropertyA, NULL);
            }

        private:
            ItemRandomPropertiesEntry m_propertyA = {};
            ItemRandomPropertiesEntry m_propertyB = {};
            ItemRandomSuffixEntry m_suffixA = {};
    };
}

TEST(ItemEnchantGroupCheck_AGroupWhoseChancesSumBelowAHundredIsValid)
{
    SeededGroups groups;

    ItemEnchantGroupCheck const property = CheckItemRandomPropertyGroup(kPropertyGroup);
    CHECK(property.hasRows);
    CHECK(property.unknownEnchants.empty());

    ItemEnchantGroupCheck const suffix = CheckItemRandomSuffixGroup(kSuffixGroup);
    CHECK(suffix.hasRows);
    CHECK(suffix.unknownEnchants.empty());
}

TEST(ItemEnchantGroupCheck_AMemberTheDbcStoreLacksIsReported)
{
    SeededGroups groups;

    ItemEnchantGroupCheck const property = CheckItemRandomPropertyGroup(kBrokenPropertyGroup);
    CHECK(property.hasRows);
    REQUIRE(property.unknownEnchants.size() == 1);
    CHECK_EQ(property.unknownEnchants[0], kUnknownEnchant);

    ItemEnchantGroupCheck const suffix = CheckItemRandomSuffixGroup(kBrokenSuffixGroup);
    CHECK(suffix.hasRows);
    REQUIRE(suffix.unknownEnchants.size() == 1);
    CHECK_EQ(suffix.unknownEnchants[0], kPropertyA);
}

TEST(ItemEnchantGroupCheck_AGroupWithNoRowsIsMissing)
{
    SeededGroups groups;

    ItemEnchantGroupCheck const property = CheckItemRandomPropertyGroup(kNoSuchGroup);
    CHECK(!property.hasRows);
    CHECK(property.unknownEnchants.empty());

    CHECK(!CheckItemRandomPropertyGroup(kSuffixGroup).hasRows);
    CHECK(!CheckItemRandomSuffixGroup(kPropertyGroup).hasRows);
}
