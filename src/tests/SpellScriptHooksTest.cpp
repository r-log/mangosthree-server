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

/// Tests of the spell script hooks: their keys, a body per hook on a fresh registry, the dummy-effect context's cast
/// target and script word, the target context's cast target, the after-hit context's search around a unit in no world,
/// its cast through an aura the caster lacks and its Immune notice, the after-cast context's count and point, the aura
/// handle's reads, its removal of one effect, its stack count and the stack cap ModStackAmount holds a holder to,
/// RegisterRanks, the holder's script words, the holder copy's answer for a unit holding none and the period, tick
/// time, duration and amount a copy keeps, the low-health multiplier a script query answers from the caster's
/// class-script auras, and the game table, whose only hook rows are the mage's scripts.
///
/// The contexts are built on bare Creatures (update fields allocated; no map, template or AI) and a hand-made
/// Spell.dbc row that is not in the spell store. A bare creature is in no world, so each context's owner check
/// passes. The aura handle runs on a holder's storage that was never constructed, its fields written through the
/// holder's setters; Amount and DropCharge are not called there (the first reads an Aura, which this binary cannot
/// build, the second tells the clients). The script words of a constructed holder are read from a holder built, on a
/// spell put into the store as the registry's own holder test does, in storage whose bytes were all 0xA5. The periodic
/// damage query reads holders and auras made through their constructors, on spells put into the spell, effect and
/// duration stores, held in a bare creature's holder map; the copy is added through Unit::AddSpellAuraHolder to a bare
/// creature, which sends nothing out of a world, and removed through Unit::RemoveSpellAuraHolder. The effect removal,
/// the stack count and the stack cap run on holders made the same way, of auras whose apply and removal change nothing
/// on the unit; the cap's spell takes its three stacks from a SpellAuraOptions row put into its store.
/// The low-health multiplier reads class-script auras made through their constructors, on spells put into the spell,
/// effect and class options stores, held in a bare creature's per-type aura list.
///
/// The dispel-mechanic effect's count, and its mount dispel that takes every mount, run Spell::EffectDispelMechanic of a
/// spell built on a bare creature, with the target and the value Spell::HandleEffects would set, at holders made
/// through their constructor with no aura, their mechanic the spell's own (a SpellCategories row), held in a bare
/// creature's holder map; each holder the effect takes leaves through Unit::RemoveSpellAuraHolder.

#include "TestHarness.h"
#include "spells/SpellScriptHooks.h"
#include "combat/CombatNotices.h"
#include "spells/SpellScriptQueries.h"
#include "spells/auras/SpellAuras.h"
#include "spells/pipeline/Spell.h"
#include "Creature.h"
#include "DBCStores.h"
#include "data/ServerSpellStore.h"

#include <cmath>
#include <cstddef>
#include <list>
#include <new>
#include <vector>

namespace
{
    const uint32 kHookSpell = 94201;
    const uint32 kOtherSpell = 94202;
    const uint32 kStoredHolderSpell = 94203;
    const uint32 kDotSpell = 94204;
    const uint32 kOtherDotSpell = 94205;
    const uint32 kNotHeldSpell = 94206;
    const uint32 kCopiedDotSpell = 94210;
    const uint32 kThreeAuraSpell = 94251;
    const uint32 kStackedAuraSpell = 94252;
    const uint32 kCappedAuraSpell = 94253;
    const uint32 kCappedAuraOptions = 94253;    // a SpellAuraOptions row of three stacks

    // A creature with its update fields allocated and no world, map, template or AI.
    class BareCreature : public Creature
    {
        public:
            BareCreature()
            {
                _InitValues();
            }

            // Unit::CleanupsBeforeDelete tears down an in-world unit only while the update fields exist. The holders
            // HoldAura put in the map are the test's own and leave the map first.
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

    // A spell's records in the spell, effect and duration stores, kept alive for the rest of the run.
    struct StoredDot
    {
        ServerSpell spell;
        SpellDurationEntry duration;
        SpellEffectEntry dummy;
    };

    // Puts `spellId` into the stores once: effect 0 a periodic damage aura at the target every `period` ms, a duration
    // row of its own id of `duration` ms, and with `withDummy` effect 1 a dummy aura at the target.
    SpellEntry const* StoreDot(StoredDot& dot, uint32 spellId, uint32 period, int32 duration, bool withDummy)
    {
        if (dot.spell.Entry().ID == spellId)
        {
            return &dot.spell.Entry();
        }
        dot.duration.ID = spellId;
        dot.duration.Duration[0] = duration;
        dot.duration.Duration[1] = 0;
        dot.duration.Duration[2] = duration;
        sSpellDurationStore.SetEntry(spellId, &dot.duration);
        dot.spell.Entry().ID = spellId;
        dot.spell.Entry().DurationIndex = spellId;
        dot.spell.effect.SpellID = spellId;
        dot.spell.effect.EffectIndex = EFFECT_INDEX_0;
        dot.spell.effect.Effect = SPELL_EFFECT_APPLY_AURA;
        dot.spell.effect.EffectAura = SPELL_AURA_PERIODIC_DAMAGE;
        dot.spell.effect.EffectAuraPeriod = period;
        dot.spell.effect.ImplicitTarget_0 = TARGET_CHAIN_DAMAGE;
        sSpellStore.SetEntry(spellId, &dot.spell.Entry());
        AddSpellEffectEntry(&dot.spell.effect);
        if (withDummy)
        {
            dot.dummy = SpellEffectEntry();
            dot.dummy.SpellID = spellId;
            dot.dummy.EffectIndex = EFFECT_INDEX_1;
            dot.dummy.Effect = SPELL_EFFECT_APPLY_AURA;
            dot.dummy.EffectAura = SPELL_AURA_DUMMY;
            dot.dummy.ImplicitTarget_0 = TARGET_CHAIN_DAMAGE;
            AddSpellEffectEntry(&dot.dummy);
        }
        return &dot.spell.Entry();
    }

    // A holder of `spell` on `target` cast by `caster`, made through its constructor, with an aura made through
    // CreateAura at every effect the spell has: `amount` at effect 0, 0 at the others.
    SpellAuraHolder* DotHolder(SpellEntry const* spell, Unit& target, ObjectGuid const& caster, int32 amount)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(spell, &target, NULL, NULL, NULL);
        holder->SetCasterGuid(caster);
        for (int32 eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
        {
            if (!spell->GetSpellEffect(SpellEffectIndex(eff)))
            {
                continue;
            }
            int32 basePoints = eff == EFFECT_INDEX_0 ? amount : 0;
            holder->AddAura(CreateAura(spell, SpellEffectIndex(eff), &basePoints, holder, &target), SpellEffectIndex(eff));
        }
        return holder;
    }

    // A spell's records in the spell and effect stores, kept alive for the rest of the run.
    struct StoredAuras
    {
        ServerSpell spell;
        SpellEffectEntry more[MAX_EFFECT_INDEX - 1];
    };

    // Puts `spellId` into the stores once with `effects` effects, each an aura at the target that the unit's damage
    // taken reads and whose apply and removal change nothing (SPELL_AURA_MOD_DAMAGE_FROM_CASTER), of `basePoints`.
    SpellEntry const* StoreAuras(StoredAuras& rows, uint32 spellId, uint32 effects, int32 basePoints)
    {
        if (rows.spell.Entry().ID == spellId)
        {
            return &rows.spell.Entry();
        }
        rows.spell.Entry().ID = spellId;
        for (uint32 eff = 0; eff < effects && eff < MAX_EFFECT_INDEX; ++eff)
        {
            SpellEffectEntry& row = eff == 0 ? rows.spell.effect : rows.more[eff - 1];
            row = SpellEffectEntry();
            row.SpellID = spellId;
            row.EffectIndex = eff;
            row.Effect = SPELL_EFFECT_APPLY_AURA;
            row.EffectAura = SPELL_AURA_MOD_DAMAGE_FROM_CASTER;
            row.EffectBasePoints = basePoints;
            row.ImplicitTarget_0 = TARGET_CHAIN_DAMAGE;
            if (eff > 0)
            {
                AddSpellEffectEntry(&row);
            }
        }
        sSpellStore.SetEntry(spellId, &rows.spell.Entry());
        AddSpellEffectEntry(&rows.spell.effect);
        return &rows.spell.Entry();
    }

    // A holder of `spell` on `target`, made through its constructor with no caster, so the target is its caster, and an
    // aura made through CreateAura at every effect the spell has, of the effect's base points.
    SpellAuraHolder* AuraHolder(SpellEntry const* spell, Unit& target)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(spell, &target, NULL, NULL, NULL);
        for (int32 eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
        {
            SpellEffectEntry const* row = spell->GetSpellEffect(SpellEffectIndex(eff));
            if (!row)
            {
                continue;
            }
            int32 basePoints = row->EffectBasePoints;
            holder->AddAura(CreateAura(spell, SpellEffectIndex(eff), &basePoints, holder, &target), SpellEffectIndex(eff));
        }
        return holder;
    }

    struct SpellRowStorage
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    SpellRowStorage s_hookRow = {};
    SpellRowStorage s_storedRow = {};

    // A Spell.dbc row the contexts refer to; it is not in the spell store.
    SpellEntry const& HookSpell()
    {
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_hookRow.bytes);
        row->ID = kHookSpell;
        return *row;
    }

