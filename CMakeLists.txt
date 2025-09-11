cmake_minimum_required(VERSION 3.16)

project(Gorkemv5 VERSION 0.1 LANGUAGES CXX)

# Configure build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Include Phase 12 performance optimizations
include(optimizations/optimized_cmake_flags.cmake)

# Find required packages
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Widgets Test Charts Svg Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Test Charts Svg Network Concurrent)

# Try to find Qt 3D components (might be named differently)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS 3DCore 3DRender 3DExtras)
if(Qt63DCore_FOUND)
    message(STATUS "Found Qt 3D modules")
    set(QT3D_AVAILABLE TRUE)
else()
    # Try alternative naming
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Quick3D QUIET)
    if(Qt6_Quick3D_FOUND)
        message(STATUS "Found Qt Quick 3D")
        set(QT3D_AVAILABLE TRUE)
    else()
        message(WARNING "Qt 3D modules not found - 3D features will be disabled")
        set(QT3D_AVAILABLE FALSE)
    endif()
endif()

# Find nlohmann/json for JSON serialization (optional for now)
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found - JSON serialization will be disabled")
    set(HAVE_NLOHMANN_JSON FALSE)
else()
    set(HAVE_NLOHMANN_JSON TRUE)
    message(STATUS "Found nlohmann/json")
endif()

# Add include directories
include_directories(include)
include_directories(src)

# Compiler-specific flags
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-gnu-zero-variadic-macro-arguments)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0 -fsanitize=address -fsanitize=undefined)
        add_link_options(-fsanitize=address -fsanitize=undefined)
    else()
        add_compile_options(-O3 -DNDEBUG)
    endif()
endif()

# Parser library sources
set(PARSER_SOURCES
    # Lexer components
    src/parser/lexer/tokenizer.h
    src/parser/lexer/preprocessor.h
    src/parser/lexer/token_types.h
    src/parser/lexer/token_types.cpp

    # AST components
    src/parser/ast/ast_nodes.h
    src/parser/ast/ast_nodes.cpp
    src/parser/ast/ast_visitor.h
    src/parser/ast/ast_builder.h

    # Parser components
    src/parser/parser/struct_parser.h
    src/parser/parser/struct_parser.cpp

    # Layout components
    src/parser/layout/alignment_rules.h
    src/parser/layout/layout_calculator.h
    src/parser/layout/layout_calculator.cpp
    src/parser/layout/bitfield_handler.h

    # Serialization components
    src/parser/serialization/json_mock.h
    src/parser/serialization/json_serializer.h
    src/parser/serialization/json_deserializer.h

    # Manager components
    src/parser/manager/structure_cache.h
    src/parser/manager/structure_manager.h
    src/parser/manager/structure_manager.cpp
)

# Phase 3 Threading & Concurrency sources
set(THREADING_SOURCES
    # Threading infrastructure
    src/threading/thread_worker.h
    src/threading/thread_worker.cpp
    src/threading/thread_pool.h
    src/threading/thread_pool.cpp
    src/threading/thread_manager.h
    src/threading/thread_manager.cpp

    # Concurrent data structures
    src/concurrent/spsc_ring_buffer.h
    src/concurrent/mpsc_ring_buffer.h

    # Messaging framework
    src/messaging/message.h
    src/messaging/message.cpp
    src/messaging/message_channel.h
    src/messaging/message_channel.cpp
    src/messaging/message_bus.h
    src/messaging/message_bus.cpp

    # Synchronization primitives (to be implemented)
    # src/sync/read_write_lock.h
    # src/sync/barrier.h
    # src/sync/latch.h

    # Async framework (to be implemented)
    # src/async/future.h
    # src/async/promise.h
    # src/async/async_task.h
)

# Phase 4 Packet Processing sources
set(PACKET_SOURCES
    # Core packet infrastructure
    src/packet/core/packet_header.h
    src/packet/core/packet_buffer.h
    src/packet/core/packet.h
    src/packet/core/packet.cpp
    src/packet/core/packet_factory.h
    src/packet/core/packet_factory.cpp

    # Packet sources
    src/packet/sources/packet_source.h
    src/packet/sources/simulation_source.h
    src/packet/sources/simulation_source.cpp
    src/packet/sources/memory_source.h
    
    # Test packet structures
    src/packet/sources/test_packet_structures.h

    # Routing and distribution
    src/packet/routing/subscription_manager.h
    src/packet/routing/packet_router.h
    src/packet/routing/packet_dispatcher.h

    # Processing pipeline
    src/packet/processing/field_extractor.h
    src/packet/processing/data_transformer.h
    src/packet/processing/statistics_calculator.h
    src/packet/processing/packet_processor.h

    # Integration layer
    src/packet/packet_manager.h
    src/packet/packet_manager.cpp
)

