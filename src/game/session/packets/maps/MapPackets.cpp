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

#include "MapPackets.h"
#include "Creature.h"
#include "InstanceData.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "SoundPacket.h"
#include "WorldPacket.h"
#include "session/packets/chat/LocalizedPackets.h"
#include "session/packets/chat/MessageChatPacket.h"

namespace
{
    /// Builds a creature template's yell in one locale, as the spawn it names.
    class StaticMonsterChatBuilder
    {
        public:
            StaticMonsterChatBuilder(CreatureInfo const* cInfo, ChatMsg msgtype, int32 textId, Language language, Unit const* target, uint32 senderLowGuid = 0)
                : i_cInfo(cInfo), i_msgtype(msgtype), i_textId(textId), i_language(language), i_target(target)
            {
                // 0 lowguid not used in core, but accepted fine in this case by client
                i_senderGuid = i_cInfo->GetObjectGuid(senderLowGuid);
            }
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);

                char const* nameForLocale = i_cInfo->Name;
                sObjectMgr.GetCreatureLocaleStrings(i_cInfo->Entry, loc_idx, &nameForLocale);

                BuildMessageChatPacket(data, i_msgtype, text, i_language, CHAT_TAG_NONE, i_senderGuid, nameForLocale, i_target ? i_target->GetObjectGuid() : ObjectGuid(),
                    i_target ? i_target->GetNameForLocaleIdx(loc_idx) : "");
            }

        private:
            ObjectGuid i_senderGuid;
            CreatureInfo const* i_cInfo;
            ChatMsg i_msgtype;
            int32 i_textId;
            Language i_language;
            Unit const* i_target;
    };
}

void BuildInstanceSaveCreatedPacket(WorldPacket& packet)
{
    packet.Initialize(SMSG_INSTANCE_SAVE_CREATED, 4);
    packet << uint32(0);
}

void BuildEncounterFramePacket(WorldPacket& packet, uint32 type, ObjectGuid source, uint8 param1, uint8 param2)
{
    // size of this packet is at most 15 (usually less)
    packet.Initialize(SMSG_INSTANCE_ENCOUNTER, 15);
    packet << uint32(type);

    switch (type)
    {
        case ENCOUNTER_FRAME_ENGAGE:
        case ENCOUNTER_FRAME_DISENGAGE:
        case ENCOUNTER_FRAME_UPDATE_PRIORITY:
            MANGOS_ASSERT(source);

            packet << source.WriteAsPacked();
            packet << uint8(param1);
            break;
        case ENCOUNTER_FRAME_ADD_TIMER:
        case ENCOUNTER_FRAME_ENABLE_OBJECTIVE:
        case ENCOUNTER_FRAME_DISABLE_OBJECTIVE:
        case ENCOUNTER_FRAME_SET_COMBAT_RES_LIMIT:
            packet << uint8(param1);
            break;
        case ENCOUNTER_FRAME_UPDATE_OBJECTIVE:
            packet << uint8(param1);
            packet << uint8(param2);
            break;
        case ENCOUNTER_FRAME_UNK7:
        case ENCOUNTER_FRAME_ADD_COMBAT_RES_LIMIT:
        case ENCOUNTER_FRAME_RESET_COMBAT_RES_LIMIT:
        default:
            break;
    }
}

void BuildWeatherPacket(WorldPacket& packet, WeatherState state, float grade)
{
    packet.Initialize(SMSG_WEATHER, 4 + 4 + 1);
    packet << uint32(state) << float(grade) << uint8(0);
}

void SessionMapNotices::InstanceSaveCreated(Player& player)
{
    WorldPacket data;
    BuildInstanceSaveCreatedPacket(data);
    player.GetSession()->SendPacket(&data);
}

void SessionMapNotices::EncounterFrame(Map const& map, uint32 type, ObjectGuid source, uint8 param1, uint8 param2)
{
    WorldPacket data;
    BuildEncounterFramePacket(data, type, source, param1, param2);
    map.SendToPlayers(&data);
}

void SessionMapNotices::WeatherFor(Player& player, WeatherState state, float grade)
{
    WorldPacket data;
    BuildWeatherPacket(data, state, grade);
    player.GetSession()->SendPacket(&data);
}

bool SessionMapNotices::WeatherInZone(Map const& map, uint32 zoneId, WeatherState state, float grade)
{
    WorldPacket data;
    BuildWeatherPacket(data, state, grade);
    return map.SendToPlayersInZone(&data, zoneId);
}

void SessionMapNotices::SoundToMap(Map const& map, uint32 soundId, uint32 zoneId)
{
    WorldPacket data;
    BuildPlaySoundPacket(data, soundId);
    if (zoneId)
    {
        map.SendToPlayersInZone(&data, zoneId);
    }
    else
    {
        map.SendToPlayers(&data);
    }
}

void SessionMapNotices::MonsterYell(Map const& map, CreatureInfo const& cinfo, int32 textId, Language language, Unit const* target, uint32 senderLowGuid)
{
    StaticMonsterChatBuilder say_build(&cinfo, CHAT_MSG_MONSTER_YELL, textId, language, target, senderLowGuid);
    MaNGOS::LocalizedPacketDo<StaticMonsterChatBuilder> say_do(say_build);

    Map::PlayerList const& pList = map.GetPlayers();
    for (Map::PlayerList::const_iterator itr = pList.begin(); itr != pList.end(); ++itr)
    {
        say_do(itr->getSource());
    }
}
