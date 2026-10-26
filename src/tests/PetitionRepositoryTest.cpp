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

/// The guild charters' repository on the global CharacterDatabase backed by fakes: the reads and
/// holders are queued, not run on the calling thread, and answered on the world thread; the writes
/// reach the database in order, the new charter and the delete as one queued transaction each.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "Database/TickGuard.h"
#include "persistence/characters/social/PetitionRepository.h"

#include <optional>
#include <string>
#include <vector>

namespace
{
    /// A canned result as a connection hands it out: on its first row.
    QueryResult* CannedResult(FakeRows rows)
    {
        FakeQueryResult* result = new FakeQueryResult(std::move(rows));
        result->NextRow();
        return result;
    }

    /// The fakes on the global character database, asynchronous writes on.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;
        size_t seen = 0;

        Db() : query(CharacterDatabase), async(CharacterDatabase),
            attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            CharacterDatabase.ExecuteQueuedForTest();
            std::vector<std::string> out(async.executed.begin() + seen, async.executed.end());
            seen = async.executed.size();
            return out;
        }
    };

    void CheckLines(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got == want)
        {
            return;
        }
        std::string text = "lines differ (case line " + std::to_string(line) + "): got [";
        for (size_t i = 0; i < got.size(); ++i)
        {
            text += (i ? " | " : "") + got[i];
        }
        text += "] want [";
        for (size_t i = 0; i < want.size(); ++i)
        {
            text += (i ? " | " : "") + want[i];
        }
        testing::ReportFailure(__FILE__, line, text + "]");
    }
}

// The four single reads and the three holders are queued, run in order, and each callback runs once
// when the world thread drains the result queue.
TEST(PetitionRepository_ReadsAreAsynchronous)
{
    Db db;
    int answered = 0;
    PetitionHolderCallback held = [&answered](QueryResult* /*result*/, SqlQueryHolder* holder) { delete holder; ++answered; };
    {
        TickGuard::Scope scope;
        PetitionRepository::QueryOwnedPetitions(42, [&answered](std::vector<uint32> /*petitions*/) { ++answered; });
        PetitionRepository::QueryExists(7, [&answered](bool /*exists*/) { ++answered; });
        PetitionRepository::QueryOwner(7, [&answered](std::optional<uint32> /*owner*/) { ++answered; });
        PetitionRepository::QueryDetails(7, [&answered](std::optional<PetitionDetailsRow> /*row*/) { ++answered; });
        PetitionRepository::QueueShowSignatures(7, held);
        PetitionRepository::QueueSignRead(7, 3, held);
        PetitionRepository::QueueTurnInRead(7, held);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(7));
    }
    CheckLines(db.Take(), {
        "SELECT `petitionguid` FROM `petition` WHERE `ownerguid` = '42'",
        "SELECT 1 FROM `petition` WHERE `petitionguid` = '7'",
        "SELECT `ownerguid` FROM `petition` WHERE `petitionguid` = '7'",
        "SELECT `ownerguid`, `name`,   (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '7') AS `signs` FROM `petition` WHERE `petitionguid` = '7'",
        "SELECT 1 FROM `petition` WHERE `petitionguid` = '7'",
        "SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '7'",
        "SELECT `ownerguid`,   (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '7') AS `signs` FROM `petition` WHERE `petitionguid` = '7'",
        "SELECT `playerguid`, `petitionguid` FROM `petition_sign` WHERE `player_account` = '3'",
        "SELECT `ownerguid`, `name` FROM `petition` WHERE `petitionguid` = '7'",
        "SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '7'",
    }, __LINE__);
    CHECK_EQ(answered, 0);
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 7);
}

