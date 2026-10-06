set(SRC_GRP_TESTS
    main.cpp
    TestHarness.h
    BigNumberTest.cpp
    Utf8Test.cpp
    ByteBufferTest.cpp
    CalendarTimeTest.cpp
    BareMapTest.cpp
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
    OpcodeValuesTest.cpp
    MovementCodecTest.cpp
    MovementParityTest.cpp
    ServerRoutingTest.cpp
    SessionLinksTest.cpp
    RedirectRegistryTest.cpp
    RedirectStreamTest.cpp
    ClientFramingTest.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/loadtest/src/Framing.cpp
    PeerTest.cpp
    ReplayTest.cpp
    GoldenCaptureTest.cpp
    FamilyCodecTest.cpp
    MotionMatrixTest.cpp
    PendingChangesTest.cpp
    TimeBaseTest.cpp
    MotionWritersTest.cpp
    MotionStateTest.cpp
    HarnessTest.cpp
    WorldClockTest.cpp
    RNGSeedTest.cpp
    MotionAuthorityTest.cpp
    MotionArbiterTest.cpp
    MotionMobilityTest.cpp
    MotionBehaviourTest.cpp
    MotionTaxiTest.cpp
    TargetKinematicsTest.cpp
    JumpArcTest.cpp
    KnockBackDirectionTest.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/loadtest/src/TimeSync.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/loadtest/src/Walker.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/loadtest/src/Control.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/loadtest/src/AckEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/loadtest/src/Replay.cpp
    GlobalWmoEvictionTest.cpp
    ConnectToTest.cpp
    FakeDatabase.h
    DatabaseConcurrencyTest.cpp
    NullDatabaseTest.cpp
    TickGuardTest.cpp
    InstanceDataCacheTest.cpp
    TestSeamTest.cpp
    PetitionAsyncTest.cpp
    CharacterCacheTest.cpp
    CharacterOpsAsyncTest.cpp
    PetCacheTest.cpp
    GuildAsyncTest.cpp
    CalendarMailAsyncTest.cpp
    GameLinkTest.cpp
    MonthlyQuestResetTest.cpp
    AuthCryptTest.cpp
    ClientParserTest.cpp
    TerrainModelTest.cpp
    TileSerializerTest.cpp
    ModelMapTest.cpp
    NavBinningTest.cpp
    DynamicCollisionTest.cpp
    PlacementTest.cpp
    ByteBufferStressTest.cpp
    CodecStressTest.cpp
    PacketCodecTest.cpp
    CombatLeafTest.cpp
    CombatGoldenVectors.h
    CreatureThreatTest.cpp
    UnitSubtypeTest.cpp
    SpellHitResultTest.cpp
    AuraContainerTest.cpp
    SpellHandlerRegistryTest.cpp
    QuestStatusMgrTest.cpp
    QuestPredicateTest.cpp
    QuestCompletePacketTest.cpp
    QuestRewardTest.cpp
    TalentMgrTest.cpp
    ItemFixture.h
    ItemFixture.cpp
    ItemFixtureTest.cpp
    InventoryMgrTest.cpp
    InventoryStorageTest.cpp
    InventoryBankTest.cpp
    RuneMgrTest.cpp
    SpellCooldownMgrTest.cpp
    CooldownPacketsTest.cpp
    PlayerClientPacketsTest.cpp
    GroupUpdateFactsTest.cpp
    SpellModMgrTest.cpp
    SpellModPacketsTest.cpp
    UnitSpellCooldownTest.cpp
    UnitCombatStatsTest.cpp
    UnitItemByGuidTest.cpp
    UnitMountPetTest.cpp
    UnitVisibilitySessionTest.cpp
    UnitReputationRankTest.cpp
    UnitCooldownActionsTest.cpp
    UnitComboActionsTest.cpp
    UnitRageActionsTest.cpp
    UnitKillCreditActionsTest.cpp
    UnitFactionAndBattleGroundActionsTest.cpp
    UnitProcOneOffActionsTest.cpp
    UnitTalentRankTest.cpp
    UnitRuneCooldownTest.cpp
    UnitAuraStatePassiveCastsTest.cpp
    UnitOwnSessionPacketsTest.cpp
    UnitAccountSecurityTest.cpp
    UnitPositionAndMovingTest.cpp
    UnitClientControlTest.cpp
    UnitNearTeleportTest.cpp
    UnitDamageCreditTest.cpp
    UnitSpellDamagePlayerReadsTest.cpp
    UnitPossessPlayerCallsTest.cpp
    GlyphMgrTest.cpp
    PetMgrTest.cpp
    SocialMgrTest.cpp
    CurrencyMgrTest.cpp
    ReputationMgrTest.cpp
    HonorMgrTest.cpp
    MailLevelRewardStoreTest.cpp
    ExplorationBaseXpStoreTest.cpp
    FishingBaseSkillStoreTest.cpp
    PointOfInterestStoreTest.cpp
    QuestPOIStoreTest.cpp
    DungeonFinderStoreTest.cpp
    LfgDungeonEntranceStoreTest.cpp
    OpcodeDispatchTest.cpp
)

