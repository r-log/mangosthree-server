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
 * @file PlayerLoad.cpp
 * @brief Player's _Load* database helpers, LoadPositionFromDB, LoadCorpse and LoadPet, the instance-binding lifecycle
 * and the raid-info and saved-instance packet senders.
 */

#include "Utilities/Errors.h"
#include <string>
#include <map>
#include <list>
#include "Player.h"
#include "data/Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "spells/data/SpellMgr.h"
#include "WorldGameTime.h"
#include "entities/player/world/PlayerWorldNotices.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "GridNotifiersImpl.h"
#include "ObjectMgr.h"
#include "Bag.h"
#include "CorpseManager.h"
#include "Group.h"
#include "Pet.h"
#include "Util.h"
#include "ArenaTeam.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "Mail.h"
#include "spells/auras/SpellAuras.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "Vehicle.h"
#include "Calendar.h"
#include "DisableMgr.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/CharacterSaveRepository.h"
#include "persistence/characters/inventory/ItemRepository.h"
#include "persistence/characters/mail/MailRepository.h"
#include "persistence/characters/maps/InstanceBindRepository.h"
#include "persistence/characters/talents/TalentRepository.h"
#include "persistence/characters/quests/QuestStatusRepository.h"

static_assert(int(LOGIN_DECLINED_NAME_ROW_CASES) == int(MAX_DECLINED_NAME_CASES), "a declined name row holds every case");
static_assert(int(LOGIN_EQUIPMENT_SET_ROW_ITEMS) == int(EQUIPMENT_SLOT_END), "an equipment set row holds every equipment slot");
static_assert(int(LOGIN_AURA_ROW_EFFECTS) == int(MAX_EFFECT_INDEX), "an aura row holds every effect");

// corpse reclaim times -- file-local in Player.cpp; mirrored here so
// LoadFromDB can clamp m_deathExpireTime.
#define DEATH_EXPIRE_STEP (5*MINUTE)
#define MAX_DEATH_COUNT 3

/*********************************************************/
/***                   LOAD SYSTEM                     ***/
/*********************************************************/

void Player::_LoadDeclinedNames(std::optional<LoginDeclinedNameRow> const& row)
{
    if (!row)
    {
        return;
    }

    delete m_declinedname;
    m_declinedname = new DeclinedName;

    for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
    {
        m_declinedname->name[i] = row->name[i];
    }
}

void Player::_LoadArenaTeamInfo(std::vector<LoginArenaTeamInfoRow> const& rows)
{
    // arenateamid, played_week, played_season, personal_rating
    memset((void*)&m_uint32Values[PLAYER_FIELD_ARENA_TEAM_INFO_1_1], 0, sizeof(uint32) * MAX_ARENA_SLOT * ARENA_TEAM_END);
    if (rows.empty())
    {
        return;
    }

    for (LoginArenaTeamInfoRow const& row : rows)
    {
        uint32 arenateamid     = row.arenateamid;
        uint32 played_week     = row.playedWeek;
        uint32 played_season   = row.playedSeason;
        uint32 wons_season     = row.wonsSeason;
        uint32 personal_rating = row.personalRating;

        ArenaTeam* aTeam = sArenaTeamRegistry.Find(arenateamid);
        if (!aTeam)
        {
            sLog.outError("Player::_LoadArenaTeamInfo: couldn't load arenateam %u", arenateamid);
            continue;
        }
        uint8  arenaSlot = aTeam->GetSlot();

        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_ID, arenateamid);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_TYPE, aTeam->GetType());
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_MEMBER, (aTeam->GetCaptainGuid() == GetObjectGuid()) ? 0 : 1);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_GAMES_WEEK, played_week);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_GAMES_SEASON, played_season);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_WINS_SEASON, wons_season);
        SetArenaTeamInfoField(arenaSlot, ARENA_TEAM_PERSONAL_RATING, personal_rating);
    }
}

void Player::_LoadEquipmentSets(std::vector<LoginEquipmentSetRow> const& rows)
{
    if (rows.empty())
    {
        return;
    }

    uint32 count = 0;
    for (LoginEquipmentSetRow const& row : rows)
    {
        EquipmentSet eqSet;

        eqSet.Guid          = row.setGuid;
        uint32 index        = row.setIndex;
        eqSet.Name          = row.name;
        eqSet.IconName      = row.iconName;
        eqSet.IgnoreMask    = row.ignoreMask;
        eqSet.state         = EQUIPMENT_SET_UNCHANGED;

        for (uint32 i = 0; i < EQUIPMENT_SLOT_END; ++i)
        {
            eqSet.Items[i] = row.items[i];
        }

        m_EquipmentSets[index] = eqSet;

        ++count;

        if (count >= MAX_EQUIPMENT_SET_INDEX)               // client limit
        {
            break;
        }
    }
}

/**
 * @brief Loads battleground return and participation data from the database.
 *
 * @param row The battleground data.
 */
void Player::_LoadBGData(std::optional<LoginBattlegroundDataRow> const& row)
{
    if (!row)
    {
        return;
    }

    // Expecting only one row
    /* bgInstanceID, bgTeam, x, y, z, o, map, taxi[0], taxi[1], mountSpell */
    m_bgData.bgInstanceID = row->instanceId;
    m_bgData.bgTeam       = Team(row->team);
    m_bgData.joinPos      = WorldLocation(row->joinMap,    // Map
                                          row->joinX,     // X
                                          row->joinY,     // Y
                                          row->joinZ,     // Z
                                          row->joinO);    // Orientation
    m_bgData.taxiPath[0]  = row->taxiStart;
    m_bgData.taxiPath[1]  = row->taxiEnd;
    m_bgData.mountSpell   = row->mountSpell;
}

