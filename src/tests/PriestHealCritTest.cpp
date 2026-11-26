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

/// Tests of priest/discipline/HealCrit.cpp: the rows the file registers, in the game's table too; the heals
/// Renewed Hope and Glyph of Flash Heal raise, by their effects' class masks and, for Flash Heal and Renewed Hope, by
/// the talent's text; the rank of Renewed Hope a priest holds and the targets it asks for (Weakened Soul, or his own
/// Grace and not another's); the glyph's health percent; and the crit chance the row adds. The scripts run on bare
/// Creatures (update fields allocated; no world, map, template or AI), each with a guid of its own so a holder's
/// caster tells them apart. The talents, the glyph, Weakened Soul and Grace are put into the spell store with only
/// the fields the scripts read, their records as the client's data holds them, and are taken out of it again; the
/// heals are Spell.dbc rows with their class options, not in the spell store. Scenario 1209 records the crit chance
/// the engine computes in play, and Grace's caster check on the heal its target takes.

#include "TestHarness.h"
#include "spells/classes/priest/discipline/HealCrit.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/auras/SpellAuras.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const RENEWED_HOPE_RANKS[] = { 57470, 57472 };
    uint32 const GLYPH_OF_FLASH_HEAL = 55679;
    uint32 const WEAKENED_SOUL = 6788;
    uint32 const GRACE_AURAS[] = { 47930, 77613 };
    uint32 const FLASH_HEAL = 2061;
    uint32 const FLASH_HEAL_INSTANT = 101062;
    uint32 const GREATER_HEAL = 2060;
    uint32 const HEAL_SPELL = 2050;
    uint32 const PENANCE_HEAL = 47750;
    uint32 const PRAYER_OF_HEALING = 596;
    uint32 const RENEW = 139;
    uint32 const POWER_WORD_SHIELD = 17;

    // The SpellClassOptions rows the test's spells name; no client data holds these ids.
    uint32 const kFirstOptions = 94880;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            // The holders HoldAura put in the map are the test's own, and leave the map first.
            ~BareCreature()
            {
                m_auras.Holders().clear();
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // The guid of `counter`, its health and its maximum health.
            void Stand(uint32 counter, uint32 health, uint32 maxHealth)
            {
                SetGuidValue(OBJECT_FIELD_GUID, ObjectGuid(HIGHGUID_UNIT, 50249, counter));
                SetMaxHealth(maxHealth);
                SetHealth(health);
            }

            // Puts `holder` in the holder map under `spellId`, as Unit::AddSpellAuraHolder files it.
            void HoldAura(uint32 spellId, SpellAuraHolder& holder)
            {
                m_auras.AddHolder(spellId, &holder);
            }
    };

    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    // A heal's family flags as the client's data holds them: the first two words, then the third; and its family.
    struct HealSpell
    {
        uint32 spellId;
        SpellFamily family;
        uint32 maskA;
        uint32 maskB;
        bool renewedHope;                   ///< whether Renewed Hope raises it
        bool glyph;                         ///< whether Glyph of Flash Heal raises it
    };

    // The priest's heals and spells near them, and two mage spells on Heal's and Flash Heal's flags (ids no client
    // data holds), which the priest's masks must not take.
    HealSpell const HEAL_SPELLS[] =
    {
        { FLASH_HEAL, SPELLFAMILY_PRIEST, 0x00000800, 0, true, true },
        { FLASH_HEAL_INSTANT, SPELLFAMILY_PRIEST, 0x00000800, 0, true, true },
        { GREATER_HEAL, SPELLFAMILY_PRIEST, 0x00001000, 0, true, false },
        { HEAL_SPELL, SPELLFAMILY_PRIEST, 0x00000400, 0, true, false },
        { PENANCE_HEAL, SPELLFAMILY_PRIEST, 0, 0x00010000, true, false },
        { PRAYER_OF_HEALING, SPELLFAMILY_PRIEST, 0x00000200, 0, false, false },
        { RENEW, SPELLFAMILY_PRIEST, 0x00000040, 0, false, false },
        { POWER_WORD_SHIELD, SPELLFAMILY_PRIEST, 0x00000001, 0, false, false },
        { 94881, SPELLFAMILY_MAGE, 0x00000400, 0, false, false },
        { 94882, SPELLFAMILY_MAGE, 0x00000800, 0, false, false },
    };
    std::size_t const HEAL_SPELL_COUNT = sizeof(HEAL_SPELLS) / sizeof(HEAL_SPELLS[0]);

    // Renewed Hope's two ranks, each with its effect 0 value and class mask as the client's data holds them (5 and
    // 10, A 0x00001400, B 0x00010000); the glyph, its effect 0 (10, A 0x00000800) and its effect 1 (25), the
    // priest's family on all three; Weakened Soul and Grace's two auras with only their ids.
    ServerSpell s_ranks[2];
    ServerSpell s_glyph;
    SpellEffectEntry s_glyphPercent;
    ServerSpell s_weakenedSoul;
    ServerSpell s_grace[2];
    SpellClassOptionsEntry s_priestOptions;

    // The heals of HEAL_SPELLS, with their class options.
    SpellRowStorage s_healRows[HEAL_SPELL_COUNT];
    SpellClassOptionsEntry s_healOptions[HEAL_SPELL_COUNT];

    // A Spell.dbc row with only its id; it is not in the spell store.
    SpellEntry& SpellRow(SpellRowStorage& storage, uint32 spellId)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        SpellEntry* row = reinterpret_cast<SpellEntry*>(storage.bytes);
        row->ID = spellId;
        return *row;
    }

    // Puts the talents, the glyph, Weakened Soul and Grace into the spell store; their effects, their class options
    // and the heals' rows once.
    void StoreRows()
    {
        static bool stored = false;
        if (!stored)
        {
            stored = true;
            s_priestOptions = SpellClassOptionsEntry();
            s_priestOptions.SpellClassSet = SPELLFAMILY_PRIEST;
            sSpellClassOptionsStore.SetEntry(kFirstOptions, &s_priestOptions);
            int32 const rankValues[2] = { 5, 10 };
            for (uint32 i = 0; i < 2; ++i)
            {
                s_ranks[i].effect = SpellEffectEntry();
                s_ranks[i].effect.SpellID = RENEWED_HOPE_RANKS[i];
                s_ranks[i].effect.EffectIndex = EFFECT_INDEX_0;
                s_ranks[i].effect.Effect = SPELL_EFFECT_APPLY_AURA;
                s_ranks[i].effect.EffectAura = SPELL_AURA_DUMMY;
                s_ranks[i].effect.EffectBasePoints = rankValues[i];
                s_ranks[i].effect.EffectSpellClassMask = ClassFamilyMask(0x00001400 | (uint64(0x00010000) << 32), 0);
                AddSpellEffectEntry(&s_ranks[i].effect);
            }
            s_glyph.effect = SpellEffectEntry();
            s_glyph.effect.SpellID = GLYPH_OF_FLASH_HEAL;
            s_glyph.effect.EffectIndex = EFFECT_INDEX_0;
            s_glyph.effect.Effect = SPELL_EFFECT_APPLY_AURA;
            s_glyph.effect.EffectAura = SPELL_AURA_DUMMY;
            s_glyph.effect.EffectBasePoints = 10;
            s_glyph.effect.EffectSpellClassMask = ClassFamilyMask(0x00000800, 0);
            AddSpellEffectEntry(&s_glyph.effect);
            s_glyphPercent = SpellEffectEntry();
            s_glyphPercent.SpellID = GLYPH_OF_FLASH_HEAL;
            s_glyphPercent.EffectIndex = EFFECT_INDEX_1;
            s_glyphPercent.EffectBasePoints = 25;
            AddSpellEffectEntry(&s_glyphPercent);
            for (std::size_t i = 0; i < HEAL_SPELL_COUNT; ++i)
            {
                HealSpell const& heal = HEAL_SPELLS[i];
                uint32 const optionsId = kFirstOptions + 3 + uint32(i);
                s_healOptions[i] = SpellClassOptionsEntry();
                s_healOptions[i].SpellClassSet = heal.family;
                s_healOptions[i].SpellClassMask = ClassFamilyMask(heal.maskA | (uint64(heal.maskB) << 32), 0);
                sSpellClassOptionsStore.SetEntry(optionsId, &s_healOptions[i]);
                SpellRow(s_healRows[i], heal.spellId).ClassOptionsID = optionsId;
            }
        }
        for (uint32 i = 0; i < 2; ++i)
        {
            s_ranks[i].Entry().ID = RENEWED_HOPE_RANKS[i];
            s_ranks[i].Entry().ClassOptionsID = kFirstOptions;
            sSpellStore.SetEntry(RENEWED_HOPE_RANKS[i], &s_ranks[i].Entry());
            s_grace[i].Entry().ID = GRACE_AURAS[i];
            sSpellStore.SetEntry(GRACE_AURAS[i], &s_grace[i].Entry());
        }
        s_glyph.Entry().ID = GLYPH_OF_FLASH_HEAL;
        s_glyph.Entry().ClassOptionsID = kFirstOptions;
        sSpellStore.SetEntry(GLYPH_OF_FLASH_HEAL, &s_glyph.Entry());
        s_weakenedSoul.Entry().ID = WEAKENED_SOUL;
        sSpellStore.SetEntry(WEAKENED_SOUL, &s_weakenedSoul.Entry());
    }

    // Takes them out of the spell store again.
    void ForgetRows()
    {
        for (uint32 spellId : { RENEWED_HOPE_RANKS[0], RENEWED_HOPE_RANKS[1], GLYPH_OF_FLASH_HEAL, WEAKENED_SOUL,
                                GRACE_AURAS[0], GRACE_AURAS[1] })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }

    // The row of HEAL_SPELLS' spell `spellId`, with its class options; StoreRows makes them.
    SpellEntry const& HealRow(uint32 spellId)
    {
        for (std::size_t i = 0; i < HEAL_SPELL_COUNT; ++i)
        {
            if (HEAL_SPELLS[i].spellId == spellId)
            {
                return *reinterpret_cast<SpellEntry const*>(s_healRows[i].bytes);
            }
        }
        return *reinterpret_cast<SpellEntry const*>(s_healRows[0].bytes);
    }

    // A holder of `spellId`, from the store, on `target`, put in its holder map; `caster` its caster, `target`
    // itself when NULL.
    SpellAuraHolder* Hold(uint32 spellId, BareCreature& target, BareCreature* caster = NULL)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(sSpellStore.LookupEntry(spellId), &target, caster, NULL, NULL);
        target.HoldAura(spellId, *holder);
        return holder;
    }

    // The crit chance the game's crit-chance row of `spellId` leaves of `before` for `priest`'s heal at `target`.
    float RowChance(SpellHandlerRegistry const& registry, BareCreature& priest, BareCreature& target, uint32 spellId,
                    float before)
    {
        float chance = before;
        CritChanceContext ctx(priest, target, HealRow(spellId), HEAL, chance);
        CHECK(registry.Dispatch<CritChanceHook>(spellId, ctx).IsContinue());
        return chance;
    }
}

