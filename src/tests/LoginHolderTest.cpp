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

/// The login holder's reads: every column of a canned slot lands in its member, typed as the
/// loader read it, and an empty slot reads as none.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/talents/GlyphRepository.h"
#include "persistence/characters/inventory/CurrencyRepository.h"
#include "persistence/characters/talents/TalentRepository.h"
#include "persistence/characters/social/ReputationRepository.h"
#include "persistence/characters/quests/QuestStatusRepository.h"
#include "persistence/characters/spells/SpellCooldownRepository.h"

#include <optional>
#include <vector>

namespace
{
    /// A canned result as a connection hands it out: on its first row.
    QueryResult* CannedResult(FakeRows rows)
    {
        FakeQueryResult* result = new FakeQueryResult(std::move(rows));
        result->NextRow();
        return result;
    }
}

// Each column of a `character_skills` row of the login lands in its member, and the rows come back
// in order.
TEST(CharacterRepository_LoginSkills)
{
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADSKILLS, CannedResult(FakeRows{
        FakeRow{"10000", "10001", "10002"},
        FakeRow{"20000", "20001", "20002"},
    }));
    std::vector<LoginSkillRow> rows = CharacterRepository::LoginSkills(&holder);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].skill, uint16(10000));
    CHECK_EQ(rows[0].value, uint16(10001));
    CHECK_EQ(rows[0].max, uint16(10002));
    CHECK_EQ(rows[1].skill, uint16(20000));
}

// The read answers the first column of the first row, and nothing for no rows.
TEST(CharacterRepository_LoginGroup)
{
    {
        SqlQueryHolder holder;
        holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
        std::optional<uint32> value = CharacterRepository::LoginGroup(&holder);
        CHECK(!value);
    }
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADGROUP, CannedResult(FakeRows{
        FakeRow{"1007"}, FakeRow{"2007"},
    }));
    std::optional<uint32> value = CharacterRepository::LoginGroup(&holder);
    REQUIRE(value);
    CHECK_EQ(*value, 1007u);
}

// Each column of a `character_homebind` row of the login lands in its member, and no rows answer
// nothing.
TEST(CharacterRepository_LoginHomeBind)
{
    {
        SqlQueryHolder holder;
        holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
        CHECK(!CharacterRepository::LoginHomeBind(&holder));
    }
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADHOMEBIND, CannedResult(FakeRows{
        FakeRow{"1000", "10001", "1002.5", "1003.5", "1004.5"},
        FakeRow{"2000", "20001", "2002.5", "2003.5", "2004.5"},
    }));
    std::optional<LoginHomeBindRow> row = CharacterRepository::LoginHomeBind(&holder);
    REQUIRE(row);
    CHECK_EQ(row->map, 1000u);
    CHECK_EQ(row->zone, uint16(10001));
    CHECK_EQ(row->positionX, 1002.5f);
    CHECK_EQ(row->positionY, 1003.5f);
    CHECK_EQ(row->positionZ, 1004.5f);
}

// Each column of a `arena_team_member` row of the login lands in its member, and the rows come back
// in order.
TEST(CharacterRepository_LoginArenaTeamInfo)
{
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADARENAINFO, CannedResult(FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "1004"},
        FakeRow{"2000", "2001", "2002", "2003", "2004"},
    }));
    std::vector<LoginArenaTeamInfoRow> rows = CharacterRepository::LoginArenaTeamInfo(&holder);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].arenateamid, 1000u);
    CHECK_EQ(rows[0].playedWeek, 1001u);
    CHECK_EQ(rows[0].playedSeason, 1002u);
    CHECK_EQ(rows[0].wonsSeason, 1003u);
    CHECK_EQ(rows[0].personalRating, 1004u);
    CHECK_EQ(rows[1].arenateamid, 2000u);
}

