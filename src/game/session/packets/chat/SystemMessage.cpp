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

#include "session/packets/chat/SystemMessage.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "Utilities/Util.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "session/packets/chat/MessageChatPacket.h"

void SendSystemMessage(WorldSession& session, char const* text)
{
    WorldPacket data;

    // strtok writes into the text, so it splits a copy
    char* buf = mangos_strdup(text);
    char* pos = buf;

    while (char* line = strtok(pos, "\n"))
    {
        pos = NULL;
        BuildMessageChatPacket(data, CHAT_MSG_SYSTEM, line, LANG_UNIVERSAL, CHAT_TAG_NONE, session.GetPlayer()->GetObjectGuid());
        session.SendPacket(&data);
    }

    delete[] buf;
}

void SendSystemMessage(WorldSession& session, int32 entry)
{
    SendSystemMessage(session, session.GetMangosString(entry));
}

void SendSystemMessageFormat(WorldSession& session, int32 entry, ...)
{
    const char* format = session.GetMangosString(entry);
    va_list ap;
    char str [2048];
    va_start(ap, entry);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendSystemMessage(session, str);
}
