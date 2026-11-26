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

/// Tests of priest/holy/CircleOfHealing.cpp: the rows the file registers, in the game's table too; the units a heal
/// picks, 5 and the glyph's amount; the glyph's amount read from the holder; the health percent and the most injured
/// living units first; which units a caster's heal may take, his raid or party members and their pets; and the
/// select-targets body at each of the heal's two target modes. The units are bare creatures (update fields allocated;
/// no world, map, template or AI) and players built as PriestEchoOfLightTest builds one (a session and
/// Object::_Create, never in a world), linked to a Group as Player::SetGroup links them, with no group row. A player's
/// health is never set: it reports to the group, which a bare player has no callbacks for. A pet stands in as a
/// creature of the pet subtype, its owner's pet guid set; its owner is never found, since a bare player is in no
/// world. The glyph (55675) is put into the spell store with its effect 0, a dummy aura, and taken out again.
/// Scenario 1227 records the heal's units in play, the pet among them.

#include "TestHarness.h"
#include "spells/classes/priest/holy/CircleOfHealing.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/auras/SpellAuras.h"
#include "data/ServerSpellStore.h"
#include "Auth/BigNumber.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Group.h"
#include "Player.h"
#include "WorldSession.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <vector>

namespace
{
    uint32 const CIRCLE_OF_HEALING = 34861;
    uint32 const CIRCLE_OF_HEALING_OLD_RANK = 49306;
    uint32 const GLYPH_OF_CIRCLE_OF_HEALING = 55675;
    uint32 const WILD_GROWTH = 48438;
    uint32 const ECHO_OF_LIGHT_HEAL = 77489;
    uint32 const DIVINE_HYMN_HEAL = 64844;

    // A creature with its update fields allocated and no world, map, template or AI; of the pet subtype when asked,
    // so Unit::IsPet answers true.
    class BareCreature : public Creature
    {
        public:
            explicit BareCreature(CreatureSubtype subtype = CREATURE_SUBTYPE_GENERIC) : Creature(subtype)
            {
                _InitValues();
            }

            // The holders HoldAura put in the map are the test's own, and leave the map first.
            ~BareCreature()
            {
                m_auras.Holders().clear();
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // Its guid: of the high part `high`, the entry `entry` and the counter `counter`.
            void TakeGuid(HighGuid high, uint32 entry, uint32 counter)
            {
                SetGuidValue(OBJECT_FIELD_GUID, ObjectGuid(high, entry, counter));
            }

            // Its health and maximum health; never called on a creature of the pet subtype, whose health would report
            // to an owner as a Pet's does.
            void Stand(uint32 health, uint32 maxHealth)
            {
                SetMaxHealth(maxHealth);
                SetHealth(health);
            }

            // Marks the creature dead, as Unit::IsDead reads it.
            void Die()
            {
                m_deathState = DEAD;
            }

            // Puts `holder` in the holder map under `spellId`, as Unit::AddSpellAuraHolder files it.
            void HoldAura(uint32 spellId, SpellAuraHolder& holder)
            {
                m_auras.AddHolder(spellId, &holder);
            }
    };

    // A player of guid `guidLow`, never in a world.
    class BarePlayer : public Player
    {
        public:
            BarePlayer(WorldSession& session, uint32 guidLow) : Player(&session)
            {
                Object::_Create(guidLow, 0, HIGHGUID_PLAYER);
            }

            // The holders HoldAura put in the map are the test's own, and leave the map first; the player leaves any
            // group the test linked it to.
            ~BarePlayer()
            {
                SetGroup(NULL);
                m_auras.Holders().clear();
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // Puts `holder` in the holder map under `spellId`, as Unit::AddSpellAuraHolder files it.
            void HoldAura(uint32 spellId, SpellAuraHolder& holder)
            {
                m_auras.AddHolder(spellId, &holder);
            }
    };

    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    // A Spell.dbc row with only its id, which the contexts name; it is not in the spell store.
    SpellEntry const& SpellRow(SpellRowStorage& storage, uint32 spellId)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        SpellEntry* row = reinterpret_cast<SpellEntry*>(storage.bytes);
        row->ID = spellId;
        return *row;
    }

    // The glyph, its effect 0 a dummy aura as the client's data holds it.
    ServerSpell s_glyph;