    // A Spell.dbc row put into the spell store, which a holder's constructor checks it against.
    SpellEntry const* StoredHolderSpell()
    {
        static bool seeded = false;
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_storedRow.bytes);
        if (!seeded)
        {
            seeded = true;
            row->ID = kStoredHolderSpell;
            sSpellStore.SetEntry(kStoredHolderSpell, row);
        }
        return row;
    }

    struct HolderStorage
    {
        alignas(SpellAuraHolder) unsigned char bytes[sizeof(SpellAuraHolder)];
    };

    // A holder's storage without its constructor, every byte set to `value`.
    SpellAuraHolder& HolderFilledWith(HolderStorage& storage, unsigned char value)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = value;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // The class-script rows of the low-health multiplier's vectors, in no other test: each a Spell.dbc row with one
    // effect, an OVERRIDE_CLASS_SCRIPTS aura at the caster with the script id, the amount and the class mask of the
    // client's row, under class options of the client's family.
    const uint32 kMoltenFury1 = 94301;          // 31679: script 4919, +4
    const uint32 kMoltenFury2 = 94302;          // 31680: script 4920, +8
    const uint32 kMoltenFury3 = 94303;          // 86880: script 12368, +12
    const uint32 kDeathsEmbrace3 = 94304;       // 47200, effect 1: script 6917, +12
    const uint32 kOtherScript = 94305;          // a class script of another id (Soul Siphon's 4992) on Molten Fury's mask
    // The spells asked about, with the class options of the client's rows; not in the spell store.
    const uint32 kPyroblastRow = 94306;         // 11366: mage, A 0x00400000, C 0x00001008
    const uint32 kIgniteRow = 94307;            // 12654: mage, A 0x08000000, C 0x00000008
    const uint32 kCorruptionRow = 94308;        // 172: warlock, A 0x00000002
    // The class options rows, by their own ids.
    const uint32 kOptionsMage = 94301;          // the talents' own rows: the family alone
    const uint32 kOptionsWarlock = 94302;
    const uint32 kOptionsPyroblast = 94303;
    const uint32 kOptionsIgnite = 94304;
    const uint32 kOptionsCorruption = 94305;

    // Molten Fury's mask over the mage's damage spells (A 0x20e012f7, B 0x00039048, C 0x00002080), and Death's
    // Embrace's over the warlock's Shadow spells (A 0x00082483, B 0x00059913).
    ClassFamilyMask MoltenFuryMask() { return ClassFamilyMask(UI64LIT(0x00039048) << 32 | UI64LIT(0x20e012f7), 0x00002080); }
    ClassFamilyMask DeathsEmbraceMask() { return ClassFamilyMask(UI64LIT(0x00059913) << 32 | UI64LIT(0x00082483), 0); }

    // One class script's records, kept alive for the rest of the run.
    struct StoredClassScript
    {
        ServerSpell spell;
    };

    // Puts the class script `spellId` into the spell and effect stores once: its effect `effIndex` an
    // OVERRIDE_CLASS_SCRIPTS aura at the caster of `amount` with the script id `script` over `mask`, under the class
    // options `options`.
    SpellEntry const* StoreClassScript(StoredClassScript& rows, uint32 spellId, SpellEffectIndex effIndex, int32 script,
                                       int32 amount, ClassFamilyMask const& mask, uint32 options)
    {
        if (rows.spell.Entry().ID == spellId)
        {
            return &rows.spell.Entry();
        }
        rows.spell.Entry().ID = spellId;
        rows.spell.Entry().ClassOptionsID = options;
        rows.spell.effect.SpellID = spellId;
        rows.spell.effect.EffectIndex = effIndex;
        rows.spell.effect.Effect = SPELL_EFFECT_APPLY_AURA;
        rows.spell.effect.EffectAura = SPELL_AURA_OVERRIDE_CLASS_SCRIPTS;
        rows.spell.effect.EffectBasePoints = amount;
        rows.spell.effect.EffectMiscValue_0 = script;
        rows.spell.effect.EffectSpellClassMask = mask;
        rows.spell.effect.ImplicitTarget_0 = TARGET_SELF;
        sSpellStore.SetEntry(spellId, &rows.spell.Entry());
        AddSpellEffectEntry(&rows.spell.effect);
        return &rows.spell.Entry();
    }

    // Puts the five class options rows into their store once.
    void StoreClassOptions()
    {
        static SpellClassOptionsEntry options[5];
        static bool stored = false;
        if (stored)
        {
            return;
        }
        stored = true;
        options[0].SpellClassSet = SPELLFAMILY_MAGE;
        options[1].SpellClassSet = SPELLFAMILY_WARLOCK;
        options[2].SpellClassSet = SPELLFAMILY_MAGE;
        options[2].SpellClassMask = ClassFamilyMask(UI64LIT(0x00400000), 0x00001008);
        options[3].SpellClassSet = SPELLFAMILY_MAGE;
        options[3].SpellClassMask = ClassFamilyMask(UI64LIT(0x08000000), 0x00000008);
        options[4].SpellClassSet = SPELLFAMILY_WARLOCK;
        options[4].SpellClassMask = ClassFamilyMask(UI64LIT(0x00000002));
        uint32 const ids[] = { kOptionsMage, kOptionsWarlock, kOptionsPyroblast, kOptionsIgnite, kOptionsCorruption };
        for (std::size_t i = 0; i < 5; ++i)
        {
            sSpellClassOptionsStore.SetEntry(ids[i], &options[i]);
        }
    }

    // A Spell.dbc row asked about: its id and its class options; it is not in the spell store.
    SpellEntry const& AskedRow(SpellRowStorage& storage, uint32 spellId, uint32 options)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        SpellEntry* row = reinterpret_cast<SpellEntry*>(storage.bytes);
        row->ID = spellId;
        row->ClassOptionsID = options;
        return *row;
    }

    // A bare creature whose class-script auras sit in its per-type list, as Unit::AddSpellAuraHolder files them, from
    // holders made through their constructors; it deletes them when it goes.
    class ClassScriptCaster : public BareCreature
    {
        public:
            ~ClassScriptCaster()
            {
                Drop();
            }

            // Holds the class script `spell` through its effect `effIndex`, at the amount of its row.
            void Hold(SpellEntry const* spell, SpellEffectIndex effIndex)
            {
                SpellAuraHolder* holder = new SpellAuraHolder(spell, this, NULL, NULL, NULL);
                holder->AddAura(CreateAura(spell, effIndex, NULL, holder, this), effIndex);
                AddAuraToModList(holder->GetAuraByEffectIndex(effIndex));
                m_held.push_back(holder);
            }

            // Takes every class script off the list and deletes its holder.
            void Drop()
            {
                m_auras.ByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS).clear();
                for (SpellAuraHolder* holder : m_held)
                {
                    delete holder;
                }
                m_held.clear();
            }

        private:
            std::vector<SpellAuraHolder*> m_held;
    };

    // A bare creature that holds damage-taken percent auras on the per-type list the engine reads them from, from holders
    // made through their constructors; it deletes them when it goes.
    class TakenAuraVictim : public BareCreature
    {
        public:
            ~TakenAuraVictim()
            {
                Drop();
            }

            // Holds `spell`'s effect 0, a MOD_DAMAGE_PERCENT_TAKEN or MOD_AOE_DAMAGE_AVOIDANCE aura.
            void Hold(SpellEntry const* spell)
            {
                SpellAuraHolder* holder = new SpellAuraHolder(spell, this, NULL, NULL, NULL);
                holder->AddAura(CreateAura(spell, EFFECT_INDEX_0, NULL, holder, this), EFFECT_INDEX_0);
                AddAuraToModList(holder->GetAuraByEffectIndex(EFFECT_INDEX_0));
                m_held.push_back(holder);
            }

            void Drop()
            {
                m_auras.ByType(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN).clear();
                m_auras.ByType(SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE).clear();
                for (SpellAuraHolder* holder : m_held)
                {
                    delete holder;
                }
                m_held.clear();
            }

        private:
            std::vector<SpellAuraHolder*> m_held;
    };

    // Puts a spell whose effect 0 is an `auraType` aura (MOD_DAMAGE_PERCENT_TAKEN unless given) of `amount` percent
    // over the school mask `schools` into the spell and effect stores once.
    SpellEntry const* StoreTakenAura(StoredClassScript& rows, uint32 spellId, int32 amount, int32 schools,
                                     AuraType auraType = SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN)
    {
        if (rows.spell.Entry().ID == spellId)
        {
            return &rows.spell.Entry();
        }
        rows.spell.Entry().ID = spellId;
        rows.spell.effect.SpellID = spellId;
        rows.spell.effect.EffectIndex = EFFECT_INDEX_0;
        rows.spell.effect.Effect = SPELL_EFFECT_APPLY_AURA;
        rows.spell.effect.EffectAura = auraType;
        rows.spell.effect.EffectBasePoints = amount;
        rows.spell.effect.EffectMiscValue_0 = schools;
        rows.spell.effect.ImplicitTarget_0 = TARGET_SELF;
        sSpellStore.SetEntry(spellId, &rows.spell.Entry());
        AddSpellEffectEntry(&rows.spell.effect);
        return &rows.spell.Entry();
    }

    // Sets the victim's health to `pct` percent of 1000 and its under-35% state as Unit::Update sets it.
    void SetHealthPct(Unit& victim, uint32 pct)
    {
        victim.SetMaxHealth(1000);
        victim.SetHealth(pct * 10);
        victim.ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT, victim.GetHealth() < victim.GetMaxHealth() * 0.35f);
    }

    bool Near(float value, float expected)
    {
        return std::fabs(value - expected) < 1e-6f;
    }

    // What the bodies saw, written by them and read by the tests.
    struct Seen
    {
        uint32 amount;
        bool crit;
        uint32 absorbed;
        bool heal;
        AuraRemoveMode mode;
        AuraRemoveMode handleMode;
        uint32 charges;
        int32 duration;
        uint64 casterGuid;
        int32 triggerAmount;
        uint32 cooldown;
        bool dummy;
        uint32 targetMode;
        Unit const* castTarget;
        int32 word;
        uint32 enemies;
        bool destination;
        float x, y, z;
        uint32 itemEntry;
    };

    Seen s_seen;

    void Forget()
    {
        s_seen = Seen();
    }

    SpellHandlerOutcome<void> TestVoidBody(EffectValueContext& ctx)
    {
        ctx.value += 1;
        return SpellHandlerOutcome<void>::Continue();
    }

    SpellHandlerOutcome<SpellAuraProcResult> TestProcBody(OnProcContext& /*ctx*/)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    SpellHandlerOutcome<SpellAuraProcResult> OtherProcBody(OnProcContext& /*ctx*/)
    {
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_OK);
    }
}