# Phase 9 Network & Data Sources
set(NETWORK_SOURCES
    # Network configuration
    src/network/config/network_config.h
    src/network/config/network_config.cpp
    
    # Network sources
    src/network/sources/udp_source.h
    src/network/sources/udp_source.cpp
    src/network/sources/tcp_source.h
    src/network/sources/tcp_source.cpp
)

set(OFFLINE_SOURCES
    # Offline sources
    src/offline/sources/file_source.h
    src/offline/sources/file_source.cpp
    src/offline/sources/file_indexer.h
    src/offline/sources/file_indexer.cpp
)

# Phase 10 Test Framework sources
set(TEST_FRAMEWORK_SOURCES
    # Core test framework components
    src/test_framework/core/test_definition.h
    src/test_framework/core/test_definition.cpp
    src/test_framework/core/test_expression.h
    src/test_framework/core/test_expression.cpp
    src/test_framework/core/test_result.h
    src/test_framework/core/test_result.cpp
    src/test_framework/core/test_set.h
    src/test_framework/core/test_set.cpp
    src/test_framework/core/field_reference.h
    src/test_framework/core/field_reference.cpp
    
    # Expression parser
    src/test_framework/parser/expression_lexer.h
    src/test_framework/parser/expression_lexer.cpp
    src/test_framework/parser/expression_parser.h
    src/test_framework/parser/expression_parser.cpp
    
    # Execution engine
    src/test_framework/execution/test_runner.h
    src/test_framework/execution/test_runner.cpp
    src/test_framework/execution/test_scheduler.h
    src/test_framework/execution/test_scheduler.cpp
    src/test_framework/execution/result_collector.h
    src/test_framework/execution/result_collector.cpp
    src/test_framework/execution/alert_manager.h
    src/test_framework/execution/alert_manager.cpp
    src/test_framework/execution/expression_evaluator.h
    src/test_framework/execution/expression_evaluator.cpp
)

# Core library sources
set(CORE_SOURCES
    src/core/application.cpp
    src/core/application.h
    src/memory/memory_pool.cpp
    src/memory/memory_pool.h
    src/events/event_dispatcher.cpp
    src/events/event_dispatcher.h
    src/events/event.h
    src/logging/logger.cpp
    src/logging/logger.h
    src/profiling/profiler.cpp
    src/profiling/profiler.h
    ${PARSER_SOURCES}
    ${THREADING_SOURCES}
    ${PACKET_SOURCES}
    ${NETWORK_SOURCES}
    ${OFFLINE_SOURCES}
    ${TEST_FRAMEWORK_SOURCES}
)

# Create core library
add_library(MonitorCore ${CORE_SOURCES})
target_link_libraries(MonitorCore Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network)
target_include_directories(MonitorCore PUBLIC include src)

# Apply hot-path optimizations for performance-critical files
apply_hot_path_optimizations(MonitorCore)

# Link nlohmann/json if available
if(HAVE_NLOHMANN_JSON)
    target_link_libraries(MonitorCore nlohmann_json::nlohmann_json)
    target_compile_definitions(MonitorCore PRIVATE HAVE_NLOHMANN_JSON=1)
endif()

# Phase 5 UI Framework sources
set(UI_SOURCES
    # UI Manager components
    src/ui/managers/tab_manager.h
    src/ui/managers/tab_manager.cpp
    src/ui/managers/settings_manager.h
    src/ui/managers/settings_manager.cpp
    src/ui/managers/window_manager.h
    src/ui/managers/window_manager.cpp
    
    # UI Window components
    src/ui/windows/struct_window.h
    src/ui/windows/struct_window.cpp
    src/ui/windows/performance_dashboard.h
    src/ui/windows/performance_dashboard.cpp
    
    # UI Widget components
    src/ui/widgets/base_widget.h
    src/ui/widgets/base_widget.cpp
    src/ui/widgets/display_widget.h
    src/ui/widgets/display_widget.cpp
    src/ui/widgets/grid_widget.h
    src/ui/widgets/grid_widget.cpp
    src/ui/widgets/grid_logger_widget.h
    src/ui/widgets/grid_logger_widget.cpp
    
    # Mock implementations for Phase 6 testing
    src/packet/routing/subscription_manager_mock.h
    src/packet/routing/subscription_manager_mock.cpp
    src/packet/processing/field_extractor_mock.h
    src/packet/processing/field_extractor_mock.cpp
    
    # Phase 7 Chart Widgets
    src/ui/widgets/charts/chart_common.h
    src/ui/widgets/charts/chart_common.cpp
    src/ui/widgets/charts/chart_widget.h
    src/ui/widgets/charts/chart_widget.cpp
    src/ui/widgets/charts/line_chart_widget.h
    src/ui/widgets/charts/line_chart_widget.cpp
    src/ui/widgets/charts/bar_chart_widget.h
    src/ui/widgets/charts/bar_chart_widget.cpp
    src/ui/widgets/charts/pie_chart_widget.h
    src/ui/widgets/charts/pie_chart_widget.cpp
    
    # Phase 8 - 3D Chart Widget
    src/ui/widgets/charts/chart_3d_widget.h
    src/ui/widgets/charts/chart_3d_widget.cpp
    
    # Phase 9 - Network UI
    src/network/ui/network_config_dialog.h
    src/network/ui/network_config_dialog.cpp
    
    # Phase 10 - Test Framework UI
    src/ui/test_framework/test_manager_window.h
    src/ui/test_framework/test_manager_window.cpp
)

