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

#include "session/handlers/entities/QuestHandlers.h"

#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "Log.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "data/GameConfig.h"
#include "ObjectMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "AchievementMgr.h"
#include "Player.h"
#include "GossipDef.h"
#include "QuestDef.h"
#include "PlayerRegistry.h"
#include "data/ScriptHooks.h"
#include "QuestGiverStatus.h"
#include "Group.h"
#include "persistence/characters/quests/QuestTrackerRepository.h"

void QuestHandlers::HandleQuestgiverStatusQuery(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;
    uint32 dialogStatus = DIALOG_STATUS_NONE;

    Object* questgiver = session.GetPlayer()->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);

    if (!questgiver)
    {
        DETAIL_LOG("Error in CMSG_QUESTGIVER_STATUS_QUERY, called for not found questgiver %s", guid.GetString().c_str());
        return;
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_STATUS_QUERY - for %s to %s", session.GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str());

    switch (questgiver->GetTypeId())
    {
        case TYPEID_UNIT:
        {
            Creature* cr_questgiver = questgiver->ToCreature();

            if (!cr_questgiver->IsHostileTo(session.GetPlayer()))       // not show quest status to enemies
            {
                dialogStatus = sScriptHooks.GetDialogStatus(session.GetPlayer(), cr_questgiver);

                if (dialogStatus > DIALOG_STATUS_REWARD_REP)
                {
                    dialogStatus = QuestGiverDialogStatus(session.GetPlayer(), cr_questgiver, DIALOG_STATUS_NONE);
                }
            }
            break;
        }
        case TYPEID_GAMEOBJECT:
        {
            GameObject* go_questgiver = (GameObject*)questgiver;
            dialogStatus = sScriptHooks.GetDialogStatus(session.GetPlayer(), go_questgiver);

            if (dialogStatus > DIALOG_STATUS_REWARD_REP)
            {
                dialogStatus = QuestGiverDialogStatus(session.GetPlayer(), go_questgiver, DIALOG_STATUS_NONE);
            }

            break;
        }
        default:
            sLog.outError("QuestGiver called for unexpected type %u", questgiver->GetTypeId());
            break;
    }

    // inform client about status of quest
    session.GetPlayer()->PlayerTalkClass->SendQuestGiverStatus(dialogStatus, guid);
}

/**
 * @brief Handles the initial hello interaction with a quest giver.
 *
 * @param recv_data The received opcode packet.
 */
void QuestHandlers::HandleQuestgiverHello(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_HELLO - for %s to %s", session.GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str());

    Creature* pCreature = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleQuestgiverHelloOpcode - for %s to %s not found or you can't interact with him.", session.GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    // Stop the npc if moving; a patrol holds its leg for a while so the player can talk
    pCreature->HoldForPlayer();

    if (sScriptHooks.OnGossipHello(session.GetPlayer(), pCreature))
    {
        return;
    }

    session.GetPlayer()->PrepareGossipMenu(pCreature, pCreature->GetCreatureInfo()->GossipMenuId);
    session.GetPlayer()->SendPreparedGossip(pCreature);
}

/**
 * @brief Handles accepting a quest from a quest giver or shared source.
 *
 * @param recv_data The received opcode packet.
 */
