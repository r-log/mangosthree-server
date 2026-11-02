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

#ifndef MANGOS_H_NPCTEXTSTORE
#define MANGOS_H_NPCTEXTSTORE

#include "Platform/Define.h"
#include "data/GossipRecords.h"

#include <unordered_map>

/**
 * The npc texts (`npc_text`): the texts a gossip menu shows, one per id, each its options with their
 * language, probability and emotes. The loader fills a text through `FindOrAdd`, which answers the
 * id's text, an empty one when the id has none, so a reload overwrites the texts it reads and keeps
 * the others.
 */
class NpcTextStore
{
    public:
        GossipText const* Find(uint32 id) const;
        GossipText& FindOrAdd(uint32 id) { return m_texts[id]; }

    private:
        std::unordered_map<uint32, GossipText> m_texts;
};

#endif