TEST(SpellScriptHooks_TheSeventeenKeysAre100To116AboveEveryLegacySite)
{
    uint32 const keys[] =
    {
        CheckCastHook::Key, SelectTargetsHook::Key, EffectValueHook::Key, DummyEffectHook::Key, AfterHitHook::Key,
        AuraAmountHook::Key, AuraApplyHook::Key, AuraRemoveHook::Key, PeriodicTickHook::Key, CheckProcHook::Key,
        OnProcHook::Key, DamageDoneHook::Key, CritChanceHook::Key, DummyAuraHook::Key, AfterCastHook::Key,
        LaunchHook::Key, CastStartHook::Key,
    };
    CHECK_EQ(sizeof(keys) / sizeof(keys[0]), std::size_t(17));
    for (std::size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
    {
        CHECK_EQ(keys[i], uint32(100 + i));
        // SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_DRUID is the last and highest legacy site.
        CHECK(keys[i] > uint32(SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_DRUID));
    }
    CHECK(AfterCastHook::Key > DummyAuraHook::Key);
    CHECK(LaunchHook::Key > AfterCastHook::Key);
    CHECK(CastStartHook::Key > LaunchHook::Key);

    // Every key the game registers sits at a legacy site, below the first hook, but the mage's scripts at the target,
    // check-proc, proc, damage-done, crit-chance, dummy aura, aura-apply, aura-remove, periodic-tick, dummy-effect,
    // after-cast, launch, cast-start and after-hit hooks.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    std::size_t belowHooks = 0;
    for (uint32 site = 0; site < uint32(SPELL_SCRIPT_HOOK_CHECK_CAST); ++site)
    {
        belowHooks += game.CountAt(site);
    }
    CHECK_EQ(belowHooks + game.CountAt(SelectTargetsHook::Key) + game.CountAt(CheckProcHook::Key) +
             game.CountAt(OnProcHook::Key) + game.CountAt(DamageDoneHook::Key) + game.CountAt(CritChanceHook::Key) +
             game.CountAt(DummyAuraHook::Key) + game.CountAt(AuraApplyHook::Key) + game.CountAt(AuraRemoveHook::Key) +
             game.CountAt(PeriodicTickHook::Key) + game.CountAt(DummyEffectHook::Key) +
             game.CountAt(AfterCastHook::Key) + game.CountAt(LaunchHook::Key) + game.CountAt(CastStartHook::Key) +
             game.CountAt(AfterHitHook::Key),
             game.Count());
}

TEST(SpellScriptHooks_TheCastStartHookCarriesTheSpellTheTargetAndTheCastTime)
{
    BareCreature caster;
    BareCreature target;
    SpellEntry const& spell = HookSpell();

    // The body notes the spell, the target and the cast time it was given; a fresh registry reaches it for its spell
    // and misses another.
    SpellHandlerRegistry registry;
    CHECK(registry.Register<CastStartHook>(kHookSpell, [](CastStartContext& ctx)
    {
        s_seen.castTarget = ctx.target;
        s_seen.amount = ctx.spell.ID;
        s_seen.duration = ctx.castTimeMs;
        return SpellHandlerOutcome<void>::Continue();
    }));
    CHECK_EQ(registry.CountAt(CastStartHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(LaunchHook::Key), std::size_t(0));

    Forget();
    CastStartContext aimed(caster, spell, &target, 1120);
    CHECK(registry.Dispatch<CastStartHook>(kHookSpell, aimed).IsContinue());
    CHECK(s_seen.castTarget == &target);
    CHECK_EQ(s_seen.amount, kHookSpell);
    CHECK_EQ(s_seen.duration, int32(1120));

    // An instant cast with no unit target carries both as they are; another spell's cast reaches no body.
    Forget();
    CastStartContext instant(caster, spell, NULL, 0);
    CHECK(registry.Dispatch<CastStartHook>(kHookSpell, instant).IsContinue());
    CHECK(s_seen.castTarget == NULL);
    CHECK_EQ(s_seen.duration, int32(0));
    Forget();
    CHECK(registry.Dispatch<CastStartHook>(kOtherSpell, aimed).IsMiss());
    CHECK_EQ(s_seen.amount, uint32(0));
}

TEST(SpellScriptHooks_TheLaunchHookCarriesTheCasterTheSpellAndTheCastTime)
{
    BareCreature caster;
    SpellEntry const& spell = HookSpell();

    // The body notes the caster, the spell and the cast time it was given and takes nothing else; a fresh registry
    // reaches it for its spell and misses another.
    SpellHandlerRegistry registry;
    CHECK(registry.Register<LaunchHook>(kHookSpell, [](LaunchContext& ctx)
    {
        s_seen.castTarget = &ctx.caster;
        s_seen.amount = ctx.spell.ID;
        s_seen.duration = int32(ctx.castTime);
        return SpellHandlerOutcome<void>::Continue();
    }));
    CHECK_EQ(registry.CountAt(LaunchHook::Key), std::size_t(1));

    Forget();
    LaunchContext launch(caster, spell, 2500);
    CHECK(registry.Dispatch<LaunchHook>(kHookSpell, launch).IsContinue());
    CHECK(s_seen.castTarget == &caster);
    CHECK_EQ(s_seen.amount, kHookSpell);
    CHECK_EQ(s_seen.duration, int32(2500));
    Forget();
    LaunchContext instant(caster, spell, 0);
    CHECK(registry.Dispatch<LaunchHook>(kHookSpell, instant).IsContinue());
    CHECK_EQ(s_seen.duration, int32(0));
    Forget();
    CHECK(registry.Dispatch<LaunchHook>(kOtherSpell, launch).IsMiss());
    CHECK(s_seen.castTarget == NULL);
}

TEST(SpellScriptHooks_TheAfterCastHookCarriesTheCastsCountAndPoint)
{
    BareCreature caster;
    BareCreature target;
    SpellEntry const& spell = HookSpell();

    // The body notes what the context carries; a fresh registry reaches it for its spell and misses another.
    SpellHandlerRegistry registry;
    CHECK(registry.Register<AfterCastHook>(kHookSpell, [](AfterCastContext& ctx)
    {
        s_seen.enemies = ctx.enemiesHit;
        s_seen.castTarget = ctx.target;
        s_seen.destination = ctx.hasDestination;
        s_seen.x = ctx.destX;
        s_seen.y = ctx.destY;
        s_seen.z = ctx.destZ;
        s_seen.itemEntry = ctx.castItemEntry;
        return SpellHandlerOutcome<void>::Continue();
    }));
    CHECK_EQ(registry.CountAt(AfterCastHook::Key), std::size_t(1));

    // A cast from an item, with two enemies hit and a destination, carries the count, the three floats, the flag and
    // the item's entry.
    Forget();
    AfterCastContext aimed(caster, spell, 2, &target, true, 12.5f, -40.25f, 7.0f, 36799);
    CHECK(registry.Dispatch<AfterCastHook>(kHookSpell, aimed).IsContinue());
    CHECK_EQ(s_seen.enemies, uint32(2));
    CHECK(s_seen.castTarget == &target);
    CHECK(s_seen.destination);
    CHECK(s_seen.x == 12.5f);
    CHECK(s_seen.y == -40.25f);
    CHECK(s_seen.z == 7.0f);
    CHECK_EQ(s_seen.itemEntry, uint32(36799));

    // A cast from no item, with no destination and no unit target, says so; another spell's cast reaches no body.
    Forget();
    AfterCastContext unaimed(caster, spell, 0, NULL, false, 1.0f, 2.0f, 3.0f, 0);
    CHECK(registry.Dispatch<AfterCastHook>(kHookSpell, unaimed).IsContinue());
    CHECK_EQ(s_seen.enemies, uint32(0));
    CHECK(s_seen.castTarget == NULL);
    CHECK(!s_seen.destination);
    CHECK_EQ(s_seen.itemEntry, uint32(0));
    CHECK(registry.Dispatch<AfterCastHook>(kOtherSpell, unaimed).IsMiss());
}

TEST(SpellScriptHooks_TheCastHooksReturnWhatTheirBodiesAnswer)
{
    BareCreature caster;
    BareCreature target;
    SpellEntry const& spell = HookSpell();

    SpellHandlerRegistry registry;
    CHECK(registry.Register<CheckCastHook>(kHookSpell, [](CheckCastContext& ctx)
    {
        return SpellHandlerOutcome<SpellCastResult>::Return(ctx.strict ? SPELL_FAILED_BAD_TARGETS : SPELL_CAST_OK);
    }));
    CheckCastContext checkCast(caster, &target, spell, true);
    SpellHandlerOutcome<SpellCastResult> refused = registry.Dispatch<CheckCastHook>(kHookSpell, checkCast);
    CHECK(refused.IsReturn());
    CHECK_EQ(int(refused.GetValue()), int(SPELL_FAILED_BAD_TARGETS));
    CHECK(registry.Dispatch<CheckCastHook>(kOtherSpell, checkCast).IsMiss());

    CHECK(registry.Register<SelectTargetsHook>(kHookSpell, [](SelectTargetsContext& ctx)
    {
        s_seen.targetMode = ctx.targetMode;
        ctx.targets.pop_front();
        ctx.maxTargets = 1;
        return SpellHandlerOutcome<void>::Return();
    }));
    std::list<Unit*> targets;
    targets.push_back(&caster);
    targets.push_back(&target);
    uint32 maxTargets = 0;
    Forget();
    SelectTargetsContext selectTargets(caster, spell, EFFECT_INDEX_1, 22, targets, maxTargets);
    CHECK(selectTargets.castTarget == NULL);
    CHECK(registry.Dispatch<SelectTargetsHook>(kHookSpell, selectTargets).IsReturn());
    CHECK_EQ(targets.size(), std::size_t(1));
    CHECK(targets.front() == &target);
    CHECK_EQ(s_seen.targetMode, uint32(22));
    CHECK_EQ(maxTargets, uint32(1));

    // A target context built with the cast's explicit target carries it.
    std::list<Unit*> aimedTargets;
    uint32 aimedMax = 0;
    SelectTargetsContext aimedSelect(caster, spell, EFFECT_INDEX_0, 53, aimedTargets, aimedMax, &target);
    CHECK(aimedSelect.castTarget == &target);

    CHECK(registry.Register<EffectValueHook>(kHookSpell, &TestVoidBody));
    int32 value = 41;
    EffectValueContext effectValue(caster, &target, spell, EFFECT_INDEX_0, value);
    CHECK(registry.Dispatch<EffectValueHook>(kHookSpell, effectValue).IsContinue());
    CHECK_EQ(value, 42);

    CHECK(registry.Register<DummyEffectHook>(kHookSpell, [](DummyEffectContext& ctx)
    {
        ctx.value = ctx.target ? 7 : 8;
        return SpellHandlerOutcome<void>::Return();
    }));
    int32 dummyValue = 0;
    int32 dummyWord = 0;
    DummyEffectContext dummyEffect(caster, &target, NULL, NULL, spell, EFFECT_INDEX_2, dummyValue, ObjectGuid(), &target,
                                   dummyWord);
    CHECK(registry.Dispatch<DummyEffectHook>(kHookSpell, dummyEffect).IsReturn());
    CHECK_EQ(dummyValue, 7);

    CHECK(registry.Register<AfterHitHook>(kHookSpell, [](AfterHitContext& ctx)
    {
        s_seen.amount = ctx.amount;
        s_seen.crit = ctx.crit;
        s_seen.absorbed = ctx.absorbed;
        s_seen.heal = ctx.heal;
        return SpellHandlerOutcome<void>::Continue();
    }));
    Forget();
    AfterHitContext afterHit(caster, target, spell, 1200, true, 300, 0, true);
    CHECK(registry.Dispatch<AfterHitHook>(kHookSpell, afterHit).IsContinue());
    CHECK_EQ(s_seen.amount, uint32(1200));
    CHECK(s_seen.crit);
    CHECK_EQ(s_seen.absorbed, uint32(300));
    CHECK(s_seen.heal);

    // The damage-done and crit-chance bodies read the damage type their context carries: a periodic effect's amount
    // or tick (DOT) and a hit (SPELL_DIRECT_DAMAGE) each get their own change.
    CHECK(registry.Register<DamageDoneHook>(kHookSpell, [](DamageDoneContext& ctx)
    {
        ctx.doneTotalMod *= ctx.damageType == DOT ? 2.0f : 4.0f;
        return SpellHandlerOutcome<void>::Continue();
    }));
    float doneTotalMod = 1.5f;
    DamageDoneContext damageDone(caster, target, spell, DOT, doneTotalMod);
    CHECK(registry.Dispatch<DamageDoneHook>(kHookSpell, damageDone).IsContinue());
    CHECK(doneTotalMod == 3.0f);
    float hitMod = 1.0f;
    DamageDoneContext hitDone(caster, target, spell, SPELL_DIRECT_DAMAGE, hitMod);
    CHECK(registry.Dispatch<DamageDoneHook>(kHookSpell, hitDone).IsContinue());
    CHECK(hitMod == 4.0f);

    CHECK(registry.Register<CritChanceHook>(kHookSpell, [](CritChanceContext& ctx)
    {
        ctx.critChance += ctx.damageType == DOT ? 50.0f : 10.0f;
        return SpellHandlerOutcome<void>::Continue();
    }));
    float critChance = 5.0f;
    CritChanceContext critChanceCtx(caster, target, spell, DOT, critChance);
    CHECK(registry.Dispatch<CritChanceHook>(kHookSpell, critChanceCtx).IsContinue());
    CHECK(critChance == 55.0f);
    CHECK(registry.Dispatch<CritChanceHook>(kOtherSpell, critChanceCtx).IsMiss());
    CHECK(critChance == 55.0f);
    float hitChance = 5.0f;
    CritChanceContext hitCrit(caster, target, spell, SPELL_DIRECT_DAMAGE, hitChance);
    CHECK(registry.Dispatch<CritChanceHook>(kHookSpell, hitCrit).IsContinue());
    CHECK(hitChance == 15.0f);
}

TEST(SpellScriptHooks_ADummyEffectContextCarriesTheCastsTargetAndItsWord)
{
    BareCreature caster;
    BareCreature stunned;
    BareCreature beside;
    SpellEntry const& spell = HookSpell();

    // The body notes the cast's target and the word as it finds them, then adds one to the word.
    SpellHandlerRegistry registry;
    CHECK(registry.Register<DummyEffectHook>(kHookSpell, [](DummyEffectContext& ctx)
    {
        s_seen.castTarget = ctx.castTarget;
        s_seen.word = ctx.word;
        ++ctx.word;
        return SpellHandlerOutcome<void>::Return();
    }));

    // One cast's word, zero as the Spell makes it, seen by the effect at each of the cast's targets: the cast's target
    // is the same at both, and the second finds the first's write.
    int32 value = 0;
    int32 word = 0;
    Forget();
    DummyEffectContext atStunned(caster, &stunned, NULL, NULL, spell, EFFECT_INDEX_1, value, ObjectGuid(), &stunned,
                                 word);
    CHECK(registry.Dispatch<DummyEffectHook>(kHookSpell, atStunned).IsReturn());
    CHECK(s_seen.castTarget == &stunned);
    CHECK_EQ(s_seen.word, 0);
    CHECK_EQ(word, 1);
    Forget();
    DummyEffectContext atBeside(caster, &beside, NULL, NULL, spell, EFFECT_INDEX_1, value, ObjectGuid(), &stunned, word);
    CHECK(registry.Dispatch<DummyEffectHook>(kHookSpell, atBeside).IsReturn());
    CHECK(atBeside.target == &beside);
    CHECK(s_seen.castTarget == &stunned);
    CHECK_EQ(s_seen.word, 1);
    CHECK_EQ(word, 2);

    // A cast with no unit target gives NULL; another cast's word is its own.
    int32 otherWord = 0;
    Forget();
    DummyEffectContext noTarget(caster, NULL, NULL, NULL, spell, EFFECT_INDEX_0, value, ObjectGuid(), NULL, otherWord);
    CHECK(registry.Dispatch<DummyEffectHook>(kHookSpell, noTarget).IsReturn());
    CHECK(s_seen.castTarget == NULL);
    CHECK_EQ(s_seen.word, 0);
    CHECK_EQ(otherWord, 1);
    CHECK_EQ(word, 2);
}

TEST(SpellScriptHooks_TheAuraHooksReturnWhatTheirBodiesAnswer)
{
    BareCreature owner;
    BareCreature other;
    SpellEntry const& spell = HookSpell();
    HolderStorage storage;
    SpellAuraHolder& holder = HolderFilledWith(storage, 0);
    holder.SetRemoveMode(AURA_REMOVE_BY_DISPEL);
    holder.SetAuraCharges(2, false);
    holder.SetAuraDuration(9000);

    SpellHandlerRegistry registry;
    CHECK(registry.Register<AuraAmountHook>(kHookSpell, [](AuraAmountContext& ctx)
    {
        ctx.amount = ctx.caster ? ctx.amount * 3 : ctx.amount * 2;
        return SpellHandlerOutcome<void>::Continue();
    }));
    int32 amount = 10;
    AuraAmountContext auraAmount(NULL, owner, spell, EFFECT_INDEX_0, amount);
    CHECK(registry.Dispatch<AuraAmountHook>(kHookSpell, auraAmount).IsContinue());
    CHECK_EQ(amount, 20);

    CHECK(registry.Register<AuraApplyHook>(kHookSpell, [](AuraApplyContext& ctx)
    {
        ctx.aura.ScriptWord(EFFECT_INDEX_1) = ctx.real ? 5 : 6;
        return SpellHandlerOutcome<void>::Return();
    }));
    AuraApplyContext auraApply(owner, &other, spell, AuraHandle(holder), true);
    CHECK(registry.Dispatch<AuraApplyHook>(kHookSpell, auraApply).IsReturn());
    CHECK_EQ(holder.ScriptWord(EFFECT_INDEX_1), 5);

    CHECK(registry.Register<AuraRemoveHook>(kHookSpell, [](AuraRemoveContext& ctx)
    {
        s_seen.mode = ctx.mode;
        s_seen.handleMode = ctx.aura.RemoveMode();
        return SpellHandlerOutcome<void>::Return();
    }));
    Forget();
    AuraRemoveContext auraRemove(owner, NULL, spell, AuraHandle(holder), AURA_REMOVE_BY_DISPEL);
    CHECK(registry.Dispatch<AuraRemoveHook>(kHookSpell, auraRemove).IsReturn());
    CHECK_EQ(int(s_seen.mode), int(AURA_REMOVE_BY_DISPEL));
    CHECK_EQ(int(s_seen.handleMode), int(AURA_REMOVE_BY_DISPEL));

    CHECK(registry.Register<PeriodicTickHook>(kHookSpell, [](PeriodicTickContext& ctx)
    {
        s_seen.dummy = ctx.dummy;
        s_seen.duration = ctx.aura.Duration();
        ctx.amount = 0;
        return SpellHandlerOutcome<void>::Return();
    }));
    Forget();
    uint32 tick = 250;
    PeriodicTickContext periodicTick(owner, &other, spell, EFFECT_INDEX_2, AuraHandle(holder), tick, true);
    CHECK(registry.Dispatch<PeriodicTickHook>(kHookSpell, periodicTick).IsReturn());
    CHECK_EQ(tick, uint32(0));
    CHECK(s_seen.dummy);
    CHECK_EQ(s_seen.duration, 9000);

    CHECK(registry.Register<CheckProcHook>(kHookSpell, [](CheckProcContext& ctx)
    {
        s_seen.charges = ctx.aura.Charges();
        return SpellHandlerOutcome<bool>::Return(false);
    }));
    Forget();
    CheckProcContext checkProc(owner, &other, spell, AuraHandle(holder), NULL, 0x10000, 0x2, 900);
    SpellHandlerOutcome<bool> vetoed = registry.Dispatch<CheckProcHook>(kHookSpell, checkProc);
    CHECK(vetoed.IsReturn());
    CHECK(!vetoed.GetValue());
    CHECK_EQ(s_seen.charges, uint32(2));

    CHECK(registry.Register<OnProcHook>(kHookSpell, [](OnProcContext& ctx)
    {
        s_seen.triggerAmount = ctx.triggerAmount;
        s_seen.cooldown = ctx.cooldown;
        return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_CANT_TRIGGER);
    }));
    Forget();
    SpellCooldownMgr cooldowns;
    OnProcContext onProc(owner, &other, spell, EFFECT_INDEX_0, AuraHandle(holder), NULL, 0x10000, 0x2, 900, 13,
                         ObjectGuid(), 6, NULL, cooldowns);
    CHECK(onProc.CastItem() == NULL);
    SpellHandlerOutcome<SpellAuraProcResult> procResult = registry.Dispatch<OnProcHook>(kHookSpell, onProc);
    CHECK(procResult.IsReturn());
    CHECK_EQ(int(procResult.GetValue()), int(SPELL_AURA_PROC_CANT_TRIGGER));
    CHECK_EQ(s_seen.triggerAmount, 13);
    CHECK_EQ(s_seen.cooldown, uint32(6));
    CHECK(registry.Dispatch<OnProcHook>(kOtherSpell, onProc).IsMiss());

    // The dummy aura body doubles the amount and sets its effect's script word at the apply and goes on; at the
    // un-apply it ends the handler. The context has no aura to cast with: the body casts nothing.
    CHECK(registry.Register<DummyAuraHook>(kHookSpell, [](DummyAuraContext& ctx)
    {
        s_seen.mode = ctx.mode;
        s_seen.duration = ctx.aura.Duration();
        if (ctx.apply)
        {
            ctx.amount *= 2;
            ctx.aura.ScriptWord(ctx.effIndex) = 9;
            return SpellHandlerOutcome<void>::Continue();
        }
        return SpellHandlerOutcome<void>::Return();
    }));
    Forget();
    int32 dummyAmount = 44;
    DummyAuraContext dummyApply(owner, &other, spell, EFFECT_INDEX_1, AuraHandle(holder), true, AURA_REMOVE_BY_DEFAULT,
                                dummyAmount, NULL);
    CHECK(registry.Dispatch<DummyAuraHook>(kHookSpell, dummyApply).IsContinue());
    CHECK_EQ(dummyAmount, 88);
    CHECK_EQ(holder.ScriptWord(EFFECT_INDEX_1), 9);
    CHECK_EQ(int(s_seen.mode), int(AURA_REMOVE_BY_DEFAULT));
    CHECK_EQ(s_seen.duration, 9000);
    Forget();
    DummyAuraContext dummyRemove(owner, NULL, spell, EFFECT_INDEX_1, AuraHandle(holder), false, AURA_REMOVE_BY_EXPIRE,
                                 dummyAmount, NULL);
    CHECK(registry.Dispatch<DummyAuraHook>(kHookSpell, dummyRemove).IsReturn());
    CHECK_EQ(int(s_seen.mode), int(AURA_REMOVE_BY_EXPIRE));
    CHECK_EQ(dummyAmount, 88);
    CHECK(registry.Dispatch<DummyAuraHook>(kOtherSpell, dummyRemove).IsMiss());
}

TEST(SpellScriptHooks_TheAuraHandleReadsTheHoldersFields)
{
    static unsigned char const fills[] = { 0x00, 0xA5 };
    for (unsigned char fill : fills)
    {
        HolderStorage storage;
        SpellAuraHolder& holder = HolderFilledWith(storage, fill);
        ObjectGuid const caster(HIGHGUID_UNIT, uint32(2961), uint32(400375));
        holder.SetAuraCharges(3, false);
        holder.SetRemoveMode(AURA_REMOVE_BY_EXPIRE);
        holder.SetAuraDuration(4500);
        holder.SetCasterGuid(caster);
        holder.ScriptWord(EFFECT_INDEX_0) = -1;
        holder.ScriptWord(EFFECT_INDEX_1) = 2;
        holder.ScriptWord(EFFECT_INDEX_2) = 0;

        AuraHandle handle(holder);
        CHECK_EQ(handle.Charges(), uint32(3));
        CHECK_EQ(int(handle.RemoveMode()), int(AURA_REMOVE_BY_EXPIRE));
        CHECK_EQ(handle.Duration(), 4500);
        CHECK(handle.CasterGuid() == caster);
        CHECK_EQ(handle.ScriptWord(EFFECT_INDEX_0), -1);
        CHECK_EQ(handle.ScriptWord(EFFECT_INDEX_1), 2);
        CHECK_EQ(handle.ScriptWord(EFFECT_INDEX_2), 0);

        // The word is the holder's: a write through the handle is read back from the holder.
        handle.ScriptWord(EFFECT_INDEX_2) = 77;
        CHECK_EQ(holder.ScriptWord(EFFECT_INDEX_2), 77);
        CHECK_EQ(holder.ScriptWord(EFFECT_INDEX_1), 2);
    }
}

TEST(SpellScriptHooks_TheAuraHandleTakesOneEffectOffItsHolderButNeverTheLast)
{
    // A holder of three auras of 7 made for a bare creature that does not hold it, as at the aura-apply hook's call
    // before the add: the handle takes the aura at effect 0 out of the holder alone, with its effect bit, and leaves
    // the other two as they were.
    static StoredAuras rows;
    SpellEntry const* spell = StoreAuras(rows, kThreeAuraSpell, MAX_EFFECT_INDEX, 7);
    BareCreature target;
    SpellAuraHolder* holder = AuraHolder(spell, target);
    uint32 const effectBits = AFLAG_EFF_INDEX_0 | AFLAG_EFF_INDEX_1 | AFLAG_EFF_INDEX_2;
    AuraHandle handle(*holder);
    CHECK_EQ(uint32(holder->GetAuraFlags()) & effectBits, effectBits);

    handle.RemoveEffect(EFFECT_INDEX_0);
    CHECK(holder->GetAuraByEffectIndex(EFFECT_INDEX_0) == NULL);
    CHECK(holder->GetAuraByEffectIndex(EFFECT_INDEX_1) != NULL);
    CHECK(holder->GetAuraByEffectIndex(EFFECT_INDEX_2) != NULL);
    CHECK_EQ(handle.Amount(EFFECT_INDEX_0), int32(0));
    CHECK_EQ(handle.Amount(EFFECT_INDEX_1), int32(7));
    CHECK_EQ(handle.Amount(EFFECT_INDEX_2), int32(7));
    CHECK_EQ(uint32(holder->GetAuraFlags()) & effectBits, uint32(AFLAG_EFF_INDEX_1 | AFLAG_EFF_INDEX_2));

    // An effect the holder no longer has changes nothing.
    handle.RemoveEffect(EFFECT_INDEX_0);
    CHECK(holder->GetAuraByEffectIndex(EFFECT_INDEX_1) != NULL);
    CHECK(holder->GetAuraByEffectIndex(EFFECT_INDEX_2) != NULL);

    // Its last aura stays: the holder is never left empty by the handle.
    handle.RemoveEffect(EFFECT_INDEX_1);
    CHECK(holder->GetAuraByEffectIndex(EFFECT_INDEX_1) == NULL);
    handle.RemoveEffect(EFFECT_INDEX_2);
    CHECK(holder->GetAuraByEffectIndex(EFFECT_INDEX_2) != NULL);
    CHECK_EQ(handle.Amount(EFFECT_INDEX_2), int32(7));
    CHECK_EQ(uint32(holder->GetAuraFlags()) & effectBits, uint32(AFLAG_EFF_INDEX_2));
    CHECK(target.GetSpellAuraHolderMap().empty());
    delete holder;
}

TEST(SpellScriptHooks_TheAuraHandleTakesAnEffectOffAHolderItsTargetHolds)
{
    // The same holder held in the bare creature's holder map: the aura at effect 0 comes off the target through the
    // unit's removal of one aura and out of the holder, with its effect bit; the other two and the holder stay.
    static StoredAuras rows;
    SpellEntry const* spell = StoreAuras(rows, kThreeAuraSpell, MAX_EFFECT_INDEX, 7);
    BareCreature target;
    SpellAuraHolder* holder = AuraHolder(spell, target);
    target.HoldAura(kThreeAuraSpell, *holder);
    uint32 const effectBits = AFLAG_EFF_INDEX_0 | AFLAG_EFF_INDEX_1 | AFLAG_EFF_INDEX_2;
    AuraHandle handle(*holder);

    handle.RemoveEffect(EFFECT_INDEX_0);
    CHECK(holder->GetAuraByEffectIndex(EFFECT_INDEX_0) == NULL);
    CHECK(holder->GetAuraByEffectIndex(EFFECT_INDEX_1) != NULL);
    CHECK(holder->GetAuraByEffectIndex(EFFECT_INDEX_2) != NULL);
    CHECK_EQ(uint32(holder->GetAuraFlags()) & effectBits, uint32(AFLAG_EFF_INDEX_1 | AFLAG_EFF_INDEX_2));
    CHECK(target.GetSpellAuraHolder(kThreeAuraSpell) == holder);
    delete holder;
}

TEST(SpellScriptHooks_TheAuraHandleSetsTheStacksAsAStackChangeDoes)
{
    // A holder of one aura of -40 on a bare creature that is its caster: two stacks make the amount -80, one stack
    // -40 again, each recomputed from the aura's base points; 0 stacks changes nothing.
    static StoredAuras rows;
    SpellEntry const* spell = StoreAuras(rows, kStackedAuraSpell, 1, -40);
    BareCreature target;
    SpellAuraHolder* holder = AuraHolder(spell, target);
    AuraHandle handle(*holder);
    CHECK_EQ(handle.Stacks(), uint32(1));
    CHECK_EQ(handle.Amount(EFFECT_INDEX_0), int32(-40));

    handle.SetStacks(2);
    CHECK_EQ(handle.Stacks(), uint32(2));
    CHECK_EQ(holder->GetStackAmount(), uint32(2));
    CHECK_EQ(handle.Amount(EFFECT_INDEX_0), int32(-80));

    handle.SetStacks(1);
    CHECK_EQ(handle.Stacks(), uint32(1));
    CHECK_EQ(handle.Amount(EFFECT_INDEX_0), int32(-40));

    handle.SetStacks(0);
    CHECK_EQ(handle.Stacks(), uint32(1));
    CHECK_EQ(handle.Amount(EFFECT_INDEX_0), int32(-40));

    // The amount written through the handle is the aura's; an effect the holder has no aura at takes nothing.
    handle.SetAmount(EFFECT_INDEX_0, -25);
    CHECK_EQ(handle.Amount(EFFECT_INDEX_0), int32(-25));
    handle.SetAmount(EFFECT_INDEX_1, 9);
    CHECK_EQ(handle.Amount(EFFECT_INDEX_1), int32(0));
    delete holder;
}

TEST(SpellScriptHooks_AStackCapHoldsAReapplicationAtTheCapAndRefreshesTheHolder)
{
    // A holder of one aura of -40 on a bare creature that is its caster, of a spell whose aura options allow three
    // stacks, with 4000 ms of its 9000 left.
    static StoredAuras rows;
    static SpellAuraOptionsEntry options;
    SpellEntry const* spell = StoreAuras(rows, kCappedAuraSpell, 1, -40);
    options = SpellAuraOptionsEntry();
    options.CumulativeAura = 3;
    sSpellAuraOptionsStore.SetEntry(kCappedAuraOptions, &options);
    rows.spell.Entry().AuraOptionsID = kCappedAuraOptions;
    CHECK_EQ(spell->GetStackAmount(), uint32(3));
    BareCreature target;
    SpellAuraHolder* holder = AuraHolder(spell, target);
    holder->SetAuraMaxDuration(9000);
    holder->SetAuraDuration(4000);
    AuraHandle handle(*holder);
    CHECK_EQ(holder->GetStackCap(), uint32(0));

    // Capped at one: a stack added to its one leaves one and the amount, refreshes its time and keeps the holder.
    handle.CapStacks(1);
    CHECK_EQ(holder->GetStackCap(), uint32(1));
    CHECK(!holder->ModStackAmount(1));
    CHECK_EQ(handle.Stacks(), uint32(1));
    CHECK_EQ(handle.Amount(EFFECT_INDEX_0), int32(-40));
    CHECK_EQ(handle.Duration(), int32(9000));

    // The cap given back: the spell's three. Two stacks make -80, three -120, and a fourth stays at three.
    handle.CapStacks(0);
    CHECK_EQ(holder->GetStackCap(), uint32(0));
    CHECK(!holder->ModStackAmount(1));
    CHECK_EQ(handle.Stacks(), uint32(2));
    CHECK_EQ(handle.Amount(EFFECT_INDEX_0), int32(-80));
    CHECK(!holder->ModStackAmount(1));
    CHECK(!holder->ModStackAmount(1));
    CHECK_EQ(handle.Stacks(), uint32(3));
    CHECK_EQ(handle.Amount(EFFECT_INDEX_0), int32(-120));
    CHECK(target.GetSpellAuraHolderMap().empty());
    delete holder;
}

TEST(SpellScriptHooks_AHolderIsMadeWithItsScriptWordsAtZero)
{
    // The holder is made in storage whose every byte is 0xA5, so a word reads zero only when the constructor wrote it.
    BareCreature target;
    HolderStorage storage;
    HolderFilledWith(storage, 0xA5);
    SpellAuraHolder* fresh = new (storage.bytes) SpellAuraHolder(StoredHolderSpell(), &target, NULL, NULL, NULL);
    CHECK_EQ(fresh->ScriptWord(EFFECT_INDEX_0), 0);
    CHECK_EQ(fresh->ScriptWord(EFFECT_INDEX_1), 0);
    CHECK_EQ(fresh->ScriptWord(EFFECT_INDEX_2), 0);

    fresh->ScriptWord(EFFECT_INDEX_1) = 2;
    CHECK_EQ(fresh->ScriptWord(EFFECT_INDEX_0), 0);
    CHECK_EQ(fresh->ScriptWord(EFFECT_INDEX_1), 2);
    CHECK_EQ(fresh->ScriptWord(EFFECT_INDEX_2), 0);
    fresh->~SpellAuraHolder();
}

TEST(SpellScriptHooks_FindPeriodicDamageReadsTheCastersPeriodicDamageEffectsOnAUnit)
{
    // Two DoTs: one ticking every 3000 ms for 12 s with a dummy aura at effect 1, one every 2000 ms for 4 s. The mage
    // holds both on the target and another caster the first.
    static StoredDot dotRows;
    static StoredDot otherRows;
    SpellEntry const* dot = StoreDot(dotRows, kDotSpell, 3000, 12000, true);
    SpellEntry const* other = StoreDot(otherRows, kOtherDotSpell, 2000, 4000, false);
    BareCreature target;
    ObjectGuid const mage(HIGHGUID_UNIT, uint32(2961), uint32(400376));
    ObjectGuid const otherCaster(HIGHGUID_UNIT, uint32(2961), uint32(400377));
    SpellAuraHolder* mine = DotHolder(dot, target, mage, 500);
    SpellAuraHolder* theirs = DotHolder(dot, target, otherCaster, 900);
    SpellAuraHolder* mineToo = DotHolder(other, target, mage, 2000);
    mineToo->GetAuraByEffectIndex(EFFECT_INDEX_0)->GetModifier()->periodictime = 1600;
    target.HoldAura(kDotSpell, *mine);
    target.HoldAura(kDotSpell, *theirs);
    target.HoldAura(kOtherDotSpell, *mineToo);

    // The mage's, in the order of the ids asked for: the other caster's holder, the dummy effect and the id the unit
    // does not hold give nothing. Each tick, period and time left is the aura's and the holder's; the ticks left are
    // the aura's count over the holder's duration, none dealt yet.
    uint32 const asked[] = { kOtherDotSpell, kNotHeldSpell, kDotSpell };
    std::vector<PeriodicDamageView> found(1);
    CHECK_EQ(FindPeriodicDamage(target, mage, asked, 3, found), std::size_t(2));
    CHECK_EQ(found.size(), std::size_t(2));
    if (found.size() == 2)
    {
        CHECK_EQ(found[0].spellId, kOtherDotSpell);
        CHECK_EQ(int(found[0].effIndex), int(EFFECT_INDEX_0));
        CHECK_EQ(found[0].amount, int32(2000));
        CHECK_EQ(found[0].periodMs, uint32(1600));
        CHECK_EQ(found[0].remainingMs, int32(4000));
        CHECK_EQ(found[0].remainingTicks, uint32(2));
        CHECK_EQ(found[0].amountBeforeDone, int32(2000));
        CHECK_EQ(found[0].stacks, uint32(1));
        CHECK_EQ(found[0].basePeriodMs, uint32(2000));
        CHECK_EQ(found[1].spellId, kDotSpell);
        CHECK_EQ(int(found[1].effIndex), int(EFFECT_INDEX_0));
        CHECK_EQ(found[1].amount, int32(500));
        CHECK_EQ(found[1].periodMs, uint32(3000));
        CHECK_EQ(found[1].remainingMs, int32(12000));
        CHECK_EQ(found[1].remainingTicks, uint32(4));
    }

    // The other caster's holder is his alone, and its time left is the holder's as it runs down.
    theirs->SetAuraDuration(7500);
    CHECK_EQ(FindPeriodicDamage(target, otherCaster, asked, 3, found), std::size_t(1));
    CHECK(found.size() == 1 && found[0].spellId == kDotSpell && found[0].amount == 900 && found[0].remainingMs == 7500);

    // A holder already removed from the unit, which waits in its map for deletion, gives nothing.
    mineToo->SetDeleted();
    CHECK_EQ(FindPeriodicDamage(target, mage, asked, 3, found), std::size_t(1));
    CHECK(found.size() == 1 && found[0].spellId == kDotSpell && found[0].amount == 500);

    // A caster with nothing on the unit, and a list of no ids: nothing, and `out` is cleared.
    CHECK_EQ(FindPeriodicDamage(target, ObjectGuid(), asked, 3, found), std::size_t(0));
    CHECK(found.empty());
    CHECK_EQ(FindPeriodicDamage(target, mage, asked + 2, 1, found), std::size_t(1));
    CHECK_EQ(FindPeriodicDamage(target, mage, asked, 0, found), std::size_t(0));
    CHECK(found.empty());

    delete mineToo;
    delete theirs;
    delete mine;
}

TEST(SpellScriptHooks_LowHealthDamageMultiplierReadsTheCastersClassScriptsAgainstTheVictimsHealth)
{
    StoreClassOptions();
    static StoredClassScript moltenFury1Rows;
    static StoredClassScript moltenFury2Rows;
    static StoredClassScript moltenFury3Rows;
    static StoredClassScript deathsEmbraceRows;
    static StoredClassScript otherScriptRows;
    SpellEntry const* moltenFury1 = StoreClassScript(moltenFury1Rows, kMoltenFury1, EFFECT_INDEX_0, 4919, 4, MoltenFuryMask(), kOptionsMage);
    SpellEntry const* moltenFury2 = StoreClassScript(moltenFury2Rows, kMoltenFury2, EFFECT_INDEX_0, 4920, 8, MoltenFuryMask(), kOptionsMage);
    SpellEntry const* moltenFury3 = StoreClassScript(moltenFury3Rows, kMoltenFury3, EFFECT_INDEX_0, 12368, 12, MoltenFuryMask(), kOptionsMage);
    SpellEntry const* deathsEmbrace = StoreClassScript(deathsEmbraceRows, kDeathsEmbrace3, EFFECT_INDEX_1, 6917, 12, DeathsEmbraceMask(),
                                                       kOptionsWarlock);
    SpellEntry const* otherScript = StoreClassScript(otherScriptRows, kOtherScript, EFFECT_INDEX_0, 4992, 20, MoltenFuryMask(), kOptionsMage);

    SpellRowStorage pyroblastStorage, igniteStorage, corruptionStorage;
    SpellEntry const& pyroblast = AskedRow(pyroblastStorage, kPyroblastRow, kOptionsPyroblast);
    SpellEntry const& ignite = AskedRow(igniteStorage, kIgniteRow, kOptionsIgnite);
    SpellEntry const& corruption = AskedRow(corruptionStorage, kCorruptionRow, kOptionsCorruption);

    BareCreature victim;
    ClassScriptCaster caster;

    // No class script: 1.0 at any health.
    SetHealthPct(victim, 30);
    CHECK(victim.HasAuraState(AURA_STATE_HEALTHLESS_35_PERCENT));
    CHECK(Near(Spells::LowHealthDamageMultiplier(caster, victim, pyroblast), 1.0f));

    // Rank 3 on Pyroblast: 1.12 under 35%, 1.0 at 40%, and 1.12 again when the victim falls back under it.
    caster.Hold(moltenFury3, EFFECT_INDEX_0);
    CHECK(Near(Spells::LowHealthDamageMultiplier(caster, victim, pyroblast), 1.12f));
    SetHealthPct(victim, 40);
    CHECK(!victim.HasAuraState(AURA_STATE_HEALTHLESS_35_PERCENT));
    CHECK(Near(Spells::LowHealthDamageMultiplier(caster, victim, pyroblast), 1.0f));
    SetHealthPct(victim, 34);
    CHECK(Near(Spells::LowHealthDamageMultiplier(caster, victim, pyroblast), 1.12f));

    // Ignite's row is outside Molten Fury's mask: 1.0 whatever the health.
    SetHealthPct(victim, 30);
    CHECK(Near(Spells::LowHealthDamageMultiplier(caster, victim, ignite), 1.0f));
    SetHealthPct(victim, 100);
    CHECK(Near(Spells::LowHealthDamageMultiplier(caster, victim, ignite), 1.0f));

    // Rank 1 alone: 1.04.
    caster.Drop();
    caster.Hold(moltenFury1, EFFECT_INDEX_0);
    SetHealthPct(victim, 30);
    CHECK(Near(Spells::LowHealthDamageMultiplier(caster, victim, pyroblast), 1.04f));

    // Two class scripts that affect the spell multiply: 1.08 x 1.12 = 1.2096.
    caster.Drop();
    caster.Hold(moltenFury2, EFFECT_INDEX_0);
    caster.Hold(moltenFury3, EFFECT_INDEX_0);
    CHECK(Near(Spells::LowHealthDamageMultiplier(caster, victim, pyroblast), 1.08f * 1.12f));

    // A class script of another id over the same mask gives nothing here.
    caster.Drop();
    caster.Hold(otherScript, EFFECT_INDEX_0);
    CHECK(Near(Spells::LowHealthDamageMultiplier(caster, victim, pyroblast), 1.0f));

    // Death's Embrace rank 3 (its effect 1) reaches the warlock's spells in its mask, at the same 35%, and not the
    // mage's: the family of the spell must be the script's.
    caster.Drop();
    caster.Hold(deathsEmbrace, EFFECT_INDEX_1);
    caster.Hold(moltenFury2, EFFECT_INDEX_0);
    CHECK(Near(Spells::LowHealthDamageMultiplier(caster, victim, corruption), 1.12f));
    CHECK(Near(Spells::LowHealthDamageMultiplier(caster, victim, pyroblast), 1.08f));
    SetHealthPct(victim, 40);
    CHECK(Near(Spells::LowHealthDamageMultiplier(caster, victim, corruption), 1.0f));
}

TEST(SpellScriptHooks_RegisterRanksPutsOneBodyUnderEveryId)
{
    uint32 const ranks[] = { 11119, 11120, 12846 };
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterRanks<OnProcHook>(registry, ranks, &TestProcBody), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(3));
    for (uint32 id : ranks)
    {
        CHECK(registry.Find<OnProcHook>(id) == &TestProcBody);
    }

    // Taken keys keep their body; the count returned still names every id, so a boot's sum exceeds Count().
    CHECK_EQ(RegisterRanks<OnProcHook>(registry, ranks, &OtherProcBody), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(3));
    for (uint32 id : ranks)
    {
        CHECK(registry.Find<OnProcHook>(id) == &TestProcBody);
    }
}

