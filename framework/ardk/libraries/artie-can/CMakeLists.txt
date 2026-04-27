cmake_minimum_required(VERSION 3.10)
project(artie-can VERSION 0.1.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_STATIC_LIBS "Build static libraries" ON)
option(BUILD_TESTS "Build tests" OFF)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/backend
    ${CMAKE_CURRENT_SOURCE_DIR}/include/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/include/util
)

# Source files
set(ARTIE_CAN_SOURCES
    src/backend/backend_mcp2515.c
    src/backend/backend_tcp.c
    src/protocol/rtacp.c
    src/util/circular_buffer.c
    src/util/translationlayer.c
    src/artie_can_core.c
)

# Header files
set(ARTIE_CAN_HEADERS
    include/backend/backend_mcp2515.h
    include/backend/backend_tcp.h
    include/protocol/rtacp_context.h
    include/protocol/rtacp.h
    include/util/circular_buffer.h
    include/util/log.h
    include/util/translationlayer.h
    include/artie_can.h
    include/backend.h
    include/context.h
    include/err.h
    include/frame.h
)

# Function to apply common compile options to library targets
function(set_artie_can_compile_options target)
    # Enable warnings and treat them as errors
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
        # Ignore the following warnings:
        # C4100: unreferenced formal parameter (common in callback implementations)
        target_compile_options(${target} PRIVATE /wd4100)
        # Disable warnings about unsafe functions like strncpy
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
        # Ignore the following warnings:
        # -Wunused-parameter: unreferenced formal parameter (common in callback implementations)
        target_compile_options(${target} PRIVATE -Wno-unused-parameter)
        # Ignore -Werror=stringop-truncation
        target_compile_options(${target} PRIVATE -Wno-error=stringop-truncation)
    endif()

    # Pass through logging enable
    if(ARTIE_CAN_LOGGING_ENABLED)
        target_compile_definitions(${target} PRIVATE ARTIE_CAN_LOGGING_ENABLED=1)
    endif()
endfunction()

# Function to link platform-specific libraries
function(link_artie_can_platform_libs target)
    if(WIN32)
        # Windows: link against Winsock2 for socket functions
        target_link_libraries(${target} PRIVATE ws2_32)
    else()
        # Unix/Linux: link against pthread for threading and m for math
        target_link_libraries(${target} PRIVATE pthread m)
    endif()
endfunction()

# Shared library
if(BUILD_SHARED_LIBS)
    add_library(artie_can SHARED ${ARTIE_CAN_SOURCES})
    set_target_properties(artie_can PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 0
        PUBLIC_HEADER "${ARTIE_CAN_HEADERS}"
    )

    # Apply common compile options
    set_artie_can_compile_options(artie_can)

    # Link platform-specific libraries
    link_artie_can_platform_libs(artie_can)

    # Install shared library
    install(TARGETS artie_can
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include/artie_can
    )
endif()

# Static library
if(BUILD_STATIC_LIBS)
    add_library(artie_can_static STATIC ${ARTIE_CAN_SOURCES})
    set_target_properties(artie_can_static PROPERTIES
        OUTPUT_NAME artie_can
        PUBLIC_HEADER "${ARTIE_CAN_HEADERS}"
    )

    # Apply common compile options
    set_artie_can_compile_options(artie_can_static)

    # Link platform-specific libraries
    link_artie_can_platform_libs(artie_can_static)

    # Install static library
    install(TARGETS artie_can_static
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include/artie_can
    )
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "Artie CAN Library Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  Build static libs: ${BUILD_STATIC_LIBS}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "")
