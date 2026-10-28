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

#include "Utilities/Errors.h"
#include <sstream>
#include <vector>
#include "Player.h"
#include "TransportMap.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "spells/data/SpellMgr.h"
#include "WorldGameTime.h"
#include "data/GameConfig.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GridNotifiersImpl.h"
#include "ObjectMgr.h"
#include "Bag.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "Mail.h"
#include "spells/auras/SpellAuras.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "Vehicle.h"
#include "CharacterCache.h"
#include "DisableMgr.h"
#include "persistence/characters/CharacterSaveRepository.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/inventory/ItemRepository.h"
#include "persistence/characters/mail/MailRepository.h"
#include "persistence/characters/quests/QuestStatusRepository.h"

#include <cmath>

namespace
{
    /// An equipment set as its saved row.
    EquipmentSetSaveRow EquipmentSetRow(EquipmentSet const& eqset, uint32 index)
    {
        static_assert(EQUIPMENT_SET_ROW_ITEMS == EQUIPMENT_SLOT_END, "a saved set holds every equipment slot");

        EquipmentSetSaveRow row;
        row.setGuid = eqset.Guid;
        row.index = index;
        row.name = eqset.Name;
        row.iconName = eqset.IconName;
        row.ignoreMask = eqset.IgnoreMask;
        for (int i = 0; i < EQUIPMENT_SLOT_END; ++i)
        {
            row.items[i] = eqset.Items[i];
        }
        return row;
    }
}

/*********************************************************/
/***                   SAVE SYSTEM                     ***/
/*********************************************************/

