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

/// WorldGateway's attach on fake login and character databases, with a session registry of the
/// test's own. The link is told its session id before the registry is handed the session, so the
/// first packet the session sends, its auth response, goes out on a link that already knows the id.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "InstalledSessionRegistry.h"
#include "Database/DatabaseEnv.h"
#include "IClientLink.h"
#include "Opcodes.h"
#include "WorldGateway.h"
#include "WorldPacket.h"
#include "session/SessionRegistry.h"

#include <algorithm>
#include <deque>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    /// Hands out a pointer to a private member: an explicit instantiation may name it, and the
    /// friend the instantiation defines returns it.
    template<class Tag, typename Tag::type Member>
    struct GatewayAttachMemberAccess
    {
        friend typename Tag::type MemberOf(Tag)
        {
            return Member;
        }
    };

    struct RegistryAddQueue
    {
        typedef std::deque<WorldSession*> SessionRegistry::* type;
        friend type MemberOf(RegistryAddQueue);
    };

    template struct GatewayAttachMemberAccess<RegistryAddQueue, &SessionRegistry::m_addQueue>;

    /// The sessions handed to the registry and not yet listed by its update.
    size_t HandedOver(SessionRegistry& registry)
    {
        return (registry.*MemberOf(RegistryAddQueue())).size();
    }

    /// The fakes on one global database.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        explicit Db(Database& database) : query(database), async(database), attached(database, &query, &async, &results)
        {
        }
    };

    /// A link that records the session id it is told, how many sessions the registry had been handed
    /// at that moment, and, for each packet it sends, its opcode and the id it knew then.
    class RecordingLink : public proto::IClientLink
    {
        public:
            explicit RecordingLink(SessionRegistry& registry) : m_registry(registry) {}

            void SendPacket(WorldPacket const& packet) override
            {
                sent.push_back(std::make_pair(packet.GetOpcode(), traceSession));
            }

            void Close() override
            {
                closed = true;
            }

            std::string const& GetRemoteAddress() const override
            {
                return m_address;
            }

            bool IsClosed() const override
            {
                return closed;
            }

            void SetTraceSession(proto::SessionId session) override
            {
                traceSession = session;
                handedOverWhenTold = HandedOver(m_registry);
            }

            proto::SessionId traceSession = proto::INVALID_SESSION_ID;
            size_t handedOverWhenTold = 0;
            std::vector<std::pair<uint16, proto::SessionId>> sent;
            bool closed = false;

        private:
            SessionRegistry& m_registry;
            std::string m_address = "127.0.0.1";
    };

    proto::AuthRequest Request()
    {
        proto::AuthRequest request;
        request.build = 15595;
        request.account = "ATTACH";
        request.clientSeed = 0;
        std::fill(std::begin(request.digest), std::end(request.digest), uint8(0));
        request.peerAddress = "127.0.0.1";
        return request;
    }
}

TEST(WorldGatewayAttach_TheLinkLearnsItsSessionBeforeTheRegistryIsHandedTheSession)
{
    InstalledSessionRegistry sessions;
    Db login(LoginDatabase);
    Db characters(CharacterDatabase);
    login.query.Answer("SELECT `id`", FakeRows{ FakeRow{"7021", "0", "", "127.0.0.1", "0", "3", "0", "0", "Win"} });

    WorldGateway gateway;
    proto::AuthRequest const request = Request();
    proto::AuthLookup const lookup = gateway.LookupAccount(request);
    REQUIRE(lookup.status == proto::AuthStatus::Ok);

    std::shared_ptr<RecordingLink> link = std::make_shared<RecordingLink>(sessions.registry);
    proto::SessionId const id = gateway.Attach(request, link, lookup.context);

    REQUIRE(id != proto::INVALID_SESSION_ID);
    CHECK_EQ(link->traceSession, id);
    CHECK_EQ(link->handedOverWhenTold, size_t(0));
    CHECK_EQ(HandedOver(sessions.registry), size_t(1));
    CHECK(link->sent.empty());

    sessions.registry.Update(0);

    REQUIRE(!link->sent.empty());
    CHECK_EQ(link->sent[0].first, uint16(SMSG_AUTH_RESPONSE));
    CHECK_EQ(link->sent[0].second, id);

    sessions.registry.KickAll();
    sessions.registry.Update(0);
    CHECK_EQ(sessions.registry.ActiveAndQueuedCount(), uint32(0));
}
