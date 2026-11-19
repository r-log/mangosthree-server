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

/// Tests of what a pet's stances and its new spells start from: the Assist switch's time (Pets::AssistSwitchDue,
/// Pets::PET_ASSIST_SWITCH_MS), the unit a Defensive or Assist pet attacks for its owner (Pets::OwnerAttacker) and the
/// owner's notice with no pet (Pets::NotifyOwnerAttacked), the autocast state a pet's new spell starts in
/// (Spells::AutocastsByDefault, Spells::PetSpellAutocastDefault) on Spell.dbc rows shaped as the client's Waterbolt
/// (31707) and Freeze (33395) and three others under test-only ids, put into the effect and cooldown stores, which of
/// the owner's harmful spells count as the owner's attack for an Assist pet (Spells::BreaksOnDamage,
/// Spells::CountsAsOwnerAttack) on rows shaped as ten of the client's spells under test-only ids, put into the effect,
/// aura options and interrupts stores, and the stance buttons of the pet bar (CharmInfo::InitPetActionBar,
/// CharmInfo::LoadPetActionBar).

#include "TestHarness.h"
#include "ai/PetAssist.h"
#include "spells/SpellScriptQueries.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "spells/data/SpellAuraDefines.h"

#include <cstddef>
#include <string>

namespace
{
    // Test-only spell ids, no other test's.
    uint32 const ROW_BOLT = 9430001;         // Waterbolt's shape: school damage at an enemy, no cooldown
    uint32 const ROW_FREEZE = 9430002;       // Freeze's shape: school damage and a root on enemies around a point, 25 s
    uint32 const ROW_CATEGORY = 9430003;     // the bolt's shape with a category cooldown alone
    uint32 const ROW_HEAL = 9430004;         // a heal on the caster, no cooldown
    uint32 const ROW_ROOT = 9430005;         // a root at an enemy, no damage, no cooldown

    // SpellCooldowns.dbc rows: { CategoryRecoveryTime, RecoveryTime, StartRecoveryTime }.
    uint32 const COOLDOWNS_FREEZE = 9430002;
    uint32 const COOLDOWNS_CATEGORY = 9430003;
    SpellCooldownsEntry s_cdFreeze = { 0, 25000, 0 };
    SpellCooldownsEntry s_cdCategory = { 8000, 0, 0 };

    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    SpellRowStorage s_rows[5] = {};