void Player::SaveToDB()
{
    // we should assure this: ASSERT((m_nextSave != sGameConfig.getConfig(CONFIG_UINT32_INTERVAL_SAVE)));
    // delay auto save at any saves (manual, in code, or autosave)
    m_nextSave = sGameConfig.getConfig(CONFIG_UINT32_INTERVAL_SAVE);

    // lets allow only players in world to be saved
    if (IsBeingTeleportedFar())
    {
        ScheduleDelayedOperation(DELAYED_SAVE_PLAYER);
        return;
    }

    // first save/honor gain after midnight will also update the player's honor fields
    UpdateHonorKills();

    DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "The value of player %s at save: ", m_name.c_str());
    outDebugStatsValues();

    CharacterTransaction::Begin();

    CharacterSaveRow row;
    row.guid = GetGUIDLow();
    row.account = GetSession()->GetAccountId();
    row.name = m_name;
    row.race = getRace();
    row.playerClass = getClass();
    row.gender = getGender();
    row.level = getLevel();
    row.xp = GetUInt32Value(PLAYER_XP);
    row.money = GetMoney();
    row.playerBytes = GetUInt32Value(PLAYER_BYTES);
    row.playerBytes2 = GetUInt32Value(PLAYER_BYTES_2);
    row.playerFlags = GetUInt32Value(PLAYER_FLAGS);

    if (!IsBeingTeleported())
    {
        // What goes in this row is what the client will be handed at the next login, so it
        // is the map the ship SAILS and a point on it -- never the deck map, which the
        // client cannot load. Where he actually stands is the deck position saved in the
        // transport columns below, and that is the one that survives the voyage intact.
        //
        // The vessel is taken from the boarding relationship when it is intact, and from
        // the map otherwise: a GM `.tele` onto a hull puts him there with no transport at
        // all. Written once, a deck map in this column is unloadable for ever.
        uint32 savedMap = GetMapId();
        float savedX = Where().X(), savedY = Where().Y();
        float savedZ = Where().Z(), savedO = Where().Facing();

        Transport* vessel = m_transport;
        if (!vessel)
        {
            if (Map* on = FindMap())
            {
                if (TransportMap* hull = on->AsTransport())
                {
                    vessel = hull->Vessel();
                }
            }
        }

        if (vessel)
        {
            savedMap = vessel->GetMapId();
            savedX = vessel->Where().X();
            savedY = vessel->Where().Y();
            savedZ = vessel->Where().Z();
            savedO = vessel->Where().Facing();
        }

        row.map = savedMap;
        row.dungeonDifficulty = uint32(GetDungeonDifficulty());
        row.positionX = finiteAlways(savedX);
        row.positionY = finiteAlways(savedY);
        row.positionZ = finiteAlways(savedZ);
        row.orientation = finiteAlways(savedO);
    }
    else
    {
        row.map = GetTeleportDest().mapid;
        row.dungeonDifficulty = uint32(GetDungeonDifficulty());
        row.positionX = finiteAlways(GetTeleportDest().coord_x);
        row.positionY = finiteAlways(GetTeleportDest().coord_y);
        row.positionZ = finiteAlways(GetTeleportDest().coord_z);
        row.orientation = finiteAlways(GetTeleportDest().orientation);
    }

    std::ostringstream ss;
    ss << m_taxi;                                   // string with TaxiMaskSize numbers
    row.taxiMask = ss.str();
    ss.str("");

    row.online = IsInWorld() ? 1 : 0;

    row.cinematic = m_cinematic;

    row.totalTime = m_Played_time[PLAYED_TIME_TOTAL];
    row.levelTime = m_Played_time[PLAYED_TIME_LEVEL];

    row.restBonus = finiteAlways(m_rest_bonus);
    row.logoutTime = uint64(time(NULL));
    row.isLogoutResting = HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) ? 1 : 0;
    row.resetTalentsCost = m_talentMgr.ResetCost();
    row.resetTalentsTime = uint64(m_talentMgr.ResetTime());
    for (int i = 0; i < MAX_TALENT_SPEC_COUNT; ++i)
    {
        ss << m_talentMgr.PrimaryTree(i) << " ";
    }
    row.primaryTrees = ss.str();
    ss.str("");

    row.transX = finiteAlways(m_movementInfo.GetTransportPos()->x);
    row.transY = finiteAlways(m_movementInfo.GetTransportPos()->y);
    row.transZ = finiteAlways(m_movementInfo.GetTransportPos()->z);
    row.transO = finiteAlways(m_movementInfo.GetTransportPos()->o);
    row.transGuid = m_transport ? m_transport->GetGUIDLow() : 0;

    row.extraFlags = m_ExtraFlags;

    row.stableSlots = uint32(m_petMgr.GetStableSlots());   // to prevent save uint8 as char

    row.atLogin = uint32(m_atLoginFlags);

    row.zone = IsInWorld() ? GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z()) : GetCachedZoneId();

    row.deathExpireTime = uint64(m_deathExpireTime);

    ss << m_taxi.SaveTaxiDestinationsToString();       // string
    row.taxiPath = ss.str();
    ss.str("");

    row.totalKills = GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS);

    row.todayKills = GetUInt16Value(PLAYER_FIELD_KILLS, 0);

    row.yesterdayKills = GetUInt16Value(PLAYER_FIELD_KILLS, 1);

    row.chosenTitle = GetUInt32Value(PLAYER_CHOSEN_TITLE);

    // FIXME: at this moment send to DB as unsigned, including unit32(-1)
    row.watchedFaction = GetUInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX);

    row.drunk = uint8(GetDrunkValue());

    row.health = GetHealth();

    for (uint32 i = 0; i < MAX_STORED_POWERS; ++i)  // power1 to power5
    {
        row.power[i] = GetPowerByIndex(i);
    }

    row.specCount = uint32(m_talentMgr.SpecsCount());
    row.activeSpec = uint32(m_talentMgr.ActiveSpec());

    for (uint32 i = 0; i < PLAYER_EXPLORED_ZONES_SIZE; ++i) // string
    {
        ss << GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + i) << " ";
    }
    row.exploredZones = ss.str();
    ss.str("");

    for (uint32 i = 0; i < EQUIPMENT_SLOT_END * 2; ++i)     // string
    {
        ss << GetUInt32Value(PLAYER_VISIBLE_ITEM_1_ENTRYID + i) << " ";
    }
    row.equipmentCache = ss.str();
    ss.str("");

    for (uint32 i = 0; i < KNOWN_TITLES_SIZE * 2; ++i)      // string
    {
        ss << GetUInt32Value(PLAYER__FIELD_KNOWN_TITLES + i) << " ";
    }
    row.knownTitles = ss.str();
    ss.str("");

    row.actionBars = uint32(GetByteValue(PLAYER_FIELD_BYTES, 2));

    row.slot = m_slot;

    row.createdDate = GetCreatedDate();

    CharacterSaveRepository::ReplaceCharacter(row);

    if (m_mailsUpdated)                                     // save mails only when needed
    {
        _SaveMail();
    }

    _SaveBGData();
    _SaveInventory();
    m_questStatusMgr.SaveStatus(GetGUIDLow(), WorldGameTime::Get());
    _SaveDailyQuestStatus();
    m_questStatusMgr.SaveWeekly(GetGUIDLow());
    m_questStatusMgr.SaveMonthly(GetGUIDLow());
    _SaveSpells();
    _SaveSpellCooldowns();
    _SaveActions();
    _SaveAuras();
    _SaveSkills();
    m_achievementMgr->SaveToDB();
    m_reputationMgr.SaveToDB(GetGUIDLow());
    _SaveCurrencies();
    _SaveEquipmentSets();
    GetSession()->SaveTutorialsData();                      // changed only while character in game
    _SaveGlyphs();
    _SaveCUFProfiles();
    m_talentMgr.SaveTalents(GetGUIDLow());

    CharacterTransaction::Commit();

    // check if stats should only be saved on logout
    // save stats can be out of transaction
    if (m_session->isLogingOut() || !sGameConfig.getConfig(CONFIG_BOOL_STATS_SAVE_ONLY_ON_LOGOUT))
    {
        _SaveStats();
    }

    // save pet (hunter pet level and experience and all type pets health/mana).
    if (Pet* pet = GetPet())
    {
        pet->SavePetToDB(PET_SAVE_AS_CURRENT);
    }
}

