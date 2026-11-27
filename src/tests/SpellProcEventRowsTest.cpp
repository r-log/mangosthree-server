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

/// The Fire, Frost and Arcane talents' spell_proc_event rows, the Discipline priest's (Divine Aegis, Focused Will), the
/// Holy priest's (Surge of Light, Inspiration, Blessed Resilience, Divine Touch) and the Shadow priest's (Harnessed
/// Shadows, Mind Melt, Paralysis) against the proc gate, with the spells their 4.3.4 texts name and the ones they
/// leave out; and the interrupt mark a
/// hit's proc event carries when its interrupt effect
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
    /// 44445, 11095, 29074, 64343, 79683, 79684, 11103, 44543, 44546, 44561 and 11175).
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
    ProcSpell const kBlizzardChillRankTwo = { 12485, SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE,
                                              UI64LIT(0x0000000000100000), 0 };
    /// Piercing Chill's chill: Frostbolt's class bit.
    ProcSpell const kPiercingChill = { 83154, SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, UI64LIT(0x0000000000000020), 0 };
    ProcSpell const kOrbHitRankOne = { 95969, SPELL_SCHOOL_MASK_FIRE | SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, 0,
                                       0x00004080 };
    ProcSpell const kFlameOrbHit = { 82739, SPELL_SCHOOL_MASK_FIRE, SPELLFAMILY_MAGE, 0, 0x00000080 };
    /// Shattered Barrier's freeze, cast around the mage when damage uses up his Ice Barrier.
    ProcSpell const kShatteredBarrierFreeze = { 83073, SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE,
                                                UI64LIT(0x0000000000080000), 0 };
    /// Ring of Frost's freeze, which the ring casts for the mage.
    ProcSpell const kRingFreeze = { 82691, SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_MAGE, 0, 0x00000040 };
    /// The priest's heals: Flash Heal, Prayer of Healing, Renew (whose ticks heal) and Penance's heal (B 0x10000); and
    /// two of his harmful spells, Smite and Shadow Word: Pain.
    ProcSpell const kFlashHeal = { 2061, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST, UI64LIT(0x0000000000000800), 0 };
    ProcSpell const kPrayerOfHealing = { 596, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST, UI64LIT(0x0000000000000200),
                                         0 };
    ProcSpell const kRenew = { 139, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST, UI64LIT(0x0000000000000040), 0 };
    ProcSpell const kPenanceHeal = { 47750, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST, UI64LIT(0x0001000000000000),
                                     0 };
    ProcSpell const kSmite = { 585, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST, UI64LIT(0x0000000000000080), 0 };
    ProcSpell const kShadowWordPain = { 589, SPELL_SCHOOL_MASK_SHADOW, SPELLFAMILY_PRIEST, UI64LIT(0x0000000000008000),
                                        0 };
    /// The priest's spells the Holy rows name or leave out: Heal, Greater Heal, Binding Heal (B 0x4), the instant Flash
    /// Heal Surge of Light gives (101062, Flash Heal's bit), Power Word: Shield, Prayer of Mending and its heal (33076
    /// and 33110, B 0x20), Circle of Healing and Divine Touch's heal (C 0x1000).
    ProcSpell const kHeal = { 2050, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST, UI64LIT(0x0000000000000400), 0 };
    ProcSpell const kGreaterHeal = { 2060, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST, UI64LIT(0x0000000000001000), 0 };
    ProcSpell const kBindingHeal = { 32546, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST, UI64LIT(0x0000000400000000),
                                     0 };
    ProcSpell const kSurgeFlashHeal = { 101062, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST,
                                        UI64LIT(0x0000000000000800), 0 };
    ProcSpell const kPowerWordShield = { 17, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST, UI64LIT(0x0000000000000001),
                                         0 };
    ProcSpell const kPrayerOfMending = { 33076, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST,
                                         UI64LIT(0x0000002000000000), 0 };
    ProcSpell const kPrayerOfMendingHeal = { 33110, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST,
                                             UI64LIT(0x0000002000000000), 0 };
    ProcSpell const kCircleOfHealing = { 34861, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST,
                                         UI64LIT(0x0000000010000000), 0 };
    ProcSpell const kDivineTouchHeal = { 63544, SPELL_SCHOOL_MASK_HOLY, SPELLFAMILY_PRIEST, 0, 0x00001000 };
    /// The Shadow priest's Mind Blast (A 0x2000) and Mind Spike (C 0x10000, a shadow and frost spell).
    ProcSpell const kMindBlast = { 8092, SPELL_SCHOOL_MASK_SHADOW, SPELLFAMILY_PRIEST, UI64LIT(0x0000000000002000), 0 };
    ProcSpell const kMindSpike = { 73510, SPELL_SCHOOL_MASK_SHADOW | SPELL_SCHOOL_MASK_FROST, SPELLFAMILY_PRIEST, 0,
                                   0x00010000 };

    ProcSpell const kSpells[] = { kFireball, kFireBlast, kScorch, kPyroblast, kFrostfireBolt, kPyroblastInstant, kArcaneBlast,
                                  kArcaneMissiles, kArcaneMissile, kArcaneBarrage, kStarfire, kPolymorph, kFrostbolt,
                                  kConeOfCold, kBlizzardChill, kFrostArmorChill, kOrbHitRankTwo, kOrbSummon, kIceLance,
                                  kFrostNova, kWaterbolt, kBlizzardTick, kCounterspell, kArcaneExplosion,
                                  kArcaneBrilliance, kMageArmor, kManaShield, kBlink, kClearcasting, kPresenceOfMind,
                                  kArcanePower, kLightningBolt, kBlizzardChillRankTwo, kPiercingChill, kOrbHitRankOne,
                                  kFlameOrbHit, kShatteredBarrierFreeze, kRingFreeze, kFlashHeal, kPrayerOfHealing,
                                  kRenew, kPenanceHeal, kSmite, kShadowWordPain, kHeal, kGreaterHeal, kBindingHeal,
                                  kSurgeFlashHeal, kPowerWordShield, kPrayerOfMending, kPrayerOfMendingHeal,
                                  kCircleOfHealing, kDivineTouchHeal, kMindBlast, kMindSpike };
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
    /// Offensive State 79684: family 3, no mask, the end of a cast.
    SpellProcEventEntry const kOffensiveState = ProcRow(0, SPELLFAMILY_MAGE, 0, 0, PROC_EX_CAST_END);
    /// Impact 11103: family 3, no mask, a hit or a crit with or without damage.
    SpellProcEventEntry const kImpactTalent =
        ProcRow(0, SPELLFAMILY_MAGE, 0, 0, PROC_EX_HIT_WITHOUT_DAMAGE | PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT);
    /// Fingers of Frost 44543: family 3, A 0x100220 (Frostbolt, Cone of Cold, Chilled), B 0x1000 (Frostfire Bolt), C
    /// 0x4000 (the Frostfire Orb's two hits), a normal or a critical hit. Brain Freeze 44546: the same A and C words,
    /// no B word, the end of a cast.
    SpellProcEventEntry const kFingersOfFrost = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000100000100220), 0,
                                                        PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT, 0x00004000);
    SpellProcEventEntry const kBrainFreeze = ProcRow(0, SPELLFAMILY_MAGE, UI64LIT(0x0000000000100220), 0,
                                                     PROC_EX_CAST_END, 0x00004000);
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
    /// Divine Aegis 47509 (its two higher ranks take its row) and Focused Will 45234 (its rank 2, 45243, takes its
    /// row): every school, no family, no mask, the Spell.dbc proc flags, a normal or a critical hit. The talents'
    /// scripts refuse what makes no shield and what gives no Focused Will (PriestDivineAegisTest).
    SpellProcEventEntry const kDivineAegis = ProcRow(SPELL_SCHOOL_MASK_ALL, 0, 0, 0,
                                                     PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT);
    SpellProcEventEntry const kFocusedWill = ProcRow(SPELL_SCHOOL_MASK_ALL, 0, 0, 0,
                                                     PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT);
    /// Divine Aegis's Spell.dbc proc flags: a helpful spell landed and a periodic effect's tick dealt, at ranks 1 and
    /// 2 (0x44000); rank 3 (47515) adds the helpful area hit (0x44400). Focused Will's: the hits its owner takes, melee
    /// and ranged, of a weapon or a spell, an area spell's and a harmful spell's (0x222a8), and no periodic tick.
    uint32 const kDivineAegisFlags = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL | PROC_FLAG_ON_DO_PERIODIC;
    uint32 const kDivineAegisRank3Flags = kDivineAegisFlags | PROC_FLAG_SUCCESSFUL_POSITIVE_AOE_HIT;
    uint32 const kFocusedWillFlags = 0x222a8;

    /// Surge of Light 88687, a new row (its rank 2, 88690, takes it): family 6, the five spells its text names on every
    /// effect, Smite (A 0x80), Heal (A 0x400), Flash Heal (A 0x800), Greater Heal (A 0x1000) and Binding Heal (B 0x4),
    /// the Spell.dbc proc flags, no hit kind and no custom chance, so the client's 3% and 6% roll.
    SpellProcEventEntry const kSurgeOfLight = ProcRow(0, SPELLFAMILY_PRIEST, UI64LIT(0x0000000400001C80), 0,
                                                      PROC_EX_NONE);
    /// Inspiration 14892 (its rank 2, 15362, takes its row): family 6, Prayer of Healing, Heal, Flash Heal, Greater
    /// Heal and Circle of Healing (A 0x10001E00), Binding Heal, Prayer of Mending's heal and Penance's heal (B
    /// 0x10024), a critical hit; and the row it held before, whose B words (0x10004) left Prayer of Mending's heal out.
    SpellProcEventEntry const kInspiration = ProcRow(0, SPELLFAMILY_PRIEST, UI64LIT(0x0001002410001E00), 0,
                                                     PROC_EX_CRITICAL_HIT);
    SpellProcEventEntry const kInspirationBefore = ProcRow(0, SPELLFAMILY_PRIEST, UI64LIT(0x0001000410001E00), 0,
                                                           PROC_EX_CRITICAL_HIT);
    /// Blessed Resilience 33142 (its rank 2, 33145, takes its row): every school, no family, no mask, the Spell.dbc
    /// proc flags, a normal or a critical hit; and the row it held before, a critical hit alone. Its script refuses the
    /// normal hits not greater than 10% of the priest's health (PriestBlessedResilienceTest).
    SpellProcEventEntry const kBlessedResilience = ProcRow(SPELL_SCHOOL_MASK_ALL, 0, 0, 0,
                                                           PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT);
    SpellProcEventEntry const kBlessedResilienceBefore = ProcRow(SPELL_SCHOOL_MASK_ALL, 0, 0, 0, PROC_EX_CRITICAL_HIT);
    /// Divine Touch 63534 (its rank 2, 63542, takes its row): family 6, Renew (A 0x40) on every effect, a helpful spell
    /// landed (the client gives the talent no proc flags), no hit kind, a chance of 100 (the client gives it none).
    SpellProcEventEntry const kDivineTouch = WithCooldownAndChance(
        ProcRow(0, SPELLFAMILY_PRIEST, UI64LIT(0x0000000000000040), PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_NONE),
        0, 100.0f);
    /// The Spell.dbc proc flags: Surge of Light's, a helpful or a harmful spell landed (0x14000); Inspiration's, the
    /// helpful spell and the helpful area hit among them (0x15550); Blessed Resilience's, Focused Will's hits taken
    /// (0x222a8), no periodic tick.
    uint32 const kSurgeOfLightFlags = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL | PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT;
    uint32 const kInspirationFlags = 0x15550;
    uint32 const kBlessedResilienceFlags = 0x222a8;

    /// `row` with the family mask of effect `effect` alone, `mask`; the other effects hold none.
    SpellProcEventEntry OnEffectOnly(SpellProcEventEntry row, int effect, ClassFamilyMask const& mask)
    {
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            row.spellFamilyMask[i] = i == effect ? mask : ClassFamilyMask();
        }
        return row;
    }

    /// Harnessed Shadows 33191 (its rank 2, 78228, takes its row): every school, no family, no mask, the Spell.dbc
    /// proc flags (the hits its owner takes, 0x222a8, the flags of Focused Will and Blessed Resilience), a critical
    /// hit; and the row it held before, family 6 with the mask of the client's second effect (A 0x808000, B 0x400) on
    /// its first, no hit kind.
    SpellProcEventEntry const kHarnessedShadows = ProcRow(SPELL_SCHOOL_MASK_ALL, 0, 0, 0, PROC_EX_CRITICAL_HIT);
    SpellProcEventEntry const kHarnessedShadowsBefore =
        OnEffectZeroOnly(ProcRow(0, SPELLFAMILY_PRIEST, UI64LIT(0x0000040000808000), 0, PROC_EX_NONE));
    uint32 const kHarnessedShadowsFlags = 0x222a8;

    /// Mind Melt 14910 (its rank 2, 33371, takes its row): family 6, Mind Spike (C 0x10000) on the effect that casts the
    /// cast-time buff, its second, and no mask on the others, the Spell.dbc proc flags (a harmful spell landed,
    /// 0x10000), no hit kind; and the client's mask for that effect, the damage-over-time mask (A 0x2008000, B 0x400).
    SpellProcEventEntry const kMindMelt = OnEffectOnly(ProcRow(0, SPELLFAMILY_PRIEST, 0, 0, PROC_EX_NONE),
                                                       EFFECT_INDEX_1, ClassFamilyMask(0, 0x00010000));
    ClassFamilyMask const kMindMeltClientMask = ClassFamilyMask(UI64LIT(0x0000040002008000), 0);

    /// Paralysis 87192 (its rank 2, 87195, takes its row): family 6, Mind Blast (A 0x2000) on its one effect, the
    /// Spell.dbc proc flags (a harmful spell landed), a critical hit; and the client's mask for that effect, the one
    /// only Shadow Orb Power carries (C 0x100000).
    SpellProcEventEntry const kParalysis = OnEffectOnly(ProcRow(0, SPELLFAMILY_PRIEST, 0, 0, PROC_EX_CRITICAL_HIT),
                                                        EFFECT_INDEX_0, ClassFamilyMask(UI64LIT(0x2000), 0));
    ClassFamilyMask const kParalysisClientMask = ClassFamilyMask(0, 0x00100000);


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

    /// Whether the row of an aura whose Spell.dbc proc flags are `spellProcFlags` takes `spell` at an event of the flag
    /// `procFlag` and the hit kind `procEx`: its mask and the gate both.
    bool TakesAt(SpellProcEventEntry const& row, uint32 spellProcFlags, ProcSpell const& spell, uint32 procFlag,
                 uint32 procEx)
    {
        return FitsMask(row, spell) && Gate(row, spellProcFlags, spell, procFlag, procEx);
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

TEST(SpellProcEventRows_ImpactsTalentRowPassesEveryMageHitAndNoMissThroughTheGate)
{
    CHECK(PassesGate(kImpactTalent, kPolymorph, PROC_EX_NORMAL_HIT));
    CHECK(PassesGate(kImpactTalent, kFireball, PROC_EX_NORMAL_HIT));
    CHECK(PassesGate(kImpactTalent, kFireball, PROC_EX_CRITICAL_HIT));
    CHECK(!PassesGate(kImpactTalent, kPolymorph, PROC_EX_MISS));
    CHECK(!PassesGate(kImpactTalent, kPolymorph, PROC_EX_RESIST));
    CHECK(!PassesGate(kImpactTalent, kPolymorph, PROC_EX_IMMUNE));
    CHECK(!PassesGate(kImpactTalent, kPolymorph, PROC_EX_REFLECT));
    CHECK(!PassesGate(kImpactTalent, kFireball, PROC_EX_CAST_END));
    CHECK(!PassesGate(kImpactTalent, kStarfire, PROC_EX_NORMAL_HIT));
}

TEST(SpellProcEventRows_OffensiveStatesRowTakesTheEndOfAHarmfulMageCastAndNoHit)
{
    // The end of a harmful mage cast, whatever the cast then hits: Polymorph, Fireball, Frostbolt, Cone of Cold,
    // Counterspell and Shattered Barrier's freeze. Never a hit, a crit, a hit that interrupted a cast, a miss, a
    // resist, an immunity or a reflect.
    for (ProcSpell const& spell : { kPolymorph, kFireball, kFrostbolt, kConeOfCold, kCounterspell,
                                    kShatteredBarrierFreeze })
    {
        CHECK(PassesGate(kOffensiveState, spell, PROC_EX_CAST_END));
        for (uint32 procEx : { uint32(PROC_EX_NORMAL_HIT), uint32(PROC_EX_CRITICAL_HIT),
                               uint32(PROC_EX_NORMAL_HIT | PROC_EX_INTERRUPT), uint32(PROC_EX_MISS),
                               uint32(PROC_EX_RESIST), uint32(PROC_EX_IMMUNE), uint32(PROC_EX_REFLECT) })
        {
            CHECK(!PassesGate(kOffensiveState, spell, procEx));
        }
    }

    // With no class mask on the row, each effect then takes the hit kind through SpellMgr::HitKindAccepted, which
    // takes the end of a cast that dealt nothing.
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        CHECK(!kOffensiveState.spellFamilyMask[i]);
    }
    CHECK(SpellMgr::HitKindAccepted(kOffensiveState.procEx, PROC_EX_CAST_END, false));

    // The end of a helpful cast carries the helpful-spell flag, which the aura's own flag does not name; another
    // class's spell is not the row's family.
    CHECK(!Gate(kOffensiveState, kHarmfulSpellLanded, kBlink, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_CAST_END));
    CHECK(!Gate(kOffensiveState, kHarmfulSpellLanded, kManaShield, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL,
                PROC_EX_CAST_END));
    CHECK(!PassesGate(kOffensiveState, kStarfire, PROC_EX_CAST_END));
    CHECK(!PassesGate(kOffensiveState, kLightningBolt, PROC_EX_CAST_END));

    // The chills and freezes cast for the mage are mage spells whose cast ends the row takes as well; Offensive
    // State's script refuses them by id (MageScriptsTest).
    for (ProcSpell const& spell : { kFrostArmorChill, kBlizzardChill, kBlizzardChillRankTwo, kPiercingChill,
                                    kRingFreeze })
    {
        CHECK(PassesGate(kOffensiveState, spell, PROC_EX_CAST_END));
    }
}

