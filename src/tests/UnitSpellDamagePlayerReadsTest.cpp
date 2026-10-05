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

/// What CalculateSpellDamage reads of a player: whether an armor specialization spell fits it, its
/// combo points and the guid of its combo target.
///
/// Unit declares the three protected and `const`, the combo target returned by value. A Unit that
/// is not a Player has no armor specialization to fail, no combo points and no combo target: a
/// Creature probe with its update fields allocated (no map, no AI, no auras, no session) makes the
/// three public with using-declarations and is asked through a const reference; the armor
/// specialization fits for no spell, an armor specialization spell and another spell, the combo
/// points are 0 and the combo target is an empty guid, whatever target the creature holds, and no
/// update field changes. A Player cannot be built in this binary (it needs a WorldSession and a
/// map), so Player's bodies are not run here. The static_asserts pin Unit's declarations through
/// the probe and Player's own declarations with Unit's exact signatures, the combo target returned
/// by value on both sides, that a call through a Unit does not compile and that a call through a
/// Player compiles. That Player's three overrides are `final` is pinned by the cast proof's text of
/// Player.h.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated; the three player reads of the spell damage are
    /// public here.
    class SpellDamageReadsUnit : public Creature
    {
        public:
            using Unit::FitArmorSpecializationRules;
            using Unit::GetComboPoints;
            using Unit::GetComboTargetGuid;

            static_assert(std::is_same<decltype(&SpellDamageReadsUnit::FitArmorSpecializationRules),
                                       bool (Unit::*)(SpellEntry const*) const>::value,
                          "Unit declares the armor specialization test, const");
            static_assert(std::is_same<decltype(&SpellDamageReadsUnit::GetComboPoints), uint8 (Unit::*)() const>::value,
                          "Unit declares the combo points, const");
            static_assert(std::is_same<decltype(&SpellDamageReadsUnit::GetComboTargetGuid),
                                       ObjectGuid (Unit::*)() const>::value,
                          "Unit declares the combo target, by value, const");

            SpellDamageReadsUnit() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~SpellDamageReadsUnit()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            std::vector<uint32> Fields() const
            {
                std::vector<uint32> fields;
                for (uint16 i = 0; i < GetValuesCount(); ++i)
                {
                    fields.push_back(GetUInt32Value(i));
                }
                return fields;
            }
    };

    // Spell.dbc rows as the loader makes them (SpellEntry has no default constructor: it declares
    // a private copy constructor).
    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    SpellRowStorage s_rows[2] = {};

    SpellEntry const* Row(size_t slot, uint32 id, uint32 attributesEx8)
    {
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_rows[slot].bytes);
        row->ID = id;
        row->AttributesExH = attributesEx8;
        return row;
    }

    template<class T>
    auto CallsFitArmorSpecializationRules(int)
        -> decltype(std::declval<T const&>().FitArmorSpecializationRules(static_cast<SpellEntry const*>(NULL)),
                    std::true_type());
    template<class T>
    std::false_type CallsFitArmorSpecializationRules(...);

    template<class T>
    auto CallsGetComboPoints(int) -> decltype(std::declval<T const&>().GetComboPoints(), std::true_type());
    template<class T>
    std::false_type CallsGetComboPoints(...);

    template<class T>
    auto CallsGetComboTargetGuid(int) -> decltype(std::declval<T const&>().GetComboTargetGuid(), std::true_type());
    template<class T>
    std::false_type CallsGetComboTargetGuid(...);
}

static_assert(std::is_same<decltype(&Player::FitArmorSpecializationRules),
                           bool (Player::*)(SpellEntry const*) const>::value,
              "Player tests its armor specialization, const");
static_assert(std::is_same<decltype(&Player::GetComboPoints), uint8 (Player::*)() const>::value,
              "Player returns its combo points, const");
static_assert(std::is_same<decltype(&Player::GetComboTargetGuid), ObjectGuid (Player::*)() const>::value,
              "Player returns its combo target, by value, const");
static_assert(std::is_same<decltype(std::declval<Player const&>().GetComboTargetGuid()), ObjectGuid>::value,
              "a call of Player's combo target yields an ObjectGuid value, not a reference");

static_assert(!decltype(CallsFitArmorSpecializationRules<Unit>(0))::value,
              "Unit's armor specialization test is protected: a call through a Unit does not compile");
static_assert(decltype(CallsFitArmorSpecializationRules<Player>(0))::value,
              "Player's armor specialization test is public: a call through a const Player compiles");
static_assert(!decltype(CallsGetComboPoints<Unit>(0))::value,
              "Unit's combo points are protected: a call through a Unit does not compile");
static_assert(decltype(CallsGetComboPoints<Player>(0))::value,
              "Player's combo points are public: a call through a const Player compiles");
static_assert(!decltype(CallsGetComboTargetGuid<Unit>(0))::value,
              "Unit's combo target is protected: a call through a Unit does not compile");
static_assert(decltype(CallsGetComboTargetGuid<Player>(0))::value,
              "Player's combo target is public: a call through a const Player compiles");

TEST(UnitSpellDamagePlayerReads_ACreatureHasNoArmorSpecializationToFail)
{
    SpellDamageReadsUnit creature;
    SpellDamageReadsUnit const& unit = creature;
    std::vector<uint32> const fields = creature.Fields();
    CHECK(!fields.empty());

    CHECK(unit.FitArmorSpecializationRules(NULL));
    CHECK(unit.FitArmorSpecializationRules(Row(0, 86537, SPELL_ATTR_EX8_ARMOR_SPECIALIZATION)));
    CHECK(unit.FitArmorSpecializationRules(Row(1, 133, 0)));

    CHECK(creature.Fields() == fields);
}

TEST(UnitSpellDamagePlayerReads_ACreatureHasNoComboPoints)
{
    SpellDamageReadsUnit creature;
    SpellDamageReadsUnit const& unit = creature;
    std::vector<uint32> const fields = creature.Fields();

    CHECK(unit.GetComboPoints() == 0);
    CHECK(creature.Fields() == fields);

    ObjectGuid const target(HIGHGUID_UNIT, uint32(1), uint32(1));
    creature.SetTargetGuid(target);
    CHECK(unit.GetComboPoints() == 0);
}

TEST(UnitSpellDamagePlayerReads_ACreatureHasNoComboTarget)
{
    SpellDamageReadsUnit creature;
    SpellDamageReadsUnit const& unit = creature;

    CHECK(unit.GetComboTargetGuid().IsEmpty());
    CHECK(unit.GetComboTargetGuid() == ObjectGuid());

    ObjectGuid const target(HIGHGUID_UNIT, uint32(1), uint32(1));
    creature.SetTargetGuid(target);
    std::vector<uint32> const fields = creature.Fields();
    CHECK(creature.GetTargetGuid() == target);
    CHECK(unit.GetComboTargetGuid().IsEmpty());
    CHECK(unit.GetComboTargetGuid() != target);
    CHECK(creature.Fields() == fields);
}
