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

#include "session/handlers/entities/TaxiHandlers.h"

#include "Platform/Define.h"
#include <vector>
#include "WorldPacket.h"
#include "Server/WorldSession.h"
#include "Opcodes.h"
#include "Log/Log.h"
#include "Object/ObjectMgr.h"
#include "Object/Creature.h"
#include "entities/player/Player.h"

/**
 * @brief Handles a client request for the known status of a taxi node.
 *
 * @param recv_data The incoming taxi node status packet.
 */
void TaxiHandlers::HandleTaxiNodeStatusQuery(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_TAXINODE_STATUS_QUERY");

    ObjectGuid guid;

    recv_data >> guid;
    session.SendTaxiStatus(guid);
}

/**
 * @brief Handles a request to open a flight master's available taxi menu.
 *
 * @param recv_data The incoming taxi query packet.
 */
void TaxiHandlers::HandleTaxiQueryAvailableNodes(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_TAXIQUERYAVAILABLENODES");

    ObjectGuid guid;
    recv_data >> guid;

    // cheating checks
    Creature* unit = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!unit)
    {
        DEBUG_LOG("TaxiHandlers::HandleTaxiQueryAvailableNodes - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    // unknown taxi node case
    if (session.SendLearnNewTaxiNode(unit))
    {
        return;
    }

    // known taxi node case
    session.SendTaxiMenu(unit);
}

/**
 * @brief Handles a multi-node taxi activation request.
 *
 * @param recv_data The incoming taxi express packet.
 */
void TaxiHandlers::HandleActivateTaxiExpress(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXIEXPRESS");

    ObjectGuid guid;
    uint32 node_count;

    recv_data >> guid >> node_count;

    Creature* npc = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!npc)
    {
        DEBUG_LOG("TaxiHandlers::HandleActivateTaxiExpress - %s not found or you can't interact with it.", guid.GetString().c_str());
        return;
    }
    std::vector<uint32> nodes;

    for (uint32 i = 0; i < node_count; ++i)
    {
        uint32 node;
        recv_data >> node;

        if (!session.GetPlayer()->m_taxi.IsTaximaskNodeKnown(node) && !session.GetPlayer()->IsTaxiCheater())
        {
            session.SendActivateTaxiReply(ERR_TAXINOTVISITED);
            recv_data.rpos(recv_data.wpos()); // prevent additional spam at rejected packet
            return;
        }

        nodes.push_back(node);
    }

    if (nodes.empty())
    {
        return;
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXIEXPRESS from %d to %d" , nodes.front(), nodes.back());

    session.GetPlayer()->ActivateTaxiPathTo(nodes, npc);
}

/**
 * @brief The client's spline-done ack: read for its shape, nothing acts on it.
 *
 * @param recv_data The incoming move-spline-done packet.
 */
void TaxiHandlers::HandleMoveSplineDone(WorldSession& /*session*/, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_SPLINE_DONE");

    // The client sends it only as the active mover, so a taxi passenger, its mover revoked at the
    // takeoff, acks the landing's stop with it and never the flight spline's end. The server's
    // clock runs the flight, its seams, its map crossings and its landing, and the kernel's
    // TaxiBehaviour chains the hops and makes the crossings.
    MovementInfo movementInfo;
    recv_data >> movementInfo;
}

/**
 * @brief Handles a standard two-node taxi activation request.
 *
 * @param recv_data The incoming taxi activation packet.
 */
void TaxiHandlers::HandleActivateTaxi(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXI");

    ObjectGuid guid;
    std::vector<uint32> nodes;
    nodes.resize(2);

    recv_data >> guid >> nodes[0] >> nodes[1];
    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXI from %d to %d" , nodes[0], nodes[1]);
    Creature* npc = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!npc)
    {
        DEBUG_LOG("TaxiHandlers::HandleActivateTaxi - %s not found or you can't interact with it.", guid.GetString().c_str());
        return;
    }

    if (!session.GetPlayer()->IsTaxiCheater())
    {
        if (!session.GetPlayer()->m_taxi.IsTaximaskNodeKnown(nodes[0]) || !session.GetPlayer()->m_taxi.IsTaximaskNodeKnown(nodes[1]))
        {
            session.SendActivateTaxiReply(ERR_TAXINOTVISITED);
            return;
        }
    }

    session.GetPlayer()->ActivateTaxiPathTo(nodes, npc);
}
