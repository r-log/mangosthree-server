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

/// What a player's group is told about the player: the callbacks the session installs on a player,
/// which mark each fact's flag on the player's group update mask and an aura fact's slot on the
/// player or on the fact's pet, only while the player is in a group; the assertions on a callback
/// no session installed and on a unit that holds no callbacks; and a unit that is not a player
/// holding none.

#include "TestHarness.h"
#include "Creature.h"
#include "Group.h"
#include "Pet.h"
#include "Unit.h"
#include "entities/GroupUpdateFacts.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    /// Stands in for the player: a group or none, the group update mask, the own aura update mask,
    /// and the order of the writes.
    struct FakeOwner
    {
        bool grouped = true;
        uint32 groupUpdateMask = 0;
        uint64 auraUpdateMask = 0;
        std::vector<std::string> writes;

        FakeOwner const* GetGroup() const
        {
            return grouped ? this : NULL;
        }

        void SetGroupUpdateFlag(uint32 flag)
        {
            groupUpdateMask |= flag;
            writes.push_back("flag");
        }

        void SetAuraUpdateMask(uint8 slot)
        {
            auraUpdateMask |= (uint64(1) << slot);
            writes.push_back("slot");
        }
    };

    /// A bare Creature; the group callbacks Unit holds are public here.
    class CreatureProbe : public Creature
    {
        public:
            using Unit::m_groupCallbacks;

            CreatureProbe() : Creature(CREATURE_SUBTYPE_GENERIC) { }
    };

    /// A bare Pet; the group callbacks Unit holds are public here.
    class PetProbe : public Pet
    {
        public:
            using Unit::m_groupCallbacks;
    };

    GroupStatFact Stat(uint32 flag)
    {
        return GroupStatFact{flag};
    }

    /// The assertion cases end the process, so each runs only when MANGOS_TESTS_ABORT_CASE names
    /// it (CheckAbortCase.cmake); any other run returns at once.
    bool AbortCaseSelected(char const* name)
    {
        char const* selected = std::getenv("MANGOS_TESTS_ABORT_CASE");
        if (!selected || std::strcmp(selected, name) != 0)
        {
            return false;
        }
#ifdef _MSC_VER
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
        return true;
    }
}

// The flags the sites report, as Group.h's GroupUpdateFlags spells them.
TEST(GroupUpdateFacts_FlagValues)
{
    CHECK_EQ(uint32(GROUP_UPDATE_FLAG_CUR_HP), uint32(0x00000002));
    CHECK_EQ(uint32(GROUP_UPDATE_FLAG_MAX_HP), uint32(0x00000004));
    CHECK_EQ(uint32(GROUP_UPDATE_FLAG_POWER_TYPE), uint32(0x00000008));
    CHECK_EQ(uint32(GROUP_UPDATE_FLAG_CUR_POWER), uint32(0x00000010));
    CHECK_EQ(uint32(GROUP_UPDATE_FLAG_MAX_POWER), uint32(0x00000020));
    CHECK_EQ(uint32(GROUP_UPDATE_FLAG_LEVEL), uint32(0x00000040));
    CHECK_EQ(uint32(GROUP_UPDATE_FLAG_AURAS), uint32(0x00000400));
    CHECK_EQ(uint32(GROUP_UPDATE_FLAG_PET_AURAS), uint32(0x00080000));
}

// In a group, a stat fact marks exactly its flag and no aura slot.
TEST(GroupUpdateFacts_StatMarksItsFlagInAGroup)
{
    uint32 const flags[] = { 0x00000008, 0x00000040, 0x00000002, 0x00000004, 0x00000010, 0x00000020 };
    for (uint32 flag : flags)
    {
        FakeOwner owner;
        GroupCallbacks callbacks = GroupCallbacksFor<GroupCallbacks>(&owner);

        ReportGroupFact(callbacks.stat, Stat(flag));

        CHECK_EQ(owner.groupUpdateMask, flag);
        CHECK_EQ(owner.auraUpdateMask, uint64(0));
        CHECK_EQ(owner.writes.size(), size_t(1));
    }
}

// In a group, an own aura fact marks the auras' flag, then its slot on the owner.
TEST(GroupUpdateFacts_AuraMarksItsFlagThenItsSlotOnTheOwner)
{
    FakeOwner owner;
    GroupCallbacks callbacks = GroupCallbacksFor<GroupCallbacks>(&owner);

    ReportGroupFact(callbacks.aura, GroupAuraFact{GROUP_UPDATE_FLAG_AURAS, 37});

    CHECK_EQ(owner.groupUpdateMask, uint32(0x00000400));
    CHECK_EQ(owner.auraUpdateMask, uint64(0x0000002000000000));
    REQUIRE(owner.writes.size() == 2u);
    CHECK(owner.writes[0] == "flag");
    CHECK(owner.writes[1] == "slot");
}

