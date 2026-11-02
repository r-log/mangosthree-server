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

#ifndef MANGOS_H_AREATRIGGERSTORE
#define MANGOS_H_AREATRIGGERSTORE

#include "Platform/Define.h"
#include "Utilities/Errors.h"

#include <set>
#include <unordered_map>

struct AreaTrigger
{
    uint8  requiredLevel;
    uint32 requiredItem;
    uint32 requiredItem2;
    uint32 heroicKey;
    uint32 heroicKey2;
    uint32 requiredQuest;
    uint32 requiredQuestHeroic;
    uint32 target_mapId;
    float  target_X;
    float  target_Y;
    float  target_Z;
    float  target_Orientation;

    // Operators
    bool IsMinimal() const
    {
        return requiredLevel == 0 && requiredItem == 0 && requiredItem2 == 0 && heroicKey == 0 &&
               heroicKey2 == 0 && requiredQuest == 0 && requiredQuestHeroic == 0;
    }

    bool IsLessOrEqualThan(AreaTrigger const* l) const      // Expected to have same map
    {
        MANGOS_ASSERT(target_mapId == l->target_mapId);
        return requiredLevel <= l->requiredLevel && requiredItem <= l->requiredItem && requiredItem2 <= l->requiredItem2
               && heroicKey <= l->heroicKey && heroicKey2 <= l->heroicKey2 && requiredQuest <= l->requiredQuest && requiredQuestHeroic <= l->requiredQuestHeroic;
    }
};

/**
 * The area triggers: the teleports (`areatrigger_teleport`), one destination and its requirements per
 * trigger id; the quest objective triggers, one quest per trigger id; and the tavern triggers. Each
 * part is cleared and filled by its loader; a trigger id with no entry answers NULL, 0 or false.
 */
class AreaTriggerStore
{
    public:
        typedef std::unordered_map<uint32, AreaTrigger> AreaTriggerMap;

        void ClearTeleports() { m_teleports.clear(); }
        void SetTeleport(uint32 triggerId, AreaTrigger const& trigger) { m_teleports[triggerId] = trigger; }
        AreaTrigger const* FindTeleport(uint32 triggerId) const;
        AreaTriggerMap const& Teleports() const { return m_teleports; }

        void ClearQuestTriggers() { m_questTriggers.clear(); }
        void SetQuestTrigger(uint32 triggerId, uint32 questId) { m_questTriggers[triggerId] = questId; }
        uint32 QuestFor(uint32 triggerId) const;

        void ClearTaverns() { m_taverns.clear(); }
        void AddTavern(uint32 triggerId) { m_taverns.insert(triggerId); }
        bool IsTavern(uint32 triggerId) const { return m_taverns.find(triggerId) != m_taverns.end(); }

    private:
        AreaTriggerMap m_teleports;
        std::unordered_map<uint32, uint32> m_questTriggers;
        std::set<uint32> m_taverns;
};

#endif
