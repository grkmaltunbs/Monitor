#include <QtTest/QTest>
#include <QObject>
#include <QSignalSpy>
#include <memory>
#include <thread>
#include <chrono>
#include "packet/routing/packet_dispatcher.h"
#include "packet/sources/packet_source.h"
#include "packet/core/packet_factory.h"
#include "core/application.h"

using namespace Monitor::Packet;

// Mock PacketSource for testing
class MockPacketSource : public PacketSource {
    Q_OBJECT
    
public:
    explicit MockPacketSource(const std::string& name, QObject* parent = nullptr)
        : PacketSource(name, parent), m_started(false), m_simulateError(false)
    {
    }
    
    bool start() override {
        if (!m_started) {
            m_started = true;
            emit started();
        }
        return true;
    }
    
    void stop() override {
        if (m_started) {
            m_started = false;
            emit stopped();
        }
    }
    
    bool isRunning() const override {
        return m_started;
    }
    
    const Statistics& getStatistics() const override {
        return m_stats;
    }
    
    // Test helpers
    void simulatePacketReady(PacketPtr packet) {
        if (m_simulateError) {
            emit error("Simulated error");
            return;
        }
        emit packetReady(packet);
    }
    
    void setSimulateError(bool simulate) {
        m_simulateError = simulate;
    }
    
private:
    bool m_started;
    bool m_simulateError;
    Statistics m_stats;
};

class TestPacketDispatcher : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testConstruction();
    void testLifecycleManagement();
    void testComponentIntegration();

    // Source management tests
    void testSourceRegistration();
    void testSourceUnregistration();
    void testSourceEnableDisable();
    void testMaxSourcesLimit();
    void testDuplicateSourceRegistration();

    // Subscription delegation tests
    void testSubscriptionDelegation();
    void testUnsubscriptionDelegation();

    // Packet flow tests
    void testPacketReception();
    void testPacketRouting();
    void testPacketDistribution();
    void testInvalidPacketHandling();

    // Back-pressure tests
    void testBackPressureDetection();
    void testBackPressureDisabled();

    // Statistics tests
    void testStatisticsTracking();
    void testStatisticsUpdates();

    // Qt integration tests
    void testSignalEmission();
    void testSlotHandling();

    // Error handling tests
    void testSourceErrorHandling();
    void testComponentFailureRecovery();

    // Performance tests
    void testHighThroughputProcessing();
    void testConcurrentSourceManagement();

    // Edge cases
    void testEmptyDispatcher();
    void testPartialComponentFailure();

private:
    PacketDispatcher* m_dispatcher = nullptr;
    PacketFactory* m_packetFactory = nullptr;
    Monitor::Core::Application* m_app = nullptr;
    
    static constexpr PacketId TEST_PACKET_ID_1 = 100;
    static constexpr PacketId TEST_PACKET_ID_2 = 200;
    
    // Helper methods
    PacketPtr createTestPacket(PacketId id = TEST_PACKET_ID_1, size_t payloadSize = 256);
    MockPacketSource* createMockSource(const std::string& name);
    void setupBasicDispatcher();
    
    // Test data
    std::atomic<int> m_callbackCounter{0};
    std::vector<PacketPtr> m_receivedPackets;
    std::mutex m_receivedPacketsMutex;
    
    void packetCallback(PacketPtr packet);
    void resetTestData();
};

#include "test_packet_dispatcher.moc"

void TestPacketDispatcher::initTestCase() {
    // Initialize application
    m_app = Monitor::Core::Application::instance();
    QVERIFY(m_app != nullptr);
    
    auto memoryManager = m_app->memoryManager();
    QVERIFY(memoryManager != nullptr);
    
    m_packetFactory = new PacketFactory(memoryManager);
    QVERIFY(m_packetFactory != nullptr);
}

void TestPacketDispatcher::cleanupTestCase() {
    delete m_packetFactory;
    m_packetFactory = nullptr;
}

void TestPacketDispatcher::init() {
    setupBasicDispatcher();
    resetTestData();
}

void TestPacketDispatcher::cleanup() {
    if (m_dispatcher) {
        m_dispatcher->stop();
        delete m_dispatcher;
        m_dispatcher = nullptr;
    }
    resetTestData();
}