    // A Spell.dbc row with only its id and its cooldowns row, as the loader makes it; it is not in the spell store.
    SpellEntry const& Row(std::size_t slot, uint32 id, uint32 cooldownsId)
    {
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_rows[slot].bytes);
        row->ID = id;
        row->CooldownsID = cooldownsId;
        return *row;
    }

    // The rows' effects and cooldowns, put into their stores once for the rest of the run.
    void AddRows()
    {
        struct Effect
        {
            uint32 spellId;
            uint32 effectIndex;
            uint32 effect;
            uint32 aura;
            uint32 target;
        };
        static Effect const effects[] =
        {
            { ROW_BOLT, 0, SPELL_EFFECT_SCHOOL_DAMAGE, 0, TARGET_CHAIN_DAMAGE },
            { ROW_FREEZE, 0, SPELL_EFFECT_SCHOOL_DAMAGE, 0, TARGET_ALL_ENEMY_IN_AREA_INSTANT },
            { ROW_FREEZE, 1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_ROOT, TARGET_ALL_ENEMY_IN_AREA_INSTANT },
            { ROW_CATEGORY, 0, SPELL_EFFECT_SCHOOL_DAMAGE, 0, TARGET_CHAIN_DAMAGE },
            { ROW_HEAL, 0, SPELL_EFFECT_HEAL, 0, TARGET_SELF },
            { ROW_ROOT, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_ROOT, TARGET_CHAIN_DAMAGE },
        };
        static SpellEffectEntry rows[sizeof(effects) / sizeof(effects[0])];
        static bool added = false;
        if (added)
        {
            return;
        }
        added = true;
        for (std::size_t i = 0; i < sizeof(effects) / sizeof(effects[0]); ++i)
        {
            rows[i] = SpellEffectEntry();
            rows[i].SpellID = effects[i].spellId;
            rows[i].EffectIndex = effects[i].effectIndex;
            rows[i].Effect = effects[i].effect;
            rows[i].EffectAura = effects[i].aura;
            rows[i].ImplicitTarget_0 = effects[i].target;
            AddSpellEffectEntry(&rows[i]);
        }
        sSpellCooldownsStore.SetEntry(COOLDOWNS_FREEZE, &s_cdFreeze);
        sSpellCooldownsStore.SetEntry(COOLDOWNS_CATEGORY, &s_cdCategory);
    }

    // Test-only spell ids, each shaped as the client's spell it names: its effects and auras, its SpellAuraOptions.dbc
    // row and its SpellInterrupts.dbc row, each of the two stored under the spell's own test id.
    uint32 const ROW_POLYMORPH = 9430006;    // Polymorph 118: a confuse and a transform
    uint32 const ROW_HEX = 9430007;          // Hex 51514: a transform, a pacify-silence and a screen effect
    uint32 const ROW_FEAR = 9430008;         // Fear 5782: a fear, a speed aura and a root
    uint32 const ROW_RING_FREEZE = 9430009;  // Ring of Frost's freeze 82691: a stun
    uint32 const ROW_FROST_NOVA = 9430010;   // Frost Nova 122: school damage and a root
    uint32 const ROW_FROSTBOLT = 9430011;    // Frostbolt 116: a slow, school damage and a healing aura
    uint32 const ROW_SLOW = 9430012;         // Slow 31589: a slow and two haste auras; no options or interrupts row
    uint32 const ROW_COUNTERSPELL = 9430013; // Counterspell 2139: a cast interrupt; no options row
    uint32 const ROW_DEEP_FREEZE = 9430014;  // Deep Freeze 44572: a stun; no options row
    uint32 const ROW_BLIND = 9430015;        // Blind 2094: a slow and a confuse; no options row

    // SpellAuraOptions.dbc rows: { CumulativeAura, ProcChance, ProcCharges, ProcTypeMask }.
    SpellAuraOptionsEntry s_optPolymorph = { 0, 100, 0, 0x000AAAA8 };
    SpellAuraOptionsEntry s_optTakenHits = { 0, 100, 0, 0x000A22A8 };   // Hex's, Fear's and Frost Nova's
    SpellAuraOptionsEntry s_optRingFreeze = { 0, 101, 0, 0x000A2AA8 };

    // SpellInterrupts.dbc rows: { AuraInterruptFlags, ChannelInterruptFlags, InterruptFlags }.
    SpellInterruptsEntry s_intPolymorph = { 0x08080000, 0, 0x0F };     // Polymorph's and Hex's
    SpellInterruptsEntry s_intFear = { 0x08480000, 0, 0x0F };
    SpellInterruptsEntry s_intRingFreeze = { 0x08480000, 0, 0 };
    SpellInterruptsEntry s_intFrostNova = { 0x00480000, 0, 0x08 };
    SpellInterruptsEntry s_intCast = { 0, 0, 0x0F };                   // Frostbolt's and Deep Freeze's
    SpellInterruptsEntry s_intCounterspell = { 0, 0, 0x08 };
    SpellInterruptsEntry s_intBlind = { 0x00480002, 0, 0 };

    struct AttackShape
    {
        uint32 id;
        SpellAuraOptionsEntry* options;
        SpellInterruptsEntry* interrupts;
    };

    AttackShape const s_attackShapes[] =
    {
        { ROW_POLYMORPH, &s_optPolymorph, &s_intPolymorph },
        { ROW_HEX, &s_optTakenHits, &s_intPolymorph },
        { ROW_FEAR, &s_optTakenHits, &s_intFear },
        { ROW_RING_FREEZE, &s_optRingFreeze, &s_intRingFreeze },
        { ROW_FROST_NOVA, &s_optTakenHits, &s_intFrostNova },
        { ROW_FROSTBOLT, NULL, &s_intCast },
        { ROW_SLOW, NULL, NULL },
        { ROW_COUNTERSPELL, NULL, &s_intCounterspell },
        { ROW_DEEP_FREEZE, NULL, &s_intCast },
        { ROW_BLIND, NULL, &s_intBlind },
    };
    std::size_t const ATTACK_SHAPES = sizeof(s_attackShapes) / sizeof(s_attackShapes[0]);

    SpellRowStorage s_attackRows[ATTACK_SHAPES] = {};

    // The rows' effects, aura options and interrupts, put into their stores once for the rest of the run.
    void AddAttackRows()
    {
        struct Effect
        {
            uint32 spellId;
            uint32 effectIndex;
            uint32 effect;
            uint32 aura;
        };
        static Effect const effects[] =
        {
            { ROW_POLYMORPH, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_CONFUSE },
            { ROW_POLYMORPH, 1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_TRANSFORM },
            { ROW_HEX, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_TRANSFORM },
            { ROW_HEX, 1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_PACIFY_SILENCE },
            { ROW_HEX, 2, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_SCREEN_EFFECT },
            { ROW_FEAR, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_FEAR },
            { ROW_FEAR, 1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_INCREASE_SPEED },
            { ROW_FEAR, 2, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_ROOT },
            { ROW_RING_FREEZE, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_STUN },
            { ROW_FROST_NOVA, 0, SPELL_EFFECT_SCHOOL_DAMAGE, 0 },
            { ROW_FROST_NOVA, 1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_ROOT },
            { ROW_FROSTBOLT, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_DECREASE_SPEED },
            { ROW_FROSTBOLT, 1, SPELL_EFFECT_SCHOOL_DAMAGE, 0 },
            { ROW_FROSTBOLT, 2, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_HEALING_PCT },
            { ROW_SLOW, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_DECREASE_SPEED },
            { ROW_SLOW, 1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_HASTE_RANGED },
            { ROW_SLOW, 2, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_HASTE_SPELLS },
            { ROW_COUNTERSPELL, 0, SPELL_EFFECT_INTERRUPT_CAST, 0 },
            { ROW_DEEP_FREEZE, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_STUN },
            { ROW_BLIND, 0, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_DECREASE_SPEED },
            { ROW_BLIND, 1, SPELL_EFFECT_APPLY_AURA, SPELL_AURA_MOD_CONFUSE },
        };
        static SpellEffectEntry rows[sizeof(effects) / sizeof(effects[0])];
        static bool added = false;
        if (added)
        {
            return;
        }
        added = true;
        for (std::size_t i = 0; i < sizeof(effects) / sizeof(effects[0]); ++i)
        {
            rows[i] = SpellEffectEntry();
            rows[i].SpellID = effects[i].spellId;
            rows[i].EffectIndex = effects[i].effectIndex;
            rows[i].Effect = effects[i].effect;
            rows[i].EffectAura = effects[i].aura;
            rows[i].ImplicitTarget_0 = TARGET_CHAIN_DAMAGE;
            AddSpellEffectEntry(&rows[i]);
        }
        for (std::size_t i = 0; i < ATTACK_SHAPES; ++i)
        {
            if (s_attackShapes[i].options)
            {
                sSpellAuraOptionsStore.SetEntry(s_attackShapes[i].id, s_attackShapes[i].options);
            }
            if (s_attackShapes[i].interrupts)
            {
                sSpellInterruptsStore.SetEntry(s_attackShapes[i].id, s_attackShapes[i].interrupts);
            }
        }
    }

    // The Spell.dbc row of shape `id` with its id and its aura options and interrupts rows, as the loader makes it; it
    // is not in the spell store.
    SpellEntry const& AttackRow(uint32 id)
    {
        std::size_t slot = 0;
        while (slot + 1 < ATTACK_SHAPES && s_attackShapes[slot].id != id)
        {
            ++slot;
        }
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_attackRows[slot].bytes);
        row->ID = s_attackShapes[slot].id;
        row->AuraOptionsID = s_attackShapes[slot].options ? s_attackShapes[slot].id : 0;
        row->InterruptsID = s_attackShapes[slot].interrupts ? s_attackShapes[slot].id : 0;
        return *row;
    }

    // A creature with its update fields allocated and no world, map, template, pet or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            ~BareCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    // The pet bar's stance slots as "<action>:<type>", slots 7 to 9.
    std::string Stances(CharmInfo const& info)
    {
        std::string out;
        for (uint8 slot = ACTION_BAR_INDEX_PET_SPELL_END; slot < ACTION_BAR_INDEX_END; ++slot)
        {
            UnitActionBarEntry const* entry = info.GetActionBarEntry(slot);
            out += (out.empty() ? "" : " ") + std::to_string(entry->GetAction()) + ":" +
                   std::to_string(uint32(entry->GetType()));
        }
        return out;
    }
}

