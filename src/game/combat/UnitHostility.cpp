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

#include "Unit.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "GridMap.h"
#include "ObjectGuid.h"
#include "QuestDef.h"
#include "Player.h"
#include "Formulas.h"
#include "Util.h"
#include "GridNotifiersImpl.h"
#include "movement/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "GameTime.h"

#include <math.h>
#include <stdarg.h>

/**
 * @brief Checks whether this unit is hostile to another unit.
 *
 * @param unit The other unit.
 * @return True if the units are hostile; otherwise, false.
 */
bool Unit::IsHostileTo(Unit const* unit) const
{
    // always non-hostile to self
    if (unit == this)
    {
        return false;
    }

    // always non-hostile to GM in GM mode
    if (unit->isGameMaster())
    {
        return false;
    }

    // always hostile to enemy
    if (getVictim() == unit || unit->getVictim() == this)
    {
        return true;
    }

    // test pet/charm masters instead pers/charmeds
    Unit const* testerOwner = GetCharmerOrOwner();
    Unit const* targetOwner = unit->GetCharmerOrOwner();

    // always hostile to owner's enemy
    if (testerOwner && (testerOwner->getVictim() == unit || unit->getVictim() == testerOwner))
    {
        return true;
    }

    // always hostile to enemy owner
    if (targetOwner && (getVictim() == targetOwner || targetOwner->getVictim() == this))
    {
        return true;
    }

    // always hostile to owner of owner's enemy
    if (testerOwner && targetOwner && (testerOwner->getVictim() == targetOwner || targetOwner->getVictim() == testerOwner))
    {
        return true;
    }

    Unit const* tester = testerOwner ? testerOwner : this;
    Unit const* target = targetOwner ? targetOwner : unit;

    // always non-hostile to target with common owner, or to owner/pet
    if (tester == target)
    {
        return false;
    }

    // special cases (Duel, etc)
    if (tester->GetTypeId() == TYPEID_PLAYER && target->GetTypeId() == TYPEID_PLAYER)
    {
        Player const* pTester = (Player const*)tester;
        Player const* pTarget = (Player const*)target;

        // Duel
        if (pTester->IsInDuelWith(pTarget))
        {
            return true;
        }

        // Group
        if (pTester->GetGroup() && pTester->GetGroup() == pTarget->GetGroup())
        {
            return false;
        }

        // Sanctuary
        if (pTarget->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY) && pTester->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY))
        {
            return false;
        }

        // PvP FFA state
        if (pTester->IsFFAPvP() && pTarget->IsFFAPvP())
        {
            return true;
        }

        //= PvP states
        // Green/Blue (can't attack)
        if (pTester->GetTeam() == pTarget->GetTeam())
        {
            return false;
        }

        // Red (can attack) if true, Blue/Yellow (can't attack) in another case
        return pTester->IsPvP() && pTarget->IsPvP();
    }

    // faction base cases
    FactionTemplateEntry const* tester_faction = tester->getFactionTemplateEntry();
    FactionTemplateEntry const* target_faction = target->getFactionTemplateEntry();
    if (!tester_faction || !target_faction)
    {
        return false;
    }

    if (target->isAttackingPlayer() && tester->IsContestedGuard())
    {
        return true;
    }

    // PvC forced reaction and reputation case
    if (tester->GetTypeId() == TYPEID_PLAYER)
    {
        if (target_faction->Faction)
        {
            // forced reaction
            if (ReputationRank const* force = ((Player*)tester)->GetReputationMgr().GetForcedRankIfAny(target_faction))
            {
                return *force <= REP_HOSTILE;
            }

            // if faction have reputation then hostile state for tester at 100% dependent from at_war state
            if (FactionEntry const* raw_target_faction = sFactionStore.LookupEntry(target_faction->Faction))
                if (FactionState const* factionState = ((Player*)tester)->GetReputationMgr().GetState(raw_target_faction))
                {
                    return (factionState->Flags & FACTION_FLAG_AT_WAR);
                }
        }
    }
    // CvP forced reaction and reputation case
    else if (target->GetTypeId() == TYPEID_PLAYER)
    {
        if (tester_faction->Faction)
        {
            // forced reaction
            if (ReputationRank const* force = ((Player*)target)->GetReputationMgr().GetForcedRankIfAny(tester_faction))
            {
                return *force <= REP_HOSTILE;
            }

            // apply reputation state
            FactionEntry const* raw_tester_faction = sFactionStore.LookupEntry(tester_faction->Faction);
            if (raw_tester_faction && raw_tester_faction->ReputationIndex >= 0)
            {
                return ((Player const*)target)->GetReputationMgr().GetRank(raw_tester_faction) <= REP_HOSTILE;
            }
        }
    }

    // common faction based case (CvC,PvC,CvP)
    return tester_faction->IsHostileTo(*target_faction);
}

