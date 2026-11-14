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

/// A character's spell modifiers with no character: the sums a modifier change reports, computed
/// before the list changes; the removal of every entry of the same aura; the application of the
/// listed modifiers to a spell's value for each of the three value types, with the family and
/// class mask filter, the skips for a zero value and for the instant-cast case; the amount read
/// through its address at each use; the assertion on a sink no session installed; and the units
/// that answer no spell modifiers.
///
/// Spell.dbc and SpellClassOptions.dbc rows are seeded with DBCStorage::SetEntry, as
/// TalentMgrTest.cpp does; the ids below are used by no other test in this binary.

#include "TestHarness.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Pet.h"
#include "Totem.h"
#include "Unit.h"
#include "spells/PeriodicTiming.h"
#include "spells/SpellModMgr.h"

#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    const uint32 kSpellMage = 94101;        // family mage, class mask bit 1 and bit 66
    const uint32 kSpellMageOther = 94102;   // family mage, class mask bit 2
    const uint32 kSpellPriest = 94103;      // family priest, class mask bit 1
    const uint32 kSpellNoOptions = 94104;   // no SpellClassOptions row
    const uint32 kSpellUnknown = 94199;     // not in the store

    const uint32 kOptionsMage = 94101;
    const uint32 kOptionsMageOther = 94102;
    const uint32 kOptionsPriest = 94103;

    SpellClassOptionsEntry s_optionsMage;
    SpellClassOptionsEntry s_optionsMageOther;
    SpellClassOptionsEntry s_optionsPriest;

    // Spell.dbc rows as the loader makes them (SpellEntry has no default constructor).
    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    SpellRowStorage s_rows[4] = {};

    SpellEntry* Row(size_t slot, uint32 id, uint32 classOptionsId)
    {
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_rows[slot].bytes);
        row->ID = id;
        row->ClassOptionsID = classOptionsId;
        return row;
    }

    void SeedStores()
    {
        static bool seeded = false;
        if (seeded)
        {
            return;
        }
        seeded = true;

        s_optionsMage.SpellClassMask = ClassFamilyMask(UI64LIT(0x0000000000000002), 0x00000004);
        s_optionsMage.SpellClassSet = SPELLFAMILY_MAGE;
        s_optionsMageOther.SpellClassMask = ClassFamilyMask(UI64LIT(0x0000000000000004));
        s_optionsMageOther.SpellClassSet = SPELLFAMILY_MAGE;
        s_optionsPriest.SpellClassMask = ClassFamilyMask(UI64LIT(0x0000000000000002));
        s_optionsPriest.SpellClassSet = SPELLFAMILY_PRIEST;
        sSpellClassOptionsStore.SetEntry(kOptionsMage, &s_optionsMage);
        sSpellClassOptionsStore.SetEntry(kOptionsMageOther, &s_optionsMageOther);
        sSpellClassOptionsStore.SetEntry(kOptionsPriest, &s_optionsPriest);

        sSpellStore.SetEntry(kSpellMage, Row(0, kSpellMage, kOptionsMage));
        sSpellStore.SetEntry(kSpellMageOther, Row(1, kSpellMageOther, kOptionsMageOther));
        sSpellStore.SetEntry(kSpellPriest, Row(2, kSpellPriest, kOptionsPriest));
        sSpellStore.SetEntry(kSpellNoOptions, Row(3, kSpellNoOptions, 0));
    }

    /// A modifier as an aura holds it: an identity, an amount and a class mask the entry points at.
    struct FakeModifier
    {
        float amount;
        ClassFamilyMask mask;
        Aura const* aura;

        FakeModifier(float amountIn, ClassFamilyMask maskIn, uintptr_t identity)
            : amount(amountIn), mask(maskIn), aura(reinterpret_cast<Aura const*>(identity)) { }

        SpellModEntry Entry(bool flat, SpellModOp op, SpellFamily family = SPELLFAMILY_MAGE, bool mastery = false) const
        {
            SpellModEntry entry = { aura, flat, &amount, int32(op), uint32(family), &mask, mastery };
            return entry;
        }
    };

    /// Records every fact reported.
    struct Recorder
    {
        std::vector<SpellModChangedFact> facts;

        SpellModChangedSink Sink()
        {
            return [this](SpellModChangedFact const& fact)
            {
                facts.push_back(fact);
            };
        }
    };

    bool SameValues(SpellModChangedFact const& fact, std::vector<std::pair<int, float> > const& want)
    {
        if (fact.values.size() != want.size())
        {
            return false;
        }
        for (size_t i = 0; i < want.size(); ++i)
        {
            if (int(fact.values[i].effect) != want[i].first || fact.values[i].value != want[i].second)
            {
                return false;
            }
        }
        return true;
    }

    /// A bare Creature; the spell modifier pointer Unit holds is public here.
    class CreatureProbe : public Creature
    {
        public:
            using Unit::m_spellMods;

            CreatureProbe() : Creature(CREATURE_SUBTYPE_GENERIC) { }
    };

    /// A Pet or a Totem with its update fields allocated (the owner's guid is one), never in a
    /// world; the spell modifier pointer Unit holds is public here.
    template <class Base>
    class OwnedProbe : public Base
    {
        public:
            using Unit::m_spellMods;

            OwnedProbe()
            {
                this->_InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~OwnedProbe()
            {
                delete[] this->m_uint32Values;
                this->m_uint32Values = NULL;
            }
    };

    typedef OwnedProbe<Pet> PetProbe;
    typedef OwnedProbe<Totem> TotemProbe;

    /// The assertion cases end the process, so each runs only when MANGOS_TESTS_ABORT_CASE names
    /// it (CheckAbortCase.cmake); any other run returns at once.
    bool AbortCaseSelected(char const* name)
    {
        char const* selected = std::getenv("MANGOS_TESTS_ABORT_CASE");
        if (!selected || std::strcmp(selected, name) != 0)
        {
            return false;
        }
#ifdef _MSC_VER
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
        return true;
    }

    const ClassFamilyMask kBit1(UI64LIT(0x0000000000000002));
}

