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
/// target and script word, the aura handle's reads, RegisterRanks, the holder's script words, the holder copy's answer
/// for a unit holding none and the period, tick time, duration and amount a copy keeps, the low-health multiplier a
/// script query answers from the caster's class-script auras, and the game table, whose only hook rows are the mage's
/// scripts.
///
/// The contexts are built on bare Creatures (update fields allocated; no map, template or AI) and a hand-made
/// Spell.dbc row that is not in the spell store. A bare creature is in no world, so each context's owner check
/// passes. The aura handle runs on a holder's storage that was never constructed, its fields written through the
/// holder's setters; Amount and DropCharge are not called there (the first reads an Aura, which this binary cannot
/// build, the second tells the clients). The script words of a constructed holder are read from a holder built, on a
/// spell put into the store as the registry's own holder test does, in storage whose bytes were all 0xA5. The periodic
/// damage query reads holders and auras made through their constructors, on spells put into the spell, effect and
/// duration stores, held in a bare creature's holder map; the copy is added through Unit::AddSpellAuraHolder to a bare
/// creature, which sends nothing out of a world, and removed through Unit::RemoveSpellAuraHolder.
/// The low-health multiplier reads class-script auras made through their constructors, on spells put into the spell,
/// effect and class options stores, held in a bare creature's per-type aura list.

#include "TestHarness.h"
#include "spells/SpellScriptHooks.h"
#include "spells/SpellScriptQueries.h"
#include "spells/auras/SpellAuras.h"
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

TEST(SpellScriptHooks_TheFourteenKeysAre100To113AboveEveryLegacySite)
{
    uint32 const keys[] =
    {
        CheckCastHook::Key, SelectTargetsHook::Key, EffectValueHook::Key, DummyEffectHook::Key, AfterHitHook::Key,
        AuraAmountHook::Key, AuraApplyHook::Key, AuraRemoveHook::Key, PeriodicTickHook::Key, CheckProcHook::Key,
        OnProcHook::Key, DamageDoneHook::Key, CritChanceHook::Key, DummyAuraHook::Key,
    };
    CHECK_EQ(sizeof(keys) / sizeof(keys[0]), std::size_t(14));
    for (std::size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
    {
        CHECK_EQ(keys[i], uint32(100 + i));
        // SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_DRUID is the last and highest legacy site.
        CHECK(keys[i] > uint32(SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_DRUID));
    }

    // Every key the game registers sits at a legacy site, below the first hook, but the mage's scripts at the target,
    // proc, damage-done, dummy aura, aura-apply, aura-remove and dummy-effect hooks.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    std::size_t belowHooks = 0;
    for (uint32 site = 0; site < uint32(SPELL_SCRIPT_HOOK_CHECK_CAST); ++site)
    {
        belowHooks += game.CountAt(site);
    }
    CHECK_EQ(belowHooks + game.CountAt(SelectTargetsHook::Key) + game.CountAt(OnProcHook::Key) + game.CountAt(DamageDoneHook::Key) +
             game.CountAt(DummyAuraHook::Key) + game.CountAt(AuraApplyHook::Key) + game.CountAt(AuraRemoveHook::Key) +
             game.CountAt(DummyEffectHook::Key),
             game.Count());
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
    CHECK(registry.Dispatch<SelectTargetsHook>(kHookSpell, selectTargets).IsReturn());
    CHECK_EQ(targets.size(), std::size_t(1));
    CHECK(targets.front() == &target);
    CHECK_EQ(s_seen.targetMode, uint32(22));
    CHECK_EQ(maxTargets, uint32(1));

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

    CHECK(registry.Register<DamageDoneHook>(kHookSpell, [](DamageDoneContext& ctx)
    {
        ctx.doneTotalMod *= 2.0f;
        return SpellHandlerOutcome<void>::Continue();
    }));
    float doneTotalMod = 1.5f;
    DamageDoneContext damageDone(caster, target, spell, doneTotalMod);
    CHECK(registry.Dispatch<DamageDoneHook>(kHookSpell, damageDone).IsContinue());
    CHECK(doneTotalMod == 3.0f);

    CHECK(registry.Register<CritChanceHook>(kHookSpell, [](CritChanceContext& ctx)
    {
        ctx.critChance += 50.0f;
        return SpellHandlerOutcome<void>::Continue();
    }));
    float critChance = 5.0f;
    CritChanceContext critChanceCtx(caster, target, spell, critChance);
    CHECK(registry.Dispatch<CritChanceHook>(kHookSpell, critChanceCtx).IsContinue());
    CHECK(critChance == 55.0f);
    CHECK(registry.Dispatch<CritChanceHook>(kOtherSpell, critChanceCtx).IsMiss());
    CHECK(critChance == 55.0f);
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
        CHECK_EQ(found[0].periodMs, uint32(2000));
        CHECK_EQ(found[0].remainingMs, int32(4000));
        CHECK_EQ(found[0].remainingTicks, uint32(2));
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
    // The proc hook holds the mage's 21 proc scripts, the target hook Blink's and the orb's scan's, the damage-done
    // hook Ice Lance's, the dummy aura hook Living Bomb's, the aura-apply hook Pyromaniac's nine (its seven DoTs and two
    // ranks), the aura-remove hook Hot Streak's buff's and Pyromaniac's nine, and the dummy-effect hook Combustion's,
    // Impact's spread, Flame Orb's and the orb's scan's (MageScriptsTest names their ids); no other hook holds a row.
    uint32 const emptyKeys[] =
    {
        CheckCastHook::Key, EffectValueHook::Key, AfterHitHook::Key, AuraAmountHook::Key,
        PeriodicTickHook::Key, CheckProcHook::Key, CritChanceHook::Key,
    };
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 key : emptyKeys)
    {
        CHECK_EQ(game.CountAt(key), std::size_t(0));
    }
    CHECK_EQ(game.CountAt(OnProcHook::Key), std::size_t(21));
    CHECK_EQ(game.CountAt(SelectTargetsHook::Key), std::size_t(2));
    CHECK_EQ(game.CountAt(DamageDoneHook::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(DummyAuraHook::Key), std::size_t(1));
    CHECK_EQ(game.CountAt(AuraApplyHook::Key), std::size_t(9));
    CHECK_EQ(game.CountAt(AuraRemoveHook::Key), std::size_t(10));
    CHECK_EQ(game.CountAt(DummyEffectHook::Key), std::size_t(4));
    CHECK_EQ(game.Count(), std::size_t(1277));
    CHECK_EQ(game.CountDefaults(), std::size_t(29));
}
