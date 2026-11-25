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
#include "spells/classes/mage/frost/FrostShared.h"
#include "spells/pipeline/Spell.h"
#include "spells/data/SpellMgr.h"
#include "spells/SpellModMgr.h"
#include "entities/Unit.h"
#include "entities/Creature.h"
#include "entities/Pet.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace
{
    /// Every mage spell with a damage effect that Shatter's class mask covers (the data's mask over the mage's spells
    /// and the spells they cast).
    uint32 const SHATTER_SPELLS[] =
    {
        116, 120, 122, 133, 1449, 2120, 2136, 2948, 7268, 11113, 11366, 30451, 30455, 31661, 34913, 42208, 44425, 44457,
        44461, 44614, 71757, 82739, 84721, 88148, 92315, 95969
    };

    /// Every spell of SHATTER_SPELLS: Shatter multiplies the whole crit chance by its factor when the spell counts its
    /// target as frozen, Deep Freeze's damage (71757) and a periodic tick by the target's own state alone. The caster's
    /// flat crit-chance modifiers for the spell are added after this hook, so the chance is raised to the factor times
    /// the chance with them, less them.
    SpellHandlerOutcome<void> FrostCritChance(CritChanceContext& ctx)
    {
        if (Mage::CountsAsFrozen(ctx.caster, ctx.victim, ctx.spell, ctx.damageType,
                                 Mage::CastTimeGoingOff(ctx.caster, ctx.spell)) != Mage::NOT_FROZEN)
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
}

uint32 Mage::CastTimeGoingOff(Unit const& caster, SpellEntry const& spell)
{
    Spell const* cast = caster.FindCurrentSpellBySpellId(spell.ID);
    if (!cast || cast->getState() != SPELL_STATE_PREPARING || cast->GetCastTime() <= 0)
    {
        return 0;
    }
    return uint32(cast->GetCastTime());
}

Mage::FrozenKind Mage::CountsAsFrozen(Unit const& caster, Unit const& target, SpellEntry const& spell,
                                      DamageEffectType damageType, uint32 castTime)
{
    if (target.IsFrozen())
    {
        return REALLY_FROZEN;
    }
    if (damageType == DOT)
    {
        return NOT_FROZEN;
    }
    if (UsesFingers(caster, spell, castTime))
    {
        return FINGERS;
    }
    return NOT_FROZEN;
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

namespace Mage
{
    uint32 RegisterShatter(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<CritChanceHook>(registry, SHATTER_SPELLS, &FrostCritChance);
    }
}
