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

/// The client's question "who ends these quests" (CMSG_QUERY_QUEST_COMPLETION_NPCS, 0x7302) and the
/// answer (SMSG_QUEST_COMPLETION_NPC_RESPONSE, 0x75A1), byte for byte against the 15595 client.
///
/// The client writes the question in PlayerQueryQuestCompletionNPCs::Put (OFFSET 0x00680AB0/
/// 0x14032A460): a 24-bit quest count, flushed, then the uint32 quest ids. It reads the answer in
/// ClientQuestNpcQueryResponse::Unpack (OFFSET 0x006F6DB0/0x1403BE970): a 23-bit quest count, a
/// 24-bit ender count for each quest, the bits padded to a whole byte, then for each quest its
/// uint32 id and its uint32 enders. Both sides put the highest bit of a value first and fill each
/// byte from bit 7. The minimap matches a game object blip against `ender & 0x7FFFFFFF`
/// (IsCreatureInBlipList, OFFSET 0x009CF160/0x14075FDB0), so a game object ender has bit 31 set.

#include "TestHarness.h"

#include "Auth/BigNumber.h"
#include "OpcodeTable.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "data/QuestRelationStore.h"
#include "session/handlers/entities/QuestHandlers.h"
#include "session/packets/quests/QuestCompletionNpcsPacket.h"

#include <vector>

namespace
{
    uint32 const kQuestMarshal = 28766;     // ended by Marshal McBride, creature 197
    uint32 const kQuestNoEnder = 26389;
    uint32 const kQuestOther = 28767;
    uint32 const kMarshalMcBride = 197;
    uint32 const kCrate = 196394;           // a quest giver game object
    uint32 const kOtherCreature = 823;

    /// Marshal McBride and the crate end 28766, creature 823 ends 28767, nothing ends 26389.
    QuestRelationStore Relations()
    {
        QuestRelationStore store;
        store.CreatureEndsForEdit().insert(QuestRelationsMap::value_type(kMarshalMcBride, kQuestMarshal));
        store.GameObjectEndsForEdit().insert(QuestRelationsMap::value_type(kCrate, kQuestMarshal));
        store.CreatureEndsForEdit().insert(QuestRelationsMap::value_type(kOtherCreature, kQuestOther));
        store.RebuildQuestEnders();
        return store;
    }

    /// The client's question for `questIds`, as PlayerQueryQuestCompletionNPCs::Put writes it.
    WorldPacket Question(std::vector<uint32> const& questIds)
    {
        WorldPacket request(CMSG_QUERY_QUEST_COMPLETION_NPCS, 3 + 4 * questIds.size());
        request.WriteBits(uint32(questIds.size()), 24);
        request.FlushBits();
        for (uint32 questId : questIds)
        {
            request << uint32(questId);
        }
        return request;
    }

    struct AnsweredQuest
    {
        uint32 questId;
        std::vector<uint32> enders;
    };

    /// The answer read as ClientQuestNpcQueryResponse::Unpack reads it; `complete` is false when a
    /// byte is missing or left over.
    std::vector<AnsweredQuest> ReadAnswer(WorldPacket const& reply, bool& complete)
    {
        WorldPacket wire(reply);
        wire.rpos(0);
        wire.ResetBitReader();

        std::vector<AnsweredQuest> quests(wire.ReadBits(23));
        for (AnsweredQuest& quest : quests)
        {
            quest.enders.resize(wire.ReadBits(24));
        }

        complete = true;
        for (AnsweredQuest& quest : quests)
        {
            if (wire.size() - wire.rpos() < 4 * (1 + quest.enders.size()))
            {
                complete = false;
                return quests;
            }
            wire >> quest.questId;
            for (uint32& ender : quest.enders)
            {
                wire >> ender;
            }
        }
        complete = wire.rpos() == wire.size();
        return quests;
    }

    void CapturePacket(void* context, WorldPacket const& packet)
    {
        static_cast<std::vector<WorldPacket>*>(context)->push_back(packet);
    }
}

TEST(QuestCompletionNpcs_TheAnswerForOneQuestEndedByACreature)
{
    QuestRelationStore relations;
    relations.CreatureEndsForEdit().insert(QuestRelationsMap::value_type(kMarshalMcBride, kQuestMarshal));
    relations.RebuildQuestEnders();

    WorldPacket packet;
    BuildQuestCompletionNpcResponse(packet, relations, std::vector<uint32>{ kQuestMarshal });

    CHECK_EQ(packet.GetOpcode(), uint16(SMSG_QUEST_COMPLETION_NPC_RESPONSE));
    // 000002000002  bits: quest count 1 in 23 bits, ender count 1 in 24 bits, one pad bit (47 + 1)
    // 5e700000      quest 28766 = 0x705E
    // c5000000      Marshal McBride, creature 197 = 0xC5
    CHECK_HEX(packet.contents(), packet.size(), "000002000002" "5e700000" "c5000000");
}

