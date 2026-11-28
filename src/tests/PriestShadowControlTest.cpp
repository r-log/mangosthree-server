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

/// Tests of priest/shadow/PsychicScream.cpp, Silence.cpp and Phantasm.cpp: the three aura-apply rows the files
/// register, in the game's table too; Psychic Scream's root, which its row takes off a holder whose third effect holds
/// no amount above 0 and leaves on one that does, at the call before the holder is added and not after, and never
/// as the holder's last effect; the interrupt Silence gives a target that is no player; Phantasm's chance by rank, its
/// roll and the rank a priest holds, and Fade's row, which does nothing at the call before the add. The scripts run on
/// bare Creatures (update fields allocated; no world, map, template or AI); the spells whose effects a test reads are
/// rows of the test's own, put into the spell store by the test that reads them and taken out again. The cast of the
/// interrupt and the removal of the roots need a map: scenarios 1242 and 1243 record them in play, and 1244 records
/// the Shadowfiend that SpellTablesTest's Spells::SummonedPetAssists row sends at its target.

#include "TestHarness.h"
#include "spells/classes/priest/shadow/Phantasm.h"
#include "spells/classes/priest/shadow/PsychicScream.h"
#include "spells/classes/priest/shadow/Silence.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>

namespace
{
    uint32 const PSYCHIC_SCREAM = 8122;
    uint32 const GLYPH_OF_PSYCHIC_SCREAM = 55676;
    uint32 const SILENCE = 15487;
    uint32 const FADE = 586;
    uint32 const PHANTASM_RANK_1 = 47569;
    uint32 const PHANTASM_RANK_2 = 47570;

    /// A spell id no client data holds, whose three effect rows stand in for Psychic Scream's: the fear, the speed and
    /// the root, each a dummy aura here, so that a holder of it changes nothing on its unit.
    uint32 const TEST_SCREAM = 94490;

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

