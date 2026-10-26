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

/// The repositories' start-up work for ObjectMgr on the global databases backed by fakes: the
/// expired-mail holder and writes queued in order, the group renumbering and the missing items'
/// rows each one queued transaction, and the highest-id and corpse reads on the calling thread.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/CharacterSaveRepository.h"
#include "persistence/characters/economy/AuctionRepository.h"
#include "persistence/characters/events/GameEventStatusRepository.h"
#include "persistence/characters/inventory/ItemRepository.h"
#include "persistence/characters/mail/MailRepository.h"
#include "persistence/characters/maps/CorpseRepository.h"
#include "persistence/characters/maps/InstanceRepository.h"
#include "persistence/characters/pets/PetRepository.h"
#include "persistence/characters/social/ArenaTeamRepository.h"
#include "persistence/characters/social/GroupRepository.h"
#include "persistence/characters/social/GuildRepository.h"
#include "persistence/world/spawns/SpawnRepository.h"

#include <string>
#include <vector>

namespace
{
    /// The fakes on one of the global databases, asynchronous writes on.
    struct Db
    {
        Database& database;
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        explicit Db(Database& db) : database(db), query(db), async(db),
            attached(db, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            database.ExecuteQueuedForTest();
            return async.executed;
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
}

// The expired-mail run: the start-up delete, the holder's two reads, then one returned mail's and one
// dropped mail's writes, in order; the holder's callback runs on the world thread.
TEST(MailRepository_ExpiredMail)
{
    Db db(CharacterDatabase);
    int answered = 0;
    {
        TickGuard::Scope scope;
        MailRepository::DeleteEmptyExpiredMails(1700000000);
        MailRepository::QueueExpiredMailReads(1700000000, [&answered](QueryResult* /*result*/, SqlQueryHolder* holder) { delete holder; ++answered; });
        MailRepository::DeleteExpiredMailItem(5);
        MailRepository::ReturnExpiredMail(42, 43, 1702592000, 1700000000, 2, 9);
        MailRepository::SetReturnedItemReceiver(43, 6);
        MailRepository::SetReturnedItemOwner(43, 6);
        MailRepository::DeleteMailRow(10);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(7));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `mail` WHERE `expire_time` < '1700000000' AND `has_items` = '0' AND `body` = ''",
        "SELECT `id`,`messageType`,`sender`,`receiver`,`has_items`,`expire_time`,`cod`,`checked`,`mailTemplateId` FROM `mail` WHERE `expire_time` < '1700000000' ORDER BY `id`",
        "SELECT `mail_items`.`item_guid`,`mail_items`.`item_template`,`mail_items`.`mail_id` FROM `mail_items` JOIN `mail` ON `mail_items`.`mail_id` = `mail`.`id` WHERE `mail`.`expire_time` < '1700000000' AND `mail`.`has_items` <> 0 ORDER BY `mail_items`.`mail_id`, `mail_items`.`item_guid`",
        "DELETE FROM `item_instance` WHERE `guid` = '5'",
        "UPDATE `mail` SET `sender` = '42', `receiver` = '43', `expire_time` = '1702592000', `deliver_time` = '1700000000', `cod` = '0', `checked` = '2' WHERE `id` = '9'",
        "UPDATE `mail_items` SET `receiver` = 43 WHERE `item_guid` = '6'",
        "UPDATE `item_instance` SET `owner_guid` = 43 WHERE `guid` = '6'",
        "DELETE FROM `mail` WHERE `id` = '10'",
    }, __LINE__);
    CHECK_EQ(answered, 0);
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 1);
}

