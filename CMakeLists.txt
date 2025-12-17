# EDN.C - CMake build configuration
# Cross-platform build support for Unix, macOS, and Windows
cmake_minimum_required(VERSION 3.10)
project(edn C)

# C11 standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Optional features (disabled by default)
option(EDN_ENABLE_MAP_NAMESPACE_SYNTAX "Enable map namespace syntax (#:ns{...})" OFF)
option(EDN_ENABLE_EXTENDED_CHARACTERS "Enable extended characters (\\formfeed, \\backspace, \\oNNN)" OFF)
option(EDN_ENABLE_METADATA "Enable metadata parsing (^{...} form)" OFF)
option(EDN_ENABLE_TEXT_BLOCKS "Enable text block parsing (Java-style \"\"\" blocks)" OFF)
option(EDN_ENABLE_RATIO "Enable ratio numbers (22/7)" OFF)
option(EDN_ENABLE_EXTENDED_INTEGERS "Enable hex (0xFF), octal (0777), binary (2r1010), and radix (36rZZ) integers" OFF)
option(EDN_ENABLE_UNDERSCORE_IN_NUMERIC "Enable underscores in numeric literals (1_000 -> 1000)" OFF)
option(EDN_ENABLE_DEBUG "Enable debug build with sanitizers" OFF)

# Apply feature flags
if(EDN_ENABLE_MAP_NAMESPACE_SYNTAX)
    add_compile_definitions(EDN_ENABLE_MAP_NAMESPACE_SYNTAX)
endif()

if(EDN_ENABLE_EXTENDED_CHARACTERS)
    add_compile_definitions(EDN_ENABLE_EXTENDED_CHARACTERS)
endif()

if(EDN_ENABLE_METADATA)
    add_compile_definitions(EDN_ENABLE_METADATA)
endif()

if(EDN_ENABLE_TEXT_BLOCKS)
    add_compile_definitions(EDN_ENABLE_TEXT_BLOCKS)
endif()

if(EDN_ENABLE_RATIO)
    add_compile_definitions(EDN_ENABLE_RATIO)
endif()

if(EDN_ENABLE_EXTENDED_INTEGERS)
    add_compile_definitions(EDN_ENABLE_EXTENDED_INTEGERS)
endif()

if(EDN_ENABLE_UNDERSCORE_IN_NUMERIC)
    add_compile_definitions(EDN_ENABLE_UNDERSCORE_IN_NUMERIC)
endif()

# Compiler flags
if(MSVC)
    # MSVC compiler flags
    add_compile_options(/W4)
    # Debug vs Release
    if(EDN_ENABLE_DEBUG)
        add_compile_options(/Od /Zi)
        add_compile_definitions(DEBUG)
        # AddressSanitizer support for MSVC
        add_compile_options(/fsanitize=address)
        # Disable runtime checks that conflict with ASAN
        string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    else()
        add_compile_options(/O2)
        add_compile_definitions(NDEBUG)
    endif()
    # Enable SSE4.2 on x86_64 Windows
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        add_compile_options(/arch:AVX)
    endif()
    # Disable specific MSVC warnings that are too strict
    add_compile_options(/wd4100)  # unreferenced formal parameter
else()
    # GCC/Clang flags
    add_compile_options(-Wall -Wextra -Wpedantic)
    # Debug vs Release
    if(EDN_ENABLE_DEBUG)
        add_compile_options(-g -O0)
        add_compile_definitions(DEBUG)
        # Sanitizers for debug build
        add_compile_options(-fsanitize=address,leak,undefined)
        add_link_options(-fsanitize=address,leak,undefined)
    else()
        add_compile_options(-O2)
        add_compile_definitions(NDEBUG)
    endif()
    # Enable SSE4.2 on x86_64
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        add_compile_options(-msse4.2)
    endif()
endif()

# Include directories
include_directories(include src)

# Source files
set(SOURCES
    src/edn.c
    src/arena.c
    src/simd.c
    src/string.c
    src/number.c
    src/character.c
    src/identifier.c
    src/symbolic.c
    src/equality.c
    src/uniqueness.c
    src/collection.c
    src/tagged.c
    src/discard.c
    src/reader.c
    src/metadata.c
    src/newline_finder.c
)

# Library target
add_library(edn STATIC ${SOURCES})

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(edn PUBLIC m)
endif()

# Test executables
file(GLOB TEST_SOURCES "test/*.c")
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} edn)
endforeach()

# Benchmark executables
file(GLOB BENCH_SOURCES "bench/*.c")
foreach(BENCH_SOURCE ${BENCH_SOURCES})
    get_filename_component(BENCH_NAME ${BENCH_SOURCE} NAME_WE)
    add_executable(${BENCH_NAME} ${BENCH_SOURCE})
    target_link_libraries(${BENCH_NAME} edn)
    # Set maximum optimization for benchmarks
    if(MSVC)
        target_compile_options(${BENCH_NAME} PRIVATE /O2)
    else()
        target_compile_options(${BENCH_NAME} PRIVATE -O3)
    endif()
    target_compile_definitions(${BENCH_NAME} PRIVATE _POSIX_C_SOURCE=200809L)
endforeach()

# Example executables
file(GLOB EXAMPLE_SOURCES "examples/*.c")
# Exclude Unix-specific examples on Windows
if(WIN32)
    list(FILTER EXAMPLE_SOURCES EXCLUDE REGEX "edn_cli\\.c$")
    list(FILTER EXAMPLE_SOURCES EXCLUDE REGEX "edn_tui\\.c$")
endif()
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    target_link_libraries(${EXAMPLE_NAME} edn)
endforeach()

# Install targets
install(TARGETS edn DESTINATION lib)
install(FILES include/edn.h DESTINATION include)

# Print configuration
message(STATUS "EDN.C Build Configuration:")
message(STATUS "  System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Compiler: ${CMAKE_C_COMPILER_ID}")
message(STATUS "  Debug mode: ${EDN_ENABLE_DEBUG}")
message(STATUS "")
message(STATUS "Optional features:")
message(STATUS "  MAP_NAMESPACE_SYNTAX:     ${EDN_ENABLE_MAP_NAMESPACE_SYNTAX}")
message(STATUS "  EXTENDED_CHARACTERS:      ${EDN_ENABLE_EXTENDED_CHARACTERS}")
message(STATUS "  METADATA:                 ${EDN_ENABLE_METADATA}")
message(STATUS "  TEXT_BLOCKS:              ${EDN_ENABLE_TEXT_BLOCKS}")
message(STATUS "  RATIO:                    ${EDN_ENABLE_RATIO}")
message(STATUS "  EXTENDED_INTEGERS:        ${EDN_ENABLE_EXTENDED_INTEGERS}")
message(STATUS "  UNDERSCORE_IN_NUMERIC:    ${EDN_ENABLE_UNDERSCORE_IN_NUMERIC}")
