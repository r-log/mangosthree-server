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

#ifndef MANGOSSERVER_GOSSIP_H
#define MANGOSSERVER_GOSSIP_H

#include "Platform/Define.h"
#include <string>
#include <vector>
#include "Log.h"
#include "QuestDef.h"
#include "data/GossipRecords.h"
#include "ObjectGuid.h"

class PlayerSession;

#define GOSSIP_MAX_MENU_ITEMS       32                      // client supports showing max 32 items
#define DEFAULT_GOSSIP_MESSAGE      0xffffff

/**
 * Structure representing a gossip menu item.
 */
struct GossipMenuItem
{
    uint8       m_gIcon;            // Icon for the gossip menu item
    bool        m_gCoded;           // Whether the gossip menu item is coded
    std::string m_gMessage;         // Message for the gossip menu item
    uint32      m_gSender;          // Sender ID for the gossip menu item
    uint32      m_gOptionId;        // Option ID for the gossip menu item
    std::string m_gBoxMessage;      // Box message for the gossip menu item
    uint32      m_gBoxMoney;        // Box money for the gossip menu item
};

typedef std::vector<GossipMenuItem> GossipMenuItemList;

/**
 * Structure representing data for a gossip menu item.
 */
struct GossipMenuItemData
{
    int32  m_gAction_menu;          // Action menu ID (negative for close gossip)
    uint32 m_gAction_poi;           // Action POI ID
    uint32 m_gAction_script;        // Action script ID
};

typedef std::vector<GossipMenuItemData> GossipMenuItemDataList;

/**
 * Structure representing a quest menu item.
 */
struct QuestMenuItem
{
    uint32      m_qId;              // Quest ID
    uint8       m_qIcon;            // Icon for the quest menu item
};

typedef std::vector<QuestMenuItem> QuestMenuItemList;

/**
 * Class representing a gossip menu.
 */
class GossipMenu
{
    public:
        GossipMenu(PlayerSession* session, int localeIndex);
        ~GossipMenu();

        void AddMenuItem(uint8 Icon, const std::string& Message, bool Coded = false);
        void AddMenuItem(uint8 Icon, const std::string& Message, uint32 dtSender, uint32 dtAction, const std::string& BoxMessage, uint32 BoxMoney, bool Coded = false);

        // for using from scripts, don't must be inlined
        void AddMenuItem(uint8 Icon, char const* Message, bool Coded = false);
        void AddMenuItem(uint8 Icon, char const* Message, uint32 dtSender, uint32 dtAction, char const* BoxMessage, uint32 BoxMoney, bool Coded = false);

        void AddMenuItem(uint8 Icon, int32 itemText, uint32 dtSender, uint32 dtAction, int32 boxText, uint32 BoxMoney, bool Coded = false);

        void SetMenuId(uint32 menu_id) { m_gMenuId = menu_id; }
        uint32 GetMenuId() const { return m_gMenuId; }

        void AddMenuItemData(int32 action_menu, uint32 action_poi, uint32 action_script);

        unsigned int MenuItemCount() const
        {
            return static_cast<unsigned int>(m_gItems.size());
        }

        unsigned int MenuItemDataCount() const
        {
            return static_cast<unsigned int>(m_gItemsData.size());
        }

        bool Empty() const
        {
            return m_gItems.empty();
        }

        GossipMenuItem const& GetItem(unsigned int Id) const
        {
            return m_gItems.at(Id);
        }

        GossipMenuItemData const* GetItemData(unsigned int indexId) const
        {
            if (indexId >= m_gItemsData.size())
            {
                sLog.outError("GossipMenu::GetItemData> indexId is out of bounds!");
                return nullptr;
            }
            return &m_gItemsData.at(indexId);
        }

        uint32 MenuItemSender(unsigned int ItemId) const;
        uint32 MenuItemAction(unsigned int ItemId) const;
        bool MenuItemCoded(unsigned int ItemId) const;

        void ClearMenu();

        PlayerSession* GetMenuSession() const { return m_session; }

    protected:
        GossipMenuItemList      m_gItems;           // List of gossip menu items
        GossipMenuItemDataList  m_gItemsData;       // List of gossip menu item data

        uint32 m_gMenuId;                           // Gossip menu ID

    private:
        PlayerSession* m_session;                    // Session associated with the gossip menu
        int m_localeIndex;                           // the session's database locale, for the menu's texts
};

/**
 * Class representing a quest menu.
 */
class QuestMenu
{
    public:
        QuestMenu();
        ~QuestMenu();

