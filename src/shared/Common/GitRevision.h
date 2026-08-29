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

#ifndef GITREVISION_H
#define GITREVISION_H

#include "Define.h"

#include <string>
#include <vector>

// Every version this build declares, behind functions.
//
// BuildInfo.h -- the one generated header, written from BuildInfo.h.in with the
// values in cmake/MangosVersions.cmake -- is rewritten whenever any of them
// changes, so everything that includes it is rebuilt with it. Reaching a
// version macro from a widely included header therefore turns a config-version
// bump into a rebuild of the server; behind these functions it recompiles one
// translation unit and relinks.
//
// So no HEADER includes BuildInfo.h, and nothing reaches a version through it.
// A .cpp may include it for the install paths, config file names and defaults it
// also carries -- those change when the install layout does, not when a version
// does. src/tests/CheckVersionSources.cmake enforces both halves.

namespace GitRevision
{
    // github data
    char const* GetHash();
    char const* GetDate();
    char const* GetBranch();

    // system data
    char const* GetCMakeVersion();
    char const* GetHostOSVersion();
    char const* GetRunningSystem();

    // database data
    char const* GetProjectRevision();
    char const* GetRealmDBVersion();
    char const* GetRealmDBStructure();
    char const* GetRealmDBContent();
    char const* GetRealmDBUpdateDescription();

    char const* GetCharDBVersion();
    char const* GetCharDBStructure();
    char const* GetCharDBContent();
    char const* GetCharDBUpdateDescription();

    char const* GetWorldDBVersion();
    char const* GetWorldDBStructure();
    char const* GetWorldDBContent();
    char const* GetWorldDBUpdateDescription();

    // configuration files
    //
    // What each server compares against the ConfVersion in the .conf it loaded.
    // Format YYYYMMDDRR; declared in cmake/MangosVersions.cmake.
    uint32 GetWorldConfigVersion();
    uint32 GetRealmConfigVersion();
    uint32 GetAhbotConfigVersion();

    // client
    //
    // The builds this server speaks. One entry today, and a list rather than a
    // scalar because both call sites want to name every accepted build when
    // they refuse one.
    const std::vector<uint32>& GetAcceptedClientBuilds();
    std::string GetAcceptedClientBuildsStr();
    bool IsAcceptedClientBuild(uint32 build);
    char const* GetClientVersion();

    // application data
    char const* GetFullRevision();
    char const* GetCompanyNameStr();
    char const* GetLegalCopyrightStr();
    char const* GetFileVersionStr();
    char const* GetProductVersionStr();
}

#endif
