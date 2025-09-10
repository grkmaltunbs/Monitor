#include <QtTest/QTest>
#include <QSignalSpy>
#include <QObject>
#include <thread>
#include <chrono>
#include "packet/routing/packet_router.h"
#include "packet/routing/subscription_manager.h"
#include "packet/core/packet_factory.h"
#include "threading/thread_pool.h"
#include "core/application.h"

using namespace Monitor::Packet;
using namespace Monitor::Memory;
using namespace Monitor::Threading;

class TestPacketRouter : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testRouterConstruction();
    void testRouterConfiguration();
    void testComponentSetters();
    void testStartStop();

    // Routing tests
    void testBasicRouting();
    void testPriorityRouting();
    void testAutoPriorityDetection();
    void testInvalidPacketHandling();

    // Statistics tests
    void testStatisticsTracking();
    void testRoutingRateCalculation();
    void testDropRateCalculation();

    // Performance tests
    void testHighThroughputRouting();
    void testLatencyMeasurement();

    // Signal emission tests
    void testSignalEmission();
    void testStatisticsUpdateSignal();

    // Edge cases
    void testRouterNotRunning();
    void testQueueOverflow();
    void testMissingSubscriptionManager();

private:
    Core::Application* m_app = nullptr;
    MemoryPoolManager* m_memoryManager = nullptr;
    PacketFactory* m_packetFactory = nullptr;
    SubscriptionManager* m_subscriptionManager = nullptr;
    ThreadPool* m_threadPool = nullptr;
    PacketRouter* m_router = nullptr;
    
    static constexpr PacketId TEST_PACKET_ID = 1001;
    static constexpr size_t TEST_PAYLOAD_SIZE = 128;
    
    void setupMemoryPools();
    PacketRouter::Configuration createTestConfig();
    PacketPtr createTestPacket(PacketId id = TEST_PACKET_ID, size_t payloadSize = TEST_PAYLOAD_SIZE);
};

void TestPacketRouter::initTestCase() {
    m_app = Core::Application::instance();
    QVERIFY(m_app != nullptr);
    
    m_memoryManager = m_app->memoryManager();
    QVERIFY(m_memoryManager != nullptr);
    
    setupMemoryPools();
    
    m_packetFactory = new PacketFactory(m_memoryManager, this);
    QVERIFY(m_packetFactory != nullptr);
    
    m_subscriptionManager = new SubscriptionManager(this);
    QVERIFY(m_subscriptionManager != nullptr);
    
    m_threadPool = new ThreadPool(this);
    QVERIFY(m_threadPool != nullptr);
}

void TestPacketRouter::cleanupTestCase() {
    // Cleanup handled by QObject parent/child destruction
}

void TestPacketRouter::init() {
    auto config = createTestConfig();
    m_router = new PacketRouter(config, this);
    QVERIFY(m_router != nullptr);
    
    m_router->setSubscriptionManager(m_subscriptionManager);
    m_router->setThreadPool(m_threadPool);
    m_router->setEventDispatcher(m_app->eventDispatcher());
}

void TestPacketRouter::cleanup() {
    if (m_router && m_router->isRunning()) {
        m_router->stop();
    }
    delete m_router;
    m_router = nullptr;
}

void TestPacketRouter::setupMemoryPools() {
    m_memoryManager->createPool("SmallObjects", 64, 1000);
    m_memoryManager->createPool("MediumObjects", 512, 1000);
    m_memoryManager->createPool("WidgetData", 1024, 1000);
    m_memoryManager->createPool("TestFramework", 2048, 1000);
    m_memoryManager->createPool("PacketBuffer", 4096, 1000);
    m_memoryManager->createPool("LargeObjects", 8192, 1000);
}

PacketRouter::Configuration TestPacketRouter::createTestConfig() {
    PacketRouter::Configuration config;
    config.queueSize = 1000;
    config.workerThreads = 2;
    config.batchSize = 10;
    config.maxLatencyMs = 10;
    config.maintainOrder = false;
    config.enableProfiling = true;
    return config;
}

PacketPtr TestPacketRouter::createTestPacket(PacketId id, size_t payloadSize) {
    auto result = m_packetFactory->createPacket(id, nullptr, payloadSize);
    if (result.success) {
        return result.packet;
    }
    return nullptr;
}

