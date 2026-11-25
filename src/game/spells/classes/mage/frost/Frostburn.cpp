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
    uint32 const FROSTBURN = 76613;
    uint32 const DEEP_FREEZE_DAMAGE = 71757;

    /// Fingers of Frost's text: "Ice Lance deals 25% more damage" while a charge is up. No field of the data holds it.
    int32 const ICE_LANCE_FINGERS_PERCENT = 25;

    /// Every mage spell with a damage effect that Frostburn's class mask covers.
    uint32 const FROSTBURN_SPELLS[] =
    {
        116, 120, 122, 133, 1449, 2120, 2136, 2948, 7268, 11113, 11366, 30451, 30455, 31661, 34913, 42208, 44425, 44461,
        44614, 71757, 88148, 92315
    };

    /// Every spell of FROSTBURN_SPELLS, when it counts its target as frozen: Frostburn's mastery amount raises the
    /// damage done; Frostbolt also takes Shatter's effect-1 percent for the rank the caster knows, and Ice Lance its
    /// own effect-1 percent (100: double damage) and ICE_LANCE_FINGERS_PERCENT more while it uses a Fingers of Frost
    /// charge. Deep Freeze's damage (71757), cast only at a target no stun holds, takes Frostburn at any target,
    /// whenever it is cast. A periodic effect's amount counts the target by its own state alone. Ice Lance and
    /// Frostfire Bolt travel to their target, and their hit is reckoned at the launch, before FrostLaunch takes the
    /// charge and Brain Freeze's buff.
    SpellHandlerOutcome<void> FrostDamageDone(DamageDoneContext& ctx)
    {
        uint32 const castTime = Mage::CastTimeGoingOff(ctx.caster, ctx.spell);
        if (ctx.spell.ID != DEEP_FREEZE_DAMAGE &&
            Mage::CountsAsFrozen(ctx.caster, ctx.victim, ctx.spell, ctx.damageType, castTime) == Mage::NOT_FROZEN)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        if (SpellAuraHolder* holder = ctx.caster.GetSpellAuraHolder(FROSTBURN))
        {
            ctx.doneTotalMod *= (100.0f + AuraHandle(*holder).Amount(EFFECT_INDEX_0)) / 100.0f;
        }
        if (ctx.spell.ID == FROSTBOLT)
        {
            uint32 const shatter = ctx.caster.HasAura(SHATTER_RANK_TWO) ? SHATTER_RANK_TWO :
                                   ctx.caster.HasAura(SHATTER_RANK_ONE) ? SHATTER_RANK_ONE : 0;
            if (shatter)
            {
                ctx.doneTotalMod *= (100.0f + Mage::EffectValue(shatter, EFFECT_INDEX_1)) / 100.0f;
            }
        }
        else if (ctx.spell.ID == ICE_LANCE_SPELL)
        {
            ctx.doneTotalMod *= Mage::IceLanceMultiplier(true, Mage::UsesFingers(ctx.caster, ctx.spell, castTime),
                                                         Mage::EffectValue(ICE_LANCE_SPELL, EFFECT_INDEX_1),
                                                         ICE_LANCE_FINGERS_PERCENT);
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

int32 Mage::EffectValue(uint32 spellId, SpellEffectIndex index)
{
    SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
    return spell ? spell->CalculateSimpleValue(index) : 0;
}

bool Mage::FrostburnCovers(uint32 spellId)
{
    return std::find(std::begin(FROSTBURN_SPELLS), std::end(FROSTBURN_SPELLS), spellId) != std::end(FROSTBURN_SPELLS);
}

namespace Mage
{
    uint32 RegisterFrostburn(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<DamageDoneHook>(registry, FROSTBURN_SPELLS, &FrostDamageDone);
    }
}
