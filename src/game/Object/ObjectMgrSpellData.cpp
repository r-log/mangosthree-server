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

/**
 * @file ObjectMgrSpellData.cpp
 * @brief ObjectMgr's creature spell-click, creature template spell and fishing skill loaders.
 */

#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"
#include "SQLStorages.h"
#include "DBCStores.h"
#include "persistence/world/creatures/CreatureTemplateSpellsRepository.h"
#include "persistence/world/economy/SkillDataRepository.h"
#include "persistence/world/npcs/NpcDataRepository.h"

void ObjectMgr::LoadNPCSpellClickSpells()
{
    uint32 count = 0;

    mSpellClickInfoMap.clear();
    std::vector<SpellClickSpellRow> rows = NpcDataRepository::QuerySpellClickSpells();

    if (rows.empty())
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 spellclick spells. DB table `npc_spellclick_spells` is empty.");
        return;
    }

    BarGoLink bar(rows.size());

    for (SpellClickSpellRow const& row : rows)
    {
        bar.step();

        SpellClickInfo info;
        uint32 npc_entry         = row.npcEntry;
        info.spellId             = row.spellId;
        info.questStart          = row.questStart;
        info.questStartCanActive = row.questStartActive;
        info.questEnd            = row.questEnd;
        info.castFlags           = row.castFlags;
        info.conditionId         = row.conditionId;

        CreatureInfo const* cInfo = GetCreatureTemplate(npc_entry);
        if (!cInfo)
        {
            sLog.outErrorDb("Table npc_spellclick_spells references unknown creature_template %u. Skipping entry.", npc_entry);
            continue;
        }

        // spell can be 0 for special or custom cases
        if (info.spellId)
        {
            SpellEntry const* spellinfo = sSpellStore.LookupEntry(info.spellId);
            if (!spellinfo)
            {
                sLog.outErrorDb("Table npc_spellclick_spells references unknown spellid %u. Skipping entry.", info.spellId);
                continue;
            }
        }

        if (info.conditionId && !sObjectMgr.GetCondition(info.conditionId))
        {
            sLog.outErrorDb("Table npc_spellclick_spells references unknown condition %u. Skipping entry.", info.conditionId);
            continue;
        }
        else if (!info.conditionId)                         // TODO Drop block after finished converting
        {
            // quest might be 0 to enable spellclick independent of any quest
            if (info.questStart && mQuestTemplates.find(info.questStart) == mQuestTemplates.end())
            {
                sLog.outErrorDb("Table npc_spellclick_spells references unknown start quest %u. Skipping entry.", info.questStart);
                continue;
            }

            // quest might be 0 to enable spellclick active infinity after start quest
            if (info.questEnd && mQuestTemplates.find(info.questEnd) == mQuestTemplates.end())
            {
                sLog.outErrorDb("Table npc_spellclick_spells references unknown end quest %u. Skipping entry.", info.questEnd);
                continue;
            }
        }

        mSpellClickInfoMap.insert(SpellClickInfoMap::value_type(npc_entry, info));

        // mark creature template as spell clickable
        m_creatureTemplates.FindForEdit(cInfo->Entry)->NpcFlags |= UNIT_NPC_FLAG_SPELLCLICK;

        ++count;
    }

    sLog.outString();
    sLog.outString(">> Loaded %u spellclick definitions", count);
}

/**
 * @brief Loads base fishing skill requirements for areas.
 */
void ObjectMgr::LoadFishingBaseSkillLevel()
{
    m_fishingBaseSkill.Clear();                             // for reload case

    uint32 count = 0;
    std::vector<FishingBaseLevelRow> rows = SkillDataRepository::QueryFishingBaseLevels();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded `skill_fishing_base_level`, table is empty!");
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (FishingBaseLevelRow const& row : rows)
    {
        bar.step();

        uint32 entry  = row.entry;
        int32 skill   = row.skill;

        AreaTableEntry const* fArea = GetAreaEntryByAreaID(entry);
        if (!fArea)
        {
            sLog.outErrorDb("AreaId %u defined in `skill_fishing_base_level` does not exist", entry);
            continue;
        }

        m_fishingBaseSkill.Set(entry, skill);
        ++count;
    }

    sLog.outString(">> Loaded %u areas for fishing base skill level", count);
    sLog.outString();
}

static_assert(int(CREATURE_TEMPLATE_SPELL_COLUMNS) == int(CREATURE_MAX_SPELLS), "a creature template spells row holds every creature spell");

/**
 * @brief Loads creature template spell assignments and validates their spell ids.
 */
void ObjectMgr::LoadCreatureTemplateSpells()
{
    TableRead<CreatureTemplateSpellsRow> read = CreatureTemplateSpellsRepository::QueryCreatureTemplateSpells();
    if (read.Unreadable())
    {
        sLog.outError("Error loading `creature_template_spells` table: %s\n", TableReadReason(read.status));
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    uint32 loaded = 0;
    if (read.status == TableReadStatus::Empty)
    {
        sLog.outError("creature_template_spells table is empty!\n");
    }
    else
    {
        m_creatureTemplateSpells.Clear();
        BarGoLink bar(read.rows.size());
        for (CreatureTemplateSpellsRow const& row : read.rows)
        {
            bar.step();
            CreatureTemplateSpells spells;
            spells.entry = row.entry;
            for (uint8 i = 0; i < CREATURE_MAX_SPELLS; ++i)
            {
                spells.spells[i] = row.spells[i];
            }
            m_creatureTemplateSpells.Set(row.entry, spells);
        }
        loaded = uint32(read.rows.size());
    }

    // every row read, in the order read, the rows of a creature already erased included
    for (CreatureTemplateSpellsRow const& row : read.rows)
    {
        if (!ObjectMgr::GetCreatureTemplate(row.entry))
        {
            sLog.outErrorDb("LoadCreatureTemplateSpells: Spells found for creature entry %u, but creature does not exist, skipping", row.entry);
            m_creatureTemplateSpells.Erase(row.entry);
        }
        for (uint8 i = 0; i < CREATURE_MAX_SPELLS; ++i)
        {
            if (row.spells[i] && !sSpellStore.LookupEntry(row.spells[i]))
            {
                sLog.outErrorDb("LoadCreatureTemplateSpells: Spells found for creature entry %u, assigned spell %u does not exist, set to 0", row.entry, row.spells[i]);
                if (CreatureTemplateSpells* spells = m_creatureTemplateSpells.FindForEdit(row.entry))
                {
                    spells->spells[i] = 0;
                }
            }
        }
    }

    sLog.outString(">> Loaded %u creature_template_spells definitions", loaded);
    sLog.outString();
}
