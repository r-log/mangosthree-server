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
/// share, the dummy-effect labels that end the effect with no unit target, the proc hook's ids per proc script, the
/// refusals of the proc scripts, the count of crits in a row, Ice Lance's damage-done multiplier and Blink's target
/// count. A body that casts, reads a level or creates an item
/// needs a live unit and a spell; the proc scripts' refusals run on bare Creatures (update fields allocated; no map,
/// template or AI), with the effect rows the scripts read added to the effect store as the client's data holds them.

#include "TestHarness.h"
#include "spells/classes/mage/Mage.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/AurasHandlers.h"
#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <algorithm>
#include <list>
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

    // The ids each proc script holds at the proc hook: every mage spell with the talent's icon (or class mask) and an
    // aura of the type the script stands for, in the client's data.
    uint32 const MAGIC_ABSORPTION_IDS[] = { 29441, 29444 };
    uint32 const MASTER_OF_ELEMENTS_IDS[] = { 29074, 29075, 29076 };
    uint32 const ARCANE_POTENCY_IDS[] = { 31571, 31572 };
    uint32 const HOT_STREAK_IDS[] = { 44445, 44446, 44448 };
    uint32 const BLAZING_SPEED_IDS[] = { 31641, 31642 };
    uint32 const MANA_SHIELD_IDS[] = { 1463 };
    uint32 const PERSISTENT_SHIELD_IDS[] = { 26467 };
    uint32 const DEEP_FREEZE_IMMUNITY_IDS[] = { 71761 };
    uint32 const FINGERS_OF_FROST_IDS[] = { 44543, 44545, 83074 };

    // The ids the damage-done and target scripts hold: every mage spell with the arm's icon and a damage effect (Ice
    // Lance), and the one id the arm named (Blink).
    uint32 const ICE_LANCE_IDS[] = { 30455 };
    uint32 const BLINK_IDS[] = { 38194 };

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

    // Every id of `ids` holds one body at the proc hook, on `registry` and in the game's table alike; answers it.
    template <std::size_t N>
    SpellHandler<OnProcHook>::Function OneProcBody(SpellHandlerRegistry const& registry, uint32 const (&ids)[N])
    {
        SpellHandler<OnProcHook>::Function body = registry.Find<OnProcHook>(ids[0]);
        CHECK(body != NULL);
        for (uint32 spellId : ids)
        {
            CHECK(registry.Find<OnProcHook>(spellId) == body);
            CHECK(SpellHandlerRegistry::Game().Find<OnProcHook>(spellId) == body);
        }
        return body;
    }

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            // Unit::CleanupsBeforeDelete tears down an in-world unit only while the update fields exist.
            ~BareCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    struct HolderStorage
    {
        alignas(SpellAuraHolder) unsigned char bytes[sizeof(SpellAuraHolder)];
    };

    // A Spell.dbc row with only its id, which the proc context names; it is not in the spell store.
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

    // A holder's storage without its constructor, every byte 0: the aura handle reads its script words there.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // The effect rows of the client's data the proc scripts read, (spell, index, aura type) each, put into the effect
    // store once for the rest of the run; no other test uses these ids.
    void AddProcEffectRows()
    {
        struct Row
        {
            uint32 spellId;
            uint32 effectIndex;
            uint32 aura;
        };
        static Row const rows[] =
        {
            { 29441, 0, SPELL_AURA_DUMMY }, { 29441, 1, SPELL_AURA_MOD_RESISTANCE },
            { 29074, 0, SPELL_AURA_DUMMY },
            { 31571, 0, SPELL_AURA_DUMMY },
            { 44445, 0, SPELL_AURA_DUMMY }, { 44445, 1, SPELL_AURA_DUMMY },
            { 31641, 0, SPELL_AURA_PROC_TRIGGER_SPELL },
            { 1463, 0, SPELL_AURA_MANA_SHIELD },
            { 26467, 0, SPELL_AURA_PROC_TRIGGER_SPELL },
            { 71761, 0, SPELL_AURA_PROC_TRIGGER_SPELL },
            { 83074, 0, SPELL_AURA_PROC_TRIGGER_SPELL },
        };
        static SpellEffectEntry effects[sizeof(rows) / sizeof(rows[0])];
        static bool added = false;
        if (added)
        {
            return;
        }
        added = true;
        for (std::size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
        {
            effects[i] = SpellEffectEntry();
            effects[i].SpellID = rows[i].spellId;
            effects[i].EffectIndex = rows[i].effectIndex;
            effects[i].Effect = SPELL_EFFECT_APPLY_AURA;
            effects[i].EffectAura = rows[i].aura;
            AddSpellEffectEntry(&effects[i]);
        }
    }

    // A proc of `spellId`'s effect `effIndex` on `owner`, with the event's spell, flags and the aura's amount given.
    SpellHandlerOutcome<SpellAuraProcResult> Proc(SpellHandlerRegistry const& registry, Unit& owner, uint32 spellId,
                                                  SpellEffectIndex effIndex, SpellAuraHolder& holder, SpellEntry const* procSpell,
                                                  uint32 procEx, int32 amount)
    {
        SpellRowStorage row;
        SpellCooldownMgr cooldowns;
        OnProcContext ctx(owner, NULL, SpellRow(row, spellId), effIndex, AuraHandle(holder), procSpell, 0x10000, procEx, 900,
                          amount, ObjectGuid(), 0, NULL, cooldowns);
        return registry.Dispatch<OnProcHook>(spellId, ctx);
    }
}

