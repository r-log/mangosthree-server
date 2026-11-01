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

#include "ChatPackets.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "session/packets/chat/MessageChatPacket.h"

namespace MaNGOS
{
    /**
     * @brief Monster chat builder functor
     *
     * Builds localized chat packets for monster messages.
     */
    class MonsterChatBuilder
    {
        public:
            /**
             * @brief Constructor
             * @param obj Source object
             * @param msgtype Chat message type
             * @param textData Localized text data
             * @param language Language
             * @param target Target unit
             */
            MonsterChatBuilder(WorldObject const& obj, ChatMsg msgtype, MangosStringLocale const* textData, Language language, Unit const* target)
                : i_object(obj), i_msgtype(msgtype), i_textData(textData), i_language(language), i_target(target) {}

            /**
             * @brief Build chat packet
             * @param data Packet to build
             * @param loc_idx Locale index
             */
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = NULL;
                if ((int32)i_textData->Content.size() > loc_idx + 1 && !i_textData->Content[loc_idx + 1].empty())
                {
                    text = i_textData->Content[loc_idx + 1].c_str();
                }
                else
                {
                    text = i_textData->Content[0].c_str();
                }

                BuildMessageChatPacket(data, i_msgtype, text, i_language, CHAT_TAG_NONE, i_object.GetObjectGuid(), i_object.GetNameForLocaleIdx(loc_idx),
                                i_target ? i_target->GetObjectGuid() : ObjectGuid(), i_target ? i_target->GetNameForLocaleIdx(loc_idx) : "");
            }

        private:
            WorldObject const& i_object; ///< Source object
            ChatMsg i_msgtype; ///< Chat message type
            MangosStringLocale const* i_textData; ///< Localized text data
            Language i_language; ///< Language
            Unit const* i_target; ///< Target unit
    };
}                                                           // namespace MaNGOS

void SessionChatNotices::MonsterSaid(WorldObject const& speaker, ChatMsg msgtype, char const* text, Unit const* target, float range)
{
    WorldPacket data;
    BuildMessageChatPacket(data, msgtype, text, LANG_UNIVERSAL, CHAT_TAG_NONE, speaker.GetObjectGuid(), speaker.GetName(),
                           target ? target->GetObjectGuid() : ObjectGuid(), target ? target->GetName() : "");
    speaker.SendMessageToSetInRange(&data, range, true);
}

void SessionChatNotices::MonsterTextAround(WorldObject const& speaker, MangosStringLocale const& text, ChatMsg msgtype, Language language, Unit const* target, float range)
{
    MaNGOS::MonsterChatBuilder say_build(speaker, msgtype, &text, language, target);
    MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> say_do(say_build);
    MaNGOS::CameraDistWorker<MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> > say_worker(&speaker, range, say_do);
    Cell::VisitWorldObjects(&speaker, say_worker, range);
}

void SessionChatNotices::MonsterTextTo(WorldObject const& speaker, MangosStringLocale const& text, ChatMsg msgtype, Language language, Unit const* target, std::vector<Player const*> const& listeners)
{
    MaNGOS::MonsterChatBuilder say_build(speaker, msgtype, &text, language, target);
    MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> say_do(say_build);
    for (std::vector<Player const*>::const_iterator itr = listeners.begin(); itr != listeners.end(); ++itr)
    {
        say_do(*itr);
    }
}

void SessionChatNotices::Said(Player& speaker, ChatMsg msgtype, std::string const& text, uint32 language, float range, bool ownTeamOnly)
{
    WorldPacket data;
    BuildMessageChatPacket(data, msgtype, text.c_str(), Language(language), speaker.GetChatTag(), speaker.GetObjectGuid(), speaker.GetName());
    speaker.SendMessageToSetInRange(&data, range, true, ownTeamOnly);
}

void SessionChatNotices::Whispered(Player& speaker, Player& receiver, std::string const& text, uint32 language)
{
    WorldPacket data;
    BuildMessageChatPacket(data, CHAT_MSG_WHISPER, text.c_str(), Language(language), speaker.GetChatTag(), speaker.GetObjectGuid(), speaker.GetName());
    receiver.SendDirectMessage(&data);

    if (language == LANG_ADDON)
    {
        data.clear();
        BuildMessageChatPacket(data, CHAT_MSG_WHISPER, text.c_str(), Language(language), CHAT_TAG_NONE, receiver.GetObjectGuid());
        speaker.SendDirectMessage(&data);
    }

    data.clear();
    BuildMessageChatPacket(data, CHAT_MSG_WHISPER_INFORM, text.c_str(), Language(language), CHAT_TAG_NONE, receiver.GetObjectGuid());
    speaker.SendDirectMessage(&data);
}

void SessionChatNotices::AutoReplied(Player& speaker, Player const& receiver, ChatMsg msgtype)
{
    WorldPacket data;
    BuildMessageChatPacket(data, msgtype, receiver.autoReplyMsg.c_str(), LANG_UNIVERSAL, CHAT_TAG_NONE, receiver.GetObjectGuid());
    speaker.SendDirectMessage(&data);
}