void QuestHandlers::HandleQuestgiverAcceptQuest(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint32 quest;
    uint32 unk1;
    recv_data >> guid >> quest >> unk1;

    if (!CanInteractWithQuestGiver(session, guid, "CMSG_QUESTGIVER_ACCEPT_QUEST"))
    {
        return;
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_ACCEPT_QUEST - for %s to %s, quest = %u, unk1 = %u", session.GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str(), quest, unk1);

    Object* pObject = session.GetPlayer()->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_GAMEOBJECT_PLAYER_OR_ITEM);

    // no or incorrect quest giver (the player is the quest giver for SPELL_EFFECT_QUEST_OFFER)
    Player* giverPlayer = pObject ? pObject->ToPlayer() : NULL;
    if (!pObject
            || (!giverPlayer && !pObject->HasQuest(quest))
            || (giverPlayer && giverPlayer != session.GetPlayer() && !giverPlayer->CanShareQuest(quest))
       )
    {
        session.GetPlayer()->PlayerTalkClass->CloseGossip();
        session.GetPlayer()->ClearDividerGuid();
        return;
    }

    Quest const* qInfo = sObjectMgr.GetQuestTemplate(quest);
    if (qInfo)
    {
        // prevent cheating
        if (!session.GetPlayer()->CanTakeQuest(qInfo, true))
        {
            session.GetPlayer()->PlayerTalkClass->CloseGossip();
            session.GetPlayer()->ClearDividerGuid();
            return;
        }

        if (Player* pPlayer = sPlayerRegistry.Find(session.GetPlayer()->GetDividerGuid()))
        {
            pPlayer->SendPushToPartyResponse(session.GetPlayer(), QUEST_PARTY_MSG_ACCEPT_QUEST);
            session.GetPlayer()->ClearDividerGuid();
        }

        if (session.GetPlayer()->CanAddQuest(qInfo, true))
        {
            session.GetPlayer()->AddQuest(qInfo, pObject);              // pObject (if it item) can be destroyed at call

            if (qInfo->HasQuestFlag(QUEST_FLAGS_PARTY_ACCEPT))
            {
                if (Group* pGroup = session.GetPlayer()->GetGroup())
                {
                    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                    {
                        Player* pPlayer = itr->getSource();

                        if (!pPlayer || pPlayer == session.GetPlayer()) // not self
                        {
                            continue;
                        }

                        if (pPlayer->CanTakeQuest(qInfo, true))
                        {
                            pPlayer->SetDividerGuid(session.GetPlayer()->GetObjectGuid());

                            // need confirmation that any gossip window will close
                            pPlayer->PlayerTalkClass->CloseGossip();

                            session.GetPlayer()->SendQuestConfirmAccept(qInfo, pPlayer);
                        }
                    }
                }
            }

            if (session.GetPlayer()->CanCompleteQuest(quest))
            {
                session.GetPlayer()->CompleteQuest(quest);
            }

            session.GetPlayer()->GetAchievementMgr().StartTimedAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST, quest);

            session.GetPlayer()->PlayerTalkClass->CloseGossip();

            if (qInfo->GetSrcSpell() > 0)
            {
                session.GetPlayer()->CastSpell(session.GetPlayer(), qInfo->GetSrcSpell(), true);
            }

            return;
        }
    }

    session.GetPlayer()->PlayerTalkClass->CloseGossip();
}

/**
 * @brief Sends quest details for a specific quest offered by a quest giver.
 *
 * @param recv_data The received opcode packet.
 */
void QuestHandlers::HandleQuestgiverQueryQuest(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint32 quest;
    uint8 unk1;
    recv_data >> guid >> quest >> unk1;

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_QUERY_QUEST - for %s to %s, quest = %u, unk1 = %u", session.GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str(), quest, unk1);

    // Verify that the guid is valid and is a questgiver or involved in the requested quest
    Object* pObject = session.GetPlayer()->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_GAMEOBJECT_OR_ITEM);
    if (!pObject || (!pObject->HasQuest(quest) && !pObject->HasInvolvedQuest(quest)))
    {
        session.GetPlayer()->PlayerTalkClass->CloseGossip();
        return;
    }

    if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest))
    {
        session.GetPlayer()->PlayerTalkClass->SendQuestGiverQuestDetails(pQuest, pObject->GetObjectGuid(), true);
    }
}

/**
 * @brief Sends static quest template data for a quest id.
 *
 * @param recv_data The received opcode packet.
 */
void QuestHandlers::HandleQuestQuery(WorldSession& session, WorldPacket& recv_data)
{
    uint32 quest;
    recv_data >> quest;
    DEBUG_LOG("WORLD: Received opcode CMSG_QUEST_QUERY quest = %u", quest);

    Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest);
    if (pQuest)
    {
        session.GetPlayer()->PlayerTalkClass->SendQuestQueryResponse(pQuest);
    }
}

/**
 * @brief Handles reward selection when turning in a quest.
 *
 * @param recv_data The received opcode packet.
 */
