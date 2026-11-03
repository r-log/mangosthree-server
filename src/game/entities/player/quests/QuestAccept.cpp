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

#include "QuestAccept.h"

#include "AchievementMgr.h"
#include "GossipDef.h"
#include "Group.h"
#include "Player.h"
#include "QuestDef.h"

void StartAcceptedQuest(Player& player, Quest const* quest, Object* questGiver)
{
    uint32 const questId = quest->GetQuestId();

    player.AddQuest(quest, questGiver);

    if (quest->HasQuestFlag(QUEST_FLAGS_PARTY_ACCEPT))
    {
        if (Group* group = player.GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* member = itr->getSource();

                if (!member || member == &player)
                {
                    continue;
                }

                if (member->CanTakeQuest(quest, true))
                {
                    member->SetDividerGuid(player.GetObjectGuid());

                    // the member's open gossip window closes before the confirmation shows
                    member->PlayerTalkClass->CloseGossip();

                    player.SendQuestConfirmAccept(quest, member);
                }
            }
        }
    }

    if (player.CanCompleteQuest(questId))
    {
        player.CompleteQuest(questId);
    }

    player.GetAchievementMgr().StartTimedAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST, questId);
}

bool AutoAcceptQuest(Player& player, Quest const* quest, Object* questGiver)
{
    if (!quest->IsAutoAccept() || !player.CanTakeQuest(quest, false) || !player.CanAddQuest(quest, true))
    {
        return false;
    }

    StartAcceptedQuest(player, quest, questGiver);

    if (quest->GetSrcSpell() > 0)
    {
        player.CastSpell(&player, quest->GetSrcSpell(), true);
    }

    return true;
}