/**
 * @brief Loads a character position directly from the database.
 *
 * @param guid The player GUID to query.
 * @param mapid Output map identifier.
 * @param x Output X coordinate.
 * @param y Output Y coordinate.
 * @param z Output Z coordinate.
 * @param o Output orientation.
 * @param in_flight Output flag indicating whether the character was in flight.
 * @return True if the position was loaded successfully; otherwise, false.
 */
bool Player::LoadPositionFromDB(ObjectGuid guid, uint32& mapid, float& x, float& y, float& z, float& o, bool& in_flight)
{
    std::optional<CharacterPositionRow> position = CharacterRepository::QueryPosition(guid.GetCounter());
    if (!position)
    {
        return false;
    }

    x = position->positionX;
    y = position->positionY;
    z = position->positionZ;
    o = position->orientation;
    mapid = position->map;
    in_flight = !position->taxiPath.empty();
    return true;
}

/**
 * @brief Loads serialized uint32 values into the player's update field array.
 *
 * @param data The serialized space-separated field data.
 * @param startOffset The first update-field offset to populate.
 * @param count The number of uint32 values expected.
 */
void Player::_LoadIntoDataField(const char* data, uint32 startOffset, uint32 count)
{
    if (!data)
    {
        return;
    }

    Tokens tokens = StrSplit(data, " ");

    if (tokens.size() != count)
    {
        return;
    }

    Tokens::iterator iter;
    uint32 index;
    for (iter = tokens.begin(), index = 0; index < count; ++iter, ++index)
    {
        m_uint32Values[startOffset + index] = std::stoul((*iter).c_str());
    }
}

/**
 * @brief Loads action bar bindings from the database.
 *
 * @param rows The action bindings.
 */
void Player::_LoadActions(std::vector<LoginActionRow> const& rows)
{
    for (int i = 0; i < MAX_TALENT_SPEC_COUNT; ++i)
    {
        m_actionButtons[i].clear();
    }


    if (!rows.empty())
    {
        for (LoginActionRow const& row : rows)
        {
            uint8 spec = row.spec;
            uint8 button = row.button;
            uint32 action = row.action;
            uint8 type = row.type;

            if (ActionButton* ab = addActionButton(spec, button, action, type))
            {
                ab->uState = ACTIONBUTTON_UNCHANGED;
            }
            else if ((type == ACTION_BUTTON_SPELL && !sSpellStore.LookupEntry(action)) ||
                     (type == ACTION_BUTTON_ITEM && !ObjectMgr::GetItemPrototype(action)))
            {
                sLog.outError("  ...at loading, and kept in DB");
            }
            else
            {
                sLog.outError("  ...at loading, and will deleted in DB also");

                // Will deleted in DB at next save (it can create data until save but marked as deleted)
                m_actionButtons[spec][button].uState = ACTIONBUTTON_DELETED;
            }
        }
    }
}

/**
 * @brief Loads saved auras from the database and restores their remaining durations.
 *
 * @param rows The aura rows.
 * @param timediff The elapsed offline time used to age aura durations.
 */
void Player::_LoadAuras(std::vector<LoginAuraRow> const& rows, uint32 timediff)
{
    // RemoveAllAuras(); -- some spells casted before aura load, for example in LoadSkills, aura list explicitly cleaned early


    if (!rows.empty())
    {
        for (LoginAuraRow const& row : rows)
        {
            ObjectGuid caster_guid = ObjectGuid(row.casterGuid);
            uint32 item_lowguid = row.itemGuid;
            uint32 spellid = row.spell;
            uint32 stackcount = row.stackCount;
            uint32 remaincharges = row.remainCharges;
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = row.basePoints[i];
                periodicTime[i] = row.periodicTime[i];
            }

            int32 maxduration = row.maxDuration;
            int32 remaintime = row.remainTime;
            uint32 effIndexMask = row.effIndexMask;

            SpellEntry const* spellproto = sSpellStore.LookupEntry(spellid);
            if (!spellproto)
            {
                sLog.outError("Unknown spell (spellid %u), ignore.", spellid);
                continue;
            }

            if (remaintime != -1 && !IsPositiveSpell(spellproto))
            {
                if (remaintime / IN_MILLISECONDS <= int32(timediff))
                {
                    continue;
                }

                remaintime -= timediff * IN_MILLISECONDS;
            }

            // prevent wrong values of remaincharges
            if (uint32 procCharges = spellproto->GetProcCharges())
            {
                if (remaincharges <= 0 || remaincharges > procCharges)
                {
                    remaincharges = procCharges;
                }
            }
            else
            {
                remaincharges = 0;
            }

            uint32 defstackamount = spellproto->GetStackAmount();
            if (!defstackamount)
            {
                stackcount = 1;
            }
            else if (defstackamount < stackcount)
            {
                stackcount = defstackamount;
            }
            else if (!stackcount)
            {
                stackcount = 1;
            }

            SpellAuraHolder* holder = CreateSpellAuraHolder(spellproto, this, NULL);
            holder->SetLoadedState(caster_guid, ObjectGuid(HIGHGUID_ITEM, item_lowguid), stackcount, remaincharges, maxduration, remaintime);

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if ((effIndexMask & (1 << i)) == 0)
                {
                    continue;
                }

                Aura* aura = CreateAura(spellproto, SpellEffectIndex(i), NULL, holder, this);
                if (!damage[i])
                {
                    damage[i] = aura->GetModifier()->m_amount;
                }

                aura->SetLoadedState(damage[i], periodicTime[i]);
                holder->AddAura(aura, SpellEffectIndex(i));
            }

            if (!holder->IsEmptyHolder())
            {
                // reset stolen single target auras
                if (caster_guid != GetObjectGuid() && holder->GetTrackedAuraType() == TRACK_AURA_TYPE_SINGLE_TARGET)
                {
                    holder->SetTrackedAuraType(TRACK_AURA_TYPE_NOT_TRACKED);
                }

                AddSpellAuraHolder(holder);
                DETAIL_LOG("Added auras from spellid %u", spellproto->ID);
            }
            else
            {
                delete holder;
            }
        }
    }

    if (getClass() == CLASS_WARRIOR && !HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
    {
        CastSpell(this, SPELL_ID_PASSIVE_BATTLE_STANCE, true);
    }
}

