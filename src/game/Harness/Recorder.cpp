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

#include "Recorder.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "OpcodeTable.h"
#include "Log.h"
#include "Opcodes.h"

#include <cstdio>

namespace
{
    /// The one place a TRACE line reaches the log (see Scenario.cpp's Out for why sLog cannot
    /// be spelled inside a member of a class that has its own Log).
    void Out(std::string const& line)
    {
        sLog.outString("%s", line.c_str());
    }
}

namespace Harness
{
    Recorder::Recorder()
        : m_active(false), m_digested(false), m_seq(0), m_digest(Trace::kFnvOffset), m_digestedLines(0)
    {
    }

    Recorder::~Recorder()
    {
        // Nothing to take off. A recorder is a member of a registered scenario, so it lives until
        // static destruction -- long after every session it was ever installed on: the scenario
        // ends it before its verdict, and when a scenario is abandoned instead, the runner's
        // teardown deletes the harness session, and the sink with it, while this object is still
        // alive to answer any packet the teardown sends. Reaching for the player registry here,
        // at static destruction, would be the only unsafe thing it could do.
    }

    Player* Recorder::Resolve() const
    {
        return m_player ? sPlayerRegistry.Find(m_player, false) : NULL;
    }

    void Recorder::Start(char const* scenario, Player* player, Trace::Roles const& roles)
    {
        m_scenario = scenario;
        m_player = player ? player->GetObjectGuid() : ObjectGuid();
        m_roles = roles;
        m_roles.self = m_player.GetRawValue();
        m_seq = 0;
        m_digest = Trace::kFnvOffset;
        m_digestedLines = 0;
        m_packets.clear();
        m_last = State();                // the spawn window's delta is the whole state
        m_window = "spawn";
        m_digested = false;
        m_active = player && player->GetSession();
        if (m_active)
        {
            player->GetSession()->SetSocketlessSink(&Recorder::Sink, this);
        }
    }

    void Recorder::Open(std::string const& window, bool digested)
    {
        if (!m_active)
        {
            return;
        }
        CloseWindow();
        m_window = window;
        m_digested = digested;
        m_lastMini = Mini();    // a window's first snap line reads against its opening state
    }

    void Recorder::Note(std::string const& text)
    {
        if (m_active)
        {
            Emit("call " + text);
        }
    }

    void Recorder::End()
    {
        if (!m_active)
        {
            return;
        }
        CloseWindow();
        if (Player* p = Resolve())
        {
            if (WorldSession* s = p->GetSession())
            {
                s->SetSocketlessSink(NULL, NULL);
            }
        }
        m_active = false;
        m_window.clear();
    }

    uint32 Recorder::CountIn(std::string const& window, uint16 opcode) const
    {
        uint32 n = 0;
        for (size_t i = 0; i < m_packets.size(); ++i)
        {
            if (m_packets[i].window == window && m_packets[i].opcode == opcode)
            {
                ++n;
            }
        }
        return n;
    }

    uint32 Recorder::CountIn(std::string const& window) const
    {
        uint32 n = 0;
        for (size_t i = 0; i < m_packets.size(); ++i)
        {
            if (m_packets[i].window == window)
            {
                ++n;
            }
        }
        return n;
    }

    std::vector<Recorder::Seen const*> Recorder::SeenIn(std::string const& window, uint16 opcode) const
    {
        std::vector<Seen const*> out;
        for (size_t i = 0; i < m_packets.size(); ++i)
        {
            if (m_packets[i].window == window && m_packets[i].opcode == opcode)
            {
                out.push_back(&m_packets[i]);
            }
        }
        return out;
    }

    uint32 Recorder::CountAll(uint16 opcode) const
    {
        uint32 n = 0;
        for (size_t i = 0; i < m_packets.size(); ++i)
        {
            if (m_packets[i].opcode == opcode)
            {
                ++n;
            }
        }
        return n;
    }

    std::vector<uint32> Recorder::FiredCriteriaIds() const
    {
        std::vector<uint32> ids;
        for (size_t i = 0; i < m_packets.size(); ++i)
        {
            std::vector<uint8> const& p = m_packets[i].payload;
            if (m_packets[i].opcode == SMSG_CRITERIA_UPDATE && p.size() >= 4)
            {
                ids.push_back(uint32(p[0]) | (uint32(p[1]) << 8) | (uint32(p[2]) << 16) | (uint32(p[3]) << 24));
            }
        }
        return ids;
    }

    void Recorder::Sink(void* context, WorldPacket const& packet)
    {
        static_cast<Recorder*>(context)->OnPacket(packet);
    }

    void Recorder::OnPacket(WorldPacket const& packet)
    {
        if (!m_active)
        {
            return;
        }
        // What a socket would have been handed: SendPacket's socket branch flushes the pending
        // bits into the packet itself before it writes, so the copy does that and the caller's
        // packet is left exactly as it came.
        WorldPacket copy(packet);
        copy.FlushBits();
        const uint16 opcode = copy.GetOpcode();
        // The socket branch refuses these before anything is written, so they never leave the
        // server; they are recorded as refused, not as sent.
        const bool unhandled = opcodeTable[opcode].status == STATUS_UNHANDLED;
        const std::string record = Trace::PacketRecord(opcode, LookupOpcodeName(opcode),
                                                       copy.contents(), copy.size(), unhandled, m_roles);
        Seen seen;
        seen.window = m_window;
        seen.opcode = opcode;
        if (copy.size())
        {
            seen.payload.assign(copy.contents(), copy.contents() + copy.size());
        }
        m_packets.push_back(seen);
        Emit("pkt " + record);
        if (m_digested)
        {
            if (Trace::SnapDue(m_lastMini, Mini()))
            {
                Emit("snap " + m_lastMini);
            }
        }
    }

    void Recorder::Emit(std::string const& text)
    {
        ++m_seq;
        Out(Trace::TraceLine(m_scenario.c_str(), m_seq, m_window, text));
        if (m_digested)
        {
            m_digest = Trace::DigestLine(m_digest, m_window, text);
            ++m_digestedLines;
        }
    }

    void Recorder::CloseWindow()
    {
        const State now = Take();
        const std::vector<std::string> lines = Trace::StateDelta(m_last, now);
        for (size_t i = 0; i < lines.size(); ++i)
        {
            Emit(lines[i]);
        }
        m_last = now;
    }
}
