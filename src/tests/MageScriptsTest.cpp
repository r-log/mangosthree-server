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

/// Tests of the mage's spell scripts: the rows RegisterMageScripts registers, each file's share, the bodies that labels
/// share, and the dummy-effect labels that end the effect with no unit target. A body that casts, reads a level or
/// creates an item needs a live unit and a spell.

#include "TestHarness.h"
#include "spells/classes/mage/Mage.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/AurasHandlers.h"
#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "DBCStructure.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

namespace
{
    // The labels the class files register at each mage site, in their tables' order.
    uint32 const TRIGGER_SPELL_IDS[] = { 66 };
    uint32 const BOOST_IDS[] = { 11129, 28682, 74396 };
    uint32 const EFFECT_DUMMY_IDS[] = { 32826, 38194, 42955, 1459, 61316, 31687 };
    uint32 const DUMMY_PROC_IDS[] = { 56372, 56375, 64411, 11119, 11120, 12846, 12847, 12848, 74396 };

    // Every id of `ids` is a key of `Site` on `registry`, and the game's table holds the same body for it.
    template <class Site, std::size_t N>
    void CheckRowsInTheGame(SpellHandlerRegistry const& registry, uint32 const (&ids)[N])
    {
        SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
        for (uint32 spellId : ids)
        {
            CHECK(registry.Find<Site>(spellId) != NULL);
            CHECK(game.Find<Site>(spellId) == registry.Find<Site>(spellId));
        }
    }

    // A SpellEffect.dbc row in raw storage, as the loader makes it; zeroed, with the effect index set.
    struct EffectRow
    {
        alignas(SpellEffectEntry) unsigned char bytes[sizeof(SpellEffectEntry)];
    };

    EffectRow s_effectRow = {};

    SpellEffectEntry const* DummyEffect(uint32 effectIndex)
    {
        SpellEffectEntry* row = reinterpret_cast<SpellEffectEntry*>(s_effectRow.bytes);
        row->EffectIndex = effectIndex;
        return row;
    }
}

