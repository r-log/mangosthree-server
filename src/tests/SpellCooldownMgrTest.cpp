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

/// Decoupling D4k: a character's spell cooldowns, added, queried, removed, sent, loaded and saved
/// with no character and fixed clocks.
///
/// Before this PR SpellCooldownMgr held a pointer to its owner and read the owner's clock
/// (time(NULL)), ranged attack time, cooldown spell mods, guid and session. Now the reads are
/// parameters (`now`, SpellCooldownMgr::CastInputs, the guid) and the writes are callbacks, so
/// every case builds one from nothing, and a Wire records, in call order, what the callbacks
/// received: each item prototype lookup, each spell-mod call (spell id and the value it was
/// handed), each one-spell clear and each packet (opcode and bytes).
///
/// The spell data the manager reads itself -- the spell store (a loaded row's spell must exist,
/// the arena reset reads recovery times), the spell cooldown and category rows behind
/// SpellEntry::GetRecoveryTime/GetCategoryRecoveryTime/GetCategory, and the spell category sets
/// -- is seeded, the way TalentMgrTest seeds its stores: DBCStorage::SetEntry for the DBC stores
/// and an insert into the category-set map. SetEntry never removes an entry, so the ids below
/// (93001-93999 for spells, 935xx/936xx for the rows, 9301 for the category) are used by no other
/// test in this binary, and every case seeds the same entries through SeedStores(). The spells
/// handed to the manager directly are SpellEntry rows built as the DBC loader builds them: zeroed
/// raw storage with the fields set.
///
/// The save runs through the GLOBAL CharacterDatabase with D7a's fakes attached and asynchronous
/// writes on: the statements are queued, and what the delay thread would have sent to MySQL is the
/// exact SQL asserted after ExecuteQueuedForTest().
///
/// Every expected value is derived by hand in the comments. MONTH is 30 days (2592000 s), so the
/// infinity mark is now + 2592000 and the save skips an end after now + 1296000.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "DBCStores.h"
#include "ItemPrototype.h"
#include "ObjectGuid.h"
#include "SpellCooldownMgr.h"
#include "WorldPacket.h"

#include <cstring>
#include <string>
#include <vector>

namespace
{
    const uint32 kGuid = 42;
    const time_t kNow = time_t(1700000000);

    // The owner's guid in the packets: raw 0x0100FF0000A5003C, so its bytes (low first) are
    // g0 3c, g1 00, g2 a5, g3 00, g4 00, g5 ff, g6 00, g7 01 -- zero and non-zero bytes on both
    // sides of every mask, and g7 = 01, which the packed form writes as 01 ^ 1 = 00.
    const uint64 kOwnerRaw = UI64LIT(0x0100FF0000A5003C);

    // Spells. 93999 is deliberately never seeded into the spell store.
    const uint32 kSpellPlain = 93001;       // recovery 8000 ms, no category
    const uint32 kSpellCat = 93002;         // category 9301, recovery 10000 ms, category recovery 30000 ms
    const uint32 kSpellCatB = 93003;        // category 9301 member, no cooldown row
    const uint32 kSpellCatC = 93004;        // category 9301 member, no cooldown row
    const uint32 kSpellNone = 93005;        // no rows at all: recovery 0, category recovery 0, category 0
    const uint32 kSpellRanged76 = 93006;    // category 76, no cooldown row
    const uint32 kSpellAutoRepeat = 93007;  // no rows; the owner says whether it auto-repeats
    const uint32 kSpellCatOnly = 93008;     // category 9301 (not a member of its set), category recovery 5000 ms only
    const uint32 kSpellCatNoCatRec = 93009; // category 9301 (not a member of its set), recovery 6000 ms only
    const uint32 kSpellArena15 = 93010;     // recovery exactly 15 minutes (900000 ms)
    const uint32 kSpellArenaLong = 93011;   // recovery 900001 ms
    const uint32 kSpellArenaCatLong = 93012;// category recovery 900001 ms, recovery 1000 ms
    const uint32 kSpellUnknown = 93999;

    const uint32 kCategory = 9301;
    const uint32 kCategoryMissing = 9399;   // never in the category-set map

    // SpellCooldowns.dbc rows: { CategoryRecoveryTime, RecoveryTime, StartRecoveryTime }.
    SpellCooldownsEntry s_cdPlain = { 0, 8000, 0 };
    SpellCooldownsEntry s_cdCat = { 30000, 10000, 0 };
    SpellCooldownsEntry s_cdCatOnly = { 5000, 0, 0 };
    SpellCooldownsEntry s_cdCatNoCatRec = { 0, 6000, 0 };
    SpellCooldownsEntry s_cdArena15 = { 0, 900000, 0 };
    SpellCooldownsEntry s_cdArenaLong = { 0, 900001, 0 };
    SpellCooldownsEntry s_cdArenaCatLong = { 900001, 1000, 0 };

    // SpellCategories.dbc rows: { Category, DefenseType, DispelType, Mechanic, PreventionType,
    // StartRecoveryCategory }.
    SpellCategoriesEntry s_catSet = { kCategory, 0, 0, 0, 0, 0 };
    SpellCategoriesEntry s_cat76 = { 76, 0, 0, 0, 0, 0 };

    // Spell.dbc rows as the loader makes them (SpellEntry has no default constructor: it declares
    // a private copy constructor).
    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    SpellRowStorage s_rows[14] = {};