TEST(MageScripts_TheTableRegistersEveryRowOnce)
{
    // The class-wide file: Invisibility at the trigger-spell site; Polymorph Cast Visual, Blink, Conjure Refreshment
    // and the two raid buffs at the dummy-effect site; the Glyphs of Ice Block and Polymorph and Blessing of Ancient
    // Kings at the dummy proc site; the defaults of the trigger-spell, periodic dummy tick and boost sites; Blink at
    // the target hook and Persistent Shield at the proc hook. Fire: Combustion's two boost labels and the five Ignite
    // ranks. Frost: the 74396 boost and proc labels and Summon Water Elemental; Ice Lance at the damage-done hook. At
    // the proc hook, Fire: Master of Elements, Hot Streak and Blazing Speed; Frost: Deep Freeze Immunity State and
    // Fingers of Frost; Arcane: Magic Absorption, Arcane Potency and Mana Shield. 39 rows, 39 keys and 3 defaults,
    // none twice.
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterMageScripts(registry), uint32(42));
    CHECK_EQ(registry.Count(), std::size_t(39));
    CHECK_EQ(registry.CountDefaults(), std::size_t(3));
    CHECK_EQ(registry.CountAt(AuraTriggerSpellMageSite::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraPeriodicDummyTickMageSite::Key), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraBoostMageSite::Key), std::size_t(3));
    CHECK_EQ(registry.CountAt(SpellEffectDummyMageSite::Key), std::size_t(6));
    CHECK_EQ(registry.CountAt(DummyAuraProcMageSite::Key), std::size_t(9));
    CHECK_EQ(registry.CountAt(SpellCritChanceAuraProcMageSite::Key), std::size_t(0));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(18));
    CHECK_EQ(registry.CountAt(SelectTargetsHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(DamageDoneHook::Key), std::size_t(1));
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
        { &Mage::RegisterClassWide, 14, 11, 3 },
        { &Mage::RegisterFire, 15, 15, 0 },
        { &Mage::RegisterFrost, 8, 8, 0 },
        { &Mage::RegisterArcane, 5, 5, 0 },
    };
    for (PerFile const& file : files)
    {
        SpellHandlerRegistry own;
        CHECK_EQ(file.registerRows(own), file.answered);
        CHECK_EQ(own.Count(), file.keys);
        CHECK_EQ(own.CountDefaults(), file.defaults);
    }

    // Registering again on the same table changes nothing: every key and default is taken.
    CHECK_EQ(RegisterMageScripts(registry), uint32(42));
    CHECK_EQ(registry.Count(), std::size_t(39));
    CHECK_EQ(registry.CountDefaults(), std::size_t(3));

    // The game's table holds every one of these keys with the same body, and the same three defaults. The handler
    // files hold the mage sites' other rows: Mirror Image at the periodic dummy tick site, the three Pushing the Limit
    // labels at the boost site, Cold Snap at the dummy-effect site and Focus Magic at the crit-chance proc site.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    std::vector<std::pair<uint32, uint32> > const mine = registry.Keys();
    std::vector<std::pair<uint32, uint32> > const all = game.Keys();
    CHECK_EQ(mine.size(), std::size_t(39));
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

