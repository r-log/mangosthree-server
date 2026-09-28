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

/// Decoupling D4k: a character's pet-ownership state -- the stable-slot count, the pet a temporary
/// unsummon put away, the packet that clears the pet action bar -- with no character and no pet.
///
/// Before this PR PetMgr held a pointer to its owner, found the owner's live pet and read it,
/// unsummoned it, and loaded a new one. Now the live pet comes in as plain facts (LivePet), the
/// owner's two facts and the load as ResummonInputs, and the unsummon, the load and the packet go
/// out through callbacks. A Wire records what the callbacks received, in call order, and -- the
/// point of the order rows -- the manager's pending number AT THE MOMENT each callback ran: the
/// pet's unsummon and the pet's save read that number (Pet::Unsummon, Pet::SavePetToDB), so it has
/// to be recorded before the unsummon and cleared only after the load.
///
/// Values are chosen so that no two outputs can be confused: the pet numbers 4242, 7 and 99 are
/// distinct from each other, from the stable-slot values and from the save modes (0 and 100).

#include "TestHarness.h"
#include "PetMgr.h"
#include "SharedDefines.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <string>
#include <vector>

namespace
{
    const uint32 kPetNumber = 4242;     ///< the live pet's number
    const uint32 kPending   = 7;        ///< a number an earlier unsummon left pending

    // The opcode and the save modes the rows below expect, pinned to their values: a row that
    // compares against a constant from the same header would not see that header change.
    static_assert(SMSG_PET_SPELLS == 0x4114, "SMSG_PET_SPELLS is 0x4114 in 4.3.4 15595");
    static_assert(PET_SAVE_AS_CURRENT == 0, "PET_SAVE_AS_CURRENT is 0");
    static_assert(PET_SAVE_NOT_IN_SLOT == 100, "PET_SAVE_NOT_IN_SLOT is 100");
    static_assert(MAX_PET_STABLES == 5, "MAX_PET_STABLES is 5");

    std::string PacketText(WorldPacket const& packet)
    {
        static const char* digits = "0123456789abcdef";
        std::string text;
        uint16 opcode = packet.GetOpcode();
        for (int shift = 12; shift >= 0; shift -= 4)
        {
            text += digits[(opcode >> shift) & 0x0F];
        }
        text += ":";
        text += testing::BytesToHex(packet.contents(), packet.size());
        return text;
    }

    /// What the callbacks received, in call order, each with the manager's pending number at
    /// that moment ("seen").
    struct Wire
    {
        PetMgr& mgr;
        std::vector<std::string> events;

        explicit Wire(PetMgr& m) : mgr(m) {}

        PetMgr::UnsummonSink Unsummon()
        {
            return [this](PetSaveMode mode)
            {
                events.push_back("unsummon mode " + std::to_string(int(mode)) + " seen "
                                 + std::to_string(mgr.GetTemporaryUnsummonedPetNumber()));
            };
        }

        PetMgr::PetLoadSink Load()
        {
            return [this](uint32 petNumber)
            {
                events.push_back("load " + std::to_string(petNumber) + " seen "
                                 + std::to_string(mgr.GetTemporaryUnsummonedPetNumber()));
            };
        }

        ManagerPacketSink Send()
        {
            return [this](WorldPacket const* packet)
            {
                events.push_back("packet " + PacketText(*packet));
            };
        }

        std::string Text() const
        {
            std::string text;
            for (size_t i = 0; i < events.size(); ++i)
            {
                text += (i ? "; " : "") + events[i];
            }
            return text;
        }
    };

    PetMgr::LivePet Facts(bool present, bool controlled, bool temporarySummoned, uint32 petNumber)
    {
        PetMgr::LivePet pet;
        pet.present = present;
        pet.controlled = controlled;
        pet.temporarySummoned = temporarySummoned;
        pet.petNumber = petNumber;
        return pet;
    }

    PetMgr::ResummonInputs Resummon(Wire& wire, bool needTemporaryUnsummon, bool petGuidSet)
    {
        PetMgr::ResummonInputs inputs;
        inputs.needTemporaryUnsummon = needTemporaryUnsummon;
        inputs.petGuidSet = petGuidSet;
        inputs.load = wire.Load();
        return inputs;
    }
}

