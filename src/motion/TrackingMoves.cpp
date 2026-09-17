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

#include "TrackingMoves.h"

#include <cmath>
#include "Utilities/MathDefines.h"

namespace Motion
{
    namespace
    {
        constexpr float CONTACT_DISTANCE = 0.5f;            ///< retail's contact gap, on top of the two reaches
        constexpr float FOLLOW_RECALCULATE_FACTOR = 1.0f;   ///< how much of the two radii counts toward "the follow spot is stale"
        constexpr float FOLLOW_DIST_GAP_FOR_DIST_FACTOR = 3.0f;///< a follow distance beyond this gets extra slop
        constexpr float FOLLOW_DIST_RECALCULATE_FACTOR = 1.0f; ///< how much of it

        /// The world bearing from one point to another, normalised to [0, 2pi).
        float AngleFromTo(Vector3 const& from, Vector3 const& to)
        {
            const float a = std::atan2(to.y - from.y, to.x - from.x);
            return a >= 0.0f ? a : 2 * M_PI_F + a;
        }
    }

    // ---- TrackingBehaviour ----------------------------------------------------------

    void TrackingBehaviour::ResetTracking()
    {
        m_haveDest = false;
        m_reached = false;
        m_relayLatch = false;
        m_routine = 0;
    }

    float TrackingBehaviour::Bearing(Sight const& sight, Vector3 const& centre) const
    {
        if (Kind() == Motion::Kind::Chase && m_p.angle == 0.0f)
        {
            return AngleFromTo(centre, sight.position);   // head-on: approach from where the mover already is
        }
        return sight.target.facing + m_p.angle;
    }

    Step TrackingBehaviour::Activate(Sight const& sight, Services& /*svc*/)
    {
        // Initialize: the kind's state bit (never the _MOVE one: that follows a laid leg), the tracking reset.
        m_lastRunning = sight.runningState;
        ResetTracking();
        Step s;
        s.resetLeg = true;
        OnActivate(sight, s);
        s.effects.push_back(Effect::State(m_p.stateSet, 0));
        return s;
    }

    Step TrackingBehaviour::Resume(Sight const& sight, Services& svc, bool reset)
    {
        return reset ? Activate(sight, svc) : Step::None();   // Reset was Initialize
    }

    Step TrackingBehaviour::Suspend()
    {
        // Interrupt: InterruptMoving, both bits cleared, the tracking reset.
        ResetTracking();
        Step s;
        s.interrupt = true;
        s.resetLeg = true;
        s.effects.push_back(Effect::State(0, m_p.stateSet | m_p.stateMove));
        OnSuspendOrFinish(s.effects);
        return s;
    }

    Outcome TrackingBehaviour::Finish(FinishReason why, Sight const& /*sight*/, Services& /*svc*/)
    {
        Outcome o;
        o.interrupt = Displacing(why);   // the generator's Interrupt stopped the mover; Finalize did not
        o.effects.push_back(Effect::State(0, m_p.stateSet | m_p.stateMove));
        OnSuspendOrFinish(o.effects);
        return o;
    }

    FinishReason TrackingBehaviour::EndReason(Sight const& /*sight*/) const
    {
        return FinishReason::TargetLost;
    }

    void TrackingBehaviour::Derive(Sight const& sight, Services& svc, RelayCause why, Step& s)
    {
        const Vector3 centre = AimCentre(sight);
        Vector3 spot;
        if (!svc.StandingSpot(centre, StandingDistance(sight), Bearing(sight, centre), spot))
        {
            spot = centre;   // no free spot: the centre itself, as the generator's NearPoint fell back to the raw point
        }
        m_dest = spot;
        m_haveDest = true;
        m_reached = false;
        m_relays.Count(why);
        s.effects.push_back(Effect::State(m_p.stateMove, 0));
    }

    Step TrackingBehaviour::Tick(Sight const& sight, Services& svc, uint32 diff)
    {
        m_lastRunning = sight.runningState;
        // 1. The target is gone.
        if (!sight.target.valid) { return Step::Of(MoveIntent::Done()); }
        // 2. The mover is dead.
        if (!sight.alive) { return Step::Of(MoveIntent::Hold()); }
        // 3. A state holds it (the chase also under NO_COMBAT_MOVEMENT), or the chase lost its victim.
        const bool held = !sight.canMove || (UsesCombatMovement() && sight.combatMovementHeld) || LostTarget(sight);
        if (held)
        {
            if (sight.status.partial || sight.status.cut) { m_relayLatch = true; m_latchCause = sight.status.cut ? RelayCause::Cut : RelayCause::Partial; }
            Step s = Step::Of(MoveIntent::Hold());
            s.effects.push_back(Effect::State(0, m_p.stateMove));
            return s;
        }
        // 4. A cast with a cast time, or a channel: stop once, hold.
        if (svc.Casting())
        {
            if (sight.status.partial || sight.status.cut) { m_relayLatch = true; m_latchCause = sight.status.cut ? RelayCause::Cut : RelayCause::Partial; }
            Step s = Step::Of(MoveIntent::Hold());
            s.stop = sight.status.traveling;
            return s;
        }
        // 5. The routine cadence: has the target drifted past the edge from the leg's goal?
        bool needDest = !m_haveDest;
        RelayCause cause = RelayCause::First;
        m_routine -= int32(diff);
        if (m_routine <= 0)
        {
            m_routine = int32(m_p.routineMs);
            if (m_haveDest && Drifted(sight)) { needDest = true; cause = RelayCause::Routine; }
        }
        // 6. The event recoveries: a cut or a partial leg (latched, consumed on a tick that moves).
        if (sight.status.partial || sight.status.cut) { m_relayLatch = true; m_latchCause = sight.status.cut ? RelayCause::Cut : RelayCause::Partial; }
        if (m_relayLatch) { needDest = true; cause = m_latchCause; m_relayLatch = false; }
        // 7. A finished leg whose target has drifted (the driver reports arrived once). The
        //    generator had no such case: it caught the same drift on its next 100 ms poll,
        //    which this native's one-second cadence no longer offers, so the design counts
        //    the finished leg as a recovery cause of its own.
        if (!needDest && sight.status.arrived && Drifted(sight)) { needDest = true; cause = RelayCause::Finished; }
        Step s;
        if (needDest) { Derive(sight, svc, cause, s); }
        // 8. The arrival at the spot: once per approach.
        const bool idle = !sight.status.traveling && !sight.status.partial;
        if (idle && !m_reached && !needDest) { m_reached = true; }
        // 9. Nothing changed: hold with the kind's facing; the chase engages while idle.
        if (!needDest && !sight.status.traveling)
        {
            s.apply = true;
            s.intent = MoveIntent::Hold(FacingFor(sight, false));
            if (idle) { OnIdle(sight, s); }
            return s;
        }
        // 10. The leg.
        uint32 flags = MOVE_REQUIRE_PATH;
        if (Walks(sight)) { flags |= MOVE_WALK; }
        if (ForcesDestination(sight)) { flags |= MOVE_FORCE_DEST; }
        s.apply = true;
        s.intent = MoveIntent::Move(m_dest, flags, FacingFor(sight, true));
        return s;
    }

