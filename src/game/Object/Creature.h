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
 * @file Creature.h
 * @brief Creature (NPC) class definition and related structures.
 *
 * This file defines the Creature class which represents non-player characters (NPCs)
 * in the game world. It extends the Unit class with creature-specific functionality
 * including:
 * - Creature data storage and initialization
 * - Loot management and drop handling
 * - Movement AI and pathfinding
 * - Respawn and despawn mechanics
 * - Ability and skill management
 * - Faction and reputation systems
 *
 * The file also contains CreatureInfo struct for storing static creature template data
 * from the database, and various creature-related flags and enumerations.
 *
 * @see Creature for the main creature implementation
 * @see Unit for the base unit class
 * @see CreatureInfo for creature template data
 */

#ifndef MANGOSSERVER_CREATURE_H
#define MANGOSSERVER_CREATURE_H

#include <unordered_map>
#include "Platform/Define.h"
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include "Common/TimeConstants.h"
#include "GameTime.h"
#include "Unit.h"
#include "SharedDefines.h"
#include "LootMgr.h"
#include "DBCEnums.h"
#include "Cell.h"
#include "data/CreatureRecords.h"

#include <list>

struct SpellEntry;

class CreatureAI;
class Group;
class Quest;
class Player;
class WorldSession;

struct GameEventCreatureData;
struct CreatureDataAddon;

#define USE_DEFAULT_DATABASE_LEVEL  0                       // just used to show we don't want to force the new creature level and use the level stored in db

enum SplineFlags
{
    SPLINEFLAG_WALKMODE     = 0x00001000,
    SPLINEFLAG_FLYING       = 0x00002000,
};

// Enums used by StringTextData::Type (CreatureEventAI)
enum ChatType
{
    CHAT_TYPE_SAY               = 0,
    CHAT_TYPE_YELL              = 1,
    CHAT_TYPE_TEXT_EMOTE        = 2,
    CHAT_TYPE_BOSS_EMOTE        = 3,
    CHAT_TYPE_WHISPER           = 4,
    CHAT_TYPE_BOSS_WHISPER      = 5,
    CHAT_TYPE_ZONE_YELL         = 6
};

// Selection method used by SelectAttackingTarget
enum AttackingTarget
{
    ATTACKING_TARGET_RANDOM = 0,                            // Just selects a random target
    ATTACKING_TARGET_TOPAGGRO,                              // Selects targes from top aggro to bottom
    ATTACKING_TARGET_BOTTOMAGGRO,                           // Selects targets from bottom aggro to top
};

enum SelectFlags
{
    SELECT_FLAG_IN_LOS              = 0x001,                // Default Selection Requirement for Spell-targets
    SELECT_FLAG_PLAYER              = 0x002,
    SELECT_FLAG_POWER_MANA          = 0x004,                // For Energy based spells, like manaburn
    SELECT_FLAG_POWER_RAGE          = 0x008,
    SELECT_FLAG_POWER_ENERGY        = 0x010,
    SELECT_FLAG_POWER_RUNIC         = 0x020,
    SELECT_FLAG_IN_MELEE_RANGE      = 0x040,
    SELECT_FLAG_NOT_IN_MELEE_RANGE  = 0x080,
};

struct VendorItemCount
{
    explicit VendorItemCount(uint32 _item, uint32 _count)
        : itemId(_item), count(_count), lastIncrementTime(time(NULL)) {}

    uint32 itemId;
    uint32 count;
    time_t lastIncrementTime;
};

typedef std::list<VendorItemCount> VendorItemCounts;

typedef std::map<uint32, time_t> CreatureSpellCooldowns;

// max different by z coordinate for creature aggro reaction
#define CREATURE_Z_ATTACK_RANGE 3

enum VirtualItemSlot
{
    VIRTUAL_ITEM_SLOT_0 = 0,
    VIRTUAL_ITEM_SLOT_1 = 1,
    VIRTUAL_ITEM_SLOT_2 = 2,
};

#define MAX_VIRTUAL_ITEM_SLOT 3

