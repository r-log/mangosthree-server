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

#include "CharacterRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"
#include "Database/SqlOperations.h"

#include <utility>

namespace
{
    /// Queues a staged holder on a database, deleting it if the database refuses it.
    void QueueHolder(Database& database, SqlQueryHolder* holder, CharacterHolderCallback callback)
    {
        if (!database.DelayQueryHolder([callback = std::move(callback)](QueryResult* /*result*/, SqlQueryHolder* answered)
                                       {
                                           callback(answered);
                                       },
                                       holder))
        {
            delete holder;                                  // delete all unprocessed queries
        }
    }
}

namespace CharacterRepository
{
    std::vector<uint32> QueryAccountCharacters(uint32 accountId)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `guid` FROM `characters` WHERE `account`='%u'", accountId);
        return ReadRows<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    std::optional<uint32> QueryAccountCharacterCount(uint32 accountId)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'", accountId);
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static CharacterPositionRow ReadPosition(Field* fields)
    {
        CharacterPositionRow row;
        row.positionX = fields[0].GetFloat();
        row.positionY = fields[1].GetFloat();
        row.positionZ = fields[2].GetFloat();
        row.orientation = fields[3].GetFloat();
        row.map = fields[4].GetUInt32();
        row.taxiPath = fields[5].GetCppString();
        return row;
    }

