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

/// The two answers Unit's kill and heal credit ask: a creature, through a Creature reference, names
/// the unit that summoned a temporary summon; a unit, through a Unit reference, says whether its
/// heals are logged and scored as its owner's (a totem's are, a statue's are not). The creatures
/// are built without update fields or a map, so they only answer and are destroyed.

#include "TestHarness.h"
#include "Creature.h"
#include "TemporarySummon.h"
#include "Totem.h"
#include "Unit.h"

#include <type_traits>

namespace
{
    /// A totem of a given type, as Totem::SetTypeBySummonSpell sets it from the summon spell.
    class TypedTotem : public Totem
    {
        public:
            explicit TypedTotem(TotemType type)
            {
                m_type = type;
            }
    };
}

static_assert(std::is_same<decltype(&Unit::CreditsHealToOwner), bool (Unit::*)() const>::value,
              "Unit declares the heal credit question, const");

TEST(CreatureCreditAnswers_ATemporarySummonNamesItsSummoner)
{
    ObjectGuid const byCreature(HIGHGUID_UNIT, uint32(1234), uint32(77));
    ObjectGuid const byPlayer(HIGHGUID_PLAYER, uint32(5));
    TemporarySummon creatureSummon(byCreature);
    TemporarySummon playerSummon(byPlayer);
    Creature const& first = creatureSummon;
    Creature const& second = playerSummon;
    Unit const& firstUnit = creatureSummon;

    CHECK(first.IsTemporarySummon());
    CHECK(first.GetSummonerGuid() == byCreature);
    CHECK(first.GetSummonerGuid().IsCreatureOrVehicle());
    CHECK(second.GetSummonerGuid() == byPlayer);
    CHECK(!second.GetSummonerGuid().IsCreatureOrVehicle());
    CHECK(!firstUnit.CreditsHealToOwner());
}

TEST(CreatureCreditAnswers_AnyOtherCreatureHasNoSummonerAndKeepsItsHeals)
{
    Creature plain(CREATURE_SUBTYPE_GENERIC);
    Creature const& creature = plain;
    Unit const& unit = plain;

    CHECK(!creature.IsTemporarySummon());
    CHECK(creature.GetSummonerGuid().IsEmpty());
    CHECK(!unit.CreditsHealToOwner());
}

TEST(CreatureCreditAnswers_ATotemsHealsAreItsOwnersButAStatuesAreNot)
{
    TypedTotem passive(TOTEM_PASSIVE);
    TypedTotem active(TOTEM_ACTIVE);
    TypedTotem statue(TOTEM_STATUE);
    Unit const& passiveUnit = passive;
    Unit const& activeUnit = active;
    Unit const& statueUnit = statue;
    Creature const& statueCreature = statue;

    CHECK(passiveUnit.IsTotem());
    CHECK(passiveUnit.CreditsHealToOwner());
    CHECK(activeUnit.CreditsHealToOwner());
    CHECK(!statueUnit.CreditsHealToOwner());
    CHECK(statueCreature.GetSummonerGuid().IsEmpty());
}
