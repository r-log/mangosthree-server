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

#include "DefaultMoves.h"

#include <algorithm>
#include <cmath>

namespace Motion
{
    namespace
    {
        constexpr int    CHANCE_NO_BREAK = 30;
        constexpr uint32 REST_AFTER_HOP_MIN = 3000;
        constexpr uint32 REST_AFTER_HOP_MAX = 10000;
        constexpr uint32 RETRY_DELAY = 50;
        constexpr float  MIN_FLIGHT_CLEARANCE = 0.5f;
        constexpr float  ORBIT_STEP_MIN = 0.45f;
        constexpr float  ORBIT_STEP_MAX = 1.15f;
        constexpr float  ORBIT_RADIUS_MIN_FACTOR = 0.55f;
        constexpr float  TWO_PI = 6.28318530718f;
    }

    Step WanderBehaviour::Activate(Sight const& sight, Services& svc)
    {
        // The generator's Initialize: ROAMING, both orbit draws (always), the rest cleared, no hop.
        m_lastRunning = sight.runningState;
        m_orbitTilt = svc.Frand(0.0f, TWO_PI);
        m_orbitAngle = svc.Frand(0.0f, TWO_PI);
        m_rest = 0;
        m_haveHop = false;
        // m_retries is untouched: the generator's Initialize never reset it either (git history: the deleted wander generator's Initialize).
        Step s;
        s.roaming = Roaming::SetRoam;   // ROAMING alone; ROAMING_MOVE follows the first hop
        s.resetLeg = true;
        return s;
    }

    Step WanderBehaviour::Suspend()
    {
        // The generator's Interrupt: InterruptMoving, Finalize (bits cleared, walk restored), the hop forgotten, the leg reset.
        Step s;
        s.interrupt = true;
        s.roaming = Roaming::ClearBoth;
        s.effects.push_back(Effect::Walk(!m_lastRunning));
        s.resetLeg = true;
        m_haveHop = false;
        return s;
    }

    Step WanderBehaviour::Resume(Sight const& sight, Services& svc, bool reset)
    {
        return reset ? Activate(sight, svc) : Step::None();   // the generator's Reset is its Initialize
    }

    uint32 WanderBehaviour::RetryDelay()
    {
        const uint32 delay = std::min<uint32>(RETRY_DELAY << m_retries, REST_AFTER_HOP_MIN);
        if (delay < REST_AFTER_HOP_MIN)
        {
            ++m_retries;
        }
        return delay;
    }

    Vector3 WanderBehaviour::NextOrbitPoint(Services& svc)
    {
        m_orbitAngle += svc.Frand(ORBIT_STEP_MIN, ORBIT_STEP_MAX);
        if (m_orbitAngle > TWO_PI)
        {
            m_orbitAngle -= TWO_PI;
        }
        const float r = m_p.radius * svc.Frand(ORBIT_RADIUS_MIN_FACTOR, 1.0f);
        Vector3 p(m_p.centre.x + r * std::cos(m_orbitAngle),
                  m_p.centre.y + r * std::sin(m_orbitAngle),
                  m_p.centre.z + m_p.verticalZ * std::sin(m_orbitAngle - m_orbitTilt));
        float floorZ = 0.0f;
        if (svc.Ground(p, floorZ))
        {
            p.z = std::max(p.z, floorZ + MIN_FLIGHT_CLEARANCE);
        }
        return p;
    }

    Step WanderBehaviour::Tick(Sight const& sight, Services& svc, uint32 diff)
    {
        m_lastRunning = sight.runningState;
        const uint32 hopFlags = m_p.airborne ? (MOVE_FLY | MOVE_STRAIGHT) : MOVE_WALK;

        if (!sight.alive || !sight.canMove)
        {
            m_rest = 0;
            Step s = Step::Of(MoveIntent::Hold());
            s.roaming = Roaming::ClearMove;
            return s;
        }
        if (sight.status.blocked)
        {
            m_haveHop = false;
            m_rest = int32(RetryDelay());
        }
        if (sight.status.arrived || (sight.status.traveling && m_haveHop))
        {
            m_retries = 0;
        }
        if (sight.status.traveling && m_haveHop)
        {
            return Step::Of(MoveIntent::Move(m_hop, hopFlags));
        }
        m_rest -= int32(diff);
        if (m_rest > 0)
        {
            return Step::Of(MoveIntent::Hold());
        }
        if (m_p.airborne)
        {
            m_hop = NextOrbitPoint(svc);
        }
        else
        {
            Vector3 hop;
            if (!svc.RandomPoint(m_p.centre, m_p.radius, hop))
            {
                m_rest = int32(RetryDelay());
                return Step::Of(MoveIntent::Hold());
            }
            m_hop = hop;
        }
        m_haveHop = true;
        // The rest after THIS hop is decided now; a flier never rests (its arcs join).
        m_rest = int32((m_p.airborne || svc.Irand(0, 99) < CHANCE_NO_BREAK)
            ? RETRY_DELAY
            : svc.Urand(REST_AFTER_HOP_MIN, REST_AFTER_HOP_MAX));
        Step s = Step::Of(MoveIntent::Move(m_hop, hopFlags));
        s.roaming = Roaming::SetMove;   // the generator adds ROAMING_MOVE here (ROAMING was set at Initialize)
        return s;
    }

    Outcome WanderBehaviour::Finish(FinishReason, Sight const& sight, Services&)
    {
        Outcome o;
        o.roaming = Roaming::ClearBoth;
        o.effects.push_back(Effect::Walk(!sight.runningState));
        return o;
    }
}
