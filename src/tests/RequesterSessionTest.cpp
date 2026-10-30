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

/// Three answers from the character database reach only the session that asked for them: the
/// character list, the login and the name query for a character who is not online. Each request
/// captures its account id and its session id, and its callback finds the session again through
/// WorldSession::FindRequesterSession. A session listed under the account with another session id
/// (a reconnect that replaced the one that asked) is sent nothing, and the answer is freed: the
/// list's and the name query's result, the login's query holder with the results it holds. The
/// session that asked is sent its list and its name answer; the login reaches it, finds no row for
/// the character (this binary answers none) and leaves it with no player and no longer loading.
/// Each test installs a session registry of its own. The registry's session list and a session's
/// loading flag are private, so the tests reach them the way ZonePlayerSetTest.cpp reaches the set's
/// lock; a listed session is unlisted before the test ends, since the registry deletes every session
/// still listed when it is destroyed.

#include "TestHarness.h"
#include "InstalledSessionRegistry.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Auth/BigNumber.h"
#include "session/handlers/entities/CharacterHandlers.h"
#include "session/handlers/entities/QueryHandlers.h"
#include "session/SessionRegistry.h"

#include <string>
#include <utility>
#include <vector>

namespace
{
    const uint32 kAccountId = 17;
    const uint32 kOtherAccountId = 18;
    const proto::SessionId kRequesterSessionId = 3;
    const proto::SessionId kReplacementSessionId = 4;
    const uint32 kCharLow = 42;

    /// Hands out a pointer to a private member: an explicit instantiation may name it, and the
    /// friend the instantiation defines returns it.
    template<class Tag, typename Tag::type Member>
    struct RequesterSessionMemberAccess
    {
        friend typename Tag::type MemberOf(Tag)
        {
            return Member;
        }
    };

    struct WorldSessionList
    {
        typedef SessionMap SessionRegistry::* type;
        friend type MemberOf(WorldSessionList);
    };

    template struct RequesterSessionMemberAccess<WorldSessionList, &SessionRegistry::m_sessions>;

    struct SessionLoadingFlag
    {
        typedef bool WorldSession::* type;
        friend type MemberOf(SessionLoadingFlag);
    };

    template struct RequesterSessionMemberAccess<SessionLoadingFlag, &WorldSession::m_playerLoading>;

    /// Lists a session under its account in the installed registry's session list, and unlists it
    /// when the guard goes.
    class ListedSession
    {
        public:
            explicit ListedSession(WorldSession& session) : m_accountId(session.GetAccountId())
            {
                (sSessionRegistry.*MemberOf(WorldSessionList()))[m_accountId] = &session;
            }

            ~ListedSession()
            {
                (sSessionRegistry.*MemberOf(WorldSessionList())).erase(m_accountId);
            }

            ListedSession(ListedSession const&) = delete;
            ListedSession& operator=(ListedSession const&) = delete;

        private:
            uint32 m_accountId;
    };

    /// Marks a session as loading a character of its own, and clears the mark when the guard goes.
    class MarkedLoading
    {
        public:
            explicit MarkedLoading(WorldSession& session) : m_session(session)
            {
                m_session.*MemberOf(SessionLoadingFlag()) = true;
            }

            ~MarkedLoading()
            {
                m_session.*MemberOf(SessionLoadingFlag()) = false;
            }

            MarkedLoading(MarkedLoading const&) = delete;
            MarkedLoading& operator=(MarkedLoading const&) = delete;

        private:
            WorldSession& m_session;
    };

    /// A session built as the dispatch tests build one: no socket, the packets it sends collected
    /// through its sink.
    class CollectingSession
    {
        public:
            CollectingSession(const char* name, proto::SessionId sessionId)
                : session(kAccountId, name, nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber())
            {
                session.SetSessionId(sessionId);
                session.SetSocketlessSink(&CapturePacket, &sent);
            }

            ~CollectingSession()
            {
                session.SetSocketlessSink(nullptr, nullptr);
            }

            std::vector<WorldPacket> sent;
            WorldSession session;

        private:
            static void CapturePacket(void* context, WorldPacket const& packet)
            {
                static_cast<std::vector<WorldPacket>*>(context)->push_back(packet);
            }
    };

    /// A result that counts itself out when it is deleted, so a test sees whether an answer was
    /// freed.
    class CountedResult : public FakeQueryResult
    {
        public:
            CountedResult(FakeRows rows, int& live) : FakeQueryResult(std::move(rows)), m_live(live)
            {
                ++m_live;
            }

            ~CountedResult()
            {
                --m_live;
            }