TEST(SpellProcEventRows_FingersOfFrostRollsAtTheHitsOfItsChillsAndNeverAtTheEndOfACast)
{
    // Frostbolt, Cone of Cold, Ice Shards' Chilled at both ranks, Piercing Chill's chill (Frostbolt's bit), Frostfire
    // Bolt and the Frostfire Orb's hits at both ranks: their hits and crits, never their cast end, a miss, a resist,
    // an immunity or a reflect. With a mask on the row the gate is the whole check, so a hit that dealt no damage, as
    // a Chilled's, passes as well.
    ProcSpell const chills[] = { kFrostbolt, kConeOfCold, kBlizzardChill, kBlizzardChillRankTwo, kPiercingChill,
                                 kFrostfireBolt, kOrbHitRankTwo, kOrbHitRankOne };
    for (ProcSpell const& spell : chills)
    {
        CHECK(FitsMask(kFingersOfFrost, spell));
        CHECK(Takes(kFingersOfFrost, spell, PROC_EX_NORMAL_HIT));
        CHECK(Takes(kFingersOfFrost, spell, PROC_EX_CRITICAL_HIT));
        CHECK(!Takes(kFingersOfFrost, spell, PROC_EX_CAST_END));
        for (uint32 missed : { uint32(PROC_EX_MISS), uint32(PROC_EX_RESIST), uint32(PROC_EX_IMMUNE),
                               uint32(PROC_EX_REFLECT) })
        {
            CHECK(!Takes(kFingersOfFrost, spell, missed));
        }
    }

    // Flame Orb's hit and the Frostfire Orb's summon carry the C bit 0x80 alone; an Arcane Missiles missile, Fire
    // Blast, Ice Lance, Frost Nova, Waterbolt and Arcane Blast none of the row's bits
    for (ProcSpell const& spell : { kFlameOrbHit, kOrbSummon, kArcaneMissile, kFireBlast, kIceLance, kFrostNova,
                                    kWaterbolt, kArcaneBlast })
    {
        CHECK(!FitsMask(kFingersOfFrost, spell));
        CHECK(!Takes(kFingersOfFrost, spell, PROC_EX_NORMAL_HIT));
    }

    // Frost Armor's Chilled shares Ice Shards' Chilled's bit, so the row takes its hit; Fingers of Frost's script
    // refuses it by id, as it refuses the orb's first-rank hit (MageScriptsTest).
    CHECK(Takes(kFingersOfFrost, kFrostArmorChill, PROC_EX_NORMAL_HIT));
}

