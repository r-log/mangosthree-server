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
#include "spells/handlers/AurasHandlers.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "entities/Unit.h"
#include "entities/Creature.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

#include <algorithm>
#include <iterator>

/// 74396: absent from the client's spell data
static SpellHandlerOutcome<void> AuraBoostMage74396(AuraBoostContext& ctx)
{
    if (!ctx.apply)
    {
        ctx.spellId1 = 44544;
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 74396: absent from the client's spell data: removes one stack of 74396
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcMage74396(DummyAuraProcContext& ctx)
{
    // Remove only single aura from stack and remove holder if its last stack
    ctx.unit->RemoveAuraHolderFromStack(74396);
    return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
}

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

    /// Every spell of SHATTER_SPELLS: Shatter multiplies the crit chance by its factor when the spell counts its target
    /// as frozen.
    SpellHandlerOutcome<void> FrostCritChance(CritChanceContext& ctx)
    {
        if (Mage::CountsAsFrozen(ctx.caster, ctx.victim, ctx.spell) != Mage::NOT_FROZEN)
        {
            ctx.critChance *= Mage::ShatterFactor(ctx.caster.HasAura(SHATTER_RANK_ONE), ctx.caster.HasAura(SHATTER_RANK_TWO));
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// Every spell of FROSTBURN_SPELLS, when it counts its target as frozen: Frostburn's mastery amount (when above 0)
    /// raises the damage done; Frostbolt also takes Shatter's effect-1 percent for the rank the caster knows, and Ice
    /// Lance its own effect-1 percent (100: double damage). Deep Freeze's damage (71757) is cast only at a target no
    /// stun holds, so it counts as frozen whenever it is cast.
    SpellHandlerOutcome<void> FrostDamageDone(DamageDoneContext& ctx)
    {
        if (Mage::CountsAsFrozen(ctx.caster, ctx.victim, ctx.spell) == Mage::NOT_FROZEN)
        {
            return SpellHandlerOutcome<void>::Continue();
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
            ctx.doneTotalMod *= (100.0f + EffectValue(ICE_LANCE_SPELL, EFFECT_INDEX_1)) / 100.0f;
        }
        return SpellHandlerOutcome<void>::Continue();
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
}

uint32 Mage::RegisterFrost(SpellHandlerRegistry& registry)
{
    static Row<AuraBoostMageSite> const boostMage[] =
    {
        { 74396, &AuraBoostMage74396 },
    };

    static Row<DummyAuraProcMageSite> const dummyAuraProcMage[] =
    {
        { 74396, &DummyAuraProcMage74396 },
    };

    uint32 rows = RegisterRows(registry, boostMage);
    rows += RegisterRows(registry, dummyAuraProcMage);
    rows += RegisterRanks<DamageDoneHook>(registry, FROSTBURN_SPELLS, &FrostDamageDone);
    rows += RegisterRanks<CritChanceHook>(registry, SHATTER_SPELLS, &FrostCritChance);
    rows += RegisterRanks<OnProcHook>(registry, DEEP_FREEZE_IMMUNITY, &DeepFreezeImmunityProc);
    rows += RegisterRanks<OnProcHook>(registry, FINGERS_OF_FROST, &FingersOfFrostProc);
    return rows;
}

Mage::FrozenKind Mage::CountsAsFrozen(Unit const& caster, Unit const& target, SpellEntry const& spell)
{
    if (target.IsFrozen())
    {
        return REALLY_FROZEN;
    }
    if (spell.ID == DEEP_FREEZE_DAMAGE)
    {
        return FINGERS;
    }
    if ((spell.ID == ICE_LANCE_SPELL || spell.ID == DEEP_FREEZE) && caster.HasAura(FINGERS_OF_FROST_BUFF))
    {
        return FINGERS;
    }
    return NOT_FROZEN;
}

float Mage::ShatterFactor(bool knowsRankOne, bool knowsRankTwo)
{
    return knowsRankTwo ? 3.0f : knowsRankOne ? 2.0f : 1.0f;
}

bool Mage::ShatterCovers(uint32 spellId)
{
    return std::find(std::begin(SHATTER_SPELLS), std::end(SHATTER_SPELLS), spellId) != std::end(SHATTER_SPELLS);
}

bool Mage::FrostburnCovers(uint32 spellId)
{
    return std::find(std::begin(FROSTBURN_SPELLS), std::end(FROSTBURN_SPELLS), spellId) != std::end(FROSTBURN_SPELLS);
}