// The start-up renumbering's delete and renumber are one transaction each, and so is the delete of
// the rows naming item guids past the highest one.
TEST(GroupRepository_StartupTransactions)
{
    Db db(CharacterDatabase);
    {
        TickGuard::Scope scope;
        GroupRepository::DeleteInvalidGroup(0);
        GroupRepository::RenumberGroup(1, 4);
        ItemRepository::DeleteRowsOfMissingItems(100);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(3));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `groups` WHERE `groupId` = '0'",
        "DELETE FROM `group_member` WHERE `groupId` = '0'",
        "UPDATE `groups` SET `groupId` = '1' WHERE `groupId` = '4'",
        "UPDATE `group_member` SET `groupId` = '1' WHERE `groupId` = '4'",
        "DELETE FROM `character_inventory` WHERE `item` >= '100'",
        "DELETE FROM `mail_items` WHERE `item_guid` >= '100'",
        "DELETE FROM `auction` WHERE `itemguid` >= '100'",
        "DELETE FROM `guild_bank_item` WHERE `item_guid` >= '100'",
    }, __LINE__);
}

// The highest-id reads, the group ids and the corpses run on the calling thread, in call order.
TEST(StartupRepository_Reads)
{
    Db chars(CharacterDatabase);
    Db world(WorldDatabase);
    GroupRepository::QueryGroupIds();
    CharacterRepository::QueryHighestGuid();
    SpawnRepository::QueryHighestCreatureGuid();
    ItemRepository::QueryHighestGuid();
    InstanceRepository::QueryHighestId();
    SpawnRepository::QueryHighestGameObjectGuid();
    AuctionRepository::QueryHighestId();
    MailRepository::QueryHighestId();
    CorpseRepository::QueryHighestGuid();
    ArenaTeamRepository::QueryHighestId();
    CharacterSaveRepository::QueryHighestEquipmentSetId();
    GuildRepository::QueryHighestId();
    GroupRepository::QueryHighestId();
    PetRepository::QueryHighestId();
    CorpseRepository::QueryCorpses();
    CheckLines(chars.query.executed, {
        "SELECT `groupId` FROM `groups`",
        "SELECT MAX(`guid`) FROM `characters`",
        "SELECT MAX(`guid`) FROM `item_instance`",
        "SELECT MAX(`id`) FROM `instance`",
        "SELECT MAX(`id`) FROM `auction`",
        "SELECT MAX(`id`) FROM `mail`",
        "SELECT MAX(`guid`) FROM `corpse`",
        "SELECT MAX(`arenateamid`) FROM `arena_team`",
        "SELECT MAX(`setguid`) FROM `character_equipmentsets`",
        "SELECT MAX(`guildid`) FROM `guild`",
        "SELECT MAX(`groupId`) FROM `groups`",
        "SELECT MAX(`id`) FROM `character_pet`",
        "SELECT `corpse`.`guid`, `player`, `corpse`.`position_x`, `corpse`.`position_y`, `corpse`.`position_z`, `corpse`.`orientation`, `corpse`.`map`, "
        "`time`, `corpse_type`, `instance`, `phaseMask`, `gender`, `race`, `class`, `playerBytes`, `playerBytes2`, `equipmentCache`, `guildId`, `playerFlags` FROM `corpse` "
        "JOIN `characters` ON `player` = `characters`.`guid` "
        "LEFT JOIN `guild_member` ON `player`=`guild_member`.`guid` WHERE `corpse_type` <> 0",
    }, __LINE__);
    CheckLines(world.query.executed, {
        "SELECT MAX(`guid`) FROM `creature`",
        "SELECT MAX(`guid`) FROM `gameobject`",
    }, __LINE__);
    CHECK_EQ(chars.async.executed.size(), size_t(0));
    CHECK_EQ(world.async.executed.size(), size_t(0));
}