static_assert(std::is_same<decltype(&Unit::GetSpellMods), SpellModMgr* (Unit::*)() const>::value,
              "Unit::GetSpellMods answers the spell modifiers by pointer and changes nothing");

// A spell not in the store gets nothing: the call answers 0 and leaves the value alone.
TEST(SpellModMgr_AnAbsentSpellReturnsZeroAndLeavesTheValue)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier flat(5, kBit1, 1);
    mgr.Change(flat.Entry(true, SPELLMOD_DAMAGE), true, recorder.Sink());

    int32 value = 100;
    CHECK_EQ(mgr.ApplySpellMod(kSpellUnknown, SPELLMOD_DAMAGE, value), int32(0));
    CHECK_EQ(value, int32(100));
}

// Flat amounts add up and are added to the value; the call answers what was added.
TEST(SpellModMgr_FlatAmountsAreSummed)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier first(5, kBit1, 1);
    FakeModifier second(7, kBit1, 2);
    mgr.Change(first.Entry(true, SPELLMOD_DAMAGE), true, recorder.Sink());
    mgr.Change(second.Entry(true, SPELLMOD_DAMAGE), true, recorder.Sink());

    int32 value = 100;
    CHECK_EQ(mgr.ApplySpellMod(kSpellMage, SPELLMOD_DAMAGE, value), int32(12));
    CHECK_EQ(value, int32(112));
}

// A cleared manager forgets every modifier and tells no client: nothing applies afterwards.
TEST(SpellModMgr_ClearingForgetsEveryModifierWithoutAReport)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier first(5, kBit1, 1);
    mgr.Change(first.Entry(true, SPELLMOD_DAMAGE), true, recorder.Sink());
    size_t const reported = recorder.facts.size();

    mgr.Clear();
    CHECK_EQ(recorder.facts.size(), reported);

    int32 value = 100;
    CHECK_EQ(mgr.ApplySpellMod(kSpellMage, SPELLMOD_DAMAGE, value), int32(0));
    CHECK_EQ(value, int32(100));
}

