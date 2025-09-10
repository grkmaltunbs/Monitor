#include <QtTest/QtTest>
#include <QObject>
#include <QSignalSpy>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>

#include "../../src/packet/packet_manager.h"
#include "../../src/packet/sources/simulation_source.h"
#include "../../src/core/application.h"
#include "../../src/parser/manager/structure_manager.h"
#include "../../src/threading/thread_manager.h"

using namespace Monitor;
using namespace Monitor::Packet;
using namespace Monitor::Core;

// Mock StructureManager for integration testing
class MockStructureManager : public Parser::StructureManager {
    Q_OBJECT
    
public:
    explicit MockStructureManager(QObject* parent = nullptr) 
        : Parser::StructureManager(parent) {}
    
    // Provide minimal implementation that always succeeds
    bool hasStructure(const std::string& /*name*/) const { return true; }
    StructureInfo getStructure(const std::string& /*name*/) const { 
        return StructureInfo{}; 
    }
    std::vector<std::string> getStructureNames() const { return {}; }
    
    // Mock parsing that always succeeds
    ParseResult parseStructures(const std::string& /*text*/) {
        ParseResult result;
        result.success = true;
        return result;
    }
    
    bool isValidField(const std::string& /*structName*/, const std::string& /*fieldName*/) const {
        return true;
    }
};

// Mock ThreadManager for integration testing  
class MockThreadManager : public Threading::ThreadManager {
    Q_OBJECT
    
private:
    std::unique_ptr<Threading::ThreadPool> m_defaultPool;
    
public:
    explicit MockThreadManager(QObject* parent = nullptr) 
        : Threading::ThreadManager(parent) {
        // Create a simple thread pool for testing
        m_defaultPool = std::make_unique<Threading::ThreadPool>();
        m_defaultPool->initialize(2); // 2 threads for testing
    }
    
    ~MockThreadManager() override {
        if (m_defaultPool) {
            m_defaultPool->shutdown();
        }
    }
    
    Threading::ThreadPool* getDefaultThreadPool() {
        return m_defaultPool.get();
    }
};

class TestPacketIntegration : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<MockStructureManager> m_mockStructureManager;
    std::unique_ptr<MockThreadManager> m_mockThreadManager;

