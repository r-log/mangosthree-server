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
 * @file PlayerSpellCooldown.cpp
 * @brief The character's side of its spell cooldowns (spells/SpellCooldownMgr).
 *
 * SpellCooldownMgr holds the cooldown map and the rules over it and never sees the character.
 * The wrappers here read and write the character for it: the clock, the one-spell clear of the
 * arena reset and the login loop. Each input is read just before the one call into the manager;
 * each write is a callback the manager calls at the write. A cast's cooldowns and the potion's are
 * Unit's (Unit.cpp), the forwarders whose inputs are plain expressions (the clock, the guid, a
 * stored callback) stay inline in Player.h, and the cooldown event and the clear of every cooldown
 * go to the session's callbacks in CooldownSinks.
 */

#include "Player.h"
#include "DBCStores.h"
#include "ObjectMgr.h"
#include "spells/pipeline/Spell.h"
#include "spells/data/SpellMgr.h"
#include "persistence/characters/spells/SpellCooldownRepository.h"

#include <ctime>

namespace
{
    /// The one-spell clear SpellCooldownMgr::RemoveArenaSpellCooldowns reports.
    SpellCooldownMgr::ClearSink ClearCooldownSink(Player* player)
    {
        return [player](uint32 spellId)
        {
            player->SendClearCooldown(spellId, player);
        };
    }
}

void Player::RemoveArenaSpellCooldowns()
{
    m_spellCooldownMgr.RemoveArenaSpellCooldowns(ClearCooldownSink(this));
}

void Player::_LoadSpellCooldowns(std::vector<SpellCooldownRow> const& rows)
{
    // some cooldowns can be already set at aura loading...

    if (!rows.empty())
    {
        time_t curTime = time(NULL);

        for (SpellCooldownRow const& row : rows)
        {
            m_spellCooldownMgr.LoadRow(row, curTime, GetGUIDLow());
        }

    }
}
