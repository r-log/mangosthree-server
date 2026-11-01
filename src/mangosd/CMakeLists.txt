set(SRC_GRP_MAIN
  AntiFreezeService.cpp
  AntiFreezeService.h
  CliService.cpp
  CliService.h
  LegacyScripts.cpp
  LegacyScripts.h
  Master.cpp
  Master.h
  Service.h
  mangosd.cpp
)
source_group("Main" FILES ${SRC_GRP_MAIN})

add_executable(mangosd
    ${SRC_GRP_MAIN}
    $<$<BOOL:${WIN32}>:${CMAKE_SOURCE_DIR}/cmake/win/VersionInfo.rc>
)

if(WIN32)
    target_compile_definitions(mangosd
        PRIVATE
            PRODUCT_ICON="${CMAKE_CURRENT_SOURCE_DIR}/mangosd.ico"
            PRODUCT_FILE_NAME="mangosd.exe"
            PRODUCT_FILE_DESCRIPTION="${MANGOS_PKG} World Server")
endif()

target_include_directories(mangosd
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(mangosd
    PRIVATE
        $<$<BOOL:${SCRIPT_LIB_SD3}>:ENABLE_SD3>
)

target_link_libraries(mangosd
    PRIVATE
        world
        proto
        scripts
        Threads::Threads
        $<$<BOOL:${SCRIPT_LIB_SD3}>:scripts_legacy>
)

install(
    TARGETS mangosd
    DESTINATION ${BIN_DIR}
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mangosd.conf.dist.in ${CMAKE_CURRENT_BINARY_DIR}/mangosd.conf.dist)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/mangosd.conf.dist
    DESTINATION ${CONF_INSTALL_DIR}
)

if(WIN32 AND MSVC)
    install(
        FILES $<TARGET_PDB_FILE:mangosd>
        DESTINATION ${BIN_DIR}
        OPTIONAL
        )
endif()
