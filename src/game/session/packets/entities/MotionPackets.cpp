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

#include "MotionPackets.h"
#include "GameTime.h"
#include "Opcodes.h"
#include "Player.h"
#include "PlayerSession.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "Writers.h"
#include "movement/MonsterMovePacket.h"
#include "movement/MoveSpline.h"
#include "movement/MovementBridge.h"

bool SessionMotionNotices::MoverTold(Unit const& mover, PlayerSession& session, uint32 counter, Motion::Change const& change)
{
    WorldPacket data;
    if (!Motion::BuildMover(data, mover.GetObjectGuid().GetRawValue(), counter, change))
    {
        return false;
    }
    session.SendPacket(&data);
    return true;
}

bool SessionMotionNotices::SplineShown(Unit const& mover, Motion::Change const& change)
{
    WorldPacket data;
    if (!Motion::BuildSpline(data, mover.GetObjectGuid().GetRawValue(), change))
    {
        return false;
    }
    mover.SendMessageToSet(&data, true);
    return true;
}

bool SessionMotionNotices::ObserversTold(Unit const& mover, uint32 counter, Motion::Change const& change, Player const* acked)
{
    WorldPacket data;
    if (!Motion::BuildObserver(data, mover.GetObjectGuid().GetRawValue(), counter, change, Movement::ToWire(mover.WireStatus())))
    {
        return false;
    }
    if (acked)
    {
        mover.SendMessageToSetExcept(&data, acked);
    }
    else
    {
        mover.SendMessageToSet(&data, false);
    }
    return true;
}

void SessionMotionNotices::HeartBeat(Unit const& mover)
{
    WorldPacket data(MSG_MOVE_HEARTBEAT, 64);
    data << mover.GetPackGUID();
    mover.WriteMovementInfo(data, data.GetOpcode(), GameTime::GetGameTimeMS());
    mover.SendMessageToSet(&data, true);
}

void SessionMotionNotices::MonsterMoved(Unit const& mover, Movement::Aboard const& aboard)
{
    WorldPacket data;
    Movement::BuildMonsterMove(data, mover.GetObjectGuid(), aboard, *mover.movespline);
    mover.SendMessageToSet(&data, true);
}

void SessionMotionNotices::MonsterMoveStopped(Unit const& mover, Movement::Aboard const& aboard, Movement::Location const& at)
{
    WorldPacket data;
    Movement::BuildMonsterMoveStop(data, mover.GetObjectGuid(), aboard, at, mover.movespline->GetId());
    mover.SendMessageToSet(&data, true);
}