    SpellEntry* Row(size_t slot, uint32 id, uint32 categoriesId, uint32 cooldownsId)
    {
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_rows[slot].bytes);
        row->ID = id;
        row->CategoriesID = categoriesId;
        row->CooldownsID = cooldownsId;
        return row;
    }

    SpellEntry* s_plain = NULL;
    SpellEntry* s_cat = NULL;
    SpellEntry* s_catB = NULL;
    SpellEntry* s_catC = NULL;
    SpellEntry* s_none = NULL;
    SpellEntry* s_ranged76 = NULL;
    SpellEntry* s_autoRepeat = NULL;
    SpellEntry* s_catOnly = NULL;
    SpellEntry* s_catNoCatRec = NULL;
    SpellEntry* s_arena15 = NULL;
    SpellEntry* s_arenaLong = NULL;
    SpellEntry* s_arenaCatLong = NULL;
    SpellEntry* s_autoShot = NULL;          // SPELL_ID_AUTOSHOT (75): built, never seeded

    void SeedStores()
    {
        static bool seeded = false;
        if (seeded)
        {
            return;
        }
        seeded = true;

        sSpellCooldownsStore.SetEntry(93601, &s_cdPlain);
        sSpellCooldownsStore.SetEntry(93602, &s_cdCat);
        sSpellCooldownsStore.SetEntry(93608, &s_cdCatOnly);
        sSpellCooldownsStore.SetEntry(93609, &s_cdCatNoCatRec);
        sSpellCooldownsStore.SetEntry(93610, &s_cdArena15);
        sSpellCooldownsStore.SetEntry(93611, &s_cdArenaLong);
        sSpellCooldownsStore.SetEntry(93612, &s_cdArenaCatLong);

        sSpellCategoriesStore.SetEntry(93501, &s_catSet);
        sSpellCategoriesStore.SetEntry(93502, &s_cat76);

        s_plain = Row(0, kSpellPlain, 0, 93601);
        s_cat = Row(1, kSpellCat, 93501, 93602);
        s_catB = Row(2, kSpellCatB, 93501, 0);
        s_catC = Row(3, kSpellCatC, 93501, 0);
        s_none = Row(4, kSpellNone, 0, 0);
        s_ranged76 = Row(5, kSpellRanged76, 93502, 0);
        s_autoRepeat = Row(6, kSpellAutoRepeat, 0, 0);
        s_catOnly = Row(7, kSpellCatOnly, 93501, 93608);
        s_arena15 = Row(8, kSpellArena15, 0, 93610);
        s_arenaLong = Row(9, kSpellArenaLong, 0, 93611);
        s_arenaCatLong = Row(10, kSpellArenaCatLong, 0, 93612);
        s_autoShot = Row(11, SPELL_ID_AUTOSHOT, 0, 0);
        s_catNoCatRec = Row(12, kSpellCatNoCatRec, 93501, 93609);

        SpellEntry* seededRows[] = { s_plain, s_cat, s_catB, s_catC, s_none, s_ranged76, s_autoRepeat,
                                     s_catOnly, s_catNoCatRec, s_arena15, s_arenaLong, s_arenaCatLong };
        for (SpellEntry* row : seededRows)
        {
            sSpellStore.SetEntry(row->ID, row);
        }

        // The category's set, as LoadDBCStores builds it from Spell.dbc: every spell whose
        // category is 9301 -- except kSpellCatOnly and kSpellCatNoCatRec, left out on purpose so
        // a category cooldown of theirs would reach all three members.
        sSpellCategoryStore[kCategory].insert(kSpellCat);
        sSpellCategoryStore[kCategory].insert(kSpellCatB);
        sSpellCategoryStore[kCategory].insert(kSpellCatC);
    }

    std::string Hex(WorldPacket const& packet)
    {
        static const char* digits = "0123456789abcdef";
        std::string text;
        uint16 opcode = packet.GetOpcode();
        for (int shift = 12; shift >= 0; shift -= 4)
        {
            text += digits[(opcode >> shift) & 0x0F];
        }
        text += ":";
        text += testing::BytesToHex(packet.contents(), packet.size());
        return text;
    }

    /// Item prototypes the lookup answers from; anything else answers NULL.
    struct Items
    {
        std::vector<std::pair<uint32, ItemPrototype const*> > known;

        ItemPrototype const* Find(uint32 itemId) const
        {
            for (size_t i = 0; i < known.size(); ++i)
            {
                if (known[i].first == itemId)
                {
                    return known[i].second;
                }
            }
            return NULL;
        }
    };

    /// What the callbacks received, in call order: "item <id>" per prototype lookup,
    /// "mod <spell> <value in>" per spell-mod call, "clear <spell>" per one-spell clear,
    /// "<opcode>:<hex>" per packet.
    struct Wire
    {
        std::vector<std::string> events;
        Items items;
        int32 modDelta = 0;         // added to the value by each spell-mod call
        bool modHalves = false;     // or: the value halves
        bool modZero = false;       // or: the value becomes 0
        int32 modSet = 0;           // or, when non-zero: the value becomes this

        SpellCooldownMgr::CastInputs Inputs(bool autoRepeatRanged = false, uint32 rangedAttackTime = 0)
        {
            SpellCooldownMgr::CastInputs inputs;
            inputs.itemPrototype = [this](uint32 itemId) -> ItemPrototype const*
            {
                events.push_back("item " + std::to_string(itemId));
                return items.Find(itemId);
            };
            inputs.autoRepeatRanged = autoRepeatRanged;
            inputs.rangedAttackTime = rangedAttackTime;
            inputs.applyCooldownMod = [this](uint32 spellId, int32& cooldown)
            {
                events.push_back("mod " + std::to_string(spellId) + " " + std::to_string(cooldown));
                if (modZero)
                {
                    cooldown = 0;
                }
                else if (modHalves)
                {
                    cooldown = cooldown / 2;
                }
                else if (modSet != 0)
                {
                    cooldown = modSet;
                }
                else
                {
                    cooldown += modDelta;
                }
            };
            return inputs;
        }

        SpellCooldownMgr::ClearSink Clear()
        {
            return [this](uint32 spellId)
            {
                events.push_back("clear " + std::to_string(spellId));
            };
        }

        SpellCooldownMgr::PacketSink Sink()
        {
            return [this](WorldPacket const* packet)
            {
                events.push_back(Hex(*packet));
            };
        }
    };

    /// The cooldown's end, or -1 when the spell has none.
    int64 End(SpellCooldownMgr const& mgr, uint32 spellId)
    {
        SpellCooldowns::const_iterator itr = mgr.GetSpellCooldownMap().find(spellId);
        return itr == mgr.GetSpellCooldownMap().end() ? int64(-1) : int64(itr->second.end);
    }

    uint32 ItemOf(SpellCooldownMgr const& mgr, uint32 spellId)
    {
        SpellCooldowns::const_iterator itr = mgr.GetSpellCooldownMap().find(spellId);
        return itr == mgr.GetSpellCooldownMap().end() ? uint32(0xFFFFFFFF) : uint32(itr->second.itemid);
    }

    void CheckEvents(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        bool same = got.size() == want.size();
        for (size_t i = 0; same && i < got.size(); ++i)
        {
            same = got[i] == want[i];
        }
        if (!same)
        {
            std::string text = "events differ (case line " + std::to_string(line) + "): got [";
            for (size_t i = 0; i < got.size(); ++i)
            {
                text += (i ? ", " : "") + got[i];
            }
            text += "] want [";
            for (size_t i = 0; i < want.size(); ++i)
            {
                text += (i ? ", " : "") + want[i];
            }
            testing::ReportFailure(__FILE__, line, text + "]");
        }
    }

    /// One `character_spell_cooldown` row: spell, item, time.
    FakeRow CooldownRow(uint32 spellId, uint32 itemId, time_t end)
    {
        FakeRow row;
        row.push_back(std::to_string(spellId));
        row.push_back(std::to_string(itemId));
        row.push_back(std::to_string(int64(end)));
        return row;
    }

    std::string InsertSql(uint32 spellId, uint32 itemId, time_t end)
    {
        return "INSERT INTO `character_spell_cooldown` (`guid`,`spell`,`item`,`time`) VALUES( '42', '"
               + std::to_string(spellId) + "', '" + std::to_string(itemId) + "', '" + std::to_string(int64(end)) + "')";
    }
}

