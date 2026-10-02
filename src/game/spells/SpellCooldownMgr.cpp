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

#include "SpellCooldownMgr.h"
#include "Log.h"
#include "Utilities/Errors.h"
#include "ObjectGuid.h"
#include "ItemPrototype.h"
#include "SharedDefines.h"
#include "DBCStores.h"
#include "Database/DatabaseEnv.h"

void SpellCooldownMgr::AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo, uint32 itemId, time_t now, CastInputs const& inputs, bool infinityCooldown)
{
    // init cooldown values
    uint32 cat   = 0;
    int32 rec    = -1;
    int32 catrec = -1;

    // some special item spells without correct cooldown in SpellInfo
    // cooldown information stored in item prototype
    // This used in same way in WorldSession::HandleItemQuerySingleOpcode data sending to client.

    if (itemId)
    {
        if (ItemPrototype const* proto = inputs.itemPrototype(itemId))
        {
            for (int idx = 0; idx < MAX_ITEM_PROTO_SPELLS; ++idx)
            {
                if (proto->Spells[idx].SpellId == spellInfo->ID)
                {
                    cat    = proto->Spells[idx].SpellCategory;
                    rec    = proto->Spells[idx].SpellCooldown;
                    catrec = proto->Spells[idx].SpellCategoryCooldown;
                    break;
                }
            }
        }
    }

    // if no cooldown found above then base at DBC data
    if (rec < 0 && catrec < 0)
    {
        cat = spellInfo->GetCategory();
        rec = spellInfo->GetRecoveryTime();
        catrec = spellInfo->GetCategoryRecoveryTime();
    }

    time_t curTime = now;

    time_t catrecTime;
    time_t recTime;

    // overwrite time for selected category
    if (infinityCooldown)
    {
        // use +MONTH as infinity mark for spell cooldown (will checked as MONTH/2 at save ans skipped)
        // but not allow ignore until reset or re-login
        catrecTime = catrec > 0 ? curTime + infinityCooldownDelay : 0;
        recTime    = rec    > 0 ? curTime + infinityCooldownDelay : catrecTime;
    }
    else
    {
        // shoot spells used equipped item cooldown values already assigned in GetAttackTime(RANGED_ATTACK)
        // prevent 0 cooldowns set by another way
        if (rec <= 0 && catrec <= 0 && (cat == 76 || (inputs.autoRepeatRanged && spellInfo->ID != SPELL_ID_AUTOSHOT)))
        {
            rec = inputs.rangedAttackTime;
        }

        // Now we have cooldown data (if found any), time to apply mods
        if (rec > 0)
        {
            inputs.applyCooldownMod(spellInfo->ID, rec);
        }

        if (catrec > 0)
        {
            inputs.applyCooldownMod(spellInfo->ID, catrec);
        }

        // replace negative cooldowns by 0
        if (rec < 0) rec = 0;
        {
            if (catrec < 0) catrec = 0;
        }

        // no cooldown after applying spell mods
        if (rec == 0 && catrec == 0)
        {
            return;
        }

        catrecTime = catrec ? curTime + catrec / IN_MILLISECONDS : 0;
        recTime    = rec ? curTime + rec / IN_MILLISECONDS : catrecTime;
    }

    // self spell cooldown
    if (recTime > 0)
    {
        AddSpellCooldown(spellInfo->ID, itemId, recTime);
    }

    // category spells
    if (cat && catrec > 0)
    {
        SpellCategoryStore::const_iterator i_scstore = sSpellCategoryStore.find(cat);
        if (i_scstore != sSpellCategoryStore.end())
        {
            for (SpellCategorySet::const_iterator i_scset = i_scstore->second.begin(); i_scset != i_scstore->second.end(); ++i_scset)
            {
                if (*i_scset == spellInfo->ID)              // skip main spell, already handled above
                {
                    continue;
                }

                AddSpellCooldown(*i_scset, itemId, catrecTime);
            }
        }
    }
}

void SpellCooldownMgr::AddSpellCooldown(uint32 spellid, uint32 itemid, time_t end_time)
{
    SpellCooldown sc;
    sc.end = end_time;
    sc.itemid = itemid;
    m_cooldowns[spellid] = sc;
}

void SpellCooldownMgr::SendCooldownEvent(SpellEntry const* spellInfo, uint32 itemId, time_t now, CastInputs const& inputs, ObjectGuid ownerGuid, CooldownEventSink const& sendEvent)
{
    // start cooldowns at server side, if any
    AddSpellAndCategoryCooldowns(spellInfo, itemId, now, inputs);

    // Send activate cooldown timer (possible 0) at client side
    CooldownEventFact fact;
    fact.spellId = spellInfo->ID;
    fact.owner = ownerGuid;
    MANGOS_ASSERT(sendEvent);
    sendEvent(fact);
}

