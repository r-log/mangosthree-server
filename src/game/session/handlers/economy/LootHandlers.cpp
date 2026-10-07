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

#include "session/handlers/economy/LootHandlers.h"

#include <vector>

#include "Platform/Define.h"
#include "Server/OpcodeTable.h"
#include "WorldPacket.h"
#include "Log/Log.h"
#include "Object/Corpse.h"
#include "Object/Creature.h"
#include "Object/GameObject.h"
#include "WorldHandlers/AchievementMgr.h"
#include "entities/player/Player.h"
#include "entities/player/PlayerRegistry.h"
#include "Object/ObjectGuid.h"
#include "Server/WorldSession.h"
#include "Object/Item.h"
#include "Object/LootMgr.h"
#include "Object/Object.h"
#include "WorldHandlers/Group.h"
#include "Object/Unit.h"
#include "Server/DBCStores.h"

void LootHandlers::HandleAutostoreLootItem(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: %s", LookupOpcodeName(recv_data.GetOpcode()));
    Player*  player = session.GetPlayer();
    ObjectGuid lguid = player->GetLootGuid();
    Loot* loot = nullptr;
    uint8 lootSlot = 0;

    recv_data >> lootSlot;

    if (lguid.IsGameObject())
    {
        GameObject* go = player->GetMap()->GetGameObject(lguid);

        /* Checking if the player is in range of the object. */
        if (!go || ((go->GetOwnerGuid() != session.GetPlayer()->GetObjectGuid() && go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE) &&
            !InReach(*go, *session.GetPlayer(), INTERACTION_DISTANCE)))
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &go->loot;
    }
    else if (lguid.IsItem())
    {
        Item* pItem = player->GetInventoryMgr().GetItemByGuid(lguid);
        if (!pItem)
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &pItem->loot;
    }
    else if (lguid.IsCorpse())
    {
        // need to change this function to ObjectAccessor::GetCorpse() when implemented
        Corpse* bones = player->GetMap()->GetCorpse(lguid);
        if (!bones)
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &bones->loot;
    }
    else
    {
        Creature* creature = session.GetPlayer()->GetMap()->GetCreature(lguid);

        /* Checking if the player is a rogue and if the creature is alive. */
        bool lootAllowed = creature && creature->IsAlive() == (player->getClass() == CLASS_ROGUE && creature->loot.loot_type == LOOT_PICKPOCKETING);
        if (!lootAllowed || !InReach(*creature, *session.GetPlayer(), INTERACTION_DISTANCE))
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &creature->loot;
    }

    /* Checking if the loot is looted and if the guid is an item. If it is, it will release the loot. */
    if (loot->isLooted() && lguid.IsItem())
    {
        player->GetSession()->DoLootRelease(lguid);
    }

    QuestItem* qitem = NULL;
    QuestItem* ffaitem = NULL;
    QuestItem* conditem = NULL;
    QuestItem* currency = NULL;

    LootItem* item = loot->LootItemInSlot(lootSlot, player, &qitem, &ffaitem, &conditem, &currency);

    if (!item)
    {
        player->SendEquipError(EQUIP_ERR_ALREADY_LOOTED, NULL, NULL);
        return;
    }

    // questitems use the blocked field for other purposes
    // ToDo: this call is handled else where, not here.
    if (!qitem && item->is_blocked)
    {
        player->SendLootRelease(lguid);
        return;
    }

    if (currency)
    {
        if (CurrencyTypesEntry const * currencyEntry = sCurrencyTypesStore.LookupEntry(item->itemid))
        {
            player->ModifyCurrencyCount(item->itemid, int32(item->count * currencyEntry->GetPrecision()));
        }

        player->SendNotifyLootItemRemoved(lootSlot, true);
        currency->is_looted = true;
        --loot->unlootedCount;
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item->itemid, item->count);
    if (msg == EQUIP_ERR_OK)
    {
        Item* newitem = player->StoreNewItem(dest, item->itemid, true, item->randomPropertyId);

        if (qitem)
        {
            qitem->is_looted = true;
            // freeforall is 1 if everyone's supposed to get the quest item.
            if (item->freeforall || loot->GetPlayerQuestItems().size() == 1)
            {
                player->SendNotifyLootItemRemoved(lootSlot);
            }
            else
            {
                loot->NotifyQuestItemRemoved(qitem->index);
            }
        }
        else
        {
            if (ffaitem)
            {
                // freeforall case, notify only one player of the removal
                ffaitem->is_looted = true;
                player->SendNotifyLootItemRemoved(lootSlot);
            }
            else
            {
                // not freeforall, notify everyone
                if (conditem)
                {
                    conditem->is_looted = true;
                }
                loot->NotifyItemRemoved(lootSlot);
            }
        }

        // if only one person is supposed to loot the item, then set it to looted
        if (!item->freeforall)
        {
            item->is_looted = true;
        }

        --loot->unlootedCount;

        player->SendNewItem(newitem, uint32(item->count), false, false, true);

        player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM, item->itemid, item->count);
        player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE, loot->loot_type, item->count);
        player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_EPIC_ITEM, item->itemid, item->count);
    }
    else
    {
        player->SendEquipError(msg, NULL, NULL, item->itemid);
    }
}

