/*
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

#include "SkillExtraItems.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"
#include "Player.h"
#include "DBCStores.h"

#include <map>
#include "persistence/world/economy/SkillDataRepository.h"

// some type definitions
// no use putting them in the header file, they're only used in this .cpp

// struct to store information about extra item creation
// one entry for every spell that is able to create an extra item
struct SkillExtraItemEntry
{
    // the spell id of the specialization required to create extra items
    uint32 requiredSpecialization;
    // the chance to create one additional item
    float additionalCreateChance;
    // maximum number of extra items created per crafting
    uint8 additionalMaxNum;

    SkillExtraItemEntry()
        : requiredSpecialization(0), additionalCreateChance(0.0f), additionalMaxNum(0) {}

    SkillExtraItemEntry(uint32 rS, float aCC, uint8 aMN)
        : requiredSpecialization(rS), additionalCreateChance(aCC), additionalMaxNum(aMN) {}
};

// map to store the extra item creation info, the key is the spellId of the creation spell, the mapped value is the assigned SkillExtraItemEntry
typedef std::map<uint32, SkillExtraItemEntry> SkillExtraItemMap;

SkillExtraItemMap SkillExtraItemStore;

// loads the extra item creation info from DB
void LoadSkillExtraItemTable()
{
    uint32 count = 0;

    SkillExtraItemStore.clear();                            // need for reload

    std::vector<SkillExtraItemRow> rows = SkillDataRepository::QueryExtraItems();

    if (!rows.empty())
    {
        BarGoLink bar(rows.size());

        for (SkillExtraItemRow const& row : rows)
        {
            bar.step();

            uint32 spellId = row.spellId;

            if (!sSpellStore.LookupEntry(spellId))
            {
                sLog.outError("Skill specialization %u has nonexistent spell id in `skill_extra_item_template`!", spellId);
                continue;
            }

            uint32 requiredSpecialization = row.requiredSpecialization;
            if (!sSpellStore.LookupEntry(requiredSpecialization))
            {
                sLog.outError("Skill specialization %u have nonexistent required specialization spell id %u in `skill_extra_item_template`!", spellId, requiredSpecialization);
                continue;
            }

            float additionalCreateChance = row.additionalCreateChance;
            if (additionalCreateChance <= 0.0f)
            {
                sLog.outError("Skill specialization %u has too low additional create chance in `skill_extra_item_template`!", spellId);
                continue;
            }

            uint8 additionalMaxNum = row.additionalMaxNum;
            if (!additionalMaxNum)
            {
                sLog.outError("Skill specialization %u has 0 max number of extra items in `skill_extra_item_template`!", spellId);
                continue;
            }

            SkillExtraItemEntry& skillExtraItemEntry = SkillExtraItemStore[spellId];

            skillExtraItemEntry.requiredSpecialization = requiredSpecialization;
            skillExtraItemEntry.additionalCreateChance = additionalCreateChance;
            skillExtraItemEntry.additionalMaxNum       = additionalMaxNum;

            ++count;
        }

        sLog.outString();
        sLog.outString(">> Loaded %u spell specialization definitions", count);
    }
    else
    {
        sLog.outString();
        sLog.outString(">> Loaded 0 spell specialization definitions. DB table `skill_extra_item_template` is empty.");
    }
}

bool canCreateExtraItems(Player* player, uint32 spellId, float& additionalChance, uint8& additionalMax)
{
    // get the info for the specified spell
    SkillExtraItemMap::const_iterator ret = SkillExtraItemStore.find(spellId);
    if (ret == SkillExtraItemStore.end())
    {
        return false;
    }

    SkillExtraItemEntry const* specEntry = &ret->second;

    // if no entry, then no extra items can be created
    if (!specEntry)
    {
        return false;
    }

    // the player doesn't have the required specialization, return false
    if (!player->HasSpell(specEntry->requiredSpecialization))
    {
        return false;
    }

    // set the arguments to the appropriate values
    additionalChance = specEntry->additionalCreateChance;
    additionalMax = specEntry->additionalMaxNum;

    // enable extra item creation
    return true;
}