TEST(PriestHealCrit_TheFileHoldsItsFiveRowsInTheGame)
{
    // Five rows, all at the crit-chance hook, all one body: Flash Heal and its instant cast, Greater Heal, Heal and
    // Penance's heal; no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterHealCrit(registry), uint32(5));
    CHECK_EQ(registry.Count(), std::size_t(5));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(CritChanceHook::Key), std::size_t(5));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { FLASH_HEAL, FLASH_HEAL_INSTANT, GREATER_HEAL, HEAL_SPELL, PENANCE_HEAL })
    {
        CHECK(registry.Find<CritChanceHook>(spellId) != NULL);
        CHECK(registry.Find<CritChanceHook>(spellId) == registry.Find<CritChanceHook>(FLASH_HEAL));
        CHECK(game.Find<CritChanceHook>(spellId) == registry.Find<CritChanceHook>(spellId));
    }

    // The talents, the glyph, Weakened Soul, Grace and the heals neither raises hold no row.
    for (uint32 spellId : { RENEWED_HOPE_RANKS[0], RENEWED_HOPE_RANKS[1], GLYPH_OF_FLASH_HEAL, WEAKENED_SOUL,
                            GRACE_AURAS[0], GRACE_AURAS[1], PRAYER_OF_HEALING, RENEW })
    {
        CHECK(registry.Find<CritChanceHook>(spellId) == NULL);
    }
}