            // The type the unit reports, so that the Silence row sees a player's.
            void SetType(uint8 typeId)
            {
                m_objectTypeId = typeId;
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

    // A holder's storage without its constructor, every byte 0: it has no aura and only stands in the contexts.
    SpellAuraHolder& EmptyHolder(HolderStorage& storage)
    {
        for (unsigned char& byte : storage.bytes)
        {
            byte = 0;
        }
        return *reinterpret_cast<SpellAuraHolder*>(storage.bytes);
    }

    // A spell of the client's data as the scripts read it: its id and the base points of its three effects, which
    // each hold a dummy aura of that value.
    struct StoredSpell
    {
        ServerSpell row;
        SpellEffectEntry effects[MAX_EFFECT_INDEX];
        bool added;

        StoredSpell() : added(false) {}

        // Puts the spell `spellId` into the spell store, its effects added once.
        void Store(uint32 spellId, int32 points0, int32 points1, int32 points2)
        {
            int32 const points[MAX_EFFECT_INDEX] = { points0, points1, points2 };
            if (!added)
            {
                added = true;
                for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                {
                    effects[i] = SpellEffectEntry();
                    effects[i].SpellID = spellId;
                    effects[i].EffectIndex = SpellEffectIndex(i);
                    effects[i].Effect = SPELL_EFFECT_APPLY_AURA;
                    effects[i].EffectAura = SPELL_AURA_DUMMY;
                    effects[i].EffectBasePoints = points[i];
                    AddSpellEffectEntry(&effects[i]);
                }
            }
            row.Entry().ID = spellId;
            sSpellStore.SetEntry(spellId, &row.Entry());
        }

        // Takes the spell out of the spell store again.
        void Forget(uint32 spellId)
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    };

    StoredSpell s_scream;
    StoredSpell s_rankOne;
    StoredSpell s_rankTwo;

    // The client's two ranks of Phantasm: effect 0 of 50 and 100.
    void StoreRanks()
    {
        s_rankOne.Store(PHANTASM_RANK_1, 50, 0, 0);
        s_rankTwo.Store(PHANTASM_RANK_2, 100, 0, 0);
    }

    void ForgetRanks()
    {
        s_rankOne.Forget(PHANTASM_RANK_1);
        s_rankTwo.Forget(PHANTASM_RANK_2);
    }

    // Files a holder under `spellId` on `unit`: the zeroed storage stands for it, as HasAura reads the key alone.
    void Hold(BareCreature& unit, uint32 spellId, HolderStorage& storage)
    {
        unit.HoldAura(spellId, EmptyHolder(storage));
    }

    // A holder of the test's scream on `target`, made through its constructor with no caster, with an aura made
    // through CreateAura at each effect `mask` names, of the amounts Psychic Scream's data gives: 0 for the fear, 25 for
    // the speed, `root` for the third effect. The holder is not on its target.
    SpellAuraHolder* ScreamHolder(Unit& target, uint32 mask, int32 root)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(TEST_SCREAM);
        int32 const amounts[MAX_EFFECT_INDEX] = { 0, 25, root };
        SpellAuraHolder* holder = new SpellAuraHolder(spell, &target, NULL, NULL, NULL);
        for (int32 eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
        {
            if (mask & (1u << eff))
            {
                int32 points = amounts[eff];
                holder->AddAura(CreateAura(spell, SpellEffectIndex(eff), &points, holder, &target), SpellEffectIndex(eff));
            }
        }
        return holder;
    }

    // Which effects of `holder` hold an aura, as a mask of the effect indexes.
    uint32 EffectsHeld(SpellAuraHolder const& holder)
    {
        uint32 mask = 0;
        for (int32 eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
        {
            mask |= holder.GetAuraByEffectIndex(SpellEffectIndex(eff)) ? 1u << eff : 0u;
        }
        return mask;
    }
}

TEST(PriestShadowControl_TheThreeFilesHoldOneAuraApplyRowEachInTheGame)
{
    // Psychic Scream, Silence and Fade, each at the aura-apply hook: three rows, three keys, no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterPsychicScream(registry), uint32(1));
    CHECK_EQ(Priest::RegisterSilence(registry), uint32(1));
    CHECK_EQ(Priest::RegisterPhantasm(registry), uint32(1));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraApplyHook::Key), std::size_t(3));

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { PSYCHIC_SCREAM, SILENCE, FADE })
    {
        CHECK(registry.Find<AuraApplyHook>(spellId) != NULL);
        CHECK(game.Find<AuraApplyHook>(spellId) == registry.Find<AuraApplyHook>(spellId));

        // No other hook holds a row of the three.
        CHECK(registry.Find<AuraRemoveHook>(spellId) == NULL);
        CHECK(registry.Find<CheckProcHook>(spellId) == NULL);
        CHECK(registry.Find<OnProcHook>(spellId) == NULL);
        CHECK(registry.Find<AuraAmountHook>(spellId) == NULL);
    }
    CHECK(registry.Find<AuraApplyHook>(PSYCHIC_SCREAM) != registry.Find<AuraApplyHook>(SILENCE));
    CHECK(registry.Find<AuraApplyHook>(SILENCE) != registry.Find<AuraApplyHook>(FADE));

    // The glyph and the talent's own rows hold none: the glyph's modifier and Phantasm's dummy proc are the engine's.
    for (uint32 spellId : { GLYPH_OF_PSYCHIC_SCREAM, PHANTASM_RANK_1, PHANTASM_RANK_2 })
    {
        CHECK(registry.Find<AuraApplyHook>(spellId) == NULL);
        CHECK(game.Find<AuraApplyHook>(spellId) == NULL);
    }
}

TEST(PriestShadowControl_PsychicScreamsRootIsKeptForAnAmountAboveZeroOnly)
{
    // The client's value of the third effect is 0, and the glyph's flat addition of 1 is the only thing above it.
    CHECK(!Priest::PsychicScreamKeepsRoot(0));
    CHECK(Priest::PsychicScreamKeepsRoot(1));
    CHECK(Priest::PsychicScreamKeepsRoot(2));
    CHECK(Priest::PsychicScreamKeepsRoot(100));
    CHECK(!Priest::PsychicScreamKeepsRoot(-1));
    CHECK(!Priest::PsychicScreamKeepsRoot(-100));
}

