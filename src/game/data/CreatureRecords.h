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

#ifndef MANGOS_H_CREATURERECORDS
#define MANGOS_H_CREATURERECORDS

#include <string>
#include <unordered_map>
#include <vector>

#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "DBCEnums.h"

/**
 * @brief Creature extra flags enumeration
 *
 * Additional flags that modify creature behavior.
 */
enum CreatureFlagsExtra
{
    CREATURE_FLAG_EXTRA_INSTANCE_BIND = 0x00000001,          ///< Creature kill binds instance with killer and killer's group
    CREATURE_FLAG_EXTRA_CIVILIAN = 0x00000002,               ///< Not aggro (ignore faction/reputation hostility)
    CREATURE_FLAG_EXTRA_NO_PARRY = 0x00000004,               ///< Creature can't parry
    CREATURE_FLAG_EXTRA_NO_PARRY_HASTEN = 0x00000008,        ///< Creature can't counter-attack at parry
    CREATURE_FLAG_EXTRA_NO_BLOCK = 0x00000010,               ///< Creature can't block
    CREATURE_FLAG_EXTRA_NO_CRUSH = 0x00000020,               ///< Creature can't do crush attacks
    CREATURE_FLAG_EXTRA_NO_XP_AT_KILL = 0x00000040,          ///< Creature kill doesn't provide XP
    CREATURE_FLAG_EXTRA_INVISIBLE = 0x00000080,              ///< Creature is always invisible for player (mostly trigger creatures)
    CREATURE_FLAG_EXTRA_NOT_TAUNTABLE = 0x00000100,          ///< Creature is immune to taunt auras and effect attack me
    CREATURE_FLAG_EXTRA_AGGRO_ZONE = 0x00000200,             ///< Creature sets itself in combat with zone on aggro
    CREATURE_FLAG_EXTRA_GUARD = 0x00000400,                  ///< Creature is a guard
    CREATURE_FLAG_EXTRA_NO_CALL_ASSIST = 0x00000800,         ///< Creature shouldn't call for assistance on aggro
    CREATURE_FLAG_EXTRA_ACTIVE = 0x00001000,                 ///< Creature is active object (grid will be loaded and creature set as active)
    CREATURE_FLAG_EXTRA_MMAP_FORCE_ENABLE = 0x00002000,      ///< creature is forced to use MMaps
    CREATURE_FLAG_EXTRA_MMAP_FORCE_DISABLE = 0x00004000,     ///< creature is forced to NOT use MMaps
    CREATURE_FLAG_EXTRA_WALK_IN_WATER = 0x00008000,          ///< creature is forced to walk in water even it can swim
    CREATURE_FLAG_EXTRA_HAVE_NO_SWIM_ANIMATION = 0x00010000, ///< we have to not set "swim" animation or creature will have "no animation"
    CREATURE_FLAG_EXTRA_NO_MELEE = 0x00020000,               ///< creature can't melee
    CREATURE_FLAG_EXTRA_UNKILLABLE = 0x00040000,             ///< creature cannot be killed by external damage; damage that would kill is clamped to leave it at 1 HP (e.g. training dummies)
};

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

#define MAX_KILL_CREDIT 2
#define MAX_CREATURE_MODEL 4                                // only single send to client in static data

/**
 * creature_template.MovementType / creature.MovementType: what a creature does by default. The
 * DB's vocabulary, not the kernel's; MotionMaster::Initialize maps it to a Motion::Kind (idle,
 * a wander around the spawn, the default waypoint path).
 */
enum CreatureMovementType : uint8
{
    CREATURE_MOVEMENT_IDLE     = 0,
    CREATURE_MOVEMENT_RANDOM   = 1,
    CREATURE_MOVEMENT_WAYPOINT = 2,
    CREATURE_MOVEMENT_MAX      = 3   ///< the bound the loaders check the DB value against
};

/**
 * @brief Creature information structure
 *
 * Data from `creature_template` table.
 */