TEST(PriestHealCrit_TheMasksAndTheTextNameTheHealsEachRaises)
{
    StoreRows();
    for (HealSpell const& heal : HEAL_SPELLS)
    {
        SpellEntry const& row = HealRow(heal.spellId);
        CHECK_EQ(Priest::RenewedHopeTakes(row), heal.renewedHope);
        CHECK_EQ(Priest::FlashHealGlyphTakes(row), heal.glyph);
    }
    ForgetRows();

    // Without the talent's and the glyph's records the masks take nothing; Flash Heal stays Renewed Hope's by the
    // talent's text.
    for (HealSpell const& heal : HEAL_SPELLS)
    {
        SpellEntry const& row = HealRow(heal.spellId);
        bool const flashHeal = heal.spellId == FLASH_HEAL || heal.spellId == FLASH_HEAL_INSTANT;
        CHECK_EQ(Priest::RenewedHopeTakes(row), flashHeal);
        CHECK(!Priest::FlashHealGlyphTakes(row));
    }
}

TEST(PriestHealCrit_RenewedHopeAsksForWeakenedSoulOrTheCastersOwnGrace)
{
    StoreRows();
    {
        // The rank a priest holds, the second first.
        BareCreature priest;
        priest.Stand(1, 1000, 1000);
        CHECK_EQ(Priest::RenewedHopeRank(priest), uint32(0));
        Hold(RENEWED_HOPE_RANKS[0], priest);
        CHECK_EQ(Priest::RenewedHopeRank(priest), RENEWED_HOPE_RANKS[0]);
        Hold(RENEWED_HOPE_RANKS[1], priest);
        CHECK_EQ(Priest::RenewedHopeRank(priest), RENEWED_HOPE_RANKS[1]);

        BareCreature other;
        other.Stand(2, 1000, 1000);
        CHECK_EQ(Priest::RenewedHopeRank(other), uint32(0));

        // A target with neither aura.
        BareCreature plain;
        plain.Stand(3, 1000, 1000);
        CHECK(!Priest::RenewedHopeTarget(priest, plain));

        // Weakened Soul, whoever put it on.
        BareCreature shielded;
        shielded.Stand(4, 1000, 1000);
        Hold(WEAKENED_SOUL, shielded, &other);
        CHECK(Priest::RenewedHopeTarget(priest, shielded));
        CHECK(Priest::RenewedHopeTarget(other, shielded));

        // Each of Grace's auras, when the priest put it on; another's Grace is not his.
        for (uint32 grace : GRACE_AURAS)
        {
            BareCreature graced;
            graced.Stand(5, 1000, 1000);
            Hold(grace, graced, &other);
            CHECK(!Priest::RenewedHopeTarget(priest, graced));
            CHECK(Priest::RenewedHopeTarget(other, graced));
            Hold(grace, graced, &priest);
            CHECK(Priest::RenewedHopeTarget(priest, graced));
        }

        // A Grace the target put on itself is its own, not the priest's.
        BareCreature selfGraced;
        selfGraced.Stand(6, 1000, 1000);
        Hold(GRACE_AURAS[1], selfGraced);
        CHECK(!Priest::RenewedHopeTarget(priest, selfGraced));
        CHECK(Priest::RenewedHopeTarget(selfGraced, selfGraced));
    }
    ForgetRows();
}

