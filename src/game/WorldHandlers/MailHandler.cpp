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
 * @file MailHandler.cpp
 * @brief Mail system opcode handlers
 *
 * This file handles mail-related opcodes including:
 * - CMSG_SEND_MAIL: Send mail to another player
 * - CMSG_MAIL_DELETE: Delete mail
 * - CMSG_MAIL_RETURN: Return mail to sender
 * - CMSG_MAIL_MARK_AS_READ: Mark mail as read
 * - CMSG_MAIL_CREATE_TEXT_ITEM: Create item from mail text
 * - CMSG_MAIL_TAKE_ITEM: Take item from mail
 * - CMSG_MAIL_TAKE_MONEY: Take money from mail
 * - CMSG_MAIL_QUERY_NEXT_TIME: Query next mail delivery time
 *
 * Mail operations require proper validation of recipient, money,
 * and item attachments.
 */

#include <string>
#include <utility>
#include "Mail.h"
#include "Language.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "CharacterCache.h"
#include "Item.h"
#include "AchievementMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Chat.h"

/**
 * @brief Verifies that the player can legally access the requested mailbox.
 *
 * @param guid The mailbox guid or player guid used for mailbox access.
 * @return true if mailbox access is allowed; otherwise false.
 */
bool WorldSession::CheckMailBox(ObjectGuid guid)
{
    // GM case
    if (guid == GetPlayer()->GetObjectGuid())
    {
        // command case will return only if player have real access to command
        if (!ChatHandler(GetPlayer()).FindCommand("mailbox"))
        {
            DEBUG_LOG("%s attempt open mailbox in cheating way.", guid.GetString().c_str());
            return false;
        }
    }
    // mailbox case
    else if (guid.IsGameObject())
    {
        if (!GetPlayer()->GetGameObjectIfCanInteractWith(guid, GAMEOBJECT_TYPE_MAILBOX))
        {
            DEBUG_LOG("Mailbox %s not found or %s can't interact with him.", guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());
            return false;
        }
    }
    // squire case
    else if (guid.IsAnyTypeCreature())
    {
        Creature* creature = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
        if (!creature)
        {
            DEBUG_LOG("%s not found or %s can't interact with him.", guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());
            return false;
        }

        if (!(creature->GetCreatureInfo()->CreatureTypeFlags & CREATURE_TYPEFLAGS_SQUIRE))
        {
            DEBUG_LOG("%s not have access to mailbox.", creature->GetGuidStr().c_str());
            return false;
        }

        if (creature->GetOwnerGuid() != GetPlayer()->GetObjectGuid())
        {
            DEBUG_LOG("%s not owned by %s for access to mailbox.", creature->GetGuidStr().c_str(), GetPlayer()->GetGuidStr().c_str());
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

/*
 * Handles the Packet sent by the client when sending a mail.
 *
 * This methods takes the packet sent by the client and performs the following actions:
 * - Checks whether the mail is valid: i.e. can he send the selected items,
 *   does he have enough money, etc.
 * - Creates a MailDraft and adds the needed items, money, cost data.
 * - Sends the mail.
 *
 * Depending on the outcome of the checks performed the player will recieve a different
 * MailResponseResult.
 *
 * @see MailResponseResult
 * @see SendMailResult()
 *
 * @param recv_data the WorldPacket containing the data sent by the client.
 */
void WorldSession::HandleSendMail(WorldPacket& recv_data)
{
    sLog.outError("WORLD: CMSG_SEND_MAIL");
    MailSendRequest request;
    uint8 receiverLen, subjectLen, bodyLen;

    recv_data >> request.unk1;                              // stationery?
    recv_data >> request.unk2;                              // 0x00000000

    recv_data >> request.COD >> request.money;              // cod and money

    bodyLen = recv_data.ReadBits(12);
    subjectLen = recv_data.ReadBits(9);

    uint8 items_count = recv_data.ReadBits(5);              // attached items count
    if (items_count > MAX_MAIL_ITEMS)                       // client limit
    {
        GetPlayer()->SendMailResult(0, MAIL_SEND, MAIL_ERR_TOO_MANY_ATTACHMENTS);
        recv_data.rfinish();                                // set to end to avoid warnings spam
        return;
    }

    recv_data.ReadGuidMask<0>(request.mailboxGuid);

    request.itemGuids.resize(items_count);
    for (uint8 i = 0; i < items_count; ++i)
    {
        recv_data.ReadGuidMask<2, 6, 3, 7, 1, 0, 4, 5>(request.itemGuids[i]);
    }

    recv_data.ReadGuidMask<3, 4>(request.mailboxGuid);

    receiverLen = recv_data.ReadBits(7);

    recv_data.ReadGuidMask<2, 6, 1, 7, 5>(request.mailboxGuid);

    recv_data.ReadGuidBytes<4>(request.mailboxGuid);

    for (uint8 i = 0; i < items_count; ++i)
    {
        recv_data.ReadGuidBytes<6, 1, 7, 2>(request.itemGuids[i]);
        recv_data.read_skip<uint8>();                       // item slot in mail, not used
        recv_data.ReadGuidBytes<3, 0, 4, 5>(request.itemGuids[i]);
    }

    recv_data.ReadGuidBytes<7, 3, 6, 5>(request.mailboxGuid);

    request.subject = recv_data.ReadString(subjectLen);
    request.receiver = recv_data.ReadString(receiverLen);

    recv_data.ReadGuidBytes<2, 0>(request.mailboxGuid);

    request.body = recv_data.ReadString(bodyLen);

    recv_data.ReadGuidBytes<1>(request.mailboxGuid);

    DEBUG_LOG("WORLD: CMSG_SEND_MAIL receiver '%s' subject '%s' body '%s' mailbox " UI64FMTD " money " UI64FMTD " COD " UI64FMTD " unkt1 %u unk2 %u",
        request.receiver.c_str(), request.subject.c_str(), request.body.c_str(), request.mailboxGuid.GetRawValue(),
        request.money, request.COD, request.unk1, request.unk2);
    // packet read complete, now do check

    // Decoupling D7g: the receiver is resolved HERE, out of memory alone -- normalizePlayerName
    // is a string operation and ObjectMgr::GetPlayerGuidByName has read the character cache
    // since D7c -- because the guards below and the decision to defer both need the guid.
    //
    // The normalisation writes back only when it SUCCEEDS, which the in-place call it replaces
    // could not do: WStrToUtf8 clears the string it was given when the name is not valid UTF-8.
    // In the old order that clearing happened after the `receiver.empty()` test and before the
    // "recipient not found" log, so it could only ever blank a log line; here the test comes
    // after, so the raw name has to survive a failed normalisation or a garbage name would
    // return silently instead of answering MAIL_ERR_RECIPIENT_NOT_FOUND.
    std::string normalized = request.receiver;
    if (normalizePlayerName(normalized))
    {
        request.receiver = normalized;
        request.receiverGuid = sObjectMgr.GetPlayerGuidByName(request.receiver);
    }

    Player* pl = _player;

    // Every check below reads memory only, so it stays here: a request that cannot succeed
    // never reaches the database at all (D7b's contract, HandlePetitionBuyOpcode). They are
    // the old handler's guards, in the old order, sending the old replies -- and each of them
    // runs AGAIN in ProcessSendMail (C3), because on the deferred path a tick passes before
    // anything is charged. Running them twice in one tick costs nothing and sends nothing: the
    // first copy returns on failure, so only one reply ever leaves.
    if (!CheckMailBox(request.mailboxGuid))
    {
        return;
    }

    if (request.receiver.empty())
    {
        return;
    }

    if (!request.receiverGuid)
    {
        DEBUG_LOG("%s is sending mail to %s (GUID: nonexistent!) with subject %s and body %s includes %u items, " UI64FMTD " copper and " UI64FMTD " COD copper with unk1 = %u, unk2 = %u",
                   pl->GetGuidStr().c_str(), request.receiver.c_str(), request.subject.c_str(), request.body.c_str(),
                   items_count, request.money, request.COD, request.unk1, request.unk2);
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_RECIPIENT_NOT_FOUND);
        return;
    }

    if (pl->GetObjectGuid() == request.receiverGuid)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_CANNOT_SEND_TO_SELF);
        return;
    }

    // safeguard against possible money dupe
    if (request.money && request.COD)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    uint32 cost = items_count ? 30 * items_count : 30;      // price hardcoded in client

    uint64 reqmoney = cost + request.money;

    if (pl->GetMoney() < reqmoney)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_NOT_ENOUGH_MONEY);
        return;
    }

    if (!sObjectMgr.GetPlayer(request.receiverGuid))
    {
        // An OFFLINE receiver is the one case that used to block the tick: his mailbox count
        // is a `SELECT COUNT(*) FROM mail`, and it is not a cached fact (it changes with every
        // mail anyone sends him and he is not here to keep it). It becomes a continuation
        // (C1-C5): the read is queued and everything below runs a tick later. Nothing is
        // mutated in front of it -- the money and the items only move after the count is
        // tested -- so there is no C3 reorder, only a re-validation.
        QueueSendMailboxCountRead(GetAccountId(), GetSessionId(), pl->GetObjectGuid(), std::move(request));
        return;
    }

    // An online receiver answers from memory (Player::GetMailSize), so this stays in the tick
    // it arrived in.
    ProcessSendMail(request, 0);
}