// fast save function for item/money cheating preventing - save only inventory and money state
void Player::SaveInventoryAndGoldToDB()
{
    _SaveInventory();
    SaveGoldToDB();
}

/**
 * @brief Persists the player's current money value to the database.
 */
void Player::SaveGoldToDB()
{
    CharacterSaveRepository::SetMoney(GetMoney(), GetGUIDLow());
}

/**
 * @brief Saves changed action bar bindings to the database.
 */
void Player::_SaveActions()
{
    for (int i = 0; i < MAX_TALENT_SPEC_COUNT; ++i)
    {
        for (ActionButtonList::iterator itr = m_actionButtons[i].begin(); itr != m_actionButtons[i].end();)
        {
            switch (itr->second.uState)
            {
                case ACTIONBUTTON_NEW:
                    CharacterSaveRepository::InsertAction(GetGUIDLow(), i, uint32(itr->first), itr->second.GetAction(), uint32(itr->second.GetType()));
                    itr->second.uState = ACTIONBUTTON_UNCHANGED;
                    ++itr;
                    break;
                case ACTIONBUTTON_CHANGED:
                    CharacterSaveRepository::UpdateAction(GetGUIDLow(), i, uint32(itr->first), itr->second.GetAction(), uint32(itr->second.GetType()));
                    itr->second.uState = ACTIONBUTTON_UNCHANGED;
                    ++itr;
                    break;
                case ACTIONBUTTON_DELETED:
                    CharacterSaveRepository::DeleteAction(GetGUIDLow(), i, uint32(itr->first));
                    m_actionButtons[i].erase(itr++);
                    break;
                default:
                    ++itr;
                    break;
            }
        }
    }
}

/**
 * @brief Saves eligible active aura state to the database.
 */
