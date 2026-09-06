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

#ifndef MANGOS_WIREPARITY_H
#define MANGOS_WIREPARITY_H

#include "Platform/Define.h"
#include "wire/MovementStatus.h"

#include <functional>
#include <string>

class MovementInfo;
class WorldPacket;

/**
 * @brief The wire codec run in shadow beside the legacy movement reader and
 *        writers, counting where they disagree. Design v2 §13 P1: passive.
 *
 * Nothing here changes what the server does: the legacy MovementInfo still
 * feeds every handler and the legacy writers still build every packet. The
 * shadow decodes a copy, compares, counts, and drops the result. Off unless
 * Movement.WireParity is set; then every hook is one atomic load.
 *
 * Two legacy quirks are counted apart, not as mismatches, because they are
 * known and each has its own fate: the legacy header named the two
 * fall-direction floats the other way round on 28 layouts (labelSwapped;
 * P1-B's real-client golden found the legacy header's labels were the ones
 * the client's own bytes agree with, so the registry now emits those labels
 * and labelSwapped counts nothing -- it can be removed in P2), and the legacy
 * reader stores a transport's vehicle id into fallTime (vehicleIdInFallTime;
 * P2 retires that reader).
 */
namespace WireParity
{
    void Enable(bool on);
    bool Enabled();

    /// The legacy status in wire terms. Fields the legacy reader never carries are
    /// taken from `wireOnly` (the codec's decode of the same bytes) so they cannot
    /// count as a difference: counter, value, twoBits, the unnamed bit, the
    /// empty-flags-block markers, the height-change-failed bit, the vehicle id.
    Wire::MovementStatus ToWire(MovementInfo const& legacy, Wire::MovementStatus const& wireOnly);

    /// An inbound registered packet, after the legacy reader consumed it.
    void Inbound(uint16 opcode, WorldPacket const& packet, MovementInfo const& legacy);
    /// A relay the legacy writer built from `legacy`, before it is sent.
    void Relay(uint16 opcode, WorldPacket const& packet, MovementInfo const& legacy);
    /// Any other registered packet this server sends: must decode, whole.
    void Outbound(uint16 opcode, WorldPacket const& packet);

    /// One summary line, then one line per opcode that saw traffic.
    void Report(std::function<void(std::string const&)> const& line);
    std::string Summary();
}

#endif
