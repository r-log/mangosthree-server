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

#include "Recorder.h"

#include <string>
#include <vector>

class Player;

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
     * The harness's reward recorder (decoupling D4f0, design note §3): the Recorder core
     * (Recorder.h -- the windows, the sink, the pkt, snap, call and state lines, the digest) reading
     * the state a quest reward moves.
     *
     * ITS SNAP LINE (Ruling 19): the watched quests' status, rewarded flag and log-slot state, the
     * money, the XP and the level -- so the log-slot clear moved across GiveXP's packets (the
     * D4f0-1 task review's I-1) or a status flipped and restored inside one call (design note Q6,
     * 924's MoneyChanged) changes the digest.
     *
     * ITS STATE DELTA at each window's close: inventory slots as entry and count, money, XP and
     * level, talents, the watched reputations, titles, the watched quests' status, slot and
     * counters, dailies, currencies and the mail count; then the spells and the completed
     * achievements as id sets. Never a guid, never a clock.
     */
    class QuestRecorder : public Recorder
    {
    public:
        /// Installs the sink and opens the setup window `spawn`, whose close prints the player's
        /// whole state as the delta from nothing. `giver` reads as the role "giver".
        void Begin(char const* scenario, Player* player, ObjectGuid giver, TraceWatch const& watch);

    private:
        State Take() const override;
        /// The snap line's state: "q<id>=<status>/<rewarded>/<slot state or -> ... m=<money>
        /// xp=<xp> l=<level>".
        std::string Mini() const override;

        TraceWatch                  m_watch;
    };
}

#endif
