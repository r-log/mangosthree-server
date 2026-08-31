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

// mangos-loadtest -- drives synthetic 4.3.4 sessions against a running mangosd.
//
// Phase 1 is one client that logs in and holds the session. That is the piece
// that de-risks the rest: it proves this side of the protocol against the real
// server, and everything a larger harness would do is that sequence repeated.
//
// The session key is not negotiated. Rather than reimplement SRP-6 and a second
// server's protocol, the harness derives a key from the account name and the
// operator writes that same value into `account.sessionkey` -- `--emit-sql`
// prints the statement. A derived key means no state travels between the two
// steps: run the SQL once, then run any number of logins, in any order, from any
// machine, with nothing to keep in sync.
//
// Nothing here weakens a check. The world server still verifies
// SHA-1(account || 0 || client seed || server seed || K) on the first stream and
// SHA-1(account || K || server seed) on the second, against the row it reads
// from its own database. It cannot tell this client from a real one, which is
// the point.

#include "SyntheticClient.hpp"

#include "Auth/Sha1.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    void Usage()
    {
        std::printf(
            "mangos-loadtest -- synthetic 4.3.4 client\n"
            "\n"
            "  --account NAME       account to log in as (upper case, as realmd stores it)\n"
            "  --character GUID     `characters`.`guid` of a character on that account\n"
            "  --host ADDRESS       world server address        (default 127.0.0.1)\n"
            "  --port N             first world stream          (default 8085)\n"
            "  --stream-port N      second world stream         (default 8086)\n"
            "  --build N            client build to claim       (default 15595)\n"
            "  --hold SECONDS       stay in world afterwards    (default 0)\n"
            "  --key HEX            session key, 80 hex digits; overrides the derived one\n"
            "  --emit-sql           print the SQL that plants the derived key, and exit\n"
            "  --verbose            trace each milestone as it is reached\n"
            "\n"
            "Typical use:\n"
            "  mangos-loadtest --account LOAD01 --emit-sql | mysql -u root realmd\n"
            "  mangos-loadtest --account LOAD01 --character 9 --hold 60 --verbose\n");
    }

    /**
     * @brief A 40-byte session key derived from the account name alone.
     *
     * Deterministic on purpose: the value must be identical in the SQL that
     * plants it and in the login that uses it, and deriving it means those two
     * runs need share nothing -- no file, no argument, no ordering.
     *
     * It is not a secret and is not meant to be one. These are throwaway load-test
     * accounts on a server the operator already controls; anything stronger would
     * add a key-distribution problem to a harness in order to protect accounts
     * that exist to be logged into by a program.
     */
    std::string DeriveSessionKeyHex(const std::string& account)
    {
        std::string hex;
        hex.reserve(80);

        // Two SHA-1s give the 40 bytes realmd's own key is; the counter keeps the
        // halves from being the same digest.
        for (uint32 block = 0; block < 2; ++block)
        {
            Sha1Hash sha;
            sha.UpdateData(std::string("mangos-loadtest/v1"));
            sha.UpdateData(account);
            sha.UpdateData(reinterpret_cast<const uint8*>(&block), sizeof(block));
            sha.Finalize();

            char byteText[3];
            for (size_t i = 0; i < 20; ++i)
            {
                std::snprintf(byteText, sizeof(byteText), "%.2X", sha.GetDigest()[i]);
                hex += byteText;
            }
        }
        return hex;
    }

    bool WantsValue(int argc, int index, const char* flag)
    {
        if (index + 1 < argc)
        {
            return true;
        }
        std::fprintf(stderr, "%s needs a value\n", flag);
        return false;
    }
}

