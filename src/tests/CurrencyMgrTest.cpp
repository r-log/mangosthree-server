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

/// Decoupling D4k: a character's currencies -- counts, caps, changes, flags, the weekly reset, the
/// four packets, the load and the save -- with no character.
///
/// Before this PR CurrencyMgr held a pointer to its owner and read the owner's currency-gain aura
/// multiplier, its in-world and loading state, its guid and the server's conquest week cap, and
/// wrote the owner's achievement criteria, session and quest log. Now the multiplier and the
/// in-world check are read callbacks and the cap a value (CurrencyInputs), the guid a parameter,
/// and the writes callbacks (ModifySinks), so every case builds one from nothing. A Wire records,
/// in call order, every read callback's call and every write callback's arguments, and for a quest
/// check the manager's count AT THAT MOMENT ("seen"): the owner's quest checks read the new count
/// (CanCompleteQuest and CurrencyRemovedQuestCheck call HasCurrencyCount), so the map must be
/// written before them.
///
/// The currency types store is seeded the way TalentMgrTest seeds its stores: DBCStorage::SetEntry,
/// the path production uses for runtime entries. No other test in this binary touches this store.
/// The ids 390 (conquest points), 483 and 484 (the two conquest metas) are the ones the manager
/// treats specially; 361, 395 and 396 are ordinary ids with the flags and caps each case needs;
/// 9999 is never seeded.
///
/// Packets are recorded as they would go on the wire: WorldSession::SendPacket flushes a packet's
/// pending bits before it sends (WorldSession.cpp), so the Wire flushes a copy the same way. Every
/// expected byte string is derived by hand in its comment (bits most significant first, padded
/// with zeros to a byte before the first whole value; values little-endian), and an independent
/// Python model of the byte buffer agreed with each.
///
/// The statements run through the GLOBAL CharacterDatabase with D7a's fakes attached and
/// asynchronous writes on: they are queued, and what the delay thread would have sent to MySQL is
/// the exact SQL asserted after ExecuteQueuedForTest(). The map is an unordered_map, so a case with
/// more than one statement compares them as a sorted list.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "DBCStores.h"
#include "CurrencyMgr.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace
{
    const uint32 kGuid = 42;
    const uint32 kConquestCap = 2700;       ///< the config's conquest week cap (the DBC row says 5000)

    // The opcodes, the criteria type and the currency constants the rows below expect, pinned to
    // their values: a row that compares against a constant from the same header would not see
    // that header change.
    static_assert(SMSG_SET_CURRENCY == 0x59B0, "SMSG_SET_CURRENCY is 0x59B0 in 4.3.4 15595");
    static_assert(SMSG_SET_CURRENCY_WEEK_LIMIT == 0x70A7, "SMSG_SET_CURRENCY_WEEK_LIMIT is 0x70A7 in 4.3.4 15595");
    static_assert(SMSG_SEND_CURRENCIES == 0x15A5, "SMSG_SEND_CURRENCIES is 0x15A5 in 4.3.4 15595");
    static_assert(SMSG_WEEKLY_RESET_CURRENCIES == 0x3CA1, "SMSG_WEEKLY_RESET_CURRENCIES is 0x3CA1 in 4.3.4 15595");
    static_assert(ACHIEVEMENT_CRITERIA_TYPE_CURRENCY_EARNED == 12, "the currency-earned criteria type is 12");
    static_assert(CURRENCY_CONQUEST_POINTS == 390 && CURRENCY_CONQUEST_ARENA_META == 483 && CURRENCY_CONQUEST_BG_META == 484,
                  "the three currencies the manager treats specially");
    static_assert(CURRENCY_CATEGORY_META == 89, "the meta category hides the client's message");
    static_assert(CURRENCY_FLAG_HAS_PRECISION == 0x08 && CURRENCY_FLAG_HAS_SEASON_COUNT == 0x80, "the two entry flags");
    static_assert(PLAYERCURRENCY_MASK_USED_BY_CLIENT == 0x0C, "the loaded flags keep 0x04 and 0x08");

    const uint32 kPlain = 361;              ///< no cap, no precision, no season
    const uint32 kPrecise = 395;            ///< precision (the client divides by 100)
    const uint32 kCapped = 396;             ///< total cap 4000, week cap 1000
    const uint32 kUnknown = 9999;           ///< never seeded

    // CurrencyTypes.dbc rows: { ID, CategoryID, Name_lang, MaxQty, MaxEarnablePerWeek, Flags }.
    CurrencyTypesEntry s_plain    = { kPlain, 0, NULL, 0, 0, 0 };
    CurrencyTypesEntry s_precise  = { kPrecise, 0, NULL, 0, 0, CURRENCY_FLAG_HAS_PRECISION };
    CurrencyTypesEntry s_capped   = { kCapped, 0, NULL, 4000, 1000, 0 };
    CurrencyTypesEntry s_conquest = { CURRENCY_CONQUEST_POINTS, 0, NULL, 0, 5000, CURRENCY_FLAG_HAS_PRECISION | CURRENCY_FLAG_HAS_SEASON_COUNT };
    CurrencyTypesEntry s_arenaMeta = { CURRENCY_CONQUEST_ARENA_META, CURRENCY_CATEGORY_META, NULL, 0, 0, 0 };
    CurrencyTypesEntry s_bgMeta   = { CURRENCY_CONQUEST_BG_META, CURRENCY_CATEGORY_META, NULL, 0, 0, 0 };

    void SeedStores()
    {
        static bool seeded = false;
        if (seeded)
        {
            return;
        }
        seeded = true;

        sCurrencyTypesStore.SetEntry(kPlain, &s_plain);
        sCurrencyTypesStore.SetEntry(kPrecise, &s_precise);
        sCurrencyTypesStore.SetEntry(kCapped, &s_capped);
        sCurrencyTypesStore.SetEntry(CURRENCY_CONQUEST_POINTS, &s_conquest);
        sCurrencyTypesStore.SetEntry(CURRENCY_CONQUEST_ARENA_META, &s_arenaMeta);
        sCurrencyTypesStore.SetEntry(CURRENCY_CONQUEST_BG_META, &s_bgMeta);
    }

    /// The packet as it goes on the wire: a copy with its pending bits flushed, then the opcode
    /// and the bytes in hex.
    std::string PacketText(WorldPacket const& packet)
    {
        WorldPacket wire(packet);
        wire.FlushBits();
        static const char* digits = "0123456789abcdef";
        std::string text;
        uint16 opcode = wire.GetOpcode();
        for (int shift = 12; shift >= 0; shift -= 4)
        {
            text += digits[(opcode >> shift) & 0x0F];
        }
        text += ":";
        text += testing::BytesToHex(wire.contents(), wire.size());
        return text;
    }

    /// What the callbacks received, in call order. The multiplier answers `multiplier`, the check
    /// answers `canNotify`, and the config's conquest cap is `conquestWeekCap`.
    struct Wire
    {
        CurrencyMgr& mgr;
        float multiplier = 1.0f;
        bool canNotify = true;
        uint32 conquestWeekCap = kConquestCap;
        std::vector<std::string> events;

        explicit Wire(CurrencyMgr& m) : mgr(m) {}

        CurrencyMgr::CurrencyInputs Inputs()
        {
            CurrencyMgr::CurrencyInputs inputs;
            inputs.gainMultiplier = [this](uint32 currencyId)
            {
                events.push_back("mult " + std::to_string(currencyId));
                return multiplier;
            };
            inputs.canNotify = [this]()
            {
                events.push_back("gate");
                return canNotify;
            };
            inputs.conquestWeekCap = conquestWeekCap;
            return inputs;
        }

        ManagerPacketSink Send()
        {
            return [this](WorldPacket const* packet)
            {
                events.push_back("packet " + PacketText(*packet));
            };
        }

        CurrencyMgr::ModifySinks Sinks()
        {
            CurrencyMgr::ModifySinks sinks;
            sinks.updateAchievement = [this](AchievementCriteriaTypes type, uint32 miscValue1, uint32 miscValue2)
            {
                events.push_back("ach " + std::to_string(uint32(type)) + " " + std::to_string(miscValue1) + " "
                                 + std::to_string(miscValue2));
            };
            sinks.send = Send();
            sinks.addedQuestCheck = [this](uint32 currencyId)
            {
                events.push_back("added " + std::to_string(currencyId) + " seen " + std::to_string(mgr.GetCount(currencyId)));
            };
            sinks.removedQuestCheck = [this](uint32 currencyId)
            {
                events.push_back("removed " + std::to_string(currencyId) + " seen " + std::to_string(mgr.GetCount(currencyId)));
            };
            return sinks;
        }

        void Modify(uint32 id, int32 count, bool modifyWeek = true, bool modifySeason = true, bool ignoreMultipliers = false)
        {
            mgr.ModifyCount(id, count, modifyWeek, modifySeason, ignoreMultipliers, Inputs(), Sinks());
        }
    };

    std::string Join(std::vector<std::string> const& list)
    {
        std::string text;
        for (size_t i = 0; i < list.size(); ++i)
        {
            text += (i ? "; " : "") + list[i];
        }
        return text;
    }

    void CheckEvents(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got != want)
        {
            testing::ReportFailure(__FILE__, line, "events differ: got [" + Join(got) + "] want [" + Join(want) + "]");
        }
    }

    FakeRow CurrencyRow(uint32 id, uint32 total, uint32 week, uint32 season, uint32 flags)
    {
        FakeRow row;
        row.push_back(std::to_string(id));
        row.push_back(std::to_string(total));
        row.push_back(std::to_string(week));
        row.push_back(std::to_string(season));
        row.push_back(std::to_string(flags));
        return row;
    }

    /// Loads one row (id, total, week, season, flags) into `mgr` as the login would, with the
    /// config's conquest cap.
    void Load(CurrencyMgr& mgr, uint32 id, uint32 total, uint32 week, uint32 season, uint32 flags = 0)
    {
        FakeQueryResult result(FakeRows{ CurrencyRow(id, total, week, season, flags) });
        REQUIRE(result.NextRow());
        mgr.LoadRow(result.Fetch(), kConquestCap, ObjectGuid(HIGHGUID_PLAYER, kGuid));
    }

    std::string InsertSql(uint32 id, uint32 total, uint32 week, uint32 season, uint32 flags)
    {
        return "INSERT INTO `character_currencies` (`guid`, `id`, `totalCount`, `weekCount`, `seasonCount`, `flags`) VALUES ('42', '"
            + std::to_string(id) + "', '" + std::to_string(total) + "', '" + std::to_string(week) + "', '"
            + std::to_string(season) + "', '" + std::to_string(flags) + "')";
    }

    std::string UpdateSql(uint32 id, uint32 total, uint32 week, uint32 season, uint32 flags)
    {
        return "UPDATE `character_currencies` SET `totalCount` = '" + std::to_string(total) + "', `weekCount` = '"
            + std::to_string(week) + "', `seasonCount` = '" + std::to_string(season) + "', `flags` = '"
            + std::to_string(flags) + "' WHERE `guid` = '42' AND `id` = '" + std::to_string(id) + "'";
    }

    /// The save's statements, sorted (the map's iteration order is the hash's).
    std::vector<std::string> SaveSorted(CurrencyMgr& mgr, FakeConnection& async)
    {
        size_t before = async.executed.size();
        mgr.Save(kGuid);
        CharacterDatabase.ExecuteQueuedForTest();
        std::vector<std::string> got(async.executed.begin() + before, async.executed.end());
        std::sort(got.begin(), got.end());
        return got;
    }

    std::vector<std::string> Sorted(std::vector<std::string> list)
    {
        std::sort(list.begin(), list.end());
        return list;
    }

    /// The three counts of one currency.
    std::string Counts(CurrencyMgr const& mgr, uint32 id)
    {
        return std::to_string(mgr.GetCount(id)) + "/" + std::to_string(mgr.GetWeekCount(id)) + "/"
            + std::to_string(mgr.GetSeasonCount(id));
    }
}