// Percentages add up and scale the value before the flat amounts are added.
TEST(SpellModMgr_PercentagesAreSummed)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier first(10, kBit1, 1);
    FakeModifier second(20, kBit1, 2);
    FakeModifier flat(3, kBit1, 3);
    mgr.Change(first.Entry(false, SPELLMOD_DAMAGE), true, recorder.Sink());
    mgr.Change(second.Entry(false, SPELLMOD_DAMAGE), true, recorder.Sink());
    mgr.Change(flat.Entry(true, SPELLMOD_DAMAGE), true, recorder.Sink());

    int32 value = 100;
    CHECK_EQ(mgr.ApplySpellMod(kSpellMage, SPELLMOD_DAMAGE, value), int32(33));
    CHECK_EQ(value, int32(133));
}

// A zero value takes no percentage, only the flat amounts.
TEST(SpellModMgr_AZeroValueSkipsThePercentages)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier pct(50, kBit1, 1);
    FakeModifier flat(4, kBit1, 2);
    mgr.Change(pct.Entry(false, SPELLMOD_COST), true, recorder.Sink());
    mgr.Change(flat.Entry(true, SPELLMOD_COST), true, recorder.Sink());

    int32 value = 0;
    CHECK_EQ(mgr.ApplySpellMod(kSpellMage, SPELLMOD_COST, value), int32(4));
    CHECK_EQ(value, int32(4));
}

// A casting time of 10 s or more takes no percentage of -100 or less (the instant-cast case);
// a shorter one, a smaller cut, or another operation does.
TEST(SpellModMgr_TheInstantCastCaseSkipsLongCasts)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier instant(-100, kBit1, 1);
    mgr.Change(instant.Entry(false, SPELLMOD_CASTING_TIME), true, recorder.Sink());

    uint32 longCast = 10000;
    CHECK_EQ(mgr.ApplySpellMod(kSpellMage, SPELLMOD_CASTING_TIME, longCast), uint32(0));
    CHECK_EQ(longCast, uint32(10000));

    uint32 shortCast = 9999;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_CASTING_TIME, shortCast);
    CHECK_EQ(shortCast, uint32(0));

    instant.amount = -99;
    uint32 smallerCut = 10000;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_CASTING_TIME, smallerCut);
    CHECK_EQ(smallerCut, uint32(100));

    FakeModifier otherOp(-100, kBit1, 2);
    mgr.Change(otherOp.Entry(false, SPELLMOD_DURATION), true, recorder.Sink());
    int32 duration = 10000;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DURATION, duration);
    CHECK_EQ(duration, int32(0));
}

// A modifier applies only to a spell of its family whose class mask shares a bit with its own,
// in either word, and only to its own operation; a spell with no class options takes none.
TEST(SpellModMgr_TheFamilyAndTheMaskFilter)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier low(1, kBit1, 1);
    FakeModifier high(10, ClassFamilyMask(0, 0x00000004), 2);
    FakeModifier priest(100, kBit1, 3);
    mgr.Change(low.Entry(true, SPELLMOD_RANGE), true, recorder.Sink());
    mgr.Change(high.Entry(true, SPELLMOD_RANGE), true, recorder.Sink());
    mgr.Change(priest.Entry(true, SPELLMOD_RANGE, SPELLFAMILY_PRIEST), true, recorder.Sink());

    float mage = 0.0f;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_RANGE, mage);
    CHECK(mage == 11.0f);

    float mageOther = 0.0f;
    mgr.ApplySpellMod(kSpellMageOther, SPELLMOD_RANGE, mageOther);
    CHECK(mageOther == 0.0f);

    float priestSpell = 0.0f;
    mgr.ApplySpellMod(kSpellPriest, SPELLMOD_RANGE, priestSpell);
    CHECK(priestSpell == 100.0f);

    float noOptions = 0.0f;
    mgr.ApplySpellMod(kSpellNoOptions, SPELLMOD_RANGE, noOptions);
    CHECK(noOptions == 0.0f);

    float otherOp = 0.0f;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_RADIUS, otherOp);
    CHECK(otherOp == 0.0f);
}

