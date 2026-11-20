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

/// A creature's base mana (CreatureInfo::BaseManaAt): the player base-mana table of its class at its level, whatever
/// its power multiplier, and 0 for a class the table gives no mana or holds no row of. The table is seeded with rows of
/// the 15595 gtOCTBaseMPByClass.dbc, each at its row id (class - 1) x 100 + level - 1, through DBCStorage::SetEntry,
/// the path GlyphMgrTest and TalentMgrTest seed their stores by; no other test in this binary reads this store.

#include "TestHarness.h"
#include "DBCStores.h"
#include "data/CreatureRecords.h"

namespace
{
    struct GtRow
    {
        uint32 id;
        GtOCTBaseMPByClassEntry entry;
    };

    // Rows of gtOCTBaseMPByClass.dbc (15595): mage (class 8) at 56, 60, 84, 85, 86 and 100, paladin (class 2) at 55,
    // 56, 83, 84 and 85, warrior (class 1) and rogue (class 4) at 85.
    GtRow s_rows[] =
    {
        { 755, { 1159.0f } }, { 759, { 1213.0f } }, { 783, { 12464.0f } }, { 784, { 17418.0f } },
        { 785, { 3885.0f } }, { 799, { 5323.0f } },
        { 154, { 1392.0f } }, { 155, { 1419.0f } }, { 182, { 11993.0f } }, { 183, { 16760.0f } },
        { 184, { 23422.0f } },
        { 84, { 0.0f } }, { 384, { 0.0f } },
    };

    void SeedTable()
    {
        static bool seeded = false;
        if (seeded)
        {
            return;
        }
        seeded = true;
        for (GtRow& row : s_rows)
        {
            sGtOCTBaseMPByClassStore.SetEntry(row.id, &row.entry);
        }
    }

    /// A template of `unitClass` whose power multiplier is `multiplier`, every other field zero.
    CreatureInfo Template(uint32 unitClass, float multiplier)
    {
        CreatureInfo info = {};
        info.UnitClass = unitClass;
        info.PowerMultiplier = multiplier;
        return info;
    }
}

TEST(CreatureBaseMana_TheClassRowAtTheLevelOfThePlayerTable)
{
    SeedTable();

    // the 4.0.x captures' BASE_MANA: mage 56 and 60, paladin 55, 56 and 83
    CHECK_EQ(Template(CLASS_MAGE, 1.0f).BaseManaAt(56), uint32(1159));
    CHECK_EQ(Template(CLASS_MAGE, 1.0f).BaseManaAt(60), uint32(1213));
    CHECK_EQ(Template(CLASS_PALADIN, 1.0f).BaseManaAt(55), uint32(1392));
    CHECK_EQ(Template(CLASS_PALADIN, 1.0f).BaseManaAt(56), uint32(1419));
    CHECK_EQ(Template(CLASS_PALADIN, 1.0f).BaseManaAt(83), uint32(11993));

    // level 85, and the rows on either side of it
    CHECK_EQ(Template(CLASS_MAGE, 1.0f).BaseManaAt(84), uint32(12464));
    CHECK_EQ(Template(CLASS_MAGE, 1.0f).BaseManaAt(85), uint32(17418));
    CHECK_EQ(Template(CLASS_MAGE, 1.0f).BaseManaAt(86), uint32(3885));
    CHECK_EQ(Template(CLASS_PALADIN, 1.0f).BaseManaAt(84), uint32(16760));
    CHECK_EQ(Template(CLASS_PALADIN, 1.0f).BaseManaAt(85), uint32(23422));

    // a level past the table's last reads the last
    CHECK_EQ(Template(CLASS_MAGE, 1.0f).BaseManaAt(100), uint32(5323));
    CHECK_EQ(Template(CLASS_MAGE, 1.0f).BaseManaAt(255), uint32(5323));
}

TEST(CreatureBaseMana_ThePowerMultiplierScalesNone)
{
    SeedTable();

    // the Mirror Image's images (class 8, multiplier 2) and a paladin at 30 times his class's mana, as in the captures
    CHECK_EQ(Template(CLASS_MAGE, 2.0f).BaseManaAt(85), uint32(17418));
    CHECK_EQ(Template(CLASS_MAGE, 0.5f).BaseManaAt(85), uint32(17418));
    CHECK_EQ(Template(CLASS_PALADIN, 30.0f).BaseManaAt(55), uint32(1392));
    CHECK_EQ(Template(CLASS_PALADIN, 0.0f).BaseManaAt(55), uint32(1392));
}

TEST(CreatureBaseMana_AClassWithNoManaOrNoRowHasNone)
{
    SeedTable();

    // warrior and rogue: the table's rows hold 0
    CHECK_EQ(Template(CLASS_WARRIOR, 1.0f).BaseManaAt(85), uint32(0));
    CHECK_EQ(Template(CLASS_ROGUE, 1.0f).BaseManaAt(85), uint32(0));
    CHECK_EQ(Template(CLASS_WARRIOR, 5.0f).BaseManaAt(85), uint32(0));

    // no class, a class past the table's last, and a row the table does not hold
    CHECK_EQ(Template(0, 1.0f).BaseManaAt(85), uint32(0));
    CHECK_EQ(Template(MAX_CLASSES, 1.0f).BaseManaAt(85), uint32(0));
    CHECK_EQ(Template(CLASS_MAGE, 1.0f).BaseManaAt(70), uint32(0));
}
