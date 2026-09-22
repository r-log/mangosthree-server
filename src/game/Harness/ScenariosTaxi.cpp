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
#include "HarnessAI.h"
#include "Creature.h"
#include "Pet.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "MotionMaster.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "WorldSession.h"
#include "World.h"
#include "DBCStores.h"
#include "Log.h"
#include "TaxiRoute.h"
#include "movement/MoveSpline.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// The taxi-contract family (design/2026-09-22-taxi-contract-and-pets-design.md), orders 908-911,
// all four of them player scenarios and therefore at the tail of the 900 block where nothing they
// do can shift a scenario ahead of them.
namespace Harness
{
    namespace
    {
        const Pt P0 = { -3122.6f, -261.3f, 46.0f };   // Mulgore, the plain every family starts on

        // ---- the route ------------------------------------------------------------------
        //
        // TaxiPath 1957, node 402 (Bloodhoof Village, Mulgore) -> node 22 (Thunder Bluff,
        // Mulgore). Read out of the DBC rather than picked for convenience: 21 TaxiPathNode rows,
        // EVERY ONE of them ContinentID 1, 1 488 yd end to end, which is 50 s at the 30 yd/s
        // Movement.TaxiSpeed. All on the harness's own map, so nothing here can make the runner
        // create a second one; and node 402 carries MountCreatureID[0] = 2224, so the mount
        // lookup answers for the harness's human warrior through its alt-team fallback and the
        // takeoff has a real display id to write.
        const uint32 SOURCE_NODE = 402;
        const uint32 DEST_NODE   = 22;

        /// The mount display the takeoff writes, 0 when the DBC or the world database cannot
        /// answer -- which every verdict below reports rather than passing over.
        uint32 TaxiMount(Player* p)
        {
            return sObjectMgr.GetTaxiMountDisplayId(SOURCE_NODE, p->GetTeam(), true);
        }

        /// Both halves of the client's mover membership, as ScenariosControl.cpp reads them.
        bool OwnMover(Player* p)
        {
            WorldSession* s = p->GetSession();
            return s && s->Movers().IsMember(p->GetObjectGuid().GetRawValue()) && p->MoverSession() == s;
        }

        /// The flight put on him through the session's own entry point, which is what
        /// Player::ActivateTaxiPathTo ends with: the route into m_taxi, then SendDoFlight.
        /// The kernel activates on its next update, so the takeoff lands a tick later.
        void Board(Player* p, uint32 startNode = 0)
        {
            p->m_taxi.ClearTaxiDestinations();
            p->m_taxi.AddTaxiDestination(SOURCE_NODE);
            p->m_taxi.AddTaxiDestination(DEST_NODE);
            p->GetSession()->SendDoFlight(TaxiMount(p), p->m_taxi.GetTaxiDestinations(), startNode);
        }

    }

    /**
     * S68 (order 908): a dead passenger must not read as taxi-flying.
     *
     * WHAT THIS PINS, AND WHAT IT CANNOT. The flight IS unwound on the death path today, at the
     * very end of it: Unit::SetDeathState's i_motionMaster.Die() finishes the kernel's Taxi entry
     * and its Abort effect reaches Player::TaxiAbort. What the change moves is the ORDER -- the
     * unwind now runs at the TOP of Player::SetDeathState's JUST_DIED branch, in front of its own
     * RemovePet and in front of everything Unit::SetDeathState does -- and the harness cannot see
     * between two statements of one call. So the end state below reads the same with the change
     * reverted, and this scenario is a CONTRACT PIN, not a regression catcher: it says that a dead
     * passenger carries no taxi state and keeps his own mover, and it would fail loudly if either
     * ever stopped being true. The consequence the ordering actually fixes -- TaxiAbort's
     * ResummonPetTemporaryUnSummonedIfAny landing behind RemovePet, on a body whose m_deathState
     * is still ALIVE, and loading the passenger's pet back out of the database onto his corpse --
     * needs a database-backed pet and is named for the live checklist instead.
     *
     * The death is driven as SetDeathState(JUST_DIED) directly, which is the function under test.
     * Unit::Kill's loot, durability and PvP machinery is deliberately NOT on the path: it would
     * write against a character row that does not exist, and none of it is what this is about.
     */
    class TaxiDeathClearsTheFlight : public Scenario
    {
    public:
        TaxiDeathClearsTheFlight() : Scenario("taxi-death-clears-the-flight", 908) {}
        bool UsesPlayer() const override { return true; }

