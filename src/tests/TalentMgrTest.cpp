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

/// Decoupling D4c: a character's talents and specs, loaded, kept, priced and saved with no character.
///
/// Before this PR the per-spec talent maps, the primary trees, the active spec and spec count,
/// the talent points and the reset cost and time lived on the character class, which this
/// binary cannot construct (its constructor reads the session, the world config and builds a
/// gossip menu). TalentMgr takes the character's guid, class mask, level and class, the game
/// time, the talent rate and the quest-reward bonus as parameters, so every case here builds
/// one from nothing.
///
/// The talent DBC stores and the spell store are seeded with DBCStorage::SetEntry, the path
/// production uses for runtime entries (Transports.cpp). SetEntry can overwrite an entry but
/// never remove one, so the ids below are used by no other test in this binary (none seeds
/// these stores) and every case seeds the same entries through SeedStores(). The one case that
/// needs an entry missing -- the level-55 row of NumTalentsAtLevel.dbc -- overwrites it with
/// NULL, which LookupEntry returns as "no entry", and a guard puts the row back.
///
/// The statements run through the GLOBAL CharacterDatabase with D7a's fakes attached and
/// asynchronous writes on: they are queued, and what the delay thread would have sent to MySQL
/// is the exact SQL asserted after ExecuteQueuedForTest().

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "DBCStores.h"
#include "TalentMgr.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace
{
    const uint32 kGuid = 42;
    const uint32 kOwnClassMask = 1 << (CLASS_WARRIOR - 1);
    const uint32 kOtherClassMask = 1 << (CLASS_MAGE - 1);

    // Talent tabs. 90199 is deliberately never seeded.
    const uint32 kTabOwn = 90101;
    const uint32 kTabOther = 90102;
    const uint32 kTabMissing = 90199;

    // Talents. 90999 is deliberately never seeded.
    const uint32 kTalentA = 90001;      // own tab, ranks 0-2 (91001..91003)
    const uint32 kTalentB = 90002;      // own tab, rank 0 only (91011)
    const uint32 kTalentOtherClass = 90003;
    const uint32 kTalentNoTab = 90004;
    const uint32 kTalentC = 90005;      // own tab, ranks 0-1 (91041, 91042)
    const uint32 kTalentD = 90006;      // own tab, ranks 0-1 (91051, 91052)
    const uint32 kTalentE = 90007;      // own tab, ranks 0-1 (91061, 91062)
    const uint32 kTalentUnknown = 90999;

    TalentTabEntry s_tabOwn = { kTabOwn, kOwnClassMask, 0, 0, 0, { 0, 0 } };
    TalentTabEntry s_tabOther = { kTabOther, kOtherClassMask, 0, 0, 0, { 0, 0 } };

    TalentEntry s_talentA = { kTalentA, kTabOwn, 0, 0, { 91001, 91002, 91003, 0, 0 }, 0, 0 };
    TalentEntry s_talentB = { kTalentB, kTabOwn, 0, 1, { 91011, 0, 0, 0, 0 }, 0, 0 };
    TalentEntry s_talentOtherClass = { kTalentOtherClass, kTabOther, 0, 0, { 91021, 0, 0, 0, 0 }, 0, 0 };
    TalentEntry s_talentNoTab = { kTalentNoTab, kTabMissing, 0, 0, { 91031, 0, 0, 0, 0 }, 0, 0 };
    TalentEntry s_talentC = { kTalentC, kTabOwn, 1, 0, { 91041, 91042, 0, 0, 0 }, 0, 0 };
    TalentEntry s_talentD = { kTalentD, kTabOwn, 1, 1, { 91051, 91052, 0, 0, 0 }, 0, 0 };
    TalentEntry s_talentE = { kTalentE, kTabOwn, 2, 0, { 91061, 91062, 0, 0, 0 }, 0, 0 };

    // NumTalentsAtLevel.dbc, keyed by level. 42 is deliberately absent, 100 is the top.
    NumTalentsAtLevelEntry s_level10 = { 1.0f };
    NumTalentsAtLevelEntry s_level20 = { 6.0f };
    NumTalentsAtLevelEntry s_level54 = { 18.0f };
    NumTalentsAtLevelEntry s_level55 = { 19.0f };
    NumTalentsAtLevelEntry s_level60 = { 21.0f };
    NumTalentsAtLevelEntry s_level85 = { 41.0f };
    NumTalentsAtLevelEntry s_level100 = { 41.0f };

    // Spell.dbc rows for talent A's ranks 1 and 2 (91002, 91003); every other talent spell id
    // above is deliberately absent from the spell store. SpellEntry cannot be built as a value
    // (it declares a private copy constructor, so it has no default one); the DBC loader makes
    // its rows the same way, as zeroed raw storage the store holds a pointer into.
    const uint32 kSpellARank1 = 91002;
    const uint32 kSpellARank2 = 91003;
    alignas(SpellEntry) unsigned char s_spellARank1Bytes[sizeof(SpellEntry)] = {};
    alignas(SpellEntry) unsigned char s_spellARank2Bytes[sizeof(SpellEntry)] = {};

    SpellEntry* SpellRow(unsigned char* bytes, uint32 id)
    {
        SpellEntry* row = reinterpret_cast<SpellEntry*>(bytes);
        row->ID = id;
        return row;
    }

    SpellEntry const* SpellARank1() { return reinterpret_cast<SpellEntry const*>(s_spellARank1Bytes); }
    SpellEntry const* SpellARank2() { return reinterpret_cast<SpellEntry const*>(s_spellARank2Bytes); }

    void SeedStores()
    {
        static bool seeded = false;
        if (seeded)
        {
            return;
        }
        seeded = true;

        sTalentTabStore.SetEntry(kTabOwn, &s_tabOwn);
        sTalentTabStore.SetEntry(kTabOther, &s_tabOther);

        sTalentStore.SetEntry(kTalentA, &s_talentA);
        sTalentStore.SetEntry(kTalentB, &s_talentB);
        sTalentStore.SetEntry(kTalentOtherClass, &s_talentOtherClass);
        sTalentStore.SetEntry(kTalentNoTab, &s_talentNoTab);
        sTalentStore.SetEntry(kTalentC, &s_talentC);
        sTalentStore.SetEntry(kTalentD, &s_talentD);
        sTalentStore.SetEntry(kTalentE, &s_talentE);

        sNumTalentsAtLevelStore.SetEntry(10, &s_level10);
        sNumTalentsAtLevelStore.SetEntry(20, &s_level20);
        sNumTalentsAtLevelStore.SetEntry(54, &s_level54);
        sNumTalentsAtLevelStore.SetEntry(55, &s_level55);
        sNumTalentsAtLevelStore.SetEntry(60, &s_level60);
        sNumTalentsAtLevelStore.SetEntry(85, &s_level85);
        sNumTalentsAtLevelStore.SetEntry(100, &s_level100);

        sSpellStore.SetEntry(kSpellARank1, SpellRow(s_spellARank1Bytes, kSpellARank1));
        sSpellStore.SetEntry(kSpellARank2, SpellRow(s_spellARank2Bytes, kSpellARank2));
    }

    /// The level-55 row of NumTalentsAtLevel.dbc missing for the guard's lifetime.
    class Level55Missing
    {
        public:
            Level55Missing() { sNumTalentsAtLevelStore.SetEntry(55, NULL); }
            ~Level55Missing() { sNumTalentsAtLevelStore.SetEntry(55, &s_level55); }

            Level55Missing(Level55Missing const&) = delete;
            Level55Missing& operator=(Level55Missing const&) = delete;
    };

    /// One `character_talent` row: talent_id, current_rank, spec.
    FakeRow TalentRow(uint32 talentId, uint32 rank, uint32 spec)
    {
        FakeRow row;
        row.push_back(std::to_string(talentId));
        row.push_back(std::to_string(rank));
        row.push_back(std::to_string(spec));
        return row;
    }

    std::string DeleteSql(uint32 talentId, uint32 spec)
    {
        return "DELETE FROM `character_talent` WHERE `guid` = '42' and `talent_id` = '" + std::to_string(talentId)
               + "' and `spec` = '" + std::to_string(spec) + "'";
    }

    std::string InsertSql(uint32 talentId, uint32 rank, uint32 spec)
    {
        return "INSERT INTO `character_talent` (`guid`, `talent_id`, `current_rank`, `spec`) VALUES ('42', '"
               + std::to_string(talentId) + "', '" + std::to_string(rank) + "', '" + std::to_string(spec) + "')";
    }

    /// The talent and rank a talent spell stands for, as sTalentSpellPosMap would say.
    struct SpellPos
    {
        uint32 talentId;
        uint32 rank;
    };

    std::map<uint32, SpellPos> SpellPositions()
    {
        std::map<uint32, SpellPos> positions;
        TalentEntry const* talents[] = { &s_talentA, &s_talentB, &s_talentC, &s_talentD, &s_talentE };
        for (TalentEntry const* talent : talents)
        {
            for (uint32 rank = 0; rank < MAX_TALENT_RANK; ++rank)
            {
                if (talent->SpellRank[rank])
                {
                    positions[talent->SpellRank[rank]] = SpellPos{ talent->ID, rank };
                }
            }
        }
        return positions;
    }

    /// What the owner's addSpell does to the manager for a talent spell (the talent-map half
    /// and the point count; the spell book, the packets and the free-point refresh are the
    /// owner's own).
    void LearnSpell(TalentMgr& mgr, uint32 spellId, bool inWorld)
    {
        SpellPos pos = SpellPositions().at(spellId);
        mgr.LearnRank(pos.talentId, pos.rank, inWorld);
        mgr.SpendPoints(pos.rank + 1);
    }

    /// What the owner's removeSpell does to the manager for a talent spell.
    bool UnlearnSpell(TalentMgr& mgr, uint32 spellId)
    {
        SpellPos pos = SpellPositions().at(spellId);
        bool held = mgr.UnlearnRank(pos.talentId);
        mgr.RefundPoints(pos.rank + 1);
        return held;
    }

    PlayerTalent const* Find(TalentMgr const& mgr, uint8 spec, uint32 talentId)
    {
        PlayerTalentMap::const_iterator itr = mgr.Talents(spec).find(talentId);
        return itr != mgr.Talents(spec).end() ? &itr->second : NULL;
    }
}