// Each highest-id read answers the first column of its first row, and nothing for no rows.
TEST(StartupRepository_HighestIds)
{
    {
        Db chars(CharacterDatabase);
        Db world(WorldDatabase);
        CHECK(!CharacterRepository::QueryHighestGuid());
        CHECK(!SpawnRepository::QueryHighestCreatureGuid());
        CHECK(!ItemRepository::QueryHighestGuid());
        CHECK(!InstanceRepository::QueryHighestId());
        CHECK(!SpawnRepository::QueryHighestGameObjectGuid());
        CHECK(!AuctionRepository::QueryHighestId());
        CHECK(!MailRepository::QueryHighestId());
        CHECK(!CorpseRepository::QueryHighestGuid());
        CHECK(!ArenaTeamRepository::QueryHighestId());
        CHECK(!CharacterSaveRepository::QueryHighestEquipmentSetId());
        CHECK(!GuildRepository::QueryHighestId());
        CHECK(!GroupRepository::QueryHighestId());
        CHECK(!PetRepository::QueryHighestId());
    }
    Db chars(CharacterDatabase);
    Db world(WorldDatabase);
    chars.query.Answer("SELECT MAX(`guid`) FROM `characters`", FakeRows{FakeRow{"101"}});
    world.query.Answer("SELECT MAX(`guid`) FROM `creature`", FakeRows{FakeRow{"102"}});
    chars.query.Answer("SELECT MAX(`guid`) FROM `item_instance`", FakeRows{FakeRow{"103"}});
    chars.query.Answer("SELECT MAX(`id`) FROM `instance`", FakeRows{FakeRow{"104"}});
    world.query.Answer("SELECT MAX(`guid`) FROM `gameobject`", FakeRows{FakeRow{"105"}});
    chars.query.Answer("SELECT MAX(`id`) FROM `auction`", FakeRows{FakeRow{"106"}});
    chars.query.Answer("SELECT MAX(`id`) FROM `mail`", FakeRows{FakeRow{"107"}});
    chars.query.Answer("SELECT MAX(`guid`) FROM `corpse`", FakeRows{FakeRow{"108"}});
    chars.query.Answer("SELECT MAX(`arenateamid`) FROM `arena_team`", FakeRows{FakeRow{"109"}});
    chars.query.Answer("SELECT MAX(`setguid`) FROM `character_equipmentsets`", FakeRows{FakeRow{"4294967297"}});
    chars.query.Answer("SELECT MAX(`guildid`) FROM `guild`", FakeRows{FakeRow{"110"}});
    chars.query.Answer("SELECT MAX(`groupId`) FROM `groups`", FakeRows{FakeRow{"111"}});
    chars.query.Answer("SELECT MAX(`id`) FROM `character_pet`", FakeRows{FakeRow{"112"}});
    CHECK_EQ(CharacterRepository::QueryHighestGuid().value_or(0), 101u);
    CHECK_EQ(SpawnRepository::QueryHighestCreatureGuid().value_or(0), 102u);
    CHECK_EQ(ItemRepository::QueryHighestGuid().value_or(0), 103u);
    CHECK_EQ(InstanceRepository::QueryHighestId().value_or(0), 104u);
    CHECK_EQ(SpawnRepository::QueryHighestGameObjectGuid().value_or(0), 105u);
    CHECK_EQ(AuctionRepository::QueryHighestId().value_or(0), 106u);
    CHECK_EQ(MailRepository::QueryHighestId().value_or(0), 107u);
    CHECK_EQ(CorpseRepository::QueryHighestGuid().value_or(0), 108u);
    CHECK_EQ(ArenaTeamRepository::QueryHighestId().value_or(0), 109u);
    CHECK_EQ(CharacterSaveRepository::QueryHighestEquipmentSetId().value_or(0), uint64(4294967297ull));
    CHECK_EQ(GuildRepository::QueryHighestId().value_or(0), 110u);
    CHECK_EQ(GroupRepository::QueryHighestId().value_or(0), 111u);
    CHECK_EQ(PetRepository::QueryHighestId().value_or(0), 112u);
}

