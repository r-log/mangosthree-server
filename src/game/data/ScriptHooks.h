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

#ifndef MANGOS_H_SCRIPTHOOKS
#define MANGOS_H_SCRIPTHOOKS

#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "DBCEnums.h"

struct AreaTriggerEntry;
class Aura;
class Creature;
class CreatureAI;
class GameObject;
class GameObjectAI;
class InstanceData;
class Item;
class Map;
class Object;
class Player;
class Quest;
class SpellCastTargets;
class Unit;

/**
 * What the core calls on the script library: its start, free and version, and the hooks for gossip,
 * quests, object and item use, area triggers, spell clicks, events, scripted spell effects and
 * auras, and the AI and instance data of a scripted creature, object or map. Each hook answers
 * whether a script handled the call, or what the script made. This base answers what a server with
 * no script library answers: false, NULL, and DIALOG_STATUS_UNDEFINED for a dialog status; it starts
 * and frees nothing and has no version. The core calls the installed hooks through sScriptHooks;
 * until hooks are installed, it calls this base.
 */
class ScriptHooks
{
    public:
        virtual ~ScriptHooks();

        virtual void InitScriptLibrary();
        virtual void FreeScriptLibrary();
        virtual char const* GetScriptLibraryVersion();

        virtual CreatureAI* GetCreatureAI(Creature* pCreature);
        virtual GameObjectAI* GetGameObjectAI(GameObject* pGo);
        virtual InstanceData* CreateInstanceData(Map* pMap);
        virtual bool OnGossipHello(Player* pPlayer, Creature* pCreature);
        virtual bool OnGossipHello(Player* pPlayer, GameObject* pGameObject);
        virtual bool OnGossipHello(Player* pPlayer, Item* pItem);
        virtual bool OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 sender, uint32 action, const char* code);
        virtual bool OnGossipSelect(Player* pPlayer, GameObject* pGameObject, uint32 sender, uint32 action, const char* code);
        virtual bool OnGossipSelect(Player* pPlayer, Item* pItem, uint32 sender, uint32 action, const char* code);
        virtual bool OnQuestAccept(Player* pPlayer, Creature* pCreature, Quest const* pQuest);
        virtual bool OnQuestAccept(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest);
        virtual bool OnQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest);
        virtual bool OnQuestRewarded(Player* pPlayer, Creature* pCreature, Quest const* pQuest, uint32 reward);
        virtual bool OnQuestRewarded(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest, uint32 reward);
        virtual uint32 GetDialogStatus(Player* pPlayer, Creature* pCreature);
        virtual uint32 GetDialogStatus(Player* pPlayer, GameObject* pGameObject);
        virtual bool OnGameObjectUse(Player* pPlayer, GameObject* pGameObject);
        virtual bool OnGameObjectUse(Unit* pUnit, GameObject* pGameObject);
        virtual bool OnItemUse(Player* pPlayer, Item* pItem, SpellCastTargets const& targets);
        virtual bool OnAreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry);
        virtual bool OnNpcSpellClick(Player* pPlayer, Creature* pClickedCreature, uint32 spellId);
        virtual bool OnProcessEvent(uint32 eventId, Object* pSource, Object* pTarget, bool isStart);
        virtual bool OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid);
        virtual bool OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, GameObject* pTarget, ObjectGuid originalCasterGuid);
        virtual bool OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Item* pTarget, ObjectGuid originalCasterGuid);
        virtual bool OnEffectScriptEffect(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid);
        virtual bool OnAuraDummy(Aura const* pAura, bool apply);

        /// The installed hooks, or the base when none are installed.
        static ScriptHooks& Installed();
        /// Installs `hooks` and answers the hooks it replaces (NULL for the base). NULL installs
        /// the base. Installed once at start-up, before the map threads run.
        static ScriptHooks* Install(ScriptHooks* hooks);
        /// Whether hooks are installed, as a script library installs its own.
        static bool IsInstalled();
};

#define sScriptHooks ScriptHooks::Installed()

#endif
