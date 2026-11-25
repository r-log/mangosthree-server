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

/// Tests of spells/classes/priest/DispelMagic.cpp and of the dispel signal its dummy row reads (DispelWatch,
/// SpellScriptHooks.cpp): the rows the file registers, in the game's table too, with Glyph of Dispel Magic's proc row
/// gone from the priest's legacy proc site; the units Dispel Magic is refused at; the dispel it casts at a unit; when
/// the glyph heals and how much; the check row, through the game's table, at the priest himself and with no target;
/// the dummy row at the effects and targets it leaves alone; and the stacks a watch counts as taken, the engine's own
/// dispel removal among them. The scripts and the watch run on bare Creatures (update fields allocated; no world, map,
/// template or AI). The watched spells are put into the spell store with only their ids and their dispel types, and
/// are taken out of it again; their holders are made through their constructor with no aura, put in a bare
/// creature's holder map, and leave it through the engine's removals. Scenario 1210 records the dispels, the refusal
/// and the glyph's heal in play.

#include "TestHarness.h"
#include "spells/classes/priest/DispelMagic.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "ObjectGuid.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const DISPEL_MAGIC_SPELL = 527;
    uint32 const DISPEL_MAGIC_FRIENDLY = 97690;
    uint32 const DISPEL_MAGIC_HOSTILE = 97691;
    uint32 const ABSOLUTION = 33167;
    uint32 const GLYPH_OF_DISPEL_MAGIC = 55677;
    uint32 const GLYPH_OF_DISPEL_MAGIC_HEAL = 56131;
    uint32 const MASS_DISPEL = 32375;

    // The watched spells and their SpellCategories rows; no client data holds these ids.
    uint32 const kMagicDebuff = 94951;
    uint32 const kMagicBuff = 94952;
    uint32 const kPoison = 94953;
    uint32 const kUndispellable = 94954;
    uint32 const kMagicCategories = 94955;
    uint32 const kPoisonCategories = 94956;

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            // The holders HoldAura put in the map and the engine did not remove are the test's own, and leave the
            // map first.
            ~BareCreature()
            {
                m_auras.Holders().clear();
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            // The guid of `counter`.
            void Stand(uint32 counter)
            {
                SetGuidValue(OBJECT_FIELD_GUID, ObjectGuid(HIGHGUID_UNIT, 50249, counter));
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

    // The watched spells: two magic ones, a poison and one of no dispel type; and Absolution, with only its id.
    ServerSpell s_magicDebuff;
    ServerSpell s_magicBuff;
    ServerSpell s_poison;
    ServerSpell s_undispellable;
    ServerSpell s_absolution;
    SpellCategoriesEntry s_magicCategories;
    SpellCategoriesEntry s_poisonCategories;

    // Puts the watched spells, with their categories, and Absolution into the spell store.
    void StoreRows()
    {
        s_magicCategories = SpellCategoriesEntry();
        s_magicCategories.DispelType = DISPEL_MAGIC;
        sSpellCategoriesStore.SetEntry(kMagicCategories, &s_magicCategories);
        s_poisonCategories = SpellCategoriesEntry();
        s_poisonCategories.DispelType = DISPEL_POISON;
        sSpellCategoriesStore.SetEntry(kPoisonCategories, &s_poisonCategories);

        s_magicDebuff.Entry().ID = kMagicDebuff;
        s_magicDebuff.Entry().CategoriesID = kMagicCategories;
        sSpellStore.SetEntry(kMagicDebuff, &s_magicDebuff.Entry());
        s_magicBuff.Entry().ID = kMagicBuff;
        s_magicBuff.Entry().CategoriesID = kMagicCategories;
        sSpellStore.SetEntry(kMagicBuff, &s_magicBuff.Entry());
        s_poison.Entry().ID = kPoison;
        s_poison.Entry().CategoriesID = kPoisonCategories;
        sSpellStore.SetEntry(kPoison, &s_poison.Entry());
        s_undispellable.Entry().ID = kUndispellable;
        sSpellStore.SetEntry(kUndispellable, &s_undispellable.Entry());
        s_absolution.Entry().ID = ABSOLUTION;
        sSpellStore.SetEntry(ABSOLUTION, &s_absolution.Entry());
    }

    // Takes them out of the stores again.
    void ForgetRows()
    {
        for (uint32 spellId : { kMagicDebuff, kMagicBuff, kPoison, kUndispellable, ABSOLUTION })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
        sSpellCategoriesStore.SetEntry(kMagicCategories, NULL);
        sSpellCategoriesStore.SetEntry(kPoisonCategories, NULL);
    }

    // A holder of `spellId`, from the store, cast by `unit` on itself, put in its holder map.
    void Hold(uint32 spellId, BareCreature& unit)
    {
        unit.HoldAura(spellId, *new SpellAuraHolder(sSpellStore.LookupEntry(spellId), &unit, NULL, NULL, NULL));
    }
}

TEST(PriestDispelMagic_TheFileHoldsItsTwoRowsInTheGameAndTheGlyphsProcRowIsGone)
{
    // Two rows on Dispel Magic: its check-cast row and its dummy-effect row; no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterDispelMagic(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(CheckCastHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(DummyEffectHook::Key), std::size_t(1));
    CHECK(registry.Find<CheckCastHook>(DISPEL_MAGIC_SPELL) != NULL);
    CHECK(registry.Find<DummyEffectHook>(DISPEL_MAGIC_SPELL) != NULL);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<CheckCastHook>(DISPEL_MAGIC_SPELL) == registry.Find<CheckCastHook>(DISPEL_MAGIC_SPELL));
    CHECK(game.Find<DummyEffectHook>(DISPEL_MAGIC_SPELL) == registry.Find<DummyEffectHook>(DISPEL_MAGIC_SPELL));

    // The two dispels, Absolution, the glyph, its heal and Mass Dispel hold no row of the file.
    for (uint32 spellId : { DISPEL_MAGIC_FRIENDLY, DISPEL_MAGIC_HOSTILE, ABSOLUTION, GLYPH_OF_DISPEL_MAGIC,
                            GLYPH_OF_DISPEL_MAGIC_HEAL, MASS_DISPEL })
    {
        CHECK(registry.Find<CheckCastHook>(spellId) == NULL);
        CHECK(registry.Find<DummyEffectHook>(spellId) == NULL);
    }

    // The glyph's legacy proc row is gone from the priest's dummy proc site, in the game's table too.
    CHECK(game.Find<DummyAuraProcPriestSite>(GLYPH_OF_DISPEL_MAGIC) == NULL);
}

