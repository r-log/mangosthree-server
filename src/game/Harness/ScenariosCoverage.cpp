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

#include "Scenario.h"
#include "Harness.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "MotionMaster.h"
#include "Vehicle.h"
#include "movement/MoveSpline.h"
#include "Log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <memory>
#include <vector>

// The coverage scenarios (after P5-A): the feeders the block scenarios never exercised --
// a vehicle seat, a real death and respawn, a possession by a creature charmer, two feign
// auras on one unit (design 2026-09-16-movement-p5-coverage-design.md; reference 9, 11,
// 15.4, 15.5). Every scenario spawns its own actors at SE and ends them.
namespace Harness
{
    namespace
    {
        const uint32 WOLF = 69;
        const uint32 KOBOLD = 6;
        const uint32 DEMOLISHER = 28094;   // Wintergrasp Demolisher, vehicle 106: a driver seat and passenger seats
        const Pt SE = { -3200.0f, -300.0f, 47.0f };
        const uint32 ROOT = 745;           // Web: a plain root aura, about 5 s
        const uint32 FEAR = 5782;          // Fear: the claim's spell id for SetFeared
        const uint32 FEIGN_A = 29266;      // Permanent Feign Death (the dummy family)
        const uint32 FEIGN_B = 37493;      // Permanent Feign Death, a second member of the family

        /// The kernel's reads a category asserts, gathered once per sample.
        struct BlockRead
        {
            bool rooted;       ///< MotionMaster::Inhibited(Rooted)
            bool stunned;      ///< MotionMaster::Inhibited(Stunned)
            bool dead;         ///< MotionMaster::Inhibited(Dead)
            bool possessed;    ///< MotionMaster::Inhibited(Possessed)
            bool mayMove;      ///< Mobility().mayMove
            uint8 reasons;     ///< Mobility().reasons
            size_t rootSources;
            size_t deadSources;
        };

        BlockRead ReadBlock(Creature* c)
        {
            MotionMaster* mm = c->GetMotionMaster();
            const Motion::MobilityDecision d = mm->Mobility();
            BlockRead r = { mm->Inhibited(Motion::Inhibition::Rooted), mm->Inhibited(Motion::Inhibition::Stunned),
                            mm->Inhibited(Motion::Inhibition::Dead), mm->Inhibited(Motion::Inhibition::Possessed),
                            d.mayMove, d.reasons,
                            mm->Arbiter().Sources(Motion::Inhibition::Rooted).size(),
                            mm->Arbiter().Sources(Motion::Inhibition::Dead).size() };
            return r;
        }

        /// S32: a vehicle seat roots its passenger (reference 9.4-9.5; design P5-A 9.10): the
        /// seat is one Rooted source, a point requested on the seat is held, a seat switch
        /// keeps the root with the new seat's source and the seat pose itself, the unboard
        /// releases it and a fresh point plays. A creature takes any seat
        /// (IsUsableSeatForCreature); a passenger seat (index 1 or above) keeps the vehicle's
        /// control seat empty, so no charm forms -- seatRoots asserts that too. A rider's
        /// Where() IS its seat pose (WorldObject::Where(), TransportInfo::SetSeatPose): a
        /// Spread on a seated unit's samples measures the seat pose's drift in the VEHICLE's
        /// frame, not a world displacement. With a stationary vehicle the two happen to
        /// coincide, which is exactly why a seat pose corrupted with world-scale coordinates
        /// (fixed in VehicleInfo::Board -- a rider's world spline, still running the instant it
        /// became a rider, used to land in the seat pose once the seat root's own claim
        /// suspended it) once read heldOnSeat=OK too; seatPoseIsLocal pins the pose itself so
        /// that mistake cannot pass silently again.
        class SeatHoldsPassenger : public Scenario
        {
        public:
            SeatHoldsPassenger() : Scenario("seat-holds-passenger", 32) {}