# Create UI library
add_library(MonitorUI ${UI_SOURCES})
target_link_libraries(MonitorUI 
    PUBLIC MonitorCore
    PUBLIC Qt${QT_VERSION_MAJOR}::Core
    PUBLIC Qt${QT_VERSION_MAJOR}::Widgets
    PUBLIC Qt${QT_VERSION_MAJOR}::Charts
    PUBLIC Qt${QT_VERSION_MAJOR}::Svg
    PUBLIC Qt${QT_VERSION_MAJOR}::Network
    PUBLIC Qt${QT_VERSION_MAJOR}::Concurrent
)

# Conditionally link 3D libraries if available
if(QT3D_AVAILABLE)
    if(Qt63DCore_FOUND)
        target_link_libraries(MonitorUI 
            PUBLIC Qt${QT_VERSION_MAJOR}::3DCore
            PUBLIC Qt${QT_VERSION_MAJOR}::3DRender
            PUBLIC Qt${QT_VERSION_MAJOR}::3DExtras
        )
    elseif(Qt6_Quick3D_FOUND)
        target_link_libraries(MonitorUI 
            PUBLIC Qt${QT_VERSION_MAJOR}::Quick3D
        )
    endif()
    target_compile_definitions(MonitorUI PUBLIC HAS_QT3D=1)
else()
    target_compile_definitions(MonitorUI PUBLIC HAS_QT3D=0)
endif()
target_include_directories(MonitorUI PUBLIC include src)

# Main application sources
set(APP_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

# Create main executable
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Gorkemv5 MANUAL_FINALIZATION ${APP_SOURCES})
else()
    add_executable(Gorkemv5 ${APP_SOURCES})
endif()

target_link_libraries(Gorkemv5 PRIVATE
    MonitorUI
)