TEST(PetAssist_TheSwitchIsDueAtThreeSeconds)
{
    CHECK_EQ(Pets::PET_ASSIST_SWITCH_MS, uint32(3000));
    CHECK(!Pets::AssistSwitchDue(0, Pets::PET_ASSIST_SWITCH_MS));
    CHECK(!Pets::AssistSwitchDue(2999, Pets::PET_ASSIST_SWITCH_MS));
    CHECK(Pets::AssistSwitchDue(3000, Pets::PET_ASSIST_SWITCH_MS));
    CHECK(Pets::AssistSwitchDue(3001, Pets::PET_ASSIST_SWITCH_MS));
}

TEST(PetAssist_AnOwnerNobodyAttacksGivesNoUnitAndAnOwnerWithNoPetTellsNobody)
{
    BareCreature owner;
    BareCreature victim;
    CHECK(owner.getAttackers().empty());
    CHECK(Pets::OwnerAttacker(owner) == NULL);
    CHECK(owner.GetPet() == NULL);
    Pets::NotifyOwnerAttacked(owner, victim);
    CHECK(owner.GetPet() == NULL);
}

TEST(PetAutocast_OnlyAHarmfulDamagingSpellWithoutACooldownStartsOn)
{
    CHECK(Spells::AutocastsByDefault(true, true, 0, 0));
    CHECK(!Spells::AutocastsByDefault(false, true, 0, 0));
    CHECK(!Spells::AutocastsByDefault(true, false, 0, 0));
    CHECK(!Spells::AutocastsByDefault(true, true, 25000, 0));
    CHECK(!Spells::AutocastsByDefault(true, true, 0, 8000));
}