TEST(PriestShadowControl_PsychicScreamsRowTakesTheRootOffAHolderWhoseThirdEffectHoldsNothing)
{
    SpellHandlerRegistry registry;
    Priest::RegisterPsychicScream(registry);
    s_scream.Store(TEST_SCREAM, 0, 25, 0);
    SpellRowStorage row;
    BareCreature target;
    BareCreature priest;

    // The call before the add, a priest without the glyph: the root, effect 2 of amount 0, is taken off the holder, and
    // the fear and the speed stay.
    SpellAuraHolder* plain = ScreamHolder(target, 0x7, 0);
    AuraApplyContext plainBefore(target, &priest, SpellRow(row, PSYCHIC_SCREAM), AuraHandle(*plain), false);
    CHECK(registry.Dispatch<AuraApplyHook>(PSYCHIC_SCREAM, plainBefore).IsContinue());
    CHECK_EQ(EffectsHeld(*plain), uint32(0x3));
    delete plain;

    // With the glyph the amount is 1 and the holder keeps all three; a larger amount keeps them too.
    for (int32 amount : { 1, 2 })
    {
        SpellAuraHolder* glyphed = ScreamHolder(target, 0x7, amount);
        AuraApplyContext glyphedBefore(target, &priest, SpellRow(row, PSYCHIC_SCREAM), AuraHandle(*glyphed), false);
        CHECK(registry.Dispatch<AuraApplyHook>(PSYCHIC_SCREAM, glyphedBefore).IsContinue());
        CHECK_EQ(EffectsHeld(*glyphed), uint32(0x7));
        delete glyphed;
    }

    // The call after the add does nothing, whatever the amount.
    SpellAuraHolder* added = ScreamHolder(target, 0x7, 0);
    AuraApplyContext plainAfter(target, &priest, SpellRow(row, PSYCHIC_SCREAM), AuraHandle(*added), true);
    CHECK(registry.Dispatch<AuraApplyHook>(PSYCHIC_SCREAM, plainAfter).IsContinue());
    CHECK_EQ(EffectsHeld(*added), uint32(0x7));
    delete added;

    // The row does not look at the caster: an apply with none loses the root all the same.
    SpellAuraHolder* orphan = ScreamHolder(target, 0x7, 0);
    AuraApplyContext noCaster(target, NULL, SpellRow(row, PSYCHIC_SCREAM), AuraHandle(*orphan), false);
    CHECK(registry.Dispatch<AuraApplyHook>(PSYCHIC_SCREAM, noCaster).IsContinue());
    CHECK_EQ(EffectsHeld(*orphan), uint32(0x3));
    delete orphan;

    // A holder with no third effect (a target immune to the root) is left as it is, and so is one whose third effect is
    // its last: a holder is removed whole, by its unit's own removal.
    SpellAuraHolder* immune = ScreamHolder(target, 0x3, 0);
    AuraApplyContext immuneBefore(target, &priest, SpellRow(row, PSYCHIC_SCREAM), AuraHandle(*immune), false);
    CHECK(registry.Dispatch<AuraApplyHook>(PSYCHIC_SCREAM, immuneBefore).IsContinue());
    CHECK_EQ(EffectsHeld(*immune), uint32(0x3));
    delete immune;
    SpellAuraHolder* alone = ScreamHolder(target, 0x4, 0);
    AuraApplyContext aloneBefore(target, &priest, SpellRow(row, PSYCHIC_SCREAM), AuraHandle(*alone), false);
    CHECK(registry.Dispatch<AuraApplyHook>(PSYCHIC_SCREAM, aloneBefore).IsContinue());
    CHECK_EQ(EffectsHeld(*alone), uint32(0x4));
    delete alone;

    CHECK(target.GetSpellAuraHolderMap().empty());
    s_scream.Forget(TEST_SCREAM);
}

TEST(PriestShadowControl_SilenceInterruptsEveryTypeOfUnitButAPlayer)
{
    CHECK(!Priest::SilenceInterrupts(TYPEID_PLAYER));
    CHECK(Priest::SilenceInterrupts(TYPEID_UNIT));
    CHECK(Priest::SilenceInterrupts(TYPEID_GAMEOBJECT));
    CHECK(Priest::SilenceInterrupts(TYPEID_DYNAMICOBJECT));
    CHECK(Priest::SilenceInterrupts(TYPEID_CORPSE));
}