// Each column of a instance bind of the login (`character_instance`), with its instance's map,
// difficulty and reset time lands in its member, and the rows come back in order.
TEST(CharacterRepository_LoginBoundInstances)
{
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES, CannedResult(FakeRows{
        FakeRow{"1000", "1", "1002", "103", "4294967300"},
        FakeRow{"2000", "1", "2002", "203", "8589934596"},
    }));
    std::vector<LoginBoundInstanceRow> rows = CharacterRepository::LoginBoundInstances(&holder);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].permanent, true);
    CHECK_EQ(rows[0].map, 1002u);
    CHECK_EQ(rows[0].difficulty, uint8(103));
    CHECK_EQ(rows[0].resettime, uint64(4294967300ull));
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a `character_battleground_data` row of the login lands in its member, and no rows
// answer nothing.
TEST(CharacterRepository_LoginBGData)
{
    {
        SqlQueryHolder holder;
        holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
        CHECK(!CharacterRepository::LoginBGData(&holder));
    }
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADBGDATA, CannedResult(FakeRows{
        FakeRow{"1000", "1001", "1002.5", "1003.5", "1004.5", "1005.5", "1006", "1007", "1008",
            "1009"},
        FakeRow{"2000", "2001", "2002.5", "2003.5", "2004.5", "2005.5", "2006", "2007", "2008",
            "2009"},
    }));
    std::optional<LoginBattlegroundDataRow> row = CharacterRepository::LoginBGData(&holder);
    REQUIRE(row);
    CHECK_EQ(row->instanceId, 1000u);
    CHECK_EQ(row->team, 1001u);
    CHECK_EQ(row->joinX, 1002.5f);
    CHECK_EQ(row->joinY, 1003.5f);
    CHECK_EQ(row->joinZ, 1004.5f);
    CHECK_EQ(row->joinO, 1005.5f);
    CHECK_EQ(row->joinMap, 1006u);
    CHECK_EQ(row->taxiStart, 1007u);
    CHECK_EQ(row->taxiEnd, 1008u);
    CHECK_EQ(row->mountSpell, 1009u);
}

// Each column of a `mail` row of the login lands in its member, and the rows come back in order.
TEST(CharacterRepository_LoginMails)
{
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADMAILS, CannedResult(FakeRows{
        FakeRow{"1000", "101", "1002", "1003", "s1.4", "s1.5", "4294967302", "4294967303", "1008",
            "1009", "1010", "111", "-1012", "1"},
        FakeRow{"2000", "201", "2002", "2003", "s2.4", "s2.5", "8589934598", "8589934599", "2008",
            "2009", "2010", "211", "-2012", "1"},
    }));
    std::vector<LoginMailRow> rows = CharacterRepository::LoginMails(&holder);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].messageType, uint8(101));
    CHECK_EQ(rows[0].sender, 1002u);
    CHECK_EQ(rows[0].receiver, 1003u);
    CHECK_STR(rows[0].subject, "s1.4");
    CHECK_STR(rows[0].body, "s1.5");
    CHECK_EQ(rows[0].expireTime, uint64(4294967302ull));
    CHECK_EQ(rows[0].deliverTime, uint64(4294967303ull));
    CHECK_EQ(rows[0].money, 1008u);
    CHECK_EQ(rows[0].cod, 1009u);
    CHECK_EQ(rows[0].checked, 1010u);
    CHECK_EQ(rows[0].stationery, uint8(111));
    CHECK_EQ(rows[0].mailTemplateId, int16(-1012));
    CHECK_EQ(rows[0].hasItems, true);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a `character_cuf_profiles` row of the login lands in its member, and the rows come
// back in order.
TEST(CharacterRepository_LoginCUFProfiles)
{
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADCUFPROFILES, CannedResult(FakeRows{
        FakeRow{"100", "s1.1", "10002", "10003", "104", "105", "1006", "107", "108", "109", "10010",
            "10011", "10012"},
        FakeRow{"200", "s2.1", "20002", "20003", "204", "205", "2006", "207", "208", "209", "20010",
            "20011", "20012"},
    }));
    std::vector<LoginCufProfileRow> rows = CharacterRepository::LoginCUFProfiles(&holder);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, uint8(100));
    CHECK_STR(rows[0].name, "s1.1");
    CHECK_EQ(rows[0].frameHeight, uint16(10002));
    CHECK_EQ(rows[0].frameWidth, uint16(10003));
    CHECK_EQ(rows[0].sortBy, uint8(104));
    CHECK_EQ(rows[0].healthText, uint8(105));
    CHECK_EQ(rows[0].boolOptions, 1006u);
    CHECK_EQ(rows[0].topPoint, uint8(107));
    CHECK_EQ(rows[0].bottomPoint, uint8(108));
    CHECK_EQ(rows[0].leftPoint, uint8(109));
    CHECK_EQ(rows[0].topOffset, uint16(10010));
    CHECK_EQ(rows[0].bottomOffset, uint16(10011));
    CHECK_EQ(rows[0].leftOffset, uint16(10012));
    CHECK_EQ(rows[1].id, uint8(200));
}