// The three getters read three different fields, and an unknown id answers 0. The loaded values
// differ in every field and between the two currencies, so a getter reading the wrong field or the
// wrong entry cannot pass.
TEST(CurrencyMgr_CountsByIdAndUnknownIsZero)
{
    SeedStores();
    CurrencyMgr mgr;
    CHECK_EQ(mgr.GetCount(kPlain), 0u);
    CHECK_EQ(mgr.GetWeekCount(kPlain), 0u);
    CHECK_EQ(mgr.GetSeasonCount(kPlain), 0u);

    Load(mgr, kPlain, 11, 22, 33);
    Load(mgr, kCapped, 444, 555, 666);
    CHECK_EQ(mgr.GetCount(kPlain), 11u);
    CHECK_EQ(mgr.GetWeekCount(kPlain), 22u);
    CHECK_EQ(mgr.GetSeasonCount(kPlain), 33u);
    CHECK_EQ(mgr.GetCount(kCapped), 444u);
    CHECK_EQ(mgr.GetWeekCount(kCapped), 555u);
    CHECK_EQ(mgr.GetSeasonCount(kCapped), 666u);
    CHECK_EQ(mgr.GetCount(kPrecise), 0u);
    CHECK_EQ(mgr.GetCount(kUnknown), 0u);
}

