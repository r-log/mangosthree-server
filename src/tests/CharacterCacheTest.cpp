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

/// Decoupling D7c: the offline player facts come out of CharacterCache instead of a
/// blocking SELECT. The cache itself is plain memory, so the first three cases drive it
/// directly; the last two go through the production lookups (Player::GetLevelFromDB and
/// its siblings) against the GLOBAL CharacterDatabase backed by D7a's fakes, inside a
/// TickGuard::Scope -- which is what proves the tick no longer waits and that no SQL is
/// issued at all.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "ArenaTeam.h"
#include "CharacterCache.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "ObjectGuid.h"
#include "Player.h"

#include <atomic>
#include <string>
#include <thread>

namespace
{
    const uint32 kAldorLow = 101;
    const uint32 kNaaruLow = 202;
    const uint32 kUnknownLow = 999;

    ObjectGuid AldorGuid() { return ObjectGuid(HIGHGUID_PLAYER, kAldorLow); }
    ObjectGuid NaaruGuid() { return ObjectGuid(HIGHGUID_PLAYER, kNaaruLow); }
    ObjectGuid UnknownGuid() { return ObjectGuid(HIGHGUID_PLAYER, kUnknownLow); }

    /// Two characters, as LoadFromDB would have left them.
    void SeedTwo()
    {
        sCharacterCache.Clear();

        CharacterCacheEntry aldor;
        aldor.guid        = AldorGuid();
        aldor.accountId   = 7;
        aldor.name        = "Aldor";
        aldor.race        = RACE_HUMAN;
        aldor.playerClass = CLASS_PALADIN;
        aldor.level       = 62;
        aldor.zoneId      = 3703;
        aldor.guildId     = 11;
        aldor.guildRank   = 3;
        sCharacterCache.Add(aldor);

        CharacterCacheEntry naaru;
        naaru.guid        = NaaruGuid();
        naaru.accountId   = 9;
        naaru.name        = "Naaru";
        naaru.race        = RACE_ORC;
        naaru.playerClass = CLASS_SHAMAN;
        naaru.level       = 80;
        naaru.zoneId      = 1637;
        sCharacterCache.Add(naaru);
    }
}

TEST(CharacterCache_AnswersByGuidAndByNameWithoutRegardToCase)
{
    SeedTwo();
    CHECK_EQ(sCharacterCache.Size(), size_t(2));

    CharacterCacheRef aldor = sCharacterCache.GetByGuid(AldorGuid());
    REQUIRE(aldor != nullptr);
    CHECK(aldor->name == "Aldor");
    CHECK_EQ(uint32(aldor->level), 62u);
    CHECK_EQ(aldor->zoneId, 3703u);
    CHECK_EQ(aldor->accountId, 7u);
    CHECK_EQ(aldor->guildId, 11u);
    CHECK_EQ(aldor->guildRank, 3u);

    // `characters`.`name` is a case-insensitive column, so the SELECT this index replaces
    // matched every one of these spellings, and so must the index.
    REQUIRE(sCharacterCache.GetByName("Aldor") != nullptr);
    CHECK(sCharacterCache.GetByName("aldor")->guid == AldorGuid());
    CHECK(sCharacterCache.GetByName("ALDOR")->guid == AldorGuid());
    CHECK(sCharacterCache.GetByName("aLdOr")->guid == AldorGuid());
    CHECK(sCharacterCache.GetByName("Naaru")->guid == NaaruGuid());

    CHECK(sCharacterCache.GetByName("Nobody") == nullptr);
    CHECK(sCharacterCache.GetByGuid(UnknownGuid()) == nullptr);

    // The derived answers the ObjectMgr lookups hand back. The team is the production
    // race->team mapping applied to the CACHED race, which is the whole of what
    // ObjectMgr::GetPlayerTeamByGUID's SELECT ever contributed -- asserted against that
    // same mapping, because ChrRaces is a DBC this binary does not load.
    CHECK_EQ(int(sCharacterCache.GetTeam(AldorGuid())), int(Player::TeamForRace(RACE_HUMAN)));
    CHECK_EQ(int(sCharacterCache.GetTeam(NaaruGuid())), int(Player::TeamForRace(RACE_ORC)));
    CHECK_EQ(int(sCharacterCache.GetTeam(UnknownGuid())), int(TEAM_NONE));
    CHECK_EQ(sCharacterCache.GetAccountId(NaaruGuid()), 9u);
    CHECK_EQ(sCharacterCache.GetAccountId(UnknownGuid()), 0u);

    sCharacterCache.Clear();
}

