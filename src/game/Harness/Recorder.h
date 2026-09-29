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

#ifndef MANGOS_HARNESS_RECORDER_H
#define MANGOS_HARNESS_RECORDER_H

#include "Trace.h"
#include "ObjectGuid.h"

#include <string>
#include <vector>

class Player;
class WorldPacket;

namespace Harness
{
    /**
     * The harness's recorder core (decoupling D4f0 built it inside the quest recorder; decoupling
     * D11 PR 1 moved it here, unchanged in what it prints, so the spell family records on the
     * same machinery): what the server does to a harness player, in order, as MVTEST TRACE lines,
     * and the digest of those lines. A family's recorder derives from it and says only WHAT state
     * it reads -- Take() for the state delta at a window's close, Mini() for the per-packet snap.
     *
     * WINDOWS. Every server call a step makes is one window (`accept`, `cast`, ...), and a step's
     * last window is followed by a `<window>+tick` window that stays open until the next step opens
     * a window of its own -- which carries what the maps' updates did in between. Between Start and
     * End exactly one window is open, so every packet the session sends lands in one. A window may
     * be a setup window (the spawn, a level set): logged like the rest, never digested.
     *
     * WHAT A WINDOW RECORDS. Each packet the moment it is sent (a `pkt` line, Trace::PacketRecord),
     * and in a digested window, right after it, a `snap` line whenever the family's small state
     * (Mini) differs from the last one printed (Ruling 19, always on: no scenario can leave it out).
     * It puts an in-memory statement in order against the packets around it. A `call` line for each
     * server call's result that the step notes; and, when the window closes, a `state` line for each
     * value of the family's state (Take) that changed in it -- the keyed values in key order, then
     * each id set as its +/- delta, in the order the family lists them. Never a guid, never a clock.
     *
     * THE SINK. Start installs it on the player's session (WorldSession::SetSocketlessSink) and End
     * takes it off; a scenario ends the recorder before its verdict, and the runner's teardown
     * deletes the session and the sink with it in any case. The recorder copies each packet and
     * flushes the copy's pending bits -- what a socket would have been handed -- and never touches
     * the caller's packet.
     */
    class Recorder
    {
    public:
        virtual ~Recorder();

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
        /// Who the watched units are, for a category that decodes a recorded packet itself.
        Trace::Roles const& Roles() const { return m_roles; }
        /// Every packet of `opcode` recorded, in any window.
        uint32 CountAll(uint16 opcode) const;
        /// The criteria id of every criteria-update packet recorded, in order: its first word, as
        /// AchievementMgr::SendCriteriaUpdate writes it. The families' noPersistence maps each
        /// through the criteria store (UnmodelledCriteriaTypes) so the achievement closure checks
        /// itself against what the run really fired.
        std::vector<uint32> FiredCriteriaIds() const;

    protected:
        Recorder();

        /// A family's state at one moment: keyed values, printed as `state <key> <old>-><new>` in
        /// key order when they differ, then id sets, printed as `state <name> +a -b` in the order
        /// listed when they differ (Trace::StateDelta). A key or a set missing on one side reads
        /// as "-" or as empty.
        typedef Trace::StateSnapshot State;

        /// Installs the sink on `player`'s session and opens the setup window `spawn`, whose close
        /// prints the whole state as the delta from nothing. `roles.self` is the player.
        void Start(char const* scenario, Player* player, Trace::Roles const& roles);
        /// The family's state, read through public accessors; never a guid, never a clock.
        virtual State Take() const = 0;
        /// The family's snap line state: small, and cheap enough to read after every packet.
        virtual std::string Mini() const = 0;
        /// The recorded player, or NULL once he is gone.
        Player* Resolve() const;

    private:
        static void Sink(void* context, WorldPacket const& packet);
        void OnPacket(WorldPacket const& packet);
        void Emit(std::string const& text);
        void CloseWindow();

        std::string                 m_scenario;
        ObjectGuid                  m_player;
        Trace::Roles                m_roles;
        bool                        m_active;
        std::string                 m_window;
        bool                        m_digested;
        uint32                      m_seq;
        uint32                      m_digest;
        uint32                      m_digestedLines;
        State                       m_last;
        std::vector<Seen>           m_packets;
        std::string                 m_lastMini;
    };
}

#endif