// The three value types: the difference is computed in float and converted back to each.
TEST(SpellModMgr_EachValueType)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier pct(-25, kBit1, 1);
    FakeModifier flat(3, kBit1, 2);
    mgr.Change(pct.Entry(false, SPELLMOD_THREAT), true, recorder.Sink());
    mgr.Change(flat.Entry(true, SPELLMOD_THREAT), true, recorder.Sink());

    // 10 * -25 / 100 + 3 = 0.5: int32(0.5) = 0, int32(10.5) = 10.
    int32 signedValue = 10;
    CHECK_EQ(mgr.ApplySpellMod(kSpellMage, SPELLMOD_THREAT, signedValue), int32(0));
    CHECK_EQ(signedValue, int32(10));

    // 7 * -25 / 100 + 3 = 1.25: uint32(1.25) = 1, uint32(8.25) = 8.
    uint32 unsignedValue = 7;
    CHECK_EQ(mgr.ApplySpellMod(kSpellMage, SPELLMOD_THREAT, unsignedValue), uint32(1));
    CHECK_EQ(unsignedValue, uint32(8));

    // 2 * -25 / 100 + 3 = 2.5.
    float floatValue = 2.0f;
    CHECK(mgr.ApplySpellMod(kSpellMage, SPELLMOD_THREAT, floatValue) == 2.5f);
    CHECK(floatValue == 4.5f);
}

// The amount is read through its address at each use: a write to it while the modifier is listed
// changes what it adds.
TEST(SpellModMgr_TheAmountIsReadAtEachUse)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    float amount = 10.0f;
    ClassFamilyMask mask = kBit1;
    SpellModEntry entry = { reinterpret_cast<Aura const*>(uintptr_t(1)), false, &amount, int32(SPELLMOD_DAMAGE),
                            uint32(SPELLFAMILY_MAGE), &mask, false };
    mgr.Change(entry, true, recorder.Sink());

    float before = 100.0f;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DAMAGE, before);
    CHECK(before == 110.0f);

    amount = 30.0f;

    float after = 100.0f;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DAMAGE, after);
    CHECK(after == 130.0f);
}

// A percentage keeps its fraction: Flashburn's 22.4 on the DoT operation takes a tick of 1000 to 1224, and 38.018
// (1000 mastery rating) to 1380; the client is told 22.4, not 22.
TEST(SpellModMgr_AFractionalPercentageKeepsItsFraction)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier flashburn(22.4f, kBit1, 1);
    mgr.Change(flashburn.Entry(false, SPELLMOD_DOT, SPELLFAMILY_MAGE, true), true, recorder.Sink());

    REQUIRE(recorder.facts.size() == 1u);
    REQUIRE(recorder.facts[0].values.size() == 1u);
    CHECK(recorder.facts[0].values[0].value == 22.4f);

    uint32 tick = 1000;
    CHECK_EQ(mgr.ApplySpellMod(kSpellMage, SPELLMOD_DOT, tick), uint32(224));
    CHECK_EQ(tick, uint32(1224));

    flashburn.amount = 38.018f;
    uint32 rated = 1000;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DOT, rated);
    CHECK_EQ(rated, uint32(1380));
}

// The mastery-only apply takes the modifiers whose aura's spell is a mastery and leaves the others out: 22.4 from a
// mastery beside an ordinary +15 (Critical Mass' shape) gives 1374 in the plain apply and 1224 in the mastery-only
// one.
TEST(SpellModMgr_TheMasteryOnlyApplyTakesTheMasteryModifiersAlone)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier mastery(22.4f, kBit1, 1);
    FakeModifier ordinary(15.0f, kBit1, 2);
    mgr.Change(mastery.Entry(false, SPELLMOD_DOT, SPELLFAMILY_MAGE, true), true, recorder.Sink());
    mgr.Change(ordinary.Entry(false, SPELLMOD_DOT), true, recorder.Sink());

    uint32 plain = 1000;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DOT, plain);
    CHECK_EQ(plain, uint32(1374));

    uint32 masteryOnly = 1000;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DOT, masteryOnly, true);
    CHECK_EQ(masteryOnly, uint32(1224));

    float masteryOnlyFloat = 1000.0f;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DOT, masteryOnlyFloat, true);
    CHECK(uint32(masteryOnlyFloat) == 1224u);

    // without a mastery listed, the mastery-only apply leaves the value alone
    mgr.Change(mastery.Entry(false, SPELLMOD_DOT, SPELLFAMILY_MAGE, true), false, recorder.Sink());
    uint32 none = 1000;
    CHECK_EQ(mgr.ApplySpellMod(kSpellMage, SPELLMOD_DOT, none, true), uint32(0));
    CHECK_EQ(none, uint32(1000));
}

