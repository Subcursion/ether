cmake_minimum_required(VERSION 3.24...3.31)

project(ether LANGUAGES C)

find_package(Vulkan REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

# Reconfigure automatically when C sources are added or removed.
file(GLOB_RECURSE ETHER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(ether ${ETHER_SOURCES})

set_target_properties(ether PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    # The empty generator expression prevents Debug/Release subdirectories
    # with multi-configuration generators. The justfile sets this to build/.
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<0:>"
)

target_include_directories(ether
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/pubinc"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc"
)

target_link_libraries(ether PRIVATE Vulkan::Vulkan SDL3::SDL3)

if(MSVC)
    target_compile_options(ether PRIVATE /Wall /WX)
elseif(CMAKE_C_COMPILER_ID MATCHES "^(AppleClang|Clang)$")
    target_compile_options(ether PRIVATE -Weverything -Werror -pedantic-errors)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # GCC has no equivalent to Clang's -Weverything.
    target_compile_options(ether PRIVATE
        -Wall -Wextra -Wpedantic -Werror -pedantic-errors
        -Wconversion -Wsign-conversion -Wshadow -Wformat=2
        -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition
        -Wcast-align=strict -Wcast-qual -Wwrite-strings -Wundef
        -Wdouble-promotion -Wnull-dereference -Wvla
        -Wswitch-enum -Wswitch-default -Wredundant-decls
    )
else()
    message(FATAL_ERROR "Strict warning flags are not configured for ${CMAKE_C_COMPILER_ID}")
endif()
