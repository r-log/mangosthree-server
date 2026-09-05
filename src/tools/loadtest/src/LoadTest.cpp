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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <thread>
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
            "Protocol peer (movement P0-B):\n"
            "  --walk SECONDS       walk straight ahead for SECONDS once in the world\n"
            "  --heading DEGREES    walk in this direction instead of the character's facing\n"
            "  --return             walk the same time back, so the character ends where it began\n"
            "  --ack MODE           answer movement changes: immediate | delay:MS | mismatch | stale | drop\n"
            "  --observe GUID       count relayed movement of this mover\n"
            "  --pair ACCOUNT:GUID  run a second, observing session of that character alongside,\n"
            "                       watching this one, and print the relay verdict\n"
            "\n"
            "Typical use:\n"
            "  mangos-loadtest --account LOAD01 --emit-sql | mysql -u root realmd\n"
            "  mangos-loadtest --account LOAD01 --character 9 --hold 60 --verbose\n"
            "  mangos-loadtest --account RNDBOT0 --character 46 --walk 6 --return --hold 12 --pair RNDBOT1:47\n");
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
    std::string pairAccount;
    uint64      pairGuid = 0;

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
        else if (arg == "--walk")
        {
            if (!WantsValue(argc, i, "--walk")) { return 2; }
            config.script.walk.seconds = uint32(std::strtoul(argv[++i], NULL, 10));
        }
        else if (arg == "--heading")
        {
            if (!WantsValue(argc, i, "--heading")) { return 2; }
            config.script.walk.headingSet = true;
            config.script.walk.heading = float(std::strtod(argv[++i], NULL) * 3.14159265358979323846 / 180.0);
        }
        else if (arg == "--return")     { config.script.walk.returnHome = true; }
        else if (arg == "--ack")
        {
            if (!WantsValue(argc, i, "--ack")) { return 2; }
            const std::string mode = argv[++i];
            if (mode == "immediate")            { config.script.ack.mode = loadtest::AckMode::Immediate; }
            else if (mode == "mismatch")        { config.script.ack.mode = loadtest::AckMode::Mismatch; }
            else if (mode == "drop")            { config.script.ack.mode = loadtest::AckMode::Drop; }
            else if (mode == "stale")           { config.script.ack.mode = loadtest::AckMode::Stale; }
            else if (mode.compare(0, 6, "delay:") == 0)
            {
                config.script.ack.mode = loadtest::AckMode::Delay;
                config.script.ack.delayMs = uint32(std::strtoul(mode.c_str() + 6, NULL, 10));
            }
            else
            {
                std::fprintf(stderr, "--ack wants immediate, delay:MS, mismatch, stale or drop\n");
                return 2;
            }
        }
        else if (arg == "--observe")
        {
            if (!WantsValue(argc, i, "--observe")) { return 2; }
            config.script.observeGuid = std::strtoull(argv[++i], NULL, 10);
        }
        else if (arg == "--pair")
        {
            if (!WantsValue(argc, i, "--pair")) { return 2; }
            const std::string spec = argv[++i];
            const size_t colon = spec.find(':');
            if (colon == std::string::npos || colon == 0 || colon + 1 >= spec.size())
            {
                std::fprintf(stderr, "--pair wants ACCOUNT:GUID\n");
                return 2;
            }
            pairAccount = spec.substr(0, colon);
            pairGuid = std::strtoull(spec.c_str() + colon + 1, NULL, 10);
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

    for (size_t i = 0; i < pairAccount.size(); ++i)
    {
        pairAccount[i] = char(std::toupper(static_cast<unsigned char>(pairAccount[i])));
    }
    // A walk needs the session to outlive it, with room for the lead and the stop.
    if (config.script.walk.seconds > 0)
    {
        const uint32 legs = config.script.walk.returnHome ? 2 : 1;
        const uint32 needed = config.script.walk.seconds * legs + config.script.walk.leadMs / 1000 + 3;
        if (config.holdSeconds < needed)
        {
            config.holdSeconds = needed;
        }
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

    // The paired session logs in first and simply watches; the primary's walk
    // lead (3 s by default) gives it time to be in the world before anything
    // moves. Its own key is derived from its account name like the primary's.
    loadtest::Result pairResult;
    std::thread pairThread;
    if (!pairAccount.empty())
    {
        loadtest::Config pairConfig = config;
        pairConfig.account = pairAccount;
        pairConfig.sessionKey.SetHexStr(DeriveSessionKeyHex(pairAccount).c_str());
        pairConfig.characterGuid = pairGuid;
        pairConfig.script = loadtest::PeerScript();
        pairConfig.script.observeGuid = config.characterGuid;
        pairConfig.holdSeconds = config.holdSeconds;
        std::printf("pairing '%s' (character %llu), watching character %llu\n",
                    pairAccount.c_str(), static_cast<unsigned long long>(pairGuid),
                    static_cast<unsigned long long>(config.characterGuid));
        pairThread = std::thread([pairConfig, &pairResult]()
        {
            loadtest::SyntheticClient pair(pairConfig);
            pairResult = pair.Run();
        });
    }

    loadtest::SyntheticClient client(config);
    const loadtest::Result result = client.Run();
    if (pairThread.joinable())
    {
        pairThread.join();
    }
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

    const loadtest::PeerReport& peer = result.peer;
    std::printf("PEER timesync answered=%u controlUpdates=%u other=%u\n",
                peer.timeSyncsAnswered, peer.controlUpdates, peer.otherPackets);
    std::printf("PEER walk start=%u heartbeats=%u stop=%u final=%.1f %.1f %.1f lastTime=%u\n",
                peer.walkStarts, peer.walkHeartbeats, peer.walkStops,
                peer.walkFinal.x, peer.walkFinal.y, peer.walkFinal.z, peer.walkLastTime);
    std::printf("PEER acks sent=%u dropped=%u unregistered=", peer.acksSent, peer.acksDropped);
    for (std::map<uint16, uint32>::const_iterator it = peer.unregisteredChanges.begin();
         it != peer.unregisteredChanges.end(); ++it)
    {
        std::printf("0x%.4X:%u ", uint32(it->first), it->second);
    }
    std::printf("\n");
    std::printf("PEER decodefail ");
    for (std::map<uint16, uint32>::const_iterator it = peer.decodeFailures.begin();
         it != peer.decodeFailures.end(); ++it)
    {
        std::printf("0x%.4X:%u ", uint32(it->first), it->second);
    }
    std::printf("\n");

    bool verdictsOk = true;
    // The server sends a time-sync request at login and every ten seconds; any
    // hold long enough to answer one proves the responder.
    const bool timesyncOk = peer.timeSyncsAnswered >= 1;
    std::printf("PEER VERDICT timesync %s (answered %u)\n", timesyncOk ? "OK" : "BUG",
                peer.timeSyncsAnswered);
    verdictsOk = verdictsOk && timesyncOk;

    if (config.script.walk.seconds > 0)
    {
        // One start and one stop per leg, and at least two heartbeats per second
        // walked minus one per leg for the tail.
        const uint32 legs = config.script.walk.returnHome ? 2 : 1;
        const bool walkOk = peer.walkStarts == legs && peer.walkStops == legs &&
                            peer.walkHeartbeats + legs >= config.script.walk.seconds * 2 * legs;
        std::printf("PEER VERDICT walk %s (start %u, heartbeats %u, stop %u, legs %u)\n",
                    walkOk ? "OK" : "BUG", peer.walkStarts, peer.walkHeartbeats, peer.walkStops, legs);
        verdictsOk = verdictsOk && walkOk;
    }

    if (!pairAccount.empty() && config.script.walk.seconds == 0)
    {
        std::printf("PEER VERDICT relay SKIPPED (no walk)\n");
    }
    else if (!pairAccount.empty())
    {
        const loadtest::PeerReport& seen = pairResult.peer;
        const bool pairIn = pairResult.Reached(loadtest::Stage::InWorld) && pairResult.error.empty();
        const float dx = seen.lastTargetObservation.pos.x - peer.walkFinal.x;
        const float dy = seen.lastTargetObservation.pos.y - peer.walkFinal.y;
        const float gap = std::sqrt(dx * dx + dy * dy);
        // The observer must have seen the walker's relayed movement, and its last
        // sighting must be where the walker says it stopped.
        const bool relayOk = pairIn && seen.observedTarget >= 2 && gap <= 3.0f;
        std::printf("PEER pair stage=%s error=%s observedTarget=%u observedOthers=%u lastSeen=%.1f %.1f gap=%.1f\n",
                    loadtest::StageName(pairResult.stage),
                    pairResult.error.empty() ? "-" : pairResult.error.c_str(),
                    seen.observedTarget, seen.observedOthers,
                    seen.lastTargetObservation.pos.x, seen.lastTargetObservation.pos.y, gap);
        std::printf("PEER VERDICT relay %s\n", relayOk ? "OK" : "BUG");
        verdictsOk = verdictsOk && relayOk;
    }

    return (ok && verdictsOk) ? 0 : 1;
}
