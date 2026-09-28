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

/// Decoupling D4k: a character's glyphs, set, applied, initialised for a level, loaded and saved
/// with no character.
///
/// Before this PR GlyphMgr held a pointer to its owner and read the owner's level, active spec,
/// spec count, guid, name and glyph slot fields, and wrote the owner's glyph slot and update
/// fields, cast the glyph's spell and removed its auras. Now the reads are parameters (and the
/// glyph slot field of a loaded row a read callback, called where the old body read it) and the
/// writes are callbacks, so every case builds one from nothing, and a Wire records, in call order,
/// what the callbacks received.
///
/// The DBC data the manager reads itself -- the glyph slot store and the glyph property store --
/// is seeded the way TalentMgrTest seeds its stores: DBCStorage::SetEntry, the path production
/// uses for runtime entries. No other test in this binary touches these two stores. SetEntry
/// switches a store to its map: GetNumRows() is then the number of entries (not the highest id
/// plus one, as for a loaded DBC) and LookupEntry(i) finds the entry with id i. So the slot
/// store's ids are 1-4 and 6-13 (twelve entries, id 0 and id 5 absent): InitGlyphsForLevel walks
/// ids 0 to 11, skips the two holes, and stops at nine slots before reaching id 11.
///
/// The statements run through the GLOBAL CharacterDatabase with D7a's fakes attached and
/// asynchronous writes on: they are queued, and what the delay thread would have sent to MySQL
/// is the exact SQL asserted after ExecuteQueuedForTest().
///
/// The update field indexes, by hand from Object/UpdateFields.h: OBJECT_END = 0x8, UNIT_END =
/// OBJECT_END + 0x8A = 146, PLAYER_FIELD_GLYPH_SLOTS_1 = UNIT_END + 0x4A9 = 1339,
/// PLAYER_FIELD_GLYPHS_1 = UNIT_END + 0x4B2 = 1348, PLAYER_GLYPHS_ENABLED = UNIT_END + 0x4BB = 1357.
/// Every other expected value is derived by hand in the comments.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "DBCStores.h"
#include "GlyphMgr.h"
#include "Object/UpdateFields.h"

#include <string>
#include <vector>

namespace
{
    const uint32 kGuid = 42;
    const char* const kName = "Tester";

    static_assert(PLAYER_FIELD_GLYPH_SLOTS_1 == 1339, "the glyph slot fields, derived by hand above");
    static_assert(PLAYER_FIELD_GLYPHS_1 == 1348, "the glyph fields, derived by hand above");
    static_assert(PLAYER_GLYPHS_ENABLED == 1357, "the glyphs-enabled field, derived by hand above");

    // GlyphProperties.dbc rows: { ID, SpellID, GlyphSlotFlags, SpellIconID }. 94999 is never seeded.
    const uint32 kGlyphMajor = 94101;       // slot type 0, spell 96101
    const uint32 kGlyphMinor = 94102;       // slot type 1, spell 96102
    const uint32 kGlyphPrime = 94103;       // slot type 2, spell 96103
    const uint32 kGlyphUnknown = 94999;
    GlyphPropertiesEntry s_major = { kGlyphMajor, 96101, 0, 0 };
    GlyphPropertiesEntry s_minor = { kGlyphMinor, 96102, 1, 0 };
    GlyphPropertiesEntry s_prime = { kGlyphPrime, 96103, 2, 0 };

    // GlyphSlot.dbc rows: { ID, Type, Tooltip }. Ids 1-4 are type 0, 6-9 type 1, 10-13 type 2;
    // id 0 and id 5 are never seeded.
    GlyphSlotEntry s_slots[12] =
    {
        { 1, 0, 0 }, { 2, 0, 0 }, { 3, 0, 0 }, { 4, 0, 0 },
        { 6, 1, 0 }, { 7, 1, 0 }, { 8, 1, 0 }, { 9, 1, 0 },
        { 10, 2, 0 }, { 11, 2, 0 }, { 12, 2, 0 }, { 13, 2, 0 },
    };

