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
#include "spells/classes/mage/frost/FrostShared.h"
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
    uint32 const PIERCING_CHILL_RANK_ONE = 83156;
    uint32 const PIERCING_CHILL_RANK_TWO = 83157;

    /// 83154: Piercing Chill: for each effect, of the units the engine found within the chill's radius around the
    /// cast's explicit target, keeps the caster's count (Mage::PiercingChillCount) nearest that target, nearest first
    /// (Mage::Nearest), and makes the list final: a living target stands at no distance from itself and comes first; a
    /// dead one is no candidate and leaves the list, and the count nearest where it lies are kept. For a cast with no
    /// explicit unit target it leaves the engine's list and sets the cut after the hook to the count, so the engine
    /// keeps that many of the units it found, at random. Empties the list for a caster whose count is 0.
    SpellHandlerOutcome<void> PiercingChillTargets(SelectTargetsContext& ctx)
    {
        int32 const count = Mage::PiercingChillCount(Mage::PiercingChillTalent(ctx.caster));
        if (count <= 0)
        {
            ctx.targets.clear();
            return SpellHandlerOutcome<void>::Return();
        }
        if (!ctx.castTarget)
        {
            ctx.maxTargets = uint32(count);
            return SpellHandlerOutcome<void>::Continue();
        }
        if (!ctx.castTarget->IsAlive())
        {
            ctx.targets.remove(ctx.castTarget);
        }
        Mage::Nearest(ctx.targets, *ctx.castTarget, count);
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 const PIERCING_CHILL_CHILLS[] = { 83154 };
}

uint32 Mage::PiercingChillTalent(Unit const& mage)
{
    return mage.HasAura(PIERCING_CHILL_RANK_TWO) ? PIERCING_CHILL_RANK_TWO :
           mage.HasAura(PIERCING_CHILL_RANK_ONE) ? PIERCING_CHILL_RANK_ONE : 0;
}

int32 Mage::PiercingChillCount(uint32 talentSpell)
{
    if (talentSpell != PIERCING_CHILL_RANK_ONE && talentSpell != PIERCING_CHILL_RANK_TWO)
    {
        return 0;
    }
    return EffectValue(talentSpell, EFFECT_INDEX_0);
}

void Mage::Nearest(std::list<Unit*>& targets, WorldObject const& from, int32 count)
{
    std::vector<std::pair<float, Unit*> > byDistance;
    for (Unit* unit : targets)
    {
        if (!unit)
        {
            continue;
        }
        bool listed = false;
        for (std::pair<float, Unit*> const& kept : byDistance)
        {
            listed = listed || kept.second == unit;
        }
        if (!listed)
        {
            byDistance.push_back(std::make_pair(from.Where().DistanceTo(unit->Where()), unit));
        }
    }
    std::stable_sort(byDistance.begin(), byDistance.end(),
                     [](std::pair<float, Unit*> const& a, std::pair<float, Unit*> const& b)
                     {
                         return a.first < b.first;
                     });
    targets.clear();
    for (std::size_t i = 0; count > 0 && i < byDistance.size() && i < std::size_t(count); ++i)
    {
        targets.push_back(byDistance[i].second);
    }
}

namespace Mage
{
    uint32 RegisterPiercingChill(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<SelectTargetsHook>(registry, PIERCING_CHILL_CHILLS, &PiercingChillTargets);
    }
}
