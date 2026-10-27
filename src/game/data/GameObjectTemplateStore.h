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

#ifndef MANGOS_H_GAMEOBJECTTEMPLATESTORE
#define MANGOS_H_GAMEOBJECTTEMPLATESTORE

#include "Common/TimeConstants.h"
#include "Platform/Define.h"
#include "SharedDefines.h"

#include <map>
#include <memory>
#include <string>

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

/// @brief Game object template structure.
///
/// Contains static configuration data for all game object instances loaded from
/// the `gameobject_template` DBC and database. Each game object type has different
/// data requirements, stored in the `data` union with type-specific nested structures.
///
/// The data union is indexed by the `type` field and contains configuration for:
/// - DOOR: Door mechanics (locks, auto-close)
/// - BUTTON: Buttons with linked traps
/// - QUESTGIVER: Quest and gossip NPCs (game objects)
/// - CHEST: Treasure and loot containers
/// - GENERIC: Generic interactive objects
/// - TRAP: Harmful triggered areas
/// - CHAIR: Sitting objects with slots
/// - SPELL_FOCUS: Focus objects for spellcasting
/// - TEXT: Readable text objects
/// - GOOBER: Complex interactive objects
/// - TRANSPORT: Fixed transport (elevators, etc.)
/// - AREADAMAGE: Damage zones
/// - CAMERA: Camera trigger points
/// - MAPOBJECT: Generic map objects
/// - MO_TRANSPORT: Moving transport (ships, zeppelins)
/// - DUELFLAG: Duel flag placement
/// - FISHINGNODE: Fishing pool
/// - SUMMONING_RITUAL: Summoning circle
/// - MAILBOX: Mail access point
/// - AUCTIONHOUSE: Auction house access
/// - GUARDPOST: Guard post structures
/// - SPELLCASTER: Spell casting object
/// - MEETINGSTONE: Dungeon meeting stones
///
/// @note Template data is read from DBC at server startup and is static
struct GameObjectInfo
{
    uint32  id;          ///> Game object entry ID (unique identifier)
    uint32  type;        ///> Game object type (enum GameObjectTypes)
    uint32  displayId;   ///> Display model ID
    char*   name;        ///> Game object name
    char*   IconName;
    char*   castBarCaption;
    char*   unk1;
    uint32  faction;     ///> Faction ID (determines PvP rules and attacking)
    uint32  flags;       ///> Flags (enum GameObjectFlags)
    float   size;        ///> Display scale factor
    uint32  questItems[6];
    union                                                   // different GO types have different data field
    {
        // 0 GAMEOBJECT_TYPE_DOOR
        struct
        {
            uint32 startOpen;                               // 0 used client side to determine GO_ACTIVATED means open/closed
            uint32 lockId;                                  // 1 -> Lock.dbc
            uint32 autoCloseTime;                           //2 secs till autoclose = autoCloseTime / IN_MILLISECONDS (previous was 0x10000)
            uint32 noDamageImmune;                          // 3 break opening whenever you recieve damage?
            uint32 openTextID;                              // 4 can be used to replace castBarCaption?
            uint32 closeTextID;                             // 5
            uint32 ignoredByPathing;                        //6
        } door;
        // 1 GAMEOBJECT_TYPE_BUTTON
        struct
        {
            uint32 startOpen;                               // 0
            uint32 lockId;                                  // 1 -> Lock.dbc
            uint32 autoCloseTime;                           //2 secs till autoclose = autoCloseTime / IN_MILLISECONDS (previous was 0x10000)
            uint32 linkedTrapId;                            // 3
            uint32 noDamageImmune;                          // 4 isBattlegroundObject
            uint32 large;                                   // 5
            uint32 openTextID;                              // 6 can be used to replace castBarCaption?
            uint32 closeTextID;                             // 7
            uint32 losOK;                                   // 8
        } button;
        // 2 GAMEOBJECT_TYPE_QUESTGIVER
        struct
        {
            uint32 lockId;                                  // 0 -> Lock.dbc
            uint32 questList;                               // 1
            uint32 pageMaterial;                            // 2
            uint32 gossipID;                                // 3
            uint32 customAnim;                              // 4
            uint32 noDamageImmune;                          // 5
            uint32 openTextID;                              // 6 can be used to replace castBarCaption?
            uint32 losOK;                                   // 7
            uint32 allowMounted;                            // 8
            uint32 large;                                   // 9
        } questgiver;
        // 3 GAMEOBJECT_TYPE_CHEST
        struct
        {
            uint32 lockId;                                  // 0 -> Lock.dbc
            uint32 lootId;                                  // 1
            uint32 chestRestockTime;                        // 2
            uint32 consumable;                              // 3
            uint32 minSuccessOpens;                         // 4
            uint32 maxSuccessOpens;                         // 5
            uint32 eventId;                                 // 6 lootedEvent
            uint32 linkedTrapId;                            // 7
            uint32 questId;                                 // 8 not used currently but store quest required for GO activation for player
            uint32 level;                                   // 9
            uint32 losOK;                                   // 10
            uint32 leaveLoot;                               // 11
            uint32 notInCombat;                             // 12
            uint32 logLoot;                                 // 13
            uint32 openTextID;                              // 14 can be used to replace castBarCaption?
            uint32 groupLootRules;                          // 15
            uint32 floatingTooltip;                         //16
        } chest;
        // 4 GAMEOBJECT_TYPE_BINDER - empty
        // 5 GAMEOBJECT_TYPE_GENERIC
        struct
        {
            uint32 floatingTooltip;                         // 0
            uint32 highlight;                               // 1
            uint32 serverOnly;                              // 2
            uint32 large;                                   // 3
            uint32 floatOnWater;                            // 4
            uint32 questID;                                 // 5
        } _generic;
        // 6 GAMEOBJECT_TYPE_TRAP
        struct
        {
            uint32 lockId;                                  // 0 -> Lock.dbc
            uint32 level;                                   // 1
            uint32 radius;                                  // 2 radius for trap activation
            uint32 spellId;                                 // 3
            uint32 charges;                                 // 4 need respawn (if > 0)
            uint32 cooldown;                                // 5 time in secs
            uint32 autoCloseTime;                           //6 secs till autoclose = autoCloseTime / IN_MILLISECONDS (previous was 0x10000)
            uint32 startDelay;                              // 7
            uint32 serverOnly;                              // 8
            uint32 stealthed;                               // 9
            uint32 large;                                   // 10
            uint32 stealthAffected;                         // 11
            uint32 openTextID;                              // 12 can be used to replace castBarCaption?
            uint32 closeTextID;                             // 13
            uint32 ignoreTotems;                            //14
        } trap;
        // 7 GAMEOBJECT_TYPE_CHAIR
        struct
        {
            uint32 slots;                                   // 0
            uint32 height;                                  // 1
            uint32 onlyCreatorUse;                          // 2
            uint32 triggeredEvent;                          //3
        } chair;
        // 8 GAMEOBJECT_TYPE_SPELL_FOCUS
        struct
        {
            uint32 focusId;                                 // 0
            uint32 dist;                                    // 1
            uint32 linkedTrapId;                            // 2
            uint32 serverOnly;                              // 3
            uint32 questID;                                 // 4
            uint32 large;                                   // 5
            uint32 floatingTooltip;                         //6
        } spellFocus;
        // 9 GAMEOBJECT_TYPE_TEXT
        struct
        {
            uint32 pageID;                                  // 0
            uint32 language;                                // 1
            uint32 pageMaterial;                            // 2
            uint32 allowMounted;                            // 3
        } text;
        // 10 GAMEOBJECT_TYPE_GOOBER
        struct
        {
            uint32 lockId;                                  // 0 -> Lock.dbc
            uint32 questId;                                 // 1
            uint32 eventId;                                 // 2
            uint32 autoCloseTime;                           //3 secs till autoclose = autoCloseTime / IN_MILLISECONDS (previous was 0x10000)
            uint32 customAnim;                              // 4
            uint32 consumable;                              // 5
            uint32 cooldown;                                // 6
            uint32 pageId;                                  // 7
            uint32 language;                                // 8
            uint32 pageMaterial;                            // 9
            uint32 spellId;                                 // 10
            uint32 noDamageImmune;                          // 11
            uint32 linkedTrapId;                            // 12
            uint32 large;                                   // 13
            uint32 openTextID;                              // 14 can be used to replace castBarCaption?
            uint32 closeTextID;                             // 15
            uint32 losOK;                                   // 16 isBattlegroundObject
            uint32 allowMounted;                            // 17
            uint32 floatingTooltip;                         // 18
            uint32 gossipID;                                // 19
            uint32 WorldStateSetsState;                     //20
        } goober;
        // 11 GAMEOBJECT_TYPE_TRANSPORT
        struct
        {
            uint32 startFrame;                              //0
            uint32 startOpen;                               // 1
            uint32 autoCloseTime;                           //2 secs till autoclose = autoCloseTime / IN_MILLISECONDS (previous was 0x10000)
            uint32 pause1EventID;                           //3
            uint32 pause2EventID;                           //4
            uint32 baseMap;                                 //5
            uint32 nextFrame1;                              //6
            uint32 unk7;                                    //7
            uint32 nextFrame2;                              //8
            uint32 unk9;                                    //9
            uint32 nextFrame3;                              //10
            uint32 unk11;                                   //11
            uint32 unk12;                                   //12
            uint32 unk13;                                   //13
            uint32 unk14;                                   //14
            uint32 unk15;                                   //15
            uint32 unk16;                                   //16
            uint32 unk17;                                   //17
            uint32 unk18;                                   //18
            uint32 unk19;                                   //19
            uint32 unk20;                                   //20
            uint32 unk21;                                   //21
            uint32 unk22;                                   //22 ring of valor elevators
            uint32 unk23;                                   //23 ring of valor elevators
        } transport;
        // 12 GAMEOBJECT_TYPE_AREADAMAGE
        struct
        {
            uint32 lockId;                                  // 0
            uint32 radius;                                  // 1
            uint32 damageMin;                               // 2
            uint32 damageMax;                               // 3
            uint32 damageSchool;                            // 4
            uint32 autoCloseTime;                           //5 secs till autoclose = autoCloseTime / IN_MILLISECONDS (previous was 0x10000)
            uint32 openTextID;                              // 6
            uint32 closeTextID;                             // 7
        } areadamage;
        // 13 GAMEOBJECT_TYPE_CAMERA
        struct
        {
            uint32 lockId;                                  // 0 -> Lock.dbc
            uint32 cinematicId;                             // 1
            uint32 eventID;                                 // 2
            uint32 openTextID;                              // 3 can be used to replace castBarCaption?
        } camera;
        // 14 GAMEOBJECT_TYPE_MAPOBJECT - empty
        // 15 GAMEOBJECT_TYPE_MO_TRANSPORT
        struct
        {
            uint32 taxiPathId;                              // 0
            uint32 moveSpeed;                               // 1
            uint32 accelRate;                               // 2
            uint32 startEventID;                            // 3
            uint32 stopEventID;                             // 4
            uint32 transportPhysics;                        // 5
            uint32 mapID;                                   // 6
            uint32 worldState1;                             //7
        } moTransport;
        // 16 GAMEOBJECT_TYPE_DUELFLAG - empty
        // 17 GAMEOBJECT_TYPE_FISHINGNODE - empty
        // 18 GAMEOBJECT_TYPE_SUMMONING_RITUAL
        struct
        {
            uint32 reqParticipants;                         // 0
            uint32 spellId;                                 // 1
            uint32 animSpell;                               // 2
            uint32 ritualPersistent;                        // 3
            uint32 casterTargetSpell;                       // 4
            uint32 casterTargetSpellTargets;                // 5
            uint32 castersGrouped;                          // 6
            uint32 ritualNoTargetCheck;                     // 7
        } summoningRitual;
        // 19 GAMEOBJECT_TYPE_MAILBOX - empty
        //20 GAMEOBJECT_TYPE_DONOTUSE - empty
        // 21 GAMEOBJECT_TYPE_GUARDPOST
        struct
        {
            uint32 creatureID;                              // 0
            uint32 charges;                                 // 1
        } guardpost;
        // 22 GAMEOBJECT_TYPE_SPELLCASTER
        struct
        {
            uint32 spellId;                                 // 0
            uint32 charges;                                 // 1
            uint32 partyOnly;                               // 2
            uint32 allowMounted;                            //3
            uint32 large;                                   //4
        } spellcaster;
        // 23 GAMEOBJECT_TYPE_MEETINGSTONE
        struct
        {
            uint32 minLevel;                                // 0
            uint32 maxLevel;                                // 1
            uint32 areaID;                                  // 2
        } meetingstone;
        // 24 GAMEOBJECT_TYPE_FLAGSTAND
        struct
        {
            uint32 lockId;                                  // 0
            uint32 pickupSpell;                             // 1
            uint32 radius;                                  // 2
            uint32 returnAura;                              // 3
            uint32 returnSpell;                             // 4
            uint32 noDamageImmune;                          // 5
            uint32 openTextID;                              // 6
            uint32 losOK;                                   // 7
        } flagstand;
        // 25 GAMEOBJECT_TYPE_FISHINGHOLE
        struct
        {
            uint32 radius;                                  // 0 how close bobber must land for sending loot
            uint32 lootId;                                  // 1
            uint32 minSuccessOpens;                         // 2
            uint32 maxSuccessOpens;                         // 3
            uint32 lockId;                                  // 4 -> Lock.dbc; possibly 1628 for all?
        } fishinghole;
        // 26 GAMEOBJECT_TYPE_FLAGDROP
        struct
        {
            uint32 lockId;                                  // 0
            uint32 eventID;                                 // 1
            uint32 pickupSpell;                             // 2
            uint32 noDamageImmune;                          // 3
            uint32 openTextID;                              // 4
        } flagdrop;
        // 27 GAMEOBJECT_TYPE_MINI_GAME
        struct
        {
            uint32 gameType;                                // 0
        } miniGame;
        // 29 GAMEOBJECT_TYPE_CAPTURE_POINT
        struct
        {
            uint32 radius;                                  // 0
            uint32 spell;                                   // 1
            uint32 worldState1;                             // 2
            uint32 worldState2;                             // 3
            uint32 winEventID1;                             // 4
            uint32 winEventID2;                             // 5
            uint32 contestedEventID1;                       // 6
            uint32 contestedEventID2;                       // 7
            uint32 progressEventID1;                        // 8
            uint32 progressEventID2;                        // 9
            uint32 neutralEventID1;                         // 10
            uint32 neutralEventID2;                         // 11
            uint32 neutralPercent;                          // 12
            uint32 worldState3;                             // 13
            uint32 minSuperiority;                          // 14
            uint32 maxSuperiority;                          // 15
            uint32 minTime;                                 // 16
            uint32 maxTime;                                 // 17
            uint32 large;                                   // 18
            uint32 highlight;                               // 19
            uint32 startingValue;                           //20
            uint32 unidirectional;                          //21
        } capturePoint;
        // 30 GAMEOBJECT_TYPE_AURA_GENERATOR
        struct
        {
            uint32 startOpen;                               // 0
            uint32 radius;                                  // 1
            uint32 auraID1;                                 // 2
            uint32 conditionID1;                            // 3
            uint32 auraID2;                                 // 4
            uint32 conditionID2;                            // 5
            uint32 serverOnly;                              // 6
        } auraGenerator;
        //31 GAMEOBJECT_TYPE_DUNGEON_DIFFICULTY
        struct
        {
            uint32 mapID;                                   //0
            uint32 difficulty;                              //1
        } dungeonDifficulty;
        //32 GAMEOBJECT_TYPE_BARBER_CHAIR
        struct
        {
            uint32 chairheight;                             //0
            uint32 heightOffset;                            //1
        } barberChair;
        //33 GAMEOBJECT_TYPE_DESTRUCTIBLE_BUILDING          // Much guesswork
        struct
        {
            uint32 intactNumHits;                           //0
            uint32 creditProxyCreature;                     //1
            uint32 empty1;                                  //2
            uint32 intactEvent;                             //3
            uint32 damagedDisplayId;                        //4
            uint32 damagedNumHits;                          //5
            uint32 unk1;                                    //6
            uint32 unk2;                                    //7
            uint32 unk3;                                    //8
            uint32 damagedEvent;                            //9
            uint32 destroyedDisplayId;                      //10
            uint32 unk4;                                    //11
            uint32 unk5;                                    //12
            uint32 unk6;                                    //13
            uint32 destroyedEvent;                          //14
            uint32 empty10;                                 //15
            uint32 debuildingTimeSecs;                      //16 // unk, only few with value 300)
            uint32 empty11;                                 //17
            uint32 destructibleData;                        //18 m_ID of DestructibleModelData.DBC
            uint32 empty12;                                 //19
            uint32 unk7;                                    //20
            uint32 empty13;                                 //21
            uint32 rebuildingEvent;                         //22
            uint32 unk8;                                    //23
        } destructibleBuilding;
        //34 GAMEOBJECT_TYPE_GUILDBANK - empty
        //35 GAMEOBJECT_TYPE_TRAPDOOR
        struct
        {
            uint32 whenToPause;                             // 0
            uint32 startOpen;                               // 1
            uint32 autoClose;                               // 2
        } trapDoor;

