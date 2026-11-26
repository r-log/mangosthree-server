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
#include "spells/handlers/AurasHandlers.h"
#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/pipeline/Spell.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "entities/Pet.h"
#include "DBCStores.h"
#include "Log.h"
#include "Utilities/Util.h"

/// 66: Invisibility
static SpellHandlerOutcome<void> AuraTriggerSpellMage66(AuraTriggerSpellContext& ctx)
{
    // Its trigger spell, 35009, is not in the client's spell data: the tick takes a tenth of the mage's threat from
    // every enemy's list instead and casts nothing
    ctx.target->GetHostileRefManager().addThreatPercent(Mage::InvisibilityThreatPercent(ctx.auraId));
    return SpellHandlerOutcome<void>::Return();
}

namespace
{
    /// Invisibility Speed: the mage's run speed raised by its own effect 0 value while Invisibility (32612) holds him.
    uint32 const INVISIBILITY_SPEED = 87833;
    uint32 const INVISIBILITY[] = { 32612 };
    uint32 const PET_INVISIBILITY = 96243;

    /// 32612: Invisibility: when it lands on the mage, his living pet casts its own Invisibility (96243) on itself, and
    /// when the holder's third effect holds an amount above 0 (Mage::InvisibilitySpeedFor) he casts Invisibility Speed
    /// (87833) on himself, triggered. The call before the holder is added does nothing.
    SpellHandlerOutcome<void> InvisibilityAuraApply(AuraApplyContext& ctx)
    {
        if (ctx.real)
        {
            Pet* pet = ctx.target.GetPet();
            if (pet && pet->IsAlive())
            {
                ctx.CastTriggeredFrom(*pet, *pet, PET_INVISIBILITY);
            }
            if (uint32 const speed = Mage::InvisibilitySpeedFor(ctx.aura))
            {
                ctx.CastTriggeredFrom(ctx.target, ctx.target, speed);
            }
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 32612: Invisibility: when it leaves the mage, for any reason, his pet's Invisibility (96243) and his
    /// Invisibility Speed (87833) go with it.
    SpellHandlerOutcome<void> InvisibilityAuraRemove(AuraRemoveContext& ctx)
    {
        if (Pet* pet = ctx.target.GetPet())
        {
            ctx.RemoveAura(*pet, PET_INVISIBILITY);
        }
        ctx.RemoveAura(ctx.target, INVISIBILITY_SPEED);
        return SpellHandlerOutcome<void>::Continue();
    }
}

int32 Mage::InvisibilityThreatPercent(uint32 spellId)
{
    constexpr uint32 INVISIBILITY_FADE = 66;
    constexpr int32 FADE_TICK_PERCENT = -10;
    return spellId == INVISIBILITY_FADE ? FADE_TICK_PERCENT : 0;
}

uint32 Mage::InvisibilitySpeedFor(AuraHandle const& invisibility)
{
    return invisibility.Amount(EFFECT_INDEX_2) > 0 ? INVISIBILITY_SPEED : 0;
}

namespace Mage
{
    uint32 RegisterInvisibility(SpellHandlerRegistry& registry)
    {
        static Row<AuraTriggerSpellMageSite> const triggerSpellMage[] =
        {
            { 66, &AuraTriggerSpellMage66 },
        };

        uint32 rows = RegisterRows(registry, triggerSpellMage);
        rows += RegisterRanks<AuraApplyHook>(registry, INVISIBILITY, &InvisibilityAuraApply);
        rows += RegisterRanks<AuraRemoveHook>(registry, INVISIBILITY, &InvisibilityAuraRemove);
        return rows;
    }
}
