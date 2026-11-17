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

/// The Fire talents' spell_proc_event rows against the proc gate, with the spells their 4.3.4 texts name and the ones
/// they leave out. A row is built by hand with the values the world database holds for it. The gate is the two checks
/// a proc event meets before an aura's effect handles it: SpellMgr::IsSpellProcEventCanTriggeredBy (the proc flags,
/// the school, the family and the kind of hit) and, for an effect whose row names a family mask, the proc spell's
/// class mask against that mask (SpellEntry::IsFitToFamilyMask, as Unit::ProcDamageAndSpellFor tests it). The proc
/// flags are the spell's own from Spell.dbc where the row holds none, as Unit::IsTriggeredAtSpellProcEvent reads
/// them.
///
/// The proc spells are Spell.dbc rows with their 4.3.4 school and the class mask of their SpellClassOptions.dbc row,
/// built as SpellHitResultTest.cpp builds a row and seeded as SpellModMgrTest.cpp seeds class options; they are not
/// put in the spell store. The class options ids below are used by no other test in this binary.

#include "TestHarness.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "spells/data/SpellMgr.h"

#include <cstddef>

namespace
{
    /// The proc flag every row below answers: a harmful spell of the aura's owner landed (Spell.dbc's 0x10000 for
    /// 44445, 11095, 29074, 64343 and 79683).
    uint32 const kHarmfulSpellLanded = PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT;

    /// One proc spell: its id, its school mask and its class family and mask, as the 4.3.4 client data holds them.
    struct ProcSpell
    {
        uint32 id;
        uint32 schoolMask;
        uint32 family;
        uint64 maskAB;
        uint32 maskC;
    };