// The week cap is the DBC's, except conquest points', which is the config value handed in (even
// when that is 0); the total cap is the DBC's MaxQty.
TEST(CurrencyMgr_Caps)
{
    SeedStores();
    CurrencyMgr mgr;
    CHECK_EQ(mgr.GetWeekCap(&s_capped, kConquestCap), 1000u);
    CHECK_EQ(mgr.GetWeekCap(&s_plain, kConquestCap), 0u);
    CHECK_EQ(mgr.GetWeekCap(&s_conquest, kConquestCap), kConquestCap);       // not the DBC's 5000
    CHECK_EQ(mgr.GetWeekCap(&s_conquest, 0), 0u);
    CHECK_EQ(mgr.GetWeekCap(&s_arenaMeta, kConquestCap), 0u);                // a meta is not conquest points
    CHECK_EQ(mgr.GetTotalCap(&s_capped), 4000u);
    CHECK_EQ(mgr.GetTotalCap(&s_conquest), 0u);
}

// ModifyCount over a table. Each row starts from a fresh manager, optionally loads one row, makes
// one change with the Wire answering `multiplier` and `canNotify`, and checks the counts
// (total/week/season), the save statement the new state produces, and every callback in order.
TEST(CurrencyMgr_ModifyCountTable)
{
    SeedStores();

    TickGuard::ResetViolations();
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    struct Row
    {
        const char* what;
        uint32 id;
        bool loaded;
        uint32 total, week, season;         // the loaded counts
        int32 count;
        bool modifyWeek, modifySeason, ignoreMultipliers;
        float multiplier;
        bool canNotify;
        const char* wantCounts;             // total/week/season afterwards
        std::vector<std::string> wantEvents;
        std::string wantSave;               // "" = no statement
    };
    const std::string noChange;
    const std::vector<Row> rows =
    {
        // A new currency: NEW, the multiplier read (1.0), week and season follow the gain. Packet:
        // bits week-shown 0 (no week cap), season 0, meta 0 -> 00; total 5; id 361 = 0x169.
        { "new gain", kPlain, false, 0, 0, 0, 5, true, true, false, 1.0f, true, "5/5/5",
          { "mult 361", "gate", "ach 12 361 5", "packet 59b0:000500000069010000", "added 361 seen 5" },
          InsertSql(kPlain, 5, 5, 5, 0) },
        // The multiplier: int32 3 *= 2.5f is 7.5, truncated to 7.
        { "multiplier applied", kPlain, false, 0, 0, 0, 3, true, true, false, 2.5f, true, "7/7/7",
          { "mult 361", "gate", "ach 12 361 7", "packet 59b0:000700000069010000", "added 361 seen 7" },
          InsertSql(kPlain, 7, 7, 7, 0) },
        // ignoreMultipliers: the multiplier is not even read.
        { "multiplier ignored", kPlain, false, 0, 0, 0, 10, true, true, true, 2.5f, true, "10/10/10",
          { "gate", "ach 12 361 10", "packet 59b0:000a00000069010000", "added 361 seen 10" },
          InsertSql(kPlain, 10, 10, 10, 0) },
        // A loss: no multiplier (count < 0), the week and the season keep their counts, no
        // achievement, the removed check; UNCHANGED -> CHANGED. Total 42 = 0x2a.
        { "loss", kPlain, true, 50, 20, 40, -8, true, true, false, 2.5f, true, "42/20/40",
          { "gate", "packet 59b0:002a00000069010000", "removed 361 seen 42" },
          UpdateSql(kPlain, 42, 20, 40, 0) },
        // A loss below zero stops at 0.
        { "loss below zero", kPlain, true, 5, 20, 40, -8, true, true, false, 1.0f, true, "0/20/40",
          { "gate", "packet 59b0:000000000069010000", "removed 361 seen 0" },
          UpdateSql(kPlain, 0, 20, 40, 0) },
        // modifyWeek false: the week keeps 20. Total 55 = 0x37.
        { "week not modified", kPlain, true, 50, 20, 40, 5, false, true, false, 1.0f, true, "55/20/45",
          { "mult 361", "gate", "ach 12 361 55", "packet 59b0:003700000069010000", "added 361 seen 55" },
          UpdateSql(kPlain, 55, 20, 45, 0) },
        // modifySeason false: the season keeps 40.
        { "season not modified", kPlain, true, 50, 20, 40, 5, true, false, false, 1.0f, true, "55/25/40",
          { "mult 361", "gate", "ach 12 361 55", "packet 59b0:003700000069010000", "added 361 seen 55" },
          UpdateSql(kPlain, 55, 25, 40, 0) },
        // The total cap: 3900 + 300 = 4200 > 4000, the 200 over comes off the week (100 + 300 ->
        // 200); the season gets what landed (100). Packet: week shown (a week cap, a gain) -> 80;
        // total 4000 = 0x0fa0; id 396 = 0x18c; week 200 = 0xc8.
        { "total cap", kCapped, true, 3900, 100, 0, 300, true, true, false, 1.0f, true, "4000/200/100",
          { "mult 396", "gate", "ach 12 396 4000", "packet 59b0:80a00f00008c010000c8000000", "added 396 seen 4000" },
          UpdateSql(kCapped, 4000, 200, 100, 0) },
        // The week cap: 900 + 300 = 1200 > 1000, the 200 over comes off the total (100 + 300 ->
        // 200). Week 1000 = 0x3e8.
        { "week cap", kCapped, true, 100, 900, 0, 300, true, true, false, 1.0f, true, "200/1000/100",
          { "mult 396", "gate", "ach 12 396 200", "packet 59b0:80c80000008c010000e8030000", "added 396 seen 200" },
          UpdateSql(kCapped, 200, 1000, 100, 0) },
        // Both: the total cap first (4200 -> 4000, week 1250 -> 1050), then the week cap (1050 ->
        // 1000, total 4000 -> 3950 = 0x0f6e).
        { "both caps", kCapped, true, 3900, 950, 0, 300, true, true, false, 1.0f, true, "3950/1000/50",
          { "mult 396", "gate", "ach 12 396 3950", "packet 59b0:806e0f00008c010000e8030000", "added 396 seen 3950" },
          UpdateSql(kCapped, 3950, 1000, 50, 0) },
        // KEPT (backlog): with the week not modified, the total cap's 10 over still comes off the
        // week (5 -> -5), stored as uint32 4294967291. The packet does not show the week.
        { "total cap wraps an unmodified week", kCapped, true, 3990, 5, 0, 20, false, true, false, 1.0f, true, "4000/4294967291/10",
          { "mult 396", "gate", "ach 12 396 4000", "packet 59b0:00a00f00008c010000", "added 396 seen 4000" },
          UpdateSql(kCapped, 4000, 4294967291u, 10, 0) },
        // Already at the total cap: the gain clamps back to the old total, so nothing changes --
        // no state change, no check, no packet; only the multiplier was read.
        { "at the total cap", kCapped, true, 4000, 0, 0, 10, true, true, false, 1.0f, true, "4000/0/0",
          { "mult 396" }, noChange },
        // Zero: nothing at all.
        { "zero", kPlain, true, 50, 20, 40, 0, true, true, false, 1.0f, true, "50/20/40", {}, noChange },
        // Not in the world or loading: the counts change, nothing is sent or checked.
        { "gate closed", kPlain, false, 0, 0, 0, 5, true, true, false, 1.0f, false, "5/5/5",
          { "mult 361", "gate" }, InsertSql(kPlain, 5, 5, 5, 0) },
        // Precision: the packet carries floor(12345 / 100) = 123 = 0x7b; the map keeps 12345.
        // id 395 = 0x18b.
        { "precision", kPrecise, false, 0, 0, 0, 12345, true, true, false, 1.0f, true, "12345/12345/12345",
          { "mult 395", "gate", "ach 12 395 12345", "packet 59b0:007b0000008b010000", "added 395 seen 12345" },
          InsertSql(kPrecise, 12345, 12345, 12345, 0) },
        // Conquest points, new: the week cap is the config's 2700, which differs from the DBC's
        // 5000, so the week limit follows the count, with the check read again. Packet: week shown
        // and season -> c0; season 10, total 10, id 390 = 0x186, week 10 (1000 / 100). Limit:
        // floor(2700 / 100) = 27 = 0x1b, id 390.
        { "conquest new", CURRENCY_CONQUEST_POINTS, false, 0, 0, 0, 1000, true, true, false, 1.0f, true, "1000/1000/1000",
          { "mult 390", "gate", "ach 12 390 1000", "packet 59b0:c00a0000000a000000860100000a000000",
            "gate", "packet 70a7:1b00000086010000", "added 390 seen 1000" },
          InsertSql(CURRENCY_CONQUEST_POINTS, 1000, 1000, 1000, 0) },
        // Conquest points against the config cap, not the DBC's: 2600 + 500 = 3100 > 2700, the
        // 400 over comes off the total (2500 -> 2100). Loaded, so no week limit. Season 5000 + 100
        // = 5100 -> 51 = 0x33; total 21 = 0x15; week 27 = 0x1b.
        { "conquest config cap", CURRENCY_CONQUEST_POINTS, true, 2000, 2600, 5000, 500, true, true, false, 1.0f, true, "2100/2700/5100",
          { "mult 390", "gate", "ach 12 390 2100", "packet 59b0:c03300000015000000860100001b000000", "added 390 seen 2100" },
          UpdateSql(CURRENCY_CONQUEST_POINTS, 2100, 2700, 5100, 0) },
        // One over the week cap: 700 + 301 = 1001 > 1000, the 1 over comes off the total (100 + 301
        // = 401 -> 400 = 0x190); the season gets what landed (300). Week 1000 = 0x3e8.
        { "one over the week cap", kCapped, true, 100, 700, 0, 301, true, true, false, 1.0f, true, "400/1000/300",
          { "mult 396", "gate", "ach 12 396 400", "packet 59b0:80900100008c010000e8030000", "added 396 seen 400" },
          UpdateSql(kCapped, 400, 1000, 300, 0) },
        // One over the total cap: 3900 + 101 = 4001 > 4000, the 1 over comes off the week (101 ->
        // 100 = 0x64); the season gets what landed (100). Total 4000 = 0x0fa0.
        { "one over the total cap", kCapped, true, 3900, 0, 0, 101, true, true, false, 1.0f, true, "4000/100/100",
          { "mult 396", "gate", "ach 12 396 4000", "packet 59b0:80a00f00008c01000064000000", "added 396 seen 4000" },
          UpdateSql(kCapped, 4000, 100, 100, 0) },
        // A new currency whose week cap is the DBC's (1000): no week limit follows the count (the
        // limit is only for a cap that differs from the DBC's, i.e. conquest points'). Packet: week
        // shown -> 80; total 5, id 396, week 5.
        { "new with the DBC's week cap", kCapped, false, 0, 0, 0, 5, true, true, false, 1.0f, true, "5/5/5",
          { "mult 396", "gate", "ach 12 396 5", "packet 59b0:80050000008c01000005000000", "added 396 seen 5" },
          InsertSql(kCapped, 5, 5, 5, 0) },
    };

    for (size_t i = 0; i < rows.size(); ++i)
    {
        Row const& row = rows[i];
        CurrencyMgr mgr;
        if (row.loaded)
        {
            Load(mgr, row.id, row.total, row.week, row.season);
        }
        Wire wire(mgr);
        wire.multiplier = row.multiplier;
        wire.canNotify = row.canNotify;
        wire.Modify(row.id, row.count, row.modifyWeek, row.modifySeason, row.ignoreMultipliers);

        if (Counts(mgr, row.id) != row.wantCounts)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::string(row.what) + ": counts " + Counts(mgr, row.id) + " want " + row.wantCounts);
        }
        if (wire.events != row.wantEvents)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::string(row.what) + ": events [" + Join(wire.events) + "] want [" + Join(row.wantEvents) + "]");
        }
        std::vector<std::string> want;
        if (!row.wantSave.empty())
        {
            want.push_back(row.wantSave);
        }
        std::vector<std::string> got = SaveSorted(mgr, async);
        if (got != want)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::string(row.what) + ": save [" + Join(got) + "] want [" + Join(want) + "]");
        }
    }
}

