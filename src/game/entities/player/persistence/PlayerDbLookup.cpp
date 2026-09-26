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

#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "SkillDiscovery.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "ArenaTeam.h"
#include "Chat.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "SQLStorages.h"
#include "Vehicle.h"
#include "Calendar.h"
#include "CharacterCache.h"
#include "DisableMgr.h"

#include <cmath>

/**
 * Decoupling D7c: these six lookups answered from `characters`, `guild_member` and
 * `arena_team_member` with a blocking PQuery, inside the tick, every time a caller
 * asked about a character who was not logged in. They now read CharacterCache, which
 * holds the same rows in memory and is kept current at every setter that writes one.
 *
 * The names keep the `FromDB` suffix because 17 call sites spell them that way and this
 * PR does not touch callers; what changed is where the answer comes from, not what it is.
 */

/**
 * @brief Reads a player's guild identifier from the character cache.
 *
 * @param guid The player GUID to look up.
 * @return The guild identifier, or zero if none is found.
 */
uint32 Player::GetGuildIdFromDB(ObjectGuid guid)
{
    if (CharacterCacheRef entry = sCharacterCache.GetByGuid(guid))
    {
        return entry->guildId;
    }

    return 0;
}

ObjectGuid Player::GetGuildGuidFromDB(ObjectGuid guid)
{
    if (uint32 guildId = GetGuildIdFromDB(guid))
    {
        return ObjectGuid(HIGHGUID_GUILD, GetGuildIdFromDB(guid));
    }
    else
    {
        return ObjectGuid();
    }
}

/**
 * @brief Reads a player's guild rank from the character cache.
 *
 * @param guid The player GUID to look up.
 * @return The guild rank, or zero if none is found.
 */
uint32 Player::GetRankFromDB(ObjectGuid guid)
{
    if (CharacterCacheRef entry = sCharacterCache.GetByGuid(guid))
    {
        // A cached rank is only meaningful while the character is in a guild; the entry
        // clears it when the guild goes, so a non-member reads 0 exactly as the missing
        // `guild_member` row used to.
        return entry->guildRank;
    }

    return 0;
}

uint32 Player::GetArenaTeamIdFromDB(ObjectGuid guid, ArenaType type)
{
    const uint8 slot = ArenaTeam::GetSlotByType(type);
    if (slot >= MAX_ARENA_SLOT)
    {
        return 0;
    }

    if (CharacterCacheRef entry = sCharacterCache.GetByGuid(guid))
    {
        return entry->arenaTeamId[slot];
    }

    return 0;
}

/**
 * @brief Reads, or derives, a player's saved zone identifier.
 *
 * @param guid The player GUID to look up.
 * @return The resolved zone identifier, or zero on failure.
 */
uint32 Player::GetZoneIdFromDB(ObjectGuid guid)
{
    CharacterCacheRef entry = sCharacterCache.GetByGuid(guid);
    if (!entry)
    {
        return 0;
    }

    uint32 zone = entry->zoneId;
    if (zone)
    {
        return zone;
    }

    // Stored zone is zero: the generic and slow zone detection, from the position the
    // cache kept for exactly this case. The terrain call and the write-back are the ones
    // this function has always made -- only the two SELECTs in front of them are gone.
    uint32 map = 0;
    float posx = 0.0f, posy = 0.0f, posz = 0.0f;
    if (!sCharacterCache.GetPositionForZonelessCharacter(guid, map, posx, posy, posz))
    {
        return 0;
    }

    zone = sTerrainMgr.GetZoneId(map, posx, posy, posz);

    if (zone > 0)
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `zone`='%u' WHERE `guid`='%u'", zone, guid.GetCounter());
        sCharacterCache.UpdateZone(guid, zone);
    }

    return zone;
}

/**
 * @brief Reads a player's level from the character cache.
 *
 * @param guid The player GUID to look up.
 * @return The stored level, or zero on failure.
 */
uint32 Player::GetLevelFromDB(ObjectGuid guid)
{
    if (CharacterCacheRef entry = sCharacterCache.GetByGuid(guid))
    {
        return entry->level;
    }

    return 0;
}
