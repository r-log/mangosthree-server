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

#ifndef MANGOS_H_QUESTREWARDRULES
#define MANGOS_H_QUESTREWARDRULES

#include "Platform/Define.h"

class Quest;

/**
 * @file QuestRewardRules.h
 * @brief Decoupling D4f: the arithmetic and the template reads of a quest reward, as pure functions.
 *
 * The reward check and the reward itself stay with the owner: they are orchestration over update
 * fields, packets, spells and several managers. What they computed inline and what has no manager
 * to live in -- the quest-XP rule, the money a character at the maximum level receives, whether the
 * required money can be paid, and which item a reward choice resolves to -- is here, over plain
 * values (and the quest template, read only), so `mangos_tests` can pin each rule over a table
 * (src/tests/QuestRewardTest.cpp). Each body is the old expression with the reads it made from
 * the character, the world config and the template turned into parameters; the owner computes
 * them with the old expressions at the old statement, so every value is the one it was.
 */

/// The item a reward choice resolves to. `itemId` 0: nothing is chosen (the quest offers no choice,
/// or the chosen slot is empty); `count` is then 0.
struct QuestRewardItem
{
    uint32 itemId;
    uint32 count;
};

namespace QuestRewardRules
{
    /// The chosen reward item: `RewChoiceItemId[reward]` and `RewChoiceItemCount[reward]` when the
    /// quest offers a choice at all (`GetRewChoiceItemsCount() > 0`), else nothing. The slots are
    /// read only then, as before; `reward` is bounded by the caller (the handler refuses an index
    /// of QUEST_REWARD_CHOICES_COUNT or more, the auto-reward passes 0).
    QuestRewardItem ChosenItem(Quest const* quest, uint32 reward);

    /// The quest XP the reward gives below the maximum level: the template's XP value for the
    /// character's level scaled by the quest-XP rate, truncated (`uint32(xpValue * rate)`).
    uint32 Xp(uint32 xpValue, float rate);

    /// The money a character at the maximum level receives instead of the XP:
    /// `uint32(rewMoneyMaxLevel * moneyRate)`, replaced by the template's reward money when that is
    /// larger (compared as `int32`).
    uint32 MaxLevelMoney(uint32 rewMoneyMaxLevel, int32 rewOrReqMoney, float moneyRate);

    /// False when the quest requires money (`rewOrReqMoney < 0`) and `money` is less than that
    /// amount; true otherwise.
    bool CanPayRequiredMoney(int32 rewOrReqMoney, uint64 money);
}

#endif