void Player::_SaveAuras()
{
    static_assert(CHARACTER_AURA_ROW_EFFECTS == MAX_EFFECT_INDEX, "a saved aura holds every effect of its spell");

    std::vector<CharacterAuraSaveRow> rows;

    SpellAuraHolderMap const& auraHolders = GetSpellAuraHolderMap();
    for (SpellAuraHolderMap::const_iterator itr = auraHolders.begin(); itr != auraHolders.end(); ++itr)
    {
        SpellAuraHolder* holder = itr->second;
        // skip all holders from spells that are passive or channeled
        // save singleTarget auras if self cast.
        bool selfCastHolder = holder->GetCasterGuid() == GetObjectGuid();
        TrackedAuraType trackedType = holder->GetTrackedAuraType();
        if (!holder->IsPassive() && !IsChanneledSpell(holder->GetSpellProto()) &&
           (trackedType == TRACK_AURA_TYPE_NOT_TRACKED || (trackedType == TRACK_AURA_TYPE_SINGLE_TARGET && selfCastHolder)))
        {
            CharacterAuraSaveRow row;

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if (Aura* aur = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                {
                    // don't save not own area auras
                    if (aur->IsAreaAura() && holder->GetCasterGuid() != GetObjectGuid())
                    {
                        continue;
                    }

                    row.basePoints[i] = aur->GetModifier()->m_amount;
                    row.periodicTime[i] = aur->GetModifier()->periodictime;
                    row.effIndexMask |= (1 << i);
                }
            }

            if (!row.effIndexMask)
            {
                continue;
            }

            row.casterGuid = holder->GetCasterGuid().GetRawValue();
            row.itemGuid = holder->GetCastItemGuid().GetCounter();
            row.spell = holder->GetId();
            row.stackCount = holder->GetStackAmount();
            row.remainCharges = uint8(holder->GetAuraCharges());
            row.maxDuration = holder->GetAuraMaxDuration();
            row.remainTime = holder->GetAuraDuration();
            rows.push_back(row);
        }
    }

    CharacterSaveRepository::ReplaceAuras(GetGUIDLow(), rows);
}

// Player::_SaveGlyphs moved to GlyphMgr::Save (2026-05-12); thin delegating wrapper lives inline in Player.h.

/**
 * @brief Writes the character's raid-frame profiles, but only if the client changed them.
 */
void Player::_SaveCUFProfiles()
{
    if (!m_cufProfilesChanged)
    {
        return;
    }

    std::vector<CUFProfileSaveRow> rows;
    for (uint8 i = 0; i < MAX_CUF_PROFILES; ++i)
    {
        CUFProfile const* profile = m_cufProfiles[i].get();
        if (!profile)
        {
            continue;
        }

        CUFProfileSaveRow row;
        row.id = i;
        row.name = profile->ProfileName;
        row.frameHeight = profile->FrameHeight;
        row.frameWidth = profile->FrameWidth;
        row.sortBy = profile->SortBy;
        row.healthText = profile->HealthText;
        row.boolOptions = uint32(profile->BoolOptions.to_ulong());
        row.topPoint = profile->TopPoint;
        row.bottomPoint = profile->BottomPoint;
        row.leftPoint = profile->LeftPoint;
        row.topOffset = profile->TopOffset;
        row.bottomOffset = profile->BottomOffset;
        row.leftOffset = profile->LeftOffset;
        rows.push_back(row);
    }

    CharacterSaveRepository::ReplaceCUFProfiles(GetGUIDLow(), rows);

    m_cufProfilesChanged = false;
}

/**
 * @brief Saves inventory state changes and queued item records to the database.
 */
