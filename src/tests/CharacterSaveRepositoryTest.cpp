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

/// The character save's repository, the inventory, mail and daily quest writes the save calls, and
/// the load's fixes, instance binds and small character writes, on the global CharacterDatabase
/// backed by fakes: every write in order with its values substituted as the server would send them
/// to MySQL.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/CharacterSaveRepository.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/inventory/ItemRepository.h"
#include "persistence/characters/mail/MailRepository.h"
#include "persistence/characters/maps/InstanceBindRepository.h"
#include "persistence/characters/quests/QuestStatusRepository.h"
#include "persistence/characters/social/TicketRepository.h"

#include <string>
#include <vector>

namespace
{
    /// The fakes on the global character database, asynchronous writes on.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;
        size_t seen = 0;

        Db() : query(CharacterDatabase), async(CharacterDatabase),
            attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            CharacterDatabase.ExecuteQueuedForTest();
            std::vector<std::string> out(async.executed.begin() + seen, async.executed.end());
            seen = async.executed.size();
            return out;
        }
    };

    void CheckLines(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got == want)
        {
            return;
        }
        std::string text = "lines differ (case line " + std::to_string(line) + "): got [";
        for (size_t i = 0; i < got.size(); ++i)
        {
            text += (i ? " | " : "") + got[i];
        }
        text += "] want [";
        for (size_t i = 0; i < want.size(); ++i)
        {
            text += (i ? " | " : "") + want[i];
        }
        testing::ReportFailure(__FILE__, line, text + "]");
    }

    bool StartsWith(std::string const& text, std::string const& prefix)
    {
        return text.compare(0, prefix.size(), prefix) == 0;
    }

    bool EndsWith(std::string const& text, std::string const& suffix)
    {
        return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
}

