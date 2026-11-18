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

/// The Fire, Frost and Arcane talents' spell_proc_event rows against the proc gate, with the spells their 4.3.4 texts
/// name and the ones they leave out; and the interrupt mark a hit's proc event carries when its interrupt effect
/// stopped a cast, against Improved Counterspell's row, an aura with no row and a row that asks for the mark. A row is
/// built by hand with the values the world database holds for it. The gate is the two checks a proc event meets
/// before an aura's effect handles it: SpellMgr::IsSpellProcEventCanTriggeredBy (the proc flags, the school, the
/// family and the kind of hit) and, for an effect whose row names a family mask, the proc spell's class mask against
/// that mask (SpellEntry::IsFitToFamilyMask, as Unit::ProcDamageAndSpellFor tests it). The proc flags are the
/// spell's own from Spell.dbc where the row holds none, as Unit::IsTriggeredAtSpellProcEvent reads them.
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
    /// 44445, 11095, 29074, 64343, 79683, 44543, 44546, 44561 and 11175).
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
    ProcSpell const kFrostbolt = { 116, SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, UI64LIT(0x0000000000000020), 0 };
    ProcSpell const kConeOfCold = { 120, SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, UI64LIT(0x0000000000000200), 0 };
    ProcSpell const kBlizzardChill = { 12484, SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, UI64LIT(0x0000000000100000),
                                       0 };
    ProcSpell const kFrostArmorChill = { 7321, SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, UI64LIT(0x0000000000100000),
                                         0 };
    ProcSpell const kOrbHitRankTwo = { 84721, SPELL_SCHOOL_MASK_FIRE | SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, 0,
                                       0x00004080 };
    ProcSpell const kOrbSummon = { 84714, SPELL_SCHOOL_MASK_FIRE | SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, 0,
                                   0x00000080 };
    ProcSpell const kIceLance = { 30455, SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, UI64LIT(0x0000000000020000), 0 };
    ProcSpell const kFrostNova = { 122, SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, UI64LIT(0x0000000000000040), 0 };
    ProcSpell const kWaterbolt = { 31707, SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, 0, 0 };
    ProcSpell const kBlizzardTick = { 42208, SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, UI64LIT(0x0000000000080080),
                                      0 };
    ProcSpell const kArcaneExplosion = { 1449, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0000000000001000), 0 };
    ProcSpell const kArcaneBrilliance = { 1459, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0000000000000400),
                                          0x00000008 };
    ProcSpell const kMageArmor = { 6117, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0000000010000000), 0 };
    ProcSpell const kManaShield = { 1463, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0000000000008000),
                                    0x00000008 };
    ProcSpell const kBlink = { 1953, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0000000000010000), 0 };
    ProcSpell const kClearcasting = { 12536, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0000000200000000),
                                      0x00000008 };
    ProcSpell const kPresenceOfMind = { 12043, SPELL_SCHOOL_MASK_NORMAL, SPELLFAMILY_MAGE, UI64LIT(0x0000002000000000),
                                        0x00000008 };
    ProcSpell const kArcanePower = { 12042, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0008000000000000),
                                     0x00000008 };
    ProcSpell const kLightningBolt = { 403, SPELL_SCHOOL_MASK_NATURE, SPELLFAMILY_SHAMAN, UI64LIT(0x0000000000000001), 0 };

    ProcSpell const kCounterspell = { 2139, SPELL_SCHOOL_MASK_ARCANE, SPELLFAMILY_MAGE, UI64LIT(0x0000000000004000),
                                      0 };

    ProcSpell const kSpells[] = { kFireball, kFireBlast, kScorch, kPyroblast, kFrostfireBolt, kPyroblastInstant, kArcaneBlast,
                                  kArcaneMissiles, kArcaneMissile, kArcaneBarrage, kStarfire, kPolymorph, kFrostbolt,
                                  kConeOfCold, kBlizzardChill, kFrostArmorChill, kOrbHitRankTwo, kOrbSummon, kIceLance,
                                  kFrostNova, kWaterbolt, kBlizzardTick, kCounterspell, kArcaneExplosion,
                                  kArcaneBrilliance, kMageArmor, kManaShield, kBlink, kClearcasting, kPresenceOfMind,
                                  kArcanePower, kLightningBolt };
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

    /// A spell_proc_event row: school mask, family, the same mask for each of the three effects (its A and B words,
    /// then its C word), proc flags, procEx.
    SpellProcEventEntry ProcRow(uint32 schoolMask, uint32 family, uint64 maskAB, uint32 procFlags, uint32 procEx,
                                uint32 maskC = 0)
    {
        SpellProcEventEntry row;
        row.schoolMask = schoolMask;
        row.spellFamilyName = family;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            row.spellFamilyMask[i] = ClassFamilyMask(maskAB, maskC);
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
    /// Arcane Missiles! 79683: the Arcane Missiles channel (A 0x800), a hit or a crit.
    SpellProcEventEntry const kArcaneMissilesProc = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000000000000800), 0,
                                                            PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT);
    /// Improved Counterspell 11255: family 3, Counterspell's class mask, no extra requirement.
    SpellProcEventEntry const kImprovedCounterspell = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000000000004000), 0,
                                                              PROC_EX_NONE);
    /// Offensive State 79684 and Impact 11103: family 3, no mask, a hit or a crit with or without damage.
    SpellProcEventEntry const kOffensiveState = ProcRow(0, SPELLFAMILY_MAGE, 0, 0,
                                                        PROC_EX_HIT_WITHOUT_DAMAGE | PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT);
    /// Fingers of Frost 44543 and Brain Freeze 44546: family 3, A 0x100220 (Frostbolt, Cone of Cold, Chilled), B 0x1000
    /// (Frostfire Bolt, Fingers of Frost only), C 0x4080 (the Frostfire Orb's hits), the end of a cast.
    SpellProcEventEntry const kFingersOfFrost = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000100000100220), 0,
                                                        PROC_EX_CAST_END, 0x00004080);
    SpellProcEventEntry const kBrainFreeze = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000000000100220), 0,
                                                     PROC_EX_CAST_END, 0x00004080);
    /// Enduring Winter 44561: Frostbolt's hit. Permafrost 11175: the hits of the spells its dummy's mask names.
    SpellProcEventEntry const kEnduringWinter = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000000000000020), 0,
                                                        PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT);
    SpellProcEventEntry const kPermafrost = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000100000100220), 0,
                                                    PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT);

    /// `row` with a cooldown of `seconds` and a custom chance of `chance`.
    SpellProcEventEntry WithCooldownAndChance(SpellProcEventEntry row, uint32 seconds, float chance)
    {
        row.cooldown = seconds;
        row.customChance = chance;
        return row;
    }

    /// `row` with no family mask on effects 1 and 2.
    SpellProcEventEntry OnEffectZeroOnly(SpellProcEventEntry row)
    {
        row.spellFamilyMask[1] = ClassFamilyMask();
        row.spellFamilyMask[2] = ClassFamilyMask();
        return row;
    }

    /// Arcane Concentration 11213: family 3, no mask, a hit or a crit, a cooldown of 15 seconds.
    SpellProcEventEntry const kArcaneConcentration = WithCooldownAndChance(
        ProcRow(0, SPELLFAMILY_MAGE, 0, 0, PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT), 15, 0.0f);
    /// Arcane Potency 31571: family 3, effect 0's B 0x22 (Clearcasting, Presence of Mind), the helpful-spell flag, no
    /// hit kind, a chance of 100.
    SpellProcEventEntry const kArcanePotency = WithCooldownAndChance(
        OnEffectZeroOnly(ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000002200000000), PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL,
                                 PROC_EX_NONE)), 0, 100.0f);
    /// Improved Blink 31569: family 3, Blink (A 0x10000) on every effect, no hit kind.
    SpellProcEventEntry const kImprovedBlink = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000000000010000), 0, PROC_EX_NONE);


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

    /// SpellMgr::IsSpellProcEventCanTriggeredBy for an aura whose Spell.dbc proc flags are `spellProcFlags`, at an
    /// event of the flag `procFlag` and the hit kind `procEx` of `spell`.
    bool Gate(SpellProcEventEntry const& row, uint32 spellProcFlags, ProcSpell const& spell, uint32 procFlag, uint32 procEx)
    {
        return SpellMgr::IsSpellProcEventCanTriggeredBy(&row, EventFlags(&row, spellProcFlags), Row(spell), procFlag, procEx);
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

    /// Whether the row takes `spell` at the hit kind `procEx`: its mask and the gate both.
    bool Takes(SpellProcEventEntry const& row, ProcSpell const& spell, uint32 procEx)
    {
        return FitsMask(row, spell) && PassesGate(row, spell, procEx);
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

TEST(SpellProcEventRows_ArcaneMissilesProcIsUsedUpByTheChannelAndNotByAMissile)
{
    CHECK(FitsMask(kArcaneMissilesProc, kArcaneMissiles));
    CHECK(PassesGate(kArcaneMissilesProc, kArcaneMissiles, PROC_EX_NORMAL_HIT));
    CHECK(PassesGate(kArcaneMissilesProc, kArcaneMissiles, PROC_EX_CRITICAL_HIT));
    CHECK(!FitsMask(kArcaneMissilesProc, kArcaneMissile));
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

TEST(SpellProcEventRows_HitKindAcceptedForRowsWithoutTheBitRefusesAMissAndAnAskedForHitThatDealtNothing)
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

TEST(SpellProcEventRows_OffensiveStatesRowPassesEveryMageHitAndNoMissThroughTheGate)
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

TEST(SpellProcEventRows_FingersOfFrostRollsAtTheCastOfItsChillsAndNeverAtTheirHits)
{
    ProcSpell const chills[] = { kFrostbolt, kConeOfCold, kBlizzardChill, kFrostfireBolt, kOrbHitRankTwo };
    for (ProcSpell const& spell : chills)
    {
        CHECK(Takes(kFingersOfFrost, spell, PROC_EX_CAST_END));
        CHECK(!Takes(kFingersOfFrost, spell, PROC_EX_NORMAL_HIT));
        CHECK(!Takes(kFingersOfFrost, spell, PROC_EX_CRITICAL_HIT));
    }
    for (ProcSpell const& spell : { kFireBlast, kIceLance, kFrostNova, kWaterbolt, kArcaneBlast })
    {
        CHECK(!Takes(kFingersOfFrost, spell, PROC_EX_CAST_END));
    }

    // Frost Armor's Chilled shares Blizzard's Chilled's bit, so the row takes it; Fingers of Frost's script refuses it
    // by id (MageScriptsTest).
    CHECK(Takes(kFingersOfFrost, kFrostArmorChill, PROC_EX_CAST_END));

    // The Frostfire Orb's summon shares the orb hit's C bit 0x80; its cast is helpful, so its cast end carries the
    // helpful-spell flag, which the talent's proc flags do not hold.
    CHECK(FitsMask(kFingersOfFrost, kOrbSummon));
    uint32 const talentFlags = EventFlags(&kFingersOfFrost, kHarmfulSpellLanded);
    CHECK(!SpellMgr::IsSpellProcEventCanTriggeredBy(&kFingersOfFrost, talentFlags, Row(kOrbSummon),
                                                    PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_CAST_END));
    CHECK(SpellMgr::IsSpellProcEventCanTriggeredBy(&kFingersOfFrost, talentFlags, Row(kOrbSummon), kHarmfulSpellLanded,
                                                   PROC_EX_CAST_END));
}

TEST(SpellProcEventRows_BrainFreezeRollsAtTheCastOfTheSameChillsLessFrostfireBolt)
{
    ProcSpell const chills[] = { kFrostbolt, kConeOfCold, kBlizzardChill, kOrbHitRankTwo };
    for (ProcSpell const& spell : chills)
    {
        CHECK(Takes(kBrainFreeze, spell, PROC_EX_CAST_END));
        CHECK(!Takes(kBrainFreeze, spell, PROC_EX_NORMAL_HIT));
    }
    for (ProcSpell const& spell : { kFrostfireBolt, kFireBlast, kIceLance, kFrostNova, kWaterbolt, kArcaneBlast })
    {
        CHECK(!Takes(kBrainFreeze, spell, PROC_EX_CAST_END));
    }
}

TEST(SpellProcEventRows_EnduringWinterTakesFrostboltsHitAndPermafrostTheHitsOfItsChills)
{
    CHECK(Takes(kEnduringWinter, kFrostbolt, PROC_EX_NORMAL_HIT));
    CHECK(Takes(kEnduringWinter, kFrostbolt, PROC_EX_CRITICAL_HIT));
    CHECK(!Takes(kEnduringWinter, kFrostbolt, PROC_EX_CAST_END));
    CHECK(!Takes(kEnduringWinter, kConeOfCold, PROC_EX_NORMAL_HIT));
    CHECK(!Takes(kEnduringWinter, kIceLance, PROC_EX_NORMAL_HIT));

    for (ProcSpell const& spell : { kFrostbolt, kConeOfCold, kFrostfireBolt })
    {
        CHECK(Takes(kPermafrost, spell, PROC_EX_NORMAL_HIT));
        CHECK(!Takes(kPermafrost, spell, PROC_EX_CAST_END));
    }
    CHECK(!Takes(kPermafrost, kBlizzardTick, PROC_EX_NORMAL_HIT));
    CHECK(!Takes(kPermafrost, kArcaneBlast, PROC_EX_NORMAL_HIT));
}

TEST(SpellProcEventRows_TheInterruptMarkChangesNothingForARowThatDoesNotAskForIt)
{
    // A Counterspell hit that interrupted a cast carries PROC_EX_INTERRUPT beside its hit kind; Improved Counterspell's
    // row, which asks for no hit kind, takes it as it takes one that interrupted nothing.
    uint32 const interrupted = PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT;
    CHECK(PassesGate(kImprovedCounterspell, kCounterspell, PROC_EX_NORMAL_HIT));
    CHECK(PassesGate(kImprovedCounterspell, kCounterspell, interrupted));
    CHECK(FitsMask(kImprovedCounterspell, kCounterspell));
    CHECK(!FitsMask(kImprovedCounterspell, kFireball));

    // An aura with no row (Invocation 84722) passes the gate with either, and a row that asks for hits with either.
    CHECK(SpellMgr::IsSpellProcEventCanTriggeredBy(NULL, kHarmfulSpellLanded, Row(kCounterspell), kHarmfulSpellLanded,
                                                   PROC_EX_NORMAL_HIT));
    CHECK(SpellMgr::IsSpellProcEventCanTriggeredBy(NULL, kHarmfulSpellLanded, Row(kCounterspell), kHarmfulSpellLanded,
                                                   interrupted));
    CHECK(PassesGate(kImpact, kFireBlast, PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT));

    // The hit-kind check of an effect with no class mask reads the mark as no hit kind: a hit that dealt nothing is
    // refused with it as without it.
    for (bool active : { false, true })
    {
        CHECK_EQ(SpellMgr::HitKindAccepted(PROC_EX_NONE, interrupted, active),
                 SpellMgr::HitKindAccepted(PROC_EX_NONE, PROC_EX_NORMAL_HIT, active));
        uint32 const hits = PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT;
        CHECK_EQ(SpellMgr::HitKindAccepted(hits, interrupted, active),
                 SpellMgr::HitKindAccepted(hits, PROC_EX_NORMAL_HIT, active));
    }
}

TEST(SpellProcEventRows_ARowThatAsksForTheInterruptMarkTakesOnlyAHitThatInterruptedACast)
{
    // The row another class's interrupt talent can hold: its spell's class mask and PROC_EX_INTERRUPT.
    SpellProcEventEntry const interruptRow =
        ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000000000004000), 0, PROC_EX_INTERRUPT);
    CHECK(PassesGate(interruptRow, kCounterspell, PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT));
    CHECK(!PassesGate(interruptRow, kCounterspell, PROC_EX_NORMAL_HIT));
    CHECK(!PassesGate(interruptRow, kCounterspell, PROC_EX_CAST_END));
    CHECK(!PassesGate(interruptRow, kCounterspell, PROC_EX_MISS));
    CHECK(SpellMgr::InterruptedACast(PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT));
    CHECK(!SpellMgr::InterruptedACast(PROC_EX_NORMAL_HIT));
}

TEST(SpellProcEventRows_ArcaneConcentrationTakesTheHitsOfMageSpellsAndHoldsAFifteenSecondCooldown)
{
    // 11213: Spell.dbc proc flags 0x15550, helpful spells among them; the row names no class mask, so each effect then
    // takes the hit kind through SpellMgr::HitKindAccepted with whether the hit dealt damage.
    uint32 const kConcentrationFlags = 0x15550;
    CHECK_EQ(EventFlags(&kArcaneConcentration, kConcentrationFlags), kConcentrationFlags);
    CHECK_EQ(kArcaneConcentration.cooldown, uint32(15));
    CHECK(kArcaneConcentration.customChance == 0.0f);
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        CHECK(!kArcaneConcentration.spellFamilyMask[i]);
    }

    for (ProcSpell const& spell : { kArcaneBlast, kArcaneMissile, kArcaneExplosion, kArcaneBarrage, kFireball, kFrostbolt })
    {
        CHECK(Gate(kArcaneConcentration, kConcentrationFlags, spell, kHarmfulSpellLanded, PROC_EX_NORMAL_HIT));
        CHECK(Gate(kArcaneConcentration, kConcentrationFlags, spell, kHarmfulSpellLanded, PROC_EX_CRITICAL_HIT));
        CHECK(!Gate(kArcaneConcentration, kConcentrationFlags, spell, kHarmfulSpellLanded, PROC_EX_CAST_END));
        CHECK(!Gate(kArcaneConcentration, kConcentrationFlags, spell, kHarmfulSpellLanded, PROC_EX_MISS));
        CHECK(!Gate(kArcaneConcentration, kConcentrationFlags, spell, kHarmfulSpellLanded, PROC_EX_RESIST));
    }

    // Another class's spell is not the row's family.
    CHECK(!Gate(kArcaneConcentration, kConcentrationFlags, kLightningBolt, kHarmfulSpellLanded, PROC_EX_NORMAL_HIT));
    CHECK(!Gate(kArcaneConcentration, kConcentrationFlags, kStarfire, kHarmfulSpellLanded, PROC_EX_CRITICAL_HIT));

    // A helpful mage spell's hit passes the flags and the family; its hit kind is refused for want of damage, as is a
    // harmful hit that dealt none.
    for (ProcSpell const& spell : { kMageArmor, kArcaneBrilliance, kPresenceOfMind, kManaShield })
    {
        CHECK(Gate(kArcaneConcentration, kConcentrationFlags, spell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_NORMAL_HIT));
    }
    CHECK(!SpellMgr::HitKindAccepted(kArcaneConcentration.procEx, PROC_EX_NORMAL_HIT, false));
    CHECK(!SpellMgr::HitKindAccepted(kArcaneConcentration.procEx, PROC_EX_CRITICAL_HIT, false));
    CHECK(SpellMgr::HitKindAccepted(kArcaneConcentration.procEx, PROC_EX_NORMAL_HIT, true));
    CHECK(SpellMgr::HitKindAccepted(kArcaneConcentration.procEx, PROC_EX_CRITICAL_HIT, true));
}