void Player::_SaveInventory()
{
    // force items in buyback slots to new state
    // and remove those that aren't already
    for (uint8 i = BUYBACK_SLOT_START; i < BUYBACK_SLOT_END; ++i)
    {
        Item* item = m_inventoryMgr.Slot(i);
        if (!item || item->GetState() == ITEM_NEW)
        {
            continue;
        }

        ItemRepository::DeleteInventoryRow(item->GetGUIDLow());
        ItemRepository::DeleteInstance(item->GetGUIDLow());

        m_inventoryMgr.Slot(i)->FSetState(ITEM_NEW);
    }

    // update enchantment durations
    for (EnchantDurationList::const_iterator itr = m_enchantDuration.begin(); itr != m_enchantDuration.end(); ++itr)
    {
        itr->item->SetEnchantmentDuration(itr->slot, itr->leftduration);
    }

    // if no changes
    if (m_itemUpdateQueue.empty())
    {
        return;
    }

    // do not save if the update queue is corrupt
    bool error = false;
    for (size_t i = 0; i < m_itemUpdateQueue.size(); ++i)
    {
        Item* item = m_itemUpdateQueue[i];
        if (!item || item->GetState() == ITEM_REMOVED)
        {
            continue;
        }
        Item* test = m_inventoryMgr.GetItemByPos(item->GetBagSlot(), item->GetSlot());

        if (test == NULL)
        {
            sLog.outError("Player(GUID: %u Name: %s)::_SaveInventory - the bag(%d) and slot(%d) values for the item with guid %d are incorrect, the player doesn't have an item at that position!", GetGUIDLow(), GetName(), item->GetBagSlot(), item->GetSlot(), item->GetGUIDLow());
            error = true;
        }
        else if (test != item)
        {
            sLog.outError("Player(GUID: %u Name: %s)::_SaveInventory - the bag(%d) and slot(%d) values for the item with guid %d are incorrect, the item with guid %d is there instead!", GetGUIDLow(), GetName(), item->GetBagSlot(), item->GetSlot(), item->GetGUIDLow(), test->GetGUIDLow());
            error = true;
        }
    }

    if (error)
    {
        sLog.outError("Player::_SaveInventory - one or more errors occurred save aborted!");
        ReportSystemMessage(LANG_ITEM_SAVE_FAILED);
        return;
    }

    for (size_t i = 0; i < m_itemUpdateQueue.size(); ++i)
    {
        Item* item = m_itemUpdateQueue[i];
        if (!item)
        {
            continue;
        }

        Bag* container = item->GetContainer();
        uint32 bag_guid = container ? container->GetGUIDLow() : 0;

        switch (item->GetState())
        {
            case ITEM_NEW:
                ItemRepository::InsertInventoryRow(GetGUIDLow(), bag_guid, item->GetSlot(), item->GetGUIDLow(), item->GetEntry());
                break;
            case ITEM_CHANGED:
                ItemRepository::UpdateInventoryRow(GetGUIDLow(), bag_guid, item->GetSlot(), item->GetGUIDLow(), item->GetEntry());
                break;
            case ITEM_REMOVED:
                ItemRepository::DeleteInventoryRow(item->GetGUIDLow());
                break;
            case ITEM_UNCHANGED:
                break;
        }

        item->SaveToDB();                                   // item have unchanged inventory record and can be save standalone
    }
    m_itemUpdateQueue.clear();
}

/**
 * @brief Saves changed mail records and deletes removed mail from the database.
 */
void Player::_SaveMail()
{
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end(); ++itr)
    {
        Mail* m = (*itr);
        if (m->state == MAIL_STATE_CHANGED)
        {
            MailRepository::UpdateState(m->messageID, m->HasItems() ? 1 : 0, uint64(m->expire_time), uint64(m->deliver_time),
                                        uint32(m->money), uint32(m->COD), m->checked);

            if (!m->removedItems.empty())
            {
                MailRepository::DeleteTakenItems(m->removedItems);
                m->removedItems.clear();
            }
            m->state = MAIL_STATE_UNCHANGED;
        }
        else if (m->state == MAIL_STATE_DELETED)
        {
            if (m->HasItems())
            {
                for (MailItemInfoVec::const_iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                {
                    ItemRepository::DeleteInstance(itr2->item_guid);
                }
            }

            MailRepository::DeleteDeletedMail(m->messageID);
        }
    }

    // deallocate deleted mails...
    for (PlayerMails::iterator itr = m_mail.begin(); itr != m_mail.end();)
    {
        if ((*itr)->state == MAIL_STATE_DELETED)
        {
            Mail* m = *itr;
            m_mail.erase(itr);
            delete m;
            itr = m_mail.begin();
        }
        else
        {
            ++itr;
        }
    }

    m_mailsUpdated = false;
}

