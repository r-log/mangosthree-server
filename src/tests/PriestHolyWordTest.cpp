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

/// Tests of priest/holy/HolyWord.cpp: the rows the file registers, in the game's table too; the state each Word needs
/// and what each cast answers for a priest with or without Revelations in each state; the check-cast row on priests
/// who hold the auras; and the tick and launch rows where no Sanctuary area stands. The scripts run on bare Creatures
/// (update fields allocated; no world, map, template or AI); a cast and a dynamic object need a world, so no body here
/// reaches one: a priest with no area standing is the only one the tick and the launch can be run for. Revelations
/// and the states are put into the spell store with their ids alone and taken out of it again; their holders are made
/// through their constructor with no aura and put in a bare creature's holder map. Scenario 1229 records the Words in
/// play.

#include "TestHarness.h"
#include "spells/classes/priest/holy/HolyWord.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/data/SpellMgr.h"
#include "spells/auras/SpellAuras.h"
#include "data/ServerSpellStore.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"

#include <cstddef>
#include <cstdint>

namespace
{
    uint32 const SERENITY_WORD = 88684;
    uint32 const SANCTUARY_WORD = 88685;
    uint32 const SANCTUARY_HEAL = 88686;
    uint32 const CHASTISE_WORD = 88625;
    uint32 const REVELATIONS = 88627;
    uint32 const SERENITY_STATE = 81208;
    uint32 const SANCTUARY_STATE = 81206;
    uint32 const SANCTUARY_RENEW = 81207;
    uint32 const CHASTISE_STATE = 81209;

    /// The states, and the answers that are no state: none, Sanctuary's Renew half alone.
    uint32 const STATES[] = { SERENITY_STATE, SANCTUARY_STATE, CHASTISE_STATE };

    /// Spells that are no Holy Word: Chastise, Sanctuary's heal, the states, Revelations, Heal, nothing.
    uint32 const OTHER_SPELLS[] = { CHASTISE_WORD, SANCTUARY_HEAL, SERENITY_STATE, SANCTUARY_STATE, CHASTISE_STATE,
                                    REVELATIONS, 2050, 64844, 0 };

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

    // Revelations and the states with only their ids.
    uint32 const STORED[] = { REVELATIONS, SERENITY_STATE, SANCTUARY_STATE, SANCTUARY_RENEW, CHASTISE_STATE };
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

    // A holder of `spellId`, from the store, cast by `target` itself, put in its map.
    SpellAuraHolder* Hold(uint32 spellId, BareCreature& target)
    {
        SpellAuraHolder* holder = new SpellAuraHolder(sSpellStore.LookupEntry(spellId), &target, NULL, NULL, NULL);
        target.HoldAura(spellId, *holder);
        return holder;
    }

    // The check-cast row of `spellId` on `priest`: what the cast of that spell answers.
    SpellHandlerOutcome<SpellCastResult> Check(SpellHandlerRegistry const& registry, BareCreature const& priest,
                                               uint32 spellId)
    {
        SpellRowStorage row;
        CheckCastContext ctx(priest, NULL, SpellRow(row, spellId), true);
        return registry.Dispatch<CheckCastHook>(spellId, ctx);
    }
}

TEST(PriestHolyWord_TheFileHoldsItsFourRowsInTheGame)
{
    // Four rows: both Words at the check-cast hook, Sanctuary at the periodic-tick hook and at the launch hook; no
    // default.
    SpellHandlerRegistry registry;
    CHECK_EQ(Priest::RegisterHolyWord(registry), uint32(4));
    CHECK_EQ(registry.Count(), std::size_t(4));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(CheckCastHook::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(PeriodicTickHook::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(LaunchHook::Key), std::size_t(1));

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 word : { SERENITY_WORD, SANCTUARY_WORD })
    {
        CHECK(registry.Find<CheckCastHook>(word) != NULL);
        CHECK(game.Find<CheckCastHook>(word) == registry.Find<CheckCastHook>(word));
    }
    CHECK(registry.Find<PeriodicTickHook>(SANCTUARY_WORD) != NULL);
    CHECK(game.Find<PeriodicTickHook>(SANCTUARY_WORD) == registry.Find<PeriodicTickHook>(SANCTUARY_WORD));
    CHECK(registry.Find<LaunchHook>(SANCTUARY_WORD) != NULL);
    CHECK(game.Find<LaunchHook>(SANCTUARY_WORD) == registry.Find<LaunchHook>(SANCTUARY_WORD));

    // Serenity's heal and aura are the data's: it holds no tick and no launch row. Neither does Sanctuary's heal
    // (88686), a plain heal at a point, nor Chastise.
    CHECK(game.Find<PeriodicTickHook>(SERENITY_WORD) == NULL);
    CHECK(game.Find<LaunchHook>(SERENITY_WORD) == NULL);
    for (uint32 spellId : { SANCTUARY_HEAL, CHASTISE_WORD })
    {
        CHECK(game.Find<CheckCastHook>(spellId) == NULL);
        CHECK(game.Find<PeriodicTickHook>(spellId) == NULL);
        CHECK(game.Find<LaunchHook>(spellId) == NULL);
    }
}

