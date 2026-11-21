cmake_minimum_required(VERSION 3.24...3.31)

project(ether LANGUAGES C)

find_package(Vulkan REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

# Reconfigure automatically when C sources are added or removed.
file(GLOB_RECURSE ETHER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(ether ${ETHER_SOURCES})

# Keep source paths in logs/debug information independent of the checkout path.
# MSVC does not accept this GCC/Clang option; unsupported compilers omit it.
if(NOT MSVC)
    include(CheckCCompilerFlag)
    set(ETHER_FILE_PREFIX_MAP "-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=")
    check_c_compiler_flag("${ETHER_FILE_PREFIX_MAP}" ETHER_HAS_FILE_PREFIX_MAP)
    if(ETHER_HAS_FILE_PREFIX_MAP)
        target_compile_options(ether PRIVATE "${ETHER_FILE_PREFIX_MAP}")
    endif()
endif()

set_target_properties(ether PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

target_include_directories(ether
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/pubinc"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)

target_link_libraries(ether PRIVATE Vulkan::Vulkan SDL3::SDL3 ${CMAKE_DL_LIBS})
set_target_properties(ether PROPERTIES ENABLE_EXPORTS ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_path(ETHER_LIBDW_INCLUDE_DIR elfutils/libdwfl.h)
    find_library(ETHER_LIBDW_LIBRARY dw)
    if(ETHER_LIBDW_INCLUDE_DIR AND ETHER_LIBDW_LIBRARY)
        target_include_directories(ether SYSTEM PRIVATE "${ETHER_LIBDW_INCLUDE_DIR}")
        target_link_libraries(ether PRIVATE "${ETHER_LIBDW_LIBRARY}")
        target_compile_definitions(ether PRIVATE ETHER_PROFILER_LIBDW)
    endif()
endif()

# add cglm
target_include_directories(ether SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/cglm/include"
)

target_compile_definitions(ether PRIVATE
    _POSIX_C_SOURCE=199309L
    _GNU_SOURCE
    CGLM_FORCE_DEPTH_ZERO_TO_ONE
)

# C math functions on Linux.
if(UNIX AND NOT APPLE)
    target_link_libraries(ether PRIVATE m)
endif()

if(MSVC)
    target_compile_options(ether PRIVATE /Wall /WX)
elseif(CMAKE_C_COMPILER_ID MATCHES "^(AppleClang|Clang)$")
    target_compile_options(ether PRIVATE -Weverything -Werror -pedantic-errors
    -finstrument-functions -finstrument-functions-exclude-file-list=vk_enum_string_helper.h)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # GCC has no equivalent to Clang's -Weverything.
    target_compile_options(ether PRIVATE
        -Wall -Wextra -Wpedantic -Werror -pedantic-errors
        -Wconversion -Wsign-conversion -Wshadow -Wformat=2
        -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition
        -Wcast-align=strict -Wcast-qual -Wwrite-strings -Wundef
        -Wdouble-promotion -Wnull-dereference -Wvla
        -Wswitch-enum -Wswitch-default -Wredundant-decls
        -finstrument-functions
        -finstrument-functions-exclude-file-list=vk_enum_string_helper.h
    )
else()
    message(FATAL_ERROR "Strict warning flags are not configured for ${CMAKE_C_COMPILER_ID}")
endif()
