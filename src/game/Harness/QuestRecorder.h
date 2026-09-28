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

#ifndef MANGOS_HARNESS_QUEST_RECORDER_H
#define MANGOS_HARNESS_QUEST_RECORDER_H

#include "Trace.h"
#include "ObjectGuid.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class Player;
class WorldPacket;

namespace Harness
{
    /// What the recorder reads beyond the fixed set (money, XP, level, talents, titles, spells,
    /// dailies, completed achievements, mail, inventory): the scenario's quests, the factions
    /// they reward and the currencies they move.
    struct TraceWatch
    {
        std::vector<uint32> quests;
        std::vector<uint32> factions;
        std::vector<uint32> currencies;
    };

    /**
     * The harness's reward recorder (decoupling D4f0, design note §3): what the server does to a
     * harness player, in order, as MVTEST TRACE lines, and the digest of those lines.
     *
     * WINDOWS. Every server call a step makes is one window (`accept`, `credit#k`, `reward`,
     * ...), and a step's last window is followed by a `<window>+tick` window that stays open until
     * the next step opens a window of its own -- which carries what the maps' updates did in
     * between, the batched SMSG_UPDATE_OBJECT above all. Between Begin and End exactly one window
     * is open, so every packet the session sends lands in one. A window may be a setup window (the
     * spawn, a level set): logged like the rest, never digested.
     *
     * WHAT A WINDOW RECORDS. Each packet the moment it is sent (a `pkt` line, Trace::PacketRecord);
     * a `call` line for each server call's result that the step notes; and, when the window
     * closes, a `state` line for each thing the player's state changed in it: inventory slots as
     * entry and count, money, XP and level, talents, the watched reputations, titles, spells,
     * the watched quests' status, slot and counters, dailies, currencies, the completed
     * achievements and the mail count. Never a guid, never a clock.
     *
     * THE SINK. Begin installs it on the player's session (WorldSession::SetSocketlessSink) and
     * End takes it off; a scenario ends the recorder before its verdict, and the runner's teardown
     * deletes the session and the sink with it in any case. The recorder copies each packet and
     * flushes the copy's pending bits -- what a socket would have been handed -- and never
     * touches the caller's packet.
     */
    class QuestRecorder
    {
    public:
        QuestRecorder();
        ~QuestRecorder();

        /// Installs the sink and opens the setup window `spawn`, whose close prints the player's
        /// whole state as the delta from nothing.
        void Begin(char const* scenario, Player* player, ObjectGuid giver, TraceWatch const& watch);
        /// Closes the open window (its state delta) and opens `window`.
        void Open(std::string const& window, bool digested = true);
        /// A `call` line in the open window: a server call's result, as the step reads it.
        void Note(std::string const& text);
        /// Closes the open window and takes the sink off. The digest is final from here.
        void End();
        /// FNV-1a over every digested line, Trace::DigestLine.
        uint32 Digest() const { return m_digest; }
        uint32 DigestedLines() const { return m_digestedLines; }

        /// One packet as recorded: the window it landed in, its opcode, and the bytes a socket
        /// would have been handed (for a category that reads a field out of a recorded packet).
        struct Seen
        {
            std::string        window;
            uint16             opcode;
            std::vector<uint8> payload;
        };
        std::vector<Seen> const& Packets() const { return m_packets; }
        /// Packets of `opcode` recorded in `window`.
        uint32 CountIn(std::string const& window, uint16 opcode) const;
        /// Every packet recorded in `window`.
        uint32 CountIn(std::string const& window) const;
        /// The packets of `opcode` in `window`, in order.
        std::vector<Seen const*> SeenIn(std::string const& window, uint16 opcode) const;

    private:
        struct Snap
        {
            std::map<std::string, std::string> values;
            std::set<uint32> spells;
            std::set<uint32> achievements;
        };

        static void Sink(void* context, WorldPacket const& packet);
        void OnPacket(WorldPacket const& packet);
        void Emit(std::string const& text);
        void CloseWindow();
        Player* Resolve() const;
        Snap Take() const;

        std::string                 m_scenario;
        ObjectGuid                  m_player;
        Trace::Roles                m_roles;
        TraceWatch                  m_watch;
        bool                        m_active;
        std::string                 m_window;
        bool                        m_digested;
        uint32                      m_seq;
        uint32                      m_digest;
        uint32                      m_digestedLines;
        Snap                        m_last;
        std::vector<Seen>           m_packets;
    };
}

#endif