TEST(PriestShadowControl_SilencesRowCastsNothingBeforeTheAddOrWithNoCaster)
{
    SpellHandlerRegistry registry;
    Priest::RegisterSilence(registry);
    SpellRowStorage row;
    HolderStorage storage;
    BareCreature target;
    BareCreature priest;

    // The call before the add and an apply whose caster cannot be found are Continue and cast nothing: a cast would
    // need the spell store and a map.
    AuraApplyContext before(target, &priest, SpellRow(row, SILENCE), AuraHandle(EmptyHolder(storage)), false);
    CHECK(registry.Dispatch<AuraApplyHook>(SILENCE, before).IsContinue());
    AuraApplyContext noCaster(target, NULL, SpellRow(row, SILENCE), AuraHandle(EmptyHolder(storage)), true);
    CHECK(registry.Dispatch<AuraApplyHook>(SILENCE, noCaster).IsContinue());

    // A player target gets none either, whoever casts and at either call.
    BareCreature player;
    player.SetType(TYPEID_PLAYER);
    AuraApplyContext onPlayer(player, &priest, SpellRow(row, SILENCE), AuraHandle(EmptyHolder(storage)), true);
    CHECK(registry.Dispatch<AuraApplyHook>(SILENCE, onPlayer).IsContinue());
    AuraApplyContext onPlayerBefore(player, &priest, SpellRow(row, SILENCE), AuraHandle(EmptyHolder(storage)), false);
    CHECK(registry.Dispatch<AuraApplyHook>(SILENCE, onPlayerBefore).IsContinue());
}

TEST(PriestShadowControl_APhantasmRanksChanceIsItsEffectZero)
{
    // Without the spells in the store there is none.
    ForgetRanks();
    CHECK_EQ(Priest::PhantasmChance(PHANTASM_RANK_1), int32(0));
    CHECK_EQ(Priest::PhantasmChance(PHANTASM_RANK_2), int32(0));

    // The client's numbers: 50 and 100 percent; no other spell is a rank, Fade among them.
    StoreRanks();
    CHECK_EQ(Priest::PhantasmChance(PHANTASM_RANK_1), int32(50));
    CHECK_EQ(Priest::PhantasmChance(PHANTASM_RANK_2), int32(100));
    CHECK_EQ(Priest::PhantasmChance(FADE), int32(0));
    CHECK_EQ(Priest::PhantasmChance(0), int32(0));
    ForgetRanks();
}

TEST(PriestShadowControl_ThePhantasmRankAPriestHoldsIsTheHigherOfTheTwo)
{
    BareCreature priest;
    CHECK_EQ(Priest::PhantasmRank(priest), uint32(0));
    HolderStorage first, second;
    Hold(priest, PHANTASM_RANK_1, first);
    CHECK_EQ(Priest::PhantasmRank(priest), PHANTASM_RANK_1);
    Hold(priest, PHANTASM_RANK_2, second);
    CHECK_EQ(Priest::PhantasmRank(priest), PHANTASM_RANK_2);
}

TEST(PriestShadowControl_ARollBelowPhantasmsChanceFreesHimAndAHundredPercentDrawsNone)
{
    // A roll of 0 to 99 frees him below the chance: at 50, the rolls 0 to 49 and no other.
    for (int32 roll = 0; roll < 100; ++roll)
    {
        CHECK_EQ(Priest::PhantasmRolls(50, roll), roll < 50);
        CHECK(Priest::PhantasmRolls(100, roll));
        CHECK(Priest::PhantasmRolls(120, roll));
        CHECK(!Priest::PhantasmRolls(0, roll));
        CHECK(!Priest::PhantasmRolls(-50, roll));
    }
}

TEST(PriestShadowControl_FadesRowDoesNothingBeforeTheAddAndOnAPriestWithNoRank)
{
    SpellHandlerRegistry registry;
    Priest::RegisterPhantasm(registry);
    StoreRanks();
    SpellRowStorage row;
    HolderStorage storage;
    BareCreature priest;

    // No rank: Continue, the chance 0 draws nothing and nothing is removed (a removal would walk the unit's holders
    // and need a map).
    AuraApplyContext plain(priest, &priest, SpellRow(row, FADE), AuraHandle(EmptyHolder(storage)), true);
    CHECK(registry.Dispatch<AuraApplyHook>(FADE, plain).IsContinue());

    // The call before the add does nothing for a priest of the second rank either: it does not read the unit's holders.
    HolderStorage rank;
    Hold(priest, PHANTASM_RANK_2, rank);
    AuraApplyContext before(priest, &priest, SpellRow(row, FADE), AuraHandle(EmptyHolder(storage)), false);
    CHECK(registry.Dispatch<AuraApplyHook>(FADE, before).IsContinue());
    ForgetRanks();
}
