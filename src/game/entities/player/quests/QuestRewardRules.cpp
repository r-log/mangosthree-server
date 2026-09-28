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

#include "QuestRewardRules.h"
#include "QuestDef.h"

// Every body below moved here in decoupling D4f from the owner's quest reward check and quest
// reward. The expressions are the old ones; what changed is where the inputs come from: the
// character's level-scaled XP value, the world's quest-XP and money rates, the template's money
// columns and the character's money are parameters, computed by the owner at the old statement.

QuestRewardItem QuestRewardRules::ChosenItem(Quest const* quest, uint32 reward)
{
    QuestRewardItem chosen = { 0, 0 };

    if (quest->GetRewChoiceItemsCount() > 0)
    {
        if (uint32 itemId = quest->RewChoiceItemId[reward])
        {
            chosen.itemId = itemId;
            chosen.count = quest->RewChoiceItemCount[reward];
        }
    }

    return chosen;
}

uint32 QuestRewardRules::Xp(uint32 xpValue, float rate)
{
    return uint32(xpValue * rate);
}

uint32 QuestRewardRules::MaxLevelMoney(uint32 rewMoneyMaxLevel, int32 rewOrReqMoney, float moneyRate)
{
    // reward money for max level already included in rewMoneyMaxLevel
    uint32 money = uint32(rewMoneyMaxLevel * moneyRate);

    // reward money used if > xp replacement money
    if (rewOrReqMoney > int32(money))
    {
        money = rewOrReqMoney;
    }

    return money;
}

bool QuestRewardRules::CanPayRequiredMoney(int32 rewOrReqMoney, uint64 money)
{
    // prevent receive reward with low money and GetRewOrReqMoney() < 0
    if (rewOrReqMoney < 0 && money < uint64(-rewOrReqMoney))
    {
        return false;
    }

    return true;
}
