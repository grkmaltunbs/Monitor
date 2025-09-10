# Integration Tests CMakeLists.txt

# Find Qt6 components needed for integration tests
find_package(Qt6 REQUIRED COMPONENTS Core Test Network Widgets)

# Enable Qt MOC processing for integration tests  
set(CMAKE_AUTOMOC ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Common libraries for all integration tests
set(INTEGRATION_LIBRARIES
    Qt6::Core
    Qt6::Test
    Qt6::Network
    Qt6::Widgets
    memory_pool
    event_dispatcher
    packet_factory
    simulation_source
    udp_source
    tcp_source
    network_config
    file_source
    file_indexer
    grid_widget
    grid_logger_widget
    test_framework_core
    test_framework_parser
    test_framework_execution
    gtest
)

# Test executable: test_system_integration
add_executable(test_system_integration
    test_system_integration.cpp
)

target_link_libraries(test_system_integration ${INTEGRATION_LIBRARIES})

set_target_properties(test_system_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/integration
)

add_test(NAME SystemIntegrationTest COMMAND test_system_integration)

# Test executable: test_packet_pipeline
add_executable(test_packet_pipeline
    test_packet_pipeline.cpp
)

target_link_libraries(test_packet_pipeline ${INTEGRATION_LIBRARIES})

set_target_properties(test_packet_pipeline PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/integration
)

add_test(NAME PacketPipelineTest COMMAND test_packet_pipeline)

# Test executable: test_network_integration
add_executable(test_network_integration
    test_network_integration.cpp
)

target_link_libraries(test_network_integration ${INTEGRATION_LIBRARIES})

set_target_properties(test_network_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/integration
)

add_test(NAME NetworkIntegrationTest COMMAND test_network_integration)

# Test executable: test_offline_integration
add_executable(test_offline_integration
    test_offline_integration.cpp
)

target_link_libraries(test_offline_integration ${INTEGRATION_LIBRARIES})

set_target_properties(test_offline_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/integration
)

add_test(NAME OfflineIntegrationTest COMMAND test_offline_integration)

# Test Framework Integration Test
add_executable(test_framework_integration
    test_framework_integration.cpp
)

target_link_libraries(test_framework_integration ${INTEGRATION_LIBRARIES})

set_target_properties(test_framework_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/integration
)

add_test(NAME TestFrameworkIntegrationTest COMMAND test_framework_integration)

# Set common test properties
set_tests_properties(
    SystemIntegrationTest
    PacketPipelineTest
    NetworkIntegrationTest
    OfflineIntegrationTest
    TestFrameworkIntegrationTest
    PROPERTIES
        TIMEOUT 120
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)