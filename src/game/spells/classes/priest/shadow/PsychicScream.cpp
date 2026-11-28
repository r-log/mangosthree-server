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

#include "spells/classes/priest/shadow/PsychicScream.h"
#include "entities/Unit.h"

namespace
{
    /// Psychic Scream, whose third effect (index 2) is the root.
    uint32 const PSYCHIC_SCREAM[] = { 8122 };

    /// 8122: Psychic Scream: at the call before the holder is added to its target, when the holder's third effect
    /// holds no amount above 0 (Priest::PsychicScreamKeepsRoot) that effect is taken off the holder, so that the target
    /// is feared and quickened and never rooted. The amount is the one the aura was made with, after the priest's spell
    /// modifiers, so the glyph's addition keeps the root. A holder with no third effect, a target immune to the root
    /// among them, is left as it is. The call after the add does nothing.
    SpellHandlerOutcome<void> PsychicScreamApplied(AuraApplyContext& ctx)
    {
        if (!ctx.real && !Priest::PsychicScreamKeepsRoot(ctx.aura.Amount(EFFECT_INDEX_2)))
        {
            ctx.aura.RemoveEffect(EFFECT_INDEX_2);
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterPsychicScream(SpellHandlerRegistry& registry)
{
    return RegisterRanks<AuraApplyHook>(registry, PSYCHIC_SCREAM, &PsychicScreamApplied);
}

bool Priest::PsychicScreamKeepsRoot(int32 amount)
{
    return amount > 0;
}