// Two Ignites in a row under 8 mastery points (Flashburn's 22.4 listed as a mastery on the DoT operation): the first
// one's tick of 1000 takes the mastery step once, 1224. A second crit with one of the first one's two ticks left
// re-applies it, and the rollover spreads the new two ticks and the carried one over three (Spells::RolledOverTick):
// with the carried tick as it was before its own step, (1000 x 2 + 1000 x 1) / 3 = 1000, and the step gives 1224.
// Carrying the stepped 1224 instead would make (2000 + 1224) / 3 = 1074 and 1314: the carried part would take
// Flashburn twice.
TEST(SpellModMgr_ARefreshedIgniteTakesTheMasteryOnce)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier flashburn(22.4f, kBit1, 1);
    mgr.Change(flashburn.Entry(false, SPELLMOD_DOT, SPELLFAMILY_MAGE, true), true, recorder.Sink());

    const int32 share = 1000;
    float first = float(share);
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DOT, first, true);
    CHECK_EQ(uint32(first), uint32(1224));

    const int32 carried = Spells::RolledOverTick(share, 2, share, 1, 3);
    CHECK_EQ(carried, int32(1000));
    float refreshed = float(carried);
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DOT, refreshed, true);
    CHECK_EQ(uint32(refreshed), uint32(1224));

    const int32 steppedTwice = Spells::RolledOverTick(share, 2, int32(first), 1, 3);
    CHECK_EQ(steppedTwice, int32(1074));
    float twice = float(steppedTwice);
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DOT, twice, true);
    CHECK_EQ(uint32(twice), uint32(1314));
}

// A mastery update lists its modifier again: the removal reports the sums without it, the new value is written, and
// the add reports the sums with it, so the client is told the new fraction.
TEST(SpellModMgr_ListingAgainReportsTheNewValue)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier flashburn(22.4f, kBit1, 1);
    mgr.Change(flashburn.Entry(false, SPELLMOD_DOT, SPELLFAMILY_MAGE, true), true, recorder.Sink());

    mgr.Change(flashburn.Entry(false, SPELLMOD_DOT, SPELLFAMILY_MAGE, true), false, recorder.Sink());
    flashburn.amount = 38.018f;
    mgr.Change(flashburn.Entry(false, SPELLMOD_DOT, SPELLFAMILY_MAGE, true), true, recorder.Sink());

    REQUIRE(recorder.facts.size() == 3u);
    REQUIRE(recorder.facts[1].values.size() == 1u);
    REQUIRE(recorder.facts[2].values.size() == 1u);
    CHECK(recorder.facts[1].values[0].value == 0.0f);
    CHECK(recorder.facts[2].values[0].value == 38.018f);

    uint32 tick = 1000;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DOT, tick);
    CHECK_EQ(tick, uint32(1380));
}

// The fact of a change is computed from the list before it changes: on an add, the listed sums
// plus the new amount; on a removal, the listed sums (the removed one included) minus it.
TEST(SpellModMgr_TheFactIsComputedBeforeTheListChanges)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier first(25, ClassFamilyMask(UI64LIT(0x0000000000000021)), 1);
    FakeModifier second(3, ClassFamilyMask(UI64LIT(0x0000000000000020), 0x00000040), 2);

    mgr.Change(first.Entry(true, SPELLMOD_CRITICAL_CHANCE), true, recorder.Sink());
    mgr.Change(second.Entry(true, SPELLMOD_CRITICAL_CHANCE), true, recorder.Sink());
    mgr.Change(first.Entry(true, SPELLMOD_CRITICAL_CHANCE), false, recorder.Sink());
    mgr.Change(second.Entry(true, SPELLMOD_CRITICAL_CHANCE), false, recorder.Sink());

    REQUIRE(recorder.facts.size() == 4u);
    for (SpellModChangedFact const& fact : recorder.facts)
    {
        CHECK(fact.flat);
        CHECK_EQ(uint32(fact.op), uint32(SPELLMOD_CRITICAL_CHANCE));
    }
    CHECK(SameValues(recorder.facts[0], { { 0, 25 }, { 5, 25 } }));
    CHECK(SameValues(recorder.facts[1], { { 5, 28 }, { 70, 3 } }));
    CHECK(SameValues(recorder.facts[2], { { 0, 0 }, { 5, 3 } }));
    CHECK(SameValues(recorder.facts[3], { { 5, 0 }, { 70, 0 } }));

    int32 value = 10;
    CHECK_EQ(mgr.ApplySpellMod(kSpellMage, SPELLMOD_CRITICAL_CHANCE, value), int32(0));
}

