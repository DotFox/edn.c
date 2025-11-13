# EDN.C - CMake build configuration
# Cross-platform build support for Unix, macOS, and Windows
cmake_minimum_required(VERSION 3.10)
project(edn C)

# C11 standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
if(MSVC)
    # MSVC compiler flags
    add_compile_options(/W4 /WX- /O2)
    # Enable SSE4.2 on x86_64 Windows
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        add_compile_options(/arch:AVX)
    endif()
    # Disable specific MSVC warnings that are too strict
    add_compile_options(/wd4100)  # unreferenced formal parameter
else()
    # GCC/Clang flags
    add_compile_options(-Wall -Wextra -Wpedantic -O2)
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
)

# Library target
add_library(edn STATIC ${SOURCES})

# Test executables
file(GLOB TEST_SOURCES "test/*.c")
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} edn)
    if(NOT MSVC)
        # Add sanitizers for non-MSVC debug builds
        if(CMAKE_BUILD_TYPE MATCHES Debug)
            target_compile_options(${TEST_NAME} PRIVATE -fsanitize=address,undefined)
            target_link_options(${TEST_NAME} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
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

# Install targets
install(TARGETS edn DESTINATION lib)
install(FILES include/edn.h DESTINATION include)

# Print configuration
message(STATUS "EDN.C Build Configuration:")
message(STATUS "  System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Compiler: ${CMAKE_C_COMPILER_ID}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
