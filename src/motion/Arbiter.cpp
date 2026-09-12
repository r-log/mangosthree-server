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

#include "Arbiter.h"

namespace Motion
{
    Layer LayerOf(Kind kind)
    {
        switch (kind)
        {
            case Kind::Idle:
            case Kind::Wander:
            case Kind::Patrol:
            case Kind::Follow:
                return Layer::Default;
            case Kind::Chase:
                return Layer::Combat;
            case Kind::Point:
            case Kind::FlyLand:
            case Kind::Home:
            case Kind::AssistRun:
                return Layer::Scripted;
            case Kind::Distract:
            case Kind::AssistDistract:
                return Layer::Distract;
            case Kind::Fear:
            case Kind::Confused:
                return Layer::Control;
            case Kind::Effect:
                return Layer::Forced;
            case Kind::Taxi:
                return Layer::Taxi;
            default:
                return Layer::Default;
        }
    }

    Policy PolicyOf(Kind kind, bool resumeCombat)
    {
        switch (kind)
        {
            case Kind::Wander:
            case Kind::Patrol:
            case Kind::FlyLand:
            case Kind::Home:
            case Kind::AssistRun:
            case Kind::Taxi:
                return Policy::Override;
            case Kind::Point:
                return resumeCombat ? Policy::Suspend : Policy::Override;
            case Kind::Follow:
            case Kind::Chase:
                return Policy::Supersede;
            default:
                return Policy::Suspend;   // Idle-as-command, Distract, AssistDistract, Fear, Confused, Effect
        }
    }

    bool SelfExpiring(Kind kind)
    {
        return kind == Kind::Home || kind == Kind::Distract || kind == Kind::Effect;
    }

    char const* KindName(Kind kind)
    {
        static char const* const names[] =
        {
            "Idle", "Wander", "Patrol", "Follow", "Chase", "Point", "FlyLand", "Home",
            "AssistRun", "Distract", "AssistDistract", "Fear", "Confused", "Effect", "Taxi"
        };
        static_assert(sizeof(names) / sizeof(names[0]) == static_cast<size_t>(Kind::Count),
                      "KindName out of sync with Kind");
        return kind < Kind::Count ? names[static_cast<uint8>(kind)] : "?";
    }

    char const* LayerName(Layer layer)
    {
        static char const* const names[] =
        {
            "Default", "Combat", "Scripted", "Distract", "Control", "Forced", "Taxi"
        };
        static_assert(sizeof(names) / sizeof(names[0]) == static_cast<size_t>(Layer::Count),
                      "LayerName out of sync with Layer");
        return layer < Layer::Count ? names[static_cast<uint8>(layer)] : "?";
    }

    char const* ReasonName(FinishReason reason)
    {
        static char const* const names[] =
        {
            "Arrived", "Cut", "Blocked", "Expired", "Superseded", "Overridden", "Cleared",
            "Cancelled", "TargetLost", "Died"
        };
        const size_t index = static_cast<size_t>(reason);
        return index < sizeof(names) / sizeof(names[0]) ? names[index] : "?";
    }
}