// Each column of a `character_spell` row of the login lands in its member, and the rows come back
// in order.
TEST(CharacterRepository_LoginSpells)
{
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADSPELLS, CannedResult(FakeRows{
        FakeRow{"1000", "1", "1"},
        FakeRow{"2000", "1", "1"},
    }));
    std::vector<LoginSpellRow> rows = CharacterRepository::LoginSpells(&holder);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].spell, 1000u);
    CHECK_EQ(rows[0].active, true);
    CHECK_EQ(rows[0].disabled, true);
    CHECK_EQ(rows[1].spell, 2000u);
}

// Each column of a `character_queststatus_daily` row of the login lands in its member, and the rows
// come back in order.
TEST(CharacterRepository_LoginDailyQuestStatus)
{
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS, CannedResult(FakeRows{
        FakeRow{"1000"},
        FakeRow{"2000"},
    }));
    std::vector<LoginDailyQuestStatusRow> rows = CharacterRepository::LoginDailyQuestStatus(&holder);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].quest, 1000u);
    CHECK_EQ(rows[1].quest, 2000u);
}

// Each column of a `character_action` row of the login lands in its member, and the rows come back
// in order.
TEST(CharacterRepository_LoginActions)
{
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADACTIONS, CannedResult(FakeRows{
        FakeRow{"100", "101", "1002", "103"},
        FakeRow{"200", "201", "2002", "203"},
    }));
    std::vector<LoginActionRow> rows = CharacterRepository::LoginActions(&holder);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].spec, uint8(100));
    CHECK_EQ(rows[0].button, uint8(101));
    CHECK_EQ(rows[0].action, 1002u);
    CHECK_EQ(rows[0].type, uint8(103));
    CHECK_EQ(rows[1].spec, uint8(200));
}

// Each column of The login's declined names (`character_declinedname`), one per case lands in its
// member, and no rows answer nothing.
TEST(CharacterRepository_LoginDeclinedNames)
{
    {
        SqlQueryHolder holder;
        holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
        CHECK(!CharacterRepository::LoginDeclinedNames(&holder));
    }
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES, CannedResult(FakeRows{
        FakeRow{"s1.0", "s1.1", "s1.2", "s1.3", "s1.4"},
        FakeRow{"s2.0", "s2.1", "s2.2", "s2.3", "s2.4"},
    }));
    std::optional<LoginDeclinedNameRow> row = CharacterRepository::LoginDeclinedNames(&holder);
    REQUIRE(row);
    CHECK_STR(row->name[0], "s1.0");
    CHECK_STR(row->name[1], "s1.1");
    CHECK_STR(row->name[2], "s1.2");
    CHECK_STR(row->name[3], "s1.3");
    CHECK_STR(row->name[4], "s1.4");
}

// Each column of a `character_equipmentsets` row of the login lands in its member, and the rows
// come back in order.
TEST(CharacterRepository_LoginEquipmentSets)
{
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADEQUIPMENTSETS, CannedResult(FakeRows{
        FakeRow{"4294967296", "1001", "s1.2", "s1.3", "1004", "1005", "1006", "1007", "1008",
            "1009", "1010", "1011", "1012", "1013", "1014", "1015", "1016", "1017", "1018",
            "1019", "1020", "1021", "1022", "1023"},
        FakeRow{"8589934592", "2001", "s2.2", "s2.3", "2004", "2005", "2006", "2007", "2008",
            "2009", "2010", "2011", "2012", "2013", "2014", "2015", "2016", "2017", "2018",
            "2019", "2020", "2021", "2022", "2023"},
    }));
    std::vector<LoginEquipmentSetRow> rows = CharacterRepository::LoginEquipmentSets(&holder);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].setGuid, uint64(4294967296ull));
    CHECK_EQ(rows[0].setIndex, 1001u);
    CHECK_STR(rows[0].name, "s1.2");
    CHECK_STR(rows[0].iconName, "s1.3");
    CHECK_EQ(rows[0].ignoreMask, 1004u);
    CHECK_EQ(rows[0].items[0], 1005u);
    CHECK_EQ(rows[0].items[1], 1006u);
    CHECK_EQ(rows[0].items[2], 1007u);
    CHECK_EQ(rows[0].items[3], 1008u);
    CHECK_EQ(rows[0].items[4], 1009u);
    CHECK_EQ(rows[0].items[5], 1010u);
    CHECK_EQ(rows[0].items[6], 1011u);
    CHECK_EQ(rows[0].items[7], 1012u);
    CHECK_EQ(rows[0].items[8], 1013u);
    CHECK_EQ(rows[0].items[9], 1014u);
    CHECK_EQ(rows[0].items[10], 1015u);
    CHECK_EQ(rows[0].items[11], 1016u);
    CHECK_EQ(rows[0].items[12], 1017u);
    CHECK_EQ(rows[0].items[13], 1018u);
    CHECK_EQ(rows[0].items[14], 1019u);
    CHECK_EQ(rows[0].items[15], 1020u);
    CHECK_EQ(rows[0].items[16], 1021u);
    CHECK_EQ(rows[0].items[17], 1022u);
    CHECK_EQ(rows[0].items[18], 1023u);
    CHECK_EQ(rows[1].setGuid, uint64(8589934592ull));
}