void QuestHandlers::HandleQuestgiverChooseReward(WorldSession& session, WorldPacket& recv_data)
{
    uint32 quest, reward;
    ObjectGuid guid;
    recv_data >> guid >> quest >> reward;

    if (reward >= QUEST_REWARD_CHOICES_COUNT)
    {
        sLog.outError("Error in CMSG_QUESTGIVER_CHOOSE_REWARD - %s tried to get invalid reward (%u) (probably packet hacking)", session.GetPlayer()->GetGuidStr().c_str(), reward);
        return;
    }

    if (!CanInteractWithQuestGiver(session, guid, "CMSG_QUESTGIVER_CHOOSE_REWARD"))
    {
        return;
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_CHOOSE_REWARD - for %s to %s, quest = %u, reward = %u", session.GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str(), quest, reward);

    Object* pObject = session.GetPlayer()->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);
    if (!pObject)
    {
        return;
    }

    if (!pObject->HasInvolvedQuest(quest))
    {
        return;
    }

    Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest);
    if (pQuest)
    {
        if (session.GetPlayer()->CanRewardQuest(pQuest, reward, true))
        {
            // Decided before the reward so the packet's UseQuestReward bit and the follow-up below agree:
            // the client keeps QuestFrame open only when the next quest's details really follow.
            Quest const* nextquest = session.GetPlayer()->GetNextQuest(guid, pQuest);
            session.GetPlayer()->RewardQuest(pQuest, reward, pObject, true, nextquest != NULL);

            // Send next quest
            if (nextquest)
            {
                session.GetPlayer()->PlayerTalkClass->SendQuestGiverQuestDetails(nextquest, guid, true);
            }
            else if (Creature* pCreature = pObject->ToCreature())
            {
                session.GetPlayer()->PrepareGossipMenu(pCreature, pCreature->GetCreatureInfo()->GossipMenuId);

                if (!session.GetPlayer()->PlayerTalkClass->Empty())
                {
                    session.GetPlayer()->SendPreparedGossip(pCreature);
                }
            }
            else
            {
                session.GetPlayer()->PrepareQuestMenu(guid);
                session.GetPlayer()->SendPreparedQuest(guid);
            }
        }
        else
        {
            session.GetPlayer()->PlayerTalkClass->SendQuestGiverOfferReward(pQuest, guid, true);
        }
    }
}

/**
 * @brief Requests the reward dialog for a completed quest.
 *
 * @param recv_data The received opcode packet.
 */
void QuestHandlers::HandleQuestgiverRequestReward(WorldSession& session, WorldPacket& recv_data)
{
    uint32 quest;
    ObjectGuid guid;
    recv_data >> guid >> quest;

    if (!CanInteractWithQuestGiver(session, guid, "CMSG_QUESTGIVER_REQUEST_REWARD"))
    {
        return;
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_REQUEST_REWARD - for %s to %s, quest = %u", session.GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str(), quest);

    Object* pObject = session.GetPlayer()->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);
    if (!pObject || !pObject->HasInvolvedQuest(quest))
    {
        return;
    }

    if (session.GetPlayer()->CanCompleteQuest(quest))
    {
        session.GetPlayer()->CompleteQuest(quest);
    }

    if (session.GetPlayer()->GetQuestStatusMgr().GetQuestStatus(quest) != QUEST_STATUS_COMPLETE)
    {
        return;
    }

    if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest))
    {
        session.GetPlayer()->PlayerTalkClass->SendQuestGiverOfferReward(pQuest, guid, true);
    }
}

/**
 * @brief Removes a quest from the player's quest log.
 *
 * @param recv_data The received opcode packet.
 */
