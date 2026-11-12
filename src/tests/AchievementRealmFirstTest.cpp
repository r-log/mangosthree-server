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

/// The realm's completed achievements and the holders of its realm firsts. A realm first stays taken
/// while one character holds it, and can be earned again once the last holder loses it: through a
/// reset of its achievements, online or offline, or a criteria taken back below its goal. Other
/// achievements stay completed on the realm. The startup load counts the holders already saved.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Auth/BigNumber.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "DBCStores.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "WorldSession.h"
#include "entities/player/achievements/AchievementMgr.h"
#include "persistence/characters/CharacterRepository.h"

#include <string>
#include <thread>
#include <vector>

namespace
{
    const uint32 kRealmFirst = 9101;
    const uint32 kRealmFirstKill = 9102;
    const uint32 kOrdinary = 9103;
    const uint32 kCriteria = 9104;
    const uint32 kPerThread = 20000;

    AchievementEntry s_realmFirst;
    AchievementEntry s_realmFirstKill;
    AchievementEntry s_ordinary;
    AchievementCriteriaEntry s_criteria;

    /// The three achievements and a kill criteria of the realm first in the stores for the scope.
    struct SeededAchievements
    {
        SeededAchievements()
        {
            Seed(s_realmFirst, kRealmFirst, ACHIEVEMENT_FLAG_REALM_FIRST_REACH);
            Seed(s_realmFirstKill, kRealmFirstKill, ACHIEVEMENT_FLAG_REALM_FIRST_KILL);
            Seed(s_ordinary, kOrdinary, ACHIEVEMENT_FLAG_NONE);
            s_criteria.ID = kCriteria;
            s_criteria.referredAchievement = kRealmFirst;
            s_criteria.requiredType = ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE;
            s_criteria.kill_creature.creatureCount = 10;
            sAchievementCriteriaStore.SetEntry(kCriteria, &s_criteria);
        }

        ~SeededAchievements()
        {
            sAchievementCriteriaStore.SetEntry(kCriteria, NULL);
            sAchievementStore.SetEntry(kRealmFirst, NULL);
            sAchievementStore.SetEntry(kRealmFirstKill, NULL);
            sAchievementStore.SetEntry(kOrdinary, NULL);
        }

        static void Seed(AchievementEntry& entry, uint32 id, uint32 flags)
        {
            entry.ID = id;
            entry.Flags = flags;
            sAchievementStore.SetEntry(id, &entry);
        }
    };

    /// The fakes on the character database, asynchronous writes on.
    struct CharacterDb
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        CharacterDb() : query(CharacterDatabase), async(CharacterDatabase),
            attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        size_t RunQueued()
        {
            CharacterDatabase.ExecuteQueuedForTest();
            return async.executed.size();
        }
    };

    /// A character with guid 1 that has completed the realm first and holds progress in its criteria,
    /// seated on `session` while it lives.
    class RealmFirstHolder : public Player
    {
        public:
            explicit RealmFirstHolder(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
                session.SetPlayer(this);

                std::vector<LoginAchievementRow> achievements(1);
                achievements[0].achievement = kRealmFirst;
                achievements[0].date = 1300000000;
                std::vector<LoginCriteriaProgressRow> criteria(1);
                criteria[0].criteria = kCriteria;
                criteria[0].counter = 10;
                criteria[0].date = 1300000000;
                GetAchievementMgr().LoadFromDB(achievements, criteria);
            }

            ~RealmFirstHolder()
            {
                SessionOf(*this)->SetPlayer(nullptr);
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };
}

// A realm first is taken once a holder is set, and free again once that holder is cleared.
TEST(AchievementRealmFirst_SetThenClearFreesIt)
{
    SeededAchievements seeded;
    AchievementGlobalMgr mgr;
    CHECK(!mgr.IsRealmCompleted(&s_realmFirst));

    mgr.SetRealmCompleted(&s_realmFirst, 7);
    CHECK(mgr.IsRealmCompleted(&s_realmFirst));

    mgr.ClearRealmCompleted(&s_realmFirst, 7);
    CHECK(!mgr.IsRealmCompleted(&s_realmFirst));
}

// With two holders, the first one cleared keeps the realm first taken and the second frees it.
TEST(AchievementRealmFirst_TwoHoldersFreeItOnlyAtTheLast)
{
    SeededAchievements seeded;
    AchievementGlobalMgr mgr;
    mgr.SetRealmCompleted(&s_realmFirst, 7);
    mgr.SetRealmCompleted(&s_realmFirst, 8);

    mgr.ClearRealmCompletedBy(7);
    CHECK(mgr.IsRealmCompleted(&s_realmFirst));

    mgr.ClearRealmCompletedBy(8);
    CHECK(!mgr.IsRealmCompleted(&s_realmFirst));
}

// Clearing a character that holds no realm first changes nothing, and an ordinary achievement stays
// completed on the realm when its holder is cleared.
TEST(AchievementRealmFirst_ClearingAnotherCharacterChangesNothing)
{
    SeededAchievements seeded;
    AchievementGlobalMgr mgr;
    mgr.SetRealmCompleted(&s_realmFirst, 7);
    mgr.SetRealmCompleted(&s_ordinary, 8);

    mgr.ClearRealmCompletedBy(8);
    mgr.ClearRealmCompleted(&s_realmFirst, 8);
    mgr.ClearRealmCompleted(&s_ordinary, 8);
    CHECK(mgr.IsRealmCompleted(&s_realmFirst));
    CHECK(mgr.IsRealmCompleted(&s_ordinary));

    mgr.ClearRealmCompletedBy(7);
    CHECK(!mgr.IsRealmCompleted(&s_realmFirst));
    CHECK(mgr.IsRealmCompleted(&s_ordinary));
}

