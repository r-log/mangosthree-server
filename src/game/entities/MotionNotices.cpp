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

#include "entities/MotionNotices.h"

#include <atomic>
#include <cstddef>

namespace
{
    std::atomic<MotionNotices*> s_motionNoticesInstalled(NULL);

    MotionNotices& BaseMotionNotices()
    {
        static MotionNotices notices;
        return notices;
    }
}

MotionNotices::~MotionNotices()
{
}

bool MotionNotices::MoverTold(Unit const& /*mover*/, PlayerSession& /*session*/, uint32 /*counter*/, Motion::Change const& /*change*/)
{
    return false;
}

bool MotionNotices::SplineShown(Unit const& /*mover*/, Motion::Change const& /*change*/)
{
    return false;
}

bool MotionNotices::ObserversTold(Unit const& /*mover*/, uint32 /*counter*/, Motion::Change const& /*change*/, Player const* /*acked*/)
{
    return false;
}

void MotionNotices::HeartBeat(Unit const& /*mover*/)
{
}

void MotionNotices::MonsterMoved(Unit const& /*mover*/, Movement::Aboard const& /*aboard*/)
{
}

void MotionNotices::MonsterMoveStopped(Unit const& /*mover*/, Movement::Aboard const& /*aboard*/, Movement::Location const& /*at*/)
{
}

MotionNotices& MotionNotices::Installed()
{
    MotionNotices* notices = s_motionNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseMotionNotices();
}

MotionNotices* MotionNotices::Install(MotionNotices* notices)
{
    return s_motionNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
