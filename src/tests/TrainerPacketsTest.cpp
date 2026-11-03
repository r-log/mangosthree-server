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

/// A trainer list spell holds every field the 15595 client reads, in its order, and nothing after them.

#include "TestHarness.h"
#include "WorldPacket.h"
#include "data/CreatureRecords.h"
#include "session/packets/entities/TrainerPackets.h"

TEST(TrainerPackets_SpellHoldsEveryFieldTheClientReads)
{
    TrainerSpell spell(1079, 4400, 0, 0, 20, 1079, false);
    spell.reqAbility[0] = 768;
    spell.reqAbility[2] = 5487;

    WorldPacket packet;
    AppendTrainerSpell(packet, spell, 2, 4180, 20);

    uint32 spellId = 0, cost = 0, skill = 0, rank = 0, talentTree = 1;
    uint8 state = 0, level = 0;
    packet >> spellId >> state >> cost >> level >> skill >> rank;
    CHECK_EQ(spellId, 1079u);
    CHECK_EQ(state, uint8(2));
    CHECK_EQ(cost, 4180u);
    CHECK_EQ(level, uint8(20));
    CHECK_EQ(skill, 0u);
    CHECK_EQ(rank, 0u);
    uint32 ability[3] = { 1, 1, 1 };
    packet >> ability[0] >> ability[1] >> ability[2] >> talentTree;
    CHECK_EQ(ability[0], 768u);
    CHECK_EQ(ability[1], 0u);
    CHECK_EQ(ability[2], 5487u);
    CHECK_EQ(talentTree, 0u);
    CHECK_EQ(packet.rpos(), packet.size());
}
