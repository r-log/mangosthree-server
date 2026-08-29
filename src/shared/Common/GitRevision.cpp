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

#include "GitRevision.h"
#include "BuildInfo.h"
#include "BuildInfo.h"

#include <sstream>

char const* GitRevision::GetHash()
{
    return REVISION_HASH;
}

char const* GitRevision::GetDate()
{
    return REVISION_DATE;
}

char const* GitRevision::GetBranch()
{
    return REVISION_BRANCH;
}


char const* GitRevision::GetCMakeVersion()
{
    return CMAKE_VERSION;
}

char const* GitRevision::GetHostOSVersion()
{
    return "Compiled on: " CMAKE_HOST_SYSTEM;
}

// Platform Define
#if PLATFORM == PLATFORM_WINDOWS
    #ifdef _WIN64
        #define MANGOS_PLATFORM_STR "Win64"
    #else
        #define MANGOS_PLATFORM_STR "Win32"
    #endif
#elif PLATFORM == PLATFORM_APPLE
    #define MANGOS_PLATFORM_STR "MacOSX"
#elif PLATFORM == PLATFORM_INTEL
    #define MANGOS_PLATFORM_STR "Intel"
#elif PLATFORM == PLATFORM_UNIX
    #define MANGOS_PLATFORM_STR "Linux"
#else
    #define MANGOS_PLATFORM_STR "Unknown System"
#endif

// Database Revision
char const* GitRevision::GetProjectRevision()
{
    return PROJECT_REVISION_NR;
}

char const* GitRevision::GetRealmDBVersion()
{
    return REALMD_DB_VERSION_NR;
}

char const* GitRevision::GetRealmDBStructure()
{
    return REALMD_DB_STRUCTURE_NR;
}

char const* GitRevision::GetRealmDBContent()
{
    return REALMD_DB_CONTENT_NR;
}

char const* GitRevision::GetRealmDBUpdateDescription()
{
    return REALMD_DB_UPDATE_DESCRIPT;
}

char const* GitRevision::GetCharDBVersion()
{
    return CHAR_DB_VERSION_NR;
}

char const* GitRevision::GetCharDBStructure()
{
    return CHAR_DB_STRUCTURE_NR;
}

char const* GitRevision::GetCharDBContent()
{
    return CHAR_DB_CONTENT_NR;
}

char const* GitRevision::GetCharDBUpdateDescription()
{
    return CHAR_DB_UPDATE_DESCRIPT;
}

char const* GitRevision::GetWorldDBVersion()
{
    return WORLD_DB_VERSION_NR;
}

char const* GitRevision::GetWorldDBStructure()
{
    return WORLD_DB_STRUCTURE_NR;
}

char const* GitRevision::GetWorldDBContent()
{
    return WORLD_DB_CONTENT_NR;
}

char const* GitRevision::GetWorldDBUpdateDescription()
{
    return WORLD_DB_UPDATE_DESCRIPT;
}

char const* GitRevision::GetFullRevision()
{
    return "Mangos revision: " VER_PRODUCTVERSION_STR;
}

char const* GitRevision::GetRunningSystem()
{
    return "Running on: " CMAKE_HOST_SYSTEM;
}

char const* GitRevision::GetCompanyNameStr()
{
    return VER_COMPANY_NAME_STR;
}

char const* GitRevision::GetLegalCopyrightStr()
{
    return VER_LEGALCOPYRIGHT_STR;
}

char const* GitRevision::GetFileVersionStr()
{
    return VER_FILEVERSION_STR;
}

char const* GitRevision::GetProductVersionStr()
{
    return VER_PRODUCTVERSION_STR;
}

// --- Configuration files ---------------------------------------------------

uint32 GitRevision::GetWorldConfigVersion()
{
    return MANGOSD_CONFIG_VERSION;
}

uint32 GitRevision::GetRealmConfigVersion()
{
    return REALMD_CONFIG_VERSION;
}

uint32 GitRevision::GetAhbotConfigVersion()
{
    return AHBOT_CONFIG_VERSION;
}

// --- Client ----------------------------------------------------------------

const std::vector<uint32>& GitRevision::GetAcceptedClientBuilds()
{
    // The generated macro is a brace-init list terminated by a zero, which is
    // how the two call sites used to walk it. Unpack it once, here, so that
    // neither of them has to know the terminator exists.
    static const std::vector<uint32> builds = []
    {
        const int declared[] = EXPECTED_MANGOSD_CLIENT_BUILD;
        std::vector<uint32> out;
        for (size_t i = 0; declared[i]; ++i)
        {
            out.push_back(uint32(declared[i]));
        }
        return out;
    }();

    return builds;
}

std::string GitRevision::GetAcceptedClientBuildsStr()
{
    std::ostringstream text;
    for (uint32 build : GetAcceptedClientBuilds())
    {
        text << build << " ";
    }
    return text.str();
}

bool GitRevision::IsAcceptedClientBuild(uint32 build)
{
    for (uint32 accepted : GetAcceptedClientBuilds())
    {
        if (accepted == build)
        {
            return true;
        }
    }
    return false;
}

char const* GitRevision::GetClientVersion()
{
    return EXPECTED_MANGOSD_CLIENT_VERSION;
}
