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
 * @file GossipDef.cpp
 * @brief NPC gossip menu implementation
 *
 * This file implements the gossip menu system used by NPCs to present
 * interactive options to players. Features:
 *
 * - Gossip menu creation and management
 * - Menu item addition with icons and actions
 * - Quest gossip integration
 * - Point-of-interest (POI) handling
 * - Player choice processing
 * - Scripted gossip options
 *
 * Gossip menus are loaded from the `gossip_menu` database tables
 * and can be extended with scripts.
 *
 * @see GossipMenu for the menu class
 * @see GossipMenuData for menu data
 */

#include "Utilities/Errors.h"
#include <string>
#include "GossipDef.h"
#include "QuestDef.h"
#include "ObjectMgr.h"

#include <atomic>

// Constructor for GossipMenu, initializes the session and reserves space for menu items
GossipMenu::GossipMenu(PlayerSession* session, int localeIndex) : m_session(session), m_localeIndex(localeIndex)
{
    m_gItems.reserve(16); // can be set for max from most often sizes to speedup push_back and less memory use
    m_gMenuId = 0;
}

// Destructor for GossipMenu, clears the menu items
GossipMenu::~GossipMenu()
{
    ClearMenu();
}

// Adds a menu item to the gossip menu
void GossipMenu::AddMenuItem(uint8 Icon, const std::string& Message, uint32 dtSender, uint32 dtAction, const std::string& BoxMessage, uint32 BoxMoney, bool Coded)
{
    MANGOS_ASSERT(m_gItems.size() <= GOSSIP_MAX_MENU_ITEMS);

    GossipMenuItem gItem;

    gItem.m_gIcon       = Icon;
    gItem.m_gMessage    = Message;
    gItem.m_gCoded      = Coded;
    gItem.m_gSender     = dtSender;
    gItem.m_gOptionId   = dtAction;
    gItem.m_gBoxMessage = BoxMessage;
    gItem.m_gBoxMoney   = BoxMoney;

    m_gItems.push_back(gItem);
}

// Adds gossip menu item data
void GossipMenu::AddMenuItemData(int32 action_menu, uint32 action_poi, uint32 action_script)
{
    GossipMenuItemData pItemData{};

    pItemData.m_gAction_menu    = action_menu;
    pItemData.m_gAction_poi     = action_poi;
    pItemData.m_gAction_script  = action_script;

    m_gItemsData.push_back(pItemData);
}

// Overloaded method to add a menu item with fewer parameters
void GossipMenu::AddMenuItem(uint8 Icon, const std::string& Message, bool Coded)
{
    AddMenuItem(Icon, Message, 0, 0, "", 0, Coded);
}

// Overloaded method to add a menu item with a C-style string message
void GossipMenu::AddMenuItem(uint8 Icon, char const* Message, bool Coded)
{
    AddMenuItem(Icon, std::string(Message ? Message : ""), Coded);
}

void GossipMenu::AddMenuItem(uint8 Icon, char const* Message, uint32 dtSender, uint32 dtAction, char const* BoxMessage, uint32 BoxMoney, bool Coded)
{
    AddMenuItem(Icon, std::string(Message ? Message : ""), dtSender, dtAction, std::string(BoxMessage ? BoxMessage : ""), BoxMoney, Coded);
}

void GossipMenu::AddMenuItem(uint8 Icon, int32 itemText, uint32 dtSender, uint32 dtAction, int32 boxText, uint32 BoxMoney, bool Coded)
{
    uint32 loc_idx = m_localeIndex;

    char const* item_text = itemText ? sObjectMgr.GetMangosString(itemText, loc_idx) : "";
    char const* box_text = boxText ? sObjectMgr.GetMangosString(boxText, loc_idx) : "";

    AddMenuItem(Icon, std::string(item_text), dtSender, dtAction, std::string(box_text), BoxMoney, Coded);
}

// Returns the sender ID of a menu item
uint32 GossipMenu::MenuItemSender(unsigned int ItemId) const
{
    if (ItemId >= m_gItems.size())
    {
        return 0;
    }

    return m_gItems[ItemId].m_gSender;
}

// Returns the action ID of a menu item
uint32 GossipMenu::MenuItemAction(unsigned int ItemId) const
{
    if (ItemId >= m_gItems.size())
    {
        return 0;
    }

    return m_gItems[ItemId].m_gOptionId;
}

// Returns whether a menu item is coded
bool GossipMenu::MenuItemCoded(unsigned int ItemId) const
{
    if (ItemId >= m_gItems.size())
    {
        return 0;
    }

    return m_gItems[ItemId].m_gCoded;
}