TEST(SpellProcEventRows_BrainFreezeRollsAtTheEndOfTheCastOfTheSameChillsLessFrostfireBolt)
{
    ProcSpell const chills[] = { kFrostbolt, kConeOfCold, kBlizzardChill, kBlizzardChillRankTwo, kPiercingChill,
                                 kOrbHitRankTwo, kOrbHitRankOne };
    for (ProcSpell const& spell : chills)
    {
        CHECK(Takes(kBrainFreeze, spell, PROC_EX_CAST_END));
        CHECK(!Takes(kBrainFreeze, spell, PROC_EX_NORMAL_HIT));
        CHECK(!Takes(kBrainFreeze, spell, PROC_EX_CRITICAL_HIT));
    }
    for (ProcSpell const& spell : { kFrostfireBolt, kFlameOrbHit, kOrbSummon, kFireBlast, kIceLance, kFrostNova,
                                    kWaterbolt, kArcaneBlast })
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

TEST(SpellProcEventRows_DivineAegisTakesTheNormalAndCriticalHeals)
{
    CHECK_EQ(kDivineAegisFlags, uint32(0x44000));
    CHECK_EQ(kDivineAegisRank3Flags, uint32(0x44400));
    CHECK_EQ(kDivineAegis.procEx, uint32(3));
    CHECK_EQ(kDivineAegis.schoolMask, uint32(127));
    CHECK_EQ(kDivineAegis.spellFamilyName, uint32(0));
    CHECK_EQ(kDivineAegis.procFlags, uint32(0));
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        CHECK(!kDivineAegis.spellFamilyMask[i]);
    }

    // A direct heal's hit, normal or critical, and a heal over time's tick, normal or critical, at every rank: the
    // script decides which make a shield. Never the end of a cast.
    for (uint32 flags : { kDivineAegisFlags, kDivineAegisRank3Flags })
    {
        for (ProcSpell const& spell : { kFlashHeal, kPrayerOfHealing, kPenanceHeal })
        {
            CHECK(Gate(kDivineAegis, flags, spell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_NORMAL_HIT));
            CHECK(Gate(kDivineAegis, flags, spell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_CRITICAL_HIT));
            CHECK(!Gate(kDivineAegis, flags, spell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_CAST_END));
        }
        CHECK(Gate(kDivineAegis, flags, kRenew, PROC_FLAG_ON_DO_PERIODIC,
                   PROC_EX_PERIODIC_POSITIVE | PROC_EX_NORMAL_HIT));
        CHECK(Gate(kDivineAegis, flags, kRenew, PROC_FLAG_ON_DO_PERIODIC,
                   PROC_EX_PERIODIC_POSITIVE | PROC_EX_CRITICAL_HIT));

        // A harmful spell's hit carries a flag the talent does not name.
        CHECK(!Gate(kDivineAegis, flags, kSmite, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT, PROC_EX_CRITICAL_HIT));
    }

    // With no class mask on the row, each effect then takes the hit kind through SpellMgr::HitKindAccepted: a heal that
    // healed nothing is refused.
    CHECK(SpellMgr::HitKindAccepted(kDivineAegis.procEx, PROC_EX_NORMAL_HIT, true));
    CHECK(SpellMgr::HitKindAccepted(kDivineAegis.procEx, PROC_EX_CRITICAL_HIT, true));
    CHECK(!SpellMgr::HitKindAccepted(kDivineAegis.procEx, PROC_EX_NORMAL_HIT, false));
    CHECK(!SpellMgr::HitKindAccepted(kDivineAegis.procEx, PROC_EX_CRITICAL_HIT, false));
}

