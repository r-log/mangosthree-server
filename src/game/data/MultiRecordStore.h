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

#ifndef MANGOS_H_MULTIRECORDSTORE
#define MANGOS_H_MULTIRECORDSTORE

#include "Platform/Define.h"

#include <map>

/**
 * Records held several to a key, each key's records in the order they were added. A key's records
 * come back as a pair of iterators that read a record with `->`, as the world template loader's
 * multi storage handed them out. Erasing a key forgets every one of its records, and clearing
 * forgets every key.
 */
template<class Record>
class MultiRecordStore
{
    public:
        typedef std::multimap<uint32, Record> RecordMap;

        /// Walks one key's records; `->` and `*` read the record.
        class const_iterator
        {
            public:
                explicit const_iterator(typename RecordMap::const_iterator itr) : m_itr(itr) {}

                Record const& operator*() const { return m_itr->second; }
                Record const* operator->() const { return &m_itr->second; }
                const_iterator& operator++()
                {
                    ++m_itr;
                    return *this;
                }
                bool operator==(const_iterator const& other) const { return m_itr == other.m_itr; }
                bool operator!=(const_iterator const& other) const { return m_itr != other.m_itr; }

            private:
                typename RecordMap::const_iterator m_itr;
        };

        /// One key's records, from `first` to just before `second`.
        struct Bounds
        {
            const_iterator first;
            const_iterator second;
        };

        void Clear()
        {
            m_records.clear();
        }

        void Add(uint32 key, Record const& record)
        {
            m_records.insert(typename RecordMap::value_type(key, record));
        }

        /// The records of `key`, in the order they were added.
        Bounds GetBounds(uint32 key) const
        {
            std::pair<typename RecordMap::const_iterator, typename RecordMap::const_iterator> range = m_records.equal_range(key);
            Bounds bounds = { const_iterator(range.first), const_iterator(range.second) };
            return bounds;
        }

        void Erase(uint32 key)
        {
            m_records.erase(key);
        }

        uint32 GetCount() const
        {
            return uint32(m_records.size());
        }

    private:
        RecordMap m_records;
};

#endif