/**
 * @brief Checks whether this unit is friendly to another unit.
 *
 * @param unit The other unit.
 * @return True if the units are friendly; otherwise, false.
 */
bool Unit::IsFriendlyTo(Unit const* unit) const
{
    // always friendly to self
    if (unit == this)
    {
        return true;
    }

    // always friendly to GM in GM mode
    if (unit->isGameMaster())
    {
        return true;
    }

    // always non-friendly to enemy
    if (getVictim() == unit || unit->getVictim() == this)
    {
        return false;
    }

    // test pet/charm masters instead pers/charmeds
    Unit const* testerOwner = GetCharmerOrOwner();
    Unit const* targetOwner = unit->GetCharmerOrOwner();

    // always non-friendly to owner's enemy
    if (testerOwner && (testerOwner->getVictim() == unit || unit->getVictim() == testerOwner))
    {
        return false;
    }

    // always non-friendly to enemy owner
    if (targetOwner && (getVictim() == targetOwner || targetOwner->getVictim() == this))
    {
        return false;
    }

    // always non-friendly to owner of owner's enemy
    if (testerOwner && targetOwner && (testerOwner->getVictim() == targetOwner || targetOwner->getVictim() == testerOwner))
    {
        return false;
    }

    Unit const* tester = testerOwner ? testerOwner : this;
    Unit const* target = targetOwner ? targetOwner : unit;

    // always friendly to target with common owner, or to owner/pet
    if (tester == target)
    {
        return true;
    }

    // special cases (Duel)
    if (tester->GetTypeId() == TYPEID_PLAYER && target->GetTypeId() == TYPEID_PLAYER)
    {
        Player const* pTester = (Player const*)tester;
        Player const* pTarget = (Player const*)target;

        // Duel
        if (pTester->IsInDuelWith(pTarget))
        {
            return false;
        }

        // Group
        if (pTester->GetGroup() && pTester->GetGroup() == pTarget->GetGroup())
        {
            return true;
        }

        // Sanctuary
        if (pTarget->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY) && pTester->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY))
        {
            return true;
        }

        // PvP FFA state
        if (pTester->IsFFAPvP() && pTarget->IsFFAPvP())
        {
            return false;
        }

        //= PvP states
        // Green/Blue (non-attackable)
        if (pTester->GetTeam() == pTarget->GetTeam())
        {
            return true;
        }

        // Blue (friendly/non-attackable) if not PVP, or Yellow/Red in another case (attackable)
        return !pTarget->IsPvP();
    }

    // faction base cases
    FactionTemplateEntry const* tester_faction = tester->getFactionTemplateEntry();
    FactionTemplateEntry const* target_faction = target->getFactionTemplateEntry();
    if (!tester_faction || !target_faction)
    {
        return false;
    }

    if (target->isAttackingPlayer() && tester->IsContestedGuard())
    {
        return false;
    }

    // PvC forced reaction and reputation case
    if (tester->GetTypeId() == TYPEID_PLAYER)
    {
        if (target_faction->Faction)
        {
            // forced reaction
            if (ReputationRank const* force = ((Player*)tester)->GetReputationMgr().GetForcedRankIfAny(target_faction))
            {
                return *force >= REP_FRIENDLY;
            }

            // if faction have reputation then friendly state for tester at 100% dependent from at_war state
            if (FactionEntry const* raw_target_faction = sFactionStore.LookupEntry(target_faction->Faction))
                if (FactionState const* factionState = ((Player*)tester)->GetReputationMgr().GetState(raw_target_faction))
                {
                    return !(factionState->Flags & FACTION_FLAG_AT_WAR);
                }
        }
    }
    // CvP forced reaction and reputation case
    else if (target->GetTypeId() == TYPEID_PLAYER)
    {
        if (tester_faction->Faction)
        {
            // forced reaction
            if (ReputationRank const* force = ((Player*)target)->GetReputationMgr().GetForcedRankIfAny(tester_faction))
            {
                return *force >= REP_FRIENDLY;
            }

            // apply reputation state
            if (FactionEntry const* raw_tester_faction = sFactionStore.LookupEntry(tester_faction->Faction))
                if (raw_tester_faction->ReputationIndex >= 0)
                {
                    return ((Player const*)target)->GetReputationMgr().GetRank(raw_tester_faction) >= REP_FRIENDLY;
                }
        }
    }

    // common faction based case (CvC,PvC,CvP)
    return tester_faction->IsFriendlyTo(*target_faction);
}

/**
 * @brief Checks whether this unit is intrinsically hostile to players.
 *
 * @return True if the faction template is hostile to players; otherwise, false.
 */
bool Unit::IsHostileToPlayers() const
{
    FactionTemplateEntry const* my_faction = getFactionTemplateEntry();
    if (!my_faction || !my_faction->Faction)
    {
        return false;
    }

    FactionEntry const* raw_faction = sFactionStore.LookupEntry(my_faction->Faction);
    if (raw_faction && raw_faction->ReputationIndex >= 0)
    {
        return false;
    }

    return my_faction->IsHostileToPlayers();
}