TEST(CharacterCache_NameIndexMatchesWhatUtf8GeneralCiCompares)
{
    // The values below are not chosen, they are what the live `character3` answered for
    // `SELECT <a> = <b> COLLATE utf8_general_ci` -- the collation `characters`.`name`
    // carries. Every UTF-8 literal is written as bytes so the file stays ASCII and no
    // compiler's source encoding can change what is being asserted.
    const std::string oDiaeresisLower = "\xC3\xB6";          // U+00F6
    const std::string oDiaeresisUpper = "\xC3\x96";          // U+00D6
    const std::string sharpS          = "\xC3\x9F";          // U+00DF
    const std::string lStroke         = "\xC5\x81";          // U+0141
    const std::string aeLigature      = "\xC3\x86";          // U+00C6
    const std::string cyrillicIoUpper = "\xD0\x81";          // U+0401
    const std::string cyrillicIoLower = "\xD1\x91";          // U+0451
    const std::string cyrillicELower  = "\xD0\xB5";          // U+0435
    const std::string cyrillicLya     = "\xD0\xBB\xD1\x8F";  // U+043B U+044F
    const std::string cjkOne          = "\xE4\xB8\x80";      // U+4E00
    const std::string cjkTwo          = "\xE4\xBA\x8C";      // U+4E8C
    const std::string cjkThree        = "\xE4\xB8\x89";      // U+4E09
    const std::string fullwidthUpper  = "\xEF\xBC\xA2\xEF\xBD\x8A\xEF\xBD\x8F\xEF\xBD\x92\xEF\xBD\x8E";  // Bjorn, U+FF22 U+FF4A U+FF4F U+FF52 U+FF4E
    const std::string fullwidthLower  = "\xEF\xBD\x82\xEF\xBD\x8A\xEF\xBD\x8F\xEF\xBD\x92\xEF\xBD\x8E";  // bjorn, U+FF42 …

    sCharacterCache.Clear();

    CharacterCacheEntry bjorn;
    bjorn.guid = ObjectGuid(HIGHGUID_PLAYER, uint32(11));
    bjorn.name = "Bj" + oDiaeresisLower + "rn";
    sCharacterCache.Add(bjorn);

    // 'o with diaeresis' = 'o' answered 1, and case-insensitively in both directions.
    REQUIRE(sCharacterCache.GetByName("Bjorn") != nullptr);
    CHECK(sCharacterCache.GetByName("Bjorn")->guid == bjorn.guid);
    CHECK(sCharacterCache.GetByName("bjorn") != nullptr);
    CHECK(sCharacterCache.GetByName("BJ" + oDiaeresisUpper + "RN") != nullptr);
    CHECK(sCharacterCache.GetByName("Bj" + oDiaeresisLower + "rn") != nullptr);
    CHECK(sCharacterCache.GetByName("BJORN") != nullptr);

    // sharp s = 's' answered 1; sharp s = "ss" answered 0.
    CharacterCacheEntry strasse;
    strasse.guid = ObjectGuid(HIGHGUID_PLAYER, uint32(12));
    strasse.name = "Stra" + sharpS + "e";
    sCharacterCache.Add(strasse);
    CHECK(sCharacterCache.GetByName("Strase") != nullptr);
    CHECK(sCharacterCache.GetByName("Strasse") == nullptr);

    // 'l with stroke' = 'l' answered 0, and the ae ligature = "ae" answered 0: a fold that
    // merely stripped accents would wrongly merge both of these.
    CharacterCacheEntry lukasz;
    lukasz.guid = ObjectGuid(HIGHGUID_PLAYER, uint32(13));
    lukasz.name = lStroke + "ukasz";
    sCharacterCache.Add(lukasz);
    CHECK(sCharacterCache.GetByName("Lukasz") == nullptr);
    CHECK(sCharacterCache.GetByName(lStroke + "ukasz") != nullptr);

    CharacterCacheEntry aegir;
    aegir.guid = ObjectGuid(HIGHGUID_PLAYER, uint32(14));
    aegir.name = aeLigature + "gir";
    sCharacterCache.Add(aegir);
    CHECK(sCharacterCache.GetByName("Aegir") == nullptr);
    CHECK(sCharacterCache.GetByName(aeLigature + "gir") != nullptr);

    // CYRILLIC IO = CYRILLIC E answered 1, in both cases and across them.
    CharacterCacheEntry elya;
    elya.guid = ObjectGuid(HIGHGUID_PLAYER, uint32(15));
    elya.name = cyrillicIoUpper + cyrillicLya;
    sCharacterCache.Add(elya);
    CHECK(sCharacterCache.GetByName(cyrillicELower + cyrillicLya) != nullptr);
    CHECK(sCharacterCache.GetByName(cyrillicIoLower + cyrillicLya) != nullptr);

    // A script the probe never asked about -- East Asian names are admitted when
    // StrictPlayerNames is 0 -- is compared as it came, which is what utf8_general_ci does
    // with it: equal to itself, unequal to anything else.
    CharacterCacheEntry cjk;
    cjk.guid = ObjectGuid(HIGHGUID_PLAYER, uint32(16));
    cjk.name = cjkOne + cjkTwo;
    sCharacterCache.Add(cjk);
    CHECK(sCharacterCache.GetByName(cjkOne + cjkTwo) != nullptr);
    CHECK(sCharacterCache.GetByName(cjkOne + cjkThree) == nullptr);

    // The one East Asian range that is NOT identity: the fullwidth small letters weigh as
    // the fullwidth capitals ('ａ' = 'Ａ' answered 1), and neither folds to ASCII.
    CharacterCacheEntry fullwidth;
    fullwidth.guid = ObjectGuid(HIGHGUID_PLAYER, uint32(17));
    fullwidth.name = fullwidthUpper;
    sCharacterCache.Add(fullwidth);
    CHECK(sCharacterCache.GetByName(fullwidthLower) != nullptr);
    CHECK(sCharacterCache.GetByName("Bjorn")->guid == bjorn.guid);   // still the Latin one

    // Seven distinct characters, and no fold collapsed two of them into one entry.
    CHECK_EQ(sCharacterCache.Size(), size_t(7));

    sCharacterCache.Clear();
}