TEST(SpellProcEventRows_FocusedWillTakesTheNormalAndCriticalHitsItsOwnerTakesAndNoTick)
{
    CHECK_EQ(kFocusedWill.procEx, uint32(3));
    CHECK_EQ(kFocusedWill.schoolMask, uint32(127));
    CHECK_EQ(kFocusedWill.procFlags, uint32(0));

    // A melee hit taken, normal or critical, passes; a dodge, a parry, a miss and a block do not.
    CHECK(SpellMgr::IsSpellProcEventCanTriggeredBy(&kFocusedWill, kFocusedWillFlags, NULL, PROC_FLAG_TAKEN_MELEE_HIT,
                                                   PROC_EX_NORMAL_HIT));
    CHECK(SpellMgr::IsSpellProcEventCanTriggeredBy(&kFocusedWill, kFocusedWillFlags, NULL, PROC_FLAG_TAKEN_MELEE_HIT,
                                                   PROC_EX_CRITICAL_HIT));
    for (uint32 procEx : { uint32(PROC_EX_DODGE), uint32(PROC_EX_PARRY), uint32(PROC_EX_MISS), uint32(PROC_EX_BLOCK) })
    {
        CHECK(!SpellMgr::IsSpellProcEventCanTriggeredBy(&kFocusedWill, kFocusedWillFlags, NULL,
                                                        PROC_FLAG_TAKEN_MELEE_HIT, procEx));
    }

    // A harmful spell's hit taken, of any school, normal or critical; never its resist.
    for (ProcSpell const& spell : { kSmite, kShadowWordPain, kFireball, kFrostbolt })
    {
        CHECK(Gate(kFocusedWill, kFocusedWillFlags, spell, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT, PROC_EX_NORMAL_HIT));
        CHECK(Gate(kFocusedWill, kFocusedWillFlags, spell, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT, PROC_EX_CRITICAL_HIT));
        CHECK(!Gate(kFocusedWill, kFocusedWillFlags, spell, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT, PROC_EX_RESIST));
    }

    // A periodic tick taken carries a flag the talent does not name, critical or not.
    CHECK(!Gate(kFocusedWill, kFocusedWillFlags, kShadowWordPain, PROC_FLAG_ON_TAKE_PERIODIC, PROC_EX_CRITICAL_HIT));
    CHECK(!Gate(kFocusedWill, kFocusedWillFlags, kShadowWordPain, PROC_FLAG_ON_TAKE_PERIODIC, PROC_EX_NORMAL_HIT));
}

