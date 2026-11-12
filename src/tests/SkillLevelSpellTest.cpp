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

/// The spells a class learns with its skill line at a level: the list of nine, the store that refuses
/// every other spell, and the rule that learns a listed spell once the character reaches its base level.
#include "TestHarness.h"
#include "data/SkillLevelSpellStore.h"

#include <algorithm>
#include <set>
#include <vector>

namespace
{
    uint32 const RACE_HUMAN = 1 << 0;
    uint32 const RACE_ORC = 1 << 1;
    uint32 const CLASS_MASK_WARRIOR = 1 << 0;
    uint32 const CLASS_MASK_HUNTER = 1 << 2;
    uint32 const CLASS_MASK_ROGUE = 1 << 3;
    uint32 const CLASS_MASK_PRIEST = 1 << 4;
    uint32 const CLASS_MASK_DEATH_KNIGHT = 1 << 5;
    uint32 const CLASS_MASK_MAGE = 1 << 7;
    uint32 const CLASS_MASK_WARLOCK = 1 << 8;

    /// The learned-with-skill rows of the nine spells as SkillLineAbility.dbc and SpellLevels.dbc of
    /// build 15595 hold them; Holy Focus has two.
    SkillLevelSpell const LISTED[] =
    {
        { 2842,  253, 0, CLASS_MASK_ROGUE,        10 },     // Poisons, Assassination
        { 79327, 38,  0, CLASS_MASK_ROGUE,        3  },     // Sinister Strike Enabler, Combat
        { 56816, 771, 0, CLASS_MASK_DEATH_KNIGHT, 67 },     // Rune Strike, Frost
        { 84733, 56,  0, CLASS_MASK_PRIEST,       1  },     // Holy Focus, Holy
        { 84733, 804, 0, CLASS_MASK_PRIEST,       1  },     // Holy Focus, Priest - General
        { 82945, 51,  0, CLASS_MASK_HUNTER,       22 },     // Immolation Trap (Trap Launcher), Survival
        { 60192, 51,  0, CLASS_MASK_HUNTER,       28 },     // Freezing Trap (Trap Launcher), Survival
        { 82939, 51,  0, CLASS_MASK_HUNTER,       38 },     // Explosive Trap (Trap Launcher), Survival
        { 82941, 51,  0, CLASS_MASK_HUNTER,       46 },     // Ice Trap (Trap Launcher), Survival
        { 82948, 51,  0, CLASS_MASK_HUNTER,       66 },     // Snake Trap (Trap Launcher), Survival
    };

    /// Learned-with-skill rows of the same build that a rule by skill line would also learn: talent,
    /// variant and proc forms, a pet's ability, a trainer's spell and the armour skills.
    SkillLevelSpell const UNLISTED[] =
    {
        { 88684,  56,  0, 0,                       20 },    // Holy Word: Serenity (Chakra)
        { 88685,  56,  0, 0,                       1  },    // Holy Word: Sanctuary (Chakra)
        { 101062, 56,  0, 0,                       3  },    // Flash Heal variant
        { 585,    56,  0, CLASS_MASK_PRIEST,       1  },    // Smite
        { 92315,  8,   0, CLASS_MASK_MAGE,         10 },    // Pyroblast!
        { 86213,  355, 0, CLASS_MASK_WARLOCK,      1  },    // Soul Swap Exhale
        { 89751,  761, 0, CLASS_MASK_WARLOCK,      45 },    // Felstorm, Pet - Felguard
        { 45477,  771, 0, CLASS_MASK_DEATH_KNIGHT, 55 },    // Icy Touch
        { 59921,  771, 0, CLASS_MASK_DEATH_KNIGHT, 0  },    // Frost Fever
        { 61455,  771, 0, CLASS_MASK_DEATH_KNIGHT, 0  },    // Runic Focus
        { 48266,  771, 0, CLASS_MASK_DEATH_KNIGHT, 55 },    // Frost Presence
        { 750,    293, 0, 0x23,                    0  },    // Plate Mail
        { 8737,   413, 0, 0x67,                    0  },    // Mail
    };

    /// A store fed every row above, as the loader would be if it read them all.
    SkillLevelSpellStore FedStore()
    {
        SkillLevelSpellStore store;
        for (SkillLevelSpell const& row : LISTED)
        {
            store.Add(row);
        }
        for (SkillLevelSpell const& row : UNLISTED)
        {
            store.Add(row);
        }
        return store;
    }

    std::function<bool(uint32)> SkillLines(std::set<uint32> const& skills)
    {
        return [skills](uint32 skillLine) { return skills.count(skillLine) != 0; };
    }

    bool Contains(std::vector<uint32> const& spells, uint32 spellId)
    {
        return std::find(spells.begin(), spells.end(), spellId) != spells.end();
    }
}