struct CreatureInfo
{
    uint32 Entry;                                             ///< Creature entry ID
    char* Name;                                               ///< Creature name
    char* SubName;                                            ///< Creature sub-name
    char* IconName;
    uint32 MinLevel;                                          ///< Minimum level
    uint32 MaxLevel;                                          ///< Maximum level
    uint32 DifficultyEntry[MAX_DIFFICULTY - 1];
    uint32 ModelId[MAX_CREATURE_MODEL];                       ///< Model IDs
    uint32 FactionAlliance;                                   ///< Alliance faction
    uint32 FactionHorde;                                      ///< Horde faction
    float Scale;                                              ///< Scale factor
    uint32 Family;                                            ///< Creature family (enum CreatureFamily values, optional)
    uint32 CreatureType;                                      ///< Creature type (enum CreatureType values)
    uint32 InhabitType;                                       ///< Inhabit type
    uint32 RegenerateStats;                                   ///< Regenerate stats
    int32 RacialLeader;                                       ///< Is racial leader
    uint32 NpcFlags;                                          ///< NPC flags
    uint32 UnitFlags;                                         ///< Unit flags (enum UnitFlags mask values)
    uint32 UnitFlags2;                                        ///< enum UnitFlags2 mask values
    uint32 DynamicFlags;                                      ///< Dynamic flags
    uint32 ExtraFlags;                                        ///< Extra flags
    uint32 CreatureTypeFlags;                                 ///< Creature type flags (enum CreatureTypeFlags mask values)
    float SpeedWalk;                                          ///< Walk speed
    float SpeedRun;                                           ///< Run speed
    uint32 UnitClass;                                         ///< Unit class (enum Classes, note only 4 classes are known for creatures)
    uint32 Rank;                                              ///< Creature rank
    int32 Expansion;                                          ///< creature expansion, important for stats, CAN BE -1 as marker for some invalid cases.
    float HealthMultiplier;                                   ///< Health multiplier
    float PowerMultiplier;                                    ///< Power multiplier
    float DamageMultiplier;                                   ///< Damage multiplier
    float DamageVariance;                                     ///< Damage variance
    float ArmorMultiplier;                                    ///< Armor multiplier
    float ExperienceMultiplier;                               ///< Experience multiplier
    uint32 MinLevelHealth;
    uint32 MaxLevelHealth;
    uint32 MinLevelMana;
    uint32 MaxLevelMana;
    float MinMeleeDmg;                                        ///< Minimum melee damage
    float MaxMeleeDmg;                                        ///< Maximum melee damage
    float MinRangedDmg;                                       ///< Minimum ranged damage
    float MaxRangedDmg;                                       ///< Maximum ranged damage
    uint32 Armor;                                             ///< Armor value
    uint32 MeleeAttackPower;                                  ///< Melee attack power
    uint32 RangedAttackPower;                                 ///< Ranged attack power
    uint32 MeleeBaseAttackTime;                               ///< Melee attack base time (milliseconds)
    uint32 RangedBaseAttackTime;                              ///< Ranged attack base time (milliseconds)
    uint32 DamageSchool;                                      ///< Primary damage school (enum SpellSchools)
    uint32 MinLootGold;                                       ///< Minimum loot gold dropped
    uint32 MaxLootGold;                                       ///< Maximum loot gold dropped
    uint32 LootId;                                            ///< Loot table ID
    uint32 PickpocketLootId;                                  ///< Pickpocket loot table ID
    uint32 SkinningLootId;                                    ///< Skinning loot table ID
    uint32 KillCredit[MAX_KILL_CREDIT];                       ///< Kill credit IDs for quest tracking
    uint32 QuestItems[6];
    uint32 MechanicImmuneMask;                                ///< Mechanic immunity mask (enum Mechanics)
    int32 ResistanceHoly;                                     ///< Holy resistance
    int32 ResistanceFire;                                     ///< Fire resistance
    int32 ResistanceNature;                                   ///< Nature resistance
    int32 ResistanceFrost;                                    ///< Frost resistance
    int32 ResistanceShadow;                                   ///< Shadow resistance
    int32 ResistanceArcane;                                   ///< Arcane resistance
    uint32 PetSpellDataId;                                    ///< Pet spell data ID
    uint32 MovementType;                                      ///< Movement type (enum CreatureMovementType)
    uint32 MovementTemplateId;
    uint32 TrainerType;                                       ///< Trainer type (enum TrainerType)
    uint32 TrainerSpell;                                      ///< Trainer spell ID
    uint32 TrainerClass;                                      ///< Trainer class (enum Classes)
    uint32 TrainerRace;                                       ///< Trainer race (enum Races)
    uint32 TrainerTemplateId;                                 ///< Trainer template ID
    uint32 VendorTemplateId;                                  ///< Vendor template ID
    uint32 EquipmentTemplateId;                               ///< Equipment template ID
    uint32 VehicleTemplateId;
    uint32 GossipMenuId;                                      ///< Gossip menu ID
    char const* AIName;                                       ///< Custom AI name for special behavior