// Player::_LoadGlyphs moved to GlyphMgr (2026-05-12); since decoupling D4k the loop lives in talents/PlayerGlyph.cpp and each row in GlyphMgr::LoadRow.

/**
 * @brief Restores the character's raid-frame profiles.
 *
 * \arg \c result
 *   Rows from `character_cuf_profiles`; may be NULL when none are stored.
 */
void Player::_LoadCUFProfiles(std::vector<LoginCufProfileRow> const& rows)
{
    if (rows.empty())
    {
        return;
    }

    for (LoginCufProfileRow const& row : rows)
    {
        uint8 id = row.id;
        if (id >= MAX_CUF_PROFILES)
        {
            sLog.outError("Player::_LoadCUFProfiles: %s has a profile with invalid id %u, max is %u",
                          GetGuidStr().c_str(), id, MAX_CUF_PROFILES);
            continue;
        }

        std::unique_ptr<CUFProfile> profile(new CUFProfile());
        profile->ProfileName  = row.name;
        profile->FrameHeight  = row.frameHeight;
        profile->FrameWidth   = row.frameWidth;
        profile->SortBy       = row.sortBy;
        profile->HealthText   = row.healthText;
        profile->BoolOptions  = std::bitset<CUF_BOOL_OPTIONS_COUNT>(row.boolOptions);
        profile->TopPoint     = row.topPoint;
        profile->BottomPoint  = row.bottomPoint;
        profile->LeftPoint    = row.leftPoint;
        profile->TopOffset    = row.topOffset;
        profile->BottomOffset = row.bottomOffset;
        profile->LeftOffset   = row.leftOffset;

        m_cufProfiles[id] = std::move(profile);
    }

    // straight from the DB, so nothing to write back yet
    m_cufProfilesChanged = false;
}

/**
 * @brief Restores corpse state for a dead player or cleans it up for a living one.
 */
void Player::LoadCorpse()
{
    if (IsAlive())
    {
        sCorpseManager.ConvertCorpseForPlayer(GetObjectGuid());
    }
    else
    {
        if (Corpse* corpse = GetCorpse())
        {
            ApplyModByteFlag(PLAYER_FIELD_BYTES, 0, PLAYER_FIELD_BYTE_RELEASE_TIMER, corpse && !sMapStore.LookupEntry(corpse->GetMapId())->Instanceable());
        }
        else
        {
            // Prevent Dead Player login without corpse
            ResurrectPlayer(0.5f);
        }
    }
}

/**
 * @brief Loads inventory, bank, and equipped items from the database.
 *
 * @param rows The inventory rows, ordered by bag.
 * @param timediff The elapsed offline time used for time-sensitive item checks.
 */
