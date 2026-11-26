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
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 1463: Mana Shield: its proc, which a chance-of-success modifier such as the Incanter's Regalia set's (37424)
    /// gives it, casts Arcane Surge (37436) on a player owner.
    ProcOutcome ManaShieldProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_MANA_SHIELD))
        {
            return ProcOutcome::Continue();
        }
        if (ctx.caster.GetTypeId() != TYPEID_PLAYER)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, ctx.caster, 37436));
    }

    uint32 const INCANTERS_ABSORPTION[] = { 44394, 44395 };
    uint32 const INCANTERS_ABSORPTION_KNOCKBACK = 86261;

    /// 1463: Mana Shield: when damage used up its last point and the mage knows a rank of Incanter's Absorption, he
    /// casts 86261, which knocks back the enemies within 6 yards of him (Mage::IncantersAbsorptionKnocksBack). A shield
    /// that is cancelled, runs out or is dispelled casts nothing. The removal's boosts run.
    SpellHandlerOutcome<void> ManaShieldAuraRemove(AuraRemoveContext& ctx)
    {
        bool knowsTalent = false;
        for (uint32 rank : INCANTERS_ABSORPTION)
        {
            knowsTalent = knowsTalent || ctx.target.HasAura(rank);
        }
        if (Mage::IncantersAbsorptionKnocksBack(ctx.mode, knowsTalent))
        {
            ctx.CastTriggered(ctx.target, INCANTERS_ABSORPTION_KNOCKBACK);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const MANA_SHIELD[] = { 1463 };
}

bool Mage::IncantersAbsorptionKnocksBack(AuraRemoveMode mode, bool knowsTalent)
{
    return mode == AURA_REMOVE_BY_SHIELD_BREAK && knowsTalent;
}

namespace Mage
{
    uint32 RegisterManaShield(SpellHandlerRegistry& registry)
    {
        uint32 rows = RegisterRanks<OnProcHook>(registry, MANA_SHIELD, &ManaShieldProc);
        rows += RegisterRanks<AuraRemoveHook>(registry, MANA_SHIELD, &ManaShieldAuraRemove);
        return rows;
    }
}