void SpellCooldownMgr::RemoveSpellCooldown(uint32 spell_id, bool update, ClearSink const& sendClear)
{
    m_cooldowns.erase(spell_id);

    if (update)
    {
        sendClear(spell_id);
    }
}

void SpellCooldownMgr::RemoveSpellCategoryCooldown(uint32 cat, bool update, ClearSink const& sendClear)
{
    SpellCategoryStore::const_iterator ct = sSpellCategoryStore.find(cat);
    if (ct == sSpellCategoryStore.end())
    {
        return;
    }

    const SpellCategorySet& ct_set = ct->second;
    for (SpellCooldowns::const_iterator i = m_cooldowns.begin(); i != m_cooldowns.end();)
    {
        if (ct_set.find(i->first) != ct_set.end())
        {
            RemoveSpellCooldown((i++)->first, update, sendClear);
        }
        else
        {
            ++i;
        }
    }
}

void SpellCooldownMgr::RemoveArenaSpellCooldowns(ClearSink const& sendClear)
{
    // remove cooldowns on spells that has < 15 min CD
    SpellCooldowns::iterator itr, next;
    // iterate spell cooldowns
    for (itr = m_cooldowns.begin(); itr != m_cooldowns.end(); itr = next)
    {
        next = itr;
        ++next;
        SpellEntry const* entry = sSpellStore.LookupEntry(itr->first);
        // check if spellentry is present and if the cooldown is less than 15 mins
        if (entry &&
            entry->GetRecoveryTime() <= 15 * MINUTE * IN_MILLISECONDS &&
            entry->GetCategoryRecoveryTime() <= 15 * MINUTE * IN_MILLISECONDS )
        {
            // remove & notify
            RemoveSpellCooldown(itr->first, true, sendClear);
        }
    }
}

void SpellCooldownMgr::RemoveAllSpellCooldown(ObjectGuid ownerGuid, CooldownsClearedSink const& sendCleared)
{
    if (!m_cooldowns.empty())
    {
        CooldownsClearedFact fact;
        fact.owner = ownerGuid;
        fact.spellIds.reserve(m_cooldowns.size());

        for (SpellCooldowns::const_iterator itr = m_cooldowns.begin(); itr != m_cooldowns.end(); ++itr)
        {
            fact.spellIds.push_back(itr->first);
        }

        MANGOS_ASSERT(sendCleared);
        sendCleared(fact);

        m_cooldowns.clear();
    }
}

void SpellCooldownMgr::LoadRow(Field* fields, time_t now, uint32 ownerGuidLow)
{
    uint32 spell_id = fields[0].GetUInt32();
    uint32 item_id  = fields[1].GetUInt32();
    time_t db_time  = (time_t)fields[2].GetUInt64();

    if (!sSpellStore.LookupEntry(spell_id))
    {
        sLog.outError("Player %u has unknown spell %u in `character_spell_cooldown`, skipping.", ownerGuidLow, spell_id);
        return;
    }

    // skip outdated cooldown
    if (db_time <= now)
    {
        return;
    }

    AddSpellCooldown(spell_id, item_id, db_time);

    DEBUG_LOG("Player (GUID: %u) spell %u, item %u cooldown loaded (%u secs).", ownerGuidLow, spell_id, item_id, uint32(db_time - now));
}

void SpellCooldownMgr::SaveToDB(uint32 ownerGuidLow, time_t now)
{
    static SqlStatementID deleteSpellCooldown ;
    static SqlStatementID insertSpellCooldown ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(deleteSpellCooldown, "DELETE FROM `character_spell_cooldown` WHERE `guid` = ?");
    stmt.PExecute(ownerGuidLow);

    time_t curTime = now;
    time_t infTime = curTime + infinityCooldownDelayCheck;

    // remove outdated and save active
    for (SpellCooldowns::iterator itr = m_cooldowns.begin(); itr != m_cooldowns.end();)
    {
        if (itr->second.end <= curTime)
        {
            m_cooldowns.erase(itr++);
        }
        else if (itr->second.end <= infTime)                // not save locked cooldowns, it will be reset or set at reload
        {
            stmt = CharacterDatabase.CreateStatement(insertSpellCooldown, "INSERT INTO `character_spell_cooldown` (`guid`,`spell`,`item`,`time`) VALUES( ?, ?, ?, ?)");
            stmt.PExecute(ownerGuidLow, itr->first, itr->second.itemid, uint64(itr->second.end));
            ++itr;
        }
        else
        {
            ++itr;
        }
    }
}
