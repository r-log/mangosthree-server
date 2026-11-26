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

/// Tests of priest/holy/CircleOfHealing.cpp and of the select-targets context's raid or party pick it calls: the rows
/// the file registers, in the game's table too; the units a heal picks, 5 and the glyph's amount, read from the holder;
/// the context's pick, which puts the engine's raid or party pick by health in place of the list (none outside a
/// cast); and the row at each of the heal's two target modes. The units are bare creatures (update fields allocated;
/// no world, map, template or AI); a creature in no group stands for itself in the engine's pick, so it is the one
/// unit a pick finds, and its pet, found only on a map, is never found. The cast is a Spell of the test's own row,
/// put into the spell store and taken out again, as SpellHandlerRegistryTest builds one. The glyph (55675) is put
/// into the spell store with its effect 0, a dummy aura, and taken out again. Scenario 1227 records the heal's units in
/// play, a group's members and a member's pet among them.

#include "TestHarness.h"
#include "spells/classes/priest/holy/CircleOfHealing.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/auras/SpellAuras.h"
#include "spells/pipeline/Spell.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

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

    /// The test's own spell row, the cast's: no Spell.dbc row holds this id.
    uint32 const CAST_ROW = 93861;

    /// The radius Spell::SetTargetMap reckons for 34861's effect: SpellRadius.dbc row 10.
    float const CIRCLE_RADIUS = 30.0f;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
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

            // Its health and maximum health.
            void Stand(uint32 health, uint32 maxHealth)
            {
                SetMaxHealth(maxHealth);
                SetHealth(health);
            }

            // Marks the creature dead, as Unit::IsDead reads it, or alive again.
            void Die(bool dead = true)
            {
                m_deathState = dead ? DEAD : ALIVE;
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

    // The cast's row, put into the spell store, which a Spell's constructor checks it against.
    SpellRowStorage s_castRow;

    SpellEntry const* StoreCastRow()
    {
        SpellRow(s_castRow, CAST_ROW);
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_castRow.bytes);
        sSpellStore.SetEntry(CAST_ROW, row);
        return row;
    }

    // Takes it out of the spell store again.
    void ForgetCastRow()
    {
        sSpellStore.SetEntry(CAST_ROW, NULL);
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

    // The units of `targets`, in their order.
    std::vector<Unit*> UnitsOf(std::list<Unit*> const& targets)
    {
        return std::vector<Unit*>(targets.begin(), targets.end());
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

TEST(PriestCircleOfHealing_TheContextPutsTheEnginesRaidOrPartyPickByHealthInPlaceOfTheList)
{
    SpellEntry const* row = StoreCastRow();
    {
        BareCreature caster;
        caster.TakeGuid(HIGHGUID_UNIT, 27677, 1);
        caster.Stand(500, 1000);
        BareCreature other;
        other.TakeGuid(HIGHGUID_UNIT, 50249, 2);
        other.Stand(100, 1000);
        uint32 maxTargets = 0;
        std::vector<Unit*> const alone{ &caster };

        // A context built outside a cast leaves the list empty.
        std::list<Unit*> targets{ &other, &caster };
        SelectTargetsContext outside(caster, *row, EFFECT_INDEX_0, TARGET_ALL_FRIENDLY_UNITS_IN_AREA, targets,
                                     maxTargets);
        outside.RaidOrPartyByHealthWithPets(5);
        CHECK(targets.empty());

        // With the cast: the area's units give way to the engine's pick. A creature in no group stands for itself, the
        // centre when the cast has no unit target, so it alone is picked; the other creature, though lower in health,
        // is no member. The target limit is not touched.
        Spell cast(&caster, row, true);
        targets.assign({ &other, &caster });
        SelectTargetsContext noTarget(caster, *row, EFFECT_INDEX_0, TARGET_ALL_FRIENDLY_UNITS_IN_AREA, targets,
                                      maxTargets, NULL, &cast, CIRCLE_RADIUS);
        noTarget.RaidOrPartyByHealthWithPets(5);
        CHECK(UnitsOf(targets) == alone);
        CHECK_EQ(maxTargets, uint32(0));

        // The same with the caster as the cast's unit target; a count of 0 picks none.
        targets.assign({ &other, &caster });
        SelectTargetsContext atHimself(caster, *row, EFFECT_INDEX_0, TARGET_ALL_FRIENDLY_UNITS_IN_AREA, targets,
                                       maxTargets, &caster, &cast, CIRCLE_RADIUS);
        atHimself.RaidOrPartyByHealthWithPets(6);
        CHECK(UnitsOf(targets) == alone);
        targets.assign({ &other, &caster });
        atHimself.RaidOrPartyByHealthWithPets(0);
        CHECK(targets.empty());

        // A dead caster is not picked: the engine's pick takes the living.
        caster.Die();
        targets.assign({ &other, &caster });
        atHimself.RaidOrPartyByHealthWithPets(5);
        CHECK(targets.empty());
        caster.Die(false);
    }
    ForgetCastRow();
    CHECK(sSpellStore.LookupEntry(CAST_ROW) == NULL);
}

TEST(PriestCircleOfHealing_TheRowPutsThePickInPlaceOfTheAreaAtTheAreaModeAlone)
{
    SpellHandlerRegistry registry;
    Priest::RegisterCircleOfHealing(registry);
    SpellEntry const* row = StoreCastRow();
    {
        BareCreature caster;
        caster.TakeGuid(HIGHGUID_UNIT, 27677, 1);
        caster.Stand(500, 1000);
        BareCreature other;
        other.TakeGuid(HIGHGUID_UNIT, 50249, 2);
        other.Stand(100, 1000);
        BareCreature third;
        third.TakeGuid(HIGHGUID_UNIT, 50249, 3);
        third.Stand(200, 1000);
        Spell cast(&caster, row, true);
        std::vector<Unit*> const alone{ &caster };

        // For both ranks: the destination's mode leaves the list as it is; the area's puts the engine's pick in place
        // of the area's units, its caster alone; the target limit is not touched.
        for (uint32 spellId : { CIRCLE_OF_HEALING, CIRCLE_OF_HEALING_OLD_RANK })
        {
            SpellRowStorage storage;
            std::list<Unit*> targets{ &other, &caster, &third };
            uint32 maxTargets = 0;
            SelectTargetsContext destination(caster, SpellRow(storage, spellId), EFFECT_INDEX_0,
                                             TARGET_DUELVSPLAYER_COORDINATES, targets, maxTargets, &caster, &cast,
                                             CIRCLE_RADIUS);
            CHECK(registry.Dispatch<SelectTargetsHook>(spellId, destination).IsContinue());
            CHECK_EQ(targets.size(), std::size_t(3));
            SelectTargetsContext area(caster, SpellRow(storage, spellId), EFFECT_INDEX_0,
                                      TARGET_ALL_FRIENDLY_UNITS_IN_AREA, targets, maxTargets, &caster, &cast,
                                      CIRCLE_RADIUS);
            CHECK(registry.Dispatch<SelectTargetsHook>(spellId, area).IsContinue());
            CHECK(UnitsOf(targets) == alone);
            CHECK_EQ(maxTargets, uint32(0));
        }

        // The row asks for 5 units, and 6 with the glyph's 1 on the caster: with the one unit a creature finds, both
        // pick it; a dead caster leaves none.
        StoreGlyph();
        SpellAuraHolder* glyph = GlyphHolder(caster, 1);
        caster.HoldAura(GLYPH_OF_CIRCLE_OF_HEALING, *glyph);
        SpellRowStorage storage;
        std::list<Unit*> targets{ &other, &caster };
        uint32 maxTargets = 0;
        SelectTargetsContext area(caster, SpellRow(storage, CIRCLE_OF_HEALING), EFFECT_INDEX_0,
                                  TARGET_ALL_FRIENDLY_UNITS_IN_AREA, targets, maxTargets, &caster, &cast,
                                  CIRCLE_RADIUS);
        CHECK(registry.Dispatch<SelectTargetsHook>(CIRCLE_OF_HEALING, area).IsContinue());
        CHECK(UnitsOf(targets) == alone);
        caster.Die();
        targets.assign({ &other, &caster });
        CHECK(registry.Dispatch<SelectTargetsHook>(CIRCLE_OF_HEALING, area).IsContinue());
        CHECK(targets.empty());
        caster.Die(false);
        delete glyph;
        ForgetGlyph();
    }
    ForgetCastRow();
}