TEST(PriestHolyWord_EachWordNamesTheStateItNeeds)
{
    CHECK_EQ(Priest::HolyWordState(SERENITY_WORD), SERENITY_STATE);
    CHECK_EQ(Priest::HolyWordState(SANCTUARY_WORD), SANCTUARY_STATE);

    // No other spell needs a state: Chastise, Sanctuary's heal, the states, Revelations, Heal, nothing.
    for (uint32 spellId : OTHER_SPELLS)
    {
        CHECK_EQ(Priest::HolyWordState(spellId), uint32(0));
    }
}

TEST(PriestHolyWord_AWordIsRefusedWithoutRevelationsAndOutsideItsStateAndNoOtherSpellIsRefused)
{
    for (uint32 word : { SERENITY_WORD, SANCTUARY_WORD })
    {
        uint32 const own = Priest::HolyWordState(word);

        // Revelations and the Word's state: the cast stands.
        CHECK_EQ(int(Priest::HolyWordRefusal(word, true, own)), int(SPELL_CAST_OK));

        // Revelations and no state, or another state: the state is wanted.
        CHECK_EQ(int(Priest::HolyWordRefusal(word, true, 0)), int(SPELL_FAILED_CASTER_AURASTATE));
        for (uint32 state : STATES)
        {
            if (state != own)
            {
                CHECK_EQ(int(Priest::HolyWordRefusal(word, true, state)), int(SPELL_FAILED_CASTER_AURASTATE));
            }
        }

        // No Revelations: the Word is not known, whatever the state.
        CHECK_EQ(int(Priest::HolyWordRefusal(word, false, own)), int(SPELL_FAILED_NOT_KNOWN));
        CHECK_EQ(int(Priest::HolyWordRefusal(word, false, 0)), int(SPELL_FAILED_NOT_KNOWN));
        for (uint32 state : STATES)
        {
            CHECK_EQ(int(Priest::HolyWordRefusal(word, false, state)), int(SPELL_FAILED_NOT_KNOWN));
        }
    }

    // A spell that is no Holy Word is never refused, with Revelations or without, in any state.
    for (uint32 spellId : OTHER_SPELLS)
    {
        for (bool revelations : { true, false })
        {
            for (uint32 state : { uint32(0), SERENITY_STATE, SANCTUARY_STATE, CHASTISE_STATE })
            {
                CHECK_EQ(int(Priest::HolyWordRefusal(spellId, revelations, state)), int(SPELL_CAST_OK));
            }
        }
    }
}