// Inner Focus as the harness applies and removes it (scenario 934): a percentage of -100 on the
// cost and a flat +25 on the critical chance, both on class mask bits 9, 11, 12 and 34.
TEST(SpellModMgr_InnerFocusFacts)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    ClassFamilyMask focusMask(UI64LIT(0x0000000400001A00));
    FakeModifier cost(-100, focusMask, 1);
    FakeModifier crit(25, focusMask, 2);

    mgr.Change(cost.Entry(false, SPELLMOD_COST, SPELLFAMILY_PRIEST), true, recorder.Sink());
    mgr.Change(crit.Entry(true, SPELLMOD_CRITICAL_CHANCE, SPELLFAMILY_PRIEST), true, recorder.Sink());
    mgr.Change(cost.Entry(false, SPELLMOD_COST, SPELLFAMILY_PRIEST), false, recorder.Sink());
    mgr.Change(crit.Entry(true, SPELLMOD_CRITICAL_CHANCE, SPELLFAMILY_PRIEST), false, recorder.Sink());

    REQUIRE(recorder.facts.size() == 4u);
    CHECK(!recorder.facts[0].flat);
    CHECK_EQ(uint32(recorder.facts[0].op), uint32(14));
    CHECK(SameValues(recorder.facts[0], { { 9, -100 }, { 11, -100 }, { 12, -100 }, { 34, -100 } }));
    CHECK(recorder.facts[1].flat);
    CHECK_EQ(uint32(recorder.facts[1].op), uint32(7));
    CHECK(SameValues(recorder.facts[1], { { 9, 25 }, { 11, 25 }, { 12, 25 }, { 34, 25 } }));
    CHECK(SameValues(recorder.facts[2], { { 9, 0 }, { 11, 0 }, { 12, 0 }, { 34, 0 } }));
    CHECK(SameValues(recorder.facts[3], { { 9, 0 }, { 11, 0 }, { 12, 0 }, { 34, 0 } }));
}

// The sums count only the listed modifiers of the same kind and operation.
TEST(SpellModMgr_TheOtherKindAndOtherOperationsAreNotSummed)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier pct(50, kBit1, 1);
    FakeModifier otherOp(9, kBit1, 2);
    FakeModifier flat(4, kBit1, 3);
    mgr.Change(pct.Entry(false, SPELLMOD_COST), true, recorder.Sink());
    mgr.Change(otherOp.Entry(true, SPELLMOD_RANGE), true, recorder.Sink());
    mgr.Change(flat.Entry(true, SPELLMOD_COST), true, recorder.Sink());

    REQUIRE(recorder.facts.size() == 3u);
    CHECK(!recorder.facts[0].flat);
    CHECK(SameValues(recorder.facts[0], { { 1, 50 } }));
    CHECK(SameValues(recorder.facts[2], { { 1, 4 } }));
}

// The values follow the bit order of the modifier's own mask, across the 64-bit word and the
// 32-bit one: bits 0-63 of the first, then 64-95 from the second.
TEST(SpellModMgr_TheValuesAreInBitOrderAcrossTheTwoWords)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier wide(-7, ClassFamilyMask(UI64LIT(0x8000000000000001), 0x80000001), 1);
    mgr.Change(wide.Entry(false, SPELLMOD_DURATION), true, recorder.Sink());

    REQUIRE(recorder.facts.size() == 1u);
    CHECK(SameValues(recorder.facts[0], { { 0, -7 }, { 63, -7 }, { 64, -7 }, { 95, -7 } }));
}

