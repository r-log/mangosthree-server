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

/// The script dispatcher: with no legacy library it answers what the hook base answers; with one, every
/// hook reaches the library with its arguments and the library's answer comes back; a name is served
/// when the library serves it; and the bound names no script serves are listed in id order. The
/// objects handed to the hooks are distinct addresses the hooks pass on and never read.

#include "TestHarness.h"
#include "dispatch/Dispatcher.h"
#include "data/ScriptNameStore.h"
#include "DBCEnums.h"
#include "ObjectGuid.h"
#include "QuestDef.h"
#include "spells/pipeline/Spell.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    template <class T>
    T* Fake(uintptr_t address)
    {
        return reinterpret_cast<T*>(address);
    }

    Player* const kPlayer = Fake<Player>(0x1000);
    Creature* const kCreature = Fake<Creature>(0x2000);
    GameObject* const kObject = Fake<GameObject>(0x3000);
    Item* const kItem = Fake<Item>(0x4000);
    Quest const* const kQuest = Fake<Quest const>(0x5000);
    Unit* const kUnit = Fake<Unit>(0x6000);
    Map* const kMap = Fake<Map>(0x7000);
    Object* const kSource = Fake<Object>(0x8000);
    Object* const kTarget = Fake<Object>(0x9000);
    Aura const* const kAura = Fake<Aura const>(0xA000);
    AreaTriggerEntry const* const kTrigger = Fake<AreaTriggerEntry const>(0xB000);
    CreatureAI* const kCreatureAI = Fake<CreatureAI>(0xC000);
    GameObjectAI* const kObjectAI = Fake<GameObjectAI>(0xD000);
    InstanceData* const kInstanceData = Fake<InstanceData>(0xE000);
    const ObjectGuid kCaster(HIGHGUID_PLAYER, uint32(7));
    SpellCastTargets const kTargets;

    /// A legacy library that records each call, checks the arguments every hook is given, and answers
    /// what no base answers: true, its own AI, instance data and version, and dialog status 7.
    class RecordingLegacy : public ScriptHooks
    {
        public:
            std::vector<std::string> calls;
            bool argumentsCorrect = true;

            void Expect(bool correct)
            {
                argumentsCorrect = argumentsCorrect && correct;
            }

            void InitScriptLibrary() override { calls.push_back("init"); }
            void FreeScriptLibrary() override { calls.push_back("free"); }
            char const* GetScriptLibraryVersion() override { calls.push_back("version"); return "legacy"; }
            bool ServesScript(char const* name) override { calls.push_back("serves"); return std::strcmp(name, "npc_b") == 0; }

            CreatureAI* GetCreatureAI(Creature* c) override { calls.push_back("creatureAI"); Expect(c == kCreature); return kCreatureAI; }
            GameObjectAI* GetGameObjectAI(GameObject* g) override { calls.push_back("objectAI"); Expect(g == kObject); return kObjectAI; }
            InstanceData* CreateInstanceData(Map* m) override { calls.push_back("instance"); Expect(m == kMap); return kInstanceData; }
            bool OnGossipHello(Player* p, Creature* c) override { calls.push_back("helloCreature"); Expect(p == kPlayer && c == kCreature); return true; }
            bool OnGossipHello(Player* p, GameObject* g) override { calls.push_back("helloObject"); Expect(p == kPlayer && g == kObject); return true; }
            bool OnGossipHello(Player* p, Item* i) override { calls.push_back("helloItem"); Expect(p == kPlayer && i == kItem); return true; }
            bool OnGossipSelect(Player* p, Creature* c, uint32 s, uint32 a, const char* code) override { calls.push_back("selectCreature"); Expect(p == kPlayer && c == kCreature && s == 1 && a == 2 && code == NULL); return true; }
            bool OnGossipSelect(Player* p, GameObject* g, uint32 s, uint32 a, const char* code) override { calls.push_back("selectObject"); Expect(p == kPlayer && g == kObject && s == 3 && a == 4 && std::strcmp(code, "x") == 0); return true; }
            bool OnGossipSelect(Player* p, Item* i, uint32 s, uint32 a, const char* code) override { calls.push_back("selectItem"); Expect(p == kPlayer && i == kItem && s == 5 && a == 6 && code == NULL); return true; }
            bool OnQuestAccept(Player* p, Creature* c, Quest const* q) override { calls.push_back("acceptCreature"); Expect(p == kPlayer && c == kCreature && q == kQuest); return true; }
            bool OnQuestAccept(Player* p, GameObject* g, Quest const* q) override { calls.push_back("acceptObject"); Expect(p == kPlayer && g == kObject && q == kQuest); return true; }
            bool OnQuestAccept(Player* p, Item* i, Quest const* q) override { calls.push_back("acceptItem"); Expect(p == kPlayer && i == kItem && q == kQuest); return true; }
            bool OnQuestRewarded(Player* p, Creature* c, Quest const* q, uint32 r) override { calls.push_back("rewardCreature"); Expect(p == kPlayer && c == kCreature && q == kQuest && r == 8); return true; }
            bool OnQuestRewarded(Player* p, GameObject* g, Quest const* q, uint32 r) override { calls.push_back("rewardObject"); Expect(p == kPlayer && g == kObject && q == kQuest && r == 9); return true; }
            uint32 GetDialogStatus(Player* p, Creature* c) override { calls.push_back("dialogCreature"); Expect(p == kPlayer && c == kCreature); return 7; }
            uint32 GetDialogStatus(Player* p, GameObject* g) override { calls.push_back("dialogObject"); Expect(p == kPlayer && g == kObject); return 7; }
            bool OnGameObjectUse(Player* p, GameObject* g) override { calls.push_back("usePlayer"); Expect(p == kPlayer && g == kObject); return true; }
            bool OnGameObjectUse(Unit* u, GameObject* g) override { calls.push_back("useUnit"); Expect(u == kUnit && g == kObject); return true; }
            bool OnItemUse(Player* p, Item* i, SpellCastTargets const& t) override { calls.push_back("itemUse"); Expect(p == kPlayer && i == kItem && &t == &kTargets); return true; }
            bool OnAreaTrigger(Player* p, AreaTriggerEntry const* at) override { calls.push_back("areaTrigger"); Expect(p == kPlayer && at == kTrigger); return true; }
            bool OnNpcSpellClick(Player* p, Creature* c, uint32 spellId) override { calls.push_back("spellClick"); Expect(p == kPlayer && c == kCreature && spellId == 10); return true; }
            bool OnProcessEvent(uint32 eventId, Object* s, Object* t, bool isStart) override { calls.push_back("event"); Expect(eventId == 11 && s == kSource && t == kTarget && isStart); return true; }
            bool OnEffectDummy(Unit* c, uint32 spellId, SpellEffectIndex e, Unit* t, ObjectGuid g) override { calls.push_back("dummyUnit"); Expect(c == kUnit && spellId == 12 && e == EFFECT_INDEX_1 && t == kUnit && g == kCaster); return true; }
            bool OnEffectDummy(Unit* c, uint32 spellId, SpellEffectIndex e, GameObject* t, ObjectGuid g) override { calls.push_back("dummyObject"); Expect(c == kUnit && spellId == 13 && e == EFFECT_INDEX_2 && t == kObject && g == kCaster); return true; }
            bool OnEffectDummy(Unit* c, uint32 spellId, SpellEffectIndex e, Item* t, ObjectGuid g) override { calls.push_back("dummyItem"); Expect(c == kUnit && spellId == 14 && e == EFFECT_INDEX_0 && t == kItem && g == kCaster); return true; }
            bool OnEffectScriptEffect(Unit* c, uint32 spellId, SpellEffectIndex e, Unit* t, ObjectGuid g) override { calls.push_back("scriptEffect"); Expect(c == kUnit && spellId == 15 && e == EFFECT_INDEX_1 && t == kUnit && g == kCaster); return true; }
            bool OnAuraDummy(Aura const* a, bool apply) override { calls.push_back("auraDummy"); Expect(a == kAura && !apply); return true; }
    };

    void AddThreeNames(ScriptNameStore& store)
    {
        store.AddName("");
        store.AddName("npc_c");
        store.AddName("npc_a");
        store.AddName("npc_b");
        store.SortNames();
    }
}

