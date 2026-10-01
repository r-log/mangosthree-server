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

/// Unit::SpellHitResult for a unit that is its own victim.
///
/// The unit is a bare Creature with its update fields allocated: no map, no template, no AI.
/// A second unit cannot stand in as a victim here: Creature::IsImmuneToSpell reads the creature
/// template for any victim that is not the caster.
/// Its subtype is PET so that MagicSpellHitResult's level read (Creature::GetLevelForTarget ->
/// IsWorldBoss) answers without a creature template. The spell is a Spell.dbc row built as the
/// loader builds it, fire school, with a SpellCategories row whose DefenseType is MAGIC, and no
/// Spell.dbc store entry, so IsPositiveSpell answers false. The unit's spell hit chance is
/// lowered by 100 points, which clamps MagicSpellHitResult's hit chance to 1%: a roll misses
/// 99 times in 100, so a result that never misses over many seeded calls was never rolled.
///
/// For the reflect check the unit carries one SPELL_AURA_REFLECT_SPELLS aura of amount 100. An
/// `Aura` cannot be constructed in this binary (its constructor reaches the spell stores and a
/// holder), so it is zeroed storage the size of one with only its modifier's amount written:
/// the reflect check reads nothing else of it (Unit::GetTotalAuraModifier reads
/// `GetModifier()->m_amount`), and the unit holds no aura holder, so the PROC_EX_REFLECT proc it
/// fires walks an empty holder map.

#include "TestHarness.h"
#include "Creature.h"
#include "DBCStores.h"
#include "RNGen.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "Unit.h"

namespace
{
    const uint32 kSpellFire = 93701;
    const uint32 kCategoriesMagic = 93701;
    const int kCalls = 200;

    // SpellCategories.dbc row: { Category, DefenseType, DispelType, Mechanic, PreventionType,
    // StartRecoveryCategory }.
    SpellCategoriesEntry s_catMagic = { 0, SPELL_DAMAGE_CLASS_MAGIC, 0, 0, 0, 0 };

    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    SpellRowStorage s_row = {};

    struct AuraStorage
    {
        alignas(Aura) unsigned char bytes[sizeof(Aura)];
    };

    AuraStorage s_reflect = {};

    /// A REFLECT_SPELLS aura whose only written field is its amount: a 100% reflect chance.
    Aura* ReflectEverything()
    {
        Aura* aura = reinterpret_cast<Aura*>(s_reflect.bytes);
        aura->GetModifier()->m_amount = 100;
        return aura;
    }

    SpellEntry const* FireSpell()
    {
        static bool seeded = false;
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_row.bytes);
        if (!seeded)
        {
            seeded = true;
            sSpellCategoriesStore.SetEntry(kCategoriesMagic, &s_catMagic);
            row->ID = kSpellFire;
            row->CategoriesID = kCategoriesMagic;
            row->SchoolMask = SPELL_SCHOOL_MASK_FIRE;
        }
        return row;
    }

    class BareUnit : public Creature
    {
        public:
            BareUnit() : Creature(CREATURE_SUBTYPE_PET)
            {
                _InitValues();
                m_modSpellHitChance = -100.0f;
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~BareUnit()
            {
                m_auras.ByType(SPELL_AURA_REFLECT_SPELLS).clear();
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            /// Puts the aura on the unit's REFLECT_SPELLS list, where the reflect check reads it.
            void Carry(Aura* reflect)
            {
                m_auras.ByType(SPELL_AURA_REFLECT_SPELLS).push_back(reflect);
            }
    };

    /// How many of kCalls seeded calls answered `result`.
    int Count(Unit& unit, SpellHitFor hitFor, SpellMissInfo result, bool canReflect = false)
    {
        SpellEntry const* spell = FireSpell();
        RNG::Seed(1949);
        int n = 0;
        for (int i = 0; i < kCalls; ++i)
        {
            if (unit.SpellHitResult(&unit, spell, canReflect, hitFor) == result)
            {
                ++n;
            }
        }
        return n;
    }
}

TEST(SpellHitResult_TheSpellIsBuiltAsMagic)
{
    SpellEntry const* spell = FireSpell();
    CHECK_EQ(spell->GetDmgClass(), uint32(SPELL_DAMAGE_CLASS_MAGIC));
    CHECK(!IsPositiveSpell(spell->ID));
}

TEST(SpellHitResult_ASelfCastHitsItsCasterWithoutARoll)
{
    BareUnit unit;
    CHECK_EQ(Count(unit, SpellHitFor::Cast, SPELL_MISS_NONE), kCalls);
}

TEST(SpellHitResult_ASelfCastMeetsItsCastersReflectionFirst)
{
    BareUnit unit;
    unit.Carry(ReflectEverything());
    CHECK_EQ(unit.GetTotalAuraModifier(SPELL_AURA_REFLECT_SPELLS), 100);
    CHECK_EQ(Count(unit, SpellHitFor::Cast, SPELL_MISS_REFLECT, true), kCalls);
    CHECK_EQ(Count(unit, SpellHitFor::Cast, SPELL_MISS_NONE, false), kCalls);
}

TEST(SpellHitResult_AReflectionStillRollsAgainstItsCaster)
{
    BareUnit unit;
    const int misses = Count(unit, SpellHitFor::Reflection, SPELL_MISS_MISS);
    const int hits = Count(unit, SpellHitFor::Reflection, SPELL_MISS_NONE);
    CHECK(misses > kCalls / 2);
    CHECK_EQ(misses + hits, kCalls);
}

TEST(SpellHitResult_ASelfCastKeepsItsSchoolImmunity)
{
    BareUnit unit;
    unit.ApplySpellImmune(0, IMMUNITY_SCHOOL, SPELL_SCHOOL_MASK_FIRE, true);
    CHECK_EQ(Count(unit, SpellHitFor::Cast, SPELL_MISS_IMMUNE), kCalls);
}

TEST(SpellHitResult_ASelfCastKeepsItsDamageImmunity)
{
    BareUnit unit;
    unit.ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_FIRE, true);
    CHECK(!unit.IsImmuneToSpell(FireSpell(), true));
    CHECK_EQ(Count(unit, SpellHitFor::Cast, SPELL_MISS_IMMUNE), kCalls);
}