TEST(PriestHolyWord_TheCheckRowReadsRevelationsAndTheStateHeHolds)
{
    SpellHandlerRegistry registry;
    Priest::RegisterHolyWord(registry);
    StoreRows();
    {
        // No Revelations and no state: both Words are refused as not known.
        BareCreature bare;
        for (uint32 word : { SERENITY_WORD, SANCTUARY_WORD })
        {
            SpellHandlerOutcome<SpellCastResult> refused = Check(registry, bare, word);
            CHECK(refused.IsReturn());
            CHECK_EQ(int(refused.GetValue()), int(SPELL_FAILED_NOT_KNOWN));
        }

        // A state with no Revelations: still not known.
        BareCreature stated;
        Hold(SERENITY_STATE, stated);
        SpellHandlerOutcome<SpellCastResult> noRevelations = Check(registry, stated, SERENITY_WORD);
        CHECK(noRevelations.IsReturn());
        CHECK_EQ(int(noRevelations.GetValue()), int(SPELL_FAILED_NOT_KNOWN));

        // Revelations and no state: both are refused for the state.
        BareCreature revealed;
        Hold(REVELATIONS, revealed);
        for (uint32 word : { SERENITY_WORD, SANCTUARY_WORD })
        {
            SpellHandlerOutcome<SpellCastResult> refused = Check(registry, revealed, word);
            CHECK(refused.IsReturn());
            CHECK_EQ(int(refused.GetValue()), int(SPELL_FAILED_CASTER_AURASTATE));
        }

        // Revelations and Serenity: Serenity stands, Sanctuary is refused.
        Hold(SERENITY_STATE, revealed);
        CHECK(Check(registry, revealed, SERENITY_WORD).IsContinue());
        SpellHandlerOutcome<SpellCastResult> sanctuaryInSerenity = Check(registry, revealed, SANCTUARY_WORD);
        CHECK(sanctuaryInSerenity.IsReturn());
        CHECK_EQ(int(sanctuaryInSerenity.GetValue()), int(SPELL_FAILED_CASTER_AURASTATE));

        // Revelations and Sanctuary, its Renew half with it: Sanctuary stands, Serenity is refused.
        BareCreature sanctified;
        Hold(REVELATIONS, sanctified);
        Hold(SANCTUARY_STATE, sanctified);
        Hold(SANCTUARY_RENEW, sanctified);
        CHECK(Check(registry, sanctified, SANCTUARY_WORD).IsContinue());
        SpellHandlerOutcome<SpellCastResult> serenityInSanctuary = Check(registry, sanctified, SERENITY_WORD);
        CHECK(serenityInSanctuary.IsReturn());
        CHECK_EQ(int(serenityInSanctuary.GetValue()), int(SPELL_FAILED_CASTER_AURASTATE));

        // Revelations and Sanctuary's Renew half alone, which is no state: both are refused for the state.
        BareCreature halved;
        Hold(REVELATIONS, halved);
        Hold(SANCTUARY_RENEW, halved);
        for (uint32 word : { SERENITY_WORD, SANCTUARY_WORD })
        {
            SpellHandlerOutcome<SpellCastResult> refused = Check(registry, halved, word);
            CHECK(refused.IsReturn());
            CHECK_EQ(int(refused.GetValue()), int(SPELL_FAILED_CASTER_AURASTATE));
        }

        // Revelations and Chastise's state: both are refused for the state.
        BareCreature chastised;
        Hold(REVELATIONS, chastised);
        Hold(CHASTISE_STATE, chastised);
        for (uint32 word : { SERENITY_WORD, SANCTUARY_WORD })
        {
            SpellHandlerOutcome<SpellCastResult> refused = Check(registry, chastised, word);
            CHECK(refused.IsReturn());
            CHECK_EQ(int(refused.GetValue()), int(SPELL_FAILED_CASTER_AURASTATE));
        }

        // The check is a pure read: a second answer is the first's, and it changed nothing the priest holds.
        CHECK(Check(registry, sanctified, SANCTUARY_WORD).IsContinue());
        CHECK(sanctified.HasAura(SANCTUARY_STATE));
        CHECK(sanctified.HasAura(REVELATIONS));
    }
    ForgetRows();
}

TEST(PriestHolyWord_TheTickEndsOnEffectOneAndCastsNothingWhereNoAreaStandsAndTheLaunchLeavesTheCast)
{
    SpellHandlerRegistry registry;
    Priest::RegisterHolyWord(registry);
    BareCreature priest;
    BareCreature other;
    SpellRowStorage row;
    HolderStorage storage;
    uint32 amount = 0;

    // Effect 1, the dummy tick, is the script's: it ends (Return) and, with no dynamic object of 88685 standing, casts
    // nothing.
    PeriodicTickContext tick(priest, &priest, SpellRow(row, SANCTUARY_WORD), EFFECT_INDEX_1,
                             AuraHandle(EmptyHolder(storage)), amount, true);
    CHECK(registry.Dispatch<PeriodicTickHook>(SANCTUARY_WORD, tick).IsReturn());
    CHECK_EQ(amount, uint32(0));

    // Effect 0, the area's own aura, and any other effect: the engine's tick goes on.
    for (SpellEffectIndex eff : { EFFECT_INDEX_0, EFFECT_INDEX_2 })
    {
        PeriodicTickContext aura(priest, &priest, SpellRow(row, SANCTUARY_WORD), eff, AuraHandle(EmptyHolder(storage)),
                                 amount, true);
        CHECK(registry.Dispatch<PeriodicTickHook>(SANCTUARY_WORD, aura).IsContinue());
    }

    // An aura whose caster cannot be found: the engine's tick goes on, and the context casts nothing at a point.
    PeriodicTickContext orphan(priest, NULL, SpellRow(row, SANCTUARY_WORD), EFFECT_INDEX_1,
                               AuraHandle(EmptyHolder(storage)), amount, true);
    CHECK(registry.Dispatch<PeriodicTickHook>(SANCTUARY_WORD, orphan).IsContinue());
    orphan.CastTriggeredAt(1.0f, 2.0f, 3.0f, SANCTUARY_HEAL);
    CHECK(priest.GetSpellAuraHolderMap().empty());

    // Another spell's tick reaches no row.
    PeriodicTickContext otherTick(priest, &other, SpellRow(row, SANCTUARY_HEAL), EFFECT_INDEX_1,
                                  AuraHandle(EmptyHolder(storage)), amount, true);
    CHECK(registry.Dispatch<PeriodicTickHook>(SANCTUARY_HEAL, otherTick).IsMiss());

    // The launch of a priest with no area standing takes nothing away and lets the cast go on.
    LaunchContext launch(priest, SpellRow(row, SANCTUARY_WORD), 500);
    CHECK(registry.Dispatch<LaunchHook>(SANCTUARY_WORD, launch).IsContinue());
    CHECK(priest.GetDynObject(SANCTUARY_WORD) == NULL);
}