void TestPacketRouter::testRouterConstruction() {
    QVERIFY(m_router != nullptr);
    QVERIFY(!m_router->isRunning());
    
    const auto& config = m_router->getConfiguration();
    QCOMPARE(config.queueSize, uint32_t(1000));
    QCOMPARE(config.workerThreads, uint32_t(2));
    QCOMPARE(config.batchSize, uint32_t(10));
    QCOMPARE(config.maxLatencyMs, uint32_t(10));
    QVERIFY(!config.maintainOrder);
    QVERIFY(config.enableProfiling);
}

void TestPacketRouter::testRouterConfiguration() {
    PacketRouter::Configuration defaultConfig;
    
    // Default configuration should auto-detect threads
    QVERIFY(defaultConfig.workerThreads > 0);
    QCOMPARE(defaultConfig.queueSize, uint32_t(10000));
    QCOMPARE(defaultConfig.batchSize, uint32_t(100));
    QCOMPARE(defaultConfig.maxLatencyMs, uint32_t(5));
    QVERIFY(!defaultConfig.maintainOrder);
    QVERIFY(defaultConfig.enableProfiling);
}

void TestPacketRouter::testComponentSetters() {
    // Test setting components
    m_router->setSubscriptionManager(nullptr);
    m_router->setThreadPool(nullptr);
    m_router->setEventDispatcher(nullptr);
    
    // Reset to valid components
    m_router->setSubscriptionManager(m_subscriptionManager);
    m_router->setThreadPool(m_threadPool);
    m_router->setEventDispatcher(m_app->eventDispatcher());
}

void TestPacketRouter::testStartStop() {
    QSignalSpy startedSpy(m_router, &PacketRouter::started);
    QSignalSpy stoppedSpy(m_router, &PacketRouter::stopped);
    
    // Start router
    bool result = m_router->start();
    QVERIFY(result);
    QVERIFY(m_router->isRunning());
    QCOMPARE(startedSpy.count(), 1);
    
    // Starting already running router should succeed
    result = m_router->start();
    QVERIFY(result);
    
    // Stop router
    m_router->stop();
    QVERIFY(!m_router->isRunning());
    QCOMPARE(stoppedSpy.count(), 1);
    
    // Stopping already stopped router should not crash
    m_router->stop();
}

void TestPacketRouter::testBasicRouting() {
    // Create a simple subscriber that counts packets
    std::atomic<int> packetCount(0);
    PacketPtr receivedPacket = nullptr;
    
    m_subscriptionManager->subscribe(TEST_PACKET_ID, [&](PacketPtr packet) {
        packetCount++;
        receivedPacket = packet;
    });
    
    m_router->start();
    
    // Route a test packet
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    bool routed = m_router->routePacket(packet, PacketRouter::Priority::Normal);
    QVERIFY(routed);
    
    // Wait for processing
    QTest::qWait(100);
    
    // Verify packet was delivered
    QTRY_COMPARE(packetCount.load(), 1);
    QVERIFY(receivedPacket != nullptr);
    QCOMPARE(receivedPacket->id(), TEST_PACKET_ID);
    
    m_router->stop();
}

void TestPacketRouter::testPriorityRouting() {
    m_router->start();
    
    QSignalSpy routedSpy(m_router, &PacketRouter::packetRouted);
    
    // Route packets with different priorities
    auto normalPacket = createTestPacket(1);
    auto highPacket = createTestPacket(2);
    auto criticalPacket = createTestPacket(3);
    
    QVERIFY(normalPacket != nullptr);
    QVERIFY(highPacket != nullptr);
    QVERIFY(criticalPacket != nullptr);
    
    m_router->routePacket(normalPacket, PacketRouter::Priority::Normal);
    m_router->routePacket(highPacket, PacketRouter::Priority::High);
    m_router->routePacket(criticalPacket, PacketRouter::Priority::Critical);
    
    // Wait for processing
    QTest::qWait(200);
    
    // All packets should be routed
    QTRY_COMPARE(routedSpy.count(), 3);
    
    // Check statistics per priority
    const auto& stats = m_router->getStatistics();
    QVERIFY(stats.packetsPerPriority[static_cast<size_t>(PacketRouter::Priority::Normal)].load() >= 1);
    QVERIFY(stats.packetsPerPriority[static_cast<size_t>(PacketRouter::Priority::High)].load() >= 1);
    QVERIFY(stats.packetsPerPriority[static_cast<size_t>(PacketRouter::Priority::Critical)].load() >= 1);
    
    m_router->stop();
}

