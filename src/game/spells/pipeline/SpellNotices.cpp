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

#include "spells/pipeline/SpellNotices.h"

#include <atomic>

namespace
{
    std::atomic<SpellNotices*> s_spellNoticesInstalled(NULL);

    SpellNotices& BaseSpellNotices()
    {
        static SpellNotices notices;
        return notices;
    }
}

SpellNotices::~SpellNotices()
{
}

void SpellNotices::CastResult(Player& /*caster*/, SpellEntry const* /*spellInfo*/, uint8 /*castCount*/, SpellCastResult /*result*/, bool /*isPetCastResult*/)
{
}

void SpellNotices::SpellStart(Spell const& /*spell*/, uint32 /*castFlags*/, SpellHealPrediction const& /*prediction*/)
{
}

void SpellNotices::SpellGo(Spell const& /*spell*/, uint32 /*castFlags*/, SpellGoTargets const& /*targets*/)
{
}

void SpellNotices::LogExecute(Spell const& /*spell*/)
{
}

void SpellNotices::Interrupted(Spell const& /*spell*/, uint8 /*result*/)
{
}

void SpellNotices::ChannelUpdate(Spell const& /*spell*/, uint32 /*time*/)
{
}

void SpellNotices::ChannelStart(Spell const& /*spell*/, uint32 /*duration*/)
{
}

void SpellNotices::ResurrectRequest(Spell const& /*spell*/, Player& /*target*/)
{
}

SpellNotices& SpellNotices::Installed()
{
    SpellNotices* notices = s_spellNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseSpellNotices();
}

SpellNotices* SpellNotices::Install(SpellNotices* notices)
{
    return s_spellNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