TEST(SpellScriptHooks_RegisterScriptPutsOneBodyUnderOneId)
{
    SpellHandlerRegistry registry;
    CHECK(RegisterScript<OnProcHook>(registry, 11119, &TestProcBody));
    CHECK_EQ(registry.Count(), std::size_t(1));
    CHECK(registry.Find<OnProcHook>(11119) == &TestProcBody);

    // A taken key keeps its body, and a NULL body is refused.
    CHECK(!RegisterScript<OnProcHook>(registry, 11119, &OtherProcBody));
    CHECK(!RegisterScript<OnProcHook>(registry, 11120, NULL));
    CHECK_EQ(registry.Count(), std::size_t(1));
    CHECK(registry.Find<OnProcHook>(11119) == &TestProcBody);
}

TEST(SpellScriptHooks_CopyAuraHolderCopiesNothingFromAUnitThatHoldsNone)
{
    // A unit that holds no holder of the spell from the caster gives nothing to copy: false, and the other unit is left
    // without a holder.
    BareCreature from;
    BareCreature to;
    ObjectGuid const mage(HIGHGUID_UNIT, uint32(2961), uint32(400378));
    CHECK(!CopyAuraHolder(from, to, kStoredHolderSpell, mage));
    CHECK(to.GetSpellAuraHolder(kStoredHolderSpell, mage) == NULL);
    CHECK(to.GetSpellAuraHolderMap().empty());
}

