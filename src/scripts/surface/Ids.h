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

#ifndef MANGOS_H_SCRIPT_IDS
#define MANGOS_H_SCRIPT_IDS

#include "Platform/Define.h"

#include <cstddef>
#include <functional>

namespace Script
{
    /**
     * A strong id: an integer of one kind (a spell, a text, a creature entry) that converts from its
     * integer only explicitly, never to another kind of id, and does no arithmetic. The default id
     * is 0.
     */
    template <class Tag, class Int>
    class Id
    {
        public:
            constexpr Id() : m_value(0) { }
            constexpr explicit Id(Int value) : m_value(value) { }

            constexpr Int Value() const { return m_value; }

            friend constexpr bool operator==(Id a, Id b) { return a.m_value == b.m_value; }
            friend constexpr bool operator!=(Id a, Id b) { return a.m_value != b.m_value; }
            friend constexpr bool operator<(Id a, Id b) { return a.m_value < b.m_value; }

        private:
            Int m_value;
    };

    struct SpellTag;
    struct TextTag;
    struct CreatureTag;
    struct ObjectTag;
    struct QuestTag;

    typedef Id<SpellTag, uint32> SpellId;
    /// A script text; script texts have negative ids.
    typedef Id<TextTag, int32> TextId;
    typedef Id<CreatureTag, uint32> CreatureEntry;
    typedef Id<ObjectTag, uint32> ObjectEntry;
    typedef Id<QuestTag, uint32> QuestId;

    /// What a handle names.
    enum class Kind : uint8
    {
        None,
        Creature,
        Pet,
        Vehicle,
        Player,
        GameObject,
        Transport,
        Item,
        DynamicObject,
        Corpse
    };

    /**
     * A handle to an object in the world: the object's guid, held as its value and kind. A script
     * keeps handles across ticks and looks the object up through them; the empty handle names
     * nothing.
     */
    class Handle
    {
        public:
            constexpr Handle() : m_value(0), m_kind(Kind::None) { }
            constexpr Handle(uint64 value, Kind kind) : m_value(value), m_kind(kind) { }

            constexpr uint64 Value() const { return m_value; }
            constexpr Kind GetKind() const { return m_kind; }

            /// Whether the handle names an object.
            constexpr explicit operator bool() const { return m_value != 0; }

            friend constexpr bool operator==(Handle a, Handle b) { return a.m_value == b.m_value; }
            friend constexpr bool operator!=(Handle a, Handle b) { return a.m_value != b.m_value; }
            friend constexpr bool operator<(Handle a, Handle b) { return a.m_value < b.m_value; }

        private:
            uint64 m_value;
            Kind m_kind;
    };
}

namespace std
{
    template <class Tag, class Int>
    struct hash<Script::Id<Tag, Int>>
    {
        std::size_t operator()(Script::Id<Tag, Int> id) const { return std::hash<Int>()(id.Value()); }
    };

    template <>
    struct hash<Script::Handle>
    {
        std::size_t operator()(Script::Handle handle) const { return std::hash<uint64>()(handle.Value()); }
    };
}

#endif