struct CreatureCreatePos
{
    public:
        // exactly coordinates used
        CreatureCreatePos(Map* map, float x, float y, float z, float o, uint32 phaseMask)
            : m_map(map), m_phaseMask(phaseMask), m_closeObject(NULL), m_angle(0.0f), m_dist(0.0f) { m_pos.x = x; m_pos.y = y; m_pos.z = z; m_pos.o = o; }
        // if dist == 0.0f -> exactly object coordinates used, in other case close point to object (CONTACT_DIST can be used as minimal distances)
        CreatureCreatePos(WorldObject* closeObject, float ori, float dist = 0.0f, float angle = 0.0f)
            : m_map(closeObject->GetMap()), m_phaseMask(closeObject->GetPhaseMask()),
              m_closeObject(closeObject), m_angle(angle), m_dist(dist) { m_pos.o = ori; }
    public:
        Map* GetMap() const { return m_map; }
        uint32 GetPhaseMask() const { return m_phaseMask; }
        void SelectFinalPoint(Creature* cr);
        bool PlaceOn(Creature* cr) const;

        // read only after SelectFinalPoint
        Position m_pos;
    private:
        Map* m_map;
        uint32 m_phaseMask;
        WorldObject* m_closeObject;
        float m_angle;
        float m_dist;
};

enum CreatureSubtype
{
    CREATURE_SUBTYPE_GENERIC,                               // new Creature
    CREATURE_SUBTYPE_PET,                                   // new Pet
    CREATURE_SUBTYPE_TOTEM,                                 // new Totem
    CREATURE_SUBTYPE_TEMPORARY_SUMMON,                      // new TemporarySummon
};

enum TemporaryFactionFlags                                  // Used at real faction changes
{
    TEMPFACTION_NONE                    = 0x00,             // When no flag is used in temporary faction change, faction will be persistent. It will then require manual change back to default/another faction when changed once
    TEMPFACTION_RESTORE_RESPAWN         = 0x01,             // Default faction will be restored at respawn
    TEMPFACTION_RESTORE_COMBAT_STOP     = 0x02,             // ... at CombatStop() (happens at creature death, at evade or custom scripte among others)
    TEMPFACTION_RESTORE_REACH_HOME      = 0x04,             // ... at reaching home in home movement (evade), if not already done at CombatStop()
    TEMPFACTION_TOGGLE_NON_ATTACKABLE   = 0x08,             // Remove UNIT_FLAG_NON_ATTACKABLE(0x02) when faction is changed (reapply when temp-faction is removed)
    TEMPFACTION_TOGGLE_OOC_NOT_ATTACK   = 0x10,             // Remove UNIT_FLAG_OOC_NOT_ATTACKABLE(0x100) when faction is changed (reapply when temp-faction is removed)
    TEMPFACTION_TOGGLE_PASSIVE          = 0x20,             // Remove UNIT_FLAG_PASSIVE(0x200) when faction is changed (reapply when temp-faction is removed)
    TEMPFACTION_TOGGLE_PACIFIED         = 0x40,             // Remove UNIT_FLAG_PACIFIED(0x20000) when faction is changed (reapply when temp-faction is removed)
    TEMPFACTION_TOGGLE_NOT_SELECTABLE   = 0x80,             // Remove UNIT_FLAG_NOT_SELECTABLE(0x2000000) when faction is changed (reapply when temp-faction is removed)
    TEMPFACTION_ALL,
};

// grace period a creature keeps trying before evading a target it has no path to (TC parity)
#define CREATURE_NOPATH_EVADE_TIME (5 * IN_MILLISECONDS)

class Creature : public Unit
{
        CreatureAI* i_AI;

    public:

        explicit Creature(CreatureSubtype subtype = CREATURE_SUBTYPE_GENERIC);
        virtual ~Creature();

        void AddToWorld() override;
        void RemoveFromWorld() override;