// In a group, a pet aura fact marks the pet auras' flag on the owner and its slot on the pet, not
// on the owner.
TEST(GroupUpdateFacts_PetAuraMarksTheFlagOnTheOwnerAndTheSlotOnThePet)
{
    FakeOwner owner;
    Pet pet;
    GroupCallbacks callbacks = GroupCallbacksFor<GroupCallbacks>(&owner);

    ReportGroupFact(callbacks.petAura, PetGroupAuraFact{GROUP_UPDATE_FLAG_PET_AURAS, 5, &pet});

    CHECK_EQ(owner.groupUpdateMask, uint32(0x00080000));
    CHECK_EQ(owner.auraUpdateMask, uint64(0));
    CHECK_EQ(pet.GetAuraUpdateMask(), uint64(0x20));
    REQUIRE(owner.writes.size() == 1u);
    CHECK(owner.writes[0] == "flag");
}

// With no group, no fact marks anything.
TEST(GroupUpdateFacts_NothingIsMarkedWithoutAGroup)
{
    FakeOwner owner;
    owner.grouped = false;
    Pet pet;
    GroupCallbacks callbacks = GroupCallbacksFor<GroupCallbacks>(&owner);

    ReportGroupFact(callbacks.stat, Stat(GROUP_UPDATE_FLAG_CUR_HP));
    ReportGroupFact(callbacks.aura, GroupAuraFact{GROUP_UPDATE_FLAG_AURAS, 3});
    ReportGroupFact(callbacks.petAura, PetGroupAuraFact{GROUP_UPDATE_FLAG_PET_AURAS, 3, &pet});

    CHECK_EQ(owner.groupUpdateMask, uint32(0));
    CHECK_EQ(owner.auraUpdateMask, uint64(0));
    CHECK_EQ(pet.GetAuraUpdateMask(), uint64(0));
    CHECK(owner.writes.empty());
}

// The group is asked at each report: joining and leaving between reports decides each one.
TEST(GroupUpdateFacts_TheGroupIsAskedAtEachReport)
{
    FakeOwner owner;
    owner.grouped = false;
    GroupCallbacks callbacks = GroupCallbacksFor<GroupCallbacks>(&owner);

    ReportGroupFact(callbacks.stat, Stat(GROUP_UPDATE_FLAG_CUR_HP));
    owner.grouped = true;
    ReportGroupFact(callbacks.stat, Stat(GROUP_UPDATE_FLAG_MAX_HP));
    owner.grouped = false;
    ReportGroupFact(callbacks.stat, Stat(GROUP_UPDATE_FLAG_LEVEL));

    CHECK_EQ(owner.groupUpdateMask, uint32(0x00000004));
}

// Each owner's callbacks mark that owner only.
TEST(GroupUpdateFacts_TheCallbacksMarkTheOwnerTheyWereMadeFor)
{
    FakeOwner first;
    FakeOwner second;
    GroupCallbacks firsts = GroupCallbacksFor<GroupCallbacks>(&first);
    GroupCallbacks seconds = GroupCallbacksFor<GroupCallbacks>(&second);

    ReportGroupFact(firsts.stat, Stat(GROUP_UPDATE_FLAG_LEVEL));
    ReportGroupFact(seconds.aura, GroupAuraFact{GROUP_UPDATE_FLAG_AURAS, 0});

    CHECK_EQ(first.groupUpdateMask, uint32(0x00000040));
    CHECK_EQ(first.auraUpdateMask, uint64(0));
    CHECK_EQ(second.groupUpdateMask, uint32(0x00000400));
    CHECK_EQ(second.auraUpdateMask, uint64(1));
}

// A reported fact reaches the callback once, as given, through the callbacks the pointer names.
TEST(GroupUpdateFacts_ReportCallsTheCallbackOnce)
{
    std::vector<uint32> seen;
    GroupCallbacks callbacks;
    callbacks.stat = [&seen](GroupStatFact const& fact)
    {
        seen.push_back(fact.flag);
    };

    ReportGroupFact(InstalledGroupCallbacks(&callbacks).stat, Stat(GROUP_UPDATE_FLAG_MAX_POWER));
    ReportGroupFact(InstalledGroupCallbacks(&callbacks).stat, Stat(GROUP_UPDATE_FLAG_CUR_POWER));

    CHECK(&InstalledGroupCallbacks(&callbacks) == &callbacks);
    REQUIRE(seen.size() == 2u);
    CHECK_EQ(seen[0], uint32(0x00000020));
    CHECK_EQ(seen[1], uint32(0x00000010));
}

// A unit that is not a player holds no group callbacks.
TEST(GroupUpdateFacts_ACreatureAndAPetHoldNone)
{
    CreatureProbe creature;
    PetProbe pet;

    CHECK(creature.m_groupCallbacks == NULL);
    CHECK(pet.m_groupCallbacks == NULL);
}

TEST(GroupUpdateFacts_EmptyCallbackAsserts)
{
    if (!AbortCaseSelected("GroupUpdateFacts_EmptyCallbackAsserts"))
    {
        return;
    }
    ReportGroupFact(GroupStatSink(), Stat(GROUP_UPDATE_FLAG_CUR_HP));
    testing::ReportFailure(__FILE__, __LINE__, "the empty group callback passed the assertion");
}

TEST(GroupUpdateFacts_NoCallbacksAssert)
{
    if (!AbortCaseSelected("GroupUpdateFacts_NoCallbacksAssert"))
    {
        return;
    }
    InstalledGroupCallbacks(NULL);
    testing::ReportFailure(__FILE__, __LINE__, "a unit holding no group callbacks passed the assertion");
}
