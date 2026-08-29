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
# The two halves of admitting a client to the second world stream.
#
#   secret-gen    mints the keypair, once per realm
#   mangos-patch  writes the public half into a client binary, once per client
#
# They are built together because they are one workflow and one failure mode: a
# client patched from a different generation than the server is running is a
# client that logs in and then hangs, with nothing anywhere that says why.
# =============================================================================

add_executable(secret-gen SecretGen.cpp)

# proto owns the signer, so the digest a client must carry comes from the same
# implementation the server verifies against, and the generated key is proved to
# sign an acceptable redirect before it is written to disk.
target_link_libraries(secret-gen
    PRIVATE
        proto
        shared
        mangos_crypto
)

set_target_properties(secret-gen PROPERTIES FOLDER "tools")

install(TARGETS secret-gen DESTINATION ${BIN_DIR}/tools)

# The client patcher stands alone: it reads a PE image and a client.secret, and
# links nothing of the server.
add_executable(mangos-patch
    src/main.cpp
    src/PeImage.cpp
    src/ClientFile.cpp
    src/Patch.cpp
    src/Targets.cpp
)

target_include_directories(mangos-patch PRIVATE src)
# The hashes are the tree's own (src/shared/Crypto) -- and nothing else: no shared, no proto, no OpenSSL.
target_link_libraries(mangos-patch PRIVATE mangos_crypto)

if(MSVC)
    target_compile_options(mangos-patch PRIVATE /W4 /permissive-)
else()
    target_compile_options(mangos-patch PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(mangos-patch PROPERTIES FOLDER "tools")

install(TARGETS mangos-patch DESTINATION ${BIN_DIR}/tools)
