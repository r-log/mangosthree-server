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

#include "spells/classes/mage/Mage.h"
#include "spells/SpellModMgr.h"
#include "entities/Unit.h"
#include "entities/Creature.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

#include <algorithm>
#include <iterator>

namespace
{
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    uint32 const SHATTER_RANK_ONE = 11170;
    uint32 const SHATTER_RANK_TWO = 12982;
    uint32 const FROSTBURN = 76613;
    uint32 const FINGERS_OF_FROST_BUFF = 44544;
    uint32 const FROSTBOLT = 116;
    uint32 const ICE_LANCE_SPELL = 30455;
    uint32 const DEEP_FREEZE = 44572;
    uint32 const DEEP_FREEZE_DAMAGE = 71757;
    uint32 const FROSTFIRE_BOLT = 44614;
    uint32 const BRAIN_FREEZE_BUFF = 57761;
    uint32 const GLYPH_OF_FROSTFIRE = 61205;
    uint32 const FROST_ARMOR_CHILL = 7321;
    uint32 const FROSTFIRE_ORB_RANK_ONE_HIT = 95969;

    /// Fingers of Frost's text: "Ice Lance deals 25% more damage" while a charge is up. No field of the data holds it.
    int32 const ICE_LANCE_FINGERS_PERCENT = 25;

    /// The bits of Fingers of Frost's script word: a spell that travels to its target and left with a charge.
    uint32 const FINGERS_MARK_ICE_LANCE = 0x1;
    uint32 const FINGERS_MARK_FROSTFIRE_BOLT = 0x2;

    /// Every mage spell with a damage effect that Shatter's class mask covers (the data's mask over the mage's spells
    /// and the spells they cast).
    uint32 const SHATTER_SPELLS[] =
    {
        116, 120, 122, 133, 1449, 2120, 2136, 2948, 7268, 11113, 11366, 30451, 30455, 31661, 34913, 42208, 44425, 44457,
        44461, 44614, 71757, 82739, 84721, 88148, 92315, 95969
    };

    /// Every mage spell with a damage effect that Frostburn's class mask covers.
    uint32 const FROSTBURN_SPELLS[] =
    {
        116, 120, 122, 133, 1449, 2120, 2136, 2948, 7268, 11113, 11366, 30451, 30455, 31661, 34913, 42208, 44425, 44461,
        44614, 71757, 88148, 92315
    };

