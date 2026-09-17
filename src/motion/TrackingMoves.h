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

// The two natives that keep a distance from another unit, as pure kernel policy (P5-B
// family 3): ChaseBehaviour and FollowBehaviour replace TargetedMovementGenerator together
// with its ChaseMovementGenerator and FollowMovementGenerator subclasses -- all three
// deleted from src/game/MotionGenerators/ on this branch. Everything the generator did to
// its unit (the free-spot search, the live cast read, the speed sync, the attack in reach)
// is a Services call or an Effect now.

namespace Motion
{
    /// The chase's and the follow's shared policy (design §4): the deleted
    /// TargetedMovementGenerator::Intent() with one routine cadence per kind and counted
    /// event recoveries in place of the 100/50 ms polls. Replaces src/game/MotionGenerators/
    /// TargetedMovementGenerator.h/.cpp (deleted on this branch).
    class TrackingBehaviour : public Behaviour
    {
        public:
            struct Params
            {
                uint64 target = 0;          ///< the tracked unit's raw guid
                float  offset = 0.0f;       ///< the requested distance to keep
                float  angle = 0.0f;        ///< the requested bearing relative to the target's facing; 0 = head-on
                uint32 stateSet = 0;        ///< UNIT_STAT_CHASE or UNIT_STAT_FOLLOW (opaque)
                uint32 stateMove = 0;       ///< UNIT_STAT_CHASE_MOVE or UNIT_STAT_FOLLOW_MOVE (opaque)
                uint32 routineMs = 1000;    ///< the drift re-check cadence
            };
            bool TracksTarget() const override { return true; }
            uint64 Target() const override { return m_p.target; }
            RelayCounts const* Relays() const override { return &m_relays; }
            Step Activate(Sight const& sight, Services& svc) override;
            Step Suspend() override;
            Step Resume(Sight const& sight, Services& svc, bool reset) override;
            Step Tick(Sight const& sight, Services& svc, uint32 diff) override;
            FinishReason EndReason(Sight const& sight) const override;
            Outcome Finish(FinishReason why, Sight const& sight, Services& svc) override;
        protected:
            explicit TrackingBehaviour(Params const& p) : m_p(p) {}
            // The per-kind hooks (the generator's virtuals).
            virtual bool  UsesCombatMovement() const = 0;                ///< the chase holds under NO_COMBAT_MOVEMENT
            virtual bool  LostTarget(Sight const& sight) const = 0;      ///< the chase: no longer the victim
            virtual float StandingDistance(Sight const& sight) const = 0;///< how far from the centre the spot is asked for
            virtual bool  Drifted(Sight const& sight) const = 0;         ///< the target moved past the re-approach edge from the leg's goal
            virtual Vector3 AimCentre(Sight const& sight) const = 0;     ///< live, or led
            virtual bool  Walks(Sight const& sight) const = 0;           ///< the leg's gait
            virtual bool  ForcesDestination(Sight const& sight) const = 0;///< MOVE_FORCE_DEST on the leg
            virtual Facing FacingFor(Sight const& sight, bool moving) const = 0;///< the leg's or the idle hold's facing
            virtual void  OnActivate(Sight const& sight, Step& s) = 0;   ///< the kind's activation effects
            virtual void  OnIdle(Sight const& sight, Step& s) = 0;       ///< an idle tick at the spot (the chase engages)
            virtual void  OnSuspendOrFinish(std::vector<Effect>& effects) = 0;///< the kind's teardown effects
            float Bearing(Sight const& sight, Vector3 const& centre) const;   ///< head-on: from the centre to the mover; else the target's facing + angle
            Params m_p;                       ///< the shared parameters, as the kind's own copy was built
        private:
            void ResetTracking();             ///< the generator's ResetTracking: forget the leg and the cadence
            void Derive(Sight const& sight, Services& svc, RelayCause why, Step& s);///< one fresh standing spot, counted
            Vector3 m_dest;                   ///< the spot the last derive produced
            bool    m_haveDest = false;       ///< a spot has been derived at least once
            bool    m_reached = false;        ///< the arrival at that spot was seen (once per approach)
            bool    m_relayLatch = false;     ///< a cut or a partial leg ended: derive on the next tick that moves
            RelayCause m_latchCause = RelayCause::Cut;///< which of the two the latch holds
            int32   m_routine = 0;            ///< ms until the next drift re-check
            RelayCounts m_relays;             ///< every derive, by cause: the GM dump's numbers
            bool    m_lastRunning = false;    ///< the last tick's UNIT_STAT_RUNNING_STATE, as the default natives keep
    };

    /// A creature closing on its victim (design §4.1): retail's band and cadence; the lead an experiment.
    class ChaseBehaviour : public TrackingBehaviour
    {
        public:
            struct ChaseParams : Params
            {
                bool   lead = false;        ///< the experiment: aim ahead by leadMs of trusted velocity
                uint32 leadMs = 500;        ///< how far ahead, in ms of that velocity
            };
            explicit ChaseBehaviour(ChaseParams const& p) : TrackingBehaviour(p), m_c(p) {}
            Motion::Kind Kind() const override { return Motion::Kind::Chase; }
        protected:
            bool  UsesCombatMovement() const override { return true; }
            bool  LostTarget(Sight const& sight) const override { return !sight.target.isVictim; }
            float StandingDistance(Sight const& sight) const override;
            bool  Drifted(Sight const& sight) const override;
            Vector3 AimCentre(Sight const& sight) const override;
            bool  Walks(Sight const&) const override { return false; }
            bool  ForcesDestination(Sight const&) const override { return false; }
            Facing FacingFor(Sight const& sight, bool moving) const override;
            void  OnActivate(Sight const& sight, Step& s) override;
            void  OnIdle(Sight const& sight, Step& s) override;
            void  OnSuspendOrFinish(std::vector<Effect>& effects) override;
        private:
            ChaseParams m_c;                ///< the chase's own fields, beside the shared Params
    };

    /// A follower trailing its leader (design §4.2): a bounded horizon, the leader's facing at rest.
    class FollowBehaviour : public TrackingBehaviour
    {
        public:
            struct FollowParams : Params
            {
                uint32 horizonMs = 400;     ///< the extrapolation of a trusted velocity: one cadence
                float  recalcRange = 1.5f;  ///< the shell's TargetPosRecalculateRange
            };
            explicit FollowBehaviour(FollowParams const& p) : TrackingBehaviour(p), m_f(p) {}
            Motion::Kind Kind() const override { return Motion::Kind::Follow; }
        protected:
            bool  UsesCombatMovement() const override { return false; }
            bool  LostTarget(Sight const&) const override { return false; }
            float StandingDistance(Sight const& sight) const override;
            bool  Drifted(Sight const& sight) const override;
            Vector3 AimCentre(Sight const& sight) const override;
            bool  Walks(Sight const& sight) const override { return sight.isCreature && sight.target.walking; }
            bool  ForcesDestination(Sight const& sight) const override { return sight.isPet; }
            Facing FacingFor(Sight const& sight, bool moving) const override;
            void  OnActivate(Sight const& sight, Step& s) override;
            void  OnIdle(Sight const&, Step&) override {}
            void  OnSuspendOrFinish(std::vector<Effect>& effects) override;
        private:
            FollowParams m_f;               ///< the follow's own fields, beside the shared Params
    };
}

#endif
