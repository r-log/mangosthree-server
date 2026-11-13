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

/// The creature addon load on a fake world database. An addon aura whose periodic trigger names a
/// spell no store holds is reported once per aura and trigger, whichever addon tables name it, and
/// the aura is still kept; an aura whose trigger spell exists is not reported.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "data/ServerSpellStore.h"
#include "spells/data/SpellAuraDefines.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    /// Hands out a pointer to a private member: an explicit instantiation may name it, and the
    /// friend the instantiation defines returns it.
    template<class Tag, typename Tag::type Member>
    struct AddonTriggerMemberAccess
    {
        friend typename Tag::type MemberOf(Tag)
        {
            return Member;
        }
    };

    struct LogMainFile
    {
        typedef FILE* Log::* type;
        friend type MemberOf(LogMainFile);
    };

    template struct AddonTriggerMemberAccess<LogMainFile, &Log::logfile>;

    uint32 const kAuraWithMissingTrigger = 990701;
    uint32 const kAuraWithKnownTrigger = 990702;
    uint32 const kKnownTrigger = 990703;
    uint32 const kMissingTrigger = 990799;

    /// The fakes on the global world database.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        Db() : query(WorldDatabase), async(WorldDatabase), attached(WorldDatabase, &query, &async, &results)
        {
        }
    };

    /// A self-cast spell whose first effect applies a periodic trigger aura naming `trigger`.
    void SeedPeriodicTriggerAura(ServerSpell& spell, uint32 id, uint32 trigger)
    {
        spell.Entry().ID = id;
        spell.effect.SpellID = id;
        spell.effect.EffectIndex = EFFECT_INDEX_0;
        spell.effect.Effect = SPELL_EFFECT_APPLY_AURA;
        spell.effect.EffectAura = SPELL_AURA_PERIODIC_TRIGGER_SPELL;
        spell.effect.EffectTriggerSpell = trigger;
        spell.effect.ImplicitTarget_0 = TARGET_SELF;
        sSpellStore.SetEntry(id, &spell.Entry());
        AddSpellEffectEntry(&spell.effect);
    }

    /// The two auras and the trigger spell that exists, seeded once for the test run.
    void SeedSpells()
    {
        static bool seeded = false;
        if (seeded)
        {
            return;
        }
        seeded = true;

        static ServerSpell missing;
        static ServerSpell known;
        static ServerSpell trigger;
        SeedPeriodicTriggerAura(missing, kAuraWithMissingTrigger, kMissingTrigger);
        SeedPeriodicTriggerAura(known, kAuraWithKnownTrigger, kKnownTrigger);
        trigger.Entry().ID = kKnownTrigger;
        sSpellStore.SetEntry(kKnownTrigger, &trigger.Entry());
    }

    /// The messages of the error lines the log's main file receives while the call runs: the file is
    /// swapped for a scratch file and put back.
    template <typename Call>
    std::vector<std::string> ErrorsLoggedBy(Call call)
    {
        std::filesystem::path const path = std::filesystem::temp_directory_path() /
            ("mangos_addon_trigger_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".log");
        FILE* scratch = std::fopen(path.string().c_str(), "w+");
        if (!scratch)
        {
            return std::vector<std::string>(1, "the scratch log could not be opened");
        }
        FILE* const saved = sLog.*MemberOf(LogMainFile());
        sLog.*MemberOf(LogMainFile()) = scratch;
        call();
        sLog.*MemberOf(LogMainFile()) = saved;

        std::string text;
        std::fseek(scratch, 0, SEEK_SET);
        char buffer[512];
        for (std::size_t n; (n = std::fread(buffer, 1, sizeof(buffer), scratch)) > 0;)
        {
            text.append(buffer, n);
        }
        std::fclose(scratch);
        std::filesystem::remove(path);

        std::string const marker = "ERROR:";
        std::vector<std::string> errors;
        for (std::size_t at = text.find(marker); at != std::string::npos; at = text.find(marker, at))
        {
            at += marker.size();
            std::size_t const end = text.find('\n', at);
            errors.push_back(text.substr(at, end == std::string::npos ? std::string::npos : end - at));
        }
        return errors;
    }

    /// The error lines about a periodic trigger, out of everything the load reports.
    std::vector<std::string> TriggerErrors(std::vector<std::string> const& errors)
    {
        std::vector<std::string> found;
        for (std::string const& error : errors)
        {
            if (error.find("periodic trigger") != std::string::npos)
            {
                found.push_back(error);
            }
        }
        return found;
    }
}

TEST(CreatureAddonTriggerCheck_AnAuraWhoseTriggerSpellIsMissingIsReportedOnce)
{
    SeedSpells();
    std::vector<std::string> const errors = ErrorsLoggedBy([]()
    {
        Db db;
        db.query.Answer("SELECT `entry`", FakeRows{
            FakeRow{"990801", "0", "0", "1", "0", "0", "0", "990701"},
            FakeRow{"990802", "0", "0", "1", "0", "0", "0", "990702 990701"},
        });
        db.query.Answer("SELECT `guid`", FakeRows{
            FakeRow{"990901", "0", "0", "1", "0", "0", "0", "990701"},
        });
        sObjectMgr.LoadCreatureAddons();
    });

    std::vector<std::string> const reported = TriggerErrors(errors);
    REQUIRE(reported.size() == 1);
    CHECK_STR(reported[0], "Creature (Entry: 990801) has spell 990701 defined in `auras` field in `creature_template_addon`, "
                           "whose periodic trigger in effect 0 names spell 990799, which neither Spell.dbc nor `spell_template` holds.");

    CreatureDataAddon const* addon = ObjectMgr::GetCreatureTemplateAddon(990801);
    REQUIRE(addon != NULL);
    REQUIRE(addon->auras != NULL);
    CHECK_EQ(addon->auras[0], kAuraWithMissingTrigger);
    CHECK_EQ(addon->auras[1], 0u);
}

TEST(CreatureAddonTriggerCheck_AnAuraWhoseTriggerSpellExistsIsNotReported)
{
    SeedSpells();
    std::vector<std::string> const errors = ErrorsLoggedBy([]()
    {
        Db db;
        db.query.Answer("SELECT `entry`", FakeRows{
            FakeRow{"990803", "0", "0", "1", "0", "0", "0", "990702"},
        });
        db.query.Answer("SELECT `guid`", FakeRows{
            FakeRow{"990902", "0", "0", "1", "0", "0", "0", ""},
        });
        sObjectMgr.LoadCreatureAddons();
    });

    CHECK(TriggerErrors(errors).empty());
    CreatureDataAddon const* addon = ObjectMgr::GetCreatureTemplateAddon(990803);
    REQUIRE(addon != NULL);
    REQUIRE(addon->auras != NULL);
    CHECK_EQ(addon->auras[0], kAuraWithKnownTrigger);
}
