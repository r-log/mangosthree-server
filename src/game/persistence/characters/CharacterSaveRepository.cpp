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

#include "CharacterSaveRepository.h"
#include "Database/DatabaseEnv.h"

#include <sstream>

namespace CharacterSaveRepository
{
    void ReplaceCharacter(CharacterSaveRow const& row)
    {
        static SqlStatementID delChar ;
        static SqlStatementID insChar ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delChar, "DELETE FROM `characters` WHERE `guid` = ?");
        stmt.PExecute(row.guid);

        SqlStatement uberInsert = CharacterDatabase.CreateStatement(insChar, "INSERT INTO `characters` (`guid`,`account`,`name`,`race`,`class`,`gender`, "
                                  "`level`,`xp`,`money`,`playerBytes`,`playerBytes2`,`playerFlags`,"
                                  "`map`, `dungeon_difficulty`, `position_x`, `position_y`, `position_z`, `orientation`, "
                                  "`taximask`, `online`, `cinematic`, "
                                  "`totaltime`, `leveltime`, `rest_bonus`, `logout_time`, `is_logout_resting`, `resettalents_cost`, `resettalents_time`, `primary_trees`, "
                                  "`trans_x`, `trans_y`, `trans_z`, `trans_o`, `transguid`, `extra_flags`, `stable_slots`, `at_login`, `zone`, "
                                  "`death_expire_time`, `taxi_path`, `totalKills`, "
                                  "`todayKills`, `yesterdayKills`, `chosenTitle`, "
                                  "`watchedFaction`, `drunk`, `health`, `power1`, `power2`, `power3`, "
                                  "`power4`, `power5`, `specCount`, `activeSpec`, `exploredZones`, `equipmentCache`, `knownTitles`, "
                                  "`actionBars`, `slot`,   `deleteInfos_Account`, `deleteInfos_Name`, `deleteDate`, `createdDate`) "
                                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                                  "?, ?, ?, ?, ?, ?, "
                                  "?, ?, ?, "
                                  "?, ?, ?, ?, ?, ?, ?, ?, "
                                  "?, ?, ?, ?, ?, ?, ?, ?, ?, "
                                  "?, ?, ?, "
                                  "?, ?, ?, ?, ?, ?, ?, ?, ?, "
                                  "?, ?, ?, ?, ?, ?, ?, "
                                  "?, ?, null, null, null, ?) ");

        uberInsert.addUInt32(row.guid);
        uberInsert.addUInt32(row.account);
        uberInsert.addString(row.name);
        uberInsert.addUInt8(row.race);
        uberInsert.addUInt8(row.playerClass);
        uberInsert.addUInt8(row.gender);
        uberInsert.addUInt32(row.level);
        uberInsert.addUInt32(row.xp);
        uberInsert.addUInt64(row.money);
        uberInsert.addUInt32(row.playerBytes);
        uberInsert.addUInt32(row.playerBytes2);
        uberInsert.addUInt32(row.playerFlags);
        uberInsert.addUInt32(row.map);
        uberInsert.addUInt32(row.dungeonDifficulty);
        uberInsert.addFloat(row.positionX);
        uberInsert.addFloat(row.positionY);
        uberInsert.addFloat(row.positionZ);
        uberInsert.addFloat(row.orientation);
        uberInsert.addString(row.taxiMask);
        uberInsert.addUInt32(row.online);
        uberInsert.addUInt32(row.cinematic);
        uberInsert.addUInt32(row.totalTime);
        uberInsert.addUInt32(row.levelTime);
        uberInsert.addFloat(row.restBonus);
        uberInsert.addUInt64(row.logoutTime);
        uberInsert.addUInt32(row.isLogoutResting);
        uberInsert.addUInt32(row.resetTalentsCost);
        uberInsert.addUInt64(row.resetTalentsTime);
        uberInsert.addString(row.primaryTrees);
        uberInsert.addFloat(row.transX);
        uberInsert.addFloat(row.transY);
        uberInsert.addFloat(row.transZ);
        uberInsert.addFloat(row.transO);
        uberInsert.addUInt32(row.transGuid);
        uberInsert.addUInt32(row.extraFlags);
        uberInsert.addUInt32(row.stableSlots);
        uberInsert.addUInt32(row.atLogin);
        uberInsert.addUInt32(row.zone);
        uberInsert.addUInt64(row.deathExpireTime);
        uberInsert.addString(row.taxiPath);
        uberInsert.addUInt32(row.totalKills);
        uberInsert.addUInt16(row.todayKills);
        uberInsert.addUInt16(row.yesterdayKills);
        uberInsert.addUInt32(row.chosenTitle);
        uberInsert.addUInt32(row.watchedFaction);
        uberInsert.addUInt8(row.drunk);
        uberInsert.addUInt32(row.health);
        for (int i = 0; i < CHARACTER_SAVE_ROW_POWERS; ++i)
        {
            uberInsert.addUInt32(row.power[i]);
        }
        uberInsert.addUInt32(row.specCount);
        uberInsert.addUInt32(row.activeSpec);
        uberInsert.addString(row.exploredZones);
        uberInsert.addString(row.equipmentCache);
        uberInsert.addString(row.knownTitles);
        uberInsert.addUInt32(row.actionBars);
        uberInsert.addUInt8(row.slot);
        uberInsert.addUInt32(row.createdDate);

