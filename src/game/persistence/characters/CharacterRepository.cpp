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
        if (!database.DelayQueryHolder(std::move(callback), holder))
        {
            delete holder;                                  // delete all unprocessed queries
        }
    }
}

namespace CharacterRepository
{
    QueryResult* QueryAccountCharacters(uint32 accountId)
    {
        return CharacterDatabase.PQuery("SELECT `guid` FROM `characters` WHERE `account`='%u'", accountId);
    }

    QueryResult* QueryAccountCharacterCount(uint32 accountId)
    {
        return CharacterDatabase.PQuery("SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'", accountId);
    }

    QueryResult* QueryPosition(uint32 guid)
    {
        return CharacterDatabase.PQuery("SELECT `position_x`,`position_y`,`position_z`,`orientation`,`map`,`taxi_path` FROM `characters` WHERE `guid` = '%u'", guid);
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

    void QueryEnum(uint32 accountId, uint32 currentPetSlot, bool declinedNames, CharacterReadCallback callback)
    {
        CharacterDatabase.AsyncPQuery(std::move(callback),
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
        return CharacterDatabase.DelayQueryHolder(std::move(callback), holder);
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

    void QueryName(uint32 guid, bool declinedNames, CharacterReadCallback callback)
    {
        CharacterDatabase.AsyncPQuery(std::move(callback),
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

    void QueryRenameTarget(uint32 guid, uint32 accountId, uint32 renameFlag, CharacterReadCallback callback)
    {
        CharacterDatabase.AsyncPQuery(std::move(callback),
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

    QueryResult* QueryCacheRows()
    {
        //                                      0       1         2       3       4        5        6       7      8             9             10
        return CharacterDatabase.Query("SELECT `guid`, `account`, `name`, `race`, `class`, `level`, `zone`, `map`, `position_x`, `position_y`, `position_z` FROM `characters` ORDER BY `guid`");
    }

    QueryResult* QueryAccountsOfCharactersNamed(std::string filter)
    {
        LoginDatabase.escape_string(filter);
        return CharacterDatabase.PQuery("SELECT `account` FROM `characters` WHERE `name` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), filter.c_str());
    }

    QueryResult* QueryAccountCharacterList(uint32 accountId)
    {
        return CharacterDatabase.PQuery("SELECT `guid`, `name`, `race`, `class`, `level` FROM `characters` WHERE `account` = %u", accountId);
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

    QueryResult* QueryDeletedCharacter(uint32 guid)
    {
        return CharacterDatabase.PQuery("SELECT `guid`, `deleteInfos_Name`, `deleteInfos_Account`, `deleteDate` FROM `characters` WHERE `deleteDate` IS NOT NULL AND `guid` = %u", guid);
    }

    QueryResult* QueryDeletedCharactersNamed(char const* name)
    {
        return CharacterDatabase.PQuery("SELECT `guid`, `deleteInfos_Name`, `deleteInfos_Account`, `deleteDate` FROM `characters` WHERE `deleteDate` IS NOT NULL AND `deleteInfos_Name` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), name);
    }

    QueryResult* QueryDeletedCharacters()
    {
        return CharacterDatabase.Query("SELECT `guid`, `deleteInfos_Name`, `deleteInfos_Account`, `deleteDate` FROM `characters` WHERE `deleteDate` IS NOT NULL");
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

    QueryResult* QueryPlayerInfo(uint32 guid)
    {
        //                                      0          1      2      3
        return CharacterDatabase.PQuery("SELECT `totaltime`, `level`, `money`, `account` FROM `characters` WHERE `guid` = '%u'", guid);
    }

    QueryResult* QueryAccountOfCharacterNamed(std::string const& name)
    {
        return CharacterDatabase.PQuery("SELECT `account` FROM `characters` WHERE `name` = '%s'", name.c_str());
    }

    void QueryAccountCharacterCountAsync(uint32 accountId, std::function<void(QueryResult*)> callback)
    {
        CharacterDatabase.AsyncPQuery(std::move(callback),
                                      "SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'", accountId);
    }

    bool QueryCharacterCountInGuidRange(uint32 first, uint32 last, std::function<void(QueryResult*)> callback)
    {
        return CharacterDatabase.AsyncPQuery(std::move(callback),
                                             "SELECT COUNT(*) FROM `characters` WHERE `guid` BETWEEN %u AND %u",
                                             first, last);
    }

    void SetEveryCharacterOffline()
    {
        CharacterDatabase.Execute("UPDATE `characters` SET `online` = 0 WHERE `online` <> 0");
    }
}