TEST(MageScripts_TheProcHookHoldsTheIdsOfEachProcTalentOnce)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();

    // Each talent's ids share one body, and each talent has its own: 18 ids in 9 bodies, the same in the game's table.
    std::set<SpellHandler<OnProcHook>::Function> bodies;
    bodies.insert(OneProcBody(registry, MAGIC_ABSORPTION_IDS));
    bodies.insert(OneProcBody(registry, MASTER_OF_ELEMENTS_IDS));
    bodies.insert(OneProcBody(registry, ARCANE_POTENCY_IDS));
    bodies.insert(OneProcBody(registry, HOT_STREAK_IDS));
    bodies.insert(OneProcBody(registry, BLAZING_SPEED_IDS));
    bodies.insert(OneProcBody(registry, MANA_SHIELD_IDS));
    bodies.insert(OneProcBody(registry, PERSISTENT_SHIELD_IDS));
    bodies.insert(OneProcBody(registry, DEEP_FREEZE_IMMUNITY_IDS));
    bodies.insert(OneProcBody(registry, FINGERS_OF_FROST_IDS));
    CHECK_EQ(bodies.size(), std::size_t(9));
    std::size_t const ids = sizeof(MAGIC_ABSORPTION_IDS) / sizeof(uint32) + sizeof(MASTER_OF_ELEMENTS_IDS) / sizeof(uint32) +
                            sizeof(ARCANE_POTENCY_IDS) / sizeof(uint32) + sizeof(HOT_STREAK_IDS) / sizeof(uint32) +
                            sizeof(BLAZING_SPEED_IDS) / sizeof(uint32) + sizeof(MANA_SHIELD_IDS) / sizeof(uint32) +
                            sizeof(PERSISTENT_SHIELD_IDS) / sizeof(uint32) + sizeof(DEEP_FREEZE_IMMUNITY_IDS) / sizeof(uint32) +
                            sizeof(FINGERS_OF_FROST_IDS) / sizeof(uint32);
    CHECK_EQ(ids, std::size_t(18));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), ids);
    CHECK_EQ(game.CountAt(OnProcHook::Key), ids);
    CHECK_EQ(game.CountAt(CheckProcHook::Key), std::size_t(0));

    // No script stands for spells no 4.3.4 proc reaches: Burnout's five ranks (no dummy aura), Combustion (11129 applies
    // no aura) and 28682 (absent from the client's data), and Arcane Blast's buff (36032 holds no damage-done aura).
    for (uint32 spellId : { 44449u, 44469u, 44470u, 44471u, 44472u, 11129u, 28682u, 36032u })
    {
        CHECK(game.Find<OnProcHook>(spellId) == NULL);
        CHECK(game.Find<CheckProcHook>(spellId) == NULL);
    }

    // The proc-trigger scripts end the proc themselves (Mage::CastProcTriggerSpell): no row and no default of the
    // legacy site of trigger spells, which the proc-trigger handler dispatches for the spell it casts, answers for the
    // spells they cast: Blazing Speed's 31643, Persistent Shield's 26470, Deep Freeze's damage 71757 and Fingers of
    // Frost's 44544.
    for (uint32 spellId : { 31643u, 26470u, 71757u, 44544u })
    {
        CHECK(game.Find<ProcTriggerSpellAuraProcTriggeredSite>(spellId) == NULL);
    }
    CHECK(game.FindDefault<ProcTriggerSpellAuraProcTriggeredSite>() == NULL);

    // No script stands for the holders of icon 32 (Ice Barrier 11426 and 98864, Glyph of Ice Barrier 63095): no row of
    // the mage boost site, the aura-apply or the aura-remove hook names them, and the site's default returns for them.
    for (uint32 spellId : { 11426u, 98864u, 63095u })
    {
        CHECK(game.Find<AuraBoostMageSite>(spellId) == NULL);
        CHECK(game.Find<AuraApplyHook>(spellId) == NULL);
        CHECK(game.Find<AuraRemoveHook>(spellId) == NULL);
    }
    CHECK(game.FindDefault<AuraBoostMageSite>() != NULL);
}

