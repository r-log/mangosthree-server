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

/// Tests of priest/holy/CircleOfHealing.cpp: the rows the file registers, in the game's table too; the units a
/// heal picks, 5 and the glyph's amount, read from the holder. The units are bare creatures (update fields
/// allocated; no world, map, template or AI). The glyph (55675) is put into the spell store with its effect 0, a
/// dummy aura, and taken out again. Scenario 1227 records the heal's units in play, a group's members and a
/// member's pet among them.

#include "TestHarness.h"
#include "spells/classes/priest/holy/CircleOfHealing.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/auras/SpellAuras.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const CIRCLE_OF_HEALING = 34861;
    uint32 const CIRCLE_OF_HEALING_OLD_RANK = 49306;
    uint32 const GLYPH_OF_CIRCLE_OF_HEALING = 55675;
    uint32 const WILD_GROWTH = 48438;
    uint32 const ECHO_OF_LIGHT_HEAL = 77489;
    uint32 const DIVINE_HYMN_HEAL = 64844;

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
