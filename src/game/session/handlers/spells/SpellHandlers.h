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

#ifndef MANGOS_H_SPELLHANDLERS
#define MANGOS_H_SPELLHANDLERS

#include "ObjectGuid.h"
#include "SessionId.h"

#include <optional>

class WorldPacket;
class WorldSession;
struct GiftRow;

/// The client's spell and use opcodes: an item used or opened, a game object used or its use
/// reported, a spell cast or cancelled, an aura cancelled on the player or its pet, the growth aura
/// and the auto-repeat spell cancelled, a channel stopped, a totem destroyed, the self-resurrection,
/// a spell click on a creature and the mirror image's look asked for. Static entry points the opcode
/// table binds, each borrowing the session for one call and storing nothing.
struct SpellHandlers
{
    public:
        static void HandleUseItem(WorldSession& session, WorldPacket& recvPacket);
        static void HandleOpenItem(WorldSession& session, WorldPacket& recvPacket);
        static void HandleGameObjectUse(WorldSession& session, WorldPacket& recv_data);
        static void HandleGameObjectReportUse(WorldSession& session, WorldPacket& recvPacket);
        static void HandleCastSpell(WorldSession& session, WorldPacket& recvPacket);
        static void HandleCancelCast(WorldSession& session, WorldPacket& recvPacket);
        static void HandleCancelAura(WorldSession& session, WorldPacket& recvPacket);
        static void HandlePetCancelAura(WorldSession& session, WorldPacket& recvPacket);
        static void HandleCancelGrowthAura(WorldSession& session, WorldPacket& recvPacket);
        static void HandleCancelAutoRepeatSpell(WorldSession& session, WorldPacket& recvPacket);
        static void HandleCancelChanneling(WorldSession& session, WorldPacket& recv_data);
        static void HandleTotemDestroyed(WorldSession& session, WorldPacket& recv_data);
        static void HandleSelfRes(WorldSession& session, WorldPacket& recv_data);
        static void HandleSpellClick(WorldSession& session, WorldPacket& recv_data);
        static void HandleGetMirrorImageData(WorldSession& session, WorldPacket& recv_data);

        /// Stages the wrapped item's gift-row read; the item tests drive it.
        static void QueueOpenWrappedItemRead(uint32 accountId, proto::SessionId sessionId,
                                             ObjectGuid playerGuid, ObjectGuid itemGuid);

    private:
        static void HandleOpenWrappedItemCallback(std::optional<GiftRow> const& gift, uint32 accountId,
                                                  proto::SessionId sessionId, ObjectGuid playerGuid,
                                                  ObjectGuid itemGuid);
};

#endif
