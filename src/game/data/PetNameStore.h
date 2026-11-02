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

#ifndef MANGOS_H_PETNAMESTORE
#define MANGOS_H_PETNAMESTORE

#include "Platform/Define.h"

#include <map>
#include <string>
#include <vector>

/**
 * The pet name parts (`pet_name_generation`): per creature entry, the first halves and the second
 * halves a random pet name is made of, each in the order added. An entry with none answers an empty
 * list.
 */
class PetNameStore
{
    public:
        void AddFirstHalf(uint32 entry, std::string const& word) { m_firstHalves[entry].push_back(word); }
        void AddSecondHalf(uint32 entry, std::string const& word) { m_secondHalves[entry].push_back(word); }
        std::vector<std::string> const& FirstHalves(uint32 entry) const { return Find(m_firstHalves, entry); }
        std::vector<std::string> const& SecondHalves(uint32 entry) const { return Find(m_secondHalves, entry); }

    private:
        typedef std::map<uint32, std::vector<std::string> > HalfNameMap;

        static std::vector<std::string> const& Find(HalfNameMap const& halves, uint32 entry);

        HalfNameMap m_firstHalves;
        HalfNameMap m_secondHalves;
};

#endif