TEST(TalentMgr_StartsWithOneEmptySpecAndNoPoints)
{
    TalentMgr mgr;

    CHECK_EQ(uint32(mgr.ActiveSpec()), 0u);
    CHECK_EQ(uint32(mgr.SpecsCount()), 1u);
    for (uint8 spec = 0; spec < MAX_TALENT_SPEC_COUNT; ++spec)
    {
        CHECK(mgr.Talents(spec).empty());
        CHECK_EQ(mgr.PrimaryTree(spec), 0u);
    }
    CHECK_EQ(mgr.FreePoints(), 0u);
    CHECK_EQ(mgr.UsedPoints(), 0u);
    CHECK_EQ(mgr.ResetCost(), 0u);
    CHECK_EQ(int64(mgr.ResetTime()), int64(0));
    CHECK(mgr.GetKnownTalentById(int32(kTalentA)) == NULL);
    CHECK(mgr.GetKnownTalentRankById(int32(kTalentA)) == NULL);
}

// resetTalentsCost, every branch, over (last cost, last reset time, now). MONTH is 30 days.
TEST(TalentMgr_ResetTalentsCostGoldenTable)
{
    const time_t now = time_t(1700000000);
    const time_t month = time_t(MONTH);

    struct Row
    {
        uint32 cost;
        time_t resetTime;
        uint32 expected;
    };

    const Row rows[] =
    {
        // Below 1 gold: the first reset costs 1 gold, whatever the time.
        { 0,       0,                    10000 },
        { 9999,    now,                  10000 },
        // Below 5 gold: 5 gold.
        { 10000,   now,                  50000 },
        { 49999,   now - 10 * month,     50000 },
        // Below 10 gold: 10 gold.
        { 50000,   now,                  100000 },
        { 99999,   now - 10 * month,     100000 },
        // 10 gold or more, less than a month since the last reset: +5 gold, capped at 50 gold.
        { 100000,  now,                  150000 },
        { 300000,  now - (month - 1),    350000 },
        { 450000,  now,                  500000 },   // exactly the cap
        { 460000,  now,                  500000 },   // over the cap
        { 500000,  now,                  500000 },
        { 1000000, now,                  500000 },
        // The last reset in the future: a negative month count is not "months > 0", so +5 gold.
        { 200000,  now + 2 * month,      250000 },
        // Whole months since the last reset: -5 gold per month, down to 10 gold.
        { 300000,  now - month,          250000 },
        { 300000,  now - 3 * month,      150000 },
        { 300000,  now - 4 * month,      100000 },   // exactly the floor
        { 300000,  now - 5 * month,      100000 },   // under the floor
        { 1000000, now - month,          950000 },   // the decay starts from the stored cost, not the cap
        // Never reset (time 0) with a stored cost of 10 gold or more: hundreds of months, the floor.
        { 300000,  0,                    100000 },
    };

    for (Row const& row : rows)
    {
        TalentMgr mgr;
        mgr.SetResetCost(row.cost);
        mgr.SetResetTime(row.resetTime);
        CHECK_EQ(mgr.ResetTalentsCost(now), row.expected);
        // A query: the stored cost and time are untouched.
        CHECK_EQ(mgr.ResetCost(), row.cost);
        CHECK_EQ(int64(mgr.ResetTime()), int64(row.resetTime));
    }
}

