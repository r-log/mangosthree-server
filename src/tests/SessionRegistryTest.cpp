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

/// The session registry's update: a session handed over by Add is listed only at the next Update,
/// which sends it its auth response, lets it in or queues it under the player limit, and counts the
/// peak; the same Update then updates every listed session and deletes the ones whose update ends
/// them. A session with no socket ends at its first update, so each test sees the whole path in one
/// Update and the registry is empty after it.

#include "TestHarness.h"
#include "InstalledSessionRegistry.h"
#include "Auth/BigNumber.h"
#include "Opcodes.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <vector>

namespace
{
    /// The byte the auth response's result code is written at: the flag bits, then the five
    /// account fields before it (4 + 1 + 4 + 1 + 4 + 1 bytes).
    const size_t kAuthResultOffset = 16;

    void CapturePacket(void* context, WorldPacket const& packet)
    {
        static_cast<std::vector<WorldPacket>*>(context)->push_back(packet);
    }

    /// A session with no socket whose packets go to `sent`; the registry owns and deletes it.
    WorldSession* NewSession(uint32 accountId, std::vector<WorldPacket>& sent)
    {
        WorldSession* session = new WorldSession(accountId, "registry", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0,
                                                 LOCALE_enUS, BigNumber());
        session->SetSocketlessSink(&CapturePacket, &sent);
        return session;
    }

    uint8 AuthResult(WorldPacket const& packet)
    {
        return packet.read<uint8>(kAuthResultOffset);
    }
}

TEST(SessionRegistry_AnAddedSessionIsListedAtTheNextUpdateAndDeletedWhenItsUpdateEndsIt)
{
    InstalledSessionRegistry sessions;
    SessionRegistry& registry = sessions.registry;
    std::vector<WorldPacket> sent;

    registry.Add(NewSession(21, sent));

    // Handed over, not listed: nothing is found and nothing is sent yet.
    CHECK(registry.Find(21) == NULL);
    CHECK_EQ(registry.ActiveAndQueuedCount(), uint32(0));
    CHECK_EQ(sent.size(), size_t(0));

    registry.Update(0);

    // Listed and let in: its auth response says AUTH_OK, and the peak counts it. Its update then
    // ended it, so the registry deleted it and lists nothing.
    REQUIRE(!sent.empty());
    CHECK_EQ(sent[0].GetOpcode(), uint16(SMSG_AUTH_RESPONSE));
    CHECK_EQ(AuthResult(sent[0]), uint8(AUTH_OK));
    CHECK_EQ(registry.MaxActiveCount(), uint32(1));
    CHECK_EQ(registry.MaxQueuedCount(), uint32(0));
    CHECK(registry.Find(21) == NULL);
    CHECK_EQ(registry.ActiveAndQueuedCount(), uint32(0));
}

TEST(SessionRegistry_UnderAPlayerLimitOfOneTheSecondSessionIsQueuedAtPositionOne)
{
    InstalledSessionRegistry sessions;
    SessionRegistry& registry = sessions.registry;
    std::vector<WorldPacket> first;
    std::vector<WorldPacket> second;

    registry.SetPlayerLimit(1);
    CHECK_EQ(registry.PlayerAmountLimit(), uint32(1));
    CHECK_EQ(registry.PlayerSecurityLimit(), SEC_PLAYER);

    registry.Add(NewSession(31, first));
    registry.Add(NewSession(32, second));
    registry.Update(0);

    // The first is let in; the second finds the limit reached and waits first in the queue.
    REQUIRE(!first.empty());
    CHECK_EQ(first[0].GetOpcode(), uint16(SMSG_AUTH_RESPONSE));
    CHECK_EQ(AuthResult(first[0]), uint8(AUTH_OK));
    REQUIRE(!second.empty());
    CHECK_EQ(second[0].GetOpcode(), uint16(SMSG_AUTH_RESPONSE));
    CHECK_EQ(AuthResult(second[0]), uint8(AUTH_WAIT_QUEUE));
    CHECK_EQ(second[0].read<uint32>(kAuthResultOffset + 1), uint32(1));

    // Both were counted at their peak, and both were deleted by the same update.
    CHECK_EQ(registry.MaxActiveCount(), uint32(1));
    CHECK_EQ(registry.MaxQueuedCount(), uint32(1));
    CHECK_EQ(registry.ActiveAndQueuedCount(), uint32(0));
    CHECK_EQ(registry.QueuedCount(), uint32(0));
}