void Player::_LoadInventory(std::vector<LoginInventoryRow> const& rows, uint32 timediff)
{
    std::map<uint32, Bag*> bagMap;                          // fast guid lookup for bags
    // NOTE: the "order by `bag`" is important because it makes sure
    // the bagMap is filled before items in the bags are loaded
    // NOTE2: the "order by `slot`" is needed because mainhand weapons are (wrongly?)
    // expected to be equipped before offhand items (TODO: fixme)

    uint32 zone = GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z());

    if (!rows.empty())
    {
        std::list<Item*> problematicItems;

        // prevent items from being added to the queue when stored
        m_itemUpdateQueueBlocked = true;
        for (LoginInventoryRow const& row : rows)
        {
            uint32 bag_guid  = row.bag;
            uint8  slot      = row.slot;
            uint32 item_lowguid = row.item;
            uint32 item_id   = row.itemTemplate;

            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_id);

            if (!proto)
            {
                ItemRepository::DropInventoryRow(item_lowguid);
                sLog.outError("Player::_LoadInventory: Player %s has an unknown item (GUID: %u id: #%u) in inventory, kept in `item_instance`, its slot freed.", GetName(), item_lowguid, item_id);
                continue;
            }

            Item* item = NewItemOrBag(proto);

            if (!item->LoadFromDB(item_lowguid, row.instance, GetObjectGuid()))
            {
                sLog.outError("Player::_LoadInventory: Player %s has broken item (id: #%u) in inventory, deleted.", GetName(), item_id);
                ItemRepository::DropInventoryRow(item_lowguid);
                item->FSetState(ITEM_REMOVED);
                item->SaveToDB();                           // it also deletes item object !
                continue;
            }

            // not allow have in alive state item limited to another map/zone
            if (IsAlive() && item->IsLimitedToAnotherMapOrZone(GetMapId(), zone))
            {
                ItemRepository::DropInventoryRow(item_lowguid);
                item->FSetState(ITEM_REMOVED);
                item->SaveToDB();                           // it also deletes item object !
                continue;
            }

            // "Conjured items disappear if you are logged out for more than 15 minutes"
            if (timediff > 15 * MINUTE && (item->GetProto()->Flags & ITEM_FLAG_CONJURED))
            {
                ItemRepository::DropInventoryRow(item_lowguid);
                item->FSetState(ITEM_REMOVED);
                item->SaveToDB();                           // it also deletes item object !
                continue;
            }

            bool success = true;

            // the item/bag is not in a bag
            if (!bag_guid)
            {
                item->SetContainer(NULL);
                item->SetSlot(slot);

                if (InventoryMgr::IsInventoryPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    ItemPosCountVec dest;
                    if (CanStoreItem(INVENTORY_SLOT_BAG_0, slot, dest, item, false) == EQUIP_ERR_OK)
                    {
                        item = StoreItem(dest, item, true);
                    }
                    else
                    {
                        success = false;
                    }
                }
                else if (InventoryMgr::IsEquipmentPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    uint16 dest;
                    if (CanEquipItem(slot, dest, item, false, false) == EQUIP_ERR_OK)
                    {
                        QuickEquipItem(dest, item);
                    }
                    else
                    {
                        success = false;
                    }
                }
                else if (InventoryMgr::IsBankPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    ItemPosCountVec dest;
                    if (CanBankItem(INVENTORY_SLOT_BAG_0, slot, dest, item, false, false) == EQUIP_ERR_OK)
                    {
                        item = BankItem(dest, item, true);
                    }
                    else
                    {
                        success = false;
                    }
                }

                if (success)
                {
                    // store bags that may contain items in them
                    if (item->IsBag() && InventoryMgr::IsBagPos(item->GetPos()))
                    {
                        bagMap[item_lowguid] = (Bag*)item;
                    }
                }
            }
            // the item/bag in a bag
            else
            {
                item->SetSlot(NULL_SLOT);
                // the item is in a bag, find the bag
                std::map<uint32, Bag*>::const_iterator itr = bagMap.find(bag_guid);
                if (itr != bagMap.end() && slot < itr->second->GetBagSize())
                {
                    ItemPosCountVec dest;
                    if (CanStoreItem(itr->second->GetSlot(), slot, dest, item, false) == EQUIP_ERR_OK)
                    {
                        item = StoreItem(dest, item, true);
                    }
                    else
                    {
                        success = false;
                    }
                }
                else
                {
                    success = false;
                }
            }

            // item's state may have changed after stored
            if (success)
            {
                item->SetState(ITEM_UNCHANGED, this);

                // restore container unchanged state also
                if (item->GetContainer())
                {
                    item->GetContainer()->SetState(ITEM_UNCHANGED, this);
                }

                // recharged mana gem
                if (timediff > 15 * MINUTE && proto->ItemLimitCategory == ITEM_LIMIT_CATEGORY_MANA_GEM)
                {
                    item->RestoreCharges();
                }
            }
            else
            {
                sLog.outError("Player::_LoadInventory: Player %s has item (GUID: %u Entry: %u) can't be loaded to inventory (Bag GUID: %u Slot: %u) by some reason, will send by mail.", GetName(), item_lowguid, item_id, bag_guid, slot);
                ItemRepository::DropInventoryRow(item_lowguid);
                problematicItems.push_back(item);
            }
        }

        m_itemUpdateQueueBlocked = false;

        // send by mail problematic items
        while (!problematicItems.empty())
        {
            std::string subject = GetSession()->GetMangosString(LANG_NOT_EQUIPPED_ITEM);

            // fill mail
            MailDraft draft(subject, "There's were problems with equipping item(s).");

            for (int i = 0; !problematicItems.empty() && i < MAX_MAIL_ITEMS; ++i)
            {
                Item* item = problematicItems.front();
                problematicItems.pop_front();

                draft.AddItem(item);
            }

            draft.SendMailTo(this, MailSender(this, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
        }
    }

    // if (IsAlive())
    _ApplyAllItemMods();
}

/**
 * @brief Loads persisted item loot contents for the player's items.
 *
 * @param rows The saved item loot rows.
 */
void Player::_LoadItemLoot(std::vector<LoginItemLootRow> const& rows)
{
    if (!rows.empty())
    {
        for (LoginItemLootRow const& row : rows)
        {
            uint32 item_guid   = row.itemGuid;

            Item* item = m_inventoryMgr.GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, item_guid));

            if (!item)
            {
                ItemRepository::DropLoot(item_guid);
                sLog.outError("Player::_LoadItemLoot: Player %s has loot for nonexistent item (GUID: %u) in `item_loot`, deleted.", GetName(), item_guid);
                continue;
            }

            item->LoadLootFromDB(row);
        }
    }
}

// load mailed item which should receive current player
void Player::_LoadMailedItems(std::vector<LoginMailedItemRow> const& rows)
{
    if (rows.empty())
    {
        return;
    }

    for (LoginMailedItemRow const& row : rows)
    {
        uint32 mail_id       = row.mailId;
        uint32 item_guid_low = row.itemGuid;
        uint32 item_template = row.itemTemplate;

        Mail* mail = GetMail(mail_id);
        if (!mail)
        {
            continue;
        }
        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_template);

        if (!proto)
        {
            sLog.outError("Player %u has unknown item_template (ProtoType) in mailed items(GUID: %u template: %u) in mail (%u), kept in `item_instance`, taken out of the mail.", GetGUIDLow(), item_guid_low, item_template, mail->messageID);
            MailRepository::DropMailedItem(item_guid_low);
            continue;
        }

        mail->AddItem(item_guid_low, item_template);

        Item* item = NewItemOrBag(proto);

        if (!item->LoadFromDB(item_guid_low, row.instance, GetObjectGuid()))
        {
            sLog.outError("Player::_LoadMailedItems - Item in mail (%u) doesn't exist !!!! - item guid: %u, deleted from mail", mail->messageID, item_guid_low);
            MailRepository::DropMailedItem(item_guid_low);
            item->FSetState(ITEM_REMOVED);
            item->SaveToDB();                               // it also deletes item object !
            continue;
        }

        AddMItem(item);
    }
}

/**
 * @brief Loads the player's mailbox headers from the database.
 *
 * @param rows The mail records.
 */