void QuestHandlers::HandleQuestLogRemoveQuest(WorldSession& session, WorldPacket& recv_data)
{
    uint8 slot;
    recv_data >> slot;

    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTLOG_REMOVE_QUEST slot = %u", slot);

    if (slot < MAX_QUEST_LOG_SIZE)
    {
        if (uint32 quest = session.GetPlayer()->GetQuestSlotQuestId(slot))
        {
            if (!session.GetPlayer()->TakeQuestSourceItem(quest, true))
            {
                return;                                     // can't un-equip some items, reject quest cancel
            }

            if (const Quest* pQuest = sObjectMgr.GetQuestTemplate(quest))
            {
                if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
                {
                    session.GetPlayer()->GetQuestStatusMgr().RemoveTimedQuest(quest);
                }

                for (int i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
                {
                    if (pQuest->ReqSourceId[i])
                    {
                        ItemPrototype const* iProto = ObjectMgr::GetItemPrototype(pQuest->ReqSourceId[i]);
                        if (iProto && iProto->Bonding == BIND_QUEST_ITEM)
                        {
                            session.GetPlayer()->DestroyItemCount(pQuest->ReqSourceId[i], pQuest->ReqSourceCount[i], true, false, true);
                        }
                    }
                }
            }

            session.GetPlayer()->SetQuestStatus(quest, QUEST_STATUS_NONE);

            if (sGameConfig.getConfig(CONFIG_BOOL_ENABLE_QUEST_TRACKER)) // check if Quest Tracker is enabled
            {
                DEBUG_LOG("QUEST TRACKER: Quest Abandoned.");
                QuestTrackerRepository::SetAbandonTime(quest, session.GetPlayer()->GetGUIDLow());
            }
        }

        session.GetPlayer()->SetQuestSlot(slot, 0);

        session.GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_QUEST_ABANDONED, 1);
    }
}

/**
 * @brief Confirms acceptance of a quest shared by another party member.
 *
 * @param recv_data The received opcode packet.
 */
void QuestHandlers::HandleQuestConfirmAccept(WorldSession& session, WorldPacket& recv_data)
{
    uint32 quest;
    recv_data >> quest;

    DEBUG_LOG("WORLD: Received opcode CMSG_QUEST_CONFIRM_ACCEPT quest = %u", quest);

    if (const Quest* pQuest = sObjectMgr.GetQuestTemplate(quest))
    {
        if (!pQuest->HasQuestFlag(QUEST_FLAGS_PARTY_ACCEPT))
        {
            return;
        }

        Player* pOriginalPlayer = sPlayerRegistry.Find(session.GetPlayer()->GetDividerGuid());

        if (!pOriginalPlayer)
        {
            return;
        }

        if (pQuest->IsAllowedInRaid())
        {
            if (!session.GetPlayer()->IsInSameRaidWith(pOriginalPlayer))
            {
                return;
            }
        }
        else
        {
            if (!session.GetPlayer()->IsInSameGroupWith(pOriginalPlayer))
            {
                return;
            }
        }

        if (session.GetPlayer()->CanAddQuest(pQuest, true))
        {
            session.GetPlayer()->AddQuest(pQuest, NULL);                // NULL, this prevent DB script from duplicate running
        }

        session.GetPlayer()->ClearDividerGuid();
    }
}

/**
 * @brief Begins quest completion processing with a quest giver.
 *
 * @param recv_data The received opcode packet.
 */
void QuestHandlers::HandleQuestgiverCompleteQuest(WorldSession& session, WorldPacket& recv_data)
{
    uint32 quest;
    ObjectGuid guid;
    uint8 unk;
    recv_data >> guid >> quest >> unk;

    if (!CanInteractWithQuestGiver(session, guid, "CMSG_QUESTGIVER_COMPLETE_QUEST"))
    {
        return;
    }

    // All ok, continue
    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_COMPLETE_QUEST - for %s to %s, quest = %u, unk = %u", session.GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str(), quest, unk);

    if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest))
    {
        if (session.GetPlayer()->GetQuestStatusMgr().GetQuestStatus(quest) != QUEST_STATUS_COMPLETE)
        {
            if (pQuest->IsRepeatable())
            {
                session.GetPlayer()->PlayerTalkClass->SendQuestGiverRequestItems(pQuest, guid, session.GetPlayer()->CanCompleteRepeatableQuest(pQuest), false);
            }
            else
            {
                session.GetPlayer()->PlayerTalkClass->SendQuestGiverRequestItems(pQuest, guid, session.GetPlayer()->CanRewardQuest(pQuest, false), false);
            }
        }
        else
        {
            if (pQuest->GetReqItemsCount() || pQuest->GetReqCurrencyCount())    // some items or currency required
            {
                session.GetPlayer()->PlayerTalkClass->SendQuestGiverRequestItems(pQuest, guid, session.GetPlayer()->CanRewardQuest(pQuest, false), false);
            }
            else                                            // no items required
            {
                session.GetPlayer()->PlayerTalkClass->SendQuestGiverOfferReward(pQuest, guid, true);
            }
        }
    }
}