TEST(SpellCooldownMgr_HasAndDelayAtBeforeAndAfterTheEnd)
{
    SpellCooldownMgr mgr;
    CHECK(mgr.GetSpellCooldownMap().empty());

    mgr.AddSpellCooldown(kSpellPlain, 0, kNow + 10);

    // Has: the end strictly after the clock. Delay: end - clock while that holds, else 0.
    CHECK(mgr.HasSpellCooldown(kSpellPlain, kNow - 100));       // well before
    CHECK(mgr.HasSpellCooldown(kSpellPlain, kNow));
    CHECK(mgr.HasSpellCooldown(kSpellPlain, kNow + 9));         // one second before the end
    CHECK(!mgr.HasSpellCooldown(kSpellPlain, kNow + 10));       // at the end: over
    CHECK(!mgr.HasSpellCooldown(kSpellPlain, kNow + 11));       // after
    CHECK(!mgr.HasSpellCooldown(kSpellCat, kNow));              // no entry
    CHECK_EQ(int64(mgr.GetSpellCooldownDelay(kSpellPlain, kNow - 100)), int64(110));
    CHECK_EQ(int64(mgr.GetSpellCooldownDelay(kSpellPlain, kNow)), int64(10));
    CHECK_EQ(int64(mgr.GetSpellCooldownDelay(kSpellPlain, kNow + 9)), int64(1));
    CHECK_EQ(int64(mgr.GetSpellCooldownDelay(kSpellPlain, kNow + 10)), int64(0));
    CHECK_EQ(int64(mgr.GetSpellCooldownDelay(kSpellPlain, kNow + 11)), int64(0));
    CHECK_EQ(int64(mgr.GetSpellCooldownDelay(kSpellCat, kNow)), int64(0));

    // An expired entry stays in the map until the save sweeps it: the queries only compare.
    CHECK_EQ(End(mgr, kSpellPlain), int64(kNow + 10));

    // A second add replaces the entry, end and item.
    mgr.AddSpellCooldown(kSpellPlain, 5, kNow + 3);
    CHECK_EQ(End(mgr, kSpellPlain), int64(kNow + 3));
    CHECK_EQ(ItemOf(mgr, kSpellPlain), 5u);
    CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(1));

    // KEPT: the item id is held as uint16, so 70000 is kept as 70000 - 65536 = 4464.
    mgr.AddSpellCooldown(kSpellCat, 70000, kNow + 3);
    CHECK_EQ(ItemOf(mgr, kSpellCat), 4464u);
}