TEST(CharacterCache_ASharedNameKeepsAnsweringTheFirstCharacterToHoldIt)
{
    // `characters`.`idx_name` is NOT unique -- two rows may carry the same name, and the
    // SELECT this index replaces walked that index and answered the LOWER guid. So a later
    // arrival under a name that is already taken must not steal it. `.pdump load` is the
    // path that reaches this: it loads a character under a name that exists and flags it to
    // be renamed at login, and until then both rows are in the table.
    sCharacterCache.Clear();

    CharacterCacheEntry first;
    first.guid = ObjectGuid(HIGHGUID_PLAYER, uint32(20));
    first.name = "Aldor";
    sCharacterCache.Add(first);

    CharacterCacheEntry second;
    second.guid = ObjectGuid(HIGHGUID_PLAYER, uint32(99));
    second.name = "ALDOR";                                  // the same name to the collation
    sCharacterCache.Add(second);

    // Both characters are cached, and the name still answers the one that had it.
    CHECK_EQ(sCharacterCache.Size(), size_t(2));
    REQUIRE(sCharacterCache.GetByName("aldor") != nullptr);
    CHECK(sCharacterCache.GetByName("aldor")->guid == first.guid);
    REQUIRE(sCharacterCache.GetByGuid(second.guid) != nullptr);
    CHECK(sCharacterCache.GetByGuid(second.guid)->name == "ALDOR");

    // Removing the one that does NOT hold the key leaves the holder answering.
    sCharacterCache.Remove(second.guid);
    REQUIRE(sCharacterCache.GetByName("aldor") != nullptr);
    CHECK(sCharacterCache.GetByName("aldor")->guid == first.guid);

    sCharacterCache.Clear();
}