void Player::_LoadMails(std::vector<LoginMailRow> const& rows)
{
    m_mail.clear();
    if (rows.empty())
    {
        return;
    }

    for (LoginMailRow const& row : rows)
    {
        Mail* m = new Mail;
        m->messageID = row.id;
        m->messageType = row.messageType;
        m->sender = row.sender;
        m->receiverGuid = ObjectGuid(HIGHGUID_PLAYER, row.receiver);
        m->subject = row.subject;
        m->body = row.body;
        m->expire_time = (time_t)row.expireTime;
        m->deliver_time = (time_t)row.deliverTime;
        m->money = row.money;
        m->COD = row.cod;
        m->checked = row.checked;
        m->stationery = row.stationery;
        m->mailTemplateId = row.mailTemplateId;
        m->has_items = row.hasItems;                // true, if mail have items or mail have template and items generated (maybe none)

        if (m->mailTemplateId && !sMailTemplateStore.LookupEntry(m->mailTemplateId))
        {
            sLog.outError("Player::_LoadMail - Mail (%u) have nonexistent MailTemplateId (%u), remove at load", m->messageID, m->mailTemplateId);
            m->mailTemplateId = 0;
        }

        m->state = MAIL_STATE_UNCHANGED;

        m_mail.push_back(m);

        if (m->mailTemplateId && !m->has_items)
        {
            m->prepareTemplateItems(this);
        }
    }
}

/**
 * @brief Fills the per-character pet cache from the login holder.
 *
 * Five results, one per table, all of them this character's. `character_pet` first, because
 * the other four are keyed on a pet id and the statements that fetched them joined through it
 * -- so a row in any of them names a pet this character owns, and there are no orphans to
 * guard against here (the ones in the table are never selected).
 *
 * A character with no pets gets five empty row lists and an empty cache, which is what every
 * lookup used to get from a SELECT that matched nothing.
 */
void Player::_LoadPetCache(SqlQueryHolder* holder)
{
    PlayerPetCache& cache = GetPetCache();
    cache.Clear();

    for (LoginPetRow const& pet : CharacterRepository::LoginPets(holder))
    {
        PetCacheRow row;
        row.id               = pet.id;
        row.entry            = pet.entry;
        row.owner            = pet.owner;
        row.modelId          = pet.modelId;
        row.level            = pet.level;
        row.exp              = pet.exp;
        row.reactState       = pet.reactState;
        row.slot             = pet.slot;
        row.name             = pet.name;
        row.renamed          = pet.renamed;
        row.curHealth        = pet.curHealth;
        row.curMana          = pet.curMana;
        row.abData           = pet.abData;
        row.saveTime         = pet.saveTime;
        row.resetTalentsCost = pet.resetTalentsCost;
        row.resetTalentsTime = pet.resetTalentsTime;
        row.createdBySpell   = pet.createdBySpell;
        row.petType          = pet.petType;

        cache.LoadRow(row);
    }

    for (LoginPetAuraRow const& row : CharacterRepository::LoginPetAuras(holder))
    {
        PetCacheAura aura;
        aura.casterGuid    = row.casterGuid;
        aura.itemGuid      = row.itemGuid;
        aura.spell         = row.spell;
        aura.stackCount    = row.stackCount;
        aura.remainCharges = row.remainCharges;
        for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            aura.basePoints[i]   = row.basePoints[i];
            aura.periodicTime[i] = row.periodicTime[i];
        }
        aura.maxDuration  = row.maxDuration;
        aura.remainTime   = row.remainTime;
        aura.effIndexMask = row.effIndexMask;

        cache.LoadAura(row.petId, aura);
    }

    for (LoginPetSpellRow const& row : CharacterRepository::LoginPetSpells(holder))
    {
        cache.LoadSpell(row.petId, PetCacheSpell(row.spell, row.active));
    }

    for (LoginPetSpellCooldownRow const& row : CharacterRepository::LoginPetSpellCooldowns(holder))
    {
        cache.LoadCooldown(row.petId, PetCacheCooldown(row.spell, row.time));
    }

    for (LoginPetDeclinedNameRow const& row : CharacterRepository::LoginPetDeclinedNames(holder))
    {
        PetCacheDeclinedName names;
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            names.name[i] = row.name[i];
        }

        cache.LoadDeclinedName(row.petId, names);
    }
}

/**
 * @brief Loads the player's currently active pet from the database when possible.
 */
void Player::LoadPet()
{
    // fixme: the pet should still be loaded if the player is not in world
    // just not added to the map
    if (IsInWorld())
    {
        Pet* pet = new Pet;
        if (!pet->LoadPetFromDB(this, 0, 0, true))
        {
            delete pet;
        }
    }
}

/**
 * @brief Loads quest status records and rebuilds the in-memory quest log state.
 *
 * @param rows The quest status rows.
 */
