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

#include "data/GameConfig.h"

GameConfig sGameConfig;

float GameConfig::m_MaxVisibleDistanceOnContinents = DEFAULT_VISIBILITY_DISTANCE;
float GameConfig::m_MaxVisibleDistanceInInstances  = DEFAULT_VISIBILITY_INSTANCE;
float GameConfig::m_MaxVisibleDistanceInBGArenas   = DEFAULT_VISIBILITY_BGARENAS;

float GameConfig::m_MaxVisibleDistanceInFlight     = DEFAULT_VISIBILITY_DISTANCE;
float GameConfig::m_VisibleUnitGreyDistance        = 0;
float GameConfig::m_VisibleObjectGreyDistance      = 0;

float  GameConfig::m_relocation_lower_limit_sq     = 10.f * 10.f;
uint32 GameConfig::m_relocation_ai_notify_delay    = 1000u;

bool   GameConfig::m_visibility_observer_sweep_enabled  = true;
uint32 GameConfig::m_visibility_observer_sweep_interval = 2000u;