TEST(SpellScriptHooks_CopyAuraHolderKeepsTheSourcesPeriodTickTimeAndAmount)
{
    // A DoT of 3000 ms for 12 s in the client's data, ticking every 2400 ms on the source as a hasted caster's would,
    // 1000 ms from its next tick with 7000 ms left. The bare creatures' guids are all empty, so the source's caster is
    // the unit it is on.
    static StoredDot rows;
    SpellEntry const* dot = StoreDot(rows, kCopiedDotSpell, 3000, 12000, false);
    BareCreature from;
    BareCreature to;
    ObjectGuid const caster;
    SpellAuraHolder* source = DotHolder(dot, from, caster, 500);
    Aura* sourceAura = source->GetAuraByEffectIndex(EFFECT_INDEX_0);
    sourceAura->SetPeriodic(true);
    sourceAura->GetModifier()->periodictime = 2400;
    sourceAura->CarryTickTimer(1000);
    source->SetAuraDuration(7000);
    from.HoldAura(kCopiedDotSpell, *source);

    // The copy ticks at the source's period, not the data's, from the source's time to its next tick, lasts the time
    // the source has left as its full duration, so it deals the source's three ticks left, and holds its amount.
    CHECK(CopyAuraHolder(from, to, kCopiedDotSpell, caster));
    SpellAuraHolder* copy = to.GetSpellAuraHolder(kCopiedDotSpell, caster);
    CHECK(copy != NULL);
    if (copy)
    {
        Aura* copied = copy->GetAuraByEffectIndex(EFFECT_INDEX_0);
        CHECK(copied != NULL);
        if (copied)
        {
            CHECK_EQ(copied->GetModifier()->periodictime, uint32(2400));
            CHECK_EQ(copied->GetPeriodicTimer(), int32(1000));
            CHECK_EQ(copied->GetModifier()->m_amount, int32(500));
            CHECK_EQ(copied->GetAuraMaxTicks(), uint32(3));
        }
        CHECK_EQ(copy->GetAuraDuration(), int32(7000));
        CHECK_EQ(copy->GetAuraMaxDuration(), int32(7000));

        // A second copy finds the caster's holder already there and changes nothing.
        CHECK(!CopyAuraHolder(from, to, kCopiedDotSpell, caster));
        CHECK_EQ(to.GetSpellAuraHolderMap().size(), std::size_t(1));
        to.RemoveSpellAuraHolder(copy);
    }
    CHECK(to.GetSpellAuraHolderMap().empty());
    CHECK(from.GetSpellAuraHolder(kCopiedDotSpell, caster) == source);
    delete source;
}