// A meta currency's change is forwarded to conquest points after the meta's own effects, with the
// same flags: the multiplier is read again (for 390) and applies again, the check is read again,
// and a new conquest entry sends its week limit. Meta packet: bits 0, 0, meta 1 -> 20; total 600 =
// 0x258; id 483 = 0x1e3. Conquest: c0; season, total and week 6 (600 / 100).
TEST(CurrencyMgr_MetaForwardsToConquest)
{
    SeedStores();
    {
        CurrencyMgr mgr;
        Wire wire(mgr);
        wire.Modify(CURRENCY_CONQUEST_ARENA_META, 600);
        const std::vector<std::string> want =
        {
            "mult 483", "gate", "ach 12 483 600", "packet 59b0:2058020000e3010000", "added 483 seen 600",
            "mult 390", "gate", "ach 12 390 600", "packet 59b0:c006000000060000008601000006000000",
            "gate", "packet 70a7:1b00000086010000", "added 390 seen 600",
        };
        CheckEvents(wire.events, want, __LINE__);
        CHECK_STR(Counts(mgr, CURRENCY_CONQUEST_ARENA_META), std::string("600/600/600"));
        CHECK_STR(Counts(mgr, CURRENCY_CONQUEST_POINTS), std::string("600/600/600"));
    }
    {
        // The other meta, with a multiplier of 2: 100 -> 200 on the meta, and the forwarded 200 ->
        // 400 on conquest points (applied twice, kept). Meta total 200 = 0xc8, id 484 = 0x1e4;
        // conquest 4 (400 / 100).
        CurrencyMgr mgr;
        Wire wire(mgr);
        wire.multiplier = 2.0f;
        wire.Modify(CURRENCY_CONQUEST_BG_META, 100);
        const std::vector<std::string> want =
        {
            "mult 484", "gate", "ach 12 484 200", "packet 59b0:20c8000000e4010000", "added 484 seen 200",
            "mult 390", "gate", "ach 12 390 400", "packet 59b0:c004000000040000008601000004000000",
            "gate", "packet 70a7:1b00000086010000", "added 390 seen 400",
        };
        CheckEvents(wire.events, want, __LINE__);
        CHECK_STR(Counts(mgr, CURRENCY_CONQUEST_POINTS), std::string("400/400/400"));
    }
    {
        // The forward is outside the check: with the check closed both counts still move.
        CurrencyMgr mgr;
        Wire wire(mgr);
        wire.canNotify = false;
        wire.Modify(CURRENCY_CONQUEST_ARENA_META, 600);
        CheckEvents(wire.events, { "mult 483", "gate", "mult 390", "gate" }, __LINE__);
        CHECK_STR(Counts(mgr, CURRENCY_CONQUEST_POINTS), std::string("600/600/600"));
    }
    {
        // A loss on a meta is forwarded as a loss: -100 from 600 on both, the removed checks.
        CurrencyMgr mgr;
        Load(mgr, CURRENCY_CONQUEST_ARENA_META, 600, 0, 0);
        Load(mgr, CURRENCY_CONQUEST_POINTS, 600, 0, 0);
        Wire wire(mgr);
        wire.Modify(CURRENCY_CONQUEST_ARENA_META, -100);
        // Meta: 20; total 500 = 0x1f4. Conquest: week not shown (a loss) and season -> 40; season
        // 0, total 5, id 390.
        const std::vector<std::string> want =
        {
            "gate", "packet 59b0:20f4010000e3010000", "removed 483 seen 500",
            "gate", "packet 59b0:40000000000500000086010000", "removed 390 seen 500",
        };
        CheckEvents(wire.events, want, __LINE__);
    }
    {
        // What is forwarded is what the meta actually moved (diff), not the count asked for: a meta
        // at 50 losing 100 stops at 0, so conquest points lose 50 (600 -> 550), not 100. Meta: 20,
        // total 0, id 483. Conquest: season bit -> 40; season 0, total floor(550 / 100) = 5, id 390.
        CurrencyMgr mgr;
        Load(mgr, CURRENCY_CONQUEST_ARENA_META, 50, 0, 0);
        Load(mgr, CURRENCY_CONQUEST_POINTS, 600, 0, 0);
        Wire wire(mgr);
        wire.Modify(CURRENCY_CONQUEST_ARENA_META, -100);
        const std::vector<std::string> want =
        {
            "gate", "packet 59b0:2000000000e3010000", "removed 483 seen 0",
            "gate", "packet 59b0:40000000000500000086010000", "removed 390 seen 550",
        };
        CheckEvents(wire.events, want, __LINE__);
        CHECK_STR(Counts(mgr, CURRENCY_CONQUEST_ARENA_META), std::string("0/0/0"));
        CHECK_STR(Counts(mgr, CURRENCY_CONQUEST_POINTS), std::string("550/0/0"));
    }
    {
        // The forward carries the caller's week and season flags (a GM's `.modify currency` passes
        // false, false): conquest points' week and season stay 0 and the packet does not show the
        // week (bit0 0, season bit -> 40; season 0, total 6, id 390). New conquest points still send
        // their week limit (27, id 390).
        CurrencyMgr mgr;
        Wire wire(mgr);
        wire.Modify(CURRENCY_CONQUEST_ARENA_META, 600, false, false);
        const std::vector<std::string> want =
        {
            "mult 483", "gate", "ach 12 483 600", "packet 59b0:2058020000e3010000", "added 483 seen 600",
            "mult 390", "gate", "ach 12 390 600", "packet 59b0:40000000000600000086010000",
            "gate", "packet 70a7:1b00000086010000", "added 390 seen 600",
        };
        CheckEvents(wire.events, want, __LINE__);
        CHECK_STR(Counts(mgr, CURRENCY_CONQUEST_ARENA_META), std::string("600/0/0"));
        CHECK_STR(Counts(mgr, CURRENCY_CONQUEST_POINTS), std::string("600/0/0"));
    }
}