// The DBC path: no item (or an item whose prototype is missing), so the category and both
// recovery times come from the spell's rows. kSpellCat: category 9301, recovery 10000 ms,
// category recovery 30000 ms; the spell mods leave the values alone here.
TEST(SpellCooldownMgr_SpellAndCategoryCooldownsFromTheDbc)
{
    SeedStores();

    {
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_cat, 0, kNow, wire.Inputs());
        // Item 0: no lookup. Mods on recovery then category recovery, each once, with the DBC
        // values. recTime = now + 10000/1000 = now + 10; catrecTime = now + 30000/1000 = now + 30.
        CheckEvents(wire.events, { "mod 93002 10000", "mod 93002 30000" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellCat), int64(kNow + 10));
        // The category set {93002, 93003, 93004} minus the spell itself: both at catrecTime.
        CHECK_EQ(End(mgr, kSpellCatB), int64(kNow + 30));
        CHECK_EQ(End(mgr, kSpellCatC), int64(kNow + 30));
        CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(3));
        CHECK_EQ(ItemOf(mgr, kSpellCat), 0u);
        CHECK_EQ(ItemOf(mgr, kSpellCatB), 0u);
    }
    {
        // Item 777 whose prototype the lookup does not know: looked up once, then the DBC
        // values; every entry carries the item.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_cat, 777, kNow, wire.Inputs());
        CheckEvents(wire.events, { "item 777", "mod 93002 10000", "mod 93002 30000" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellCat), int64(kNow + 10));
        CHECK_EQ(End(mgr, kSpellCatC), int64(kNow + 30));
        CHECK_EQ(ItemOf(mgr, kSpellCat), 777u);
        CHECK_EQ(ItemOf(mgr, kSpellCatB), 777u);
        CHECK_EQ(ItemOf(mgr, kSpellCatC), 777u);
    }
    {
        // No category (kSpellPlain, category 0, recovery 8000): the spell alone, one mod call.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_plain, 0, kNow, wire.Inputs());
        CheckEvents(wire.events, { "mod 93001 8000" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellPlain), int64(kNow + 8));
        CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(1));
    }
    {
        // Category recovery only (kSpellCatOnly: category 9301, category recovery 5000, recovery
        // 0): one mod call; recTime = rec ? ... : catrecTime, so the spell and the three
        // members all end at now + 5 (the spell is not in the set, so nothing is skipped).
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_catOnly, 0, kNow, wire.Inputs());
        CheckEvents(wire.events, { "mod 93008 5000" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellCatOnly), int64(kNow + 5));
        CHECK_EQ(End(mgr, kSpellCat), int64(kNow + 5));
        CHECK_EQ(End(mgr, kSpellCatB), int64(kNow + 5));
        CHECK_EQ(End(mgr, kSpellCatC), int64(kNow + 5));
        CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(4));
    }
    {
        // A category but no category recovery (kSpellCatNoCatRec: category 9301, recovery 6000):
        // `cat && catrec > 0` is false, so the spell alone at now + 6 -- the members get nothing
        // (catrecTime is 0 here).
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_catNoCatRec, 0, kNow, wire.Inputs());
        CheckEvents(wire.events, { "mod 93009 6000" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellCatNoCatRec), int64(kNow + 6));
        CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(1));
    }
    {
        // Nothing at all (kSpellNone): recovery 0 and category recovery 0, no mod call, and
        // "no cooldown after applying spell mods" returns with the map untouched.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_none, 0, kNow, wire.Inputs());
        CHECK(wire.events.empty());
        CHECK(mgr.GetSpellCooldownMap().empty());
    }
    {
        // A category that has no set in the store: the spell only.
        SpellCategoriesEntry missingCategory = { kCategoryMissing, 0, 0, 0, 0, 0 };
        sSpellCategoriesStore.SetEntry(93503, &missingCategory);
        SpellRowStorage storage = {};
        SpellEntry* spell = reinterpret_cast<SpellEntry*>(storage.bytes);
        spell->ID = 93020;
        spell->CategoriesID = 93503;
        spell->CooldownsID = 93602;         // recovery 10000, category recovery 30000
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(spell, 0, kNow, wire.Inputs());
        sSpellCategoriesStore.SetEntry(93503, NULL);
        CHECK_EQ(End(mgr, 93020), int64(kNow + 10));
        CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(1));
    }
}

// The item path: an item's spell slot overrides the DBC when either of its times is set.
TEST(SpellCooldownMgr_ItemPrototypeOverridesTheDbc)
{
    SeedStores();

    ItemPrototype proto = ItemPrototype();
    // Slot 0 names another spell; slot 2 names kSpellCat with its own times and category.
    proto.Spells[0].SpellId = kSpellPlain;
    proto.Spells[0].SpellCooldown = 1000;
    proto.Spells[0].SpellCategoryCooldown = 1000;
    proto.Spells[2].SpellId = kSpellCat;
    proto.Spells[2].SpellCategory = kCategory;
    proto.Spells[2].SpellCooldown = 4000;
    proto.Spells[2].SpellCategoryCooldown = 20000;

    {
        SpellCooldownMgr mgr;
        Wire wire;
        wire.items.known.push_back(std::make_pair(800u, &proto));
        mgr.AddSpellAndCategoryCooldowns(s_cat, 800, kNow, wire.Inputs());
        // Slot 2 matches: cat 9301, rec 4000, catrec 20000; the DBC is not read.
        // recTime = now + 4, catrecTime = now + 20.
        CheckEvents(wire.events, { "item 800", "mod 93002 4000", "mod 93002 20000" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellCat), int64(kNow + 4));
        CHECK_EQ(End(mgr, kSpellCatB), int64(kNow + 20));
        CHECK_EQ(End(mgr, kSpellCatC), int64(kNow + 20));
        CHECK_EQ(ItemOf(mgr, kSpellCatB), 800u);
    }
    {
        // The slot says -1 and -1: no cooldown found there, so the DBC values (10000/30000).
        ItemPrototype unset = proto;
        unset.Spells[2].SpellCooldown = -1;
        unset.Spells[2].SpellCategoryCooldown = -1;
        unset.Spells[2].SpellCategory = 0;
        SpellCooldownMgr mgr;
        Wire wire;
        wire.items.known.push_back(std::make_pair(801u, &unset));
        mgr.AddSpellAndCategoryCooldowns(s_cat, 801, kNow, wire.Inputs());
        CheckEvents(wire.events, { "item 801", "mod 93002 10000", "mod 93002 30000" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellCat), int64(kNow + 10));
        CHECK_EQ(End(mgr, kSpellCatB), int64(kNow + 30));
    }
    {
        // The slot says 0 and -1: 0 is "found" (not < 0), so the DBC is skipped; the slot's
        // category 0, no mod call (neither value > 0), -1 becomes 0, and nothing is added.
        ItemPrototype zero = proto;
        zero.Spells[2].SpellCooldown = 0;
        zero.Spells[2].SpellCategoryCooldown = -1;
        zero.Spells[2].SpellCategory = 0;
        SpellCooldownMgr mgr;
        Wire wire;
        wire.items.known.push_back(std::make_pair(802u, &zero));
        mgr.AddSpellAndCategoryCooldowns(s_cat, 802, kNow, wire.Inputs());
        CheckEvents(wire.events, { "item 802" }, __LINE__);
        CHECK(mgr.GetSpellCooldownMap().empty());
    }
    {
        // The item's slots do not name the spell: the DBC values.
        SpellCooldownMgr mgr;
        Wire wire;
        wire.items.known.push_back(std::make_pair(800u, &proto));
        mgr.AddSpellAndCategoryCooldowns(s_catOnly, 800, kNow, wire.Inputs());
        CheckEvents(wire.events, { "item 800", "mod 93008 5000" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellCatOnly), int64(kNow + 5));
    }
}

