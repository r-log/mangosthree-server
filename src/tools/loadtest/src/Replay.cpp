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

#include "Replay.hpp"

#include "Opcodes.h"
#include "WorldPacket.h"
#include "wire/MovementCodec.h"
#include "wire/MovementSequences.h"

#include <cstring>
#include <istream>
#include <sstream>

namespace loadtest
{
    namespace
    {
        int HexValue(char c)
        {
            if (c >= '0' && c <= '9') { return c - '0'; }
            if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
            if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
            return -1;
        }

        std::string Hex(uint8 const* bytes, size_t size)
        {
            static const char digits[] = "0123456789ABCDEF";
            std::string out;
            out.reserve(size * 2);
            for (size_t i = 0; i < size; ++i)
            {
                out += digits[bytes[i] >> 4];
                out += digits[bytes[i] & 0x0F];
            }
            return out;
        }
    }

    bool ParseCaptureLine(std::string const& text, CaptureLine& out, std::string& error)
    {
        error.clear();
        size_t start = text.find_first_not_of(" \t\r\n");
        if (start == std::string::npos || text[start] == '#')
        {
            return false;                                   // blank or comment: skip
        }
        std::istringstream in(text.substr(start));
        std::string direction, opcode, hex;
        in >> direction >> opcode >> hex;
        if (direction.size() != 1 || (direction[0] != 'C' && direction[0] != 'S'))
        {
            error = "direction must be C or S";
            return false;
        }
        if (opcode.size() < 3 || opcode[0] != '0' || (opcode[1] != 'x' && opcode[1] != 'X'))
        {
            error = "opcode must be 0x....";
            return false;
        }
        uint32 value = 0;
        for (size_t i = 2; i < opcode.size(); ++i)
        {
            const int v = HexValue(opcode[i]);
            if (v < 0) { error = "opcode is not hex"; return false; }
            value = (value << 4) | uint32(v);
        }
        if (value > 0xFFFF) { error = "opcode does not fit 16 bits"; return false; }
        if (hex.size() % 2 != 0) { error = "payload hex has odd length"; return false; }
        out.direction = direction[0];
        out.opcode = uint16(value);
        out.bytes.clear();
        out.bytes.reserve(hex.size() / 2);
        for (size_t i = 0; i < hex.size(); i += 2)
        {
            const int hi = HexValue(hex[i]);
            const int lo = HexValue(hex[i + 1]);
            if (hi < 0 || lo < 0) { error = "payload is not hex"; return false; }
            out.bytes.push_back(uint8((hi << 4) | lo));
        }
        return true;
    }

    ReplayReport Replay(std::istream& capture)
    {
        ReplayReport report;
        std::string text;
        uint32 number = 0;
        while (std::getline(capture, text))
        {
            ++number;
            CaptureLine line;
            std::string error;
            if (!ParseCaptureLine(text, line, error))
            {
                if (!error.empty())
                {
                    ++report.malformed;
                }
                continue;
            }
            ++report.lines;
            ReplayRow& row = report.byOpcode[line.opcode];
            ++row.lines;
            if (Wire::IsEmbeddedLayout(line.opcode))
            {
                ++row.embedded;
                ++report.embedded;
                continue;
            }
            const Wire::Sequence layout = Wire::SequenceFor(line.opcode);
            if (!layout)
            {
                ++row.unregistered;
                ++report.unregistered;
                continue;
            }
            WorldPacket packet(line.opcode, line.bytes.size());
            packet.append(line.bytes.data(), line.bytes.size());
            Wire::MovementStatus status;
            const Wire::DecodeResult result = Wire::Decode(packet, layout, status);
            if (!result.ok() || result.consumed != line.bytes.size())
            {
                ++row.failed;
                ++report.failed;
                if (row.firstProblem.empty())
                {
                    std::ostringstream why;
                    why << "line " << number << ": decode "
                        << (result.error == Wire::DecodeError::Overread ? "overread" :
                            result.error == Wire::DecodeError::BadElement ? "bad element" :
                            result.ok() ? "left bytes" : "failed")
                        << ", consumed " << result.consumed << " of " << line.bytes.size();
                    row.firstProblem = why.str();
                }
                continue;
            }
            ++row.decoded;
            ++report.decoded;
            WorldPacket again(line.opcode, line.bytes.size());
            Wire::Encode(again, layout, status);
            if (again.size() == line.bytes.size() && std::memcmp(again.contents(), line.bytes.data(), again.size()) == 0)
            {
                ++row.exact;
                ++report.exact;
            }
            else if (row.firstProblem.empty())
            {
                std::ostringstream why;
                why << "line " << number << ": re-encoded to " << Hex(again.contents(), again.size())
                    << " from " << Hex(line.bytes.data(), line.bytes.size());
                row.firstProblem = why.str();
            }
        }
        return report;
    }

    void PrintReplay(ReplayReport const& report, std::FILE* to)
    {
        // Every line the file had: report.lines counts the parsed ones only, so a
        // capture with malformed lines would otherwise be summarised as smaller
        // than it is.
        std::fprintf(to, "REPLAY %u line(s): %u decoded, %u exact, %u failed, %u unregistered, %u embedded, %u malformed -> %s\n",
                     report.lines + report.malformed,
                     report.decoded, report.exact, report.failed, report.unregistered, report.embedded, report.malformed,
                     report.Clean() ? "CLEAN" : "NOT CLEAN");
        for (std::map<uint16, ReplayRow>::const_iterator it = report.byOpcode.begin(); it != report.byOpcode.end(); ++it)
        {
            const ReplayRow& row = it->second;
            std::fprintf(to, "  0x%.4X  lines %u  decoded %u  exact %u  failed %u  unregistered %u  embedded %u\n",
                         uint32(it->first), row.lines, row.decoded, row.exact, row.failed, row.unregistered, row.embedded);
            if (!row.firstProblem.empty())
            {
                std::fprintf(to, "          %s\n", row.firstProblem.c_str());
            }
        }
    }
}