    /// The value of effect `index` of spell `spellId` in the spell store; 0 when the store lacks the spell.
    int32 EffectValue(uint32 spellId, SpellEffectIndex index)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
        return spell ? spell->CalculateSimpleValue(index) : 0;
    }

    /// Every spell of SHATTER_SPELLS: Shatter multiplies the whole crit chance by its factor when the spell counts its
    /// target as frozen. The caster's flat crit-chance modifiers for the spell are added after this hook, so the chance
    /// is raised to the factor times the chance with them, less them.
    SpellHandlerOutcome<void> FrostCritChance(CritChanceContext& ctx)
    {
        if (Mage::CountsAsFrozen(ctx.caster, ctx.victim, ctx.spell) != Mage::NOT_FROZEN)
        {
            float flatMods = 0.0f;
            if (SpellModMgr* mods = ctx.caster.GetSpellMods())
            {
                mods->ApplySpellMod(ctx.spell.ID, SPELLMOD_CRITICAL_CHANCE, flatMods);
            }
            float const factor = Mage::ShatterFactor(ctx.caster.HasAura(SHATTER_RANK_ONE), ctx.caster.HasAura(SHATTER_RANK_TWO));
            ctx.critChance = Mage::ShatterCritChance(ctx.critChance, factor, flatMods);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// Every spell of FROSTBURN_SPELLS, when it counts its target as frozen: Frostburn's mastery amount (when above 0)
    /// raises the damage done; Frostbolt also takes Shatter's effect-1 percent for the rank the caster knows, and Ice
    /// Lance its own effect-1 percent (100: double damage) and ICE_LANCE_FINGERS_PERCENT more while it uses a Fingers
    /// of Frost charge. Deep Freeze's damage (71757) is cast only at a target no stun holds, so it counts as frozen
    /// whenever it is cast. Ice Lance and Frostfire Bolt travel to their target, and their damage is reckoned at the
    /// launch: one that uses a charge marks it on the Fingers of Frost holder there, for FingersUse to take when the
    /// cast is over.
    SpellHandlerOutcome<void> FrostDamageDone(DamageDoneContext& ctx)
    {
        if (Mage::CountsAsFrozen(ctx.caster, ctx.victim, ctx.spell) == Mage::NOT_FROZEN)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        bool const fingers = Mage::UsesFingers(ctx.caster, ctx.spell);
        if (fingers)
        {
            if (SpellAuraHolder* holder = ctx.caster.GetSpellAuraHolder(FINGERS_OF_FROST_BUFF))
            {
                AuraHandle handle(*holder);
                Mage::MarkFingers(handle, ctx.spell.ID);
            }
        }
        if (SpellAuraHolder* holder = ctx.caster.GetSpellAuraHolder(FROSTBURN))
        {
            int32 const amount = AuraHandle(*holder).Amount(EFFECT_INDEX_0);
            if (amount > 0)
            {
                ctx.doneTotalMod *= (100.0f + amount) / 100.0f;
            }
        }
        if (ctx.spell.ID == FROSTBOLT)
        {
            uint32 const shatter = ctx.caster.HasAura(SHATTER_RANK_TWO) ? SHATTER_RANK_TWO :
                                   ctx.caster.HasAura(SHATTER_RANK_ONE) ? SHATTER_RANK_ONE : 0;
            if (shatter)
            {
                ctx.doneTotalMod *= (100.0f + EffectValue(shatter, EFFECT_INDEX_1)) / 100.0f;
            }
        }
        else if (ctx.spell.ID == ICE_LANCE_SPELL)
        {
            ctx.doneTotalMod *= Mage::IceLanceMultiplier(true, fingers, EffectValue(ICE_LANCE_SPELL, EFFECT_INDEX_1),
                                                         ICE_LANCE_FINGERS_PERCENT);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 30455, 44572, 44614: once the cast is over, takes the Fingers of Frost charge it used when it landed on an
    /// enemy; one that missed takes none. Ice Lance and a Frostfire Bolt find the mark FrostDamageDone left at their
    /// launch, and their cast is over once they land; Deep Freeze lands as it is cast, so it uses the charge the caster
    /// holds.
    SpellHandlerOutcome<void> FingersUse(AfterCastContext& ctx)
    {
        SpellAuraHolder* holder = ctx.caster.GetSpellAuraHolder(FINGERS_OF_FROST_BUFF);
        if (!holder)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        AuraHandle fingers(*holder);
        bool const used = Mage::FingersMarkOf(ctx.spell.ID) ? Mage::TakeFingersMark(fingers, ctx.spell.ID) : true;
        if (used && ctx.enemiesHit > 0)
        {
            Mage::UseFingersCharge(ctx.caster);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 44543, 44545, 83074: Fingers of Frost refuses the procs of the sources FingersRefusesSource names.
    SpellHandlerOutcome<bool> FingersCheckProc(CheckProcContext& ctx)
    {
        if (ctx.procSpell && Mage::FingersRefusesSource(ctx.procSpell->ID, ctx.owner.HasAura(GLYPH_OF_FROSTFIRE)))
        {
            return SpellHandlerOutcome<bool>::Return(false);
        }
        return SpellHandlerOutcome<bool>::Continue();
    }

    /// 71761: Deep Freeze Immunity State, which every mage holds: its proc casts its trigger spell (71757, Deep Freeze's
    /// damage) only at a creature whose template is immune to stuns, through Mage::CastProcTriggerSpell.
    ProcOutcome DeepFreezeImmunityProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        Creature* victimCreature = ctx.victim ? ctx.victim->ToCreature() : NULL;
        if (!victimCreature || (victimCreature->GetCreatureInfo()->MechanicImmuneMask & (1 << (MECHANIC_STUN - 1))) == 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, Mage::EffectTriggerSpell(ctx), NULL));
    }

    /// 44543, 44545, 83074: Fingers of Frost: its proc rolls the aura's amount as a percent chance, and on a pass casts
    /// its trigger spell (44544) through Mage::CastProcTriggerSpell.
    ProcOutcome FingersOfFrostProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        if (!roll_chance_i(ctx.triggerAmount))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, Mage::EffectTriggerSpell(ctx), NULL));
    }

    uint32 const DEEP_FREEZE_IMMUNITY[] = { 71761 };
    uint32 const FINGERS_OF_FROST[] = { 44543, 44545, 83074 };
    uint32 const FINGERS_OF_FROST_USERS[] = { 30455, 44572, 44614 };
}