void Player::_LoadQuestStatus(std::vector<QuestStatusRow> const& rows)
{
    m_questStatusMgr.ClearStatus();

    uint32 slot = 0;

    ////                                                       0        1         2           3           4        5            6            7            8            9             10            11            12            13            14

    if (!rows.empty())
    {
        for (QuestStatusRow const& statusRow : rows)
        {
            // Decoupling D4a: the manager fills THIS row, and it is applied below before the
            // next row is read. Applying a rewarded row casts spells, and a quest-complete
            // effect reaches AreaExploredOrEventHappens, which reads and writes a later row --
            // so the load must stay one pass, row by row, as it always was.
            QuestRowResult row = m_questStatusMgr.FillRow(statusRow, WorldGameTime::Get(), GetName(), QuestTemplateLookup());
            if (row.accepted)
            {
                uint32 quest_id = row.questId;
                Quest const* pQuest = row.quest;
                QuestStatusData& questStatusData = m_questStatusMgr.Entry(quest_id);

                // add to quest log
                if (slot < MAX_QUEST_LOG_SIZE &&
                        ((questStatusData.m_status == QUEST_STATUS_INCOMPLETE ||
                          questStatusData.m_status == QUEST_STATUS_COMPLETE ||
                          questStatusData.m_status == QUEST_STATUS_FAILED) &&
                         (!questStatusData.m_rewarded || pQuest->IsRepeatable())))
                {
                    SetQuestSlot(slot, quest_id, row.slotTime);

                    if (questStatusData.m_explored)
                    {
                        SetQuestSlotState(slot, QUEST_STATE_COMPLETE);
                    }

                    if (questStatusData.m_status == QUEST_STATUS_COMPLETE)
                    {
                        SetQuestSlotState(slot, QUEST_STATE_COMPLETE);
                    }

                    if (questStatusData.m_status == QUEST_STATUS_FAILED)
                    {
                        SetQuestSlotState(slot, QUEST_STATE_FAIL);
                    }

                    for (uint8 idx = 0; idx < QUEST_OBJECTIVES_COUNT; ++idx)
                        if (questStatusData.m_creatureOrGOcount[idx])
                        {
                            SetQuestSlotCounter(slot, idx, questStatusData.m_creatureOrGOcount[idx]);
                        }

                    ++slot;
                }

                if (questStatusData.m_rewarded)
                {
                    // learn rewarded spell if unknown
                    learnQuestRewardedSpells(pQuest);

                    // set rewarded title if any
                    if (pQuest->GetCharTitleId())
                    {
                        if (CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(pQuest->GetCharTitleId()))
                        {
                            SetTitle(titleEntry);
                        }
                    }

                    if (pQuest->GetBonusTalents())
                    {
                        m_questRewardTalentCount += pQuest->GetBonusTalents();
                    }
                }

                DEBUG_LOG("Quest status is {%u} for quest {%u} for player (GUID: %u)", questStatusData.m_status, quest_id, GetGUIDLow());
            }
        }

    }

    // clear quest log tail
    for (uint16 i = slot; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        SetQuestSlot(i, 0);
    }
}

void Player::_LoadDailyQuestStatus(std::vector<LoginDailyQuestStatusRow> const& rows)
{
    for (uint32 quest_daily_idx = 0; quest_daily_idx < PLAYER_MAX_DAILY_QUESTS; ++quest_daily_idx)
    {
        SetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx, 0);
    }


    if (!rows.empty())
    {
        uint32 quest_daily_idx = 0;

        for (LoginDailyQuestStatusRow const& row : rows)
        {
            if (quest_daily_idx >= PLAYER_MAX_DAILY_QUESTS) // max amount with exist data in query
            {
                sLog.outError("Player (GUID: %u) have more 25 daily quest records in `charcter_queststatus_daily`", GetGUIDLow());
                break;
            }

            uint32 quest_id = row.quest;

            Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);
            if (!pQuest)
            {
                continue;
            }

            SetUInt32Value(PLAYER_FIELD_DAILY_QUESTS_1 + quest_daily_idx, quest_id);
            ++quest_daily_idx;

            DEBUG_LOG("Daily quest {%u} cooldown for player (GUID: %u)", quest_id, GetGUIDLow());
        }
    }

    m_DailyQuestChanged = false;
}

/**
 * @brief Loads known spells from the database.
 *
 * @param rows The learned spell rows.
 */
void Player::_LoadSpells(std::vector<LoginSpellRow> const& rows)
{
    if (!rows.empty())
    {
        for (LoginSpellRow const& row : rows)
        {
            uint32 spell_id = row.spell;

            // skip talents & drop unneeded data
            if (GetTalentSpellPos(spell_id))
            {
                sLog.outError("Player::_LoadSpells: %s has talent spell %u in character_spell, removing it.",
                              GetGuidStr().c_str(), spell_id);
                CharacterSaveRepository::DeleteSpell(GetGUIDLow(), spell_id);
                continue;
            }

            addSpell(spell_id, row.active, false, false, row.disabled);
        }
    }
}

void Player::_LoadTalents(std::vector<TalentRow> const& rows)
{
    if (!rows.empty())
    {
        // Decoupling D4c: TalentMgr::LoadRow validates one row, issues its DELETE when it fails,
        // stores an inactive-spec row, and calls this back for an active-spec row at the exact
        // point of the row where this loop used to learn it.
        TalentMgr::ActiveSpellCallback const onActiveSpell = [this](uint32 spellId)
        {
            addSpell(spellId, true, false, false, false);
        };

        for (TalentRow const& row : rows)
        {
            m_talentMgr.LoadRow(row, GetGUIDLow(), getClassMask(), m_talentMgr.SpecsCount(), m_talentMgr.ActiveSpec(), onActiveSpell);
        }
    }
    UpdateGroupLeaderFlag();
}

/**
 * @brief Loads the player's current group membership from the database.
 *
 * @param memberGroupId The group identifier.
 */
void Player::_LoadGroup(std::optional<uint32> const& memberGroupId)
{
    if (memberGroupId)
    {
        uint32 groupId = *memberGroupId;

        if (Group* group = sGroupRegistry.Find(groupId))
        {
            uint8 subgroup = group->GetMemberGroup(GetObjectGuid());
            SetGroup(group, subgroup);
            if (getLevel() >= LEVELREQUIREMENT_HEROIC)
            {
                // the group leader may change the instance difficulty while the player is offline
                SetDungeonDifficulty(group->GetDungeonDifficulty());
                SetRaidDifficulty(group->GetRaidDifficulty());
            }
        }
    }
}

/**
 * @brief Loads the player's saved dungeon and raid instance bindings.
 *
 * @param rows The character instance bind rows.
 */
