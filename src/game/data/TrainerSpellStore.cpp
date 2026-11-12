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

#include "data/TrainerSpellStore.h"
#include "spells/data/SpellMgr.h"

namespace
{
    bool IsTradeskillLine(uint32 skill)
    {
        return IsProfessionSkill(skill) || skill == SKILL_ARCHAEOLOGY;
    }

    bool MakesTradeskillList(TrainerSpell const& spell)
    {
        return SpellMgr::IsProfessionSpell(spell.learnedSpell) || IsTradeskillLine(spell.reqSkill);
    }
}

void TrainerSpellStore::Clear(bool templates)
{
    CacheTrainerSpellMap& list = List(templates);
    for (CacheTrainerSpellMap::iterator itr = list.begin(); itr != list.end(); ++itr)
    {
        itr->second.Clear();
    }
    list.clear();
}

TrainerSpellData const* TrainerSpellStore::Find(bool templates, uint32 entry) const
{
    CacheTrainerSpellMap const& list = List(templates);
    CacheTrainerSpellMap::const_iterator itr = list.find(entry);
    return itr != list.end() ? &itr->second : NULL;
}

void TrainerSpellStore::DeriveTrainerTypes(bool templates)
{
    CacheTrainerSpellMap& list = List(templates);
    for (CacheTrainerSpellMap::iterator itr = list.begin(); itr != list.end(); ++itr)
    {
        itr->second.trainerType = TrainerListType(itr->second.spellList);
    }
}

TrainerSpell const* FindTrainerSpell(TrainerSpellData const* creatureSpells, TrainerSpellData const* templateSpells, uint32 spellId)
{
    TrainerSpell const* spell = creatureSpells ? creatureSpells->Find(spellId) : NULL;
    if (!spell && templateSpells)
    {
        spell = templateSpells->Find(spellId);
    }
    return spell;
}

uint32 TrainerListType(TrainerSpellMap const& spells)
{
    for (TrainerSpellMap::const_iterator itr = spells.begin(); itr != spells.end(); ++itr)
    {
        if (MakesTradeskillList(itr->second))
        {
            return TRAINER_TYPE_TRADESKILLS;
        }
    }
    return TRAINER_TYPE_CLASS;
}