TEST(ScriptDispatcher_WithNoLegacyLibraryItAnswersWhatTheBaseAnswers)
{
    Script::Dispatcher dispatcher(NULL);

    CHECK(dispatcher.GetScriptLibraryVersion() == NULL);
    CHECK(!dispatcher.ServesScript("npc_b"));
    CHECK(dispatcher.GetCreatureAI(kCreature) == NULL);
    CHECK(dispatcher.CreateInstanceData(kMap) == NULL);
    CHECK(!dispatcher.OnGossipHello(kPlayer, kCreature));
    CHECK(!dispatcher.OnQuestAccept(kPlayer, kItem, kQuest));
    CHECK(!dispatcher.OnGameObjectUse(kUnit, kObject));
    CHECK(!dispatcher.OnEffectDummy(kUnit, 12, EFFECT_INDEX_1, kItem, kCaster));
    CHECK(!dispatcher.OnAuraDummy(kAura, true));
    CHECK_EQ(dispatcher.GetDialogStatus(kPlayer, kCreature), uint32(DIALOG_STATUS_UNDEFINED));
    CHECK_EQ(dispatcher.GetDialogStatus(kPlayer, kObject), uint32(DIALOG_STATUS_UNDEFINED));
}

TEST(ScriptDispatcher_EveryHookReachesTheLegacyLibraryWithItsArgumentsAndAnswers)
{
    RecordingLegacy legacy;
    Script::Dispatcher dispatcher(&legacy);

    dispatcher.FreeScriptLibrary();
    CHECK_STR(std::string(dispatcher.GetScriptLibraryVersion()), "legacy");
    CHECK(dispatcher.GetCreatureAI(kCreature) == kCreatureAI);
    CHECK(dispatcher.GetGameObjectAI(kObject) == kObjectAI);
    CHECK(dispatcher.CreateInstanceData(kMap) == kInstanceData);
    CHECK(dispatcher.OnGossipHello(kPlayer, kCreature));
    CHECK(dispatcher.OnGossipHello(kPlayer, kObject));
    CHECK(dispatcher.OnGossipHello(kPlayer, kItem));
    CHECK(dispatcher.OnGossipSelect(kPlayer, kCreature, 1, 2, NULL));
    CHECK(dispatcher.OnGossipSelect(kPlayer, kObject, 3, 4, "x"));
    CHECK(dispatcher.OnGossipSelect(kPlayer, kItem, 5, 6, NULL));
    CHECK(dispatcher.OnQuestAccept(kPlayer, kCreature, kQuest));
    CHECK(dispatcher.OnQuestAccept(kPlayer, kObject, kQuest));
    CHECK(dispatcher.OnQuestAccept(kPlayer, kItem, kQuest));
    CHECK(dispatcher.OnQuestRewarded(kPlayer, kCreature, kQuest, 8));
    CHECK(dispatcher.OnQuestRewarded(kPlayer, kObject, kQuest, 9));
    CHECK_EQ(dispatcher.GetDialogStatus(kPlayer, kCreature), 7u);
    CHECK_EQ(dispatcher.GetDialogStatus(kPlayer, kObject), 7u);
    CHECK(dispatcher.OnGameObjectUse(kPlayer, kObject));
    CHECK(dispatcher.OnGameObjectUse(kUnit, kObject));
    CHECK(dispatcher.OnItemUse(kPlayer, kItem, kTargets));
    CHECK(dispatcher.OnAreaTrigger(kPlayer, kTrigger));
    CHECK(dispatcher.OnNpcSpellClick(kPlayer, kCreature, 10));
    CHECK(dispatcher.OnProcessEvent(11, kSource, kTarget, true));
    CHECK(dispatcher.OnEffectDummy(kUnit, 12, EFFECT_INDEX_1, kUnit, kCaster));
    CHECK(dispatcher.OnEffectDummy(kUnit, 13, EFFECT_INDEX_2, kObject, kCaster));
    CHECK(dispatcher.OnEffectDummy(kUnit, 14, EFFECT_INDEX_0, kItem, kCaster));
    CHECK(dispatcher.OnEffectScriptEffect(kUnit, 15, EFFECT_INDEX_1, kUnit, kCaster));
    CHECK(dispatcher.OnAuraDummy(kAura, false));

    CHECK(legacy.argumentsCorrect);
    const std::vector<std::string> expected = {
        "free", "version", "creatureAI", "objectAI", "instance", "helloCreature", "helloObject", "helloItem",
        "selectCreature", "selectObject", "selectItem", "acceptCreature", "acceptObject", "acceptItem",
        "rewardCreature", "rewardObject", "dialogCreature", "dialogObject", "usePlayer", "useUnit", "itemUse",
        "areaTrigger", "spellClick", "event", "dummyUnit", "dummyObject", "dummyItem", "scriptEffect", "auraDummy"};
    CHECK(legacy.calls == expected);
}

TEST(ScriptDispatcher_ANameIsServedWhenTheLegacyLibraryServesIt)
{
    RecordingLegacy legacy;
    Script::Dispatcher withLegacy(&legacy);
    Script::Dispatcher withoutLegacy(NULL);

    CHECK(!withLegacy.ServesScript("npc_a"));
    CHECK(withLegacy.ServesScript("npc_b"));
    CHECK(!withoutLegacy.ServesScript("npc_b"));
}

TEST(ScriptDispatcher_TheUnservedNamesAreTheBoundNamesNoScriptServesInIdOrder)
{
    ScriptNameStore store;
    AddThreeNames(store);

    RecordingLegacy legacy;
    const std::vector<std::string> withLegacy = Script::Dispatcher(&legacy).UnservedNames(store);
    const std::vector<std::string> withoutLegacy = Script::Dispatcher(NULL).UnservedNames(store);

    CHECK(withLegacy == std::vector<std::string>({"npc_a", "npc_c"}));
    CHECK(withoutLegacy == std::vector<std::string>({"npc_a", "npc_b", "npc_c"}));
}
