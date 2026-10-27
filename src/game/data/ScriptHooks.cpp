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

#include "data/ScriptHooks.h"
#include "QuestDef.h"

#include <atomic>

namespace
{
    std::atomic<ScriptHooks*> s_installed(NULL);

    ScriptHooks& BaseHooks()
    {
        static ScriptHooks hooks;
        return hooks;
    }
}

ScriptHooks::~ScriptHooks()
{
}

void ScriptHooks::InitScriptLibrary()
{
}

void ScriptHooks::FreeScriptLibrary()
{
}

char const* ScriptHooks::GetScriptLibraryVersion()
{
    return NULL;
}

CreatureAI* ScriptHooks::GetCreatureAI(Creature* /*pCreature*/)
{
    return NULL;
}

GameObjectAI* ScriptHooks::GetGameObjectAI(GameObject* /*pGo*/)
{
    return NULL;
}

InstanceData* ScriptHooks::CreateInstanceData(Map* /*pMap*/)
{
    return NULL;
}

bool ScriptHooks::OnGossipHello(Player* /*pPlayer*/, Creature* /*pCreature*/)
{
    return false;
}

bool ScriptHooks::OnGossipHello(Player* /*pPlayer*/, GameObject* /*pGameObject*/)
{
    return false;
}

bool ScriptHooks::OnGossipHello(Player* /*pPlayer*/, Item* /*pItem*/)
{
    return false;
}

bool ScriptHooks::OnGossipSelect(Player* /*pPlayer*/, Creature* /*pCreature*/, uint32 /*sender*/, uint32 /*action*/, const char* /*code*/)
{
    return false;
}

bool ScriptHooks::OnGossipSelect(Player* /*pPlayer*/, GameObject* /*pGameObject*/, uint32 /*sender*/, uint32 /*action*/, const char* /*code*/)
{
    return false;
}

bool ScriptHooks::OnGossipSelect(Player* /*pPlayer*/, Item* /*pItem*/, uint32 /*sender*/, uint32 /*action*/, const char* /*code*/)
{
    return false;
}

bool ScriptHooks::OnQuestAccept(Player* /*pPlayer*/, Creature* /*pCreature*/, Quest const* /*pQuest*/)
{
    return false;
}

bool ScriptHooks::OnQuestAccept(Player* /*pPlayer*/, GameObject* /*pGameObject*/, Quest const* /*pQuest*/)
{
    return false;
}

bool ScriptHooks::OnQuestAccept(Player* /*pPlayer*/, Item* /*pItem*/, Quest const* /*pQuest*/)
{
    return false;
}

bool ScriptHooks::OnQuestRewarded(Player* /*pPlayer*/, Creature* /*pCreature*/, Quest const* /*pQuest*/, uint32 /*reward*/)
{
    return false;
}

bool ScriptHooks::OnQuestRewarded(Player* /*pPlayer*/, GameObject* /*pGameObject*/, Quest const* /*pQuest*/, uint32 /*reward*/)
{
    return false;
}

uint32 ScriptHooks::GetDialogStatus(Player* /*pPlayer*/, Creature* /*pCreature*/)
{
    return DIALOG_STATUS_UNDEFINED;
}

uint32 ScriptHooks::GetDialogStatus(Player* /*pPlayer*/, GameObject* /*pGameObject*/)
{
    return DIALOG_STATUS_UNDEFINED;
}

bool ScriptHooks::OnGameObjectUse(Player* /*pPlayer*/, GameObject* /*pGameObject*/)
{
    return false;
}

bool ScriptHooks::OnGameObjectUse(Unit* /*pUnit*/, GameObject* /*pGameObject*/)
{
    return false;
}

bool ScriptHooks::OnItemUse(Player* /*pPlayer*/, Item* /*pItem*/, SpellCastTargets const& /*targets*/)
{
    return false;
}

bool ScriptHooks::OnAreaTrigger(Player* /*pPlayer*/, AreaTriggerEntry const* /*atEntry*/)
{
    return false;
}

bool ScriptHooks::OnNpcSpellClick(Player* /*pPlayer*/, Creature* /*pClickedCreature*/, uint32 /*spellId*/)
{
    return false;
}

bool ScriptHooks::OnProcessEvent(uint32 /*eventId*/, Object* /*pSource*/, Object* /*pTarget*/, bool /*isStart*/)
{
    return false;
}

bool ScriptHooks::OnEffectDummy(Unit* /*pCaster*/, uint32 /*spellId*/, SpellEffectIndex /*effIndex*/, Unit* /*pTarget*/, ObjectGuid /*originalCasterGuid*/)
{
    return false;
}

bool ScriptHooks::OnEffectDummy(Unit* /*pCaster*/, uint32 /*spellId*/, SpellEffectIndex /*effIndex*/, GameObject* /*pTarget*/, ObjectGuid /*originalCasterGuid*/)
{
    return false;
}

bool ScriptHooks::OnEffectDummy(Unit* /*pCaster*/, uint32 /*spellId*/, SpellEffectIndex /*effIndex*/, Item* /*pTarget*/, ObjectGuid /*originalCasterGuid*/)
{
    return false;
}

bool ScriptHooks::OnEffectScriptEffect(Unit* /*pCaster*/, uint32 /*spellId*/, SpellEffectIndex /*effIndex*/, Unit* /*pTarget*/, ObjectGuid /*originalCasterGuid*/)
{
    return false;
}

bool ScriptHooks::OnAuraDummy(Aura const* /*pAura*/, bool /*apply*/)
{
    return false;
}

ScriptHooks& ScriptHooks::Installed()
{
    ScriptHooks* hooks = s_installed.load(std::memory_order_acquire);
    return hooks ? *hooks : BaseHooks();
}

ScriptHooks* ScriptHooks::Install(ScriptHooks* hooks)
{
    return s_installed.exchange(hooks, std::memory_order_acq_rel);
}

bool ScriptHooks::IsInstalled()
{
    return s_installed.load(std::memory_order_acquire) != NULL;
}