/**
 * @brief Stages the send-mail handler's only read (decoupling D7g).
 *
 * Split out of the handler so it can be driven on its own: everything it needs is in its
 * arguments, so a test can queue exactly what a real request queues without a Player.
 *
 * @param accountId The sending account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The sender.
 * @param request The parsed mail, moved into the continuation.
 */
void WorldSession::QueueSendMailboxCountRead(uint32 accountId, proto::SessionId sessionId,
                                             ObjectGuid playerGuid, MailSendRequest request)
{
    // Read the one number the statement needs BEFORE the move: the order in which a call's
    // arguments are evaluated is unspecified, so the lambda could be built first.
    uint32 const receiverLow = request.receiverGuid.GetCounter();

    // The request carries three strings and a vector; it is moved into the capture and moved
    // again out of it, so a deferred send copies it once (at the parse) instead of four times.
    // `mutable` is what makes the second move a move: a lambda's operator() is const by
    // default, and std::move on a const member yields a const rvalue, which copies.
    CharacterDatabase.AsyncPQuery([accountId, sessionId, playerGuid, request = std::move(request)](QueryResult* result) mutable
                                  {
                                      WorldSession::HandleSendMailCallback(std::unique_ptr<QueryResult>(result),
                                                                           accountId, sessionId, playerGuid,
                                                                           std::move(request));
                                  },
                                  "SELECT COUNT(*) FROM `mail` WHERE `receiver` = '%u'",
                                  receiverLow);
}

