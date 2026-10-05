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

#ifndef MANGOS_H_PLAYERPACKETSINKS
#define MANGOS_H_PLAYERPACKETSINKS

#include "PlayerClientFacts.h"
#include "WorldPacket.h"
#include "session/packets/combat/AttackSwingPackets.h"
#include "session/packets/entities/PetGuidsPacket.h"
#include "session/packets/entities/StandStatePacket.h"
#include "session/packets/spells/AutoRepeatPackets.h"

class Player;

/// The callback for `owner` of the fact `build` takes: builds the fact's packet with `build` and
/// sends it to the session `owner->GetSession()` returns at that send.
template <class Fact, class Owner>
std::function<void(Fact const&)> FactToSession(Owner* owner, void (*build)(WorldPacket&, Fact const&))
{
    return [owner, build](Fact const& fact)
    {
        WorldPacket data;
        build(data, fact);
        owner->GetSession()->SendPacket(&data);
    };
}

/// The security level query for `owner`: the account security of the session `owner->GetSession()`
/// returns, read at each call.
template <class Owner>
SecurityLevelQuery SecurityLevelOfSession(Owner* owner)
{
    return [owner]()
    {
        return uint32(owner->GetSession()->GetSecurity());
    };
}

/// The callbacks of `owner`'s own client: each fact goes to the builder of its packet, and the
/// security level is read from the session.
template <class Callbacks, class Owner>
Callbacks ClientCallbacksToSession(Owner* owner)
{
    Callbacks callbacks;
    callbacks.swingOutOfReach = FactToSession(owner, &BuildAttackSwingNotInRangePacket);
    callbacks.swingBadFacing = FactToSession(owner, &BuildAttackSwingBadFacingPacket);
    callbacks.combatCancelled = FactToSession(owner, &BuildCancelCombatPacket);
    callbacks.autoRepeatCancelled = FactToSession(owner, &BuildCancelAutoRepeatPacket);
    callbacks.currentPet = FactToSession(owner, &BuildPetGuidsPacket);
    callbacks.standState = FactToSession(owner, &BuildStandStateUpdatePacket);
    callbacks.securityLevel = SecurityLevelOfSession(owner);
    return callbacks;
}

/// Gives `player` every callback the session installs: the cooldown callbacks, those of its own
/// client and those of its group updates. Every place that creates a player calls it right after the
/// construction, before the player is loaded, created or used.
void InstallPlayerPacketSinks(Player& player);

#endif
