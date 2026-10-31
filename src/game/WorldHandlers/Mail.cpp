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
 * @file Mail.cpp
 * @brief Mail system implementation
 *
 * This file implements the Mail class which manages in-game mail:
 *
 * - Mail creation and sending
 * - Mail with items and money
 * - Mail expiration and deletion
 * - COD (Cash on Delivery) payments
 * - Mail reading and item retrieval
 * - Auction house mail notifications
 *
 * Mails are stored in the `mail` database table and loaded
 * when players log in. System mails can be sent for various
 * game events (auctions, GM messages, etc.).
 *
 * @see Mail for the mail class
 * @see MailHandler for network opcode handling
 */

#include "Utilities/Errors.h"
#include <string>
#include "Mail.h"
#include "Log.h"
#include "Database/SqlOperations.h"
#include "Util.h"
#include "Utilities/ProgressBar.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Item.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/mail/MailRepository.h"
#include "data/GameConfig.h"
#include "Calendar.h"
#include "AuctionHouseMgr.h"

/**
 * Creates a new MailSender object.
 *
 * @param sender The object/player sending this mail.
 * @param stationery The stationary associated with this sender.
 */
MailSender::MailSender(Object* sender, MailStationery stationery) : m_stationery(stationery)
{
    switch (sender->GetTypeId())
    {
        case TYPEID_UNIT:
            m_messageType = MAIL_CREATURE;
            m_senderId = sender->GetEntry();
            break;
        case TYPEID_GAMEOBJECT:
            m_messageType = MAIL_GAMEOBJECT;
            m_senderId = sender->GetEntry();
            break;
        case TYPEID_PLAYER:
            m_messageType = MAIL_NORMAL;
            m_senderId = sender->GetGUIDLow();
            break;
        default:
            m_messageType = MAIL_NORMAL;
            m_senderId = 0;                                 // will show mail from nonexistent player
            sLog.outError("MailSender::MailSender - Mail have unexpected sender typeid (%u)", sender->GetTypeId());
            break;
    }
}
/**
 * Creates a new MailSender object from an AuctionEntry.
 *
 * @param sender the AuctionEntry from which this mail is generated.
 */
MailSender::MailSender(AuctionEntry* sender)
    : m_messageType(MAIL_AUCTION), m_senderId(sender->GetHouseId()), m_stationery(MAIL_STATIONERY_AUCTION)
{
}

/*
 * Creates a new MailSender object from an event.
 *
 * @param sender the calendar event from which this mail is generated.
 *
 * Note : Actualy it seem no info from event is required. We need more research to correctly initialise m_senderId
 */
MailSender::MailSender(CalendarEvent const* sender)
    : m_messageType(MAIL_CALENDAR), m_senderId(0), m_stationery(MAIL_STATIONERY_DEFAULT)
{
}

/*
 * Creates a new MailReceiver object.
 *
 * @param receiver The player receiving the mail.
 */
MailReceiver::MailReceiver(Player* receiver) : m_receiver(receiver), m_receiver_guid(receiver->GetObjectGuid())
{
}
/**
 * Creates a new MailReceiver object with a specified GUID.
 *
 * @param receiver The player receiving the mail.
 * @param receiver_guid The GUID to use instead of the receivers.
 */
MailReceiver::MailReceiver(Player* receiver, ObjectGuid receiver_guid) : m_receiver(receiver), m_receiver_guid(receiver_guid)
{
    MANGOS_ASSERT(!receiver || receiver->GetObjectGuid() == receiver_guid);
}

/*
 * Adds an item to the MailDraft.
 *
 * @param item The item to be added to the MailDraft.
 * @returns the MailDraft the item was added to.
 */
MailDraft& MailDraft::AddItem(Item* item)
{
    m_items[item->GetGUIDLow()] = item;
    return *this;
}
/**
 * Prepares the items in a MailDraft.
 */
bool MailDraft::prepareItems(Player* receiver)
{
    if (!m_mailTemplateId || !m_mailTemplateItemsNeed)
    {
        return false;
    }

    m_mailTemplateItemsNeed = false;

    Loot mailLoot(NULL);

    // can be empty
    mailLoot.FillLoot(m_mailTemplateId, LootTemplates_Mail, receiver, true, true);

    uint32 max_slot = mailLoot.GetMaxSlotInLootFor(receiver);
    for (uint32 i = 0; m_items.size() < MAX_MAIL_ITEMS && i < max_slot; ++i)
    {
        if (LootItem* lootitem = mailLoot.LootItemInSlot(i, receiver))
        {
            if (Item* item = Item::CreateItem(lootitem->itemid, lootitem->count, receiver))
            {
                item->SaveToDB();                           // save for prevent lost at next mail load, if send fail then item will deleted
                AddItem(item);
            }
        }
    }

    return true;
}
/**
 * Deletes the items included in a MailDraft.
 *
 * @param inDB A boolean specifying whether the change should be saved to the database or not.
 */
