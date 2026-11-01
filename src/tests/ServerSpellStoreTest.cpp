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

/// The server-side spell records: ObjectMgr::FillServerSpell puts each `spell_template` column in the
/// field it names and nothing elsewhere, and leaves the effect out when the row has none or its aura
/// needs base points. ServerSpellStore owns the records and the name, and keeps their addresses.
/// DBCStorage::AppendEntry adds an entry past the last row.

#include "TestHarness.h"
#include "ObjectMgr.h"
#include "DBCStores.h"
#include "data/ServerSpellStore.h"
#include "persistence/world/spells/SpellTemplateRepository.h"
#include "DataStores/DBCStore.h"
#include "spells/data/SpellAuraDefines.h"

#include <cstring>

namespace
{
    SpellTemplateRow DistinctRow()
    {
        SpellTemplateRow row;
        row.id = 70001;
        row.attributes = 101;
        row.attributesEx = 102;
        row.attributesEx2 = 103;
        row.attributesEx3 = 104;
        row.procFlags = 105;
        row.procChance = 106;
        row.durationIndex = 107;
        row.effect = SPELL_EFFECT_APPLY_AURA;
        row.effectImplicitTargetA = 109;
        row.effectImplicitTargetB = 110;
        row.effectRadiusIndex = 111;
        row.effectApplyAuraName = SPELL_AURA_PROC_TRIGGER_SPELL;
        row.effectMiscValue = 113;
        row.effectMiscValueB = 114;
        row.effectTriggerSpell = 115;
        row.comments = "a server-side spell";
        return row;
    }
}

// Each column lands in the field it names; every other field of the three records is 0, and the
// main record keeps the store's name.
TEST(FillServerSpell_EachColumnInItsField)
{
    ServerSpellStore store;
    ServerSpell& spell = store.Add(70001, "a server-side spell");
    ObjectMgr::FillServerSpell(DistinctRow(), spell);
    CHECK(spell.hasEffect);

    char expectedBytes[sizeof(SpellEntry)] = {};
    SpellEntry& expected = *reinterpret_cast<SpellEntry*>(expectedBytes);
    expected.ID = 70001;
    expected.Attributes = 101;
    expected.AttributesEx = 102;
    expected.AttributesExB = 103;
    expected.AttributesExC = 104;
    expected.DurationIndex = 107;
    expected.Name_lang = spell.Entry().Name_lang;
    expected.NameSubtext_lang = spell.Entry().NameSubtext_lang;
    CHECK(std::memcmp(&spell.Entry(), &expected, sizeof(SpellEntry)) == 0);

    CHECK_EQ(spell.auraOptions.CumulativeAura, 0u);
    CHECK_EQ(spell.auraOptions.ProcChance, 106u);
    CHECK_EQ(spell.auraOptions.ProcCharges, 0u);
    CHECK_EQ(spell.auraOptions.ProcTypeMask, 105u);

    SpellEffectEntry expectedEffect = SpellEffectEntry();
    expectedEffect.Effect = SPELL_EFFECT_APPLY_AURA;
    expectedEffect.ImplicitTarget_0 = 109;
    expectedEffect.ImplicitTarget_1 = 110;
    expectedEffect.EffectRadiusIndex_0 = 111;
    expectedEffect.EffectAura = SPELL_AURA_PROC_TRIGGER_SPELL;
    expectedEffect.EffectMiscValue_0 = 113;
    expectedEffect.EffectMiscValue_1 = 114;
    expectedEffect.EffectTriggerSpell = 115;
    expectedEffect.SpellID = 70001;
    expectedEffect.EffectIndex = 0;
    CHECK(std::memcmp(&spell.effect, &expectedEffect, sizeof(SpellEffectEntry)) == 0);
}

