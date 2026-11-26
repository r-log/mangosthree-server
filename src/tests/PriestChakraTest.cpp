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

/// Tests of priest/holy/Chakra.cpp: the rows the file registers, in the game's table too; the state each spell enters
/// and the auras of each state; the events Chakra and Serenity keep, Prayer of Mending's heal not among Chakra's; the
/// proc's exits and the states it takes off; the state a priest is in; Serenity's refresh of his Renew; and Sanctuary's
/// Renew half leaving with it. The scripts run on bare Creatures (update fields allocated; no world, map, template or
/// AI); a cast needs a world, so no body here reaches one: the proc is run only where the priest already holds the
/// state it enters. The states and Renew are put into the spell store with their ids alone and taken out of it again;
/// their holders are made through their constructor with no aura and put in a bare creature's holder map, and the
/// engine's own removal takes them off. Scenario 1228 records the states in play.

#include "TestHarness.h"
#include "spells/classes/priest/holy/Chakra.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "spells/SpellCooldownMgr.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

namespace
{
    uint32 const CHAKRA = 14751;
    uint32 const SERENITY = 81208;
    uint32 const SANCTUARY = 81206;
    uint32 const SANCTUARY_RENEW = 81207;
    uint32 const CHASTISE = 81209;
    uint32 const SERENITY_REFRESH = 81585;
    uint32 const RENEW = 139;
    uint32 const PRAYER_OF_MENDING = 33076;
    uint32 const PRAYER_OF_MENDING_HEAL = 33110;
    uint32 const SMITE = 585;
    uint32 const HEAL_SPELL = 2050;

    /// The four direct heals that enter Serenity, Flash Heal's two ids among them.
    uint32 const SERENITY_SPELLS[] = { 2050, 2060, 2061, 32546, 101062 };
    /// The two spells that enter Sanctuary: Prayer of Healing and Prayer of Mending.
    uint32 const SANCTUARY_SPELLS[] = { 596, 33076 };
    /// The two spells that enter Chastise: Smite and Mind Spike.
    uint32 const CHASTISE_SPELLS[] = { 585, 73510 };
    /// Spells that enter no state: Prayer of Mending's heal, its aura and its jump; Renew; Chakra, the states and
    /// Serenity's refresh; Circle of Healing; the Holy Words (Chastise, Serenity, Sanctuary); Holy Fire; Divine Hymn's
    /// heal; Echo of Light's heal; Penance's heal.
    uint32 const STATELESS_SPELLS[] = { 33110, 41635, 41637, 139, 14751, 81206, 81207, 81208, 81209, 81585, 34861,
                                        88625, 88684, 88685, 14914, 64844, 77489, 47750 };

    // The nine ids of the eight spells that enter a state.
    std::vector<uint32> EnteringSpells()
    {
        std::vector<uint32> spells(std::begin(SERENITY_SPELLS), std::end(SERENITY_SPELLS));
        spells.insert(spells.end(), std::begin(SANCTUARY_SPELLS), std::end(SANCTUARY_SPELLS));
        spells.insert(spells.end(), std::begin(CHASTISE_SPELLS), std::end(CHASTISE_SPELLS));
        return spells;
    }

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
            void Name(uint32 counter)
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

    // The states' auras and Renew with only their ids.
    uint32 const STORED[] = { SANCTUARY, SANCTUARY_RENEW, SERENITY, CHASTISE, RENEW };
    ServerSpell s_rows[5];

    // Puts the five spells into the spell store.
    void StoreRows()
    {
        for (uint32 i = 0; i < 5; ++i)
        {
            s_rows[i].Entry().ID = STORED[i];
            sSpellStore.SetEntry(STORED[i], &s_rows[i].Entry());
        }
    }

    // Takes them out of the spell store again.
    void ForgetRows()
    {
        for (uint32 spellId : STORED)
        {
            sSpellStore.SetEntry(spellId, NULL);
        }
    }