void MailDraft::deleteIncludedItems(bool inDB /**= false*/)
{
    for (MailItemMap::iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
    {
        Item* item = mailItemIter->second;

        if (inDB)
        {
            MailRepository::DeleteDroppedItem(item->GetGUIDLow());
        }

        delete item;
    }

    m_items.clear();
}
/**
 * Clone MailDraft from another MailDraft.
 *
 * @param draft Point to source for draft cloning.
 */
void MailDraft::CloneFrom(MailDraft const& draft)
{
    m_mailTemplateId = draft.GetMailTemplateId();
    m_mailTemplateItemsNeed = draft.m_mailTemplateItemsNeed;

    m_subject = draft.GetSubject();
    m_body = draft.GetBody();
    m_money = draft.GetMoney();
    m_COD = draft.GetCOD();

    for (MailItemMap::const_iterator mailItemIter = draft.m_items.begin(); mailItemIter != draft.m_items.end(); ++mailItemIter)
    {
        Item* item = mailItemIter->second;

        if (Item* newitem = item->CloneItem(item->GetCount()))
        {
            newitem->SaveToDB();
            AddItem(newitem);
        }
    }
}

/*
 * Returns a mail to its sender.
 * @param sender_acc           The id of the account of the sender.
 * @param sender_guid          The low part of the GUID of the sender.
 * @param receiver_guid        The low part of the GUID of the receiver.
 */
void MailDraft::SendReturnToSender(uint32 sender_acc, ObjectGuid sender_guid, ObjectGuid receiver_guid)
{
    Player* receiver = sObjectMgr.GetPlayer(receiver_guid);

    uint32 rc_account = 0;
    if (!receiver)
    {
        rc_account = sObjectMgr.GetPlayerAccountIdByGUID(receiver_guid);
    }

    if (!receiver && !rc_account)                           // sender not exist
    {
        deleteIncludedItems(true);
        return;
    }

    // prepare mail and send in other case
    bool needItemDelay = false;

    if (!m_items.empty())
    {
        // if item send to character at another account, then apply item delivery delay
        needItemDelay = sender_acc != rc_account;

        // set owner to new receiver (to prevent delete item with sender char deleting)
        CharacterTransaction::Begin();
        for (MailItemMap::iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
        {
            Item* item = mailItemIter->second;
            item->SaveToDB();                               // item not in inventory and can be save standalone
            // owner in data will set at mail receive and item extracting
            MailRepository::SetMailedItemOwner(item->GetGUIDLow(), receiver_guid.GetCounter());
        }
        CharacterTransaction::Commit();
    }

    // If theres is an item, there is a one hour delivery delay.
    uint32 deliver_delay = needItemDelay ? sGameConfig.getConfig(CONFIG_UINT32_MAIL_DELIVERY_DELAY) : 0;

    // will delete item or place to receiver mail list
    SendMailTo(MailReceiver(receiver, receiver_guid), MailSender(MAIL_NORMAL, sender_guid.GetCounter()), MAIL_CHECK_MASK_RETURNED, deliver_delay);
}
/**
 * Sends a mail.
 *
 * @param receiver             The MailReceiver to which this mail is sent.
 * @param sender               The MailSender from which this mail is originated.
 * @param checked              The mask used to specify the mail.
 * @param deliver_delay        The delay after which the mail is delivered in seconds
 */
void MailDraft::SendMailTo(MailReceiver const& receiver, MailSender const& sender, MailCheckMask checked, uint32 deliver_delay)
{
    Player* pReceiver = receiver.GetPlayer();               // can be NULL

    uint32 pReceiverAccount = 0;
    if (!pReceiver)
    {
        pReceiverAccount = sObjectMgr.GetPlayerAccountIdByGUID(receiver.GetPlayerGuid());
    }

    if (!pReceiver && !pReceiverAccount)                    // receiver not exist
    {
        deleteIncludedItems(true);
        return;
    }

    bool has_items = !m_items.empty();

    // generate mail template items for online player, for offline player items will generated at open
    if (pReceiver)
    {
        if (prepareItems(pReceiver))
        {
            has_items = true;
        }
    }

    uint32 mailId = sObjectMgr.GenerateMailID();

    time_t deliver_time = time(NULL) + deliver_delay;

    uint32 expire_delay;
    // auction mail without any items and money (auction sale note) pending 1 hour
    if (sender.GetMailMessageType() == MAIL_AUCTION && m_items.empty() && !m_money)
    {
        expire_delay = HOUR;
    }
    // default case: expire time if COD 3 days, if no COD 30 days
    else
    {
        expire_delay = (m_COD > 0) ? 3 * DAY : 30 * DAY;
    }

    time_t expire_time = deliver_time + expire_delay;

    // Add to DB: the subject and the body are bound, so the binding (and, on the plain-statement
    // fallback, the escaping) happens on the delay thread, not here in the tick.
    MailRow row;
    row.id             = mailId;
    row.messageType    = uint32(sender.GetMailMessageType());
    row.stationery     = uint32(sender.GetStationery());
    row.mailTemplateId = uint32(GetMailTemplateId());
    row.sender         = sender.GetSenderId();
    row.receiver       = receiver.GetPlayerGuid().GetCounter();
    row.subject        = GetSubject();
    row.body           = GetBody();
    row.hasItems       = has_items ? 1 : 0;
    row.expireTime     = uint64(expire_time);
    row.deliverTime    = uint64(deliver_time);
    row.money          = m_money;
    row.cod            = m_COD;
    row.checked        = uint32(checked);

    CharacterTransaction::Begin();
    MailRepository::InsertMail(row);

    for (MailItemMap::const_iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
    {
        Item* item = mailItemIter->second;
        MailRepository::InsertMailItem(mailId, item->GetGUIDLow(), item->GetEntry(), receiver.GetPlayerGuid().GetCounter());
    }
    CharacterTransaction::Commit();

    // For online receiver update in game mail status and data
    if (pReceiver)
    {
        pReceiver->AddNewMailDeliverTime(deliver_time);

        Mail* m = new Mail;
        m->messageID = mailId;
        m->mailTemplateId = GetMailTemplateId();
        m->subject = GetSubject();
        m->body = GetBody();
        m->money = GetMoney();
        m->COD = GetCOD();

        for (MailItemMap::const_iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
        {
            Item* item = mailItemIter->second;
            m->AddItem(item->GetGUIDLow(), item->GetEntry());
        }

        m->messageType = sender.GetMailMessageType();
        m->stationery = sender.GetStationery();
        m->sender = sender.GetSenderId();
        m->receiverGuid = receiver.GetPlayerGuid();
        m->expire_time = expire_time;
        m->deliver_time = deliver_time;
        m->checked = checked;
        m->state = MAIL_STATE_UNCHANGED;

        pReceiver->AddMail(m);                           // to insert new mail to beginning of maillist

        if (!m_items.empty())
        {
            for (MailItemMap::iterator mailItemIter = m_items.begin(); mailItemIter != m_items.end(); ++mailItemIter)
            {
                pReceiver->AddMItem(mailItemIter->second);
            }
        }
    }
    else if (!m_items.empty())
    {
        deleteIncludedItems();
    }
}

/**
 * Generate items from template at mails loading (this happens when mail with mail template items send in time when receiver has been offline)
 *
 * @param receiver             reciver of mail
 */

void Mail::prepareTemplateItems(Player* receiver)
{
    if (!mailTemplateId || !items.empty())
    {
        return;
    }

    has_items = true;

    Loot mailLoot(NULL);

    // can be empty
    mailLoot.FillLoot(mailTemplateId, LootTemplates_Mail, receiver, true, true);

    CharacterTransaction::Begin();
    MailRepository::SetHasItems(messageID);

    uint32 max_slot = mailLoot.GetMaxSlotInLootFor(receiver);
    for (uint32 i = 0; items.size() < MAX_MAIL_ITEMS && i < max_slot; ++i)
    {
        if (LootItem* lootitem = mailLoot.LootItemInSlot(i, receiver))
        {
            if (Item* item = Item::CreateItem(lootitem->itemid, lootitem->count, receiver))
            {
                item->SaveToDB();

                AddItem(item->GetGUIDLow(), item->GetEntry());

                receiver->AddMItem(item);

                MailRepository::InsertMailItem(messageID, item->GetGUIDLow(), item->GetEntry(), receiver->GetGUIDLow());
            }
        }
    }

    CharacterTransaction::Commit();
}

/*! @} */

/**
 * @brief Returns the expired mails that hold items to their senders and deletes the rest.
 *
 * Runs once a day and at start-up.
 *
 * @param serverUp true if the server is already running, false when the server is started.
 */
void ReturnOrDeleteOldMails(bool serverUp)
{
    time_t curTime = time(NULL);
    tm lt;
    lt = safe_localtime(curTime);
    uint64 basetime(curTime);
    sLog.outString("Returning mails current time: hour: %d, minute: %d, second: %d ", lt.tm_hour, lt.tm_min, lt.tm_sec);

    // delete all old mails without item and without body immediately, if starting server
    if (!serverUp)
    {
        MailRepository::DeleteEmptyExpiredMails((uint64)basetime);
    }

    // Runs at start-up and on the daily mail timer inside World::Update, so both reads are staged here and the expired
    // mails' returns and deletes happen in the callback. Slot 1 reads the expired mails' items in one join ordered by
    // `mail_id`; the callback walks it as a cursor beside the mail rows, since both results ascend in that key.
    MailRepository::QueueExpiredMailReads((uint64)basetime, [basetime, serverUp](SqlQueryHolder* h)
                                          {
                                              ReturnOrDeleteOldMailsCallback(std::unique_ptr<SqlQueryHolder>(h), basetime, serverUp);
                                          });
}

/**
 * @brief Returns or deletes the expired mails, once both reads have answered.
 *
 * Walks slot 1 as a cursor beside the mail rows. Every write it makes goes through PExecute, so nothing here runs a
 * synchronous database call.
 *
 * @param holder   The two staged reads.
 * @param basetime The expiry cut-off the reads were staged with, which the returned mails'
 *                 new expiry and delivery times are computed from -- captured rather than
 *                 recomputed, so a mail cannot be given a window that disagrees with the
 *                 set it was selected in.
 * @param serverUp The flag the caller passed, verbatim: it decides whether a mail whose
 *                 receiver is online is left alone.
 */
void ReturnOrDeleteOldMailsCallback(std::unique_ptr<SqlQueryHolder> holder, uint64 basetime, bool serverUp)
{
    std::vector<ExpiredMailRow> mails = MailRepository::ExpiredMails(holder.get());
    std::vector<ExpiredMailItemRow> mailItems = MailRepository::ExpiredMailItems(holder.get());

    if (mails.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Only expired mails (need to be return or delete) or DB table `mail` is empty.");
        sLog.outString();
        return;                                             // no mail needs to be returned or deleted
    }

    BarGoLink bar(mails.size());
    uint32 count = 0;

    /// The item cursor: the next item row to read.
    size_t nextItem = 0;

    for (ExpiredMailRow const& mail : mails)
    {
        bar.step();

        Mail* m = new Mail;
        m->messageID = mail.id;

        // Step over the item rows of any mail EARLIER than this one: a mail whose items
        // were not consumed (its receiver was online) left its run standing.
        while (nextItem < mailItems.size() && mailItems[nextItem].mailId < m->messageID)
        {
            ++nextItem;
        }

        m->messageType = mail.messageType;
        m->sender = mail.sender;
        m->receiverGuid = ObjectGuid(HIGHGUID_PLAYER, mail.receiver);
        bool has_items = mail.hasItems;
        m->expire_time = (time_t)mail.expireTime;
        m->deliver_time = 0;
        m->COD = mail.cod;
        m->checked = mail.checked;
        m->mailTemplateId = mail.mailTemplateId;

        Player* pl = 0;
        if (serverUp)
        {
            pl = sPlayerRegistry.Find(m->receiverGuid);
        }
        if (pl)
        {
            // this code will run very improbably (the time is between 4 and 5 am, in game is online a player, who has old mail
            // his in mailbox and he has already listed his mails )
            delete m;
            continue;
        }
        // delete or return mail:
        if (has_items)
        {
            // This mail's own run of the joined item rows: contiguous, because both
            // reads ascend in `mail_id`.
            while (nextItem < mailItems.size())
            {
                ExpiredMailItemRow const& mailItem = mailItems[nextItem];
                if (mailItem.mailId != m->messageID)
                {
                    break;
                }

                m->AddItem(mailItem.itemGuid, mailItem.itemTemplate);

                ++nextItem;
            }
            // if it is mail from non-player, or if it's already return mail, it shouldn't be returned, but deleted
            if (m->messageType != MAIL_NORMAL || (m->checked & (MAIL_CHECK_MASK_COD_PAYMENT | MAIL_CHECK_MASK_RETURNED)))
            {
                // mail open and then not returned
                for (MailItemInfoVec::iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                {
                    MailRepository::DeleteExpiredMailItem(itr2->item_guid);
                }
            }
            else
            {
                // mail will be returned:
                MailRepository::ReturnExpiredMail(m->receiverGuid.GetCounter(), m->sender, (uint64)(basetime + 30 * DAY), (uint64)basetime,
                                                  MAIL_CHECK_MASK_RETURNED, m->messageID);
                for (MailItemInfoVec::iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                {
                    // update receiver in mail items for its proper delivery, and in instance_item for avoid lost item at sender delete
                    MailRepository::SetReturnedItemReceiver(m->sender, itr2->item_guid);
                    MailRepository::SetReturnedItemOwner(m->sender, itr2->item_guid);
                }
                delete m;
                continue;
            }
        }

        MailRepository::DeleteMailRow(m->messageID);
        delete m;
        ++count;
    }

    sLog.outString(">> Loaded %u mails", count);
    sLog.outString();
}
