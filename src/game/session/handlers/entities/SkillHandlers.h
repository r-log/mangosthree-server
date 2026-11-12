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

#ifndef MANGOS_H_SKILLHANDLERS
#define MANGOS_H_SKILLHANDLERS

#include "Platform/Define.h"

class WorldPacket;
class WorldSession;

/// The profession specialization spell the client's unlearn index names (0 Gnomish Engineer,
/// 1 Goblin Engineer, 2 Elixir Master, 3 Potion Master, 4 Transmutation Master), or 0 for any
/// other index.
uint32 SpecializationSpellOfIndex(uint32 index);

/// The client's talent and skill opcodes: learning one talent rank, learning the talents chosen in
/// the talent preview as one group, confirming a talent wipe at a class trainer, unlearning a
/// skill and unlearning a profession specialization: static entry points the opcode table binds,
/// each borrowing the session for one call and storing nothing.
struct SkillHandlers
{
    public:
        static void HandleLearnTalent(WorldSession& session, WorldPacket& recv_data);
        static void HandleLearnPreviewTalents(WorldSession& session, WorldPacket& recv_data);
        static void HandleTalentWipeConfirm(WorldSession& session, WorldPacket& recv_data);
        static void HandleUnlearnSkill(WorldSession& session, WorldPacket& recv_data);
        static void HandleUnlearnSpecialization(WorldSession& session, WorldPacket& recv_data);
};

#endif
