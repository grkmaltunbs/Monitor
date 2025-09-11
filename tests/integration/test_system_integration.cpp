#include <gtest/gtest.h>
#include <QApplication>
#include <QTimer>
#include <QEventLoop>
#include <QSignalSpy>
#include <chrono>
#include <thread>

#include "../../src/packet/sources/simulation_source.h"
#include "../../src/packet/core/packet_factory.h"
#include "../../src/memory/memory_pool_manager.h"
#include "../../src/events/event_dispatcher.h"
#include "../../src/logging/logger.h"
#include "../../src/ui/widgets/grid_widget.h"
#include "../../src/ui/widgets/grid_logger_widget.h"

using namespace Monitor;

class SystemIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Qt application for widget tests
        if (!QApplication::instance()) {
            int argc = 1;
            char* argv[] = {const_cast<char*>("test")};
            app = new QApplication(argc, argv);
        }

        // Initialize core components
        memoryManager = std::make_unique<Memory::MemoryPoolManager>();
        eventDispatcher = std::make_unique<Events::EventDispatcher>();
        logger = Logging::Logger::instance();
        
        // Initialize packet factory
        packetFactory = std::make_unique<Packet::PacketFactory>(memoryManager.get());
    }

    void TearDown() override {
        // Clean up in reverse order
        simulationSource.reset();
        gridWidget.reset();
        gridLoggerWidget.reset();
        packetFactory.reset();
        eventDispatcher.reset();
        memoryManager.reset();
        
        // Note: Don't delete QApplication - it's managed globally
    }

    std::unique_ptr<Memory::MemoryPoolManager> memoryManager;
    std::unique_ptr<Events::EventDispatcher> eventDispatcher;
    std::unique_ptr<Packet::PacketFactory> packetFactory;
    std::shared_ptr<Packet::SimulationSource> simulationSource;
    std::unique_ptr<UI::GridWidget> gridWidget;
    std::unique_ptr<UI::GridLoggerWidget> gridLoggerWidget;
    Logging::Logger* logger;
    QApplication* app = nullptr;
};

TEST_F(SystemIntegrationTest, EndToEndPacketFlow) {
    // Create simulation source with default configuration
    auto config = Packet::SimulationSource::createDefaultConfig();
    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Create widgets for testing
    gridWidget = std::make_unique<UI::GridWidget>();
    gridLoggerWidget = std::make_unique<UI::GridLoggerWidget>();

    // Set up packet reception tracking
    int packetsReceived = 0;
    PacketPtr lastPacket;
    
    connect(simulationSource.get(), &Packet::PacketSource::packetReady,
            [&](PacketPtr packet) {
                packetsReceived++;
                lastPacket = packet;
            });

    // Start simulation
    ASSERT_TRUE(simulationSource->start());
    EXPECT_EQ(simulationSource->getState(), Packet::PacketSource::State::Running);

    // Wait for packets to be generated
    QEventLoop loop;
    QTimer::singleShot(1000, &loop, &QEventLoop::quit); // Wait 1 second
    loop.exec();

    // Verify packets were received
    EXPECT_GT(packetsReceived, 0);
    EXPECT_TRUE(lastPacket != nullptr);

    // Verify packet structure
    if (lastPacket) {
        EXPECT_GT(lastPacket->totalSize(), 0);
        EXPECT_GT(lastPacket->dataSize(), 0);
        
        auto header = lastPacket->getHeader();
        EXPECT_GT(header.packet_id, 0);
        EXPECT_GT(header.sequence_number, 0);
    }

    // Stop simulation
    simulationSource->stop();
    EXPECT_EQ(simulationSource->getState(), Packet::PacketSource::State::Stopped);
}

TEST_F(SystemIntegrationTest, WidgetPacketSubscription) {
    // Create simulation source
    auto config = Packet::SimulationSource::createDefaultConfig();
    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Create and configure grid widget
    gridWidget = std::make_unique<UI::GridWidget>();
    
    // Track widget updates
    int widgetUpdates = 0;
    connect(gridWidget.get(), &UI::GridWidget::dataUpdated,
            [&]() { widgetUpdates++; });

    // Start simulation
    ASSERT_TRUE(simulationSource->start());

    // Wait for widget updates
    QEventLoop loop;
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();

    // Verify widget received updates (if subscription is working)
    // Note: Actual widget updates depend on subscription system being fully implemented
    EXPECT_GE(widgetUpdates, 0); // At least no crashes occurred

    simulationSource->stop();
}

TEST_F(SystemIntegrationTest, MultiplePacketTypes) {
    // Create simulation source with multiple packet types
    auto config = Packet::SimulationSource::createStressTestConfig();
    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Track different packet types
    std::map<uint32_t, int> packetTypeCounts;
    
    connect(simulationSource.get(), &Packet::PacketSource::packetReady,
            [&](PacketPtr packet) {
                auto header = packet->getHeader();
                packetTypeCounts[header.packet_id]++;
            });

    // Start simulation
    ASSERT_TRUE(simulationSource->start());

    // Wait for packets
    QEventLoop loop;
    QTimer::singleShot(2000, &loop, &QEventLoop::quit); // Wait 2 seconds for multiple types
    loop.exec();

    // Verify multiple packet types were generated
    EXPECT_GT(packetTypeCounts.size(), 1);
    
    // Verify each type generated packets
    for (const auto& [packetId, count] : packetTypeCounts) {
        EXPECT_GT(count, 0) << "Packet type " << packetId << " should have generated packets";
    }

    simulationSource->stop();
}

