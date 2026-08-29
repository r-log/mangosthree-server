# SPDX-License-Identifier: GPL-3.0-or-later
#
# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

# =============================================================================
# Unit tests. -DWITH_TESTS=1 builds them; -DWITH_NET_TESTS=1 adds the socket tests.
#
# Scope is deliberately narrow: the pieces that are pure functions over bytes and
# whose failure modes are invisible in ordinary use -- packet reassembly under
# fragmentation, and a zero-padding bug that shows about one login in 256.
#
# THE TWO SWITCHES ARE DISJOINT. The socket tests bind real listeners and cost more
# wall-clock than the whole rest of the suite, so WITH_TESTS never implies them. They
# are not run in CI. Ask for them by name when a change touches the reactor.
#
# No test framework is vendored; see TestHarness.h.
# =============================================================================

set(SRC_GRP_TESTS
    main.cpp
    TestHarness.h
    BigNumberTest.cpp
    Utf8Test.cpp
    ByteBufferTest.cpp
    LFGPacketsTest.cpp
    LFGEmpowermentTest.cpp
    LFGLockReasonTest.cpp
    LFGRoleAssignmentTest.cpp
    LFGProposalLogicTest.cpp
    LFGBootLogicTest.cpp
    LFGDungeonResolutionTest.cpp
    LFGRewardLogicTest.cpp
    LFGCallToArmsLogicTest.cpp
    SessionMailboxTest.cpp
    SessionProtocolPolicyTest.cpp
    OpcodeSlotsTest.cpp
    SessionLinksTest.cpp
    RedirectRegistryTest.cpp
    ConnectToTest.cpp
    DatabaseConcurrencyTest.cpp
    AuthCryptTest.cpp
    ClientParserTest.cpp
    TerrainModelTest.cpp
    TileSerializerTest.cpp
    ModelMapTest.cpp
    NavBinningTest.cpp
    DynamicCollisionTest.cpp
    PlacementTest.cpp
    ArbiterModelTest.cpp
    ShadowClassifierTest.cpp
    # Compiled in, not linked from `game`: game.lib pulls the whole server, down to the
    # database globals that only mangosd defines. These three know nothing of it.
    ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers/DynamicCollision.cpp
    ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers/GameObjectModel.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Server/SessionMailbox.cpp
    # The movement arbiter's pure core: no Unit, no driver, no map.
    ${CMAKE_SOURCE_DIR}/src/game/MotionGenerators/Arbiter/MoveContract.cpp
    ${CMAKE_SOURCE_DIR}/src/game/MotionGenerators/Arbiter/ArbiterModel.cpp
    ${CMAKE_SOURCE_DIR}/src/game/MotionGenerators/Arbiter/ShadowClassifier.cpp
    ByteBufferStressTest.cpp
    CodecStressTest.cpp
    CryptoStressTest.cpp
    PacketCodecTest.cpp
    # The SRP6 unit realmd calls and the vectors captured on the OpenSSL build.
    Srp6Test.cpp
    CryptoGoldenTest.cpp
    # The in-house primitives (src/shared/Crypto), checked against the vectors above.
    CryptoHashTest.cpp
    CryptoRandomTest.cpp
    CryptoBigIntTest.cpp
    CryptoMontgomeryTest.cpp
    CryptoRsaTest.cpp
    RsaTestKey.h
    CryptoOracleTest.cpp
    CryptoBench.cpp
    BigIntVectors.h
    CryptoGoldenVectors.h
    ${CMAKE_SOURCE_DIR}/src/realmd/Auth/Srp6.cpp
)

# The socket tests. Off unless asked for -- see the note above.
if(WITH_NET_TESTS)
    list(APPEND SRC_GRP_TESTS GracefulCloseTest.cpp)
    list(APPEND SRC_GRP_TESTS NetworkStressTest.cpp)
endif()

source_group("tests" FILES ${SRC_GRP_TESTS})

add_executable(mangos_tests ${SRC_GRP_TESTS})

target_include_directories(mangos_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers
        ${CMAKE_SOURCE_DIR}/src/game/Server
        ${CMAKE_SOURCE_DIR}/src/game/MotionGenerators
        ${CMAKE_SOURCE_DIR}/src/realmd)