TEST(PetAutocast_WaterboltsShapeStartsOnAndFreezesShapeOff)
{
    AddRows();
    CHECK(sSpellCooldownsStore.LookupEntry(COOLDOWNS_FREEZE) != NULL);

    // Waterbolt's shape: harmful, damaging, no cooldown.
    CHECK_EQ(uint32(Spells::PetSpellAutocastDefault(Row(0, ROW_BOLT, 0))), uint32(ACT_ENABLED));

    // Freeze's shape: harmful and damaging, with a 25 second cooldown.
    CHECK_EQ(uint32(Spells::PetSpellAutocastDefault(Row(1, ROW_FREEZE, COOLDOWNS_FREEZE))), uint32(ACT_DISABLED));

    // A category cooldown alone keeps it off too; so does a heal, and a root that deals no damage.
    CHECK_EQ(uint32(Spells::PetSpellAutocastDefault(Row(2, ROW_CATEGORY, COOLDOWNS_CATEGORY))), uint32(ACT_DISABLED));
    CHECK_EQ(uint32(Spells::PetSpellAutocastDefault(Row(3, ROW_HEAL, 0))), uint32(ACT_DISABLED));
    CHECK_EQ(uint32(Spells::PetSpellAutocastDefault(Row(4, ROW_ROOT, 0))), uint32(ACT_DISABLED));
}