TEST(TalentMgr_CalculateTalentsPointsTable)
{
    SeedStores();

    struct Row
    {
        uint32 level;
        uint8 classId;
        float rate;
        uint32 questBonus;
        uint32 expected;
    };

    const Row rows[] =
    {
        // Every class but the death knight: the level's count times the rate, the bonus ignored.
        { 10,  CLASS_WARRIOR,      1.0f, 0,  1 },
        { 20,  CLASS_MAGE,         1.0f, 0,  6 },
        { 20,  CLASS_MAGE,         2.0f, 0,  12 },
        { 20,  CLASS_MAGE,         1.5f, 0,  9 },
        { 85,  CLASS_PRIEST,       0.5f, 0,  20 },   // 20.5 truncated
        { 85,  CLASS_PRIEST,       1.0f, 7,  41 },
        // No NumTalentsAtLevel entry for the level: nothing.
        { 42,  CLASS_WARRIOR,      1.0f, 0,  0 },
        { 0,   CLASS_WARRIOR,      1.0f, 0,  0 },
        // Above 100 the level-100 entry is read.
        { 120, CLASS_WARRIOR,      1.0f, 0,  41 },
        // Death knight: nothing below 55 even with a bonus ...
        { 54,  CLASS_DEATH_KNIGHT, 1.0f, 10, 0 },
        { 42,  CLASS_DEATH_KNIGHT, 1.0f, 10, 0 },    // (no entry at all)
        // ... then the levels past 55 plus the quest-reward bonus ...
        { 55,  CLASS_DEATH_KNIGHT, 1.0f, 0,  0 },
        { 60,  CLASS_DEATH_KNIGHT, 1.0f, 0,  2 },
        { 60,  CLASS_DEATH_KNIGHT, 1.0f, 5,  7 },
        { 60,  CLASS_DEATH_KNIGHT, 2.0f, 5,  14 },
        // ... capped at the other classes' count for the level, before the rate.
        { 60,  CLASS_DEATH_KNIGHT, 1.0f, 30, 21 },
        { 60,  CLASS_DEATH_KNIGHT, 0.5f, 30, 10 },
        { 85,  CLASS_DEATH_KNIGHT, 1.0f, 51, 41 },
        { 85,  CLASS_DEATH_KNIGHT, 1.0f, 3,  25 },
    };

    for (Row const& row : rows)
    {
        CHECK_EQ(TalentMgr::CalculateTalentsPoints(row.level, row.classId, row.rate, row.questBonus), row.expected);
    }
}

