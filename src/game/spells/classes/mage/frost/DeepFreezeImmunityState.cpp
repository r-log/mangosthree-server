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

    /// 71761: Deep Freeze Immunity State, which every mage holds: its proc casts its trigger spell (71757, Deep Freeze's
    /// damage) only at a creature whose template is immune to stuns, through Mage::CastProcTriggerSpell.
    ProcOutcome DeepFreezeImmunityProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        Creature* victimCreature = ctx.victim ? ctx.victim->ToCreature() : NULL;
        if (!victimCreature || (victimCreature->GetCreatureInfo()->MechanicImmuneMask & (1 << (MECHANIC_STUN - 1))) == 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, Mage::EffectTriggerSpell(ctx), NULL));
    }

    uint32 const DEEP_FREEZE_IMMUNITY[] = { 71761 };
}

namespace Mage
{
    uint32 RegisterDeepFreezeImmunityState(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<OnProcHook>(registry, DEEP_FREEZE_IMMUNITY, &DeepFreezeImmunityProc);
    }
}