    void SeedStores()
    {
        static bool seeded = false;
        if (seeded)
        {
            return;
        }
        seeded = true;

        sGlyphPropertiesStore.SetEntry(kGlyphMajor, &s_major);
        sGlyphPropertiesStore.SetEntry(kGlyphMinor, &s_minor);
        sGlyphPropertiesStore.SetEntry(kGlyphPrime, &s_prime);
        for (size_t i = 0; i < sizeof(s_slots) / sizeof(s_slots[0]); ++i)
        {
            sGlyphSlotStore.SetEntry(s_slots[i].ID, &s_slots[i]);
        }
    }

    /// Records, in call order, what the manager's callbacks received.
    struct Wire
    {
        std::vector<std::string> events;

        GlyphMgr::GlyphSlotSink SlotSink()
        {
            return [this](uint8 slot, uint32 slotType)
            {
                events.push_back("slot " + std::to_string(slot) + " " + std::to_string(slotType));
            };
        }

        GlyphMgr::FieldSink FieldSink()
        {
            return [this](uint16 index, uint32 value)
            {
                events.push_back("field " + std::to_string(index) + " " + std::to_string(value));
            };
        }

        GlyphMgr::ApplySinks Sinks()
        {
            GlyphMgr::ApplySinks sinks;
            sinks.castOnSelf = [this](uint32 spellId)
            {
                events.push_back("cast " + std::to_string(spellId));
            };
            sinks.removeAuras = [this](uint32 spellId)
            {
                events.push_back("remove " + std::to_string(spellId));
            };
            sinks.setField = FieldSink();
            return sinks;
        }
    };

    void CheckEvents(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got != want)
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

    FakeRow GlyphRow(uint32 spec, uint32 slot, uint32 glyph)
    {
        FakeRow row;
        row.push_back(std::to_string(spec));
        row.push_back(std::to_string(slot));
        row.push_back(std::to_string(glyph));
        return row;
    }

    std::string InsertSql(uint32 spec, uint32 slot, uint32 glyph)
    {
        return "INSERT INTO `character_glyphs` (`guid`, `spec`, `slot`, `glyph`) VALUES ('42', '"
            + std::to_string(spec) + "', '" + std::to_string(slot) + "', '" + std::to_string(glyph) + "')";
    }

    std::string UpdateSql(uint32 spec, uint32 slot, uint32 glyph)
    {
        return "UPDATE `character_glyphs` SET `glyph` = '" + std::to_string(glyph)
            + "' WHERE `guid` = '42' AND `spec` = '" + std::to_string(spec) + "' AND `slot` = '" + std::to_string(slot) + "'";
    }

    std::string DeleteSql(uint32 spec, uint32 slot)
    {
        return "DELETE FROM `character_glyphs` WHERE `guid` = '42' AND `spec` = '" + std::to_string(spec)
            + "' AND `slot` = '" + std::to_string(slot) + "'";
    }

    /// Loads one row into `mgr` with a glyph slot read that answers `slotType` for any slot.
    void LoadValid(GlyphMgr& mgr, uint32 spec, uint32 slot, uint32 glyph, uint32 slotType)
    {
        FakeQueryResult result(FakeRows{ GlyphRow(spec, slot, glyph) });
        REQUIRE(result.NextRow());
        GlyphMgr::RowInputs inputs;
        inputs.ownerGuidLow = kGuid;
        inputs.ownerName = kName;
        inputs.glyphSlot = [slotType](uint8 /*slot*/)
        {
            return slotType;
        };
        mgr.LoadRow(result.Fetch(), inputs);
    }
}

