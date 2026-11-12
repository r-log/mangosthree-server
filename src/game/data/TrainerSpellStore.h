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

#ifndef MANGOS_H_TRAINERSPELLSTORE
#define MANGOS_H_TRAINERSPELLSTORE

#include "Platform/Define.h"
#include "data/CreatureRecords.h"

#include <unordered_map>

typedef std::unordered_map<uint32, TrainerSpellData> CacheTrainerSpellMap;

/**
 * The trainer spell lists: per trainer template id (`npc_trainer_template`) and per creature entry
 * (`npc_trainer`), the spells taught. `templates` picks the template lists or the creature lists. A
 * list is filled through `FindOrAdd`, which adds an empty one when the key has none.
 */
class TrainerSpellStore
{
    public:
        void Clear(bool templates);
        TrainerSpellData& FindOrAdd(bool templates, uint32 entry) { return List(templates)[entry]; }
        TrainerSpellData const* Find(bool templates, uint32 entry) const;
        CacheTrainerSpellMap const& Templates() const { return m_templates; }

        /// Sets the trainer type of each list on one side from its spells (`TrainerListType`).
        void DeriveTrainerTypes(bool templates);

    private:
        CacheTrainerSpellMap& List(bool templates) { return templates ? m_templates : m_trainers; }
        CacheTrainerSpellMap const& List(bool templates) const { return templates ? m_templates : m_trainers; }

        CacheTrainerSpellMap m_templates;
        CacheTrainerSpellMap m_trainers;
};

/// The trainer spell `spellId` from the creature's own list, else from the template list; null when
/// neither list holds it. Either list may be null.
TrainerSpell const* FindTrainerSpell(TrainerSpellData const* creatureSpells, TrainerSpellData const* templateSpells, uint32 spellId);

/// The trainer type the client gets with a list: TRAINER_TYPE_TRADESKILLS when a spell of the list
/// teaches a profession rank or needs a profession skill (Archaeology included), else TRAINER_TYPE_CLASS.
uint32 TrainerListType(TrainerSpellMap const& spells);

#endif