/**
 * @brief Handles looting money from the player's current loot target.
 *
 * @param recv_data The unused incoming packet.
 */
void LootHandlers::HandleLootMoney(WorldSession& session, WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: CMSG_LOOT_MONEY");

    Player* player = session.GetPlayer();
    ObjectGuid guid = player->GetLootGuid();
    if (!guid)
    {
        return;
    }

    Loot* pLoot = NULL;
    Item* pItem = NULL;
    bool shareMoney = true;

    switch (guid.GetHigh())
    {
        case HIGHGUID_GAMEOBJECT:
        {
            GameObject* pGameObject = session.GetPlayer()->GetMap()->GetGameObject(guid);

            // not check distance for GO in case owned GO (fishing bobber case, for example)
            if (pGameObject && (pGameObject->GetOwnerGuid() == session.GetPlayer()->GetObjectGuid() || InReach(*pGameObject, *session.GetPlayer(), INTERACTION_DISTANCE)))
            {
                pLoot = &pGameObject->loot;
            }

            break;
        }
        case HIGHGUID_CORPSE:                               // remove insignia ONLY in BG
        {
            Corpse* bones = session.GetPlayer()->GetMap()->GetCorpse(guid);

            if (bones && InReach(*bones, *session.GetPlayer(), INTERACTION_DISTANCE))
            {
                pLoot = &bones->loot;
                shareMoney = false;
            }

            break;
        }
        case HIGHGUID_ITEM:
        {
            if (Item* item = session.GetPlayer()->GetInventoryMgr().GetItemByGuid(guid))
            {
                pLoot = &item->loot;
                shareMoney = false;
            }
            break;
        }
        case HIGHGUID_UNIT:
        case HIGHGUID_VEHICLE:
        {
            Creature* pCreature = session.GetPlayer()->GetMap()->GetCreature(guid);

            bool ok_loot = pCreature && pCreature->IsAlive() == (player->getClass() == CLASS_ROGUE && pCreature->lootForPickPocketed);

            if (ok_loot && InReach(*pCreature, *session.GetPlayer(), INTERACTION_DISTANCE))
            {
                pLoot = &pCreature->loot;
                if (pCreature->IsAlive())
                {
                    shareMoney = false;
                }
            }

            break;
        }
        default:
            return;                                         // unlootable type
    }

    if (pLoot)
    {
        pLoot->NotifyMoneyRemoved();
        if (shareMoney && player->GetGroup())           // item, pickpocket and players can be looted only single player
        {
            Group* group = player->GetGroup();

            std::vector<Player*> playersNear;
            for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* playerGroup = itr->getSource();
                if (!playerGroup)
                {
                    continue;
                }

                if (player->IsAtGroupRewardDistance(playerGroup))
                {
                    playersNear.push_back(playerGroup);
                }
            }

            if (playersNear.empty())
            {
                player->ModifyMoney(pLoot->gold);
                player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, pLoot->gold);

                WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4 + 1);
                data << uint32(pLoot->gold);
                data << uint8(1);
                session.SendPacket(&data);
            }
            else
            {
                uint64 money_per_player = uint32((pLoot->gold) / (playersNear.size()));

                for (std::vector<Player*>::const_iterator i = playersNear.begin(); i != playersNear.end(); ++i)
                {
                    (*i)->ModifyMoney(money_per_player);
                    (*i)->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, money_per_player);

                    WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4 + 1);
                    data << uint32(money_per_player);
                    // Controls the text displayed in chat. 0 is "Your share is..." and 1 is "You loot..."
                    data << uint8(playersNear.size() <= 1);
                    (*i)->SendDirectMessage(&data);
                }
            }
        }
        else
        {
            player->ModifyMoney(pLoot->gold);
            player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, pLoot->gold);

            WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4 + 1);
            data << uint32(pLoot->gold);
            data << uint8(1); // 1 is "you loot..."
            session.SendPacket(&data);
        }

        pLoot->gold = 0;

        /* Checking if the loot is looted and if the guid is an item. If it is, it will release the loot. */
        if (pLoot->isLooted() && guid.IsItem())
        {
            player->GetSession()->DoLootRelease(guid);
        }
    }
}

/**
 * @brief Starts a loot interaction for the requested object guid.
 *
 * @param recv_data The incoming loot request packet.
 */
