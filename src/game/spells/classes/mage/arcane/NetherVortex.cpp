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
#include "spells/classes/mage/arcane/ArcaneShared.h"
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
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    uint32 const SLOW = 31589;

    /// 86181, 86209: Nether Vortex: after the talent's own proc chance, a hit of the mage's Arcane Blast that deals
    /// damage casts Slow (31589) at its target, triggered, so even beyond Slow's range, while none of his Slow is on
    /// any unit (Mage::NetherVortexSlows, Mage::HasSlowOut). Every other event casts nothing.
    ProcOutcome NetherVortexProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        uint32 const procSpellId = ctx.procSpell ? ctx.procSpell->ID : 0;
        if (!ctx.victim || ctx.victim == &ctx.caster ||
            !Mage::NetherVortexSlows(procSpellId, ctx.procEx, ctx.damage, Mage::HasSlowOut(ctx.caster)))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, *ctx.victim, SLOW));
    }

    uint32 const NETHER_VORTEX[] = { 86181, 86209 };
}

bool Mage::HasSlowOut(Unit const& mage)
{
    Unit::TrackedAuraTargetMap const& tracked = mage.GetTrackedAuraTargets(TRACK_AURA_TYPE_SINGLE_TARGET);
    for (Unit::TrackedAuraTargetMap::const_iterator i = tracked.begin(); i != tracked.end(); ++i)
    {
        if (i->first && i->first->ID == SLOW)
        {
            return true;
        }
    }
    return false;
}

bool Mage::NetherVortexSlows(uint32 procSpellId, uint32 procEx, uint32 damage, bool slowOut)
{
    return procSpellId == ARCANE_BLAST && (procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)) != 0 && damage > 0 &&
           !slowOut;
}

namespace Mage
{
    uint32 RegisterNetherVortex(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<OnProcHook>(registry, NETHER_VORTEX, &NetherVortexProc);
    }
}
