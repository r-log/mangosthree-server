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

#include "Utilities/Errors.h"
#include "Object.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "data/GameConfig.h"
#include "Creature.h"
#include "Player.h"
#include "Vehicle.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "UpdateMask.h"
#include "Util.h"
#include "MapManager.h"
#include "Log.h"
#include "Transports.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "movement/packet_builder.h"
#include "CreatureLinkingMgr.h"
#include "entities/ChatNotices.h"
#include "GameTime.h"
#include "TransportMap.h"

/**
 * @file WorldObjectChat.cpp
 * @brief WorldObject's chat and messages: monster say, yell, emote and whisper, localized text broadcast,
 * message-to-set delivery and the despawn and custom-animation packets.
 */

/**
 * @brief Send monster say message
 * @param text Text to say
 * @param language Language (unused)
 * @param target Target unit
 *
 * Sends a monster say message to nearby players.
 */
void WorldObject::MonsterSay(const char* text, uint32 language, Unit const* target) const
{
    sChatNotices.MonsterSaid(*this, CHAT_MSG_MONSTER_SAY, text, target, sGameConfig.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY));
}

/**
 * @brief Send monster yell message
 * @param text Text to yell
 * @param language Language (unused)
 * @param target Target unit
 *
 * Sends a monster yell message to nearby players.
 */
void WorldObject::MonsterYell(const char* text, uint32 language, Unit const* target) const
{
    sChatNotices.MonsterSaid(*this, CHAT_MSG_MONSTER_YELL, text, target, sGameConfig.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL));
}

/**
 * @brief Send monster text emote message
 * @param text Text to emote
 * @param target Target unit
 * @param IsBossEmote If true, use boss emote range
 *
 * Sends a monster text emote message to nearby players.
 */
void WorldObject::MonsterTextEmote(const char* text, Unit const* target, bool IsBossEmote) const
{
    sChatNotices.MonsterSaid(*this, IsBossEmote ? CHAT_MSG_RAID_BOSS_EMOTE : CHAT_MSG_MONSTER_EMOTE, text, target,
                             sGameConfig.getConfig(IsBossEmote ? CONFIG_FLOAT_LISTEN_RANGE_YELL : CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE));
}

/**
 * @brief Send monster whisper message
 * @param text Text to whisper
 * @param target Target unit
 * @param IsBossWhisper If true, use boss whisper
 *
 * Sends a monster whisper message to the target player.
 */
void WorldObject::MonsterWhisper(const char* text, Unit const* target, bool IsBossWhisper) const
{
    ClientCallbacks const* client = target ? target->GetClientCallbacks() : NULL;
    if (!client)
    {
        return;
    }

    MANGOS_ASSERT(text);
    MonsterWhisperFact fact;
    fact.sender = GetObjectGuid();
    fact.senderName = GetName();
    fact.text = text;
    fact.bossWhisper = IsBossWhisper;
    ReportClientFact(client->monsterWhisper, fact);
}

/**
 * @brief Send monster text
 * @param textData Localized text data
 * @param target Target unit
 *
 * Sends a text message associated with a MangosString,
 * localized for each player's locale.
 */
/**
 * @brief Sends a localized monster text defined by a Mangos string entry.
 *
 * @param textData The localized text definition.
 * @param target Optional target unit.
 */
