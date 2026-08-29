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

#include "TestHarness.h"

#include "ConnectTo.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    /**
     * @brief A throwaway RSA-2048 keypair, for this file only.
     *
     * The real one lives outside the tree and is never in a test. What is being
     * checked here is the structure of the block, which the key does not change:
     * the client verifies the plaintext, and the plaintext is the same shape
     * whichever key signed it.
     */
    const char* TEST_MODULUS =
        "C0F1E1AB9E44BFC0F8DA0C5B94EF77B4C1C7C0B27FDD1DB0DB6E4A9E76B0F1C7"
        "6B26E0E4C2ED8A4CFA1D26A2E1F0B3C48E4C1DB8B21DFAC0AA7C5B5E8E5D1D6E"
        "9D6C0A3F51A3D1C8C5A3D0D7B5CD9A2C4A2C3B5D4E1F2A3B4C5D6E7F8091A2B3"
        "C4D5E6F708192A3B4C5D6E7F8091A2B3C4D5E6F708192A3B4C5D6E7F8091A2B3"
        "1D2E3F4051627384950A1B2C3D4E5F60718293A4B5C6D7E8F90A1B2C3D4E5F60"
        "718293A4B5C6D7E8F90A1B2C3D4E5F60718293A4B5C6D7E8F90A1B2C3D4E5F61"
        "0B1C2D3E4F5061728394A5B6C7D8E9FA0B1C2D3E4F5061728394A5B6C7D8E9FB"
        "0C1D2E3F405162738495A6B7C8D9EAFB0C1D2E3F405162738495A6B7C8D9EB0D";

    /// The digest a client has to hold for an all-zero auth blob. Independent of
    /// the key: it is an HMAC over the blob alone, with the constant the client
    /// keeps in its own image.
    const char* ZERO_BLOB_DIGEST = "4775ECCC17494E4FD1BFAE19737735A4F451E6FB";

    std::vector<uint8> Ipv4(uint8 a, uint8 b, uint8 c, uint8 d)
    {
        std::vector<uint8> address;
        address.push_back(a);
        address.push_back(b);
        address.push_back(c);
        address.push_back(d);
        return address;
    }
}

TEST(ConnectTo_expected_digest_for_an_empty_auth_blob)
{
    // The auth blob and the digest are patched into the client as a pair, and
    // the server is where an operator reads the digest their blob needs. Getting
    // this wrong is a client that refuses every redirect.
    proto::RedirectSigner signer;
    CHECK(signer.Load(TEST_MODULUS, "010001", ""));
    CHECK_STR(signer.ExpectedClientDigest(), ZERO_BLOB_DIGEST);
}

TEST(ConnectTo_rejects_a_malformed_key)
{
    proto::RedirectSigner signer;

    CHECK(!signer.Load("", "", ""));
    CHECK(!signer.Load(TEST_MODULUS, "", ""));

    CHECK(!signer.Load(TEST_MODULUS, "01000Z", ""));

    // The modulus has to be RSA-2048; the client's public operation is fixed at
    // 256 bytes, so a shorter one cannot round-trip the plaintext.
    CHECK(!signer.Load("ABCD", "010001", ""));

    CHECK(!signer.IsLoaded());
}

TEST(ConnectTo_accepts_an_exponent_written_without_its_leading_zero)
{
    // Key generators print the private exponent as a plain integer, so its top
    // nibble being zero costs it a digit. That is the same number, and refusing
    // it would look to an operator exactly like a mistyped key.
    proto::RedirectSigner odd;
    CHECK(odd.Load(TEST_MODULUS, "10001", ""));
    CHECK(odd.IsLoaded());

    proto::RedirectSigner padded;
    CHECK(padded.Load(TEST_MODULUS, "010001", ""));
    CHECK(padded.IsLoaded());
}

TEST(ConnectTo_rejects_a_wrong_sized_auth_blob)
{
    proto::RedirectSigner signer;
    CHECK(!signer.Load(TEST_MODULUS, "010001", "0011"));
    CHECK(!signer.IsLoaded());
}

TEST(ConnectTo_unloaded_signer_builds_nothing)
{
    proto::RedirectSigner signer;
    WorldPacket packet;
    CHECK(!signer.BuildConnectTo(Ipv4(203, 0, 113, 7), proto::RedirectFamily::IPv4,
                                 8086, proto::LinkSlot::One, packet));
}

TEST(ConnectTo_rejects_an_address_that_does_not_match_its_family)
{
    proto::RedirectSigner signer;
    CHECK(signer.Load(TEST_MODULUS, "010001", ""));

    WorldPacket packet;

    // Four bytes announced as IPv6, and sixteen announced as IPv4. Either would
    // put the port where the client does not read it.
    CHECK(!signer.BuildConnectTo(Ipv4(203, 0, 113, 7), proto::RedirectFamily::IPv6,
                                 8086, proto::LinkSlot::One, packet));

    CHECK(!signer.BuildConnectTo(std::vector<uint8>(16, 0), proto::RedirectFamily::IPv4,
                                 8086, proto::LinkSlot::One, packet));
}

TEST(ConnectTo_reads_a_server_secret_file)
{
    const std::string path = "connectto_test_server.secret";

    {
        std::ofstream file(path.c_str());
        file << "# Generated by secret-gen.\n"
             << "\n"
             << "Modulus = " << TEST_MODULUS << "\n"
             << "PrivateExponent = 010001   # trailing comment\n"
             << "AuthBlob = " << std::string(146, '0') << "\n";
    }

    proto::RedirectSigner signer;
    CHECK(signer.LoadFromFile(path));
    CHECK(signer.IsLoaded());
    CHECK_STR(signer.ExpectedClientDigest(), ZERO_BLOB_DIGEST);

    std::remove(path.c_str());
}

TEST(ConnectTo_rejects_a_client_secret_given_where_a_server_secret_belongs)
{
    // A client.secret carries Modulus and Digest, no private exponent. Loading
    // one here would leave a server that cannot sign anything, so say which
    // file is wrong rather than failing later at the first login.
    const std::string path = "connectto_test_client.secret";

    {
        std::ofstream file(path.c_str());
        file << "Modulus = " << TEST_MODULUS << "\n"
             << "Digest = " << ZERO_BLOB_DIGEST << "\n";
    }

    proto::RedirectSigner signer;
    CHECK(!signer.LoadFromFile(path));
    CHECK(!signer.IsLoaded());

    std::remove(path.c_str());
}

TEST(ConnectTo_missing_secret_file_is_not_a_crash)
{
    proto::RedirectSigner signer;
    CHECK(!signer.LoadFromFile("connectto_test_no_such_file.secret"));
    CHECK(!signer.IsLoaded());
}

TEST(ConnectTo_refuses_a_modulus_and_exponent_that_are_not_a_pair)
{
    // The signature is checked against the client's own public operation before
    // it goes out. Without that, a mismatched key ships a redirect the client
    // silently ignores, and the fault presents as a client that never opens its
    // second stream -- indistinguishable from a firewall.
    proto::RedirectSigner signer;
    CHECK(signer.Load(TEST_MODULUS, "0203", ""));

    WorldPacket packet;
    CHECK(!signer.BuildConnectTo(Ipv4(203, 0, 113, 7), proto::RedirectFamily::IPv4,
                                 8086, proto::LinkSlot::One, packet));
}
