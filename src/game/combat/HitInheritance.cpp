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

#include "combat/HitInheritance.h"

#include <algorithm>

#include "combat/MeleeChances.h"
#include "Creature.h"
#include "DBCStores.h"
#include "SharedDefines.h"
#include "Unit.h"

namespace Combat
{
    namespace
    {
        // The level the caps are taken at, and the levels a boss stands above it.
        uint32 const kCapLevel = 85;
        uint32 const kCapLevelDifference = 3;

        // The skill points per level of Unit::GetMaxSkillValueForLevel.
        uint32 const kSkillPerLevel = 5;

        // The expertise that takes a boss dodge off a unit, and the hundredths of a percent each point takes off.
        int32 const kExpertiseAtCap = 26;
        int32 const kExpertiseHundredths = 25;

        // The page of the shaman melee tree among his three talent trees.
        uint32 const kShamanMeleeTreePage = 1;

        // Whether the shaman holds a mastery spell of his melee tree, which he holds only while that tree is his primary tree.
        bool UsesMeleeTree(Unit const& owner)
        {
            if (owner.getClass() != CLASS_SHAMAN)
            {
                return false;
            }
            std::vector<uint32> const* mastery = GetTalentTreeMasterySpells(GetTalentTabPages(CLASS_SHAMAN)[kShamanMeleeTreePage]);
            if (!mastery)
            {
                return false;
            }
            for (size_t i = 0; i < mastery->size(); ++i)
            {
                if (owner.HasAura(mastery->at(i)))
                {
                    return true;
                }
            }
            return false;
        }

        OwnerHitShare LiveShare(Unit const& owner, bool totem)
        {
            HitCaps const caps = BossHitCaps();
            HitKind const kind = SummonHitKind(owner.getClass(), UsesMeleeTree(owner), totem, caps);
            float percent = owner.m_modSpellHitChance;
            if (kind.source == HIT_SOURCE_MELEE)
            {
                percent = owner.m_modMeleeHitChance;
            }
            else if (kind.source == HIT_SOURCE_RANGED)
            {
                percent = owner.m_modRangedHitChance;
            }
            return InheritedShare(kind, percent, caps);
        }
    }

    HitCaps BossHitCaps()
    {
        HitCaps caps;
        caps.spell = float(100 - MagicSpellBaseHitChance(false, kCapLevel + kCapLevelDifference, kCapLevel));
        caps.melee = MeleeMissChance(true, BASE_ATTACK, false, false, false,
                                     uint16(kCapLevel * kSkillPerLevel),
                                     uint16((kCapLevel + kCapLevelDifference) * kSkillPerLevel),
                                     false, 0.0f, 0.0f, 0, 0);
        return caps;
    }

    HitKind OwnerHitKind(uint8 ownerClass, bool meleeTree, HitCaps const& caps)
    {
        HitKind kind;
        switch (ownerClass)
        {
            case CLASS_HUNTER:
                kind.source = HIT_SOURCE_RANGED;
                kind.cap = caps.melee;
                break;
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_ROGUE:
            case CLASS_DEATH_KNIGHT:
                kind.source = HIT_SOURCE_MELEE;
                kind.cap = caps.melee;
                break;
            case CLASS_SHAMAN:
                kind.source = meleeTree ? HIT_SOURCE_MELEE : HIT_SOURCE_SPELL;
                kind.cap = meleeTree ? caps.melee : caps.spell;
                break;
            default:
                kind.source = HIT_SOURCE_SPELL;
                kind.cap = caps.spell;
                break;
        }
        return kind;
    }

    HitKind SummonHitKind(uint8 ownerClass, bool meleeTree, bool totem, HitCaps const& caps)
    {
        return OwnerHitKind(ownerClass, !totem && meleeTree, caps);
    }

    float InheritedHit(float ownerPercent, float ownerCap, float petCap)
    {
        if (ownerCap <= 0.0f)
        {
            return 0.0f;
        }
        return std::min(ownerPercent, ownerCap) * petCap / ownerCap;
    }

    int32 InheritedExpertise(float ownerPercent, float ownerCap)
    {
        if (ownerCap <= 0.0f || ownerPercent <= 0.0f)
        {
            return 0;
        }
        return int32(std::min(ownerPercent, ownerCap) * float(kExpertiseAtCap * kExpertiseHundredths) / ownerCap);
    }

    OwnerHitShare InheritedShare(HitKind const& kind, float ownerPercent, HitCaps const& caps)
    {
        OwnerHitShare share;
        share.spell = InheritedHit(ownerPercent, kind.cap, caps.spell);
        share.melee = InheritedHit(ownerPercent, kind.cap, caps.melee);
        share.ranged = share.melee;
        share.expertise = InheritedExpertise(ownerPercent, kind.cap);
        return share;
    }

    OwnerHitShare ChosenShare(OwnerHitSnapshot const& snapshot, OwnerHitShare const& live)
    {
        return snapshot.taken ? snapshot.share : live;
    }

    Unit const* RatingOwner(Unit const& unit)
    {
        if (unit.GetTypeId() != TYPEID_UNIT)
        {
            return NULL;
        }
        ObjectGuid const& ownerGuid = unit.GetOwnerGuid();
        if (!ownerGuid.IsPlayer())
        {
            return NULL;
        }
        ObjectGuid const& charmerGuid = unit.GetCharmerGuid();
        if (charmerGuid && charmerGuid != ownerGuid)
        {
            return NULL;
        }
        Unit const* owner = unit.GetOwner();
        return owner && owner->GetTypeId() == TYPEID_PLAYER ? owner : NULL;
    }

    OwnerHitShare OwnerShareOf(Unit const& unit)
    {
        Creature const* creature = unit.ToCreature();
        if (!creature)
        {
            return OwnerHitShare();
        }
        OwnerHitSnapshot const& snapshot = creature->GetOwnerHitSnapshot();
        Unit const* owner = snapshot.taken || !unit.IsInWorld() ? NULL : RatingOwner(unit);
        return ChosenShare(snapshot, owner ? LiveShare(*owner, unit.IsTotem()) : OwnerHitShare());
    }

    void TakeOwnerHitSnapshot(Unit& unit)
    {
        Creature* creature = unit.ToCreature();
        Unit const* owner = creature ? RatingOwner(unit) : NULL;
        if (!owner)
        {
            return;
        }
        OwnerHitSnapshot snapshot;
        snapshot.taken = true;
        snapshot.share = LiveShare(*owner, unit.IsTotem());
        creature->SetOwnerHitSnapshot(snapshot);
    }
}