void Player::_SaveDailyQuestStatus()
{
    if (!m_DailyQuestChanged)
    {
        return;
    }

    std::vector<uint32> quests;
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
    {
        if (GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx))
        {
            quests.push_back(GetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx));
        }
    }
    QuestStatusRepository::SaveDaily(GetGUIDLow(), quests);
    m_DailyQuestChanged = false;
}

/**
 * @brief Saves skill value changes to the database.
 */
void Player::_SaveSkills()
{
    // we don't need transactions here.
    for (SkillStatusMap::iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end();)
    {
        if (itr->second.uState == SKILL_UNCHANGED)
        {
            ++itr;
            continue;
        }

        if (itr->second.uState == SKILL_DELETED)
        {
            CharacterSaveRepository::DeleteSkill(GetGUIDLow(), itr->first);
            mSkillStatus.erase(itr++);
            continue;
        }

        uint16 field = itr->second.pos / 2;
        uint8 offset = itr->second.pos & 1;

        uint16 value = GetUInt16Value(PLAYER_SKILL_RANK_0 + field, offset);
        uint16 max = GetUInt16Value(PLAYER_SKILL_MAX_RANK_0 + field, offset);

        switch (itr->second.uState)
        {
            case SKILL_NEW:
                CharacterSaveRepository::InsertSkill(GetGUIDLow(), itr->first, value, max);
                break;
            case SKILL_CHANGED:
                CharacterSaveRepository::UpdateSkill(GetGUIDLow(), itr->first, value, max);
                break;
            case SKILL_UNCHANGED:
            case SKILL_DELETED:
                MANGOS_ASSERT(false);
                break;
        };
        itr->second.uState = SKILL_UNCHANGED;

        ++itr;
    }
}

/**
 * @brief Saves learned spell state changes to the database.
 */
void Player::_SaveSpells()
{
    for (PlayerSpellMap::iterator itr = m_spells.begin(), next = m_spells.begin(); itr != m_spells.end();)
    {
        uint32 talentCosts = GetTalentSpellCost(itr->first);

        if (!talentCosts)
        {
            if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.state == PLAYERSPELL_CHANGED)
            {
                CharacterSaveRepository::DeleteSpell(GetGUIDLow(), itr->first);
            }

            // add only changed/new not dependent spells
            if (!itr->second.dependent && (itr->second.state == PLAYERSPELL_NEW || itr->second.state == PLAYERSPELL_CHANGED))
            {
                CharacterSaveRepository::InsertSpell(GetGUIDLow(), itr->first, uint8(itr->second.active ? 1 : 0), uint8(itr->second.disabled ? 1 : 0));
            }
        }

        if (itr->second.state == PLAYERSPELL_REMOVED)
        {
            m_spells.erase(itr++);
        }
        else
        {
            itr->second.state = PLAYERSPELL_UNCHANGED;
            ++itr;
        }
    }
}

// save player stats -- only for external usage
// real stats will be recalculated on player login
void Player::_SaveStats()
{
    // check if stat saving is enabled and if char level is high enough
    if (!sGameConfig.getConfig(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE) || getLevel() < sGameConfig.getConfig(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE))
    {
        return;
    }

    static_assert(CHARACTER_SAVE_ROW_POWERS == MAX_STORED_POWERS, "a saved row holds every stored power");
    static_assert(CHARACTER_STATS_ROW_STATS == MAX_STATS, "a saved row holds every base stat");
    static_assert(CHARACTER_STATS_ROW_SCHOOLS == MAX_SPELL_SCHOOL, "a saved row holds every spell school");

    CharacterStatsSaveRow row;
    row.maxHealth = GetMaxHealth();
    for (uint32 i = 0; i < MAX_STORED_POWERS; ++i)
    {
        row.maxPower[i] = GetMaxPowerByIndex(i);
    }
    for (int i = 0; i < MAX_STATS; ++i)
    {
        row.stat[i] = GetStat(Stats(i));
    }
    // armor + school resistances
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        row.resistance[i] = uint32(GetResistance(SpellSchools(i)));
    }
    row.blockPct = GetFloatValue(PLAYER_BLOCK_PERCENTAGE);
    row.dodgePct = GetFloatValue(PLAYER_DODGE_PERCENTAGE);
    row.parryPct = GetFloatValue(PLAYER_PARRY_PERCENTAGE);
    row.critPct = GetFloatValue(PLAYER_CRIT_PERCENTAGE);
    row.rangedCritPct = GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE);
    row.spellCritPct = GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1);
    row.attackPower = GetUInt32Value(UNIT_FIELD_ATTACK_POWER);
    row.rangedAttackPower = GetUInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER);
    row.spellPower = GetBaseSpellPowerBonus();

    CharacterSaveRepository::ReplaceStats(GetGUIDLow(), row);
}

