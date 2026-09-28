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
 * @brief Decoupling D4k: the character's side of its spell cooldowns (spells/SpellCooldownMgr).
 *
 * SpellCooldownMgr holds the cooldown map and the rules over it and never sees the character.
 * The wrappers here are the part of the old SpellCooldownMgr bodies that read or wrote the
 * character: the clock, the ranged attack time, the auto-repeat check, the item prototype store,
 * the cooldown spell mods, the one-spell clear, the login loop and the potion bookkeeping. Each
 * input is read with the old expression just before the one call into the manager; each write
 * is a callback the manager calls where the old body wrote. The forwarders whose inputs are plain
 * expressions (the clock, the guid, the session sink) stay inline in Player.h, and the packets the
 * manager builds go through Player::SessionSink().
 */

#include "Player.h"
#include "DBCStores.h"
#include "ObjectMgr.h"
#include "Spell.h"
#include "SpellMgr.h"

#include <ctime>

namespace
{
    /// What SpellCooldownMgr::AddSpellAndCategoryCooldowns read from the character before
    /// decoupling D4k, read here just before the call. The spell mods keep the cast they were
    /// given (`spell`, possibly NULL) and apply to the value the manager hands them.
    SpellCooldownMgr::CastInputs ReadCastInputs(Player* player, SpellEntry const* spellInfo, Spell* spell)
    {
        SpellCooldownMgr::CastInputs inputs;
        inputs.itemPrototype = ObjectMgr::GetItemPrototype;
        inputs.autoRepeatRanged = IsAutoRepeatRangedSpell(spellInfo);
        inputs.rangedAttackTime = player->GetAttackTime(RANGED_ATTACK);
        inputs.applyCooldownMod = [player, spell](uint32 spellId, int32& cooldown)
        {
            player->ApplySpellMod(spellId, SPELLMOD_COOLDOWN, cooldown, spell);
        };
        return inputs;
    }

    /// The one-spell clear SpellCooldownMgr::RemoveSpellCooldown sent before decoupling D4k.
    SpellCooldownMgr::ClearSink ClearCooldownSink(Player* player)
    {
        return [player](uint32 spellId)
        {
            player->SendClearCooldown(spellId, player);
        };
    }
}

void Player::AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo, uint32 itemId, Spell* spell, bool infinityCooldown)
{
    time_t now = time(NULL);
    SpellCooldownMgr::CastInputs const inputs = ReadCastInputs(this, spellInfo, spell);

    m_spellCooldownMgr.AddSpellAndCategoryCooldowns(spellInfo, itemId, now, inputs, infinityCooldown);
}

void Player::SendCooldownEvent(SpellEntry const* spellInfo, uint32 itemId, Spell* spell)
{
    time_t now = time(NULL);
    SpellCooldownMgr::CastInputs const inputs = ReadCastInputs(this, spellInfo, spell);

    m_spellCooldownMgr.SendCooldownEvent(spellInfo, itemId, now, inputs, GetObjectGuid(), SessionSink());
}

void Player::RemoveSpellCooldown(uint32 spell_id, bool update)
{
    m_spellCooldownMgr.RemoveSpellCooldown(spell_id, update, ClearCooldownSink(this));
}

void Player::RemoveSpellCategoryCooldown(uint32 cat, bool update)
{
    m_spellCooldownMgr.RemoveSpellCategoryCooldown(cat, update, ClearCooldownSink(this));
}

void Player::RemoveArenaSpellCooldowns()
{
    m_spellCooldownMgr.RemoveArenaSpellCooldowns(ClearCooldownSink(this));
}

void Player::_LoadSpellCooldowns(QueryResult* result)
{
    // some cooldowns can be already set at aura loading...

    // the rows are the login holder's PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS result (CharacterHandler.cpp)

    if (result)
    {
        time_t curTime = time(NULL);

        do
        {
            m_spellCooldownMgr.LoadRow(result->Fetch(), curTime, GetGUIDLow());
        }
        while (result->NextRow());

        delete result;
    }
}

void Player::UpdatePotionCooldown(Spell* spell)
{
    // no potion used in combat or still in combat
    if (!GetLastPotionId() || IsInCombat())
    {
        return;
    }

    // Call not from spell cast, send cooldown event for item spells if no in combat
    if (!spell)
    {
        // spell/item pair let set proper cooldown (except nonexistent charged spell cooldown spellmods for potions)
        if (ItemPrototype const* proto = ObjectMgr::GetItemPrototype(GetLastPotionId()))
            for (int idx = 0; idx < 5; ++idx)
                if (proto->Spells[idx].SpellId && proto->Spells[idx].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
                    if (SpellEntry const* spellInfo = sSpellStore.LookupEntry(proto->Spells[idx].SpellId))
                    {
                        SendCooldownEvent(spellInfo, GetLastPotionId());
                    }
    }
    // from spell cases (m_lastPotionId set in Spell::SendSpellCooldown)
    else
    {
        SendCooldownEvent(spell->m_spellInfo, GetLastPotionId(), spell);
    }

    SetLastPotionId(0);
}