TEST(PriestHealCrit_TheGlyphAsksForATargetAtOrUnderItsPercent)
{
    struct Vector
    {
        uint32 health;
        uint32 maxHealth;
        int32 percent;
        bool expected;
    };
    Vector const vectors[] =
    {
        { 25, 100, 25, true }, { 24, 100, 25, true }, { 0, 100, 25, true }, { 26, 100, 25, false },
        { 50, 100, 25, false }, { 100, 100, 25, false },
        // A whole quarter of 42377 is 10594.25: 10594 is under it, 10595 above.
        { 10594, 42377, 25, true }, { 10595, 42377, 25, false },
        // No overflow at the largest health.
        { UINT32_MAX, UINT32_MAX, 100, true }, { UINT32_MAX, UINT32_MAX, 99, false },
        { UINT32_MAX / 4, UINT32_MAX, 25, true },
        // No maximum, no percent.
        { 0, 0, 25, false }, { 10, 100, 0, false }, { 0, 100, 0, false }, { 10, 100, -25, false },
    };
    for (Vector const& v : vectors)
    {
        CHECK_EQ(Priest::AtOrUnderPercent(v.health, v.maxHealth, v.percent), v.expected);
    }
}

TEST(PriestHealCrit_TheRowAddsTheRanksAndTheGlyphsValues)
{
    SpellHandlerRegistry registry;
    Priest::RegisterHealCrit(registry);
    StoreRows();
    {
        BareCreature priest;
        priest.Stand(1, 1000, 1000);
        Hold(RENEWED_HOPE_RANKS[1], priest);
        Hold(GLYPH_OF_FLASH_HEAL, priest);
        BareCreature other;
        other.Stand(2, 1000, 1000);

        // A target at full health with neither aura: nothing is added.
        BareCreature plain;
        plain.Stand(3, 40000, 40000);
        for (uint32 spellId : { FLASH_HEAL, FLASH_HEAL_INSTANT, GREATER_HEAL, HEAL_SPELL, PENANCE_HEAL })
        {
            CHECK_EQ(RowChance(registry, priest, plain, spellId, 7.5f), 7.5f);
            CHECK_EQ(Priest::HealCritBonus(priest, plain, HealRow(spellId)), int32(0));
        }

        // Weakened Soul: the second rank's 10 on each heal Renewed Hope raises, none on Prayer of Healing.
        BareCreature shielded;
        shielded.Stand(4, 40000, 40000);
        Hold(WEAKENED_SOUL, shielded, &other);
        for (uint32 spellId : { FLASH_HEAL, FLASH_HEAL_INSTANT, GREATER_HEAL, HEAL_SPELL, PENANCE_HEAL })
        {
            CHECK_EQ(RowChance(registry, priest, shielded, spellId, 7.5f), 17.5f);
        }
        CHECK_EQ(Priest::HealCritBonus(priest, shielded, HealRow(PRAYER_OF_HEALING)), int32(0));

        // His own Grace: 10; another's: nothing; with Weakened Soul too: 10 once, not twice.
        BareCreature graced;
        graced.Stand(5, 40000, 40000);
        Hold(GRACE_AURAS[1], graced, &other);
        CHECK_EQ(RowChance(registry, priest, graced, GREATER_HEAL, 7.5f), 7.5f);
        Hold(GRACE_AURAS[1], graced, &priest);
        CHECK_EQ(RowChance(registry, priest, graced, GREATER_HEAL, 7.5f), 17.5f);
        CHECK_EQ(RowChance(registry, priest, graced, FLASH_HEAL, 7.5f), 17.5f);
        Hold(WEAKENED_SOUL, graced, &other);
        CHECK_EQ(RowChance(registry, priest, graced, GREATER_HEAL, 7.5f), 17.5f);

        // At a quarter of its health: the glyph's 10 on Flash Heal alone; with Weakened Soul, both: 20.
        BareCreature low;
        low.Stand(6, 10000, 40000);
        CHECK_EQ(RowChance(registry, priest, low, FLASH_HEAL, 7.5f), 17.5f);
        CHECK_EQ(RowChance(registry, priest, low, FLASH_HEAL_INSTANT, 7.5f), 17.5f);
        CHECK_EQ(RowChance(registry, priest, low, GREATER_HEAL, 7.5f), 7.5f);
        CHECK_EQ(RowChance(registry, priest, low, PENANCE_HEAL, 7.5f), 7.5f);
        Hold(WEAKENED_SOUL, low, &other);
        CHECK_EQ(RowChance(registry, priest, low, FLASH_HEAL, 7.5f), 27.5f);
        CHECK_EQ(RowChance(registry, priest, low, GREATER_HEAL, 7.5f), 17.5f);

        // Just above a quarter, and at half: no glyph.
        BareCreature quarterAndOne;
        quarterAndOne.Stand(7, 10001, 40000);
        CHECK_EQ(RowChance(registry, priest, quarterAndOne, FLASH_HEAL, 7.5f), 7.5f);
        BareCreature half;
        half.Stand(8, 20000, 40000);
        CHECK_EQ(RowChance(registry, priest, half, FLASH_HEAL, 7.5f), 7.5f);

        // The first rank's 5; and a priest with neither the talent nor the glyph adds nothing anywhere.
        BareCreature rankOne;
        rankOne.Stand(9, 1000, 1000);
        Hold(RENEWED_HOPE_RANKS[0], rankOne);
        CHECK_EQ(RowChance(registry, rankOne, shielded, GREATER_HEAL, 7.5f), 12.5f);
        CHECK_EQ(RowChance(registry, rankOne, low, FLASH_HEAL, 7.5f), 12.5f);
        CHECK_EQ(RowChance(registry, other, low, FLASH_HEAL, 7.5f), 7.5f);
        CHECK_EQ(RowChance(registry, other, shielded, GREATER_HEAL, 7.5f), 7.5f);
    }
    ForgetRows();
}