        private:
            int& m_live;
    };

    /// The connection the queued queries run on: a statement starting with a registered prefix is
    /// answered with a counted result over the registered rows, any other with NULL (no rows).
    class CountingConnection final : public SqlConnection
    {
        public:
            explicit CountingConnection(Database& database) : SqlConnection(database), live(0) { }

            bool Initialize(const char*) override { return true; }

            void Answer(std::string sqlPrefix, FakeRows rows)
            {
                m_answers.push_back(std::make_pair(std::move(sqlPrefix), std::move(rows)));
            }

            QueryResult* Query(const char* sql) override
            {
                std::string const statement(sql ? sql : "");
                executed.push_back(statement);
                for (size_t i = 0; i < m_answers.size(); ++i)
                {
                    if (statement.compare(0, m_answers[i].first.size(), m_answers[i].first) == 0)
                    {
                        CountedResult* result = new CountedResult(m_answers[i].second, live);
                        result->NextRow();
                        return result;
                    }
                }
                return NULL;
            }

            QueryNamedResult* QueryNamed(const char*) override { return NULL; }

            bool Execute(const char* sql) override
            {
                executed.push_back(sql ? sql : "");
                return true;
            }

            std::vector<std::string> executed;
            int live;

        private:
            std::vector<std::pair<std::string, FakeRows> > m_answers;
    };

    WorldPacket CharEnumRequest()
    {
        return WorldPacket(CMSG_CHAR_ENUM, 0);
    }

    /// A login of the empty guid: one byte of guid mask bits, all clear, so no guid byte follows.
    WorldPacket LoginRequest()
    {
        WorldPacket request(CMSG_PLAYER_LOGIN, 1);
        request << uint8(0);
        return request;
    }

    WorldPacket NameQueryRequest()
    {
        WorldPacket request(CMSG_NAME_QUERY, 8);
        request << ObjectGuid(HIGHGUID_PLAYER, kCharLow);
        return request;
    }

    /// Runs the one queued query on the connection and its callback on this thread, with
    /// `listed` the only session the registry lists.
    void AnswerWithListed(WorldSession& listed)
    {
        CharacterDatabase.ExecuteQueuedForTest();
        ListedSession listing(listed);
        CharacterDatabase.ProcessResultQueue();
    }
}

TEST(RequesterSession_FindRequesterSessionAnswersOnlyTheSessionTheRequestCameOn)
{
    InstalledSessionRegistry sessions;

    CollectingSession requester("requester", kRequesterSessionId);

    // Nothing listed: nothing found.
    CHECK(WorldSession::FindRequesterSession(kAccountId, kRequesterSessionId) == NULL);

    {
        ListedSession listing(requester.session);

        // The account and the session id of the request: found. Another session id under the
        // account, or another account with the request's session id: not.
        CHECK(WorldSession::FindRequesterSession(kAccountId, kRequesterSessionId) == &requester.session);
        CHECK(WorldSession::FindRequesterSession(kAccountId, kReplacementSessionId) == NULL);
        CHECK(WorldSession::FindRequesterSession(kOtherAccountId, kRequesterSessionId) == NULL);
    }

    CHECK(WorldSession::FindRequesterSession(kAccountId, kRequesterSessionId) == NULL);
}

TEST(RequesterSession_CharacterListGoesOnlyToTheSessionThatAsked)
{
    InstalledSessionRegistry sessions;

    FakeConnection query(CharacterDatabase);
    CountingConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    CollectingSession requester("requester", kRequesterSessionId);
    CollectingSession replacement("replacement", kReplacementSessionId);

    // The session that asked is listed when the answer (no rows) comes: it is sent its list, empty.
    WorldPacket first = CharEnumRequest();
    CharacterHandlers::HandleCharEnum(requester.session, first);
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    AnswerWithListed(requester.session);
    REQUIRE(async.executed.size() == 1);
    REQUIRE(requester.sent.size() == 1);
    CHECK_EQ(requester.sent[0].GetOpcode(), uint16(SMSG_CHAR_ENUM));
    CHECK_EQ(replacement.sent.size(), size_t(0));

    // The same account reconnected before the answer (one row this time) comes: the replacement
    // is listed under the account with its own session id. It is sent nothing, and the row is
    // freed unread.
    async.Answer("SELECT `characters`.`guid`", FakeRows{FakeRow{"42", "Testchar", "1", "1", "0", "0", "0", "1",
                                                                "12", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "", "0"}});
    WorldPacket second = CharEnumRequest();
    CharacterHandlers::HandleCharEnum(requester.session, second);
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    CharacterDatabase.ExecuteQueuedForTest();
    CHECK_EQ(async.executed.size(), size_t(2));
    CHECK_EQ(async.live, 1);
    {
        ListedSession listing(replacement.session);
        CharacterDatabase.ProcessResultQueue();
    }
    CHECK_EQ(replacement.sent.size(), size_t(0));
    CHECK_EQ(requester.sent.size(), size_t(1));
    CHECK_EQ(async.live, 0);
}