    // Puts the glyph into the spell store; its effect once.
    void StoreGlyph()
    {
        static bool stored = false;
        if (!stored)
        {
            s_glyph.effect = SpellEffectEntry();
            s_glyph.effect.SpellID = GLYPH_OF_CIRCLE_OF_HEALING;
            s_glyph.effect.EffectIndex = EFFECT_INDEX_0;
            s_glyph.effect.Effect = SPELL_EFFECT_APPLY_AURA;
            s_glyph.effect.EffectAura = SPELL_AURA_DUMMY;
            s_glyph.effect.EffectBasePoints = 1;
            s_glyph.effect.ImplicitTarget_0 = TARGET_SELF;
            AddSpellEffectEntry(&s_glyph.effect);
            stored = true;
        }
        s_glyph.Entry().ID = GLYPH_OF_CIRCLE_OF_HEALING;
        sSpellStore.SetEntry(GLYPH_OF_CIRCLE_OF_HEALING, &s_glyph.Entry());
    }

    // Takes it out of the spell store again.
    void ForgetGlyph()
    {
        sSpellStore.SetEntry(GLYPH_OF_CIRCLE_OF_HEALING, NULL);
    }

    // A holder of the glyph on `unit`, its caster, with its effect 0 aura at `amount`, or with no aura when `aura` is
    // false; the caller puts it in the unit's holder map and deletes it.
    SpellAuraHolder* GlyphHolder(Unit& unit, int32 amount, bool aura = true)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(&s_glyph.Entry(), &unit, NULL, NULL, NULL);
        if (aura)
        {
            holder->AddAura(CreateAura(&s_glyph.Entry(), EFFECT_INDEX_0, &amount, holder, &unit), EFFECT_INDEX_0);
        }
        return holder;
    }

    // Circle of Healing's select-targets row for `caster`'s cast of `spellId`, at the target mode `targetMode`, over
    // `targets`; the target limit it leaves in `maxTargets`.
    SpellHandlerOutcome<void> PickTargets(SpellHandlerRegistry const& registry, uint32 spellId, Unit& caster,
                                          uint32 targetMode, std::list<Unit*>& targets, uint32& maxTargets)
    {
        SpellRowStorage row;
        SelectTargetsContext ctx(caster, SpellRow(row, spellId), EFFECT_INDEX_0, targetMode, targets, maxTargets);
        return registry.Dispatch<SelectTargetsHook>(spellId, ctx);
    }
}

TEST(PriestCircleOfHealing_TheFileHoldsItsTwoSelectTargetsRowsInTheGame)
{
    // Two rows, the player's Circle of Healing and the creature's older rank, one body; no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterCircleOfHealing(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SelectTargetsHook::Key), std::size_t(2));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    REQUIRE(registry.Find<SelectTargetsHook>(CIRCLE_OF_HEALING) != NULL);
    CHECK(registry.Find<SelectTargetsHook>(CIRCLE_OF_HEALING_OLD_RANK) ==
          registry.Find<SelectTargetsHook>(CIRCLE_OF_HEALING));
    for (uint32 spellId : { CIRCLE_OF_HEALING, CIRCLE_OF_HEALING_OLD_RANK })
    {
        CHECK(game.Find<SelectTargetsHook>(spellId) == registry.Find<SelectTargetsHook>(CIRCLE_OF_HEALING));
    }

    // Wild Growth, which picks its units the same way in the engine, the glyph, Echo of Light's heal and Divine
    // Hymn's heal hold no select-targets row.
    for (uint32 spellId : { WILD_GROWTH, GLYPH_OF_CIRCLE_OF_HEALING, ECHO_OF_LIGHT_HEAL, DIVINE_HYMN_HEAL })
    {
        CHECK(game.Find<SelectTargetsHook>(spellId) == NULL);
    }
}

TEST(PriestCircleOfHealing_TheHealPicksFiveUnitsAndOneMoreForEachPointOfTheGlyph)
{
    // The client's 5, and the glyph's amount on top; an amount of 0 or less adds nothing.
    CHECK_EQ(Priest::CircleOfHealingUnits(0), uint32(5));
    CHECK_EQ(Priest::CircleOfHealingUnits(1), uint32(6));
    CHECK_EQ(Priest::CircleOfHealingUnits(3), uint32(8));
    for (int32 glyph : { -1, -5, INT32_MIN })
    {
        CHECK_EQ(Priest::CircleOfHealingUnits(glyph), uint32(5));
    }

    // The glyph's amount is the effect 0 amount of the holder he holds: none without one, none for a holder with no
    // aura there, 1 for the client's.
    StoreGlyph();
    {
        BareCreature priest;
        CHECK_EQ(Priest::CircleOfHealingGlyph(priest), int32(0));

        SpellAuraHolder* empty = GlyphHolder(priest, 1, false);
        priest.HoldAura(GLYPH_OF_CIRCLE_OF_HEALING, *empty);
        CHECK_EQ(Priest::CircleOfHealingGlyph(priest), int32(0));

        BareCreature glyphed;
        SpellAuraHolder* glyph = GlyphHolder(glyphed, 1);
        glyphed.HoldAura(GLYPH_OF_CIRCLE_OF_HEALING, *glyph);
        CHECK_EQ(Priest::CircleOfHealingGlyph(glyphed), int32(1));
        CHECK_EQ(Priest::CircleOfHealingUnits(Priest::CircleOfHealingGlyph(glyphed)), uint32(6));
        delete glyph;
        delete empty;
    }
    ForgetGlyph();
    CHECK(sSpellStore.LookupEntry(GLYPH_OF_CIRCLE_OF_HEALING) == NULL);
}