    ProcSpell const kFireball = { 133, SPELL_SCHOOL_MASK_FIRE, SPELLFAMILY_MAGE, UI64LIT(0x0000000000000001), 0x00000008 };
    ProcSpell const kFireBlast = { 2136, SPELL_SCHOOL_MASK_FIRE, SPELLFAMILY_MAGE, UI64LIT(0x0000000000000002), 0 };
    ProcSpell const kScorch = { 2948, SPELL_SCHOOL_MASK_FIRE, SPELLFAMILY_MAGE, UI64LIT(0x0000000000000010), 0 };
    ProcSpell const kPyroblast = { 11366, SPELL_SCHOOL_MASK_FIRE, SPELLFAMILY_MAGE, UI64LIT(0x0000000000400000), 0x00001008 };
    ProcSpell const kFrostfireBolt = { 44614, SPELL_SCHOOL_MASK_FIRE | SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE,
                                       UI64LIT(0x0000100000000000), 0x00000008 };
    ProcSpell const kPyroblastInstant = { 92315, SPELL_SCHOOL_MASK_FIRE, SPELLFAMILY_MAGE, UI64LIT(0x0000000000400000), 0x00000008 };
    ProcSpell const kArcaneBlast = { 30451, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0000000020000000), 0 };
    ProcSpell const kArcaneMissiles = { 5143, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0000000000000800), 0 };
    ProcSpell const kArcaneMissile = { 7268, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0000000000200000), 0 };
    ProcSpell const kArcaneBarrage = { 44425, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0000800000000000), 0 };
    ProcSpell const kStarfire = { 2912, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_DRUID, UI64LIT(0x0000000000000004), 0 };
    ProcSpell const kPolymorph = { 118, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0000000001000000), 0 };

    ProcSpell const kSpells[] = { kFireball, kFireBlast, kScorch, kPyroblast, kFrostfireBolt, kPyroblastInstant, kArcaneBlast,
                                  kArcaneMissiles, kArcaneMissile, kArcaneBarrage, kStarfire, kPolymorph };
    std::size_t const kSpellCount = sizeof(kSpells) / sizeof(kSpells[0]);
    uint32 const kFirstOptionsId = 95801;

    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    SpellRowStorage s_rows[kSpellCount] = {};
    SpellClassOptionsEntry s_options[kSpellCount];

    /// The Spell.dbc row of `spell`, with its class options seeded on first use.
    SpellEntry const* Row(ProcSpell const& spell)
    {
        static bool seeded = false;
        if (!seeded)
        {
            seeded = true;
            for (std::size_t i = 0; i < kSpellCount; ++i)
            {
                s_options[i].SpellClassMask = ClassFamilyMask(kSpells[i].maskAB, kSpells[i].maskC);
                s_options[i].SpellClassSet = kSpells[i].family;
                sSpellClassOptionsStore.SetEntry(kFirstOptionsId + uint32(i), &s_options[i]);

                SpellEntry* row = reinterpret_cast<SpellEntry*>(s_rows[i].bytes);
                row->ID = kSpells[i].id;
                row->SchoolMask = kSpells[i].schoolMask;
                row->ClassOptionsID = kFirstOptionsId + uint32(i);
            }
        }
        for (std::size_t i = 0; i < kSpellCount; ++i)
        {
            if (kSpells[i].id == spell.id)
            {
                return reinterpret_cast<SpellEntry const*>(s_rows[i].bytes);
            }
        }
        return NULL;
    }

    /// A spell_proc_event row: school mask, family, the same mask for each of the three effects, proc flags, procEx.
    SpellProcEventEntry ProcRow(uint32 schoolMask, uint32 family, uint64 maskAB, uint32 procFlags, uint32 procEx)
    {
        SpellProcEventEntry row;
        row.schoolMask = schoolMask;
        row.spellFamilyName = family;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            row.spellFamilyMask[i] = ClassFamilyMask(maskAB);
        }
        row.procFlags = procFlags;
        row.procEx = procEx;
        row.ppmRate = 0.0f;
        row.customChance = 0.0f;
        row.cooldown = 0;
        return row;
    }

    // The rows of the world database, column for column.
    SpellProcEventEntry const kHotStreak = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000100000400013), 0,
                                                   PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT);
    SpellProcEventEntry const kCriticalMass = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000000000400010), 0, PROC_EX_NONE);
    SpellProcEventEntry const kMasterOfElements = ProcRow(SPELL_SCHOOL_MASK_MAGIC, SPELLFAMILY_MAGE, 0, 0, PROC_EX_CRITICAL_HIT);
    SpellProcEventEntry const kImpact = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000000000000002), 0,
                                                PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT);
    SpellProcEventEntry const kArcaneMissilesProc = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000000000200800), 0,
                                                            PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT);
    /// Offensive State 79684 and Impact 11103: family 3, no mask, a hit or a crit with or without damage.
    SpellProcEventEntry const kOffensiveState = ProcRow(0, SPELLFAMILY_MAGE, 0, 0,
                                                        PROC_EX_HIT_WITHOUT_DAMAGE | PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT);

    /// The proc flags the aura answers: the row's, or Spell.dbc's when the row holds none.
    uint32 EventFlags(SpellProcEventEntry const* row, uint32 spellProcFlags)
    {
        return row && row->procFlags ? row->procFlags : spellProcFlags;
    }

    /// SpellMgr::IsSpellProcEventCanTriggeredBy for an aura whose Spell.dbc proc flags are kHarmfulSpellLanded, at a
    /// landed harmful spell with the hit kind `procEx`.
    bool PassesGate(SpellProcEventEntry const& row, ProcSpell const& spell, uint32 procEx)
    {
        return SpellMgr::IsSpellProcEventCanTriggeredBy(&row, EventFlags(&row, kHarmfulSpellLanded), Row(spell), kHarmfulSpellLanded, procEx);
    }

    /// The proc spell's class mask against the row's family mask of each of the three effects; false when they
    /// disagree between effects.
    bool FitsMask(SpellProcEventEntry const& row, ProcSpell const& spell)
    {
        bool const fits = Row(spell)->IsFitToFamilyMask(row.spellFamilyMask[0]);
        for (int i = 1; i < MAX_EFFECT_INDEX; ++i)
        {
            if (Row(spell)->IsFitToFamilyMask(row.spellFamilyMask[i]) != fits)
            {
                return false;
            }
        }
        return fits;
    }
}

TEST(SpellProcEventRows_HotStreakTakesTheCritsAndHitsOfItsFiveSpellsAndNotTheEndOfACast)
{
    ProcSpell const named[] = { kFireball, kFireBlast, kScorch, kPyroblast, kFrostfireBolt, kPyroblastInstant };
    for (ProcSpell const& spell : named)
    {
        CHECK(FitsMask(kHotStreak, spell));
        CHECK(PassesGate(kHotStreak, spell, PROC_EX_CRITICAL_HIT));
        CHECK(PassesGate(kHotStreak, spell, PROC_EX_NORMAL_HIT));
        CHECK(!PassesGate(kHotStreak, spell, PROC_EX_CAST_END));
    }
    CHECK(!FitsMask(kHotStreak, kArcaneBlast));
    CHECK(!FitsMask(kHotStreak, kArcaneMissile));
}