// Clears all menu items
void GossipMenu::ClearMenu()
{
    m_gItems.clear();
    m_gItemsData.clear();
    m_gMenuId = 0;
}

// Constructor for PlayerMenu, initializes the gossip menu
PlayerMenu::PlayerMenu(PlayerSession* session, int localeIndex) : mGossipMenu(session, localeIndex)
{
}

// Destructor for PlayerMenu, clears all menus
PlayerMenu::~PlayerMenu()
{
    ClearMenus();
}

// Clears all menus in the player menu
void PlayerMenu::ClearMenus()
{
    mGossipMenu.ClearMenu();
    mQuestMenu.ClearMenu();
}

// Returns the sender ID of a gossip option
uint32 PlayerMenu::GossipOptionSender(unsigned int Selection) const
{
    return mGossipMenu.MenuItemSender(Selection);
}

// Returns the action ID of a gossip option
uint32 PlayerMenu::GossipOptionAction(unsigned int Selection) const
{
    return mGossipMenu.MenuItemAction(Selection);
}

// Returns whether a gossip option is coded
bool PlayerMenu::GossipOptionCoded(unsigned int Selection) const
{
    return mGossipMenu.MenuItemCoded(Selection);
}

// Sends the gossip menu to the player
void PlayerMenu::SendGossipMenu(uint32 TitleTextId, ObjectGuid objectGuid)
{
    sGossipNotices.SendGossipMenu(*GetMenuSession(), mGossipMenu, mQuestMenu, TitleTextId, objectGuid);
}

// Closes the gossip menu
void PlayerMenu::CloseGossip() const
{
    sGossipNotices.CloseGossip(*GetMenuSession());
}

// Sends a point of interest to the player (outdated method)
void PlayerMenu::SendPointOfInterest(float X, float Y, uint32 Icon, uint32 Flags, uint32 Data, char const* locName) const
{
    sGossipNotices.SendPointOfInterest(*GetMenuSession(), X, Y, Icon, Flags, Data, locName);
}

// Sends a point of interest to the player by ID
void PlayerMenu::SendPointOfInterest(uint32 poi_id) const
{
    sGossipNotices.SendPointOfInterest(*GetMenuSession(), poi_id);
}

// Sends a talking message to the player by text ID
void PlayerMenu::SendTalking(uint32 textID) const
{
    sGossipNotices.SendTalking(*GetMenuSession(), textID);
}

// Sends a talking message to the player by title and text
void PlayerMenu::SendTalking(char const* title, char const* text) const
{
    sGossipNotices.SendTalking(*GetMenuSession(), title, text);
}

/*********************************************************/
/***                    QUEST SYSTEM                   ***/
/*********************************************************/

// Constructor for QuestMenu, reserves space for quest items
QuestMenu::QuestMenu()
{
    m_qItems.reserve(16); // can be set for max from most often sizes to speedup push_back and less memory use
}

// Destructor for QuestMenu, clears the menu items
QuestMenu::~QuestMenu()
{
    ClearMenu();
}

// Adds a quest menu item to the quest menu
void QuestMenu::AddMenuItem(uint32 QuestId, uint8 Icon)
{
    Quest const* qinfo = sObjectMgr.GetQuestTemplate(QuestId);
    if (!qinfo)
    {
        return;
    }

    MANGOS_ASSERT(m_qItems.size() <= GOSSIP_MAX_MENU_ITEMS);

    QuestMenuItem qItem{};

    qItem.m_qId   = QuestId;
    qItem.m_qIcon = Icon;

    m_qItems.push_back(qItem);
}

// Checks if the quest menu has a specific item
bool QuestMenu::HasItem(uint32 questid) const
{
    for (QuestMenuItemList::const_iterator i = m_qItems.begin(); i != m_qItems.end(); ++i)
    {
        if (i->m_qId == questid)
        {
            return true;
        }
    }
    return false;
}

// Clears all quest menu items
void QuestMenu::ClearMenu()
{
    m_qItems.clear();
}

// Sends the quest giver quest list to the player
void PlayerMenu::SendQuestGiverQuestList(QEmote eEmote, const std::string& Title, ObjectGuid npcGUID)
{
    sGossipNotices.SendQuestGiverQuestList(*GetMenuSession(), mQuestMenu, eEmote, Title, npcGUID);
}

