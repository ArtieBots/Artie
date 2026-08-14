cmake_minimum_required(VERSION 3.10)

# The integration-test node: one Artie CAN node per process, driven across containers by the
# can-integration-tests artie-tool task.
add_executable(itest_node itest_node.c)

target_include_directories(itest_node PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link against the static library if it exists, otherwise shared
if(TARGET artie_can_static)
    target_link_libraries(itest_node PRIVATE artie_can_static)
elseif(TARGET artie_can)
    target_link_libraries(itest_node PRIVATE artie_can)
else()
    message(FATAL_ERROR "No artie_can library target found")
endif()

# Same warning settings as the rest of the tree
set_artie_can_compile_options(itest_node)

# Sockets/threads/math, as the library itself needs
link_artie_can_platform_libs(itest_node)

# Deliberately NOT registered with add_test(): this is a long-running node (or a one-shot scenario
# driver) for the container-based integration tests, not a unit test. Registering it would make
# `ctest` in the test image hang waiting on a daemon.

message(STATUS "Integration test node will be built in: ${CMAKE_CURRENT_BINARY_DIR}")