// The death knight's points are counted from level 55's entry. Without it the branch returns
// nothing, while the other classes -- which never read it -- are unaffected; with it back, the
// same rows count again.
TEST(TalentMgr_DeathKnightPointsNeedTheLevel55Entry)
{
    SeedStores();

    // The entry is there: levels past 55 plus the bonus, capped before the rate.
    CHECK_EQ(TalentMgr::CalculateTalentsPoints(60, CLASS_DEATH_KNIGHT, 1.0f, 5), 7u);
    CHECK_EQ(TalentMgr::CalculateTalentsPoints(85, CLASS_DEATH_KNIGHT, 2.0f, 51), 82u);
    CHECK_EQ(TalentMgr::CalculateTalentsPoints(55, CLASS_DEATH_KNIGHT, 1.0f, 4), 4u);

    {
        Level55Missing missing;
        REQUIRE(sNumTalentsAtLevelStore.LookupEntry(55) == NULL);
        REQUIRE(sNumTalentsAtLevelStore.LookupEntry(60) != NULL);

        // The level's own entry is there, level 55's is not: nothing, whatever the bonus or rate.
        CHECK_EQ(TalentMgr::CalculateTalentsPoints(60, CLASS_DEATH_KNIGHT, 1.0f, 5), 0u);
        CHECK_EQ(TalentMgr::CalculateTalentsPoints(85, CLASS_DEATH_KNIGHT, 2.0f, 51), 0u);
        // At level 55 itself the level's own entry is the missing one: nothing either.
        CHECK_EQ(TalentMgr::CalculateTalentsPoints(55, CLASS_DEATH_KNIGHT, 1.0f, 4), 0u);
        // Other classes never read level 55's entry.
        CHECK_EQ(TalentMgr::CalculateTalentsPoints(60, CLASS_WARRIOR, 1.0f, 5), 21u);
        CHECK_EQ(TalentMgr::CalculateTalentsPoints(85, CLASS_PRIEST, 0.5f, 0), 20u);
    }

    // Back again.
    REQUIRE(sNumTalentsAtLevelStore.LookupEntry(55) == &s_level55);
    CHECK_EQ(TalentMgr::CalculateTalentsPoints(60, CLASS_DEATH_KNIGHT, 1.0f, 5), 7u);
    CHECK_EQ(TalentMgr::CalculateTalentsPoints(85, CLASS_DEATH_KNIGHT, 2.0f, 51), 82u);
}

// The learned rank's spell: the active spec's entry, its current rank, that rank's Spell.dbc row.
TEST(TalentMgr_KnownTalentRankIsTheLearnedRanksSpellRow)
{
    SeedStores();
    TalentMgr mgr;

    // Not held: no lookup result.
    CHECK(mgr.GetKnownTalentRankById(int32(kTalentA)) == NULL);

    // Rank 2 of A -> SpellRank[2] = 91003 -> the seeded row.
    mgr.LearnRank(kTalentA, 2, false);
    SpellEntry const* spell = mgr.GetKnownTalentRankById(int32(kTalentA));
    REQUIRE(spell != NULL);
    CHECK(spell == SpellARank2());
    CHECK_EQ(spell->ID, kSpellARank2);

    // The rank moves, the row follows: rank 1 -> 91002.
    mgr.LearnRank(kTalentA, 1, true);
    spell = mgr.GetKnownTalentRankById(int32(kTalentA));
    REQUIRE(spell != NULL);
    CHECK(spell == SpellARank1());
    CHECK_EQ(spell->ID, kSpellARank1);

    // Held, but its rank's spell (91011) has no Spell.dbc row: NULL from the store, not from the map.
    mgr.LearnRank(kTalentB, 0, false);
    CHECK(mgr.GetKnownTalentById(int32(kTalentB)) != NULL);
    CHECK(mgr.GetKnownTalentRankById(int32(kTalentB)) == NULL);

    // Another spec is invisible until it is the active one.
    mgr.SetActiveSpec(1);
    CHECK(mgr.GetKnownTalentRankById(int32(kTalentA)) == NULL);
    mgr.LearnRank(kTalentA, 2, false);
    CHECK(mgr.GetKnownTalentRankById(int32(kTalentA)) == SpellARank2());
    mgr.SetActiveSpec(0);
    CHECK(mgr.GetKnownTalentRankById(int32(kTalentA)) == SpellARank1());

    // A removed entry is not known: NULL although its spell row exists.
    CHECK(mgr.UnlearnRank(kTalentA));
    CHECK(mgr.GetKnownTalentRankById(int32(kTalentA)) == NULL);
}

