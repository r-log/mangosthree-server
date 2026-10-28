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

/**
 * @file GameObject.h
 * @brief Game object (GO) declarations and template structures.
 *
 * This file defines the GameObject class which represents static or interactive objects
 * in the game world. Game objects include doors, buttons, chests, traps, banners, chairs,
 * and various interactive environmental elements.
 *
 * Key functionality includes:
 * - Interactive object state management (opened/closed, activated/deactivated)
 * - Loot container mechanics
 * - Trap and spell effect triggers
 * - Capture point and control zone mechanics
 * - Door and transport system integration
 * - AI-driven game object behavior
 * - Animation and state synchronization
 * - Custom model and animation support
 *
 * GameObjectInfo, the template data of every game object type with its type-specific
 * field unions, is in data/GameObjectTemplateStore.h.
 *
 * @see GameObject for the main game object implementation
 * @see GameObjectInfo for template data structure
 * @see GameObjectAI for behavior control
 */

#ifndef MANGOSSERVER_GAMEOBJECT_H
#define MANGOSSERVER_GAMEOBJECT_H

#include <cmath>
#include "Platform/Define.h"
#include "Common/TimeConstants.h"
#include <ctime>
#include <string>
#include <vector>
#include "SharedDefines.h"
#include "data/GameObjectTemplateStore.h"
#include "data/GameObjectRecords.h"
#include "Object.h"
#include "LootMgr.h"
#include "Database/DatabaseEnv.h"
#include "Utilities/EventProcessor.h"
#include <memory>

class GameObjectAI;

// For containers:  [GO_NOT_READY]->GO_READY (close)->GO_ACTIVATED (open) ->GO_JUST_DEACTIVATED->GO_READY        -> ...
// For bobber:      [GO_NOT_READY]->GO_READY (close)->GO_ACTIVATED (open) ->GO_JUST_DEACTIVATED-><deleted>
// For door(closed):[GO_NOT_READY]->GO_READY (close)->GO_ACTIVATED (open) ->GO_JUST_DEACTIVATED->GO_READY(close) -> ...
// For door(open):  [GO_NOT_READY]->GO_READY (open) ->GO_ACTIVATED (close)->GO_JUST_DEACTIVATED->GO_READY(open)  -> ...
enum LootState
{
    GO_NOT_READY = 0,
    GO_READY,                                               // can be ready but despawned, and then not possible activate until spawn
    GO_ACTIVATED,
    GO_JUST_DEACTIVATED
};

enum CapturePointState
{
    CAPTURE_STATE_NEUTRAL = 0,
    CAPTURE_STATE_PROGRESS_ALLIANCE,
    CAPTURE_STATE_PROGRESS_HORDE,
    CAPTURE_STATE_CONTEST_ALLIANCE,
    CAPTURE_STATE_CONTEST_HORDE,
    CAPTURE_STATE_WIN_ALLIANCE,
    CAPTURE_STATE_WIN_HORDE
};

enum CapturePointSliderValue
{
    CAPTURE_SLIDER_ALLIANCE         = 100,                  // full alliance
    CAPTURE_SLIDER_HORDE            = 0,                    // full horde
    CAPTURE_SLIDER_MIDDLE           = 50                    // middle
};

class Unit;
class GameObjectModel;
#include "Geometry/Quat.h"

struct GameObjectDisplayInfoEntry;

// 5 sec for bobber catch
#define FISHING_BOBBER_READY_TIME 5

#define GO_ANIMPROGRESS_DEFAULT 0xFF

class GameObject : public WorldObject
{

    public:
        explicit GameObject();
        ~GameObject();

        void AddToWorld() override;
        void RemoveFromWorld() override;

        bool Create(uint32 guidlow, uint32 name_id, Map* map, uint32 phaseMask, float x, float y, float z, float ang,
                    const QuaternionData& rotation = QuaternionData(), uint8 animprogress = GO_ANIMPROGRESS_DEFAULT, GOState go_state = GO_STATE_READY);
        void Update(uint32 update_diff, uint32 p_time) override;
        GameObjectInfo const* GetGOInfo() const;

        bool IsTransport() const;

        bool HasStaticDBSpawnData() const;                  // listed in `gameobject` table and have fixed in DB guid

        // overwrite WorldObject function for proper name localization
        const char* GetNameForLocaleIdx(int32 locale_idx) const override;

        void SaveToDB();
        void SaveToDB(uint32 mapid, uint8 spawnMask, uint32 phaseMask);
        bool LoadFromDB(uint32 guid, Map* map);
        void DeleteFromDB();

        // z_rot, y_rot, x_rot - rotation angles around z, y and x axes
        void SetWorldRotationAngles(float z_rot, float y_rot, float x_rot);
        void SetWorldRotation(float qx, float qy, float qz, float qw);
        void SetTransportPathRotation(const QuaternionData& rotation);      // transforms(rotates) transport's path

