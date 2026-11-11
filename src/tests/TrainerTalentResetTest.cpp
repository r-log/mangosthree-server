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

/// A class trainer resets the talents of a player of its own class from level 10; a trainer of
/// another class, or one that is not a class trainer, never does.
#include "TestHarness.h"
#include "Creature.h"

namespace
{
    CreatureInfo Trainer(uint32 trainerType, uint32 trainerClass)
    {
        CreatureInfo info = {};
        info.TrainerType = trainerType;
        info.TrainerClass = trainerClass;
        return info;
    }
}

TEST(TrainerTalentReset_ClassTrainerResetsItsOwnClassFromLevel10)
{
    CreatureInfo const warriorTrainer = Trainer(TRAINER_TYPE_CLASS, CLASS_WARRIOR);

    CHECK(CanResetTalents(warriorTrainer, CLASS_WARRIOR, 10));
    CHECK(CanResetTalents(warriorTrainer, CLASS_WARRIOR, 85));
}

TEST(TrainerTalentReset_BelowLevel10IsRefused)
{
    CreatureInfo const warriorTrainer = Trainer(TRAINER_TYPE_CLASS, CLASS_WARRIOR);

    CHECK(!CanResetTalents(warriorTrainer, CLASS_WARRIOR, 9));
    CHECK(!CanResetTalents(warriorTrainer, CLASS_WARRIOR, 1));
}

TEST(TrainerTalentReset_AnotherClassIsRefused)
{
    CreatureInfo const warriorTrainer = Trainer(TRAINER_TYPE_CLASS, CLASS_WARRIOR);

    CHECK(!CanResetTalents(warriorTrainer, CLASS_MAGE, 10));
    CHECK(!CanResetTalents(warriorTrainer, CLASS_PALADIN, 85));
}

TEST(TrainerTalentReset_OnlyAClassTrainerResets)
{
    CHECK(!CanResetTalents(Trainer(TRAINER_TYPE_PETS, CLASS_HUNTER), CLASS_HUNTER, 85));
    CHECK(!CanResetTalents(Trainer(TRAINER_TYPE_TRADESKILLS, 0), CLASS_WARRIOR, 85));
    CHECK(!CanResetTalents(Trainer(TRAINER_TYPE_MOUNTS, 0), CLASS_WARRIOR, 85));
}