void LootHandlers::HandleLoot(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_LOOT");

    ObjectGuid guid;
    recv_data >> guid;

    // Check possible cheat
    if (!session.GetPlayer()->IsAlive() || !guid.IsCreatureOrVehicle())
    {
        return;
    }

    session.GetPlayer()->SendLoot(guid, LOOT_CORPSE);

    /* Checking if the player is casting a spell, and if so, it is interrupting it. */
    if (session.GetPlayer()->IsNonMeleeSpellCasted(false))
    {
        session.GetPlayer()->InterruptNonMeleeSpells(false);
    }
}

/**
 * @brief Handles a client request to close the active loot window.
 *
 * @param recv_data The incoming loot release packet.
 */
void LootHandlers::HandleLootRelease(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_LOOT_RELEASE");

    // cheaters can modify lguid to prevent correct apply loot release code and re-loot
    // use internal stored guid
    recv_data.read_skip<uint64>();                          // guid;

    if (ObjectGuid lootGuid = session.GetPlayer()->GetLootGuid())
    {
        session.DoLootRelease(lootGuid);
    }
}

/**
 * @brief Handles master-loot assignment of a specific loot slot to another player.
 *
 * @param recv_data The incoming master-loot packet.
 */
void LootHandlers::HandleLootMasterGive(WorldSession& session, WorldPacket& recv_data)
{
    uint8 slotid;
    ObjectGuid lootguid;
    ObjectGuid target_playerguid;

    recv_data >> lootguid >> slotid >> target_playerguid;

    if (!session.GetPlayer()->GetGroup() || session.GetPlayer()->GetGroup()->GetLooterGuid() != session.GetPlayer()->GetObjectGuid())
    {
        session.GetPlayer()->SendLootRelease(session.GetPlayer()->GetLootGuid());
        return;
    }

    Player* target = sPlayerRegistry.Find(target_playerguid);
    if (!target)
    {
        return;
    }

    DEBUG_LOG("WorldSession::HandleLootMasterGiveOpcode (CMSG_LOOT_MASTER_GIVE, 0x02A3) Target = %s [%s].", target_playerguid.GetString().c_str(), target->GetName());

    if (session.GetPlayer()->GetLootGuid() != lootguid)
    {
        return;
    }

    /* Checking if the player is in the same raid as the target and if the player is in the same map as the target. */
    if (!session.GetPlayer()->IsInSameRaidWith(target->ToPlayer()) || !session.GetPlayer()->Where().ShareFrame(target->Where()))
    {
        sLog.outBasic("MasterLootItem: Player %s tried to give an item to ineligible player %s!", session.GetPlayer()->GetName(), target->GetName());
        return;
    }

    Loot* pLoot = NULL;

    if (session.GetPlayer()->GetLootGuid().IsCreatureOrVehicle())
    {
        Creature* pCreature = session.GetPlayer()->GetMap()->GetCreature(lootguid);
        if (!pCreature)
        {
            return;
        }

        pLoot = &pCreature->loot;
    }
    else if (session.GetPlayer()->GetLootGuid().IsGameObject())
    {
        GameObject* pGO = session.GetPlayer()->GetMap()->GetGameObject(lootguid);
        if (!pGO)
        {
            return;
        }

        pLoot = &pGO->loot;
    }

    if (!pLoot)
    {
        return;
    }

    if (slotid >= pLoot->items.size())
    {
        DEBUG_LOG("AutoLootItem: Player %s might be using a hack! (slot %d, size %zu)", session.GetPlayer()->GetName(), slotid, (unsigned long)pLoot->items.size());
        return;
    }

    LootItem& item = pLoot->items[slotid];
    if (item.currency)
    {
        sLog.outError("WorldSession::HandleLootMasterGiveOpcode: player %s tried to give currency via master loot! Hack alert! Slot %u, currency id %u",
            session.GetPlayer()->GetName(), slotid, item.itemid);
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = target->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item.itemid, item.count);
    if (!item.AllowedForPlayer(target, nullptr))
    {
        msg = EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM;
    }

    // ToDo: fix the equp error messages
    if (msg != EQUIP_ERR_OK)
    {
        target->SendEquipError(msg, NULL, NULL, item.itemid);

        // send duplicate of error massage to master looter
        session.GetPlayer()->SendEquipError(msg, NULL, NULL, item.itemid);
        return;
    }

    // now move item from loot to target inventory
    Item* newitem = target->StoreNewItem(dest, item.itemid, true, item.randomPropertyId);
    target->SendNewItem(newitem, uint32(item.count), false, false, true);
    target->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM, item.itemid, item.count);
    target->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE, pLoot->loot_type, item.count);
    target->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_EPIC_ITEM, item.itemid, item.count);

    // mark as looted
    item.count = 0;
    item.is_looted = true;

    pLoot->NotifyItemRemoved(slotid);
    --pLoot->unlootedCount;
}