TEST(SpellProcEventRows_SurgeOfLightTakesItsFiveSpellsAndNotShieldRenewMendingOrCircle)
{
    CHECK_EQ(kSurgeOfLightFlags, uint32(0x14000));
    CHECK_EQ(kSurgeOfLight.schoolMask, uint32(0));
    CHECK_EQ(kSurgeOfLight.spellFamilyName, uint32(SPELLFAMILY_PRIEST));
    CHECK_EQ(kSurgeOfLight.procFlags, uint32(0));
    CHECK_EQ(kSurgeOfLight.procEx, uint32(PROC_EX_NONE));
    CHECK(kSurgeOfLight.customChance == 0.0f);
    CHECK_EQ(kSurgeOfLight.cooldown, uint32(0));

    // Smite's hit, a harmful spell landed, normal or critical.
    CHECK(TakesAt(kSurgeOfLight, kSurgeOfLightFlags, kSmite, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT,
                  PROC_EX_NORMAL_HIT));
    CHECK(TakesAt(kSurgeOfLight, kSurgeOfLightFlags, kSmite, PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT,
                  PROC_EX_CRITICAL_HIT));

    // The four heals' hits, a helpful spell landed, normal or critical; the instant Flash Heal of Surge of Light's
    // own buff shares Flash Heal's bit, so it rolls again too (the client's data).
    for (ProcSpell const& spell : { kHeal, kFlashHeal, kGreaterHeal, kBindingHeal, kSurgeFlashHeal })
    {
        CHECK(TakesAt(kSurgeOfLight, kSurgeOfLightFlags, spell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL,
                      PROC_EX_NORMAL_HIT));
        CHECK(TakesAt(kSurgeOfLight, kSurgeOfLightFlags, spell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL,
                      PROC_EX_CRITICAL_HIT));
        CHECK(!TakesAt(kSurgeOfLight, kSurgeOfLightFlags, spell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_MISS));
    }

    // Power Word: Shield, Renew, Prayer of Mending and its heal, Circle of Healing, and the priest's other heals and
    // harmful spells: none of the row's bits. Without the row any of their hits rolled.
    for (ProcSpell const& spell : { kPowerWordShield, kRenew, kPrayerOfMending, kPrayerOfMendingHeal, kCircleOfHealing,
                                    kPrayerOfHealing, kPenanceHeal, kDivineTouchHeal, kShadowWordPain })
    {
        CHECK(!FitsMask(kSurgeOfLight, spell));
        CHECK(!TakesAt(kSurgeOfLight, kSurgeOfLightFlags, spell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL,
                       PROC_EX_NORMAL_HIT));
        CHECK(SpellMgr::IsSpellProcEventCanTriggeredBy(NULL, kSurgeOfLightFlags, Row(spell),
                                                       PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_NORMAL_HIT));
    }

    // Another class's spell is not the row's family.
    CHECK(!Gate(kSurgeOfLight, kSurgeOfLightFlags, kFireball, kHarmfulSpellLanded, PROC_EX_NORMAL_HIT));
}

