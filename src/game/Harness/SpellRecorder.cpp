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

#include "SpellRecorder.h"
#include "Player.h"
#include "Map.h"
#include "Spell.h"
#include "SpellAuras.h"

#include <cstdio>

namespace
{
    std::string U(uint64 v)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v);
        return buf;
    }

    /// The four current-spell slots, by CurrentSpellTypes.
    char const* const kSlotNames[CURRENT_MAX_SPELL] = { "melee", "generic", "autorepeat", "channeled" };

    /// A current-spell slot as "<spell> state=<SpellState>", or "none".
    std::string SlotValue(Unit* unit, uint32 slot)
    {
        Spell* spell = unit->GetCurrentSpell(slot);
        if (!spell)
        {
            return "none";
        }
        return U(spell->m_spellInfo->ID) + " state=" + U(spell->getState());
    }
}

namespace Harness
{
    void SpellRecorder::Begin(char const* scenario, Player* player, SpellWatch const& watch)
    {
        m_watch = watch;
        Trace::Roles roles;
        roles.target = watch.target.GetRawValue();
        roles.caster = watch.caster.GetRawValue();
        Start(scenario, player, roles);
    }

    Unit* SpellRecorder::UnitIn(ObjectGuid guid) const
    {
        Player* p = Resolve();
        if (!guid || !p || !p->IsInWorld())
        {
            return NULL;
        }
        return p->GetMap()->GetUnit(guid);
    }

    void SpellRecorder::TakeUnit(char const* role, Unit* unit, State& s) const
    {
        const std::string k = std::string(role) + ".";
        if (!unit)
        {
            s.values[role] = "gone";
            return;
        }
        s.values[k + "health"] = U(unit->GetHealth()) + "/" + U(unit->GetMaxHealth());
        const Powers power = unit->GetPowerType();
        s.values[k + "power"] = U(uint32(power)) + ":" + U(unit->GetPower(power)) + "/" + U(unit->GetMaxPower(power));
        s.values[k + "combat"] = unit->IsInCombat() ? "1" : "0";
        Unit* victim = unit->getVictim();
        s.values[k + "victim"] = Trace::RoleOf(Roles(), victim ? victim->GetObjectGuid().GetRawValue() : 0);
        for (uint32 slot = 0; slot < CURRENT_MAX_SPELL; ++slot)
        {
            s.values[k + "spell." + kSlotNames[slot]] = SlotValue(unit, slot);
        }

        // Every aura holder, by spell and its place among that spell's holders: the container
        // is a multimap by spell id, so the order within one id is the order of insertion.
        Unit::SpellAuraHolderMap const& holders = unit->GetSpellAuraHolderMap();
        uint32 lastId = 0, nth = 0;
        for (Unit::SpellAuraHolderMap::const_iterator i = holders.begin(); i != holders.end(); ++i)
        {
            SpellAuraHolder const* holder = i->second;
            nth = (i->first == lastId) ? nth + 1 : 0;
            lastId = i->first;
            uint32 mask = 0;
            for (uint32 e = 0; e < MAX_EFFECT_INDEX; ++e)
            {
                if (holder->GetAuraByEffectIndex(SpellEffectIndex(e)))
                {
                    mask |= 1u << e;
                }
            }
            s.values[k + "aura." + U(i->first) + "#" + U(nth)] =
                Trace::AuraHolderValue(mask, holder->GetStackAmount(), holder->GetAuraCharges(),
                                       Trace::RoleOf(Roles(), holder->GetCasterGuid().GetRawValue()), holder->GetAuraSlot(),
                                       holder->GetAuraDuration(), holder->GetAuraMaxDuration());
        }

        // The visible slots, as the client's aura bar holds them.
        Unit::VisibleAuraMap const& visible = unit->GetVisibleAuras();
        char key[48];
        for (Unit::VisibleAuraMap::const_iterator i = visible.begin(); i != visible.end(); ++i)
        {
            snprintf(key, sizeof(key), "%sslot.%02u", k.c_str(), uint32(i->first));
            s.values[key] = i->second ? U(i->second->GetId()) : std::string("empty");
        }

        // A player's cooldowns: the stored keys, never HasSpellCooldown (which compares the end
        // with time(NULL), a clock the stepped world does not move). `auto const&`: the map's
        // type is a row type of the state-ownership gate, and nothing here needs to spell it.
        if (unit->GetTypeId() == TYPEID_PLAYER)
        {
            std::set<uint32> keys;
            auto const& cooldowns = static_cast<Player*>(unit)->GetSpellCooldownMgr().GetSpellCooldownMap();
            for (auto c = cooldowns.begin(); c != cooldowns.end(); ++c)
            {
                keys.insert(c->first);
            }
            s.sets.push_back(std::make_pair(k + "cooldowns", keys));
        }
    }

    Recorder::State SpellRecorder::Take() const
    {
        State s;
        TakeUnit("self", Resolve(), s);
        if (m_watch.target)
        {
            TakeUnit("target", UnitIn(m_watch.target), s);
        }
        if (m_watch.caster)
        {
            TakeUnit("caster", UnitIn(m_watch.caster), s);
        }
        return s;
    }

    std::string SpellRecorder::MiniUnit(char const* role, Unit* unit) const
    {
        if (!unit)
        {
            return std::string(role) + "=gone";
        }
        Spell* generic = unit->GetCurrentSpell(CURRENT_GENERIC_SPELL);
        Spell* channeled = unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
        return std::string(role) + "=h" + U(unit->GetHealth()) + " p" + U(unit->GetPower(unit->GetPowerType())) +
               " c" + (unit->IsInCombat() ? "1" : "0") +
               " s" + U(generic ? generic->m_spellInfo->ID : 0) + "/" + U(channeled ? channeled->m_spellInfo->ID : 0) +
               " a" + U(unit->GetSpellAuraHolderMap().size());
    }

    std::string SpellRecorder::Mini() const
    {
        std::string out = MiniUnit("self", Resolve());
        if (m_watch.target)
        {
            out += " " + MiniUnit("target", UnitIn(m_watch.target));
        }
        if (m_watch.caster)
        {
            out += " " + MiniUnit("caster", UnitIn(m_watch.caster));
        }
        return out;
    }
}
