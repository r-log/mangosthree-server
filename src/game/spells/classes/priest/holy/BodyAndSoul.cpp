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

#include "spells/classes/priest/holy/BodyAndSoul.h"
#include "spells/classes/priest/holy/LeapOfFaith.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

namespace
{
    uint32 const BODY_AND_SOUL_RANK_1 = 64127;
    uint32 const BODY_AND_SOUL_RANK_2 = 64129;
    uint32 const BODY_AND_SOUL_SPEED_1 = 64128;
    uint32 const BODY_AND_SOUL_SPEED_2 = 65081;

    /// The client's Body and Soul dispel: its effect 0 takes one poison (dispel type 4) off its target.
    uint32 const BODY_AND_SOUL_CLEANSE = 64136;

    uint32 const POWER_WORD_SHIELD[] = { 17 };
    uint32 const LEAP_OF_FAITH[] = { 73325 };
    uint32 const CURE_DISEASE[] = { 528 };

    /// 17: Power Word: Shield: once its holder is added to a unit and its auras applied, not while the unit loads,
    /// the priest who cast it, when he holds a rank of Body and Soul (Priest::BodyAndSoulRank), casts the rank's speed
    /// (Priest::BodyAndSoulSpeed: 64128 or 65081, its effect 0 the rank's +30% or +60% run speed, for 4 seconds) at
    /// that unit, triggered, as his own cast. Nothing at the call before the stacking checks, for a shield whose caster
    /// cannot be found, or for a caster without the talent. The holder's boosts run after it.
    SpellHandlerOutcome<void> ShieldApplied(AuraApplyContext& ctx)
    {
        if (!ctx.real || !ctx.caster || ctx.target.IsLoading())
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        uint32 const speed = Priest::BodyAndSoulSpeed(Priest::BodyAndSoulRank(*ctx.caster));
        if (speed)
        {
            ctx.CastTriggeredFrom(*ctx.caster, ctx.target, speed);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 73325: Leap of Faith: once his cast is over, at the unit it was cast at when that unit is one it pulls
    /// (Priest::LeapOfFaithPulls: not the priest himself), a priest who holds a rank of Body and Soul casts the rank's
    /// speed, triggered by Leap of Faith. Nothing for a cast with no unit target, or for a caster without the talent.
    SpellHandlerOutcome<void> LeapOfFaithCast(AfterCastContext& ctx)
    {
        if (!ctx.target || !Priest::LeapOfFaithPulls(ctx.target == &ctx.caster))
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        uint32 const speed = Priest::BodyAndSoulSpeed(Priest::BodyAndSoulRank(ctx.caster));
        if (speed)
        {
            ctx.CastTriggered(*ctx.target, speed);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 528: Cure Disease: once his cast of it at himself is over, a priest who holds a rank of Body and Soul takes one
    /// poison off himself at the rank's chance (Priest::BodyAndSoulPoisonChance, Priest::BodyAndSoulCleansesPoison): he
    /// casts 64136 at himself, triggered by Cure Disease, and its dispel effect takes one poison off him, as the
    /// engine's dispel takes an aura of the dispel type its effect names. Nothing for his cast at another unit, or for
    /// a caster without the talent.
    SpellHandlerOutcome<void> CureDiseaseCast(AfterCastContext& ctx)
    {
        if (ctx.target != &ctx.caster)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 const chance = Priest::BodyAndSoulPoisonChance(Priest::BodyAndSoulRank(ctx.caster));
        if (Priest::BodyAndSoulCleansesPoison(chance, [](int32 percent) { return roll_chance_i(percent); }))
        {
            ctx.CastTriggered(ctx.caster, BODY_AND_SOUL_CLEANSE);
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterBodyAndSoul(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<AuraApplyHook>(registry, POWER_WORD_SHIELD, &ShieldApplied);
    rows += RegisterRanks<AfterCastHook>(registry, LEAP_OF_FAITH, &LeapOfFaithCast);
    rows += RegisterRanks<AfterCastHook>(registry, CURE_DISEASE, &CureDiseaseCast);
    return rows;
}

uint32 Priest::BodyAndSoulRank(Unit const& priest)
{
    if (priest.HasAura(BODY_AND_SOUL_RANK_2))
    {
        return BODY_AND_SOUL_RANK_2;
    }
    return priest.HasAura(BODY_AND_SOUL_RANK_1) ? BODY_AND_SOUL_RANK_1 : 0;
}

uint32 Priest::BodyAndSoulSpeed(uint32 rank)
{
    if (rank == BODY_AND_SOUL_RANK_1)
    {
        return BODY_AND_SOUL_SPEED_1;
    }
    return rank == BODY_AND_SOUL_RANK_2 ? BODY_AND_SOUL_SPEED_2 : 0;
}

int32 Priest::BodyAndSoulPoisonChance(uint32 rank)
{
    if (rank != BODY_AND_SOUL_RANK_1 && rank != BODY_AND_SOUL_RANK_2)
    {
        return 0;
    }
    SpellEntry const* talent = sSpellStore.LookupEntry(rank);
    return talent ? talent->CalculateSimpleValue(EFFECT_INDEX_1) : 0;
}

bool Priest::BodyAndSoulCleansesPoison(int32 chance, std::function<bool(int32)> const& roll)
{
    if (chance <= 0)
    {
        return false;
    }
    return chance >= 100 || roll(chance);
}