/**
 * @brief Writes a character position directly to the database.
 *
 * @param guid The character GUID to update.
 * @param mapid The destination map identifier.
 * @param x The X coordinate.
 * @param y The Y coordinate.
 * @param z The Z coordinate.
 * @param o The orientation.
 * @param zone The zone identifier.
 */
void Player::SavePositionInDB(ObjectGuid guid, uint32 mapid, float x, float y, float z, float o, uint32 zone)
{
    std::string statement = CharacterSaveRepository::SetPosition(guid.GetCounter(), mapid, x, y, z, o, zone);
    DEBUG_LOG("%s", statement.c_str());

    // Decoupling D7c: this is the one place a `characters` zone is written for a character
    // who is not logged in (a GM teleporting an offline target), so it is a setter too.
    sCharacterCache.UpdateZone(guid, zone);
}

void Player::_SaveEquipmentSets()
{
    for (EquipmentSets::iterator itr = m_EquipmentSets.begin(); itr != m_EquipmentSets.end();)
    {
        uint32 index = itr->first;
        EquipmentSet& eqset = itr->second;
        switch (eqset.state)
        {
            case EQUIPMENT_SET_UNCHANGED:
                ++itr;
                break;                                      // nothing do
            case EQUIPMENT_SET_CHANGED:
            {
                CharacterSaveRepository::UpdateEquipmentSet(GetGUIDLow(), EquipmentSetRow(eqset, index));

                eqset.state = EQUIPMENT_SET_UNCHANGED;
                ++itr;
                break;
            }
            case EQUIPMENT_SET_NEW:
            {
                CharacterSaveRepository::InsertEquipmentSet(GetGUIDLow(), EquipmentSetRow(eqset, index));

                eqset.state = EQUIPMENT_SET_UNCHANGED;
                ++itr;
                break;
            }
            case EQUIPMENT_SET_DELETED:
            {
                CharacterSaveRepository::DeleteEquipmentSet(eqset.Guid);
                m_EquipmentSets.erase(itr++);
                break;
            }
        }
    }
}

/**
 * @brief Saves battleground return position and instance data to the database.
 */
void Player::_SaveBGData()
{
    // nothing save
    if (!m_bgData.m_needSave)
    {
        return;
    }

    if (m_bgData.bgInstanceID)
    {
        BattlegroundDataSaveRow row;
        row.instanceId = m_bgData.bgInstanceID;
        row.team = uint32(m_bgData.bgTeam);
        row.x = m_bgData.joinPos.coord_x;
        row.y = m_bgData.joinPos.coord_y;
        row.z = m_bgData.joinPos.coord_z;
        row.o = m_bgData.joinPos.orientation;
        row.map = m_bgData.joinPos.mapid;
        row.taxiStart = m_bgData.taxiPath[0];
        row.taxiEnd = m_bgData.taxiPath[1];
        row.mountSpell = m_bgData.mountSpell;
        CharacterSaveRepository::ReplaceBattlegroundData(GetGUIDLow(), &row);
    }
    else
    {
        CharacterSaveRepository::ReplaceBattlegroundData(GetGUIDLow(), NULL);
    }

    m_bgData.m_needSave = false;
}