        // not use for specific field access (only for output with loop by all filed), also this determinate max union size
        struct
        {
            uint32 data[32];
        } raw;
    };

    uint32 unk2;
    uint32 MinMoneyLoot;
    uint32 MaxMoneyLoot;

    // helpers
    bool IsDespawnAtAction() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_CHEST:  return chest.consumable;
            case GAMEOBJECT_TYPE_GOOBER: return goober.consumable;
            default: return false;
        }
    }

    uint32 GetLockId() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_DOOR:       return door.lockId;
            case GAMEOBJECT_TYPE_BUTTON:     return button.lockId;
            case GAMEOBJECT_TYPE_QUESTGIVER: return questgiver.lockId;
            case GAMEOBJECT_TYPE_CHEST:      return chest.lockId;
            case GAMEOBJECT_TYPE_TRAP:       return trap.lockId;
            case GAMEOBJECT_TYPE_GOOBER:     return goober.lockId;
            case GAMEOBJECT_TYPE_AREADAMAGE: return areadamage.lockId;
            case GAMEOBJECT_TYPE_CAMERA:     return camera.lockId;
            case GAMEOBJECT_TYPE_FLAGSTAND:  return flagstand.lockId;
            case GAMEOBJECT_TYPE_FISHINGHOLE: return fishinghole.lockId;
            case GAMEOBJECT_TYPE_FLAGDROP:   return flagdrop.lockId;
            default: return 0;
        }
    }

    bool GetDespawnPossibility() const                      // despawn at targeting of cast?
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_DOOR:       return door.noDamageImmune;
            case GAMEOBJECT_TYPE_BUTTON:     return button.noDamageImmune;
            case GAMEOBJECT_TYPE_QUESTGIVER: return questgiver.noDamageImmune;
            case GAMEOBJECT_TYPE_GOOBER:     return goober.noDamageImmune;
            case GAMEOBJECT_TYPE_FLAGSTAND:  return flagstand.noDamageImmune;
            case GAMEOBJECT_TYPE_FLAGDROP:   return flagdrop.noDamageImmune;
            default: return true;
        }
    }

    uint32 GetCharges() const                               // despawn at uses amount
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_TRAP:        return trap.charges;
            case GAMEOBJECT_TYPE_GUARDPOST:   return guardpost.charges;
            case GAMEOBJECT_TYPE_SPELLCASTER: return spellcaster.charges;
            default: return 0;
        }
    }

    uint32 GetCooldown() const                              // not triggering at detection target or use until coolodwn expire
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_TRAP:        return trap.cooldown;
            case GAMEOBJECT_TYPE_GOOBER:      return goober.cooldown;
            default: return 0;
        }
    }

    uint32 GetLinkedGameObjectEntry() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_BUTTON:      return button.linkedTrapId;
            case GAMEOBJECT_TYPE_CHEST:       return chest.linkedTrapId;
            case GAMEOBJECT_TYPE_SPELL_FOCUS: return spellFocus.linkedTrapId;
            case GAMEOBJECT_TYPE_GOOBER:      return goober.linkedTrapId;
            default: return 0;
        }
    }

    uint32 GetAutoCloseTime() const
    {
        uint32 autoCloseTime = 0;
        switch (type)
        {
            case GAMEOBJECT_TYPE_DOOR:          autoCloseTime = door.autoCloseTime; break;
            case GAMEOBJECT_TYPE_BUTTON:        autoCloseTime = button.autoCloseTime; break;
            case GAMEOBJECT_TYPE_TRAP:          autoCloseTime = trap.autoCloseTime; break;
            case GAMEOBJECT_TYPE_GOOBER:        autoCloseTime = goober.autoCloseTime; break;
            case GAMEOBJECT_TYPE_TRANSPORT:     autoCloseTime = transport.autoCloseTime; break;
            case GAMEOBJECT_TYPE_AREADAMAGE:    autoCloseTime = areadamage.autoCloseTime; break;
            default: break;
        }
        return autoCloseTime / IN_MILLISECONDS;             // prior to 3.0.3, conversion was / 0x10000;
    }

    uint32 GetLootId() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_CHEST:       return chest.lootId;
            case GAMEOBJECT_TYPE_FISHINGHOLE: return fishinghole.lootId;
            default: return 0;
        }
    }

    uint32 GetGossipMenuId() const
    {
        switch (type)
        {
            case GAMEOBJECT_TYPE_QUESTGIVER:    return questgiver.gossipID;
            case GAMEOBJECT_TYPE_GOOBER:        return goober.gossipID;
            default: return 0;
        }
    }
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