target_link_libraries(mangos_tests
    PRIVATE
        proto
        shared
        mangos_crypto
        extractor_client
        extractor_data
        Threads::Threads
        mangos_openssl_strict
)

# The big-integer oracle (src/tests/oracle): s2n-bignum's proved routines, assembled where
# their GAS sources build -- Linux x86-64, GCC or Clang -- and compared with the in-house
# kernels in CryptoOracleTest. Elsewhere the same comparisons run against the Python vectors.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64" AND NOT MSVC)
    add_subdirectory(oracle)
    target_link_libraries(mangos_tests PRIVATE s2n_bignum_oracle)
    target_compile_definitions(mangos_tests PRIVATE MANGOS_CRYPTO_ORACLE=1)
endif()

add_test(NAME mangos_tests COMMAND mangos_tests)

# The golden-vector generator. Run on the OpenSSL build, its stdout is CryptoGoldenVectors.h
# and its stderr the timing baseline the in-house kernel is measured against:
#   cmake --build <build> --config Release --target crypto_golden_capture
#   <build>/src/tests/[Release/]crypto_golden_capture > src/tests/CryptoGoldenVectors.h 2> baseline.md
# Not built by default. It captures what OpenSSL computes, so it leaves the tree with OpenSSL.
add_executable(crypto_golden_capture EXCLUDE_FROM_ALL
    tools/CryptoGoldenCapture.cpp
    ${CMAKE_SOURCE_DIR}/src/realmd/Auth/Srp6.cpp)
target_include_directories(crypto_golden_capture PRIVATE ${CMAKE_SOURCE_DIR}/src/realmd)
target_link_libraries(crypto_golden_capture PRIVATE shared Threads::Threads mangos_openssl mangos_openssl_strict)
set_target_properties(crypto_golden_capture PROPERTIES FOLDER "tests")

add_test(NAME proto_boundary
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckProtoBoundary.cmake)

add_test(NAME warden_dormant_boundary
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckDormantWardenBoundary.cmake)

add_test(NAME build_policy
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckBuildPolicy.cmake)

add_test(NAME spatial_boundary
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckSpatialBoundary.cmake)

add_test(NAME version_sources
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckVersionSources.cmake)

# Put the runtime DLLs beside the test binary so it can be run from the build tree.
# A convenience, never a gate: the search is derived from the library that was actually
# found, and a miss warns rather than failing the configure.
if(WIN32)
    get_filename_component(MANGOS_SSL_LIB_DIR "${OPENSSL_CRYPTO_LIBRARY}" DIRECTORY)
    get_filename_component(MANGOS_SSL_ROOT "${MANGOS_SSL_LIB_DIR}" DIRECTORY)

    find_file(MANGOS_TEST_OPENSSL_CRYPTO_DLL
        NAMES libcrypto-3-x64.dll libcrypto-3.dll libcrypto.dll
        HINTS "${MANGOS_SSL_ROOT}/bin" "${MANGOS_SSL_LIB_DIR}" "${OPENSSL_ROOT_DIR}/bin")

    if(MANGOS_TEST_OPENSSL_CRYPTO_DLL)
        add_custom_command(TARGET mangos_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MANGOS_TEST_OPENSSL_CRYPTO_DLL}"
                "$<TARGET_FILE_DIR:mangos_tests>")
    else()
        message(WARNING
            "OpenSSL runtime DLL not found; mangos_tests may need it on PATH to run.")
    endif()

    get_filename_component(
        MANGOS_TEST_MYSQL_DLL_DIR "${MySQL_LIBRARY}" DIRECTORY)
    get_filename_component(
        MANGOS_TEST_MYSQL_DLL_NAME "${MySQL_LIBRARY}" NAME)
    string(REPLACE ".lib" ".dll" MANGOS_TEST_MYSQL_DLL_NAME
        "${MANGOS_TEST_MYSQL_DLL_NAME}")
    if(EXISTS "${MANGOS_TEST_MYSQL_DLL_DIR}/${MANGOS_TEST_MYSQL_DLL_NAME}")
        add_custom_command(TARGET mangos_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MANGOS_TEST_MYSQL_DLL_DIR}/${MANGOS_TEST_MYSQL_DLL_NAME}"
                "$<TARGET_FILE_DIR:mangos_tests>")
    endif()
endif()