    // A holder of `spellId`, from the store, cast by `caster` (NULL: `target` itself) on `target`, put in its map.
    SpellAuraHolder* Hold(uint32 spellId, BareCreature& target, BareCreature* caster = NULL)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(sSpellStore.LookupEntry(spellId), &target, caster, NULL, NULL);
        target.HoldAura(spellId, *holder);
        return holder;
    }

    // Every state aura on `priest`: Sanctuary, its Renew half, Serenity and Chastise.
    void HoldEveryState(BareCreature& priest)
    {
        for (uint32 aura : { SANCTUARY, SANCTUARY_RENEW, SERENITY, CHASTISE })
        {
            if (!priest.HasAura(aura))
            {
                Hold(aura, priest);
            }
        }
    }

    // Which of the four state auras `priest` holds, as a mask: 1 Sanctuary, 2 its Renew half, 4 Serenity, 8 Chastise.
    uint32 StatesHeld(BareCreature const& priest)
    {
        return (priest.HasAura(SANCTUARY) ? 1u : 0u) | (priest.HasAura(SANCTUARY_RENEW) ? 2u : 0u) |
               (priest.HasAura(SERENITY) ? 4u : 0u) | (priest.HasAura(CHASTISE) ? 8u : 0u);
    }

    // The check-proc row of `auraId` (Chakra or Serenity) on `owner`, for an event of `procSpell` (NULL: a melee
    // swing) of a helpful hit's flag and `procEx`.
    SpellHandlerOutcome<bool> CheckProc(SpellHandlerRegistry const& registry, uint32 auraId, BareCreature const& owner,
                                        SpellEntry const* procSpell, uint32 procEx)
    {
        SpellRowStorage auraRow;
        HolderStorage storage;
        CheckProcContext ctx(owner, NULL, SpellRow(auraRow, auraId), AuraHandle(EmptyHolder(storage)), procSpell,
                             PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, procEx, 0);
        return registry.Dispatch<CheckProcHook>(auraId, ctx);
    }

    // Chakra's proc row on `priest`, at effect `eff`, for the hit of `procSpell` (NULL: a melee swing).
    SpellHandlerOutcome<SpellAuraProcResult> Proc(SpellHandlerRegistry const& registry, BareCreature& priest,
                                                  SpellEntry const* procSpell, SpellEffectIndex eff)
    {
        SpellRowStorage chakraRow;
        HolderStorage storage;
        SpellCooldownMgr cooldowns;
        OnProcContext ctx(priest, &priest, SpellRow(chakraRow, CHAKRA), eff, AuraHandle(EmptyHolder(storage)),
                          procSpell, PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL, PROC_EX_NORMAL_HIT, 0, 10, ObjectGuid(), 0,
                          NULL, cooldowns);
        return registry.Dispatch<OnProcHook>(CHAKRA, ctx);
    }

    // Serenity's refresh, cast by `priest` at `unit` (NULL: an effect with no unit).
    SpellHandlerOutcome<void> Refresh(SpellHandlerRegistry const& registry, BareCreature& priest, BareCreature* unit)
    {
        SpellRowStorage row;
        int32 value = 0;
        int32 word = 0;
        DummyEffectContext ctx(priest, unit, NULL, NULL, SpellRow(row, SERENITY_REFRESH), EFFECT_INDEX_0, value,
                               ObjectGuid(), unit, word);
        return registry.Dispatch<DummyEffectHook>(SERENITY_REFRESH, ctx);
    }
}