// The conquest week cap is read at each change, so a lower config value (.reload config) finds a
// week count above it. KEPT (backlog): a change that does not modify the week leaves it there, and
// one that does takes the whole excess off the total -- a spend of 100 costs 700.
TEST(CurrencyMgr_ConquestCapLowered)
{
    SeedStores();
    {
        CurrencyMgr mgr;
        Load(mgr, CURRENCY_CONQUEST_POINTS, 2000, 2600, 0);         // under the 2700 cap
        Wire wire(mgr);
        wire.conquestWeekCap = 2000;
        wire.canNotify = false;
        wire.Modify(CURRENCY_CONQUEST_POINTS, 100, false);           // the week is not modified: no clamp
        CHECK_STR(Counts(mgr, CURRENCY_CONQUEST_POINTS), std::string("2100/2600/100"));
    }
    {
        CurrencyMgr mgr;
        Load(mgr, CURRENCY_CONQUEST_POINTS, 2000, 2600, 0);
        Wire wire(mgr);
        wire.conquestWeekCap = 2000;
        wire.canNotify = false;
        wire.Modify(CURRENCY_CONQUEST_POINTS, -100);                // 1900, then 600 over the cap off the total
        CHECK_STR(Counts(mgr, CURRENCY_CONQUEST_POINTS), std::string("1300/2000/0"));
    }
}

