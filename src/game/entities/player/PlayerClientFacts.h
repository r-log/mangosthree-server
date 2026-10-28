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

#ifndef MANGOS_H_PLAYERCLIENTFACTS
#define MANGOS_H_PLAYERCLIENTFACTS

#include "Platform/Define.h"
#include "Utilities/Errors.h"
#include "ObjectGuid.h"

#include <functional>
#include <string>

// What a player's own client is told: one fact per notice, each reported through a callback the
// session installs on the player, which builds the client's packet and sends it.

/// The player's melee swing is out of reach of its victim.
struct SwingOutOfReachFact
{
};

/// The player's melee swing faces away from its victim.
struct SwingBadFacingFact
{
};

/// The player's melee and ranged attack is cancelled.
struct CombatCancelledFact
{
};

/// The player's auto-repeat spell is cancelled; `target` is the unit the client is told about.
struct AutoRepeatCancelledFact
{
    ObjectGuid target;
};

/// The player's pet is `pet`.
struct CurrentPetFact
{
    ObjectGuid pet;
};

/// The player's pet action bar is cleared.
struct PetActionBarClearedFact
{
};

/// The player's stand state is now `state`.
struct StandStateFact
{
    uint8 state = 0;
};

/// The player is credited `honor` points for a kill; `victim` is the victim the client names (an
/// empty guid names none) and `victimRank` the victim's rank.
struct PvpCreditFact
{
    uint32 honor = 0;
    ObjectGuid victim;
    uint32 victimRank = 0;
};

/// The player completed a quest; the client is told what it gained. The fields are in wire order,
/// and the offsets are where the client's reader stores each one.
struct QuestCompleteFact
{
    uint32 bonusTalents = 0;        ///< +40: "You have gained %d talent points." when > 0
    uint32 rewSkillPoints = 0;      ///< +60: stored by the client, never read
    uint32 money = 0;               ///< +44: "Received %s." when > 0 (a signed compare)
    uint32 xp = 0;                  ///< +48: "Experience gained: %d." when != 0
    uint32 questId = 0;             ///< +32: the QuestCache key (title, reward items, turn-in sound)
    uint32 rewSkillId = 0;          ///< +56: stored by the client, never read
    bool launchGossip = false;      ///< first bit (bit 7) -> +36: the client sends CMSG_GOSSIP_HELLO to the giver
    bool useQuestReward = false;    ///< second bit (bit 6) -> +52: keep the quest frame open for the next quest's details
};

/// A system message to the player's own client: the string table entry `entry` in the client's
/// locale or, when `entry` is 0, `text`; one chat line per line of it.
struct SystemMessageFact
{
    int32 entry = 0;
    std::string text;
};

typedef std::function<void(SwingOutOfReachFact const&)> SwingOutOfReachSink;
typedef std::function<void(SwingBadFacingFact const&)> SwingBadFacingSink;
typedef std::function<void(CombatCancelledFact const&)> CombatCancelledSink;
typedef std::function<void(AutoRepeatCancelledFact const&)> AutoRepeatCancelledSink;
typedef std::function<void(CurrentPetFact const&)> CurrentPetSink;
typedef std::function<void(PetActionBarClearedFact const&)> PetActionBarClearedSink;
typedef std::function<void(StandStateFact const&)> StandStateSink;
typedef std::function<void(PvpCreditFact const&)> PvpCreditSink;
typedef std::function<void(QuestCompleteFact const&)> QuestCompleteSink;
typedef std::function<void(SystemMessageFact const&)> SystemMessageSink;

/// The account security level of the player's session, read at each call. An unsigned integer:
/// the levels run from 0 to 4, and a caller comparing two of them with `<=` gets the answer the
/// comparison of the session's own values gives, since both promote to int.
typedef std::function<uint32()> SecurityLevelQuery;

/// Reports `fact` through `sink`; an empty `sink` (a player no session installed its callbacks
/// on) fails the assertion.
template <class Fact>
void ReportClientFact(std::function<void(Fact const&)> const& sink, Fact const& fact)
{
    MANGOS_ASSERT(sink);
    sink(fact);
}

#endif
