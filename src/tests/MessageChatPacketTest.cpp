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

#include "Opcodes.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "session/packets/chat/MessageChatPacket.h"
#include "WorldPacket.h"

/**
 * @file
 * @brief SMSG_MESSAGECHAT and SMSG_GM_MESSAGECHAT byte for byte, one case per branch of the builder.
 *
 * Every message starts with the type (one byte), the language (uint32), the sender's guid (uint64)
 * and a zero uint32, and ends with the text's length (uint32, the NUL counted), the text and its
 * NUL, and the tag byte. All values are little-endian; a string is its bytes and a NUL.
 */

namespace
{
    ObjectGuid const kSender(uint64(0x0102030405060708));
    ObjectGuid const kPlayer(HIGHGUID_PLAYER, 5);              // raw 0x0000000000000005
    ObjectGuid const kCreature(HIGHGUID_UNIT, 448, 0x56);      // raw 0xF13001C000000056
}

TEST(MessageChatPacket_system_message)
{
    WorldPacket data;
    BuildMessageChatPacket(data, CHAT_MSG_SYSTEM, "Hi", LANG_UNIVERSAL, CHAT_TAG_NONE, kSender);

    CHECK_EQ(data.GetOpcode(), uint16(SMSG_MESSAGECHAT));
    // 00 type | 00000000 language | sender | 00000000 | the target (empty) | 03000000 "Hi" | 00 tag
    CHECK_HEX(data.contents(), data.size(),
        "00" "00000000" "0807060504030201" "00000000" "0000000000000000" "03000000" "486900" "00");
}

TEST(MessageChatPacket_gm_tag_sends_the_gm_opcode_and_the_name)
{
    WorldPacket data;
    BuildMessageChatPacket(data, CHAT_MSG_SAY, "Hi", LANG_COMMON, CHAT_TAG_GM, kPlayer, "GM");

    CHECK_EQ(data.GetOpcode(), uint16(SMSG_GM_MESSAGECHAT));
    // 01 say | 07000000 common | sender | 00000000 | 03000000 "GM" | the target (empty) | text | 04 GM tag
    CHECK_HEX(data.contents(), data.size(),
        "01" "07000000" "0500000000000000" "00000000" "03000000" "474d00" "0000000000000000" "03000000" "486900" "04");
}

TEST(MessageChatPacket_channel_message_carries_the_channel_name)
{
    WorldPacket data;
    BuildMessageChatPacket(data, CHAT_MSG_CHANNEL, "Hi", LANG_COMMON, CHAT_TAG_NONE, kPlayer, NULL, ObjectGuid(), NULL, "World");

    CHECK_EQ(data.GetOpcode(), uint16(SMSG_MESSAGECHAT));
    // 11 channel | language | sender | 00000000 | "World" | the target (empty) | text | 00
    CHECK_HEX(data.contents(), data.size(),
        "11" "07000000" "0500000000000000" "00000000" "576f726c6400" "0000000000000000" "03000000" "486900" "00");
}

TEST(MessageChatPacket_addon_message_carries_the_prefix)
{
    WorldPacket data;
    BuildMessageChatPacket(data, CHAT_MSG_ADDON, "Hi", LANG_ADDON, CHAT_TAG_NONE, kPlayer, NULL, ObjectGuid(), NULL, NULL, 0, "PFX");

    // ff addon (0xFFFFFFFF as a byte) | ffffffff the addon language | sender | 00000000 | "PFX" | text | 00
    CHECK_HEX(data.contents(), data.size(),
        "ff" "ffffffff" "0500000000000000" "00000000" "50465800" "03000000" "486900" "00");
}

TEST(MessageChatPacket_achievement_message_ends_with_the_achievement)
{
    WorldPacket data;
    BuildMessageChatPacket(data, CHAT_MSG_ACHIEVEMENT, "Hi", LANG_UNIVERSAL, CHAT_TAG_NONE, kPlayer, NULL, kPlayer, NULL, NULL, 546);

    // 30 achievement | language | sender | 00000000 | the target | text | 00 | 22020000 achievement 546
    CHECK_HEX(data.contents(), data.size(),
        "30" "00000000" "0500000000000000" "00000000" "0500000000000000" "03000000" "486900" "00" "22020000");
}

TEST(MessageChatPacket_raid_boss_whisper_ends_with_a_float_and_a_byte)
{
    WorldPacket data;
    BuildMessageChatPacket(data, CHAT_MSG_RAID_BOSS_WHISPER, "Hi", LANG_UNIVERSAL, CHAT_TAG_NONE, kCreature, "Boss", kPlayer);

    // 2a raid boss whisper | language | sender | 00000000 | 05000000 "Boss" | the target, a player,
    // so no target name | text | 00 | 00000000 float 0 | 00
    CHECK_HEX(data.contents(), data.size(),
        "2a" "00000000" "56000000c00130f1" "00000000" "05000000" "426f737300" "0500000000000000" "03000000" "486900" "00"
        "00000000" "00");
}

TEST(MessageChatPacket_monster_say_names_a_creature_target)
{
    WorldPacket data;
    BuildMessageChatPacket(data, CHAT_MSG_MONSTER_SAY, "Hi", LANG_UNIVERSAL, CHAT_TAG_NONE, kCreature, "Hogger", kCreature, "Bob");

    // 0c monster say | language | sender | 00000000 | 07000000 "Hogger" | the target, a creature,
    // so 04000000 "Bob" | text | 00
    CHECK_HEX(data.contents(), data.size(),
        "0c" "00000000" "56000000c00130f1" "00000000" "07000000" "486f6767657200" "56000000c00130f1" "04000000" "426f6200"
        "03000000" "486900" "00");
}