TEST(SpellProcEventRows_InspirationTakesTheCriticalHealOfPrayerOfMendingAsWellAsItsOtherHeals)
{
    CHECK_EQ(kInspirationFlags & PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, uint32(PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL));
    CHECK_EQ(kInspiration.spellFamilyName, uint32(SPELLFAMILY_PRIEST));
    CHECK_EQ(kInspiration.procFlags, uint32(0));
    CHECK_EQ(kInspiration.procEx, uint32(PROC_EX_CRITICAL_HIT));

    // Prayer of Mending's heal (33110), at a crit: the retail 4.3.4 sniff (build 15595, 2012-05-28) shows its critical
    // heal of 14638, packet 168443, followed by Inspiration, packets 168615 and 168616. Not at a normal heal. The row
    // before took none of it.
    CHECK(TakesAt(kInspiration, kInspirationFlags, kPrayerOfMendingHeal, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL,
                  PROC_EX_CRITICAL_HIT));
    CHECK(!TakesAt(kInspiration, kInspirationFlags, kPrayerOfMendingHeal, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL,
                   PROC_EX_NORMAL_HIT));
    CHECK(!FitsMask(kInspirationBefore, kPrayerOfMendingHeal));

    // The other heals its text names, at a crit, with this row and the one before: Flash Heal, Heal, Greater Heal,
    // Binding Heal, Penance's heal, Prayer of Healing and Circle of Healing.
    for (ProcSpell const& spell : { kFlashHeal, kHeal, kGreaterHeal, kBindingHeal, kPenanceHeal, kPrayerOfHealing,
                                    kCircleOfHealing })
    {
        CHECK(TakesAt(kInspiration, kInspirationFlags, spell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL,
                      PROC_EX_CRITICAL_HIT));
        CHECK(!TakesAt(kInspiration, kInspirationFlags, spell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL,
                       PROC_EX_NORMAL_HIT));
        CHECK(FitsMask(kInspirationBefore, spell));
    }

    // Renew, Power Word: Shield, Smite and Divine Touch's heal are not among them.
    for (ProcSpell const& spell : { kRenew, kPowerWordShield, kSmite, kDivineTouchHeal })
    {
        CHECK(!FitsMask(kInspiration, spell));
    }
}

TEST(SpellProcEventRows_BlessedResilienceTakesTheNormalAndCriticalHitsItsOwnerTakesAndNoTick)
{
    CHECK_EQ(kBlessedResilienceFlags, uint32(0x222a8));
    CHECK_EQ(kBlessedResilience.procEx, uint32(3));
    CHECK_EQ(kBlessedResilience.schoolMask, uint32(127));
    CHECK_EQ(kBlessedResilience.spellFamilyName, uint32(0));
    CHECK_EQ(kBlessedResilience.procFlags, uint32(0));
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        CHECK(!kBlessedResilience.spellFamilyMask[i]);
    }

    // A melee hit taken, normal or critical, passes; with the row before, a normal hit did not. A dodge, a parry, a
    // miss and a block do not.
    uint32 const meleeTaken = PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE;
    CHECK(SpellMgr::IsSpellProcEventCanTriggeredBy(&kBlessedResilience, kBlessedResilienceFlags, NULL, meleeTaken,
                                                   PROC_EX_NORMAL_HIT));
    CHECK(SpellMgr::IsSpellProcEventCanTriggeredBy(&kBlessedResilience, kBlessedResilienceFlags, NULL, meleeTaken,
                                                   PROC_EX_CRITICAL_HIT));
    CHECK(!SpellMgr::IsSpellProcEventCanTriggeredBy(&kBlessedResilienceBefore, kBlessedResilienceFlags, NULL,
                                                    meleeTaken, PROC_EX_NORMAL_HIT));
    CHECK(SpellMgr::IsSpellProcEventCanTriggeredBy(&kBlessedResilienceBefore, kBlessedResilienceFlags, NULL,
                                                   meleeTaken, PROC_EX_CRITICAL_HIT));
    for (uint32 procEx : { uint32(PROC_EX_DODGE), uint32(PROC_EX_PARRY), uint32(PROC_EX_MISS), uint32(PROC_EX_BLOCK) })
    {
        CHECK(!SpellMgr::IsSpellProcEventCanTriggeredBy(&kBlessedResilience, kBlessedResilienceFlags, NULL, meleeTaken,
                                                        procEx));
    }

    // A harmful spell's hit taken, of any school, normal or critical; never its resist.
    for (ProcSpell const& spell : { kSmite, kShadowWordPain, kFireball, kFrostbolt })
    {
        CHECK(Gate(kBlessedResilience, kBlessedResilienceFlags, spell, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT,
                   PROC_EX_NORMAL_HIT));
        CHECK(Gate(kBlessedResilience, kBlessedResilienceFlags, spell, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT,
                   PROC_EX_CRITICAL_HIT));
        CHECK(!Gate(kBlessedResilience, kBlessedResilienceFlags, spell, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT,
                    PROC_EX_RESIST));
    }

    // A periodic tick taken carries flags the talent does not name, critical or not, however large.
    uint32 const tickTaken = PROC_FLAG_ON_TAKE_PERIODIC | PROC_FLAG_TAKEN_ANY_DAMAGE;
    CHECK(!Gate(kBlessedResilience, kBlessedResilienceFlags, kShadowWordPain, tickTaken, PROC_EX_NORMAL_HIT));
    CHECK(!Gate(kBlessedResilience, kBlessedResilienceFlags, kShadowWordPain, tickTaken, PROC_EX_CRITICAL_HIT));
}