TEST(MageScripts_TheTableRegistersEveryRowOnce)
{
    // The class-wide file: Invisibility at the trigger-spell site; Polymorph Cast Visual, Blink, Conjure Refreshment
    // and the two raid buffs at the dummy-effect site; the Glyphs of Ice Block and Polymorph and Blessing of Ancient
    // Kings at the dummy proc site; the defaults of the trigger-spell, periodic dummy tick and boost sites. Fire:
    // Combustion's two boost labels and the five Ignite ranks. Frost: the 74396 boost and proc labels and Summon Water
    // Elemental. Arcane: none. 19 rows, 19 keys and 3 defaults, none twice.
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterMageScripts(registry), uint32(22));
    CHECK_EQ(registry.Count(), std::size_t(19));
    CHECK_EQ(registry.CountDefaults(), std::size_t(3));
    CHECK_EQ(registry.CountAt(AuraTriggerSpellMageSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraPeriodicDummyTickMageSite::Key), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraBoostMageSite::Key), std::size_t(3));
    CHECK_EQ(registry.CountAt(SpellEffectDummyMageSite::Key), std::size_t(6));
    CHECK_EQ(registry.CountAt(DummyAuraProcMageSite::Key), std::size_t(9));
    CHECK_EQ(registry.CountAt(SpellCritChanceAuraProcMageSite::Key), std::size_t(0));
    CHECK(registry.FindDefault<AuraTriggerSpellMageSite>() != NULL);
    CHECK(registry.FindDefault<AuraPeriodicDummyTickMageSite>() != NULL);
    CHECK(registry.FindDefault<AuraBoostMageSite>() != NULL);
    CHECK(registry.FindDefault<SpellEffectDummyMageSite>() == NULL);
    CHECK(registry.FindDefault<DummyAuraProcMageSite>() == NULL);

    // Each file on a table of its own: what it answers, its keys and its defaults.
    struct PerFile
    {
        uint32 (*registerRows)(SpellHandlerRegistry&);
        uint32 answered;
        std::size_t keys;
        std::size_t defaults;
    };
    PerFile const files[] =
    {
        { &Mage::RegisterClassWide, 12, 9, 3 },
        { &Mage::RegisterFire, 7, 7, 0 },
        { &Mage::RegisterFrost, 3, 3, 0 },
        { &Mage::RegisterArcane, 0, 0, 0 },
    };
    for (PerFile const& file : files)
    {
        SpellHandlerRegistry own;
        CHECK_EQ(file.registerRows(own), file.answered);
        CHECK_EQ(own.Count(), file.keys);
        CHECK_EQ(own.CountDefaults(), file.defaults);
    }

    // Registering again on the same table changes nothing: every key and default is taken.
    CHECK_EQ(RegisterMageScripts(registry), uint32(22));
    CHECK_EQ(registry.Count(), std::size_t(19));
    CHECK_EQ(registry.CountDefaults(), std::size_t(3));

    // The game's table holds every one of these keys with the same body, and the same three defaults. The handler
    // files hold the mage sites' other rows: Mirror Image at the periodic dummy tick site, the three Pushing the Limit
    // labels at the boost site, Cold Snap at the dummy-effect site and Focus Magic at the crit-chance proc site.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    std::vector<std::pair<uint32, uint32> > const mine = registry.Keys();
    std::vector<std::pair<uint32, uint32> > const all = game.Keys();
    CHECK_EQ(mine.size(), std::size_t(19));
    CHECK(std::includes(all.begin(), all.end(), mine.begin(), mine.end()));
    CheckRowsInTheGame<AuraTriggerSpellMageSite>(registry, TRIGGER_SPELL_IDS);
    CheckRowsInTheGame<AuraBoostMageSite>(registry, BOOST_IDS);
    CheckRowsInTheGame<SpellEffectDummyMageSite>(registry, EFFECT_DUMMY_IDS);
    CheckRowsInTheGame<DummyAuraProcMageSite>(registry, DUMMY_PROC_IDS);
    CHECK(game.FindDefault<AuraTriggerSpellMageSite>() == registry.FindDefault<AuraTriggerSpellMageSite>());
    CHECK(game.FindDefault<AuraPeriodicDummyTickMageSite>() == registry.FindDefault<AuraPeriodicDummyTickMageSite>());
    CHECK(game.FindDefault<AuraBoostMageSite>() == registry.FindDefault<AuraBoostMageSite>());
    CHECK_EQ(game.CountAt(AuraTriggerSpellMageSite::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(AuraPeriodicDummyTickMageSite::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(AuraBoostMageSite::Key), std::size_t(6));
    CHECK_EQ(game.CountAt(SpellEffectDummyMageSite::Key), std::size_t(7));
    CHECK_EQ(game.CountAt(DummyAuraProcMageSite::Key), std::size_t(9));
    CHECK_EQ(game.CountAt(SpellCritChanceAuraProcMageSite::Key), std::size_t(1));
    CHECK(game.Find<AuraPeriodicDummyTickMageSite>(55342) != NULL);
    CHECK(registry.Find<AuraPeriodicDummyTickMageSite>(55342) == NULL);
    for (uint32 spellId : { 44401u, 48108u, 57761u })
    {
        CHECK(game.Find<AuraBoostMageSite>(spellId) != NULL);
        CHECK(registry.Find<AuraBoostMageSite>(spellId) == NULL);
    }
    CHECK(game.Find<SpellEffectDummyMageSite>(11958) != NULL);
    CHECK(registry.Find<SpellEffectDummyMageSite>(11958) == NULL);
    CHECK(game.Find<SpellCritChanceAuraProcMageSite>(54646) != NULL);
    CHECK(registry.Find<SpellCritChanceAuraProcMageSite>(54646) == NULL);
}

TEST(MageScripts_TheIgniteRanksShareOneBody)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);

    // The five Ignite ranks share one body; the Glyphs of Ice Block and Polymorph, Blessing of Ancient Kings and 74396
    // each have their own: 9 labels in 5 bodies.
    typedef SpellHandler<DummyAuraProcMageSite>::Function ProcBody;
    ProcBody ignite = registry.Find<DummyAuraProcMageSite>(11119);
    CHECK(ignite != NULL);
    for (uint32 spellId : { 11120u, 12846u, 12847u, 12848u })
    {
        CHECK(registry.Find<DummyAuraProcMageSite>(spellId) == ignite);
    }
    std::set<ProcBody> procBodies;
    for (uint32 spellId : DUMMY_PROC_IDS)
    {
        procBodies.insert(registry.Find<DummyAuraProcMageSite>(spellId));
    }
    CHECK(procBodies.count(NULL) == 0);
    CHECK_EQ(procBodies.size(), std::size_t(5));

    // Combustion's two labels and the 74396 boost each have their own body.
    std::set<SpellHandler<AuraBoostMageSite>::Function> boostBodies;
    for (uint32 spellId : BOOST_IDS)
    {
        boostBodies.insert(registry.Find<AuraBoostMageSite>(spellId));
    }
    CHECK(boostBodies.count(NULL) == 0);
    CHECK_EQ(boostBodies.size(), std::size_t(3));
}