TEST(SpellProcEventRows_CriticalMassTakesPyroblastAndScorch)
{
    CHECK(FitsMask(kCriticalMass, kPyroblast));
    CHECK(FitsMask(kCriticalMass, kScorch));
    CHECK(FitsMask(kCriticalMass, kPyroblastInstant));
    CHECK(!FitsMask(kCriticalMass, kFireball));
    CHECK(!FitsMask(kCriticalMass, kFireBlast));
    CHECK(PassesGate(kCriticalMass, kPyroblast, PROC_EX_NORMAL_HIT));
    CHECK(PassesGate(kCriticalMass, kPyroblast, PROC_EX_CRITICAL_HIT));
}

TEST(SpellProcEventRows_MasterOfElementsTakesTheCritsOfEveryMagicSchoolOfTheMage)
{
    CHECK_EQ(uint32(SPELL_SCHOOL_MASK_MAGIC), uint32(126));
    CHECK(PassesGate(kMasterOfElements, kArcaneBlast, PROC_EX_CRITICAL_HIT));
    CHECK(PassesGate(kMasterOfElements, kFireball, PROC_EX_CRITICAL_HIT));
    CHECK(PassesGate(kMasterOfElements, kFrostfireBolt, PROC_EX_CRITICAL_HIT));
    CHECK(!PassesGate(kMasterOfElements, kFireball, PROC_EX_NORMAL_HIT));
    CHECK(!PassesGate(kMasterOfElements, kArcaneBlast, PROC_EX_CAST_END));
    CHECK(!PassesGate(kMasterOfElements, kStarfire, PROC_EX_CRITICAL_HIT));
}

TEST(SpellProcEventRows_ImpactsChargeTakesFireBlastAlone)
{
    CHECK(FitsMask(kImpact, kFireBlast));
    CHECK(PassesGate(kImpact, kFireBlast, PROC_EX_NORMAL_HIT));
    CHECK(PassesGate(kImpact, kFireBlast, PROC_EX_CRITICAL_HIT));
    CHECK(!PassesGate(kImpact, kFireBlast, PROC_EX_CAST_END));
    CHECK(!FitsMask(kImpact, kFireball));
    CHECK(!FitsMask(kImpact, kPyroblast));
    CHECK(!FitsMask(kImpact, kScorch));
}

TEST(SpellProcEventRows_ArcaneMissilesProcIsUsedUpByArcaneMissilesAlone)
{
    CHECK(FitsMask(kArcaneMissilesProc, kArcaneMissile));
    CHECK(FitsMask(kArcaneMissilesProc, kArcaneMissiles));
    CHECK(PassesGate(kArcaneMissilesProc, kArcaneMissile, PROC_EX_NORMAL_HIT));
    CHECK(PassesGate(kArcaneMissilesProc, kArcaneMissile, PROC_EX_CRITICAL_HIT));
    CHECK(!FitsMask(kArcaneMissilesProc, kFireball));
    CHECK(!FitsMask(kArcaneMissilesProc, kArcaneBlast));
    CHECK(!FitsMask(kArcaneMissilesProc, kArcaneBarrage));
}

TEST(SpellProcEventRows_HotStreaksBuffWithNoRowTakesNoProcEvent)
{
    // 48108: no row, and no proc flags in Spell.dbc.
    CHECK(!SpellMgr::IsSpellProcEventCanTriggeredBy(NULL, EventFlags(NULL, 0), Row(kPyroblastInstant), kHarmfulSpellLanded,
                                                    PROC_EX_CRITICAL_HIT));
    CHECK(!SpellMgr::IsSpellProcEventCanTriggeredBy(NULL, EventFlags(NULL, 0), Row(kPyroblastInstant), kHarmfulSpellLanded,
                                                    PROC_EX_CAST_END));
}

