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

#include "spells/pipeline/SpellEffectNotices.h"

#include <atomic>

namespace
{
    std::atomic<SpellEffectNotices*> s_spellEffectNoticesInstalled(NULL);

    SpellEffectNotices& BaseSpellEffectNotices()
    {
        static SpellEffectNotices notices;
        return notices;
    }
}

SpellEffectNotices::~SpellEffectNotices()
{
}

void SpellEffectNotices::InstaKillLog(Spell const& /*spell*/, ObjectGuid /*casterGuid*/, Unit const& /*victim*/)
{
}

void SpellEffectNotices::DispelLog(Spell const& /*spell*/, Unit const& /*victim*/, std::vector<uint32> const& /*spellIds*/)
{
}

void SpellEffectNotices::DispelFailed(Spell const& /*spell*/, Unit const& /*victim*/, std::vector<uint32> const& /*spellIds*/)
{
}

void SpellEffectNotices::StealLog(Spell const& /*spell*/, Unit const& /*victim*/, std::vector<uint32> const& /*spellIds*/)
{
}

void SpellEffectNotices::ClearTarget(Unit const& /*target*/)
{
}

void SpellEffectNotices::DuelRequested(Player& /*challenger*/, Player& /*challenged*/, ObjectGuid /*flag*/)
{
}

void SpellEffectNotices::TotemCreated(Player& /*owner*/, uint8 /*slot*/, ObjectGuid /*totem*/, uint32 /*duration*/, uint32 /*spellId*/)
{
}

void SpellEffectNotices::TaxiNodeLearned(Player& /*player*/, ObjectGuid /*source*/)
{
}

void SpellEffectNotices::HomeBound(Player& /*player*/, ObjectGuid /*binder*/, WorldLocation const& /*home*/, uint32 /*areaId*/)
{
}

SpellEffectNotices& SpellEffectNotices::Installed()
{
    SpellEffectNotices* notices = s_spellEffectNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseSpellEffectNotices();
}

SpellEffectNotices* SpellEffectNotices::Install(SpellEffectNotices* notices)
{
    return s_spellEffectNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
