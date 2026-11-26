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
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "spells/SpellScriptQueries.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "Log.h"

#include <algorithm>
#include <iterator>

namespace
{
    uint32 const ARCANE_MISSILES_BUFF = 79683;
    uint32 const ARCANE_MISSILES_AURASTATE = 79808;

    /// Casts Arcane Missiles Aurastate (79808) on the unit the context's aura is on, or takes it off him, as
    /// Mage::ArcaneMissilesAurastateStep answers for what he holds of it and of Arcane Missiles! (79683).
    template <class Context>
    void PairArcaneMissilesAurastate(Context& ctx)
    {
        switch (Mage::ArcaneMissilesAurastateStep(ctx.target.HasAura(ARCANE_MISSILES_BUFF),
                                                  ctx.target.HasAura(ARCANE_MISSILES_AURASTATE)))
        {
            case Mage::AURASTATE_APPLY:
                ctx.CastTriggered(ctx.target, ARCANE_MISSILES_AURASTATE);
                break;
            case Mage::AURASTATE_REMOVE:
                ctx.RemoveAura(ctx.target, ARCANE_MISSILES_AURASTATE);
                break;
            case Mage::AURASTATE_KEEP:
                break;
        }
    }

    /// 79683: Arcane Missiles!: at its apply the mage gets Arcane Missiles Aurastate (79808), which Arcane Missiles
    /// (5143) asks for on its caster. The call before the holder is added does nothing.
    SpellHandlerOutcome<void> ArcaneMissilesBuffApply(AuraApplyContext& ctx)
    {
        if (ctx.real)
        {
            PairArcaneMissilesAurastate(ctx);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 79683: Arcane Missiles!: at its removal (used up, run out, cancelled or at death) Arcane Missiles
    /// Aurastate (79808) is taken off the mage.
    SpellHandlerOutcome<void> ArcaneMissilesBuffRemove(AuraRemoveContext& ctx)
    {
        PairArcaneMissilesAurastate(ctx);
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 5143: Arcane Missiles: the channel's holder on the mage himself, applied at the cast, takes Arcane Missiles!
    /// (79683) off him: the cast uses it up. The channel's holder on its target, and the call before the holder is
    /// added, do nothing here.
    SpellHandlerOutcome<void> ArcaneMissilesChannelApply(AuraApplyContext& ctx)
    {
        if (ctx.real && ctx.caster == &ctx.target)
        {
            ctx.RemoveAura(ctx.target, ARCANE_MISSILES_BUFF);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const ARCANE_MISSILES_BUFFS[] = { ARCANE_MISSILES_BUFF };
    uint32 const ARCANE_MISSILES_CHANNEL[] = { 5143 };
}

Mage::AurastateStep Mage::ArcaneMissilesAurastateStep(bool holdsBuff, bool holdsAurastate)
{
    if (holdsBuff == holdsAurastate)
    {
        return AURASTATE_KEEP;
    }
    return holdsBuff ? AURASTATE_APPLY : AURASTATE_REMOVE;
}

namespace Mage
{
    uint32 RegisterArcaneMissiles(SpellHandlerRegistry& registry)
    {
        uint32 rows = RegisterRanks<AuraApplyHook>(registry, ARCANE_MISSILES_BUFFS, &ArcaneMissilesBuffApply);
        rows += RegisterRanks<AuraRemoveHook>(registry, ARCANE_MISSILES_BUFFS, &ArcaneMissilesBuffRemove);
        rows += RegisterRanks<AuraApplyHook>(registry, ARCANE_MISSILES_CHANNEL, &ArcaneMissilesChannelApply);
        return rows;
    }
}
