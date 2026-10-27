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

#include "MapDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace MapDataRepository
{
    static TransportRow ReadTransport(Field* fields)
    {
        TransportRow row;
        row.entry = fields[0].GetUInt32();
        row.name = fields[1].GetCppString();
        row.period = fields[2].GetUInt32();
        return row;
    }

    std::vector<TransportRow> QueryTransports()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `name`, `period` FROM `transports`");
        return ReadRows<TransportRow>(result, ReadTransport);
    }

    static TransportGameObjectRow ReadTransportGameObject(Field* fields)
    {
        TransportGameObjectRow row;
        row.guid = fields[0].GetUInt32();
        row.id = fields[1].GetUInt32();
        row.name = fields[2].GetCppString();
        return row;
    }

    std::vector<TransportGameObjectRow> QueryTransportGameObjects()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `gameobject`.`guid`,`gameobject`.`id`,`transports`.`name` FROM `gameobject`,`transports` WHERE `gameobject`.`id` = `transports`.`entry`");
        return ReadRows<TransportGameObjectRow>(result, ReadTransportGameObject);
    }

    static WeatherRow ReadWeather(Field* fields)
    {
        WeatherRow row;
        row.zone = fields[0].GetUInt32();
        for (int season = 0; season < WEATHER_ROW_SEASONS; ++season)
        {
            row.rainChance[season] = fields[season * WEATHER_ROW_CHANCES + 1].GetUInt32();
            row.snowChance[season] = fields[season * WEATHER_ROW_CHANCES + 2].GetUInt32();
            row.stormChance[season] = fields[season * WEATHER_ROW_CHANCES + 3].GetUInt32();
        }
        return row;
    }

    std::vector<WeatherRow> QueryWeather()
    {
        //                                 0     1                   2                   3                    4                   5                   6                    7                 8                 9                  10                  11                  12
        QueryResult* result = WorldDatabase.Query("SELECT `zone`, `spring_rain_chance`, `spring_snow_chance`, `spring_storm_chance`, `summer_rain_chance`, `summer_snow_chance`, `summer_storm_chance`, `fall_rain_chance`, `fall_snow_chance`, `fall_storm_chance`, `winter_rain_chance`, `winter_snow_chance`, `winter_storm_chance` FROM `game_weather`");
        return ReadRows<WeatherRow>(result, ReadWeather);
    }

    static GameTeleRow ReadGameTele(Field* fields)
    {
        GameTeleRow row;
        row.id = fields[0].GetUInt32();
        row.positionX = fields[1].GetFloat();
        row.positionY = fields[2].GetFloat();
        row.positionZ = fields[3].GetFloat();
        row.orientation = fields[4].GetFloat();
        row.map = fields[5].GetUInt32();
        row.name = fields[6].GetCppString();
        return row;
    }

    std::vector<GameTeleRow> QueryTeles()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, `name` FROM `game_tele`");
        return ReadRows<GameTeleRow>(result, ReadGameTele);
    }

    bool InsertTele(uint32 id, float x, float y, float z, float orientation, uint32 mapId, std::string const& name)
    {
        std::string safeName(name);
        WorldDatabase.escape_string(safeName);

        return WorldDatabase.PExecuteLog("INSERT INTO `game_tele` "
                                         "(`id`,`position_x`,`position_y`,`position_z`,`orientation`,`map`,`name`) "
                                         "VALUES (%u,%f,%f,%f,%f,%u,'%s')",
                                         id, x, y, z,
                                         orientation, mapId, safeName.c_str());
    }

    void DeleteTele(char const* name)
    {
        std::string safeName(name);
        WorldDatabase.escape_string(safeName);

        WorldDatabase.PExecuteLog("DELETE FROM `game_tele` WHERE `name` = '%s'", safeName.c_str());
    }

    static GraveyardZoneRow ReadGraveyardZone(Field* fields)
    {
        GraveyardZoneRow row;
        row.id = fields[0].GetUInt32();
        row.ghostZone = fields[1].GetUInt32();
        row.faction = fields[2].GetUInt32();
        return row;
    }

    std::vector<GraveyardZoneRow> QueryGraveyardZones()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `id`,`ghost_zone`,`faction` FROM `game_graveyard_zone`");
        return ReadRows<GraveyardZoneRow>(result, ReadGraveyardZone);
    }

    void InsertGraveyardZone(uint32 id, uint32 zoneId, uint32 team)
    {
        WorldDatabase.PExecuteLog("INSERT INTO `game_graveyard_zone` (`id`,`ghost_zone`,`faction`) VALUES ('%u', '%u','%u')", id, zoneId, team);
    }

    static AreaTriggerTeleportRow ReadAreaTriggerTeleport(Field* fields)
    {
        AreaTriggerTeleportRow row;
        row.id = fields[0].GetUInt32();
        row.requiredLevel = fields[1].GetUInt8();
        row.requiredItem = fields[2].GetUInt32();
        row.requiredItem2 = fields[3].GetUInt32();
        row.heroicKey = fields[4].GetUInt32();
        row.heroicKey2 = fields[5].GetUInt32();
        row.requiredQuestDone = fields[6].GetUInt32();
        row.requiredQuestDoneHeroic = fields[7].GetUInt32();
        row.targetMap = fields[8].GetUInt32();
        row.targetPositionX = fields[9].GetFloat();
        row.targetPositionY = fields[10].GetFloat();
        row.targetPositionZ = fields[11].GetFloat();
        row.targetOrientation = fields[12].GetFloat();
        return row;
    }

    std::vector<AreaTriggerTeleportRow> QueryAreaTriggerTeleports()
    {
        //                                  0     1                 2                3                 4             5              6                      7                             8             9                    10                   11                   12
        QueryResult* result = WorldDatabase.Query("SELECT `id`, `required_level`, `required_item`, `required_item2`, `heroic_key`, `heroic_key2`, `required_quest_done`, `required_quest_done_heroic`, `target_map`, `target_position_x`, `target_position_y`, `target_position_z`, `target_orientation` FROM `areatrigger_teleport`");
        return ReadRows<AreaTriggerTeleportRow>(result, ReadAreaTriggerTeleport);
    }

    static TavernAreaTriggerRow ReadTavernAreaTrigger(Field* fields)
    {
        TavernAreaTriggerRow row;
        row.id = fields[0].GetUInt32();
        return row;
    }

    std::vector<TavernAreaTriggerRow> QueryTavernAreaTriggers()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `id` FROM `areatrigger_tavern`");
        return ReadRows<TavernAreaTriggerRow>(result, ReadTavernAreaTrigger);
    }
}
