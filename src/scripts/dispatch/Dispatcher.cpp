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

#include "dispatch/Dispatcher.h"

#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "data/ScriptNameStore.h"

namespace Script
{
    Dispatcher::Dispatcher(ScriptHooks* legacy) : m_legacy(legacy)
    {
    }

    ScriptHooks& Dispatcher::Target()
    {
        return m_legacy ? *m_legacy : m_base;
    }

    std::vector<std::string> Dispatcher::UnservedNames(ScriptNameStore const& names) const
    {
        std::vector<std::string> unserved;
        for (uint32 id = 1; id < names.GetScriptIdsCount(); ++id)
        {
            char const* name = names.GetScriptName(id);
            if (!m_legacy || !m_legacy->ServesScript(name))
            {
                unserved.push_back(name);
            }
        }
        return unserved;
    }

    void Dispatcher::InitScriptLibrary()
    {
        sLog.outString("Loading Script Texts...");
        LoadMangosStrings(WorldDatabase, "script_texts", MAX_CREATURE_AI_TEXT_STRING_ID, 2 * MAX_CREATURE_AI_TEXT_STRING_ID + 1, true);

        Target().InitScriptLibrary();

        std::vector<std::string> unserved = UnservedNames(sScriptNameStore);
        if (m_legacy)
        {
            for (std::string const& name : unserved)
            {
                sLog.outErrorScriptLib("No script found for ScriptName '%s'.", name.c_str());
            }
        }
        else if (!unserved.empty())
        {
            sLog.outError("No script library is built in: %u bound script names have no script.", uint32(unserved.size()));
        }
    }

    void Dispatcher::FreeScriptLibrary()
    {
        Target().FreeScriptLibrary();
    }

    char const* Dispatcher::GetScriptLibraryVersion()
    {
        return Target().GetScriptLibraryVersion();
    }

    bool Dispatcher::ServesScript(char const* name)
    {
        return Target().ServesScript(name);
    }

    CreatureAI* Dispatcher::GetCreatureAI(Creature* pCreature)
    {
        return Target().GetCreatureAI(pCreature);
    }

    GameObjectAI* Dispatcher::GetGameObjectAI(GameObject* pGo)
    {
        return Target().GetGameObjectAI(pGo);
    }

    InstanceData* Dispatcher::CreateInstanceData(Map* pMap)
    {
        return Target().CreateInstanceData(pMap);
    }

    bool Dispatcher::OnGossipHello(Player* pPlayer, Creature* pCreature)
    {
        return Target().OnGossipHello(pPlayer, pCreature);
    }

    bool Dispatcher::OnGossipHello(Player* pPlayer, GameObject* pGameObject)
    {
        return Target().OnGossipHello(pPlayer, pGameObject);
    }

    bool Dispatcher::OnGossipHello(Player* pPlayer, Item* pItem)
    {
        return Target().OnGossipHello(pPlayer, pItem);
    }

    bool Dispatcher::OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 sender, uint32 action, const char* code)
    {
        return Target().OnGossipSelect(pPlayer, pCreature, sender, action, code);
    }

    bool Dispatcher::OnGossipSelect(Player* pPlayer, GameObject* pGameObject, uint32 sender, uint32 action, const char* code)
    {
        return Target().OnGossipSelect(pPlayer, pGameObject, sender, action, code);
    }

    bool Dispatcher::OnGossipSelect(Player* pPlayer, Item* pItem, uint32 sender, uint32 action, const char* code)
    {
        return Target().OnGossipSelect(pPlayer, pItem, sender, action, code);
    }

    bool Dispatcher::OnQuestAccept(Player* pPlayer, Creature* pCreature, Quest const* pQuest)
    {
        return Target().OnQuestAccept(pPlayer, pCreature, pQuest);
    }

    bool Dispatcher::OnQuestAccept(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest)
    {
        return Target().OnQuestAccept(pPlayer, pGameObject, pQuest);
    }

    bool Dispatcher::OnQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest)
    {
        return Target().OnQuestAccept(pPlayer, pItem, pQuest);
    }

    bool Dispatcher::OnQuestRewarded(Player* pPlayer, Creature* pCreature, Quest const* pQuest, uint32 reward)
    {
        return Target().OnQuestRewarded(pPlayer, pCreature, pQuest, reward);
    }

    bool Dispatcher::OnQuestRewarded(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest, uint32 reward)
    {
        return Target().OnQuestRewarded(pPlayer, pGameObject, pQuest, reward);
    }

    uint32 Dispatcher::GetDialogStatus(Player* pPlayer, Creature* pCreature)
    {
        return Target().GetDialogStatus(pPlayer, pCreature);
    }

    uint32 Dispatcher::GetDialogStatus(Player* pPlayer, GameObject* pGameObject)
    {
        return Target().GetDialogStatus(pPlayer, pGameObject);
    }

    bool Dispatcher::OnGameObjectUse(Player* pPlayer, GameObject* pGameObject)
    {
        return Target().OnGameObjectUse(pPlayer, pGameObject);
    }

    bool Dispatcher::OnGameObjectUse(Unit* pUnit, GameObject* pGameObject)
    {
        return Target().OnGameObjectUse(pUnit, pGameObject);
    }

    bool Dispatcher::OnItemUse(Player* pPlayer, Item* pItem, SpellCastTargets const& targets)
    {
        return Target().OnItemUse(pPlayer, pItem, targets);
    }

    bool Dispatcher::OnAreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry)
    {
        return Target().OnAreaTrigger(pPlayer, atEntry);
    }

    bool Dispatcher::OnNpcSpellClick(Player* pPlayer, Creature* pClickedCreature, uint32 spellId)
    {
        return Target().OnNpcSpellClick(pPlayer, pClickedCreature, spellId);
    }

    bool Dispatcher::OnProcessEvent(uint32 eventId, Object* pSource, Object* pTarget, bool isStart)
    {
        return Target().OnProcessEvent(eventId, pSource, pTarget, isStart);
    }

    bool Dispatcher::OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid)
    {
        return Target().OnEffectDummy(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
    }

    bool Dispatcher::OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, GameObject* pTarget, ObjectGuid originalCasterGuid)
    {
        return Target().OnEffectDummy(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
    }

    bool Dispatcher::OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Item* pTarget, ObjectGuid originalCasterGuid)
    {
        return Target().OnEffectDummy(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
    }

    bool Dispatcher::OnEffectScriptEffect(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid)
    {
        return Target().OnEffectScriptEffect(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
    }

    bool Dispatcher::OnAuraDummy(Aura const* pAura, bool apply)
    {
        return Target().OnAuraDummy(pAura, apply);
    }
}