void WorldObject::MonsterText(MangosStringLocale const* textData, Unit const* target) const
{
    MANGOS_ASSERT(textData);

    switch (textData->Type)
    {
        case CHAT_TYPE_SAY:
            sChatNotices.MonsterTextAround(*this, *textData, CHAT_MSG_MONSTER_SAY, textData->LanguageId, target, sGameConfig.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY));
            break;
        case CHAT_TYPE_YELL:
            sChatNotices.MonsterTextAround(*this, *textData, CHAT_MSG_MONSTER_YELL, textData->LanguageId, target, sGameConfig.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL));
            break;
        case CHAT_TYPE_TEXT_EMOTE:
            sChatNotices.MonsterTextAround(*this, *textData, CHAT_MSG_MONSTER_EMOTE, LANG_UNIVERSAL, target, sGameConfig.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE));
            break;
        case CHAT_TYPE_BOSS_EMOTE:
            sChatNotices.MonsterTextAround(*this, *textData, CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, target, sGameConfig.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL));
            break;
        case CHAT_TYPE_WHISPER:
        {
            Player const* targetPlayer = target ? target->ToPlayer() : NULL;
            if (!targetPlayer)
            {
                return;
            }
            sChatNotices.MonsterTextTo(*this, *textData, CHAT_MSG_MONSTER_WHISPER, LANG_UNIVERSAL, target, std::vector<Player const*>(1, targetPlayer));
            break;
        }
        case CHAT_TYPE_BOSS_WHISPER:
        {
            Player const* targetPlayer = target ? target->ToPlayer() : NULL;
            if (!targetPlayer)
            {
                return;
            }
            sChatNotices.MonsterTextTo(*this, *textData, CHAT_MSG_RAID_BOSS_WHISPER, LANG_UNIVERSAL, target, std::vector<Player const*>(1, targetPlayer));
            break;
        }
        case CHAT_TYPE_ZONE_YELL:
        {
            uint32 zoneid = GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z());
            std::vector<Player const*> listeners;
            Map::PlayerList const& pList = GetMap()->GetPlayers();
            for (Map::PlayerList::const_iterator itr = pList.begin(); itr != pList.end(); ++itr)
                if (itr->getSource()->GetTerrain()->GetZoneId(itr->getSource()->Where().X(), itr->getSource()->Where().Y(), itr->getSource()->Where().Z()) == zoneid)
                {
                    listeners.push_back(itr->getSource());
                }
            sChatNotices.MonsterTextTo(*this, *textData, CHAT_MSG_MONSTER_YELL, textData->LanguageId, target, listeners);
            break;
        }
    }
}

/**
 * @brief Broadcasts a packet to all players in the object's visibility set.
 *
 * @param data The packet to send.
 * @param bToSelf Unused self-delivery flag.
 */
void WorldObject::SendMessageToSet(WorldPacket* data, bool /*bToSelf*/) const
{
    if (!IsInWorld())
    {
        return;
    }

    // A boarded unit's audience is EVERYONE ON ITS OWN MAP -- which is the deck, and which
    // is exactly where the passengers are too. That is the whole point of the vessel being
    // a map: the ordinary broadcast below already reaches the people standing next to him.
    GetMap()->MessageBroadcast(this, data);

    // THE RELAY, OUTBOUND. The people ashore are on another map and no cell of theirs will
    // ever hold this deckhand, so the same packet goes out again to the watchers the vessel
    // gathered at the top of this tick. Sent immediately: the deck runs INSIDE the tick of
    // the map it sails, on that map's own thread, so there is nothing to wait for and no
    // race to avoid. Without this a deckhand walks for his shipmates and stands frozen for
    // the pier.
    if (GetMap()->AsTransport())
    {
        for (Player* observer : GetMap()->ExternalObservers())
        {
            if (observer && observer->GetSession())
            {
                observer->GetSession()->SendPacket(data);
            }
        }

        return;
    }

    // THE RELAY, INBOUND, and it is not filtered by anything. Whatever happens on the water
    // a ship is crossing has to reach the people standing on her: they are on another map,
    // no cell of theirs will ever hold the thing that moved, and a passenger who is told
    // nothing watches a harbour full of statues.
    //
    // No distance test. The one object that could measure it is the vessel, whose pose is an
    // estimate we already refuse to trust for anything that decides something -- and being
    // told about a creature too far away costs a packet, while not being told about one in
    // front of you costs the illusion that the world is running.
    MapManager::TransportsByMapType::const_iterator vessels =
        sMapMgr.m_TransportsByMap.find(GetMapId());
    if (vessels == sMapMgr.m_TransportsByMap.end())
    {
        return;
    }

    for (Transport* vessel : vessels->second)
    {
        TransportMap* hull = vessel->AsMap();
        if (!hull || vessel->FindMap() != FindMap())
        {
            continue;
        }

        Map::PlayerList const& aboard = hull->GetPlayers();
        for (Map::PlayerList::const_iterator itr = aboard.begin(); itr != aboard.end(); ++itr)
        {
            Player* passenger = itr->getSource();
            if (passenger && passenger->GetSession())
            {
                passenger->GetSession()->SendPacket(data);
            }
        }
    }
}