// The startup load marks every saved achievement completed and counts the saved holders of the realm
// firsts, asking only for the realm firsts' holders.
TEST(AchievementRealmFirst_TheStartupLoadCountsTheSavedHolders)
{
    SeededAchievements seeded;
    CharacterDb db;
    db.query.Answer("SELECT `achievement`, `guid`", FakeRows{
        FakeRow{"9101", "7"},
        FakeRow{"9101", "8"},
        FakeRow{"9102", "9"},
    });
    db.query.Answer("SELECT `achievement` FROM", FakeRows{
        FakeRow{"9101"},
        FakeRow{"9102"},
        FakeRow{"9103"},
    });

    AchievementGlobalMgr mgr;
    mgr.LoadCompletedAchievements();
    REQUIRE(db.query.executed.size() == size_t(2));
    CHECK(db.query.executed[1].find("IN (9101,9102)") != std::string::npos);
    CHECK(mgr.IsRealmCompleted(&s_realmFirst));
    CHECK(mgr.IsRealmCompleted(&s_realmFirstKill));
    CHECK(mgr.IsRealmCompleted(&s_ordinary));

    mgr.ClearRealmCompletedBy(7);
    CHECK(mgr.IsRealmCompleted(&s_realmFirst));
    mgr.ClearRealmCompletedBy(8);
    CHECK(!mgr.IsRealmCompleted(&s_realmFirst));
    mgr.ClearRealmCompletedBy(9);
    CHECK(!mgr.IsRealmCompleted(&s_realmFirstKill));
    CHECK(mgr.IsRealmCompleted(&s_ordinary));
}

// An offline reset deletes the character's rows and frees the realm firsts it held.
TEST(AchievementRealmFirst_AnOfflineResetFreesIt)
{
    SeededAchievements seeded;
    CharacterDb db;
    sAchievementMgr.SetRealmCompleted(&s_realmFirst, 42);
    sAchievementMgr.SetRealmCompleted(&s_realmFirst, 43);

    AchievementMgr::DeleteFromDB(ObjectGuid(HIGHGUID_PLAYER, uint32(42)));
    CHECK(sAchievementMgr.IsRealmCompleted(&s_realmFirst));
    AchievementMgr::DeleteFromDB(ObjectGuid(HIGHGUID_PLAYER, uint32(43)));
    CHECK(!sAchievementMgr.IsRealmCompleted(&s_realmFirst));
    CHECK_EQ(db.RunQueued(), size_t(4));
}

// An online reset frees the realm first the character held.
TEST(AchievementRealmFirst_AnOnlineResetFreesIt)
{
    SeededAchievements seeded;
    CharacterDb db;
    WorldSession session(1, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    RealmFirstHolder player(session);
    sAchievementMgr.SetRealmCompleted(&s_realmFirst, player.GetGUIDLow());
    REQUIRE(player.GetAchievementMgr().HasAchievement(kRealmFirst));

    player.GetAchievementMgr().Reset();
    CHECK(!player.GetAchievementMgr().HasAchievement(kRealmFirst));
    CHECK(!sAchievementMgr.IsRealmCompleted(&s_realmFirst));
    db.RunQueued();
}

// A criteria taken back below its goal takes the realm first from its holder and frees it.
TEST(AchievementRealmFirst_ACriteriaTakenBackFreesIt)
{
    SeededAchievements seeded;
    CharacterDb db;
    WorldSession session(1, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    RealmFirstHolder player(session);
    sAchievementMgr.SetRealmCompleted(&s_realmFirst, player.GetGUIDLow());

    player.GetAchievementMgr().SetCriteriaProgress(&s_criteria, &s_realmFirst, 0, AchievementMgr::PROGRESS_SET);
    CHECK(!player.GetAchievementMgr().HasAchievement(kRealmFirst));
    CHECK(!sAchievementMgr.IsRealmCompleted(&s_realmFirst));
    db.RunQueued();
}

// Two threads set and clear holders while a third reads: whatever the interleaving, the one holder
// never cleared keeps the realm first taken, and clearing it frees it.
TEST(AchievementRealmFirst_TwoThreadsSettingAndClearing)
{
    SeededAchievements seeded;
    AchievementGlobalMgr mgr;

    std::thread reader([&mgr]()
    {
        for (uint32 i = 0; i < 4 * kPerThread; ++i)
        {
            mgr.IsRealmCompleted(&s_realmFirst);
        }
    });
    std::thread evens([&mgr]()
    {
        for (uint32 i = 0; i < kPerThread; ++i)
        {
            mgr.SetRealmCompleted(&s_realmFirst, 2 * i);
        }
        for (uint32 i = 1; i < kPerThread; ++i)
        {
            mgr.ClearRealmCompletedBy(2 * i);
        }
    });
    std::thread odds([&mgr]()
    {
        for (uint32 i = 0; i < kPerThread; ++i)
        {
            mgr.SetRealmCompleted(&s_realmFirst, 2 * i + 1);
        }
        for (uint32 i = 0; i < kPerThread; ++i)
        {
            mgr.ClearRealmCompleted(&s_realmFirst, 2 * i + 1);
        }
    });
    reader.join();
    evens.join();
    odds.join();

    CHECK(mgr.IsRealmCompleted(&s_realmFirst));
    mgr.ClearRealmCompletedBy(0);
    CHECK(!mgr.IsRealmCompleted(&s_realmFirst));
}