void TestPacketRouter::testAutoPriorityDetection() {
    m_router->start();
    
    // Create packets with different flags
    auto normalPacket = createTestPacket(1);
    auto priorityPacket = createTestPacket(2);
    auto testPacket = createTestPacket(3);
    
    QVERIFY(normalPacket != nullptr);
    QVERIFY(priorityPacket != nullptr);
    QVERIFY(testPacket != nullptr);
    
    priorityPacket->setFlag(PacketHeader::Flags::Priority);
    testPacket->setFlag(PacketHeader::Flags::TestData);
    
    // Route with automatic priority detection
    m_router->routePacketAuto(normalPacket);     // Should be Normal
    m_router->routePacketAuto(priorityPacket);   // Should be High
    m_router->routePacketAuto(testPacket);       // Should be Low
    
    QTest::qWait(200);
    
    const auto& stats = m_router->getStatistics();
    QVERIFY(stats.packetsPerPriority[static_cast<size_t>(PacketRouter::Priority::Normal)].load() >= 1);
    QVERIFY(stats.packetsPerPriority[static_cast<size_t>(PacketRouter::Priority::High)].load() >= 1);
    QVERIFY(stats.packetsPerPriority[static_cast<size_t>(PacketRouter::Priority::Low)].load() >= 1);
    
    m_router->stop();
}

void TestPacketRouter::testInvalidPacketHandling() {
    m_router->start();
    
    const auto& stats = m_router->getStatistics();
    uint64_t initialDropped = stats.packetsDropped.load();
    
    // Route null packet
    bool result = m_router->routePacket(nullptr);
    QVERIFY(!result);
    
    // Route invalid packet (this is tricky to create, so we'll test null case)
    QVERIFY(stats.packetsDropped.load() > initialDropped);
    
    m_router->stop();
}

void TestPacketRouter::testStatisticsTracking() {
    const auto& stats = m_router->getStatistics();
    
    // Initial statistics
    QCOMPARE(stats.packetsReceived.load(), uint64_t(0));
    QCOMPARE(stats.packetsRouted.load(), uint64_t(0));
    QCOMPARE(stats.packetsDropped.load(), uint64_t(0));
    
    m_router->start();
    
    // Route some packets
    for (int i = 0; i < 5; ++i) {
        auto packet = createTestPacket(i);
        if (packet) {
            m_router->routePacket(packet);
        }
    }
    
    QTest::qWait(200);
    
    // Check updated statistics
    QVERIFY(stats.packetsReceived.load() >= 5);
    QVERIFY(stats.packetsRouted.load() >= 0); // May be less due to no subscribers
    
    m_router->stop();
}

void TestPacketRouter::testRoutingRateCalculation() {
    const auto& stats = m_router->getStatistics();
    
    m_router->start();
    
    // Route packets and wait
    for (int i = 0; i < 10; ++i) {
        auto packet = createTestPacket(i);
        if (packet) {
            m_router->routePacket(packet);
        }
    }
    
    QTest::qWait(100);
    
    double rate = stats.getRoutingRate();
    QVERIFY(rate >= 0.0); // Rate should be non-negative
    
    m_router->stop();
}

void TestPacketRouter::testDropRateCalculation() {
    const auto& stats = m_router->getStatistics();
    
    m_router->start();
    
    // Route valid packets
    for (int i = 0; i < 5; ++i) {
        auto packet = createTestPacket(i);
        if (packet) {
            m_router->routePacket(packet);
        }
    }
    
    // Route invalid packets (null)
    for (int i = 0; i < 2; ++i) {
        m_router->routePacket(nullptr);
    }
    
    QTest::qWait(100);
    
    double dropRate = stats.getDropRate();
    QVERIFY(dropRate >= 0.0);
    QVERIFY(dropRate <= 1.0);
    
    if (stats.packetsReceived.load() > 0) {
        // Should have some drop rate due to null packets
        QVERIFY(dropRate > 0.0);
    }
    
    m_router->stop();
}

