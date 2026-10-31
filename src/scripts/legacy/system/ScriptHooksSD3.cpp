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

#include "precompiled.h"
#include "ScriptDevMgr.h"

namespace
{
    /**
     * SD3's answers to the core's script hooks: each hook calls the SD3 function that serves it, and
     * the library's start, free and version are SD3's own. `mangosd` installs these hooks at start-up.
     */
    class SD3ScriptHooks : public ScriptHooks
    {
        public:
            void InitScriptLibrary() override
            {
                SD3::InitScriptLibrary();
            }

            void FreeScriptLibrary() override
            {
                SD3::FreeScriptLibrary();
            }

            char const* GetScriptLibraryVersion() override
            {
                return SD3::GetScriptLibraryVersion();
            }

            CreatureAI* GetCreatureAI(Creature* pCreature) override
            {
                return SD3::GetCreatureAI(pCreature);
            }

            GameObjectAI* GetGameObjectAI(GameObject* pGo) override
            {
                return SD3::GetGameObjectAI(pGo);
            }

            InstanceData* CreateInstanceData(Map* pMap) override
            {
                return SD3::CreateInstanceData(pMap);
            }

            bool OnGossipHello(Player* pPlayer, Creature* pCreature) override
            {
                return SD3::GossipHello(pPlayer, pCreature);
            }

            bool OnGossipHello(Player* pPlayer, GameObject* pGameObject) override
            {
                return SD3::GOGossipHello(pPlayer, pGameObject);
            }

            bool OnGossipHello(Player* pPlayer, Item* pItem) override
            {
                return SD3::ItemGossipHello(pPlayer, pItem);
            }

            bool OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 sender, uint32 action, const char* code) override
            {
                if (code)
                {
                    return SD3::GossipSelectWithCode(pPlayer, pCreature, sender, action, code);
                }
                else
                {
                    return SD3::GossipSelect(pPlayer, pCreature, sender, action);
                }
            }

            bool OnGossipSelect(Player* pPlayer, GameObject* pGameObject, uint32 sender, uint32 action, const char* code) override
            {
                if (code)
                {
                    return SD3::GOGossipSelectWithCode(pPlayer, pGameObject, sender, action, code);
                }
                else
                {
                    return SD3::GOGossipSelect(pPlayer, pGameObject, sender, action);
                }
            }

            bool OnGossipSelect(Player* pPlayer, Item* pItem, uint32 sender, uint32 action, const char* code) override
            {
                if (code)
                {
                    return SD3::ItemGossipSelectWithCode(pPlayer, pItem, sender, action, code);
                }
                else
                {
                    return SD3::ItemGossipSelect(pPlayer, pItem, sender, action);
                }
            }

            bool OnQuestAccept(Player* pPlayer, Creature* pCreature, Quest const* pQuest) override
            {
                return SD3::QuestAccept(pPlayer, pCreature, pQuest);
            }

            bool OnQuestAccept(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest) override
            {
                return SD3::GOQuestAccept(pPlayer, pGameObject, pQuest);
            }

            bool OnQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest) override
            {
                return SD3::ItemQuestAccept(pPlayer, pItem, pQuest);
            }

            bool OnQuestRewarded(Player* pPlayer, Creature* pCreature, Quest const* pQuest, uint32 /*reward*/) override
            {
                return SD3::QuestRewarded(pPlayer, pCreature, pQuest);
            }

            bool OnQuestRewarded(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest, uint32 /*reward*/) override
            {
                return SD3::GOQuestRewarded(pPlayer, pGameObject, pQuest);
            }

            uint32 GetDialogStatus(Player* pPlayer, Creature* pCreature) override
            {
                return SD3::GetNPCDialogStatus(pPlayer, pCreature);
            }

            uint32 GetDialogStatus(Player* pPlayer, GameObject* pGameObject) override
            {
                return SD3::GetGODialogStatus(pPlayer, pGameObject);
            }

            bool OnGameObjectUse(Player* pPlayer, GameObject* pGameObject) override
            {
                return SD3::GOUse(pPlayer, pGameObject);
            }

            bool OnGameObjectUse(Unit* pUnit, GameObject* pGameObject) override
            {
                return SD3::GOUse(pUnit, pGameObject);
            }

            bool OnItemUse(Player* pPlayer, Item* pItem, SpellCastTargets const& targets) override
            {
                return SD3::ItemUse(pPlayer, pItem, targets);
            }

            bool OnAreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry) override
            {
                return SD3::AreaTrigger(pPlayer, atEntry);
            }

            bool OnNpcSpellClick(Player* pPlayer, Creature* pClickedCreature, uint32 spellId) override
            {
                return SD3::NpcSpellClick(pPlayer, pClickedCreature, spellId);
            }

            bool OnProcessEvent(uint32 eventId, Object* pSource, Object* pTarget, bool isStart) override
            {
                return SD3::ProcessEvent(eventId, pSource, pTarget, isStart);
            }

            bool OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid) override
            {
                return SD3::EffectDummyUnit(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
            }

            bool OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, GameObject* pTarget, ObjectGuid originalCasterGuid) override
            {
                return SD3::EffectDummyGameObject(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
            }

            bool OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Item* pTarget, ObjectGuid originalCasterGuid) override
            {
                return SD3::EffectDummyItem(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
            }

            bool OnEffectScriptEffect(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid) override
            {
                return SD3::EffectScriptEffectUnit(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
            }

            bool OnAuraDummy(Aura const* pAura, bool apply) override
            {
                return SD3::AuraDummy(pAura, apply);
            }
    };
}

ScriptHooks& SD3::GetScriptHooks()
{
    static SD3ScriptHooks hooks;
    return hooks;
}