TEST(PriestChakra_TheFileHoldsItsFiveRowsInTheGame)
{
    // Five rows: Chakra at the check-proc and proc hooks, Serenity at the check-proc hook, its refresh at the
    // dummy-effect hook and Sanctuary at the aura-remove hook; no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterChakra(registry), uint32(5));
    CHECK_EQ(registry.Count(), std::size_t(5));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(CheckProcHook::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(OnProcHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(DummyEffectHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraRemoveHook::Key), std::size_t(1));

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(registry.Find<CheckProcHook>(CHAKRA) != NULL);
    CHECK(game.Find<CheckProcHook>(CHAKRA) == registry.Find<CheckProcHook>(CHAKRA));
    CHECK(registry.Find<OnProcHook>(CHAKRA) != NULL);
    CHECK(game.Find<OnProcHook>(CHAKRA) == registry.Find<OnProcHook>(CHAKRA));
    CHECK(registry.Find<CheckProcHook>(SERENITY) != NULL);
    CHECK(game.Find<CheckProcHook>(SERENITY) == registry.Find<CheckProcHook>(SERENITY));
    CHECK(registry.Find<DummyEffectHook>(SERENITY_REFRESH) != NULL);
    CHECK(game.Find<DummyEffectHook>(SERENITY_REFRESH) == registry.Find<DummyEffectHook>(SERENITY_REFRESH));
    CHECK(registry.Find<AuraRemoveHook>(SANCTUARY) != NULL);
    CHECK(game.Find<AuraRemoveHook>(SANCTUARY) == registry.Find<AuraRemoveHook>(SANCTUARY));
    CHECK(registry.Find<CheckProcHook>(CHAKRA) != registry.Find<CheckProcHook>(SERENITY));

    // Serenity holds no proc row: the engine's trigger of its effect 1 casts the refresh. The other states, Renew and
    // Prayer of Mending's heal hold no proc row either, and Sanctuary's Renew half no aura-remove row.
    CHECK(game.Find<OnProcHook>(SERENITY) == NULL);
    for (uint32 spellId : { SANCTUARY, SANCTUARY_RENEW, CHASTISE, RENEW, PRAYER_OF_MENDING_HEAL })
    {
        CHECK(game.Find<CheckProcHook>(spellId) == NULL);
        CHECK(game.Find<OnProcHook>(spellId) == NULL);
    }
    CHECK(game.Find<AuraRemoveHook>(SANCTUARY_RENEW) == NULL);
}

TEST(PriestChakra_EachSpellNamesTheStateItEntersAndEachStateItsAuras)
{
    // The eight spells of Chakra's text, by id, Flash Heal's two ids among them.
    for (uint32 spellId : SERENITY_SPELLS)
    {
        CHECK_EQ(Priest::ChakraStateOf(spellId), SERENITY);
    }
    for (uint32 spellId : SANCTUARY_SPELLS)
    {
        CHECK_EQ(Priest::ChakraStateOf(spellId), SANCTUARY);
    }
    for (uint32 spellId : CHASTISE_SPELLS)
    {
        CHECK_EQ(Priest::ChakraStateOf(spellId), CHASTISE);
    }

    // No other spell: Prayer of Mending's heal, the hotfix of 2010-12-20, first among them.
    CHECK_EQ(Priest::ChakraStateOf(PRAYER_OF_MENDING_HEAL), uint32(0));
    CHECK_EQ(Priest::ChakraStateOf(0), uint32(0));
    for (uint32 spellId : STATELESS_SPELLS)
    {
        CHECK_EQ(Priest::ChakraStateOf(spellId), uint32(0));
    }

    // Serenity and Chastise are one aura each; Sanctuary is two, its Renew half after it.
    std::vector<uint32> const serenity = Priest::ChakraStateAuras(SERENITY);
    CHECK_EQ(serenity.size(), std::size_t(1));
    CHECK_EQ(serenity[0], SERENITY);
    std::vector<uint32> const sanctuary = Priest::ChakraStateAuras(SANCTUARY);
    CHECK_EQ(sanctuary.size(), std::size_t(2));
    CHECK_EQ(sanctuary[0], SANCTUARY);
    CHECK_EQ(sanctuary[1], SANCTUARY_RENEW);
    std::vector<uint32> const chastise = Priest::ChakraStateAuras(CHASTISE);
    CHECK_EQ(chastise.size(), std::size_t(1));
    CHECK_EQ(chastise[0], CHASTISE);

    // Anything else is no state: the Renew half, Chakra, a spell that enters a state, nothing.
    for (uint32 spellId : { SANCTUARY_RENEW, CHAKRA, HEAL_SPELL, 0u })
    {
        CHECK(Priest::ChakraStateAuras(spellId).empty());
    }
}

TEST(PriestChakra_ChakraKeepsItsEightSpellsAndDropsTheMendingHeal)
{
    SpellHandlerRegistry registry;
    Priest::RegisterChakra(registry);
    BareCreature priest;
    SpellRowStorage row;

    // A spell that enters a state is kept, at its cast end and at its hit, normal or critical.
    for (uint32 spellId : EnteringSpells())
    {
        for (uint32 procEx : { uint32(PROC_EX_CAST_END), uint32(PROC_EX_NORMAL_HIT), uint32(PROC_EX_CRITICAL_HIT) })
        {
            SpellHandlerOutcome<bool> kept = CheckProc(registry, CHAKRA, priest, &SpellRow(row, spellId), procEx);
            CHECK(kept.IsReturn());
            CHECK(kept.GetValue());
        }
    }

    // Any other spell's event is dropped: Prayer of Mending's heal, which a bounce casts for him, first; and a melee
    // swing, which has no spell.
    for (uint32 spellId : STATELESS_SPELLS)
    {
        SpellHandlerOutcome<bool> dropped = CheckProc(registry, CHAKRA, priest, &SpellRow(row, spellId),
                                                      PROC_EX_NORMAL_HIT);
        CHECK(dropped.IsReturn());
        CHECK(!dropped.GetValue());
    }
    SpellHandlerOutcome<bool> swing = CheckProc(registry, CHAKRA, priest, NULL, PROC_EX_NORMAL_HIT);
    CHECK(swing.IsReturn());
    CHECK(!swing.GetValue());
}

TEST(PriestChakra_SerenityKeepsTheFourDirectHealsAlone)
{
    SpellHandlerRegistry registry;
    Priest::RegisterChakra(registry);
    BareCreature priest;
    SpellRowStorage row;

    // Heal, Flash Heal (both ids), Greater Heal and Binding Heal are kept, normal or critical.
    for (uint32 spellId : SERENITY_SPELLS)
    {
        for (uint32 procEx : { uint32(PROC_EX_NORMAL_HIT), uint32(PROC_EX_CRITICAL_HIT) })
        {
            SpellHandlerOutcome<bool> kept = CheckProc(registry, SERENITY, priest, &SpellRow(row, spellId), procEx);
            CHECK(kept.IsReturn());
            CHECK(kept.GetValue());
        }
    }

    // The area heals, Smite and Mind Spike, Smite among the spells the client's effect 1 mask names, and every other
    // spell are dropped; so is a melee swing.
    for (uint32 spellId : EnteringSpells())
    {
        if (Priest::ChakraStateOf(spellId) == SERENITY)
        {
            continue;
        }
        SpellHandlerOutcome<bool> dropped = CheckProc(registry, SERENITY, priest, &SpellRow(row, spellId),
                                                      PROC_EX_NORMAL_HIT);
        CHECK(dropped.IsReturn());
        CHECK(!dropped.GetValue());
    }
    for (uint32 spellId : STATELESS_SPELLS)
    {
        SpellHandlerOutcome<bool> dropped = CheckProc(registry, SERENITY, priest, &SpellRow(row, spellId),
                                                      PROC_EX_NORMAL_HIT);
        CHECK(dropped.IsReturn());
        CHECK(!dropped.GetValue());
    }
    SpellHandlerOutcome<bool> swing = CheckProc(registry, SERENITY, priest, NULL, PROC_EX_NORMAL_HIT);
    CHECK(swing.IsReturn());
    CHECK(!swing.GetValue());
}

TEST(PriestChakra_TheProcTakesTheOtherStatesOffAndCastsNoStateHeHolds)
{
    SpellHandlerRegistry registry;
    Priest::RegisterChakra(registry);
    StoreRows();
    {
        BareCreature priest;
        SpellRowStorage row;

        // Effects 1 and 2 of a hit that enters a state, and effect 0 of any other spell's hit or a swing: nothing.
        HoldEveryState(priest);
        for (SpellEffectIndex eff : { EFFECT_INDEX_1, EFFECT_INDEX_2 })
        {
            SpellHandlerOutcome<SpellAuraProcResult> other = Proc(registry, priest, &SpellRow(row, SMITE), eff);
            CHECK(other.IsReturn());
            CHECK(other.GetValue() == SPELL_AURA_PROC_CANT_TRIGGER);
        }
        for (uint32 spellId : { PRAYER_OF_MENDING_HEAL, RENEW, CHAKRA })
        {
            SpellHandlerOutcome<SpellAuraProcResult> none = Proc(registry, priest, &SpellRow(row, spellId),
                                                                 EFFECT_INDEX_0);
            CHECK(none.IsReturn());
            CHECK(none.GetValue() == SPELL_AURA_PROC_CANT_TRIGGER);
        }
        SpellHandlerOutcome<SpellAuraProcResult> swing = Proc(registry, priest, NULL, EFFECT_INDEX_0);
        CHECK(swing.IsReturn());
        CHECK(swing.GetValue() == SPELL_AURA_PROC_CANT_TRIGGER);
        CHECK_EQ(StatesHeld(priest), uint32(15));

        // Heal, with Serenity held: the other states go, Sanctuary's Renew half with Sanctuary; OK, so the engine
        // spends Chakra's charge.
        SpellHandlerOutcome<SpellAuraProcResult> heal = Proc(registry, priest, &SpellRow(row, HEAL_SPELL),
                                                             EFFECT_INDEX_0);
        CHECK(heal.IsReturn());
        CHECK(heal.GetValue() == SPELL_AURA_PROC_OK);
        CHECK_EQ(StatesHeld(priest), uint32(4));

        // Smite, with Chastise held: Serenity and Sanctuary go.
        HoldEveryState(priest);
        SpellHandlerOutcome<SpellAuraProcResult> smite = Proc(registry, priest, &SpellRow(row, SMITE), EFFECT_INDEX_0);
        CHECK(smite.IsReturn());
        CHECK(smite.GetValue() == SPELL_AURA_PROC_OK);
        CHECK_EQ(StatesHeld(priest), uint32(8));

        // Prayer of Mending, with Sanctuary and its Renew half held: Serenity and Chastise go, the two halves stay.
        HoldEveryState(priest);
        SpellHandlerOutcome<SpellAuraProcResult> mending = Proc(registry, priest, &SpellRow(row, PRAYER_OF_MENDING),
                                                                EFFECT_INDEX_0);
        CHECK(mending.IsReturn());
        CHECK(mending.GetValue() == SPELL_AURA_PROC_OK);
        CHECK_EQ(StatesHeld(priest), uint32(3));
    }
    ForgetRows();
}

TEST(PriestChakra_TheStateIsTheStateAuraHeHolds)
{
    StoreRows();
    {
        // None, then Sanctuary's Renew half alone: no state.
        BareCreature priest;
        CHECK_EQ(Priest::ChakraState(priest), uint32(0));
        Hold(SANCTUARY_RENEW, priest);
        CHECK_EQ(Priest::ChakraState(priest), uint32(0));

        // Chastise; Sanctuary over Chastise; Serenity over both.
        Hold(CHASTISE, priest);
        CHECK_EQ(Priest::ChakraState(priest), CHASTISE);
        Hold(SANCTUARY, priest);
        CHECK_EQ(Priest::ChakraState(priest), SANCTUARY);
        Hold(SERENITY, priest);
        CHECK_EQ(Priest::ChakraState(priest), SERENITY);

        // Each alone.
        for (uint32 state : { SERENITY, SANCTUARY, CHASTISE })
        {
            BareCreature alone;
            Hold(state, alone);
            CHECK_EQ(Priest::ChakraState(alone), state);
        }
    }
    ForgetRows();
}

TEST(PriestChakra_SerenityPutsHisRenewBackToItsWholeDuration)
{
    SpellHandlerRegistry registry;
    Priest::RegisterChakra(registry);
    StoreRows();
    {
        BareCreature priest;
        priest.Name(1);
        BareCreature healed;
        healed.Name(2);
        BareCreature other;
        other.Name(3);

        // His own Renew on himself, 5 of its 12 seconds left: back to 12, its one stack kept.
        SpellAuraHolder* own = Hold(RENEW, priest);
        own->SetAuraMaxDuration(12000);
        own->SetAuraDuration(5000);
        CHECK(Priest::SerenityRefreshesRenew(priest, priest));
        CHECK_EQ(own->GetAuraDuration(), int32(12000));
        CHECK_EQ(own->GetAuraMaxDuration(), int32(12000));
        CHECK_EQ(own->GetStackAmount(), uint32(1));

        // Another priest's Renew on a friend is not his, and stays as it is; a unit with no Renew holds none of his.
        SpellAuraHolder* theirs = Hold(RENEW, healed, &other);
        theirs->SetAuraMaxDuration(12000);
        theirs->SetAuraDuration(3000);
        CHECK(!Priest::SerenityRefreshesRenew(priest, healed));
        CHECK_EQ(theirs->GetAuraDuration(), int32(3000));
        CHECK(!Priest::SerenityRefreshesRenew(priest, other));

        // The refresh's script effect does the same at its unit, then ends the effect; at a unit whose Renew is
        // another's, and with no unit, it ends it with nothing changed.
        own->SetAuraDuration(4000);
        CHECK(Refresh(registry, priest, &priest).IsReturn());
        CHECK_EQ(own->GetAuraDuration(), int32(12000));
        CHECK(Refresh(registry, priest, &healed).IsReturn());
        CHECK_EQ(theirs->GetAuraDuration(), int32(3000));
        CHECK(Refresh(registry, priest, NULL).IsReturn());
    }
    ForgetRows();
}

TEST(PriestChakra_SanctuaryTakesItsRenewHalfWithItButForANewSanctuary)
{
    SpellHandlerRegistry registry;
    Priest::RegisterChakra(registry);
    StoreRows();
    {
        BareCreature priest;
        HolderStorage storage;
        SpellRowStorage row;

        // Every removal but a new Sanctuary's takes the Renew half off; the holder's boosts run after it.
        for (AuraRemoveMode mode : { AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DEATH,
                                     AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DISPEL })
        {
            Hold(SANCTUARY_RENEW, priest);
            AuraRemoveContext ctx(priest, &priest, SpellRow(row, SANCTUARY), AuraHandle(EmptyHolder(storage)), mode);
            CHECK(registry.Dispatch<AuraRemoveHook>(SANCTUARY, ctx).IsContinue());
            CHECK(!priest.HasAura(SANCTUARY_RENEW));
        }

        // A new Sanctuary of his replacing it (AURA_REMOVE_BY_STACK) leaves the half.
        Hold(SANCTUARY_RENEW, priest);
        AuraRemoveContext replaced(priest, &priest, SpellRow(row, SANCTUARY), AuraHandle(EmptyHolder(storage)),
                                   AURA_REMOVE_BY_STACK);
        CHECK(registry.Dispatch<AuraRemoveHook>(SANCTUARY, replaced).IsContinue());
        CHECK(priest.HasAura(SANCTUARY_RENEW));

        // Through the engine: Sanctuary's removal takes the half with it, and leaves Chastise.
        Hold(SANCTUARY, priest);
        Hold(CHASTISE, priest);
        priest.RemoveAurasDueToSpell(SANCTUARY);
        CHECK(!priest.HasAura(SANCTUARY));
        CHECK(!priest.HasAura(SANCTUARY_RENEW));
        CHECK(priest.HasAura(CHASTISE));
    }
    ForgetRows();
}
