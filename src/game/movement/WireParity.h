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
#include "wire/MovementCodec.h"
#include "wire/MovementStatus.h"

#include <functional>
#include <string>

class MovementInfo;
class WorldPacket;

/**
 * @brief The movement wire's parity checks and its codec-rejection counters.
 *
 *  - Outbound: under the shadow, every packet the wire layer knows that this
 *    server sends, a registry layout or a family, is decoded whole and
 *    re-encoded. A packet that decodes but does not reproduce its own bytes is
 *    counted `inexact`: the decode was sound, and the writer or the encoder
 *    disagrees.
 *  - Rejected: a packet the registry's codec could not decode.
 *    MovementInfo::Read, HandleMoveTeleportAckOpcode and
 *    HandleSetActiveMoverOpcode count it and throw, and WorldSession::Update
 *    logs it and, by config, kicks. Counted whether or not the shadow is on.
 *  - BridgeCheck: under the shadow, whether MovementBridge's ToWire, given the
 *    record FromWire just filled in, reproduces the status the codec decoded.
 *
 * Every line names its opcode through the function Enable installs.
 */
namespace WireParity
{
    /// Turns the shadow on or off and installs the function that names an opcode
    /// in the report and in the codec's error lines.
    void Enable(bool on, char const* (*name)(uint16 opcode));
    bool Enabled();

    /// The installed function's name for `opcode`, or "?" while none is installed.
    char const* OpcodeName(uint16 opcode);

    /// Any other packet the wire layer knows that this server sends -- a registry
    /// layout or a family: must decode whole, and should re-encode to its own bytes.
    void Outbound(uint16 opcode, WorldPacket const& packet);

    /// A packet the registry's codec could not decode -- MovementInfo::Read's own
    /// bad-packet path. Counted on the opcode's row (or a single unknown-opcode
    /// bucket, when the opcode names neither a registry layout nor a family)
    /// whether or not the shadow is on.
    void Rejected(uint16 opcode, Wire::DecodeError error);

    /// Under the shadow only. `decoded` is what the codec just read off the wire;
    /// `record` is the MovementInfo the bridge mapped it into. Counts the opcode's
    /// row seen either way; a mismatch means the bridge lost something on the way
    /// in -- fix MovementBridge.cpp, not the codec.
    void BridgeCheck(uint16 opcode, Wire::MovementStatus const& decoded, MovementInfo const& record);

    /// True once any hook has counted a packet, whatever the switch says now.
    /// The shutdown report asks this instead of Enabled(), so a `.reload config`
    /// that turns the shadow off before shutdown does not discard the run.
    bool Saw();

    /// One summary line, then one line per opcode that saw traffic.
    void Report(std::function<void(std::string const&)> const& line);
}

#endif
