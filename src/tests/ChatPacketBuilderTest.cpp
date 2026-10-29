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

#include "TestHarness.h"

#include <string>
#include "Opcodes.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "data/ChatPacketBuilder.h"
#include "session/packets/chat/MessageChatPacket.h"

/**
 * @file
 * @brief The chat packet builder of the code below the session: BuildChatPacket hands every field
 * to the installed builder, and with the session's builder installed it writes the session's bytes.
 */

namespace
{
    std::string g_seen;

    /// Records the fields it is handed, in order.
    void RecordingBuilder(WorldPacket& /*data*/, ChatMsg msgtype, char const* message, Language language, ChatTagFlags chatTag,
                          ObjectGuid const& senderGuid, char const* senderName,
                          ObjectGuid const& targetGuid, char const* targetName,
                          char const* channelName, uint32 achievementId, const char* addonPrefix)
    {
        g_seen = std::to_string(uint32(msgtype)) + "|" + message + "|" + std::to_string(uint32(language)) + "|" + std::to_string(chatTag) + "|"
                 + std::to_string(senderGuid.GetRawValue()) + "|" + (senderName ? senderName : "-") + "|"
                 + std::to_string(targetGuid.GetRawValue()) + "|" + (targetName ? targetName : "-") + "|"
                 + (channelName ? channelName : "-") + "|" + std::to_string(achievementId) + "|" + (addonPrefix ? addonPrefix : "-");
    }

    /// Installs `builder` for one test and puts the old one back.
    class InstalledBuilder
    {
        public:
            explicit InstalledBuilder(ChatPacketBuilder builder) : m_previous(InstallChatPacketBuilder(builder)) {}
            ~InstalledBuilder() { InstallChatPacketBuilder(m_previous); }
        private:
            ChatPacketBuilder m_previous;
    };
}

TEST(ChatPacketBuilder_hands_every_field_to_the_installed_builder)
{
    InstalledBuilder installed(&RecordingBuilder);
    WorldPacket data;

    BuildChatPacket(data, CHAT_MSG_CHANNEL, "Hi", LANG_COMMON, CHAT_TAG_GM, ObjectGuid(uint64(5)), "GM", ObjectGuid(uint64(7)), "Bob", "World", 546, "PFX");
    CHECK_STR(g_seen, "17|Hi|7|4|5|GM|7|Bob|World|546|PFX");

    BuildChatPacket(data, CHAT_MSG_SYSTEM, "Hi");
    CHECK_STR(g_seen, "0|Hi|0|0|0|-|0|-|-|0|-");
}

TEST(ChatPacketBuilder_with_the_session_builder_writes_the_session_bytes)
{
    InstalledBuilder installed(&BuildMessageChatPacket);
    WorldPacket data;

    BuildChatPacket(data, CHAT_MSG_SYSTEM, "Hi", LANG_UNIVERSAL, CHAT_TAG_NONE, ObjectGuid(uint64(0x0102030405060708)));

    CHECK_EQ(data.GetOpcode(), uint16(SMSG_MESSAGECHAT));
    CHECK_HEX(data.contents(), data.size(),
        "00" "00000000" "0807060504030201" "00000000" "0000000000000000" "03000000" "486900" "00");
}