// infinityCooldown: the end is now + MONTH (2592000) for whichever time is set, the spell mods
// and the ranged path are not consulted, and nothing is added when neither time is set.
TEST(SpellCooldownMgr_InfinityCooldown)
{
    SeedStores();
    CHECK_EQ(SpellCooldownMgr::infinityCooldownDelay, 2592000u);
    CHECK_EQ(SpellCooldownMgr::infinityCooldownDelayCheck, 1296000u);
    const int64 infinity = int64(kNow) + 2592000;

    {
        // Both set: recTime = catrecTime = now + MONTH.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_cat, 0, kNow, wire.Inputs(), true);
        CHECK(wire.events.empty());
        CHECK_EQ(End(mgr, kSpellCat), infinity);
        CHECK_EQ(End(mgr, kSpellCatB), infinity);
        CHECK_EQ(End(mgr, kSpellCatC), infinity);
    }
    {
        // Recovery only: the spell at now + MONTH; category recovery 0 gives catrecTime 0 and
        // no category.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_plain, 0, kNow, wire.Inputs(), true);
        CHECK_EQ(End(mgr, kSpellPlain), infinity);
        CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(1));
    }
    {
        // Category recovery only: recTime falls back to catrecTime = now + MONTH.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_catOnly, 0, kNow, wire.Inputs(), true);
        CHECK_EQ(End(mgr, kSpellCatOnly), infinity);
        CHECK_EQ(End(mgr, kSpellCat), infinity);
        CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(4));
    }
    {
        // Neither: both 0, nothing added. The ranged path is not taken either (it is in the
        // other branch), even for category 76 with an auto-repeat fact and an attack time.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_none, 0, kNow, wire.Inputs(true, 3000), true);
        mgr.AddSpellAndCategoryCooldowns(s_ranged76, 0, kNow, wire.Inputs(true, 3000), true);
        CHECK(wire.events.empty());
        CHECK(mgr.GetSpellCooldownMap().empty());
    }
}

// "Shoot spells used equipped item cooldown values": with no recovery and no category recovery,
// category 76 or an auto-repeat ranged spell other than Auto Shot (75) takes the ranged attack
// time as its recovery, and the spell mods apply to it.
TEST(SpellCooldownMgr_RangedAttackTimePath)
{
    SeedStores();

    {
        // Category 76: rec = 2500 -> one mod call with 2500 -> recTime = now + 2500/1000 = now + 2.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_ranged76, 0, kNow, wire.Inputs(false, 2500));
        CheckEvents(wire.events, { "mod 93006 2500" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellRanged76), int64(kNow + 2));
        CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(1));      // category 76, catrec 0: no category loop
    }
    {
        // Auto-repeat ranged, not Auto Shot: the same.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_autoRepeat, 0, kNow, wire.Inputs(true, 2500));
        CheckEvents(wire.events, { "mod 93007 2500" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellAutoRepeat), int64(kNow + 2));
    }
    {
        // Not auto-repeat, not category 76: no cooldown at all, whatever the attack time.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_autoRepeat, 0, kNow, wire.Inputs(false, 2500));
        CHECK(wire.events.empty());
        CHECK(mgr.GetSpellCooldownMap().empty());
    }
    {
        // Auto Shot itself is excluded: no cooldown.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_autoShot, 0, kNow, wire.Inputs(true, 2500));
        CHECK(wire.events.empty());
        CHECK(mgr.GetSpellCooldownMap().empty());
    }
    {
        // A spell with its own recovery ignores the attack time (rec 8000 is not <= 0).
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_plain, 0, kNow, wire.Inputs(true, 2500));
        CheckEvents(wire.events, { "mod 93001 8000" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellPlain), int64(kNow + 8));
    }
    {
        // An auto-repeat ranged spell whose row names a category that has a set, and no recovery
        // times: the attack time is the spell's own recovery, not a category recovery, so the
        // members of 9301 get nothing. Category 9301 (row 93501; 93013 is not in the set), recovery
        // and category recovery 0 (no cooldown row), auto-repeat and not Auto Shot, so rec = 2500:
        // one mod call with 2500, recTime = now + 2500/1000 = now + 2, catrecTime = 0, and
        // `cat && catrec > 0` is false -- the spell alone.
        SpellRowStorage storage = {};
        SpellEntry* spell = reinterpret_cast<SpellEntry*>(storage.bytes);
        spell->ID = 93013;
        spell->CategoriesID = 93501;        // category 9301; 93013 is not in its set
        spell->CooldownsID = 0;
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(spell, 0, kNow, wire.Inputs(true, 2500));
        CheckEvents(wire.events, { "mod 93013 2500" }, __LINE__);
        CHECK_EQ(End(mgr, 93013), int64(kNow + 2));
        CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(1));
    }
    {
        // KEPT: an attack time under a second truncates to now + 0 -- rec is non-zero, so
        // recTime = now, which is > 0 and is stored: a cooldown that is already over.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.AddSpellAndCategoryCooldowns(s_ranged76, 0, kNow, wire.Inputs(false, 999));
        CHECK_EQ(End(mgr, kSpellRanged76), int64(kNow));
        CHECK(!mgr.HasSpellCooldown(kSpellRanged76, kNow));
    }
}