// SetCount is a change by the difference with the week, the season and the multiplier left out;
// the same count is no change; an absent currency set to 0 (the character's creation) is nothing,
// not even a store lookup.
TEST(CurrencyMgr_SetCount)
{
    SeedStores();
    CurrencyMgr mgr;
    Load(mgr, kPlain, 50, 20, 40);
    Wire wire(mgr);

    mgr.SetCount(kPlain, 30, wire.Inputs(), wire.Sinks());      // -20; total 30 = 0x1e
    CheckEvents(wire.events, { "gate", "packet 59b0:001e00000069010000", "removed 361 seen 30" }, __LINE__);
    CHECK_STR(Counts(mgr, kPlain), std::string("30/20/40"));

    wire.events.clear();
    mgr.SetCount(kPlain, 30, wire.Inputs(), wire.Sinks());
    CheckEvents(wire.events, {}, __LINE__);

    wire.events.clear();
    wire.multiplier = 3.0f;
    mgr.SetCount(kPlain, 70, wire.Inputs(), wire.Sinks());      // +40, no multiplier; total 70 = 0x46
    CheckEvents(wire.events, { "gate", "ach 12 361 70", "packet 59b0:004600000069010000", "added 361 seen 70" }, __LINE__);
    CHECK_STR(Counts(mgr, kPlain), std::string("70/20/40"));

    wire.events.clear();
    mgr.SetCount(kUnknown, 0, wire.Inputs(), wire.Sinks());
    CheckEvents(wire.events, {}, __LINE__);
    CHECK_EQ(mgr.GetCount(kUnknown), 0u);
}

