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

#include "data/TerrainFacts.h"

#include <atomic>
#include <cstddef>

namespace
{
    std::atomic<TerrainFacts*> s_installed(NULL);

    TerrainFacts& BaseFacts()
    {
        static TerrainFacts facts;
        return facts;
    }
}

TerrainFacts::~TerrainFacts()
{
}

uint32 TerrainFacts::ZoneId(uint32 /*mapId*/, float /*x*/, float /*y*/, float /*z*/) const
{
    return 0;
}

std::optional<float> TerrainFacts::StaticFloor(uint32 /*mapId*/, float /*x*/, float /*y*/, float /*z*/) const
{
    return std::nullopt;
}

TerrainFacts& TerrainFacts::Installed()
{
    TerrainFacts* facts = s_installed.load(std::memory_order_acquire);
    return facts ? *facts : BaseFacts();
}

TerrainFacts* TerrainFacts::Install(TerrainFacts* facts)
{
    return s_installed.exchange(facts, std::memory_order_acq_rel);
}

bool TerrainFacts::IsInstalled()
{
    return s_installed.load(std::memory_order_acquire) != NULL;
}
