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

#include "combat/ArmorReduction.h"

#include <algorithm>

namespace Combat
{
    uint32 ArmorReducedDamage(uint32 damage, uint32 victimArmor, int32 targetResistanceMod,
                              bool isPlayer, uint32 attackerLevel, uint32 victimLevel,
                              float armorPenetrationPct)
    {
        uint32 newdamage = 0;
        float armor = (float)victimArmor;

        // Ignore enemy armor by SPELL_AURA_MOD_TARGET_RESISTANCE aura
        armor += targetResistanceMod;

        // Apply Player CR_ARMOR_PENETRATION rating and percent talents
        if (isPlayer)
        {
            float maxArmorPen = 400 + 85 * victimLevel;
            if (attackerLevel > 59)
            {
                maxArmorPen += 4.5f * 85 * (victimLevel - 59);
            }
            // Cap ignored armor to this value
            maxArmorPen = std::min(((armor + maxArmorPen) / 3), armor);
            // Also, armor penetration is limited to 100% since 3.1.2, before greater values did
            // continue to give benefit for targets with more armor than the above cap
            float armorPenPct = std::min(100.f, armorPenetrationPct);
            armor -= maxArmorPen * armorPenPct / 100.0f;
        }

        if (armor < 0.0f)
        {
            armor = 0.0f;
        }

        float levelModifier = (float)attackerLevel;
        if (levelModifier > 59)
        {
            levelModifier = levelModifier + (4.5f * (levelModifier - 59));
        }

        float tmpvalue = 0.1f * armor / (8.5f * levelModifier + 40);
        tmpvalue = tmpvalue / (1.0f + tmpvalue);

        if (tmpvalue < 0.0f)
        {
            tmpvalue = 0.0f;
        }
        if (tmpvalue > 0.75f)
        {
            tmpvalue = 0.75f;
        }

        newdamage = uint32(damage - (damage * tmpvalue));

        return (newdamage > 1) ? newdamage : 1;
    }
}
