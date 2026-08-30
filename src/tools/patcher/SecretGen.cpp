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

/**
 * @file SecretGen.cpp
 * @brief Mints the keypair that lets this server open a client's second stream.
 *
 * A 4.3.4 client will not open its second world connection just because it is
 * asked to. It verifies the instruction against an RSA public key compiled into
 * its own binary, and nobody outside Blizzard holds the private half of the
 * shipped one. So a server that wants the second stream has to substitute a key
 * it owns: patch the modulus into every client, and keep the private exponent.
 *
 * Two files come out of that, because two different parties need two different
 * halves and confusing them is how a private exponent ends up in a download:
 *
 *   client.secret   the modulus and one digest. Goes to the client patcher, and
 *                   ends up inside every client binary. Not secret -- it is
 *                   published to every player by definition.
 *
 *   server.secret   the private exponent as well. Never leaves the server. A
 *                   copy of this file is a licence to redirect this realm's
 *                   players to any address its holder likes.
 *
 * Both are written from one generation, so they cannot disagree. That is the
 * whole reason this is one command and not three config fields: a modulus in
 * mangosd.conf that does not match the one in the client produces a realm where
 * every login hangs at the loading screen, with nothing in any log that says so.
 */

#include "Auth/BigNumber.h"
#include "ConnectTo.h"

