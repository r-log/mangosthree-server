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

/// The script surface's ids and handles: an id holds the integer it was made from, compares and hashes
/// by it, keeps a script text's sign, and converts from its integer only explicitly and never to an id
/// of another kind; a handle holds a guid's value and kind, names something only when the guid does,
/// and turns back into the guid it came from.

#include "TestHarness.h"
#include "surface/Ids.h"
#include "surface/detail/Guids.h"
#include "ObjectGuid.h"

#include <type_traits>
#include <unordered_set>

static_assert(!std::is_convertible<Script::SpellId, Script::QuestId>::value, "ids of two kinds do not convert");
static_assert(!std::is_convertible<uint32, Script::SpellId>::value, "an integer becomes an id only explicitly");
static_assert(!std::is_convertible<Script::SpellId, uint32>::value, "an id becomes its integer only through Value");
static_assert(std::is_constructible<Script::SpellId, uint32>::value, "an id is made from its integer");

TEST(ScriptIds_AnIdHoldsComparesAndHashesByItsInteger)
{
    const Script::SpellId fireball(133);
    CHECK_EQ(fireball.Value(), 133u);
    CHECK_EQ(Script::SpellId().Value(), 0u);
    CHECK(fireball == Script::SpellId(133));
    CHECK(fireball != Script::SpellId(116));
    CHECK(Script::SpellId(116) < fireball);

    std::unordered_set<Script::SpellId> spells;
    spells.insert(fireball);
    spells.insert(Script::SpellId(133));
    spells.insert(Script::SpellId(116));
    CHECK_EQ(spells.size(), size_t(2));
    CHECK(spells.count(Script::SpellId(116)) == 1);
}

TEST(ScriptIds_AScriptTextKeepsItsNegativeId)
{
    const Script::TextId say(-1000001);
    CHECK_EQ(say.Value(), int32(-1000001));
    CHECK(say < Script::TextId(-1000000));
}

TEST(ScriptIds_AHandleNamesWhatItsGuidNamesAndTurnsBackIntoIt)
{
    const ObjectGuid creature(HIGHGUID_UNIT, uint32(3200), uint32(41));
    const ObjectGuid player(HIGHGUID_PLAYER, uint32(7));
    const ObjectGuid empty;

    const Script::Handle creatureHandle = Script::Detail::ToHandle(creature);
    const Script::Handle playerHandle = Script::Detail::ToHandle(player);
    const Script::Handle emptyHandle = Script::Detail::ToHandle(empty);

    CHECK(creatureHandle.GetKind() == Script::Kind::Creature);
    CHECK(playerHandle.GetKind() == Script::Kind::Player);
    CHECK(emptyHandle.GetKind() == Script::Kind::None);
    CHECK(bool(creatureHandle));
    CHECK(bool(playerHandle));
    CHECK(!bool(emptyHandle));
    CHECK(!bool(Script::Handle()));
    CHECK(creatureHandle != playerHandle);

    CHECK(Script::Detail::ToGuid(creatureHandle) == creature);
    CHECK(Script::Detail::ToGuid(playerHandle) == player);
    CHECK(Script::Detail::ToGuid(emptyHandle).IsEmpty());

    std::unordered_set<Script::Handle> handles;
    handles.insert(creatureHandle);
    handles.insert(Script::Detail::ToHandle(creature));
    CHECK_EQ(handles.size(), size_t(1));
}

TEST(ScriptIds_EachObjectKindFollowsItsHighGuid)
{
    CHECK(Script::Detail::KindOf(ObjectGuid(HIGHGUID_PET, uint32(1), uint32(2))) == Script::Kind::Pet);
    CHECK(Script::Detail::KindOf(ObjectGuid(HIGHGUID_VEHICLE, uint32(1), uint32(2))) == Script::Kind::Vehicle);
    CHECK(Script::Detail::KindOf(ObjectGuid(HIGHGUID_GAMEOBJECT, uint32(1), uint32(2))) == Script::Kind::GameObject);
    CHECK(Script::Detail::KindOf(ObjectGuid(HIGHGUID_TRANSPORT, uint32(1), uint32(2))) == Script::Kind::Transport);
    CHECK(Script::Detail::KindOf(ObjectGuid(HIGHGUID_MO_TRANSPORT, uint32(2))) == Script::Kind::Transport);
    CHECK(Script::Detail::KindOf(ObjectGuid(HIGHGUID_ITEM, uint32(2))) == Script::Kind::Item);
    CHECK(Script::Detail::KindOf(ObjectGuid(HIGHGUID_DYNAMICOBJECT, uint32(1), uint32(2))) == Script::Kind::DynamicObject);
    CHECK(Script::Detail::KindOf(ObjectGuid(HIGHGUID_CORPSE, uint32(2))) == Script::Kind::Corpse);
    CHECK(Script::Detail::KindOf(ObjectGuid(HIGHGUID_GROUP, uint32(2))) == Script::Kind::None);
    CHECK(Script::Detail::KindOf(ObjectGuid(HIGHGUID_GUILD, uint32(2))) == Script::Kind::None);
}
