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

#include "combat/MeleeChances.h"

// The switch in APMultiplier names InventoryType and ItemSubclassWeapon values.
// ItemPrototype.h is a plain data header -- it reaches nothing but Platform/Define.h
// and the standard library -- so naming the constants here beats copying their values.
#include "ItemPrototype.h"

namespace Combat
{
    float APMultiplier(uint32 attackTime, bool isPlayer, bool normalized, bool hasWeapon,
                       uint32 weaponInventoryType, uint32 weaponSubClass)
    {
        if (!normalized || !isPlayer)
        {
            return float(attackTime) / 1000.0f;
        }

        if (!hasWeapon)
        {
            return 2.4f;                                         // fist attack
        }

        switch (weaponInventoryType)
        {
            case INVTYPE_2HWEAPON:
                return 3.3f;
            case INVTYPE_RANGED:
            case INVTYPE_RANGEDRIGHT:
            case INVTYPE_THROWN:
                return 2.8f;
            case INVTYPE_WEAPON:
            case INVTYPE_WEAPONMAINHAND:
            case INVTYPE_WEAPONOFFHAND:
            default:
                return weaponSubClass == ITEM_SUBCLASS_WEAPON_DAGGER ? 1.7f : 2.4f;
        }
    }

    float MeleeMissChance(bool hasVictim, WeaponAttackType attType, bool hasOffhandWeapon,
                          bool isNormalSpellActive, bool hasMeleeSpell, uint16 attackerSkill,
                          uint16 victimDefenseSkill, bool victimIsPlayer,
                          float modRangedHitChance, float modMeleeHitChance,
                          int32 victimRangedHitChanceMod, int32 victimMeleeHitChanceMod)
    {
        if (!hasVictim)
        {
            return 0.0f;
        }

        // Base misschance 5%
        float missChance = 5.0f;

        // DualWield - white damage has additional 19% miss penalty
        if (hasOffhandWeapon && attType != RANGED_ATTACK)
        {
            if (!isNormalSpellActive && !hasMeleeSpell)
            {
                missChance += 19.0f;
            }
        }

        int32 skillDiff = int32(attackerSkill) - int32(victimDefenseSkill);

        // PvP - PvE melee chances
        // TODO: implement diminishing returns for defense from player's defense rating
        // pure skill diff is not sufficient since 3.x anymore, but exact formulas hard to research
        if (victimIsPlayer)
        {
            missChance -= skillDiff * 0.04f;
        }
        else if (skillDiff < -10)
        {
            missChance -= (skillDiff + 10) * 0.4f - 1.0f;
        }
        else
        {
            missChance -=  skillDiff * 0.1f;
        }

        // Hit chance bonus from attacker based on ratings and auras
        if (attType == RANGED_ATTACK)
        {
            missChance -= modRangedHitChance;
        }
        else
        {
            missChance -= modMeleeHitChance;
        }

        // Modify miss chance by victim auras
        if (attType == RANGED_ATTACK)
        {
            missChance -= victimRangedHitChanceMod;
        }
        else
        {
            missChance -= victimMeleeHitChanceMod;
        }

        // Limit miss chance from 0 to 60%
        if (missChance < 0.0f)
        {
            return 0.0f;
        }
        if (missChance > 60.0f)
        {
            return 60.0f;
        }

        return missChance;
    }

    float UnitCriticalChance(WeaponAttackType attackType, bool isPlayer, float playerOffhandCrit,
                             float playerMainhandCrit, float playerRangedCrit, int32 critAuraMod,
                             int32 victimRangedCritMod, int32 victimMeleeCritMod,
                             int32 victimSpellAndWeaponCritMod)
    {
        float crit;

        if (isPlayer)
        {
            switch (attackType)
            {
                case OFF_ATTACK:
                    crit = playerOffhandCrit;
                    break;
                case BASE_ATTACK:
                    crit = playerMainhandCrit;
                    break;
                case RANGED_ATTACK:
                    crit = playerRangedCrit;
                    break;
                    // Just for good manner
                default:
                    crit = 0.0f;
                    break;
            }
        }
        else
        {
            crit = 5.0f;
            crit += critAuraMod;
        }

        // flat aura mods
        if (attackType == RANGED_ATTACK)
        {
            crit += victimRangedCritMod;
        }
        else
        {
            crit += victimMeleeCritMod;
        }

        crit += victimSpellAndWeaponCritMod;

        if (crit < 0.0f)
        {
            crit = 0.0f;
        }
        return crit;
    }

    float UnitDodgeChance(bool isStunned, bool isPlayer, float playerDodgePercentage,
                          bool isTotem, int32 dodgeAuraMod)
    {
        if (isStunned)
        {
            return 0.0f;
        }
        if (isPlayer)
        {
            return playerDodgePercentage;
        }
        else
        {
            if (isTotem)
            {
                return 0.0f;
            }
            else
            {
                float dodge = 5.0f;
                dodge += dodgeAuraMod;
                return dodge > 0.0f ? dodge : 0.0f;
            }
        }
    }

    float UnitParryChance(bool isCastingNonMeleeSpell, bool isStunned, bool isPlayer,
                          bool isCreature, bool canParry, bool hasParryWeapon,
                          float playerParryPercentage, uint32 creatureType, int32 parryAuraMod)
    {
        if (isCastingNonMeleeSpell || isStunned)
        {
            return 0.0f;
        }

        float chance = 0.0f;

        if (isPlayer)
        {
            if (canParry)
            {
                if (hasParryWeapon)
                {
                    chance = playerParryPercentage;
                }
            }
        }
        else if (isCreature)
        {
            if (creatureType == CREATURE_TYPE_HUMANOID)
            {
                chance = 5.0f;
                chance += parryAuraMod;
            }
        }

        return chance > 0.0f ? chance : 0.0f;
    }

    float UnitBlockChance(bool isCastingNonMeleeSpell, bool isStunned, bool isPlayer,
                          bool canBlock, bool canUseOffhandWeapon, bool hasUnbrokenOffhandItem,
                          float playerBlockPercentage, bool isTotem, int32 blockAuraMod)
    {
        if (isCastingNonMeleeSpell || isStunned)
        {
            return 0.0f;
        }

        if (isPlayer)
        {
            if (canBlock && canUseOffhandWeapon)
            {
                if (hasUnbrokenOffhandItem)
                {
                    return playerBlockPercentage;
                }
            }
            // is player but has no block ability or no not broken shield equipped
            return 0.0f;
        }
        else
        {
            if (isTotem)
            {
                return 0.0f;
            }
            else
            {
                float block = 5.0f;
                block += blockAuraMod;
                return block > 0.0f ? block : 0.0f;
            }
        }
    }

    int32 MagicSpellBaseHitChance(bool victimIsPlayer, uint32 victimLevel, uint32 attackerLevel)
    {
        // PvP - PvE spell misschances per leveldif > 2
        int32 lchance = victimIsPlayer ? 7 : 11;
        int32 leveldif = int32(victimLevel) - int32(attackerLevel);

        // Base hit chance from attacker and victim levels
        int32 modHitChance;
        if (leveldif < 3)
        {
            modHitChance = 96 - leveldif;
        }
        else
        {
            modHitChance = 94 - (leveldif - 2) * lchance;
        }

        return modHitChance;
    }
}