TEST(SkillLevelSpell_TheListHoldsExactlyTheNineSpells)
{
    std::array<uint32, 9> const& list = SkillLevelSpellStore::Allowlist();
    std::set<uint32> const listed(list.begin(), list.end());
    std::set<uint32> const expected = { 2842, 56816, 84733, 79327, 82939, 60192, 82941, 82945, 82948 };
    CHECK(listed == expected);
    CHECK_EQ(listed.size(), std::size_t(9));

    for (uint32 spellId : expected)
    {
        CHECK(SkillLevelSpellStore::IsAllowlisted(spellId));
    }

    // The store refuses every row whose spell is off the list, and holds the ten rows of the nine.
    SkillLevelSpellStore store;
    for (SkillLevelSpell const& row : LISTED)
    {
        CHECK(store.Add(row));
    }
    for (SkillLevelSpell const& row : UNLISTED)
    {
        CHECK(!SkillLevelSpellStore::IsAllowlisted(row.spellId));
        CHECK(!store.Add(row));
    }
    CHECK_EQ(store.Rows().size(), std::size_t(10));

    std::set<uint32> held;
    for (SkillLevelSpell const& row : store.Rows())
    {
        held.insert(row.spellId);
    }
    CHECK(held == expected);

    store.Clear();
    CHECK(store.Rows().empty());
}

TEST(SkillLevelSpell_ARogueLearnsPoisonsAtLevel10)
{
    SkillLevelSpellStore const store = FedStore();

    CHECK(Contains(store.SpellsToLearn(RACE_HUMAN, CLASS_MASK_ROGUE, 10, SkillLines({ 253 })), 2842));
    CHECK(!Contains(store.SpellsToLearn(RACE_HUMAN, CLASS_MASK_ROGUE, 9, SkillLines({ 253 })), 2842));

    // Without the skill line it is not learned.
    CHECK(!Contains(store.SpellsToLearn(RACE_HUMAN, CLASS_MASK_ROGUE, 10, SkillLines({ 38 })), 2842));
}

TEST(SkillLevelSpell_EachSpellIsLearnedAtItsBaseLevelAndNotOneBelow)
{
    SkillLevelSpellStore const store = FedStore();

    for (SkillLevelSpell const& row : LISTED)
    {
        CHECK(Contains(store.SpellsToLearn(RACE_ORC, row.classMask, row.baseLevel, SkillLines({ row.skillLine })), row.spellId));
        CHECK(!Contains(store.SpellsToLearn(RACE_ORC, row.classMask, row.baseLevel - 1, SkillLines({ row.skillLine })), row.spellId));
    }

    // A hunter's traps come one by one: at 38 three are learned, Ice Trap and Snake Trap not yet.
    std::vector<uint32> const hunter = store.SpellsToLearn(RACE_ORC, CLASS_MASK_HUNTER, 38, SkillLines({ 51 }));
    CHECK_EQ(hunter.size(), std::size_t(3));
    CHECK(Contains(hunter, 82945));
    CHECK(Contains(hunter, 60192));
    CHECK(Contains(hunter, 82939));
}

TEST(SkillLevelSpell_TalentProcPetAndArmourSpellsAreNeverLearned)
{
    SkillLevelSpellStore const store = FedStore();

    // A priest without Chakra, with both skill lines of Holy Focus, learns Holy Focus once and nothing else.
    std::vector<uint32> const priest = store.SpellsToLearn(RACE_HUMAN, CLASS_MASK_PRIEST, 85, SkillLines({ 56, 804 }));
    CHECK_EQ(priest.size(), std::size_t(1));
    CHECK(Contains(priest, 84733));
    CHECK(!Contains(priest, 88684));
    CHECK(!Contains(priest, 88685));
    CHECK(!Contains(priest, 101062));

    // A mage does not get Pyroblast!.
    CHECK(store.SpellsToLearn(RACE_HUMAN, CLASS_MASK_MAGE, 85, SkillLines({ 8 })).empty());

    // A warlock does not get the felguard's Felstorm, nor Soul Swap Exhale.
    CHECK(store.SpellsToLearn(RACE_HUMAN, CLASS_MASK_WARLOCK, 85, SkillLines({ 355, 761 })).empty());

    // A warrior at 40 with the plate skill does not get Plate Mail from this rule.
    CHECK(store.SpellsToLearn(RACE_HUMAN, CLASS_MASK_WARRIOR, 40, SkillLines({ 293, 413 })).empty());

    // A death knight learns Rune Strike alone from the Frost line.
    std::vector<uint32> const deathKnight = store.SpellsToLearn(RACE_HUMAN, CLASS_MASK_DEATH_KNIGHT, 85, SkillLines({ 771 }));
    CHECK_EQ(deathKnight.size(), std::size_t(1));
    CHECK(Contains(deathKnight, 56816));
}

TEST(SkillLevelSpell_ARowOfAnotherClassOrRaceIsNotLearned)
{
    SkillLevelSpellStore const store = FedStore();

    // A rogue given the hunter's Survival line gets no trap.
    CHECK(store.SpellsToLearn(RACE_HUMAN, CLASS_MASK_ROGUE, 85, SkillLines({ 51 })).empty());

    // A row limited to orcs is not learned by a human.
    SkillLevelSpellStore orcOnly;
    CHECK(orcOnly.Add({ 2842, 253, RACE_ORC, CLASS_MASK_ROGUE, 10 }));
    CHECK(orcOnly.SpellsToLearn(RACE_HUMAN, CLASS_MASK_ROGUE, 10, SkillLines({ 253 })).empty());
    CHECK(Contains(orcOnly.SpellsToLearn(RACE_ORC, CLASS_MASK_ROGUE, 10, SkillLines({ 253 })), 2842));
}
