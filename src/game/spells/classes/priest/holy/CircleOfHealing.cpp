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

#include "spells/classes/priest/holy/CircleOfHealing.h"
#include "entities/Unit.h"

namespace
{
    /// 34861: the priest's Circle of Healing; 49306: an older rank of it, which a creature casts at itself. Both are
    /// the ones the client's data holds of the priest's family with Circle of Healing's visual (8253) and a friendly
    /// area target mode.
    uint32 const CIRCLE_OF_HEALING[] = { 34861, 49306 };

    /// The units the client's text of 34861 names ("Heals up to $?s55675[6][5] friendly party or raid members").
    uint32 const CIRCLE_OF_HEALING_UNITS = 5;

    /// Glyph of Circle of Healing: its effect 0, a dummy aura, holds the units it adds (1).
    uint32 const GLYPH_OF_CIRCLE_OF_HEALING = 55675;

    /// 34861, 49306: Circle of Healing: at its area target mode (TARGET_ALL_FRIENDLY_UNITS_IN_AREA), the units the
    /// engine's area fill found give way to the engine's raid or party pick by health with the pets
    /// (SelectTargetsContext::RaidOrPartyByHealthWithPets: the members of the caster's raid or party and their pets
    /// within the effect's radius of the cast's unit target, or of the caster without one, the living ones of the
    /// lowest health percent first), as many as Priest::CircleOfHealingUnits names for the caster's glyph. A creature's
    /// cast, with no player behind it, takes itself and its pet. Its other target mode, the destination's, fills no
    /// unit and is left as it is. The caster's removal and the cut to the spell's target limit run after it.
    SpellHandlerOutcome<void> CircleOfHealingTargets(SelectTargetsContext& ctx)
    {
        if (ctx.targetMode == TARGET_ALL_FRIENDLY_UNITS_IN_AREA)
        {
            ctx.RaidOrPartyByHealthWithPets(Priest::CircleOfHealingUnits(Priest::CircleOfHealingGlyph(ctx.caster)));
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterCircleOfHealing(SpellHandlerRegistry& registry)
{
    return RegisterRanks<SelectTargetsHook>(registry, CIRCLE_OF_HEALING, &CircleOfHealingTargets);
}

uint32 Priest::CircleOfHealingUnits(int32 glyph)
{
    return CIRCLE_OF_HEALING_UNITS + (glyph > 0 ? uint32(glyph) : 0);
}

int32 Priest::CircleOfHealingGlyph(Unit const& caster)
{
    SpellAuraHolder* glyph = caster.GetSpellAuraHolder(GLYPH_OF_CIRCLE_OF_HEALING);
    return glyph ? AuraHandle(*glyph).Amount(EFFECT_INDEX_0) : 0;
}