        /// The pose the collision body is built from. Cataclysm keeps the rotation in
        /// m_worldRotation rather than in the object fields the earlier expansions use,
        /// so this is the one accessor GameObjectModel asks -- it must not have to know
        /// which expansion it is compiled for.
        void GetQuaternion(Geometry::Quat& q) const
        {
            q.x = m_worldRotation.x;
            q.y = m_worldRotation.y;
            q.z = m_worldRotation.z;
            q.w = m_worldRotation.w;
        }
        int64 GetPackedWorldRotation() const { return m_packedRotation; }

        void SetOwnerGuid(ObjectGuid ownerGuid)
        {
            m_spawnedByDefault = false;                     // all object with owner is despawned after delay
            SetGuidValue(OBJECT_FIELD_CREATED_BY, ownerGuid);
        }
        ObjectGuid const& GetOwnerGuid() const { return GetGuidValue(OBJECT_FIELD_CREATED_BY); }
        Unit* GetOwner() const;

        void SetSpellId(uint32 id)
        {
            m_spawnedByDefault = false;                     // all summoned object is despawned after delay
            m_spellId = id;
        }
        uint32 GetSpellId() const { return m_spellId;}

        time_t GetRespawnTime() const { return m_respawnTime; }
        time_t GetRespawnTimeEx() const
        {
            time_t now = time(NULL);
            if (m_respawnTime > now)
            {
                return m_respawnTime;
            }
            else
            {
                return now;
            }
        }

        void SetRespawnTime(time_t respawn)
        {
            m_respawnTime = respawn > 0 ? time(NULL) + respawn : 0;
            m_respawnDelayTime = respawn > 0 ? uint32(respawn) : 0;
        }
        void Respawn();
        bool isSpawned() const
        {
            return m_respawnDelayTime == 0 ||
                   (m_respawnTime > 0 && !m_spawnedByDefault) ||
                   (m_respawnTime == 0 && m_spawnedByDefault);
        }
        bool isSpawnedByDefault() const { return m_spawnedByDefault; }
        void SetSpawnedByDefault(bool b) { m_spawnedByDefault = b; }
        uint32 GetRespawnDelay() const { return m_respawnDelayTime; }
        void Refresh();
        void Delete();

        // Functions spawn/remove gameobject with DB guid in all loaded map copies (if point grid loaded in map)
        static void AddToRemoveListInMaps(uint32 db_guid, GameObjectData const* data);
        static void SpawnInMaps(uint32 db_guid, GameObjectData const* data);

        GameobjectTypes GetGoType() const { return GameobjectTypes(GetByteValue(GAMEOBJECT_BYTES_1, 1)); }
        void SetGoType(GameobjectTypes type) { SetByteValue(GAMEOBJECT_BYTES_1, 1, type); }
        GOState GetGoState() const { return GOState(GetByteValue(GAMEOBJECT_BYTES_1, 0)); }
        void SetGoState(GOState state);
        uint8 GetGoArtKit() const { return GetByteValue(GAMEOBJECT_BYTES_1, 2); }
        void SetGoArtKit(uint8 artkit) { SetByteValue(GAMEOBJECT_BYTES_1, 2, artkit); }
        uint8 GetGoAnimProgress() const { return GetByteValue(GAMEOBJECT_BYTES_1, 3); }
        void SetGoAnimProgress(uint8 animprogress) { SetByteValue(GAMEOBJECT_BYTES_1, 3, animprogress); }
        uint32 GetDisplayId() const { return GetUInt32Value(GAMEOBJECT_DISPLAYID); }
        void SetDisplayId(uint32 modelId);
        void SetPhaseMask(uint32 newPhaseMask, bool update) override;

        float ComputeBoundingRadius() const override;     // overwrite WorldObject version

        void Use(Unit* user);

        LootState getLootState() const { return m_lootState; }
        void SetLootState(LootState s);

        void AddToSkillupList(Player* player);
        bool IsInSkillupList(Player* player) const;
        void ClearSkillupList() { m_SkillupSet.clear(); }
        void ClearAllUsesData()
        {
            ClearSkillupList();
            m_useTimes = 0;
            m_firstUser.Clear();
            m_UniqueUsers.clear();
        }

        void AddUniqueUse(Player* player);
        void AddUse() { ++m_useTimes; }
        bool IsInUse() const { return m_isInUse; }
        void SetInUse(bool use);

        uint32 GetUseCount() const { return m_useTimes; }
        uint32 GetUniqueUseCount() const { return m_UniqueUsers.size(); }

        void SaveRespawnTime() override;

        // Loot System
        Loot loot;
        void StartGroupLoot(Group* group, uint32 timer) override;