// A default-constructed manager: five stable slots (Cata's free slots), nothing pending, no pet
// rows -- the state the owner's constructor gave it before, now from the in-class initialisers.
TEST(PetMgr_DefaultStateAndAccessors)
{
    PetMgr mgr;
    CHECK_EQ(mgr.GetStableSlots(), uint32(5));
    CHECK_EQ(mgr.GetTemporaryUnsummonedPetNumber(), uint32(0));
    CHECK_EQ(mgr.GetPetCache().RowCount(), size_t(0));

    mgr.SetStableSlots(3);
    CHECK_EQ(mgr.GetStableSlots(), uint32(3));
    mgr.SetTemporaryUnsummonedPetNumber(kPending);
    CHECK_EQ(mgr.GetTemporaryUnsummonedPetNumber(), kPending);
    CHECK_EQ(mgr.GetStableSlots(), uint32(3));      // the two fields are apart
}

// The raw `stable_slots` column gives MAX_PET_STABLES whatever it holds: 0 (the pre-Cata default)
// and 1..4 are raised, 5 is kept, anything above is lowered (and logged -- not observed here).
// Each load overwrites a value set before it.
TEST(PetMgr_LoadStableSlotsFromFieldIsAlwaysTheMax)
{
    const uint32 raws[] = { 0, 1, 4, 5, 6, 200, 0xFFFFFFFF };
    for (uint32 raw : raws)
    {
        PetMgr mgr;
        mgr.SetStableSlots(2);
        mgr.LoadStableSlotsFromField(raw);
        CHECK_EQ(mgr.GetStableSlots(), uint32(5));
        CHECK_EQ(mgr.GetTemporaryUnsummonedPetNumber(), uint32(0));    // touches nothing else
    }
}

// SMSG_PET_SPELLS with an empty guid: opcode 0x4114, eight zero bytes (the guid as a uint64),
// one packet per call.
TEST(PetMgr_RemoveActionBarBytes)
{
    PetMgr mgr;
    Wire wire(mgr);
    mgr.RemoveActionBar(wire.Send());
    CHECK_STR(wire.Text(), "packet 4114:0000000000000000");

    mgr.RemoveActionBar(wire.Send());
    CHECK_EQ(wire.events.size(), size_t(2));
}

// No pet out: nothing is recorded and nothing is unsummoned, whatever the other facts say and
// whether or not a number is already pending.
TEST(PetMgr_UnsummonTemporaryWithNoPetDoesNothing)
{
    {
        PetMgr mgr;
        Wire wire(mgr);
        mgr.UnsummonTemporaryIfAny(Facts(false, true, false, kPetNumber), wire.Unsummon());
        CHECK_STR(wire.Text(), "");
        CHECK_EQ(mgr.GetTemporaryUnsummonedPetNumber(), uint32(0));
    }
    {
        PetMgr mgr;
        mgr.SetTemporaryUnsummonedPetNumber(kPending);
        Wire wire(mgr);
        mgr.UnsummonTemporaryIfAny(Facts(false, true, false, kPetNumber), wire.Unsummon());
        CHECK_STR(wire.Text(), "");
        CHECK_EQ(mgr.GetTemporaryUnsummonedPetNumber(), kPending);
    }
}

// A controlled pet that is not a temporary summon, with nothing pending: its number is recorded,
// and recorded BEFORE the unsummon (which reads it), which is made once with PET_SAVE_AS_CURRENT.
TEST(PetMgr_UnsummonTemporaryRecordsACandidateBeforeTheUnsummon)
{
    PetMgr mgr;
    Wire wire(mgr);
    mgr.UnsummonTemporaryIfAny(Facts(true, true, false, kPetNumber), wire.Unsummon());
    CHECK_STR(wire.Text(), "unsummon mode 0 seen 4242");
    CHECK_EQ(mgr.GetTemporaryUnsummonedPetNumber(), kPetNumber);
}

