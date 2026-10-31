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

#include "session/packets/entities/GossipPackets.h"

#include "Utilities/Errors.h"
#include <string>
#include "QuestDef.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "Formulas.h"
#include "data/GameConfig.h"

void SessionGossipNotices::SendQuestGiverQuestList(PlayerSession& session, QuestMenu const& questMenu, QEmote eEmote, const std::string& Title, ObjectGuid npcGUID)
{
    WorldPacket data(SMSG_QUESTGIVER_QUEST_LIST, 100); // guess size
    data << ObjectGuid(npcGUID);
    data << Title;
    data << uint32(eEmote._Delay); // player emote
    data << uint32(eEmote._Emote); // NPC emote

    size_t count_pos = data.wpos();
    data << uint8(questMenu.MenuItemCount());
    uint32 count = 0;
    for (count = 0; count < questMenu.MenuItemCount(); ++count)
    {
        QuestMenuItem const& qmi = questMenu.GetItem(count);

        uint32 questID = qmi.m_qId;

        if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(questID))
        {
            int loc_idx = session.GetSessionDbLocaleIndex();
            std::string title = pQuest->GetTitle();
            sObjectMgr.GetQuestLocaleStrings(questID, loc_idx, &title);

            data << uint32(questID);
            data << uint32(qmi.m_qIcon);
            data << int32(pQuest->GetQuestLevel());
            data << uint32(pQuest->GetQuestFlags());        // 3.3.3 quest flags
            data << uint8(0);                               // 3.3.3 changes icon: blue question or yellow exclamation
            data << title;
        }
    }
    data.put<uint8>(count_pos, count);
    session.SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_LIST NPC Guid = %s", npcGUID.GetString().c_str());
}

void SessionGossipNotices::SendQuestGiverStatus(PlayerSession& session, uint32 questStatus, ObjectGuid npcGUID)
{
    WorldPacket data(SMSG_QUESTGIVER_STATUS, 12);
    data << npcGUID;
    data << uint32(questStatus);

    session.SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_STATUS for %s", npcGUID.GetString().c_str());
}