TEST(CharacterCache_EverySetterIsVisibleToTheNextLookup)
{
    SeedTwo();

    sCharacterCache.UpdateLevel(AldorGuid(), 63);
    CHECK_EQ(uint32(sCharacterCache.GetByGuid(AldorGuid())->level), 63u);

    sCharacterCache.UpdateZone(AldorGuid(), 3483);
    CHECK_EQ(sCharacterCache.GetByGuid(AldorGuid())->zoneId, 3483u);

    sCharacterCache.UpdateGuild(AldorGuid(), 44, 2);
    CHECK_EQ(sCharacterCache.GetByGuid(AldorGuid())->guildId, 44u);
    CHECK_EQ(sCharacterCache.GetByGuid(AldorGuid())->guildRank, 2u);

    sCharacterCache.UpdateGuildRank(AldorGuid(), 1);
    CHECK_EQ(sCharacterCache.GetByGuid(AldorGuid())->guildRank, 1u);

    // Leaving a guild clears the rank with it, the way a missing `guild_member` row read
    // back as rank 0.
    sCharacterCache.UpdateGuild(AldorGuid(), 0);
    CHECK_EQ(sCharacterCache.GetByGuid(AldorGuid())->guildId, 0u);
    CHECK_EQ(sCharacterCache.GetByGuid(AldorGuid())->guildRank, 0u);

    sCharacterCache.UpdateArenaTeam(AldorGuid(), 1, 77);
    CHECK_EQ(sCharacterCache.GetByGuid(AldorGuid())->arenaTeamId[0], 0u);
    CHECK_EQ(sCharacterCache.GetByGuid(AldorGuid())->arenaTeamId[1], 77u);
    sCharacterCache.UpdateArenaTeam(AldorGuid(), 1, 0);
    CHECK_EQ(sCharacterCache.GetByGuid(AldorGuid())->arenaTeamId[1], 0u);

    sCharacterCache.UpdateAccount(AldorGuid(), 0);
    CHECK_EQ(sCharacterCache.GetAccountId(AldorGuid()), 0u);

    // A rename re-keys the name index: the old spelling stops answering, the new one
    // starts, and the guid keeps its entry.
    sCharacterCache.UpdateName(AldorGuid(), "Renamed");
    CHECK(sCharacterCache.GetByName("Aldor") == nullptr);
    REQUIRE(sCharacterCache.GetByName("renamed") != nullptr);
    CHECK(sCharacterCache.GetByName("renamed")->guid == AldorGuid());
    CHECK(sCharacterCache.GetByGuid(AldorGuid())->name == "Renamed");

    // A cleared name (the soft delete) frees the name without dropping the character.
    sCharacterCache.UpdateName(NaaruGuid(), "");
    CHECK(sCharacterCache.GetByName("Naaru") == nullptr);
    REQUIRE(sCharacterCache.GetByGuid(NaaruGuid()) != nullptr);
    CHECK(sCharacterCache.GetByGuid(NaaruGuid())->name.empty());

    // Remove makes both lookups miss.
    sCharacterCache.Remove(AldorGuid());
    CHECK(sCharacterCache.GetByGuid(AldorGuid()) == nullptr);
    CHECK(sCharacterCache.GetByName("renamed") == nullptr);
    CHECK_EQ(sCharacterCache.Size(), size_t(1));

    // The bulk clears, which the start-up repairs use.
    SeedTwo();
    sCharacterCache.UpdateArenaTeam(NaaruGuid(), 2, 55);
    sCharacterCache.ClearGuild(11);
    CHECK_EQ(sCharacterCache.GetByGuid(AldorGuid())->guildId, 0u);
    sCharacterCache.ClearArenaTeam(55);
    CHECK_EQ(sCharacterCache.GetByGuid(NaaruGuid())->arenaTeamId[2], 0u);

    sCharacterCache.Clear();
}