// Each column of a `character_aura` row of the login lands in its member, and the rows come back in
// order.
TEST(CharacterRepository_LoginAuras)
{
    SqlQueryHolder holder;
    holder.SetSize(MAX_PLAYER_LOGIN_QUERY);
    holder.SetResult(PLAYER_LOGIN_QUERY_LOADAURAS, CannedResult(FakeRows{
        FakeRow{"4294967296", "1001", "1002", "1003", "1004", "-1005", "-1006", "-1007", "1008",
            "1009", "1010", "-1011", "-1012", "1013"},
        FakeRow{"8589934592", "2001", "2002", "2003", "2004", "-2005", "-2006", "-2007", "2008",
            "2009", "2010", "-2011", "-2012", "2013"},
    }));
    std::vector<LoginAuraRow> rows = CharacterRepository::LoginAuras(&holder);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].casterGuid, uint64(4294967296ull));
    CHECK_EQ(rows[0].itemGuid, 1001u);
    CHECK_EQ(rows[0].spell, 1002u);
    CHECK_EQ(rows[0].stackCount, 1003u);
    CHECK_EQ(rows[0].remainCharges, 1004u);
    CHECK_EQ(rows[0].basePoints[0], -1005);
    CHECK_EQ(rows[0].basePoints[1], -1006);
    CHECK_EQ(rows[0].basePoints[2], -1007);
    CHECK_EQ(rows[0].periodicTime[0], 1008u);
    CHECK_EQ(rows[0].periodicTime[1], 1009u);
    CHECK_EQ(rows[0].periodicTime[2], 1010u);
    CHECK_EQ(rows[0].maxDuration, -1011);
    CHECK_EQ(rows[0].remainTime, -1012);
    CHECK_EQ(rows[0].effIndexMask, 1013u);
    CHECK_EQ(rows[1].casterGuid, uint64(8589934592ull));
}

// Each column of a saved glyph lands in its member, and the rows come back in order.
TEST(GlyphRepository_ReadHolder)
{
    SqlQueryHolder holder;
    holder.SetSize(1);
    holder.SetResult(0, CannedResult(FakeRows{
        FakeRow{"100", "101", "1002"},
        FakeRow{"200", "201", "2002"},
    }));
    std::vector<GlyphRow> rows = GlyphRepository::ReadHolder(&holder, 0);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].spec, uint8(100));
    CHECK_EQ(rows[0].slot, uint8(101));
    CHECK_EQ(rows[0].glyph, 1002u);
    CHECK_EQ(rows[1].spec, uint8(200));
}

// Each column of a currency's saved row lands in its member, and the rows come back in order.
TEST(CurrencyRepository_ReadHolder)
{
    SqlQueryHolder holder;
    holder.SetSize(1);
    holder.SetResult(0, CannedResult(FakeRows{
        FakeRow{"10000", "1001", "1002", "1003", "104"},
        FakeRow{"20000", "2001", "2002", "2003", "204"},
    }));
    std::vector<CurrencyRow> rows = CurrencyRepository::ReadHolder(&holder, 0);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, uint16(10000));
    CHECK_EQ(rows[0].totalCount, 1001u);
    CHECK_EQ(rows[0].weekCount, 1002u);
    CHECK_EQ(rows[0].seasonCount, 1003u);
    CHECK_EQ(rows[0].flags, uint8(104));
    CHECK_EQ(rows[1].id, uint16(20000));
}