/**
 * The game object templates (`gameobject_template`), one per entry. The store owns each template's
 * name, icon name, cast bar caption and `unk1` text. Setting an entry that already holds a template
 * overwrites it, an entry with no template answers NULL, clearing forgets every entry, and the
 * templates are walked in ascending entry order.
 */
class GameObjectTemplateStore
{
    private:
        struct Entry
        {
            GameObjectInfo info;
            std::unique_ptr<char[]> name;
            std::unique_ptr<char[]> iconName;
            std::unique_ptr<char[]> castBarCaption;
            std::unique_ptr<char[]> unk1;
        };

        typedef std::map<uint32, Entry> TemplateMap;

    public:
        /// Walks the templates in ascending entry order; `->` and `*` read the template.
        class const_iterator
        {
            public:
                explicit const_iterator(TemplateMap::const_iterator itr) : m_itr(itr) {}

                GameObjectInfo const& operator*() const { return m_itr->second.info; }
                GameObjectInfo const* operator->() const { return &m_itr->second.info; }
                const_iterator& operator++()
                {
                    ++m_itr;
                    return *this;
                }
                bool operator==(const_iterator const& other) const { return m_itr == other.m_itr; }
                bool operator!=(const_iterator const& other) const { return m_itr != other.m_itr; }

            private:
                TemplateMap::const_iterator m_itr;
        };

        void Clear();
        /// Holds `info` under its entry with the four texts, copied; `info`'s own text pointers are not read.
        void Set(GameObjectInfo const& info, std::string const& name, std::string const& iconName, std::string const& castBarCaption, std::string const& unk1);
        GameObjectInfo const* Find(uint32 entry) const;
        /// The template `entry` for the loader's checks to change, or NULL.
        GameObjectInfo* FindForEdit(uint32 entry);
        uint32 GetCount() const;
        /// One past the highest entry held, 0 when the store is empty.
        uint32 GetMaxEntry() const;
        const_iterator begin() const;
        const_iterator end() const;

    private:
        TemplateMap m_templates;
};

#endif
