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
 * @file LootHandler.cpp
 * @brief Loot interaction opcode handlers
 *
 * This file handles loot-related opcodes including:
 * - CMSG_AUTOSTORE_LOOT_ITEM: Auto-loot item to inventory
 * - CMSG_LOOT: Open loot window
 * - CMSG_LOOT_MONEY: Loot money
 * - CMSG_LOOT_RELEASE: Close loot window
 * - CMSG_LOOT_ROLL: Roll for loot item
 * - CMSG_MASTER_LOOT_ITEM: Master looter distributes item
 *
 * Loot can come from creatures, gameobjects, fishing, and mail.
 * Different loot methods (Free for All, Round Robin, Master Looter, Group Loot)
 * determine how items are distributed among party members.
 */

#include "Platform/Define.h"
#include "Corpse.h"
#include "Creature.h"
#include "GameObject.h"
#include "Player.h"
#include "ObjectGuid.h"
#include "WorldSession.h"
#include "Item.h"
#include "LootMgr.h"
#include "Object.h"
#include "Unit.h"

/**
 * @brief Finalizes loot state updates when a player releases a loot target.
 *
 * @param lguid The guid of the released loot source.
 */
void WorldSession::DoLootRelease(ObjectGuid lguid)
{
    Player* player = GetPlayer();
    Loot* loot;

    player->SetLootGuid(ObjectGuid());
    player->SendLootRelease(lguid);

    player->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_LOOTING);

    if (!player->IsInWorld())
    {
        return;
    }

    if (lguid.IsGameObject())
    {
        GameObject* go = GetPlayer()->GetMap()->GetGameObject(lguid);

        /* Checking if the player is in range of the object. */
        if (!go || ((go->GetOwnerGuid() != _player->GetObjectGuid() && go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE) &&
            !InReach(*go, *_player, INTERACTION_DISTANCE)))
            return;

        loot = &go->loot;

        /* The above code is checking if the gameobject is a door, if it is then it uses the door. If
         * the gameobject is a fishing node, then it checks if it has been looted, if it has then it
         * checks if the max success opens is greater than 0, if it is then it sets the loot state to
         * just deactivated, if not then it sets the loot state to ready. If the gameobject is not a
         * door or a fishing node, then it sets the loot state to activated.
         */
        if (go->GetGoType() == GAMEOBJECT_TYPE_DOOR)
        {
            go->UseDoorOrButton();
        }
        else if (loot->isLooted() || go->GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE)
        {
            if (go->GetGoType() == GAMEOBJECT_TYPE_FISHINGHOLE)
            {
                go->AddUse();
                if (go->GetUseCount() >= go->GetGOInfo()->fishinghole.maxSuccessOpens)
                {
                    go->SetLootState(GO_JUST_DEACTIVATED);
                }
                else
                {
                    go->SetLootState(GO_READY);
                }
            }
            else
            {
                go->SetLootState(GO_JUST_DEACTIVATED);
            }

            /* Clearing the loot vector. */
            loot->clear();
        }
        else
        {
            go->SetLootState(GO_ACTIVATED);
        }
    }
    else if (lguid.IsCorpse()) // ONLY remove insignia at BG
    {
        Corpse* corpse = _player->GetMap()->GetCorpse(lguid);
        if (!corpse || !InReach(*corpse, *_player, INTERACTION_DISTANCE))
        {
            return;
        }

        loot = &corpse->loot;

        /* Checking if the corpse is looted, if it is, it clears the loot and removes the lootable flag. */
        if (loot->isLooted())
        {
            /* Removing the lootable flag from the corpse. */
            loot->clear();
            corpse->RemoveFlag(CORPSE_FIELD_DYNAMIC_FLAGS, CORPSE_DYNFLAG_LOOTABLE);
        }
    }
    else if (lguid.IsItem())
    {
        Item* pItem = player->GetInventoryMgr().GetItemByGuid(lguid);
        if (!pItem)
        {
            return;
        }

        /* Checking if the item is prospectable or millable, if it is, it will clear the loot, then it
         * will check if the count is greater than 5, if it is, it will set the count to 5, then it
         * will destroy the item.
         */
        if (pItem->loot.loot_type == ITEM_FLAG_PROSPECTABLE || pItem->loot.loot_type == ITEM_FLAG_MILLABLE)
        {
            /* Clearing the loot vector of the item. */
            pItem->loot.clear();

            uint32 count = pItem->GetCount();
            if (count > 5)
            {
                count = 5;
            }

            /* Destroying the item count. */
            player->DestroyItemCount(pItem, count, true);
        }
        else
        {
            /* Checking if the item is looted or not. If it is looted, it will destroy the item. */
            if (pItem->loot.isLooted() || pItem->loot.loot_type != ITEM_FLAG_LOOTABLE)
            {
                /* Destroying the item in the player's inventory. */
                player->DestroyItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
            }
        }
        return; // item can be looted only single player
    }
    else
    {
        Creature* creature = GetPlayer()->GetMap()->GetCreature(lguid);

        /* Checking if the creature is alive and if the player is a rogue. */
        bool lootAllowed = creature && creature->IsAlive() == (player->getClass() == CLASS_ROGUE && creature->loot.loot_type == LOOT_PICKPOCKETING);
        if (!lootAllowed || !InReach(*creature, *_player, INTERACTION_DISTANCE))
        {
            return;
        }

        loot = &creature->loot;

        /* Checking if the creature is looted, if it is then it removes the lootable flag and clears the loot. */
        if (loot->isLooted())
        {
            creature->RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);

            if (!creature->IsAlive())
            {
                creature->AllLootRemovedFromCorpse();
            }

            loot->clear();
        }
        else
        {
            /* Forcing the creature to update its dynamic flags. */
            creature->ForceValuesUpdateAtIndex(UNIT_DYNAMIC_FLAGS);
        }
    }

    /* Removing the player from the looter list. */
    loot->RemoveLooter(player->GetObjectGuid());
}