    /// @brief Helper methods for CreatureInfo

    /// @brief Get the high GUID type for creatures.
    /// @return Always returns HIGHGUID_UNIT for creatures
    HighGuid GetHighGuid() const
    {
        return VehicleTemplateId ? HIGHGUID_VEHICLE : HIGHGUID_UNIT;
    }

    /// @brief Create a full ObjectGuid for this creature template.
    /// @param lowguid The low GUID (unique creature instance identifier)
    /// @return ObjectGuid combining creature entry and low GUID
    ObjectGuid GetObjectGuid(uint32 lowguid) const { return ObjectGuid(GetHighGuid(), Entry, lowguid); }

    /// @brief Determine the required skill to loot this creature.
    /// @return SkillType for herbalism, mining, or skinning based on creature type flags
    SkillType GetRequiredLootSkill() const
    {
        if (CreatureTypeFlags & CREATURE_TYPEFLAGS_HERBLOOT)
        {
            return SKILL_HERBALISM;
        }
        else if (CreatureTypeFlags & CREATURE_TYPEFLAGS_MININGLOOT)
        {
            return SKILL_MINING;
        }
        else if (CreatureTypeFlags & CREATURE_TYPEFLAGS_ENGINEERLOOT)
        {
            return SKILL_ENGINEERING;
        }
        else
        {
            return SKILL_SKINNING;                           // normal case
        }
    }

    /// @brief Check if this creature can be tamed as a pet.
    /// @return True if creature is tameable beast with valid family, false otherwise
    bool IsExotic() const
    {
        return (CreatureTypeFlags & CREATURE_TYPEFLAGS_EXOTIC);
    }

    bool isTameable(bool exotic) const
    {
        if (CreatureType != CREATURE_TYPE_BEAST || Family == 0 || (CreatureTypeFlags & CREATURE_TYPEFLAGS_TAMEABLE) == 0)
        {
            return false;
        }

        // if can tame exotic then can tame any temable
        return exotic || !IsExotic();
    }
};

/// @brief Creature spell list structure.
///
/// Stores spell IDs for creatures to cast during combat or special events.
struct CreatureTemplateSpells
{
    uint32 entry;                          ///> Creature entry ID
    uint32 spells[CREATURE_MAX_SPELLS];    ///> Spell IDs creature can cast (up to CREATURE_MAX_SPELLS)
};

// from `creature` table
struct CreatureData
{
    uint32 id;                                              // entry in creature_template
    uint32 mapid;
    uint32 phaseMask;
    uint32 modelid_override;                                // overrides any model defined in creature_template
    int32 equipmentId;
    float posX;
    float posY;
    float posZ;
    float orientation;
    uint32 spawntimesecs;
    float spawndist;
    uint32 currentwaypoint;
    uint32 curhealth;
    uint32 curmana;
    bool  is_dead;
    uint8 movementType;
    uint8 spawnMask;

    // helper function
    ObjectGuid GetObjectGuid(uint32 lowguid) const;
};

// Bases values for given Level and UnitClass
struct CreatureClassLvlStats
{
    uint32  BaseHealth;
    uint32  BaseMana;
    float   BaseDamage;
    float   BaseMeleeAttackPower;
    float   BaseRangedAttackPower;
    uint32  BaseArmor;
};