        uberInsert.Execute();
    }

    void SetMoney(uint64 money, uint32 guid)
    {
        static SqlStatementID updateGold ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(updateGold, "UPDATE `characters` SET `money` = ? WHERE `guid` = ?");
        stmt.PExecute(money, guid);
    }

    std::string SetPosition(uint32 guid, uint32 map, float x, float y, float z, float o, uint32 zone)
    {
        std::ostringstream ss;
        ss << "UPDATE `characters` SET `position_x`='" << x << "',`position_y`='" << y
           << "',`position_z`='" << z << "',`orientation`='" << o << "',`map`='" << map
           << "',`zone`='" << zone << "',`trans_x`='0',`trans_y`='0',`trans_z`='0',"
           << "`transguid`='0',`taxi_path`='' WHERE `guid`='" << guid << "'";
        CharacterDatabase.Execute(ss.str().c_str());
        return ss.str();
    }

    void InsertAction(uint32 guid, uint32 spec, uint32 button, uint32 action, uint32 type)
    {
        static SqlStatementID insertAction ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insertAction, "INSERT INTO `character_action` (`guid`,`spec`, `button`,`action`,`type`) VALUES (?, ?, ?, ?, ?)");
        stmt.addUInt32(guid);
        stmt.addUInt32(spec);
        stmt.addUInt32(button);
        stmt.addUInt32(action);
        stmt.addUInt32(type);
        stmt.Execute();
    }

    void UpdateAction(uint32 guid, uint32 spec, uint32 button, uint32 action, uint32 type)
    {
        static SqlStatementID updateAction ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updateAction, "UPDATE `character_action` SET `action` = ?, `type` = ? WHERE `guid` = ? AND `button` = ? AND `spec` = ?");
        stmt.addUInt32(action);
        stmt.addUInt32(type);
        stmt.addUInt32(guid);
        stmt.addUInt32(button);
        stmt.addUInt32(spec);
        stmt.Execute();
    }

    void DeleteAction(uint32 guid, uint32 spec, uint32 button)
    {
        static SqlStatementID deleteAction ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(deleteAction, "DELETE FROM `character_action` WHERE `guid` = ? AND `button` = ? AND `spec` = ?");
        stmt.addUInt32(guid);
        stmt.addUInt32(button);
        stmt.addUInt32(spec);
        stmt.Execute();
    }

    void ReplaceAuras(uint32 guid, std::vector<CharacterAuraSaveRow> const& auras)
    {
        static SqlStatementID deleteAuras ;
        static SqlStatementID insertAuras ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(deleteAuras, "DELETE FROM `character_aura` WHERE `guid` = ?");
        stmt.PExecute(guid);

        if (auras.empty())
        {
            return;
        }

        stmt = CharacterDatabase.CreateStatement(insertAuras, "INSERT INTO `character_aura` (`guid`, `caster_guid`, `item_guid`, `spell`, `stackcount`, `remaincharges`, "
                "`basepoints0`, `basepoints1`, `basepoints2`, `periodictime0`, `periodictime1`, `periodictime2`, `maxduration`, `remaintime`, `effIndexMask`) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        for (std::vector<CharacterAuraSaveRow>::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
        {
            stmt.addUInt32(guid);
            stmt.addUInt64(itr->casterGuid);
            stmt.addUInt32(itr->itemGuid);
            stmt.addUInt32(itr->spell);
            stmt.addUInt32(itr->stackCount);
            stmt.addUInt8(itr->remainCharges);

            for (int i = 0; i < CHARACTER_AURA_ROW_EFFECTS; ++i)
            {
                stmt.addInt32(itr->basePoints[i]);
            }

            for (int i = 0; i < CHARACTER_AURA_ROW_EFFECTS; ++i)
            {
                stmt.addUInt32(itr->periodicTime[i]);
            }

            stmt.addInt32(itr->maxDuration);
            stmt.addInt32(itr->remainTime);
            stmt.addUInt32(itr->effIndexMask);
            stmt.Execute();
        }
    }

    void ReplaceCUFProfiles(uint32 guid, std::vector<CUFProfileSaveRow> const& profiles)
    {
        static SqlStatementID delProfiles;
        static SqlStatementID insProfile;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delProfiles,
                            "DELETE FROM `character_cuf_profiles` WHERE `guid` = ?");
        stmt.PExecute(guid);

        for (std::vector<CUFProfileSaveRow>::const_iterator itr = profiles.begin(); itr != profiles.end(); ++itr)
        {
            stmt = CharacterDatabase.CreateStatement(insProfile,
                   "INSERT INTO `character_cuf_profiles` (`guid`,`id`,`name`,`frameHeight`,`frameWidth`,`sortBy`,"
                   "`healthText`,`boolOptions`,`topPoint`,`bottomPoint`,`leftPoint`,`topOffset`,`bottomOffset`,`leftOffset`) "
                   "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
            stmt.addUInt32(guid);
            stmt.addUInt8(itr->id);
            stmt.addString(itr->name);
            stmt.addUInt16(itr->frameHeight);
            stmt.addUInt16(itr->frameWidth);
            stmt.addUInt8(itr->sortBy);
            stmt.addUInt8(itr->healthText);
            stmt.addUInt32(itr->boolOptions);
            stmt.addUInt8(itr->topPoint);
            stmt.addUInt8(itr->bottomPoint);
            stmt.addUInt8(itr->leftPoint);
            stmt.addUInt16(itr->topOffset);
            stmt.addUInt16(itr->bottomOffset);
            stmt.addUInt16(itr->leftOffset);
            stmt.Execute();
        }
    }

    void InsertSkill(uint32 guid, uint32 skill, uint16 value, uint16 max)
    {
        static SqlStatementID insSkills ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insSkills, "INSERT INTO `character_skills` (`guid`, `skill`, `value`, `max`) VALUES (?, ?, ?, ?)");
        stmt.PExecute(guid, skill, value, max);
    }

    void UpdateSkill(uint32 guid, uint32 skill, uint16 value, uint16 max)
    {
        static SqlStatementID updSkills ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updSkills, "UPDATE `character_skills` SET `value` = ?, `max` = ? WHERE `guid` = ? AND `skill` = ?");
        stmt.PExecute(value, max, guid, skill);
    }

    void DeleteSkill(uint32 guid, uint32 skill)
    {
        static SqlStatementID delSkills ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(delSkills, "DELETE FROM `character_skills` WHERE `guid` = ? AND `skill` = ?");
        stmt.PExecute(guid, skill);
    }

    void InsertSpell(uint32 guid, uint32 spell, uint8 active, uint8 disabled)
    {
        static SqlStatementID insSpells ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insSpells, "INSERT INTO `character_spell` (`guid`,`spell`,`active`,`disabled`) VALUES (?, ?, ?, ?)");
        stmt.PExecute(guid, spell, active, disabled);
    }

    void DeleteSpell(uint32 guid, uint32 spell)
    {
        static SqlStatementID delSpells ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(delSpells, "DELETE FROM `character_spell` WHERE `guid` = ? and `spell` = ?");
        stmt.PExecute(guid, spell);
    }

    void DeleteSpellOfEveryCharacter(uint32 spell)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_spell` WHERE `spell` = '%u'", spell);
    }

    void DeleteInvalidSkill(uint32 guid, uint32 skill)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_skills` WHERE `guid` = '%u' AND `skill` = '%u' ", guid, skill);
    }

    void DeleteHomebind(uint32 guid)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_homebind` WHERE `guid` = '%u'", guid);
    }

    void InsertHomebind(uint32 guid, uint32 map, uint32 area, float x, float y, float z)
    {
        CharacterDatabase.PExecute("INSERT INTO `character_homebind` (`guid`,`map`,`zone`,`position_x`,`position_y`,`position_z`) VALUES ('%u', '%u', '%u', '%f', '%f', '%f')", guid, map, area, x, y, z);
    }

    void UpdateHomebind(uint32 guid, uint32 map, uint32 area, float x, float y, float z)
    {
        CharacterDatabase.PExecute("UPDATE `character_homebind` SET `map` = '%u', `zone` = '%u', `position_x` = '%f', `position_y` = '%f', `position_z` = '%f' WHERE `guid` = '%u'",
                                   map, area, x, y, z, guid);
    }

    void ReplaceStats(uint32 guid, CharacterStatsSaveRow const& row)
    {
        static SqlStatementID delStats ;
        static SqlStatementID insertStats ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delStats, "DELETE FROM `character_stats` WHERE `guid` = ?");
        stmt.PExecute(guid);

        stmt = CharacterDatabase.CreateStatement(insertStats, "INSERT INTO `character_stats` (`guid`, `maxhealth`, `maxpower1`, `maxpower2`, `maxpower3`, `maxpower4`, `maxpower5`,"
                "`strength`, `agility`, `stamina`, `intellect`, `spirit`, `armor`, `resHoly`, `resFire`, `resNature`, `resFrost`, `resShadow`, `resArcane`, "
                "`blockPct`, `dodgePct`, `parryPct`, `critPct`, `rangedCritPct`, `spellCritPct`, `attackPower`, `rangedAttackPower`, `spellPower`) "
            "VALUES ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        stmt.addUInt32(guid);
        stmt.addUInt32(row.maxHealth);
        for (int i = 0; i < CHARACTER_SAVE_ROW_POWERS; ++i)
        {
            stmt.addUInt32(row.maxPower[i]);
        }
        for (int i = 0; i < CHARACTER_STATS_ROW_STATS; ++i)
        {
            stmt.addFloat(row.stat[i]);
        }
        for (int i = 0; i < CHARACTER_STATS_ROW_SCHOOLS; ++i)
        {
            stmt.addUInt32(row.resistance[i]);
        }
        stmt.addFloat(row.blockPct);
        stmt.addFloat(row.dodgePct);
        stmt.addFloat(row.parryPct);
        stmt.addFloat(row.critPct);
        stmt.addFloat(row.rangedCritPct);
        stmt.addFloat(row.spellCritPct);
        stmt.addUInt32(row.attackPower);
        stmt.addUInt32(row.rangedAttackPower);
        stmt.addUInt32(row.spellPower);

        stmt.Execute();
    }

    void InsertEquipmentSet(uint32 guid, EquipmentSetSaveRow const& row)
    {
        static SqlStatementID insSets ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insSets, "INSERT INTO `character_equipmentsets` VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        stmt.addUInt32(guid);
        stmt.addUInt64(row.setGuid);
        stmt.addUInt32(row.index);
        stmt.addString(row.name);
        stmt.addString(row.iconName);
        stmt.addUInt32(row.ignoreMask);

        for (int i = 0; i < EQUIPMENT_SET_ROW_ITEMS; ++i)
        {
            stmt.addUInt32(row.items[i]);
        }

        stmt.Execute();
    }

    void UpdateEquipmentSet(uint32 guid, EquipmentSetSaveRow const& row)
    {
        static SqlStatementID updSets ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updSets, "UPDATE `character_equipmentsets` SET `name`=?, `iconname`=?, `ignore_mask`=?, `item0`=?, `item1`=?, `item2`=?, `item3`=?, `item4`=?, "
                            "`item5`=?, `item6`=?, `item7`=?, `item8`=?, `item9`=?, `item10`=?, `item11`=?, `item12`=?, `item13`=?, `item14`=?, "
                            "`item15`=?, `item16`=?, `item17`=?, `item18`=? WHERE `guid`=? AND `setguid`=? AND `setindex`=?");

        stmt.addString(row.name);
        stmt.addString(row.iconName);
        stmt.addUInt32(row.ignoreMask);

        for (int i = 0; i < EQUIPMENT_SET_ROW_ITEMS; ++i)
        {
            stmt.addUInt32(row.items[i]);
        }

        stmt.addUInt32(guid);
        stmt.addUInt64(row.setGuid);
        stmt.addUInt32(row.index);

        stmt.Execute();
    }

    void DeleteEquipmentSet(uint64 setGuid)
    {
        static SqlStatementID delSets ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(delSets, "DELETE FROM `character_equipmentsets` WHERE `setguid` = ?");
        stmt.PExecute(setGuid);
    }

    void ReplaceBattlegroundData(uint32 guid, BattlegroundDataSaveRow const* row)
    {
        static SqlStatementID delBGData ;
        static SqlStatementID insBGData ;

        SqlStatement stmt =  CharacterDatabase.CreateStatement(delBGData, "DELETE FROM `character_battleground_data` WHERE `guid` = ?");

        stmt.PExecute(guid);

        if (row)
        {
            stmt = CharacterDatabase.CreateStatement(insBGData, "INSERT INTO `character_battleground_data` VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
            /* guid, bgInstanceID, bgTeam, x, y, z, o, map, taxi[0], taxi[1], mountSpell */
            stmt.addUInt32(guid);
            stmt.addUInt32(row->instanceId);
            stmt.addUInt32(row->team);
            stmt.addFloat(row->x);
            stmt.addFloat(row->y);
            stmt.addFloat(row->z);
            stmt.addFloat(row->o);
            stmt.addUInt32(row->map);
            stmt.addUInt32(row->taxiStart);
            stmt.addUInt32(row->taxiEnd);
            stmt.addUInt32(row->mountSpell);

            stmt.Execute();
        }
    }
}