TEST(PetAssist_CrowdControlThatBreaksOnDamageIsNotTheOwnersAttack)
{
    AddAttackRows();
    CHECK(sSpellAuraOptionsStore.LookupEntry(ROW_POLYMORPH) != NULL);
    CHECK(sSpellInterruptsStore.LookupEntry(ROW_POLYMORPH) != NULL);

    // Polymorph's confuse, Hex's pacify-silence, Fear's fear and root, and Ring of Frost's freeze, a stun whose flags
    // hold the second damage flag alone, proc on a hit that deals damage, which takes them off; none deals damage.
    uint32 const procRemoved[] = { ROW_POLYMORPH, ROW_HEX, ROW_FEAR, ROW_RING_FREEZE };
    for (std::size_t i = 0; i < sizeof(procRemoved) / sizeof(procRemoved[0]); ++i)
    {
        SpellEntry const& spell = AttackRow(procRemoved[i]);
        CHECK_EQ(spell.ID, procRemoved[i]);
        CHECK(!Spells::DealsDamage(spell));
        CHECK(Spells::BreaksOnDamage(spell));
        CHECK(!Spells::CountsAsOwnerAttack(spell));
    }

    // Blind's confuse has no proc flags; its aura interrupt flags hold the damage flag.
    CHECK(Spells::BreaksOnDamage(AttackRow(ROW_BLIND)));
    CHECK(!Spells::CountsAsOwnerAttack(AttackRow(ROW_BLIND)));
}

TEST(PetAssist_DamageAndCrowdControlThatHoldsAreTheOwnersAttack)
{
    AddAttackRows();

    // Frost Nova's root breaks on damage, but the spell deals damage.
    CHECK(Spells::DealsDamage(AttackRow(ROW_FROST_NOVA)));
    CHECK(Spells::BreaksOnDamage(AttackRow(ROW_FROST_NOVA)));
    CHECK(Spells::CountsAsOwnerAttack(AttackRow(ROW_FROST_NOVA)));

    // Frostbolt deals damage, and its slow does not break on damage.
    CHECK(Spells::DealsDamage(AttackRow(ROW_FROSTBOLT)));
    CHECK(!Spells::BreaksOnDamage(AttackRow(ROW_FROSTBOLT)));
    CHECK(Spells::CountsAsOwnerAttack(AttackRow(ROW_FROSTBOLT)));

    // Slow, Counterspell and Deep Freeze deal no damage and hold no aura that damage takes off: Deep Freeze's stun has
    // neither proc flags nor the damage flag.
    uint32 const holding[] = { ROW_SLOW, ROW_COUNTERSPELL, ROW_DEEP_FREEZE };
    for (std::size_t i = 0; i < sizeof(holding) / sizeof(holding[0]); ++i)
    {
        SpellEntry const& spell = AttackRow(holding[i]);
        CHECK_EQ(spell.ID, holding[i]);
        CHECK(!Spells::DealsDamage(spell));
        CHECK(!Spells::BreaksOnDamage(spell));
        CHECK(Spells::CountsAsOwnerAttack(spell));
    }
}

TEST(PetBar_TheStanceButtonsAreAssistDefensiveAndPassive)
{
    // A fresh bar: Assist (3), Defensive (1) and Passive (0), each a reaction button (6).
    CharmInfo fresh(NULL);
    fresh.InitPetActionBar();
    CHECK_STR(Stances(fresh), "3:6 1:6 0:6");

    // A saved bar's Aggressive button (2) loads as the Assist button; its other stance buttons stay.
    CharmInfo saved(NULL);
    saved.LoadPetActionBar("7 2 7 1 7 0 129 0 129 0 129 0 129 0 6 2 6 1 6 0");
    CHECK_STR(Stances(saved), "3:6 1:6 0:6");

    // A saved bar of another length is not read: the fresh bar stays.
    CharmInfo shortBar(NULL);
    shortBar.LoadPetActionBar("7 2 7 1");
    CHECK_STR(Stances(shortBar), "3:6 1:6 0:6");
}
