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

/// Tests of spells/classes/priest/InnerFocus.cpp: the rows the file registers, in the game's table too; which spells
/// Inner Focus's class masks take; its after-cast row, which takes Inner Focus off a caster whose finished cast it
/// took and leaves it for any other cast; Strength of Soul's immunities, rank and heals, its check-proc row and the
/// Weakened Soul its effect shortens; Train of Thought's cooldowns, cuts and chances, and its proc row, which shortens
/// the cooldown in the owner's store. The scripts run on bare Creatures (update fields allocated; no world, map,
/// template or AI). The spells are put into the stores with only the fields the scripts read, their records as the
/// client's data holds them, and the spell rows are taken out of the spell store again; a cast spell the scripts only
/// name is a Spell.dbc row that is not in the spell store, with its class options. The holders are made through their
/// constructor with no aura and put in a bare creature's holder map; a holder the engine takes off leaves through
/// Unit::RemoveAurasDueToSpell. Scenario 1207 records Inner Focus, its immunity, Weakened Soul and the cooldown in
/// play.

#include "TestHarness.h"
#include "spells/classes/priest/InnerFocus.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "spells/SpellCooldownMgr.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "ObjectGuid.h"

#include <cstddef>
#include <cstdint>
#include <ctime>

namespace
{
    uint32 const INNER_FOCUS = 89485;
    uint32 const INNER_FOCUS_TAKERS[] = { 2061, 101062, 2060, 596, 32546 };
    uint32 const STRENGTH_OF_SOUL_RANKS[] = { 89488, 89489 };
    uint32 const STRENGTH_OF_SOUL_IMMUNITIES[] = { 96266, 96267 };
    uint32 const STRENGTH_OF_SOUL_EFFECT = 89490;
    uint32 const WEAKENED_SOUL = 6788;
    uint32 const TRAIN_OF_THOUGHT_RANKS[] = { 92295, 92297 };
    uint32 const HEAL_SPELL = 2050;
    uint32 const GREATER_HEAL = 2060;
    uint32 const FLASH_HEAL = 2061;
    uint32 const FLASH_HEAL_INSTANT = 101062;
    uint32 const PRAYER_OF_HEALING = 596;
    uint32 const BINDING_HEAL = 32546;
    uint32 const SMITE = 585;
    uint32 const PENANCE = 47540;
    uint32 const PENANCE_HEAL_BOLT = 47750;
    uint32 const PRAYER_OF_MENDING_HEAL = 33110;
    uint32 const RENEW = 139;
    uint32 const POWER_WORD_SHIELD = 17;
    uint32 const HOLY_FIRE = 14914;

    // The SpellClassOptions rows the test's spells name, and the SpellAuraOptions row of Train of Thought's second
    // rank; no client data holds these ids.
    uint32 const kFirstOptions = 94750;
    uint32 const kTrainOfThoughtAuraOptions = 94770;

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

    struct HolderStorage
    {
        alignas(SpellAuraHolder) unsigned char bytes[sizeof(SpellAuraHolder)];
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

    // A holder's storage without its constructor, every byte 0: it has no aura and is only named by the contexts.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // A priest spell's family flags as the client's data holds them: the first two words, then the third.
    struct PriestSpell
    {
        uint32 spellId;
        uint32 maskA;
        uint32 maskB;
        uint32 maskC;
        bool taken;                         ///< whether Inner Focus's mods take it
    };

    PriestSpell const PRIEST_SPELLS[] =
    {
        { FLASH_HEAL, 0x00000800, 0, 0, true },
        { FLASH_HEAL_INSTANT, 0x00000800, 0, 0, true },
        { GREATER_HEAL, 0x00001000, 0, 0, true },
        { PRAYER_OF_HEALING, 0x00000200, 0, 0, true },
        { BINDING_HEAL, 0, 0x00000004, 0, true },
        { HEAL_SPELL, 0x00000400, 0, 0, false },
        { SMITE, 0x00000080, 0, 0, false },
        { PENANCE, 0, 0x00800000, 0, false },
        { PENANCE_HEAL_BOLT, 0, 0x00010000, 0, false },
        { RENEW, 0x00000040, 0, 0, false },
        { POWER_WORD_SHIELD, 0x00000001, 0, 0, false },
        { PRAYER_OF_MENDING_HEAL, 0, 0x00000020, 0, false },
        { HOLY_FIRE, 0x00100000, 0, 0, false },
    };
    std::size_t const PRIEST_SPELL_COUNT = sizeof(PRIEST_SPELLS) / sizeof(PRIEST_SPELLS[0]);

