/*
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

#include "session/handlers/entities/VehicleHandlers.h"

#include "Platform/Define.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "ObjectGuid.h"
#include "Log.h"
#include "Player.h"
#include "Vehicle.h"
#include "ObjectMgr.h"

void VehicleHandlers::HandleDismissControlledVehicle(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_DISMISS_CONTROLLED_VEHICLE");
    recvPacket.hexlike();

    MovementInfo movementInfo;                              // Not used at the moment

    recvPacket >> movementInfo;

    TransportInfo* transportInfo = session.GetPlayer()->GetTransportInfo();
    if (!transportInfo || !transportInfo->IsOnVehicle())
    {
        return;
    }

    Unit* vehicle = (Unit*)transportInfo->GetTransport();

    // Something went wrong
    if (movementInfo.GetGuid() != vehicle->GetObjectGuid())
    {
        return;
    }

    // Remove Vehicle Control Aura
    vehicle->RemoveSpellsCausingAura(SPELL_AURA_CONTROL_VEHICLE, session.GetPlayer()->GetObjectGuid());
}

void VehicleHandlers::HandleRequestVehicleExit(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_REQUEST_VEHICLE_EXIT");
    recvPacket.hexlike();

    TransportInfo* transportInfo = session.GetPlayer()->GetTransportInfo();
    if (!transportInfo || !transportInfo->IsOnVehicle())
    {
        return;
    }

    Unit* vehicle = (Unit*)transportInfo->GetTransport();

    // Check for exit flag
    if (VehicleSeatEntry const* seatEntry = vehicle->GetVehicleInfo()->GetSeatEntry(transportInfo->GetTransportSeat()))
        if (seatEntry->Flags & SEAT_FLAG_CAN_EXIT)
        {
            vehicle->RemoveSpellsCausingAura(SPELL_AURA_CONTROL_VEHICLE, session.GetPlayer()->GetObjectGuid());
        }
}

void VehicleHandlers::HandleRequestVehicleSwitchSeat(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_REQUEST_VEHICLE_SWITCH_SEAT");
    recvPacket.hexlike();

    ObjectGuid vehicleGuid;
    uint8 seat;

    recvPacket >> vehicleGuid.ReadAsPacked();
    recvPacket >> seat;

    TransportInfo* transportInfo = session.GetPlayer()->GetTransportInfo();
    if (!transportInfo || !transportInfo->IsOnVehicle())
    {
        return;
    }

    Unit* vehicle = (Unit*)transportInfo->GetTransport();

    // Something went wrong
    if (vehicleGuid != vehicle->GetObjectGuid())
    {
        return;
    }

    vehicle->GetVehicleInfo()->SwitchSeat(session.GetPlayer(), seat);
}

void VehicleHandlers::HandleChangeSeatsOnControlledVehicle(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_CHANGE_SEATS_ON_CONTROLLED_VEHICLE");
    recvPacket.hexlike();

    ObjectGuid srcVehicleGuid;
    MovementInfo movementInfo;
    ObjectGuid destVehicleGuid;
    uint8 seat;

    recvPacket >> movementInfo;

    srcVehicleGuid = movementInfo.GetGuid();
    destVehicleGuid = movementInfo.GetGuid2();
    seat = movementInfo.GetByteParam();

    TransportInfo* transportInfo = session.GetPlayer()->GetTransportInfo();
    if (!transportInfo || !transportInfo->IsOnVehicle())
    {
        return;
    }

    Unit* srcVehicle = (Unit*)transportInfo->GetTransport();

    // Something went wrong
    if (srcVehicleGuid != srcVehicle->GetObjectGuid())
    {
        return;
    }

    if (srcVehicleGuid != destVehicleGuid)
    {
        Unit* destVehicle = session.GetPlayer()->GetMap()->GetUnit(destVehicleGuid);

        if (!destVehicle || !destVehicle->IsVehicle())
        {
            return;
        }

        // Change vehicle is not possible
        if (destVehicle->GetVehicleInfo()->GetVehicleEntry()->Flags & VEHICLE_FLAG_DISABLE_SWITCH)
        {
            return;
        }

        SpellClickInfoMapBounds clickPair = sObjectMgr.GetSpellClickInfoMapBounds(destVehicle->GetEntry());
        for (SpellClickInfoMap::const_iterator itr = clickPair.first; itr != clickPair.second; ++itr)
            if (itr->second.IsFitToRequirements(session.GetPlayer(), destVehicle->ToCreature()))
            {
                session.GetPlayer()->CastSpell(destVehicle, itr->second.spellId, true);
            }
    }
    else
    {
        srcVehicle->GetVehicleInfo()->SwitchSeat(session.GetPlayer(), seat);
    }
}

void VehicleHandlers::HandleRideVehicleInteract(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_RIDE_VEHICLE_INTERACT");
    recvPacket.hexlike();

    ObjectGuid playerGuid;
    recvPacket >> playerGuid;

    Player* vehicle = session.GetPlayer()->GetMap()->GetPlayer(playerGuid);

    if (!vehicle || !vehicle->IsVehicle())
    {
        return;
    }

    // Only allowed if in same raid
    if (!vehicle->IsInSameRaidWith(session.GetPlayer()))
    {
        return;
    }

    session.GetPlayer()->CastSpell(vehicle, SPELL_RIDE_VEHICLE_HARDCODED, true);
}

void VehicleHandlers::HandleEjectPassenger(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_CONTROLLER_EJECT_PASSENGER");
    recvPacket.hexlike();

    ObjectGuid passengerGuid;
    recvPacket >> passengerGuid;

    Unit* passenger = session.GetPlayer()->GetMap()->GetUnit(passengerGuid);

    if (!passenger || !passenger->IsBoarded())
    {
        return;
    }

    // the player is not a vehicle
    if (!session.GetPlayer()->IsVehicle())
    {
        return;
    }

    VehicleInfo* vehicleInfo = session.GetPlayer()->GetVehicleInfo();

    // the player must be transporting the passenger
    if (!vehicleInfo->HasOnBoard(passenger))
    {
        return;
    }

    // Check for eject flag
    if (VehicleSeatEntry const* seatEntry = vehicleInfo->GetSeatEntry(passenger->GetTransportInfo()->GetTransportSeat()))
    {
        if (seatEntry->FlagsB & SEAT_FLAG_B_EJECTABLE)
        {
            session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_CONTROL_VEHICLE, passengerGuid);
        }
    }
}

void VehicleHandlers::HandleRequestVehiclePrevSeat(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received CMSG_REQUEST_VEHICLE_PREV_SEAT");

        // ToDo
}

void VehicleHandlers::HandleRequestVehicleNextSeat(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received CMSG_REQUEST_VEHICLE_NEXT_SEAT");

        // ToDo
}
