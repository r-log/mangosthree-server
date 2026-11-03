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

#ifndef MANGOS_H_QUESTACCEPT
#define MANGOS_H_QUESTACCEPT

class Object;
class Player;
class Quest;

/**
 * Puts an accepted quest into the player's quest log and starts what accepting it starts: the
 * offer to the party of a party-accept quest, completion when nothing is left to do and the timed
 * quest achievement criteria. The caller casts the quest's source spell.
 * @param player - who accepted the quest
 * @param quest - the quest accepted
 * @param questGiver - who or what gave it; an item giver can be destroyed by the call
 */
void StartAcceptedQuest(Player& player, Quest const* quest, Object* questGiver);

/**
 * Accepts an auto-accept quest for the player when its details are shown, and casts its source
 * spell. The client sends no accept for such a quest: its Accept button only closes the window.
 * @param player - whom the details are shown to
 * @param quest - the quest shown
 * @param questGiver - who or what offers it
 * @return true if the quest went into the quest log
 */
bool AutoAcceptQuest(Player& player, Quest const* quest, Object* questGiver);

#endif