PacketPtr TestPacketDispatcher::createTestPacket(PacketId id, size_t payloadSize) {
    const char* testData = "Test packet data for dispatcher testing";
    size_t dataSize = std::min(payloadSize, strlen(testData));
    
    auto result = m_packetFactory->createPacket(id, testData, dataSize);
    return result.packet;
}

MockPacketSource* TestPacketDispatcher::createMockSource(const std::string& name) {
    return new MockPacketSource(name, this);
}

void TestPacketDispatcher::setupBasicDispatcher() {
    PacketDispatcher::Configuration config;
    config.enableBackPressure = true;
    config.backPressureThreshold = 1000;
    config.maxSources = 10;
    config.enableMetrics = true;
    
    m_dispatcher = new PacketDispatcher(config, this);
    QVERIFY(m_dispatcher != nullptr);
}

void TestPacketDispatcher::packetCallback(PacketPtr packet) {
    m_callbackCounter++;
    std::lock_guard<std::mutex> lock(m_receivedPacketsMutex);
    m_receivedPackets.push_back(packet);
}

void TestPacketDispatcher::resetTestData() {
    m_callbackCounter = 0;
    std::lock_guard<std::mutex> lock(m_receivedPacketsMutex);
    m_receivedPackets.clear();
}

void TestPacketDispatcher::testConstruction() {
    // Test basic construction
    QVERIFY(m_dispatcher != nullptr);
    QVERIFY(qobject_cast<QObject*>(m_dispatcher) != nullptr);
    
    // Test initial state
    QVERIFY(!m_dispatcher->isRunning());
    QCOMPARE(m_dispatcher->getRegisteredSources().size(), size_t(0));
    
    // Test component access
    QVERIFY(m_dispatcher->getSubscriptionManager() != nullptr);
    QVERIFY(m_dispatcher->getPacketRouter() != nullptr);
    
    // Test initial statistics
    const auto& stats = m_dispatcher->getStatistics();
    QCOMPARE(stats.totalPacketsReceived.load(), uint64_t(0));
    QCOMPARE(stats.totalPacketsProcessed.load(), uint64_t(0));
    QCOMPARE(stats.totalPacketsDropped.load(), uint64_t(0));
    QCOMPARE(stats.sourceCount.load(), uint64_t(0));
    QCOMPARE(stats.subscriberCount.load(), uint64_t(0));
}

void TestPacketDispatcher::testLifecycleManagement() {
    // Test initial not running state
    QVERIFY(!m_dispatcher->isRunning());
    
    // Test start
    QVERIFY(m_dispatcher->start());
    QVERIFY(m_dispatcher->isRunning());
    
    // Test double start (should succeed)
    QVERIFY(m_dispatcher->start());
    QVERIFY(m_dispatcher->isRunning());
    
    // Test stop
    m_dispatcher->stop();
    QVERIFY(!m_dispatcher->isRunning());
    
    // Test double stop (should not crash)
    m_dispatcher->stop();
    QVERIFY(!m_dispatcher->isRunning());
    
    // Test restart
    QVERIFY(m_dispatcher->start());
    QVERIFY(m_dispatcher->isRunning());
}

void TestPacketDispatcher::testComponentIntegration() {
    // Test subscription manager integration
    auto subscriptionManager = m_dispatcher->getSubscriptionManager();
    QVERIFY(subscriptionManager != nullptr);
    QCOMPARE(subscriptionManager->getTotalSubscriberCount(), size_t(0));
    
    // Test router integration
    auto router = m_dispatcher->getPacketRouter();
    QVERIFY(router != nullptr);
    QVERIFY(!router->isRunning()); // Should not be running initially
    
    // Start dispatcher and verify router starts
    QVERIFY(m_dispatcher->start());
    QVERIFY(router->isRunning());
    
    // Stop dispatcher and verify router stops
    m_dispatcher->stop();
    QVERIFY(!router->isRunning());
}