// A modifier with no bit in its mask is still reported, with no values.
TEST(SpellModMgr_AnEmptyMaskReportsNoValues)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier none(5, ClassFamilyMask(), 1);
    mgr.Change(none.Entry(true, SPELLMOD_DAMAGE), true, recorder.Sink());

    REQUIRE(recorder.facts.size() == 1u);
    CHECK(recorder.facts[0].values.empty());
}

// The removal takes every entry of the aura, and only those: an aura added twice leaves at once,
// and the others stay listed. The list's order is not observable through either rule (both sum
// integers), so the test pins membership.
TEST(SpellModMgr_TheRemovalTakesEveryEntryOfTheAura)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder recorder;
    FakeModifier twice(5, kBit1, 1);
    FakeModifier first(100, kBit1, 2);
    FakeModifier last(1000, kBit1, 3);
    mgr.Change(first.Entry(true, SPELLMOD_DAMAGE), true, recorder.Sink());
    mgr.Change(twice.Entry(true, SPELLMOD_DAMAGE), true, recorder.Sink());
    mgr.Change(twice.Entry(true, SPELLMOD_DAMAGE), true, recorder.Sink());
    mgr.Change(last.Entry(true, SPELLMOD_DAMAGE), true, recorder.Sink());

    int32 all = 0;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DAMAGE, all);
    CHECK_EQ(all, int32(1110));

    mgr.Change(twice.Entry(true, SPELLMOD_DAMAGE), false, recorder.Sink());
    REQUIRE(recorder.facts.size() == 5u);
    CHECK(SameValues(recorder.facts[4], { { 1, 1105 } }));

    int32 rest = 0;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DAMAGE, rest);
    CHECK_EQ(rest, int32(1100));

    mgr.Change(first.Entry(true, SPELLMOD_DAMAGE), false, recorder.Sink());
    int32 lastOnly = 0;
    mgr.ApplySpellMod(kSpellMage, SPELLMOD_DAMAGE, lastOnly);
    CHECK_EQ(lastOnly, int32(1000));
}

// Each change reports once, to the sink of that call.
TEST(SpellModMgr_EachChangeReportsOnce)
{
    SeedStores();
    SpellModMgr mgr;
    Recorder first;
    Recorder second;
    FakeModifier mod(5, kBit1, 1);
    mgr.Change(mod.Entry(true, SPELLMOD_DAMAGE), true, first.Sink());
    mgr.Change(mod.Entry(true, SPELLMOD_DAMAGE), false, second.Sink());

    CHECK_EQ(first.facts.size(), size_t(1));
    CHECK_EQ(second.facts.size(), size_t(1));
}

// A unit that is not a player holds no spell modifiers, and a pet or a totem whose owner the
// lookup does not find (none named, a player out of the registry, a creature out of any world)
// answers none, asked again at each call.
TEST(SpellModMgr_ACreatureAPetAndATotemWithNoOwnerAnswerNone)
{
    CreatureProbe creature;
    PetProbe pet;
    TotemProbe totem;

    CHECK(creature.m_spellMods == NULL);
    CHECK(pet.m_spellMods == NULL);
    CHECK(totem.m_spellMods == NULL);
    CHECK(creature.GetSpellMods() == NULL);
    CHECK(pet.GetSpellMods() == NULL);
    CHECK(totem.GetSpellMods() == NULL);

    pet.SetOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, uint32(94101)));
    totem.SetOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, uint32(94101)));
    CHECK(pet.GetSpellMods() == NULL);
    CHECK(totem.GetSpellMods() == NULL);

    pet.SetOwnerGuid(ObjectGuid(HIGHGUID_UNIT, uint32(94101), uint32(94101)));
    CHECK(pet.GetSpellMods() == NULL);
}

TEST(SpellModMgr_EmptySinkAsserts)
{
    if (!AbortCaseSelected("SpellModMgr_EmptySinkAsserts"))
    {
        return;
    }
    SeedStores();
    SpellModMgr mgr;
    FakeModifier mod(5, kBit1, 1);
    mgr.Change(mod.Entry(true, SPELLMOD_DAMAGE), true, SpellModChangedSink());
    testing::ReportFailure(__FILE__, __LINE__, "the empty spell modifier sink passed the assertion");
}
