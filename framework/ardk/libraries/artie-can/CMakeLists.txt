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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(ARTIE_CAN_SOURCES
    src/artie_can_core.c
    src/artie_can_utils.c
    src/artie_can_rtacp.c
    src/artie_can_rpcacp.c
    src/artie_can_psacp.c
    src/artie_can_bwacp.c
    src/artie_can_backend_mock.c
    src/artie_can_backend_socketcan.c
    src/artie_can_backend_mcp2515.c
)

# Header files
set(ARTIE_CAN_HEADERS
    include/artie_can.h
    include/artie_can_backends.h
)

# Shared library
if(BUILD_SHARED_LIBS)
    add_library(artie_can SHARED ${ARTIE_CAN_SOURCES})
    set_target_properties(artie_can PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 0
        PUBLIC_HEADER "${ARTIE_CAN_HEADERS}"
    )

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