// The effect is loaded for no aura, a dummy, a proc trigger and a transform; not for an aura that
// needs base points, nor for a row with no effect.
TEST(FillServerSpell_EffectLoadedOnlyWhenTheTableCanHoldIt)
{
    ServerSpellStore store;
    SpellTemplateRow row = DistinctRow();

    uint32 const loaded[] = { SPELL_AURA_NONE, SPELL_AURA_DUMMY, SPELL_AURA_PROC_TRIGGER_SPELL, SPELL_AURA_TRANSFORM };
    for (uint32 aura : loaded)
    {
        row.effectApplyAuraName = aura;
        ServerSpell& spell = store.Add(row.id, row.comments);
        ObjectMgr::FillServerSpell(row, spell);
        CHECK(spell.hasEffect);
    }

    row.effectApplyAuraName = SPELL_AURA_FORCE_REACTION;
    ServerSpell& reaction = store.Add(row.id, row.comments);
    ObjectMgr::FillServerSpell(row, reaction);
    CHECK(!reaction.hasEffect);
    CHECK_EQ(reaction.Entry().ID, 70001u);

    row.effectApplyAuraName = SPELL_AURA_NONE;
    row.effect = 0;
    ServerSpell& none = store.Add(row.id, row.comments);
    ObjectMgr::FillServerSpell(row, none);
    CHECK(!none.hasEffect);
}

// The store zeroes a spell's records but for its id and name, names it in every locale with no
// subtext, keeps the records' addresses when more spells are added, and replaces a spell added again.
TEST(ServerSpellStore_OwnsRecordsAndName)
{
    ServerSpellStore store;
    ServerSpell& spell = store.Add(70001, "a server-side spell");
    CHECK(!spell.hasEffect);
    CHECK_EQ(spell.Entry().ID, 70001u);
    CHECK_EQ(spell.Entry().Attributes, 0u);
    CHECK_EQ(spell.Entry().AuraOptionsID, 0u);
    CHECK_EQ(spell.effect.Effect, 0u);
    CHECK_EQ(spell.auraOptions.ProcChance, 0u);
    REQUIRE(spell.Entry().Name_lang != NULL);
    REQUIRE(spell.Entry().NameSubtext_lang != NULL);
    for (uint32 i = 0; i < MAX_LOCALE; ++i)
    {
        CHECK_STR(spell.Entry().Name_lang[i], "a server-side spell");
        CHECK_STR(spell.Entry().NameSubtext_lang[i], "");
    }

    SpellEntry const* address = &spell.Entry();
    for (uint32 id = 70002; id < 70100; ++id)
    {
        store.Add(id, "another");
    }
    CHECK_EQ(store.GetCount(), 99u);
    REQUIRE(store.Find(70001) != NULL);
    CHECK(&store.Find(70001)->Entry() == address);
    CHECK_STR(store.Find(70001)->Entry().Name_lang[0], "a server-side spell");
    CHECK(store.Find(69999) == NULL);

    ServerSpell& again = store.Add(70050, "replaced");
    again.Entry().Attributes = 7;
    CHECK_EQ(store.GetCount(), 99u);
    CHECK_STR(store.Find(70050)->Entry().Name_lang[0], "replaced");
    CHECK_EQ(store.Find(70050)->Entry().Attributes, 7u);
}

// An appended entry takes the id one past the last row, and the earlier entries stay.
TEST(DBCStorage_AppendEntryAddsPastTheLastRow)
{
    DBCStorage<SpellAuraOptionsEntry> store("diiii");
    SpellAuraOptionsEntry first = SpellAuraOptionsEntry();
    SpellAuraOptionsEntry second = SpellAuraOptionsEntry();
    CHECK_EQ(store.AppendEntry(&first), 0u);
    CHECK_EQ(store.AppendEntry(&second), 1u);
    CHECK_EQ(store.GetNumRows(), 2u);
    CHECK(store.LookupEntry(0) == &first);
    CHECK(store.LookupEntry(1) == &second);
    CHECK(store.LookupEntry(2) == NULL);
}