// The new charter's delete list, then its transaction; the rename; the delete's transaction.
TEST(PetitionRepository_Writes)
{
    Db db;
    CHECK_STR(PetitionRepository::GuidList({ 4, 5 }, 9), "'4' , '5' , '9'");
    CHECK_STR(PetitionRepository::GuidList({}, 9), "'9'");
    {
        TickGuard::Scope scope;
        PetitionRepository::Create(42, 9, "Wardens", PetitionRepository::GuidList({ 4 }, 9));
        PetitionRepository::Rename(9, "Watchers");
        PetitionRepository::Delete(9);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(3));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `petition` WHERE `petitionguid` IN ( '4' , '9' )",
        "DELETE FROM `petition_sign` WHERE `petitionguid` IN ( '4' , '9' )",
        "INSERT INTO `petition` (`ownerguid`, `petitionguid`, `name`) VALUES ('42', '9', 'Wardens')",
        "UPDATE `petition` SET `name` = 'Watchers' WHERE `petitionguid` = '9'",
        "DELETE FROM `petition` WHERE `petitionguid` = '9'",
        "DELETE FROM `petition_sign` WHERE `petitionguid` = '9'",
    }, __LINE__);
}

// Each asynchronous read answers its rows or values: the owner's petitions in order, whether the
// petition exists, its owner, its details, and a player's signatures in order.
TEST(PetitionRepository_AsyncRows)
{
    Db db;
    db.async.Answer("SELECT `petitionguid` FROM `petition`", FakeRows{FakeRow{"1000"}, FakeRow{"2000"}});
    db.async.Answer("SELECT 1 FROM `petition`", FakeRows{FakeRow{"1"}});
    db.async.Answer("SELECT `ownerguid` FROM `petition`", FakeRows{FakeRow{"1000"}});
    db.async.Answer("SELECT `ownerguid`, `name`", FakeRows{FakeRow{"1000", "s1.1", "102"}});
    db.async.Answer("SELECT `ownerguid`,`petitionguid`", FakeRows{FakeRow{"1000", "1001"}, FakeRow{"2000", "2001"}});
    std::vector<uint32> petitions;
    bool exists = false;
    std::optional<uint32> owner;
    std::optional<PetitionDetailsRow> details;
    std::vector<PetitionSignatureRow> signs;
    PetitionRepository::QueryOwnedPetitions(42, [&petitions](std::vector<uint32> got) { petitions = got; });
    PetitionRepository::QueryExists(7, [&exists](bool got) { exists = got; });
    PetitionRepository::QueryOwner(7, [&owner](std::optional<uint32> got) { owner = got; });
    PetitionRepository::QueryDetails(7, [&details](std::optional<PetitionDetailsRow> got) { details = got; });
    PetitionRepository::QuerySignaturesBy(42, [&signs](std::vector<PetitionSignatureRow> got) { signs = got; });
    db.Take();
    CharacterDatabase.ProcessResultQueue();
    REQUIRE(petitions.size() == 2);
    CHECK_EQ(petitions[0], 1000u);
    CHECK_EQ(petitions[1], 2000u);
    CHECK(exists);
    REQUIRE(owner);
    CHECK_EQ(*owner, 1000u);
    REQUIRE(details);
    CHECK_EQ(details->ownerGuid, 1000u);
    CHECK_STR(details->name, "s1.1");
    CHECK_EQ(details->signs, uint8(102));
    REQUIRE(signs.size() == 2);
    CHECK_EQ(signs[0].ownerGuid, 1000u);
    CHECK_EQ(signs[0].petitionGuid, 1001u);
    CHECK_EQ(signs[1].ownerGuid, 2000u);
}

// A petition that does not exist answers no existence, no owner and no details.
TEST(PetitionRepository_AsyncNothing)
{
    Db db;
    bool exists = true;
    bool ownerAnswered = false;
    std::optional<uint32> owner;
    bool detailsAnswered = false;
    std::optional<PetitionDetailsRow> details;
    PetitionRepository::QueryExists(7, [&exists](bool got) { exists = got; });
    PetitionRepository::QueryOwner(7, [&ownerAnswered, &owner](std::optional<uint32> got) { ownerAnswered = true; owner = got; });
    PetitionRepository::QueryDetails(7, [&detailsAnswered, &details](std::optional<PetitionDetailsRow> got) { detailsAnswered = true; details = got; });
    db.Take();
    CharacterDatabase.ProcessResultQueue();
    CHECK(!exists);
    CHECK(ownerAnswered);
    CHECK(!owner);
    CHECK(detailsAnswered);
    CHECK(!details);
}

