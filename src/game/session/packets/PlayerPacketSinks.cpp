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

#include "PlayerPacketSinks.h"
#include "Player.h"
#include "WorldSession.h"
#include "Log.h"
#include "Group.h"
#include "Pet.h"
#include "session/packets/spells/CooldownPacketSinks.h"
#include "session/packets/spells/SpellModPackets.h"
#include "session/packets/chat/MonsterWhisperPacket.h"
#include "session/packets/chat/SystemMessage.h"

namespace
{
    /// The player's system messages: the fact's entry or text, sent to the session the player has
    /// at that send, one message per line.
    SystemMessageSink SystemMessageToSession(Player* player)
    {
        return [player](SystemMessageFact const& fact)
        {
            if (fact.entry)
            {
                SendSystemMessage(*WorldSession::Of(*player), fact.entry);
            }
            else
            {
                SendSystemMessage(*WorldSession::Of(*player), fact.text.c_str());
            }
        };
    }

    /// The player's notifications: the fact's string table entry, sent to the session the player has
    /// at that send.
    NotificationSink NotificationToSession(Player* player)
    {
        return [player](NotificationFact const& fact)
        {
            player->GetSession()->SendNotification(fact.entry);
        };
    }

    /// A monster's whisper to the player, sent to the session the player has at that send, the
    /// player named as its target.
    MonsterWhisperSink MonsterWhisperToSession(Player* player)
    {
        return [player](MonsterWhisperFact const& fact)
        {
            WorldPacket data;
            BuildMonsterWhisperPacket(data, fact, player->GetObjectGuid(), player->GetName());
            player->GetSession()->SendPacket(&data);
        };
    }

    /// The player's party kill log: the packet goes to the group that tapped the victim, when there
    /// is one, the killer itself skipped, and then to the session the player has at that send.
    PartyKillLogSink PartyKillLogToGroupAndSession(Player* player)
    {
        return [player](PartyKillLogFact const& fact)
        {
            WorldPacket data;
            BuildPartyKillLogPacket(data, fact);
            if (fact.group)
            {
                fact.group->BroadcastPacket(&data, false, fact.group->GetMemberGroup(fact.killer), fact.killer);
            }
            player->GetSession()->SendPacket(&data);
        };
    }

    /// The player's camera, set to the fact's unit's view.
    ViewFromSink ViewFromToCamera(Player* player)
    {
        return [player](ViewFromFact const& fact)
        {
            player->GetCamera().SetView(fact.target);
        };
    }

    /// The player's camera, set back to the player's own view.
    ViewResetSink ViewResetToCamera(Player* player)
    {
        return [player](ViewResetFact const& /*fact*/)
        {
            player->GetCamera().ResetView();
        };
    }

    /// The action bar of the unit the player possesses, sent to the session the player has at that
    /// send; nothing when it possesses no unit, and an error when the unit has no charm info.
    PossessBarSink PossessBarToSession(Player* player)
    {
        return [player](PossessBarFact const& fact)
        {
            Unit* charm = fact.charm;
            if (!charm)
            {
                return;
            }

            CharmInfo* charmInfo = charm->GetCharmInfo();
            if (!charmInfo)
            {
                sLog.outError("Unit::PossessSpellInitialize(): charm (GUID: %u TypeId: %u) has no charminfo!", charm->GetGUIDLow(), charm->GetTypeId());
                return;
            }

            WorldPacket data;
            BuildPossessBarPacket(data, charm->GetObjectGuid(), *charmInfo);
            player->GetSession()->SendPacket(&data);
        };
    }
}

void InstallPlayerPacketSinks(Player& player)
{
    InstallCooldownPacketSinks(player);
    ClientCallbacks callbacks = ClientCallbacksToSession<ClientCallbacks>(&player);
    callbacks.systemMessage = SystemMessageToSession(&player);
    callbacks.notification = NotificationToSession(&player);
    callbacks.monsterWhisper = MonsterWhisperToSession(&player);
    callbacks.partyKillLog = PartyKillLogToGroupAndSession(&player);
    callbacks.viewFrom = ViewFromToCamera(&player);
    callbacks.viewReset = ViewResetToCamera(&player);
    callbacks.possessBar = PossessBarToSession(&player);
    player.SetClientCallbacks(callbacks);
    GroupCallbacks group = GroupCallbacksFor<GroupCallbacks>(&player);
    Player* owner = &player;
    group.groupVisibleFor = [owner](Player const* viewer) { return IsGroupVisible(*owner, viewer); };
    group.nextRandomRaidMember = [owner](float radius) { return RandomRaidMemberNear(*owner, radius); };
    player.SetGroupCallbacks(group);
    player.SetSpellModSink(FactToSession(&player, &BuildSpellModifierPacket));
}
