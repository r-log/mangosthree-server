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
    // A throwaway RSA-2048 pair generated for these tests (e = 65537). The
    // realm's key lives outside the tree and is never in a test.
    const char* TEST_MODULUS =
        "91744218A5AC0134DC2BE63E95AA3F2F3130C1714DB7084E3AC31A170C4FA02B"
        "F9D62E5CB1E444258D852B100395CF7D2DC0088BB7B96DF2B1EDC3BFF0D968F8"
        "D1003D4726DBC79417B001AB37653922BC10FF538B43B186E0071E41653382C5"
        "2FE58D701948AAA8F20D15B62EC4147AE1142AC3A287461D8F8C9D4021369889"
        "FED17806C3662CEC078092D90CBE881343A160C0CD8984094C7D6F18481DE73E"
        "14EB6766D7C9EF88A575B5910C67D62F4071EFB3639EF5A1E7892CF16F6D6D43"
        "F56495769D0683580030F514CB21208360116717C16CCC4B233A11F8CCB3CEBC"
        "D9873C33CFA81ACD619F567403D8839B88CA37FA08392F02439054B88E70E233";
    const char* TEST_PRIVATE_EXPONENT =
        "0B8B0F67C756142E6EBEA922145C93711A554534C9B719D8A37F3245DBFB41B9"
        "DBB4ECAEFC8B22015CEED1910EC7C7D4A659D413CA7BD3C6EBE9F39BFAF0360D"
        "7100B4DC3DB039717E43C08E26F2488B8223532FFD205D295804189995FF7584"
        "529DC410BE60EEF2436B586AC1E15BC2B8B41204BE943FB33EDE28E89AFA2B36"
        "C145CA75174805FA0278EC54B545CDFF1D744C63BDC2F568692480D04933E295"
        "E0AFE18D886721843D41B51F491900EB58771749BD032F5B9453484516B26642"
        "A1355D38C1CD98F2E079FC3B66303DB17EEAE5B8A2DAFE605093031EE242C2C2"
        "C84BD028B6E94F3B87C2800FD79D7EFCD5DC40C53853A448DB17393EFDFEFEB9";

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
    CHECK(signer.Load(TEST_MODULUS, TEST_PRIVATE_EXPONENT, ""));
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
    // TEST_PRIVATE_EXPONENT starts with a zero nibble; drop that digit.
    CHECK(odd.Load(TEST_MODULUS, TEST_PRIVATE_EXPONENT + 1, ""));
    CHECK(odd.IsLoaded());

    proto::RedirectSigner padded;
    CHECK(padded.Load(TEST_MODULUS, TEST_PRIVATE_EXPONENT, ""));
    CHECK(padded.IsLoaded());
}

TEST(ConnectTo_rejects_a_wrong_sized_auth_blob)
{
    proto::RedirectSigner signer;
    CHECK(!signer.Load(TEST_MODULUS, TEST_PRIVATE_EXPONENT, "0011"));
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
    CHECK(signer.Load(TEST_MODULUS, TEST_PRIVATE_EXPONENT, ""));

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
             << "PrivateExponent = " << TEST_PRIVATE_EXPONENT << "   # trailing comment\n"
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

TEST(ConnectTo_signs_with_a_real_pair)
{
    proto::RedirectSigner signer;
    REQUIRE(signer.Load(TEST_MODULUS, TEST_PRIVATE_EXPONENT, ""));
    WorldPacket packet;
    CHECK(signer.BuildConnectTo(Ipv4(203, 0, 113, 7), proto::RedirectFamily::IPv4,
                                8086, proto::LinkSlot::One, packet));
    CHECK_EQ(packet.size(), size_t(4 + 4 + 4 + 256 + 1));
}

TEST(ConnectTo_refuses_a_modulus_and_exponent_that_are_not_a_pair)
{
    // The signature is checked against the client's own public operation before
    // it goes out. Without that, a mismatched key ships a redirect the client
    // silently ignores, and the fault presents as a client that never opens its
    // second stream -- indistinguishable from a firewall.
    proto::RedirectSigner signer;
    CHECK(!signer.Load(TEST_MODULUS, "0203", ""));
    CHECK(!signer.IsLoaded());
}