void Player::_LoadBoundInstances(std::vector<LoginBoundInstanceRow> const& rows)
{
    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
    {
        m_boundInstances[i].clear();
    }

    Group* group = GetGroup();

    if (!rows.empty())
    {
        for (LoginBoundInstanceRow const& row : rows)
        {
            bool perm = row.permanent;
            uint32 mapId = row.map;
            uint32 instanceId = row.id;
            uint8 difficulty = row.difficulty;

            time_t resetTime = (time_t)row.resettime;
            // the resettime for normal instances is only saved when the InstanceSave is unloaded
            // so the value read from the DB may be wrong here but only if the InstanceSave is loaded
            // and in that case it is not used

            MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
            if (!mapEntry)
            {
                sLog.outError("_LoadBoundInstances: player %s(%d) has bind to nonexistent map %d, kept", GetName(), GetGUIDLow(), mapId);
                continue;
            }

            if (!mapEntry->IsDungeon())
            {
                sLog.outError("_LoadBoundInstances: player %s(%d) has bind to not dungeon map %d", GetName(), GetGUIDLow(), mapId);
                InstanceBindRepository::DeleteBind(GetGUIDLow(), instanceId);
                continue;
            }

            if (difficulty >= MAX_DIFFICULTY)
            {
                sLog.outError("_LoadBoundInstances: player %s(%d) has bind to nonexistent difficulty %d instance for map %u", GetName(), GetGUIDLow(), difficulty, mapId);
                InstanceBindRepository::DeleteBind(GetGUIDLow(), instanceId);
                continue;
            }

            MapDifficultyEntry const* mapDiff = GetMapDifficultyData(mapId, Difficulty(difficulty));
            if (!mapDiff)
            {
                sLog.outError("_LoadBoundInstances: player %s(%d) has bind to nonexistent difficulty %d instance for map %u, kept", GetName(), GetGUIDLow(), difficulty, mapId);
                continue;
            }

            if (!perm && group)
            {
                sLog.outError("_LoadBoundInstances: %s is in group (Id: %d) but has a non-permanent character bind to map %d,%d,%d",
                              GetGuidStr().c_str(), group->GetId(), mapId, instanceId, difficulty);
                InstanceBindRepository::DeleteBind(GetGUIDLow(), instanceId);
                continue;
            }

            // since non permanent binds are always solo bind, they can always be reset
            DungeonPersistentState* state = reinterpret_cast<DungeonPersistentState*>(sMapPersistentStateMgr.AddPersistentState(mapEntry, instanceId, Difficulty(difficulty), resetTime, !perm, true));
            if (state)
            {
                BindToInstance(state, perm, true);
            }
        }
    }
}

/**
 * @brief Finds the player's instance bind for a specific map.
 *
 * @param mapid The map identifier to look up.
 * @return The matching bind entry, or NULL if none exists.
 */
InstancePlayerBind* Player::GetBoundInstance(uint32 mapid, Difficulty difficulty)
{
    // some instances only have one difficulty
    MapDifficultyEntry const* mapDiff = GetMapDifficultyData(mapid, difficulty);
    if (!mapDiff)
    {
        return NULL;
    }

    BoundInstancesMap::iterator itr = m_boundInstances[difficulty].find(mapid);
    if (itr != m_boundInstances[difficulty].end())
    {
        return &itr->second;
    }
    else
    {
        return NULL;
    }
}

/**
 * @brief Removes the player's instance bind for a map.
 *
 * @param mapid The bound map identifier.
 * @param unload True when called during load or unload paths that should skip DB deletion.
 */
void Player::UnbindInstance(uint32 mapid, Difficulty difficulty, bool unload)
{
    BoundInstancesMap::iterator itr = m_boundInstances[difficulty].find(mapid);
    UnbindInstance(itr, difficulty, unload);
}

/**
 * @brief Removes the instance bind referenced by an iterator.
 *
 * @param itr Iterator pointing at the bind to remove.
 * @param unload True when called during load or unload paths that should skip DB deletion.
 */
void Player::UnbindInstance(BoundInstancesMap::iterator& itr, Difficulty difficulty, bool unload)
{
    if (itr != m_boundInstances[difficulty].end())
    {
        if (!unload)
            InstanceBindRepository::DeleteBind(GetGUIDLow(), itr->second.state->GetInstanceId());

        sCalendarMgr.SendCalendarRaidLockoutRemove(this, itr->second.state);

        itr->second.state->RemovePlayer(this);              // state can become invalid
        m_boundInstances[difficulty].erase(itr++);
    }
}

/**
 * @brief Binds the player to a dungeon persistent state.
 *
 * @param state The persistent instance state to bind.
 * @param permanent True for a permanent lock, false for a temporary one.
 * @param load True when rebuilding the bind from saved data.
 * @return The resulting bind entry, or NULL if the state was invalid.
 */
InstancePlayerBind* Player::BindToInstance(DungeonPersistentState* state, bool permanent, bool load)
{
    if (state)
    {
        InstancePlayerBind& bind = m_boundInstances[state->GetDifficulty()][state->GetMapId()];
        if (bind.state)
        {
            // update the state when the group kills a boss
            if (permanent != bind.perm || state != bind.state)
                if (!load)
                    InstanceBindRepository::UpdateBind(state->GetInstanceId(), permanent, GetGUIDLow(), bind.state->GetInstanceId());
        }
        else
        {
            if (!load)
                InstanceBindRepository::InsertBind(GetGUIDLow(), state->GetInstanceId(), permanent);
        }

        if (bind.state != state)
        {
            if (bind.state)
            {
                bind.state->RemovePlayer(this);
            }
            state->AddPlayer(this);
        }

        if (permanent)
        {
            state->SetCanReset(false);
        }

        bind.state = state;
        bind.perm = permanent;
        if (!load)
        {
            DEBUG_LOG("Player::BindToInstance: %s(%d) is now bound to map %d, instance %d, difficulty %d",
                      GetName(), GetGUIDLow(), state->GetMapId(), state->GetInstanceId(), state->GetDifficulty());
        }
        return &bind;
    }
    else
    {
        return NULL;
    }
}

