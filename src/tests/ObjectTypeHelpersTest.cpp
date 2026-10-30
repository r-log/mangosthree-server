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

/// Object's checked downcasts: ToPlayer, ToCreature, ToUnit, ToGameObject, ToCorpse and
/// ToDynObject answer the object itself when its type id (its type mask, for ToUnit) names that
/// kind, and NULL otherwise. Pet.h's ToPet answers the pet a unit is, and NULL for no unit and for
/// a unit that is no pet.
///
/// Object.h sees the derived classes only as declarations, so each helper converts with
/// reinterpret_cast, which gives the derived object only while the Object sits at its start. The
/// first test pins that layout for every kind the helpers answer, a totem's included: the
/// conversion to the base and the reinterpret give the same address. The others ask a player, a
/// creature and each other kind of world object every helper, const and not, and a pet, a creature, a
/// player and no unit ToPet. The player is on a socketless session with its update fields allocated;
/// the other objects are bare.

#include "TestHarness.h"
#include "Auth/BigNumber.h"
#include "Corpse.h"
#include "Creature.h"
#include "DynamicObject.h"
#include "GameObject.h"
#include "Pet.h"
#include "Player.h"
#include "Totem.h"
#include "Unit.h"
#include "WorldSession.h"

namespace
{
    /// A player with its update fields allocated, on a session with no socket, in no world.
    class TypedPlayer : public Player
    {
        public:
            explicit TypedPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
            }

            ~TypedPlayer()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// Whether the Object of `derived` sits at its start: the conversion to the base and the
    /// reinterpret give the same address.
    template<class T>
    bool StartsWithItsObject(T* derived)
    {
        return static_cast<Object*>(derived) == reinterpret_cast<Object*>(derived);
    }
}

TEST(ObjectTypeHelpers_EveryKindStartsWithItsObject)
{
    WorldSession session(1, "typed", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    TypedPlayer player(session);
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Totem totem;
    GameObject gameObject;
    Corpse corpse;
    DynamicObject dynamicObject;

    CHECK(StartsWithItsObject<Player>(&player));
    CHECK(StartsWithItsObject<Unit>(&player));
    CHECK(StartsWithItsObject<Creature>(&creature));
    CHECK(StartsWithItsObject<Unit>(&creature));
    CHECK(StartsWithItsObject<Creature>(&totem));
    CHECK(StartsWithItsObject<GameObject>(&gameObject));
    CHECK(StartsWithItsObject<Corpse>(&corpse));
    CHECK(StartsWithItsObject<DynamicObject>(&dynamicObject));
}

TEST(ObjectTypeHelpers_APlayerIsOnlyAPlayerAndAUnit)
{
    WorldSession session(1, "typed", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    TypedPlayer player(session);
    Object* object = &player;
    Object const* constObject = &player;

    CHECK(object->ToPlayer() == &player);
    CHECK(constObject->ToPlayer() == &player);
    CHECK(object->ToUnit() == static_cast<Unit*>(&player));
    CHECK(constObject->ToUnit() == static_cast<Unit const*>(&player));
    CHECK(object->ToCreature() == NULL);
    CHECK(constObject->ToCreature() == NULL);
    CHECK(object->ToGameObject() == NULL);
    CHECK(object->ToCorpse() == NULL);
    CHECK(object->ToDynObject() == NULL);
}

TEST(ObjectTypeHelpers_ACreatureIsOnlyACreatureAndAUnit)
{
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Totem totem;
    Object* object = &creature;
    Object const* constObject = &creature;

    CHECK(object->ToCreature() == &creature);
    CHECK(constObject->ToCreature() == &creature);
    CHECK(object->ToUnit() == static_cast<Unit*>(&creature));
    CHECK(constObject->ToUnit() == static_cast<Unit const*>(&creature));
    CHECK(object->ToPlayer() == NULL);
    CHECK(constObject->ToPlayer() == NULL);
    CHECK(object->ToGameObject() == NULL);
    CHECK(object->ToCorpse() == NULL);
    CHECK(object->ToDynObject() == NULL);

    CHECK(static_cast<Object*>(&totem)->ToCreature() == static_cast<Creature*>(&totem));
}

TEST(ObjectTypeHelpers_AWorldObjectThatIsNoUnitIsOnlyItsOwnKind)
{
    GameObject gameObject;
    Corpse corpse;
    DynamicObject dynamicObject;
    Object* objects[] = { &gameObject, &corpse, &dynamicObject };

    CHECK(objects[0]->ToGameObject() == &gameObject);
    CHECK(static_cast<Object const*>(objects[0])->ToGameObject() == &gameObject);
    CHECK(objects[1]->ToCorpse() == &corpse);
    CHECK(static_cast<Object const*>(objects[1])->ToCorpse() == &corpse);
    CHECK(objects[2]->ToDynObject() == &dynamicObject);
    CHECK(static_cast<Object const*>(objects[2])->ToDynObject() == &dynamicObject);
    for (Object* object : objects)
    {
        CHECK(object->ToPlayer() == NULL);
        CHECK(object->ToCreature() == NULL);
        CHECK(object->ToUnit() == NULL);
    }
    CHECK(objects[0]->ToCorpse() == NULL && objects[0]->ToDynObject() == NULL);
    CHECK(objects[1]->ToGameObject() == NULL && objects[1]->ToDynObject() == NULL);
    CHECK(objects[2]->ToGameObject() == NULL && objects[2]->ToCorpse() == NULL);
}

TEST(ObjectTypeHelpers_ToPetAnswersOnlyAPet)
{
    WorldSession session(1, "typed", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    TypedPlayer player(session);
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Pet pet(HUNTER_PET);

    CHECK(ToPet(static_cast<Unit*>(&pet)) == &pet);
    CHECK(ToPet(static_cast<Unit const*>(&pet)) == &pet);
    CHECK(ToPet(static_cast<Unit*>(&creature)) == NULL);
    CHECK(ToPet(static_cast<Unit const*>(&creature)) == NULL);
    CHECK(ToPet(static_cast<Unit*>(&player)) == NULL);
    CHECK(ToPet(static_cast<Unit*>(NULL)) == NULL);
    CHECK(ToPet(static_cast<Unit const*>(NULL)) == NULL);
}