/**
 * @brief Sends the mail once the offline receiver's mailbox count is known.
 *
 * @param result The single COUNT(*) row.
 * @param accountId The sending account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The sender.
 * @param request The parsed mail.
 */
void WorldSession::HandleSendMailCallback(std::unique_ptr<QueryResult> result, uint32 accountId,
                                          proto::SessionId sessionId, ObjectGuid playerGuid,
                                          MailSendRequest request)
{
    WorldSession* session = NULL;
    Player* player = NULL;
    if (!FindRequesterPlayer(accountId, sessionId, playerGuid, session, player))
    {
        return;
    }

    uint8 mailsCount = 0;
    if (result)
    {
        mailsCount = result->Fetch()[0].GetUInt32();
    }

    session->ProcessSendMail(request, mailsCount);
}

/**
 * @brief The whole of CMSG_SEND_MAIL below its packet read, verbatim (decoupling D7g).
 *
 * Shared by the two paths: an online receiver runs it in the arriving tick, an offline one
 * from the continuation a tick later. The first six guards below are the memory-only ones the
 * handler has ALREADY run and refused on -- they are repeated here because C3 asks a deferred
 * handler to re-validate: a tick has passed, so the mailbox may be out of range, the money may
 * be spent and the items may be gone. On the same-tick path they simply pass a second time.
 *
 * @param request The parsed mail, with the receiver already resolved.
 * @param offlineMailsCount The offline receiver's mailbox count; unread when he is online.
 */