// The character's row is a DELETE and then the INSERT, both inside the save's transaction, with
// every column bound in order.
TEST(CharacterSaveRepository_CharacterRowJoinsTheSave)
{
    Db db;
    CharacterSaveRow row;
    row.guid = 42;
    row.account = 17;
    row.name = "Bob";
    row.race = 1;
    row.playerClass = 2;
    row.gender = 0;
    row.level = 80;
    row.xp = 5;
    row.money = 1000;
    row.playerBytes = 3;
    row.playerBytes2 = 4;
    row.playerFlags = 6;
    row.slot = 2;
    row.createdDate = 1700000000;
    {
        TickGuard::Scope scope;
        CharacterTransaction::Begin();
        CharacterSaveRepository::ReplaceCharacter(row);
        CharacterTransaction::Commit();
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    std::vector<std::string> got = db.Take();
    REQUIRE(got.size() == size_t(2));
    CHECK_STR(got[0], "DELETE FROM `characters` WHERE `guid` = '42'");
    CHECK(StartsWith(got[1], "INSERT INTO `characters` (`guid`,`account`,`name`,`race`,`class`,`gender`, "));
    CHECK(got[1].find("VALUES ('42', '17', 'Bob', '1', '2', '0', '80', '5', '1000', '3', '4', '6', ") != std::string::npos);
    CHECK(EndsWith(got[1], "'0', '2', null, null, null, '1700000000') "));
    CHECK(got[1].find('?') == std::string::npos);
}

// The save's single-table writes, each in the argument order its text names them.
TEST(CharacterSaveRepository_Writes)
{
    Db db;
    std::vector<CharacterAuraSaveRow> auras(1);
    auras[0].casterGuid = 9;
    auras[0].spell = 774;
    auras[0].stackCount = 1;
    auras[0].remainCharges = 2;
    auras[0].basePoints[0] = 30;
    auras[0].periodicTime[0] = 3000;
    auras[0].maxDuration = 15000;
    auras[0].remainTime = 12000;
    auras[0].effIndexMask = 1;
    std::vector<CUFProfileSaveRow> profiles(1);
    profiles[0].id = 0;
    profiles[0].name = "Raid";
    profiles[0].frameHeight = 36;
    profiles[0].frameWidth = 72;
    EquipmentSetSaveRow set;
    set.setGuid = 5;
    set.index = 1;
    set.name = "Tank";
    set.iconName = "INV_Shield";
    BattlegroundDataSaveRow bg;
    bg.instanceId = 3;
    bg.team = 469;
    bg.x = 1.5f;
    bg.map = 489;
    {
        TickGuard::Scope scope;
        CharacterSaveRepository::SetMoney(2500, 42);
        CharacterSaveRepository::InsertAction(42, 0, 1, 133, 0);
        CharacterSaveRepository::UpdateAction(42, 1, 2, 116, 0);
        CharacterSaveRepository::DeleteAction(42, 0, 3);
        CharacterSaveRepository::ReplaceAuras(42, auras);
        CharacterSaveRepository::ReplaceAuras(43, std::vector<CharacterAuraSaveRow>());
        CharacterSaveRepository::ReplaceCUFProfiles(42, profiles);
        CharacterSaveRepository::InsertSkill(42, 164, 1, 75);
        CharacterSaveRepository::UpdateSkill(42, 164, 2, 75);
        CharacterSaveRepository::DeleteSkill(42, 165);
        CharacterSaveRepository::InsertSpell(42, 133, 1, 0);
        CharacterSaveRepository::DeleteSpell(42, 116);
        CharacterSaveRepository::DeleteEquipmentSet(6);
        CharacterSaveRepository::ReplaceBattlegroundData(42, &bg);
        CharacterSaveRepository::ReplaceBattlegroundData(43, NULL);
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CheckLines(db.Take(), {
        "UPDATE `characters` SET `money` = '2500' WHERE `guid` = '42'",
        "INSERT INTO `character_action` (`guid`,`spec`, `button`,`action`,`type`) VALUES ('42', '0', '1', '133', '0')",
        "UPDATE `character_action` SET `action` = '116', `type` = '0' WHERE `guid` = '42' AND `button` = '2' AND `spec` = '1'",
        "DELETE FROM `character_action` WHERE `guid` = '42' AND `button` = '3' AND `spec` = '0'",
        "DELETE FROM `character_aura` WHERE `guid` = '42'",
        "INSERT INTO `character_aura` (`guid`, `caster_guid`, `item_guid`, `spell`, `stackcount`, `remaincharges`, "
        "`basepoints0`, `basepoints1`, `basepoints2`, `periodictime0`, `periodictime1`, `periodictime2`, `maxduration`, `remaintime`, `effIndexMask`) "
        "VALUES ('42', '9', '0', '774', '1', '2', '30', '0', '0', '3000', '0', '0', '15000', '12000', '1')",
        "DELETE FROM `character_aura` WHERE `guid` = '43'",
        "DELETE FROM `character_cuf_profiles` WHERE `guid` = '42'",
        "INSERT INTO `character_cuf_profiles` (`guid`,`id`,`name`,`frameHeight`,`frameWidth`,`sortBy`,"
        "`healthText`,`boolOptions`,`topPoint`,`bottomPoint`,`leftPoint`,`topOffset`,`bottomOffset`,`leftOffset`) "
        "VALUES ('42','0','Raid','36','72','0','0','0','0','0','0','0','0','0')",
        "INSERT INTO `character_skills` (`guid`, `skill`, `value`, `max`) VALUES ('42', '164', '1', '75')",
        "UPDATE `character_skills` SET `value` = '2', `max` = '75' WHERE `guid` = '42' AND `skill` = '164'",
        "DELETE FROM `character_skills` WHERE `guid` = '42' AND `skill` = '165'",
        "INSERT INTO `character_spell` (`guid`,`spell`,`active`,`disabled`) VALUES ('42', '133', '1', '0')",
        "DELETE FROM `character_spell` WHERE `guid` = '42' and `spell` = '116'",
        "DELETE FROM `character_equipmentsets` WHERE `setguid` = '6'",
        "DELETE FROM `character_battleground_data` WHERE `guid` = '42'",
        "INSERT INTO `character_battleground_data` VALUES ('42', '3', '469', '1.5', '0', '0', '0', '489', '0', '0', '0')",
        "DELETE FROM `character_battleground_data` WHERE `guid` = '43'",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// An equipment set's INSERT names its owner, guid and index first; its UPDATE names them last.
TEST(CharacterSaveRepository_EquipmentSets)
{
    Db db;
    EquipmentSetSaveRow set;
    set.setGuid = 5;
    set.index = 1;
    set.name = "Tank";
    set.iconName = "INV_Shield";
    set.ignoreMask = 2;
    set.items[0] = 100;
    CharacterSaveRepository::InsertEquipmentSet(42, set);
    CharacterSaveRepository::UpdateEquipmentSet(42, set);
    std::vector<std::string> got = db.Take();
    REQUIRE(got.size() == size_t(2));
    CHECK_STR(got[0], "INSERT INTO `character_equipmentsets` VALUES ('42', '5', '1', 'Tank', 'INV_Shield', '2', '100', "
                      "'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0')");
    CHECK(StartsWith(got[1], "UPDATE `character_equipmentsets` SET `name`='Tank', `iconname`='INV_Shield', `ignore_mask`='2', `item0`='100', "));
    CHECK(EndsWith(got[1], "`item18`='0' WHERE `guid`='42' AND `setguid`='5' AND `setindex`='1'"));
}

// An offline character's position is written through a statement the repository returns.
TEST(CharacterSaveRepository_Position)
{
    Db db;
    std::string statement = CharacterSaveRepository::SetPosition(42, 0, 1.5f, -2.25f, 3.0f, 0.5f, 12);
    CHECK_STR(statement, "UPDATE `characters` SET `position_x`='1.5',`position_y`='-2.25',`position_z`='3',`orientation`='0.5',`map`='0',"
                         "`zone`='12',`trans_x`='0',`trans_y`='0',`trans_z`='0',`transguid`='0',`taxi_path`='' WHERE `guid`='42'");
    CheckLines(db.Take(), { statement }, __LINE__);
}

// The inventory rows, the mail state and the daily quests the save writes.
TEST(CharacterSaveRepository_InventoryMailAndDailies)
{
    Db db;
    {
        TickGuard::Scope scope;
        ItemRepository::InsertInventoryRow(42, 0, 23, 77, 6948);
        ItemRepository::UpdateInventoryRow(42, 78, 1, 77, 6948);
        MailRepository::UpdateState(7, 1, 1700000000, 1690000000, 50, 0, 1);
        MailRepository::DeleteTakenItems({ 80, 81 });
        MailRepository::DeleteDeletedMail(8);
        QuestStatusRepository::SaveDaily(42, { 11, 12 });
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CheckLines(db.Take(), {
        "INSERT INTO `character_inventory` (`guid`,`bag`,`slot`,`item`,`item_template`) VALUES ('42', '0', '23', '77', '6948')",
        "UPDATE `character_inventory` SET `guid` = '42', `bag` = '78', `slot` = '1', `item_template` = '6948' WHERE `item` = '77'",
        "UPDATE `mail` SET `has_items` = '1', `expire_time` = '1700000000', `deliver_time` = '1690000000', `money` = '50', `cod` = '0', `checked` = '1' WHERE `id` = '7'",
        "DELETE FROM `mail_items` WHERE `item_guid` = '80'",
        "DELETE FROM `mail_items` WHERE `item_guid` = '81'",
        "DELETE FROM `mail` WHERE `id` = '8'",
        "DELETE FROM `mail_items` WHERE `mail_id` = '8'",
        "DELETE FROM `character_queststatus_daily` WHERE `guid` = '42'",
        "INSERT INTO `character_queststatus_daily` (`guid`,`quest`) VALUES ('42', '11')",
        "INSERT INTO `character_queststatus_daily` (`guid`,`quest`) VALUES ('42', '12')",
    }, __LINE__);
}

// The load's fixes, the instance binds, the home bind and the character's own small writes, each
// one queued statement with its values substituted; the offline position is read on the calling
// thread.
TEST(CharacterSaveRepository_LoadFixesBindsAndSmallWrites)
{
    Db db;
    CharacterRepository::QueryPosition(42);
    CheckLines(db.query.executed, {
        "SELECT `position_x`,`position_y`,`position_z`,`orientation`,`map`,`taxi_path` FROM `characters` WHERE `guid` = '42'",
    }, __LINE__);
    {
        TickGuard::Scope scope;
        ItemRepository::DropInventoryRow(77);
        ItemRepository::DeleteUnknownItem(77);
        ItemRepository::DropLoot(78);
        MailRepository::DropMailedItem(79);
        CharacterSaveRepository::DeleteInvalidSkill(42, 164);
        CharacterSaveRepository::DeleteHomebind(42);
        CharacterSaveRepository::InsertHomebind(42, 0, 12, 1.5f, -2.25f, 3.0f);
        CharacterSaveRepository::UpdateHomebind(42, 1, 14, 4.0f, 5.5f, 6.0f);
        InstanceBindRepository::InsertBind(42, 7, true);
        InstanceBindRepository::UpdateBind(8, false, 42, 7);
        InstanceBindRepository::DeleteBind(42, 8);
        InstanceBindRepository::CopyBindsToGroup(42);
        InstanceBindRepository::DeleteTemporaryBinds(42);
        CharacterRepository::SetAtLoginFlag(1, 42);
        CharacterRepository::ClearAtLoginFlag(8, 42);
        CharacterRepository::SetZone(12, 42);
        CharacterRepository::SetPhaseMap(42, 654);
        CharacterRepository::SetAppearance(1, 0x04030201, 5, 42);
        TicketRepository::InsertWhisper(43, 42, "hello", 3);
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CheckLines(db.Take(), {
        "DELETE FROM `character_inventory` WHERE `item` = '77'",
        "DELETE FROM `item_instance` WHERE `guid` = '77'",
        "DELETE FROM `item_loot` WHERE `guid` = '78'",
        "DELETE FROM `mail_items` WHERE `item_guid` = '79'",
        "DELETE FROM `character_skills` WHERE `guid` = '42' AND `skill` = '164' ",
        "DELETE FROM `character_homebind` WHERE `guid` = '42'",
        "INSERT INTO `character_homebind` (`guid`,`map`,`zone`,`position_x`,`position_y`,`position_z`) VALUES ('42', '0', '12', '1.500000', '-2.250000', '3.000000')",
        "UPDATE `character_homebind` SET `map` = '1', `zone` = '14', `position_x` = '4.000000', `position_y` = '5.500000', `position_z` = '6.000000' WHERE `guid` = '42'",
        "INSERT INTO `character_instance` (`guid`, `instance`, `permanent`) VALUES ('42', '7', '1')",
        "UPDATE `character_instance` SET `instance` = '8', `permanent` = '0' WHERE `guid` = '42' AND `instance` = '7'",
        "DELETE FROM `character_instance` WHERE `guid` = '42' AND `instance` = '8'",
        "INSERT INTO `group_instance` SELECT `guid`, `instance`, `permanent` FROM `character_instance` WHERE `guid` = '42'",
        "DELETE FROM `character_instance` WHERE `guid` = '42' AND `permanent` = 0",
        "UPDATE `characters` SET `at_login` = `at_login` | '1' WHERE `guid` ='42'",
        "UPDATE `characters` set `at_login` = `at_login` & ~ 8 WHERE `guid` ='42'",
        "UPDATE `characters` SET `zone`='12' WHERE `guid`='42'",
        "REPLACE INTO `character_phase_data` (`guid`, `map`) VALUES (42, 654)",
        "UPDATE `characters` SET `gender` = '1', `playerBytes` = '67305985', `playerBytes2` = '5' WHERE `guid` = '42'",
        "INSERT INTO `character_whispers` (`to_guid`, `from_guid`, `message`, `regarding_ticket_id`) VALUES ('43', '42', 'hello', '3')",
    }, __LINE__);
}
