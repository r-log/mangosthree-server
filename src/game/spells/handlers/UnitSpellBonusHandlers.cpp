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

#include "spells/handlers/UnitSpellBonusHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "Object/Unit.h"
#include "spells/auras/SpellAuras.h"

/// 45182: Cheating Death: on a player, spell damage taken scales by the aura's amount if its school mask has physical
static SpellHandlerOutcome<void> SpellDamageTakenCheatingDeath(SpellDamageTakenContext& ctx)
{
    if ((*ctx.i)->GetModifier()->m_miscvalue & SPELL_SCHOOL_MASK_NORMAL)
    {
        if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
        {
            return SpellHandlerOutcome<void>::LoopContinue();
        }

        ctx.TakenTotalMod *= ((*ctx.i)->GetModifier()->m_amount + 100.0f) / 100.0f;
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 20911, 25899: Sanctuary: spell damage taken scales by the aura's amount
static SpellHandlerOutcome<void> SpellDamageTakenSanctuary(SpellDamageTakenContext& ctx)
{
    ctx.TakenTotalMod *= ((*ctx.i)->GetModifier()->m_amount + 100.0f) / 100.0f;
    return SpellHandlerOutcome<void>::Continue();
}

/// 47580, 47581, 47582: Pain and Suffering: Shadow Word: Death's damage taken scales by the aura's amount
static SpellHandlerOutcome<void> SpellDamageTakenPainAndSuffering(SpellDamageTakenContext& ctx)
{
    // Shadow Word: Death
    if (ctx.spellProto->IsFitToFamilyMask(UI64LIT(0x0000000200000000)))
    {
        ctx.TakenTotalMod *= ((*ctx.i)->GetModifier()->m_amount + 100.0f) / 100.0f;
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 34246: Increased Lifebloom Periodic, 60779: Idol of Lush Moss: Lifebloom's flat bonus gains a seventh of the
/// aura's amount
static SpellHandlerOutcome<void> SpellHealingDoneLifebloomIdols(SpellHealingDoneContext& ctx)
{
    ctx.DoneTotal += (*ctx.i)->GetModifier()->m_amount / 7;
    return SpellHandlerOutcome<void>::Continue();
}

/// 45182: Cheating Death: on a player, melee damage taken scales by the aura's amount if its school mask has physical
static SpellHandlerOutcome<void> MeleeDamageTakenCheatingDeath(MeleeDamageTakenContext& ctx)
{
    if ((*ctx.i)->GetModifier()->m_miscvalue & SPELL_SCHOOL_MASK_NORMAL)
    {
        if (ctx.unit->GetTypeId() != TYPEID_PLAYER)
        {
            return SpellHandlerOutcome<void>::LoopContinue();
        }

        ctx.TakenPercent *= ((*ctx.i)->GetModifier()->m_amount + 100.0f) / 100.0f;
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 20911, 25899: Sanctuary: melee damage taken scales by the aura's amount
static SpellHandlerOutcome<void> MeleeDamageTakenSanctuary(MeleeDamageTakenContext& ctx)
{
    ctx.TakenPercent *= ((*ctx.i)->GetModifier()->m_amount + 100.0f) / 100.0f;
    return SpellHandlerOutcome<void>::Continue();
}

template <class Site>
struct UnitSpellBonusRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterUnitSpellBonusRows(SpellHandlerRegistry& registry, UnitSpellBonusRow<Site> const (&rows)[N])
{
    for (UnitSpellBonusRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterUnitSpellBonusHandlers(SpellHandlerRegistry& registry)
{
    static UnitSpellBonusRow<SpellDamageTakenSite> const spellDamageTaken[] =
    {
        { 45182, &SpellDamageTakenCheatingDeath },
        { 20911, &SpellDamageTakenSanctuary },
        { 25899, &SpellDamageTakenSanctuary },
        { 47580, &SpellDamageTakenPainAndSuffering },
        { 47581, &SpellDamageTakenPainAndSuffering },
        { 47582, &SpellDamageTakenPainAndSuffering },
    };

    static UnitSpellBonusRow<SpellHealingDoneSite> const spellHealingDone[] =
    {
        { 34246, &SpellHealingDoneLifebloomIdols },
        { 60779, &SpellHealingDoneLifebloomIdols },
    };

    static UnitSpellBonusRow<MeleeDamageTakenSite> const meleeDamageTaken[] =
    {
        { 45182, &MeleeDamageTakenCheatingDeath },
        { 20911, &MeleeDamageTakenSanctuary },
        { 25899, &MeleeDamageTakenSanctuary },
    };

    uint32 rows = RegisterUnitSpellBonusRows(registry, spellDamageTaken);
    rows += RegisterUnitSpellBonusRows(registry, spellHealingDone);
    rows += RegisterUnitSpellBonusRows(registry, meleeDamageTaken);
    return rows;
}