#include "Crypto/BigInt.h"
#include "Crypto/Rsa.h"
#include "Crypto/SecureZero.h"
#include "Crypto/SystemRandom.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef _WIN32
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace
{
    /// Size of the deployment blob the client checks its baked digest against.
    const size_t AUTH_BLOB_SIZE = 73;

    /// The client's public operation is fixed at 2048 bits.
    const int KEY_BITS = 2048;

    std::string ToHex(const std::vector<uint8>& bytes)
    {
        static const char digits[] = "0123456789ABCDEF";

        std::string text;
        text.reserve(bytes.size() * 2);
        for (size_t i = 0; i < bytes.size(); ++i)
        {
            text.push_back(digits[bytes[i] >> 4]);
            text.push_back(digits[bytes[i] & 0x0F]);
        }
        return text;
    }

    /**
     * @brief Serialise a BigInt to a fixed width, big-endian.
     *
     * Fixed width matters: the modulus has to be exactly 256 bytes because that
     * is the width the client's verifier reads, and a private exponent whose top
     * byte happens to be zero must not come out a byte short and be mistaken for
     * a different number.
     */
    std::string FixedWidthHex(const MaNGOS::Crypto::BigInt& value, size_t width)
    {
        const std::vector<uint8> bytes = value.ToBytesBE(width);
        return bytes.empty() ? std::string() : ToHex(bytes);
    }

    /// The pair as the files carry it: n and d at the full 256-byte width, the CRT
    /// parameters (PKCS#1 names) at 128 bytes -- the shape the server's CRT path reads.
    struct Keypair
    {
        std::string modulusHex;
        std::string privateExponentHex;
        std::string prime1Hex;
        std::string prime2Hex;
        std::string exponent1Hex;
        std::string exponent2Hex;
        std::string coefficientHex;
    };

    /// The private half passes through std::strings on its way to the file; they are
    /// erased when they go out of scope, whichever way the run ends.
    void Erase(std::string& text)
    {
        if (!text.empty())
        {
            MaNGOS::Crypto::SecureZero(&text[0], text.size());
        }
        text.clear();
    }

    struct KeypairEraser
    {
        Keypair& pair;
        ~KeypairEraser()
        {
            Erase(pair.privateExponentHex);
            Erase(pair.prime1Hex);
            Erase(pair.prime2Hex);
            Erase(pair.exponent1Hex);
            Erase(pair.exponent2Hex);
            Erase(pair.coefficientHex);
        }
    };

    struct StringEraser
    {
        std::string& text;
        ~StringEraser() { Erase(text); }
    };

    bool GenerateKeypair(Keypair& out)
    {
        // The tree's own generator (src/shared/Crypto/Prime, Rsa): FIPS 186-4 primes,
        // 64 Miller-Rabin rounds, the pair loaded and a probe signed before it is
        // handed back. Once per realm, offline, so it can afford all of that.
        MaNGOS::Crypto::RsaKeyPair pair;
        if (!MaNGOS::Crypto::RsaGenerateKey(size_t(KEY_BITS), MaNGOS::Crypto::BigInt(65537),
                                            MaNGOS::Crypto::SystemRandom::Instance(), pair))
        {
            std::fprintf(stderr, "secret-gen: RSA key generation failed\n");
            return false;
        }
        out.modulusHex         = FixedWidthHex(pair.n, KEY_BITS / 8);
        out.privateExponentHex = FixedWidthHex(pair.d, KEY_BITS / 8);
        out.prime1Hex          = FixedWidthHex(pair.p, KEY_BITS / 16);
        out.prime2Hex          = FixedWidthHex(pair.q, KEY_BITS / 16);
        out.exponent1Hex       = FixedWidthHex(pair.dP, KEY_BITS / 16);
        out.exponent2Hex       = FixedWidthHex(pair.dQ, KEY_BITS / 16);
        out.coefficientHex     = FixedWidthHex(pair.qInv, KEY_BITS / 16);
        const bool ok = !out.modulusHex.empty() && !out.privateExponentHex.empty() && !out.prime1Hex.empty() &&
                        !out.prime2Hex.empty() && !out.exponent1Hex.empty() && !out.exponent2Hex.empty() &&
                        !out.coefficientHex.empty();
        if (!ok)
        {
            std::fprintf(stderr, "secret-gen: the generated key does not have the expected widths\n");
        }
        return ok;
    }

    bool GenerateAuthBlob(std::string& out)
    {
        // The OS CSPRNG through the tree's own wrapper; a failure is fatal there, not
        // a weak blob here.
        std::vector<uint8> blob(AUTH_BLOB_SIZE, 0);
        MaNGOS::Crypto::SystemRandom::Instance().FillExact(blob.data(), blob.size());
        out = ToHex(blob);
        return true;
    }

    /**
     * @brief Refuse to replace a file that is already there.
     *
     * Overwriting a server.secret silently would lock every already-patched
     * client out of the realm, with the only copy of the key that admits them
     * gone. Making the operator move the old one out of the way first is the
     * cheapest possible guard against that. Checked for both files before
     * anything is generated, so a refusal never leaves half a pair behind.
     */
    bool Preflight(const std::string& path, bool force)
    {
        if (force)
        {
            return true;
        }
        std::ifstream existing(path.c_str());
        if (existing.good())
        {
            std::fprintf(stderr,
                "secret-gen: %s already exists. Move it aside first, or pass "
                "--force if you mean to replace it -- every client patched "
                "with the old key stops being able to log in.\n", path.c_str());
            return false;
        }
        return true;
    }

    /**
     * @brief Write a file through a temporary beside it, then rename into place.
     *
     * A private file is created readable by its owner only, from the first byte:
     * the private exponent must never sit on disk with the process's umask
     * permissions, even briefly. On Windows the file inherits the directory's
     * ACL, which is the operator's to restrict.
     */
    bool WriteFile(const std::string& path, const std::string& body, bool privateFile)
    {
        const std::string tmp = path + ".tmp";
        std::error_code ec;
        std::filesystem::remove(tmp, ec);   // a stale temporary is never reused
        bool written = false;
#ifndef _WIN32
        if (privateFile)
        {
            // Exclusive creation with the final mode: an existing file, or a
            // symlink planted at this name, fails here instead of being written through.
            const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
            if (fd >= 0)
            {
                size_t done = 0;
                while (done < body.size())
                {
                    const ssize_t n = ::write(fd, body.data() + done, body.size() - done);
                    if (n <= 0)
                    {
                        break;
                    }
                    done += size_t(n);
                }
                written = (done == body.size()) && (::close(fd) == 0);
            }
        }
        else
#endif
        {
            std::ofstream file(tmp.c_str(), std::ios::binary | std::ios::trunc);
            if (file.good())
            {
                file << body;
                file.flush();
                written = file.good();
            }
        }
        if (!written)
        {
            std::fprintf(stderr, "secret-gen: cannot write %s\n", tmp.c_str());
            std::filesystem::remove(tmp, ec);
            return false;
        }
        std::filesystem::rename(tmp, path, ec);
        if (ec)
        {
            std::fprintf(stderr, "secret-gen: cannot replace %s: %s\n", path.c_str(), ec.message().c_str());
            std::filesystem::remove(tmp, ec);
            return false;
        }
        return true;
    }

    void Usage()
    {
        std::printf(
            "secret-gen -- mint the keypair for the 4.3.4 second world stream\n"
            "\n"
            "  secret-gen [--out-dir DIR] [--force]\n"
            "\n"
            "Writes two files:\n"
            "\n"
            "  client.secret   modulus + digest. Feed it to the client patcher; it\n"
            "                  ends up in every client binary. Not secret.\n"
            "\n"
            "  server.secret   the above plus the private exponent. Point\n"
            "                  Redirect.SecretFile in mangosd.conf at it, and keep\n"
            "                  it readable only by the account the server runs as.\n"
            "                  Anyone holding it can redirect your players.\n"
            "\n"
            "Options:\n"
            "  --out-dir DIR   where to write both files (default: current directory)\n"
            "  --force         replace files that already exist\n"
            "  --help          this text\n"
            "\n"
            "Regenerating invalidates every client patched with the previous key.\n");
    }
}