// The spell-mod callback is in/out on the value, called for the recovery (when > 0) and then
// for the category recovery (when > 0), each with the spell's id.
TEST(SpellCooldownMgr_SpellModCallbackInOutAndCount)
{
    SeedStores();

    {
        // Halving: 10000 -> 5000 (now + 5), 30000 -> 15000 (now + 15).
        SpellCooldownMgr mgr;
        Wire wire;
        wire.modHalves = true;
        mgr.AddSpellAndCategoryCooldowns(s_cat, 0, kNow, wire.Inputs());
        CheckEvents(wire.events, { "mod 93002 10000", "mod 93002 30000" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellCat), int64(kNow + 5));
        CHECK_EQ(End(mgr, kSpellCatB), int64(kNow + 15));
    }
    {
        // Raising: 10000 + 2345 = 12345 -> now + 12; 30000 + 2345 = 32345 -> now + 32.
        SpellCooldownMgr mgr;
        Wire wire;
        wire.modDelta = 2345;
        mgr.AddSpellAndCategoryCooldowns(s_cat, 0, kNow, wire.Inputs());
        CHECK_EQ(End(mgr, kSpellCat), int64(kNow + 12));
        CHECK_EQ(End(mgr, kSpellCatC), int64(kNow + 32));
    }
    {
        // To zero: both 0 after the mods -> "no cooldown after applying spell mods", nothing
        // added, and both calls were still made.
        SpellCooldownMgr mgr;
        Wire wire;
        wire.modZero = true;
        mgr.AddSpellAndCategoryCooldowns(s_cat, 0, kNow, wire.Inputs());
        CheckEvents(wire.events, { "mod 93002 10000", "mod 93002 30000" }, __LINE__);
        CHECK(mgr.GetSpellCooldownMap().empty());
    }
    {
        // Below zero: -12000 + 10000 = -2000 and -12000 + 30000 = 18000. The negative recovery
        // becomes 0, so recTime = catrecTime = now + 18 for the spell and the members.
        SpellCooldownMgr mgr;
        Wire wire;
        wire.modDelta = -12000;
        mgr.AddSpellAndCategoryCooldowns(s_cat, 0, kNow, wire.Inputs());
        CHECK_EQ(End(mgr, kSpellCat), int64(kNow + 18));
        CHECK_EQ(End(mgr, kSpellCatB), int64(kNow + 18));
    }
    {
        // The category recovery pushed below zero: -40000 + 30000 = -10000 -> 0 after the
        // replace; the recovery -40000 + 10000 = -30000 -> 0 as well: nothing added.
        SpellCooldownMgr mgr;
        Wire wire;
        wire.modDelta = -40000;
        mgr.AddSpellAndCategoryCooldowns(s_cat, 0, kNow, wire.Inputs());
        CHECK_EQ(wire.events.size(), size_t(2));
        CHECK(mgr.GetSpellCooldownMap().empty());
    }
    {
        // A recovery raised from the ranged path is modded like any other: 2500 -> 7000 (now + 7).
        SpellCooldownMgr mgr;
        Wire wire;
        wire.modSet = 7000;
        mgr.AddSpellAndCategoryCooldowns(s_ranged76, 0, kNow, wire.Inputs(false, 2500));
        CheckEvents(wire.events, { "mod 93006 2500" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellRanged76), int64(kNow + 7));
    }
}

// SMSG_COOLDOWN_EVENT (0x4F26): uint32 spell id, then the owner's guid as a uint64, sent after the
// cooldowns are added -- and sent even when there is none ("possible 0").
TEST(SpellCooldownMgr_SendCooldownEventAddsThenSendsThePacket)
{
    SeedStores();
    const ObjectGuid owner(kOwnerRaw);

    {
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.SendCooldownEvent(s_cat, 0, kNow, wire.Inputs(), owner, wire.Sink());
        // 93002 = 0x00016B4A -> 4a6b0100; the guid, low byte first -> 3c00a50000ff0001.
        CheckEvents(wire.events, { "mod 93002 10000", "mod 93002 30000", "4f26:4a6b01003c00a50000ff0001" }, __LINE__);
        CHECK_EQ(End(mgr, kSpellCat), int64(kNow + 10));
        CHECK_EQ(End(mgr, kSpellCatB), int64(kNow + 30));
    }
    {
        // No cooldown: the packet still goes. 93005 = 0x00016B4D.
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.SendCooldownEvent(s_none, 0, kNow, wire.Inputs(), owner, wire.Sink());
        CheckEvents(wire.events, { "4f26:4d6b01003c00a50000ff0001" }, __LINE__);
        CHECK(mgr.GetSpellCooldownMap().empty());
    }
    {
        // The item reaches the add (not the packet).
        SpellCooldownMgr mgr;
        Wire wire;
        mgr.SendCooldownEvent(s_plain, 555, kNow, wire.Inputs(), owner, wire.Sink());
        CheckEvents(wire.events, { "item 555", "mod 93001 8000", "4f26:496b01003c00a50000ff0001" }, __LINE__);
        CHECK_EQ(ItemOf(mgr, kSpellPlain), 555u);
    }
}