        ObjectGuid GetLootRecipientGuid() const { return m_lootRecipientGuid; }
        uint32 GetLootGroupRecipientId() const { return m_lootGroupRecipientId; }
        Player* GetLootRecipient() const;                   // use group cases as prefered
        Group* GetGroupLootRecipient() const;
        bool HasLootRecipient() const { return m_lootGroupRecipientId || !m_lootRecipientGuid.IsEmpty(); }
        bool IsGroupLootRecipient() const { return m_lootGroupRecipientId; }
        void SetLootRecipient(Unit* pUnit);
        Player* GetOriginalLootRecipient() const;           // ignore group changes/etc, not for looting

        bool HasQuest(uint32 quest_id) const override;
        bool HasInvolvedQuest(uint32 quest_id) const override;
        bool ActivateToQuest(Player* pTarget) const;
        void UseDoorOrButton(uint32 time_to_restore = 0, bool alternative = false);
        // 0 = use `gameobject`.`spawntimesecs`
        void ResetDoorOrButton();

        void UpdateRotationFields(float rotation2 /*=0.0f*/, float rotation3 /*=0.0f*/);
        bool IsHostileTo(Unit const* unit) const override;
        bool IsFriendlyTo(Unit const* unit) const override;

        void SummonLinkedTrapIfAny();
        void TriggerLinkedGameObject(Unit* target);

        // Destructible GO handling
        void DealGameObjectDamage(uint32 damage, uint32 spell, Unit* caster);
        void RebuildGameObject(uint32 spell, Unit* caster);
        void ForceGameObjectHealth(int32 diff, Unit* caster);
        uint32 GetHealth() const { return m_useTimes; }
        uint32 GetMaxHealth() const { return m_goInfo->destructibleBuilding.intactNumHits + m_goInfo->destructibleBuilding.damagedNumHits; }

        bool IsVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const override;

        bool IsCollisionEnabled() const;                    // Check if a go should collide. Like if a door is closed

        GameObject* LookupFishingHoleAround(float range);

        void SetCapturePointSlider(float value, bool isLocked);
        float GetCapturePointSliderValue() const { return m_captureSlider; }

        float GetInteractionDistance();

        uint32 GetScriptId();

        bool AIM_Initialize();

        GameObjectAI* AI() const { return m_AI.get(); }

        GridReference<GameObject>& GetGridRef() { return m_gridRef; }

        GameObjectModel* m_model;

        // Event Handler
        EventProcessor m_Events;

    protected:
        uint32      m_spellId;
        time_t      m_respawnTime;                          // (secs) time of next respawn (or despawn if GO have owner()),
        uint32      m_respawnDelayTime;                     // (secs) if 0 then current GO state no dependent from timer
        LootState   m_lootState;
        bool        m_spawnedByDefault;
        time_t      m_cooldownTime;                         // used as internal reaction delay time store (not state change reaction).
        // For traps/goober this: spell casting cooldown, for doors/buttons: reset time.

        uint32      m_captureTimer;                         // (msecs) timer used for capture points
        float       m_captureSlider;                        // capture point slider value in range of [0..100]
        CapturePointState m_captureState;

        GuidSet m_SkillupSet;                               // players that already have skill-up at GO use

        uint32 m_useTimes;                                  // amount uses/charges triggered - also used for health for DESTRUCTIBLE_BUILDING

        // collected only for GAMEOBJECT_TYPE_SUMMONING_RITUAL
        ObjectGuid m_firstUser;                             // first GO user, in most used cases owner, but in some cases no, for example non-summoned multi-use GAMEOBJECT_TYPE_SUMMONING_RITUAL
        GuidSet m_UniqueUsers;                              // all players who use item, some items activated after specific amount unique uses

        GameObjectInfo const* m_goInfo;
        GameObjectDisplayInfoEntry const* m_displayInfo;
        int64 m_packedRotation;
        QuaternionData m_worldRotation;

        // Loot System
        uint32 m_groupLootTimer;                            // (msecs)timer used for group loot
        uint32 m_groupLootId;                               // used to find group which is looting
        void StopGroupLoot() override;
        ObjectGuid m_lootRecipientGuid;                     // player who will have rights for looting if m_lootGroupRecipient==0 or group disbanded
        uint32 m_lootGroupRecipientId;                      // group who will have rights for looting if set and exist

                 // Used for trap type
        time_t m_rearmTimer;                                // timer to rearm the trap once disarmed

        // Used for chest type
        bool m_isInUse;                                     // only one player at time are allowed to open chest
        time_t m_reStockTimer;                              // timer to refill the chest
        time_t m_despawnTimer;                              // timer to despawn the chest if something changed in it
        bool m_AI_locked;

        std::unique_ptr<GameObjectAI> m_AI;

    private:
        void SwitchDoorOrButton(bool activate, bool alternative = false);
        void TickCapturePoint();
        void UpdateModel();                                 // updates model in case displayId were changed
        void UpdateCollisionState() const;                  // updates state in Map's dynamic collision tree

        GridReference<GameObject> m_gridRef;
};

#endif