/**
 * @brief Shares a quest with nearby party members.
 *
 * @param recvPacket The received opcode packet.
 */
void QuestHandlers::HandlePushQuestToParty(WorldSession& session, WorldPacket& recvPacket)
{
    uint32 questId;
    recvPacket >> questId;

    DEBUG_LOG("WORLD: Received opcode CMSG_PUSHQUESTTOPARTY quest = %u", questId);

    if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(questId))
    {
        if (Group* pGroup = session.GetPlayer()->GetGroup())
        {
            for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* pPlayer = itr->getSource();

                if (!pPlayer || pPlayer == session.GetPlayer())         // skip self
                {
                    continue;
                }

                session.GetPlayer()->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_SHARING_QUEST);

                if (!pPlayer->SatisfyQuestStatus(pQuest, false))
                {
                    session.GetPlayer()->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_HAVE_QUEST);
                    continue;
                }

                if (pPlayer->GetQuestStatusMgr().GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
                {
                    session.GetPlayer()->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_FINISH_QUEST);
                    continue;
                }

                if (!pPlayer->CanTakeQuest(pQuest, false))
                {
                    session.GetPlayer()->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_CANT_TAKE_QUEST);
                    continue;
                }

                if (!pPlayer->SatisfyQuestLog(false))
                {
                    session.GetPlayer()->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_LOG_FULL);
                    continue;
                }

                if (pPlayer->GetDividerGuid())
                {
                    session.GetPlayer()->SendPushToPartyResponse(pPlayer, QUEST_PARTY_MSG_BUSY);
                    continue;
                }

                pPlayer->PlayerTalkClass->SendQuestGiverQuestDetails(pQuest, session.GetPlayer()->GetObjectGuid(), true);
                pPlayer->SetDividerGuid(session.GetPlayer()->GetObjectGuid());
            }
        }
    }
}

/**
 * @brief Relays a quest sharing result back to the original sharer.
 *
 * @param recvPacket The received opcode packet.
 */
void QuestHandlers::HandleQuestPushResult(WorldSession& session, WorldPacket& recvPacket)
{
    ObjectGuid guid;
    uint32 quest;
    uint8 msg;
    recvPacket >> guid >> quest >> msg;

    DEBUG_LOG("WORLD: Received opcode MSG_QUEST_PUSH_RESULT");

    if (Player* pPlayer = sPlayerRegistry.Find(session.GetPlayer()->GetDividerGuid()))
    {
        WorldPacket data(MSG_QUEST_PUSH_RESULT, (8 + 1));
        data << ObjectGuid(guid);
        data << uint8(msg);                             // valid values: 0-8
        pPlayer->GetSession()->SendPacket(&data);
        session.GetPlayer()->ClearDividerGuid();
    }
}

/**
 * @brief Queries quest status markers for all visible quest givers.
 *
 * @param recvPacket The received opcode packet.
 */
void QuestHandlers::HandleQuestgiverStatusMultipleQuery(WorldSession& session, WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY");

    session.GetPlayer()->SendQuestGiverStatusMultiple();
}

/**
 * @brief Checks whether the player can interact with the specified quest giver.
 *
 * @param guid The quest giver guid.
 * @param descr The opcode description used for logging.
 * @return true if interaction is allowed; otherwise false.
 */
bool QuestHandlers::CanInteractWithQuestGiver(WorldSession& session, ObjectGuid guid, char const* descr)
{
    if (guid.IsCreatureOrVehicle())
    {
        Creature* pCreature = session.GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_QUESTGIVER);
        if (!pCreature)
        {
            DEBUG_LOG("WORLD: %s - %s cannot interact with %s.", descr, session.GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str());
            return false;
        }
    }
    else if (guid.IsGameObject())
    {
        GameObject* pGo = session.GetPlayer()->GetGameObjectIfCanInteractWith(guid, GAMEOBJECT_TYPE_QUESTGIVER);
        if (!pGo)
        {
            DEBUG_LOG("WORLD: %s - %s cannot interact with %s.", descr, session.GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str());
            return false;
        }
    }
    else if (!session.GetPlayer()->IsAlive())
    {
        DEBUG_LOG("WORLD: %s - %s is dead, requested guid was %s", descr, session.GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str());
        return false;
    }

    return true;
}