TEST(QuestCompletionNpcs_TwoQuestsOneWithBothKindsOfEnderAndOneWithNone)
{
    WorldPacket packet;
    BuildQuestCompletionNpcResponse(packet, Relations(), std::vector<uint32>{ kQuestMarshal, kQuestNoEnder });

    CHECK_EQ(packet.GetOpcode(), uint16(SMSG_QUEST_COMPLETION_NPC_RESPONSE));
    // 000004  quest count 2 in 23 bits (bits 0-22), then the first bit of 28766's ender count
    // 000004  the rest of 28766's ender count 2 (bits 23-46), then the first bit of 26389's
    // 000000  the rest of 26389's ender count 0 (bits 47-70), and the pad bit 71
    // 5e700000  quest 28766, then its enders: the creature first, the game object flagged
    // c5000000  creature 197
    // 2aff0280  game object 196394 = 0x2FF2A, with bit 31 set: 0x8002FF2A
    // 15670000  quest 26389 = 0x6715, with no enders after it
    CHECK_HEX(packet.contents(), packet.size(),
        "000004" "000004" "000000" "5e700000" "c5000000" "2aff0280" "15670000");
}

TEST(QuestCompletionNpcs_NoQuestsIsAZeroCountAndAPaddedByte)
{
    // 23 bits of count and one pad bit. The builder initializes the packet, so a reused one keeps
    // neither its old bytes nor its old opcode.
    WorldPacket packet(SMSG_QUESTUPDATE_COMPLETE, 8);
    packet << uint32(0xDEADBEEF);
    packet.WriteBit(true);

    BuildQuestCompletionNpcResponse(packet, Relations(), std::vector<uint32>());

    CHECK_EQ(packet.GetOpcode(), uint16(SMSG_QUEST_COMPLETION_NPC_RESPONSE));
    CHECK_HEX(packet.contents(), packet.size(), "000000");
}

TEST(QuestCompletionNpcs_AQuestWithNoEndersGetsAnEmptyListAndTheOthersKeepTheirs)
{
    WorldPacket request = Question({ kQuestMarshal, kQuestNoEnder, kQuestOther });
    WorldPacket reply;
    AnswerQuestCompletionNpcsQuery(request, Relations(), reply);

    CHECK_EQ(request.rpos(), request.size());
    CHECK_EQ(reply.GetOpcode(), uint16(SMSG_QUEST_COMPLETION_NPC_RESPONSE));

    bool complete = false;
    std::vector<AnsweredQuest> quests = ReadAnswer(reply, complete);
    CHECK(complete);
    CHECK_EQ(quests.size(), size_t(3));
    if (quests.size() != 3)
    {
        return;
    }

    CHECK_EQ(quests[0].questId, kQuestMarshal);
    CHECK_EQ(quests[0].enders.size(), size_t(2));
    if (quests[0].enders.size() == 2)
    {
        CHECK_EQ(quests[0].enders[0], kMarshalMcBride);
        CHECK_EQ(quests[0].enders[1], kCrate | 0x80000000);
    }

    CHECK_EQ(quests[1].questId, kQuestNoEnder);
    CHECK_EQ(quests[1].enders.size(), size_t(0));

    CHECK_EQ(quests[2].questId, kQuestOther);
    CHECK_EQ(quests[2].enders.size(), size_t(1));
    if (quests[2].enders.size() == 1)
    {
        CHECK_EQ(quests[2].enders[0], kOtherCreature);
    }
}

TEST(QuestCompletionNpcs_AnAnswerCoversAtMostTheTwentyFiveQuestsOfAFullLog)
{
    std::vector<uint32> asked;
    for (uint32 questId = 1001; questId <= 1030; ++questId)
    {
        asked.push_back(questId);
    }
    asked[3] = kQuestMarshal;

    WorldPacket request = Question(asked);
    WorldPacket reply;
    AnswerQuestCompletionNpcsQuery(request, Relations(), reply);

    // The ids past the 25th are skipped, not left unread.
    CHECK_EQ(request.rpos(), request.size());

    bool complete = false;
    std::vector<AnsweredQuest> quests = ReadAnswer(reply, complete);
    CHECK(complete);
    CHECK_EQ(quests.size(), size_t(25));
    for (size_t i = 0; i < quests.size(); ++i)
    {
        CHECK_EQ(quests[i].questId, asked[i]);
        CHECK_EQ(quests[i].enders.size(), size_t(i == 3 ? 2 : 0));
    }

    // A full log of 25 is answered whole.
    asked.resize(25);
    WorldPacket fullLog = Question(asked);
    AnswerQuestCompletionNpcsQuery(fullLog, Relations(), reply);
    quests = ReadAnswer(reply, complete);
    CHECK(complete);
    CHECK_EQ(quests.size(), size_t(25));
}

TEST(QuestCompletionNpcs_TheRowSendsOneAnswerForTheQuestsAsked)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "questlog", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    std::vector<uint32> asked;
    for (uint32 questId = 2001; questId <= 2030; ++questId)
    {
        asked.push_back(questId);
    }
    WorldPacket request = Question(asked);
    opcodeTable[CMSG_QUERY_QUEST_COMPLETION_NPCS].handler(session, request);

    session.SetSocketlessSink(nullptr, nullptr);

    CHECK_EQ(request.rpos(), request.size());
    CHECK_EQ(sent.size(), size_t(1));
    if (sent.size() != 1)
    {
        return;
    }

    CHECK_EQ(sent[0].GetOpcode(), uint16(SMSG_QUEST_COMPLETION_NPC_RESPONSE));
    bool complete = false;
    std::vector<AnsweredQuest> quests = ReadAnswer(sent[0], complete);
    CHECK(complete);
    CHECK_EQ(quests.size(), size_t(25));
    for (size_t i = 0; i < quests.size(); ++i)
    {
        CHECK_EQ(quests[i].questId, asked[i]);
    }
}