    std::optional<CharacterPositionRow> QueryPosition(uint32 guid)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `position_x`,`position_y`,`position_z`,`orientation`,`map`,`taxi_path` FROM `characters` WHERE `guid` = '%u'", guid);
        return ReadRow<CharacterPositionRow>(result, ReadPosition);
    }

    void SetAtLoginFlag(uint32 flag, uint32 guid)
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `at_login` = `at_login` | '%u' WHERE `guid` ='%u'",
                                   flag, guid);
    }

    void ClearAtLoginFlag(uint32 flag, uint32 guid)
    {
        CharacterDatabase.PExecute("UPDATE `characters` set `at_login` = `at_login` & ~ %u WHERE `guid` ='%u'", flag, guid);
    }

    void SetZone(uint32 zone, uint32 guid)
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `zone`='%u' WHERE `guid`='%u'", zone, guid);
    }

    void SetPhaseMap(uint32 guid, uint32 map)
    {
        CharacterDatabase.PExecute("REPLACE INTO `character_phase_data` (`guid`, `map`) VALUES (%u, %u)", guid, map);
    }

    void SetAppearance(uint32 gender, uint32 playerBytes, uint32 playerBytes2, uint32 guid)
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `gender` = '%u', `playerBytes` = '%u', `playerBytes2` = '%u' WHERE `guid` = '%u'", gender, playerBytes, playerBytes2, guid);
    }

    static CharacterEnumRow ReadEnum(Field* fields, bool declinedNames)
    {
        CharacterEnumRow row;
        row.guid = fields[0].GetUInt32();
        row.name = fields[1].GetCppString();
        row.race = fields[2].GetUInt8();
        row.classId = fields[3].GetUInt8();
        row.gender = fields[4].GetUInt8();
        row.playerBytes = fields[5].GetUInt32();
        row.playerBytes2 = fields[6].GetUInt32();
        row.level = fields[7].GetUInt8();
        row.zone = fields[8].GetUInt32();
        row.map = fields[9].GetUInt32();
        row.positionX = fields[10].GetFloat();
        row.positionY = fields[11].GetFloat();
        row.positionZ = fields[12].GetFloat();
        row.guildId = fields[13].GetUInt32();
        row.playerFlags = fields[14].GetUInt32();
        row.atLogin = fields[15].GetUInt32();
        row.petEntry = fields[16].GetUInt32();
        row.petModelId = fields[17].GetUInt32();
        row.petLevel = fields[18].GetUInt32();
        row.equipmentCache = fields[19].GetCppString();
        row.slot = fields[20].GetUInt8();
        if (declinedNames)
        {
            row.genitive = fields[21].GetCppString();
        }
        return row;
    }

    void QueryEnum(uint32 accountId, uint32 currentPetSlot, bool declinedNames, std::function<void(std::vector<CharacterEnumRow>)> callback)
    {
        CharacterDatabase.AsyncPQuery([declinedNames, callback = std::move(callback)](QueryResult* result)
                                      {
                                          callback(ReadRows<CharacterEnumRow>(result, [declinedNames](Field* fields) { return ReadEnum(fields, declinedNames); }));
                                      },
                                      !declinedNames ?
                                      //   ------- Query Without Declined Names --------
                                      //           0               1                2                3                 4                  5                       6                        7
                                      "SELECT `characters`.`guid`, `characters`.`name`, `characters`.`race`, `characters`.`class`, `characters`.`gender`, `characters`.`playerBytes`, `characters`.`playerBytes2`, `characters`.`level`, "
                                      //   8                9               10                     11                     12                     13                    14
                                      "`characters`.`zone`, `characters`.`map`, `characters`.`position_x`, `characters`.`position_y`, `characters`.`position_z`, `guild_member`.`guildid`, `characters`.`playerFlags`, "
                                      //             15                          16                       17                         18                    19                             20
                                      "`characters`.`at_login`, `character_pet`.`entry`, `character_pet`.`modelid`, `character_pet`.`level`, `characters`.`equipmentCache`, `characters`.`slot` "
                                      "FROM `characters` LEFT JOIN `character_pet` ON `characters`.`guid`=`character_pet`.`owner` AND `character_pet`.`slot`='%u' "
                                      "LEFT JOIN `guild_member` ON `characters`.`guid` = `guild_member`.`guid` "
                                      "WHERE `characters`.`account` = '%u' ORDER BY `characters`.`guid`"
                                      :
                                      //   --------- Query With Declined Names ---------
                                      //                    0                    1                    2                    3                     4                      5                           6                            7
                                      "SELECT `characters`.`guid`, `characters`.`name`, `characters`.`race`, `characters`.`class`, `characters`.`gender`, `characters`.`playerBytes`, `characters`.`playerBytes2`, `characters`.`level`, "
                                      //             8                    9                   10                         11                         12                           13                      14
                                      "`characters`.`zone`, `characters`.`map`, `characters`.`position_x`, `characters`.`position_y`, `characters`.`position_z`, `guild_member`.`guildid`, `characters`.`playerFlags`, "
                                      //             15                          16                       17                         18                    19                             20                               21
                                      "`characters`.`at_login`, `character_pet`.`entry`, `character_pet`.`modelid`, `character_pet`.`level`, `characters`.`equipmentCache`, `characters`.`slot`, `character_declinedname`.`genitive` "
                                      "FROM `characters` LEFT JOIN `character_pet` ON `characters`.`guid` = `character_pet`.`owner` AND `character_pet`.`slot`='%u' "
                                      "LEFT JOIN `character_declinedname` ON `characters`.`guid` = `character_declinedname`.`guid` "
                                      "LEFT JOIN `guild_member` ON `characters`.`guid` = `guild_member`.`guid` "
                                      "WHERE `characters`.`account` = '%u' ORDER BY `characters`.`guid`",
                                      currentPetSlot, accountId);
    }

    void QueueCreateAccountRead(uint32 accountId, CharacterHolderCallback callback)
    {
        SqlQueryHolder* holder = new SqlQueryHolder;
        holder->SetSize(CHAR_CREATE_ACCOUNT_COUNT);
        holder->SetPQuery(CHAR_CREATE_ACCOUNT_REALM_CHARS,
                          "SELECT SUM(`numchars`) FROM `realmcharacters` WHERE `acctid` = '%u'", accountId);
        QueueHolder(LoginDatabase, holder, std::move(callback));
    }

    void QueueCreateRealmReads(uint32 accountId, bool everyCharacter, CharacterHolderCallback callback)
    {
        SqlQueryHolder* holder = new SqlQueryHolder;
        holder->SetSize(CHAR_CREATE_REALM_COUNT);

        holder->SetPQuery(CHAR_CREATE_REALM_CHAR_COUNT,
                          "SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'", accountId);

        holder->SetPQuery(CHAR_CREATE_REALM_ACCOUNT_CHARS,
                          "SELECT `level`,`race`,`class` FROM `characters` WHERE `account` = '%u' %s",
                          accountId, everyCharacter ? "" : "LIMIT 1");

        holder->SetQuery(CHAR_CREATE_REALM_PET_ID, "SELECT id FROM character_pet ORDER BY id DESC LIMIT 1");

        QueueHolder(CharacterDatabase, holder, std::move(callback));
    }

    bool StageLoginReads(SqlQueryHolder* holder, uint32 guid, bool declinedNames)
    {
        holder->SetSize(MAX_PLAYER_LOGIN_QUERY);

        bool res = true;

        // NOTE: all fields in `characters` must be read to prevent lost character data at next save in case wrong DB structure.
        // !!! NOTE: including unused `zone`,`online`
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADFROM,            "SELECT `guid`, `account`, `name`, `race`, `class`, `gender`, `level`, `xp`, `money`, `playerBytes`, `playerBytes2`, `playerFlags`,"
                                 "`position_x`, `position_y`, `position_z`, `map`, `orientation`, `taximask`, `cinematic`, `totaltime`, `leveltime`, `rest_bonus`, `logout_time`, `is_logout_resting`, `resettalents_cost`,"
                                 "`resettalents_time`, `primary_trees`, `trans_x`, `trans_y`, `trans_z`, `trans_o`, `transguid`, `extra_flags`, `stable_slots`, `at_login`, `zone`, `online`, `death_expire_time`, `taxi_path`, `dungeon_difficulty`,"
                                 "`totalKills`, `todayKills`, `yesterdayKills`, `chosenTitle`, `watchedFaction`, `drunk`,"
                                 "`health`, `power1`, `power2`, `power3`, `power4`, `power5`, `specCount`, `activeSpec`, `exploredZones`, `equipmentCache`, `knownTitles`, `actionBars`, `slot`, `createdDate` FROM `characters` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADGROUP,           "SELECT `groupId` FROM group_member WHERE `memberGuid` ='%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES,  "SELECT `id`, `permanent`, `map`, `difficulty`, `resettime` FROM `character_instance` LEFT JOIN `instance` ON `instance` = `id` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADAURAS,           "SELECT `caster_guid`,`item_guid`,`spell`,`stackcount`,`remaincharges`,`basepoints0`,`basepoints1`,`basepoints2`,`periodictime0`,`periodictime1`,`periodictime2`,`maxduration`,`remaintime`,`effIndexMask` FROM `character_aura` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLS,          "SELECT `spell`,`active`,`disabled` FROM `character_spell` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,     "SELECT `quest`,`status`,`rewarded`,`explored`,`timer`,`mobcount1`,`mobcount2`,`mobcount3`,`mobcount4`,`itemcount1`,`itemcount2`,`itemcount3`,`itemcount4`,`itemcount5`,`itemcount6` FROM `character_queststatus` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS, "SELECT `quest` FROM `character_queststatus_daily` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADWEEKLYQUESTSTATUS, "SELECT `quest` FROM `character_queststatus_weekly` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADMONTHLYQUESTSTATUS, "SELECT `quest` FROM `character_queststatus_monthly` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADREPUTATION,      "SELECT `faction`,`standing`,`flags` FROM `character_reputation` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADINVENTORY,       "SELECT `data`,`text`,`bag`,`slot`,`item`,`item_template` FROM `character_inventory` JOIN `item_instance` ON `character_inventory`.`item` = `item_instance`.`guid` WHERE `character_inventory`.`guid` = '%u' ORDER BY `bag`,`slot`", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADITEMLOOT,        "SELECT `guid`,`itemid`,`amount`,`suffix`,`property` FROM `item_loot` WHERE `owner_guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADACTIONS,         "SELECT `spec`,`button`,`action`,`type` FROM `character_action` WHERE `guid` = '%u' ORDER BY `button`", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADSOCIALLIST,      "SELECT `friend`,`flags`,`note` FROM `character_social` WHERE `guid` = '%u' LIMIT 255", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADHOMEBIND,        "SELECT `map`,`zone`,`position_x`,`position_y`,`position_z` FROM `character_homebind` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,  "SELECT `spell`,`item`,`time` FROM `character_spell_cooldown` WHERE `guid` = '%u'", guid);
        if (declinedNames)
        {
            res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES,   "SELECT `genitive`, `dative`, `accusative`, `instrumental`, `prepositional` FROM `character_declinedname` WHERE `guid` = '%u'", guid);
        }
        // in other case still be dummy query
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADGUILD,           "SELECT `guildid`,`rank` FROM `guild_member` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADARENAINFO,       "SELECT `arenateamid`, `played_week`, `played_season`, `wons_season`, `personal_rating` FROM `arena_team_member` WHERE `guid`='%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADACHIEVEMENTS,    "SELECT `achievement`, `date` FROM `character_achievement` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADCRITERIAPROGRESS, "SELECT `criteria`, `counter`, `date` FROM `character_achievement_progress` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADEQUIPMENTSETS,   "SELECT `setguid`, `setindex`, `name`, `iconname`, `ignore_mask`, `item0`, `item1`, `item2`, `item3`, `item4`, `item5`, `item6`, `item7`, `item8`, `item9`, `item10`, `item11`, `item12`, `item13`, `item14`, `item15`, `item16`, `item17`, `item18` FROM `character_equipmentsets` WHERE `guid` = '%u' ORDER BY setindex", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADBGDATA,          "SELECT `instance_id`, `team`, `join_x`, `join_y`, `join_z`, `join_o`, `join_map`, `taxi_start`, `taxi_end`, `mount_spell` FROM `character_battleground_data` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADACCOUNTDATA,     "SELECT `type`, `time`, `data` FROM `character_account_data` WHERE `guid`='%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADTALENTS,         "SELECT `talent_id`, `current_rank`, `spec` FROM `character_talent` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADSKILLS,          "SELECT `skill`, `value`, `max` FROM `character_skills` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADGLYPHS,          "SELECT `spec`, `slot`, `glyph` FROM `character_glyphs` WHERE `guid`='%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILS,           "SELECT `id`,`messageType`,`sender`,`receiver`,`subject`,`body`,`expire_time`,`deliver_time`,`money`,`cod`,`checked`,`stationery`,`mailTemplateId`,`has_items` FROM `mail` WHERE `receiver` = '%u' ORDER BY `id` DESC", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,     "SELECT `data`, `text`, `mail_id`, `item_guid`, `item_template` FROM `mail_items` JOIN `item_instance` ON `item_guid` = `guid` WHERE `receiver` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADCURRENCIES,      "SELECT `id`, `totalCount`, `weekCount`, `seasonCount`, `flags` FROM `character_currencies` WHERE `guid` = '%u'", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADCUFPROFILES,     "SELECT `id`, `name`, `frameHeight`, `frameWidth`, `sortBy`, `healthText`, `boolOptions`, `topPoint`, `bottomPoint`, `leftPoint`, `topOffset`, `bottomOffset`, `leftOffset` FROM `character_cuf_profiles` WHERE `guid` = '%u'", guid);

        // The five pet tables: `character_pet` and `character_pet_declinedname` carry an `owner`
        // column and are read directly; `pet_aura`, `pet_spell` and `pet_spell_cooldown` are keyed
        // on the pet id, so they reach the character through `character_pet`. Each is ordered by
        // pet and then by its own key, so every pet's rows arrive in key order.
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADPETS,            "SELECT `id`, `entry`, `owner`, `modelid`, `level`, `exp`, `Reactstate`, `slot`, `name`, `renamed`, `curhealth`, `curmana`, `abdata`, `savetime`, `resettalents_cost`, `resettalents_time`, `CreatedBySpell`, `PetType` FROM `character_pet` WHERE `owner` = '%u' ORDER BY `id`", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADPETAURAS,        "SELECT `pet_aura`.`guid`, `pet_aura`.`caster_guid`, `pet_aura`.`item_guid`, `pet_aura`.`spell`, `pet_aura`.`stackcount`, `pet_aura`.`remaincharges`, `pet_aura`.`basepoints0`, `pet_aura`.`basepoints1`, `pet_aura`.`basepoints2`, `pet_aura`.`periodictime0`, `pet_aura`.`periodictime1`, `pet_aura`.`periodictime2`, `pet_aura`.`maxduration`, `pet_aura`.`remaintime`, `pet_aura`.`effIndexMask` FROM `pet_aura` JOIN `character_pet` ON `pet_aura`.`guid` = `character_pet`.`id` WHERE `character_pet`.`owner` = '%u' ORDER BY `pet_aura`.`guid`, `pet_aura`.`caster_guid`, `pet_aura`.`item_guid`, `pet_aura`.`spell`", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADPETSPELLS,       "SELECT `pet_spell`.`guid`, `pet_spell`.`spell`, `pet_spell`.`active` FROM `pet_spell` JOIN `character_pet` ON `pet_spell`.`guid` = `character_pet`.`id` WHERE `character_pet`.`owner` = '%u' ORDER BY `pet_spell`.`guid`, `pet_spell`.`spell`", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADPETSPELLCOOLDOWNS, "SELECT `pet_spell_cooldown`.`guid`, `pet_spell_cooldown`.`spell`, `pet_spell_cooldown`.`time` FROM `pet_spell_cooldown` JOIN `character_pet` ON `pet_spell_cooldown`.`guid` = `character_pet`.`id` WHERE `character_pet`.`owner` = '%u' ORDER BY `pet_spell_cooldown`.`guid`, `pet_spell_cooldown`.`spell`", guid);
        res &= holder->SetPQuery(PLAYER_LOGIN_QUERY_LOADPETDECLINEDNAMES, "SELECT `id`, `genitive`, `dative`, `accusative`, `instrumental`, `prepositional` FROM `character_pet_declinedname` WHERE `owner` = '%u' ORDER BY `id`", guid);

        return res;
    }

    bool QueueLoginReads(SqlQueryHolder* holder, CharacterHolderCallback callback)
    {
        return CharacterDatabase.DelayQueryHolder([callback = std::move(callback)](QueryResult* /*result*/, SqlQueryHolder* answered)
                                                  {
                                                      callback(answered);
                                                  },
                                                  holder);
    }

    void SetOnline(uint32 guid)
    {
        static SqlStatementID updChars;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updChars, "UPDATE `characters` SET `online` = 1 WHERE `guid` = ?");
        stmt.PExecute(guid);
    }

    void SetAccountOffline(uint32 accountId)
    {
        static SqlStatementID updChars;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updChars, "UPDATE `characters` SET `online` = 0 WHERE `account` = ?");
        stmt.PExecute(accountId);
    }

    static CharacterNameRow ReadName(Field* fields, bool declinedNames)
    {
        CharacterNameRow row;
        row.guid = fields[0].GetUInt32();
        row.name = fields[1].GetCppString();
        row.race = fields[2].GetUInt8();
        row.gender = fields[3].GetUInt8();
        row.classId = fields[4].GetUInt8();
        if (declinedNames)
        {
            for (int i = 0; i < CHARACTER_DECLINED_NAME_ROW_CASES; ++i)
            {
                row.declinedNames[i] = fields[i + 5].GetCppString();
            }
        }
        return row;
    }

    void QueryName(uint32 guid, bool declinedNames, std::function<void(std::optional<CharacterNameRow>)> callback)
    {
        CharacterDatabase.AsyncPQuery([declinedNames, callback = std::move(callback)](QueryResult* result)
                                      {
                                          callback(ReadRow<CharacterNameRow>(result, [declinedNames](Field* fields) { return ReadName(fields, declinedNames); }));
                                      },
                                      !declinedNames ?
                                      //   ------- Query Without Declined Names --------
                                      //       0       1       2       3         4
                                      "SELECT `guid`, `name`, `race`, `gender`, `class` "
                                      "FROM `characters` WHERE `guid` = '%u'"
                                      :
                                      //   --------- Query With Declined Names ---------
                                      //                    0       1       2       3         4
                                      "SELECT `characters`.`guid`, `name`, `race`, `gender`, `class`, "
                                      //   5        6         7             8               9
                                      "`genitive`, `dative`, `accusative`, `instrumental`, `prepositional` "
                                      "FROM `characters` LEFT JOIN `character_declinedname` ON `characters`.`guid` = `character_declinedname`.`guid` WHERE `characters`.`guid` = '%u'",
                                      guid);
    }

    static RenameTargetRow ReadRenameTarget(Field* fields)
    {
        RenameTargetRow row;
        row.guid = fields[0].GetUInt32();
        row.name = fields[1].GetCppString();
        return row;
    }

    void QueryRenameTarget(uint32 guid, uint32 accountId, uint32 renameFlag, std::function<void(std::optional<RenameTargetRow>)> callback)
    {
        CharacterDatabase.AsyncPQuery([callback = std::move(callback)](QueryResult* result)
                                      {
                                          callback(ReadRow<RenameTargetRow>(result, ReadRenameTarget));
                                      },
                                      "SELECT `guid`, `name` FROM `characters` WHERE `guid` = %u AND `account` = %u AND (`at_login` & %u) = %u",
                                      guid, accountId, renameFlag, renameFlag);
    }

    void Rename(uint32 guid, std::string const& name, uint32 atLoginFlag)
    {
        static SqlStatementID renameCharacter;
        SqlStatement stmt = CharacterDatabase.CreateStatement(renameCharacter,
                            "UPDATE `characters` SET `name` = ?, `at_login` = `at_login` & ~ ? WHERE `guid` = ?");
        stmt.PExecute(name.c_str(), atLoginFlag, guid);
        CharacterDatabase.PExecute("DELETE FROM `character_declinedname` WHERE `guid` ='%u'", guid);
    }

    void SetDeclinedNames(uint32 guid, std::string const (&names)[CHARACTER_DECLINED_NAME_ROW_CASES])
    {
        static SqlStatementID insertDeclinedName;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insertDeclinedName,
                            "INSERT INTO `character_declinedname` (`guid`, `genitive`, `dative`, `accusative`, `instrumental`, `prepositional`) VALUES (?,?,?,?,?,?)");
        stmt.addUInt32(guid);
        for (int i = 0; i < CHARACTER_DECLINED_NAME_ROW_CASES; ++i)
        {
            stmt.addString(names[i]);
        }

        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `character_declinedname` WHERE `guid` = '%u'", guid);
        stmt.Execute();
        CharacterDatabase.CommitTransaction();
    }

    void QueueCustomizeReads(uint32 guid, CharacterHolderCallback callback)
    {
        SqlQueryHolder* holder = new SqlQueryHolder;
        holder->SetSize(CHAR_CUSTOMIZE_COUNT);
        holder->SetPQuery(CHAR_CUSTOMIZE_AT_LOGIN,
                          "SELECT `at_login` FROM `characters` WHERE `guid` = '%u'", guid);
        holder->SetPQuery(CHAR_CUSTOMIZE_PLAYER_BYTES2,
                          "SELECT `playerBytes2` FROM `characters` WHERE `guid` = '%u'", guid);
        QueueHolder(CharacterDatabase, holder, std::move(callback));
    }

    void SetSlot(uint8 slot, uint32 guid, uint32 accountId)
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `slot` = '%u' WHERE `guid` = '%u' AND `account` = '%u'",
                                   slot, guid, accountId);
    }

    std::optional<uint32> QueryHighestGuid()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT MAX(`guid`) FROM `characters`");
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static CharacterCacheRow ReadCharacterCache(Field* fields)
    {
        CharacterCacheRow row;
        row.guid = fields[0].GetUInt32();
        row.account = fields[1].GetUInt32();
        row.name = fields[2].GetCppString();
        row.race = fields[3].GetUInt8();
        row.classId = fields[4].GetUInt8();
        row.level = fields[5].GetUInt8();
        row.zone = fields[6].GetUInt32();
        row.map = fields[7].GetUInt32();
        row.positionX = fields[8].GetFloat();
        row.positionY = fields[9].GetFloat();
        row.positionZ = fields[10].GetFloat();
        return row;
    }

    std::vector<CharacterCacheRow> QueryCacheRows()
    {
        //                                      0       1         2       3       4        5        6       7      8             9             10
        QueryResult* result = CharacterDatabase.Query("SELECT `guid`, `account`, `name`, `race`, `class`, `level`, `zone`, `map`, `position_x`, `position_y`, `position_z` FROM `characters` ORDER BY `guid`");
        return ReadRows<CharacterCacheRow>(result, ReadCharacterCache);
    }

    std::vector<uint32> QueryAccountsOfCharactersNamed(std::string filter)
    {
        LoginDatabase.escape_string(filter);
        QueryResult* result = CharacterDatabase.PQuery("SELECT `account` FROM `characters` WHERE `name` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), filter.c_str());
        return ReadRows<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static AccountCharacterListRow ReadAccountCharacterListRow(Field* fields)
    {
        AccountCharacterListRow row;
        row.guid = fields[0].GetUInt32();
        row.name = fields[1].GetCppString();
        row.race = fields[2].GetUInt8();
        row.classId = fields[3].GetUInt8();
        row.level = fields[4].GetUInt32();
        return row;
    }

    std::vector<AccountCharacterListRow> QueryAccountCharacterList(uint32 accountId)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `guid`, `name`, `race`, `class`, `level` FROM `characters` WHERE `account` = %u", accountId);
        return ReadRows<AccountCharacterListRow>(result, ReadAccountCharacterListRow);
    }

    void SetCustomizeFlag(uint32 guid)
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `at_login` = `at_login` | '8' WHERE `guid` = '%u'", guid);
    }

    void SetRenameFlag(uint32 guid)
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `at_login` = `at_login` | '1' WHERE `guid` = '%u'", guid);
    }

    void AddAtLoginFlags(uint32 flags, uint32 guid)
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `at_login` = `at_login` | '%u' WHERE `guid` = '%u'", flags, guid);
    }

    void AddAtLoginFlagsToEveryCharacter(uint32 flags)
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `at_login` = `at_login` | '%u' WHERE (`at_login` & '%u') = '0'", flags, flags);
    }

    static DeletedCharacterInfoRow ReadDeletedCharacterInfo(Field* fields)
    {
        DeletedCharacterInfoRow row;
        row.guid = fields[0].GetUInt32();
        row.name = fields[1].GetCppString();
        row.account = fields[2].GetUInt32();
        row.deleteDate = fields[3].GetUInt64();
        return row;
    }

    std::vector<DeletedCharacterInfoRow> QueryDeletedCharacter(uint32 guid)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `guid`, `deleteInfos_Name`, `deleteInfos_Account`, `deleteDate` FROM `characters` WHERE `deleteDate` IS NOT NULL AND `guid` = %u", guid);
        return ReadRows<DeletedCharacterInfoRow>(result, ReadDeletedCharacterInfo);
    }

    std::vector<DeletedCharacterInfoRow> QueryDeletedCharactersNamed(char const* name)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `guid`, `deleteInfos_Name`, `deleteInfos_Account`, `deleteDate` FROM `characters` WHERE `deleteDate` IS NOT NULL AND `deleteInfos_Name` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), name);
        return ReadRows<DeletedCharacterInfoRow>(result, ReadDeletedCharacterInfo);
    }

    std::vector<DeletedCharacterInfoRow> QueryDeletedCharacters()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `guid`, `deleteInfos_Name`, `deleteInfos_Account`, `deleteDate` FROM `characters` WHERE `deleteDate` IS NOT NULL");
        return ReadRows<DeletedCharacterInfoRow>(result, ReadDeletedCharacterInfo);
    }

    void RestoreDeletedCharacter(char const* name, uint32 accountId, uint32 guid)
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `name`='%s', `account`='%u', `deleteDate`=NULL, `deleteInfos_Name`=NULL, `deleteInfos_Account`=NULL WHERE `deleteDate` IS NOT NULL AND `guid` = %u",
                                   name, accountId, guid);
    }

    void SetLevel(uint32 level, uint32 guid)
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `level` = '%u', `xp` = 0 WHERE `guid` = '%u'", level, guid);
    }

    static CharacterPlayerInfoRow ReadPlayerInfo(Field* fields)
    {
        CharacterPlayerInfoRow row;
        row.totaltime = fields[0].GetUInt32();
        row.level = fields[1].GetUInt32();
        row.money = fields[2].GetUInt32();
        row.account = fields[3].GetUInt32();
        return row;
    }

    std::optional<CharacterPlayerInfoRow> QueryPlayerInfo(uint32 guid)
    {
        //                                      0          1      2      3
        QueryResult* result = CharacterDatabase.PQuery("SELECT `totaltime`, `level`, `money`, `account` FROM `characters` WHERE `guid` = '%u'", guid);
        return ReadRow<CharacterPlayerInfoRow>(result, ReadPlayerInfo);
    }

    std::vector<uint32> QueryAccountOfCharacterNamed(std::string const& name)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `account` FROM `characters` WHERE `name` = '%s'", name.c_str());
        return ReadRows<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    void QueryAccountCharacterCountAsync(uint32 accountId, std::function<void(std::optional<uint32>)> callback)
    {
        CharacterDatabase.AsyncPQuery([callback = std::move(callback)](QueryResult* result)
                                      {
                                          callback(ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); }));
                                      },
                                      "SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'", accountId);
    }

    bool QueryCharacterCountInGuidRange(uint32 first, uint32 last, std::function<void(std::optional<uint32>)> callback)
    {
        return CharacterDatabase.AsyncPQuery([callback = std::move(callback)](QueryResult* result)
                                             {
                                                 callback(ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); }));
                                             },
                                             "SELECT COUNT(*) FROM `characters` WHERE `guid` BETWEEN %u AND %u",
                                             first, last);
    }

    void SetEveryCharacterOffline()
    {
        CharacterDatabase.Execute("UPDATE `characters` SET `online` = 0 WHERE `online` <> 0");
    }

    static LoginSkillRow ReadLoginSkill(Field* fields)
    {
        LoginSkillRow row;
        row.skill = fields[0].GetUInt16();
        row.value = fields[1].GetUInt16();
        row.max = fields[2].GetUInt16();
        return row;
    }

    std::vector<LoginSkillRow> LoginSkills(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADSKILLS);
        return ReadRows<LoginSkillRow>(result, ReadLoginSkill);
    }

    std::optional<uint32> LoginGroup(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADGROUP);
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static LoginHomeBindRow ReadLoginHomeBind(Field* fields)
    {
        LoginHomeBindRow row;
        row.map = fields[0].GetUInt32();
        row.zone = fields[1].GetUInt16();
        row.positionX = fields[2].GetFloat();
        row.positionY = fields[3].GetFloat();
        row.positionZ = fields[4].GetFloat();
        return row;
    }

    std::optional<LoginHomeBindRow> LoginHomeBind(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADHOMEBIND);
        return ReadRow<LoginHomeBindRow>(result, ReadLoginHomeBind);
    }

    static LoginArenaTeamInfoRow ReadLoginArenaTeamInfo(Field* fields)
    {
        LoginArenaTeamInfoRow row;
        row.arenateamid = fields[0].GetUInt32();
        row.playedWeek = fields[1].GetUInt32();
        row.playedSeason = fields[2].GetUInt32();
        row.wonsSeason = fields[3].GetUInt32();
        row.personalRating = fields[4].GetUInt32();
        return row;
    }

    std::vector<LoginArenaTeamInfoRow> LoginArenaTeamInfo(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADARENAINFO);
        return ReadRows<LoginArenaTeamInfoRow>(result, ReadLoginArenaTeamInfo);
    }

    static LoginBoundInstanceRow ReadLoginBoundInstance(Field* fields)
    {
        LoginBoundInstanceRow row;
        row.id = fields[0].GetUInt32();
        row.permanent = fields[1].GetBool();
        row.map = fields[2].GetUInt32();
        row.difficulty = fields[3].GetUInt8();
        row.resettime = fields[4].GetUInt64();
        return row;
    }

    std::vector<LoginBoundInstanceRow> LoginBoundInstances(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES);
        return ReadRows<LoginBoundInstanceRow>(result, ReadLoginBoundInstance);
    }

    static LoginBattlegroundDataRow ReadLoginBattlegroundData(Field* fields)
    {
        LoginBattlegroundDataRow row;
        row.instanceId = fields[0].GetUInt32();
        row.team = fields[1].GetUInt32();
        row.joinX = fields[2].GetFloat();
        row.joinY = fields[3].GetFloat();
        row.joinZ = fields[4].GetFloat();
        row.joinO = fields[5].GetFloat();
        row.joinMap = fields[6].GetUInt32();
        row.taxiStart = fields[7].GetUInt32();
        row.taxiEnd = fields[8].GetUInt32();
        row.mountSpell = fields[9].GetUInt32();
        return row;
    }

    std::optional<LoginBattlegroundDataRow> LoginBGData(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADBGDATA);
        return ReadRow<LoginBattlegroundDataRow>(result, ReadLoginBattlegroundData);
    }

    static LoginMailRow ReadLoginMail(Field* fields)
    {
        LoginMailRow row;
        row.id = fields[0].GetUInt32();
        row.messageType = fields[1].GetUInt8();
        row.sender = fields[2].GetUInt32();
        row.receiver = fields[3].GetUInt32();
        row.subject = fields[4].GetCppString();
        row.body = fields[5].GetCppString();
        row.expireTime = fields[6].GetUInt64();
        row.deliverTime = fields[7].GetUInt64();
        row.money = fields[8].GetUInt32();
        row.cod = fields[9].GetUInt32();
        row.checked = fields[10].GetUInt32();
        row.stationery = fields[11].GetUInt8();
        row.mailTemplateId = fields[12].GetInt16();
        row.hasItems = fields[13].GetBool();
        return row;
    }

    std::vector<LoginMailRow> LoginMails(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADMAILS);
        return ReadRows<LoginMailRow>(result, ReadLoginMail);
    }

    static LoginCufProfileRow ReadLoginCufProfile(Field* fields)
    {
        LoginCufProfileRow row;
        row.id = fields[0].GetUInt8();
        row.name = fields[1].GetCppString();
        row.frameHeight = fields[2].GetUInt16();
        row.frameWidth = fields[3].GetUInt16();
        row.sortBy = fields[4].GetUInt8();
        row.healthText = fields[5].GetUInt8();
        row.boolOptions = fields[6].GetUInt32();
        row.topPoint = fields[7].GetUInt8();
        row.bottomPoint = fields[8].GetUInt8();
        row.leftPoint = fields[9].GetUInt8();
        row.topOffset = fields[10].GetUInt16();
        row.bottomOffset = fields[11].GetUInt16();
        row.leftOffset = fields[12].GetUInt16();
        return row;
    }

    std::vector<LoginCufProfileRow> LoginCUFProfiles(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADCUFPROFILES);
        return ReadRows<LoginCufProfileRow>(result, ReadLoginCufProfile);
    }

    static LoginSpellRow ReadLoginSpell(Field* fields)
    {
        LoginSpellRow row;
        row.spell = fields[0].GetUInt32();
        row.active = fields[1].GetBool();
        row.disabled = fields[2].GetBool();
        return row;
    }

    std::vector<LoginSpellRow> LoginSpells(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADSPELLS);
        return ReadRows<LoginSpellRow>(result, ReadLoginSpell);
    }

    static LoginDailyQuestStatusRow ReadLoginDailyQuestStatus(Field* fields)
    {
        LoginDailyQuestStatusRow row;
        row.quest = fields[0].GetUInt32();
        return row;
    }

    std::vector<LoginDailyQuestStatusRow> LoginDailyQuestStatus(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS);
        return ReadRows<LoginDailyQuestStatusRow>(result, ReadLoginDailyQuestStatus);
    }

    static LoginActionRow ReadLoginAction(Field* fields)
    {
        LoginActionRow row;
        row.spec = fields[0].GetUInt8();
        row.button = fields[1].GetUInt8();
        row.action = fields[2].GetUInt32();
        row.type = fields[3].GetUInt8();
        return row;
    }

    std::vector<LoginActionRow> LoginActions(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADACTIONS);
        return ReadRows<LoginActionRow>(result, ReadLoginAction);
    }

    static LoginDeclinedNameRow ReadLoginDeclinedName(Field* fields)
    {
        LoginDeclinedNameRow row;
        for (int i = 0; i < LOGIN_DECLINED_NAME_ROW_CASES; ++i)
        {
            row.name[i] = fields[i].GetCppString();
        }
        return row;
    }

    std::optional<LoginDeclinedNameRow> LoginDeclinedNames(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES);
        return ReadRow<LoginDeclinedNameRow>(result, ReadLoginDeclinedName);
    }

    static LoginEquipmentSetRow ReadLoginEquipmentSet(Field* fields)
    {
        LoginEquipmentSetRow row;
        row.setGuid = fields[0].GetUInt64();
        row.setIndex = fields[1].GetUInt32();
        row.name = fields[2].GetCppString();
        row.iconName = fields[3].GetCppString();
        row.ignoreMask = fields[4].GetUInt32();
        for (int i = 0; i < LOGIN_EQUIPMENT_SET_ROW_ITEMS; ++i)
        {
            row.items[i] = fields[5 + i].GetUInt32();
        }
        return row;
    }

    std::vector<LoginEquipmentSetRow> LoginEquipmentSets(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADEQUIPMENTSETS);
        return ReadRows<LoginEquipmentSetRow>(result, ReadLoginEquipmentSet);
    }

    static LoginAuraRow ReadLoginAura(Field* fields)
    {
        LoginAuraRow row;
        row.casterGuid = fields[0].GetUInt64();
        row.itemGuid = fields[1].GetUInt32();
        row.spell = fields[2].GetUInt32();
        row.stackCount = fields[3].GetUInt32();
        row.remainCharges = fields[4].GetUInt32();
        for (int i = 0; i < LOGIN_AURA_ROW_EFFECTS; ++i)
        {
            row.basePoints[i] = fields[i + 5].GetInt32();
            row.periodicTime[i] = fields[i + 8].GetUInt32();
        }
        row.maxDuration = fields[11].GetInt32();
        row.remainTime = fields[12].GetInt32();
        row.effIndexMask = fields[13].GetUInt32();
        return row;
    }

    std::vector<LoginAuraRow> LoginAuras(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADAURAS);
        return ReadRows<LoginAuraRow>(result, ReadLoginAura);
    }

    static LoginInventoryRow ReadLoginInventory(Field* fields)
    {
        LoginInventoryRow row;
        row.instance = ItemRepository::ReadInstance(fields);
        row.bag = fields[2].GetUInt32();
        row.slot = fields[3].GetUInt8();
        row.item = fields[4].GetUInt32();
        row.itemTemplate = fields[5].GetUInt32();
        return row;
    }

    std::vector<LoginInventoryRow> LoginInventory(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADINVENTORY);
        return ReadRows<LoginInventoryRow>(result, ReadLoginInventory);
    }

    static LoginMailedItemRow ReadLoginMailedItem(Field* fields)
    {
        LoginMailedItemRow row;
        row.instance = ItemRepository::ReadInstance(fields);
        row.mailId = fields[2].GetUInt32();
        row.itemGuid = fields[3].GetUInt32();
        row.itemTemplate = fields[4].GetUInt32();
        return row;
    }

    std::vector<LoginMailedItemRow> LoginMailedItems(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADMAILEDITEMS);
        return ReadRows<LoginMailedItemRow>(result, ReadLoginMailedItem);
    }

    static LoginItemLootRow ReadLoginItemLoot(Field* fields)
    {
        LoginItemLootRow row;
        row.itemGuid = fields[0].GetUInt32();
        row.itemId = fields[1].GetInt32();
        row.amount = fields[2].GetUInt32();
        row.suffix = fields[3].GetUInt32();
        row.property = fields[4].GetInt32();
        return row;
    }

    std::vector<LoginItemLootRow> LoginItemLoot(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADITEMLOOT);
        return ReadRows<LoginItemLootRow>(result, ReadLoginItemLoot);
    }

    static LoginCharacterRow ReadLoginCharacter(Field* fields)
    {
        LoginCharacterRow row;
        row.account = fields[1].GetUInt32();
        row.name = fields[2].GetCppString();
        row.race = fields[3].GetUInt8();
        row.classId = fields[4].GetUInt8();
        row.gender = fields[5].GetUInt8();
        row.level = fields[6].GetUInt8();
        row.xp = fields[7].GetUInt32();
        row.money = fields[8].GetUInt64();
        row.playerBytes = fields[9].GetUInt32();
        row.playerBytes2 = fields[10].GetUInt32();
        row.playerFlags = fields[11].GetUInt32();
        row.positionX = fields[12].GetFloat();
        row.positionY = fields[13].GetFloat();
        row.positionZ = fields[14].GetFloat();
        row.map = fields[15].GetUInt32();
        row.orientation = fields[16].GetFloat();
        if (char const* taximask = fields[17].GetString())
        {
            row.taximask = taximask;
        }
        row.cinematic = fields[18].GetUInt32();
        row.totaltime = fields[19].GetUInt32();
        row.leveltime = fields[20].GetUInt32();
        row.restBonus = fields[21].GetFloat();
        row.logoutTime = fields[22].GetUInt64();
        row.isLogoutResting = fields[23].GetInt32();
        row.resettalentsCost = fields[24].GetUInt32();
        row.resettalentsTime = fields[25].GetUInt64();
        row.primaryTrees = fields[26].GetString();
        row.transX = fields[27].GetFloat();
        row.transY = fields[28].GetFloat();
        row.transZ = fields[29].GetFloat();
        row.transO = fields[30].GetFloat();
        row.transguid = fields[31].GetUInt32();
        row.extraFlags = fields[32].GetUInt32();
        row.stableSlots = fields[33].GetUInt32();
        row.atLogin = fields[34].GetUInt32();
        row.deathExpireTime = fields[37].GetUInt64();
        row.taxiPath = fields[38].GetCppString();
        row.dungeonDifficulty = fields[39].GetUInt32();
        row.totalKills = fields[40].GetUInt32();
        row.todayKills = fields[41].GetUInt16();
        row.yesterdayKills = fields[42].GetUInt16();
        row.chosenTitle = fields[43].GetUInt32();
        row.watchedFaction = fields[44].GetInt32();
        row.drunk = fields[45].GetUInt8();
        row.health = fields[46].GetUInt32();
        for (int i = 0; i < LOGIN_CHARACTER_ROW_POWERS; ++i)
        {
            row.power[i] = fields[47 + i].GetUInt32();
        }
        row.specCount = fields[52].GetUInt8();
        row.activeSpec = fields[53].GetUInt8();
        if (char const* exploredZones = fields[54].GetString())
        {
            row.exploredZones = exploredZones;
        }
        if (char const* knownTitles = fields[56].GetString())
        {
            row.knownTitles = knownTitles;
        }
        row.actionBars = fields[57].GetUInt8();
        row.slot = fields[58].GetUInt8();
        row.createdDate = fields[59].GetUInt32();
        return row;
    }

    std::optional<LoginCharacterRow> LoginCharacter(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADFROM);
        return ReadRow<LoginCharacterRow>(result, ReadLoginCharacter);
    }

    static LoginGuildRow ReadLoginGuild(Field* fields)
    {
        LoginGuildRow row;
        row.guildId = fields[0].GetUInt32();
        row.rank = fields[1].GetUInt32();
        return row;
    }

    std::optional<LoginGuildRow> LoginGuild(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADGUILD);
        return ReadRow<LoginGuildRow>(result, ReadLoginGuild);
    }

    static LoginAchievementRow ReadLoginAchievement(Field* fields)
    {
        LoginAchievementRow row;
        row.achievement = fields[0].GetUInt32();
        row.date = fields[1].GetUInt64();
        return row;
    }

    std::vector<LoginAchievementRow> LoginAchievements(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADACHIEVEMENTS);
        return ReadRows<LoginAchievementRow>(result, ReadLoginAchievement);
    }

    static LoginCriteriaProgressRow ReadLoginCriteriaProgress(Field* fields)
    {
        LoginCriteriaProgressRow row;
        row.criteria = fields[0].GetUInt32();
        row.counter = fields[1].GetUInt32();
        row.date = fields[2].GetUInt64();
        return row;
    }

    std::vector<LoginCriteriaProgressRow> LoginCriteriaProgress(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADCRITERIAPROGRESS);
        return ReadRows<LoginCriteriaProgressRow>(result, ReadLoginCriteriaProgress);
    }

    static LoginPetRow ReadLoginPet(Field* fields)
    {
        LoginPetRow row;
        row.id = fields[0].GetUInt32();
        row.entry = fields[1].GetUInt32();
        row.owner = fields[2].GetUInt32();
        row.modelId = fields[3].GetUInt32();
        row.level = fields[4].GetUInt32();
        row.exp = fields[5].GetUInt32();
        row.reactState = fields[6].GetUInt8();
        row.slot = fields[7].GetUInt32();
        row.name = fields[8].GetCppString();
        row.renamed = fields[9].GetUInt8();
        row.curHealth = fields[10].GetUInt32();
        row.curMana = fields[11].GetUInt32();
        row.abData = fields[12].GetCppString();
        row.saveTime = fields[13].GetUInt64();
        row.resetTalentsCost = fields[14].GetUInt32();
        row.resetTalentsTime = fields[15].GetUInt64();
        row.createdBySpell = fields[16].GetUInt32();
        row.petType = fields[17].GetUInt8();
        return row;
    }

    std::vector<LoginPetRow> LoginPets(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADPETS);
        return ReadRows<LoginPetRow>(result, ReadLoginPet);
    }

    static LoginPetAuraRow ReadLoginPetAura(Field* fields)
    {
        LoginPetAuraRow row;
        row.petId = fields[0].GetUInt32();
        row.casterGuid = fields[1].GetUInt64();
        row.itemGuid = fields[2].GetUInt32();
        row.spell = fields[3].GetUInt32();
        row.stackCount = fields[4].GetUInt32();
        row.remainCharges = fields[5].GetUInt32();
        for (int i = 0; i < LOGIN_AURA_ROW_EFFECTS; ++i)
        {
            row.basePoints[i] = fields[i + 6].GetInt32();
            row.periodicTime[i] = fields[i + 9].GetUInt32();
        }
        row.maxDuration = fields[12].GetInt32();
        row.remainTime = fields[13].GetInt32();
        row.effIndexMask = fields[14].GetUInt32();
        return row;
    }

    std::vector<LoginPetAuraRow> LoginPetAuras(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADPETAURAS);
        return ReadRows<LoginPetAuraRow>(result, ReadLoginPetAura);
    }

    static LoginPetSpellRow ReadLoginPetSpell(Field* fields)
    {
        LoginPetSpellRow row;
        row.petId = fields[0].GetUInt32();
        row.spell = fields[1].GetUInt32();
        row.active = fields[2].GetUInt8();
        return row;
    }

    std::vector<LoginPetSpellRow> LoginPetSpells(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADPETSPELLS);
        return ReadRows<LoginPetSpellRow>(result, ReadLoginPetSpell);
    }

    static LoginPetSpellCooldownRow ReadLoginPetSpellCooldown(Field* fields)
    {
        LoginPetSpellCooldownRow row;
        row.petId = fields[0].GetUInt32();
        row.spell = fields[1].GetUInt32();
        row.time = fields[2].GetUInt64();
        return row;
    }

    std::vector<LoginPetSpellCooldownRow> LoginPetSpellCooldowns(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADPETSPELLCOOLDOWNS);
        return ReadRows<LoginPetSpellCooldownRow>(result, ReadLoginPetSpellCooldown);
    }

    static LoginPetDeclinedNameRow ReadLoginPetDeclinedName(Field* fields)
    {
        LoginPetDeclinedNameRow row;
        row.petId = fields[0].GetUInt32();
        for (int i = 0; i < LOGIN_DECLINED_NAME_ROW_CASES; ++i)
        {
            row.name[i] = fields[i + 1].GetCppString();
        }
        return row;
    }

    std::vector<LoginPetDeclinedNameRow> LoginPetDeclinedNames(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADPETDECLINEDNAMES);
        return ReadRows<LoginPetDeclinedNameRow>(result, ReadLoginPetDeclinedName);
    }

    std::optional<uint32> CreateAccountCharacterCount(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(CHAR_CREATE_ACCOUNT_REALM_CHARS);
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    std::optional<uint8> CreateRealmCharacterCount(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(CHAR_CREATE_REALM_CHAR_COUNT);
        return ReadRow<uint8>(result, [](Field* fields) { return fields[0].GetUInt8(); });
    }

    static CreateAccountCharacterRow ReadCreateAccountCharacter(Field* fields)
    {
        CreateAccountCharacterRow row;
        row.level = fields[0].GetUInt32();
        row.race = fields[1].GetUInt32();
        row.classId = fields[2].GetUInt32();
        return row;
    }

    std::vector<CreateAccountCharacterRow> CreateAccountCharacters(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(CHAR_CREATE_REALM_ACCOUNT_CHARS);
        return ReadRows<CreateAccountCharacterRow>(result, ReadCreateAccountCharacter);
    }

    std::optional<uint32> CreateHighestPetId(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(CHAR_CREATE_REALM_PET_ID);
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    std::optional<uint32> CustomizeAtLogin(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(CHAR_CUSTOMIZE_AT_LOGIN);
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    std::optional<uint32> CustomizePlayerBytes2(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(CHAR_CUSTOMIZE_PLAYER_BYTES2);
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }
}