        bool Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, Team team = TEAM_NONE, const CreatureData* data = NULL, GameEventCreatureData const* eventData = NULL);
        bool LoadCreatureAddon(bool reload);
        void SelectLevel(const CreatureInfo* cinfo, float percentHealth = 100.0f);
        void SelectLevel(uint32 forcedLevel = USE_DEFAULT_DATABASE_LEVEL);
        void LoadEquipment(uint32 equip_entry, bool force = false);

        bool HasStaticDBSpawnData() const;                  // listed in `creature` table and have fixed in DB guid

        char const* GetSubName() const { return GetCreatureInfo()->SubName; }

        void Update(uint32 update_diff, uint32 time) override;  // overwrite Unit::Update

        virtual void RegenerateAll(uint32 update_diff);
        uint32 GetEquipmentId() const { return m_equipmentId; }

        CreatureSubtype GetSubtype() const { return m_subtype; }
        bool IsPet() const override { return m_subtype == CREATURE_SUBTYPE_PET; }
        bool IsTotem() const override { return m_subtype == CREATURE_SUBTYPE_TOTEM; }
        bool IsTemporarySummon() const { return m_subtype == CREATURE_SUBTYPE_TEMPORARY_SUMMON; }
        // The unit that summoned a temporary summon; empty for any other creature
        virtual ObjectGuid GetSummonerGuid() const { return ObjectGuid(); }
        // Whether this creature's heals are logged and scored as its owner's: a totem's are, a statue's are not
        virtual bool CreditsHealToOwner() const { return false; }

        bool IsCorpse() const { return GetDeathState() ==  CORPSE; }
        bool IsDespawned() const { return GetDeathState() ==  DEAD; }
        void SetCorpseDelay(uint32 delay, bool ignoreCorpseDecayRatio = false)
        {
            m_corpseDelay = delay;
            if (ignoreCorpseDecayRatio)
            {
                m_ignoreCorpseDecayRatio = true;
            }
        }
        uint32 GetCorpseDelay() const { return m_corpseDelay; }
        bool IsRacialLeader() const { return GetCreatureInfo()->RacialLeader; }
        bool IsCivilian() const { return (GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_CIVILIAN) != 0; }
        bool IsGuard() const { return (GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_GUARD) != 0; }
        bool IsUnkillable() const { return (GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_UNKILLABLE) != 0; }

        bool CanWalk() const { return (GetCreatureInfo()->InhabitType & INHABIT_GROUND) != 0; }
        bool CanSwim() const override; ///< Aquatic nature, swim unit flags, or actual deep-water immersion.
        bool IsSwimming() const { return (WireFlags() & MOVEFLAG_SWIMMING) != 0; }
        bool CanFly() const override { return (GetCreatureInfo()->InhabitType & INHABIT_AIR) || (GetByteValue(UNIT_FIELD_BYTES_1, 3) & UNIT_BYTE1_FLAG_FLY_ANIM) || (WireFlags() & (MOVEFLAG_LEVITATING | MOVEFLAG_CAN_FLY)) != 0; }
        bool IsFlying() const { return (WireFlags() & (MOVEFLAG_FLYING | MOVEFLAG_LEVITATING)) != 0; }

        bool IsTrainerOf(Player* player, bool msg) const;
        bool CanInteractWithBattleMaster(Player* player, bool msg) const;
        bool CanTrainAndResetTalentsOf(Player* pPlayer) const;

        bool IsOutOfThreatArea(Unit* pVictim) const;
        void FillGuidsListFromThreatList(GuidVector& guids, uint32 maxamount = 0);

        void SetCannotReachTarget(bool cannotReach);        ///< Arms/disarms the no-path evade grace timer.
        bool CanNotReachTarget() const { return m_cannotReachTarget; }

        bool IsImmuneToSpell(SpellEntry const* spellInfo, bool castOnSelf) override;
        bool IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const override;
        void SetLootStatus(CreatureLootStatus status);
        bool IsTappedBy(Player* plr) const;

        bool IsElite() const
        {
            if (IsPet())
            {
                return false;
            }

            uint32 rank = GetCreatureInfo()->Rank;
            return rank != CREATURE_ELITE_NORMAL && rank != CREATURE_ELITE_RARE;
        }

        bool IsWorldBoss() const
        {
            if (IsPet())
            {
                return false;
            }

            return GetCreatureInfo()->Rank == CREATURE_ELITE_WORLDBOSS;
        }

        uint32 GetLevelForTarget(Unit const* target) const override; // overwrite Unit::GetLevelForTarget for boss level support

        uint8 getRace() const override;

        bool IsInEvadeMode() const;

        bool AIM_Initialize();

        /// Installs an AI in place of the current one, bypassing m_AI_locked and
        /// without deleting the previous one (ownership passes to the caller): for
        /// callers outside the map phase, such as the GM harness, which runs between
        /// map updates and wraps the selected AI inside a recording one that owns it.
        void SetAI(CreatureAI* ai) { i_AI = ai; }

        CreatureAI* AI() override final { return i_AI; }

        // Threat methods only a Creature can execute (decoupling D5e, server #134): each
        // moved here from Unit unchanged, where it opened with MANGOS_ASSERT(GetTypeId() ==
        // TYPEID_UNIT). Defined in CreatureThreat.cpp.
        bool SelectHostileTarget();
        void TauntApply(Unit* pVictim);
        void TauntFadeOut(Unit* taunter);

        void SetWalk(bool enable, bool asDefault = true);
        /// Stop where it stands and, for a patrol, hold the leg a while so the player can talk.
        void HoldForPlayer();
        void SetLevitate(bool enable) override;
        void SetSwim(bool enable) override;
        void UpdateSwimmingState(); ///< Sync MOVEFLAG_SWIMMING with the liquid at the current position.
        void SetCanFly(bool enable) override;
        void SetFeatherFall(bool enable) override;
        void SetHover(bool enable) override;
        void SetRoot(bool enable) override;
        void SetWaterWalk(bool enable) override;

        uint32 GetShieldBlockDamageValue() const override         // dunno mob block value
        {
            return uint32(BASE_BLOCK_DAMAGE_PERCENT);
        }

        SpellSchoolMask GetMeleeDamageSchoolMask() const override { return m_meleeDamageSchoolMask; }
        void SetMeleeDamageSchool(SpellSchools school) { m_meleeDamageSchoolMask = SpellSchoolMask(1 << school); }

        void _AddCreatureSpellCooldown(uint32 spell_id, time_t end_time);
        void _AddCreatureCategoryCooldown(uint32 category, time_t apply_time);
        uint32 GetCreatureSpellCooldownDelay(uint32 spellId) const;
        void AddCreatureSpellCooldown(uint32 spellid);
        bool HasSpellCooldown(uint32 spell_id) const;
        bool HasCategoryCooldown(uint32 spell_id) const;

        bool HasSpell(uint32 spellID) const override;

        bool UpdateEntry(uint32 entry, Team team = ALLIANCE, const CreatureData* data = NULL, GameEventCreatureData const* eventData = NULL, bool preserveHPAndPower = true);

        void ApplyGameEventSpells(GameEventCreatureData const* eventData, bool activated);
        bool UpdateStats(Stats stat) override;
        bool UpdateAllStats() override;
        void UpdateResistances(uint32 school) override;
        void UpdateArmor() override;
        void UpdateMaxHealth() override;
        void UpdateMaxPower(Powers power) override;
        void UpdateAttackPowerAndDamage(bool ranged = false) override;
        void UpdateDamagePhysical(WeaponAttackType attType) override;
        uint32 GetCurrentEquipmentId() { return m_equipmentId; }

        static float _GetHealthMod(int32 Rank);             ///< Get custom factor to scale health (default 1, CONFIG_FLOAT_RATE_CREATURE_*_HP)
        static float _GetDamageMod(int32 Rank);             ///< Get custom factor to scale damage (default 1, CONFIG_FLOAT_RATE_*_DAMAGE)
        static float _GetSpellDamageMod(int32 Rank);        ///< Get custom factor to scale spell damage (default 1, CONFIG_FLOAT_RATE_*_SPELLDAMAGE)

        VendorItemData const* GetVendorItems() const;
        VendorItemData const* GetVendorTemplateItems() const;
        uint32 GetVendorItemCurrentCount(VendorItem const* vItem);
        uint32 UpdateVendorItemCurrentCount(VendorItem const* vItem, uint32 used_count);

        TrainerSpellData const* GetTrainerTemplateSpells() const;
        TrainerSpellData const* GetTrainerSpells() const;

        CreatureInfo const* GetCreatureInfo() const { return m_creatureInfo; }
        CreatureDataAddon const* GetCreatureAddon() const;

        static uint32 ChooseDisplayId(const CreatureInfo* cinfo, const CreatureData* data = NULL, GameEventCreatureData const* eventData = NULL);

        std::string GetAIName() const;
        std::string GetScriptName() const;
        uint32 GetScriptId() const;

        // overwrite WorldObject function for proper name localization
        const char* GetNameForLocaleIdx(int32 locale_idx) const override;

        void SetDeathState(DeathState s) override;          // overwrite virtual Unit::SetDeathState

        bool LoadFromDB(uint32 guid, Map* map);
        void SaveToDB();
        // overwrited in Pet
        virtual void SaveToDB(uint32 mapid, uint8 spawnMask, uint32 phaseMask);
        virtual void DeleteFromDB();                        // overwrited in Pet
        static void DeleteFromDB(uint32 lowguid, CreatureData const* data);

        /// Represent the loots available on the creature.
        Loot loot;

        /// Indicates whether the creature has has been pickpocked.
        bool lootForPickPocketed;

        /// Indicates whether the creature has been checked.
        bool lootForBody;

        /// Indicates whether the creature has been skinned.
        bool lootForSkin;

        /**
        * Method preparing the creature for the loot state. Based on the previous loot state, the loot ID provided in the database and the creature's type,
        * this method updates the state of the creature for loots.
        *
        * At the end of this method, the creature loot state may be:
        * Lootable: UNIT_DYNFLAG_LOOTABLE
        * Skinnable: UNIT_FLAG_SKINNABLE
        * Not lootable: No flag
        */
        void PrepareBodyLootState();

        /**
        * function returning the GUID of the loot recipient (a player GUID).
        *
        * \return ObjectGuid Player GUID.
        */
        ObjectGuid GetLootRecipientGuid() const { return m_lootRecipientGuid; }

        /**
        * function returning the group recipient ID.
        *
        * \return uint32 Group ID.
        */
        uint32 GetLootGroupRecipientId() const { return m_lootGroupRecipientId; }
        Player* GetLootRecipient() const;                   // use group cases as prefered
        Group* GetGroupLootRecipient() const;
        /**
        * function indicating whether the whether the creature has a looter recipient defined (either a group ID, either a player GUID).
        *
        * \return boolean true if the creature has a recipient defined, false otherwise.
        */
        bool HasLootRecipient() const { return m_lootGroupRecipientId || m_lootRecipientGuid; }

        /**
        * function indicating whether the recipient is a group.
        *
        * \return boolean true if the creature's recipient is a group, false otherwise.
        */
        bool IsGroupLootRecipient() const { return m_lootGroupRecipientId; }
        void SetLootRecipient(Unit* unit);
        void AllLootRemovedFromCorpse();
        Player* GetOriginalLootRecipient() const;           // ignore group changes/etc, not for looting

        SpellEntry const* ReachWithSpellAttack(Unit* pVictim);
        SpellEntry const* ReachWithSpellCure(Unit* pVictim);

        uint32 m_spells[CREATURE_MAX_SPELLS];
        CreatureSpellCooldowns m_CreatureSpellCooldowns;
        CreatureSpellCooldowns m_CreatureCategoryCooldowns;

        float GetAttackDistance(Unit const* pl) const;

        void SendAIReaction(AiReaction reactionType);

        void DoFleeToGetAssistance();
        void CallForHelp(float fRadius);
        void CallAssistance();
        void SetNoCallAssistance(bool val) { m_AlreadyCallAssistance = val; }
        void SetNoSearchAssistance(bool val) { m_AlreadySearchedAssistance = val; }
        bool HasSearchedAssistance() { return m_AlreadySearchedAssistance; }
        bool CanAssistTo(const Unit* u, const Unit* enemy, bool checkfaction = true) const;
        bool CanInitiateAttack();

        CreatureMovementType GetDefaultMovementType() const { return m_defaultMovementType; }
        void SetDefaultMovementType(CreatureMovementType type) { m_defaultMovementType = type; }

        // for use only in LoadHelper, Map::Add Map::CreatureCellRelocation
        Cell const& GetCurrentCell() const { return m_currentCell; }
        void SetCurrentCell(Cell const& cell) { m_currentCell = cell; }

        bool IsVisibleInGridForPlayer(Player* pl) const override;

        void RemoveCorpse();
        bool IsDeadByDefault() const { return m_IsDeadByDefault; };

        void ForcedDespawn(uint32 timeMSToDespawn = 0);

        time_t const& GetRespawnTime() const { return m_respawnTime; }
        time_t GetRespawnTimeEx() const;
        void SetRespawnTime(uint32 respawn) { m_respawnTime = respawn ? time(NULL) + respawn : 0; }
        void Respawn();
        void SaveRespawnTime() override;

        uint32 GetRespawnDelay() const { return m_respawnDelay; }
        void SetRespawnDelay(uint32 delay) { m_respawnDelay = delay; }

        float GetRespawnRadius() const { return m_respawnradius; }
        void SetRespawnRadius(float dist) { m_respawnradius = dist; }

        // Functions spawn/remove creature with DB guid in all loaded map copies (if point grid loaded in map)
        static void AddToRemoveListInMaps(uint32 db_guid, CreatureData const* data);
        static void SpawnInMaps(uint32 db_guid, CreatureData const* data);

        void StartGroupLoot(Group* group, uint32 timer) override;

        void SendZoneUnderAttackMessage(Player* attacker);

        void SetInCombatWithZone();

        Unit* SelectAttackingTarget(AttackingTarget target, uint32 position, uint32 uiSpellEntry, uint32 selectFlags = 0) const;
        Unit* SelectAttackingTarget(AttackingTarget target, uint32 position, SpellEntry const* pSpellInfo = NULL, uint32 selectFlags = 0) const;

        bool HasQuest(uint32 quest_id) const override;
        bool HasInvolvedQuest(uint32 quest_id)  const override;

        GridReference<Creature>& GetGridRef() { return m_gridRef; }
        bool IsRegeneratingHealth() { return GetCreatureInfo()->RegenerateStats & REGEN_FLAG_HEALTH; }
        bool IsRegeneratingPower() { return GetCreatureInfo()->RegenerateStats & REGEN_FLAG_POWER; }
        virtual uint8 GetPetAutoSpellSize() const { return CREATURE_MAX_SPELLS; }
        virtual uint32 GetPetAutoSpellOnPos(uint8 pos) const
        {
            if (pos >= CREATURE_MAX_SPELLS || m_charmInfo->GetCharmSpell(pos)->GetType() != ACT_ENABLED)
            {
                return 0;
            }
            else
            {
                return m_charmInfo->GetCharmSpell(pos)->GetAction();
            }
        }

        /// Where combat began, in the frame it began in. The leash point, and nothing
        /// composes it -- a creature pulled on a deck leashes to a deck spot.
        Geometry::Vector3 const& CombatAnchor() const { return m_combatStart; }
        void SetCombatAnchor(Geometry::Vector3 const& at) { m_combatStart = at; }

        /// Where this creature belongs: its spawn pose, in the frame it spawned in.
        /// Home movement, wander and leashing all read it, and nothing composes it.
        Geometry::Placement const& Spawn() const { return m_spawn; }
        void SetSpawn(CreatureCreatePos const& pos);
        void SetSpawn(Geometry::Vector3 const& at, float facing);
        void ResetSpawn();

        void SetDeadByDefault(bool death_state) { m_IsDeadByDefault = death_state; }

        void SetFactionTemporary(uint32 factionId, uint32 tempFactionFlags = TEMPFACTION_ALL);
        void ClearTemporaryFaction();
        uint32 GetTemporaryFactionFlags() { return m_temporaryFactionFlags; }

        void SendAreaSpiritHealerQueryOpcode(Player* pl);

        void SetVirtualItem(VirtualItemSlot slot, uint32 item_id) { SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + slot, item_id); }

        void SetDisableReputationGain(bool disable) { DisableReputationGain = disable; }
        bool IsReputationGainDisabled() { return DisableReputationGain; }
    protected:
        bool MeetsSelectAttackingRequirement(Unit* pTarget, SpellEntry const* pSpellInfo, uint32 selectFlags) const;

        bool CreateFromProto(uint32 guidlow, CreatureInfo const* cinfo, Team team, const CreatureData* data = NULL, GameEventCreatureData const* eventData = NULL);
        bool InitEntry(uint32 entry, const CreatureData* data = NULL, GameEventCreatureData const* eventData = NULL);

        uint32 m_groupLootTimer;                            // (msecs)timer used for group loot
        uint32 m_groupLootId;                               // used to find group which is looting corpse
        void StopGroupLoot() override;

        // vendor items
        VendorItemCounts m_vendorItemCounts;

        void _RealtimeSetCreatureInfo();

        uint32 m_lootMoney;
        ObjectGuid m_lootRecipientGuid;                     // player who will have rights for looting if m_lootGroupRecipient==0 or group disbanded
        uint32 m_lootGroupRecipientId;                      // group who will have rights for looting if set and exist
        CreatureLootStatus m_lootStatus;                    // loot status (used to know when we could loot, pickpocket or skin)

        /// Timers
        uint32 m_corpseDecayTimer;                          // (msecs)timer for death or corpse disappearance
        time_t m_respawnTime;                               // (secs) time of next respawn
        uint32 m_respawnDelay;                              // (secs) delay between corpse disappearance and respawning
        uint32 m_corpseDelay;                               // (secs) delay between death and corpse disappearance
        uint32 m_aggroDelay;                                // (msecs)delay between respawn and aggro due to movement
        bool m_cannotReachTarget;                           // victim is unreachable (no path); armed by SelectHostileTarget
        uint32 m_cannotReachTimer;                          // (msecs)time spent unable to reach the victim
        bool m_ignoreCorpseDecayRatio;
        float m_respawnradius;

        CreatureSubtype m_subtype;                          // set in Creatures subclasses for fast it detect without dynamic_cast use
        void RegeneratePower();
        void RegenerateHealth();
        CreatureMovementType m_defaultMovementType;
        Cell m_currentCell;                                 // store current cell where creature listed
        uint32 m_equipmentId;

        // below fields has potential for optimization
        bool m_AlreadyCallAssistance;
        bool m_AlreadySearchedAssistance;
        bool m_AI_locked;
        bool m_IsDeadByDefault;
        uint32 m_temporaryFactionFlags;                     // used for real faction changes (not auras etc)

        SpellSchoolMask m_meleeDamageSchoolMask;
        uint32 m_originalEntry;

        Geometry::Vector3 m_combatStart;

        Geometry::Placement m_spawn;

        bool DisableReputationGain;

    private:
        GridReference<Creature> m_gridRef;
        CreatureInfo const* m_creatureInfo;                 // in difficulty mode > 0 can different from ObjMgr::GetCreatureTemplate(GetEntry())
};

class ForcedDespawnDelayEvent : public BasicEvent
{
    public:
        ForcedDespawnDelayEvent(Creature& owner) : BasicEvent(), m_owner(owner) { }
        bool Execute(uint64 e_time, uint32 p_time) override;

    private:
        Creature& m_owner;
};

#endif