if(WITH_NET_TESTS)
    list(APPEND SRC_GRP_TESTS GracefulCloseTest.cpp)
    list(APPEND SRC_GRP_TESTS NetworkStressTest.cpp)
endif()

if(WITH_CRYPTO_TESTS)
    list(APPEND SRC_GRP_TESTS
        Srp6Test.cpp
        CryptoGoldenTest.cpp
        CryptoGoldenVectors.h
        CryptoHashTest.cpp
        CryptoRandomTest.cpp
        CryptoBigIntTest.cpp
        BigIntVectors.h
        CryptoMontgomeryTest.cpp
        CryptoRsaTest.cpp
        RsaTestKey.h
        CryptoPrimeTest.cpp
        PrimeVectors.h
        CryptoOracleTest.cpp
        CryptoStressTest.cpp
        CryptoBench.cpp
        ${CMAKE_SOURCE_DIR}/src/realmd/Auth/Srp6.cpp)
endif()

source_group("tests" FILES ${SRC_GRP_TESTS})

add_executable(mangos_tests ${SRC_GRP_TESTS})

target_include_directories(mangos_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/game/movement
        ${CMAKE_SOURCE_DIR}/src/realmd
        ${CMAKE_SOURCE_DIR}/src/tools/loadtest/src)

target_compile_definitions(mangos_tests PRIVATE MANGOS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/goldens")

# Decoupling D7h. TickGuard::Strict() reaches this binary the long way, through shared_db's
# PUBLIC MANGOS_STRICT_TICK; this is the same option taking the short way, straight onto the
# test target. TickGuard_StrictMatchesTheBuildConfiguration compares the two, so a build
# configured to arm the guard cannot quietly end up with an unarmed one.
target_compile_definitions(mangos_tests
    PRIVATE $<$<BOOL:${MANGOS_STRICT_TICK}>:MANGOS_TESTS_STRICT_TICK_CONFIGURED>)

target_link_libraries(mangos_tests
    PRIVATE
        game
        motion
        proto
        shared_db
        mangos_crypto
        extractor_client
        extractor_data
        Threads::Threads
)

# Decoupling D1 (server #80): the test binary links the game library. Every decoupling PR's
# tests stand on this; a tidy-up that drops the link takes them all out with it. Read the real
# link set, not the text of this file.
get_target_property(MANGOS_TESTS_LINKS mangos_tests LINK_LIBRARIES)
if(NOT "game" IN_LIST MANGOS_TESTS_LINKS)
    message(FATAL_ERROR "mangos_tests no longer links the game library (decoupling D1, server #80)")
endif()

if(WITH_CRYPTO_TESTS)
    target_compile_definitions(mangos_tests PRIVATE MANGOS_CRYPTO_TESTS=1)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64" AND NOT MSVC)
        add_subdirectory(oracle)
        target_link_libraries(mangos_tests PRIVATE s2n_bignum_oracle)
        target_compile_definitions(mangos_tests PRIVATE MANGOS_CRYPTO_ORACLE=1)
    endif()
endif()

add_test(NAME mangos_tests COMMAND mangos_tests)

add_test(NAME cooldown_empty_event_callback
    COMMAND ${CMAKE_COMMAND}
        -DTESTS_EXE=$<TARGET_FILE:mangos_tests>
        -DCASE=SpellCooldownMgr_EmptyEventCallbackAsserts
        -DEXPECT=sendEvent
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckAbortCase.cmake)

add_test(NAME cooldown_empty_cleared_callback
    COMMAND ${CMAKE_COMMAND}
        -DTESTS_EXE=$<TARGET_FILE:mangos_tests>
        -DCASE=SpellCooldownMgr_EmptyClearedCallbackAsserts
        -DEXPECT=sendCleared
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckAbortCase.cmake)

add_test(NAME player_client_empty_callback
    COMMAND ${CMAKE_COMMAND}
        -DTESTS_EXE=$<TARGET_FILE:mangos_tests>
        -DCASE=PlayerClientPackets_EmptyCallbackAsserts
        -DEXPECT=sink
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckAbortCase.cmake)

add_test(NAME group_update_empty_callback
    COMMAND ${CMAKE_COMMAND}
        -DTESTS_EXE=$<TARGET_FILE:mangos_tests>
        -DCASE=GroupUpdateFacts_EmptyCallbackAsserts
        -DEXPECT=sink
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckAbortCase.cmake)

add_test(NAME group_update_no_callbacks
    COMMAND ${CMAKE_COMMAND}
        -DTESTS_EXE=$<TARGET_FILE:mangos_tests>
        -DCASE=GroupUpdateFacts_NoCallbacksAssert
        -DEXPECT=callbacks
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckAbortCase.cmake)

add_test(NAME spell_mod_empty_sink
    COMMAND ${CMAKE_COMMAND}
        -DTESTS_EXE=$<TARGET_FILE:mangos_tests>
        -DCASE=SpellModMgr_EmptySinkAsserts
        -DEXPECT=report
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckAbortCase.cmake)

add_test(NAME crypto_asm_twin
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckAsmTwin.cmake)

add_test(NAME proto_boundary
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckProtoBoundary.cmake)

add_test(NAME motion_boundary
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckMotionBoundary.cmake)

add_test(NAME header_reach
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHeaderReach.cmake)

add_test(NAME session_seam COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckSessionSeam.cmake)

add_test(NAME sync_db COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckSyncDb.cmake)

# Decoupling D4a: no file in CheckManagerIsolation.cmake's MANAGER_FILES names the character class.
add_test(NAME manager_isolation
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckManagerIsolation.cmake)

# Decoupling D4h: a character manager's packets and tables are spelled only in the manager and in
# the files CheckStateOwnership.cmake's table allows, and each allowance only shrinks. The gate is
# table-driven: D4h's rows are reputation and achievement, and the D4k PRs add a row per manager.
add_test(NAME state_ownership
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckStateOwnership.cmake)

add_test(NAME warden_dormant_boundary
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckDormantWardenBoundary.cmake)

add_test(NAME crypto_is_ours
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckCryptoIsOurs.cmake)

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

add_test(NAME opcode_denylist
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckOpcodeDenylist.cmake)

add_test(NAME motion_shim
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckMotionMasterShim.cmake)

add_test(NAME motion_blocked_masks
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckBlockedMasks.cmake)

add_test(NAME client_map_id
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckClientMapId.cmake)

add_test(NAME headless_console
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckHeadlessConsole.cmake)

# Decoupling D4l: no two include directories of game, mangos_tests, mangosd or mangosscript offer
# a header under the same spelling, so no include is resolved by directory order.
add_test(NAME include_collisions
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckIncludeCollisions.cmake)

find_package(Python3 COMPONENTS Interpreter QUIET)
add_test(NAME layout
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -DLAYOUT_PYTHON=${Python3_EXECUTABLE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckLayout.cmake)

# Decoupling D11 (design/2026-09-28-unit-reopening.md 3(c), row (b)): the spell-ID case labels
# still in switches, counted per file under src/game, are on src/tests/case_labels.txt; a new
# label fails, and a PR that moves labels into the registry lowers its lines in the same PR.
add_test(NAME case_labels
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -DCASE_LABELS_PYTHON=${Python3_EXECUTABLE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckCaseLabels.cmake)

add_test(NAME raw_rand
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -DRAW_RAND_PYTHON=${Python3_EXECUTABLE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckRawRand.cmake)

# Decoupling D5f (server #135), ratchet R-C: the campaign's downcast counter becomes a gate.
# subtype_sites is 0 after D5f and must stay 0 -- the `GetTypeId() == TYPEID_UNIT &&
# ((Creature*)x)->IsPet()` pattern is now a virtual call. The self-test entry keeps the
# counter itself honest, since the gate is only worth as much as its regexes. (Python3 is found
# above, for the layout gate.)
if(Python3_Interpreter_FOUND)
    add_test(NAME downcast_counter_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/downcast_count.py --self-test)

    add_test(NAME subtype_downcasts
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/downcast_count.py
            --gate subtype_sites=0 ${CMAKE_SOURCE_DIR}/src/game)

    # Decoupling D7a: the synchronous-call inventory is a hint generator, not a ratchet, so
    # only its own scanner is gated -- an inventory that miscounts is worse than none.
    add_test(NAME sync_db_sites_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/sync_db_sites.py --self-test)

    # Decoupling D4a: the data-member counter (M1) every D4 PR reports.
    add_test(NAME member_counter_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/member_count.py --self-test)

    # Decoupling D4b: the method counter (R1) the D4 finish line is measured with.
    add_test(NAME method_counter_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/method_count.py --self-test)

    # Decoupling D4k: a character manager never stores a callback. The check reads the managers
    # from CheckManagerIsolation.cmake's MANAGER_FILES and walks their headers with the method
    # counter's class-scope scanner; it runs its own self-test first, and alone here as well.
    add_test(NAME stored_callbacks_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/stored_callbacks.py --self-test)

    add_test(NAME stored_callbacks
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/stored_callbacks.py
            --source-root ${CMAKE_SOURCE_DIR})

    # Decoupling D4i: the forwarder rewrite drops the character's pure forwarders to its managers
    # and moves their callers onto the managers; its self-test keeps the purity and the
    # unique-name rules honest.
    add_test(NAME forwarder_rewrite_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/forwarder_rewrite.py --self-test)

    # The layout gate's fixtures: a planted upward include, a stale list line, an unclassified
    # file and the rest fail; an allowed edge and a system include pass.
    add_test(NAME layout_gate_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/layout_gate.py --self-test)

    # Decoupling D11: the case-label ratchet's fixtures (the lexer on a nested switch, another
    # key, a commented label; a planted label and a stale baseline fail), and the spell handler
    # registry's verbatim proof on its fixtures (a moved body pastes back byte for byte; a changed
    # line, a wrong or lost registration row, a bare live-out and a Continue inside a loop fail).
    # The proof itself, verbatim.py --check, reads the base commit from git and is run by the PR.
    add_test(NAME case_labels_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/case_labels.py --self-test)

    add_test(NAME verbatim_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/verbatim.py --self-test)

    add_test(NAME raw_rand_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/raw_rand.py --self-test)

    add_test(NAME cast_verbatim_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/cast_verbatim.py --self-test)

    add_test(NAME player_sinks_selftest
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/player_sinks.py --self-test)

    add_test(NAME player_sinks
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/player_sinks.py
            --root ${CMAKE_SOURCE_DIR} --check)
else()
    message(STATUS "Python3 interpreter not found: the downcast counter tests are skipped")
endif()

if(WIN32)
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