TEST(SpellProcEventRows_MoltenArmorWithNoRowTakesTheSwingsThatHitAndNotTheOnesThatMissOrDodge)
{
    // 30482: Spell.dbc proc flags 0x222a8 (including a melee hit taken), and no row.
    uint32 const kMoltenArmorFlags = 0x222a8;
    CHECK(!SpellMgr::IsSpellProcEventCanTriggeredBy(NULL, kMoltenArmorFlags, NULL, PROC_FLAG_TAKEN_MELEE_HIT, PROC_EX_DODGE));
    CHECK(!SpellMgr::IsSpellProcEventCanTriggeredBy(NULL, kMoltenArmorFlags, NULL, PROC_FLAG_TAKEN_MELEE_HIT, PROC_EX_MISS));
    CHECK(SpellMgr::IsSpellProcEventCanTriggeredBy(NULL, kMoltenArmorFlags, NULL, PROC_FLAG_TAKEN_MELEE_HIT, PROC_EX_NORMAL_HIT));
}

TEST(SpellProcEventRows_HitKindAcceptedKeepsTodaysAnswersForRowsWithoutTheBit)
{
    // No extra requirement: a hit or a crit that dealt damage or healed, or a cast end.
    CHECK(!SpellMgr::HitKindAccepted(PROC_EX_NONE, PROC_EX_NORMAL_HIT, false));
    CHECK(SpellMgr::HitKindAccepted(PROC_EX_NONE, PROC_EX_NORMAL_HIT, true));
    CHECK(SpellMgr::HitKindAccepted(PROC_EX_NONE, PROC_EX_CRITICAL_HIT, true));
    CHECK(SpellMgr::HitKindAccepted(PROC_EX_NONE, PROC_EX_CAST_END, false));
    CHECK(!SpellMgr::HitKindAccepted(PROC_EX_NONE, PROC_EX_MISS, true));

    // An extra requirement without the bit: a hit it asks for must have dealt damage or healed.
    uint32 const hits = PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT;
    CHECK(!SpellMgr::HitKindAccepted(hits, PROC_EX_NORMAL_HIT, false));
    CHECK(SpellMgr::HitKindAccepted(hits, PROC_EX_NORMAL_HIT, true));
    CHECK(SpellMgr::HitKindAccepted(PROC_EX_CRITICAL_HIT, PROC_EX_NORMAL_HIT, false));
    CHECK(SpellMgr::HitKindAccepted(PROC_EX_RESIST, PROC_EX_RESIST, false));
}

TEST(SpellProcEventRows_HitKindAcceptedTakesAHitWithoutDamageWhenTheRowHasTheBit)
{
    uint32 const row = PROC_EX_HIT_WITHOUT_DAMAGE | PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT;
    CHECK_EQ(row, uint32(0x4003));
    CHECK(SpellMgr::HitKindAccepted(row, PROC_EX_NORMAL_HIT, false));
    CHECK(SpellMgr::HitKindAccepted(row, PROC_EX_NORMAL_HIT, true));
    CHECK(SpellMgr::HitKindAccepted(row, PROC_EX_CRITICAL_HIT, false));
    CHECK(!SpellMgr::HitKindAccepted(row, PROC_EX_MISS, false));
    CHECK(!SpellMgr::HitKindAccepted(row, PROC_EX_RESIST, false));
    CHECK(!SpellMgr::HitKindAccepted(row, PROC_EX_IMMUNE, false));
    CHECK(!SpellMgr::HitKindAccepted(row, PROC_EX_REFLECT, false));
}

TEST(SpellProcEventRows_OffensiveStateTakesEveryMageHitWithOrWithoutDamageAndNoMiss)
{
    CHECK(PassesGate(kOffensiveState, kPolymorph, PROC_EX_NORMAL_HIT));
    CHECK(PassesGate(kOffensiveState, kFireball, PROC_EX_NORMAL_HIT));
    CHECK(PassesGate(kOffensiveState, kFireball, PROC_EX_CRITICAL_HIT));
    CHECK(!PassesGate(kOffensiveState, kPolymorph, PROC_EX_MISS));
    CHECK(!PassesGate(kOffensiveState, kPolymorph, PROC_EX_RESIST));
    CHECK(!PassesGate(kOffensiveState, kPolymorph, PROC_EX_IMMUNE));
    CHECK(!PassesGate(kOffensiveState, kPolymorph, PROC_EX_REFLECT));
    CHECK(!PassesGate(kOffensiveState, kFireball, PROC_EX_CAST_END));
    CHECK(!PassesGate(kOffensiveState, kStarfire, PROC_EX_NORMAL_HIT));
}
