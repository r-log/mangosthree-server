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

#include <sstream>
#include <string>
#include "Pet.h"
#include "Player.h"
#include "PlayerPetCache.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "WorldPacket.h"
#include "Group.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Formulas.h"
#include "SpellAuras.h"
#include "CreatureAI.h"
#include "Unit.h"
#include "Util.h"

/**
 * @file PetDatabase.cpp
 * @brief Cohesion split of Pet.cpp -- pet persistence: LoadPetFromDB, SavePetToDB and DeleteFromDB. Same Pet class; no behaviour change. CMake file(GLOB) picks this file up automatically; Pet.h is unchanged.
 */

/**
 * @brief Loads a pet from the database for an owner.
 *
 * @param owner The player owning the pet.
 * @param petentry Optional creature entry filter.
 * @param petnumber Optional pet number filter.
 * @param current true to load the current pet.
 * @return true if the pet was loaded successfully; otherwise, false.
 */
bool Pet::LoadPetFromDB(Player* owner, uint32 petentry, uint32 petnumber, bool current, int32 slot)
{
    m_loading = true;

    uint32 ownerid = owner->GetGUIDLow();

    // Decoupling D7e: the five mutually exclusive SELECTs on `character_pet` became five
    // lookups over the character's own cached rows, which the login holder loaded with the
    // character. Same branches in the same order and with the same WHERE clauses; the owner
    // is implicit, because the cache holds this character's rows and no others.
    //
    // A NULL row means what a NULL QueryResult meant. The row is COPIED rather than pointed
    // at: the auto-promote block below rewrites slots in the cache, which would move the
    // fields still being read out of it.
    PlayerPetCache const& petCache = owner->GetPetCache();
    PetCacheRow const* found;

    if (petnumber)
        // known petnumber entry -- WHERE `owner` = X AND `id` = P
        found = petCache.FindById(petnumber);
    else if (current)
        // current pet (slot 0) -- WHERE `owner` = X AND `slot` = PET_SAVE_AS_CURRENT
        found = petCache.FindBySlot(uint32(PET_SAVE_AS_CURRENT));
    else if (slot >= 0)
        // Cata Call Pet 1..N: load the pet at this specific active slot
        // (0..PET_SLOT_LAST_ACTIVE_SLOT) -- WHERE `owner` = X AND `slot` = S
        found = petCache.FindBySlot(uint32(slot));
    else if (petentry)
        // known petentry entry (unique for summoned pet, but non unique for hunter pet (only from current or not stabled pets)
        // WHERE `owner` = X AND `entry` = E AND (`slot` = PET_SAVE_AS_CURRENT OR `slot` > PET_SAVE_LAST_STABLE_SLOT)
        found = petCache.FindByEntryCurrentOrUnslotted(petentry);
    else
        // any current or other non-stabled pet (for hunter "call pet")
        // WHERE `owner` = X AND (`slot` = PET_SAVE_AS_CURRENT OR `slot` > PET_SAVE_LAST_STABLE_SLOT)
        found = petCache.FindCurrentOrUnslotted();

    if (!found)
    {
        return false;
    }

    PetCacheRow const fields = *found;

    // Record the slot column on the Pet instance. Future commits on this
    // branch (audit IDs C2 / D-row hunter re-saves) read m_petSlot to keep
    // each tamed pet parked at its Call Pet 1..N slot across re-saves and
    // dismisses. Capturing it here (after the cached row is known to
    // exist, but before any early-out) means every successful
    // load -- by petnumber, current-flag, explicit slot, petentry, or
    // legacy fallback -- gets the same accurate value. Harmless for the
    // early-out failure paths below: a Pet object that returns false
    // gets discarded by the caller, m_petSlot included.
    m_petSlot = int32(fields.slot);

    // update for case of current pet "slot = 0"
    petentry = fields.entry;
    if (!petentry)
    {
        return false;
    }

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(petentry);
    if (!creatureInfo)
    {
        sLog.outError("Pet entry %u does not exist but used at pet load (owner: %s).", petentry, owner->GetGuidStr().c_str());
        return false;
    }

    uint32 summon_spell_id = fields.createdBySpell;
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(summon_spell_id);

    bool is_temporary_summoned = spellInfo && GetSpellDuration(spellInfo) > 0;

    // check temporary summoned pets like mage water elemental
    if (current && is_temporary_summoned)
    {
        return false;
    }

    PetType pet_type = PetType(fields.petType);
    if (pet_type == HUNTER_PET)
    {
        if (!creatureInfo->isTameable(owner->CanTameExoticPets()))
        {
            return false;
        }
    }

    uint32 pet_number = fields.id;

    Map* map = owner->GetMap();

    CreatureCreatePos pos(owner, owner->Where().Facing(), PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);

    uint32 guid = pos.GetMap()->GenerateLocalLowGuid(HIGHGUID_PET);
    if (!Create(guid, pos, creatureInfo, pet_number))
    {
        return false;
    }

    setPetType(pet_type);
    setFaction(owner->getFaction());
    SetUInt32Value(UNIT_CREATED_BY_SPELL, summon_spell_id);

    // reget for sure use real creature info selected for Pet at load/creating
    CreatureInfo const* cinfo = GetCreatureInfo();
    if (cinfo->CreatureType == CREATURE_TYPE_CRITTER)
    {
        AIM_Initialize();
        pos.GetMap()->Add((Creature*)this);
        return true;
    }

    m_charmInfo->SetPetNumber(pet_number, isControlled());

    SetOwnerGuid(owner->GetObjectGuid());
    SetDisplayId(fields.modelId);
    SetNativeDisplayId(fields.modelId);
    uint32 petlevel = fields.level;
    SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
    SetName(fields.name);

    // Defensive name fallback. The starter pet rows created by
    // WorldSession::HandleCharCreateOpcode (CharacterHandler.cpp) write
    // `name = ' '` (single space) into character_pet for every race's
    // hunter starter and the warlock imp. An empty / whitespace-only
    // name renders client-side as "Unknown's Pet" in unit frames and
    // produces blank-name rows in the stable list. Any future code path
    // that persists a Pet row without setting a name lands here too --
    // this fallback mirrors the family-name logic in CreateBaseAtCreature
    // (PR #135) so the load path is consistent with the tame path.
    {
        bool nameIsBlank = true;
        for (char const* p = m_name.c_str(); *p; ++p)
        {
            if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
            {
                nameIsBlank = false;
                break;
            }
        }
        if (nameIsBlank)
        {
            std::string fallback;
            if (CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(creatureInfo->Family))
            {
                fallback = cFamily->Name_lang[sWorld.GetDefaultDbcLocale()];
            }
            if (fallback.empty() && creatureInfo->Name)
            {
                fallback = creatureInfo->Name;
            }
            if (!fallback.empty())
            {
                SetName(fallback);
            }
        }
    }

    SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SUPPORTABLE | UNIT_BYTE2_FLAG_AURAS);
    SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);

    if (getPetType() == HUNTER_PET)
    {
        SetByteFlag(UNIT_FIELD_BYTES_2, 2, (fields.renamed != 0) ? UNIT_CAN_BE_ABANDONED : UNIT_CAN_BE_RENAMED | UNIT_CAN_BE_ABANDONED);
        SetPowerType(POWER_FOCUS);
    }
    else if (getPetType() != SUMMON_PET)
        sLog.outError("Pet have incorrect type (%u) for pet loading.", getPetType());

    if (owner->IsPvP())
    {
        SetPvP(true);
    }

    if (owner->IsFFAPvP())
    {
        SetFFAPvP(true);
    }

    SetCanModifyStats(true);
    InitStatsForLevel(petlevel);
    InitTalentForLevel();                                   // set original talents points before spell loading

    SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(time(NULL)));
    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, fields.exp);
    SetCreatorGuid(owner->GetObjectGuid());

    m_charmInfo->SetReactState(ReactStates(fields.reactState));

    uint32 savedhealth = fields.curHealth;
    uint32 savedpower = fields.curMana;

    // set current pet as current
    // 0=current
    // 1..MAX_PET_STABLES in stable slot
    // PET_SAVE_NOT_IN_SLOT(100) = not stable slot (summoning))
    //
    // The legacy auto-promote block below moves whichever pet we just
    // loaded into PET_SAVE_AS_CURRENT (slot 0) and bumps any previous
    // slot-0 pet to PET_SAVE_NOT_IN_SLOT (=100). That was correct under
    // the WotLK 1-active-pet model where loading a stabled pet implied
    // making it the active one and demoting the old current.
    //
    // For Cata HUNTER_PET the same code path is destructive: every pet
    // permanently lives in its assigned Call Pet 1..N slot 0..4 and
    // loading one must NOT shift it. Without this gate,
    // ResummonTemporaryUnsummonedIfAny (mount, transport, vehicle exit)
    // would force the resumed pet back to slot 0 and silently bump the
    // pet that owns slot 0 out into orphan land, breaking the next
    // Call Pet 1 cast. This was the bug audit row C2 in
    // MANGOS/PET_SAVE_CALLSITE_AUDIT.md and the gap closed PR #137 left
    // open when it was reverted.
    //
    // SUMMON_PET / GUARDIAN_PET / MINI_PET keep the legacy auto-promote
    // -- they have no Call Pet 1..N concept and the warlock pet pool
    // still expects loading from slot 100 to mean "becomes active."
    if (getPetType() != HUNTER_PET && fields.slot != 0)
    {
        CharacterDatabase.BeginTransaction();

        static SqlStatementID id_1;
        static SqlStatementID id_2;

        SqlStatement stmt = CharacterDatabase.CreateStatement(id_1, "UPDATE `character_pet` SET `slot` = ? WHERE `owner` = ? AND `slot` = ? AND `id` <> ?");
        stmt.PExecute(uint32(PET_SAVE_NOT_IN_SLOT), ownerid, uint32(PET_SAVE_AS_CURRENT), m_charmInfo->GetPetNumber());

        stmt = CharacterDatabase.CreateStatement(id_2, "UPDATE `character_pet` SET `slot` = ? WHERE `owner` = ? AND `id` = ?");
        stmt.PExecute(uint32(PET_SAVE_AS_CURRENT), ownerid, m_charmInfo->GetPetNumber());

        CharacterDatabase.CommitTransaction();

        // The cache carries the same two updates, in the same order. `fields` is a copy, so
        // the slot it holds is the one the row had before this block -- which is what the
        // rest of the function still wants.
        PlayerPetCache& writable = owner->GetPetCache();
        writable.MoveSlot(uint32(PET_SAVE_AS_CURRENT), uint32(PET_SAVE_NOT_IN_SLOT), m_charmInfo->GetPetNumber());
        writable.SetSlot(m_charmInfo->GetPetNumber(), uint32(PET_SAVE_AS_CURRENT));
    }

    // load action bar, if data broken will fill later by default spells.
    if (!is_temporary_summoned)
    {
        m_charmInfo->LoadPetActionBar(fields.abData);
    }

    // since last save (in seconds)
    uint32 timediff = uint32(time(NULL) - fields.saveTime);

    m_resetTalentsCost = fields.resetTalentsCost;
    m_resetTalentsTime = fields.resetTalentsTime;


    // load spells/cooldowns/auras
    _LoadAuras(timediff, petCache);

    // init AB
    if (is_temporary_summoned)
    {
        // Temporary summoned pets always have initial spell list at load
        InitPetCreateSpells();
    }
    else
    {
        LearnPetPassives();
        CastPetAuras(current);
        CastOwnerTalentAuras();
    }

    Powers powerType = GetPowerType();;

    SetHealth(savedhealth > GetMaxHealth() ? GetMaxHealth() : savedhealth);
    SetPower(powerType, savedpower > GetMaxPower(powerType) ? GetMaxPower(powerType) : savedpower);

    if (getPetType() == HUNTER_PET && savedhealth <= 0)
    {
        SetDeathState(JUST_DIED);
    }

    map->Add((Creature*)this);
    AIM_Initialize();

    // Spells should be loaded after pet is added to map, because in CheckCast is check on it
    _LoadSpells(petCache);
    InitLevelupSpellsForLevel();

    CleanupActionBar();                                     // remove unknown spells from action bar after load

    _LoadSpellCooldowns(petCache);

    owner->SetPet(this);                                    // in DB stored only full controlled creature
    DEBUG_LOG("New Pet has guid %u", GetGUIDLow());

    if (owner->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)owner)->PetSpellInitialize();
        if (((Player*)owner)->GetGroup())
        {
            ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_PET);
        }

        ((Player*)owner)->SendTalentsInfoData(true);
    }

    if (owner->GetTypeId() == TYPEID_PLAYER && getPetType() == HUNTER_PET)
    {
        // Decoupling D7e: `SELECT ... FROM character_pet_declinedname WHERE owner = X AND id = P`.
        // The cache holds this character's rows keyed by pet id, so the owner half of the
        // WHERE clause is the cache itself.
        if (PetCacheDeclinedName const* declined = petCache.FindDeclinedName(GetCharmInfo()->GetPetNumber()))
        {
            delete m_declinedname;
            m_declinedname = new DeclinedName;

            for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
            {
                m_declinedname->name[i] = declined->name[i];
            }
        }
    }

    m_loading = false;

    SynchronizeLevelWithOwner();
    return true;
}