TEST(RequesterSession_NameQueryGoesOnlyToTheSessionThatAsked)
{
    InstalledSessionRegistry sessions;

    FakeConnection query(CharacterDatabase);
    CountingConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    async.Answer("SELECT `guid`, `name`, `race`, `gender`, `class`", FakeRows{FakeRow{"42", "Testchar", "1", "0", "1"}});

    CollectingSession requester("requester", kRequesterSessionId);
    CollectingSession replacement("replacement", kReplacementSessionId);

    // The replacement is listed when the row comes: it is sent nothing, and the row is freed.
    WorldPacket first = NameQueryRequest();
    QueryHandlers::HandleNameQuery(requester.session, first);
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == 1);
    CHECK_EQ(async.live, 1);
    {
        ListedSession listing(replacement.session);
        CharacterDatabase.ProcessResultQueue();
    }
    CHECK_EQ(replacement.sent.size(), size_t(0));
    CHECK_EQ(requester.sent.size(), size_t(0));
    CHECK_EQ(async.live, 0);

    // The session that asked is listed when the row comes: it is sent the name.
    WorldPacket second = NameQueryRequest();
    QueryHandlers::HandleNameQuery(requester.session, second);
    AnswerWithListed(requester.session);
    REQUIRE(requester.sent.size() == 1);
    CHECK_EQ(requester.sent[0].GetOpcode(), uint16(SMSG_NAME_QUERY_RESPONSE));
    ObjectGuid guid;
    uint8 marker = 1;
    std::string name;
    requester.sent[0] >> guid.ReadAsPacked() >> marker >> name;
    CHECK_EQ(guid.GetCounter(), kCharLow);
    CHECK_EQ(marker, uint8(0));
    CHECK_STR(name, "Testchar");
    CHECK_EQ(replacement.sent.size(), size_t(0));
    CHECK_EQ(async.live, 0);
}

TEST(RequesterSession_LoginReachesOnlyTheSessionThatAskedAndADroppedHolderIsFreed)
{
    InstalledSessionRegistry sessions;

    FakeConnection query(CharacterDatabase);
    CountingConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    // One of the holder's reads is answered with a counted row, so the holder's fate can be seen.
    // The character's own row is not answered, so a login that reaches its session fails to load.
    async.Answer("SELECT `pet_aura`.`guid`", FakeRows{FakeRow{"1"}});

    CollectingSession requester("requester", kRequesterSessionId);
    CollectingSession replacement("replacement", kReplacementSessionId);

    // The session that asked is listed when the holder comes back: the login reaches it, finds no
    // character row, and leaves it with no player and no longer loading; the holder is freed.
    WorldPacket first = LoginRequest();
    requester.session.HandlePlayerLoginOpcode(first);
    CHECK_EQ(first.rpos(), size_t(1));
    CHECK(requester.session.PlayerLoading());
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    CharacterDatabase.ExecuteQueuedForTest();
    CHECK(async.executed.size() > 1);
    CHECK_EQ(async.live, 1);
    {
        ListedSession listing(requester.session);
        CharacterDatabase.ProcessResultQueue();
    }
    CHECK(!requester.session.PlayerLoading());
    CHECK(requester.session.GetPlayer() == NULL);
    CHECK_EQ(async.live, 0);

    // The same account reconnected and the replacement, listed with its own session id, is in a
    // login of its own: the old holder is dropped and freed, and the replacement is left as it
    // was, still loading, with no player and sent nothing.
    WorldPacket second = LoginRequest();
    requester.session.HandlePlayerLoginOpcode(second);
    CHECK(requester.session.PlayerLoading());
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    CharacterDatabase.ExecuteQueuedForTest();
    CHECK_EQ(async.live, 1);
    {
        ListedSession listing(replacement.session);
        MarkedLoading loading(replacement.session);
        CharacterDatabase.ProcessResultQueue();
        CHECK(replacement.session.PlayerLoading());
    }
    CHECK(replacement.session.GetPlayer() == NULL);
    CHECK_EQ(replacement.sent.size(), size_t(0));
    CHECK_EQ(async.live, 0);
    CHECK(requester.session.PlayerLoading());
}
