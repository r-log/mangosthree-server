file(GLOB SRC_GRP_AHBOT AuctionHouseBot/*.cpp AuctionHouseBot/*.h)
source_group("AhBot" FILES ${SRC_GRP_AHBOT})

file(GLOB SRC_GRP_APP app/*.cpp app/*.h)
source_group("App" FILES ${SRC_GRP_APP})

file(GLOB SRC_GRP_BATTLEGROUND BattleGround/*.cpp BattleGround/*.h)
source_group("BattleGround" FILES ${SRC_GRP_BATTLEGROUND})

file(GLOB SRC_GRP_CHAT_COMMANDS ChatCommands/*.cpp ChatCommands/*.h)
source_group("Chat Commands" FILES ${SRC_GRP_CHAT_COMMANDS})

file(GLOB SRC_GRP_COMBAT combat/*.cpp combat/*.h)
source_group("Combat" FILES ${SRC_GRP_COMBAT})

file(GLOB SRC_GRP_DATA data/*.cpp data/*.h)
source_group("Data" FILES ${SRC_GRP_DATA})

file(GLOB SRC_GRP_DATA_DBC data/dbc/*.cpp data/dbc/*.h)
source_group("Data\\DBC" FILES ${SRC_GRP_DATA_DBC})

file(GLOB SRC_GRP_DATA_WORLD data/world/*.cpp data/world/*.h)
source_group("Data\\World" FILES ${SRC_GRP_DATA_WORLD})

file(GLOB SRC_GRP_DBSCRIPTS dbscripts/*.cpp dbscripts/*.h)
source_group("Database Scripts" FILES ${SRC_GRP_DBSCRIPTS})

file(GLOB SRC_GRP_ENTITIES entities/*.cpp entities/*.h)
source_group("Entities" FILES ${SRC_GRP_ENTITIES})

file(GLOB SRC_GRP_ENTITIES_PLAYER entities/player/*.cpp entities/player/*.h)
source_group("Entities\\Player" FILES ${SRC_GRP_ENTITIES_PLAYER})

file(GLOB SRC_GRP_ENTITIES_PLAYER_COMBAT entities/player/combat/*.cpp entities/player/combat/*.h)
source_group("Entities\\Player\\Combat" FILES ${SRC_GRP_ENTITIES_PLAYER_COMBAT})

file(GLOB SRC_GRP_ENTITIES_PLAYER_INTERACTION entities/player/interaction/*.cpp entities/player/interaction/*.h)
source_group("Entities\\Player\\Interaction" FILES ${SRC_GRP_ENTITIES_PLAYER_INTERACTION})

file(GLOB SRC_GRP_ENTITIES_PLAYER_INVENTORY entities/player/inventory/*.cpp entities/player/inventory/*.h)
source_group("Entities\\Player\\Inventory" FILES ${SRC_GRP_ENTITIES_PLAYER_INVENTORY})

file(GLOB SRC_GRP_ENTITIES_PLAYER_PERSISTENCE entities/player/persistence/*.cpp entities/player/persistence/*.h)
source_group("Entities\\Player\\Persistence" FILES ${SRC_GRP_ENTITIES_PLAYER_PERSISTENCE})

file(GLOB SRC_GRP_ENTITIES_PLAYER_PETS entities/player/pets/*.cpp entities/player/pets/*.h)
source_group("Entities\\Player\\Pets" FILES ${SRC_GRP_ENTITIES_PLAYER_PETS})

file(GLOB SRC_GRP_ENTITIES_PLAYER_PVP entities/player/pvp/*.cpp entities/player/pvp/*.h)
source_group("Entities\\Player\\PvP" FILES ${SRC_GRP_ENTITIES_PLAYER_PVP})

file(GLOB SRC_GRP_ENTITIES_PLAYER_QUESTS entities/player/quests/*.cpp entities/player/quests/*.h)
source_group("Entities\\Player\\Quests" FILES ${SRC_GRP_ENTITIES_PLAYER_QUESTS})

file(GLOB SRC_GRP_ENTITIES_PLAYER_SOCIAL entities/player/social/*.cpp entities/player/social/*.h)
source_group("Entities\\Player\\Social" FILES ${SRC_GRP_ENTITIES_PLAYER_SOCIAL})

file(GLOB SRC_GRP_ENTITIES_PLAYER_SPELLS entities/player/spells/*.cpp entities/player/spells/*.h)
source_group("Entities\\Player\\Spells" FILES ${SRC_GRP_ENTITIES_PLAYER_SPELLS})

file(GLOB SRC_GRP_ENTITIES_PLAYER_TALENTS entities/player/talents/*.cpp entities/player/talents/*.h)
source_group("Entities\\Player\\Talents" FILES ${SRC_GRP_ENTITIES_PLAYER_TALENTS})

file(GLOB SRC_GRP_ENTITIES_PLAYER_WORLD entities/player/world/*.cpp entities/player/world/*.h)
source_group("Entities\\Player\\World" FILES ${SRC_GRP_ENTITIES_PLAYER_WORLD})

file(GLOB SRC_GRP_MAP_THREAD Maps/*.cpp Maps/*.h)
source_group("Map Threading" FILES ${SRC_GRP_MAP_THREAD})

file(GLOB SRC_GRP_HARNESS Harness/*.cpp Harness/*.h)
source_group("Harness" FILES ${SRC_GRP_HARNESS})

file(GLOB SRC_GRP_MOTION_GEN MotionGenerators/*.cpp MotionGenerators/*.h)
source_group("Motion generators" FILES ${SRC_GRP_MOTION_GEN})

file(GLOB SRC_GRP_OBJECT Object/*.cpp Object/*.h)
source_group("Object" FILES ${SRC_GRP_OBJECT})

file(GLOB SRC_GRP_OUTDOOR_PVP OutdoorPvP/*.cpp OutdoorPvP/*.h)
source_group("Outdoor PvP" FILES ${SRC_GRP_OUTDOOR_PVP})

file(GLOB SRC_GRP_PERSISTENCE persistence/*.cpp persistence/*.h)
source_group("Persistence" FILES ${SRC_GRP_PERSISTENCE})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS persistence/characters/*.cpp persistence/characters/*.h)
source_group("Persistence\\Characters" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_ACCOUNT persistence/characters/account/*.cpp persistence/characters/account/*.h)
source_group("Persistence\\Characters\\Account" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_ACCOUNT})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_ACHIEVEMENTS persistence/characters/achievements/*.cpp persistence/characters/achievements/*.h)
source_group("Persistence\\Characters\\Achievements" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_ACHIEVEMENTS})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_ECONOMY persistence/characters/economy/*.cpp persistence/characters/economy/*.h)
source_group("Persistence\\Characters\\Economy" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_ECONOMY})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_EVENTS persistence/characters/events/*.cpp persistence/characters/events/*.h)
source_group("Persistence\\Characters\\Events" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_EVENTS})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_INVENTORY persistence/characters/inventory/*.cpp persistence/characters/inventory/*.h)
source_group("Persistence\\Characters\\Inventory" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_INVENTORY})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_MAIL persistence/characters/mail/*.cpp persistence/characters/mail/*.h)
source_group("Persistence\\Characters\\Mail" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_MAIL})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_MAPS persistence/characters/maps/*.cpp persistence/characters/maps/*.h)
source_group("Persistence\\Characters\\Maps" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_MAPS})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_PETS persistence/characters/pets/*.cpp persistence/characters/pets/*.h)
source_group("Persistence\\Characters\\Pets" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_PETS})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_PVP persistence/characters/pvp/*.cpp persistence/characters/pvp/*.h)
source_group("Persistence\\Characters\\Pvp" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_PVP})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_QUESTS persistence/characters/quests/*.cpp persistence/characters/quests/*.h)
source_group("Persistence\\Characters\\Quests" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_QUESTS})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_SOCIAL persistence/characters/social/*.cpp persistence/characters/social/*.h)
source_group("Persistence\\Characters\\Social" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_SOCIAL})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_SPELLS persistence/characters/spells/*.cpp persistence/characters/spells/*.h)
source_group("Persistence\\Characters\\Spells" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_SPELLS})

file(GLOB SRC_GRP_PERSISTENCE_CHARACTERS_TALENTS persistence/characters/talents/*.cpp persistence/characters/talents/*.h)
source_group("Persistence\\Characters\\Talents" FILES ${SRC_GRP_PERSISTENCE_CHARACTERS_TALENTS})

file(GLOB SRC_GRP_PERSISTENCE_LOGIN persistence/login/*.cpp persistence/login/*.h)
source_group("Persistence\\Login" FILES ${SRC_GRP_PERSISTENCE_LOGIN})

file(GLOB SRC_GRP_PERSISTENCE_WORLD persistence/world/*.cpp persistence/world/*.h)
source_group("Persistence\\World" FILES ${SRC_GRP_PERSISTENCE_WORLD})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_ACHIEVEMENTS persistence/world/achievements/*.cpp persistence/world/achievements/*.h)
source_group("Persistence\\World\\Achievements" FILES ${SRC_GRP_PERSISTENCE_WORLD_ACHIEVEMENTS})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_AI persistence/world/ai/*.cpp persistence/world/ai/*.h)
source_group("Persistence\\World\\AI" FILES ${SRC_GRP_PERSISTENCE_WORLD_AI})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_CREATURES persistence/world/creatures/*.cpp persistence/world/creatures/*.h)
source_group("Persistence\\World\\Creatures" FILES ${SRC_GRP_PERSISTENCE_WORLD_CREATURES})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_ECONOMY persistence/world/economy/*.cpp persistence/world/economy/*.h)
source_group("Persistence\\World\\Economy" FILES ${SRC_GRP_PERSISTENCE_WORLD_ECONOMY})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_EVENTS persistence/world/events/*.cpp persistence/world/events/*.h)
source_group("Persistence\\World\\Events" FILES ${SRC_GRP_PERSISTENCE_WORLD_EVENTS})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_GAMEOBJECTS persistence/world/gameobjects/*.cpp persistence/world/gameobjects/*.h)
source_group("Persistence\\World\\Gameobjects" FILES ${SRC_GRP_PERSISTENCE_WORLD_GAMEOBJECTS})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_ITEMS persistence/world/items/*.cpp persistence/world/items/*.h)
source_group("Persistence\\World\\Items" FILES ${SRC_GRP_PERSISTENCE_WORLD_ITEMS})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_LOCALES persistence/world/locales/*.cpp persistence/world/locales/*.h)
source_group("Persistence\\World\\Locales" FILES ${SRC_GRP_PERSISTENCE_WORLD_LOCALES})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_MAPS persistence/world/maps/*.cpp persistence/world/maps/*.h)
source_group("Persistence\\World\\Maps" FILES ${SRC_GRP_PERSISTENCE_WORLD_MAPS})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_NPCS persistence/world/npcs/*.cpp persistence/world/npcs/*.h)
source_group("Persistence\\World\\NPCs" FILES ${SRC_GRP_PERSISTENCE_WORLD_NPCS})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_PLAYERS persistence/world/players/*.cpp persistence/world/players/*.h)
source_group("Persistence\\World\\Players" FILES ${SRC_GRP_PERSISTENCE_WORLD_PLAYERS})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_POOLS persistence/world/pools/*.cpp persistence/world/pools/*.h)
source_group("Persistence\\World\\Pools" FILES ${SRC_GRP_PERSISTENCE_WORLD_POOLS})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_PVP persistence/world/pvp/*.cpp persistence/world/pvp/*.h)
source_group("Persistence\\World\\Pvp" FILES ${SRC_GRP_PERSISTENCE_WORLD_PVP})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_QUESTS persistence/world/quests/*.cpp persistence/world/quests/*.h)
source_group("Persistence\\World\\Quests" FILES ${SRC_GRP_PERSISTENCE_WORLD_QUESTS})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_REPUTATION persistence/world/reputation/*.cpp persistence/world/reputation/*.h)
source_group("Persistence\\World\\Reputation" FILES ${SRC_GRP_PERSISTENCE_WORLD_REPUTATION})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_SPAWNS persistence/world/spawns/*.cpp persistence/world/spawns/*.h)
source_group("Persistence\\World\\Spawns" FILES ${SRC_GRP_PERSISTENCE_WORLD_SPAWNS})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_SPELLS persistence/world/spells/*.cpp persistence/world/spells/*.h)
source_group("Persistence\\World\\Spells" FILES ${SRC_GRP_PERSISTENCE_WORLD_SPELLS})

file(GLOB SRC_GRP_PERSISTENCE_WORLD_WAYPOINTS persistence/world/waypoints/*.cpp persistence/world/waypoints/*.h)
source_group("Persistence\\World\\Waypoints" FILES ${SRC_GRP_PERSISTENCE_WORLD_WAYPOINTS})

add_library(persistence STATIC
    ${SRC_GRP_PERSISTENCE}
    ${SRC_GRP_PERSISTENCE_CHARACTERS}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_ACCOUNT}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_ACHIEVEMENTS}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_ECONOMY}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_EVENTS}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_INVENTORY}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_MAIL}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_MAPS}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_PETS}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_PVP}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_QUESTS}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_SOCIAL}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_SPELLS}
    ${SRC_GRP_PERSISTENCE_CHARACTERS_TALENTS}
    ${SRC_GRP_PERSISTENCE_LOGIN}
    ${SRC_GRP_PERSISTENCE_WORLD}
    ${SRC_GRP_PERSISTENCE_WORLD_ACHIEVEMENTS}
    ${SRC_GRP_PERSISTENCE_WORLD_AI}
    ${SRC_GRP_PERSISTENCE_WORLD_CREATURES}
    ${SRC_GRP_PERSISTENCE_WORLD_ECONOMY}
    ${SRC_GRP_PERSISTENCE_WORLD_EVENTS}
    ${SRC_GRP_PERSISTENCE_WORLD_GAMEOBJECTS}
    ${SRC_GRP_PERSISTENCE_WORLD_ITEMS}
    ${SRC_GRP_PERSISTENCE_WORLD_LOCALES}
    ${SRC_GRP_PERSISTENCE_WORLD_MAPS}
    ${SRC_GRP_PERSISTENCE_WORLD_NPCS}
    ${SRC_GRP_PERSISTENCE_WORLD_PLAYERS}
    ${SRC_GRP_PERSISTENCE_WORLD_POOLS}
    ${SRC_GRP_PERSISTENCE_WORLD_PVP}
    ${SRC_GRP_PERSISTENCE_WORLD_QUESTS}
    ${SRC_GRP_PERSISTENCE_WORLD_REPUTATION}
    ${SRC_GRP_PERSISTENCE_WORLD_SPAWNS}
    ${SRC_GRP_PERSISTENCE_WORLD_SPELLS}
    ${SRC_GRP_PERSISTENCE_WORLD_WAYPOINTS}
)

target_include_directories(persistence
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
    target_compile_options(persistence PRIVATE /permissive-)
endif()

target_link_libraries(persistence
    PUBLIC
        shared_db
)

file(GLOB SRC_GRP_SESSION session/*.cpp session/*.h)
source_group("Session" FILES ${SRC_GRP_SESSION})

file(GLOB SRC_GRP_SESSION_HANDLERS_COMBAT session/handlers/combat/*.cpp session/handlers/combat/*.h)
source_group("Session\\Handlers\\Combat" FILES ${SRC_GRP_SESSION_HANDLERS_COMBAT})

file(GLOB SRC_GRP_SESSION_HANDLERS_CONNECTION session/handlers/connection/*.cpp session/handlers/connection/*.h)
source_group("Session\\Handlers\\Connection" FILES ${SRC_GRP_SESSION_HANDLERS_CONNECTION})

file(GLOB SRC_GRP_SESSION_HANDLERS_ECONOMY session/handlers/economy/*.cpp session/handlers/economy/*.h)
source_group("Session\\Handlers\\Economy" FILES ${SRC_GRP_SESSION_HANDLERS_ECONOMY})

file(GLOB SRC_GRP_SESSION_HANDLERS_ENTITIES session/handlers/entities/*.cpp session/handlers/entities/*.h)
source_group("Session\\Handlers\\Entities" FILES ${SRC_GRP_SESSION_HANDLERS_ENTITIES})

file(GLOB SRC_GRP_SESSION_HANDLERS_MAPS session/handlers/maps/*.cpp session/handlers/maps/*.h)
source_group("Session\\Handlers\\Maps" FILES ${SRC_GRP_SESSION_HANDLERS_MAPS})

file(GLOB SRC_GRP_SESSION_HANDLERS_PVP session/handlers/pvp/*.cpp session/handlers/pvp/*.h)
source_group("Session\\Handlers\\Pvp" FILES ${SRC_GRP_SESSION_HANDLERS_PVP})

file(GLOB SRC_GRP_SESSION_HANDLERS_SOCIAL session/handlers/social/*.cpp session/handlers/social/*.h)
source_group("Session\\Handlers\\Social" FILES ${SRC_GRP_SESSION_HANDLERS_SOCIAL})

file(GLOB SRC_GRP_SESSION_HANDLERS_SPELLS session/handlers/spells/*.cpp session/handlers/spells/*.h)
source_group("Session\\Handlers\\Spells" FILES ${SRC_GRP_SESSION_HANDLERS_SPELLS})

file(GLOB SRC_GRP_SESSION_PACKETS session/packets/*.cpp session/packets/*.h)
source_group("Session\\Packets" FILES ${SRC_GRP_SESSION_PACKETS})

file(GLOB SRC_GRP_SESSION_PACKETS_CHAT session/packets/chat/*.cpp session/packets/chat/*.h)
source_group("Session\\Packets\\Chat" FILES ${SRC_GRP_SESSION_PACKETS_CHAT})

file(GLOB SRC_GRP_SESSION_PACKETS_COMBAT session/packets/combat/*.cpp session/packets/combat/*.h)
source_group("Session\\Packets\\Combat" FILES ${SRC_GRP_SESSION_PACKETS_COMBAT})

file(GLOB SRC_GRP_SESSION_PACKETS_ECONOMY session/packets/economy/*.cpp session/packets/economy/*.h)
source_group("Session\\Packets\\Economy" FILES ${SRC_GRP_SESSION_PACKETS_ECONOMY})

file(GLOB SRC_GRP_SESSION_PACKETS_ENTITIES session/packets/entities/*.cpp session/packets/entities/*.h)
source_group("Session\\Packets\\Entities" FILES ${SRC_GRP_SESSION_PACKETS_ENTITIES})

file(GLOB SRC_GRP_SESSION_PACKETS_MAPS session/packets/maps/*.cpp session/packets/maps/*.h)
source_group("Session\\Packets\\Maps" FILES ${SRC_GRP_SESSION_PACKETS_MAPS})

file(GLOB SRC_GRP_SESSION_PACKETS_PVP session/packets/pvp/*.cpp session/packets/pvp/*.h)
source_group("Session\\Packets\\Pvp" FILES ${SRC_GRP_SESSION_PACKETS_PVP})

file(GLOB SRC_GRP_SESSION_PACKETS_QUESTS session/packets/quests/*.cpp session/packets/quests/*.h)
source_group("Session\\Packets\\Quests" FILES ${SRC_GRP_SESSION_PACKETS_QUESTS})

file(GLOB SRC_GRP_SESSION_PACKETS_SOCIAL session/packets/social/*.cpp session/packets/social/*.h)
source_group("Session\\Packets\\Social" FILES ${SRC_GRP_SESSION_PACKETS_SOCIAL})

file(GLOB SRC_GRP_SESSION_PACKETS_SPELLS session/packets/spells/*.cpp session/packets/spells/*.h)
source_group("Session\\Packets\\Spells" FILES ${SRC_GRP_SESSION_PACKETS_SPELLS})

file(GLOB SRC_GRP_SOCIAL social/*.cpp social/*.h)
source_group("Social" FILES ${SRC_GRP_SOCIAL})

file(GLOB SRC_GRP_SPELLS spells/*.cpp spells/*.h)
source_group("Spells" FILES ${SRC_GRP_SPELLS})

file(GLOB SRC_GRP_SPELLS_AURAS spells/auras/*.cpp spells/auras/*.h)
source_group("Spells\\Auras" FILES ${SRC_GRP_SPELLS_AURAS})

file(GLOB SRC_GRP_SPELLS_DATA spells/data/*.cpp spells/data/*.h)
source_group("Spells\\Data" FILES ${SRC_GRP_SPELLS_DATA})

file(GLOB SRC_GRP_SPELLS_HANDLERS spells/handlers/*.cpp spells/handlers/*.h)
source_group("Spells\\Handlers" FILES ${SRC_GRP_SPELLS_HANDLERS})

file(GLOB SRC_GRP_SPELLS_PIPELINE spells/pipeline/*.cpp spells/pipeline/*.h)
source_group("Spells\\Pipeline" FILES ${SRC_GRP_SPELLS_PIPELINE})

add_library(data STATIC
    ${SRC_GRP_DATA}
    ${SRC_GRP_DATA_DBC}
    ${SRC_GRP_DATA_WORLD}
    ${SRC_GRP_SPELLS_DATA}
)

target_include_directories(data
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        data/dbc
        data/world
)

if(MSVC)
    target_compile_options(data PRIVATE /permissive-)
endif()

target_link_libraries(data
    PUBLIC
        persistence
)

set(DOMAIN_SOURCES
    WorldHandlers/AchievementCriteriaRequirement.cpp
    WorldHandlers/AchievementGlobalMgr.cpp
    WorldHandlers/AchievementMgr.cpp
    WorldHandlers/AchievementMgr.h
    WorldHandlers/GossipDef.cpp
    WorldHandlers/GossipDef.h
    WorldHandlers/SkillDiscovery.cpp
    WorldHandlers/SkillDiscovery.h
    WorldHandlers/SkillExtraItems.cpp
    WorldHandlers/SkillExtraItems.h
    WorldHandlers/UpdateData.cpp
    WorldHandlers/UpdateData.h
)
list(TRANSFORM DOMAIN_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)

add_library(domain STATIC
    ${SRC_GRP_BATTLEGROUND}
    ${SRC_GRP_COMBAT}
    ${SRC_GRP_ENTITIES}
    ${SRC_GRP_ENTITIES_PLAYER}
    ${SRC_GRP_ENTITIES_PLAYER_COMBAT}
    ${SRC_GRP_ENTITIES_PLAYER_INTERACTION}
    ${SRC_GRP_ENTITIES_PLAYER_INVENTORY}
    ${SRC_GRP_ENTITIES_PLAYER_PERSISTENCE}
    ${SRC_GRP_ENTITIES_PLAYER_PETS}
    ${SRC_GRP_ENTITIES_PLAYER_PVP}
    ${SRC_GRP_ENTITIES_PLAYER_QUESTS}
    ${SRC_GRP_ENTITIES_PLAYER_SOCIAL}
    ${SRC_GRP_ENTITIES_PLAYER_SPELLS}
    ${SRC_GRP_ENTITIES_PLAYER_TALENTS}
    ${SRC_GRP_ENTITIES_PLAYER_WORLD}
    ${SRC_GRP_MAP_THREAD}
    ${SRC_GRP_MOTION_GEN}
    ${SRC_GRP_OBJECT}
    ${SRC_GRP_OUTDOOR_PVP}
    ${SRC_GRP_SOCIAL}
    ${SRC_GRP_SPELLS}
    ${SRC_GRP_SPELLS_AURAS}
    ${SRC_GRP_SPELLS_HANDLERS}
    ${SRC_GRP_SPELLS_PIPELINE}
    ${DOMAIN_SOURCES}
)

target_include_directories(domain
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        BattleGround
        ${CMAKE_CURRENT_SOURCE_DIR}/combat
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/player
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/player/combat
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/player/interaction
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/player/inventory
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/player/persistence
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/player/pets
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/player/pvp
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/player/quests
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/player/social
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/player/spells
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/player/talents
        ${CMAKE_CURRENT_SOURCE_DIR}/entities/player/world
        Maps
        MotionGenerators
        Object
        OutdoorPvP
        WorldHandlers
        social
)

if(MSVC)
    target_compile_options(domain PRIVATE /permissive-)
endif()

target_link_libraries(domain
    PUBLIC
        data
        persistence
        terrain
        geometry
        shared_db
        motion
        RecastNavigation::Detour
        ZLIB::ZLIB
)

if(PCH)
    add_cxx_pch(domain pchdomain.h)
endif()

add_library(session STATIC
    ${SRC_GRP_SESSION}
    ${SRC_GRP_SESSION_HANDLERS_COMBAT}
    ${SRC_GRP_SESSION_HANDLERS_CONNECTION}
    ${SRC_GRP_SESSION_HANDLERS_ECONOMY}
    ${SRC_GRP_SESSION_HANDLERS_ENTITIES}
    ${SRC_GRP_SESSION_HANDLERS_MAPS}
    ${SRC_GRP_SESSION_HANDLERS_PVP}
    ${SRC_GRP_SESSION_HANDLERS_SOCIAL}
    ${SRC_GRP_SESSION_HANDLERS_SPELLS}
    ${SRC_GRP_SESSION_PACKETS}
    ${SRC_GRP_SESSION_PACKETS_CHAT}
    ${SRC_GRP_SESSION_PACKETS_COMBAT}
    ${SRC_GRP_SESSION_PACKETS_ECONOMY}
    ${SRC_GRP_SESSION_PACKETS_ENTITIES}
    ${SRC_GRP_SESSION_PACKETS_MAPS}
    ${SRC_GRP_SESSION_PACKETS_PVP}
    ${SRC_GRP_SESSION_PACKETS_QUESTS}
    ${SRC_GRP_SESSION_PACKETS_SOCIAL}
    ${SRC_GRP_SESSION_PACKETS_SPELLS}
)

target_include_directories(session
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        session
)

if(MSVC)
    target_compile_options(session PRIVATE /permissive-)
endif()

target_link_libraries(session
    PUBLIC
        domain
        proto
)

if(PCH)
    add_cxx_pch(session pchsession.h)
endif()

set(GAME_SCRIPTS_SOURCES ${SRC_GRP_CHAT_COMMANDS} ${SRC_GRP_DBSCRIPTS} PARENT_SCOPE)

configure_file(AuctionHouseBot/ahbot.conf.dist.in ${CMAKE_CURRENT_BINARY_DIR}/AuctionHouseBot/ahbot.conf.dist)

add_library(world STATIC
    ${SRC_GRP_AHBOT}
    ${SRC_GRP_APP}
    ${SRC_GRP_HARNESS}
)

target_include_directories(world
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        AuctionHouseBot
        Harness
        app
)

if(MSVC)
    target_compile_options(world PRIVATE /permissive-)
endif()

target_link_libraries(world
    PUBLIC
        scripts
        session
        domain
        data
        persistence
        terrain
        geometry
        shared_db
        proto
        motion
        RecastNavigation::Detour
        ZLIB::ZLIB
)

if(PCH)
    add_cxx_pch(world pchworld.h)
endif()

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/AuctionHouseBot/ahbot.conf.dist
    DESTINATION ${CONF_INSTALL_DIR}
)