void SessionGossipNotices::SendQuestGiverQuestDetails(PlayerSession& session, Quest const* pQuest, ObjectGuid guid, bool ActivateAccept)
{
    // Retrieve the quest title, details, and objectives
    std::string Title      = pQuest->GetTitle();
    std::string Details    = pQuest->GetDetails();
    std::string Objectives = pQuest->GetObjectives();
    std::string PortraitGiverName = pQuest->GetPortraitGiverName();
    std::string PortraitGiverText = pQuest->GetPortraitGiverText();
    std::string PortraitTurnInName = pQuest->GetPortraitTurnInName();
    std::string PortraitTurnInText = pQuest->GetPortraitTurnInText();

    // Get the locale index for the session
    int loc_idx = session.GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        // Retrieve localized quest strings if available
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
            {
                Title = ql->Title[loc_idx];
            }
            if (ql->Details.size() > (size_t)loc_idx && !ql->Details[loc_idx].empty())
            {
                Details = ql->Details[loc_idx];
            }
            if (ql->Objectives.size() > (size_t)loc_idx && !ql->Objectives[loc_idx].empty())
            {
                Objectives = ql->Objectives[loc_idx];
            }
            if (ql->PortraitGiverName.size() > (size_t)loc_idx && !ql->PortraitGiverName[loc_idx].empty())
            {
                PortraitGiverName = ql->PortraitGiverName[loc_idx];
            }
            if (ql->PortraitGiverText.size() > (size_t)loc_idx && !ql->PortraitGiverText[loc_idx].empty())
            {
                PortraitGiverText = ql->PortraitGiverText[loc_idx];
            }
            if (ql->PortraitTurnInName.size() > (size_t)loc_idx && !ql->PortraitTurnInName[loc_idx].empty())
            {
                PortraitTurnInName = ql->PortraitTurnInName[loc_idx];
            }
            if (ql->PortraitTurnInText.size() > (size_t)loc_idx && !ql->PortraitTurnInText[loc_idx].empty())
            {
                PortraitTurnInText = ql->PortraitTurnInText[loc_idx];
            }
        }
    }

    // Prepare the packet to send quest details
    WorldPacket data(SMSG_QUESTGIVER_QUEST_DETAILS, 100);   // guess size
    data << guid;
    data << uint64(0);                                      // wotlk, something todo with quest sharing?
    data << uint32(pQuest->GetQuestId());
    data << Title;
    data << Details;
    data << Objectives;
    data << PortraitGiverText;
    data << PortraitGiverName;
    data << PortraitTurnInText;
    data << PortraitTurnInName;
    data << pQuest->GetPortraitGiver();
    data << pQuest->GetPortraitTurnIn();

    data << uint8(0); // this was used for auto quest accept, but it does not work

    data << pQuest->GetQuestFlags();                        // 3.3.3 questFlags
    data << pQuest->GetSuggestedPlayers();
    data << uint8(0);                                       // IsFinished? value is sent back to server in quest accept packet
    data << uint8(0);                                       // is areatrigger quest
    data << pQuest->GetReqSpellLearned();

    data << pQuest->GetRewChoiceItemsCount();

    for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
    {
        data << pQuest->RewChoiceItemId[i];
    }
    for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
    {
        data << pQuest->RewChoiceItemCount[i];
    }
    for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
        if (ItemPrototype const* IProto = ObjectMgr::GetItemPrototype(pQuest->RewChoiceItemId[i]))
        {
            data << IProto->DisplayInfoID;
        }
        else
        {
            data << uint32(0);
        }

    data << pQuest->GetRewItemsCount();

    for (uint32 i = 0; i < QUEST_REWARDS_COUNT; ++i)
    {
        data << pQuest->RewItemId[i];
    }
    for (uint32 i = 0; i < QUEST_REWARDS_COUNT; ++i)
    {
        data << pQuest->RewItemCount[i];
    }
    for (uint32 i = 0; i < QUEST_REWARDS_COUNT; ++i)
        if (ItemPrototype const* IProto = ObjectMgr::GetItemPrototype(pQuest->RewItemId[i]))
        {
            data << IProto->DisplayInfoID;
        }
        else
        {
            data << uint32(0);
        }

    // send rewMoneyMaxLevel explicit for max player level, else send RewOrReqMoney
    if (session.GetPlayer()->getLevel() >= sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        data << pQuest->GetRewMoneyMaxLevel();
    }
    else
    {
        data << pQuest->GetRewOrReqMoney();
    }

    data << pQuest->XPValue(session.GetPlayer()->getLevel());

    data << pQuest->GetCharTitleId();                       // CharTitleId, new 2.4.0, player gets this title (id from CharTitles)
    data << uint32(0);                                      // unk, unused 10 * GetRewHonorAddition ?
    data << float(0);                                       // unk, unused GetRewHonorMultiplier ?
    data << pQuest->GetBonusTalents();                      // bonus talents
    data << uint32(0);                                      // unk, unused bonus arena points?
    data << uint32(0);                                      // rep reward show mask?

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)       // reward factions ids
    {
        data << pQuest->RewRepFaction[i];
    }

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)       // columnid in QuestFactionReward.dbc (if negative, from second row)
    {
        data << pQuest->RewRepValueId[i];
    }

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)       // reward reputation override. No bonus is expected given
    {
        data << int32(0);
    }
    // data << int32(pQuest->RewRepValue[i]);            // current field for store of rep value, can be reused to implement "override value"

    data << pQuest->GetRewSpell();                  // reward spell, this spell will display (icon) (casted if RewSpellCast==0)
    data << pQuest->GetRewSpellCast();              // casted spell

    for (uint32 i = 0; i < QUEST_REWARD_CURRENCY_COUNT; ++i)
    {
        data << pQuest->RewCurrencyId[i];
    }

    for (uint32 i = 0; i < QUEST_REWARD_CURRENCY_COUNT; ++i)
    {
        data << pQuest->RewCurrencyCount[i];
    }

    data << pQuest->GetRewSkill();
    data << pQuest->GetRewSkillValue();

    data << uint32(QUEST_EMOTE_COUNT);
    for (uint32 i = 0; i < QUEST_EMOTE_COUNT; ++i)
    {
        data << pQuest->DetailsEmote[i];
        data << pQuest->DetailsEmoteDelay[i];       // DetailsEmoteDelay (in ms)
    }

    // Send the packet to the player
    session.SendPacket(&data);

    // Log the sent packet
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_DETAILS - for %s of %s, questid = %u", session.GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str(), pQuest->GetQuestId());
}