// SetFlags writes a listed currency's flags and marks it CHANGED; an unlisted id is ignored (no
// entry is made). KEPT (backlog): a NEW entry becomes CHANGED, so its save is an UPDATE.
TEST(CurrencyMgr_SetFlags)
{
    SeedStores();

    TickGuard::ResetViolations();
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    CurrencyMgr mgr;
    Load(mgr, kPlain, 50, 20, 40, 0);
    mgr.SetFlags(kPlain, 4);
    mgr.SetFlags(kUnknown, 4);
    Wire wire(mgr);
    wire.Modify(kCapped, 5);                                        // NEW
    mgr.SetFlags(kCapped, 8);
    CHECK_EQ(mgr.GetCount(kUnknown), 0u);
    std::vector<std::string> got = SaveSorted(mgr, async);
    CheckEvents(got, Sorted({ UpdateSql(kPlain, 50, 20, 40, 4), UpdateSql(kCapped, 5, 5, 5, 8) }), __LINE__);
    CheckEvents(SaveSorted(mgr, async), {}, __LINE__);                // all UNCHANGED now
}

// The weekly reset zeroes every week count, marks every entry CHANGED (KEPT: a NEW one too, so its
// save is an UPDATE) and sends the empty reset packet.
TEST(CurrencyMgr_ResetWeekCounts)
{
    SeedStores();

    TickGuard::ResetViolations();
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    CurrencyMgr mgr;
    Load(mgr, kPlain, 50, 20, 40);
    Load(mgr, kCapped, 300, 900, 7);
    Wire wire(mgr);
    wire.Modify(kPrecise, 6);                                       // NEW, week 6
    wire.events.clear();

    mgr.ResetWeekCounts(wire.Send());
    CheckEvents(wire.events, { "packet 3ca1:" }, __LINE__);
    CHECK_STR(Counts(mgr, kPlain), std::string("50/0/40"));
    CHECK_STR(Counts(mgr, kCapped), std::string("300/0/7"));
    CHECK_STR(Counts(mgr, kPrecise), std::string("6/0/6"));
    CheckEvents(SaveSorted(mgr, async),
                Sorted({ UpdateSql(kPlain, 50, 0, 40, 0), UpdateSql(kCapped, 300, 0, 7, 0), UpdateSql(kPrecise, 6, 0, 6, 0) }), __LINE__);
}

// The whole list. Each entry's bits are: week shown (a week cap and a week count), the flags in 4
// bits, a week cap, a season; then per entry the total, the cap if any, the season if any, the id
// and the week if shown -- each divided by the precision.
TEST(CurrencyMgr_SendAllBytes)
{
    SeedStores();
    {
        // Empty: 23 zero bits, flushed to three bytes on the wire.
        CurrencyMgr mgr;
        Wire wire(mgr);
        mgr.SendAll(kConquestCap, wire.Send());
        CheckEvents(wire.events, { "packet 15a5:000000" }, __LINE__);
    }
    {
        // Conquest points, total 2000, week 300, season 5000, flags 4. Bits: the count 1 in 23 bits
        // (22 zeros, 1), week shown 1, flags 0100, cap 1, season 1, two pad bits -> 00 00 03 4c.
        // Then total 20, cap 27 (the config's 2700, not the DBC's 5000), season 50, id 390, week 3.
        CurrencyMgr mgr;
        Load(mgr, CURRENCY_CONQUEST_POINTS, 2000, 300, 5000, 4);
        Wire wire(mgr);
        mgr.SendAll(kConquestCap, wire.Send());
        CheckEvents(wire.events, { "packet 15a5:0000034c140000001b000000320000008601000003000000" }, __LINE__);
    }
    {
        // A plain currency, total 7, week 3 (not shown: no cap), flags 8. Bits: 22 zeros, 1,
        // then 0, 1000, 0, 0, pad -> 00 00 02 80; total 7, id 361.
        CurrencyMgr mgr;
        Load(mgr, kPlain, 7, 3, 9, 8);
        Wire wire(mgr);
        mgr.SendAll(kConquestCap, wire.Send());
        CheckEvents(wire.events, { "packet 15a5:000002800700000069010000" }, __LINE__);
    }
    {
        // Two entries, in the map's order: the plain one above and a capped one (total 3000,
        // week 0 so not shown, flags 0, cap 1000). Count 2 in 23 bits is 21 zeros, 1, 0; each
        // entry's 7 bits follow in the same order as its values.
        CurrencyMgr mgr;
        Load(mgr, kPlain, 7, 3, 9, 8);
        Load(mgr, kCapped, 3000, 0, 11, 0);
        Wire wire(mgr);
        mgr.SendAll(kConquestCap, wire.Send());
        REQUIRE(wire.events.size() == 1);
        // plain first: 0 1000 0 0 | 0 0000 1 0 -> 00 00 04 80 10; capped first: 0 0000 1 0 | 0 1000 0 0 -> 00 00 04 09 00.
        const std::string plainFirst = "packet 15a5:00000480100700000069010000b80b0000e80300008c010000";
        const std::string cappedFirst = "packet 15a5:0000040900b80b0000e80300008c0100000700000069010000";
        CHECK(wire.events[0] == plainFirst || wire.events[0] == cappedFirst);
    }
}