            void Prepare() override
            {
                Creature* v = Spawn(DEMOLISHER, SE.x, SE.y, Ground(SE.x, SE.y, SE.z), 0.0f);
                Creature* w = Spawn(WOLF, SE.x + 5.0f, SE.y, Ground(SE.x + 5.0f, SE.y, SE.z), 3.1f);
                if (!v || !w) { Verdict(Invalid("spawn failed")); return; }
                const ObjectGuid gv = v->GetObjectGuid(), gw = w->GetObjectGuid();
                auto seat = std::make_shared<int>(-1);
                auto seat2 = std::make_shared<int>(-1);
                auto seatFailReason = std::make_shared<std::string>("not reached");
                auto preBoard = std::make_shared<Pt>();
                auto held = std::make_shared<std::vector<Pt> >();
                auto heldMayMove = std::make_shared<bool>(false);
                auto switchHeld = std::make_shared<std::vector<Pt> >();
                auto after = std::make_shared<std::vector<Pt> >();
                auto seatRoots = std::make_shared<std::string>("INVALID(not reached)");
                auto seatPoseIsLocal = std::make_shared<std::string>("INVALID(not reached)");
                auto switchKeeps = std::make_shared<std::string>("INVALID(not reached)");
                auto unboardReleases = std::make_shared<std::string>("INVALID(not reached)");
                auto unboardPlacesBack = std::make_shared<std::string>("INVALID(not reached)");

                At(500, [this, gw]()
                {
                    // A world spline in flight when it boards at +1000ms: the wolf is still on
                    // its own errand, not yet a rider, so this is the regression guard for the
                    // seat-pose corruption the class comment describes.
                    if (Creature* w = Get(gw)) { w->GetMotionMaster()->MovePoint(9, SE.x + 30.0f, SE.y, Ground(SE.x + 30.0f, SE.y, SE.z)); Log("pre-board point requested (a world spline in flight at boarding), mt=%s", TypeName(w)); }
                });
                At(1000, [this, gv, gw, seat, seatFailReason, preBoard]()
                {
                    Creature* v = Get(gv); Creature* w = Get(gw); if (!v || !w) { return; }
                    VehicleInfo* vi = v->GetVehicleInfo();
                    if (!vi) { Log("no vehicle info on %u", v->GetEntry()); *seatFailReason = "no vehicle info"; return; }
                    Log("vehicle info initialised=%d", vi->IsInitialized() ? 1 : 0);
                    for (uint8 s = 1; s < 8; ++s)
                    {
                        if (vi->IsSeatAvailableFor(w, s)) { *seat = s; break; }
                    }
                    if (*seat < 0) { Log("no passenger seat accepts the wolf"); *seatFailReason = "no passenger seat"; return; }
                    // The wolf's own world spot right before it stops being a world object: the
                    // spline's live position (up to a POSITION_UPDATE_DELAY ahead of the placement),
                    // read WITHOUT stopping it -- the spline must still be in flight when Board runs,
                    // since Board's own stop of it is what seatPoseIsLocal proves. Board reads the
                    // same spot from the stop's pending commit. unboardPlacesBack checks the unboard
                    // returns the wolf here (a stationary vehicle).
                    if (!w->movespline->Finalized())
                    {
                        Movement::Location const loc = w->movespline->ComputePosition();
                        *preBoard = Pt { loc.x, loc.y, loc.z };
                    }
                    else
                    {
                        *preBoard = Pt { w->Where().X(), w->Where().Y(), w->Where().Z() };
                    }
                    vi->Board(w, uint8(*seat));
                    Log("wolf boards seat %d from %.1f %.1f, mt=%s", *seat, preBoard->x, preBoard->y, TypeName(w));
                });
                At(1500, [this, gv, gw, seat, seatRoots]()
                {
                    Creature* v = Get(gv); Creature* w = Get(gw); if (!v || !w || *seat < 0) { return; }
                    const BlockRead r = ReadBlock(w);
                    std::vector<uint64> const& src = w->GetMotionMaster()->Arbiter().Sources(Motion::Inhibition::Rooted);
                    const uint64 want = Motion::InhibitSource(Motion::SourceDomain::Seat, v->GetObjectGuid().GetCounter(), uint32(*seat));
                    const bool noCharm = !r.possessed && v->GetCharmerGuid().IsEmpty();
                    const bool ok = w->IsRooted() && r.rooted && w->hasUnitState(UNIT_STAT_ROOT) && src.size() == 1 && src[0] == want && noCharm;
                    char text[200];
                    snprintf(text, sizeof(text), "%s(rooted=%d state=%d sources=%u possessed=%d charmer=%d)", ok ? "OK" : "BUG",
                             w->IsRooted() ? 1 : 0, w->hasUnitState(UNIT_STAT_ROOT) ? 1 : 0, uint32(src.size()), r.possessed ? 1 : 0, v->GetCharmerGuid().IsEmpty() ? 0 : 1);
                    *seatRoots = text;
                    Log("seated: %s", text);
                });
                At(1500, [this, gw, seat, seatPoseIsLocal]()
                {
                    Creature* w = Get(gw); if (!w || *seat < 0) { return; }
                    TransportInfo* ti = w->GetTransportInfo();
                    if (!ti) { *seatPoseIsLocal = "INVALID(no transport info)"; Log("seatPoseIsLocal: %s", seatPoseIsLocal->c_str()); return; }
                    // OK below ~15 yd: the wolf boards from about 5 yd out and may have walked a
                    // few yards more on its pre-board errand by now; a seat pose corrupted with a
                    // world position reads in the thousands (the bug this pins: see the class
                    // comment).
                    Geometry::Placement const& seatPose = ti->Seat();
                    const float mag = seatPose.Pos().magnitude();
                    const bool ok = mag < 15.0f;
                    char text[80];
                    snprintf(text, sizeof(text), "%s(magnitude %.1f)", ok ? "OK" : "BUG", mag);
                    *seatPoseIsLocal = text;
                    Log("seatPoseIsLocal: %s", text);
                });
                At(2000, [this, gw]()
                {
                    if (Creature* w = Get(gw)) { w->GetMotionMaster()->MovePoint(0, SE.x + 25.0f, SE.y, Ground(SE.x + 25.0f, SE.y, SE.z)); Log("point requested on the seat, mt=%s", TypeName(w)); }
                });
                for (uint32 t = 2500; t <= 5000; t += 500)
                {
                    At(t, [this, gw, held, heldMayMove, t]()
                    {
                        Creature* w = Get(gw); if (!w) { return; }
                        // w->Where() is the seat pose while boarded (class comment): this samples
                        // the rider's drift in the VEHICLE's frame, not the world's.
                        Pt p = { w->Where().X(), w->Where().Y(), w->Where().Z() };
                        held->push_back(p);
                        if (ReadBlock(w).mayMove) { *heldMayMove = true; }
                        Log("on seat +%4ums mt=%s rider(seat) %.1f %.1f mayMove=%d", t, TypeName(w), p.x, p.y, ReadBlock(w).mayMove ? 1 : 0);
                    });
                }
                At(5000, [this, gv, gw, seat, seat2]()
                {
                    Creature* v = Get(gv); Creature* w = Get(gw); if (!v || !w || *seat < 0) { return; }
                    VehicleInfo* vi = v->GetVehicleInfo(); if (!vi) { return; }
                    for (uint8 s = uint8(*seat + 1); s < 8; ++s)
                    {
                        if (vi->IsSeatAvailableFor(w, s)) { *seat2 = s; break; }
                    }
                    if (*seat2 < 0) { Log("single passenger seat, the switch is skipped"); return; }
                    vi->SwitchSeat(w, uint8(*seat2));
                    Log("wolf switches to seat %d", *seat2);
                });
                At(5500, [this, gv, gw, seat, seat2, switchKeeps]()
                {
                    Creature* v = Get(gv); Creature* w = Get(gw); if (!v || !w || *seat < 0) { return; }
                    if (*seat2 < 0) { *switchKeeps = "OK(single seat, switch skipped)"; return; }
                    std::vector<uint64> const& src = w->GetMotionMaster()->Arbiter().Sources(Motion::Inhibition::Rooted);
                    const uint64 want = Motion::InhibitSource(Motion::SourceDomain::Seat, v->GetObjectGuid().GetCounter(), uint32(*seat2));
                    const bool ok = w->IsRooted() && src.size() == 1 && src[0] == want;
                    char text[120];
                    snprintf(text, sizeof(text), "%s(rooted=%d sources=%u seat=%d)", ok ? "OK" : "BUG", w->IsRooted() ? 1 : 0, uint32(src.size()), *seat2);
                    *switchKeeps = text;
                    Log("after the switch: %s", text);
                });
                for (uint32 t = 5500; t <= 6500; t += 500)
                {
                    At(t, [this, gw, switchHeld, t]()
                    {
                        Creature* w = Get(gw); if (!w) { return; }
                        // Still the seat pose: a window between the switch and the unboard, so a
                        // switch that never recomputed the new seat's pose shows up here.
                        Pt p = { w->Where().X(), w->Where().Y(), w->Where().Z() };
                        switchHeld->push_back(p);
                        Log("after switch +%4ums mt=%s rider(seat) %.1f %.1f", t, TypeName(w), p.x, p.y);
                    });
                }
                At(7000, [this, gv, gw, seat]()
                {
                    Creature* v = Get(gv); Creature* w = Get(gw); if (!v || !w || *seat < 0) { return; }
                    if (VehicleInfo* vi = v->GetVehicleInfo()) { vi->UnBoard(w, false); Log("wolf unboards, mt=%s", TypeName(w)); }
                });
                At(7500, [this, gw, seat, unboardReleases]()
                {
                    Creature* w = Get(gw); if (!w || *seat < 0) { return; }
                    const BlockRead r = ReadBlock(w);
                    const bool ok = !r.rooted && !w->IsRooted() && !w->hasUnitState(UNIT_STAT_ROOT) && r.rootSources == 0;
                    char text[120];
                    snprintf(text, sizeof(text), "%s(rooted=%d state=%d sources=%u mt=%s)", ok ? "OK" : "BUG",
                             w->IsRooted() ? 1 : 0, w->hasUnitState(UNIT_STAT_ROOT) ? 1 : 0, uint32(r.rootSources), TypeName(w));
                    *unboardReleases = text;
                    Log("after the unboard: %s", text);
                });
                At(7500, [this, gw, seat, preBoard, unboardPlacesBack]()
                {
                    Creature* w = Get(gw); if (!w || *seat < 0) { return; }
                    // A stationary vehicle's unboard must return the rider to where it boarded
                    // (reference: the re-review's reflection, 15.6 yd on the wrong side before
                    // this was fixed) -- world coordinates this time, not the seat pose.
                    const float d = Dist2(w->Where().X(), w->Where().Y(), preBoard->x, preBoard->y);
                    const bool ok = d < 3.0f;
                    char text[80];
                    snprintf(text, sizeof(text), "%s(dist %.1f)", ok ? "OK" : "BUG", d);
                    *unboardPlacesBack = text;
                    Log("unboardPlacesBack: %s", text);
                });
                At(8000, [this, gw]()
                {
                    if (Creature* w = Get(gw)) { w->GetMotionMaster()->MovePoint(1, SE.x + 25.0f, SE.y + 10.0f, Ground(SE.x + 25.0f, SE.y + 10.0f, SE.z)); Log("fresh point after the unboard, mt=%s", TypeName(w)); }
                });
                for (uint32 t = 8500; t <= 11000; t += 500)
                {
                    At(t, [this, gw, after, t]()
                    {
                        Creature* w = Get(gw); if (!w) { return; }
                        Pt p = { w->Where().X(), w->Where().Y(), w->Where().Z() };
                        after->push_back(p);
                        Log("after +%4ums mt=%s at %.1f %.1f", t, TypeName(w), p.x, p.y);
                    });
                }
                At(11500, [this, gw, seat, seatFailReason, held, heldMayMove, switchHeld, seat2, after, seatRoots, seatPoseIsLocal, switchKeeps, unboardReleases, unboardPlacesBack]()
                {
                    if (*seat < 0) { Verdict(Invalid(seatFailReason->c_str())); return; }
                    if (!Get(gw)) { Verdict(Invalid("lost")); return; }
                    if (held->size() < 3 || after->size() < 3) { Verdict(Invalid("no samples")); return; }
                    const float heldSpread = Spread(*held);
                    const float afterSpread = Spread(*after);
                    std::string switchHoldsSeat;
                    if (*seat2 < 0) { switchHoldsSeat = "OK(single seat, switch skipped)"; }
                    else if (switchHeld->size() < 3) { switchHoldsSeat = "INVALID(no samples)"; }
                    else
                    {
                        const float switchSpread = Spread(*switchHeld);
                        char t2[64];
                        snprintf(t2, sizeof(t2), "%s(spread %.1f)", switchSpread < 0.5f ? "OK" : "BUG", switchSpread);
                        switchHoldsSeat = t2;
                    }
                    char text[600];
                    snprintf(text, sizeof(text), "seatRoots=%s | seatPoseIsLocal=%s | heldOnSeat=%s(spread %.1f, mayMove seen %d) | switchKeepsRoot=%s | switchHoldsSeat=%s | unboardReleases=%s | unboardPlacesBack=%s | movesAfterUnboard=%s(spread %.1f)",
                             seatRoots->c_str(), seatPoseIsLocal->c_str(),
                             (heldSpread < 0.5f && !*heldMayMove) ? "OK" : "BUG", heldSpread, *heldMayMove ? 1 : 0,
                             switchKeeps->c_str(), switchHoldsSeat.c_str(), unboardReleases->c_str(), unboardPlacesBack->c_str(),
                             afterSpread > 3.0f ? "OK" : "BUG", afterSpread);
                    Verdict(text);
                });
            }

