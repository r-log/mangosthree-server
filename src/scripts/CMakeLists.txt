file(GLOB SRC_GRP_DISPATCH dispatch/*.cpp dispatch/*.h)
source_group("Dispatch" FILES ${SRC_GRP_DISPATCH})

add_library(scripts STATIC ${SRC_GRP_DISPATCH})

target_include_directories(scripts
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(scripts
    PUBLIC
        game
)

if(SCRIPT_LIB_SD3 AND BUILD_MANGOSD)
    add_subdirectory(legacy)
endif()