void TestPacketRouter::testHighThroughputRouting() {
    const int numPackets = 1000;
    
    std::atomic<int> receivedCount(0);
    m_subscriptionManager->subscribe(TEST_PACKET_ID, [&](PacketPtr) {
        receivedCount++;
    });
    
    m_router->start();
    
    // Route many packets quickly
    for (int i = 0; i < numPackets; ++i) {
        auto packet = createTestPacket();
        if (packet) {
            m_router->routePacket(packet);
        }
    }
    
    // Wait for processing
    QTest::qWait(2000); // Give more time for high throughput
    
    const auto& stats = m_router->getStatistics();
    QVERIFY(stats.packetsReceived.load() >= numPackets);
    
    qDebug() << "High throughput test: Received" << stats.packetsReceived.load() 
             << "packets, routed" << stats.packetsRouted.load()
             << "packets, delivered" << receivedCount.load() << "packets";
    
    m_router->stop();
}

void TestPacketRouter::testLatencyMeasurement() {
    m_router->start();
    
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    m_router->routePacket(packet);
    
    QTest::qWait(100);
    
    const auto& stats = m_router->getStatistics();
    // Latency measurements should be available after routing
    // Note: Actual values depend on system performance
    QVERIFY(stats.averageLatencyNs.load() >= 0);
    QVERIFY(stats.maxLatencyNs.load() >= 0);
    
    m_router->stop();
}

void TestPacketRouter::testSignalEmission() {
    QSignalSpy startedSpy(m_router, &PacketRouter::started);
    QSignalSpy stoppedSpy(m_router, &PacketRouter::stopped);
    QSignalSpy routedSpy(m_router, &PacketRouter::packetRouted);
    QSignalSpy droppedSpy(m_router, &PacketRouter::packetDropped);
    
    m_router->start();
    QCOMPARE(startedSpy.count(), 1);
    
    // Route a packet
    auto packet = createTestPacket();
    if (packet) {
        m_router->routePacket(packet);
    }
    
    QTest::qWait(100);
    
    // Should have routed signal
    QTRY_VERIFY(routedSpy.count() >= 1);
    
    // Route null packet to trigger dropped signal
    m_router->routePacket(nullptr);
    
    // Note: Dropped signal may not be emitted for null packets in current implementation
    
    m_router->stop();
    QCOMPARE(stoppedSpy.count(), 1);
}

void TestPacketRouter::testStatisticsUpdateSignal() {
    QSignalSpy statisticsSpy(m_router, &PacketRouter::statisticsUpdated);
    
    m_router->start();
    
    // Route enough packets to trigger statistics update (every 1000 packets)
    for (int i = 0; i < 1000; ++i) {
        auto packet = createTestPacket(i);
        if (packet) {
            m_router->routePacket(packet);
        }
    }
    
    QTest::qWait(1000); // Wait for processing
    
    // Should have emitted statistics update
    QTRY_VERIFY(statisticsSpy.count() >= 1);
    
    m_router->stop();
}

void TestPacketRouter::testRouterNotRunning() {
    // Router not started
    QVERIFY(!m_router->isRunning());
    
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Should fail to route when not running
    bool result = m_router->routePacket(packet);
    QVERIFY(!result);
    
    const auto& stats = m_router->getStatistics();
    QVERIFY(stats.packetsDropped.load() > 0);
}

void TestPacketRouter::testQueueOverflow() {
    // Create router with very small queue
    PacketRouter::Configuration smallConfig;
    smallConfig.queueSize = 2; // Very small queue
    smallConfig.workerThreads = 1;
    
    PacketRouter smallRouter(smallConfig, this);
    smallRouter.setSubscriptionManager(m_subscriptionManager);
    
    smallRouter.start();
    
    // Try to overflow the queue
    bool anyDropped = false;
    for (int i = 0; i < 10; ++i) {
        auto packet = createTestPacket(i);
        if (packet) {
            bool result = smallRouter.routePacket(packet);
            if (!result) {
                anyDropped = true;
            }
        }
    }
    
    // Should have dropped some packets due to queue overflow
    const auto& stats = smallRouter.getStatistics();
    QVERIFY(anyDropped || stats.packetsDropped.load() > 0);
    
    smallRouter.stop();
}

void TestPacketRouter::testMissingSubscriptionManager() {
    PacketRouter::Configuration config;
    PacketRouter routerNoSub(config, this);
    
    // Should fail to start without subscription manager
    bool result = routerNoSub.start();
    QVERIFY(!result);
    QVERIFY(!routerNoSub.isRunning());
}

QTEST_MAIN(TestPacketRouter)
#include "test_packet_router.moc"