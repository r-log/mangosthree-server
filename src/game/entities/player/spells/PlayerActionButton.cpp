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

#include "Player.h"
#include "data/Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "spells/data/SpellMgr.h"
#include "entities/player/spells/PlayerSpellNotices.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GridNotifiersImpl.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Util.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "Vehicle.h"
#include "DisableMgr.h"

#include <cmath>

/* Called from Player::SendInitialPacketsBeforeAddToMap */
/**
 * @brief Sends the player's initial action bar state to the client.
 */
void Player::SendInitialActionButtons() const
{
    DETAIL_LOG("Initializing Action Buttons for '%u' spec '%u'", GetGUIDLow(), m_talentMgr.ActiveSpec());

    std::vector<uint32> packed;
    packed.reserve(MAX_ACTION_BUTTONS);
    ActionButtonList const& currentActionButtonList = m_actionButtons[m_talentMgr.ActiveSpec()];
    for (uint8 button = 0; button < MAX_ACTION_BUTTONS; ++button)
    {
        /* Try and get each action button the player could have */
        ActionButtonList::const_iterator itr = currentActionButtonList.find(button);

        /* If the button is valid and not deleted */
        if (itr != currentActionButtonList.end() && itr->second.uState != ACTIONBUTTON_DELETED)
        {
            /* Send the data */
            packed.push_back(itr->second.packedData);
        }
        else
        {
            /* Nothing to send, so just send 0 */
            packed.push_back(0);
        }
    }
    sPlayerSpellNotices.ActionButtonsShown(*this, packed);
    DETAIL_LOG("Action Buttons for '%u' spec '%u' Initialized", GetGUIDLow(), m_talentMgr.ActiveSpec());
}

void Player::SendLockActionButtons() const
{
    DETAIL_LOG("Locking Action Buttons for '%u' spec '%u'", GetGUIDLow(), m_talentMgr.ActiveSpec());
    sPlayerSpellNotices.ActionButtonsLocked(*this);
}

/**
 * @brief Adds or updates an action bar button entry.
 *
 * @param button The action bar button index.
 * @param action The action identifier to bind.
 * @param type The action button type.
 * @return The updated action button, or null if validation fails.
 */
ActionButton* Player::addActionButton(uint8 spec, uint8 button, uint32 action, uint8 type)
{
    // check action only for active spec (so not check at copy/load passive spec)
    ActionButtonOwner const owner = { GetName(), [this](uint32 spellId) { return HasSpell(spellId); } };
    if (spec == m_talentMgr.ActiveSpec() && !IsActionButtonDataValid(button, action, type, &owner))
    {
        return NULL;
    }

    // it create new button (NEW state) if need or return existing
    ActionButton& ab = m_actionButtons[spec][button];

    // set data and update to CHANGED if not NEW
    ab.SetActionAndType(action, ActionButtonType(type));

    DETAIL_LOG("Player '%u' Added Action '%u' (type %u) to Button '%u' for spec %u", GetGUIDLow(), action, uint32(type), button, spec);
    return &ab;
}

/**
 * @brief Removes an action bar button entry.
 *
 * @param button The action bar button index to remove.
 */
void Player::removeActionButton(uint8 spec, uint8 button)
{
    ActionButtonList& currentActionButtonList = m_actionButtons[spec];
    ActionButtonList::iterator buttonItr = currentActionButtonList.find(button);
    if (buttonItr == currentActionButtonList.end() || buttonItr->second.uState == ACTIONBUTTON_DELETED)
    {
        return;
    }

    if (buttonItr->second.uState == ACTIONBUTTON_NEW)
    {
        currentActionButtonList.erase(buttonItr);           // new and not saved
    }
    else
    {
        buttonItr->second.uState = ACTIONBUTTON_DELETED; // saved, will deleted at next save
    }

    DETAIL_LOG("Action Button '%u' Removed from Player '%u' for spec %u", button, GetGUIDLow(), spec);
}

ActionButton const* Player::GetActionButton(uint8 button)
{
    ActionButtonList& currentActionButtonList = m_actionButtons[m_talentMgr.ActiveSpec()];
    ActionButtonList::iterator buttonItr = currentActionButtonList.find(button);
    if (buttonItr == currentActionButtonList.end() || buttonItr->second.uState == ACTIONBUTTON_DELETED)
    {
        return NULL;
    }

    return &buttonItr->second;
}