// Every other pet is unsummoned the same way but not recorded: one that is not controlled, one
// that is a temporary summon, one that is both; and a candidate while a number is already
// pending keeps the pending one (the unsummon sees it).
TEST(PetMgr_UnsummonTemporaryNonCandidatesAreUnsummonedNotRecorded)
{
    struct Row { bool controlled; bool temporarySummoned; uint32 pending; char const* events; uint32 after; };
    const Row rows[] =
    {
        { false, false, 0,        "unsummon mode 0 seen 0", 0 },
        { true,  true,  0,        "unsummon mode 0 seen 0", 0 },
        { false, true,  0,        "unsummon mode 0 seen 0", 0 },
        { true,  false, kPending, "unsummon mode 0 seen 7", kPending },
        { false, false, kPending, "unsummon mode 0 seen 7", kPending },
    };
    for (Row const& row : rows)
    {
        PetMgr mgr;
        mgr.SetTemporaryUnsummonedPetNumber(row.pending);
        Wire wire(mgr);
        mgr.UnsummonTemporaryIfAny(Facts(true, row.controlled, row.temporarySummoned, kPetNumber), wire.Unsummon());
        CHECK_STR(wire.Text(), row.events);
        CHECK_EQ(mgr.GetTemporaryUnsummonedPetNumber(), row.after);
    }
}

// The resummon over every combination of its three conditions. Only "a number pending, the
// owner may have a pet out, it has none" loads -- with the pending number, which the load still
// sees -- and then clears it. "Not yet" (a number pending but the owner may not have a pet out,
// or already has one) keeps the number and loads nothing. With nothing pending nothing happens.
TEST(PetMgr_ResummonDecisionTable)
{
    struct Row { bool pending; bool need; bool guidSet; char const* events; uint32 after; };
    const Row rows[] =
    {
        { true,  false, false, "load 4242 seen 4242", 0 },
        { true,  true,  false, "",                    kPetNumber },
        { true,  false, true,  "",                    kPetNumber },
        { true,  true,  true,  "",                    kPetNumber },
        { false, false, false, "",                    0 },
        { false, true,  false, "",                    0 },
        { false, false, true,  "",                    0 },
        { false, true,  true,  "",                    0 },
    };
    for (Row const& row : rows)
    {
        PetMgr mgr;
        mgr.SetTemporaryUnsummonedPetNumber(row.pending ? kPetNumber : 0);
        Wire wire(mgr);
        mgr.ResummonTemporaryUnsummonedIfAny(Resummon(wire, row.need, row.guidSet));
        CHECK_STR(wire.Text(), row.events);
        CHECK_EQ(mgr.GetTemporaryUnsummonedPetNumber(), row.after);
    }
}

// The clear comes AFTER the load, whatever the load did: a load that writes a number of its own
// (the load path reaching the setter) is still followed by the clear.
TEST(PetMgr_ResummonClearsAfterTheLoad)
{
    PetMgr mgr;
    mgr.SetTemporaryUnsummonedPetNumber(kPetNumber);
    std::vector<std::string> seen;
    PetMgr::ResummonInputs inputs;
    inputs.load = [&mgr, &seen](uint32 petNumber)
    {
        seen.push_back("load " + std::to_string(petNumber));
        mgr.SetTemporaryUnsummonedPetNumber(99);
    };
    mgr.ResummonTemporaryUnsummonedIfAny(inputs);
    REQUIRE(seen.size() == 1);
    CHECK_STR(seen[0], "load 4242");
    CHECK_EQ(mgr.GetTemporaryUnsummonedPetNumber(), uint32(0));
}

// The round trip the owner makes around a mount or a taxi: the unsummon records the pet, a
// "not yet" keeps it, the resummon loads it and clears the number, and a second resummon has
// nothing left to do.
TEST(PetMgr_UnsummonThenResummonRoundTrip)
{
    PetMgr mgr;
    Wire wire(mgr);
    mgr.UnsummonTemporaryIfAny(Facts(true, true, false, kPetNumber), wire.Unsummon());
    mgr.ResummonTemporaryUnsummonedIfAny(Resummon(wire, true, false));     // still mounted
    mgr.ResummonTemporaryUnsummonedIfAny(Resummon(wire, false, false));    // landed
    mgr.ResummonTemporaryUnsummonedIfAny(Resummon(wire, false, false));    // nothing pending
    CHECK_STR(wire.Text(), "unsummon mode 0 seen 4242; load 4242 seen 4242");
    CHECK_EQ(mgr.GetTemporaryUnsummonedPetNumber(), uint32(0));
}
