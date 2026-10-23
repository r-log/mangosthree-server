if(CMAKE_VERSION VERSION_LESS 3.19)
    message(FATAL_ERROR "MANGOS_PROOF_BUILD needs CMake 3.19 or newer.")
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "MANGOS_PROOF_BUILD needs GCC for C and C++.")
endif()

set(MANGOS_PROOF_OPTIONS
    -O0
    -g0
    -w
    -fno-inline
    -ffunction-sections
    -fdata-sections
    -fmerge-constants
    -fPIC
    -fno-ipa-stack-alignment
)

set(MANGOS_PROOF_DEFINITIONS
    "__FILE__=\"\""
    "__LINE__=0"
)

function(mangos_proof_apply dir)
    get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if(type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$")
            target_compile_options(${target} PRIVATE ${MANGOS_PROOF_OPTIONS})
            target_compile_definitions(${target} PRIVATE ${MANGOS_PROOF_DEFINITIONS})
        endif()
    endforeach()
    get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(subdir IN LISTS subdirs)
        mangos_proof_apply("${subdir}")
    endforeach()
endfunction()

function(mangos_proof_finish)
    if(PCH)
        message(FATAL_ERROR "MANGOS_PROOF_BUILD needs PCH=0.")
    endif()
    mangos_proof_apply("${CMAKE_SOURCE_DIR}")
endfunction()

cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL mangos_proof_finish)