    // ---- ChaseBehaviour -------------------------------------------------------------

    float ChaseBehaviour::StandingDistance(Sight const& sight) const
    {
        return m_p.offset + CONTACT_DISTANCE + sight.target.reachSum;   // retail's stop, design §6.1
    }

    bool ChaseBehaviour::Drifted(Sight const& sight) const
    {
        // The re-approach edge is the client's melee range from the leg's goal; fliers and swimmers care about height.
        const Vector3& goal = sight.status.legGoal;
        const Vector3& t = sight.target.position;
        float d2 = (goal.x - t.x) * (goal.x - t.x) + (goal.y - t.y) * (goal.y - t.y);
        if (sight.canFlyHint || sight.swimming) { d2 += (goal.z - t.z) * (goal.z - t.z); }
        const float edge = m_p.offset + sight.target.meleeRange;
        return d2 > edge * edge;
    }

    Vector3 ChaseBehaviour::AimCentre(Sight const& sight) const
    {
        Vector3 c = sight.target.position;
        if (m_c.lead && sight.target.velocityTrusted)
        {
            c = c + sight.target.velocity * (float(m_c.leadMs) / 1000.0f);
        }
        return c;
    }

    Facing ChaseBehaviour::FacingFor(Sight const& /*sight*/, bool /*moving*/) const
    {
        return m_p.angle == 0.0f ? Facing::ToTarget(m_p.target) : Facing();
    }

    void ChaseBehaviour::OnActivate(Sight const& sight, Step& s)
    {
        if (sight.isCreature) { s.effects.push_back(Effect::Walk(false)); }   // a chase runs
    }

    void ChaseBehaviour::OnIdle(Sight const& /*sight*/, Step& s)
    {
        s.effects.push_back(Effect(Effect::EngageInReach));   // re-emitted every idle tick: a stale miss never latches
    }

    void ChaseBehaviour::OnSuspendOrFinish(std::vector<Effect>& /*effects*/) {}

    // ---- FollowBehaviour ------------------------------------------------------------

    float FollowBehaviour::StandingDistance(Sight const& sight) const
    {
        return m_p.offset + sight.extent + sight.target.extent;
    }

    bool FollowBehaviour::Drifted(Sight const& sight) const
    {
        float allowed = m_f.recalcRange - sight.target.extent + FOLLOW_RECALCULATE_FACTOR * (sight.extent + sight.target.extent);
        if (m_p.offset > FOLLOW_DIST_GAP_FOR_DIST_FACTOR) { allowed += FOLLOW_DIST_RECALCULATE_FACTOR * m_p.offset; }
        const Vector3& goal = sight.status.legGoal;
        const Vector3& t = sight.target.position;
        float d2 = (goal.x - t.x) * (goal.x - t.x) + (goal.y - t.y) * (goal.y - t.y);
        if (sight.canFlyHint || sight.swimming) { d2 += (goal.z - t.z) * (goal.z - t.z); }
        const float maxdist = allowed + sight.target.extent;
        return d2 > maxdist * maxdist;
    }

    Vector3 FollowBehaviour::AimCentre(Sight const& sight) const
    {
        Vector3 c = sight.target.position;
        if (sight.target.velocityTrusted) { c = c + sight.target.velocity * (float(m_f.horizonMs) / 1000.0f); }
        return c;
    }

    Facing FollowBehaviour::FacingFor(Sight const& sight, bool moving) const
    {
        return moving ? Facing() : Facing::ToAngle(sight.target.facing);   // the leader's facing at rest only
    }

    void FollowBehaviour::OnActivate(Sight const& /*sight*/, Step& s)
    {
        s.effects.push_back(Effect(Effect::SyncSpeed));
    }

    void FollowBehaviour::OnSuspendOrFinish(std::vector<Effect>& effects)
    {
        effects.push_back(Effect(Effect::SyncSpeed));
    }
}
