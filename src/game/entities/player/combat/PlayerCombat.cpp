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
#include "data/Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GridNotifiersImpl.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Util.h"
#include "BattleGround/BattleGround.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "Vehicle.h"
#include "DisableMgr.h"

#include <cmath>

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
 * @brief Credits the damage the player dealt to a victim: its battleground score when the victim
 * is a player too, and its achievement criteria.
 *
 * @param pVictim The unit that took the damage.
 * @param damage The damage dealt.
 */
void Player::CreditDamageDealt(Unit* pVictim, uint32 damage)
{

    // in bg, count dmg if victim is also a player
    if (pVictim->GetTypeId() == TYPEID_PLAYER)
    {
        if (BattleGround* bg = GetBattleGround())
        {
            // FIXME: kept by compatibility. don't know in BG if the restriction apply.
            bg->UpdatePlayerScore(this, SCORE_DAMAGE_DONE, damage);
        }
    }

    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DAMAGE_DONE, damage, 0, pVictim);
    UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_HIGHEST_HIT_DEALT, damage);
}
