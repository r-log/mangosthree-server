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

#include "TestHarness.h"

#include "spells/data/SpellMgr.h"

TEST(SpellTables_TheGooberSpellsAnswerTheirReplacementsAndAnyOtherIdItself)
{
    CHECK_EQ(GetGooberCastSpell(34448), uint32(26566));
    CHECK_EQ(GetGooberCastSpell(34452), uint32(26572));
    CHECK_EQ(GetGooberCastSpell(37639), uint32(36326));
    CHECK_EQ(GetGooberCastSpell(45367), uint32(45371));
    CHECK_EQ(GetGooberCastSpell(45370), uint32(45368));
    CHECK_EQ(GetGooberCastSpell(12345), uint32(12345));
    CHECK_EQ(GetGooberCastSpell(0), uint32(0));
}

TEST(SpellTables_FerociousInspirationAnswersItsPetAuraAndNothingElse)
{
    CHECK_EQ(GetFerociousInspirationPetAura(34460), uint32(75447));
    // The two lower ranks are not 4.3.4 spells: no player knows them.
    CHECK_EQ(GetFerociousInspirationPetAura(34455), uint32(0));
    CHECK_EQ(GetFerociousInspirationPetAura(34459), uint32(0));
    CHECK_EQ(GetFerociousInspirationPetAura(12345), uint32(0));
}

TEST(SpellTables_TheFiveCoreHandledMasteryProcsAreTheSetAndNoOther)
{
    for (uint32 spellId : { 76659u, 76669u, 76672u, 76838u, 77222u })
    {
        CHECK(IsCoreHandledMasteryProc(spellId));
    }
    for (uint32 spellId : { 0u, 76658u, 76671u, 77223u, 12345u })
    {
        CHECK(!IsCoreHandledMasteryProc(spellId));
    }
}

TEST(SpellTables_SoulstoneAnswersItsResurrectionAndNothingElse)
{
    CHECK_EQ(GetSoulstoneResurrectionSpell(20707), uint32(3026));
    // The higher ranks are not 4.3.4 spells: no player carries their aura.
    for (uint32 spellId : { 20762u, 20763u, 20764u, 20765u, 27239u, 47883u, 12345u })
    {
        CHECK_EQ(GetSoulstoneResurrectionSpell(spellId), uint32(0));
    }
}