TEST(SpellProcEventRows_DivineTouchRollsAtAChanceOf100WhenThePriestsRenewLands)
{
    CHECK(kDivineTouch.customChance == 100.0f);
    CHECK_EQ(kDivineTouch.procFlags, uint32(PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL));
    CHECK_EQ(kDivineTouch.procEx, uint32(PROC_EX_NONE));
    CHECK_EQ(kDivineTouch.spellFamilyName, uint32(SPELLFAMILY_PRIEST));
    CHECK_EQ(kDivineTouch.cooldown, uint32(0));

    // Renew's hit, normal or critical; the client gives the talent no proc flags, so the row's are the event's.
    CHECK_EQ(EventFlags(&kDivineTouch, 0), uint32(PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL));
    CHECK(TakesAt(kDivineTouch, 0, kRenew, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_NORMAL_HIT));
    CHECK(TakesAt(kDivineTouch, 0, kRenew, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_CRITICAL_HIT));

    // Renew's ticks carry a flag the row does not name; Divine Touch's own heal and the other heals fit none of its
    // bits.
    CHECK(!Gate(kDivineTouch, 0, kRenew, PROC_FLAG_ON_DO_PERIODIC, PROC_EX_PERIODIC_POSITIVE | PROC_EX_NORMAL_HIT));
    for (ProcSpell const& spell : { kDivineTouchHeal, kFlashHeal, kPrayerOfMendingHeal, kSurgeFlashHeal })
    {
        CHECK(!FitsMask(kDivineTouch, spell));
    }
}

TEST(SpellProcEventRows_HarnessedShadowsTakesTheCriticalHitsItsOwnerTakesOfAnyFamilyAndNoNormalHitOrTick)
{
    CHECK_EQ(kHarnessedShadowsFlags, uint32(0x222a8));
    CHECK_EQ(kHarnessedShadows.procEx, uint32(PROC_EX_CRITICAL_HIT));
    CHECK_EQ(kHarnessedShadows.schoolMask, uint32(127));
    CHECK_EQ(kHarnessedShadows.spellFamilyName, uint32(0));
    CHECK_EQ(kHarnessedShadows.procFlags, uint32(0));
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        CHECK(!kHarnessedShadows.spellFamilyMask[i]);
    }

    // A melee hit taken passes when it is a critical one; a normal hit, a dodge, a parry, a miss and a block do not.
    uint32 const meleeTaken = PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE;
    CHECK(SpellMgr::IsSpellProcEventCanTriggeredBy(&kHarnessedShadows, kHarnessedShadowsFlags, NULL, meleeTaken,
                                                   PROC_EX_CRITICAL_HIT));
    CHECK(!SpellMgr::IsSpellProcEventCanTriggeredBy(&kHarnessedShadows, kHarnessedShadowsFlags, NULL, meleeTaken,
                                                    PROC_EX_NORMAL_HIT));
    for (uint32 procEx : { uint32(PROC_EX_DODGE), uint32(PROC_EX_PARRY), uint32(PROC_EX_MISS), uint32(PROC_EX_BLOCK) })
    {
        CHECK(!SpellMgr::IsSpellProcEventCanTriggeredBy(&kHarnessedShadows, kHarnessedShadowsFlags, NULL, meleeTaken,
                                                        procEx));
    }

    // A harmful spell's hit taken, a mage's among them, passes when it is a critical one; a normal hit and a resist do
    // not. The row before took none of the mage's: it asked the priest family.
    for (ProcSpell const& spell : { kFireball, kFrostbolt, kSmite, kShadowWordPain, kMindBlast })
    {
        CHECK(Gate(kHarnessedShadows, kHarnessedShadowsFlags, spell, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT,
                   PROC_EX_CRITICAL_HIT));
        CHECK(!Gate(kHarnessedShadows, kHarnessedShadowsFlags, spell, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT,
                    PROC_EX_NORMAL_HIT));
        CHECK(!Gate(kHarnessedShadows, kHarnessedShadowsFlags, spell, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT,
                    PROC_EX_RESIST));
    }
    CHECK(!Gate(kHarnessedShadowsBefore, kHarnessedShadowsFlags, kFireball, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT,
                PROC_EX_CRITICAL_HIT));
    CHECK(!Gate(kHarnessedShadowsBefore, kHarnessedShadowsFlags, kFireball, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT,
                PROC_EX_NORMAL_HIT));

    // The row before let a priest's normal hit through as it let a critical one: its hit kind was none.
    CHECK(Gate(kHarnessedShadowsBefore, kHarnessedShadowsFlags, kShadowWordPain, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT,
               PROC_EX_NORMAL_HIT));

    // A periodic tick taken carries flags the talent does not name, critical or not.
    uint32 const tickTaken = PROC_FLAG_ON_TAKE_PERIODIC | PROC_FLAG_TAKEN_ANY_DAMAGE;
    CHECK(!Gate(kHarnessedShadows, kHarnessedShadowsFlags, kShadowWordPain, tickTaken, PROC_EX_CRITICAL_HIT));
    CHECK(!Gate(kHarnessedShadows, kHarnessedShadowsFlags, kShadowWordPain, tickTaken, PROC_EX_NORMAL_HIT));
}

