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

/// The one-byte SMSG_CHAR_RENAME refusals. The 4.3.4 client shows its own text for a refused
/// rename only for code 50, "That name is unavailable"; any other code reads "Character rename
/// failed" (CGlueMgr::OnCharRenameResult). A name that is taken when the request arrives, or is
/// taken by the time the database answers, is refused with 50; a character the account may not
/// rename is refused with 48. The session has no socket and keeps what it sends; the database is
/// the global CharacterDatabase with fakes attached.

#include "TestHarness.h"
#include "InstalledSessionRegistry.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "data/CharacterCache.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Auth/BigNumber.h"
#include "session/handlers/entities/CharacterCustomizeHandlers.h"
#include "session/SessionRegistry.h"

#include <string>
#include <vector>

namespace
{
    const uint32 kAccountId = 17;
    const proto::SessionId kSessionId = 3;
    const uint32 kRenamedLow = 42;
    const uint32 kHolderLow = 77;

    const uint8 kClientNameInUse = 50;
    const uint8 kClientCreateError = 48;

    template<class Tag, typename Tag::type Member>
    struct RenameResultMemberAccess
    {
        friend typename Tag::type MemberOf(Tag)
        {
            return Member;
        }
    };

    struct RegistrySessionList
    {
        typedef SessionMap SessionRegistry::* type;
        friend type MemberOf(RegistrySessionList);
    };

    template struct RenameResultMemberAccess<RegistrySessionList, &SessionRegistry::m_sessions>;

    /// Lists a session under its account in the installed registry while the guard lives.
    class ListedSession
    {
        public:
            explicit ListedSession(WorldSession& session) : m_accountId(session.GetAccountId())
            {
                (sSessionRegistry.*MemberOf(RegistrySessionList()))[m_accountId] = &session;
            }

            ~ListedSession()
            {
                (sSessionRegistry.*MemberOf(RegistrySessionList())).erase(m_accountId);
            }

            ListedSession(ListedSession const&) = delete;
            ListedSession& operator=(ListedSession const&) = delete;

        private:
            uint32 m_accountId;
    };

    /// A socketless session that keeps every packet it sends.
    class CollectingSession
    {
        public:
            CollectingSession()
                : session(kAccountId, "renamer", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber())
            {
                session.SetSessionId(kSessionId);
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

    /// The character cache holding one character while the guard lives.
    class CachedCharacter
    {
        public:
            explicit CachedCharacter(std::string const& name)
            {
                sCharacterCache.Clear();
                CharacterCacheEntry entry;
                entry.guid = ObjectGuid(HIGHGUID_PLAYER, kHolderLow);
                entry.accountId = kAccountId + 1;
                entry.name = name;
                entry.race = RACE_HUMAN;
                entry.playerClass = CLASS_WARRIOR;
                sCharacterCache.Add(entry);
            }

            ~CachedCharacter()
            {
                sCharacterCache.Clear();
            }

            CachedCharacter(CachedCharacter const&) = delete;
            CachedCharacter& operator=(CachedCharacter const&) = delete;
    };

    WorldPacket RenameRequest(std::string const& newName)
    {
        WorldPacket request(CMSG_CHAR_RENAME, 8 + newName.size() + 1);
        request << ObjectGuid(HIGHGUID_PLAYER, kRenamedLow);
        request << newName;
        return request;
    }

    /// The code of a one-byte SMSG_CHAR_RENAME refusal.
    uint8 RefusalCode(WorldPacket& reply)
    {
        CHECK_EQ(reply.GetOpcode(), uint16(SMSG_CHAR_RENAME));
        CHECK_EQ(reply.size(), size_t(1));
        uint8 code = 0;
        reply >> code;
        return code;
    }
}

TEST(CharacterRenameResult_ANameTakenWhenTheRequestArrivesIsRefusedAsInUse)
{
    InstalledSessionRegistry sessions;
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    CachedCharacter holder("Takenname");
    CollectingSession renamer;

    WorldPacket request = RenameRequest("Takenname");
    CharacterCustomizeHandlers::HandleCharRename(renamer.session, request);

    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(0));
    REQUIRE(renamer.sent.size() == 1);
    CHECK_EQ(RefusalCode(renamer.sent[0]), kClientNameInUse);
}

TEST(CharacterRenameResult_ANameTakenBeforeTheDatabaseAnswersIsRefusedAsInUse)
{
    InstalledSessionRegistry sessions;
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    async.Answer("SELECT `guid`, `name` FROM `characters`", FakeRows{FakeRow{"42", "Oldname"}});
    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    sCharacterCache.Clear();
    CollectingSession renamer;

    WorldPacket request = RenameRequest("Takenname");
    CharacterCustomizeHandlers::HandleCharRename(renamer.session, request);
    REQUIRE(renamer.sent.empty());
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == 1);

    CachedCharacter holder("Takenname");
    {
        ListedSession listing(renamer.session);
        CharacterDatabase.ProcessResultQueue();
    }

    REQUIRE(renamer.sent.size() == 1);
    CHECK_EQ(RefusalCode(renamer.sent[0]), kClientNameInUse);
    CHECK_EQ(async.executed.size(), size_t(1));
    CHECK_EQ(query.executed.size(), size_t(0));
}

TEST(CharacterRenameResult_ACharacterTheAccountMayNotRenameIsRefusedAsAnError)
{
    InstalledSessionRegistry sessions;
    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;
    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    sCharacterCache.Clear();
    CollectingSession renamer;

    WorldPacket request = RenameRequest("Freename");
    CharacterCustomizeHandlers::HandleCharRename(renamer.session, request);
    REQUIRE(renamer.sent.empty());
    CharacterDatabase.ExecuteQueuedForTest();
    REQUIRE(async.executed.size() == 1);
    {
        ListedSession listing(renamer.session);
        CharacterDatabase.ProcessResultQueue();
    }

    REQUIRE(renamer.sent.size() == 1);
    CHECK_EQ(RefusalCode(renamer.sent[0]), kClientCreateError);
}