// Every (state, input) pair of Glyph::SetId, the manager's save state machine. The inputs are
// the same id, 0 and another id; for id 0 "the same" and 0 coincide. Each row is set up by
// writing the two public fields directly, so the pairs SetId itself never produces (id 0 with
// CHANGED or NEW, a non-zero id with DELETED) are covered as well.
TEST(GlyphMgr_SetIdEveryTransition)
{
    struct Row
    {
        uint32 id;
        GlyphUpdateState state;
        uint32 newId;
        uint32 wantId;
        GlyphUpdateState wantState;
    };
    const Row rows[] =
    {
        // id 0: an empty slot.
        { 0, GLYPH_UNCHANGED, 0, 0, GLYPH_UNCHANGED },  // same id: nothing
        { 0, GLYPH_UNCHANGED, 7, 7, GLYPH_NEW },        // not in the database yet: INSERT
        { 0, GLYPH_CHANGED,   0, 0, GLYPH_CHANGED },    // same id: nothing
        { 0, GLYPH_CHANGED,   7, 7, GLYPH_CHANGED },    // not UNCHANGED, not 0, not NEW: CHANGED
        { 0, GLYPH_NEW,       0, 0, GLYPH_NEW },        // same id: nothing
        { 0, GLYPH_NEW,       7, 7, GLYPH_NEW },        // NEW stays NEW
        { 0, GLYPH_DELETED,   0, 0, GLYPH_DELETED },    // same id: nothing
        { 0, GLYPH_DELETED,   7, 7, GLYPH_CHANGED },    // re-set after a delete: the row still exists, UPDATE
        // id 5: a slot holding a glyph.
        { 5, GLYPH_UNCHANGED, 5, 5, GLYPH_UNCHANGED },  // same id: nothing
        { 5, GLYPH_UNCHANGED, 0, 0, GLYPH_DELETED },    // loaded, then cleared: DELETE
        { 5, GLYPH_UNCHANGED, 7, 7, GLYPH_CHANGED },    // loaded, then replaced: UPDATE
        { 5, GLYPH_CHANGED,   5, 5, GLYPH_CHANGED },    // same id: nothing
        { 5, GLYPH_CHANGED,   0, 0, GLYPH_DELETED },    // changed, then cleared: DELETE
        { 5, GLYPH_CHANGED,   7, 7, GLYPH_CHANGED },    // changed again
        { 5, GLYPH_NEW,       5, 5, GLYPH_NEW },        // same id: nothing
        { 5, GLYPH_NEW,       0, 0, GLYPH_UNCHANGED },  // added, then cleared before a save: nothing to do
        { 5, GLYPH_NEW,       7, 7, GLYPH_NEW },        // added, then replaced: still an INSERT
        { 5, GLYPH_DELETED,   5, 5, GLYPH_DELETED },    // same id: nothing
        { 5, GLYPH_DELETED,   0, 0, GLYPH_DELETED },    // 0 and not NEW: DELETED
        { 5, GLYPH_DELETED,   7, 7, GLYPH_CHANGED },    // not 0, not NEW: CHANGED
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
    {
        Glyph glyph;
        glyph.id = rows[i].id;
        glyph.uState = rows[i].state;
        glyph.SetId(rows[i].newId);
        CHECK_EQ(glyph.GetId(), rows[i].wantId);
        CHECK_EQ(uint32(glyph.uState), uint32(rows[i].wantState));
        if (glyph.GetId() != rows[i].wantId || glyph.uState != rows[i].wantState)
        {
            testing::ReportFailure(__FILE__, __LINE__, "SetId row " + std::to_string(i));
        }
    }

    // A new slot is empty and UNCHANGED; add, replace and clear in one session leave nothing to save.
    Glyph glyph;
    CHECK_EQ(glyph.GetId(), 0u);
    CHECK_EQ(uint32(glyph.uState), uint32(GLYPH_UNCHANGED));
    glyph.SetId(7);
    glyph.SetId(9);
    CHECK_EQ(uint32(glyph.uState), uint32(GLYPH_NEW));
    glyph.SetId(0);
    CHECK_EQ(uint32(glyph.uState), uint32(GLYPH_UNCHANGED));
    CHECK_EQ(glyph.GetId(), 0u);
}

// The array has one row per spec: the spec chooses the row, the slot the column.
TEST(GlyphMgr_SetAndGetPerSpec)
{
    GlyphMgr mgr;
    for (uint8 spec = 0; spec < MAX_TALENT_SPEC_COUNT; ++spec)
    {
        for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
        {
            CHECK_EQ(mgr.GetGlyph(spec, slot), 0u);
        }
    }
    mgr.SetGlyph(0, 3, kGlyphMajor);
    mgr.SetGlyph(1, 3, kGlyphMinor);
    mgr.SetGlyph(1, 8, kGlyphPrime);
    CHECK_EQ(mgr.GetGlyph(0, 3), kGlyphMajor);
    CHECK_EQ(mgr.GetGlyph(1, 3), kGlyphMinor);
    CHECK_EQ(mgr.GetGlyph(1, 8), kGlyphPrime);
    CHECK_EQ(mgr.GetGlyph(0, 8), 0u);
    CHECK_EQ(mgr.GetGlyph(1, 2), 0u);
    mgr.SetGlyph(1, 3, 0);
    CHECK_EQ(mgr.GetGlyph(1, 3), 0u);
    CHECK_EQ(mgr.GetGlyph(0, 3), kGlyphMajor);
}

// The slot types come from the glyph slot store in id order, one per slot, then the enabled mask
// for the level is written last. The store (see the file comment) walks ids 0..11: id 0 absent,
// ids 1-4 -> slots 0-3, id 5 absent, ids 6-10 -> slots 4-8; nine slots written, so the loop stops
// before id 11. Slot and id differ at every call (by one, then by two after the hole).
//
// The mask, by hand: 0x01|0x02|0x40 = 0x43 = 67 from level 25; | 0x04|0x08|0x80 (0x8C) = 0xCF =
// 207 from level 50; | 0x10|0x20|0x100 (0x130) = 0x1FF = 511 from level 75; 0 below 25.
TEST(GlyphMgr_InitGlyphsForLevelSlotsThenEnabledMask)
{
    SeedStores();
    CHECK_EQ(sGlyphSlotStore.GetNumRows(), 12u);

    const std::vector<std::string> slots =
    {
        "slot 0 1", "slot 1 2", "slot 2 3", "slot 3 4", "slot 4 6",
        "slot 5 7", "slot 6 8", "slot 7 9", "slot 8 10",
    };
    struct Row
    {
        uint32 level;
        uint32 mask;
    };
    const Row rows[] =
    {
        { 0, 0 }, { 1, 0 }, { 24, 0 },
        { 25, 67 }, { 49, 67 },
        { 50, 207 }, { 74, 207 },
        { 75, 511 }, { 85, 511 }, { 255, 511 },
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
    {
        GlyphMgr mgr;
        Wire wire;
        mgr.InitGlyphsForLevel(rows[i].level, wire.SlotSink(), wire.FieldSink());
        std::vector<std::string> want = slots;
        want.push_back("field 1357 " + std::to_string(rows[i].mask));
        CheckEvents(wire.events, want, __LINE__);
        if (wire.events != want)
        {
            testing::ReportFailure(__FILE__, __LINE__, "level " + std::to_string(rows[i].level));
        }
    }
}

// Apply casts the glyph's spell and writes the glyph id into PLAYER_FIELD_GLYPHS_1 + slot, in that
// order; remove takes the spell's auras off and writes 0 there. The active spec chooses the row.
// Slot, field index, glyph id and spell id are distinct numbers everywhere: slot 3 is field
// 1348 + 3 = 1351, slot 8 is 1356, slot 0 is 1348.
TEST(GlyphMgr_ApplyGlyphSinksAndFields)
{
    SeedStores();
    GlyphMgr mgr;
    mgr.SetGlyph(0, 3, kGlyphMajor);
    mgr.SetGlyph(1, 3, kGlyphMinor);
    mgr.SetGlyph(0, 8, kGlyphPrime);
    mgr.SetGlyph(1, 0, kGlyphPrime);
    mgr.SetGlyph(0, 5, kGlyphUnknown);

    Wire wire;
    mgr.ApplyGlyph(0, 3, true, wire.Sinks());
    CheckEvents(wire.events, { "cast 96101", "field 1351 94101" }, __LINE__);

    wire.events.clear();
    mgr.ApplyGlyph(1, 3, true, wire.Sinks());
    CheckEvents(wire.events, { "cast 96102", "field 1351 94102" }, __LINE__);

    wire.events.clear();
    mgr.ApplyGlyph(0, 3, false, wire.Sinks());
    CheckEvents(wire.events, { "remove 96101", "field 1351 0" }, __LINE__);

    wire.events.clear();
    mgr.ApplyGlyph(1, 3, false, wire.Sinks());
    CheckEvents(wire.events, { "remove 96102", "field 1351 0" }, __LINE__);

    wire.events.clear();
    mgr.ApplyGlyph(0, 8, true, wire.Sinks());
    mgr.ApplyGlyph(1, 0, true, wire.Sinks());
    CheckEvents(wire.events, { "cast 96103", "field 1356 94103", "cast 96103", "field 1348 94103" }, __LINE__);

    // Nothing at all: an empty slot (spec 0 slot 6), a slot empty in this spec only (spec 1
    // slot 8, spec 0 slot 0), a glyph the property store does not know (spec 0 slot 5) -- for
    // apply and for remove alike.
    wire.events.clear();
    mgr.ApplyGlyph(0, 6, true, wire.Sinks());
    mgr.ApplyGlyph(1, 8, true, wire.Sinks());
    mgr.ApplyGlyph(0, 0, false, wire.Sinks());
    mgr.ApplyGlyph(0, 5, true, wire.Sinks());
    mgr.ApplyGlyph(0, 5, false, wire.Sinks());
    CheckEvents(wire.events, {}, __LINE__);

    // Applying changes no state: the ids are where they were.
    CHECK_EQ(mgr.GetGlyph(0, 3), kGlyphMajor);
    CHECK_EQ(mgr.GetGlyph(1, 3), kGlyphMinor);
}

// One pass over the rows, as the owner's loop does it. The owner's glyph slot fields are the
// test's `fields` array; the read callback records each read, so the sequence shows where the old
// body read the field (once to look the slot type up, once more for the log line when it is
// missing, never for an unknown glyph) interleaved with the DELETEs the database was handed.
//
// Rows (spec, slot, glyph), by hand:
//   (0, 1, 94101): slot 1's field is 2, type 0 = the glyph's flags 0 -> stored in [0][1].
//   (1, 4, 94102): slot 4's field is 6, type 1 = flags 1 -> stored in [1][4].
//   (0, 3, 94999): no glyph property: DELETE by glyph, for every character; no field read.
//   (1, 2, 94101): slot 2's field is 5, which the slot store lacks: read twice, DELETE this row.
//   (0, 8, 94102): slot 8's field is 10, type 2 != flags 1: DELETE this row.
//   (1, 8, 94103): slot 8's field is 10, type 2 = flags 2 -> stored in [1][8].
// Slot and spec differ in both per-character DELETEs, so a swapped pair of arguments shows.
TEST(GlyphMgr_LoadRowValidAndEachInvalidKind)
{
    SeedStores();
    GlyphMgr mgr;

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

    const uint32 fields[MAX_GLYPH_SLOT_INDEX] = { 1, 2, 5, 4, 6, 7, 8, 9, 10 };
    GlyphMgr::RowInputs inputs;
    inputs.ownerGuidLow = kGuid;
    inputs.ownerName = kName;
    inputs.glyphSlot = [&](uint8 slot)
    {
        sync();
        events.push_back("read " + std::to_string(slot));
        return fields[slot];
    };

    FakeQueryResult result(FakeRows{
        GlyphRow(0, 1, kGlyphMajor),
        GlyphRow(1, 4, kGlyphMinor),
        GlyphRow(0, 3, kGlyphUnknown),
        GlyphRow(1, 2, kGlyphMajor),
        GlyphRow(0, 8, kGlyphMinor),
        GlyphRow(1, 8, kGlyphPrime),
    });
    REQUIRE(result.NextRow());
    do
    {
        mgr.LoadRow(result.Fetch(), inputs);
        sync();
    }
    while (result.NextRow());

    const std::vector<std::string> expected =
    {
        "read 1",
        "read 4",
        "DELETE FROM `character_glyphs` WHERE `glyph` = 94999",
        "read 2",
        "read 2",
        "DELETE FROM `character_glyphs` WHERE `slot` = 2 AND `spec` = 1 AND `guid` = 42",
        "read 8",
        "DELETE FROM `character_glyphs` WHERE `slot` = 8 AND `spec` = 0 AND `guid` = 42",
        "read 8",
    };
    CheckEvents(events, expected, __LINE__);
    CHECK_EQ(query.executed.size(), size_t(0));

    CHECK_EQ(mgr.GetGlyph(0, 1), kGlyphMajor);
    CHECK_EQ(mgr.GetGlyph(1, 4), kGlyphMinor);
    CHECK_EQ(mgr.GetGlyph(1, 8), kGlyphPrime);
    CHECK_EQ(mgr.GetGlyph(0, 3), 0u);
    CHECK_EQ(mgr.GetGlyph(1, 2), 0u);
    CHECK_EQ(mgr.GetGlyph(0, 8), 0u);
    CHECK_EQ(mgr.GetGlyph(1, 1), 0u);
    CHECK_EQ(mgr.GetGlyph(0, 4), 0u);

    // KEPT: a loaded glyph is UNCHANGED, so a save right after the load writes nothing.
    mgr.Save(kGuid, 2);
    sync();
    CHECK_EQ(events.size(), expected.size());

    // The guid is a parameter: another guid lands in the per-character DELETE.
    events.clear();
    inputs.ownerGuidLow = 7;
    FakeQueryResult other(FakeRows{ GlyphRow(1, 2, kGlyphMajor) });
    REQUIRE(other.NextRow());
    mgr.LoadRow(other.Fetch(), inputs);
    sync();
    CheckEvents(events, { "read 2", "read 2",
                          "DELETE FROM `character_glyphs` WHERE `slot` = 2 AND `spec` = 1 AND `guid` = 7" }, __LINE__);

    CHECK_EQ(TickGuard::Violations(), 0u);
}

// The save walks the first specsCount specs, each slot in order, and writes one statement per
// dirty slot: NEW an INSERT (guid, spec, slot, glyph), CHANGED an UPDATE (glyph; guid, spec, slot),
// DELETED a DELETE (guid, spec, slot); then every slot it visited is UNCHANGED.
//
// Set up, by hand (slot and spec differ in every statement, so a swapped pair shows):
//   spec 0: slot 2 loaded 94102 then set 94103 (CHANGED); slot 3 set 94101 (NEW); slot 5 loaded
//           94101 then cleared (DELETED); slot 7 set then cleared (NEW -> UNCHANGED: nothing);
//           slot 8 loaded 94103 (UNCHANGED: nothing).
//   spec 1: slot 0 set 94102 (NEW); slot 4 loaded 94101 then cleared (DELETED); slot 6 loaded 94102
//           then set 94101 (CHANGED).
// A save with a spec count of 1 writes spec 0's three and leaves spec 1 dirty; a save with 2 then
// writes spec 1's three only; a third writes nothing.
TEST(GlyphMgr_SaveNewChangedDeletedAcrossTwoSpecs)
{
    SeedStores();

    TickGuard::ResetViolations();
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true);

    GlyphMgr mgr;
    // Valid rows: the read answers a slot type that matches each glyph's flags (id 6 is type 1,
    // id 1 type 0, id 10 type 2).
    LoadValid(mgr, 0, 2, kGlyphMinor, 6);
    LoadValid(mgr, 0, 5, kGlyphMajor, 1);
    LoadValid(mgr, 0, 8, kGlyphPrime, 10);
    LoadValid(mgr, 1, 4, kGlyphMajor, 1);
    LoadValid(mgr, 1, 6, kGlyphMinor, 6);
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.empty());                    // every row was valid: no DELETE

    mgr.SetGlyph(0, 2, kGlyphPrime);
    mgr.SetGlyph(0, 3, kGlyphMajor);
    mgr.SetGlyph(0, 5, 0);
    mgr.SetGlyph(0, 7, kGlyphMinor);
    mgr.SetGlyph(0, 7, 0);
    mgr.SetGlyph(1, 0, kGlyphMinor);
    mgr.SetGlyph(1, 4, 0);
    mgr.SetGlyph(1, 6, kGlyphMajor);

    {
        TickGuard::Scope scope;
        mgr.Save(kGuid, 1);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(query.executed.size(), size_t(0));
    }
    CharacterDatabase.ExecuteQueuedForTest();
    const std::vector<std::string> spec0 =
    {
        UpdateSql(0, 2, kGlyphPrime),
        InsertSql(0, 3, kGlyphMajor),
        DeleteSql(0, 5),
    };
    CheckEvents(async.executed, spec0, __LINE__);
    // The literal text of each kind once, so the helpers themselves are pinned.
    REQUIRE(async.executed.size() == size_t(3));
    CHECK_STR(async.executed[0], "UPDATE `character_glyphs` SET `glyph` = '94103' WHERE `guid` = '42' AND `spec` = '0' AND `slot` = '2'");
    CHECK_STR(async.executed[1], "INSERT INTO `character_glyphs` (`guid`, `spec`, `slot`, `glyph`) VALUES ('42', '0', '3', '94101')");
    CHECK_STR(async.executed[2], "DELETE FROM `character_glyphs` WHERE `guid` = '42' AND `spec` = '0' AND `slot` = '5'");

    mgr.Save(kGuid, 2);
    CharacterDatabase.ExecuteQueuedForTest();
    std::vector<std::string> both = spec0;
    both.push_back(InsertSql(1, 0, kGlyphMinor));
    both.push_back(DeleteSql(1, 4));
    both.push_back(UpdateSql(1, 6, kGlyphMajor));
    CheckEvents(async.executed, both, __LINE__);

    // Everything saved is UNCHANGED now: a third save writes nothing.
    mgr.Save(kGuid, 2);
    CharacterDatabase.ExecuteQueuedForTest();
    CHECK_EQ(async.executed.size(), both.size());
    CHECK_EQ(query.executed.size(), size_t(0));

    // The ids are what the session left.
    CHECK_EQ(mgr.GetGlyph(0, 2), kGlyphPrime);
    CHECK_EQ(mgr.GetGlyph(0, 3), kGlyphMajor);
    CHECK_EQ(mgr.GetGlyph(0, 5), 0u);
    CHECK_EQ(mgr.GetGlyph(0, 7), 0u);
    CHECK_EQ(mgr.GetGlyph(0, 8), kGlyphPrime);
    CHECK_EQ(mgr.GetGlyph(1, 0), kGlyphMinor);
    CHECK_EQ(mgr.GetGlyph(1, 4), 0u);
    CHECK_EQ(mgr.GetGlyph(1, 6), kGlyphMajor);

    // A slot cleared and saved (its DELETE ran; it is UNCHANGED with id 0 now) and then set again
    // is NEW: an INSERT. Only before a save does a set after a clear read DELETED -> CHANGED.
    mgr.SetGlyph(0, 5, kGlyphMinor);
    mgr.Save(kGuid, 1);
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == both.size() + 1);
    CHECK_STR(async.executed.back(), InsertSql(0, 5, kGlyphMinor));

    CHECK_EQ(TickGuard::Violations(), 0u);
}