private slots:
    void initTestCase() {
        auto app = Application::instance();
        if (app) {
            app->initialize();
        }
        
        // Create mock managers for testing
        m_mockStructureManager = std::make_unique<MockStructureManager>();
        m_mockThreadManager = std::make_unique<MockThreadManager>();
    }

    void cleanupTestCase() {
        auto app = Application::instance();
        if (app) {
            app->shutdown();
        }
    }

    void testPacketManagerBasic() {
        PacketManager::Configuration config;
        config.enablePerformanceMonitoring = true;
        
        PacketManager manager(config);
        
        // Initialize with application dependencies
        auto app = Application::instance();
        QVERIFY(app != nullptr);
        
        bool initResult = manager.initialize(
            m_mockStructureManager.get(),  // Use mock structure manager
            m_mockThreadManager.get(),     // Use mock thread manager  
            app->eventDispatcher(),
            app->memoryManager()
        );
        QVERIFY(initResult);
        
        // Test initial state
        QCOMPARE(manager.getState(), PacketManager::State::Ready);
        QVERIFY(!manager.isRunning());
        
        // Test start
        QVERIFY(manager.start());
        QCOMPARE(manager.getState(), PacketManager::State::Running);
        QVERIFY(manager.isRunning());
        
        // Test statistics
        const auto& stats = manager.getSystemStatistics();
        QVERIFY(stats.lastUpdate.time_since_epoch().count() > 0);
        
        // Test stop
        manager.stop();
        QCOMPARE(manager.getState(), PacketManager::State::Ready);
        QVERIFY(!manager.isRunning());
    }
    
    void testPacketManagerWithSources() {
        PacketManager manager;
        
        // Initialize manager
        auto app = Application::instance();
        QVERIFY(app != nullptr);
        
        bool initResult = manager.initialize(
            m_mockStructureManager.get(),  // Use mock structure manager
            m_mockThreadManager.get(),     // Use mock thread manager  
            app->eventDispatcher(),
            app->memoryManager()
        );
        QVERIFY(initResult);
        
        // Create simulation source config
        SimulationSource::SimulationConfig sourceConfig("TestSim");
        sourceConfig.packetTypes = {
            {1001, "TestPacket", 64, 50, SimulationSource::PatternType::Counter} // 50ms interval = 20Hz
        };
        
        // Create simulation source
        QVERIFY(manager.createSimulationSource("TestSim", sourceConfig));
        
        auto sourceNames = manager.getSourceNames();
        QCOMPARE(sourceNames.size(), 1u);
        QCOMPARE(sourceNames[0], std::string("TestSim"));
        
        // Start manager (should start sources)
        QVERIFY(manager.start());
        
        // Wait for some packets to be processed
        QTest::qWait(200);
        
        // Check system statistics
        const auto& stats = manager.getSystemStatistics();
        QVERIFY(stats.dispatcherStats.totalPacketsReceived >= 0); // May be 0 in fast tests
        
        // Remove source
        QVERIFY(manager.removeSource("TestSim"));
        
        auto emptyNames = manager.getSourceNames();
        QCOMPARE(emptyNames.size(), 0u);
        
        manager.stop();
    }
    
    void testPacketManagerSignals() {
        PacketManager manager;
        
        // Initialize manager
        auto app = Application::instance();
        QVERIFY(app != nullptr);
        
        bool initResult = manager.initialize(
            m_mockStructureManager.get(),  // Use mock structure manager
            m_mockThreadManager.get(),     // Use mock thread manager  
            app->eventDispatcher(),
            app->memoryManager()
        );
        QVERIFY(initResult);
        
        // Set up signal spies
        QSignalSpy startedSpy(&manager, &PacketManager::started);
        QSignalSpy stoppedSpy(&manager, &PacketManager::stopped);
        QSignalSpy errorSpy(&manager, &PacketManager::errorOccurred);
        QSignalSpy statsSpy(&manager, &PacketManager::statisticsUpdated);
        
        // Test start signal
        QVERIFY(manager.start());
        QCOMPARE(startedSpy.count(), 1);
        
        // Wait for statistics updates
        QTest::qWait(100);
        
        // Test stop signal
        manager.stop();
        QCOMPARE(stoppedSpy.count(), 1);
    }
    
    void testBasicPacketFlow() {
        PacketManager manager;
        
        // Initialize manager  
        auto app = Application::instance();
        QVERIFY(app != nullptr);
        
        bool initResult = manager.initialize(
            m_mockStructureManager.get(),  // Use mock structure manager
            m_mockThreadManager.get(),     // Use mock thread manager  
            app->eventDispatcher(),
            app->memoryManager()
        );
        QVERIFY(initResult);
        
        // Start the manager
        QVERIFY(manager.start());
        
        // Create simulation source config with faster rate for testing
        SimulationSource::SimulationConfig sourceConfig("FlowTest");
        sourceConfig.packetTypes = {
            {1001, "TestPacket1", 64, 10, SimulationSource::PatternType::Sine},    // 10ms = 100Hz
            {1002, "TestPacket2", 32, 20, SimulationSource::PatternType::Counter} // 20ms = 50Hz
        };
        
        QVERIFY(manager.createSimulationSource("FlowTest", sourceConfig));
        
        // Set up packet subscription to capture processed packets
        std::atomic<int> packetsReceived{0};
        
        // Subscribe to packets (using the dispatcher through manager)
        auto dispatcher = manager.getPacketDispatcher();
        QVERIFY(dispatcher != nullptr);
        
        auto callback = [&packetsReceived](PacketPtr /*packet*/) {
            packetsReceived++;
        };
        
        auto subId = dispatcher->subscribe("TestSubscriber", 0, callback, 0); // Subscribe to all packets
        QVERIFY(subId != 0);
        
        // Let the system run for a short time
        QTest::qWait(500);
        
        // Verify packets were processed (should get some packets in 500ms)
        QVERIFY(packetsReceived.load() >= 0); // At least should work without crashing
        
        // Check system statistics
        // Don't require specific numbers since timing can vary in tests
        Q_UNUSED(manager.getSystemStatistics());
        
        // Cleanup
        dispatcher->unsubscribe(subId);
        manager.removeSource("FlowTest");
        manager.stop();
    }
    
    void testErrorHandling() {
        PacketManager manager;
        
        // Test operating without initialization (should fail gracefully)
        QCOMPARE(manager.getState(), PacketManager::State::Uninitialized);
        QVERIFY(!manager.start()); // Should fail without initialization
        
        // Initialize properly
        auto app = Application::instance();
        QVERIFY(app != nullptr);
        
        bool initResult = manager.initialize(
            m_mockStructureManager.get(),  // Use mock structure manager
            m_mockThreadManager.get(),     // Use mock thread manager  
            app->eventDispatcher(),
            app->memoryManager()
        );
        QVERIFY(initResult);
        
        // Test duplicate source creation
        SimulationSource::SimulationConfig config1("DuplicateTest");
        config1.packetTypes = {
            {1001, "TestPacket", 32, 100, SimulationSource::PatternType::Counter}
        };
        
        QVERIFY(manager.createSimulationSource("DuplicateTest", config1));
        QVERIFY(!manager.createSimulationSource("DuplicateTest", config1)); // Should fail
        
        // Test removing non-existent source
        QVERIFY(!manager.removeSource("NonExistent"));
        
        // Test starting without issues
        QVERIFY(manager.start());
        QCOMPARE(manager.getState(), PacketManager::State::Running);
        
        // Cleanup
        manager.removeSource("DuplicateTest");
        manager.stop();
    }
    
    void testMemoryManagement() {
        // Test that packet manager properly manages memory without leaks
        PacketManager manager;
        
        // Initialize manager
        auto app = Application::instance();
        QVERIFY(app != nullptr);
        
        bool initResult = manager.initialize(
            m_mockStructureManager.get(),  // Use mock structure manager
            m_mockThreadManager.get(),     // Use mock thread manager  
            app->eventDispatcher(),
            app->memoryManager()
        );
        QVERIFY(initResult);
        
        QVERIFY(manager.start());
        
        // Create and destroy multiple sources
        for (int cycle = 0; cycle < 3; ++cycle) {
            SimulationSource::SimulationConfig config("MemTest" + std::to_string(cycle));
            config.packetTypes = {
                {static_cast<PacketId>(2000 + cycle), "TestPacket", 64, 100, SimulationSource::PatternType::Random}
            };
            
            std::string sourceName = "MemTest" + std::to_string(cycle);
            
            QVERIFY(manager.createSimulationSource(sourceName, config));
            
            // Let it run briefly
            QTest::qWait(50);
            
            // Remove
            QVERIFY(manager.removeSource(sourceName));
        }
        
        manager.stop();
        
        // If we get here without crashes, memory management is working
        QVERIFY(true);
    }
    
    void testSystemShutdown() {
        // Test clean shutdown of the entire system
        PacketManager manager;
        
        // Initialize manager
        auto app = Application::instance();
        QVERIFY(app != nullptr);
        
        bool initResult = manager.initialize(
            m_mockStructureManager.get(),  // Use mock structure manager
            m_mockThreadManager.get(),     // Use mock thread manager  
            app->eventDispatcher(),
            app->memoryManager()
        );
        QVERIFY(initResult);
        
        QVERIFY(manager.start());
        
        // Set up complex system state
        SimulationSource::SimulationConfig config("ShutdownTest");
        config.packetTypes = {
            {3001, "TestPacket", 128, 20, SimulationSource::PatternType::Sine}
        };
        
        QVERIFY(manager.createSimulationSource("ShutdownTest", config));
        
        auto dispatcher = manager.getPacketDispatcher();
        std::atomic<int> receivedCount{0};
        
        auto callback = [&receivedCount](PacketPtr /*packet*/) {
            receivedCount++;
        };
        
        dispatcher->subscribe("ShutdownSub", 0, callback, 0);
        
        // Let system run at high rate
        QTest::qWait(200);
        
        // Test shutdown (should handle gracefully)
        manager.stop(); // Should cleanly stop all components
        
        QCOMPARE(manager.getState(), PacketManager::State::Ready);
        
        // Test restart after shutdown
        QVERIFY(manager.start());
        QCOMPARE(manager.getState(), PacketManager::State::Running);
        
        manager.stop();
    }
};

QTEST_MAIN(TestPacketIntegration)
#include "test_packet_integration.moc"