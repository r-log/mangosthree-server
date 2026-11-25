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

    /// 31571, 31572: Arcane Potency: casts the buff of the aura's rank (GetArcanePotencyProcSpell) on the owner.
    ProcOutcome ArcanePotencyProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (!ctx.procSpell)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        uint32 const buff = GetArcanePotencyProcSpell(ctx.spell.ID);
        if (!buff)
        {
            sLog.outError("Arcane Potency: spell %u has no buff of its rank", ctx.spell.ID);
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, ctx.caster, buff));
    }

    /// 57529, 57531: Arcane Potency's buffs: an event of a spell that neither deals nor triggers damage
    /// (Mage::DealsOrTriggersDamage) is dropped and spends no charge, such as the cast of Arcane Blast's buff (36032),
    /// which Arcane Blast triggers and whose class mask the buffs' crit modifier names. Every other event goes on to
    /// the engine's proc handling, which spends a charge at the end of a cast of a spell the modifier names.
    SpellHandlerOutcome<bool> ArcanePotencyBuffCheckProc(CheckProcContext& ctx)
    {
        SpellEntry const* const spell = ctx.procSpell;
        if (spell && !Mage::DealsOrTriggersDamage(*spell))
        {
            return SpellHandlerOutcome<bool>::Return(false);
        }
        return SpellHandlerOutcome<bool>::Continue();
    }

    uint32 const ARCANE_POTENCY[] = { 31571, 31572 };
    uint32 const ARCANE_POTENCY_BUFFS[] = { 57529, 57531 };
    /// Arcane Potency's buffs, the second rank's first: the order Mage::UseArcanePotencyCharge looks for them in.
    uint32 const ARCANE_POTENCY_BUFFS_BY_RANK[] = { 57531, 57529 };
}

uint32 Mage::UseArcanePotencyCharge(Unit& mage)
{
    for (uint32 buff : ARCANE_POTENCY_BUFFS_BY_RANK)
    {
        SpellAuraHolder* holder = mage.GetSpellAuraHolder(buff);
        if (!holder || !AuraHandle(*holder).Charges())
        {
            continue;
        }
        if (AuraHandle(*holder).DropCharge())
        {
            mage.RemoveAurasDueToSpell(buff);
        }
        return buff;
    }
    return 0;
}

namespace Mage
{
    uint32 RegisterArcanePotency(SpellHandlerRegistry& registry)
    {
        uint32 rows = RegisterRanks<OnProcHook>(registry, ARCANE_POTENCY, &ArcanePotencyProc);
        rows += RegisterRanks<CheckProcHook>(registry, ARCANE_POTENCY_BUFFS, &ArcanePotencyBuffCheckProc);
        return rows;
    }
}
