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

#ifndef MANGOS_H_KEYEDRECORDSTORE
#define MANGOS_H_KEYEDRECORDSTORE

#include "Platform/Define.h"

#include <map>

/**
 * Records held one to a key, for a record type the store need not know (it may carry behaviour
 * that reaches outside the data layer). Setting a key that already holds a record overwrites it,
 * a key with no record answers NULL, erasing a key forgets it, and clearing forgets every key.
 */
template<class Record>
class KeyedRecordStore
{
    public:
        void Clear()
        {
            m_records.clear();
        }

        void Set(uint32 key, Record const& record)
        {
            m_records[key] = record;
        }

        Record const* Find(uint32 key) const
        {
            typename RecordMap::const_iterator itr = m_records.find(key);
            if (itr != m_records.end())
            {
                return &itr->second;
            }
            return NULL;
        }

        /// The record of `key` for a loader's checks to change, or NULL.
        Record* FindForEdit(uint32 key)
        {
            typename RecordMap::iterator itr = m_records.find(key);
            if (itr != m_records.end())
            {
                return &itr->second;
            }
            return NULL;
        }

        void Erase(uint32 key)
        {
            m_records.erase(key);
        }

        uint32 GetCount() const
        {
            return uint32(m_records.size());
        }

        /// One past the highest key held, 0 when the store is empty.
        uint32 GetMaxEntry() const
        {
            if (m_records.empty())
            {
                return 0;
            }
            return m_records.rbegin()->first + 1;
        }

    private:
        typedef std::map<uint32, Record> RecordMap;

        RecordMap m_records;
};

#endif
