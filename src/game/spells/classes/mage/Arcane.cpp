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
#include "entities/Unit.h"

namespace
{
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 29441, 29444: Magic Absorption: a mana user gets back the aura's percent of his maximum mana through 29442.
    ProcOutcome MagicAbsorptionProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (ctx.caster.GetPowerType() != POWER_MANA)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        int32 const basePoints[MAX_EFFECT_INDEX] = { int32(ctx.triggerAmount * ctx.caster.GetMaxPower(POWER_MANA) / 100), 0, 0 };
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, ctx.caster, 29442, basePoints));
    }

    uint32 const MAGIC_ABSORPTION[] = { 29441, 29444 };
}

uint32 Mage::RegisterArcane(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<OnProcHook>(registry, MAGIC_ABSORPTION, &MagicAbsorptionProc);
    return rows;
}