TEST(PriestCircleOfHealing_TheMostInjuredLivingUnitsComeFirstAsManyAsTheCount)
{
    // The health percent: health x 100 over the maximum, truncated; 0 for a maximum of 0; no overflow at the top of
    // the range.
    BareCreature none;
    CHECK_EQ(Priest::CircleOfHealingHealthPercent(none), uint32(0));
    BareCreature third;
    third.Stand(1, 3);
    CHECK_EQ(Priest::CircleOfHealingHealthPercent(third), uint32(33));
    BareCreature whole;
    whole.Stand(44019, 44019);
    CHECK_EQ(Priest::CircleOfHealingHealthPercent(whole), uint32(100));
    BareCreature huge;
    huge.Stand(4000000000u, 4294967295u);
    CHECK_EQ(Priest::CircleOfHealingHealthPercent(huge), uint32(93));

    // Seven units at 60, 20, 40, 20 (after the first 20), 80 percent, one dead at 10 and one of no maximum health
    // (0): 5 keeps the five lowest living ones, the two at 20 in their list order; the dead one is dropped.
    BareCreature at60, at20, at40, at20Later, at80, dead, empty;
    at60.Stand(600, 1000);
    at20.Stand(200, 1000);
    at40.Stand(400, 1000);
    at20Later.Stand(2000, 10000);
    at80.Stand(800, 1000);
    dead.Stand(100, 1000);
    dead.Die();
    std::list<Unit*> const all{ &at60, &at20, &at40, &at20Later, NULL, &at80, &dead, &empty };

    std::list<Unit*> units = all;
    Priest::CircleOfHealingMostInjured(units, 5);
    std::vector<Unit*> const five{ &empty, &at20, &at20Later, &at40, &at60 };
    CHECK(std::vector<Unit*>(units.begin(), units.end()) == five);

    // 6 keeps the 80 too; a count above the living units keeps them all; 0 keeps none.
    units = all;
    Priest::CircleOfHealingMostInjured(units, 6);
    CHECK_EQ(units.size(), std::size_t(6));
    CHECK(units.back() == &at80);
    units = all;
    Priest::CircleOfHealingMostInjured(units, 100);
    CHECK_EQ(units.size(), std::size_t(6));
    units = all;
    Priest::CircleOfHealingMostInjured(units, 0);
    CHECK(units.empty());
}

