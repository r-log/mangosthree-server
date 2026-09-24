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

/**
 * @file UnitThreat.cpp
 * @brief Unit's side of the threat/aggro system: whether a unit can hold a threat
 *        list, adding to and deleting it, the threat modifier, the second-choice
 *        test, target fixation, and the SMSG_THREAT_* packets.
 *
 * Originally a pure file move out of Unit.cpp (bodies byte-identical, declarations
 * left in Unit.h). Since decoupling D5e (server #134) the three methods only a
 * Creature could execute -- SelectHostileTarget, TauntApply and TauntFadeOut -- live
 * on Creature, in CreatureThreat.cpp. What remains here is what a Player runs too.
 */

#include "Utilities/Errors.h"
#include "Unit.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "QuestDef.h"
#include "Player.h"
#include "Creature.h"
#include "Spell.h"
#include "Group.h"
#include "SpellAuras.h"
#include "MapManager.h"
#include "TemporarySummon.h"
#include "Formulas.h"
#include "Pet.h"
#include "Util.h"
#include "Totem.h"
#include "Vehicle.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "MapPersistentStateMgr.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "movement/MoveSplineInit.h"
#include "movement/MoveSpline.h"
#include "GameTime.h"

#include <math.h>
#include <stdarg.h>

/*########################################
########                          ########
########       AGGRO SYSTEM       ########
########                          ########
########################################*/

bool Unit::CanHaveThreatList(bool ignoreAliveState/*=false*/) const
{
    // only creatures can have threat list
    if (GetTypeId() != TYPEID_UNIT)
    {
        return false;
    }

    // only alive units can have threat list
    if (!IsAlive() && !ignoreAliveState)
    {
        return false;
    }

    Creature const* creature = ((Creature const*)this);

    // totems can not have threat list
    if (creature->IsTotem())
    {
        return false;
    }

    // pets can not have a threat list, unless they are controlled by a creature
    if (creature->IsPet() && creature->GetOwnerGuid().IsPlayer())
    {
        return false;
    }

    // charmed units can not have a threat list if charmed by player
    if (creature->GetCharmerGuid().IsPlayer())
    {
        return false;
    }

    return true;
}

//======================================================================

float Unit::ApplyTotalThreatModifier(float threat, SpellSchoolMask schoolMask)
{
    if (!HasAuraType(SPELL_AURA_MOD_THREAT))
    {
        return threat;
    }

    if (schoolMask == SPELL_SCHOOL_MASK_NONE)
    {
        return threat;
    }

    SpellSchools school = GetFirstSchoolInMask(schoolMask);

    return threat * m_threatModifier[school];
}

//======================================================================

void Unit::AddThreat(Unit* pVictim, float threat /*= 0.0f*/, bool crit /*= false*/, SpellSchoolMask schoolMask /*= SPELL_SCHOOL_MASK_NONE*/, SpellEntry const* threatSpell /*= NULL*/)
{
    // Only mobs can manage threat lists
    if (CanHaveThreatList())
    {
        m_ThreatManager.addThreat(pVictim, threat, crit, schoolMask, threatSpell);
    }
}

//======================================================================

void Unit::DeleteThreatList()
{
    if (CanHaveThreatList(true) && !m_ThreatManager.isThreatListEmpty())
    {
        SendThreatClear();
    }

    m_ThreatManager.clearReferences();
}

//======================================================================
/// if pVictim is given, the npc will fixate onto pVictim, if NULL it will remove current fixation
void Unit::FixateTarget(Unit* pVictim)
{
    if (!pVictim)                                           // Remove Fixation
    {
        m_fixateTargetGuid.Clear();
    }
    else if (pVictim->IsTargetableForAttack())              // Apply Fixation
    {
        m_fixateTargetGuid = pVictim->GetObjectGuid();
    }

    // Start attacking the fixated target or the next proper one
    // SelectHostileTarget lives on Creature since decoupling D5e (server #134); it carried
    // MANGOS_ASSERT(GetTypeId() == TYPEID_UNIT) as its first statement, so the assert that
    // guarded this call is now at the call, with the same condition and the same abort.
    Creature* creature = ToCreature();
    MANGOS_ASSERT(creature);
    creature->SelectHostileTarget();
}

//======================================================================

bool Unit::IsSecondChoiceTarget(Unit* pTarget, bool checkThreatArea) const
{
    MANGOS_ASSERT(pTarget && GetTypeId() == TYPEID_UNIT);

    return
        pTarget->IsImmunedToDamage(GetMeleeDamageSchoolMask()) ||
        pTarget->hasNegativeAuraWithInterruptFlag(AURA_INTERRUPT_FLAG_DAMAGE) ||
        checkThreatArea && ((Creature*)this)->IsOutOfThreatArea(pTarget);
}

void Unit::SendThreatUpdate()
{
    ThreatList const& tlist = GetThreatManager().getThreatList();
    if (uint32 count = tlist.size())
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Send SMSG_THREAT_UPDATE Message");
        WorldPacket data(SMSG_THREAT_UPDATE, 8 + count * 8);
        data << GetPackGUID();
        data << uint32(count);
        for (ThreatList::const_iterator itr = tlist.begin(); itr != tlist.end(); ++itr)
        {
            data << (*itr)->getUnitGuid().WriteAsPacked();
            data << uint32((*itr)->getThreat());
        }
        SendMessageToSet(&data, false);
    }
}

void Unit::SendHighestThreatUpdate(HostileReference* pHostilReference)
{
    ThreatList const& tlist = GetThreatManager().getThreatList();
    if (uint32 count = tlist.size())
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Send SMSG_HIGHEST_THREAT_UPDATE Message");
        WorldPacket data(SMSG_HIGHEST_THREAT_UPDATE, 8 + 8 + count * 8);
        data << GetPackGUID();
        data << pHostilReference->getUnitGuid().WriteAsPacked();
        data << uint32(count);
        for (ThreatList::const_iterator itr = tlist.begin(); itr != tlist.end(); ++itr)
        {
            data << (*itr)->getUnitGuid().WriteAsPacked();
            data << uint32((*itr)->getThreat());
        }
        SendMessageToSet(&data, false);
    }
}

void Unit::SendThreatClear()
{
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Send SMSG_THREAT_CLEAR Message");
    WorldPacket data(SMSG_THREAT_CLEAR, 8);
    data << GetPackGUID();
    SendMessageToSet(&data, false);
}

void Unit::SendThreatRemove(HostileReference* pHostileReference)
{
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Send SMSG_THREAT_REMOVE Message");
    WorldPacket data(SMSG_THREAT_REMOVE, 8 + 8);
    data << GetPackGUID();
    data << pHostileReference->getUnitGuid().WriteAsPacked();
    SendMessageToSet(&data, false);
}
