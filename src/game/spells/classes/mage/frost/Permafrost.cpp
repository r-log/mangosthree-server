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
#include "spells/pipeline/Spell.h"
#include "spells/data/SpellMgr.h"
#include "spells/SpellModMgr.h"
#include "entities/Unit.h"
#include "entities/Creature.h"
#include "entities/Pet.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace
{
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    uint32 const PERMAFROST_PET_HEAL = 91394;

    /// 11175, 12569, 12571: Permafrost: its dummy effect's proc heals the caster's pet through 91394 for the effect's
    /// percent of the hit's damage (Mage::PermafrostHeal), cast at the pet with that heal as its value
    /// (Mage::CastProcSpell). FAILED, with nothing cast, when he has no living pet or the heal is 0.
    ProcOutcome PermafrostProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        Pet* pet = ctx.caster.GetPet();
        if (!pet || !pet->IsAlive())
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const heal = Mage::PermafrostHeal(ctx.damage, ctx.triggerAmount);
        if (heal <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const basePoints[MAX_EFFECT_INDEX] = { heal, 0, 0 };
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, *pet, PERMAFROST_PET_HEAL, basePoints));
    }

    uint32 const PERMAFROST[] = { 11175, 12569, 12571 };
}

int32 Mage::PermafrostHeal(uint32 damage, int32 percent)
{
    if (percent <= 0)
    {
        return 0;
    }
    int64 const heal = int64(damage) * percent / 100;
    return heal > std::numeric_limits<int32>::max() ? std::numeric_limits<int32>::max() : int32(heal);
}

namespace Mage
{
    uint32 RegisterPermafrost(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<OnProcHook>(registry, PERMAFROST, &PermafrostProc);
    }
}
