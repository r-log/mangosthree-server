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

#ifndef MANGOS_H_PETITIONHANDLERS
#define MANGOS_H_PETITIONHANDLERS

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Platform/Define.h"
#include "SessionId.h"

class ObjectGuid;
class SqlQueryHolder;
class WorldPacket;
class WorldSession;

/// The client's guild charter opcodes: the petitioner's charter list, a charter bought, its
/// signatures shown, a petition queried, renamed, signed, declined, offered to another player and
/// turned in: static entry points the opcode table binds, each borrowing the session for one call
/// and storing nothing. Every database read a handler needs is queued on the character database
/// and answered a tick later by a continuation, which re-finds the session and the player by their
/// identities; the query's re-finds the session alone and the decline's answers the charter's owner,
/// found by guid.
/// QueuePetitionBuyRead stages the purchase's read, QueuePetitionSignHolder stages the sign itself
/// and PetitionSignOutcome names its result.
struct PetitionHandlers
{
    public:
        static void HandlePetitionBuy(WorldSession& session, WorldPacket& recv_data);
        static void HandlePetitionShowSign(WorldSession& session, WorldPacket& recv_data);
        static void HandlePetitionQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandlePetitionRename(WorldSession& session, WorldPacket& recv_data);
        static void HandlePetitionSign(WorldSession& session, WorldPacket& recv_data);
        static void HandlePetitionDecline(WorldSession& session, WorldPacket& recv_data);
        static void HandleOfferPetition(WorldSession& session, WorldPacket& recv_data);
        static void HandleTurnInPetition(WorldSession& session, WorldPacket& recv_data);
        static void HandlePetitionShowList(WorldSession& session, WorldPacket& recv_data);
        static void QueuePetitionBuyRead(uint32 accountId, proto::SessionId sessionId,
                                         ObjectGuid playerGuid, ObjectGuid npcGuid, std::string name);
        static void QueuePetitionSignHolder(uint32 accountId, proto::SessionId sessionId, ObjectGuid playerGuid,
                                            ObjectGuid petitionGuid, ObjectGuid ownerGuid, uint32 maxSigns);
        static uint32 PetitionSignOutcome(bool signedBefore, bool signedAfter);

    private:
        static void HandlePetitionBuyCallback(std::vector<uint32> const& oldPetitions, uint32 accountId,
                                              proto::SessionId sessionId, ObjectGuid playerGuid,
                                              ObjectGuid npcGuid, std::string name);
        static void HandlePetitionShowSignCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                   proto::SessionId sessionId, ObjectGuid playerGuid,
                                                   ObjectGuid petitionGuid);
        static void HandlePetitionRenameCallback(bool exists, uint32 accountId,
                                                 proto::SessionId sessionId, ObjectGuid playerGuid,
                                                 ObjectGuid petitionGuid, std::string newname);
        static void HandlePetitionSignCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                               proto::SessionId sessionId, ObjectGuid playerGuid,
                                               ObjectGuid petitionGuid);
        static void HandlePetitionSignedCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                 proto::SessionId sessionId, ObjectGuid playerGuid,
                                                 ObjectGuid petitionGuid, ObjectGuid ownerGuid);
        static void HandlePetitionDeclineCallback(std::optional<uint32> const& owner, ObjectGuid playerGuid);
        static void HandleOfferPetitionCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                proto::SessionId sessionId, ObjectGuid ownGuid,
                                                ObjectGuid petitionGuid, ObjectGuid playerGuid);
        static void HandleTurnInPetitionCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                 proto::SessionId sessionId, ObjectGuid playerGuid,
                                                 ObjectGuid petitionGuid);
};

#endif