void PlayerMenu::SendQuestGiverStatus(uint32 questStatus, ObjectGuid npcGUID)
{
    sGossipNotices.SendQuestGiverStatus(*GetMenuSession(), questStatus, npcGUID);
}

// Sends the quest giver quest details to the player
void PlayerMenu::SendQuestGiverQuestDetails(Quest const* pQuest, ObjectGuid guid, bool ActivateAccept) const
{
    sGossipNotices.SendQuestGiverQuestDetails(*GetMenuSession(), pQuest, guid, ActivateAccept);
}

// Sends the quest query response to the player
void PlayerMenu::SendQuestQueryResponse(Quest const* pQuest) const
{
    sGossipNotices.SendQuestQueryResponse(*GetMenuSession(), pQuest);
}

/**
 * @brief Sends the quest reward offer window for a completed quest.
 *
 * @param pQuest The quest being rewarded.
 * @param npcGUID The quest giver guid.
 * @param EnableNext True if the client should allow automatic progression to the next quest.
 */
void PlayerMenu::SendQuestGiverOfferReward(Quest const* pQuest, ObjectGuid npcGUID, bool EnableNext) const
{
    sGossipNotices.SendQuestGiverOfferReward(*GetMenuSession(), pQuest, npcGUID, EnableNext);
}

/**
 * @brief Sends the quest item request window for a quest turn-in.
 *
 * @param pQuest The quest being turned in.
 * @param npcGUID The quest giver guid.
 * @param Completable True if the player currently meets the quest completion requirements.
 * @param CloseOnCancel True if canceling should close the gossip window.
 */
void PlayerMenu::SendQuestGiverRequestItems(Quest const* pQuest, ObjectGuid npcGUID, bool Completable, bool CloseOnCancel) const
{
    sGossipNotices.SendQuestGiverRequestItems(*GetMenuSession(), pQuest, npcGUID, Completable, CloseOnCancel);
}

namespace
{
    std::atomic<GossipNotices*> s_gossipNoticesInstalled(NULL);

    GossipNotices& BaseGossipNotices()
    {
        static GossipNotices notices;
        return notices;
    }
}

GossipNotices::~GossipNotices()
{
}

void GossipNotices::SendGossipMenu(PlayerSession& /*session*/, GossipMenu const& /*gossipMenu*/, QuestMenu const& /*questMenu*/, uint32 /*TitleTextId*/, ObjectGuid /*objectGuid*/)
{
}

void GossipNotices::CloseGossip(PlayerSession& /*session*/)
{
}

void GossipNotices::SendPointOfInterest(PlayerSession& /*session*/, float /*X*/, float /*Y*/, uint32 /*Icon*/, uint32 /*Flags*/, uint32 /*Data*/, char const* /*locName*/)
{
}

void GossipNotices::SendPointOfInterest(PlayerSession& /*session*/, uint32 /*poi_id*/)
{
}

void GossipNotices::SendTalking(PlayerSession& /*session*/, uint32 /*textID*/)
{
}

void GossipNotices::SendTalking(PlayerSession& /*session*/, char const* /*title*/, char const* /*text*/)
{
}

void GossipNotices::SendQuestGiverQuestList(PlayerSession& /*session*/, QuestMenu const& /*questMenu*/, QEmote /*eEmote*/, const std::string& /*Title*/, ObjectGuid /*npcGUID*/)
{
}

void GossipNotices::SendQuestGiverStatus(PlayerSession& /*session*/, uint32 /*questStatus*/, ObjectGuid /*npcGUID*/)
{
}

void GossipNotices::SendQuestGiverQuestDetails(PlayerSession& /*session*/, Quest const* /*pQuest*/, ObjectGuid /*guid*/, bool /*ActivateAccept*/)
{
}

void GossipNotices::SendQuestQueryResponse(PlayerSession& /*session*/, Quest const* /*pQuest*/)
{
}

void GossipNotices::SendQuestGiverOfferReward(PlayerSession& /*session*/, Quest const* /*pQuest*/, ObjectGuid /*npcGUID*/, bool /*EnableNext*/)
{
}

void GossipNotices::SendQuestGiverRequestItems(PlayerSession& /*session*/, Quest const* /*pQuest*/, ObjectGuid /*npcGUID*/, bool /*Completable*/, bool /*CloseOnCancel*/)
{
}

GossipNotices& GossipNotices::Installed()
{
    GossipNotices* notices = s_gossipNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseGossipNotices();
}

GossipNotices* GossipNotices::Install(GossipNotices* notices)
{
    return s_gossipNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