void SessionGossipNotices::SendQuestQueryResponse(PlayerSession& session, Quest const* pQuest)
{
    std::string Title, Details, Objectives, EndText, CompletedText;
    std::string PortraitGiverText, PortraitGiverName;
    std::string PortraitTurnInText, PortraitTurnInName;
    std::string ObjectiveText[QUEST_OBJECTIVES_COUNT];
    Title = pQuest->GetTitle();
    Details = pQuest->GetDetails();
    Objectives = pQuest->GetObjectives();
    EndText = pQuest->GetEndText();
    CompletedText = pQuest->GetCompletedText();
    PortraitGiverName = pQuest->GetPortraitGiverName();
    PortraitGiverText = pQuest->GetPortraitGiverText();
    PortraitTurnInName = pQuest->GetPortraitTurnInName();
    PortraitTurnInText = pQuest->GetPortraitTurnInText();

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        ObjectiveText[i] = pQuest->ObjectiveText[i];
    }

    // Get the locale index for the session
    int loc_idx = session.GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        // Retrieve localized quest strings if available
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
            {
                Title = ql->Title[loc_idx];
            }
            if (ql->Details.size() > (size_t)loc_idx && !ql->Details[loc_idx].empty())
            {
                Details = ql->Details[loc_idx];
            }
            if (ql->Objectives.size() > (size_t)loc_idx && !ql->Objectives[loc_idx].empty())
            {
                Objectives = ql->Objectives[loc_idx];
            }
            if (ql->EndText.size() > (size_t)loc_idx && !ql->EndText[loc_idx].empty())
            {
                EndText = ql->EndText[loc_idx];
            }
            if (ql->CompletedText.size() > (size_t)loc_idx && !ql->CompletedText[loc_idx].empty())
            {
                CompletedText = ql->CompletedText[loc_idx];
            }
            if (ql->PortraitGiverName.size() > (size_t)loc_idx && !ql->PortraitGiverName[loc_idx].empty())
            {
                PortraitGiverName = ql->PortraitGiverName[loc_idx];
            }
            if (ql->PortraitGiverText.size() > (size_t)loc_idx && !ql->PortraitGiverText[loc_idx].empty())
            {
                PortraitGiverText = ql->PortraitGiverText[loc_idx];
            }
            if (ql->PortraitTurnInName.size() > (size_t)loc_idx && !ql->PortraitTurnInName[loc_idx].empty())
            {
                PortraitTurnInName = ql->PortraitTurnInName[loc_idx];
            }
            if (ql->PortraitTurnInText.size() > (size_t)loc_idx && !ql->PortraitTurnInText[loc_idx].empty())
            {
                PortraitTurnInText = ql->PortraitTurnInText[loc_idx];
            }

            for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
            {
                if (ql->ObjectiveText[i].size() > (size_t)loc_idx && !ql->ObjectiveText[i][loc_idx].empty())
                {
                    ObjectiveText[i] = ql->ObjectiveText[i][loc_idx];
                }
            }
        }
    }

    // Prepare the packet to send quest query response
    WorldPacket data(SMSG_QUEST_QUERY_RESPONSE, 100);       // guess size

    data << uint32(pQuest->GetQuestId());                   // quest id
    data << uint32(pQuest->GetQuestMethod());               // Accepted values: 0, 1 or 2. 0==IsAutoComplete() (skip objectives/details)
    data << int32(pQuest->GetQuestLevel());                 // may be -1, static data, in other cases must be used dynamic level: Player::GetQuestLevelForPlayer (0 is not known, but assuming this is no longer valid for quest intended for client)
    data << uint32(pQuest->GetMinLevel());                  // min required level to obtain (added for 3.3). Assumed allowed (database) range is -1 to 255 (still using uint32, since negative value would not be of any known use for client)
    data << uint32(pQuest->GetZoneOrSort());                // zone or sort to display in quest log
    data << uint32(pQuest->GetType());                      // quest type
    data << uint32(pQuest->GetSuggestedPlayers());          // suggested players count

    data << uint32(pQuest->GetRepObjectiveFaction());       // shown in quest log as part of quest objective
    data << uint32(pQuest->GetRepObjectiveValue());         // shown in quest log as part of quest objective

    data << uint32(0);                                      // RequiredOppositeRepFaction
    data << uint32(0);                                      // RequiredOppositeRepValue, required faction value with another (oposite) faction (objective)

    data << uint32(pQuest->GetNextQuestInChain());          // client will request this quest from NPC, if not 0
    data << uint32(pQuest->GetRewXPId());                   // column index in QuestXP.dbc (row based on quest level)

    // unused 4.x.x ?
    /*if (pQuest->HasQuestFlag(QUEST_FLAGS_HIDDEN_REWARDS))
        data << uint32(0);                                  // Hide money rewarded
    else*/
    {
        data << uint32(pQuest->GetRewOrReqMoney());         // reward money (below max lvl)
    }

    data << uint32(pQuest->GetRewMoneyMaxLevel());          // used in XP calculation at client
    data << uint32(pQuest->GetRewSpell());                  // reward spell, this spell will display (icon) (casted if RewSpellCast==0)
    data << uint32(pQuest->GetRewSpellCast());              // casted spell

    // rewarded honor points
    data << uint32(pQuest->GetRewHonorAddition());          // unused 4.x.x ?
    data << float(pQuest->GetRewHonorMultiplier());         // unused 4.x.x ? new reward honor (multiplied by ~62 at client side)

    data << uint32(pQuest->GetSrcItemId());                 // source item id
    data << uint32(pQuest->GetQuestFlags());                // quest flags

    data << uint32(0);                                      // MinimapTargetMark

    data << uint32(pQuest->GetCharTitleId());               // CharTitleId, new 2.4.0, player gets this title (id from CharTitles)
    data << uint32(pQuest->GetPlayersSlain());              // players slain
    data << uint32(pQuest->GetBonusTalents());              // bonus talents

    data << uint32(0);                                      // unused 4.x.x ? bonus arena points
    data << uint32(pQuest->GetRewSkill());                  // Rewarded skill id
    data << uint32(pQuest->GetRewSkillValue());             // Rewarded skill bonus points
    data << uint32(0);                                      // rew rep show mask?
    data << uint32(pQuest->GetPortraitGiver());
    data << uint32(pQuest->GetPortraitTurnIn());

    // Add reward items
    int iI;

    // unused 4.?.?
    /*if (pQuest->HasQuestFlag(QUEST_FLAGS_HIDDEN_REWARDS))
    {
        for (iI = 0; iI < QUEST_REWARDS_COUNT; ++iI)
        {
            data << uint32(0) << uint32(0);
        }
        for (iI = 0; iI < QUEST_REWARD_CHOICES_COUNT; ++iI)
        {
            data << uint32(0) << uint32(0);
        }
    }
    else*/
    {
        for (iI = 0; iI < QUEST_REWARDS_COUNT; ++iI)
        {
            data << uint32(pQuest->RewItemId[iI]);
            data << uint32(pQuest->RewItemCount[iI]);
        }
        for (iI = 0; iI < QUEST_REWARD_CHOICES_COUNT; ++iI)
        {
            data << uint32(pQuest->RewChoiceItemId[iI]);
            data << uint32(pQuest->RewChoiceItemCount[iI]);
        }
    }

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)       // reward factions ids
    {
        data << uint32(pQuest->RewRepFaction[i]);
    }

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)       // columnid in QuestFactionReward.dbc (if negative, from second row)
    {
        data << int32(pQuest->RewRepValueId[i]);
    }

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)       // reward reputation override. No bonus is expected given
    {
        data << int32(0);
    }
    // data << int32(pQuest->RewRepValue[i]);            // current field for store of rep value, can be reused to implement "override value"

    // Add quest point information
    data << pQuest->GetPointMapId();
    data << pQuest->GetPointX();
    data << pQuest->GetPointY();
    data << pQuest->GetPointOpt();

    // Add quest texts
    data << Title;
    data << Objectives;
    data << Details;
    data << EndText;
    data << CompletedText;                                  // display in quest objectives window once all objectives are completed

    // Add quest objectives
    for (iI = 0; iI < QUEST_OBJECTIVES_COUNT; ++iI)
    {
        if (pQuest->ReqCreatureOrGOId[iI] < 0)
        {
            // client expected gameobject template id in form (id|0x80000000)
            data << uint32((pQuest->ReqCreatureOrGOId[iI] * (-1)) | 0x80000000);
        }
        else
        {
            data << uint32(pQuest->ReqCreatureOrGOId[iI]);
        }
        data << uint32(pQuest->ReqCreatureOrGOCount[iI]);
        data << uint32(pQuest->ReqSourceId[iI]);
        data << uint32(pQuest->ReqSourceCount[iI]);
    }

    for (iI = 0; iI < QUEST_ITEM_OBJECTIVES_COUNT; ++iI)
    {
        data << uint32(pQuest->ReqItemId[iI]);
        data << uint32(pQuest->ReqItemCount[iI]);
    }

    data << uint32(pQuest->GetReqSpellLearned());

    for (iI = 0; iI < QUEST_OBJECTIVES_COUNT; ++iI)
    {
        data << ObjectiveText[iI];
    }

    for(iI = 0; iI < QUEST_REWARD_CURRENCY_COUNT; ++iI)
    {
        data << uint32(pQuest->RewCurrencyId[iI]);
        data << uint32(pQuest->RewCurrencyCount[iI]);
    }

    for(iI = 0; iI < QUEST_REQUIRED_CURRENCY_COUNT; ++iI)
    {
        data << uint32(pQuest->ReqCurrencyId[iI]);
        data << uint32(pQuest->ReqCurrencyCount[iI]);
    }

    data << PortraitGiverText;
    data << PortraitGiverName;
    data << PortraitTurnInText;
    data << PortraitTurnInName;

    data << uint32(pQuest->GetSoundAcceptId());
    data << uint32(pQuest->GetSoundTurnInId());

    session.SendPacket(&data);

    // Log the sent packet
    DEBUG_LOG("WORLD: Sent SMSG_QUEST_QUERY_RESPONSE questid=%u", pQuest->GetQuestId());
}

