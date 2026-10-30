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
 * @file CreatureCombat.cpp
 * @brief The creature's part of Unit's combat flow: its combat anchor, its reaction and call
 *        for assistance when it attacks, the assistance and temporary faction reset when it
 *        stops, what it tells its AI, its instance and its linked creatures when it enters
 *        combat, its out-of-combat flags when it leaves, whether its hits may crush, and its speed
 *        while it has searched for assistance.
 */

#include "Creature.h"
#include "CreatureAI.h"
#include "InstanceData.h"
#include "CreatureLinkingMgr.h"
#include "Map.h"

//======================================================================

void Creature::AnchorCombat()
{
    SetCombatAnchor(Geometry::Vector3(Where().X(), Where().Y(), Where().Z()));
}

void Creature::AttackStarted()
{
    SendAIReaction(AI_REACTION_HOSTILE);
    CallAssistance();
}

void Creature::AttackStopped()
{
    SetNoCallAssistance(false);

    if (HasSearchedAssistance())
    {
        SetNoSearchAssistance(false);
        UpdateSpeed(MOVE_RUN, false);
    }
}

void Creature::CombatStopped()
{
    if (GetTemporaryFactionFlags() & TEMPFACTION_RESTORE_COMBAT_STOP)
    {
        ClearTemporaryFaction();
    }
}

//======================================================================

void Creature::EnteredCombat(Unit* enemy)
{
    // should probably be removed for the attacked (+ it's party/group) only, not global
    RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_OOC_NOT_ATTACKABLE);

    // client does not handle this state on it's own (reset to default at LoadCreatureAddon)
    if (getStandState() == UNIT_STAND_STATE_CUSTOM)
    {
        SetStandState(UNIT_STAND_STATE_STAND);
    }

    if (AI())
    {
        AI()->EnterCombat(enemy);
    }

    // Some bosses are set into combat with zone
    if (GetMap()->IsDungeon() && (GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_AGGRO_ZONE) && enemy && enemy->IsControlledByPlayer())
    {
        SetInCombatWithZone();
    }

    if (InstanceData* mapInstance = GetInstanceData())
    {
        mapInstance->OnCreatureEnterCombat(this);
    }

    if (m_isCreatureLinkingTrigger)
    {
        GetMap()->GetCreatureLinkingHolder()->DoCreatureLinkingEvent(LINKING_EVENT_AGGRO, this, enemy);
    }
}

void Creature::LeftCombat()
{
    if (GetCreatureInfo()->UnitFlags & UNIT_FLAG_OOC_NOT_ATTACKABLE && !(GetTemporaryFactionFlags() & TEMPFACTION_TOGGLE_OOC_NOT_ATTACK))
    {
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_OOC_NOT_ATTACKABLE);
    }

    // a player's is cleared in Player::UpdateContestedPvP
    clearUnitState(UNIT_STAT_ATTACK_PLAYER);
}

bool Creature::MayCrush() const
{
    Unit* owner = GetOwner();
    return !(GetOwnerGuid() && owner && owner->GetTypeId() == TYPEID_PLAYER)
        && !(GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_CRUSH);
}

//======================================================================

float Creature::StateSpeedFactor() const
{
    // best guessed value, so this will be 33% reduction. Based off initial speed, mob can then "run", "walk fast" or "walk".
    return HasSearchedAssistance() ? 0.66f : 1.0f;
}