TEST(PriestDispelMagic_ItIsRefusedAtAnotherFriendWithoutAbsolution)
{
    struct Vector
    {
        bool self;
        bool friendly;
        bool absolution;
        bool refused;
    };
    Vector const vectors[] =
    {
        // Another friendly unit: only with Absolution.
        { false, true, false, true }, { false, true, true, false },
        // A unit not friendly to him, with Absolution or without.
        { false, false, false, false }, { false, false, true, false },
        // Himself, with Absolution or without.
        { true, true, false, false }, { true, true, true, false },
    };
    for (Vector const& v : vectors)
    {
        CHECK_EQ(Priest::DispelMagicRefused(v.self, v.friendly, v.absolution), v.refused);
    }
}

TEST(PriestDispelMagic_ItCastsTheFriendlyDispelAtAFriendAndTheHostileOneAtAnyOther)
{
    CHECK_EQ(Priest::DispelMagicSpell(true), DISPEL_MAGIC_FRIENDLY);
    CHECK_EQ(Priest::DispelMagicSpell(false), DISPEL_MAGIC_HOSTILE);
}

TEST(PriestDispelMagic_TheGlyphHealsAfterAFriendlyDispelThatTookSomething)
{
    // Only the friendly dispel, only when it took at least one stack, only with the glyph.
    CHECK(Priest::GlyphOfDispelMagicHeals(DISPEL_MAGIC_FRIENDLY, 1, true));
    CHECK(Priest::GlyphOfDispelMagicHeals(DISPEL_MAGIC_FRIENDLY, 2, true));
    CHECK(!Priest::GlyphOfDispelMagicHeals(DISPEL_MAGIC_FRIENDLY, 0, true));
    CHECK(!Priest::GlyphOfDispelMagicHeals(DISPEL_MAGIC_FRIENDLY, 1, false));
    CHECK(!Priest::GlyphOfDispelMagicHeals(DISPEL_MAGIC_HOSTILE, 1, true));
    CHECK(!Priest::GlyphOfDispelMagicHeals(DISPEL_MAGIC_HOSTILE, 0, true));
    CHECK(!Priest::GlyphOfDispelMagicHeals(DISPEL_MAGIC_SPELL, 1, true));

    struct Vector
    {
        uint32 maxHealth;
        int32 percent;
        int32 heal;
    };
    Vector const vectors[] =
    {
        { 100, 3, 3 }, { 99, 3, 2 }, { 0, 3, 0 }, { 43285, 3, 1298 },
        // No overflow at the largest health, and the heal held to INT32_MAX.
        { UINT32_MAX, 3, 128849018 }, { UINT32_MAX, 100, INT32_MAX },
        // No percent.
        { 100, 0, 0 }, { 100, -3, 0 },
    };
    for (Vector const& v : vectors)
    {
        CHECK_EQ(Priest::GlyphOfDispelMagicHeal(v.maxHealth, v.percent), v.heal);
    }
}