void WorldSession::ProcessSendMail(MailSendRequest const& request, uint8 offlineMailsCount)
{
    ObjectGuid const& mailboxGuid = request.mailboxGuid;
    uint64 const money = request.money;
    uint64 const COD = request.COD;
    std::string const& receiver = request.receiver;
    std::string const& subject = request.subject;
    std::string const& body = request.body;
    uint32 const unk1 = request.unk1;
    uint32 const unk2 = request.unk2;
    uint8 const items_count = uint8(request.itemGuids.size());

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    if (receiver.empty())
    {
        return;
    }

    Player* pl = _player;

    ObjectGuid rc = request.receiverGuid;

    if (!rc)
    {
        DEBUG_LOG("%s is sending mail to %s (GUID: nonexistent!) with subject %s and body %s includes %u items, " UI64FMTD " copper and " UI64FMTD " COD copper with unk1 = %u, unk2 = %u",
                   pl->GetGuidStr().c_str(), receiver.c_str(), subject.c_str(), body.c_str(), items_count, money, COD, unk1, unk2);
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_RECIPIENT_NOT_FOUND);
        return;
    }

    DEBUG_LOG("%s is sending mail to %s with subject %s and body %s includes %u items, %u copper and %u COD copper with unk1 = %u, unk2 = %u",
               pl->GetGuidStr().c_str(), rc.GetString().c_str(), subject.c_str(), body.c_str(), items_count, money, COD, unk1, unk2);

    if (pl->GetObjectGuid() == rc)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_CANNOT_SEND_TO_SELF);
        return;
    }

    // safeguard against possible money dupe
    if (money && COD)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    uint32 cost = items_count ? 30 * items_count : 30;      // price hardcoded in client

    uint64 reqmoney = cost + money;

    if (pl->GetMoney() < reqmoney)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_NOT_ENOUGH_MONEY);
        return;
    }

    Player* receive = sObjectMgr.GetPlayer(rc);

    Team rc_team;
    uint8 mails_count = 0;                                  // do not allow to send to one player more than 100 mails

    if (receive)
    {
        rc_team = receive->GetTeam();
        mails_count = receive->GetMailSize();
    }
    else
    {
        rc_team = sObjectMgr.GetPlayerTeamByGUID(rc);
        // Decoupling D7g: the `SELECT COUNT(*) FROM mail WHERE receiver` that stood here was
        // staged by the handler and its answer is this argument. A receiver who logged IN
        // during that tick takes the branch above instead, with his live mailbox.
        mails_count = offlineMailsCount;
    }

    // do not allow to have more than 100 mails in mailbox.. mails count is in opcode uint8!!! - so max can be 255..
    if (mails_count > 100)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_RECIPIENT_CAP_REACHED);
        return;
    }

    // check the receiver's Faction...
    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_MAIL) && pl->GetTeam() != rc_team && GetSecurity() == SEC_PLAYER)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_NOT_YOUR_TEAM);
        return;
    }

    uint32 rc_account = receive
                        ? receive->GetSession()->GetAccountId()
                        : sObjectMgr.GetPlayerAccountIdByGUID(rc);

    // Decoupling D7g (C3): the receiver was resolved from the character cache in the handler's
    // tick; a delete continuation can have removed the character since. Refuse before anything
    // is charged -- the same refusal the handler gives an unknown name.
    if (!receive && !sCharacterCache.GetByGuid(rc))
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_RECIPIENT_NOT_FOUND);
        return;
    }

    Item* items[MAX_MAIL_ITEMS];

    for (uint8 i = 0; i < items_count; ++i)
    {
        if (!request.itemGuids[i].IsItem())
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_MAIL_ATTACHMENT_INVALID);
            return;
        }

        // Re-found by guid, never carried across a tick as a pointer (C1): on the deferred
        // path an item may have been sold, destroyed or moved since the request arrived, and
        // this is the same lookup the handler always made.
        Item* item = pl->GetItemByGuid(request.itemGuids[i]);

        // prevent sending bag with items (cheat: can be placed in bag after adding equipped empty bag to mail)
        if (!item)
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_MAIL_ATTACHMENT_INVALID);
            return;
        }

        if (!item->CanBeTraded(true))
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_EQUIP_ERROR, EQUIP_ERR_MAIL_BOUND_ITEM);
            return;
        }

        if (item->IsBoundAccountWide() && item->IsSoulBound() && pl->GetSession()->GetAccountId() != rc_account)
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_EQUIP_ERROR, EQUIP_ERR_ARTEFACTS_ONLY_FOR_OWN_CHARACTERS);
            return;
        }

        if ((item->GetProto()->Flags & ITEM_FLAG_CONJURED) || item->GetUInt32Value(ITEM_FIELD_DURATION))
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_EQUIP_ERROR, EQUIP_ERR_MAIL_BOUND_ITEM);
            return;
        }

        if (COD && item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_CANT_SEND_WRAPPED_COD);
            return;
        }

        items[i] = item;
    }

    pl->SendMailResult(0, MAIL_SEND, MAIL_OK);

    pl->ModifyMoney(-int64(reqmoney));
    pl->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_MAIL, cost);

    bool needItemDelay = false;

    MailDraft draft(subject, body);

    if (items_count > 0 || money > 0)
    {
        if (items_count > 0)
        {
            for (uint8 i = 0; i < items_count; ++i)
            {
                Item* item = items[i];
                if (GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
                {
                    sLog.outCommand(GetAccountId(), "GM %s (Account: %u) mail item: %s (Entry: %u Count: %u) to player: %s (Account: %u)",
                                    GetPlayerName(), GetAccountId(), item->GetProto()->Name1, item->GetEntry(), item->GetCount(), receiver.c_str(), rc_account);
                }

                pl->MoveItemFromInventory(items[i]->GetBagSlot(), item->GetSlot(), true);
                CharacterDatabase.BeginTransaction();
                item->DeleteFromInventoryDB();              // deletes item from character's inventory
                item->SaveToDB();                           // recursive and not have transaction guard into self, item not in inventory and can be save standalone
                // owner in data will set at mail receive and item extracting
                CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid`='%u'", rc.GetCounter(), item->GetGUIDLow());
                CharacterDatabase.CommitTransaction();

                draft.AddItem(item);
            }

            // if item send to character at another account, then apply item delivery delay
            needItemDelay = pl->GetSession()->GetAccountId() != rc_account;
        }

        if (money > 0 &&  GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
        {
            sLog.outCommand(GetAccountId(), "GM %s (Account: %u) mail money: " UI64FMTD " to player: %s (Account: %u)",
                            GetPlayerName(), GetAccountId(), money, receiver.c_str(), rc_account);
        }
    }

    // If theres is an item, there is a one hour delivery delay if sent to another account's character.
    uint32 deliver_delay = needItemDelay ? sWorld.getConfig(CONFIG_UINT32_MAIL_DELIVERY_DELAY) : 0;

    // will delete item or place to receiver mail list
    draft
    .SetMoney(money)
    .SetCOD(COD)
    .SendMailTo(MailReceiver(receive, rc), pl, body.empty() ? MAIL_CHECK_MASK_COPIED : MAIL_CHECK_MASK_HAS_BODY, deliver_delay);

    CharacterDatabase.BeginTransaction();
    pl->SaveInventoryAndGoldToDB();
    CharacterDatabase.CommitTransaction();
}

/**
 * Handles the Packet sent by the client when reading a mail.
 *
 * This method is called when a client reads a mail that was previously unread.
 * It will add the MAIL_CHECK_MASK_READ flag to the mail being read.
 *
 * @see MailCheckMask
 *
 * @param recv_data the packet containing information about the mail the player read.
 *
 */
void WorldSession::HandleMailMarkAsRead(WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid;
    uint32 mailId;
    recv_data >> mailboxGuid;
    recv_data >> mailId;

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = _player;

    if (Mail* m = pl->GetMail(mailId))
    {
        if (pl->unReadMails)
        {
            --pl->unReadMails;
        }
        m->checked = m->checked | MAIL_CHECK_MASK_READ;
        pl->m_mailsUpdated = true;
        m->state = MAIL_STATE_CHANGED;
    }
}

/**
 * Handles the Packet sent by the client when deleting a mail.
 *
 * This method is called when a client deletes a mail in his mailbox.
 *
 * @param recv_data The packet containing information about the mail being deleted.
 *
 */
void WorldSession::HandleMailDelete(WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid;
    uint32 mailId;
    recv_data >> mailboxGuid;
    recv_data >> mailId;
    recv_data.read_skip<uint32>();                          // mailTemplateId

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = _player;
    pl->m_mailsUpdated = true;

    if (Mail* m = pl->GetMail(mailId))
    {
        // delete shouldn't show up for COD mails
        if (m->COD)
        {
            pl->SendMailResult(mailId, MAIL_DELETED, MAIL_ERR_INTERNAL_ERROR);
            return;
        }

        m->state = MAIL_STATE_DELETED;
    }
    pl->SendMailResult(mailId, MAIL_DELETED, MAIL_OK);
}
/**
 * Handles the Packet sent by the client when returning a mail to sender.
 * This method is called when a player chooses to return a mail to its sender.
 * It will create a new MailDraft and add the items, money, etc. associated with the mail
 * and then send the mail to the original sender.
 *
 * @param recv_data The packet containing information about the mail being returned.
 *
 */
void WorldSession::HandleMailReturnToSender(WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid;
    uint32 mailId;
    recv_data >> mailboxGuid;
    recv_data >> mailId;
    recv_data.read_skip<uint64>();                          // original sender GUID for return to, not used

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = _player;
    Mail* m = pl->GetMail(mailId);
    if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL))
    {
        pl->SendMailResult(mailId, MAIL_RETURNED_TO_SENDER, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    // we can return mail now
    // so firstly delete the old one
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `mail` WHERE `id` = '%u'", mailId);
    // needed?
    CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `mail_id` = '%u'", mailId);
    CharacterDatabase.CommitTransaction();
    pl->RemoveMail(mailId);

    // send back only to existing players and simple drop for other cases
    if (m->messageType == MAIL_NORMAL && m->sender)
    {
        MailDraft draft;
        if (m->mailTemplateId)
        {
            draft.SetMailTemplate(m->mailTemplateId, false);// items already included
        }
        else
        {
            draft.SetSubjectAndBody(m->subject, m->body);
        }

        if (m->HasItems())
        {
            for (MailItemInfoVec::iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
            {
                if (Item* item = pl->GetMItem(itr2->item_guid))
                {
                    draft.AddItem(item);
                }

                pl->RemoveMItem(itr2->item_guid);
            }
        }

        draft.SetMoney(m->money).SendReturnToSender(GetAccountId(), m->receiverGuid, ObjectGuid(HIGHGUID_PLAYER, m->sender));
    }

    delete m;                                               // we can deallocate old mail
    pl->SendMailResult(mailId, MAIL_RETURNED_TO_SENDER, MAIL_OK);
}

/**
 * Handles the packet sent by the client when taking an item from the mail.
 */
void WorldSession::HandleMailTakeItem(WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid;
    uint32 mailId;
    uint32 itemId;
    recv_data >> mailboxGuid;
    recv_data >> mailId;
    recv_data >> itemId;                                    // item guid low

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = _player;

    Mail* m = pl->GetMail(mailId);
    if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL))
    {
        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    // prevent cheating with skip client money check
    if (pl->GetMoney() < m->COD)
    {
        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_ERR_NOT_ENOUGH_MONEY);
        return;
    }

    Item* it = pl->GetMItem(itemId);
    if (!it)
    {
        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = _player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, it, false);
    if (msg == EQUIP_ERR_OK)
    {
        m->RemoveItem(itemId);
        m->removedItems.push_back(itemId);

        if (m->COD > 0)                                     // if there is COD, take COD money from player and send them to sender by mail
        {
            ObjectGuid sender_guid = ObjectGuid(HIGHGUID_PLAYER, m->sender);
            Player* sender = sObjectMgr.GetPlayer(sender_guid);

            uint32 sender_accId = 0;

            if (GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
            {
                std::string sender_name;
                if (sender)
                {
                    sender_accId = sender->GetSession()->GetAccountId();
                    sender_name = sender->GetName();
                }
                else if (sender_guid)
                {
                    // can be calculated early
                    sender_accId = sObjectMgr.GetPlayerAccountIdByGUID(sender_guid);

                    if (!sObjectMgr.GetPlayerNameByGUID(sender_guid, sender_name))
                    {
                        sender_name = sObjectMgr.GetMangosStringForDBCLocale(LANG_UNKNOWN);
                    }
                }
                sLog.outCommand(GetAccountId(), "GM %s (Account: %u) receive mail item: %s (Entry: %u Count: %u) and send COD money: %u to player: %s (Account: %u)",
                                GetPlayerName(), GetAccountId(), it->GetProto()->Name1, it->GetEntry(), it->GetCount(), m->COD, sender_name.c_str(), sender_accId);
            }
            else if (!sender)
            {
                sender_accId = sObjectMgr.GetPlayerAccountIdByGUID(sender_guid);
            }

            // check player existence
            if (sender || sender_accId)
            {
                MailDraft(m->subject, "")
                .SetMoney(m->COD)
                .SendMailTo(MailReceiver(sender, sender_guid), _player, MAIL_CHECK_MASK_COD_PAYMENT);
            }

            pl->ModifyMoney(-int64(m->COD));
        }
        m->COD = 0;
        m->state = MAIL_STATE_CHANGED;
        pl->m_mailsUpdated = true;
        pl->RemoveMItem(it->GetGUIDLow());

        uint32 count = it->GetCount();                      // save counts before store and possible merge with deleting
        pl->MoveItemToInventory(dest, it, true);

        CharacterDatabase.BeginTransaction();
        pl->SaveInventoryAndGoldToDB();
        pl->_SaveMail();
        CharacterDatabase.CommitTransaction();

        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_OK, 0, itemId, count);
    }
    else
    {
        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_ERR_EQUIP_ERROR, msg);
    }
}
/**
 * Handles the packet sent by the client when taking money from the mail.
 */
void WorldSession::HandleMailTakeMoney(WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid;
    uint32 mailId;
    uint64 money;
    recv_data >> mailboxGuid;
    recv_data >> mailId;
    recv_data >> money;

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = _player;

    Mail* m = pl->GetMail(mailId);
    if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL))
    {
        pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_OK);

    pl->ModifyMoney(m->money);
    m->money = 0;
    m->state = MAIL_STATE_CHANGED;
    pl->m_mailsUpdated = true;

    // save money and mail to prevent cheating
    CharacterDatabase.BeginTransaction();
    pl->SaveGoldToDB();
    pl->_SaveMail();
    CharacterDatabase.CommitTransaction();
}

/**
 * Handles the packet sent by the client when requesting the current mail list.
 * It will send a list of all available mails in the players mailbox to the client.
 */
void WorldSession::HandleGetMailList(WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid;
    recv_data >> mailboxGuid;

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    // client can't work with packets > max int16 value
    const uint32 maxPacketSize = 32767;

    uint32 mailsCount = 0;                                  // send to client mails amount
    uint32 realCount = 0;                                   // real mails amount

    WorldPacket data(SMSG_MAIL_LIST_RESULT, 200);           // guess size
    data << uint32(0);                                      // real mail's count
    data << uint8(0);                                       // mail's count
    time_t cur_time = time(NULL);

    for (PlayerMails::iterator itr = _player->GetMailBegin(); itr != _player->GetMailEnd(); ++itr)
    {
        // packet send mail count as uint8, prevent overflow
        if (mailsCount >= 254)
        {
            realCount += 1;
            continue;
        }

        // skip deleted or not delivered (deliver delay not expired) mails
        if ((*itr)->state == MAIL_STATE_DELETED || cur_time < (*itr)->deliver_time)
        {
            continue;
        }

        uint8 item_count = (*itr)->items.size();            // max count is MAX_MAIL_ITEMS (12)

        size_t next_mail_size = 2 + 4 + 1 + ((*itr)->messageType == MAIL_NORMAL ? 8 : 4) + 4 * 8 + ((*itr)->subject.size() + 1) + ((*itr)->body.size() + 1) + 1 + item_count * (1 + 4 + 4 + 7 * 3 * 4 + 4 + 4 + 4 + 4 + 4 + 4 + 1);

        if (data.wpos() + next_mail_size > maxPacketSize)
        {
            realCount += 1;
            continue;
        }

        data << uint16(next_mail_size);                     // Message size
        data << uint32((*itr)->messageID);                  // Message ID
        data << uint8((*itr)->messageType);                 // Message Type

        switch ((*itr)->messageType)
        {
            case MAIL_NORMAL:                               // sender guid
                data << ObjectGuid(HIGHGUID_PLAYER, (*itr)->sender);
                break;
            case MAIL_CREATURE:
            case MAIL_GAMEOBJECT:
            case MAIL_AUCTION:
                data << uint32((*itr)->sender);             // creature/gameobject entry, auction id
                break;
            case MAIL_CALENDAR:
                data << uint32(0);                          // unknown
                break;
        }

        data << uint64((*itr)->COD);                        // COD
        data << uint32(0);                                  // unknown, probably changed in 3.3.3
        data << uint32((*itr)->stationery);                 // stationery (Stationery.dbc)
        data << uint64((*itr)->money);                      // copper
        data << uint32((*itr)->checked);                    // flags
        data << float(float((*itr)->expire_time - time(NULL)) / float(DAY));// Time
        data << uint32((*itr)->mailTemplateId);             // mail template (MailTemplate.dbc)
        data << (*itr)->subject;                            // Subject string - once 00, when mail type = 3, max 256
        data << (*itr)->body;                               // message? max 8000

        data << uint8(item_count);                          // client limit is 0x10

        for (uint8 i = 0; i < item_count; ++i)
        {
            Item* item = _player->GetMItem((*itr)->items[i].item_guid);
            // item index (0-6?)
            data << uint8(i);
            // item guid low?
            data << uint32(item ? item->GetGUIDLow() : 0);
            // entry
            data << uint32(item ? item->GetEntry() : 0);
            for (uint8 j = 0; j < MAX_INSPECTED_ENCHANTMENT_SLOT; ++j)
            {
                // unsure
                data << uint32(item ? item->GetEnchantmentCharges((EnchantmentSlot)j) : 0);
                // unsure
                data << uint32(item ? item->GetEnchantmentDuration((EnchantmentSlot)j) : 0);
                // unsure
                data << uint32(item ? item->GetEnchantmentId((EnchantmentSlot)j) : 0);
            }
            // can be negative
            data << uint32(item ? item->GetItemRandomPropertyId() : 0);
            // unk
            data << uint32(item ? item->GetItemSuffixFactor() : 0);
            // stack count
            data << uint32(item ? item->GetCount() : 0);
            // charges
            data << uint32(item ? item->GetSpellCharges() : 0);
            // durability
            data << uint32(item ? item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY) : 0);
            // durability
            data << uint32(item ? item->GetUInt32Value(ITEM_FIELD_DURABILITY) : 0);
            // unknown wotlk
            data << uint8(0);
        }

        mailsCount += 1;
        realCount += 1;
    }

    data.put<uint32>(0, realCount);                         // this will display warning about undelivered mail to player if realCount > mailsCount
    data.put<uint8>(4, mailsCount);                         // set real send mails to client
    SendPacket(&data);

    // recalculate m_nextMailDelivereTime and unReadMails
    _player->UpdateNextMailTimeAndUnreads();
}

/*
 * Handles the packet sent by the client when he copies the body a mail to his inventory.
 *
 * When a player copies the body of a mail to his inventory this method is called. It will create
 * a new item with the text of the mail and store it in the players inventory (if possible).
 *
 */
void WorldSession::HandleMailCreateTextItem(WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid;
    uint32 mailId;

    recv_data >> mailboxGuid;
    recv_data >> mailId;

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = _player;

    Mail* m = pl->GetMail(mailId);
    if (!m || (m->body.empty() && !m->mailTemplateId) || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL))
    {
        pl->SendMailResult(mailId, MAIL_MADE_PERMANENT, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    Item* bodyItem = new Item;                              // This is not bag and then can be used new Item.
    if (!bodyItem->Create(sObjectMgr.GenerateItemLowGuid(), MAIL_BODY_ITEM_TEMPLATE, pl))
    {
        delete bodyItem;
        return;
    }

    // in mail template case we need create new item text
    if (m->mailTemplateId)
    {
        MailTemplateEntry const* mailTemplateEntry = sMailTemplateStore.LookupEntry(m->mailTemplateId);
        if (!mailTemplateEntry)
        {
            pl->SendMailResult(mailId, MAIL_MADE_PERMANENT, MAIL_ERR_INTERNAL_ERROR);
            delete bodyItem;
            return;
        }

        bodyItem->SetText(mailTemplateEntry->Body_lang[GetSessionDbcLocale()]);
    }
    else
    {
        bodyItem->SetText(m->body);
    }

    bodyItem->SetGuidValue(ITEM_FIELD_CREATOR, ObjectGuid(HIGHGUID_PLAYER, m->sender));
    bodyItem->SetFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_READABLE | ITEM_DYNFLAG_UNK15 | ITEM_DYNFLAG_UNK16);

    DETAIL_LOG("HandleMailCreateTextItem mailid=%u", mailId);

    ItemPosCountVec dest;
    InventoryResult msg = _player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, bodyItem, false);
    if (msg == EQUIP_ERR_OK)
    {
        m->checked = m->checked | MAIL_CHECK_MASK_COPIED;
        m->state = MAIL_STATE_CHANGED;
        pl->m_mailsUpdated = true;

        pl->StoreItem(dest, bodyItem, true);
        pl->SendMailResult(mailId, MAIL_MADE_PERMANENT, MAIL_OK);
    }
    else
    {
        pl->SendMailResult(mailId, MAIL_MADE_PERMANENT, MAIL_ERR_EQUIP_ERROR, msg);
        delete bodyItem;
    }
}

/**
 * No idea when this is called.
 */
void WorldSession::HandleQueryNextMailTime(WorldPacket & /**recv_data*/)
{
    WorldPacket data(MSG_QUERY_NEXT_MAIL_TIME, 8);

    if (_player->unReadMails > 0)
    {
        data << uint32(0);                                  // float
        data << uint32(0);                                  // count

        uint32 count = 0;
        time_t now = time(NULL);
        for (PlayerMails::iterator itr = _player->GetMailBegin(); itr != _player->GetMailEnd(); ++itr)
        {
            Mail* m = (*itr);
            // must be not checked yet
            if (m->checked & MAIL_CHECK_MASK_READ)
            {
                continue;
            }

            // and already delivered
            if (now < m->deliver_time)
            {
                continue;
            }

            data << ObjectGuid(HIGHGUID_PLAYER, m->sender); // sender guid

            switch (m->messageType)
            {
                case MAIL_AUCTION:
                    data << uint32(m->sender);              // auction house id
                    data << uint32(MAIL_AUCTION);           // message type
                    break;
                default:
                    data << uint32(0);
                    data << uint32(0);
                    break;
            }

            data << uint32(m->stationery);
            data << uint32(0xC6000000);                     // float unk, time or something

            ++count;
            if (count == 2)                                 // do not display more than 2 mails
            {
                break;
            }
        }
        data.put<uint32>(4, count);
    }
    else
    {
        data << uint32(0xC7A8C000);
        data << uint32(0x00000000);
    }
    SendPacket(&data);
}

/*! @} */