TEST(MageScripts_TheEffectDummySiteHoldsItsSixLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    CHECK_EQ(registry.CountAt(SpellEffectDummyMageSite::Key), std::size_t(6));

    // Six labels, five bodies: the two raid buffs share one; every other label has its own.
    typedef SpellHandler<SpellEffectDummyMageSite>::Function MageBody;
    MageBody raidBuff = registry.Find<SpellEffectDummyMageSite>(1459);
    CHECK(raidBuff != NULL);
    CHECK(registry.Find<SpellEffectDummyMageSite>(61316) == raidBuff);
    std::set<MageBody> bodies;
    for (uint32 spellId : { 31687u, 32826u, 38194u, 42955u })
    {
        bodies.insert(registry.Find<SpellEffectDummyMageSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK(bodies.count(raidBuff) == 0);
    CHECK_EQ(bodies.size(), std::size_t(4));

    // No default: an id with no row (the warrior's Last Stand) finds nothing, and its dispatch is a miss, so the
    // family's case goes on to the Conjure Mana Gem check with nothing changed.
    alignas(16) static unsigned char objects[1][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = NULL;
    Unit* target = NULL;
    int32 damage = 90;
    SpellEffectEntry const* effect = DummyEffect(1);
    SpellEffectDummyMageContext ctx(spell, caster, target, damage, effect);
    CHECK(registry.FindDefault<SpellEffectDummyMageSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyMageSite>(12975) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyMageSite>(12975, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(ctx.spell == spell);
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));
    CHECK_EQ(effect->EffectIndex, uint32(1));

    // The bodies that need no live unit end the effect with no unit target: Polymorph Cast Visual, Blink and the raid
    // buffs. Summon Water Elemental casts, and Conjure Refreshment reads the target's level and creates an item through
    // the spell; both need a live unit and a spell.
    for (uint32 spellId : { 32826u, 38194u, 1459u, 61316u })
    {
        SpellHandlerOutcome<void> noTarget = registry.Dispatch<SpellEffectDummyMageSite>(spellId, ctx);
        CHECK(noTarget.IsReturn());
        CHECK(!noTarget.IsContinue());
    }
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));

    // The game's table holds the same bodies, with Cold Snap's row from the handler file, and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CheckRowsInTheGame<SpellEffectDummyMageSite>(registry, EFFECT_DUMMY_IDS);
    CHECK(game.FindDefault<SpellEffectDummyMageSite>() == NULL);
}