TEST(SpellScriptHooks_TheGameTableHoldsTheMageScriptsAndNoOtherHookRow)
{
    // The check-proc hook holds Impact's two ranks', Clearcasting's, Arcane Blast's buff's, Arcane Potency's two
    // buffs', Fingers of Frost's three, Brain Freeze's three and Invocation's two ranks', the proc hook the mage's 24
    // proc scripts, the target hook Blink's, the orb's scan's, the ring's freeze's and Piercing Chill's chill's, the
    // damage-done hook the 22 spells Frostburn's class mask covers and Torment the Weak's three ranks, the crit-chance
    // hook the 26 spells Shatter's covers, the dummy aura hook Living Bomb's, the aura-apply hook Pyromaniac's ten (its
    // eight DoTs and two ranks), Arcane Missiles!'s, Arcane Missiles' and the seven forms of Polymorph's, the
    // aura-remove hook Hot Streak's buff's, Mage Ward's, Mana Shield's, Pyromaniac's ten, Arcane Missiles!'s and the
    // seven forms of Polymorph's and the ring's freeze's, the periodic-tick hook Ring of Frost's, the dummy-effect hook
    // Combustion's, Impact's spread, Flame Orb's and the orb's scan's, the after-cast hook Blast Wave's and Replenish
    // Mana's, the launch hook Ice Lance's, Deep Freeze's, Frostfire Bolt's and Fireball's, the cast-start hook
    // Frostbolt's, and the after-hit hook Frostbolt's, Cone of Cold's and Frostfire Bolt's (MageScriptsTest names their
    // ids); no other hook holds a row.
    uint32 const emptyKeys[] =
    {
        CheckCastHook::Key, EffectValueHook::Key, AuraAmountHook::Key,
    };
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 key : emptyKeys)
    {
        CHECK_EQ(game.CountAt(key), std::size_t(0));
    }
    CHECK_EQ(game.CountAt(CheckProcHook::Key), std::size_t(14));
    CHECK_EQ(game.CountAt(OnProcHook::Key), std::size_t(24));
    CHECK_EQ(game.CountAt(SelectTargetsHook::Key), std::size_t(4));
    CHECK_EQ(game.CountAt(DamageDoneHook::Key), std::size_t(25));
    CHECK_EQ(game.CountAt(CritChanceHook::Key), std::size_t(26));
    CHECK_EQ(game.CountAt(DummyAuraHook::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(AuraApplyHook::Key), std::size_t(19));
    CHECK_EQ(game.CountAt(AuraRemoveHook::Key), std::size_t(22));
    CHECK_EQ(game.CountAt(PeriodicTickHook::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(DummyEffectHook::Key), std::size_t(4));
    CHECK_EQ(game.CountAt(AfterCastHook::Key), std::size_t(2));
    CHECK_EQ(game.CountAt(LaunchHook::Key), std::size_t(4));
    CHECK_EQ(game.CountAt(CastStartHook::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(AfterHitHook::Key), std::size_t(3));
    CHECK_EQ(game.Count(), std::size_t(1375));
    CHECK_EQ(game.CountDefaults(), std::size_t(29));
}

namespace
{
    const uint32 kSnareSpell = 94220;
    const uint32 kOtherSnareSpell = 94221;
    const uint32 kRootSpell = 94222;
    const uint32 kDispelSpell = 94223;
    const uint32 kMountSpell = 94224;
    const uint32 kSnareCategories = 94220;
    const uint32 kRootCategories = 94222;
    const uint32 kMountCategories = 94224;

    // SpellCategories.dbc rows: { Category, DefenseType, DispelType, Mechanic, PreventionType, StartRecoveryCategory }.
    SpellCategoriesEntry s_snareCategories = { 0, 0, 0, MECHANIC_SNARE, 0, 0 };
    SpellCategoriesEntry s_rootCategories = { 0, 0, 0, MECHANIC_ROOT, 0, 0 };
    SpellCategoriesEntry s_mountCategories = { 0, 0, 0, MECHANIC_MOUNT, 0, 0 };

    SpellRowStorage s_snareRow = {};
    SpellRowStorage s_otherSnareRow = {};
    SpellRowStorage s_rootRow = {};
    SpellRowStorage s_dispelRow = {};
    SpellRowStorage s_mountRow = {};

    // A Spell.dbc row with only its id and its SpellCategories row (none for 0), put into the spell store once, beside
    // the three SpellCategories rows.
    SpellEntry const* StoredMechanicSpell(SpellRowStorage& storage, uint32 spellId, uint32 categoriesId)
    {
        SpellEntry* row = reinterpret_cast<SpellEntry*>(storage.bytes);
        if (row->ID != spellId)
        {
            sSpellCategoriesStore.SetEntry(kSnareCategories, &s_snareCategories);
            sSpellCategoriesStore.SetEntry(kRootCategories, &s_rootCategories);
            sSpellCategoriesStore.SetEntry(kMountCategories, &s_mountCategories);
            row->ID = spellId;
            row->CategoriesID = categoriesId;
            sSpellStore.SetEntry(spellId, row);
        }
        return row;
    }

    // A spell cast by `caster` whose dispel-mechanic effect reaches `target` with the value `value`, as
    // Spell::HandleEffects leaves its target and value before it runs the effect.
    class DispelCast : public Spell
    {
        public:
            DispelCast(Unit& caster, Unit& target, int32 value)
                : Spell(&caster, StoredMechanicSpell(s_dispelRow, kDispelSpell, 0), true)
            {
                unitTarget = &target;
                damage = value;
            }
    };

    // A holder of `spell` on `target`, made through its constructor with no aura, put in the target's holder map. Its
    // caster is the target, whose guid is empty, so its removal looks up no other unit.
    SpellAuraHolder* HeldHolder(SpellEntry const* spell, BareCreature& target)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(spell, &target, NULL, NULL, NULL);
        target.HoldAura(spell->ID, *holder);
        return holder;
    }

    // A dispel-mechanic effect row for `mechanic`.
    SpellEffectEntry MechanicDispel(Mechanics mechanic)
    {
        SpellEffectEntry effect = SpellEffectEntry();
        effect.SpellID = kDispelSpell;
        effect.Effect = SPELL_EFFECT_DISPEL_MECHANIC;
        effect.EffectMiscValue_0 = mechanic;
        return effect;
    }
}

TEST(SpellEffects_ADispelMechanicEffectRemovesAsManyHoldersAsItsValue)
{
    // Two snares and a root on the target, the snares first in its holder map, which orders holders by spell id.
    SpellEntry const* snare = StoredMechanicSpell(s_snareRow, kSnareSpell, kSnareCategories);
    SpellEntry const* otherSnare = StoredMechanicSpell(s_otherSnareRow, kOtherSnareSpell, kSnareCategories);
    SpellEntry const* root = StoredMechanicSpell(s_rootRow, kRootSpell, kRootCategories);
    BareCreature caster;
    BareCreature target;
    SpellEffectEntry const effect = MechanicDispel(MECHANIC_SNARE);
    SpellAuraHolder* firstSnare = HeldHolder(snare, target);
    SpellAuraHolder* secondSnare = HeldHolder(otherSnare, target);
    SpellAuraHolder* rooted = HeldHolder(root, target);
    CHECK(firstSnare->HasMechanic(MECHANIC_SNARE));
    CHECK(secondSnare->HasMechanic(MECHANIC_SNARE));
    CHECK(!rooted->HasMechanic(MECHANIC_SNARE));

    // A value of 0, Blazing Speed's snare effect without Molten Shields, removes none, and so does one below 0.
    for (int32 value : { 0, -1 })
    {
        DispelCast cast(caster, target, value);
        cast.EffectDispelMechanic(&effect);
        CHECK_EQ(target.GetSpellAuraHolderMap().size(), std::size_t(3));
    }

    // A value of 1, Tremor Totem's, removes the first snare and leaves the second.
    DispelCast one(caster, target, 1);
    one.EffectDispelMechanic(&effect);
    CHECK(target.GetSpellAuraHolder(kSnareSpell) == NULL);
    CHECK(target.GetSpellAuraHolder(kOtherSnareSpell) == secondSnare);
    CHECK(target.GetSpellAuraHolder(kRootSpell) == rooted);
    CHECK_EQ(target.GetSpellAuraHolderMap().size(), std::size_t(2));

    // A value of 100, Blazing Speed's with Molten Shields, removes every snare, and the root stays.
    firstSnare = HeldHolder(snare, target);
    CHECK(firstSnare->HasMechanic(MECHANIC_SNARE));
    DispelCast all(caster, target, 100);
    all.EffectDispelMechanic(&effect);
    CHECK(target.GetSpellAuraHolder(kSnareSpell) == NULL);
    CHECK(target.GetSpellAuraHolder(kOtherSnareSpell) == NULL);
    CHECK(target.GetSpellAuraHolder(kRootSpell) == rooted);
    CHECK_EQ(target.GetSpellAuraHolderMap().size(), std::size_t(1));

    // The value counts holders, not spells: of two holders of one snare spell, 1 removes one.
    firstSnare = HeldHolder(snare, target);
    secondSnare = HeldHolder(snare, target);
    CHECK(firstSnare != secondSnare);
    DispelCast oneOfTwo(caster, target, 1);
    oneOfTwo.EffectDispelMechanic(&effect);
    CHECK_EQ(target.GetSpellAuraHolderMap().size(), std::size_t(2));
    CHECK(target.GetSpellAuraHolder(kSnareSpell) != NULL);
    CHECK(target.GetSpellAuraHolder(kRootSpell) == rooted);

    // What is left leaves through the engine's own removal.
    while (SpellAuraHolder* left = target.GetSpellAuraHolder(kSnareSpell))
    {
        target.RemoveSpellAuraHolder(left);
    }
    target.RemoveSpellAuraHolder(rooted);
    CHECK(target.GetSpellAuraHolderMap().empty());
}

TEST(SpellEffects_AMountDispelRemovesEveryMountWhateverItsValue)
{
    // Two holders of a mount spell and a snare on the target. A mount dispel of value 0, the creature Polymorphs' and
    // the dismount spells', or of 1, Dazed's, removes both mounts; the snare is no mount and stays.
    SpellEntry const* mount = StoredMechanicSpell(s_mountRow, kMountSpell, kMountCategories);
    SpellEntry const* snare = StoredMechanicSpell(s_snareRow, kSnareSpell, kSnareCategories);
    BareCreature caster;
    BareCreature target;
    SpellEffectEntry const effect = MechanicDispel(MECHANIC_MOUNT);
    SpellAuraHolder* slowed = HeldHolder(snare, target);
    for (int32 value : { 0, 1 })
    {
        SpellAuraHolder* firstMount = HeldHolder(mount, target);
        SpellAuraHolder* secondMount = HeldHolder(mount, target);
        CHECK(firstMount->HasMechanic(MECHANIC_MOUNT));
        CHECK(secondMount->HasMechanic(MECHANIC_MOUNT));
        CHECK(!slowed->HasMechanic(MECHANIC_MOUNT));
        CHECK_EQ(target.GetSpellAuraHolderMap().size(), std::size_t(3));
        DispelCast cast(caster, target, value);
        cast.EffectDispelMechanic(&effect);
        CHECK(target.GetSpellAuraHolder(kMountSpell) == NULL);
        CHECK(target.GetSpellAuraHolder(kSnareSpell) == slowed);
        CHECK_EQ(target.GetSpellAuraHolderMap().size(), std::size_t(1));
    }

    // What is left leaves through the engine's own removal.
    target.RemoveSpellAuraHolder(slowed);
    CHECK(target.GetSpellAuraHolderMap().empty());
}

TEST(SpellScriptHooks_DamageTakenMultiplierIsTheProductOfTheVictimsPercentAurasForTheSpellsSchool)
{
    static StoredClassScript curseRows;
    static StoredClassScript fireRows;
    SpellEntry const* curse = StoreTakenAura(curseRows, 94311, 8, 0x7e);
    SpellEntry const* fireOnly = StoreTakenAura(fireRows, 94312, 10, SPELL_SCHOOL_MASK_FIRE);

    SpellRowStorage fireballStorage, frostboltStorage;
    SpellEntry const& fireball = AskedRow(fireballStorage, 94313, 0);
    const_cast<SpellEntry&>(fireball).SchoolMask = SPELL_SCHOOL_MASK_FIRE;
    SpellEntry const& frostbolt = AskedRow(frostboltStorage, 94314, 0);
    const_cast<SpellEntry&>(frostbolt).SchoolMask = SPELL_SCHOOL_MASK_FROST;

    BareCreature caster;
    TakenAuraVictim victim;

    // No aura: 1.0 for any school.
    CHECK(Near(Spells::DamageTakenMultiplier(victim, caster, fireball), 1.0f));

    // Curse of the Elements' +8% over the six schools: 1.08 for fire and for frost.
    victim.Hold(curse);
    CHECK(Near(Spells::DamageTakenMultiplier(victim, caster, fireball), 1.08f));
    CHECK(Near(Spells::DamageTakenMultiplier(victim, caster, frostbolt), 1.08f));

    // A fire-only +10% multiplies in for fire and leaves frost alone.
    victim.Hold(fireOnly);
    CHECK(Near(Spells::DamageTakenMultiplier(victim, caster, fireball), 1.08f * 1.10f));
    CHECK(Near(Spells::DamageTakenMultiplier(victim, caster, frostbolt), 1.08f));

    victim.Drop();
    CHECK(Near(Spells::DamageTakenMultiplier(victim, caster, fireball), 1.0f));
}

TEST(SpellScriptHooks_DamageTakenMultiplierTakesAreaAvoidanceForAnAreaRowOnly)
{
    static StoredClassScript avoidanceRows;
    static StoredClassScript areaRows;
    static StoredClassScript singleRows;
    SpellEntry const* avoidance = StoreTakenAura(avoidanceRows, 94315, -50, 0x7e, SPELL_AURA_MOD_AOE_DAMAGE_AVOIDANCE);

    // A fire row whose effect 0 targets every enemy in an area (Blast Wave's), and one that targets a single enemy.
    auto storeFireRow = [](StoredClassScript& rows, uint32 spellId, Targets target) -> SpellEntry const&
    {
        if (rows.spell.Entry().ID != spellId)
        {
            rows.spell.Entry().ID = spellId;
            rows.spell.Entry().SchoolMask = SPELL_SCHOOL_MASK_FIRE;
            rows.spell.effect.SpellID = spellId;
            rows.spell.effect.EffectIndex = EFFECT_INDEX_0;
            rows.spell.effect.ImplicitTarget_0 = target;
            sSpellStore.SetEntry(spellId, &rows.spell.Entry());
            AddSpellEffectEntry(&rows.spell.effect);
        }
        return rows.spell.Entry();
    };
    SpellEntry const& blastWave = storeFireRow(areaRows, 94316, TARGET_ALL_ENEMY_IN_AREA_INSTANT);
    SpellEntry const& ignite = storeFireRow(singleRows, 94317, TARGET_CHAIN_DAMAGE);

    BareCreature caster;
    TakenAuraVictim victim;
    victim.Hold(avoidance);

    CHECK(Near(Spells::DamageTakenMultiplier(victim, caster, blastWave), 0.5f));
    CHECK(Near(Spells::DamageTakenMultiplier(victim, caster, ignite), 1.0f));

    victim.Drop();
    CHECK(Near(Spells::DamageTakenMultiplier(victim, caster, blastWave), 1.0f));
}

TEST(SpellScriptHooks_AnAfterHitContextFindsNoEnemyAroundAUnitInNoWorldAndNeedsTheAuraItCastsThrough)
{
    BareCreature caster;
    BareCreature target;
    SpellEntry const& spell = HookSpell();
    AfterHitContext afterHit(caster, target, spell, 1200, true, 0, 0, false);

    // A unit in no world has no enemy around it: the list is emptied and nothing is added.
    std::list<Unit*> found;
    found.push_back(&caster);
    afterHit.EnemiesAround(target, 10.0f, found);
    CHECK(found.empty());

    // A cast through an aura the caster does not hold casts nothing and says so.
    CHECK(!afterHit.CastTriggeredByAura(target, kOtherSpell, kHookSpell, EFFECT_INDEX_0));
    CHECK(target.GetSpellAuraHolderMap().empty());
}

namespace
{
    // Notices that keep the spell misses they are told, in place of the session's.
    class MissNotices : public CombatNotices
    {
        public:
            void SpellMiss(Unit& caster, Unit& target, uint32 spellId, SpellMissInfo missInfo) override
            {
                ++misses;
                lastCaster = &caster;
                lastTarget = &target;
                lastSpell = spellId;
                lastMiss = missInfo;
            }

            uint32 misses = 0;
            Unit* lastCaster = NULL;
            Unit* lastTarget = NULL;
            uint32 lastSpell = 0;
            SpellMissInfo lastMiss = SPELL_MISS_NONE;
    };
}

TEST(SpellScriptHooks_AnAfterHitContextShowsImmuneAsTheEngineDoesAndCastsNothing)
{
    BareCreature caster;
    BareCreature target;
    SpellEntry const& spell = HookSpell();
    AfterHitContext afterHit(caster, target, spell, 1200, false, 0, 0, false);

    // The miss goes out as Unit::SendSpellMiss tells it: the caster's spell at the unit, immune; nothing is cast.
    MissNotices notices;
    CombatNotices* previous = CombatNotices::Install(&notices);
    afterHit.ShowImmune(target, kOtherSpell);
    CombatNotices::Install(previous);
    CHECK_EQ(notices.misses, uint32(1));
    CHECK(notices.lastCaster == &caster);
    CHECK(notices.lastTarget == &target);
    CHECK_EQ(notices.lastSpell, kOtherSpell);
    CHECK_EQ(uint32(notices.lastMiss), uint32(SPELL_MISS_IMMUNE));
    CHECK(target.GetSpellAuraHolderMap().empty());
}