        void Prepare() override
        {
            struct St
            {
                bool   boarded = false;      ///< the route went in and SendDoFlight was called
                uint32 mount = 0;            ///< the display id the takeoff had to write
                uint32 upSamples = 0;        ///< samples before the death with the flight whole
                uint32 notUp = 0;            ///< ...of which any that were not
                bool   killRan = false;
                bool   flyingBefore = false; ///< IsTaxiFlying() the instant before the death
                bool   onTaxiBefore = false; ///< the kernel's Taxi entry, likewise
                bool   moverBefore = false;
                uint32 flagsBefore = 0;
                uint32 mountBefore = 0;
                bool   flyingAfter = false;  ///< every reading below taken the instant it returned
                bool   onTaxiAfter = false;
                bool   routeAfter = true;
                uint32 flagsAfter = 0;
                uint32 mountAfter = 0;
                bool   aliveAfter = true;
                bool   moverAfter = false;
                uint32 afterSamples = 0;
                uint32 taxiBack = 0;         ///< samples after the death on which any taxi state stood again
                uint32 moverLost = 0;        ///< ...on which the ghost did not have his own mover
                uint32 aliveAgain = 0;
            };

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            const ObjectGuid g = p->GetObjectGuid();
            auto st = std::make_shared<St>();

            At(500, [this, g, st]()
            {
                Player* p = sPlayerRegistry.Find(g); if (!p) { return; }
                st->mount = TaxiMount(p);
                Board(p);
                st->boarded = true;
                Log(" 500ms boarded %u -> %u, mount display %u: onTaxi=%d",
                    SOURCE_NODE, DEST_NODE, st->mount, p->GetMotionMaster()->IsOnTaxi() ? 1 : 0);
            });
            for (uint32 t = 700; t < 2500; t += 100)
            {
                At(t, [this, g, st, t]()
                {
                    Player* p = sPlayerRegistry.Find(g); if (!p) { return; }
                    ++st->upSamples;
                    const bool whole = p->IsTaxiFlying() &&
                                       p->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_TAXI_FLIGHT) &&
                                       p->GetMotionMaster()->IsOnTaxi();
                    if (!whole) { ++st->notUp; }
                    if (t % 500 == 0)
                    {
                        Log("%4ums in the air: flying=%d onTaxi=%d flags 0x%08x mount %u own mover=%d at (%.0f, %.0f, %.0f)",
                            t, p->IsTaxiFlying() ? 1 : 0, p->GetMotionMaster()->IsOnTaxi() ? 1 : 0,
                            p->GetUInt32Value(UNIT_FIELD_FLAGS), p->GetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID),
                            OwnMover(p) ? 1 : 0, p->Where().X(), p->Where().Y(), p->Where().Z());
                    }
                });
            }
            At(2500, [this, g, st]()
            {
                Player* p = sPlayerRegistry.Find(g); if (!p) { return; }
                st->flyingBefore = p->IsTaxiFlying();
                st->onTaxiBefore = p->GetMotionMaster()->IsOnTaxi();
                st->moverBefore = OwnMover(p);
                st->flagsBefore = p->GetUInt32Value(UNIT_FIELD_FLAGS);
                st->mountBefore = p->GetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID);
                // The health first so the body the death leaves behind is a real corpse, then the
                // one call this item changes. EVERY READING BELOW IS TAKEN IN THIS STEP: the
                // whole unwind happens inside SetDeathState and there is no later moment at which
                // the two orderings could still be told apart.
                st->killRan = true;
                p->SetHealth(0);
                p->SetDeathState(JUST_DIED);
                st->flyingAfter = p->IsTaxiFlying();
                st->onTaxiAfter = p->GetMotionMaster()->IsOnTaxi();
                st->routeAfter = !p->m_taxi.empty();
                st->flagsAfter = p->GetUInt32Value(UNIT_FIELD_FLAGS);
                st->mountAfter = p->GetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID);
                st->aliveAfter = p->IsAlive();
                st->moverAfter = OwnMover(p);
                Log("2500ms THE DEATH: flying %d -> %d, onTaxi %d -> %d, flags 0x%08x -> 0x%08x, mount %u -> %u, route left=%d, alive=%d, own mover %d -> %d",
                    st->flyingBefore ? 1 : 0, st->flyingAfter ? 1 : 0,
                    st->onTaxiBefore ? 1 : 0, st->onTaxiAfter ? 1 : 0,
                    st->flagsBefore, st->flagsAfter, st->mountBefore, st->mountAfter,
                    st->routeAfter ? 1 : 0, st->aliveAfter ? 1 : 0,
                    st->moverBefore ? 1 : 0, st->moverAfter ? 1 : 0);
            });
            for (uint32 t = 2600; t <= 5500; t += 100)
            {
                At(t, [this, g, st, t]()
                {
                    Player* p = sPlayerRegistry.Find(g); if (!p) { return; }
                    ++st->afterSamples;
                    if (p->IsTaxiFlying() || p->GetMotionMaster()->IsOnTaxi() || !p->m_taxi.empty() ||
                        p->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_TAXI_FLIGHT) ||
                        p->GetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID))
                    {
                        ++st->taxiBack;
                    }
                    if (!OwnMover(p)) { ++st->moverLost; }
                    if (p->IsAlive()) { ++st->aliveAgain; }
                    if (t % 1000 == 0)
                    {
                        Log("%4ums dead: flying=%d onTaxi=%d route=%d flags 0x%08x mount %u own mover=%d alive=%d",
                            t, p->IsTaxiFlying() ? 1 : 0, p->GetMotionMaster()->IsOnTaxi() ? 1 : 0,
                            p->m_taxi.empty() ? 0 : 1, p->GetUInt32Value(UNIT_FIELD_FLAGS),
                            p->GetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID), OwnMover(p) ? 1 : 0,
                            p->IsAlive() ? 1 : 0);
                    }
                });
            }
            At(6000, [this, st]()
            {
                char flight[352], gone[448], mover[288];
                if (!st->boarded || !st->killRan)
                {
                    snprintf(flight, sizeof(flight), "INVALID(the boarding or the death step never ran: boarded=%d killed=%d)", st->boarded ? 1 : 0, st->killRan ? 1 : 0);
                    snprintf(gone, sizeof(gone), "INVALID(the boarding or the death step never ran)");
                    snprintf(mover, sizeof(mover), "INVALID(the boarding or the death step never ran)");
                }
                else
                {
                    // --- flightUpBeforeTheDeath: the precondition, and the only thing that keeps
                    // the two categories below from passing on a player who never flew.
                    if (!st->mount)
                    {
                        snprintf(flight, sizeof(flight), "INVALID(node %u answered no mount display for this team, so the takeoff had none to write and the flight is not the one the design describes)", SOURCE_NODE);
                    }
                    else if (!st->upSamples)
                    {
                        snprintf(flight, sizeof(flight), "INVALID(no sample was taken between the boarding and the death)");
                    }
                    else if (st->notUp)
                    {
                        snprintf(flight, sizeof(flight), "BUG(the flight was not whole on %u of the %u samples before the death: flying, the two flags and the kernel's Taxi entry are all required)", st->notUp, st->upSamples);
                    }
                    else if (!st->flyingBefore || !st->onTaxiBefore || !st->mountBefore)
                    {
                        snprintf(flight, sizeof(flight), "BUG(the instant before the death he was flying=%d onTaxi=%d mount=%u)", st->flyingBefore ? 1 : 0, st->onTaxiBefore ? 1 : 0, st->mountBefore);
                    }
                    else
                    {
                        snprintf(flight, sizeof(flight), "OK(flying with flags 0x%08x and mount display %u on all %u samples up to the death and the instant before it)", st->flagsBefore, st->mountBefore, st->upSamples);
                    }
                    // --- taxiGoneAtDeath: the contract. Read the instant SetDeathState returned
                    // and on every sample after it.
                    if (!st->flyingBefore || !st->onTaxiBefore)
                    {
                        snprintf(gone, sizeof(gone), "INVALID(he was not flying into the death, so there was no flight for it to unwind)");
                    }
                    else if (st->flyingAfter || st->onTaxiAfter || st->routeAfter ||
                             (st->flagsAfter & (UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_TAXI_FLIGHT)) || st->mountAfter)
                    {
                        snprintf(gone, sizeof(gone), "BUG(the instant the death returned he still read flying=%d onTaxi=%d route=%d flags 0x%08x mount=%u)",
                                 st->flyingAfter ? 1 : 0, st->onTaxiAfter ? 1 : 0, st->routeAfter ? 1 : 0, st->flagsAfter, st->mountAfter);
                    }
                    else if (st->aliveAfter)
                    {
                        snprintf(gone, sizeof(gone), "BUG(the taxi state went but he was still alive the instant the death returned, so this is not a death)");
                    }
                    else if (st->afterSamples < 10)
                    {
                        snprintf(gone, sizeof(gone), "INVALID(only %u samples after the death)", st->afterSamples);
                    }
                    else if (st->taxiBack || st->aliveAgain)
                    {
                        snprintf(gone, sizeof(gone), "BUG(taxi state stood again on %u and he read alive on %u of the %u samples after the death)", st->taxiBack, st->aliveAgain, st->afterSamples);
                    }
                    else
                    {
                        snprintf(gone, sizeof(gone), "OK(the instant the death returned: not flying, no Taxi entry, no route, flags 0x%08x, mount 0, dead -- and no taxi state on any of the %u samples after it)", st->flagsAfter, st->afterSamples);
                    }
                    // --- ghostKeepsItsMover: what TaxiAbort's own SetClientControl(this, 1) is
                    // for. No death or repop path grants the mover, so a ghost whose takeoff
                    // revoked it and whose unwind did not give it back could not move at all.
                    if (st->moverBefore)
                    {
                        // The takeoff revokes the mover; if he still had it going into the death
                        // there was nothing for the unwind's grant to give back, and an OK below
                        // would be true for the wrong reason.
                        snprintf(mover, sizeof(mover), "INVALID(he still had his own mover while flying, so the takeoff's revoke did not hold and the grant proves nothing)");
                    }
                    else if (!st->moverAfter)
                    {
                        snprintf(mover, sizeof(mover), "BUG(the ghost did not get his mover back the instant the death returned: the takeoff revoked it and nothing on the death or repop path grants it)");
                    }
                    else if (st->moverLost)
                    {
                        snprintf(mover, sizeof(mover), "BUG(the ghost was without his own mover on %u of the %u samples after the death)", st->moverLost, st->afterSamples);
                    }
                    else
                    {
                        snprintf(mover, sizeof(mover), "OK(revoked while flying, back the instant the death returned, and held on all %u samples after it)", st->afterSamples);
                    }
                }
                Verdict(std::string("flightUpBeforeTheDeath=") + flight + " | taxiGoneAtDeath=" + gone +
                        " | ghostKeepsItsMover=" + mover);
            });
        }

    private:
        static std::string Invalid(char const* why)
        {
            std::string w = std::string("INVALID(") + why + ")";
            return "flightUpBeforeTheDeath=" + w + " | taxiGoneAtDeath=" + w + " | ghostKeepsItsMover=" + w;
        }
    };

    void RegisterTaxiScenarios(Runner& r)
    {
        r.Register(new TaxiDeathClearsTheFlight());
    }
}
