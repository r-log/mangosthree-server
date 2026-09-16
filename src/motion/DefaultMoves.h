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

#ifndef MANGOS_MOTION_DEFAULTMOVES_H
#define MANGOS_MOTION_DEFAULTMOVES_H

#include "BehaviourModel.h"

namespace Motion
{
    /// The idle wander (design §4.1): a hop to a random point in the leash, a rest, again;
    /// a flier orbits an inclined ellipse instead. Every draw is the generator's, in its order.
    class WanderBehaviour : public Behaviour
    {
        public:
            struct Params
            {
                Vector3 centre;
                float   radius = 0.1f;      ///< clamped at 0.1 by the caller
                float   verticalZ = 0.0f;
                bool    airborne = false;   ///< the shell's verticalZ > 0 && CanFly()
            };
            explicit WanderBehaviour(Params const& p) : m_p(p) {}
            Motion::Kind Kind() const override { return Motion::Kind::Wander; }
            Step Activate(Sight const& sight, Services& svc) override;
            Step Suspend() override;
            Step Resume(Sight const& sight, Services& svc, bool reset) override;
            Step Tick(Sight const& sight, Services& svc, uint32 diff) override;
            FinishReason EndReason(Sight const&) const override { return FinishReason::Expired; }
            Outcome Finish(FinishReason why, Sight const& sight, Services& svc) override;
        private:
            Vector3 NextOrbitPoint(Services& svc);
            uint32  RetryDelay();
            Params  m_p;
            float   m_orbitTilt = 0.0f;
            float   m_orbitAngle = 0.0f;
            int32   m_rest = 0;            ///< ms left standing; <= 0 = passed
            bool    m_haveHop = false;
            Vector3 m_hop;
            uint32  m_retries = 0;
            bool    m_lastRunning = false; ///< the Sight's runningState at the last hook, for the walk restore
    };
}

#endif