/**
 * @brief Saves the pet to the database using the specified save mode.
 *
 * @param mode The pet save mode to apply.
 */
void Pet::SavePetToDB(PetSaveMode mode)
{
    if (!GetEntry())
    {
        return;
    }

    // save only fully controlled creature
    if (!isControlled())
    {
        return;
    }

    // not save not player pets
    if (!GetOwnerGuid().IsPlayer())
    {
        return;
    }

    Player* pOwner = (Player*)GetOwner();
    if (!pOwner)
    {
        return;
    }

    // Decoupling D7e: the owner is online by the three gates above, so it has a pet cache.
    // Every statement this function queues below is mirrored into it at the same place, in
    // the same order, so the next LoadPetFromDB reads what this save wrote.
    PlayerPetCache& petCache = pOwner->GetPetCache();

    // Cata multi-pet allocator. Resolves PET_SAVE_NEW_PET (== 102) into
    // a concrete active-roster slot 0..PET_SLOT_LAST_ACTIVE_SLOT by
    // scanning character_pet for the first vacant slot in that range.
    // Updates `mode` and `m_petSlot` in place so the rest of the
    // function continues unchanged. If the roster is full, leaves
    // m_petSlot at PET_SAVE_NOT_IN_SLOT and bails out without writing
    // any row -- the caller (EffectTameCreature, future commit) checks
    // GetSlot() after the call to detect failure and roll the tame back.
    //
    // No caller passes PET_SAVE_NEW_PET in this commit; the block
    // becomes reachable in step 10 of the audit. Adding the allocator
    // ahead of its caller keeps each diff small enough to review on
    // its own.
    if (mode == PET_SAVE_NEW_PET)
    {
        // Decoupling D7e: the vacancy scan reads the owner's cached rows instead of
        // `SELECT slot FROM character_pet WHERE owner = X AND slot <= PET_SLOT_LAST_ACTIVE_SLOT`.
        bool occupied[PET_SLOT_LAST_ACTIVE_SLOT + 1] = {};
        PlayerPetCache::RowMap const& rows = petCache.Rows();
        for (PlayerPetCache::RowMap::const_iterator itr = rows.begin(); itr != rows.end(); ++itr)
        {
            uint32 s = itr->second.slot;
            if (s <= uint32(PET_SLOT_LAST_ACTIVE_SLOT))
            {
                occupied[s] = true;
            }
        }
        int32 freeSlot = -1;
        for (int32 s = 0; s <= PET_SLOT_LAST_ACTIVE_SLOT; ++s)
        {
            if (!occupied[s])
            {
                freeSlot = s;
                break;
            }
        }
        if (freeSlot < 0)
        {
            // Roster full -- do not persist anything. m_petSlot value
            // signals "no slot allocated" to the caller.
            m_petSlot = int32(PET_SAVE_NOT_IN_SLOT);
            return;
        }
        m_petSlot = freeSlot;
        mode = static_cast<PetSaveMode>(freeSlot);
    }
    else if (getPetType() == HUNTER_PET
             && (mode == PET_SAVE_AS_CURRENT || mode == PET_SAVE_NOT_IN_SLOT)
             && m_petSlot >= 0
             && m_petSlot <= int32(PET_SLOT_LAST_ACTIVE_SLOT))
    {
        // Cata multi-pet re-save path. Every hunter pet stays parked in
        // its Call Pet 1..N slot for the entire character lifetime,
        // including across dismiss, logout, level-up, feed, range
        // unsummon and visibility teardown. The pre-Cata code routed
        // PET_SAVE_AS_CURRENT (=0) and PET_SAVE_NOT_IN_SLOT (=100)
        // directly through to the INSERT, evicting the pet from its
        // assigned slot and breaking the next Call Pet N cast with
        // NO_PET.
        //
        // Rewrite mode in place so the rest of SavePetToDB writes
        // back to the pet's known slot. Legacy WotLK paths that have
        // never set m_petSlot (m_petSlot == -1, e.g. a fresh Pet*
        // about to be saved for the first time -- though tame goes
        // through PET_SAVE_NEW_PET in step 10) still fall through to
        // the original mode-as-passed behaviour. SUMMON_PET /
        // GUARDIAN_PET also fall through (gated on HUNTER_PET).
        //
        // Audit row IDs: D1 (EffectDismissPet), D2 (mount/transport
        // temp-unsummon), D4 (out-of-range), D5 (talent reset),
        // D6 (visibility teardown), D7 (GM .pet unsummon),
        // S4 (Player::SaveToDB periodic).
        //
        // Until step 10 ships, no hunter has a pet with m_petSlot
        // in 0..PET_SLOT_LAST_ACTIVE_SLOT (taming still goes through
        // PET_SAVE_AS_CURRENT and m_petSlot stays at -1 because there
        // is no load path to populate it). So this branch is dormant
        // in practice today and engages once step 10's tame change
        // starts populating m_petSlot.
        mode = static_cast<PetSaveMode>(m_petSlot);
    }

    // current/stable/not_in_slot
    if (mode >= PET_SAVE_AS_CURRENT)
    {
        // reagents must be returned before save call
        if (mode == PET_SAVE_REAGENTS)
        {
            // Hunter Pets always save as current if dismissed or unsummoned due to range/etc.
            if (getPetType() == HUNTER_PET)
            {
                mode = PET_SAVE_AS_CURRENT;
            }
            else
            {
                mode = PET_SAVE_NOT_IN_SLOT;
            }
        }
        // not save pet as current if another pet temporary unsummoned
        else if (mode == PET_SAVE_AS_CURRENT && pOwner->GetTemporaryUnsummonedPetNumber() &&
                 pOwner->GetTemporaryUnsummonedPetNumber() != m_charmInfo->GetPetNumber())
        {
            // pet will lost anyway at restore temporary unsummoned
            if (getPetType() == HUNTER_PET)
            {
                return;
            }

            // for warlock case
            mode = PET_SAVE_NOT_IN_SLOT;
        }

        uint32 curhealth = GetHealth();
        uint32 curpower = GetPower(GetPowerType());

        // stable and not in slot saves
        if (mode != PET_SAVE_AS_CURRENT)
        {
            RemoveAllAuras();
        }

        // save pet's data as one single transaction
        CharacterDatabase.BeginTransaction();
        _SaveSpells(petCache);
        _SaveSpellCooldowns(petCache);
        _SaveAuras(petCache);

        uint32 ownerLow = GetOwnerGuid().GetCounter();
        // remove current data
        static SqlStatementID delPet ;
        static SqlStatementID insPet ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delPet, "DELETE FROM `character_pet` WHERE `owner` = ? AND `id` = ?");
        stmt.PExecute(ownerLow, m_charmInfo->GetPetNumber());
        petCache.EraseRow(m_charmInfo->GetPetNumber());

        // prevent duplicate using slot (except PET_SAVE_NOT_IN_SLOT)
        if (mode <= PET_SAVE_LAST_STABLE_SLOT)
        {
            static SqlStatementID updPet ;

            stmt = CharacterDatabase.CreateStatement(updPet, "UPDATE `character_pet` SET `slot` = ? WHERE `owner` = ? AND `slot` = ?");
            stmt.PExecute(uint32(PET_SAVE_NOT_IN_SLOT), ownerLow, uint32(mode));
            petCache.MoveSlot(uint32(mode), uint32(PET_SAVE_NOT_IN_SLOT), 0);
        }

        // Cata multi-pet hunter cleanup: reap orphan rows
        // (slot > PET_SAVE_LAST_STABLE_SLOT) only -- the
        // slot 0..PET_SAVE_LAST_STABLE_SLOT range is the Call Pet 1..N
        // roster and must be preserved across every save.
        //
        // The legacy WHERE clause was `slot = PET_SAVE_AS_CURRENT OR
        // slot > PET_SAVE_LAST_STABLE_SLOT`, which destroyed every
        // other tamed pet at slot 0 whenever the active pet was
        // re-saved (dismiss, level-up, logout, taming a new one).
        // That was the data-loss root surfaced when PR #137 was
        // tested in-game and reverted -- the fix is here, in the
        // WHERE clause, not in the per-mode intercepts.
        //
        // `id <> ?` excludes the pet we are about to INSERT below so
        // that even if its previous row was bumped to NOT_IN_SLOT by
        // the UPDATE above, we do not immediately delete the new row.
        //
        // No behaviour change for SUMMON_PET / GUARDIAN_PET (the
        // outer `getPetType() == HUNTER_PET` guard already excluded
        // them). No behaviour change for any current hunter that has
        // a single pet at slot 0 -- there are no orphan rows to reap,
        // so the DELETE is a no-op on a healthy single-pet DB. The
        // cleanup re-engages only once multi-pet hunters exist
        // (after step 10 of the audit ships).
        if (getPetType() == HUNTER_PET && (mode == PET_SAVE_AS_CURRENT || mode > PET_SAVE_LAST_STABLE_SLOT))
        {
            static SqlStatementID del ;

            stmt = CharacterDatabase.CreateStatement(del, "DELETE FROM `character_pet` WHERE `owner` = ? AND `slot` > ? AND `id` <> ?");
            stmt.PExecute(ownerLow, uint32(PET_SAVE_LAST_STABLE_SLOT), m_charmInfo->GetPetNumber());
            petCache.EraseRowsAboveSlot(uint32(PET_SAVE_LAST_STABLE_SLOT), m_charmInfo->GetPetNumber());
        }

        // save pet
        SqlStatement savePet = CharacterDatabase.CreateStatement(insPet, "INSERT INTO `character_pet` "
        "(`id`, `entry`,  `owner`, `modelid`, `level`, `exp`, `Reactstate`, `slot`, `name`, `renamed`, `curhealth`, `curmana`, `abdata`, `savetime`, `resettalents_cost`, `resettalents_time`, `CreatedBySpell`, `PetType`) "
             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        savePet.addUInt32(m_charmInfo->GetPetNumber());
        savePet.addUInt32(GetEntry());
        savePet.addUInt32(ownerLow);
        savePet.addUInt32(GetNativeDisplayId());
        savePet.addUInt32(getLevel());
        savePet.addUInt32(GetUInt32Value(UNIT_FIELD_PETEXPERIENCE));
        savePet.addUInt32(uint32(m_charmInfo->GetReactState()));
        savePet.addUInt32(uint32(mode));
        savePet.addString(m_name.c_str());
        savePet.addUInt32(uint32(HasByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED) ? 0 : 1));
        savePet.addUInt32(curhealth);
        savePet.addUInt32(curpower);

        std::ostringstream ss;
        for (uint32 i = ACTION_BAR_INDEX_START; i < ACTION_BAR_INDEX_END; ++i)
        {
            ss << uint32(m_charmInfo->GetActionBarEntry(i)->GetType()) << " "
               << uint32(m_charmInfo->GetActionBarEntry(i)->GetAction()) << " ";
        };
        // Taken before the bind: SqlStatement::addString(std::ostringstream&) EMPTIES the
        // stream it is handed (SqlPreparedStatement.h), so ss.str() afterwards is "".
        const std::string abData = ss.str();
        savePet.addString(ss);

        // One reading of the clock, so the row the cache holds and the row the INSERT writes
        // carry the same `savetime` -- which is what the next load's `timediff` is measured
        // from.
        const uint64 saveTime = uint64(time(NULL));
        savePet.addUInt64(saveTime);
        savePet.addUInt32(uint32(m_resetTalentsCost));
        savePet.addUInt64(uint64(m_resetTalentsTime));
        savePet.addUInt32(GetUInt32Value(UNIT_CREATED_BY_SPELL));
        savePet.addUInt32(uint32(getPetType()));

        savePet.Execute();
        CharacterDatabase.CommitTransaction();

        // The same row, into the cache. Built from the same values in the same order as the
        // INSERT's parameters, so a reviewer can read the two side by side.
        PetCacheRow row;
        row.id               = m_charmInfo->GetPetNumber();
        row.entry            = GetEntry();
        row.owner            = ownerLow;
        row.modelId          = GetNativeDisplayId();
        row.level            = getLevel();
        row.exp              = GetUInt32Value(UNIT_FIELD_PETEXPERIENCE);
        row.reactState       = uint8(m_charmInfo->GetReactState());
        row.slot             = uint32(mode);
        row.name             = m_name;
        row.renamed          = uint8(HasByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED) ? 0 : 1);
        row.curHealth        = curhealth;
        row.curMana          = curpower;
        row.abData           = abData;
        row.saveTime         = saveTime;
        row.resetTalentsCost = uint32(m_resetTalentsCost);
        row.resetTalentsTime = uint64(m_resetTalentsTime);
        row.createdBySpell   = GetUInt32Value(UNIT_CREATED_BY_SPELL);
        row.petType          = uint8(getPetType());
        petCache.SetRow(row);
    }
    else
    {
        RemoveAllAuras(AURA_REMOVE_BY_DELETE);
        DeleteFromDB(m_charmInfo->GetPetNumber());
        // Pet::DeleteFromDB is a static with no owner, and its other caller
        // (Player::DeleteFromDB) runs for a character who is offline and has no cache. This
        // is the one call site with a live owner, so the five-table sweep is mirrored here.
        petCache.ErasePet(m_charmInfo->GetPetNumber());
    }
}

/**
 * @brief Deletes pet-related database records.
 *
 * @param guidlow The pet number identifier.
 * @param separate_transaction true to wrap deletion in its own transaction.
 */
void Pet::DeleteFromDB(uint32 guidlow, bool separate_transaction)
{
    if (separate_transaction)
    {
        CharacterDatabase.BeginTransaction();
    }

    static SqlStatementID delPet ;
    static SqlStatementID delDeclName ;
    static SqlStatementID delAuras ;
    static SqlStatementID delSpells ;
    static SqlStatementID delSpellCD ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delPet, "DELETE FROM `character_pet` WHERE `id` = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delDeclName, "DELETE FROM `character_pet_declinedname` WHERE `id` = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delAuras, "DELETE FROM `pet_aura` WHERE `guid` = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delSpells, "DELETE FROM `pet_spell` WHERE `guid` = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delSpellCD, "DELETE FROM `pet_spell_cooldown` WHERE `guid` = ?");
    stmt.PExecute(guidlow);

    if (separate_transaction)
    {
        CharacterDatabase.CommitTransaction();
    }
}
