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

#include "CharacterEnumPacket.h"
#include "ByteBuffer.h"
#include "DBCStores.h"
#include "ItemPrototype.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Util.h"
#include "data/GameConfig.h"
#include "persistence/characters/CharacterRepository.h"

enum CharacterFlags
{
    CHARACTER_FLAG_NONE                 = 0x00000000,
    CHARACTER_FLAG_UNK1                 = 0x00000001,
    CHARACTER_FLAG_UNK2                 = 0x00000002,
    CHARACTER_LOCKED_FOR_TRANSFER       = 0x00000004,
    CHARACTER_FLAG_UNK4                 = 0x00000008,
    CHARACTER_FLAG_UNK5                 = 0x00000010,
    CHARACTER_FLAG_UNK6                 = 0x00000020,
    CHARACTER_FLAG_UNK7                 = 0x00000040,
    CHARACTER_FLAG_UNK8                 = 0x00000080,
    CHARACTER_FLAG_UNK9                 = 0x00000100,
    CHARACTER_FLAG_UNK10                = 0x00000200,
    CHARACTER_FLAG_HIDE_HELM            = 0x00000400,
    CHARACTER_FLAG_HIDE_CLOAK           = 0x00000800,
    CHARACTER_FLAG_UNK13                = 0x00001000,
    CHARACTER_FLAG_GHOST                = 0x00002000,
    CHARACTER_FLAG_RENAME               = 0x00004000,
    CHARACTER_FLAG_UNK16                = 0x00008000,
    CHARACTER_FLAG_UNK17                = 0x00010000,
    CHARACTER_FLAG_UNK18                = 0x00020000,
    CHARACTER_FLAG_UNK19                = 0x00040000,
    CHARACTER_FLAG_UNK20                = 0x00080000,
    CHARACTER_FLAG_UNK21                = 0x00100000,
    CHARACTER_FLAG_UNK22                = 0x00200000,
    CHARACTER_FLAG_UNK23                = 0x00400000,
    CHARACTER_FLAG_UNK24                = 0x00800000,
    CHARACTER_FLAG_LOCKED_BY_BILLING    = 0x01000000,
    CHARACTER_FLAG_DECLINED             = 0x02000000,
    CHARACTER_FLAG_UNK27                = 0x04000000,
    CHARACTER_FLAG_UNK28                = 0x08000000,
    CHARACTER_FLAG_UNK29                = 0x10000000,
    CHARACTER_FLAG_UNK30                = 0x20000000,
    CHARACTER_FLAG_UNK31                = 0x40000000,
    CHARACTER_FLAG_UNK32                = 0x80000000
};

enum CharacterCustomizeFlags
{
    CHAR_CUSTOMIZE_FLAG_NONE            = 0x00000000,
    CHAR_CUSTOMIZE_FLAG_CUSTOMIZE       = 0x00000001,       // name, gender, etc...
    CHAR_CUSTOMIZE_FLAG_FACTION         = 0x00010000,       // name, gender, faction, etc...
    CHAR_CUSTOMIZE_FLAG_RACE            = 0x00100000        // name, gender, race, etc...
};