/**
 * @brief Broadcasts a packet to players within a specified range.
 *
 * @param data The packet to send.
 * @param dist The broadcast distance.
 * @param bToSelf Unused self-delivery flag.
 */
void WorldObject::SendMessageToSetInRange(WorldPacket* data, float dist, bool /*bToSelf*/) const
{
    // if object is in world, map for it already created!
    if (IsInWorld())
    {
        GetMap()->MessageDistBroadcast(this, data, dist);
    }
}

/**
 * @brief Broadcasts a packet to visible players except one receiver.
 *
 * @param data The packet to send.
 * @param skipped_receiver The player to exclude.
 */
void WorldObject::SendMessageToSetExcept(WorldPacket* data, Player const* skipped_receiver) const
{
    // if object is in world, map for it already created!
    if (IsInWorld())
    {
        MaNGOS::MessageDelivererExcept notifier(this, data, skipped_receiver);
        Cell::VisitWorldObjects(this, notifier, GetMap()->GetBroadcastRadius());
    }
}

void WorldObject::SendObjectDeSpawnAnim(ObjectGuid guid)
{
    WorldPacket data(SMSG_GAMEOBJECT_DESPAWN_ANIM, 8);
    data << ObjectGuid(guid);
    SendMessageToSet(&data, true);
}

void WorldObject::SendGameObjectCustomAnim(ObjectGuid guid, uint32 animId /*= 0*/)
{
    WorldPacket data(SMSG_GAMEOBJECT_CUSTOM_ANIM, 8 + 4);
    data << ObjectGuid(guid);
    data << uint32(animId);
    SendMessageToSet(&data, true);
}

bool DoDisplayText(WorldObject* source, int32 entry, Unit const* target /*=NULL*/)
{
    MangosStringLocale const* data = sObjectMgr.GetMangosStringLocale(entry);

    if (!data)
    {
        LogTextEntryError(entry, "DoScriptText with source %s could not find text entry %i.", source->GetGuidStr().c_str(), entry);
        return false;
    }

    if (data->SoundId)
    {
        if (data->Type == CHAT_TYPE_ZONE_YELL)
        {
            source->GetMap()->PlayDirectSoundToMap(data->SoundId, source->GetTerrain()->GetZoneId(source->Where().X(), source->Where().Y(), source->Where().Z()));
        }
        else if (data->Type == CHAT_TYPE_WHISPER || data->Type == CHAT_TYPE_BOSS_WHISPER)
        {
            // An error will be displayed for the text
            if (Player const* targetPlayer = target ? target->ToPlayer() : NULL)
            {
                source->PlayDirectSound(data->SoundId, targetPlayer);
            }
        }
        else
        {
            source->PlayDirectSound(data->SoundId);
        }
    }

    if (data->Emote)
    {
        if (source->GetTypeId() == TYPEID_UNIT || source->GetTypeId() == TYPEID_PLAYER)
        {
            ((Unit*)source)->HandleEmote(data->Emote);
        }
        else
        {
            LogTextEntryError(entry, "DoDisplayText entry %i tried to process emote for invalid source %s", entry, source->GetGuidStr().c_str());
            return false;
        }
    }

    if ((data->Type == CHAT_TYPE_WHISPER || data->Type == CHAT_TYPE_BOSS_WHISPER) && (!target || target->GetTypeId() != TYPEID_PLAYER))
    {
        LogTextEntryError(entry, "DoDisplayText entry %i can not whisper without target unit (TYPEID_PLAYER).", entry);
        return false;
    }

    source->MonsterText(data, target);
    return true;
}
