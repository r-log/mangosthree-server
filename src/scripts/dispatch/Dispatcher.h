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

#ifndef MANGOS_H_SCRIPT_DISPATCHER
#define MANGOS_H_SCRIPT_DISPATCHER

#include "data/ScriptHooks.h"

#include <string>
#include <vector>

class ScriptNameStore;

namespace Script
{
    /**
     * The one implementation of the core's script hooks, which `mangosd` installs. Every hook goes to
     * the legacy library's hooks when there is one, and otherwise answers what the base answers.
     * Starting the scripts loads the script texts, starts the legacy library and reports each bound
     * script name no script serves.
     */
    class Dispatcher final : public ScriptHooks
    {
        public:
            /// `legacy` is the legacy library's hooks, or NULL when the server is built without it.
            explicit Dispatcher(ScriptHooks* legacy);

            Dispatcher(Dispatcher const&) = delete;
            Dispatcher& operator=(Dispatcher const&) = delete;

            /// The names bound in `names`, in id order, that no script serves.
            std::vector<std::string> UnservedNames(ScriptNameStore const& names) const;

            void InitScriptLibrary() override;
            void FreeScriptLibrary() override;
            char const* GetScriptLibraryVersion() override;
            bool ServesScript(char const* name) override;

            CreatureAI* GetCreatureAI(Creature* pCreature) override;
            GameObjectAI* GetGameObjectAI(GameObject* pGo) override;
            InstanceData* CreateInstanceData(Map* pMap) override;
            bool OnGossipHello(Player* pPlayer, Creature* pCreature) override;
            bool OnGossipHello(Player* pPlayer, GameObject* pGameObject) override;
            bool OnGossipHello(Player* pPlayer, Item* pItem) override;
            bool OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 sender, uint32 action, const char* code) override;
            bool OnGossipSelect(Player* pPlayer, GameObject* pGameObject, uint32 sender, uint32 action, const char* code) override;
            bool OnGossipSelect(Player* pPlayer, Item* pItem, uint32 sender, uint32 action, const char* code) override;
            bool OnQuestAccept(Player* pPlayer, Creature* pCreature, Quest const* pQuest) override;
            bool OnQuestAccept(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest) override;
            bool OnQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest) override;
            bool OnQuestRewarded(Player* pPlayer, Creature* pCreature, Quest const* pQuest, uint32 reward) override;
            bool OnQuestRewarded(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest, uint32 reward) override;
            uint32 GetDialogStatus(Player* pPlayer, Creature* pCreature) override;
            uint32 GetDialogStatus(Player* pPlayer, GameObject* pGameObject) override;
            bool OnGameObjectUse(Player* pPlayer, GameObject* pGameObject) override;
            bool OnGameObjectUse(Unit* pUnit, GameObject* pGameObject) override;
            bool OnItemUse(Player* pPlayer, Item* pItem, SpellCastTargets const& targets) override;
            bool OnAreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry) override;
            bool OnNpcSpellClick(Player* pPlayer, Creature* pClickedCreature, uint32 spellId) override;
            bool OnProcessEvent(uint32 eventId, Object* pSource, Object* pTarget, bool isStart) override;
            bool OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid) override;
            bool OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, GameObject* pTarget, ObjectGuid originalCasterGuid) override;
            bool OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Item* pTarget, ObjectGuid originalCasterGuid) override;
            bool OnEffectScriptEffect(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid) override;
            bool OnAuraDummy(Aura const* pAura, bool apply) override;

        private:
            /// The hooks a call goes to: the legacy library's, or the base's.
            ScriptHooks& Target();

            ScriptHooks* m_legacy;
            ScriptHooks m_base;
    };
}

#endif