TEST(TalentMgr_LearnAndUnlearnKeepTheActiveSpecsMap)
{
    SeedStores();
    TalentMgr mgr;

    // Loading (not in world): a new entry is UNCHANGED, it points at the DBC row.
    mgr.LearnRank(kTalentA, 1, false);
    PlayerTalent const* a = Find(mgr, 0, kTalentA);
    REQUIRE(a != NULL);
    CHECK(a->talentEntry == &s_talentA);
    CHECK_EQ(a->currentRank, 1u);
    CHECK_EQ(uint32(a->state), uint32(PLAYERSPELL_UNCHANGED));

    // The same rank again changes nothing.
    mgr.LearnRank(kTalentA, 1, true);
    CHECK_EQ(uint32(Find(mgr, 0, kTalentA)->state), uint32(PLAYERSPELL_UNCHANGED));

    // Another rank: the rank moves and the entry becomes CHANGED.
    mgr.LearnRank(kTalentA, 2, true);
    CHECK_EQ(Find(mgr, 0, kTalentA)->currentRank, 2u);
    CHECK_EQ(uint32(Find(mgr, 0, kTalentA)->state), uint32(PLAYERSPELL_CHANGED));

    // In world: a new entry is NEW, and another rank keeps it NEW.
    mgr.LearnRank(kTalentB, 0, true);
    CHECK_EQ(uint32(Find(mgr, 0, kTalentB)->state), uint32(PLAYERSPELL_NEW));
    mgr.LearnRank(kTalentC, 0, true);
    mgr.LearnRank(kTalentC, 1, true);
    CHECK_EQ(Find(mgr, 0, kTalentC)->currentRank, 1u);
    CHECK_EQ(uint32(Find(mgr, 0, kTalentC)->state), uint32(PLAYERSPELL_NEW));

    // The known-talent query sees the active spec's held entries only.
    CHECK(mgr.GetKnownTalentById(int32(kTalentA)) == Find(mgr, 0, kTalentA));
    CHECK(mgr.GetKnownTalentById(int32(kTalentUnknown)) == NULL);
    // Held, so the learned rank's spell is looked up: rank 2 of A is Spell.dbc row 91003.
    CHECK(mgr.GetKnownTalentRankById(int32(kTalentA)) == SpellARank2());

    // Unlearning a saved entry marks it REMOVED; the query no longer sees it.
    CHECK(mgr.UnlearnRank(kTalentA));
    CHECK_EQ(uint32(Find(mgr, 0, kTalentA)->state), uint32(PLAYERSPELL_REMOVED));
    CHECK(mgr.GetKnownTalentById(int32(kTalentA)) == NULL);

    // Learning a REMOVED entry again, even at its old rank, makes it CHANGED.
    mgr.LearnRank(kTalentA, 2, true);
    CHECK_EQ(uint32(Find(mgr, 0, kTalentA)->state), uint32(PLAYERSPELL_CHANGED));

    // Unlearning a NEW entry erases it; a talent the spec does not hold answers false.
    CHECK(mgr.UnlearnRank(kTalentB));
    CHECK(Find(mgr, 0, kTalentB) == NULL);
    CHECK(!mgr.UnlearnRank(kTalentB));
    CHECK(!mgr.UnlearnRank(kTalentUnknown));

    // Everything went into the active spec.
    CHECK(mgr.Talents(1).empty());

    // With spec 1 active the same calls land there, and spec 0 is invisible to the query.
    mgr.SetActiveSpec(1);
    CHECK(mgr.GetKnownTalentById(int32(kTalentA)) == NULL);
    mgr.LearnRank(kTalentD, 1, false);
    CHECK(Find(mgr, 1, kTalentD) != NULL);
    CHECK(Find(mgr, 0, kTalentD) == NULL);
    CHECK(!mgr.UnlearnRank(kTalentC));                  // held by spec 0 only
    CHECK(Find(mgr, 0, kTalentC) != NULL);
}

TEST(TalentMgr_PointsNeverGoBelowZero)
{
    TalentMgr mgr;

    mgr.SpendPoints(3);
    mgr.SpendPoints(2);
    CHECK_EQ(mgr.UsedPoints(), 5u);
    mgr.RefundPoints(3);
    CHECK_EQ(mgr.UsedPoints(), 2u);
    // A refund equal to the count is not "less than" it: the count is set to zero.
    mgr.RefundPoints(2);
    CHECK_EQ(mgr.UsedPoints(), 0u);
    mgr.SpendPoints(1);
    mgr.RefundPoints(10);
    CHECK_EQ(mgr.UsedPoints(), 0u);
    mgr.RefundPoints(1);
    CHECK_EQ(mgr.UsedPoints(), 0u);
}

