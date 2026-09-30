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

#ifndef MANGOS_MOTION_TRACKINGMOVES_H
#define MANGOS_MOTION_TRACKINGMOVES_H

#include "BehaviourModel.h"

// Three natives, pure kernel policy (P5-B family 3). ChaseBehaviour and FollowBehaviour keep
// a distance from another unit, replacing the deleted TargetedMovementGenerator together
// with its chase and follow subclasses. HomeBehaviour is the evade return, replacing the
// deleted HomeMovementGenerator. Seven files left the tree with the shell switch: four from
// src/game/MotionGenerators/ (TargetedMovementGenerator.h/.cpp, HomeMovementGenerator.h/.cpp)
// and the three of src/game/References/ (FollowerReference.h/.cpp, FollowerRefManager.h), the
// link they held their target through.
// Everything the generator did to its unit (the free-spot search, the live cast read, the
// speed sync, the attack in reach, the block-safe state clear, the arrival recipe) is a
// Services call or an Effect now.

namespace Motion
{
    /// The chase and the follow: one behaviour, told apart by its kind. Both track a unit,
    /// derive a standing spot from where it is (or where it will be), re-lay a leg when it
    /// drifts, and count every re-lay by cause. The kind decides the distance, the drift edge,
    /// the aim, the gait, the facing and the activation and teardown effects.
    class TrackingBehaviour : public Behaviour
    {
        public:
            struct Params
            {
                uint64 target = 0;          ///< the tracked unit's raw guid
                float  offset = 0.0f;       ///< the requested distance to keep
                float  angle = 0.0f;        ///< the requested bearing relative to the target's facing; 0 = head-on
                uint32 routineMs = 1000;    ///< the drift re-check cadence; 0 re-checks on every tick
                uint32 horizonMs = 400;     ///< follow: the extrapolation of a trusted velocity, one cadence
                float  recalcRange = 1.5f;  ///< follow: the shell's TargetPosRecalculateRange
            };
            TrackingBehaviour(Motion::Kind kind, Params const& p) : m_kind(kind), m_p(p) {}
            Motion::Kind Kind() const override { return m_kind; }
            bool TracksTarget() const override { return true; }
            uint64 Target() const override { return m_p.target; }
            RelayCounts const* Relays() const override { return &m_relays; }
            Step Activate(Sight const& sight, Services& svc) override;
            Step Suspend() override;
            Step Resume(Sight const& sight, Services& svc, bool reset) override;
            Step Tick(Sight const& sight, Services& svc, uint32 diff) override;
            FinishReason EndReason(Sight const& sight) const override;
            Outcome Finish(FinishReason why, Sight const& sight, Services& svc) override;
        private:
            bool  Chase() const { return m_kind == Motion::Kind::Chase; }
            float StandingDistance(Sight const& sight) const;   ///< how far from the centre the spot is asked for
            bool  Drifted(Sight const& sight) const;            ///< the target moved past the re-approach edge from the leg's goal
            Vector3 AimCentre(Sight const& sight) const;        ///< live, or led
            Facing FacingFor(Sight const& sight, bool moving) const;///< the leg's or the idle hold's facing
            float Bearing(Sight const& sight, Vector3 const& centre) const;   ///< head-on: from the centre to the mover; else the target's facing + angle
            /// The target's live distance from the leg's goal, against `edge`; a flier's and a
            /// swimmer's is measured in three dimensions, anything on the ground in two.
            bool DriftedBeyond(Sight const& sight, float edge) const;
            void ResetTracking();             ///< forget the leg and the cadence
            void LatchRelay(Sight const& sight);///< an ended leg's edge, held until a tick that may move spends it
            void Derive(Sight const& sight, Services& svc, RelayCause why, Step& s);///< one fresh standing spot, counted
            Motion::Kind m_kind;
            Params  m_p;
            Vector3 m_dest;                   ///< the spot the last derive produced
            bool    m_haveDest = false;       ///< a spot has been derived at least once
            bool    m_relayLatch = false;     ///< a cut, a partial or a refused leg: derive on the next tick that moves
            RelayCause m_latchCause = RelayCause::Cut;///< which of the three the latch holds
            int32   m_routine = 0;            ///< ms until the next drift re-check
            RelayCounts m_relays;             ///< every derive, by cause: the GM dump's numbers
    };

    /// How far ahead of a trusted target velocity the chase aims, in milliseconds of it. Half a
    /// second: the value the experiment carried, kept because it is the value that was measured.
    /// TrackingBehaviour::AimCentre has every number behind it. Public because the aim centre is no
    /// longer the target's own position, so anything measuring where the chase AIMS -- the
    /// harness's chase-relay-budget, above all -- has to take its bearings from the same point
    /// the kernel did, off one shared constant rather than a copied literal.
    const uint32 CHASE_LEAD_MS = 500;

    /// A creature closing on its victim: retail's band, retail's cadence, and a predictive aim
    /// that leads a trusted velocity by CHASE_LEAD_MS.
    struct ChaseBehaviour : TrackingBehaviour
    {
        explicit ChaseBehaviour(Params const& p) : TrackingBehaviour(Motion::Kind::Chase, p) {}
    };

    /// A follower trailing its leader: a bounded horizon, the leader's facing at rest.
    struct FollowBehaviour : TrackingBehaviour
    {
        explicit FollowBehaviour(Params const& p) : TrackingBehaviour(Motion::Kind::Follow, p) {}
    };

    /// The evade return (design §4.3): the deleted home generator with the block-safe clear
    /// and a forced endpoint.
    class HomeBehaviour : public Behaviour
    {
        public:
            struct Params
            {
                Vector3 home;               ///< world coordinates (the shell converts a Move goal to the frame)
                float   facing = 0.0f;
            };
            explicit HomeBehaviour(Params const& p) : m_p(p) {}
            Motion::Kind Kind() const override { return Motion::Kind::Home; }
            Step Activate(Sight const& sight, Services& svc) override;
            Step Suspend() override { return Step::None(); }                 // Interrupt was a no-op
            Step Resume(Sight const& sight, Services& svc, bool reset) override;
            Step Tick(Sight const& sight, Services& svc, uint32 diff) override;
            FinishReason EndReason(Sight const&) const override { return FinishReason::Arrived; }
            Outcome Finish(FinishReason why, Sight const& sight, Services& svc) override;
        private:
            Params m_p;
            bool   m_cleared = false;   ///< the dynamic states cleared on the first tick (after any block lifted)
            bool   m_arrived = false;
    };
}

#endif