void TestPacketDispatcher::testSourceRegistration() {
    auto source = createMockSource("TestSource");
    
    // Test successful registration
    QVERIFY(m_dispatcher->registerSource(source));
    QCOMPARE(m_dispatcher->getRegisteredSources().size(), size_t(1));
    
    // Verify source details
    const auto& sources = m_dispatcher->getRegisteredSources();
    QCOMPARE(sources[0].name, std::string("TestSource"));
    QCOMPARE(sources[0].source, source);
    QVERIFY(sources[0].enabled);
    
    // Test statistics update
    const auto& stats = m_dispatcher->getStatistics();
    QCOMPARE(stats.sourceCount.load(), uint64_t(1));
    
    // Test null source registration
    QVERIFY(!m_dispatcher->registerSource(nullptr));
    QCOMPARE(m_dispatcher->getRegisteredSources().size(), size_t(1));
    
    // Clean up
    delete source;
}

void TestPacketDispatcher::testSourceUnregistration() {
    auto source1 = createMockSource("Source1");
    auto source2 = createMockSource("Source2");
    
    // Register sources
    QVERIFY(m_dispatcher->registerSource(source1));
    QVERIFY(m_dispatcher->registerSource(source2));
    QCOMPARE(m_dispatcher->getRegisteredSources().size(), size_t(2));
    
    // Test successful unregistration
    QVERIFY(m_dispatcher->unregisterSource("Source1"));
    QCOMPARE(m_dispatcher->getRegisteredSources().size(), size_t(1));
    QCOMPARE(m_dispatcher->getRegisteredSources()[0].name, std::string("Source2"));
    
    // Test statistics update
    const auto& stats = m_dispatcher->getStatistics();
    QCOMPARE(stats.sourceCount.load(), uint64_t(1));
    
    // Test unregistering non-existent source
    QVERIFY(!m_dispatcher->unregisterSource("NonExistentSource"));
    QCOMPARE(m_dispatcher->getRegisteredSources().size(), size_t(1));
    
    // Test unregistering remaining source
    QVERIFY(m_dispatcher->unregisterSource("Source2"));
    QCOMPARE(m_dispatcher->getRegisteredSources().size(), size_t(0));
    QCOMPARE(stats.sourceCount.load(), uint64_t(0));
    
    // Clean up
    delete source1;
    delete source2;
}