TEST(SpellCooldownMgr_RemoveOneAndACategory)
{
    SeedStores();

    SpellCooldownMgr mgr;
    Wire wire;
    mgr.AddSpellCooldown(kSpellPlain, 0, kNow + 10);
    mgr.AddSpellCooldown(kSpellUnknown, 0, kNow + 10);

    // Without update: erased, no clear.
    mgr.RemoveSpellCooldown(kSpellPlain, false, wire.Clear());
    CHECK_EQ(End(mgr, kSpellPlain), int64(-1));
    CHECK(wire.events.empty());

    // With update: erased, one clear for the spell.
    mgr.RemoveSpellCooldown(kSpellUnknown, true, wire.Clear());
    CHECK(mgr.GetSpellCooldownMap().empty());
    CheckEvents(wire.events, { "clear 93999" }, __LINE__);

    // KEPT: with update the clear goes even when the spell had no cooldown.
    wire.events.clear();
    mgr.RemoveSpellCooldown(12345, true, wire.Clear());
    CheckEvents(wire.events, { "clear 12345" }, __LINE__);

    // A category: every entry whose spell is in the category's set, in map (spell id) order.
    // kSpellCatOnly is in category 9301 by its row but not in the set, so it stays.
    wire.events.clear();
    mgr.AddSpellCooldown(kSpellCatC, 0, kNow + 30);
    mgr.AddSpellCooldown(kSpellCat, 0, kNow + 10);
    mgr.AddSpellCooldown(kSpellCatB, 0, kNow + 30);
    mgr.AddSpellCooldown(kSpellCatOnly, 0, kNow + 5);
    mgr.AddSpellCooldown(kSpellPlain, 0, kNow + 8);
    mgr.RemoveSpellCategoryCooldown(kCategory, false, wire.Clear());
    CHECK(wire.events.empty());
    CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(2));
    CHECK_EQ(End(mgr, kSpellCatOnly), int64(kNow + 5));
    CHECK_EQ(End(mgr, kSpellPlain), int64(kNow + 8));

    mgr.AddSpellCooldown(kSpellCatC, 0, kNow + 30);
    mgr.AddSpellCooldown(kSpellCat, 0, kNow + 10);
    mgr.AddSpellCooldown(kSpellCatB, 0, kNow + 30);
    mgr.RemoveSpellCategoryCooldown(kCategory, true, wire.Clear());
    CheckEvents(wire.events, { "clear 93002", "clear 93003", "clear 93004" }, __LINE__);
    CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(2));

    // Only the members that have a cooldown are cleared.
    wire.events.clear();
    mgr.AddSpellCooldown(kSpellCatB, 0, kNow + 30);
    mgr.RemoveSpellCategoryCooldown(kCategory, true, wire.Clear());
    CheckEvents(wire.events, { "clear 93003" }, __LINE__);

    // A category with no set: nothing, even with update.
    wire.events.clear();
    mgr.RemoveSpellCategoryCooldown(kCategoryMissing, true, wire.Clear());
    CHECK(wire.events.empty());
    CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(2));
}

// The arena reset removes, and clears on the client, every cooldown whose spell exists and has
// both recovery times at most 15 minutes (900000 ms); the rest stay.
TEST(SpellCooldownMgr_RemoveArenaSpellCooldownsFifteenMinuteRule)
{
    SeedStores();

    SpellCooldownMgr mgr;
    Wire wire;
    mgr.AddSpellCooldown(kSpellPlain, 0, kNow + 8);             // 8000 / 0: removed
    mgr.AddSpellCooldown(kSpellNone, 0, kNow + 8);              // 0 / 0: removed
    mgr.AddSpellCooldown(kSpellArena15, 0, kNow + 900);         // exactly 900000: removed (<=)
    mgr.AddSpellCooldown(kSpellArenaLong, 0, kNow + 900);       // 900001: stays
    mgr.AddSpellCooldown(kSpellArenaCatLong, 0, kNow + 900);    // category 900001: stays
    mgr.AddSpellCooldown(kSpellUnknown, 0, kNow + 900);         // no spell row: stays

    mgr.RemoveArenaSpellCooldowns(wire.Clear());

    CheckEvents(wire.events, { "clear 93001", "clear 93005", "clear 93010" }, __LINE__);
    CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(3));
    CHECK_EQ(End(mgr, kSpellArenaLong), int64(kNow + 900));
    CHECK_EQ(End(mgr, kSpellArenaCatLong), int64(kNow + 900));
    CHECK_EQ(End(mgr, kSpellUnknown), int64(kNow + 900));
}

// SMSG_CLEAR_COOLDOWNS (0x59B4), the whole-map form, for guid 0x0100FF0000A5003C and the spells
// 93001, 93003, 93999:
//   bits: g1 g3 g6 = 0 0 0; the count 3 in 24 bits; g7 g5 g2 g4 g0 = 1 1 1 0 1 -- 32 bits:
//         00000000 00000000 00000000 01111101 = 00 00 00 7d
//   bytes g7 g2 g4 g5 g1 g3 (non-zero ones, each ^ 1): 01->00, a5->a4, ff->fe = 00 a4 fe
//   the spell ids, uint32 each, map order: 496b0100 4b6b0100 2f6f0100
//   bytes g0 g6: 3c->3d = 3d
TEST(SpellCooldownMgr_RemoveAllSendsOneClearWithEverySpell)
{
    SeedStores();
    const ObjectGuid owner(kOwnerRaw);

    SpellCooldownMgr mgr;
    Wire wire;

    // Empty: no packet.
    mgr.RemoveAllSpellCooldown(owner, wire.Sink());
    CHECK(wire.events.empty());

    mgr.AddSpellCooldown(kSpellUnknown, 0, kNow + 1);
    mgr.AddSpellCooldown(kSpellPlain, 0, kNow + 1);
    mgr.AddSpellCooldown(kSpellCatB, 7, kNow - 5);              // expired entries are listed too
    mgr.RemoveAllSpellCooldown(owner, wire.Sink());
    CheckEvents(wire.events, { "59b4:0000007d00a4fe496b01004b6b01002f6f01003d" }, __LINE__);
    CHECK(mgr.GetSpellCooldownMap().empty());

    // A guid of zero bytes but g0: no mask bit but g0's, and only g0's byte.
    wire.events.clear();
    mgr.AddSpellCooldown(kSpellPlain, 0, kNow + 1);
    mgr.RemoveAllSpellCooldown(ObjectGuid(uint64(0x2A)), wire.Sink());
    // bits: 000, count 1 (24 bits), 0 0 0 0 1 -> 00000000 00000000 00000000 00100001 = 00 00 00 21;
    // no g7..g3 bytes; 93001 = 496b0100; g0 2a -> 2b.
    CheckEvents(wire.events, { "59b4:00000021496b01002b" }, __LINE__);
}