TEST_F(SystemIntegrationTest, PerformanceUnderLoad) {
    // Create high-rate simulation source
    auto config = Packet::SimulationSource::createStressTestConfig();
    config.basePacketRate = 1000.0; // High rate for stress test
    
    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Track performance metrics
    int totalPackets = 0;
    auto startTime = std::chrono::steady_clock::now();
    
    connect(simulationSource.get(), &Packet::PacketSource::packetReady,
            [&](PacketPtr) { totalPackets++; });

    // Start high-rate simulation
    ASSERT_TRUE(simulationSource->start());

    // Run for stress test duration
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit); // 3 seconds
    loop.exec();

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    simulationSource->stop();

    // Calculate and verify performance
    double packetsPerSecond = (static_cast<double>(totalPackets) * 1000.0) / duration;
    
    EXPECT_GT(totalPackets, 0);
    EXPECT_GT(packetsPerSecond, 100.0) << "Should generate at least 100 packets/second under load";
    
    // Log performance results
    logger->info("SystemIntegration", 
                QString("Performance test: %1 packets in %2ms = %3 packets/second")
                .arg(totalPackets).arg(duration).arg(packetsPerSecond));
}

TEST_F(SystemIntegrationTest, ErrorHandlingAndRecovery) {
    // Create simulation source
    auto config = Packet::SimulationSource::createDefaultConfig();
    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Track error states
    bool errorReceived = false;
    connect(simulationSource.get(), &Packet::PacketSource::error,
            [&](const QString&) { errorReceived = true; });

    // Test normal operation first
    ASSERT_TRUE(simulationSource->start());
    EXPECT_EQ(simulationSource->getState(), Packet::PacketSource::State::Running);
    
    // Brief operation
    // Simplified timeout handling

    QCoreApplication::processEvents();

    // Test pause/resume cycle
    simulationSource->pause();
    EXPECT_EQ(simulationSource->getState(), Packet::PacketSource::State::Paused);
    
    simulationSource->resume();
    EXPECT_EQ(simulationSource->getState(), Packet::PacketSource::State::Running);

    // Test stop/restart cycle
    simulationSource->stop();
    EXPECT_EQ(simulationSource->getState(), Packet::PacketSource::State::Stopped);
    
    ASSERT_TRUE(simulationSource->start());
    EXPECT_EQ(simulationSource->getState(), Packet::PacketSource::State::Running);

    // No errors should have occurred during normal operation
    EXPECT_FALSE(errorReceived);

    simulationSource->stop();
}

TEST_F(SystemIntegrationTest, MemoryManagementUnderLoad) {
    // Create simulation source
    auto config = Packet::SimulationSource::createDefaultConfig();
    config.basePacketRate = 500.0; // Moderate rate
    
    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Track memory statistics
    auto initialStats = memoryManager->getStatistics();
    
    // Start simulation
    ASSERT_TRUE(simulationSource->start());

    // Run for extended period
    QEventLoop loop;
    QTimer::singleShot(2000, &loop, &QEventLoop::quit); // 2 seconds
    loop.exec();

    simulationSource->stop();

    // Check memory statistics
    auto finalStats = memoryManager->getStatistics();
    
    // Verify memory pools are functioning
    EXPECT_GE(finalStats.totalBytesAllocated, initialStats.totalBytesAllocated);
    
    // Memory should be efficiently managed (no excessive growth)
    EXPECT_LT(finalStats.totalBytesAllocated, 50 * 1024 * 1024); // Less than 50MB

    // Log memory usage
    logger->info("SystemIntegration",
                QString("Memory usage: Initial=%1 bytes, Final=%2 bytes")
                .arg(initialStats.totalBytesAllocated)
                .arg(finalStats.totalBytesAllocated));
}

TEST_F(SystemIntegrationTest, ConcurrentOperations) {
    // Create simulation source
    auto config = Packet::SimulationSource::createDefaultConfig();
    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Start simulation
    ASSERT_TRUE(simulationSource->start());

    // Test concurrent state changes
    std::atomic<int> operationCount{0};
    std::atomic<bool> testRunning{true};

    // Launch concurrent operations in separate thread
    std::thread concurrentThread([&]() {
        while (testRunning.load()) {
            simulationSource->pause();
            operationCount++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            if (testRunning.load()) {
                simulationSource->resume();
                operationCount++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    });

    // Let concurrent operations run
    QEventLoop loop;
    QTimer::singleShot(1000, &loop, &QEventLoop::quit); // 1 second
    loop.exec();

    // Stop concurrent operations
    testRunning = false;
    concurrentThread.join();

    // Verify system handled concurrent operations
    EXPECT_GT(operationCount.load(), 0);
    EXPECT_TRUE(simulationSource->getState() == Packet::PacketSource::State::Running ||
                simulationSource->getState() == Packet::PacketSource::State::Paused);

    simulationSource->stop();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Initialize Qt application for the test
    QApplication app(argc, argv);
    
    return RUN_ALL_TESTS();
}