/**
 * @brief Checks whether this unit is neutral to all factions.
 *
 * @return True if the faction template is neutral to all; otherwise, false.
 */
bool Unit::IsNeutralToAll() const
{
    FactionTemplateEntry const* my_faction = getFactionTemplateEntry();
    if (!my_faction || !my_faction->Faction)
    {
        return true;
    }

    FactionEntry const* raw_faction = sFactionStore.LookupEntry(my_faction->Faction);
    if (raw_faction && raw_faction->ReputationIndex >= 0)
    {
        return false;
    }

    return my_faction->IsNeutralToAll();
}

/**
 * @brief Gets the faction template entry for the unit.
 *
 * @return The faction template entry, or NULL if invalid.
 */
FactionTemplateEntry const* Unit::getFactionTemplateEntry() const
{
    FactionTemplateEntry const* entry = sFactionTemplateStore.LookupEntry(getFaction());
    if (!entry)
    {
        static ObjectGuid guid;                             // prevent repeating spam same faction problem

        if (GetObjectGuid() != guid)
        {
            guid = GetObjectGuid();

            if (guid.GetHigh() == HIGHGUID_PET)
            {
                sLog.outError("%s (base creature entry %u) have invalid faction template id %u, owner %s", GetGuidStr().c_str(), GetEntry(), getFaction(), GetOwnerGuid().GetString().c_str());
            }
            else
            {
                sLog.outError("%s have invalid faction template id %u", GetGuidStr().c_str(), getFaction());
            }
        }
    }
    return entry;
}

bool Unit::IsContestedGuard() const
{
    if (FactionTemplateEntry const* entry = getFactionTemplateEntry())
    {
        return entry->IsContestedGuardFaction();
    }

    return false;
}

/**
 * @brief Checks whether the unit can currently be targeted for attack.
 *
 * @param inverseAlive True to invert the alive-state requirement.
 * @return True if the unit can be attacked; otherwise, false.
 */
bool Unit::IsTargetableForAttack(bool inverseAlive /*=false*/) const
{
    if (GetTypeId() == TYPEID_PLAYER && isGameMaster())
    {
        return false;
    }

    if (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))
    {
        return false;
    }

    // to be removed if unit by any reason enter combat
    if (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_OOC_NOT_ATTACKABLE))
    {
        return false;
    }

    // inversealive is needed for some spells which need to be casted at dead targets (aoe)
    if (IsAlive() == inverseAlive)
    {
        return false;
    }

    return IsInWorld() && !IsFeigningDeath() && !IsTaxiFlying();
}

/**
 * @brief Flags the unit and owner for contested PvP.
 *
 * @param attackedPlayer The attacked player, if any.
 */
void Unit::SetContestedPvP(Player* attackedPlayer)
{
    Player* player = GetCharmerOrOwnerPlayerOrPlayerItself();

    if (!player || (attackedPlayer && (attackedPlayer == player || player->IsInDuelWith(attackedPlayer))))
    {
        return;
    }

    player->SetContestedPvPTimer(30000);

    if (!player->hasUnitState(UNIT_STAT_ATTACK_PLAYER))
    {
        player->addUnitState(UNIT_STAT_ATTACK_PLAYER);
        player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP);
        // call MoveInLineOfSight for nearby contested guards
        UpdateVisibilityAndView();
    }

    if (!hasUnitState(UNIT_STAT_ATTACK_PLAYER))
    {
        addUnitState(UNIT_STAT_ATTACK_PLAYER);
        // call MoveInLineOfSight for nearby contested guards
        UpdateVisibilityAndView();
    }
}

bool Unit::IsAllowedDamageInArea(Unit* pVictim) const
{
    // can damage self anywhere
    if (pVictim == this)
    {
        return true;
    }

    // can damage own pet anywhere
    if (pVictim->GetOwnerGuid() == GetObjectGuid())
    {
        return true;
    }

    // non player controlled unit can damage anywhere
    Player const* pOwner = GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!pOwner)
    {
        return true;
    }

    // can damage non player controlled victim anywhere
    Player const* vOwner = pVictim->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!vOwner)
    {
        return true;
    }

    // can damage opponent in duel
    if (pOwner->IsInDuelWith(vOwner))
    {
        return true;
    }

    // can't damage player controlled unit by player controlled unit in sanctuary
    AreaTableEntry const* area = GetAreaEntryByAreaID(pVictim->GetTerrain()->GetAreaId(pVictim->Where().X(), pVictim->Where().Y(), pVictim->Where().Z()));
    if (area && area->Flags & AREA_FLAG_SANCTUARY)
    {
        return false;
    }

    return true;
}