/**
 * @brief Resolves the instance save that applies to the player or the player's group.
 *
 * @param mapid The map identifier to inspect.
 * @return The applicable persistent state, or NULL if none exists.
 */
DungeonPersistentState* Player::GetBoundInstanceSaveForSelfOrGroup(uint32 mapid)
{
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
    if (!mapEntry)
    {
        return NULL;
    }

    InstancePlayerBind* pBind = GetBoundInstance(mapid, GetDifficulty(mapEntry->IsRaid()));
    DungeonPersistentState* state = pBind ? pBind->state : NULL;

    // the player's permanent player bind is taken into consideration first
    // then the player's group bind and finally the solo bind.
    if (!pBind || !pBind->perm)
    {
        InstanceGroupBind* groupBind = NULL;
        // use the player's difficulty setting (it may not be the same as the group's)
        if (Group* group = GetGroup())
            if ((groupBind = group->GetBoundInstance(mapid, this)))
            {
                state = groupBind->state;
            }
    }

    return state;
}

/**
 * @brief Sends the player's permanent raid instance lock information to the client.
 */
void Player::SendRaidInfo()
{
    std::vector<DungeonPersistentState*> permBinds;

    for (int i = 0; i < MAX_DIFFICULTY; ++i)
    {
        for (BoundInstancesMap::const_iterator itr = m_boundInstances[i].begin(); itr != m_boundInstances[i].end(); ++itr)
        {
            if (itr->second.perm)
            {
                permBinds.push_back(itr->second.state);
            }
        }
    }
    sPlayerWorldNotices.RaidInfoShown(*this, permBinds);
}

/*
- called on every successful teleportation to a map
*/
void Player::SendSavedInstances()
{
    std::vector<uint32> permMapIds;

    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
    {
        for (BoundInstancesMap::const_iterator itr = m_boundInstances[i].begin(); itr != m_boundInstances[i].end(); ++itr)
        {
            if (itr->second.perm)                           // only permanent binds are sent
            {
                permMapIds.push_back(itr->second.state->GetMapId());
            }
        }
    }

    sPlayerWorldNotices.SavedInstancesShown(*this, permMapIds);
}

/// convert the player's binds to the group
void Player::ConvertInstancesToGroup(Player* player, Group* group, ObjectGuid player_guid)
{
    bool has_binds = false;
    bool has_solo = false;

    if (player)
    {
        player_guid = player->GetObjectGuid();
        if (!group)
        {
            group = player->GetGroup();
        }
    }

    MANGOS_ASSERT(player_guid);

    // copy all binds to the group, when changing leader it's assumed the character
    // will not have any solo binds

    if (player)
    {
        for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
        {
            for (BoundInstancesMap::iterator itr = player->m_boundInstances[i].begin(); itr != player->m_boundInstances[i].end();)
            {
                has_binds = true;

                if (group)
                {
                    group->BindToInstance(itr->second.state, itr->second.perm, true);
                }

                // permanent binds are not removed
                if (!itr->second.perm)
                {
                    // increments itr in call
                    player->UnbindInstance(itr, Difficulty(i), true);
                    has_solo = true;
                }
                else
                {
                    ++itr;
                }
            }
        }
    }

    uint32 player_lowguid = player_guid.GetCounter();

    // if the player's not online we don't know what binds it has
    if (!player || !group || has_binds)
    {
        InstanceBindRepository::CopyBindsToGroup(player_lowguid);
    }

    // the following should not get executed when changing leaders
    if (!player || has_solo)
    {
        InstanceBindRepository::DeleteTemporaryBinds(player_lowguid);
    }
}

/**
 * @brief Loads and validates the player's home bind location.
 *
 * @param row The home bind data.
 * @return True if a valid home bind was loaded or defaulted successfully; otherwise, false.
 */
bool Player::_LoadHomeBind(std::optional<LoginHomeBindRow> const& row)
{
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());
    if (!info)
    {
        sLog.outError("Player have incorrect race/class pair. Can't be loaded.");
        return false;
    }

    bool ok = false;
    if (row)
    {
        m_homebindMapId = row->map;
        m_homebindAreaId = row->zone;
        m_homebindX = row->positionX;
        m_homebindY = row->positionY;
        m_homebindZ = row->positionZ;

        MapEntry const* bindMapEntry = sMapStore.LookupEntry(m_homebindMapId);

        // accept saved data only for valid position (and non instanceable), and accessable
        if (MapManager::IsValidMapCoord(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ) &&
                !bindMapEntry->Instanceable() && GetSession()->Expansion() >= bindMapEntry->Expansion())
        {
            ok = true;
        }
        else
        {
            CharacterSaveRepository::DeleteHomebind(GetGUIDLow());
        }
    }

    if (!ok)
    {
        m_homebindMapId = info->mapId;
        m_homebindAreaId = info->areaId;
        m_homebindX = info->positionX;
        m_homebindY = info->positionY;
        m_homebindZ = info->positionZ;

        CharacterSaveRepository::InsertHomebind(GetGUIDLow(), m_homebindMapId, (uint32)m_homebindAreaId, m_homebindX, m_homebindY, m_homebindZ);
    }

    DEBUG_LOG("Setting player home position: mapid is: %u, zoneid is %u, X is %f, Y is %f, Z is %f",
              m_homebindMapId, m_homebindAreaId, m_homebindX, m_homebindY, m_homebindZ);

    return true;
}