// The arena team, group and character cache loads run on the calling thread, in call order; a
// member row the group load cannot use is deleted through a queued write.
TEST(StartupRepository_CharacterLoads)
{
    Db db(CharacterDatabase);
    ArenaTeamRepository::QueryTeams();
    ArenaTeamRepository::QueryTeamMembers();
    GroupRepository::QueryGroups();
    GroupRepository::QueryMembers();
    GroupRepository::QueryInstanceBinds();
    CharacterRepository::QueryCacheRows();
    GuildRepository::QueryMemberRanks();
    ArenaTeamRepository::QueryMemberTeamTypes();
    CheckLines(db.query.executed, {
        "SELECT `arena_team`.`arenateamid`,`name`,`captainguid`,`type`,`BackgroundColor`,`EmblemStyle`,`EmblemColor`,`BorderStyle`,`BorderColor`, `rating`,`games_week`,`wins_week`,`games_season`,`wins_season`,`rank` FROM `arena_team` LEFT JOIN `arena_team_stats` ON `arena_team`.`arenateamid` = `arena_team_stats`.`arenateamid` ORDER BY `arena_team`.`arenateamid` ASC",
        "SELECT `arenateamid`,`member`.`guid`,`played_week`,`wons_week`,`played_season`,`wons_season`,`personal_rating`,`name`,`class` FROM `arena_team_member` member LEFT JOIN `characters` chars on member.`guid` = chars.`guid` ORDER BY member.`arenateamid` ASC",
        "SELECT `mainTank`, `mainAssistant`, `lootMethod`, `looterGuid`, `lootThreshold`, `icon1`, `icon2`, `icon3`, `icon4`, `icon5`, `icon6`, `icon7`, `icon8`, `groupType`, `difficulty`, `raiddifficulty`, `leaderGuid`, `groupId` FROM `groups`",
        "SELECT `memberGuid`, `assistant`, `subgroup`, `groupId`, `roles` FROM `group_member` ORDER BY `groupId`",
        "SELECT `group_instance`.`leaderGuid`, `map`, `instance`, `permanent`, `instance`.`difficulty`, `resettime`, (SELECT COUNT(*) FROM `character_instance` WHERE `guid` = `group_instance`.`leaderGuid` AND `instance` = `group_instance`.`instance` AND `permanent` = 1 LIMIT 1),  `groups`.`groupId`, `instance`.`encountersMask` FROM `group_instance` LEFT JOIN `instance` ON `instance` = `id` LEFT JOIN `groups` ON `groups`.`leaderGUID` = `group_instance`.`leaderGUID` ORDER BY `leaderGuid`",
        "SELECT `guid`, `account`, `name`, `race`, `class`, `level`, `zone`, `map`, `position_x`, `position_y`, `position_z` FROM `characters` ORDER BY `guid`",
        "SELECT `guid`, `guildid`, `rank` FROM `guild_member`",
        "SELECT `arena_team_member`.`guid`, `arena_team_member`.`arenateamid`, `arena_team`.`type` FROM `arena_team_member` JOIN `arena_team` ON `arena_team_member`.`arenateamid` = `arena_team`.`arenateamid`",
    }, __LINE__);
    GroupRepository::DeleteInvalidMember(42);
    CheckLines(db.Take(), {
        "DELETE FROM `group_member` WHERE `memberGuid` = '42'",
    }, __LINE__);
}
// The start-up read of the events left running runs on the calling thread; the truncate and each
// event's delete and insert are queued in order.
TEST(GameEventStatusRepository_ReadAndWrites)
{
    Db db(CharacterDatabase);
    GameEventStatusRepository::QueryActiveEvents();
    CheckLines(db.query.executed, {
        "SELECT `event` FROM `game_event_status`",
    }, __LINE__);
    {
        TickGuard::Scope scope;
        GameEventStatusRepository::Truncate();
        GameEventStatusRepository::DeleteEvent(5);
        GameEventStatusRepository::InsertEvent(5);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(3));
    }
    CheckLines(db.Take(), {
        "TRUNCATE game_event_status",
        "DELETE FROM `game_event_status` WHERE `event` = 5",
        "INSERT IGNORE INTO `game_event_status` (`event`) VALUES (5)",
    }, __LINE__);
}