        private:
            static std::string Invalid(char const* why)
            {
                std::string w = std::string("INVALID(") + why + ")";
                return "seatRoots=" + w + " | seatPoseIsLocal=" + w + " | heldOnSeat=" + w + " | switchKeepsRoot=" + w + " | switchHoldsSeat=" + w + " | unboardReleases=" + w + " | unboardPlacesBack=" + w + " | movesAfterUnboard=" + w;
            }
        };

        /// S33: a death drops the aura and script sources and holds the body; the respawn is
        /// clean and moves (reference 11.1, 11.5; design P5-A 9.14). Three sources go on a
        /// chasing wolf (a Web, a script root, a script stun); the wolf is killed; Rooted and
        /// Stunned are gone at once and Dead holds with the death source alone; Respawn()
        /// restamps the wall-clock respawn time so the next tick respawns it, after which no
        /// reason and no source remain and a point plays.
        class DeathDropsSources : public Scenario
        {
        public:
            DeathDropsSources() : Scenario("death-drops-sources", 33) {}

            void Prepare() override
            {
                Creature* w = Spawn(WOLF, SE.x, SE.y, Ground(SE.x, SE.y, SE.z), 0.0f);
                Creature* k = Spawn(KOBOLD, SE.x + 20.0f, SE.y, Ground(SE.x + 20.0f, SE.y, SE.z), 3.1f);
                if (!w || !k) { Verdict(Invalid("spawn failed")); return; }
                const ObjectGuid gw = w->GetObjectGuid(), gk = k->GetObjectGuid();
                auto sourcesHeld = std::make_shared<std::string>("INVALID(not reached)");
                auto deathDrops = std::make_shared<std::string>("INVALID(not reached)");
                auto respawnClean = std::make_shared<std::string>("INVALID(not reached)");
                auto dead = std::make_shared<std::vector<Pt> >();
                auto after = std::make_shared<std::vector<Pt> >();
                At(500, [this, gw, gk]()
                {
                    Creature* w = Get(gw); Creature* k = Get(gk); if (!w || !k) { return; }
                    w->Attack(k, true);
                    w->AddThreat(k, 1000.0f);
                    w->GetMotionMaster()->MoveChase(k, 0.0f, 0.0f);
                    Log("Attack + MoveChase from 20 yd, mt=%s", TypeName(w));
                });
                At(1000, [this, gw]()
                {
                    Creature* w = Get(gw); if (!w) { return; }
                    w->CastSpell(w, ROOT, true);
                    w->GetMotionMaster()->Inhibit(Motion::Inhibition::Rooted, Motion::InhibitSource(Motion::SourceDomain::Script, w->GetObjectGuid().GetCounter(), 33));
                    w->GetMotionMaster()->Inhibit(Motion::Inhibition::Stunned, Motion::InhibitSource(Motion::SourceDomain::Script, w->GetObjectGuid().GetCounter(), 33));
                    Log("a Web, a script root and a script stun, mt=%s", TypeName(w));
                });
                At(1500, [this, gw, sourcesHeld]()
                {
                    Creature* w = Get(gw); if (!w) { return; }
                    const BlockRead r = ReadBlock(w);
                    const bool ok = r.rootSources == 2 && r.stunned && r.rooted;
                    char text[120];
                    snprintf(text, sizeof(text), "%s(rootSources=%u stunned=%d)", ok ? "OK" : "BUG", uint32(r.rootSources), r.stunned ? 1 : 0);
                    *sourcesHeld = text;
                    Log("held: %s", text);
                });
                At(2000, [this, gw]()
                {
                    Creature* w = Get(gw); if (!w) { return; }
                    w->DealDamage(w, w->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
                    Log("killed: alive=%d state=%d mt=%s", w->IsAlive() ? 1 : 0, int(w->GetDeathState()), TypeName(w));
                });
                At(2500, [this, gw, deathDrops]()
                {
                    Creature* w = Get(gw); if (!w) { return; }
                    const BlockRead r = ReadBlock(w);
                    std::vector<uint64> const& src = w->GetMotionMaster()->Arbiter().Sources(Motion::Inhibition::Dead);
                    const bool ok = !w->IsAlive() && r.dead && !r.rooted && !r.stunned && src.size() == 1 && src[0] == Motion::kDeathSource && r.reasons == Motion::ReasonDead;
                    char text[160];
                    snprintf(text, sizeof(text), "%s(alive=%d dead=%d rooted=%d stunned=%d deadSources=%u reasons=%u)", ok ? "OK" : "BUG",
                             w->IsAlive() ? 1 : 0, r.dead ? 1 : 0, r.rooted ? 1 : 0, r.stunned ? 1 : 0, uint32(src.size()), uint32(r.reasons));
                    *deathDrops = text;
                    Log("after death: %s", text);
                });
                for (uint32 t = 2500; t <= 4000; t += 500)
                {
                    At(t, [this, gw, dead, t]()
                    {
                        Creature* w = Get(gw); if (!w) { return; }
                        Pt p = { w->Where().X(), w->Where().Y(), w->Where().Z() };
                        dead->push_back(p);
                        Log("dead +%4ums state=%d at %.1f %.1f", t, int(w->GetDeathState()), p.x, p.y);
                    });
                }
                At(4000, [this, gw]()
                {
                    Creature* w = Get(gw); if (!w) { return; }
                    w->Respawn();
                    Log("Respawn() called: state=%d", int(w->GetDeathState()));
                });
                At(4500, [this, gw, respawnClean]()
                {
                    Creature* w = Get(gw); if (!w) { return; }
                    if (!w->IsAlive())
                    {
                        char text[80];
                        snprintf(text, sizeof(text), "INVALID(no respawn: state %d)", int(w->GetDeathState()));
                        *respawnClean = text;
                        Log("%s", text);
                        return;
                    }
                    const BlockRead r = ReadBlock(w);
                    MotionMaster* mm = w->GetMotionMaster();
                    const bool noSources = mm->Arbiter().Sources(Motion::Inhibition::Rooted).empty() && mm->Arbiter().Sources(Motion::Inhibition::Stunned).empty() &&
                                           mm->Arbiter().Sources(Motion::Inhibition::Dead).empty() && mm->Arbiter().Sources(Motion::Inhibition::Possessed).empty();
                    const bool noState = (w->GetUnitState() & (UNIT_STAT_ROOT | UNIT_STAT_STUNNED | UNIT_STAT_DIED)) == 0;
                    const bool ok = !r.dead && r.reasons == 0 && noSources && noState;
                    char text[160];
                    snprintf(text, sizeof(text), "%s(alive dead=%d reasons=%u sources=%d state=%d mt=%s)", ok ? "OK" : "BUG",
                             r.dead ? 1 : 0, uint32(r.reasons), noSources ? 0 : 1, noState ? 0 : 1, TypeName(w));
                    *respawnClean = text;
                    Log("after respawn: %s", text);
                });
                At(5000, [this, gw]()
                {
                    Creature* w = Get(gw); if (!w || !w->IsAlive()) { return; }
                    w->GetMotionMaster()->MovePoint(2, SE.x + 25.0f, SE.y + 10.0f, Ground(SE.x + 25.0f, SE.y + 10.0f, SE.z));
                    Log("point after the respawn, mt=%s", TypeName(w));
                });
                for (uint32 t = 5500; t <= 8000; t += 500)
                {
                    At(t, [this, gw, after, t]()
                    {
                        Creature* w = Get(gw); if (!w) { return; }
                        Pt p = { w->Where().X(), w->Where().Y(), w->Where().Z() };
                        after->push_back(p);
                        Log("after +%4ums mt=%s at %.1f %.1f", t, TypeName(w), p.x, p.y);
                    });
                }
                At(8500, [this, gw, sourcesHeld, deathDrops, respawnClean, dead, after]()
                {
                    if (!Get(gw)) { Verdict(Invalid("lost")); return; }
                    if (dead->size() < 3 || after->size() < 3) { Verdict(Invalid("no samples")); return; }
                    const float deadSpread = Spread(*dead);
                    const float afterSpread = Spread(*after);
                    const bool respawned = respawnClean->compare(0, 7, "INVALID") != 0;
                    char text[420];
                    snprintf(text, sizeof(text), "sourcesHeld=%s | deathDropsAuraAndScript=%s | deadStands=%s(spread %.1f) | respawnClean=%s | movesAfterRespawn=%s(spread %.1f)",
                             sourcesHeld->c_str(), deathDrops->c_str(),
                             deadSpread < 0.5f ? "OK" : "BUG", deadSpread,
                             respawnClean->c_str(),
                             !respawned ? "INVALID" : (afterSpread > 3.0f ? "OK" : "BUG"), afterSpread);
                    Verdict(text);
                });
            }

        private:
            static std::string Invalid(char const* why)
            {
                std::string w = std::string("INVALID(") + why + ")";
                return "sourcesHeld=" + w + " | deathDropsAuraAndScript=" + w + " | deadStands=" + w + " | respawnClean=" + w + " | movesAfterRespawn=" + w;
            }
        };
    }

    void RegisterCoverageScenarios(Runner& r)
    {
        r.Register(new SeatHoldsPassenger());
        r.Register(new DeathDropsSources());
    }
}