TEST(SpellProcEventRows_MindMeltTakesTheHitOfMindSpikeOnItsSecondEffectAndNoOtherSpell)
{
    CHECK_EQ(kMindMelt.spellFamilyName, uint32(SPELLFAMILY_PRIEST));
    CHECK_EQ(kMindMelt.schoolMask, uint32(0));
    CHECK_EQ(kMindMelt.procFlags, uint32(0));
    CHECK_EQ(kMindMelt.procEx, uint32(PROC_EX_NONE));
    CHECK(!kMindMelt.spellFamilyMask[EFFECT_INDEX_0]);
    CHECK(!kMindMelt.spellFamilyMask[EFFECT_INDEX_2]);
    CHECK(kMindMelt.spellFamilyMask[EFFECT_INDEX_1]);

    // Mind Spike's hit, a harmful spell landed, normal or critical, fits the second effect's mask and passes the
    // gate; the client's mask for the effect, the damage-over-time mask, did not fit it.
    ClassFamilyMask const& mask = kMindMelt.spellFamilyMask[EFFECT_INDEX_1];
    CHECK(Row(kMindSpike)->IsFitToFamilyMask(mask));
    CHECK(!Row(kMindSpike)->IsFitToFamilyMask(kMindMeltClientMask));
    CHECK(PassesGate(kMindMelt, kMindSpike, PROC_EX_NORMAL_HIT));
    CHECK(PassesGate(kMindMelt, kMindSpike, PROC_EX_CRITICAL_HIT));

    // Mind Blast, Shadow Word: Pain and the priest's other spells fit no bit of it: Mind Blast's class bit (A 0x2000)
    // is not Mind Spike's (C 0x10000).
    for (ProcSpell const& spell : { kMindBlast, kShadowWordPain, kSmite, kFlashHeal, kRenew })
    {
        CHECK(!Row(spell)->IsFitToFamilyMask(mask));
    }

    // Another class's spell is not the row's family.
    CHECK(!PassesGate(kMindMelt, kFireball, PROC_EX_NORMAL_HIT));

    // A miss, a resist and a dodge of Mind Spike do not pass the gate.
    for (uint32 procEx : { uint32(PROC_EX_MISS), uint32(PROC_EX_RESIST), uint32(PROC_EX_DODGE) })
    {
        CHECK(!PassesGate(kMindMelt, kMindSpike, procEx));
    }
}

TEST(SpellProcEventRows_ParalysisTakesTheCriticalHitOfMindBlastAndNeitherItsNormalHitNorMindSpikes)
{
    CHECK_EQ(kParalysis.spellFamilyName, uint32(SPELLFAMILY_PRIEST));
    CHECK_EQ(kParalysis.schoolMask, uint32(0));
    CHECK_EQ(kParalysis.procFlags, uint32(0));
    CHECK_EQ(kParalysis.procEx, uint32(PROC_EX_CRITICAL_HIT));
    CHECK(kParalysis.spellFamilyMask[EFFECT_INDEX_0]);
    CHECK(!kParalysis.spellFamilyMask[EFFECT_INDEX_1]);
    CHECK(!kParalysis.spellFamilyMask[EFFECT_INDEX_2]);

    // Mind Blast's critical hit passes; its normal hit does not.
    CHECK(Row(kMindBlast)->IsFitToFamilyMask(kParalysis.spellFamilyMask[EFFECT_INDEX_0]));
    CHECK(PassesGate(kParalysis, kMindBlast, PROC_EX_CRITICAL_HIT));
    CHECK(!PassesGate(kParalysis, kMindBlast, PROC_EX_NORMAL_HIT));

    // Mind Spike's critical hit passes the gate and fits no bit of the mask; neither do Shadow Word: Pain's or
    // Smite's.
    CHECK(PassesGate(kParalysis, kMindSpike, PROC_EX_CRITICAL_HIT));
    for (ProcSpell const& spell : { kMindSpike, kShadowWordPain, kSmite })
    {
        CHECK(!Row(spell)->IsFitToFamilyMask(kParalysis.spellFamilyMask[EFFECT_INDEX_0]));
    }

    // The client's mask for the effect, the one only Shadow Orb Power carries, does not fit Mind Blast: its critical
    // hit met no Paralysis without the row.
    CHECK(!Row(kMindBlast)->IsFitToFamilyMask(kParalysisClientMask));

    // The end of Mind Blast's cast, a miss and a resist do not pass the gate.
    for (uint32 procEx : { uint32(PROC_EX_CAST_END), uint32(PROC_EX_MISS), uint32(PROC_EX_RESIST) })
    {
        CHECK(!PassesGate(kParalysis, kMindBlast, procEx));
    }
}
