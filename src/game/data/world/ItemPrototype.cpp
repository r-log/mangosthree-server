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

#include "ItemPrototype.h"

#include <cmath>

#include "DBCStores.h"

uint32 ItemPrototype::GetArmor() const
{
    if (Quality >= ITEM_QUALITY_HEIRLOOM)                   // heirlooms have it's own dbc...
    {
        return 0;
    }

    if (Class == ITEM_CLASS_ARMOR && SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
    {
        if (ItemArmorShieldEntry const* ias = sItemArmorShieldStore.LookupEntry(ItemLevel))
        {
            return uint32(floor(ias->Quality[Quality] + 0.5f));
        }
        return 0;
    }

    ItemArmorQualityEntry const* iaq = sItemArmorQualityStore.LookupEntry(ItemLevel);
    ItemArmorTotalEntry const* iat = sItemArmorTotalStore.LookupEntry(ItemLevel);

    if (!iaq || !iat)
    {
        return 0;
    }

    if (InventoryType != INVTYPE_HEAD && InventoryType != INVTYPE_CHEST && InventoryType != INVTYPE_SHOULDERS &&
        InventoryType != INVTYPE_LEGS && InventoryType != INVTYPE_FEET && InventoryType != INVTYPE_WRISTS &&
        InventoryType != INVTYPE_HANDS && InventoryType != INVTYPE_WAIST && InventoryType != INVTYPE_CLOAK &&
        InventoryType != INVTYPE_ROBE)
        return 0;

    ArmorLocationEntry const* al = NULL;

    if (InventoryType == INVTYPE_ROBE)
    {
        al = sArmorLocationStore.LookupEntry(INVTYPE_CHEST);
    }
    else
    {
        al = sArmorLocationStore.LookupEntry(InventoryType);
    }

    if (!al)
    {
        return 0;
    }

    float iatMult, alMult;

    switch (SubClass)
    {
        case ITEM_SUBCLASS_ARMOR_CLOTH:
            iatMult = iat->Value[0];
            alMult = al->Value[0];
            break;
        case ITEM_SUBCLASS_ARMOR_LEATHER:
            iatMult = iat->Value[1];
            alMult = al->Value[1];
            break;
        case ITEM_SUBCLASS_ARMOR_MAIL:
            iatMult = iat->Value[2];
            alMult = al->Value[2];
            break;
        case ITEM_SUBCLASS_ARMOR_PLATE:
            iatMult = iat->Value[3];
            alMult = al->Value[3];
            break;
        default:
            return 0;
    }

    return uint32(floor(iaq->Qualitymod[Quality] * iatMult * alMult + 0.5f));
}

float ItemPrototype::getDPS() const
{
    float damage = 0.0f;

    if (Class == ITEM_CLASS_WEAPON)
    {
        if (Quality >= ITEM_QUALITY_HEIRLOOM)               // heirlooms have it's own dbc...
        {
            return damage;
        }

        ItemDamageEntry const* id = NULL;

        switch (InventoryType)
        {
            case INVTYPE_WEAPON:
            case INVTYPE_WEAPONMAINHAND:
            case INVTYPE_WEAPONOFFHAND:
                if (Flags2 & ITEM_FLAG2_CASTER_WEAPON)      // caster weapon flag
                {
                    id = sItemDamageOneHandCasterStore.LookupEntry(ItemLevel);
                }
                else
                {
                    id = sItemDamageOneHandStore.LookupEntry(ItemLevel);
                }
                break;
            case INVTYPE_2HWEAPON:
                if (Flags2 & ITEM_FLAG2_CASTER_WEAPON)      // caster weapon flag
                {
                    id = sItemDamageTwoHandCasterStore.LookupEntry(ItemLevel);
                }
                else
                {
                    id = sItemDamageTwoHandStore.LookupEntry(ItemLevel);
                }
                break;
            case INVTYPE_AMMO:
                id = sItemDamageAmmoStore.LookupEntry(ItemLevel);
                break;
            case INVTYPE_RANGED:
            case INVTYPE_THROWN:
            case INVTYPE_RANGEDRIGHT:
                switch (SubClass)
                {
                    case ITEM_SUBCLASS_WEAPON_BOW:
                    case ITEM_SUBCLASS_WEAPON_GUN:
                    case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                        id = sItemDamageRangedStore.LookupEntry(ItemLevel);
                        break;
                    case ITEM_SUBCLASS_WEAPON_THROWN:
                        id = sItemDamageThrownStore.LookupEntry(ItemLevel);
                        break;
                    case ITEM_SUBCLASS_WEAPON_WAND:
                        id = sItemDamageWandStore.LookupEntry(ItemLevel);
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }

        if (!id)
        {
            return damage;
        }

        return id->Quality[Quality];
    }

    return damage;
}