    // Inner Focus's records: its class options (the priest's family) and its two modifier effects, each on the class
    // mask the client's data gives both (A 0x00001A00, B 0x00000004).
    ServerSpell s_innerFocus;
    SpellClassOptionsEntry s_innerFocusOptions;
    SpellEffectEntry s_innerFocusEffects[2];

    // The priest spells of PRIEST_SPELLS, with their class options, and a mage spell on Flash Heal's flags.
    SpellRowStorage s_priestRows[PRIEST_SPELL_COUNT];
    SpellClassOptionsEntry s_priestOptions[PRIEST_SPELL_COUNT];
    SpellRowStorage s_mageRow;
    SpellClassOptionsEntry s_mageOptions;

    // Puts Inner Focus's options and effects into their stores and makes the rows of PRIEST_SPELLS and the mage
    // spell, once; then puts Inner Focus into the spell store.
    void StoreInnerFocus()
    {
        static bool stored = false;
        if (!stored)
        {
            stored = true;
            s_innerFocusOptions = SpellClassOptionsEntry();
            s_innerFocusOptions.SpellClassSet = SPELLFAMILY_PRIEST;
            s_innerFocusOptions.SpellClassMask = ClassFamilyMask(uint64(0x40000000) << 32, 0);
            sSpellClassOptionsStore.SetEntry(kFirstOptions, &s_innerFocusOptions);
            SpellModOp const ops[2] = { SPELLMOD_COST, SPELLMOD_CRITICAL_CHANCE };
            uint32 const auras[2] = { SPELL_AURA_ADD_PCT_MODIFIER, SPELL_AURA_ADD_FLAT_MODIFIER };
            int32 const points[2] = { -100, 25 };
            for (uint32 i = 0; i < 2; ++i)
            {
                SpellEffectEntry& effect = s_innerFocusEffects[i];
                effect = SpellEffectEntry();
                effect.SpellID = INNER_FOCUS;
                effect.EffectIndex = i;
                effect.Effect = SPELL_EFFECT_APPLY_AURA;
                effect.EffectAura = auras[i];
                effect.EffectBasePoints = points[i];
                effect.EffectMiscValue_0 = ops[i];
                effect.ImplicitTarget_0 = TARGET_SELF;
                effect.EffectSpellClassMask = ClassFamilyMask(0x00001A00 | (uint64(0x00000004) << 32), 0);
                AddSpellEffectEntry(&effect);
            }
            for (std::size_t i = 0; i < PRIEST_SPELL_COUNT; ++i)
            {
                PriestSpell const& spell = PRIEST_SPELLS[i];
                uint32 const optionsId = kFirstOptions + 1 + uint32(i);
                s_priestOptions[i] = SpellClassOptionsEntry();
                s_priestOptions[i].SpellClassSet = SPELLFAMILY_PRIEST;
                s_priestOptions[i].SpellClassMask = ClassFamilyMask(spell.maskA | (uint64(spell.maskB) << 32),
                                                                    spell.maskC);
                sSpellClassOptionsStore.SetEntry(optionsId, &s_priestOptions[i]);
                SpellRow(s_priestRows[i], spell.spellId);
                reinterpret_cast<SpellEntry*>(s_priestRows[i].bytes)->ClassOptionsID = optionsId;
            }
            uint32 const mageOptionsId = kFirstOptions + 1 + uint32(PRIEST_SPELL_COUNT);
            s_mageOptions = SpellClassOptionsEntry();
            s_mageOptions.SpellClassSet = SPELLFAMILY_MAGE;
            s_mageOptions.SpellClassMask = ClassFamilyMask(0x00000800, 0);
            sSpellClassOptionsStore.SetEntry(mageOptionsId, &s_mageOptions);
            SpellRow(s_mageRow, 94771);
            reinterpret_cast<SpellEntry*>(s_mageRow.bytes)->ClassOptionsID = mageOptionsId;
        }
        s_innerFocus.Entry().ID = INNER_FOCUS;
        s_innerFocus.Entry().ClassOptionsID = kFirstOptions;
        sSpellStore.SetEntry(INNER_FOCUS, &s_innerFocus.Entry());
    }

    // Takes Inner Focus out of the spell store again.
    void ForgetInnerFocus()
    {
        sSpellStore.SetEntry(INNER_FOCUS, NULL);
    }

