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

#ifndef MANGOS_H_TALENTPACKETS
#define MANGOS_H_TALENTPACKETS

class Player;
class WorldPacket;

/// Writes `player`'s talent groups: its free points, its group count and active group, then each
/// group's primary tree, talents and glyphs.
void WriteTalentsInfo(WorldPacket& packet, Player const& player);
/// Writes the talents of `player`'s pet: its free points, then each learned talent and its rank.
void WritePetTalentsInfo(WorldPacket& packet, Player const& player);
/// Writes `player`'s equipped items for an inspection: the slots used, then each item's entry, its
/// enchantments, its random property, its creator and its suffix factor.
void WriteEnchantmentsInfo(WorldPacket& packet, Player const& player);

#endif
