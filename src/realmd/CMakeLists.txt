#/**
# * MaNGOS is a full featured server for World of Warcraft, supporting
# * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
# *
# * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
# *
# * This program is free software; you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation; either version 2 of the License, or
# * (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program; if not, write to the Free Software
# * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# *
# * World of Warcraft, and all World of Warcraft or Warcraft art, images,
# * and lore are copyrighted by Blizzard Entertainment, Inc.
# */

#Auth Files
file(GLOB_RECURSE SRC_GRP_AUTH Auth/*.cpp Auth/*.h)
source_group("Authentication" FILES ${SRC_GRP_AUTH})

#Realm Files
file(GLOB_RECURSE SRC_GRP_REALM Realm/*.cpp Realm/*.h)
source_group("Realmlist" FILES ${SRC_GRP_REALM})


configure_file(realmd.conf.dist.in ${CMAKE_CURRENT_BINARY_DIR}/realmd.conf.dist)

add_executable(realmd
    ${SRC_GRP_AUTH}
    ${SRC_GRP_REALM}
    Main.cpp
    $<$<BOOL:${WIN32}>:${CMAKE_SOURCE_DIR}/cmake/win/VersionInfo.rc>
)

# See the note in src/mangosd/CMakeLists.txt: only the strings that name this
# executable are per-target, and they reach the resource compiler directly.
if(WIN32)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/cmake/win/VersionInfo.rc
        TARGET_DIRECTORY realmd
        PROPERTIES COMPILE_DEFINITIONS
            "PRODUCT_ICON=\"${CMAKE_CURRENT_SOURCE_DIR}/realmd.ico\";PRODUCT_BUNDLE=\"realmd.exe\\0\";PRODUCT_FILE_DESCRIPTION=\"MaNGOS Authentication Daemon\\0\"")
endif()

target_compile_features(realmd PUBLIC cxx_std_17)
set_target_properties(realmd PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(realmd
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(realmd
    PUBLIC
        shared
        Threads::Threads
)

install(
    TARGETS realmd
    DESTINATION ${BIN_DIR}
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/realmd.conf.dist
    DESTINATION ${CONF_INSTALL_DIR}
)

if(WIN32 AND MSVC)
    install(
        FILES $<TARGET_PDB_FILE:realmd>
        DESTINATION ${BIN_DIR}
        OPTIONAL
        )
endif()

# Parent cores use two explicit test switches: WITH_TESTS in MangosTwo and
# BUILD_TESTING in MangosZero/MangosOne. Both default off in their respective
# parents, so realmd tests remain opt-in while working with every supported
# parent convention.
if(WITH_TESTS OR BUILD_TESTING)
    add_executable(realmd_client_locale_tests
        Tests/ClientLocaleTests.cpp
        Auth/ClientLocale.cpp
    )
    target_compile_features(realmd_client_locale_tests PUBLIC cxx_std_17)
    set_target_properties(realmd_client_locale_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(realmd_client_locale_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(NAME realmd_client_locale_tests COMMAND realmd_client_locale_tests)

    add_executable(realmd_auth_protocol_tests
        Tests/AuthProtocolGuardTests.cpp
        Auth/AuthProtocolGuard.cpp
    )
    target_compile_features(realmd_auth_protocol_tests PUBLIC cxx_std_17)
    set_target_properties(realmd_auth_protocol_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(realmd_auth_protocol_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(NAME realmd_auth_protocol_tests COMMAND realmd_auth_protocol_tests)

    add_executable(realmd_realm_snapshot_tests
        Tests/RealmSnapshotTests.cpp
    )
    target_compile_features(realmd_realm_snapshot_tests PUBLIC cxx_std_17)
    set_target_properties(realmd_realm_snapshot_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(realmd_realm_snapshot_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(realmd_realm_snapshot_tests PRIVATE
        shared
        Threads::Threads
    )
    add_test(NAME realmd_realm_snapshot_tests COMMAND realmd_realm_snapshot_tests)

    add_executable(realmd_client_build_policy_tests
        Tests/ClientBuildPolicyTests.cpp
        Auth/AuthResultPolicy.cpp
        Realm/ClientBuildPolicy.cpp
    )
    target_compile_features(realmd_client_build_policy_tests PUBLIC cxx_std_17)
    set_target_properties(realmd_client_build_policy_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(realmd_client_build_policy_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(realmd_client_build_policy_tests PRIVATE
        shared
    )
    add_test(NAME realmd_client_build_policy_tests COMMAND realmd_client_build_policy_tests)

    add_test(NAME realmd_auth_safety_policy
        COMMAND ${CMAKE_COMMAND}
            -DREALMD_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/CheckAuthSafety.cmake
    )

    add_test(NAME realmd_srp6_width_policy
        COMMAND ${CMAKE_COMMAND}
            -DREALMD_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/CheckSrp6Widths.cmake
    )

    add_executable(realmd_patch_safety_tests
        Tests/PatchSafetyTests.cpp
        Auth/PatchPolicy.cpp
        Auth/PatchArtifact.cpp
    )
    target_compile_features(realmd_patch_safety_tests PUBLIC cxx_std_17)
    set_target_properties(realmd_patch_safety_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(realmd_patch_safety_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(realmd_patch_safety_tests PRIVATE
        shared
    )
    add_test(NAME realmd_patch_safety_tests COMMAND realmd_patch_safety_tests)

    add_test(NAME realmd_patch_safety_policy
        COMMAND ${CMAKE_COMMAND}
            -DREALMD_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/CheckPatchSafety.cmake
    )
endif()