    // The row of PRIEST_SPELLS' spell `spellId`, with its class options; StoreInnerFocus makes them.
    SpellEntry const& PriestRow(uint32 spellId)
    {
        for (std::size_t i = 0; i < PRIEST_SPELL_COUNT; ++i)
        {
            if (PRIEST_SPELLS[i].spellId == spellId)
            {
                return *reinterpret_cast<SpellEntry const*>(s_priestRows[i].bytes);
            }
        }
        return *reinterpret_cast<SpellEntry const*>(s_mageRow.bytes);
    }

    // A holder of `spellId`, from the store, that `target` cast on itself, put in its holder map.
    SpellAuraHolder* OwnHolder(uint32 spellId, BareCreature& target)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(sSpellStore.LookupEntry(spellId), &target, NULL, NULL, NULL);
        target.HoldAura(spellId, *holder);
        return holder;
    }

    // The end the store holds for `spellId`; 0 when it holds none.
    time_t StoredEnd(SpellCooldownMgr const& cooldowns, uint32 spellId)
    {
        SpellCooldowns::const_iterator itr = cooldowns.GetSpellCooldownMap().find(spellId);
        return itr == cooldowns.GetSpellCooldownMap().end() ? 0 : itr->second.end;
    }

    // The event flags of a helpful spell's hit and of a harmful spell's hit.
    uint32 const HELPFUL_HIT = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL;
    uint32 const HARMFUL_HIT = PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT;
}