TEST(MageScripts_TheProcScriptsKeepTheirEarlyRefusals)
{
    AddProcEffectRows();
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    BareCreature owner;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);

    // Magic Absorption: an owner without mana is refused at the dummy effect; the resistance effect is its handler's.
    owner.SetByteValue(UNIT_FIELD_BYTES_0, 3, POWER_RAGE);
    SpellHandlerOutcome<SpellAuraProcResult> noMana = Proc(registry, owner, 29441, EFFECT_INDEX_0, holder, NULL, PROC_EX_RESIST, 2);
    CHECK(noMana.IsReturn());
    CHECK_EQ(int(noMana.GetValue()), int(SPELL_AURA_PROC_FAILED));
    CHECK(Proc(registry, owner, 29441, EFFECT_INDEX_1, holder, NULL, PROC_EX_RESIST, 0).IsContinue());

    // Master of Elements and Arcane Potency: an event with no spell is refused.
    SpellHandlerOutcome<SpellAuraProcResult> noSpell = Proc(registry, owner, 29074, EFFECT_INDEX_0, holder, NULL, PROC_EX_CRITICAL_HIT, 15);
    CHECK(noSpell.IsReturn());
    CHECK_EQ(int(noSpell.GetValue()), int(SPELL_AURA_PROC_FAILED));
    noSpell = Proc(registry, owner, 31571, EFFECT_INDEX_0, holder, NULL, PROC_EX_NONE, 7);
    CHECK(noSpell.IsReturn());
    CHECK_EQ(int(noSpell.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // Hot Streak: effect 1 does nothing, and effect 0 nothing while the owner holds no aura of the spell at effect 1;
    // neither touches the count.
    AuraHandle(holder).ScriptWord(EFFECT_INDEX_0) = 1;
    SpellHandlerOutcome<SpellAuraProcResult> streak = Proc(registry, owner, 44445, EFFECT_INDEX_1, holder, NULL, PROC_EX_CRITICAL_HIT, 25);
    CHECK(streak.IsReturn());
    CHECK_EQ(int(streak.GetValue()), int(SPELL_AURA_PROC_OK));
    streak = Proc(registry, owner, 44445, EFFECT_INDEX_0, holder, NULL, PROC_EX_CRITICAL_HIT, 33);
    CHECK(streak.IsReturn());
    CHECK_EQ(int(streak.GetValue()), int(SPELL_AURA_PROC_OK));
    CHECK_EQ(AuraHandle(holder).ScriptWord(EFFECT_INDEX_0), int32(1));

    // Blazing Speed: with no Blazing Speed buff (31643) in the spell store the proc is refused, as the proc-trigger
    // handler refuses a trigger spell the store lacks.
    SpellHandlerOutcome<SpellAuraProcResult> noBuff = Proc(registry, owner, 31641, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, 0);
    CHECK(noBuff.IsReturn());
    CHECK_EQ(int(noBuff.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // Mana Shield (Incanter's Regalia): an owner that is no player is refused.
    SpellHandlerOutcome<SpellAuraProcResult> notPlayer = Proc(registry, owner, 1463, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, 0);
    CHECK(notPlayer.IsReturn());
    CHECK_EQ(int(notPlayer.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // Persistent Shield: with no 26470 in the spell store the proc is refused, as the proc-trigger handler refuses.
    SpellHandlerOutcome<SpellAuraProcResult> noShield = Proc(registry, owner, 26467, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, 0);
    CHECK(noShield.IsReturn());
    CHECK_EQ(int(noShield.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // Deep Freeze Immunity State: an event with no creature victim is refused.
    SpellHandlerOutcome<SpellAuraProcResult> noCreature = Proc(registry, owner, 71761, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, 0);
    CHECK(noCreature.IsReturn());
    CHECK_EQ(int(noCreature.GetValue()), int(SPELL_AURA_PROC_FAILED));

    // Fingers of Frost: an amount of 0 is a chance of 0, so the roll refuses the proc.
    SpellHandlerOutcome<SpellAuraProcResult> noChance = Proc(registry, owner, 83074, EFFECT_INDEX_0, holder, NULL, PROC_EX_NORMAL_HIT, 0);
    CHECK(noChance.IsReturn());
    CHECK_EQ(int(noChance.GetValue()), int(SPELL_AURA_PROC_FAILED));
}

TEST(MageScripts_IceLanceTriplesTheDamageDoneAtAFrozenTarget)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : ICE_LANCE_IDS)
    {
        CHECK(registry.Find<DamageDoneHook>(spellId) != NULL);
        CHECK(game.Find<DamageDoneHook>(spellId) == registry.Find<DamageDoneHook>(spellId));
    }
    CHECK_EQ(game.CountAt(DamageDoneHook::Key), sizeof(ICE_LANCE_IDS) / sizeof(uint32));

    // A caster with no aura that lets him treat a target as frozen: an unfrozen target leaves the multiplier, a frozen
    // one triples it; both continue.
    BareCreature caster;
    BareCreature victim;
    SpellRowStorage row;
    float doneTotalMod = 1.5f;
    DamageDoneContext unfrozen(caster, victim, SpellRow(row, 30455), doneTotalMod);
    CHECK(registry.Dispatch<DamageDoneHook>(30455, unfrozen).IsContinue());
    CHECK_EQ(doneTotalMod, 1.5f);
    victim.SetFlag(UNIT_FIELD_AURASTATE, 1 << (AURA_STATE_FROZEN - 1));
    CHECK(victim.IsFrozen());
    DamageDoneContext frozen(caster, victim, SpellRow(row, 30455), doneTotalMod);
    CHECK(registry.Dispatch<DamageDoneHook>(30455, frozen).IsContinue());
    CHECK_EQ(doneTotalMod, 4.5f);
}

TEST(MageScripts_BlinkKeepsOneTarget)
{
    SpellHandlerRegistry registry;
    RegisterMageScripts(registry);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : BLINK_IDS)
    {
        CHECK(registry.Find<SelectTargetsHook>(spellId) != NULL);
        CHECK(game.Find<SelectTargetsHook>(spellId) == registry.Find<SelectTargetsHook>(spellId));
    }
    CHECK_EQ(game.CountAt(SelectTargetsHook::Key), sizeof(BLINK_IDS) / sizeof(uint32));

    // Blink sets the count the cut after the hook keeps to one and continues, so the cut runs; the list is untouched.
    BareCreature caster;
    BareCreature first;
    BareCreature second;
    SpellRowStorage row;
    std::list<Unit*> targets;
    targets.push_back(&first);
    targets.push_back(&second);
    uint32 maxTargets = 0;
    SelectTargetsContext ctx(caster, SpellRow(row, 38194), EFFECT_INDEX_0, TARGET_ALL_ENEMY_IN_AREA, targets, maxTargets);
    CHECK(registry.Dispatch<SelectTargetsHook>(38194, ctx).IsContinue());
    CHECK_EQ(maxTargets, uint32(1));
    CHECK_EQ(targets.size(), std::size_t(2));
}

TEST(MageScripts_HotStreakRollsOnTheSecondCritInARow)
{
    // crit, crit: the second rolls and the count starts again.
    int32 crits = 0;
    CHECK(!Mage::CountCritInARow(crits, true));
    CHECK_EQ(crits, int32(1));
    CHECK(Mage::CountCritInARow(crits, true));
    CHECK_EQ(crits, int32(0));

    // crit, hit, crit: the hit starts the count again, so the third does not roll.
    CHECK(!Mage::CountCritInARow(crits, true));
    CHECK(!Mage::CountCritInARow(crits, false));
    CHECK_EQ(crits, int32(0));
    CHECK(!Mage::CountCritInARow(crits, true));

    // crit, crit, crit, crit: the second and the fourth roll.
    CHECK(Mage::CountCritInARow(crits, true));
    CHECK(!Mage::CountCritInARow(crits, true));
    CHECK(Mage::CountCritInARow(crits, true));
    CHECK_EQ(crits, int32(0));
}