// One pass over the rows, as the owner's loop does it: every validation in order, the DELETE of
// a failing row (three of them for every character), the inactive-spec insertion, and the
// active-spec learn called back at the exact point of its row. The sequence below interleaves
// the statements the database was handed with the callbacks, in the order they happened.
TEST(TalentMgr_LoadRowInterleavesDeletesAndActiveSpellLearns)
{
    SeedStores();
    TalentMgr mgr;

    TickGuard::ResetViolations();
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    std::vector<std::string> events;
    size_t seen = 0;
    auto sync = [&]()
    {
        CharacterDatabase.ExecuteQueuedForTest();
        for (; seen < async.executed.size(); ++seen)
        {
            events.push_back(async.executed[seen]);
        }
    };

    // The owner's callback learns the spell; here it records the call, and does to the manager
    // what the owner's addSpell does, so the active rows end up in the active spec's map.
    TalentMgr::ActiveSpellCallback const onActiveSpell = [&](uint32 spellId)
    {
        sync();
        events.push_back("learn " + std::to_string(spellId));
        LearnSpell(mgr, spellId, /*inWorld*/ false);
    };

    FakeQueryResult result(FakeRows{
        TalentRow(kTalentA, 1, 0),              // active spec: learned
        TalentRow(kTalentUnknown, 0, 0),        // no talent: DELETE for every character
        TalentRow(kTalentNoTab, 0, 0),          // no talent tab: DELETE for every character
        TalentRow(kTalentC, 1, 1),              // inactive spec: stored, no statement
        TalentRow(kTalentOtherClass, 0, 0),     // another class's tab: DELETE for this character
        TalentRow(kTalentA, 3, 0),              // SpellRank[3] is 0: DELETE for this character
        TalentRow(kTalentB, 0, 0),              // active spec: learned
        TalentRow(kTalentA, 6, 0),              // rank above MAX_TALENT_RANK: DELETE for this character
        TalentRow(kTalentB, 0, 3),              // spec above MAX_TALENT_SPEC_COUNT: DELETE for every character
        TalentRow(kTalentB, 0, 2),              // spec 2 passes the check above (off by one) and fails the spec count
        TalentRow(kTalentA, 2, 1),              // inactive spec: stored, no statement
    });
    REQUIRE(result.NextRow());
    do
    {
        mgr.LoadRow(result.Fetch(), kGuid, kOwnClassMask, /*specsCount*/ 2, /*activeSpec*/ 0, onActiveSpell);
    }
    while (result.NextRow());
    sync();

    // A learn between two DELETEs is where its row was: the callback runs inside its row, not
    // after the loop.
    const std::vector<std::string> expected =
    {
        "learn 91002",
        "DELETE FROM `character_talent` WHERE `talent_id` = '90999'",
        "DELETE FROM `character_talent` WHERE `talent_id` = '90004'",
        "DELETE FROM `character_talent` WHERE `guid` = '42' AND `talent_id` = '90003'",
        "DELETE FROM `character_talent` WHERE `guid` = '42' AND `talent_id` = '90001'",
        "learn 91011",
        "DELETE FROM `character_talent` WHERE `guid` = '42' AND `talent_id` = '90001'",
        "DELETE FROM `character_talent` WHERE `spec` = '3' ",
        "DELETE FROM `character_talent` WHERE `guid` = '42' AND `spec` = '2' ",
    };
    REQUIRE(events.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
    {
        CHECK_STR(events[i], expected[i]);
    }
    CHECK_EQ(query.executed.size(), size_t(0));

    // The inactive rows are in spec 1, UNCHANGED, pointing at their DBC rows.
    CHECK_EQ(mgr.Talents(1).size(), size_t(2));
    PlayerTalent const* c = Find(mgr, 1, kTalentC);
    REQUIRE(c != NULL);
    CHECK(c->talentEntry == &s_talentC);
    CHECK_EQ(c->currentRank, 1u);
    CHECK_EQ(uint32(c->state), uint32(PLAYERSPELL_UNCHANGED));
    PlayerTalent const* a1 = Find(mgr, 1, kTalentA);
    REQUIRE(a1 != NULL);
    CHECK_EQ(a1->currentRank, 2u);
    CHECK_EQ(uint32(a1->state), uint32(PLAYERSPELL_UNCHANGED));

    // The active rows reached spec 0 only through the callback's learn: UNCHANGED (loading).
    CHECK_EQ(mgr.Talents(0).size(), size_t(2));
    CHECK_EQ(Find(mgr, 0, kTalentA)->currentRank, 1u);
    CHECK_EQ(uint32(Find(mgr, 0, kTalentA)->state), uint32(PLAYERSPELL_UNCHANGED));
    CHECK_EQ(uint32(Find(mgr, 0, kTalentB)->state), uint32(PLAYERSPELL_UNCHANGED));
    CHECK_EQ(mgr.UsedPoints(), 3u);                   // rank 1 of A (2) + rank 0 of B (1)

    // The spec count and the active spec are LoadRow's parameters, read by the owner at the start
    // of each row; the manager's own fields are not consulted. `other` keeps its defaults (one
    // spec, spec 0 active) throughout, and the callback only records here.
    events.clear();
    TalentMgr other;
    TalentMgr::ActiveSpellCallback const recordOnly = [&](uint32 spellId)
    {
        sync();
        events.push_back("learn " + std::to_string(spellId));
    };
    FakeQueryResult second(FakeRows{
        TalentRow(kTalentD, 0, 1),              // spec 1 with a spec count of 1: DELETE for this character
        TalentRow(kTalentD, 1, 1),              // spec 1 active, spec count 2: learned
        TalentRow(kTalentE, 0, 0),              // spec 0 inactive, spec count 2: stored
    });
    REQUIRE(second.NextRow());
    other.LoadRow(second.Fetch(), kGuid, kOwnClassMask, /*specsCount*/ 1, /*activeSpec*/ 0, recordOnly);
    REQUIRE(second.NextRow());
    other.LoadRow(second.Fetch(), kGuid, kOwnClassMask, /*specsCount*/ 2, /*activeSpec*/ 1, recordOnly);
    REQUIRE(second.NextRow());
    other.LoadRow(second.Fetch(), kGuid, kOwnClassMask, /*specsCount*/ 2, /*activeSpec*/ 1, recordOnly);
    CHECK(!second.NextRow());
    sync();
    REQUIRE(events.size() == size_t(2));
    CHECK_STR(events[0], "DELETE FROM `character_talent` WHERE `guid` = '42' AND `spec` = '1' ");
    CHECK_STR(events[1], "learn 91052");
    CHECK(Find(other, 0, kTalentE) != NULL);
    CHECK(other.Talents(1).empty());                    // an active row is never stored by LoadRow
    CHECK_EQ(uint32(other.SpecsCount()), 1u);
    CHECK_EQ(uint32(other.ActiveSpec()), 0u);

    // The class mask is a parameter too: another class's mask rejects a talent of the own tab.
    events.clear();
    FakeQueryResult third(FakeRows{ TalentRow(kTalentA, 0, 0) });
    REQUIRE(third.NextRow());
    other.LoadRow(third.Fetch(), kGuid, kOtherClassMask, 2, 0, recordOnly);
    sync();
    REQUIRE(events.size() == size_t(1));
    CHECK_STR(events[0], "DELETE FROM `character_talent` WHERE `guid` = '42' AND `talent_id` = '90001'");

    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(TalentMgr_SaveTalentsWritesNewChangedAndRemovedThenForgetsRemoved)
{
    SeedStores();
    TalentMgr mgr;

    // Spec 0: A loaded then re-ranked (CHANGED), B loaded (UNCHANGED), C loaded then unlearned
    // (REMOVED), D learned in world (NEW), E learned in world then unlearned (erased, never saved).
    mgr.LearnRank(kTalentA, 0, false);
    mgr.LearnRank(kTalentA, 2, true);
    mgr.LearnRank(kTalentB, 0, false);
    mgr.LearnRank(kTalentC, 1, false);
    CHECK(mgr.UnlearnRank(kTalentC));
    mgr.LearnRank(kTalentD, 1, true);
    mgr.LearnRank(kTalentE, 0, true);
    CHECK(mgr.UnlearnRank(kTalentE));
    // Spec 1: E learned in world (NEW), A loaded (UNCHANGED).
    mgr.SetActiveSpec(1);
    mgr.LearnRank(kTalentE, 1, true);
    mgr.LearnRank(kTalentA, 0, false);
    mgr.SetActiveSpec(0);

    // The statements follow each spec's map order, spec 0 first; per entry DELETE then INSERT.
    std::vector<std::string> expected;
    for (uint32 spec = 0; spec < MAX_TALENT_SPEC_COUNT; ++spec)
    {
        for (PlayerTalentMap::const_iterator itr = mgr.Talents(uint8(spec)).begin(); itr != mgr.Talents(uint8(spec)).end(); ++itr)
        {
            switch (itr->second.state)
            {
                case PLAYERSPELL_REMOVED:
                    expected.push_back(DeleteSql(itr->first, spec));
                    break;
                case PLAYERSPELL_CHANGED:
                    expected.push_back(DeleteSql(itr->first, spec));
                    expected.push_back(InsertSql(itr->first, itr->second.currentRank, spec));
                    break;
                case PLAYERSPELL_NEW:
                    expected.push_back(InsertSql(itr->first, itr->second.currentRank, spec));
                    break;
                default:
                    break;
            }
        }
    }
    // What that must come to, whatever the map order: spec 0 A (DELETE+INSERT rank 2), C (DELETE),
    // D (INSERT rank 1); spec 1 E (INSERT rank 1).
    REQUIRE(expected.size() == size_t(5));
    std::vector<std::string> sorted0(expected.begin(), expected.begin() + 4);
    std::sort(sorted0.begin(), sorted0.end());
    std::vector<std::string> want0 =
    {
        DeleteSql(kTalentA, 0), InsertSql(kTalentA, 2, 0), DeleteSql(kTalentC, 0), InsertSql(kTalentD, 1, 0)
    };
    std::sort(want0.begin(), want0.end());
    CHECK(sorted0 == want0);
    CHECK_STR(expected[4], InsertSql(kTalentE, 1, 1));

    TickGuard::ResetViolations();
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    {
        TickGuard::Scope scope;
        mgr.SaveTalents(kGuid);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(query.executed.size(), size_t(0));
    }

    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
    {
        CHECK_STR(async.executed[i], expected[i]);
    }
    CHECK_EQ(query.executed.size(), size_t(0));

    // REMOVED is gone, everything else is UNCHANGED.
    CHECK(Find(mgr, 0, kTalentC) == NULL);
    CHECK_EQ(mgr.Talents(0).size(), size_t(3));
    CHECK_EQ(mgr.Talents(1).size(), size_t(2));
    for (uint8 spec = 0; spec < MAX_TALENT_SPEC_COUNT; ++spec)
    {
        for (PlayerTalentMap::const_iterator itr = mgr.Talents(spec).begin(); itr != mgr.Talents(spec).end(); ++itr)
        {
            CHECK_EQ(uint32(itr->second.state), uint32(PLAYERSPELL_UNCHANGED));
        }
    }

    // A second save sends nothing.
    mgr.SaveTalents(kGuid);
    CharacterDatabase.ExecuteQueuedForTest();
    CHECK_EQ(async.executed.size(), expected.size());
    CHECK_EQ(TickGuard::Violations(), 0u);
}

// The owner's spec switch keeps running on the manager's maps. This replays its map operations
// in its order: copy the new spec's map, copy the old spec's map over the new one, switch, walk
// the active map unlearning what the new spec lacks (restarting the walk after each unlearn,
// since an unlearn may erase), then walk the copy learning through the add path and writing
// the copy's states back.
TEST(TalentMgr_ActivateSpecReplayOnTheManagersMaps)
{
    SeedStores();
    TalentMgr mgr;
    mgr.SetSpecsCount(2);

    // Spec 0 (active): A rank 2 and B rank 0 saved, C rank 1 learned this session (NEW).
    mgr.LearnRank(kTalentA, 2, false);
    mgr.LearnRank(kTalentB, 0, false);
    mgr.LearnRank(kTalentC, 1, true);
    // Spec 1: A rank 0 and D rank 1 saved, E rank 0 unlearned but not saved yet (REMOVED).
    mgr.SetActiveSpec(1);
    mgr.LearnRank(kTalentA, 0, false);
    mgr.LearnRank(kTalentD, 1, false);
    mgr.LearnRank(kTalentE, 0, false);
    CHECK(mgr.UnlearnRank(kTalentE));
    mgr.SetActiveSpec(0);

    // The maps are the manager's own: the same objects before and after, whatever the switch does.
    PlayerTalentMap* const spec0 = &mgr.Talents(0);
    PlayerTalentMap* const spec1 = &mgr.Talents(1);
    CHECK(&static_cast<TalentMgr const&>(mgr).Talents(1) == spec1);

    const uint8 specNum = 1;

    // copy of new talent spec (we will use it as model for converting current talent state to new)
    PlayerTalentMap tempSpec = mgr.Talents(specNum);

    // copy old spec talents to new one, before the switch
    mgr.Talents(specNum) = mgr.Talents(mgr.ActiveSpec());

    mgr.SetActiveSpec(specNum);

    // remove all talent spells that don't exist in next spec but exist in old
    std::vector<uint32> unlearned;
    for (PlayerTalentMap::iterator specIter = mgr.Talents(mgr.ActiveSpec()).begin(); specIter != mgr.Talents(mgr.ActiveSpec()).end();)
    {
        PlayerTalent& talent = specIter->second;

        if (talent.state == PLAYERSPELL_REMOVED)
        {
            ++specIter;
            continue;
        }

        PlayerTalentMap::iterator iterTempSpec = tempSpec.find(specIter->first);

        if (iterTempSpec == tempSpec.end() || iterTempSpec->second.state == PLAYERSPELL_REMOVED)
        {
            // the owner's removeSpell of the learned rank's spell reaches UnlearnRank
            unlearned.push_back(specIter->first);
            CHECK(UnlearnSpell(mgr, talent.talentEntry->SpellRank[talent.currentRank]));

            specIter = mgr.Talents(mgr.ActiveSpec()).begin();
        }
        else
        {
            ++specIter;
        }
    }
    // B (saved) and C (NEW) were the old spec's only talents the new spec lacks.
    std::sort(unlearned.begin(), unlearned.end());
    REQUIRE(unlearned.size() == size_t(2));
    CHECK_EQ(unlearned[0], kTalentB);
    CHECK_EQ(unlearned[1], kTalentC);
    CHECK_EQ(uint32(Find(mgr, 1, kTalentB)->state), uint32(PLAYERSPELL_REMOVED));
    CHECK(Find(mgr, 1, kTalentC) == NULL);                // NEW, so erased: why the walk restarts

    // now new spec data have only talents (maybe different rank) as in temp spec data, sync ranks then.
    std::vector<uint32> learned;
    for (PlayerTalentMap::const_iterator tempIter = tempSpec.begin(); tempIter != tempSpec.end(); ++tempIter)
    {
        PlayerTalent const& talent = tempIter->second;

        if (talent.state == PLAYERSPELL_REMOVED)
        {
            mgr.Talents(mgr.ActiveSpec())[tempIter->first] = talent;
            continue;
        }

        uint32 talentSpellId = talent.talentEntry->SpellRank[talent.currentRank];

        if (PlayerTalent const* cur_talent = mgr.GetKnownTalentById(tempIter->first))
        {
            if (cur_talent->currentRank != talent.currentRank)
            {
                learned.push_back(talentSpellId);
                LearnSpell(mgr, talentSpellId, /*inWorld*/ true);
            }
        }
        else
        {
            learned.push_back(talentSpellId);
            LearnSpell(mgr, talentSpellId, /*inWorld*/ true);
        }

        // sync states - original state is changed in the add path
        PlayerTalentMap::iterator specIter = mgr.Talents(mgr.ActiveSpec()).find(tempIter->first);
        REQUIRE(specIter != mgr.Talents(mgr.ActiveSpec()).end());
        specIter->second.state = talent.state;
    }
    // A's rank differs (2 -> 0) and D is missing: both learned through the add path.
    std::sort(learned.begin(), learned.end());
    REQUIRE(learned.size() == size_t(2));
    CHECK_EQ(learned[0], s_talentA.SpellRank[0]);
    CHECK_EQ(learned[1], s_talentD.SpellRank[1]);

    // The switch ran on the manager's own maps: no copy stood in for either.
    CHECK(&mgr.Talents(0) == spec0);
    CHECK(&mgr.Talents(1) == spec1);
    CHECK_EQ(uint32(mgr.ActiveSpec()), 1u);

    // Spec 1 now: A rank 0 and D rank 1 with their saved state restored, E restored as REMOVED,
    // B REMOVED (it came over from spec 0 and was unlearned), C gone.
    CHECK_EQ(mgr.Talents(1).size(), size_t(4));
    PlayerTalent const* a = Find(mgr, 1, kTalentA);
    PlayerTalent const* d = Find(mgr, 1, kTalentD);
    PlayerTalent const* e = Find(mgr, 1, kTalentE);
    PlayerTalent const* b = Find(mgr, 1, kTalentB);
    REQUIRE(a != NULL && d != NULL && e != NULL && b != NULL);
    CHECK_EQ(a->currentRank, 0u);
    CHECK_EQ(uint32(a->state), uint32(PLAYERSPELL_UNCHANGED));
    CHECK_EQ(d->currentRank, 1u);
    CHECK_EQ(uint32(d->state), uint32(PLAYERSPELL_UNCHANGED));
    CHECK(d->talentEntry == &s_talentD);
    CHECK_EQ(uint32(e->state), uint32(PLAYERSPELL_REMOVED));
    CHECK_EQ(uint32(b->state), uint32(PLAYERSPELL_REMOVED));
    CHECK(mgr.GetKnownTalentById(int32(kTalentB)) == NULL);
    CHECK(mgr.GetKnownTalentById(int32(kTalentA)) == a);

    // Spec 0 is as it was.
    CHECK_EQ(mgr.Talents(0).size(), size_t(3));
    CHECK_EQ(Find(mgr, 0, kTalentA)->currentRank, 2u);
    CHECK_EQ(uint32(Find(mgr, 0, kTalentC)->state), uint32(PLAYERSPELL_NEW));

    // And the save writes exactly that: spec 0's NEW C; spec 1's two REMOVED rows.
    TickGuard::ResetViolations();
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);
    mgr.SaveTalents(kGuid);
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(3));
    CHECK_STR(async.executed[0], InsertSql(kTalentC, 1, 0));
    std::vector<std::string> spec1Statements(async.executed.begin() + 1, async.executed.end());
    std::sort(spec1Statements.begin(), spec1Statements.end());
    std::vector<std::string> want1 = { DeleteSql(kTalentB, 1), DeleteSql(kTalentE, 1) };
    std::sort(want1.begin(), want1.end());
    CHECK(spec1Statements == want1);
    CHECK_EQ(mgr.Talents(1).size(), size_t(2));        // the REMOVED rows are erased
    CHECK(Find(mgr, 1, kTalentB) == NULL);
    CHECK(Find(mgr, 1, kTalentE) == NULL);
    CHECK_EQ(TickGuard::Violations(), 0u);
}