TEST(PriestDispelMagic_TheRowsLeaveTheCastAtHimselfAndTheEffectsTheyDoNotOwn)
{
    SpellRowStorage storage;
    SpellEntry const& dispelMagic = SpellRow(storage, DISPEL_MAGIC_SPELL);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    StoreRows();
    {
        BareCreature priest;
        priest.Stand(1);

        // At himself, without Absolution and with it, and with no unit target: never refused.
        CheckCastContext self(priest, &priest, dispelMagic, true);
        CHECK(game.Dispatch<CheckCastHook>(DISPEL_MAGIC_SPELL, self).IsContinue());
        CheckCastContext none(priest, NULL, dispelMagic, true);
        CHECK(game.Dispatch<CheckCastHook>(DISPEL_MAGIC_SPELL, none).IsContinue());
        Hold(ABSOLUTION, priest);
        CheckCastContext absolved(priest, &priest, dispelMagic, false);
        CHECK(game.Dispatch<CheckCastHook>(DISPEL_MAGIC_SPELL, absolved).IsContinue());

        // The dummy row leaves an effect other than effect 0, and an effect with no unit target: nothing is cast and
        // nothing removed.
        Hold(kMagicBuff, priest);
        int32 value = 0;
        int32 word = 0;
        DummyEffectContext other(priest, &priest, NULL, NULL, dispelMagic, EFFECT_INDEX_1, value, ObjectGuid(), &priest,
                                 word);
        CHECK(game.Dispatch<DummyEffectHook>(DISPEL_MAGIC_SPELL, other).IsContinue());
        DummyEffectContext untargeted(priest, NULL, NULL, NULL, dispelMagic, EFFECT_INDEX_0, value, ObjectGuid(), NULL,
                                      word);
        CHECK(game.Dispatch<DummyEffectHook>(DISPEL_MAGIC_SPELL, untargeted).IsContinue());
        CHECK(priest.HasAura(kMagicBuff));
        CHECK_EQ(word, int32(0));
        priest.RemoveAurasDueToSpell(kMagicBuff);
        priest.RemoveAurasDueToSpell(ABSOLUTION);
    }
    ForgetRows();
}

TEST(PriestDispelMagic_TheWatchCountsTheStacksTheUnitLostOfItsDispelTypes)
{
    uint32 const magic = GetDispellMask(DISPEL_MAGIC);
    StoreRows();
    {
        BareCreature unit;
        unit.Stand(2);
        Hold(kMagicDebuff, unit);
        Hold(kMagicBuff, unit);
        Hold(kPoison, unit);
        Hold(kUndispellable, unit);

        // Nothing lost yet.
        DispelWatch watch(unit, magic);
        CHECK_EQ(watch.Taken(), uint32(0));

        // The engine's dispel removal takes the magic debuff's stack: one.
        unit.RemoveAuraHolderDueToSpellByDispel(kMagicDebuff, 1, unit.GetObjectGuid(), &unit);
        CHECK(!unit.HasAura(kMagicDebuff));
        CHECK_EQ(watch.Taken(), uint32(1));

        // The poison and the holder of no dispel type leaving count nothing for a watch of magic.
        unit.RemoveAurasDueToSpell(kPoison);
        unit.RemoveAurasDueToSpell(kUndispellable);
        CHECK_EQ(watch.Taken(), uint32(1));

        // A holder is known by its spell and its caster, as a dispel names it: the debuff held again by the same
        // caster counts as kept, and the magic buff leaving counts its stack.
        Hold(kMagicDebuff, unit);
        CHECK_EQ(watch.Taken(), uint32(0));
        unit.RemoveAurasDueToSpell(kMagicBuff);
        CHECK_EQ(watch.Taken(), uint32(1));

        // A holder put on after the watch was made counts nothing when it leaves.
        DispelWatch later(unit, magic);
        Hold(kMagicBuff, unit);
        unit.RemoveAurasDueToSpell(kMagicBuff);
        CHECK_EQ(later.Taken(), uint32(0));

        // A watch of poison counts the poison; one of no type counts nothing; one of every type counts both.
        Hold(kPoison, unit);
        DispelWatch poison(unit, GetDispellMask(DISPEL_POISON));
        DispelWatch noType(unit, 0);
        DispelWatch everyType(unit, GetDispellMask(DISPEL_ALL));
        unit.RemoveAurasDueToSpell(kPoison);
        unit.RemoveAurasDueToSpell(kMagicDebuff);
        CHECK_EQ(poison.Taken(), uint32(1));
        CHECK_EQ(noType.Taken(), uint32(0));
        CHECK_EQ(everyType.Taken(), uint32(2));
    }
    ForgetRows();
}