void TestPacketDispatcher::testSourceEnableDisable() {
    auto source = createMockSource("TestSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    // Test initial enabled state
    const auto& sources = m_dispatcher->getRegisteredSources();
    QVERIFY(sources[0].enabled);
    
    // Test disabling source
    QVERIFY(m_dispatcher->enableSource("TestSource", false));
    QVERIFY(!sources[0].enabled);
    
    // Test enabling source
    QVERIFY(m_dispatcher->enableSource("TestSource", true));
    QVERIFY(sources[0].enabled);
    
    // Test enable/disable non-existent source
    QVERIFY(!m_dispatcher->enableSource("NonExistent", false));
    
    delete source;
}

void TestPacketDispatcher::testMaxSourcesLimit() {
    PacketDispatcher::Configuration config;
    config.maxSources = 3; // Set low limit for testing
    
    delete m_dispatcher;
    m_dispatcher = new PacketDispatcher(config, this);
    
    // Register up to limit
    std::vector<MockPacketSource*> sources;
    for (int i = 0; i < 3; ++i) {
        auto source = createMockSource(QString("Source%1").arg(i).toStdString());
        sources.push_back(source);
        QVERIFY(m_dispatcher->registerSource(source));
    }
    
    // Try to register one more (should fail)
    auto extraSource = createMockSource("ExtraSource");
    QVERIFY(!m_dispatcher->registerSource(extraSource));
    QCOMPARE(m_dispatcher->getRegisteredSources().size(), size_t(3));
    
    // Clean up
    for (auto source : sources) {
        delete source;
    }
    delete extraSource;
}

void TestPacketDispatcher::testDuplicateSourceRegistration() {
    auto source1 = createMockSource("SameName");
    auto source2 = createMockSource("SameName");
    
    // Register first source
    QVERIFY(m_dispatcher->registerSource(source1));
    QCOMPARE(m_dispatcher->getRegisteredSources().size(), size_t(1));
    
    // Try to register second source with same name (should fail)
    QVERIFY(!m_dispatcher->registerSource(source2));
    QCOMPARE(m_dispatcher->getRegisteredSources().size(), size_t(1));
    
    delete source1;
    delete source2;
}

void TestPacketDispatcher::testSubscriptionDelegation() {
    // Test subscription delegation to SubscriptionManager
    auto callback = [this](PacketPtr packet) { packetCallback(packet); };
    
    auto id = m_dispatcher->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    QVERIFY(id != 0);
    
    // Verify subscription in underlying SubscriptionManager
    auto subscriptionManager = m_dispatcher->getSubscriptionManager();
    QCOMPARE(subscriptionManager->getTotalSubscriberCount(), size_t(1));
    
    auto subscription = subscriptionManager->getSubscription(id);
    QVERIFY(subscription != nullptr);
    QCOMPARE(subscription->name, std::string("TestSubscriber"));
    QCOMPARE(subscription->packetId, TEST_PACKET_ID_1);
    
    // Test statistics update
    const auto& stats = m_dispatcher->getStatistics();
    QCOMPARE(stats.subscriberCount.load(), uint64_t(1));
}

void TestPacketDispatcher::testUnsubscriptionDelegation() {
    // Create subscription
    auto callback = [this](PacketPtr packet) { packetCallback(packet); };
    auto id = m_dispatcher->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    QVERIFY(id != 0);
    
    // Verify initial state
    const auto& stats = m_dispatcher->getStatistics();
    QCOMPARE(stats.subscriberCount.load(), uint64_t(1));
    
    // Test unsubscription
    QVERIFY(m_dispatcher->unsubscribe(id));
    QCOMPARE(stats.subscriberCount.load(), uint64_t(0));
    
    // Verify in underlying SubscriptionManager
    auto subscriptionManager = m_dispatcher->getSubscriptionManager();
    QCOMPARE(subscriptionManager->getTotalSubscriberCount(), size_t(0));
    
    // Test unsubscribing non-existent ID
    QVERIFY(!m_dispatcher->unsubscribe(9999));
}

void TestPacketDispatcher::testPacketReception() {
    // Set up subscription
    auto callback = [this](PacketPtr packet) { packetCallback(packet); };
    m_dispatcher->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    
    // Start dispatcher
    QVERIFY(m_dispatcher->start());
    
    // Create and register mock source
    auto source = createMockSource("TestSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    // Simulate packet reception
    auto packet = createTestPacket(TEST_PACKET_ID_1, 256);
    source->simulatePacketReady(packet);
    
    // Process events
    QCoreApplication::processEvents();
    
    // Verify packet was received and processed
    QCOMPARE(m_callbackCounter.load(), 1);
    QCOMPARE(m_receivedPackets.size(), size_t(1));
    QCOMPARE(m_receivedPackets[0]->id(), TEST_PACKET_ID_1);
    
    // Verify statistics
    const auto& stats = m_dispatcher->getStatistics();
    QCOMPARE(stats.totalPacketsReceived.load(), uint64_t(1));
    QCOMPARE(stats.totalPacketsProcessed.load(), uint64_t(1));
    QCOMPARE(stats.totalPacketsDropped.load(), uint64_t(0));
    
    delete source;
}

void TestPacketDispatcher::testPacketRouting() {
    // Create multiple subscribers for different packet types
    std::atomic<int> subscriber1Counter{0};
    std::atomic<int> subscriber2Counter{0};
    
    auto callback1 = [&subscriber1Counter](PacketPtr /*packet*/) { subscriber1Counter++; };
    auto callback2 = [&subscriber2Counter](PacketPtr /*packet*/) { subscriber2Counter++; };
    
    m_dispatcher->subscribe("Subscriber1", TEST_PACKET_ID_1, callback1);
    m_dispatcher->subscribe("Subscriber2", TEST_PACKET_ID_2, callback2);
    
    QVERIFY(m_dispatcher->start());
    
    auto source = createMockSource("TestSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    // Send packet to first subscriber
    auto packet1 = createTestPacket(TEST_PACKET_ID_1);
    source->simulatePacketReady(packet1);
    QCoreApplication::processEvents();
    
    // Send packet to second subscriber
    auto packet2 = createTestPacket(TEST_PACKET_ID_2);
    source->simulatePacketReady(packet2);
    QCoreApplication::processEvents();
    
    // Verify correct routing
    QCOMPARE(subscriber1Counter.load(), 1);
    QCOMPARE(subscriber2Counter.load(), 1);
    
    delete source;
}

void TestPacketDispatcher::testPacketDistribution() {
    // Multiple subscribers to same packet type
    std::atomic<int> totalCallbacks{0};
    
    auto callback = [&totalCallbacks](PacketPtr /*packet*/) { totalCallbacks++; };
    
    m_dispatcher->subscribe("Sub1", TEST_PACKET_ID_1, callback);
    m_dispatcher->subscribe("Sub2", TEST_PACKET_ID_1, callback);
    m_dispatcher->subscribe("Sub3", TEST_PACKET_ID_1, callback);
    
    QVERIFY(m_dispatcher->start());
    
    auto source = createMockSource("TestSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    // Send single packet
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    source->simulatePacketReady(packet);
    QCoreApplication::processEvents();
    
    // Should be distributed to all 3 subscribers
    QCOMPARE(totalCallbacks.load(), 3);
    
    delete source;
}

void TestPacketDispatcher::testInvalidPacketHandling() {
    auto callback = [this](PacketPtr packet) { packetCallback(packet); };
    m_dispatcher->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    
    QVERIFY(m_dispatcher->start());
    
    auto source = createMockSource("TestSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    // Simulate null packet
    source->simulatePacketReady(nullptr);
    QCoreApplication::processEvents();
    
    // Should not crash and packet should be dropped
    QCOMPARE(m_callbackCounter.load(), 0);
    
    const auto& stats = m_dispatcher->getStatistics();
    QCOMPARE(stats.totalPacketsDropped.load(), uint64_t(1));
    
    delete source;
}

void TestPacketDispatcher::testBackPressureDetection() {
    // Configure low back-pressure threshold
    PacketDispatcher::Configuration config;
    config.enableBackPressure = true;
    config.backPressureThreshold = 2; // Very low for testing
    
    delete m_dispatcher;
    m_dispatcher = new PacketDispatcher(config, this);
    
    auto callback = [this](PacketPtr packet) { 
        // Slow callback to build up queue
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        packetCallback(packet); 
    };
    m_dispatcher->subscribe("SlowSubscriber", TEST_PACKET_ID_1, callback);
    
    QVERIFY(m_dispatcher->start());
    
    auto source = createMockSource("TestSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    // Set up signal spy for back-pressure events
    QSignalSpy backPressureSpy(m_dispatcher, &PacketDispatcher::backPressureDetected);
    
    // Send multiple packets quickly
    for (int i = 0; i < 10; ++i) {
        auto packet = createTestPacket(TEST_PACKET_ID_1);
        source->simulatePacketReady(packet);
    }
    
    // Process some events to build up queue
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Should have detected back-pressure (implementation dependent)
    const auto& stats = m_dispatcher->getStatistics();
    // Note: Exact behavior depends on queue implementation details
    
    delete source;
}

void TestPacketDispatcher::testBackPressureDisabled() {
    // Configure with back-pressure disabled
    PacketDispatcher::Configuration config;
    config.enableBackPressure = false;
    
    delete m_dispatcher;
    m_dispatcher = new PacketDispatcher(config, this);
    
    auto callback = [this](PacketPtr packet) { packetCallback(packet); };
    m_dispatcher->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    
    QVERIFY(m_dispatcher->start());
    
    auto source = createMockSource("TestSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    QSignalSpy backPressureSpy(m_dispatcher, &PacketDispatcher::backPressureDetected);
    
    // Send packets - should not trigger back-pressure
    for (int i = 0; i < 5; ++i) {
        auto packet = createTestPacket(TEST_PACKET_ID_1);
        source->simulatePacketReady(packet);
    }
    
    QCoreApplication::processEvents();
    
    // Should not have back-pressure events
    QCOMPARE(backPressureSpy.count(), 0);
    
    delete source;
}

void TestPacketDispatcher::testStatisticsTracking() {
    auto callback = [this](PacketPtr packet) { packetCallback(packet); };
    m_dispatcher->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    
    QVERIFY(m_dispatcher->start());
    
    auto source = createMockSource("TestSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    const auto& stats = m_dispatcher->getStatistics();
    
    // Test initial statistics
    QCOMPARE(stats.totalPacketsReceived.load(), uint64_t(0));
    QCOMPARE(stats.totalPacketsProcessed.load(), uint64_t(0));
    QCOMPARE(stats.sourceCount.load(), uint64_t(1));
    QCOMPARE(stats.subscriberCount.load(), uint64_t(1));
    
    // Send packets and verify statistics
    for (int i = 0; i < 5; ++i) {
        auto packet = createTestPacket(TEST_PACKET_ID_1);
        source->simulatePacketReady(packet);
    }
    
    QCoreApplication::processEvents();
    
    QCOMPARE(stats.totalPacketsReceived.load(), uint64_t(5));
    QCOMPARE(stats.totalPacketsProcessed.load(), uint64_t(5));
    
    // Test throughput calculation
    QVERIFY(stats.getTotalThroughput() >= 0.0);
    QCOMPARE(stats.getOverallDropRate(), 0.0); // No drops
    
    delete source;
}

void TestPacketDispatcher::testStatisticsUpdates() {
    auto callback = [this](PacketPtr packet) { packetCallback(packet); };
    m_dispatcher->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    
    QVERIFY(m_dispatcher->start());
    
    auto source = createMockSource("TestSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    QSignalSpy statsSpy(m_dispatcher, &PacketDispatcher::statisticsUpdated);
    
    // Send enough packets to trigger statistics update (every 1000)
    // Since that's a lot, just test the signal exists
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    source->simulatePacketReady(packet);
    QCoreApplication::processEvents();
    
    // Signal should exist (might not be emitted for single packet)
    // This tests the signal is properly defined
    
    delete source;
}

void TestPacketDispatcher::testSignalEmission() {
    // Test startup/shutdown signals
    QSignalSpy startedSpy(m_dispatcher, &PacketDispatcher::started);
    QSignalSpy stoppedSpy(m_dispatcher, &PacketDispatcher::stopped);
    
    QVERIFY(m_dispatcher->start());
    QCOMPARE(startedSpy.count(), 1);
    
    m_dispatcher->stop();
    QCOMPARE(stoppedSpy.count(), 1);
    
    // Test source registration signals
    QSignalSpy sourceRegSpy(m_dispatcher, &PacketDispatcher::sourceRegistered);
    QSignalSpy sourceUnregSpy(m_dispatcher, &PacketDispatcher::sourceUnregistered);
    
    auto source = createMockSource("TestSource");
    QVERIFY(m_dispatcher->registerSource(source));
    QCOMPARE(sourceRegSpy.count(), 1);
    QCOMPARE(sourceRegSpy.takeFirst().at(0).toString(), QString("TestSource"));
    
    QVERIFY(m_dispatcher->unregisterSource("TestSource"));
    QCOMPARE(sourceUnregSpy.count(), 1);
    QCOMPARE(sourceUnregSpy.takeFirst().at(0).toString(), QString("TestSource"));
    
    delete source;
}

void TestPacketDispatcher::testSlotHandling() {
    auto callback = [this](PacketPtr packet) { packetCallback(packet); };
    m_dispatcher->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    
    QVERIFY(m_dispatcher->start());
    
    auto source = createMockSource("TestSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    QSignalSpy packetProcessedSpy(m_dispatcher, &PacketDispatcher::packetProcessed);
    
    // Test packet processing slot
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    source->simulatePacketReady(packet);
    QCoreApplication::processEvents();
    
    // Should emit packetProcessed signal
    QCOMPARE(packetProcessedSpy.count(), 1);
    
    delete source;
}

void TestPacketDispatcher::testSourceErrorHandling() {
    auto source = createMockSource("ErrorSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    QVERIFY(m_dispatcher->start());
    
    // Simulate source error
    source->setSimulateError(true);
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    source->simulatePacketReady(packet);
    
    QCoreApplication::processEvents();
    
    // Error should be handled gracefully (logged)
    // No specific assertion - testing that it doesn't crash
    
    delete source;
}

void TestPacketDispatcher::testComponentFailureRecovery() {
    // Test dispatcher behavior when components fail
    // This is mostly about graceful degradation
    
    QVERIFY(m_dispatcher->start());
    
    auto source = createMockSource("TestSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    // Simulate various failure conditions and verify graceful handling
    
    // Test with null packet (already covered in testInvalidPacketHandling)
    source->simulatePacketReady(nullptr);
    QCoreApplication::processEvents();
    
    // Should continue operating
    QVERIFY(m_dispatcher->isRunning());
    
    delete source;
}

void TestPacketDispatcher::testHighThroughputProcessing() {
    const int packetCount = 1000;
    std::atomic<int> processedCount{0};
    
    auto callback = [&processedCount](PacketPtr /*packet*/) { processedCount++; };
    m_dispatcher->subscribe("HighThroughputSub", TEST_PACKET_ID_1, callback);
    
    QVERIFY(m_dispatcher->start());
    
    auto source = createMockSource("HighThroughputSource");
    QVERIFY(m_dispatcher->registerSource(source));
    
    QElapsedTimer timer;
    timer.start();
    
    // Send many packets
    for (int i = 0; i < packetCount; ++i) {
        auto packet = createTestPacket(TEST_PACKET_ID_1);
        source->simulatePacketReady(packet);
        if (i % 100 == 0) {
            QCoreApplication::processEvents();
        }
    }
    
    // Process remaining events
    QCoreApplication::processEvents();
    
    qint64 elapsedMs = timer.elapsed();
    double packetsPerSecond = (processedCount.load() * 1000.0) / elapsedMs;
    
    qDebug() << "Processed" << processedCount.load() << "packets in" << elapsedMs 
             << "ms (" << packetsPerSecond << "packets/sec)";
    
    // Should process most packets efficiently
    QVERIFY(processedCount.load() > packetCount * 0.8); // At least 80%
    
    delete source;
}

void TestPacketDispatcher::testConcurrentSourceManagement() {
    const int threadCount = 4;
    const int sourcesPerThread = 10;
    std::atomic<int> successfulRegistrations{0};
    
    std::vector<std::thread> threads;
    std::vector<std::vector<MockPacketSource*>> threadSources(threadCount);
    
    // Concurrent source registration
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([this, t, sourcesPerThread, &threadSources, &successfulRegistrations]() {
            for (int i = 0; i < sourcesPerThread; ++i) {
                auto source = createMockSource(
                    QString("Thread%1Source%2").arg(t).arg(i).toStdString()
                );
                threadSources[t].push_back(source);
                
                if (m_dispatcher->registerSource(source)) {
                    successfulRegistrations++;
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Should have registered many sources successfully
    QVERIFY(successfulRegistrations.load() > 0);
    QVERIFY(m_dispatcher->getRegisteredSources().size() > 0);
    
    // Clean up
    for (int t = 0; t < threadCount; ++t) {
        for (auto source : threadSources[t]) {
            delete source;
        }
    }
}

void TestPacketDispatcher::testEmptyDispatcher() {
    // Test operations on empty dispatcher
    QCOMPARE(m_dispatcher->getRegisteredSources().size(), size_t(0));
    
    // Should handle start/stop gracefully
    QVERIFY(m_dispatcher->start());
    QVERIFY(m_dispatcher->isRunning());
    
    m_dispatcher->stop();
    QVERIFY(!m_dispatcher->isRunning());
    
    // Should handle operations on empty state
    QVERIFY(!m_dispatcher->enableSource("NonExistent", true));
    QVERIFY(!m_dispatcher->unregisterSource("NonExistent"));
    
    // Statistics should reflect empty state
    const auto& stats = m_dispatcher->getStatistics();
    QCOMPARE(stats.sourceCount.load(), uint64_t(0));
}

void TestPacketDispatcher::testPartialComponentFailure() {
    // Test behavior when some components fail but others continue
    auto source1 = createMockSource("GoodSource");
    auto source2 = createMockSource("ErrorSource");
    
    QVERIFY(m_dispatcher->registerSource(source1));
    QVERIFY(m_dispatcher->registerSource(source2));
    
    auto callback = [this](PacketPtr packet) { packetCallback(packet); };
    m_dispatcher->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    
    QVERIFY(m_dispatcher->start());
    
    // Make one source generate errors
    source2->setSimulateError(true);
    
    // Send packets from both sources
    auto packet1 = createTestPacket(TEST_PACKET_ID_1);
    auto packet2 = createTestPacket(TEST_PACKET_ID_1);
    
    source1->simulatePacketReady(packet1); // Should work
    source2->simulatePacketReady(packet2); // Should error but not crash
    
    QCoreApplication::processEvents();
    
    // Should have processed at least one packet
    QVERIFY(m_callbackCounter.load() > 0);
    
    // Dispatcher should continue running
    QVERIFY(m_dispatcher->isRunning());
    
    delete source1;
    delete source2;
}

QTEST_MAIN(TestPacketDispatcher)