        void AddMenuItem(uint32 QuestId, uint8 Icon);
        void ClearMenu();

        uint8 MenuItemCount() const
        {
            return static_cast<uint8>(m_qItems.size());
        }

        bool Empty() const
        {
            return m_qItems.empty();
        }

        bool HasItem(uint32 questid) const;

        QuestMenuItem const& GetItem(uint16 Id) const
        {
            return m_qItems.at(Id);
        }

    protected:
        QuestMenuItemList m_qItems;                 // List of quest menu items
};

/**
 * Class representing a player menu, which includes both gossip and quest menus.
 */
/// What a player's menus send the player's own client: the gossip and quest-giver dialogues, the points of
/// interest and the talking boxes, one call per packet. The session installs the notices that send them
/// at start-up; this base sends nothing.
class GossipNotices
{
    public:
        virtual ~GossipNotices();

        virtual void SendGossipMenu(PlayerSession& session, GossipMenu const& gossipMenu, QuestMenu const& questMenu, uint32 TitleTextId, ObjectGuid objectGuid);
        virtual void CloseGossip(PlayerSession& session);
        virtual void SendPointOfInterest(PlayerSession& session, float X, float Y, uint32 Icon, uint32 Flags, uint32 Data, char const* locName);
        virtual void SendPointOfInterest(PlayerSession& session, uint32 poi_id);
        virtual void SendTalking(PlayerSession& session, uint32 textID);
        virtual void SendTalking(PlayerSession& session, char const* title, char const* text);
        virtual void SendQuestGiverQuestList(PlayerSession& session, QuestMenu const& questMenu, QEmote eEmote, const std::string& Title, ObjectGuid npcGUID);
        virtual void SendQuestGiverStatus(PlayerSession& session, uint32 questStatus, ObjectGuid npcGUID);
        virtual void SendQuestGiverQuestDetails(PlayerSession& session, Quest const* pQuest, ObjectGuid guid, bool ActivateAccept);
        virtual void SendQuestQueryResponse(PlayerSession& session, Quest const* pQuest);
        virtual void SendQuestGiverOfferReward(PlayerSession& session, Quest const* pQuest, ObjectGuid npcGUID, bool EnableNext);
        virtual void SendQuestGiverRequestItems(PlayerSession& session, Quest const* pQuest, ObjectGuid npcGUID, bool Completable, bool CloseOnCancel);

        /// The installed notices, or the base when none are installed.
        static GossipNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static GossipNotices* Install(GossipNotices* notices);
};

#define sGossipNotices GossipNotices::Installed()

class PlayerMenu
{
    private:
        GossipMenu mGossipMenu;                     // Gossip menu
        QuestMenu  mQuestMenu;                      // Quest menu

    public:
        PlayerMenu(PlayerSession* Session, int localeIndex);
        ~PlayerMenu();

        GossipMenu& GetGossipMenu() { return mGossipMenu; }
        QuestMenu& GetQuestMenu() { return mQuestMenu; }

        PlayerSession* GetMenuSession() const { return mGossipMenu.GetMenuSession(); }

        bool Empty() const { return mGossipMenu.Empty() && mQuestMenu.Empty(); }

        void ClearMenus();
        uint32 GossipOptionSender(unsigned int Selection) const;
        uint32 GossipOptionAction(unsigned int Selection) const;
        bool GossipOptionCoded(unsigned int Selection) const;

        void SendGossipMenu(uint32 titleTextId, ObjectGuid objectGuid);
        void CloseGossip() const;
        void SendPointOfInterest(float X, float Y, uint32 Icon, uint32 Flags, uint32 Data, const char* locName) const;
        void SendPointOfInterest(uint32 poi_id) const;
        void SendTalking(uint32 textID) const;
        void SendTalking(char const* title, char const* text) const;

        /*********************************************************/
        /***                    QUEST SYSTEM                   ***/
        /*********************************************************/
        void SendQuestGiverStatus(uint32 questStatus, ObjectGuid npcGUID);

        void SendQuestGiverQuestList(QEmote eEmote, const std::string& Title, ObjectGuid npcGUID);

        void SendQuestQueryResponse(Quest const* pQuest) const;
        void SendQuestGiverQuestDetails(Quest const* pQuest, ObjectGuid npcGUID, bool ActivateAccept) const;

        void SendQuestGiverOfferReward(Quest const* pQuest, ObjectGuid npcGUID, bool EnbleNext) const;
        void SendQuestGiverRequestItems(Quest const* pQuest, ObjectGuid npcGUID, bool Completable, bool CloseOnCancel) const;
};
#endif