# Test sources
set(TEST_SOURCES
    tests/unit/test_memory_pool.cpp
    tests/unit/test_event_dispatcher.cpp
    tests/unit/test_logger.cpp
    tests/unit/test_profiler.cpp
    tests/unit/test_application.cpp
    # Parser tests
    tests/unit/parser/test_token_types.cpp
    tests/unit/parser/test_phase2_minimal.cpp
    # Additional parser tests (to be implemented)
    # tests/unit/parser/test_ast_nodes.cpp
    # tests/unit/parser/test_struct_parser.cpp
    # tests/unit/parser/test_layout_calculator.cpp
    # tests/unit/parser/test_structure_manager.cpp
    # tests/unit/parser/test_tokenizer.cpp
    # tests/unit/parser/test_json_serializer.cpp
    # tests/integration/test_parser_integration.cpp

    # Phase 3 Threading & Concurrency tests (simplified working versions)
    tests/unit/threading/test_thread_pool_simple.cpp
    tests/unit/concurrent/test_mpsc_simple.cpp

    # Phase 4 Packet Processing tests
    tests/unit/test_packet_core.cpp
    tests/unit/test_packet_sources.cpp
    tests/unit/test_packet_routing.cpp
    tests/unit/test_packet_processing.cpp
    tests/unit/test_packet_integration.cpp

    # Phase 5 UI Framework tests
    tests/unit/ui/test_tab_manager.cpp
    tests/unit/ui/test_struct_window.cpp
    tests/unit/ui/test_settings_manager.cpp
    tests/unit/ui/test_settings_manager_simple.cpp
    tests/unit/ui/test_window_manager.cpp
    tests/unit/ui/test_main_window.cpp
    tests/unit/ui/test_ui_integration.cpp

    # Phase 6 Widget tests
    tests/unit/ui/widgets/test_base_widget.cpp
    tests/unit/ui/widgets/test_display_widget.cpp
    tests/unit/ui/widgets/test_grid_widget.cpp
    tests/unit/ui/widgets/test_grid_logger_widget.cpp
    tests/unit/ui/widgets/test_widget_integration.cpp
    
    # Phase 7 Chart Widget tests  
    tests/unit/ui/widgets/charts/test_chart_simple.cpp
    
    # Phase 8 Advanced Visualization tests
    tests/unit/ui/widgets/charts/test_phase8_simple.cpp
    tests/unit/ui/widgets/charts/test_chart_3d_widget_minimal.cpp
    tests/unit/ui/windows/test_performance_dashboard_minimal.cpp
    tests/unit/integration/test_qt3d_integration_simple.cpp
    tests/unit/performance/test_performance_monitoring_simple.cpp
    
    # Phase 9 Network & Data Sources tests
    tests/unit/network/test_network_config.cpp
    tests/unit/network/test_udp_source.cpp
    tests/unit/network/test_tcp_source.cpp
    tests/unit/offline/test_file_source.cpp
    tests/unit/offline/test_file_indexer.cpp
    tests/unit/test_phase9_simple.cpp
    
    # Phase 9 Integration tests
    # tests/integration/test_network_integration.cpp # Temporarily disabled - missing implementations
    tests/integration/test_network_integration_simple.cpp
    tests/integration/test_offline_integration.cpp
    tests/integration/test_offline_integration_simple.cpp
    
    # Phase 9 Performance tests
    tests/performance/test_phase9_performance.cpp
    tests/performance/test_phase9_performance_simple.cpp
    
    # Phase 10 Test Framework tests
    tests/unit/test_framework/test_field_reference.cpp
    tests/unit/test_framework/test_test_definition.cpp
    tests/unit/test_framework/test_test_expression.cpp
    tests/unit/test_framework/test_expression_lexer.cpp
    tests/unit/test_framework/test_test_result.cpp
    tests/unit/test_framework/test_test_set.cpp
    tests/unit/test_framework/test_expression_parser.cpp
    tests/unit/test_framework/test_expression_evaluator.cpp
    tests/unit/test_framework/test_test_runner.cpp
    tests/unit/test_framework/test_test_runner_minimal.cpp
    tests/performance/test_framework_performance.cpp
    tests/performance/test_framework_performance_simple.cpp
    tests/integration/test_framework_integration.cpp
    tests/integration/test_framework_integration_simple.cpp
    tests/integration/test_framework_integration_minimal.cpp
    tests/integration/test_integration_debug.cpp
)

# Create separate test executable for each test file
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    
    # Link libraries based on test type
    if(${TEST_NAME} MATCHES "test_(tab_manager|struct_window|settings_manager|window_manager|main_window|ui_integration|base_widget|display_widget|grid_widget|grid_logger_widget|widget_integration|chart_simple|chart_3d_widget_minimal|performance_dashboard_minimal|phase8_simple|network_config)")
        # UI tests need UI library
        target_link_libraries(${TEST_NAME} PRIVATE
            Qt${QT_VERSION_MAJOR}::Test
            MonitorUI
        )
    elseif(${TEST_NAME} MATCHES "test_(qt3d_integration_simple|performance_monitoring_simple)")
        # Integration and performance tests need both UI and Core libraries with additional dependencies
        target_link_libraries(${TEST_NAME} PRIVATE
            Qt${QT_VERSION_MAJOR}::Test
            MonitorUI
            MonitorCore
        )
    elseif(${TEST_NAME} MATCHES "test_(udp_source|tcp_source|file_source|file_indexer|phase9_simple|network_integration|network_integration_simple|offline_integration|offline_integration_simple|phase9_performance|phase9_performance_simple)")
        # Network and offline tests need Network support
        target_link_libraries(${TEST_NAME} PRIVATE
            Qt${QT_VERSION_MAJOR}::Test
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Network
            MonitorCore
        )
    elseif(${TEST_NAME} MATCHES "test_(framework_integration.*)")
        # Test Framework integration tests need both Core and UI libraries
        target_link_libraries(${TEST_NAME} PRIVATE
            Qt${QT_VERSION_MAJOR}::Test
            Qt${QT_VERSION_MAJOR}::Core
            MonitorCore
        )
    else()
        # Core tests
        target_link_libraries(${TEST_NAME} PRIVATE
            Qt${QT_VERSION_MAJOR}::Test
            Qt${QT_VERSION_MAJOR}::Core
            MonitorCore
        )
    endif()
    
    # Add test to CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# Set target properties
set_target_properties(Gorkemv5 PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Installation rules
include(GNUInstallDirs)
install(TARGETS Gorkemv5
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Gorkemv5)
endif()