int main(int argc, char** argv)
{
    std::string outDir;
    bool        force = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            Usage();
            return 0;
        }
        if (arg == "--force")
        {
            force = true;
            continue;
        }
        if (arg == "--out-dir")
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "secret-gen: --out-dir needs a directory\n");
                return 2;
            }
            outDir = argv[++i];
            continue;
        }

        std::fprintf(stderr, "secret-gen: unknown argument '%s'\n", arg.c_str());
        Usage();
        return 2;
    }

    if (!outDir.empty() && outDir[outDir.size() - 1] != '/' && outDir[outDir.size() - 1] != '\\')
    {
        outDir += '/';
    }

    const std::string clientPath = outDir + "client.secret";
    const std::string serverPath = outDir + "server.secret";
    if (!Preflight(clientPath, force) || !Preflight(serverPath, force))
    {
        return 1;
    }

    Keypair     keypair;
    KeypairEraser eraseKeypair{ keypair };
    std::string authBlobHex;
    if (!GenerateKeypair(keypair) || !GenerateAuthBlob(authBlobHex))
    {
        return 1;
    }

    // Load the pair through the server's own signer rather than deriving the
    // digest here. It proves, before either file is written, that this keypair
    // signs a redirect the client's verifier accepts -- and the digest the
    // client must carry comes from the one implementation that defines it.
    proto::RedirectSigner signer;
    if (!signer.Load(keypair.modulusHex, keypair.privateExponentHex, authBlobHex,
                     keypair.prime1Hex, keypair.prime2Hex, keypair.exponent1Hex, keypair.exponent2Hex, keypair.coefficientHex))
    {
        std::fprintf(stderr, "secret-gen: the generated key was rejected by the signer\n");
        return 1;
    }

    WorldPacket probe;
    std::vector<uint8> loopback;
    loopback.push_back(127);
    loopback.push_back(0);
    loopback.push_back(0);
    loopback.push_back(1);

    if (!signer.BuildConnectTo(loopback, proto::RedirectFamily::IPv4, 8086,
                               proto::LinkSlot::One, probe))
    {
        std::fprintf(stderr, "secret-gen: the generated key cannot sign a redirect\n");
        return 1;
    }

    const std::string digest = signer.ExpectedClientDigest();

    const std::string clientBody =
        "# Generated by secret-gen. Feed this to the client patcher.\n"
        "#\n"
        "# Modulus replaces the RSA public key in the client binary; Digest\n"
        "# replaces the reference digest it checks the auth blob against. Both\n"
        "# have to be patched, and both have to come from the same generation as\n"
        "# the server.secret this realm is running.\n"
        "#\n"
        "# Not a secret: every patched client carries it.\n"
        "\n"
        "Modulus = " + keypair.modulusHex + "\n"
        "Digest = " + digest + "\n";

    std::string serverBody =
        "# Generated by secret-gen. Point Redirect.SecretFile in mangosd.conf here.\n"
        "#\n"
        "# SECRET. PrivateExponent is what lets this server, and only this server,\n"
        "# tell a patched client where to open its second world connection. Anyone\n"
        "# who obtains this file can redirect this realm's players to an address of\n"
        "# their choosing. Keep it readable only by the account the server runs as,\n"
        "# out of the source tree, and out of backups that travel.\n"
        "\n"
        "Modulus = " + keypair.modulusHex + "\n"
        "PrivateExponent = " + keypair.privateExponentHex + "\n"
        "AuthBlob = " + authBlobHex + "\n"
        "\n"
        "# The primes and the CRT parameters (PKCS#1 names). With them the server signs\n"
        "# each redirect with two half-size exponentiations, about four times faster;\n"
        "# a file without them still loads and signs on the slower path.\n"
        "Prime1 = " + keypair.prime1Hex + "\n"
        "Prime2 = " + keypair.prime2Hex + "\n"
        "Exponent1 = " + keypair.exponent1Hex + "\n"
        "Exponent2 = " + keypair.exponent2Hex + "\n"
        "Coefficient = " + keypair.coefficientHex + "\n";
    StringEraser eraseServerBody{ serverBody };

    // Under --force a working key may be in place. Keep it aside until both new
    // files are installed, so a failure half-way leaves the old pair, not nothing.
    std::error_code ec;
    const std::string previousServer = serverPath + ".previous";
    const bool hadServer = std::filesystem::exists(serverPath, ec);
    if (hadServer)
    {
        std::filesystem::remove(previousServer, ec);
        std::filesystem::rename(serverPath, previousServer, ec);
        if (ec)
        {
            std::fprintf(stderr, "secret-gen: cannot set the existing %s aside: %s\n",
                         serverPath.c_str(), ec.message().c_str());
            return 1;
        }
    }
    // The private half first: if it cannot be written there is no point in
    // publishing a client key for it.
    if (!WriteFile(serverPath, serverBody, true))
    {
        if (hadServer)
        {
            std::filesystem::rename(previousServer, serverPath, ec);
        }
        return 1;
    }
    if (!WriteFile(clientPath, clientBody, false))
    {
        std::filesystem::remove(serverPath, ec);
        if (hadServer)
        {
            std::filesystem::rename(previousServer, serverPath, ec);
        }
        return 1;
    }
    if (hadServer)
    {
        std::filesystem::remove(previousServer, ec);
    }

    std::printf("secret-gen: wrote %s and %s\n", clientPath.c_str(), serverPath.c_str());
    std::printf("\n");
    std::printf("  mangosd.conf:  Redirect.SecretFile = \"%s\"\n", serverPath.c_str());
    std::printf("  client patch:  %s\n", clientPath.c_str());
    std::printf("\n");
    std::printf("Every client has to be patched from this client.secret before it can\n");
    std::printf("enter the world. Restrict server.secret to the server account.\n");

    return 0;
}