TEST(PriestInnerFocus_TheFileHoldsItsElevenRowsInTheGame)
{
    // Eleven rows: the five spells Inner Focus takes at the after-cast hook, Inner Focus at the aura-apply hook,
    // Strength of Soul's ranks at the check-proc hook, its effect at the dummy-effect hook and Train of Thought's
    // ranks at the proc hook; no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterInnerFocus(registry), uint32(11));
    CHECK_EQ(registry.Count(), std::size_t(11));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(AfterCastHook::Key), std::size_t(5));
    CHECK_EQ(registry.CountAt(AuraApplyHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(CheckProcHook::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(DummyEffectHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(2));
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : INNER_FOCUS_TAKERS)
    {
        CHECK(registry.Find<AfterCastHook>(spellId) != NULL);
        CHECK(game.Find<AfterCastHook>(spellId) == registry.Find<AfterCastHook>(spellId));
    }
    CHECK(registry.Find<AuraApplyHook>(INNER_FOCUS) != NULL);
    CHECK(game.Find<AuraApplyHook>(INNER_FOCUS) == registry.Find<AuraApplyHook>(INNER_FOCUS));
    for (uint32 rank : STRENGTH_OF_SOUL_RANKS)
    {
        CHECK(registry.Find<CheckProcHook>(rank) != NULL);
        CHECK(game.Find<CheckProcHook>(rank) == registry.Find<CheckProcHook>(rank));
        CHECK(game.Find<OnProcHook>(rank) == NULL);
    }
    SpellHandler<DummyEffectHook>::Function const effect = registry.Find<DummyEffectHook>(STRENGTH_OF_SOUL_EFFECT);
    CHECK(effect != NULL);
    CHECK(game.Find<DummyEffectHook>(STRENGTH_OF_SOUL_EFFECT) == effect);
    for (uint32 rank : TRAIN_OF_THOUGHT_RANKS)
    {
        CHECK(registry.Find<OnProcHook>(rank) != NULL);
        CHECK(game.Find<OnProcHook>(rank) == registry.Find<OnProcHook>(rank));
    }

    // Heal, Smite and Penance hold no after-cast row of this file; the immunities and Weakened Soul hold none at all.
    for (uint32 spellId : { HEAL_SPELL, SMITE, PENANCE })
    {
        CHECK(registry.Find<AfterCastHook>(spellId) == NULL);
    }
    for (uint32 spellId : { STRENGTH_OF_SOUL_IMMUNITIES[0], STRENGTH_OF_SOUL_IMMUNITIES[1], WEAKENED_SOUL })
    {
        CHECK(registry.Find<AuraApplyHook>(spellId) == NULL);
        CHECK(registry.Find<DummyEffectHook>(spellId) == NULL);
        CHECK(registry.Find<AfterCastHook>(spellId) == NULL);
    }
}

TEST(PriestInnerFocus_InnerFocusTakesTheSpellsItsClassMasksCover)
{
    // With Inner Focus out of the store, its mods take nothing.
    StoreInnerFocus();
    ForgetInnerFocus();
    for (PriestSpell const& spell : PRIEST_SPELLS)
    {
        CHECK(!Priest::InnerFocusTakes(PriestRow(spell.spellId)));
    }

    // With it in: Flash Heal, its instant cast, Greater Heal, Prayer of Healing and Binding Heal, through the first
    // word of the mask or the second; not Heal, nor the priest's other spells, nor a mage spell on Flash Heal's flags.
    StoreInnerFocus();
    for (PriestSpell const& spell : PRIEST_SPELLS)
    {
        CHECK_EQ(Priest::InnerFocusTakes(PriestRow(spell.spellId)), spell.taken);
    }
    CHECK(!Priest::InnerFocusTakes(*reinterpret_cast<SpellEntry const*>(s_mageRow.bytes)));

    // Every spell of an after-cast row is one Inner Focus takes.
    for (uint32 spellId : INNER_FOCUS_TAKERS)
    {
        CHECK(Priest::InnerFocusTakes(PriestRow(spellId)));
    }
    ForgetInnerFocus();
    CHECK(sSpellStore.LookupEntry(INNER_FOCUS) == NULL);
}

TEST(PriestInnerFocus_AFinishedCastOfASpellItTakesTakesInnerFocusOffItsCaster)
{
    SpellHandlerRegistry registry;
    Priest::RegisterInnerFocus(registry);
    StoreInnerFocus();
    {
        BareCreature priest;

        // A caster without Inner Focus: his Flash Heal takes nothing and the steps after the hook run.
        AfterCastContext bare(priest, PriestRow(FLASH_HEAL), 0, &priest, false, 0.0f, 0.0f, 0.0f, 0);
        CHECK(registry.Dispatch<AfterCastHook>(FLASH_HEAL, bare).IsContinue());
        CHECK(!priest.HasAura(INNER_FOCUS));

        // With Inner Focus: a finished Heal, which its mods do not take, leaves it on him.
        OwnHolder(INNER_FOCUS, priest);
        CHECK(priest.HasAura(INNER_FOCUS));
        AfterCastContext heal(priest, PriestRow(HEAL_SPELL), 0, &priest, false, 0.0f, 0.0f, 0.0f, 0);
        CHECK(registry.Dispatch<AfterCastHook>(FLASH_HEAL, heal).IsContinue());
        CHECK(priest.HasAura(INNER_FOCUS));

        // A finished Flash Heal takes it off him.
        AfterCastContext flash(priest, PriestRow(FLASH_HEAL), 0, &priest, false, 0.0f, 0.0f, 0.0f, 0);
        CHECK(registry.Dispatch<AfterCastHook>(FLASH_HEAL, flash).IsContinue());
        CHECK(!priest.HasAura(INNER_FOCUS));

        // So does a finished Binding Heal, through the mask's second word.
        OwnHolder(INNER_FOCUS, priest);
        AfterCastContext binding(priest, PriestRow(BINDING_HEAL), 1, NULL, false, 0.0f, 0.0f, 0.0f, 0);
        CHECK(registry.Dispatch<AfterCastHook>(BINDING_HEAL, binding).IsContinue());
        CHECK(!priest.HasAura(INNER_FOCUS));
        CHECK(priest.GetSpellAuraHolderMap().empty());
    }
    ForgetInnerFocus();
}

TEST(PriestInnerFocus_EachRankOfStrengthOfSoulGivesItsOwnImmunity)
{
    CHECK_EQ(Priest::StrengthOfSoulImmunity(89488), uint32(96266));
    CHECK_EQ(Priest::StrengthOfSoulImmunity(89489), uint32(96267));
    for (uint32 spellId : { STRENGTH_OF_SOUL_EFFECT, INNER_FOCUS, 96266u, 96267u, 0u })
    {
        CHECK_EQ(Priest::StrengthOfSoulImmunity(spellId), uint32(0));
    }

    // Inner Focus's apply on a priest with no rank casts nothing, at the call before the stacking checks or after;
    // the holder's boosts run after it.
    SpellHandlerRegistry registry;
    Priest::RegisterInnerFocus(registry);
    BareCreature priest;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellRowStorage focusRow;
    CHECK_EQ(Priest::StrengthOfSoulRank(priest), uint32(0));
    for (bool real : { false, true })
    {
        AuraApplyContext ctx(priest, &priest, SpellRow(focusRow, INNER_FOCUS), AuraHandle(holder), real);
        CHECK(registry.Dispatch<AuraApplyHook>(INNER_FOCUS, ctx).IsContinue());
    }
    CHECK(priest.GetSpellAuraHolderMap().empty());
}

TEST(PriestInnerFocus_OnlyHealGreaterHealAndFlashHealTriggerStrengthOfSoul)
{
    for (uint32 spellId : { HEAL_SPELL, GREATER_HEAL, FLASH_HEAL, FLASH_HEAL_INSTANT })
    {
        CHECK(Priest::StrengthOfSoulFrom(spellId));
    }
    for (uint32 spellId : { PRAYER_OF_HEALING, BINDING_HEAL, PENANCE_HEAL_BOLT, PRAYER_OF_MENDING_HEAL, RENEW,
                            POWER_WORD_SHIELD, 56160u, SMITE, 0u })
    {
        CHECK(!Priest::StrengthOfSoulFrom(spellId));
    }

    // The check-proc row: those three heals go on to the engine's checks; every other spell, and an event with no
    // spell, is dropped from the proc.
    SpellHandlerRegistry registry;
    Priest::RegisterInnerFocus(registry);
    BareCreature priest;
    BareCreature healed;
    HolderStorage storage;
    SpellAuraHolder& holder = EmptyHolder(storage);
    SpellRowStorage talentRow, spellRow;
    for (uint32 rank : STRENGTH_OF_SOUL_RANKS)
    {
        SpellEntry const& talent = SpellRow(talentRow, rank);
        for (uint32 spellId : { HEAL_SPELL, GREATER_HEAL, FLASH_HEAL, FLASH_HEAL_INSTANT })
        {
            SpellEntry const& spell = SpellRow(spellRow, spellId);
            CheckProcContext ctx(priest, &healed, talent, AuraHandle(holder), &spell, HELPFUL_HIT,
                                 PROC_EX_NORMAL_HIT, 9000);
            CHECK(registry.Dispatch<CheckProcHook>(rank, ctx).IsContinue());
        }
        for (uint32 spellId : { PRAYER_OF_HEALING, BINDING_HEAL, PENANCE_HEAL_BOLT, PRAYER_OF_MENDING_HEAL, 56160u })
        {
            SpellEntry const& spell = SpellRow(spellRow, spellId);
            CheckProcContext ctx(priest, &healed, talent, AuraHandle(holder), &spell, HELPFUL_HIT,
                                 PROC_EX_CRITICAL_HIT, 9000);
            SpellHandlerOutcome<bool> outcome = registry.Dispatch<CheckProcHook>(rank, ctx);
            CHECK(outcome.IsReturn());
            CHECK(!outcome.GetValue());
        }
        CheckProcContext none(priest, &healed, talent, AuraHandle(holder), NULL, HELPFUL_HIT, PROC_EX_NORMAL_HIT, 900);
        SpellHandlerOutcome<bool> dropped = registry.Dispatch<CheckProcHook>(rank, none);
        CHECK(dropped.IsReturn());
        CHECK(!dropped.GetValue());
    }
}

namespace
{
    // The two ranks of Strength of Soul, each with its effect 0 value as the client's data holds it (2, 4), and
    // Weakened Soul with only its id.
    ServerSpell s_soulRanks[2];
    ServerSpell s_weakenedSoul;

    // Puts the two ranks and Weakened Soul into the spell store; the ranks' effects once.
    void StoreSoulRows()
    {
        static bool stored = false;
        int32 const seconds[2] = { 2, 4 };
        for (uint32 i = 0; i < 2; ++i)
        {
            if (!stored)
            {
                s_soulRanks[i].effect = SpellEffectEntry();
                s_soulRanks[i].effect.SpellID = STRENGTH_OF_SOUL_RANKS[i];
                s_soulRanks[i].effect.EffectIndex = EFFECT_INDEX_0;
                s_soulRanks[i].effect.EffectBasePoints = seconds[i];
                AddSpellEffectEntry(&s_soulRanks[i].effect);
            }
            s_soulRanks[i].Entry().ID = STRENGTH_OF_SOUL_RANKS[i];
            sSpellStore.SetEntry(STRENGTH_OF_SOUL_RANKS[i], &s_soulRanks[i].Entry());
        }
        stored = true;
        s_weakenedSoul.Entry().ID = WEAKENED_SOUL;
        sSpellStore.SetEntry(WEAKENED_SOUL, &s_weakenedSoul.Entry());
    }

    // Takes them out of the spell store again.
    void ForgetSoulRows()
    {
        for (uint32 spellId : { STRENGTH_OF_SOUL_RANKS[0], STRENGTH_OF_SOUL_RANKS[1], WEAKENED_SOUL })
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }

    // Strength of Soul's effect at `healed` by `priest`, effect `eff`.
    SpellHandlerOutcome<void> SoulEffect(SpellHandlerRegistry const& registry, BareCreature& priest,
                                         BareCreature* healed, SpellEffectIndex eff)
    {
        SpellRowStorage effectRow;
        int32 value = 1;
        int32 word = 0;
        DummyEffectContext ctx(priest, healed, NULL, NULL, SpellRow(effectRow, STRENGTH_OF_SOUL_EFFECT), eff, value,
                               ObjectGuid(), healed, word);
        return registry.Dispatch<DummyEffectHook>(STRENGTH_OF_SOUL_EFFECT, ctx);
    }
}

TEST(PriestInnerFocus_StrengthOfSoulShortensWeakenedSoulByTheRanksSeconds)
{
    CHECK_EQ(Priest::WeakenedSoulCut(2), int32(2000));
    CHECK_EQ(Priest::WeakenedSoulCut(4), int32(4000));
    CHECK_EQ(Priest::WeakenedSoulCut(0), int32(0));
    CHECK_EQ(Priest::WeakenedSoulCut(-4), int32(0));
    CHECK_EQ(Priest::WeakenedSoulCut(INT32_MAX), int32(INT32_MAX));

    SpellHandlerRegistry registry;
    Priest::RegisterInnerFocus(registry);
    StoreSoulRows();
    {
        BareCreature priest;
        BareCreature healed;
        SpellAuraHolder* soul = OwnHolder(WEAKENED_SOUL, healed);
        soul->SetAuraDuration(12000);

        // A priest with no rank shortens nothing.
        CHECK_EQ(Priest::StrengthOfSoulRank(priest), uint32(0));
        CHECK(SoulEffect(registry, priest, &healed, EFFECT_INDEX_0).IsContinue());
        CHECK_EQ(soul->GetAuraDuration(), int32(12000));

        // The first rank takes its 2 seconds off; the second, once he holds it too, its 4.
        OwnHolder(STRENGTH_OF_SOUL_RANKS[0], priest);
        CHECK_EQ(Priest::StrengthOfSoulRank(priest), STRENGTH_OF_SOUL_RANKS[0]);
        CHECK(SoulEffect(registry, priest, &healed, EFFECT_INDEX_0).IsContinue());
        CHECK_EQ(soul->GetAuraDuration(), int32(10000));
        OwnHolder(STRENGTH_OF_SOUL_RANKS[1], priest);
        CHECK_EQ(Priest::StrengthOfSoulRank(priest), STRENGTH_OF_SOUL_RANKS[1]);
        CHECK(SoulEffect(registry, priest, &healed, EFFECT_INDEX_0).IsContinue());
        CHECK_EQ(soul->GetAuraDuration(), int32(6000));

        // Another effect index, and an effect with no unit, take nothing.
        CHECK(SoulEffect(registry, priest, &healed, EFFECT_INDEX_1).IsContinue());
        CHECK(SoulEffect(registry, priest, NULL, EFFECT_INDEX_0).IsContinue());
        CHECK_EQ(soul->GetAuraDuration(), int32(6000));

        // Then 2 s are left, and next less than the cut: the time runs out.
        CHECK(SoulEffect(registry, priest, &healed, EFFECT_INDEX_0).IsContinue());
        CHECK_EQ(soul->GetAuraDuration(), int32(2000));
        CHECK(SoulEffect(registry, priest, &healed, EFFECT_INDEX_0).IsContinue());
        CHECK_EQ(soul->GetAuraDuration(), int32(0));

        // A unit with no Weakened Soul: nothing to shorten.
        BareCreature unmarked;
        CHECK(SoulEffect(registry, priest, &unmarked, EFFECT_INDEX_0).IsContinue());
        CHECK(unmarked.GetSpellAuraHolderMap().empty());
    }
    ForgetSoulRows();
}

TEST(PriestInnerFocus_TrainOfThoughtNamesTheCooldownTheCutAndTheChance)
{
    CHECK_EQ(Priest::TrainOfThoughtCooldown(GREATER_HEAL), INNER_FOCUS);
    CHECK_EQ(Priest::TrainOfThoughtCooldown(SMITE), PENANCE);
    for (uint32 spellId : { FLASH_HEAL, HEAL_SPELL, PRAYER_OF_HEALING, HOLY_FIRE, 47666u, PENANCE, 0u })
    {
        CHECK_EQ(Priest::TrainOfThoughtCooldown(spellId), uint32(0));
    }

    // Inner Focus's cut is effect 0's seconds, Penance's effect 1's milliseconds; nothing for another cooldown or a
    // value of 0 or less.
    CHECK_EQ(Priest::TrainOfThoughtCut(INNER_FOCUS, 5, 500), int32(5000));
    CHECK_EQ(Priest::TrainOfThoughtCut(PENANCE, 5, 500), int32(500));
    CHECK_EQ(Priest::TrainOfThoughtCut(GREATER_HEAL, 5, 500), int32(0));
    CHECK_EQ(Priest::TrainOfThoughtCut(INNER_FOCUS, 0, 500), int32(0));
    CHECK_EQ(Priest::TrainOfThoughtCut(PENANCE, 5, -500), int32(0));
    CHECK_EQ(Priest::TrainOfThoughtCut(INNER_FOCUS, INT32_MAX, 500), int32(INT32_MAX));

    // The first rank's chance is its effect 2 value, the second's its proc chance.
    CHECK_EQ(Priest::TrainOfThoughtChance(92295, 50, 100), int32(50));
    CHECK_EQ(Priest::TrainOfThoughtChance(92297, 75, 100), int32(100));
    CHECK_EQ(Priest::TrainOfThoughtChance(INNER_FOCUS, 50, 100), int32(0));
}

namespace
{
    // The first rank with its three effect values as the client's data holds them (5, 500, 50), and the second rank
    // with its SpellAuraOptions row (a proc chance of 100).
    ServerSpell s_firstThought;
    ServerSpell s_secondThought;
    SpellEffectEntry s_firstThoughtEffects[3];

    // Puts both ranks into the spell store; the effects and the options once.
    void StoreThoughtRows()
    {
        static bool stored = false;
        if (!stored)
        {
            stored = true;
            int32 const values[3] = { 5, 500, 50 };
            for (uint32 i = 0; i < 3; ++i)
            {
                s_firstThoughtEffects[i] = SpellEffectEntry();
                s_firstThoughtEffects[i].SpellID = TRAIN_OF_THOUGHT_RANKS[0];
                s_firstThoughtEffects[i].EffectIndex = i;
                s_firstThoughtEffects[i].EffectBasePoints = values[i];
                AddSpellEffectEntry(&s_firstThoughtEffects[i]);
            }
            s_secondThought.auraOptions = SpellAuraOptionsEntry();
            s_secondThought.auraOptions.ProcChance = 100;
            sSpellAuraOptionsStore.SetEntry(kTrainOfThoughtAuraOptions, &s_secondThought.auraOptions);
        }
        s_firstThought.Entry().ID = TRAIN_OF_THOUGHT_RANKS[0];
        s_secondThought.Entry().ID = TRAIN_OF_THOUGHT_RANKS[1];
        s_secondThought.Entry().AuraOptionsID = kTrainOfThoughtAuraOptions;
        sSpellStore.SetEntry(TRAIN_OF_THOUGHT_RANKS[0], &s_firstThought.Entry());
        sSpellStore.SetEntry(TRAIN_OF_THOUGHT_RANKS[1], &s_secondThought.Entry());
    }

    // Takes both ranks out of the spell store again.
    void ForgetThoughtRows()
    {
        for (uint32 rank : TRAIN_OF_THOUGHT_RANKS)
        {
            sSpellStore.SetEntry(rank, NULL);
        }
    }

    // Train of Thought's proc, of `rank`'s effect `eff`, for a hit of `procSpell`.
    SpellAuraProcResult ThoughtProc(SpellHandlerRegistry const& registry, SpellCooldownMgr& cooldowns, uint32 rank,
                                    SpellEffectIndex eff, uint32 procSpell, uint32 procFlag)
    {
        BareCreature priest;
        BareCreature other;
        HolderStorage storage;
        SpellAuraHolder& holder = EmptyHolder(storage);
        SpellRowStorage spellRow;
        SpellEntry const& spell = SpellRow(spellRow, procSpell);
        OnProcContext ctx(priest, &other, *sSpellStore.LookupEntry(rank), eff, AuraHandle(holder), &spell, procFlag,
                          PROC_EX_NORMAL_HIT, 9000, 5, ObjectGuid(), 0, NULL, cooldowns);
        SpellHandlerOutcome<SpellAuraProcResult> outcome = registry.Dispatch<OnProcHook>(rank, ctx);
        CHECK(outcome.IsReturn());
        return outcome.GetValue();
    }
}

TEST(PriestInnerFocus_TrainOfThoughtShortensTheCooldownInTheOwnersStore)
{
    SpellHandlerRegistry registry;
    Priest::RegisterInnerFocus(registry);
    StoreThoughtRows();
    CHECK_EQ(sSpellStore.LookupEntry(TRAIN_OF_THOUGHT_RANKS[1])->GetProcChance(), uint32(100));
    uint32 const secondRank = TRAIN_OF_THOUGHT_RANKS[1];

    // Inner Focus's cooldown 45 s from now, Penance's 12 s.
    SpellCooldownMgr cooldowns;
    time_t const focusEnd = time(NULL) + 45;
    time_t const penanceEnd = time(NULL) + 12;
    cooldowns.AddSpellCooldown(INNER_FOCUS, 0, focusEnd);
    cooldowns.AddSpellCooldown(PENANCE, 0, penanceEnd);

    // The second rank, at its 100%: a Greater Heal's hit takes 5 s off Inner Focus's, a Smite's 500 ms off Penance's,
    // a whole second in the store.
    CHECK_EQ(int(ThoughtProc(registry, cooldowns, secondRank, EFFECT_INDEX_0, GREATER_HEAL, HELPFUL_HIT)),
             int(SPELL_AURA_PROC_OK));
    CHECK_EQ(int64(StoredEnd(cooldowns, INNER_FOCUS)), int64(focusEnd - 5));
    CHECK_EQ(int64(StoredEnd(cooldowns, PENANCE)), int64(penanceEnd));
    CHECK_EQ(int(ThoughtProc(registry, cooldowns, secondRank, EFFECT_INDEX_0, SMITE, HARMFUL_HIT)),
             int(SPELL_AURA_PROC_OK));
    CHECK_EQ(int64(StoredEnd(cooldowns, PENANCE)), int64(penanceEnd - 1));
    CHECK_EQ(int64(StoredEnd(cooldowns, INNER_FOCUS)), int64(focusEnd - 5));

    // Any other spell cannot trigger it; another effect fails; neither shortens anything. So does the first rank's
    // other effect, before its chance is drawn.
    for (uint32 spellId : { FLASH_HEAL, HEAL_SPELL, HOLY_FIRE, PENANCE_HEAL_BOLT })
    {
        CHECK_EQ(int(ThoughtProc(registry, cooldowns, secondRank, EFFECT_INDEX_0, spellId, HELPFUL_HIT)),
                 int(SPELL_AURA_PROC_CANT_TRIGGER));
    }
    CHECK_EQ(int(ThoughtProc(registry, cooldowns, secondRank, EFFECT_INDEX_1, GREATER_HEAL, HELPFUL_HIT)),
             int(SPELL_AURA_PROC_FAILED));
    CHECK_EQ(int(ThoughtProc(registry, cooldowns, TRAIN_OF_THOUGHT_RANKS[0], EFFECT_INDEX_1, SMITE, HARMFUL_HIT)),
             int(SPELL_AURA_PROC_FAILED));
    CHECK_EQ(int64(StoredEnd(cooldowns, INNER_FOCUS)), int64(focusEnd - 5));
    CHECK_EQ(int64(StoredEnd(cooldowns, PENANCE)), int64(penanceEnd - 1));

    // Inner Focus's cooldown held while its aura stands (the store's infinity mark) is not shortened.
    time_t const held = time(NULL) + time_t(SpellCooldownMgr::infinityCooldownDelay);
    cooldowns.AddSpellCooldown(INNER_FOCUS, 0, held);
    CHECK_EQ(int(ThoughtProc(registry, cooldowns, secondRank, EFFECT_INDEX_0, GREATER_HEAL, HELPFUL_HIT)),
             int(SPELL_AURA_PROC_OK));
    CHECK_EQ(int64(StoredEnd(cooldowns, INNER_FOCUS)), int64(held));

    // With no first rank in the store there is no cut: nothing shortened.
    sSpellStore.SetEntry(TRAIN_OF_THOUGHT_RANKS[0], NULL);
    CHECK_EQ(int(ThoughtProc(registry, cooldowns, secondRank, EFFECT_INDEX_0, SMITE, HARMFUL_HIT)),
             int(SPELL_AURA_PROC_OK));
    CHECK_EQ(int64(StoredEnd(cooldowns, PENANCE)), int64(penanceEnd - 1));
    ForgetThoughtRows();
}
