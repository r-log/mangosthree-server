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

/// Decoupling D7b: the petition handlers answer through continuations. What these cases pin
/// is that the tick queues and does not wait -- the real staging code runs against the
/// GLOBAL CharacterDatabase backed by fakes (D7a's seam), inside a TickGuard::Scope, and the
/// guard's count stays at zero.
///
/// What they cannot do is enter a handler: every petition handler dereferences _player, and
/// a Player cannot be constructed in this binary (it needs a session with a socket, the DBC
/// stores, the object manager's data and a map). So the two pieces a handler hands to the
/// database -- the staging of the read, and the mapping of the answer -- are driven directly,
/// which is exactly the brief's fallback. The reply packet itself is not observable either:
/// WorldSession::SendPacket() returns early without a socket, so there is no outgoing capture
/// to assert against; the mapping function is what the reply is chosen by, and that is
/// asserted instead.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "Database/TickGuard.h"
#include "Guild.h"
#include "ObjectGuid.h"
#include "WorldSession.h"

#include <string>

namespace
{
    const uint32 kAccountId = 17;
    const proto::SessionId kSessionId = 3;
    const uint32 kPlayerLow = 42;
    const uint32 kPetitionLow = 500;
    const uint32 kOwnerLow = 10;
    const uint32 kNpcLow = 77;
    const uint32 kMaxSigns = 4;

    ObjectGuid PlayerGuid() { return ObjectGuid(HIGHGUID_PLAYER, kPlayerLow); }
    ObjectGuid OwnerGuid() { return ObjectGuid(HIGHGUID_PLAYER, kOwnerLow); }
    ObjectGuid PetitionGuid() { return ObjectGuid(HIGHGUID_ITEM, kPetitionLow); }
    ObjectGuid NpcGuid() { return ObjectGuid(HIGHGUID_UNIT, 1, kNpcLow); }

    /// Does `text` start with `prefix`?
    bool StartsWith(std::string const& text, std::string const& prefix)
    {
        return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
    }
}

TEST(PetitionAsync_SignStagesOneHolderOfThreeStatementsAndWaitsForNone)
{
    TickGuard::ResetViolations();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    // The signature is already there before the insert runs: the continuation therefore has
    // real results to take out of the holder, which is the path that has to survive.
    async.Answer("SELECT 1 FROM `petition_sign`", FakeRows{FakeRow{"1"}});

    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    {
        TickGuard::Scope scope;
        WorldSession::QueuePetitionSignHolder(kAccountId, kSessionId, PlayerGuid(),
                                              PetitionGuid(), OwnerGuid(), kMaxSigns);

        // Queued, not run: the tick waited on nothing and no connection has been touched.
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(query.executed.size(), size_t(0));
    }

    // One holder, three statements, run back to back on the delay thread in this order.
    CharacterDatabase.ExecuteQueuedForTest();
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(0));
    REQUIRE(async.executed.size() == size_t(3));
    CHECK(StartsWith(async.executed[0],
                     "SELECT 1 FROM `petition_sign` WHERE `petitionguid` = '500' AND `playerguid` = '42'"));
    CHECK(StartsWith(async.executed[1],
                     "INSERT INTO `petition_sign` (`ownerguid`, `petitionguid`, `playerguid`, `player_account`) "
                     "SELECT `ownerguid`, `petitionguid`, '42', '17' FROM `petition` "
                     "WHERE `petitionguid` = '500' "
                     "AND (SELECT COUNT(*) FROM `petition_sign` WHERE `petitionguid` = '500') < 4 "
                     "AND NOT EXISTS (SELECT 1 FROM `petition_sign` WHERE `petitionguid` = '500' AND `playerguid` = '42')"));
    CHECK(StartsWith(async.executed[2],
                     "SELECT 1 FROM `petition_sign` WHERE `petitionguid` = '500' AND `playerguid` = '42'"));

    // The continuation runs on the world thread. There is no session behind kAccountId in
    // this binary, so it drops the request -- and writes nothing while dropping it.
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(async.executed.size(), size_t(3));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(PetitionAsync_SignOutcomeNamesWhichOfTheTwoHappened)
{
    // The holder's two snapshots around the conditional insert, and what each pair means.
    // A row before the insert is a signature that was already there ...
    CHECK_EQ(WorldSession::PetitionSignOutcome(true, true), uint32(PETITION_SIGN_ALREADY_SIGNED));
    CHECK_EQ(WorldSession::PetitionSignOutcome(true, false), uint32(PETITION_SIGN_ALREADY_SIGNED));
    // ... a row only afterwards is this request's own ...
    CHECK_EQ(WorldSession::PetitionSignOutcome(false, true), uint32(PETITION_SIGN_OK));
    // ... and no row at all means the insert found no room.
    CHECK_EQ(WorldSession::PetitionSignOutcome(false, false), uint32(PETITION_SIGN_PETITION_FULL));
}

TEST(PetitionAsync_BuyReadsBeforeItChargesAndAFailedRevalidationWritesNothing)
{
    TickGuard::ResetViolations();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    // The buyer already owns a petition, so the continuation has a row to fold into its
    // delete list -- if it ever got as far as writing one.
    async.Answer("SELECT `petitionguid` FROM `petition`", FakeRows{FakeRow{"499"}});

    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    {
        TickGuard::Scope scope;
        WorldSession::QueuePetitionBuyRead(kAccountId, kSessionId, PlayerGuid(), NpcGuid(),
                                           std::string("Test Guild"));

        // One read queued, nothing waited on, and -- the point of C3 -- nothing charged or
        // created: the money and the charter item only move in the continuation.
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
        CHECK_EQ(async.executed.size(), size_t(0));
    }

    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == size_t(1));
    CHECK_STR(async.executed[0], "SELECT `petitionguid` FROM `petition` WHERE `ownerguid` = '42'");

    // The continuation re-validates first (C1/C3). Here the session is gone, so it stops --
    // and a stopped continuation leaves no delete, no insert and no escaped name behind.
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(async.executed.size(), size_t(1));
    CHECK_EQ(query.executed.size(), size_t(0));
    CHECK_EQ(TickGuard::Violations(), 0u);
}