// The week limit: nothing for a NULL entry (the check not even read), for a closed check, or for a
// currency without a week cap (read after the check); conquest points' is the config's. The id
// overload looks the entry up (an unknown id is NULL).
TEST(CurrencyMgr_SendWeekCap)
{
    SeedStores();
    CurrencyMgr mgr;
    Wire wire(mgr);

    mgr.SendWeekCap((CurrencyTypesEntry const*)NULL, wire.Inputs(), wire.Send());
    CheckEvents(wire.events, {}, __LINE__);

    mgr.SendWeekCap(kUnknown, wire.Inputs(), wire.Send());
    CheckEvents(wire.events, {}, __LINE__);

    wire.canNotify = false;
    mgr.SendWeekCap(&s_capped, wire.Inputs(), wire.Send());
    CheckEvents(wire.events, { "gate" }, __LINE__);

    wire.canNotify = true;
    wire.events.clear();
    mgr.SendWeekCap(&s_plain, wire.Inputs(), wire.Send());
    CheckEvents(wire.events, { "gate" }, __LINE__);

    wire.events.clear();
    mgr.SendWeekCap(kCapped, wire.Inputs(), wire.Send());           // 1000 = 0x3e8, id 396
    CheckEvents(wire.events, { "gate", "packet 70a7:e80300008c010000" }, __LINE__);

    wire.events.clear();
    mgr.SendWeekCap(&s_conquest, wire.Inputs(), wire.Send());       // 27, id 390
    CheckEvents(wire.events, { "gate", "packet 70a7:1b00000086010000" }, __LINE__);
}

// A loaded row: the total clamped to the total cap, the week to the week cap (conquest points' the
// config's), the season as it is, the flags masked to 0x0C, UNCHANGED (no statement at the save).
// An unknown id is not added and its rows are deleted -- KEPT (backlog): for every character, the
// DELETE names only the id.
TEST(CurrencyMgr_LoadRowValidAndInvalid)
{
    SeedStores();

    TickGuard::ResetViolations();
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    CurrencyMgr mgr;
    FakeQueryResult result(FakeRows{
        CurrencyRow(kPlain, 123, 45, 67, 255),
        CurrencyRow(kUnknown, 1, 2, 3, 4),
        CurrencyRow(kCapped, 5000, 1500, 99, 4),
        CurrencyRow(CURRENCY_CONQUEST_POINTS, 100, 3000, 7, 0),
    });
    REQUIRE(result.NextRow());
    do
    {
        mgr.LoadRow(result.Fetch(), kConquestCap, ObjectGuid(HIGHGUID_PLAYER, kGuid));
    }
    while (result.NextRow());
    CharacterDatabase.ExecuteQueuedForTest();

    CheckEvents(async.executed, { "DELETE FROM `character_currencies` WHERE `id` = '9999'" }, __LINE__);
    CHECK_STR(Counts(mgr, kPlain), std::string("123/45/67"));
    CHECK_STR(Counts(mgr, kCapped), std::string("4000/1000/99"));
    CHECK_STR(Counts(mgr, CURRENCY_CONQUEST_POINTS), std::string("100/2700/7"));
    CHECK_EQ(mgr.GetCount(kUnknown), 0u);

    // The flags: 255 & 0x0C = 12, seen through a change's UPDATE (no other accessor reads them).
    Wire wire(mgr);
    wire.canNotify = false;
    wire.Modify(kPlain, 1, false, false);
    CheckEvents(SaveSorted(mgr, async), { UpdateSql(kPlain, 124, 45, 67, 12) }, __LINE__);
}

// The save: NEW -> INSERT, CHANGED -> UPDATE, UNCHANGED -> nothing, every value in its column;
// afterwards everything is UNCHANGED, so a second save writes nothing. The save runs no statement
// synchronously (TickGuard clean) and queues all of them.
TEST(CurrencyMgr_SaveStreamPerState)
{
    SeedStores();

    TickGuard::ResetViolations();
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    CurrencyMgr mgr;
    Load(mgr, kPlain, 1, 2, 3, 4);                                  // stays UNCHANGED
    Load(mgr, kCapped, 10, 20, 30, 8);                              // CHANGED below
    Wire wire(mgr);
    wire.canNotify = false;
    wire.Modify(kCapped, 5);                                        // 15 / 25 / 35
    wire.Modify(kPrecise, 7, true, false);                          // NEW: 7 / 7 / 0
    wire.Modify(kPrecise, 2, false, true);                          // still NEW: 9 / 7 / 2

    {
        TickGuard::Scope scope;
        mgr.Save(kGuid);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(query.executed.size(), size_t(0));
    }
    CharacterDatabase.ExecuteQueuedForTest();
    std::vector<std::string> got(async.executed.begin(), async.executed.end());
    CheckEvents(Sorted(got), Sorted({ UpdateSql(kCapped, 15, 25, 35, 8), InsertSql(kPrecise, 9, 7, 2, 0) }), __LINE__);
    // The literal text of each kind once, so the helpers themselves are pinned.
    CHECK(std::find(got.begin(), got.end(),
          "UPDATE `character_currencies` SET `totalCount` = '15', `weekCount` = '25', `seasonCount` = '35', `flags` = '8' WHERE `guid` = '42' AND `id` = '396'") != got.end());
    CHECK(std::find(got.begin(), got.end(),
          "INSERT INTO `character_currencies` (`guid`, `id`, `totalCount`, `weekCount`, `seasonCount`, `flags`) VALUES ('42', '395', '9', '7', '2', '0')") != got.end());

    CheckEvents(SaveSorted(mgr, async), {}, __LINE__);
}