// Each holder read answers its slot: existence, signers in order, the sign target, the account's
// first signature, the before and after snapshots, the turn-in petition and its signers.
TEST(PetitionRepository_HolderRows)
{
    {
        SqlQueryHolder holder;
        holder.SetSize(PETITION_SHOW_SLOT_COUNT);
        CHECK(!PetitionRepository::ShowPetitionExists(&holder));
        CHECK(PetitionRepository::ShowSigners(&holder).empty());
    }
    {
        SqlQueryHolder holder;
        holder.SetSize(PETITION_SHOW_SLOT_COUNT);
        holder.SetResult(PETITION_SHOW_PETITION, CannedResult(FakeRows{FakeRow{"1"}}));
        holder.SetResult(PETITION_SHOW_SIGNATURES, CannedResult(FakeRows{FakeRow{"1000"}, FakeRow{"2000"}}));
        CHECK(PetitionRepository::ShowPetitionExists(&holder));
        std::vector<uint32> signers = PetitionRepository::ShowSigners(&holder);
        REQUIRE(signers.size() == 2);
        CHECK_EQ(signers[0], 1000u);
        CHECK_EQ(signers[1], 2000u);
    }
    {
        SqlQueryHolder holder;
        holder.SetSize(PETITION_SIGN_READ_COUNT);
        holder.SetResult(PETITION_SIGN_READ_PETITION, CannedResult(FakeRows{FakeRow{"1000", "101"}}));
        holder.SetResult(PETITION_SIGN_READ_ACCOUNT, CannedResult(FakeRows{FakeRow{"1000", "1001"}, FakeRow{"2000", "2001"}}));
        std::optional<PetitionSignTargetRow> target = PetitionRepository::SignReadPetition(&holder);
        REQUIRE(target);
        CHECK_EQ(target->ownerGuid, 1000u);
        CHECK_EQ(target->signs, uint8(101));
        std::optional<PetitionAccountSignatureRow> first = PetitionRepository::SignReadAccountSignature(&holder);
        REQUIRE(first);
        CHECK_EQ(first->playerGuid, 1000u);
        CHECK_EQ(first->petitionGuid, 1001u);
    }
    {
        SqlQueryHolder holder;
        holder.SetSize(PETITION_SIGNED_COUNT);
        holder.SetResult(PETITION_SIGNED_AFTER, CannedResult(FakeRows{FakeRow{"1"}}));
        CHECK(!PetitionRepository::SignedBefore(&holder));
        CHECK(PetitionRepository::SignedAfter(&holder));
    }
    {
        SqlQueryHolder holder;
        holder.SetSize(PETITION_TURN_IN_COUNT);
        holder.SetResult(PETITION_TURN_IN_PETITION, CannedResult(FakeRows{FakeRow{"1000", "s1.1"}}));
        holder.SetResult(PETITION_TURN_IN_SIGNATURES, CannedResult(FakeRows{FakeRow{"1000"}, FakeRow{"2000"}}));
        std::optional<PetitionTurnInRow> petition = PetitionRepository::TurnInPetition(&holder);
        REQUIRE(petition);
        CHECK_EQ(petition->ownerGuid, 1000u);
        CHECK_STR(petition->name, "s1.1");
        std::vector<uint32> signers = PetitionRepository::TurnInSigners(&holder);
        REQUIRE(signers.size() == 2);
        CHECK_EQ(signers[1], 2000u);
    }
    {
        SqlQueryHolder holder;
        holder.SetSize(1);
        holder.SetResult(0, CannedResult(FakeRows{FakeRow{"1000", "1001"}, FakeRow{"2000", "2001"}}));
        std::vector<PetitionSignatureRow> signs = PetitionRepository::ReadHolderSignatures(&holder, 0);
        REQUIRE(signs.size() == 2);
        CHECK_EQ(signs[0].ownerGuid, 1000u);
        CHECK_EQ(signs[0].petitionGuid, 1001u);
        CHECK_EQ(signs[1].ownerGuid, 2000u);
    }
}
