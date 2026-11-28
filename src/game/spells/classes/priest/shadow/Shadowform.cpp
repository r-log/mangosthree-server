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

#include "spells/classes/priest/shadow/Shadowform.h"
#include "entities/Unit.h"

namespace
{
    /// Shadowform.
    uint32 const SHADOWFORM[] = { 15473 };

    /// The look Shadowform shows beside it, and the look it shows a priest who holds Glyph of Shadow.
    uint32 const SHADOWFORM_LOOK = 107903;
    uint32 const SHADOWFORM_GLYPHED_LOOK = 107904;

    /// Glyph of Shadow, a passive aura whose text is "Alters the appearance of your Shadowform."
    uint32 const GLYPH_OF_SHADOW = 107906;

    /// 15473: Shadowform: once the holder is on the priest (`real`), the priest casts the look beside it at himself,
    /// triggered, the glyphed one when he holds Glyph of Shadow (Priest::ShadowformLook). Nothing at the call before the
    /// add. The look shares the form's icon, so the no-stack rule must let the pair stand
    /// (Spells::IsShadowformWithItsLook).
    SpellHandlerOutcome<void> ShadowformApplied(AuraApplyContext& ctx)
    {
        if (ctx.real && !ctx.target.IsLoading())
        {
            ctx.CastTriggered(ctx.target, Priest::ShadowformLook(ctx.target.HasAura(GLYPH_OF_SHADOW)));
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 15473: Shadowform: when the form leaves the priest, for any reason, both looks go with it. The form's boosts
    /// (Mind Quickening, 49868) are removed inside its shapeshift effect's removal, before this row runs.
    SpellHandlerOutcome<void> ShadowformRemoved(AuraRemoveContext& ctx)
    {
        ctx.RemoveAura(ctx.target, SHADOWFORM_LOOK);
        ctx.RemoveAura(ctx.target, SHADOWFORM_GLYPHED_LOOK);
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterShadowform(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<AuraApplyHook>(registry, SHADOWFORM, &ShadowformApplied);
    rows += RegisterRanks<AuraRemoveHook>(registry, SHADOWFORM, &ShadowformRemoved);
    return rows;
}

uint32 Priest::ShadowformLook(bool glyphed)
{
    return glyphed ? SHADOWFORM_GLYPHED_LOOK : SHADOWFORM_LOOK;
}
