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

/// The database scripts' store and step type, with no database and no script manager: a new store
/// answers an empty chain map for each type and NULL for the internal type and a type past the last,
/// a lookup answers the chains a loader filled, a step's helpers read its command's fields, and the
/// effect that starts a spell's script is the one of the highest claim, the first of equals.

#include "TestHarness.h"
#include "data/DbScriptStore.h"
#include "data/ServerSpellStore.h"
#include "DBCStores.h"

#include <mutex>

namespace
{
    /// A spell no client data holds, with the effects given (0 for none) at indexes 0 to 2. The
    /// effects stay registered for the rest of the run, as the spell's id is used by no other test.
    struct ScriptedSpell
    {
        ServerSpell spell;
        SpellEffectEntry effects[MAX_EFFECT_INDEX];

        ScriptedSpell(uint32 id, uint32 effect0, uint32 effect1, uint32 effect2)
        {
            spell.Entry().ID = id;
            uint32 const kinds[MAX_EFFECT_INDEX] = { effect0, effect1, effect2 };
            for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                effects[i] = SpellEffectEntry();
                if (!kinds[i])
                {
                    continue;
                }
                effects[i].SpellID = id;
                effects[i].EffectIndex = i;
                effects[i].Effect = kinds[i];
                effects[i].EffectTriggerSpell = 0;
                AddSpellEffectEntry(&effects[i]);
            }
        }
    };

    ScriptInfo Step(uint32 id, uint32 command)
    {
        ScriptInfo step = ScriptInfo();
        step.id = id;
        step.command = command;
        return step;
    }
}

TEST(DbScriptStore_NewStoreAnswersEmptyChainsPerType)
{
    DbScriptStore store;
    for (int t = DBS_START; t < DBS_END; ++t)
    {
        ScriptChainMap const* chains = store.GetScriptChainMap(DBScriptType(t));
        REQUIRE(chains != NULL);
        CHECK(chains->empty());
    }
    CHECK(store.GetScriptChainMap(DBS_INTERNAL) == NULL);
    CHECK(store.GetScriptChainMap(DBS_END) == NULL);
}

TEST(DbScriptStore_LookupAnswersTheChainsTheLoaderFilled)
{
    DbScriptStore store;
    {
        std::lock_guard<std::mutex> guard(store.GetLoadLock());
        ScriptChainMap& chains = store.GetChainsForLoad(DBS_ON_SPELL);
        chains[7].push_back(Step(7, SCRIPT_COMMAND_TALK));
        chains[7].push_back(Step(7, SCRIPT_COMMAND_EMOTE));
        chains[9].push_back(Step(9, SCRIPT_COMMAND_CAST_SPELL));
    }

    ScriptChainMap const* spells = store.GetScriptChainMap(DBS_ON_SPELL);
    REQUIRE(spells != NULL);
    CHECK_EQ(spells->size(), size_t(2));
    REQUIRE(spells->count(7) == 1);
    ScriptChain const& seven = spells->find(7)->second;
    REQUIRE(seven.size() == 2);
    CHECK_EQ(seven[0].command, uint32(SCRIPT_COMMAND_TALK));
    CHECK_EQ(seven[1].command, uint32(SCRIPT_COMMAND_EMOTE));
    CHECK(store.GetScriptChainMap(DBS_ON_SPELL) == spells);
    CHECK(store.GetScriptChainMap(DBS_ON_GOSSIP)->empty());
}

TEST(DbScriptStore_StepHelpersReadTheirCommandsFields)
{
    ScriptInfo respawn = Step(1, SCRIPT_COMMAND_RESPAWN_GO);
    respawn.respawnGo.goGuid = 501;
    CHECK_EQ(respawn.GetGOGuid(), 501u);
    CHECK(!respawn.IsCreatureBuddy());

    ScriptInfo door = Step(2, SCRIPT_COMMAND_OPEN_DOOR);
    door.changeDoor.goGuid = 502;
    CHECK_EQ(door.GetGOGuid(), 502u);
    CHECK(!door.IsCreatureBuddy());

    ScriptInfo talk = Step(3, SCRIPT_COMMAND_TALK);
    CHECK_EQ(talk.GetGOGuid(), 0u);
    CHECK(talk.IsCreatureBuddy());
    CHECK(!talk.HasAdditionalScriptFlag());
    CHECK(Step(4, SCRIPT_COMMAND_CAST_SPELL).HasAdditionalScriptFlag());
}

TEST(DbScriptStore_TheEffectOfTheHighestClaimStartsTheSpellsScript)
{
    static ScriptedSpell spell(990101, SPELL_EFFECT_DUMMY, SPELL_EFFECT_SCRIPT_EFFECT, SPELL_EFFECT_APPLY_AURA);
    SpellEntry const* entry = &spell.spell.Entry();
    CHECK_EQ(DbScriptStore::GetSpellStartDBScriptPriority(entry, EFFECT_INDEX_0), uint8(9));
    CHECK_EQ(DbScriptStore::GetSpellStartDBScriptPriority(entry, EFFECT_INDEX_1), uint8(10));
    CHECK_EQ(DbScriptStore::GetSpellStartDBScriptPriority(entry, EFFECT_INDEX_2), uint8(0));
    CHECK(!DbScriptStore::CanSpellEffectStartDBScript(entry, EFFECT_INDEX_0));
    CHECK(DbScriptStore::CanSpellEffectStartDBScript(entry, EFFECT_INDEX_1));
    CHECK(!DbScriptStore::CanSpellEffectStartDBScript(entry, EFFECT_INDEX_2));
}

TEST(DbScriptStore_OfTwoEqualClaimsTheFirstStartsTheSpellsScript)
{
    static ScriptedSpell spell(990102, SPELL_EFFECT_APPLY_AURA, SPELL_EFFECT_DUMMY, SPELL_EFFECT_DUMMY);
    SpellEntry const* entry = &spell.spell.Entry();
    CHECK(!DbScriptStore::CanSpellEffectStartDBScript(entry, EFFECT_INDEX_0));
    CHECK(DbScriptStore::CanSpellEffectStartDBScript(entry, EFFECT_INDEX_1));
    CHECK(!DbScriptStore::CanSpellEffectStartDBScript(entry, EFFECT_INDEX_2));
}

TEST(DbScriptStore_ATriggerOfASpellNoClientDataHoldsStartsTheScript)
{
    static ScriptedSpell spell(990103, SPELL_EFFECT_TRIGGER_MISSILE, SPELL_EFFECT_TRIGGER_SPELL, 0);
    SpellEntry const* entry = &spell.spell.Entry();
    CHECK_EQ(DbScriptStore::GetSpellStartDBScriptPriority(entry, EFFECT_INDEX_0), uint8(4));
    CHECK_EQ(DbScriptStore::GetSpellStartDBScriptPriority(entry, EFFECT_INDEX_1), uint8(5));
    CHECK_EQ(DbScriptStore::GetSpellStartDBScriptPriority(entry, EFFECT_INDEX_2), uint8(0));
    CHECK(!DbScriptStore::CanSpellEffectStartDBScript(entry, EFFECT_INDEX_0));
    CHECK(DbScriptStore::CanSpellEffectStartDBScript(entry, EFFECT_INDEX_1));
    CHECK(!DbScriptStore::CanSpellEffectStartDBScript(entry, EFFECT_INDEX_2));
}