struct CreatureModelRace
{
    uint32 modelid;                                         // Native model/base model the selection is for
    uint32 racemask;                                        // Races it applies to (and then a player source must exist for selection)
    uint32 creature_entry;                                  // Modelid from creature_template.entry will be selected
    uint32 modelid_racial;                                  // Explicit modelid. Used if creature_template entry is not defined
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

struct CreatureLocale
{
    std::vector<std::string> Name;
    std::vector<std::string> SubName;
};

struct GossipMenuItemsLocale
{
    std::vector<std::string> OptionText;
    std::vector<std::string> BoxText;
};

struct PointOfInterestLocale
{
    std::vector<std::string> IconName;
};

enum InhabitTypeValues
{
    INHABIT_GROUND = 1,
    INHABIT_WATER  = 2,
    INHABIT_AIR    = 4,
    INHABIT_ANYWHERE = INHABIT_GROUND | INHABIT_WATER | INHABIT_AIR
};

enum RegenStatsFlags
{
    REGEN_FLAG_HEALTH               = 0x001,
    REGEN_FLAG_POWER                = 0x002,
};

// Vendors

enum
{
    VENDOR_ITEM_TYPE_NONE           = 0,
    VENDOR_ITEM_TYPE_ITEM           = 1,
    VENDOR_ITEM_TYPE_CURRENCY       = 2,
    VENDOR_ITEM_TYPE_MAX            = 3,
};

// Vendors
struct VendorItem
{
    VendorItem(uint32 _item, uint8 _type, uint32 _maxcount, uint32 _incrtime, uint32 _ExtendedCost, uint16 _conditionId)
        : item(_item), type(_type), maxcount(_maxcount), incrtime(_incrtime), ExtendedCost(_ExtendedCost), conditionId(_conditionId) {}

    uint32 item;
    uint8  type;
    uint32 maxcount;                                        // 0 for infinity item amount, for type = VENDOR_ITEM_TYPE_CURRENCY, maxcount = currency count
    uint32 incrtime;                                        // time for restore items amount if maxcount != 0
    uint32 ExtendedCost;                                    // index in ItemExtendedCost.dbc
    uint16 conditionId;                                     // condition to check for this item
    bool IsCurrency() const { return type == VENDOR_ITEM_TYPE_CURRENCY; }
};
typedef std::vector<VendorItem*> VendorItemList;

struct VendorItemData
{
    VendorItemList m_items;

    VendorItem* GetItem(uint32 slot) const
    {
        if (slot >= m_items.size())
        {
            return NULL;
        }
        return m_items[slot];
    }
    bool Empty() const { return m_items.empty(); }
    uint8 GetItemCount() const { return m_items.size(); }
    void AddItem(uint32 item, uint8 type, uint32 maxcount, uint32 ptime, uint32 ExtendedCost, uint16 conditonId);
    bool RemoveItem(uint32 item_id, uint8 type);
    VendorItem const* FindItemCostPair(uint32 item_id, uint8 type, uint32 extendedCost) const;
    VendorItem const* FindItem(uint32 item_id) const;

    void Clear()
    {
        for (VendorItemList::const_iterator itr = m_items.begin(); itr != m_items.end(); ++itr)
        {
            delete(*itr);
        }
        m_items.clear();
    }
};

struct TrainerSpell
{
    TrainerSpell() : spell(0), spellCost(0), reqSkill(0), reqSkillValue(0), reqLevel(0), learnedSpell(0), isProvidedReqLevel(false) {}

    TrainerSpell(uint32 _spell, uint32 _spellCost, uint32 _reqSkill, uint32 _reqSkillValue, uint32 _reqLevel, uint32 _learnedspell, bool _isProvidedReqLevel)
        : spell(_spell), spellCost(_spellCost), reqSkill(_reqSkill), reqSkillValue(_reqSkillValue), reqLevel(_reqLevel), learnedSpell(_learnedspell), isProvidedReqLevel(_isProvidedReqLevel)
    {}

    uint32 spell;
    uint32 spellCost;
    uint32 reqSkill;
    uint32 reqSkillValue;
    uint32 reqLevel;
    uint32 learnedSpell;
    bool isProvidedReqLevel;

    // helpers
    bool IsCastable() const { return learnedSpell != spell; }
};

typedef std::unordered_map < uint32 /*spellid*/, TrainerSpell > TrainerSpellMap;

struct TrainerSpellData
{
    TrainerSpellData() : trainerType(0) {}

    TrainerSpellMap spellList;
    uint32 trainerType;                                     // trainer type based at trainer spells, can be different from creature_template value.
    // req. for correct show non-prof. trainers like weaponmaster, allowed values 0 and 2.
    TrainerSpell const* Find(uint32 spell_id) const;
    void Clear() { spellList.clear(); }
};

#define MAX_VENDOR_ITEMS 300                                // Limitation in 4.x.x item count in SMSG_LIST_INVENTORY

#endif
