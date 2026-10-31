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

#include "data/PlayerCreateInfo.h"

#include "DBCStores.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "spells/data/SpellMgr.h"

/**
 * @brief Validates action bar data before it is stored or loaded.
 *
 * @param button The action bar button index.
 * @param action The action identifier assigned to the button.
 * @param type The action button type.
 * @param owner The player being validated for, or null for template data.
 * @param msg Whether to log why the data is not valid.
 * @return True if the action button data is valid; otherwise, false.
 */
bool IsActionButtonDataValid(uint8 button, uint32 action, uint8 type, ActionButtonOwner const* owner, bool msg)
{
    if (button >= MAX_ACTION_BUTTONS)
    {
        if (msg)
        {
            if (owner)
                sLog.outError("Action %u not added into button %u for player %s: button must be < %u", action, button, owner->name, MAX_ACTION_BUTTONS);
            else
            {
                sLog.outError("Table `playercreateinfo_action` have action %u into button %u : button must be < %u", action, button, MAX_ACTION_BUTTONS);
            }
        }
        return false;
    }

    if (action >= MAX_ACTION_BUTTON_ACTION_VALUE)
    {
        if (msg)
        {
            if (owner)
                sLog.outError("Action %u not added into button %u for player %s: action must be < %u", action, button, owner->name, MAX_ACTION_BUTTON_ACTION_VALUE);
            else
            {
                sLog.outError("Table `playercreateinfo_action` have action %u into button %u : action must be < %u", action, button, MAX_ACTION_BUTTON_ACTION_VALUE);
            }
        }
        return false;
    }

    switch (type)
    {
        case ACTION_BUTTON_SPELL:
        {
            SpellEntry const* spellProto = sSpellStore.LookupEntry(action);
            if (!spellProto)
            {
                if (msg)
                {
                    if (owner)
                        sLog.outError("Spell action %u not added into button %u for player %s: spell not exist", action, button, owner->name);
                    else
                    {
                        sLog.outError("Table `playercreateinfo_action` have spell action %u into button %u: spell not exist", action, button);
                    }
                }
                return false;
            }

            if (owner)
            {
                if (!owner->knowsSpell(spellProto->ID))
                {
                    if (msg)
                        sLog.outError("Spell action %u not added into button %u for player %s: player don't known this spell", action, button, owner->name);
                    return false;
                }
                else if (IsPassiveSpell(spellProto))
                {
                    if (msg)
                        sLog.outError("Spell action %u not added into button %u for player %s: spell is passive", action, button, owner->name);
                    return false;
                }
                // current range for button of totem bar is from ACTION_BUTTON_SHAMAN_TOTEMS_BAR to (but not including) ACTION_BUTTON_SHAMAN_TOTEMS_BAR + 12
                else if (button >= ACTION_BUTTON_SHAMAN_TOTEMS_BAR && button < (ACTION_BUTTON_SHAMAN_TOTEMS_BAR + 12)
                         && !spellProto->HasAttribute(SPELL_ATTR_EX7_TOTEM_SPELL))
                {
                    if (msg)
                        sLog.outError("Spell action %u not added into button %u for player %s: attempt to add non totem spell to totem bar", action, button, owner->name);
                    return false;
                }
            }
            break;
        }
        case ACTION_BUTTON_ITEM:
        {
            if (!ObjectMgr::GetItemPrototype(action))
            {
                if (msg)
                {
                    if (owner)
                        sLog.outError("Item action %u not added into button %u for player %s: item not exist", action, button, owner->name);
                    else
                    {
                        sLog.outError("Table `playercreateinfo_action` have item action %u into button %u: item not exist", action, button);
                    }
                }
                return false;
            }
            break;
        }
        default:
            break;                                          // other cases not checked at this moment
    }

    return true;
}