// The login's rows, one at a time: an unknown spell is skipped (and logged), an end at or before
// the clock is skipped, the rest are added -- over whatever was set before the load.
TEST(SpellCooldownMgr_LoadRowSkipsUnknownSpellsAndExpiredRows)
{
    SeedStores();

    SpellCooldownMgr mgr;
    // "Some cooldowns can be already set at aura loading": one the rows replace, one they skip.
    mgr.AddSpellCooldown(kSpellPlain, 9, kNow + 5);
    mgr.AddSpellCooldown(kSpellCat, 3, kNow + 50);

    FakeQueryResult result(FakeRows
    {
        CooldownRow(kSpellPlain, 0, kNow + 100),                // added, replaces the aura's one
        CooldownRow(kSpellUnknown, 5, kNow + 100),              // no spell row: skipped
        CooldownRow(kSpellCat, 700, kNow),                      // at the clock: expired, skipped
        CooldownRow(kSpellCatB, 0, kNow - 1),                   // before the clock: skipped
        CooldownRow(kSpellCatC, 70000, kNow + 1),               // added; the item kept as uint16: 4464
        CooldownRow(kSpellNone, 12, kNow + 2592000),            // an infinity mark loads like any end
    });
    REQUIRE(result.NextRow());
    do
    {
        mgr.LoadRow(result.Fetch(), kNow, kGuid);
    }
    while (result.NextRow());

    CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(4));
    CHECK_EQ(End(mgr, kSpellPlain), int64(kNow + 100));
    CHECK_EQ(ItemOf(mgr, kSpellPlain), 0u);
    CHECK_EQ(End(mgr, kSpellCat), int64(kNow + 50));            // untouched by its expired row
    CHECK_EQ(ItemOf(mgr, kSpellCat), 3u);
    CHECK_EQ(End(mgr, kSpellCatB), int64(-1));
    CHECK_EQ(End(mgr, kSpellCatC), int64(kNow + 1));
    CHECK_EQ(ItemOf(mgr, kSpellCatC), 4464u);
    CHECK_EQ(End(mgr, kSpellNone), int64(kNow + 2592000));
    CHECK_EQ(End(mgr, kSpellUnknown), int64(-1));

    // The clock is the parameter: the same row one second later is expired.
    SpellCooldownMgr later;
    FakeQueryResult one(FakeRows{ CooldownRow(kSpellPlain, 0, kNow + 100) });
    REQUIRE(one.NextRow());
    later.LoadRow(one.Fetch(), kNow + 100, kGuid);
    CHECK(later.GetSpellCooldownMap().empty());
}

// The save: DELETE the character's rows, then per entry in map order: an end at or before the
// clock is erased, an end up to now + MONTH/2 (1296000) is INSERTed, a later one (an infinity
// mark) is kept and not saved.
TEST(SpellCooldownMgr_SaveWritesTheActiveCooldownsAndForgetsTheExpired)
{
    SeedStores();

    SpellCooldownMgr mgr;
    mgr.AddSpellCooldown(kSpellPlain, 0, kNow + 30);            // saved
    mgr.AddSpellCooldown(kSpellCat, 0, kNow);                   // at the clock: erased
    mgr.AddSpellCooldown(kSpellCatB, 0, kNow - 10);             // before: erased
    mgr.AddSpellCooldown(kSpellCatC, 0, kNow + 1296000);        // exactly the check: saved
    mgr.AddSpellCooldown(kSpellNone, 0, kNow + 1296001);        // one past: kept, not saved
    mgr.AddSpellCooldown(kSpellArena15, 0, kNow + 2592000);     // an infinity mark: kept, not saved
    mgr.AddSpellCooldown(kSpellUnknown, 777, kNow + 1);         // saved, with its item

    const std::vector<std::string> expected =
    {
        "DELETE FROM `character_spell_cooldown` WHERE `guid` = '42'",
        InsertSql(kSpellPlain, 0, kNow + 30),                   // 1700000030
        InsertSql(kSpellCatC, 0, kNow + 1296000),               // 1701296000
        InsertSql(kSpellUnknown, 777, kNow + 1),                // 1700000001
    };
    CHECK_STR(expected[1], "INSERT INTO `character_spell_cooldown` (`guid`,`spell`,`item`,`time`) VALUES( '42', '93001', '0', '1700000030')");

    TickGuard::ResetViolations();
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    {
        TickGuard::Scope scope;
        mgr.SaveToDB(kGuid, kNow);
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

    // The two expired entries are gone; the rest, saved or not, stay.
    CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(5));
    CHECK_EQ(End(mgr, kSpellCat), int64(-1));
    CHECK_EQ(End(mgr, kSpellCatB), int64(-1));
    CHECK_EQ(End(mgr, kSpellNone), int64(kNow + 1296001));
    CHECK_EQ(End(mgr, kSpellArena15), int64(kNow + 2592000));

    // The save has no "changed" state: a second one DELETEs and INSERTs again, against its own
    // clock. At now + 30 (the check now + 1296030): 93001 (now + 30) and 93999 (now + 1) are
    // erased, 93004 (now + 1296000) and 93005 (now + 1296001) are INSERTed, 93010 (now + 2592000)
    // stays unsaved.
    mgr.SaveToDB(kGuid, kNow + 30);
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(7));
    CHECK_STR(async.executed[4], expected[0]);
    CHECK_STR(async.executed[5], expected[2]);
    CHECK_STR(async.executed[6], InsertSql(kSpellNone, 0, kNow + 1296001));
    CHECK_EQ(mgr.GetSpellCooldownMap().size(), size_t(3));
    CHECK_EQ(End(mgr, kSpellPlain), int64(-1));
    CHECK_EQ(End(mgr, kSpellUnknown), int64(-1));
    CHECK_EQ(End(mgr, kSpellArena15), int64(kNow + 2592000));
    CHECK_EQ(TickGuard::Violations(), 0u);
}