TEST(SpellProcEventRows_ArcanePotencyTakesTheHelpfulHitsOfClearcastingAndPresenceOfMindAtAChanceOf100)
{
    // 31571: Spell.dbc gives it no proc flags and no chance; the row gives the helpful-spell flag and 100. Its effect 0,
    // the talent's only effect, names Clearcasting's B 0x2 and Presence of Mind's B 0x20.
    CHECK_EQ(kArcanePotency.procFlags, uint32(PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL));
    CHECK_EQ(kArcanePotency.procEx, uint32(PROC_EX_NONE));
    CHECK(kArcanePotency.customChance == 100.0f);
    CHECK(!kArcanePotency.spellFamilyMask[1]);
    CHECK(!kArcanePotency.spellFamilyMask[2]);

    for (ProcSpell const& spell : { kClearcasting, kPresenceOfMind })
    {
        CHECK(Row(spell)->IsFitToFamilyMask(kArcanePotency.spellFamilyMask[0]));
        CHECK(Gate(kArcanePotency, 0, spell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_NORMAL_HIT));
        CHECK(!Gate(kArcanePotency, 0, spell, kHarmfulSpellLanded, PROC_EX_NORMAL_HIT));
    }
    for (ProcSpell const& spell : { kArcanePower, kMageArmor, kArcaneBrilliance, kManaShield, kBlink, kArcaneBlast,
                                    kArcaneExplosion, kArcaneMissiles })
    {
        CHECK(!Row(spell)->IsFitToFamilyMask(kArcanePotency.spellFamilyMask[0]));
    }
}

TEST(SpellProcEventRows_ImprovedBlinkTakesBlinkAlone)
{
    // 31569: Spell.dbc proc flags 0x4000, a helpful spell landed; the row names Blink on every effect.
    uint32 const kImprovedBlinkFlags = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL;
    CHECK(FitsMask(kImprovedBlink, kBlink));
    CHECK(Gate(kImprovedBlink, kImprovedBlinkFlags, kBlink, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_NORMAL_HIT));
    for (ProcSpell const& spell : { kArcaneBrilliance, kMageArmor, kManaShield, kPresenceOfMind, kArcanePower,
                                    kClearcasting })
    {
        CHECK(!FitsMask(kImprovedBlink, spell));
    }
    CHECK(!Gate(kImprovedBlink, kImprovedBlinkFlags, kArcaneBlast, kHarmfulSpellLanded, PROC_EX_NORMAL_HIT));
}
