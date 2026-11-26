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

#include "spells/classes/priest/holy/SpiritOfRedemption.h"
#include "entities/Unit.h"

namespace
{
    /// Spirit of Redemption's form: 15 seconds, cast by Unit::DealDamage at the priest's killing blow.
    uint32 const SPIRIT_OF_REDEMPTION = 27827;
    uint32 const SPIRIT_OF_REDEMPTION_FORM[] = { SPIRIT_OF_REDEMPTION };

    /// The form's first boost: the root, the unattackable flag and the free Holy spells. The form takes it off before
    /// the boost whose removal kills the priest.
    uint32 const SPIRIT_ROOT[] = { 27792 };

    /// The immunity to the damage of every school the form carries in the sniff.
    uint32 const SPIRIT_IMMUNITY = 62371;

    /// The stun the sniff shows right before the death at the form's end.
    uint32 const SPIRIT_END_STUN = 99755;

    /// 27827: the form: once its holder is added to the priest and its auras are applied, which cast 27792 and 27795,
    /// he casts 62371 at himself, triggered, for the form's caster. Nothing at the call before the stacking checks. The
    /// holder's boosts run after it.
    SpellHandlerOutcome<void> FormApplied(AuraApplyContext& ctx)
    {
        if (ctx.real)
        {
            ctx.CastTriggered(ctx.target, SPIRIT_IMMUNITY);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 27792: the form's root: once it is off the priest, which the removal of the form does first of its boosts for
    /// the end, a dispel, a cancel or a recast of the form (the removal of 27795 that follows kills him,
    /// Aura::HandleSpiritOfRedemption), he casts 99755 at himself, triggered, when he is alive and in the world and the
    /// form is no longer on him (Priest::SpiritOfRedemptionStuns): the stun comes before his death. 62371 is taken off
    /// him first: the form's own removal runs FormRemoved only after its boosts, and while 62371 stands a stun, a
    /// harmful spell, misses him as immune (Unit::SpellHitResult). Nothing when the root alone is taken off while the
    /// form stands, for a priest with no health left, whom another death killed while the form stood, or for one who
    /// stands off the map. The holder's boosts run after it.
    SpellHandlerOutcome<void> RootRemoved(AuraRemoveContext& ctx)
    {
        if (Priest::SpiritOfRedemptionStuns(ctx.target.IsAlive(), ctx.target.GetHealth(), ctx.target.IsInWorld(),
                                            ctx.target.HasAura(SPIRIT_OF_REDEMPTION)))
        {
            ctx.RemoveAura(ctx.target, SPIRIT_IMMUNITY);
            ctx.CastTriggered(ctx.target, SPIRIT_END_STUN);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 27827: the form: once it is off the priest, whatever took it off, 62371 goes with it, when RootRemoved has not
    /// taken it already: it is a passive, and a death takes no passive off. The form's removal runs this after the removal of its boosts, his death among them.
    /// The holder's boosts run after it.
    SpellHandlerOutcome<void> FormRemoved(AuraRemoveContext& ctx)
    {
        ctx.RemoveAura(ctx.target, SPIRIT_IMMUNITY);
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterSpiritOfRedemption(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<AuraApplyHook>(registry, SPIRIT_OF_REDEMPTION_FORM, &FormApplied);
    rows += RegisterRanks<AuraRemoveHook>(registry, SPIRIT_ROOT, &RootRemoved);
    rows += RegisterRanks<AuraRemoveHook>(registry, SPIRIT_OF_REDEMPTION_FORM, &FormRemoved);
    return rows;
}

bool Priest::SpiritOfRedemptionStuns(bool alive, uint32 health, bool inWorld, bool holdsForm)
{
    return alive && health > 0 && inWorld && !holdsForm;
}