void SessionGossipNotices::SendQuestGiverOfferReward(PlayerSession& session, Quest const* pQuest, ObjectGuid npcGUID, bool EnableNext)
{
    // Retrieve the quest title and offer reward text
    std::string Title = pQuest->GetTitle();
    std::string OfferRewardText = pQuest->GetOfferRewardText();
    std::string PortraitGiverName = pQuest->GetPortraitGiverName();
    std::string PortraitGiverText = pQuest->GetPortraitGiverText();
    std::string PortraitTurnInName = pQuest->GetPortraitTurnInName();
    std::string PortraitTurnInText = pQuest->GetPortraitTurnInText();

    // Get the locale index for the session
    int loc_idx = session.GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        // Retrieve localized quest strings if available
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
            {
                Title = ql->Title[loc_idx];
            }
            if (ql->OfferRewardText.size() > (size_t)loc_idx && !ql->OfferRewardText[loc_idx].empty())
            {
                OfferRewardText = ql->OfferRewardText[loc_idx];
            }
            if (ql->PortraitGiverName.size() > (size_t)loc_idx && !ql->PortraitGiverName[loc_idx].empty())
            {
                PortraitGiverName = ql->PortraitGiverName[loc_idx];
            }
            if (ql->PortraitGiverText.size() > (size_t)loc_idx && !ql->PortraitGiverText[loc_idx].empty())
            {
                PortraitGiverText = ql->PortraitGiverText[loc_idx];
            }
            if (ql->PortraitTurnInName.size() > (size_t)loc_idx && !ql->PortraitTurnInName[loc_idx].empty())
            {
                PortraitTurnInName = ql->PortraitTurnInName[loc_idx];
            }
            if (ql->PortraitTurnInText.size() > (size_t)loc_idx && !ql->PortraitTurnInText[loc_idx].empty())
            {
                PortraitTurnInText = ql->PortraitTurnInText[loc_idx];
            }
        }
    }

    // Prepare the packet to send quest offer reward
    WorldPacket data(SMSG_QUESTGIVER_OFFER_REWARD, 50);     // guess size

    // Add NPC GUID, quest ID, title, and offer reward text to the packet
    data << ObjectGuid(npcGUID);
    data << uint32(pQuest->GetQuestId());
    data << Title;
    data << OfferRewardText;

    data << PortraitGiverText;
    data << PortraitGiverName;
    data << PortraitTurnInText;
    data << PortraitTurnInName;
    data << uint32(pQuest->GetPortraitGiver());
    data << uint32(pQuest->GetPortraitTurnIn());

    data << uint8(EnableNext ? 1 : 0);                      // Auto Finish
    data << uint32(pQuest->GetQuestFlags());                // 3.3.3 questFlags
    data << uint32(pQuest->GetSuggestedPlayers());          // SuggestedGroupNum

    // Add quest emotes to the packet
    uint32 EmoteCount = 0;
    for (uint32 i = 0; i < QUEST_EMOTE_COUNT; ++i)
    {
        if (pQuest->OfferRewardEmote[i] <= 0)
        {
            break;
        }
        ++EmoteCount;
    }

    data << EmoteCount;                                     // Emote Count
    for (uint32 i = 0; i < EmoteCount; ++i)
    {
        data << uint32(pQuest->OfferRewardEmoteDelay[i]);   // Delay Emote
        data << uint32(pQuest->OfferRewardEmote[i]);
    }

    data << uint32(pQuest->GetRewChoiceItemsCount());
    for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
    {
        data << uint32(pQuest->RewChoiceItemId[i]);
    }
    for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
    {
        data << uint32(pQuest->RewChoiceItemCount[i]);
    }
    for (uint32 i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
        if (ItemPrototype const* pItem = ObjectMgr::GetItemPrototype(pQuest->RewChoiceItemId[i]))
        {
            data << uint32(pItem->DisplayInfoID);
        }
        else
        {
            data << uint32(0);
        }

    // Add reward items to the packet
    data << uint32(pQuest->GetRewItemsCount());
    for (uint32 i = 0; i < QUEST_REWARDS_COUNT; ++i)
    {
        data << uint32(pQuest->RewItemId[i]);
    }
    for (uint32 i = 0; i < QUEST_REWARDS_COUNT; ++i)
    {
        data << uint32(pQuest->RewItemCount[i]);
    }
    for (uint32 i = 0; i < QUEST_REWARDS_COUNT; ++i)
        if (ItemPrototype const* pItem = ObjectMgr::GetItemPrototype(pQuest->RewItemId[i]))
        {
            data << uint32(pItem->DisplayInfoID);
        }
        else
        {
            data << uint32(0);
        }

    // send rewMoneyMaxLevel explicit for max player level, else send RewOrReqMoney
    if (session.GetPlayer()->getLevel() >= sGameConfig.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        data << uint32(pQuest->GetRewMoneyMaxLevel());
    }
    else
    {
        data << uint32(pQuest->GetRewOrReqMoney());
    }

    // xp
    data << uint32(pQuest->XPValue(session.GetPlayer()->getLevel()));

    data << uint32(pQuest->GetCharTitleId());   // character title
    data << uint32(0);                          // unk, unused 10 * GetRewHonorAddition ?
    data << float(0);                           // unk, unused GetRewHonorMultiplier ?
    data << uint32(pQuest->GetBonusTalents());  // bonus talents
    data << uint32(0);                          // unk, unused bonus arena points?
    data << uint32(0);                          // rep reward show mask?

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)       // reward factions ids
    {
        data << uint32(pQuest->RewRepFaction[i]);
    }

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)       // columnid in QuestFactionReward.dbc (if negative, from second row)
    {
        data << int32(pQuest->RewRepValueId[i]);
    }

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)       // reward reputation override. No diplomacy bonus is expected given, reward also does not display in chat window
    {
        data << int32(0);
    }
    // data << int32(pQuest->RewRepValue[i]);

    data << uint32(pQuest->GetRewSpell());                  // reward spell, this spell will display (icon) (casted if RewSpellCast==0)
    data << uint32(pQuest->GetRewSpellCast());              // casted spell

    for (uint32 i = 0; i < QUEST_REWARD_CURRENCY_COUNT; ++i)
    {
        data << uint32(pQuest->RewCurrencyId[i]);
    }

    for (uint32 i = 0; i < QUEST_REWARD_CURRENCY_COUNT; ++i)
    {
        data << uint32(pQuest->RewCurrencyCount[i]);
    }

    data << uint32(pQuest->GetRewSkill());
    data << uint32(pQuest->GetRewSkillValue());

    session.SendPacket(&data);

    // Log the sent packet
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_OFFER_REWARD NPCGuid = %s, questid = %u", npcGUID.GetString().c_str(), pQuest->GetQuestId());
}

