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

#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "SkillDiscovery.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "ArenaTeam.h"
#include "Chat.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "SQLStorages.h"
#include "Vehicle.h"
#include "Calendar.h"
#include "DisableMgr.h"

#include <cmath>

void Player::SendAttackSwingNotInRange()
{
    ReportClientFact(m_clientCallbacks.swingOutOfReach, SwingOutOfReachFact());
}

/**
 * @brief Sends the error packet for attempting to attack a dead target.
 */
void Player::SendAttackSwingDeadTarget()
{
    WorldPacket data(SMSG_ATTACKSWING_DEADTARGET, 0);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the error packet for a general inability to attack the target.
 */
void Player::SendAttackSwingCantAttack()
{
    WorldPacket data(SMSG_ATTACKSWING_CANT_ATTACK, 0);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Tells the client that the player's current attack is cancelled.
 */
void Player::SendAttackSwingCancelAttack()
{
    ReportClientFact(m_clientCallbacks.combatCancelled, CombatCancelledFact());
}

/**
 * @brief Tells the client that the player attacks while facing the wrong direction.
 */
void Player::SendAttackSwingBadFacingAttack()
{
    ReportClientFact(m_clientCallbacks.swingBadFacing, SwingBadFacingFact());
}

/**
 * @brief Tells the client that its auto-repeat attack on `target` is cancelled.
 */
void Player::SendAutoRepeatCancel(Unit* target)
{
    AutoRepeatCancelledFact fact;
    fact.target = target->GetObjectGuid();
    ReportClientFact(m_clientCallbacks.autoRepeatCancelled, fact);
}

/**
 * @brief Tells the client a melee swing error that differs from the last one it was told, and
 * remembers it.
 *
 * @param swingError 0 for none, 1 out of reach, 2 facing the wrong way.
 */
void Player::ReportSwingError(uint8 swingError)
{
    if (swingError != LastSwingErrorMsg())
    {
        if (swingError == 1)
        {
            SendAttackSwingNotInRange();
        }
        else if (swingError == 2)
        {
            SendAttackSwingBadFacingAttack();
        }
        SwingErrorMsg(swingError);
    }
}
