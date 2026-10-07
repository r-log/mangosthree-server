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

#include "spells/handlers/AuraPeriodicHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "WorldHandlers/SpellAuras.h"
#include "WorldHandlers/GridMap.h"

/// 28200: Ascendance (Talisman of Ascendance trinket): six charges at apply
static SpellHandlerOutcome<void> AuraProcTriggerAscendance(AuraProcTriggerContext& ctx)
{
        // some spell have charges by functionality not have its in spell data
    if (ctx.apply)
    {
        ctx.aura->GetHolder()->SetAuraCharges(6);
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 50720: Vigilance: the threat transfer cast at apply, reset at remove
static SpellHandlerOutcome<void> AuraProcTriggerVigilance(AuraProcTriggerContext& ctx)
{
    if (ctx.apply)
    {
        if (Unit* caster = ctx.aura->GetCaster())
        {
            ctx.target->CastSpell(caster, 59665, true);
        }
    }
    else
    {
        ctx.target->GetHostileRefManager().ResetThreatRedirection();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraProcTriggerDefault(AuraProcTriggerContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 66: Invisibility: the fade spell at expiry
static SpellHandlerOutcome<void> AuraPeriodicTriggerInvisibility(AuraPeriodicTriggerContext& ctx)
{
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
    {
        ctx.target->CastSpell(ctx.target, 32612, true, NULL, ctx.aura);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 42783: Wrath of the Astromancer: the next effect's spell at expiry
static SpellHandlerOutcome<void> AuraPeriodicTriggerWrathOfTheAstromancer(AuraPeriodicTriggerContext& ctx)
{
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE && ctx.aura->GetEffIndex() + 1 < MAX_EFFECT_INDEX)
    {
        ctx.target->CastSpell(ctx.target, ctx.aura->GetSpellProto()->CalculateSimpleValue(SpellEffectIndex(ctx.aura->GetEffIndex() + 1)), true);
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 46221: Animal Blood: the blood pool at the water surface
static SpellHandlerOutcome<void> AuraPeriodicTriggerAnimalBlood(AuraPeriodicTriggerContext& ctx)
{
    if (ctx.target->GetTypeId() == TYPEID_PLAYER && ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_DEFAULT && ctx.target->IsInWater())
    {
        // No water level means no surface to pool blood on -- the aura is
        // only reachable while IsInWater, so this is a race, not a normal path.
        if (const auto surface = ctx.target->GetTerrain()->GetWaterLevel(
                ctx.target->Where().X(), ctx.target->Where().Y(), ctx.target->Where().Z()))
        {
            // Spawn Blood Pool
            ctx.target->CastSpell(ctx.target->Where().X(), ctx.target->Where().Y(), *surface, 63471, true);
        }
    }

    return SpellHandlerOutcome<void>::Return();
}

/// 51912: Ultra-Advanced Proto-Typical Shortening Blaster: the triggered spell at expiry
static SpellHandlerOutcome<void> AuraPeriodicTriggerShorteningBlaster(AuraPeriodicTriggerContext& ctx)
{
    if (ctx.aura->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
    {
        if (Unit* pCaster = ctx.aura->GetCaster())
        {
            pCaster->CastSpell(ctx.target, ctx.aura->GetSpellEffect()->EffectTriggerSpell, true, NULL, ctx.aura);
        }
    }

    return SpellHandlerOutcome<void>::Return();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraPeriodicTriggerDefault(AuraPeriodicTriggerContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 54833: Glyph of Innervate: the amount from the caster's base mana
static SpellHandlerOutcome<void> AuraEnergizeGlyphOfInnervate(AuraPeriodicEnergizeContext& ctx)
{
    if (Unit* caster = ctx.aura->GetCaster())
    {
        ctx.aura->GetModifier()->m_amount = int32(caster->GetCreateMana() * ctx.aura->GetBasePoints() / (200 * ctx.aura->GetAuraMaxTicks()));
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 29166: Innervate: the glyph's spell, and the amount from the caster's base mana
static SpellHandlerOutcome<void> AuraEnergizeInnervate(AuraPeriodicEnergizeContext& ctx)
{
    if (Unit* caster = ctx.aura->GetCaster())
    {
        // Glyph of Innervate
        if (caster->HasAura(54832))
        {
            caster->CastSpell(caster, 54833, true, NULL, ctx.aura);
        }

        ctx.aura->GetModifier()->m_amount = int32(caster->GetCreateMana() * ctx.aura->GetBasePoints() / (100 * ctx.aura->GetAuraMaxTicks()));
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 48391: Owlkin Frenzy: two percent of the target's base mana
static SpellHandlerOutcome<void> AuraEnergizeOwlkinFrenzy(AuraPeriodicEnergizeContext& ctx)
{
    ctx.aura->GetModifier()->m_amount = ctx.target->GetCreateMana() * 2 / 100;
    return SpellHandlerOutcome<void>::Continue();
}

/// 57669, 61782: Replenishment: 0.2 percent of the target's maximum mana
static SpellHandlerOutcome<void> AuraEnergizeReplenishment(AuraPeriodicEnergizeContext& ctx)
{
    ctx.aura->GetModifier()->m_amount = ctx.target->GetMaxPower(POWER_MANA) * 2 / 1000;
    return SpellHandlerOutcome<void>::Continue();
}

/// every other id: nothing
static SpellHandlerOutcome<void> AuraEnergizeDefault(AuraPeriodicEnergizeContext& /*ctx*/)
{
    return SpellHandlerOutcome<void>::Continue();
}

/// 31666: Master of Subtlety: the visible duration at apply, the buff removed at remove
static SpellHandlerOutcome<void> AuraPeriodicDummyMasterOfSubtlety(AuraPeriodicDummyRogueContext& ctx)
{
    // Master of Subtlety
    if (ctx.apply)
    {
        // for make duration visible
        if (SpellAuraHolder* holder = ctx.target->GetSpellAuraHolder(31665))
        {
            holder->SetAuraMaxDuration(ctx.aura->GetHolder()->GetAuraDuration());
            holder->RefreshHolder();
        }
    }
    else
    {
        ctx.target->RemoveAurasDueToSpell(31665);
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 12976 ... 59465: the amount as a flat bonus to maximum and current health
static SpellHandlerOutcome<void> AuraIncreaseHealthStat(AuraIncreaseHealthContext& ctx)
{
    if (ctx.real)
    {
        if (ctx.apply)
        {
            ctx.target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, float(ctx.aura->GetModifier()->m_amount), ctx.apply);
            ctx.target->ModifyHealth(ctx.aura->GetModifier()->m_amount);
        }
        else
        {
            if (int32(ctx.target->GetHealth()) > ctx.aura->GetModifier()->m_amount)
            {
                ctx.target->ModifyHealth(-ctx.aura->GetModifier()->m_amount);
            }
            else
            {
                ctx.target->SetHealth(1);
            }
            ctx.target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, float(ctx.aura->GetModifier()->m_amount), ctx.apply);
        }
    }
    return SpellHandlerOutcome<void>::Return();
// generic case
}

/// 54443, 55233, 61254: the amount as a percentage of maximum health, then the flat bonus
static SpellHandlerOutcome<void> AuraIncreaseHealthPercent(AuraIncreaseHealthContext& ctx)
{
// Special case with temporary increase max/current health
        // Cases where we need to manually calculate the amount for the spell (by percentage)
        // recalculate to full amount at apply for proper remove
    if (ctx.real && ctx.apply)
    {
        ctx.aura->GetModifier()->m_amount = ctx.target->GetMaxHealth() * ctx.aura->GetModifier()->m_amount / 100;
    }
    // no break here

    // Cases where m_amount already has the correct value (spells cast with CastCustomSpell or absolute values)

    return AuraIncreaseHealthStat(ctx);
}

/// every other id: the amount as a flat bonus to maximum health
static SpellHandlerOutcome<void> AuraIncreaseHealthDefault(AuraIncreaseHealthContext& ctx)
{
    ctx.target->HandleStatModifier(UNIT_MOD_HEALTH, TOTAL_VALUE, float(ctx.aura->GetModifier()->m_amount), ctx.apply);
    return SpellHandlerOutcome<void>::Continue();
}

template <class Site>
struct AuraPeriodicRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterAuraPeriodicRows(SpellHandlerRegistry& registry, AuraPeriodicRow<Site> const (&rows)[N])
{
    for (AuraPeriodicRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterAuraPeriodicHandlers(SpellHandlerRegistry& registry)
{
    static AuraPeriodicRow<AuraProcTriggerSite> const procTrigger[] =
    {
        { 28200, &AuraProcTriggerAscendance },
        { 50720, &AuraProcTriggerVigilance },
    };

    static AuraPeriodicRow<AuraPeriodicTriggerSite> const triggerRemove[] =
    {
        { 66, &AuraPeriodicTriggerInvisibility },
        { 42783, &AuraPeriodicTriggerWrathOfTheAstromancer },
        { 46221, &AuraPeriodicTriggerAnimalBlood },
        { 51912, &AuraPeriodicTriggerShorteningBlaster },
    };

    static AuraPeriodicRow<AuraPeriodicEnergizeSite> const energize[] =
    {
        { 54833, &AuraEnergizeGlyphOfInnervate },
        { 29166, &AuraEnergizeInnervate },
        { 48391, &AuraEnergizeOwlkinFrenzy },
        { 57669, &AuraEnergizeReplenishment },
        { 61782, &AuraEnergizeReplenishment },
    };

    static AuraPeriodicRow<AuraPeriodicDummyRogueSite> const rogue[] =
    {
        { 31666, &AuraPeriodicDummyMasterOfSubtlety },
    };

    static AuraPeriodicRow<AuraIncreaseHealthSite> const increaseHealth[] =
    {
        { 54443, &AuraIncreaseHealthPercent },
        { 55233, &AuraIncreaseHealthPercent },
        { 61254, &AuraIncreaseHealthPercent },
        { 12976, &AuraIncreaseHealthStat },
        { 28726, &AuraIncreaseHealthStat },
        { 31616, &AuraIncreaseHealthStat },
        { 34511, &AuraIncreaseHealthStat },
        { 44055, &AuraIncreaseHealthStat },
        { 55915, &AuraIncreaseHealthStat },
        { 55917, &AuraIncreaseHealthStat },
        { 67596, &AuraIncreaseHealthStat },
        { 50322, &AuraIncreaseHealthStat },
        { 53479, &AuraIncreaseHealthStat },
        { 59465, &AuraIncreaseHealthStat },
    };

    uint32 rows = RegisterAuraPeriodicRows(registry, procTrigger);
    rows += RegisterAuraPeriodicRows(registry, triggerRemove);
    rows += RegisterAuraPeriodicRows(registry, energize);
    rows += RegisterAuraPeriodicRows(registry, rogue);
    rows += RegisterAuraPeriodicRows(registry, increaseHealth);
    registry.RegisterDefault<AuraProcTriggerSite>(&AuraProcTriggerDefault);
    registry.RegisterDefault<AuraPeriodicTriggerSite>(&AuraPeriodicTriggerDefault);
    registry.RegisterDefault<AuraPeriodicEnergizeSite>(&AuraEnergizeDefault);
    registry.RegisterDefault<AuraIncreaseHealthSite>(&AuraIncreaseHealthDefault);
    return rows + 4;
}