void SessionGossipNotices::SendQuestGiverRequestItems(PlayerSession& session, Quest const* pQuest, ObjectGuid npcGUID, bool Completable, bool CloseOnCancel)
{
    // We can always call to RequestItems, but this packet only goes out if there are actually
    // items.  Otherwise, we'll skip straight to the OfferReward

    if (!pQuest->GetReqItemsCount() && !pQuest->GetReqCurrencyCount() && Completable)
    {
        SendQuestGiverOfferReward(session, pQuest, npcGUID, true);
        return;
    }

    std::string Title = pQuest->GetTitle();
    std::string RequestItemsText = pQuest->GetRequestItemsText();

    // Get the locale index for the session
    int loc_idx = session.GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        // Retrieve localized quest strings if available
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
            {
                Title = ql->Title[loc_idx];
            }
            if (ql->RequestItemsText.size() > (size_t)loc_idx && !ql->RequestItemsText[loc_idx].empty())
            {
                RequestItemsText = ql->RequestItemsText[loc_idx];
            }
        }
    }

    WorldPacket data(SMSG_QUESTGIVER_REQUEST_ITEMS, 50);    // guess size
    data << ObjectGuid(npcGUID);                            // NPC GUID
    data << uint32(pQuest->GetQuestId());                   // Quest ID
    data << Title;                                          // Quest title
    data << RequestItemsText;                               // Request items text

    data << uint32(0x00);                                   // emote delay

    // Add the appropriate emote based on whether the quest is completable
    if (Completable)
    {
        data << pQuest->GetCompleteEmote();                 // emote id
    }
    else
    {
        data << pQuest->GetIncompleteEmote();
    }

    // Add the close on cancel flag
    if (CloseOnCancel)
    {
        data << uint32(0x01);                               // auto finish
    }
    else
    {
        data << uint32(0x00);
    }

    data << uint32(pQuest->GetQuestFlags());                // 3.3.3 questFlags
    data << uint32(pQuest->GetSuggestedPlayers());          // SuggestedGroupNum

    // Add the required money
    data << uint32(pQuest->GetRewOrReqMoney() < 0 ? -pQuest->GetRewOrReqMoney() : 0);

    // Add the required items
    data << uint32(pQuest->GetReqItemsCount());
    ItemPrototype const* pItem;
    for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
    {
        if (!pQuest->ReqItemId[i])
        {
            continue;
        }
        pItem = ObjectMgr::GetItemPrototype(pQuest->ReqItemId[i]);
        data << uint32(pQuest->ReqItemId[i]);
        data << uint32(pQuest->ReqItemCount[i]);

        if (pItem)
        {
            data << uint32(pItem->DisplayInfoID);
        }
        else
        {
            data << uint32(0);
        }
    }

    data << uint32(pQuest->GetReqCurrencyCount());
    for (int i = 0; i < QUEST_REQUIRED_CURRENCY_COUNT; ++i)
    {
        if (!pQuest->ReqCurrencyId[i])
        {
            continue;
        }

        data << uint32(pQuest->ReqCurrencyId[i]);
        data << uint32(pQuest->ReqCurrencyCount[i]);
    }

    if (!Completable)                                       // Completable = flags1 && flags2 && flags3 && flags4
    {
        data << uint32(0x00);                               // flags1
    }
    else
    {
        data << uint32(0x03);
    }

    data << uint32(0x04);                                   // flags2
    data << uint32(0x08);                                   // flags3
    data << uint32(0x10);                                   // flags4
    data << uint32(0x40);                                   // flags5

    // Send the packet to the player
    session.SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_REQUEST_ITEMS NPCGuid = %s, questid = %u", npcGUID.GetString().c_str(), pQuest->GetQuestId());
}