uint32 Mage::RegisterFrost(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<DamageDoneHook>(registry, FROSTBURN_SPELLS, &FrostDamageDone);
    rows += RegisterRanks<CritChanceHook>(registry, SHATTER_SPELLS, &FrostCritChance);
    rows += RegisterRanks<OnProcHook>(registry, DEEP_FREEZE_IMMUNITY, &DeepFreezeImmunityProc);
    rows += RegisterRanks<OnProcHook>(registry, FINGERS_OF_FROST, &FingersOfFrostProc);
    rows += RegisterRanks<CheckProcHook>(registry, FINGERS_OF_FROST, &FingersCheckProc);
    rows += RegisterRanks<AfterCastHook>(registry, FINGERS_OF_FROST_USERS, &FingersUse);
    return rows;
}

Mage::FrozenKind Mage::CountsAsFrozen(Unit const& caster, Unit const& target, SpellEntry const& spell)
{
    if (target.IsFrozen())
    {
        return REALLY_FROZEN;
    }
    if (spell.ID == DEEP_FREEZE_DAMAGE || UsesFingers(caster, spell))
    {
        return FINGERS;
    }
    return NOT_FROZEN;
}

bool Mage::UsesFingers(Unit const& caster, SpellEntry const& spell)
{
    if (!caster.HasAura(FINGERS_OF_FROST_BUFF))
    {
        return false;
    }
    return spell.ID == ICE_LANCE_SPELL || spell.ID == DEEP_FREEZE ||
           (spell.ID == FROSTFIRE_BOLT && caster.HasAura(BRAIN_FREEZE_BUFF));
}

bool Mage::RefusesChillSource(uint32 procSpellId)
{
    return procSpellId == FROST_ARMOR_CHILL || procSpellId == FROSTFIRE_ORB_RANK_ONE_HIT;
}

bool Mage::FingersRefusesSource(uint32 procSpellId, bool glyphOfFrostfire)
{
    return RefusesChillSource(procSpellId) || (procSpellId == FROSTFIRE_BOLT && glyphOfFrostfire);
}

uint32 Mage::FingersMarkOf(uint32 spellId)
{
    return spellId == ICE_LANCE_SPELL ? FINGERS_MARK_ICE_LANCE :
           spellId == FROSTFIRE_BOLT ? FINGERS_MARK_FROSTFIRE_BOLT : 0;
}

void Mage::MarkFingers(AuraHandle& fingers, uint32 spellId)
{
    fingers.ScriptWord(EFFECT_INDEX_0) |= int32(FingersMarkOf(spellId));
}

bool Mage::TakeFingersMark(AuraHandle& fingers, uint32 spellId)
{
    int32& word = fingers.ScriptWord(EFFECT_INDEX_0);
    int32 const mark = int32(FingersMarkOf(spellId));
    bool const marked = (word & mark) != 0;
    word &= ~mark;
    return marked;
}

bool Mage::UseFingersCharge(Unit& mage)
{
    if (!mage.HasAura(FINGERS_OF_FROST_BUFF))
    {
        return false;
    }
    mage.RemoveAuraHolderFromStack(FINGERS_OF_FROST_BUFF);
    return true;
}

void Mage::GiveFingers(Unit& mage, uint32 charges)
{
    for (uint32 i = 0; i < charges; ++i)
    {
        mage.CastSpell(&mage, FINGERS_OF_FROST_BUFF, true);
    }
}

float Mage::IceLanceMultiplier(bool countsAsFrozen, bool fingersUp, int32 doubledPercent, int32 fingersPercent)
{
    float const frozen = countsAsFrozen ? (100.0f + doubledPercent) / 100.0f : 1.0f;
    return fingersUp ? frozen * (100.0f + fingersPercent) / 100.0f : frozen;
}

float Mage::ShatterFactor(bool knowsRankOne, bool knowsRankTwo)
{
    return knowsRankTwo ? 3.0f : knowsRankOne ? 2.0f : 1.0f;
}

float Mage::ShatterCritChance(float chance, float factor, float flatMods)
{
    return (chance + flatMods) * factor - flatMods;
}

bool Mage::ShatterCovers(uint32 spellId)
{
    return std::find(std::begin(SHATTER_SPELLS), std::end(SHATTER_SPELLS), spellId) != std::end(SHATTER_SPELLS);
}

bool Mage::FrostburnCovers(uint32 spellId)
{
    return std::find(std::begin(FROSTBURN_SPELLS), std::end(FROSTBURN_SPELLS), spellId) != std::end(FROSTBURN_SPELLS);
}