bool WriteCharacterEnumData(CharacterEnumRow const& row, ByteBuffer* data, ByteBuffer* buffer)
{
    ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, row.guid);
    uint8 pRace = row.race;
    uint8 pClass = row.classId;

    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(pRace, pClass);
    if (!info)
    {
        sLog.outError("%s has incorrect race/class pair. Don't build enum.", guid.GetString().c_str());
        return false;
    }

    ObjectGuid guildGuid = ObjectGuid(HIGHGUID_GUILD, row.guildId);
    std::string name = row.name;
    uint8 gender = row.gender;
    uint32 playerBytes = row.playerBytes;
    uint8 level = row.level;
    uint32 playerFlags = row.playerFlags;
    uint32 atLoginFlags = row.atLogin;
    uint32 zone = row.zone;
    uint32 petDisplayId = 0;
    uint32 petLevel   = 0;
    uint32 petFamily  = 0;
    uint32 char_flags = 0;

    data->WriteGuidMask<3>(guid);
    data->WriteGuidMask<1, 7, 2>(guildGuid);
    data->WriteBits(name.length(), 7);
    data->WriteGuidMask<4, 7>(guid);
    data->WriteGuidMask<3>(guildGuid);
    data->WriteGuidMask<5>(guid);
    data->WriteGuidMask<6>(guildGuid);
    data->WriteGuidMask<1>(guid);
    data->WriteGuidMask<5, 4>(guildGuid);
    data->WriteBit(atLoginFlags & AT_LOGIN_FIRST);
    data->WriteGuidMask<0, 2, 6>(guid);
    data->WriteGuidMask<0>(guildGuid);

    // show pet at selection character in character list only for non-ghost character
    if (!(playerFlags & PLAYER_FLAGS_GHOST) && (pClass == CLASS_WARLOCK || pClass == CLASS_HUNTER || pClass == CLASS_DEATH_KNIGHT))
    {
        uint32 entry = row.petEntry;
        CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(entry);
        if (cInfo)
        {
            petDisplayId = row.petModelId;
            petLevel     = row.petLevel;
            petFamily    = cInfo->Family;
        }
    }

    if (playerFlags & PLAYER_FLAGS_HIDE_HELM)
    {
        char_flags |= CHARACTER_FLAG_HIDE_HELM;
    }
    if (playerFlags & PLAYER_FLAGS_HIDE_CLOAK)
    {
        char_flags |= CHARACTER_FLAG_HIDE_CLOAK;
    }
    if (playerFlags & PLAYER_FLAGS_GHOST)
    {
        char_flags |= CHARACTER_FLAG_GHOST;
    }
    if (atLoginFlags & AT_LOGIN_RENAME)
    {
        char_flags |= CHARACTER_FLAG_RENAME;
    }
    if (sGameConfig.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED))
    {
        if (!row.genitive.empty())
        {
            char_flags |= CHARACTER_FLAG_DECLINED;
        }
    }
    else
    {
        char_flags |= CHARACTER_FLAG_DECLINED;
    }

    *buffer << uint8(pClass);                                // class

    Tokens tdata = StrSplit(row.equipmentCache, " ");
    for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        uint32 visualbase = slot * 2;                       // entry, perm ench., temp ench.
        uint32 item_id = GetUInt32ValueFromArray(tdata, visualbase);
        const ItemPrototype* proto = ObjectMgr::GetItemPrototype(item_id);
        if (!proto)
        {
            *buffer << uint8(0);
            *buffer << uint32(0);
            *buffer << uint32(0);
            continue;
        }

        SpellItemEnchantmentEntry const* enchant = NULL;

        uint32 enchants = GetUInt32ValueFromArray(tdata, visualbase + 1);
        for (uint8 enchantSlot = PERM_ENCHANTMENT_SLOT; enchantSlot <= TEMP_ENCHANTMENT_SLOT; ++enchantSlot)
        {
            // values stored in 2 uint16
            uint32 enchantId = 0x0000FFFF & (enchants >> enchantSlot * 16);
            if (!enchantId)
            {
                continue;
            }

            if ((enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId)))
            {
                break;
            }
        }

        *buffer << uint8(proto->InventoryType);
        *buffer << uint32(proto->DisplayInfoID);
        *buffer << uint32(enchant ? enchant->ItemVisual : 0);
    }

    for (int32 i = 0; i < 4; i++)
    {
        *buffer << uint8(0);
        *buffer << uint32(0);
        *buffer << uint32(0);
    }

    *buffer << uint32(petFamily);                           // Pet DisplayID
    buffer->WriteGuidBytes<2>(guildGuid);
    *buffer << uint8(row.slot);                // char order id
    *buffer << uint8((playerBytes >> 16) & 0xFF);           // Hair style
    buffer->WriteGuidBytes<3>(guildGuid);
    *buffer << uint32(petDisplayId);                        // Pet DisplayID
    *buffer << uint32(char_flags);                          // character flags
    *buffer << uint8((playerBytes >> 24) & 0xFF);           // Hair color
    buffer->WriteGuidBytes<4>(guid);
    *buffer << uint32(row.map);               // map
    buffer->WriteGuidBytes<5>(guildGuid);
    *buffer << row.positionZ;                       // z
    buffer->WriteGuidBytes<6>(guildGuid);
    *buffer << uint32(petLevel);                            // pet level
    buffer->WriteGuidBytes<3>(guid);
    *buffer << row.positionY;                       // y
    // character customize flags
    *buffer << uint32(atLoginFlags & AT_LOGIN_CUSTOMIZE ? CHAR_CUSTOMIZE_FLAG_CUSTOMIZE : CHAR_CUSTOMIZE_FLAG_NONE);

    uint32 playerBytes2 = row.playerBytes2;
    *buffer << uint8(playerBytes2 & 0xFF);                  // facial hair
    buffer->WriteGuidBytes<7>(guid);
    *buffer << uint8(gender);                               // Gender
    buffer->append(name.c_str(), name.length());
    *buffer << uint8((playerBytes >> 8) & 0xFF);            // face

    buffer->WriteGuidBytes<0, 2>(guid);
    buffer->WriteGuidBytes<1, 7>(guildGuid);

    *buffer << row.positionX;                       // x
    *buffer << uint8(playerBytes & 0xFF);                   // skin
    *buffer << uint8(pRace);                                // Race
    *buffer << uint8(level);                                // Level
    buffer->WriteGuidBytes<6>(guid);
    buffer->WriteGuidBytes<4, 0>(guildGuid);
    buffer->WriteGuidBytes<5, 1>(guid);

    *buffer << uint32(zone);                                // Zone id

    return true;
}
