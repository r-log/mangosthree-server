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

/// The client settings a session saves and sends back: which character a per-character block is
/// saved under, before, during and after a login, and which block types the two account-data
/// handlers accept.
///
/// The session is built in storage filled with a non-zero pattern, so a member its constructor
/// leaves unset holds that pattern rather than a lucky zero. Players are probes seated on the
/// session (no map, no world). The statements run through the global CharacterDatabase with fakes
/// attached and asynchronous writes on; what the delay thread would send to MySQL is checked after
/// ExecuteQueuedForTest(). The packets the session sends go to a socketless sink.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "InstalledSessionRegistry.h"
#include "Database/DatabaseEnv.h"
#include "Auth/BigNumber.h"
#include "Opcodes.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "session/handlers/connection/ClientHandlers.h"

#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace
{
    const uint32 kAccount = 17;

    static_assert(NUM_ACCOUNT_DATA_TYPES == 8, "the 4.3.4 client keeps eight account data blocks, types 0 to 7");
    static_assert(GLOBAL_CACHE_MASK == 0x15 && PER_CHARACTER_CACHE_MASK == 0xEA,
                  "types 0, 2 and 4 are the account's, 1, 3, 5, 6 and 7 the character's");

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
        }

        /// The statements sent since the last call.
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

    std::vector<std::string> CharacterBlock(uint32 guid, uint32 type, uint32 time, std::string const& data)
    {
        return {
            "DELETE FROM `character_account_data` WHERE `guid` = '" + std::to_string(guid) + "' AND `type` = '" +
                std::to_string(type) + "'",
            "INSERT INTO `character_account_data` VALUES ('" + std::to_string(guid) + "','" + std::to_string(type) +
                "','" + std::to_string(time) + "','" + data + "')",
        };
    }

    void KeepOpcode(void* context, WorldPacket const& packet)
    {
        static_cast<std::vector<uint16>*>(context)->push_back(packet.GetOpcode());
    }

    static_assert(alignof(WorldSession) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                  "::operator new storage is aligned for a session");

    /// A session built in storage that held a non-zero pattern before the constructor ran.
    class PatternedSession
    {
        public:
            PatternedSession() : m_storage(::operator new(sizeof(WorldSession)))
            {
                std::memset(m_storage, 0x5A, sizeof(WorldSession));
                m_session = new (m_storage) WorldSession(kAccount, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0,
                                                         LOCALE_enUS, BigNumber());
                m_session->SetSocketlessSink(&KeepOpcode, &sent);
            }

            ~PatternedSession()
            {
                m_session->~WorldSession();
                ::operator delete(m_storage);
            }

            PatternedSession(PatternedSession const&) = delete;
            PatternedSession& operator=(PatternedSession const&) = delete;

            WorldSession& operator*() { return *m_session; }
            WorldSession* operator->() { return m_session; }

            std::vector<uint16> sent;

        private:
            void* m_storage;
            WorldSession* m_session;
    };

    /// A player with low guid `guidLow`, seated on `session` while it lives, in no world.
    class SeatedPlayer : public Player
    {
        public:
            SeatedPlayer(WorldSession& session, uint32 guidLow) : Player(&session)
            {
                Object::_Create(guidLow, 0, HIGHGUID_PLAYER);
                session.SetPlayer(this);
            }

            ~SeatedPlayer()
            {
                SessionOf(*this)->SetPlayer(nullptr);
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    WorldPacket UpdateAccountData(uint32 type, uint32 time)
    {
        WorldPacket packet(CMSG_UPDATE_ACCOUNT_DATA, 4 + 4 + 4);
        packet << uint32(type);
        packet << uint32(time);
        packet << uint32(0);                                // decompressed size 0: the block is erased
        return packet;
    }

    WorldPacket RequestAccountData(uint32 type)
    {
        WorldPacket packet(CMSG_REQUEST_ACCOUNT_DATA, 4);
        packet << uint32(type);
        return packet;
    }

    /// A login of the empty guid: one byte of guid mask bits, all clear, so no guid byte follows.
    WorldPacket LoginRequest()
    {
        WorldPacket packet(CMSG_PLAYER_LOGIN, 1);
        packet << uint8(0);
        return packet;
    }

    /// The statements that write: a login's reads run on the same connection.
    std::vector<std::string> Writes(std::vector<std::string> const& statements)
    {
        std::vector<std::string> out;
        for (std::string const& statement : statements)
        {
            if (statement.compare(0, 6, "SELECT") != 0)
            {
                out.push_back(statement);
            }
        }
        return out;
    }
}

// A session no player was ever seated on saves no character's block. A seated player's blocks are
// saved under its guid, and still are once it has logged out: the client uploads them after the
// logout completes. The next player seated takes over. The account's blocks stay the account's.
TEST(AccountDataSession_ACharactersBlockIsSavedUnderTheLastSeatedPlayer)
{
    Db db;
    PatternedSession session;

    session->SetAccountData(PER_CHARACTER_CHAT_CACHE, 1700000000, "chat");
    WorldPacket erase = UpdateAccountData(PER_CHARACTER_CONFIG_CACHE, 1700000000);
    ClientHandlers::HandleUpdateAccountData(*session, erase);
    CheckLines(db.Take(), {}, __LINE__);
    CHECK_STR(session->GetAccountData(PER_CHARACTER_CHAT_CACHE)->Data, "");

    {
        SeatedPlayer first(*session, 42);
        session->SetAccountData(PER_CHARACTER_CHAT_CACHE, 1700000001, "chat");
        CheckLines(db.Take(), CharacterBlock(42, PER_CHARACTER_CHAT_CACHE, 1700000001, "chat"), __LINE__);
        CHECK_STR(session->GetAccountData(PER_CHARACTER_CHAT_CACHE)->Data, "chat");
    }

    CHECK(session->GetPlayer() == NULL);
    session->SetAccountData(PER_CHARACTER_MACROS_CACHE, 1700000002, "macros");
    CheckLines(db.Take(), CharacterBlock(42, PER_CHARACTER_MACROS_CACHE, 1700000002, "macros"), __LINE__);

    session->SetAccountData(GLOBAL_MACROS_CACHE, 1700000003, "account macros");
    CheckLines(db.Take(), {
        "DELETE FROM `account_data` WHERE `account` = '17' AND `type` = '4'",
        "INSERT INTO `account_data` VALUES ('17','4','1700000003','account macros')",
    }, __LINE__);

    {
        SeatedPlayer second(*session, 43);
        session->SetAccountData(PER_CHARACTER_BINDINGS_CACHE, 1700000004, "bindings");
        CheckLines(db.Take(), CharacterBlock(43, PER_CHARACTER_BINDINGS_CACHE, 1700000004, "bindings"), __LINE__);
    }

    session->SetAccountData(PER_CHARACTER_CONFIG_CACHE, 1700000005, "config");
    CheckLines(db.Take(), CharacterBlock(43, PER_CHARACTER_CONFIG_CACHE, 1700000005, "config"), __LINE__);
}

// The uploads that reach the session before the next login are saved under the character that
// logged out. From the start of that login until its player is seated, a character's block is
// saved under no character.
TEST(AccountDataSession_ALoginInProgressSavesNoCharactersBlock)
{
    InstalledSessionRegistry sessions;
    Db db;
    PatternedSession session;

    {
        SeatedPlayer first(*session, 42);
    }
    session->SetAccountData(PER_CHARACTER_CHAT_CACHE, 1700000000, "chat");
    CheckLines(Writes(db.Take()), CharacterBlock(42, PER_CHARACTER_CHAT_CACHE, 1700000000, "chat"), __LINE__);

    WorldPacket login = LoginRequest();
    session->HandlePlayerLoginOpcode(login);
    REQUIRE(session->PlayerLoading());
    session->SetAccountData(PER_CHARACTER_MACROS_CACHE, 1700000001, "macros");
    CheckLines(Writes(db.Take()), {}, __LINE__);

    // The login's reads come back with no session listed: the holder is freed.
    CharacterDatabase.ProcessResultQueue();
    CHECK(session->GetPlayer() == NULL);
}

// Types 0 to 7 are the client's blocks; type 8 is past the session's table. The upload and the
// request both refuse it without a statement or a reply, and both still take type 7.
TEST(AccountDataSession_TheHandlersRefuseATypePastTheLast)
{
    Db db;
    PatternedSession session;
    SeatedPlayer player(*session, 42);

    WorldPacket upload8 = UpdateAccountData(NUM_ACCOUNT_DATA_TYPES, 1700000000);
    ClientHandlers::HandleUpdateAccountData(*session, upload8);
    CheckLines(db.Take(), {}, __LINE__);
    CHECK(session.sent.empty());

    WorldPacket request8 = RequestAccountData(NUM_ACCOUNT_DATA_TYPES);
    ClientHandlers::HandleRequestAccountData(*session, request8);
    CHECK(session.sent.empty());

    WorldPacket upload7 = UpdateAccountData(PER_CHARACTER_CHAT_CACHE, 1700000000);
    ClientHandlers::HandleUpdateAccountData(*session, upload7);
    CheckLines(db.Take(), CharacterBlock(42, PER_CHARACTER_CHAT_CACHE, 0, ""), __LINE__);
    CHECK(session.sent == std::vector<uint16>({uint16(SMSG_UPDATE_ACCOUNT_DATA_COMPLETE)}));

    WorldPacket request7 = RequestAccountData(PER_CHARACTER_CHAT_CACHE);
    ClientHandlers::HandleRequestAccountData(*session, request7);
    CHECK(session.sent == std::vector<uint16>({uint16(SMSG_UPDATE_ACCOUNT_DATA_COMPLETE), uint16(SMSG_UPDATE_ACCOUNT_DATA)}));
}