// Each column of a saved talent lands in its member, and the rows come back in order.
TEST(TalentRepository_ReadHolder)
{
    SqlQueryHolder holder;
    holder.SetSize(1);
    holder.SetResult(0, CannedResult(FakeRows{
        FakeRow{"1000", "1001", "1002"},
        FakeRow{"2000", "2001", "2002"},
    }));
    std::vector<TalentRow> rows = TalentRepository::ReadHolder(&holder, 0);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].talent, 1000u);
    CHECK_EQ(rows[0].rank, 1001u);
    CHECK_EQ(rows[0].spec, 1002u);
    CHECK_EQ(rows[1].talent, 2000u);
}

// Each column of a faction's saved reputation row lands in its member, and the rows come back in
// order.
TEST(ReputationRepository_ReadHolder)
{
    SqlQueryHolder holder;
    holder.SetSize(1);
    holder.SetResult(0, CannedResult(FakeRows{
        FakeRow{"1000", "1001", "1002"},
        FakeRow{"2000", "2001", "2002"},
    }));
    std::vector<ReputationRow> rows = ReputationRepository::ReadHolder(&holder, 0);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].faction, 1000u);
    CHECK_EQ(rows[0].standing, int32(1001u));
    CHECK_EQ(rows[0].flags, 1002u);
    CHECK_EQ(rows[1].faction, 2000u);
}

// Each column of a saved quest status row as the login reads it lands in its member, and the rows
// come back in order.
TEST(QuestStatusRepository_ReadHolder)
{
    SqlQueryHolder holder;
    holder.SetSize(1);
    holder.SetResult(0, CannedResult(FakeRows{
        FakeRow{"1000", "1001", "102", "103", "4294967300", "1005", "1006", "1007", "1008", "1009",
            "1010", "1011", "1012", "1013", "1014"},
        FakeRow{"2000", "2001", "202", "203", "8589934596", "2005", "2006", "2007", "2008", "2009",
            "2010", "2011", "2012", "2013", "2014"},
    }));
    std::vector<QuestStatusRow> rows = QuestStatusRepository::ReadHolder(&holder, 0);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].quest, 1000u);
    CHECK_EQ(rows[0].status, 1001u);
    CHECK_EQ(rows[0].rewarded, uint8(102));
    CHECK_EQ(rows[0].explored, uint8(103));
    CHECK_EQ(rows[0].timer, uint64(4294967300ull));
    CHECK_EQ(rows[0].creatureOrGoCount[0], 1005u);
    CHECK_EQ(rows[0].creatureOrGoCount[1], 1006u);
    CHECK_EQ(rows[0].creatureOrGoCount[2], 1007u);
    CHECK_EQ(rows[0].creatureOrGoCount[3], 1008u);
    CHECK_EQ(rows[0].itemCount[0], 1009u);
    CHECK_EQ(rows[0].itemCount[1], 1010u);
    CHECK_EQ(rows[0].itemCount[2], 1011u);
    CHECK_EQ(rows[0].itemCount[3], 1012u);
    CHECK_EQ(rows[0].itemCount[4], 1013u);
    CHECK_EQ(rows[0].itemCount[5], 1014u);
    CHECK_EQ(rows[1].quest, 2000u);
}

// Each column of a saved spell cooldown lands in its member, and the rows come back in order.
TEST(SpellCooldownRepository_ReadHolder)
{
    SqlQueryHolder holder;
    holder.SetSize(1);
    holder.SetResult(0, CannedResult(FakeRows{
        FakeRow{"1000", "1001", "4294967298"},
        FakeRow{"2000", "2001", "8589934594"},
    }));
    std::vector<SpellCooldownRow> rows = SpellCooldownRepository::ReadHolder(&holder, 0);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].spell, 1000u);
    CHECK_EQ(rows[0].item, 1001u);
    CHECK_EQ(rows[0].end, time_t(uint64(4294967298ull)));
    CHECK_EQ(rows[1].spell, 2000u);
}
