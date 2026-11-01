file(GLOB SRC_GRP_DISPATCH dispatch/*.cpp dispatch/*.h)
source_group("Dispatch" FILES ${SRC_GRP_DISPATCH})

file(GLOB SRC_GRP_SURFACE surface/*.cpp surface/*.h surface/detail/*.h)
source_group("Surface" FILES ${SRC_GRP_SURFACE})

source_group("Commands" FILES ${GAME_SCRIPTS_SOURCES})

add_library(scripts STATIC ${SRC_GRP_DISPATCH} ${SRC_GRP_SURFACE} ${GAME_SCRIPTS_SOURCES})

target_include_directories(scripts
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/game/ChatCommands
        ${CMAKE_SOURCE_DIR}/src/game/dbscripts
)

if(MSVC)
    target_compile_options(scripts PRIVATE /permissive-)
endif()

target_link_libraries(scripts
    PUBLIC
        session
)

if(PCH)
    add_cxx_pch(scripts ${CMAKE_SOURCE_DIR}/src/game/pchscripts.h)
endif()

if(SCRIPT_LIB_SD3 AND BUILD_MANGOSD)
    add_subdirectory(legacy)
endif()