TEST(CharacterCache_AReaderDuringAWriterSeesOneWholeValueOrTheOther)
{
    SeedTwo();

    // Two names of very different lengths, so a reader that saw a half-written entry
    // would see neither of them. The level goes with them.
    const std::string shortName = "Bo";
    const std::string longName = "Averylongcharactername";

    std::atomic<bool> stop(false);
    std::atomic<int> torn(0);
    std::atomic<int> reads(0);

    std::thread reader([&]()
    {
        while (!stop.load())
        {
            CharacterCacheRef entry = sCharacterCache.GetByGuid(AldorGuid());
            if (!entry)
            {
                torn.fetch_add(1);
                continue;
            }

            // Hold the reference over a second look at the same fields: an entry that can
            // be mutated behind a reader would change between these two reads.
            const std::string seen = entry->name;
            const uint8 level = entry->level;
            if (seen != shortName && seen != longName)
            {
                torn.fetch_add(1);
            }
            if (level != 1 && level != 80)
            {
                torn.fetch_add(1);
            }
            if (entry->name != seen || entry->level != level)
            {
                torn.fetch_add(1);
            }
            reads.fetch_add(1);
        }
    });

    for (int i = 0; i < 20000; ++i)
    {
        if (i % 2)
        {
            sCharacterCache.UpdateName(AldorGuid(), shortName);
            sCharacterCache.UpdateLevel(AldorGuid(), 1);
        }
        else
        {
            sCharacterCache.UpdateName(AldorGuid(), longName);
            sCharacterCache.UpdateLevel(AldorGuid(), 80);
        }
    }

    stop.store(true);
    reader.join();

    CHECK_EQ(torn.load(), 0);
    CHECK(reads.load() > 0);

    sCharacterCache.Clear();
}

TEST(CharacterCache_TheLookupsAnswerInsideATickWithoutTouchingTheDatabase)
{
    SeedTwo();
    TickGuard::ResetViolations();

    FakeConnection query(CharacterDatabase);
    FakeConnection async(CharacterDatabase);
    SqlResultQueue results;

    // Whatever any of these lookups asked for before D7c, the fake would have answered it
    // here. Nothing may reach it now.
    AttachedFakes attached(CharacterDatabase, &query, &async, &results);

    {
        TickGuard::Scope scope;

        // A cached, offline character: the values the SELECTs used to bring back.
        CHECK_EQ(Player::GetLevelFromDB(AldorGuid()), 62u);
        CHECK_EQ(Player::GetZoneIdFromDB(AldorGuid()), 3703u);
        CHECK_EQ(Player::GetGuildIdFromDB(AldorGuid()), 11u);
        CHECK_EQ(Player::GetRankFromDB(AldorGuid()), 3u);
        CHECK(Player::GetGuildGuidFromDB(AldorGuid()) == ObjectGuid(HIGHGUID_GUILD, uint32(11)));

        sCharacterCache.UpdateArenaTeam(NaaruGuid(), ArenaTeam::GetSlotByType(ARENA_TYPE_3v3), 88);
        CHECK_EQ(Player::GetArenaTeamIdFromDB(NaaruGuid(), ARENA_TYPE_3v3), 88u);
        CHECK_EQ(Player::GetArenaTeamIdFromDB(NaaruGuid(), ARENA_TYPE_2v2), 0u);

        // A character the cache has never heard of: the documented "not found" values.
        CHECK_EQ(Player::GetLevelFromDB(UnknownGuid()), 0u);
        CHECK_EQ(Player::GetZoneIdFromDB(UnknownGuid()), 0u);
        CHECK_EQ(Player::GetGuildIdFromDB(UnknownGuid()), 0u);
        CHECK_EQ(Player::GetRankFromDB(UnknownGuid()), 0u);
        CHECK_EQ(Player::GetArenaTeamIdFromDB(UnknownGuid(), ARENA_TYPE_2v2), 0u);
        const ObjectGuid noGuild;
        CHECK(Player::GetGuildGuidFromDB(UnknownGuid()) == noGuild);

        // Not one of them waited on a connection, and the tick counted nothing.
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(query.executed.size(), size_t(0));
        CHECK_EQ(async.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(0));
    }

    CHECK_EQ(TickGuard::Violations(), 0u);
    sCharacterCache.Clear();
}