int main(int argc, char** argv)
{
    loadtest::Config config;
    std::string keyHex;
    bool emitSql = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            Usage();
            return 0;
        }
        else if (arg == "--emit-sql")   { emitSql = true; }
        else if (arg == "--verbose")    { config.verbose = true; }
        else if (arg == "--account")
        {
            if (!WantsValue(argc, i, "--account")) { return 2; }
            config.account = argv[++i];
        }
        else if (arg == "--character")
        {
            if (!WantsValue(argc, i, "--character")) { return 2; }
            config.characterGuid = std::strtoull(argv[++i], NULL, 10);
        }
        else if (arg == "--host")
        {
            if (!WantsValue(argc, i, "--host")) { return 2; }
            config.host = argv[++i];
        }
        else if (arg == "--port")
        {
            if (!WantsValue(argc, i, "--port")) { return 2; }
            config.port = uint16(std::strtoul(argv[++i], NULL, 10));
        }
        else if (arg == "--stream-port")
        {
            if (!WantsValue(argc, i, "--stream-port")) { return 2; }
            config.streamPort = uint16(std::strtoul(argv[++i], NULL, 10));
        }
        else if (arg == "--build")
        {
            if (!WantsValue(argc, i, "--build")) { return 2; }
            config.build = uint16(std::strtoul(argv[++i], NULL, 10));
        }
        else if (arg == "--hold")
        {
            if (!WantsValue(argc, i, "--hold")) { return 2; }
            config.holdSeconds = uint32(std::strtoul(argv[++i], NULL, 10));
        }
        else if (arg == "--key")
        {
            if (!WantsValue(argc, i, "--key")) { return 2; }
            keyHex = argv[++i];
        }
        else
        {
            std::fprintf(stderr, "unknown option: %s\n\n", arg.c_str());
            Usage();
            return 2;
        }
    }

    if (config.account.empty())
    {
        std::fprintf(stderr, "--account is required\n\n");
        Usage();
        return 2;
    }

    // The account name is hashed into both proofs verbatim, and realmd stores it
    // upper case -- so a lower-case argument fails as a wrong password would,
    // with nothing to say why. Normalise it here instead.
    for (size_t i = 0; i < config.account.size(); ++i)
    {
        config.account[i] = char(std::toupper(static_cast<unsigned char>(config.account[i])));
    }

    if (keyHex.empty())
    {
        keyHex = DeriveSessionKeyHex(config.account);
    }
    if (keyHex.size() != 80)
    {
        std::fprintf(stderr, "a session key is 40 bytes, so 80 hex digits (got %u)\n",
                     uint32(keyHex.size()));
        return 2;
    }

    if (emitSql)
    {
        // `sessionkey` is what the world server reads to verify the login proof,
        // and it is the only column that has to change: the account row itself is
        // the operator's to create, with whatever password realmd wants.
        std::printf("UPDATE `account` SET `sessionkey` = '%s' WHERE `username` = '%s';\n",
                    keyHex.c_str(), config.account.c_str());
        std::printf("-- then, against the character database, to find a guid to log in:\n");
        std::printf("--   SELECT `guid`, `name` FROM `characters` WHERE `account` = "
                    "(SELECT `id` FROM `realmd`.`account` WHERE `username` = '%s');\n",
                    config.account.c_str());
        return 0;
    }

    config.sessionKey.SetHexStr(keyHex.c_str());

    std::string error;
    if (!loadtest::InitSockets(error))
    {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }

    std::printf("logging in as '%s' to %s:%u / :%u\n",
                config.account.c_str(), config.host.c_str(),
                uint32(config.port), uint32(config.streamPort));

    loadtest::SyntheticClient client(config);
    const loadtest::Result result = client.Run();

    loadtest::ShutdownSockets();

    // Reaching the world is not the whole of it: a session dropped during the
    // hold has an error and the world stage both, and reporting that as OK would
    // hide exactly the failure a long hold exists to catch.
    const bool ok = result.Reached(loadtest::Stage::InWorld) && result.error.empty();

    std::printf("\n%s: %s\n", ok ? "OK" : "FAILED", loadtest::StageName(result.stage));
    if (!result.error.empty())
    {
        std::printf("  %s\n", result.error.c_str());
    }

    if (result.msToAuth > 0.0)
    {
        std::printf("  authenticated   %8.1f ms\n", result.msToAuth);
    }
    if (result.msToRedirect > 0.0)
    {
        std::printf("  redirected      %8.1f ms\n", result.msToRedirect);
    }
    if (result.msToStream1 > 0.0)
    {
        std::printf("  stream 1 live   %8.1f ms\n", result.msToStream1);
    }
    if (result.msToWorld > 0.0)
    {
        std::printf("  in world        %8.1f ms\n", result.msToWorld);
    }

    // The split this harness exists to measure. Today only 19 opcodes are gated
    // onto the second stream, so a login is expected to be lopsided; printing it
    // per run is what makes a change in the routing policy visible.
    std::printf("  stream 0        %u packet(s), %llu byte(s)\n",
                result.packetsOnStream0,
                static_cast<unsigned long long>(result.bytesOnStream0));
    std::printf("  stream 1        %u packet(s), %llu byte(s)\n",
                result.packetsOnStream1,
                static_cast<unsigned long long>(result.bytesOnStream1));

    return ok ? 0 : 1;
}
