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

#include "QuestGiverStatus.h"

#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "Utilities/Errors.h"
#include "data/GameConfig.h"

uint32 QuestGiverDialogStatus(Player* pPlayer, Object* questgiver, uint32 defstatus)
{
    MANGOS_ASSERT(defstatus != DIALOG_STATUS_UNDEFINED);

    uint32 dialogStatus = defstatus;

    QuestRelationsMapBounds rbounds;                        // QuestRelations (quest-giver)
    QuestRelationsMapBounds irbounds;                       // InvolvedRelations (quest-finisher)

    switch (questgiver->GetTypeId())
    {
        case TYPEID_UNIT:
        {
            rbounds = sObjectMgr.GetCreatureQuestRelationsMapBounds(questgiver->GetEntry());
            irbounds = sObjectMgr.GetCreatureQuestInvolvedRelationsMapBounds(questgiver->GetEntry());
            break;
        }
        case TYPEID_GAMEOBJECT:
        {
            rbounds = sObjectMgr.GetGOQuestRelationsMapBounds(questgiver->GetEntry());
            irbounds = sObjectMgr.GetGOQuestInvolvedRelationsMapBounds(questgiver->GetEntry());
            break;
        }
        default:
            // it's impossible, but check ^)
            sLog.outError("Warning: GetDialogStatus called for unexpected type %u", questgiver->GetTypeId());
            return DIALOG_STATUS_NONE;
    }

    // Check markings for quest-finisher
    for (QuestRelationsMap::const_iterator itr = irbounds.first; itr != irbounds.second; ++itr)
    {
        uint32 dialogStatusNew = DIALOG_STATUS_NONE;
        uint32 quest_id = itr->second;
        Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);

        if (!pQuest || !pQuest->IsActive())
        {
            continue;
        }

        QuestStatus status = pPlayer->GetQuestStatusMgr().GetQuestStatus(quest_id);

        if (status == QUEST_STATUS_COMPLETE && !pPlayer->GetQuestRewardStatus(quest_id))
        {
            dialogStatusNew = pQuest->IsRepeatable() ? DIALOG_STATUS_REWARD_REP : DIALOG_STATUS_REWARD;
        }
        else if (pQuest->IsAutoComplete() && pPlayer->CanTakeQuest(pQuest, false))
        {
            dialogStatusNew = pQuest->IsRepeatable() ? DIALOG_STATUS_AVAILABLE_REP : DIALOG_STATUS_AVAILABLE;
        }
        else if (status == QUEST_STATUS_INCOMPLETE)
        {
            dialogStatusNew = DIALOG_STATUS_INCOMPLETE;
        }

        if (dialogStatusNew > dialogStatus)
        {
            dialogStatus = dialogStatusNew;
        }
    }

    // check markings for quest-giver
    for (QuestRelationsMap::const_iterator itr = rbounds.first; itr != rbounds.second; ++itr)
    {
        uint32 dialogStatusNew = DIALOG_STATUS_NONE;
        uint32 quest_id = itr->second;
        Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);

        if (!pQuest || !pQuest->IsActive())
        {
            continue;
        }

        QuestStatus status = pPlayer->GetQuestStatusMgr().GetQuestStatus(quest_id);

        if (status == QUEST_STATUS_NONE)                    // For all other cases the mark is handled either at some place else, or with involved-relations already
        {
            if (pPlayer->CanSeeStartQuest(pQuest))
            {
                if (pPlayer->SatisfyQuestLevel(pQuest, false))
                {
                    int32 lowLevelDiff = sGameConfig.getConfig(CONFIG_INT32_QUEST_LOW_LEVEL_HIDE_DIFF);
                    if (pQuest->IsAutoComplete() || (pQuest->IsRepeatable() && pPlayer->GetQuestStatusMgr().Map()[quest_id].m_rewarded))
                    {
                        dialogStatusNew = DIALOG_STATUS_REWARD_REP;
                    }
                    else if (lowLevelDiff < 0 || pPlayer->getLevel() <= pPlayer->GetQuestLevelForPlayer(pQuest) + uint32(lowLevelDiff))
                    {
                        if (pQuest->HasQuestFlag(QUEST_FLAGS_DAILY) || pQuest->HasQuestFlag(QUEST_FLAGS_WEEKLY))
                        {
                            dialogStatusNew = DIALOG_STATUS_AVAILABLE_REP;
                        }
                        else
                        {
                            dialogStatusNew = DIALOG_STATUS_AVAILABLE;
                        }
                    }
                    else                                    // player level much higher then quest-level
                    {
                        dialogStatusNew = DIALOG_STATUS_LOW_LEVEL_AVAILABLE;
                    }
                }
                else
                {
                    dialogStatusNew = DIALOG_STATUS_UNAVAILABLE;
                }
            }
        }

        if (dialogStatusNew > dialogStatus)
        {
            dialogStatus = dialogStatusNew;
        }
    }

    return dialogStatus;
}