TEST(PriestCircleOfHealing_TheHealTakesHisRaidOrPartyMembersAndTheirPetsAlone)
{
    WorldSession session(1, "circle", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    Group group;
    BarePlayer priest(session, 1);
    BarePlayer member(session, 2);
    BarePlayer stranger(session, 3);
    BarePlayer alone(session, 4);
    priest.SetGroup(&group, 0);
    member.SetGroup(&group, 1);
    BareCreature creature;
    creature.TakeGuid(HIGHGUID_UNIT, 50249, 5);

    // In a group: himself and a member of any subgroup; not a player outside it, nor a creature.
    CHECK(Priest::CircleOfHealingTakes(priest, priest));
    CHECK(Priest::CircleOfHealingTakes(priest, member));
    CHECK(Priest::CircleOfHealingTakes(member, priest));
    CHECK(!Priest::CircleOfHealingTakes(priest, stranger));
    CHECK(!Priest::CircleOfHealingTakes(priest, creature));

    // A pet whose owner cannot be found, its owner guid the member's and the member's pet guid its own, is not taken:
    // the owner must be a player found in the world.
    BareCreature pet(CREATURE_SUBTYPE_PET);
    pet.TakeGuid(HIGHGUID_PET, 416, 7);
    pet.SetOwnerGuid(member.GetObjectGuid());
    member.SetPetGuid(pet.GetObjectGuid());
    CHECK(pet.IsPet());
    CHECK(!Priest::CircleOfHealingTakes(priest, pet));

    // Out of a group: himself alone.
    CHECK(Priest::CircleOfHealingTakes(alone, alone));
    CHECK(!Priest::CircleOfHealingTakes(alone, stranger));
    CHECK(!Priest::CircleOfHealingTakes(alone, priest));

    // A member who left the group is no longer taken.
    member.SetGroup(NULL);
    CHECK(!Priest::CircleOfHealingTakes(priest, member));
    CHECK(Priest::CircleOfHealingTakes(priest, priest));

    // A caster no player stands behind: itself and its current pet; not another pet, a creature or a player.
    BareCreature caster;
    caster.TakeGuid(HIGHGUID_UNIT, 27677, 1);
    BareCreature own(CREATURE_SUBTYPE_PET);
    own.TakeGuid(HIGHGUID_PET, 416, 8);
    BareCreature otherPet(CREATURE_SUBTYPE_PET);
    otherPet.TakeGuid(HIGHGUID_PET, 416, 9);
    caster.SetPetGuid(own.GetObjectGuid());
    CHECK(Priest::CircleOfHealingTakes(caster, caster));
    CHECK(Priest::CircleOfHealingTakes(caster, own));
    CHECK(!Priest::CircleOfHealingTakes(caster, otherPet));
    CHECK(!Priest::CircleOfHealingTakes(caster, creature));
    CHECK(!Priest::CircleOfHealingTakes(caster, priest));

    // A creature that is no pet is not taken as one, whatever guid its owner holds as its pet's.
    caster.SetPetGuid(creature.GetObjectGuid());
    CHECK(!Priest::CircleOfHealingTakes(caster, creature));
}

TEST(PriestCircleOfHealing_TheRowKeepsTheMostInjuredOfHisGroupAtTheAreaModeAlone)
{
    SpellHandlerRegistry registry;
    Priest::RegisterCircleOfHealing(registry);

    // A creature's cast: of the area's units, itself and its pet, the pet (no maximum health, 0%) first; the other
    // creature is dropped. The destination's mode leaves the list as it is. The target limit is not touched.
    BareCreature caster;
    caster.TakeGuid(HIGHGUID_UNIT, 27677, 1);
    caster.Stand(500, 1000);
    BareCreature pet(CREATURE_SUBTYPE_PET);
    pet.TakeGuid(HIGHGUID_PET, 416, 2);
    caster.SetPetGuid(pet.GetObjectGuid());
    BareCreature other;
    other.TakeGuid(HIGHGUID_UNIT, 50249, 3);
    other.Stand(100, 1000);
    for (uint32 spellId : { CIRCLE_OF_HEALING, CIRCLE_OF_HEALING_OLD_RANK })
    {
        std::list<Unit*> targets{ &other, &caster, &pet };
        uint32 maxTargets = 0;
        CHECK(PickTargets(registry, spellId, caster, TARGET_DUELVSPLAYER_COORDINATES, targets, maxTargets)
                  .IsContinue());
        CHECK_EQ(targets.size(), std::size_t(3));
        CHECK(PickTargets(registry, spellId, caster, TARGET_ALL_FRIENDLY_UNITS_IN_AREA, targets, maxTargets)
                  .IsContinue());
        std::vector<Unit*> const kept{ &pet, &caster };
        CHECK(std::vector<Unit*>(targets.begin(), targets.end()) == kept);
        CHECK_EQ(maxTargets, uint32(0));
    }

    // A player's cast among seven members of his group and a stranger, all at one percent (a bare player's health is
    // never set): the stranger is dropped and 5 members are kept, in the list's order; with the glyph's 1, 6.
    WorldSession session(1, "circle", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    Group group;
    std::vector<BarePlayer*> members;
    for (uint32 low = 1; low <= 7; ++low)
    {
        members.push_back(new BarePlayer(session, low));
        members.back()->SetGroup(&group, low <= 4 ? 0 : 1);
    }
    BarePlayer stranger(session, 8);
    BarePlayer& priest = *members[0];
    std::list<Unit*> targets{ &stranger };
    targets.insert(targets.end(), members.begin(), members.end());
    uint32 maxTargets = 0;
    CHECK(PickTargets(registry, CIRCLE_OF_HEALING, priest, TARGET_ALL_FRIENDLY_UNITS_IN_AREA, targets, maxTargets)
              .IsContinue());
    CHECK(std::vector<Unit*>(targets.begin(), targets.end()) ==
          std::vector<Unit*>(members.begin(), members.begin() + 5));

    StoreGlyph();
    SpellAuraHolder* glyph = GlyphHolder(priest, 1);
    priest.HoldAura(GLYPH_OF_CIRCLE_OF_HEALING, *glyph);
    targets.assign(members.begin(), members.end());
    targets.push_front(&stranger);
    CHECK(PickTargets(registry, CIRCLE_OF_HEALING, priest, TARGET_ALL_FRIENDLY_UNITS_IN_AREA, targets, maxTargets)
              .IsContinue());
    CHECK(std::vector<Unit*>(targets.begin(), targets.end()) ==
          std::vector<Unit*>(members.begin(), members.begin() + 6));
    CHECK_EQ(maxTargets, uint32(0));
    delete glyph;
    ForgetGlyph();

    for (BarePlayer* member : members)
    {
        delete member;
    }
}
