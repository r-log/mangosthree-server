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

#include "spells/classes/priest/LeapOfFaith.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "DBCStores.h"

namespace
{
    uint32 const LEAP_OF_FAITH_SPELL[] = { 73325 };
    uint32 const LEAP_OF_FAITH_JUMP = 92832;
    uint32 const LEAP_OF_FAITH_AURA = 92572;
    uint32 const LEAP_OF_FAITH_FRONT = 92833;

    /// The yards ahead of the priest where Leap of Faith's jump lands: the radius of 92833's effect 0 in the client's
    /// data (radius index 16, 1 yard), whose second target is a point in front of its caster; 0 when the store lacks
    /// the spell or the effect.
    float LandingDistance()
    {
        SpellEntry const* front = sSpellStore.LookupEntry(LEAP_OF_FAITH_FRONT);
        SpellEffectEntry const* effect = front ? front->GetSpellEffect(EFFECT_INDEX_0) : NULL;
        return effect ? GetSpellRadius(sSpellRadiusStore.LookupEntry(effect->GetRadiusIndex())) : 0.0f;
    }

    /// 73325: Leap of Faith's effect 0, a dummy: the unit it lands on, when it is not the priest himself
    /// (Priest::LeapOfFaithPulls), casts, triggered by this spell, 92832 at the point LandingDistance() yards ahead of
    /// the priest along his facing, at his height (Priest::LeapOfFaithLanding). 92832's effect 0, a SPELL_EFFECT_JUMP2
    /// at the cast's destination (Spell::EffectJump), moves the unit there at the spell tables' speed
    /// (Spells::JumpSpeed, 50 yd/s) on the arc of natural gravity, facing along its path, and sends its spline. The
    /// unit then casts, triggered, 92572, the 1.5 second dummy aura 73325's effect 1 names, at itself. Return(): the
    /// effect is the script's; nothing is cast at the priest himself.
    SpellHandlerOutcome<void> LeapOfFaithEffect(DummyEffectContext& ctx)
    {
        if (ctx.effIndex != EFFECT_INDEX_0 || !ctx.target)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        Unit& target = *ctx.target;
        if (Priest::LeapOfFaithPulls(&target == &ctx.caster))
        {
            Geometry::Vector3 const landing = Priest::LeapOfFaithLanding(ctx.caster.Where(), LandingDistance());
            target.CastSpell(landing.x, landing.y, landing.z, LEAP_OF_FAITH_JUMP, true, NULL, NULL, ObjectGuid(),
                             &ctx.spell);
            target.CastSpell(&target, LEAP_OF_FAITH_AURA, true, NULL, NULL, ObjectGuid(), &ctx.spell);
        }
        return SpellHandlerOutcome<void>::Return();
    }
}

uint32 Priest::RegisterLeapOfFaith(SpellHandlerRegistry& registry)
{
    return RegisterRanks<DummyEffectHook>(registry, LEAP_OF_FAITH_SPELL, &LeapOfFaithEffect);
}

bool Priest::LeapOfFaithPulls(bool self)
{
    return !self;
}

Geometry::Vector3 Priest::LeapOfFaithLanding(Geometry::Placement const& priest, float distance)
{
    return priest.PointAhead(distance > 0.0f ? distance : 0.0f);
}
