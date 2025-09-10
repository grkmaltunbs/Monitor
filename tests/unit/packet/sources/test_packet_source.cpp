#include <QtTest/QTest>
#include <QSignalSpy>
#include <QObject>
#include <thread>
#include <chrono>
#include "packet/sources/packet_source.h"
#include "packet/core/packet_factory.h"
#include "core/application.h"

using namespace Monitor::Packet;
using namespace Monitor::Memory;

// Test implementation of PacketSource for testing the abstract base class
class TestablePacketSource : public PacketSource {
    Q_OBJECT

public:
    explicit TestablePacketSource(const Configuration& config, QObject* parent = nullptr)
        : PacketSource(config, parent)
        , m_startResult(true)
        , m_resumeResult(true)
        , m_startCalled(false)
        , m_stopCalled(false)
        , m_pauseCalled(false)
        , m_resumeCalled(false)
    {
    }
    
    // Control test behavior
    void setStartResult(bool result) { m_startResult = result; }
    void setResumeResult(bool result) { m_resumeResult = result; }
    
    // Check if methods were called
    bool wasStartCalled() const { return m_startCalled; }
    bool wasStopCalled() const { return m_stopCalled; }
    bool wasPauseCalled() const { return m_pauseCalled; }
    bool wasResumeCalled() const { return m_resumeCalled; }
    
    void resetCallFlags() {
        m_startCalled = false;
        m_stopCalled = false;
        m_pauseCalled = false;
        m_resumeCalled = false;
    }
    
    // Expose protected methods for testing
    void testDeliverPacket(PacketPtr packet) { deliverPacket(packet); }
    void testReportError(const std::string& error) { reportError(error); }
    void testSetState(State state) { setState(state); }
    bool testShouldThrottle() const { return shouldThrottle(); }

protected:
    bool doStart() override {
        m_startCalled = true;
        return m_startResult;
    }
    
    void doStop() override {
        m_stopCalled = true;
    }
    
    void doPause() override {
        m_pauseCalled = true;
    }
    
    bool doResume() override {
        m_resumeCalled = true;
        return m_resumeResult;
    }

private:
    bool m_startResult;
    bool m_resumeResult;
    bool m_startCalled;
    bool m_stopCalled;
    bool m_pauseCalled;
    bool m_resumeCalled;
};

class TestPacketSource : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Configuration tests
    void testConfiguration();
    void testConfigurationDefaults();
    void testNamedConfiguration();

    // Statistics tests
    void testStatisticsInitialization();
    void testStatisticsCopyConstructor();
    void testStatisticsAssignmentOperator();
    void testStatisticsRateCalculations();

    // Basic functionality tests
    void testSourceConstruction();
    void testSourceName();
    void testInitialState();
    void testComponentSetters();

    // State management tests
    void testStateTransitions();
    void testStartStop();
    void testPauseResume();
    void testStartFailure();
    void testResumeFailure();

    // Signal emission tests
    void testStateChangeSignals();
    void testStartStopSignals();
    void testPauseResumeSignals();
    void testErrorSignals();
    void testPacketSignals();

    // Packet delivery tests
    void testPacketDelivery();
    void testPacketCallback();
    void testPacketStatistics();
    void testNullPacketHandling();

    // Error handling tests
    void testErrorReporting();
    void testErrorCallback();
    void testErrorStateTransition();

    // Rate limiting tests
    void testRateLimiting();
    void testThrottling();
    void testUnlimitedRate();

    // Thread safety tests
    void testConcurrentStateChanges();
    void testConcurrentPacketDelivery();
    void testConcurrentStatisticsAccess();

    // Edge cases
    void testInvalidStateTransitions();
    void testStatisticsOverflow();
    void testLongRunningOperation();

private:
    Core::Application* m_app = nullptr;
    MemoryPoolManager* m_memoryManager = nullptr;
    PacketFactory* m_packetFactory = nullptr;
    TestablePacketSource* m_source = nullptr;
    
    PacketSource::Configuration createTestConfig(const std::string& name = "TestSource");
    void setupMemoryPools();
};

void TestPacketSource::initTestCase() {
    m_app = Core::Application::instance();
    QVERIFY(m_app != nullptr);
    
    m_memoryManager = m_app->memoryManager();
    QVERIFY(m_memoryManager != nullptr);
    
    setupMemoryPools();
    
    m_packetFactory = new PacketFactory(m_memoryManager, this);
    QVERIFY(m_packetFactory != nullptr);
}

void TestPacketSource::cleanupTestCase() {
    delete m_packetFactory;
    m_packetFactory = nullptr;
}

void TestPacketSource::init() {
    auto config = createTestConfig();
    m_source = new TestablePacketSource(config, this);
    QVERIFY(m_source != nullptr);
    
    m_source->setPacketFactory(m_packetFactory);
}

void TestPacketSource::cleanup() {
    delete m_source;
    m_source = nullptr;
}

void TestPacketSource::setupMemoryPools() {
    m_memoryManager->createPool("SmallObjects", 64, 1000);
    m_memoryManager->createPool("MediumObjects", 512, 1000);
    m_memoryManager->createPool("WidgetData", 1024, 1000);
    m_memoryManager->createPool("TestFramework", 2048, 1000);
    m_memoryManager->createPool("PacketBuffer", 4096, 1000);
    m_memoryManager->createPool("LargeObjects", 8192, 1000);
}

PacketSource::Configuration TestPacketSource::createTestConfig(const std::string& name) {
    PacketSource::Configuration config(name);
    config.autoStart = false;
    config.bufferSize = 500;
    config.maxPacketRate = 1000; // 1000 packets/second max
    config.enableStatistics = true;
    return config;
}

void TestPacketSource::testConfiguration() {
    auto config = createTestConfig("CustomSource");
    TestablePacketSource source(config, this);
    
    QCOMPARE(source.getName(), std::string("CustomSource"));
    QCOMPARE(source.getConfiguration().name, std::string("CustomSource"));
    QCOMPARE(source.getConfiguration().bufferSize, uint32_t(500));
    QCOMPARE(source.getConfiguration().maxPacketRate, uint32_t(1000));
    QVERIFY(source.getConfiguration().enableStatistics);
    QVERIFY(!source.getConfiguration().autoStart);
}

void TestPacketSource::testConfigurationDefaults() {
    PacketSource::Configuration defaultConfig;
    
    QVERIFY(defaultConfig.name.empty());
    QVERIFY(!defaultConfig.autoStart);
    QCOMPARE(defaultConfig.bufferSize, uint32_t(1000));
    QCOMPARE(defaultConfig.maxPacketRate, uint32_t(0)); // Unlimited
    QVERIFY(defaultConfig.enableStatistics);
}

void TestPacketSource::testNamedConfiguration() {
    PacketSource::Configuration namedConfig("NamedSource");
    
    QCOMPARE(namedConfig.name, std::string("NamedSource"));
    QVERIFY(!namedConfig.autoStart);
    QCOMPARE(namedConfig.bufferSize, uint32_t(1000));
    QCOMPARE(namedConfig.maxPacketRate, uint32_t(0));
    QVERIFY(namedConfig.enableStatistics);
}

void TestPacketSource::testStatisticsInitialization() {
    PacketSource::Statistics stats;
    
    QCOMPARE(stats.packetsGenerated.load(), uint64_t(0));
    QCOMPARE(stats.packetsDelivered.load(), uint64_t(0));
    QCOMPARE(stats.packetsDropped.load(), uint64_t(0));
    QCOMPARE(stats.bytesGenerated.load(), uint64_t(0));
    QCOMPARE(stats.errorCount.load(), uint64_t(0));
    
    // Start time should be recent
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - stats.startTime).count();
    QVERIFY(elapsed < 100); // Less than 100ms ago
}

void TestPacketSource::testStatisticsCopyConstructor() {
    PacketSource::Statistics original;
    original.packetsGenerated = 100;
    original.packetsDelivered = 95;
    original.packetsDropped = 5;
    original.bytesGenerated = 10000;
    original.errorCount = 2;
    
    PacketSource::Statistics copy(original);
    
    QCOMPARE(copy.packetsGenerated.load(), uint64_t(100));
    QCOMPARE(copy.packetsDelivered.load(), uint64_t(95));
    QCOMPARE(copy.packetsDropped.load(), uint64_t(5));
    QCOMPARE(copy.bytesGenerated.load(), uint64_t(10000));
    QCOMPARE(copy.errorCount.load(), uint64_t(2));
    QCOMPARE(copy.startTime, original.startTime);
    QCOMPARE(copy.lastPacketTime, original.lastPacketTime);
}

void TestPacketSource::testStatisticsAssignmentOperator() {
    PacketSource::Statistics original;
    original.packetsGenerated = 200;
    original.packetsDelivered = 190;
    original.bytesGenerated = 20000;
    
    PacketSource::Statistics assigned;
    assigned = original;
    
    QCOMPARE(assigned.packetsGenerated.load(), uint64_t(200));
    QCOMPARE(assigned.packetsDelivered.load(), uint64_t(190));
    QCOMPARE(assigned.bytesGenerated.load(), uint64_t(20000));
    
    // Test self-assignment
    assigned = assigned;
    QCOMPARE(assigned.packetsGenerated.load(), uint64_t(200));
}

void TestPacketSource::testStatisticsRateCalculations() {
    PacketSource::Statistics stats;
    
    // Initial rates should be zero
    QCOMPARE(stats.getPacketRate(), 0.0);
    QCOMPARE(stats.getByteRate(), 0.0);
    QCOMPARE(stats.getDropRate(), 0.0);
    
    // Set some values and wait a bit
    stats.packetsDelivered = 100;
    stats.packetsGenerated = 110;
    stats.packetsDropped = 10;
    stats.bytesGenerated = 50000;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Rates should be calculated based on elapsed time
    double packetRate = stats.getPacketRate();
    double byteRate = stats.getByteRate();
    double dropRate = stats.getDropRate();
    
    QVERIFY(packetRate > 0);
    QVERIFY(byteRate > 0);
    QCOMPARE(dropRate, 10.0 / 110.0); // 10 dropped out of 110 generated
}

void TestPacketSource::testSourceConstruction() {
    QVERIFY(m_source != nullptr);
    QCOMPARE(m_source->getName(), std::string("TestSource"));
    QCOMPARE(m_source->getState(), PacketSource::State::Stopped);
    QVERIFY(m_source->isStopped());
    QVERIFY(!m_source->isRunning());
    QVERIFY(!m_source->hasError());
}

void TestPacketSource::testSourceName() {
    auto config1 = createTestConfig("Source1");
    TestablePacketSource source1(config1);
    QCOMPARE(source1.getName(), std::string("Source1"));
    
    auto config2 = createTestConfig("AnotherSource");
    TestablePacketSource source2(config2);
    QCOMPARE(source2.getName(), std::string("AnotherSource"));
}

void TestPacketSource::testInitialState() {
    QCOMPARE(m_source->getState(), PacketSource::State::Stopped);
    QVERIFY(m_source->isStopped());
    QVERIFY(!m_source->isRunning());
    QVERIFY(!m_source->hasError());
    
    // Test state string conversion
    QCOMPARE(QString(stateToString(PacketSource::State::Stopped)), QString("Stopped"));
    QCOMPARE(QString(stateToString(PacketSource::State::Running)), QString("Running"));
    QCOMPARE(QString(stateToString(PacketSource::State::Error)), QString("Error"));
}

void TestPacketSource::testComponentSetters() {
    // Test packet factory setter
    m_source->setPacketFactory(nullptr);
    m_source->setPacketFactory(m_packetFactory);
    
    // Test event dispatcher setter
    auto dispatcher = m_app->eventDispatcher();
    m_source->setEventDispatcher(dispatcher);
    m_source->setEventDispatcher(nullptr);
    
    // Test callback setters
    bool packetCallbackCalled = false;
    bool errorCallbackCalled = false;
    
    m_source->setPacketCallback([&packetCallbackCalled](PacketPtr) {
        packetCallbackCalled = true;
    });
    
    m_source->setErrorCallback([&errorCallbackCalled](const std::string&) {
        errorCallbackCalled = true;
    });
    
    // Create and deliver a test packet to trigger callback
    auto result = m_packetFactory->createPacket(1, nullptr, 100);
    if (result.success) {
        m_source->testDeliverPacket(result.packet);
        QVERIFY(packetCallbackCalled);
    }
    
    // Trigger error callback
    m_source->testReportError("Test error");
    QVERIFY(errorCallbackCalled);
}

void TestPacketSource::testStateTransitions() {
    QSignalSpy stateSpy(m_source, &PacketSource::stateChanged);
    
    // Test direct state setting
    m_source->testSetState(PacketSource::State::Starting);
    QCOMPARE(m_source->getState(), PacketSource::State::Starting);
    QCOMPARE(stateSpy.count(), 1);
    
    auto stateArgs = stateSpy.takeFirst();
    QCOMPARE(stateArgs.at(0).value<PacketSource::State>(), PacketSource::State::Stopped);
    QCOMPARE(stateArgs.at(1).value<PacketSource::State>(), PacketSource::State::Starting);
    
    // Setting same state should not emit signal
    m_source->testSetState(PacketSource::State::Starting);
    QCOMPARE(stateSpy.count(), 0);
    
    // Test all state transitions
    std::vector<PacketSource::State> states = {
        PacketSource::State::Running,
        PacketSource::State::Pausing,
        PacketSource::State::Paused,
        PacketSource::State::Stopping,
        PacketSource::State::Stopped,
        PacketSource::State::Error
    };
    
    for (auto state : states) {
        m_source->testSetState(state);
        QCOMPARE(m_source->getState(), state);
    }
}

void TestPacketSource::testStartStop() {
    QSignalSpy startedSpy(m_source, &PacketSource::started);
    QSignalSpy stoppedSpy(m_source, &PacketSource::stopped);
    QSignalSpy stateSpy(m_source, &PacketSource::stateChanged);
    
    // Test start
    bool result = m_source->start();
    QVERIFY(result);
    QCOMPARE(m_source->getState(), PacketSource::State::Running);
    QVERIFY(m_source->isRunning());
    QVERIFY(m_source->wasStartCalled());
    
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(stateSpy.count() >= 2); // Starting -> Running
    
    // Starting already running source should succeed immediately
    bool result2 = m_source->start();
    QVERIFY(result2);
    
    // Test stop
    m_source->stop();
    QCOMPARE(m_source->getState(), PacketSource::State::Stopped);
    QVERIFY(m_source->isStopped());
    QVERIFY(m_source->wasStopCalled());
    
    QCOMPARE(stoppedSpy.count(), 1);
    
    // Stopping already stopped source should not crash
    m_source->stop();
    QCOMPARE(stoppedSpy.count(), 1); // No additional signal
}

void TestPacketSource::testPauseResume() {
    QSignalSpy pausedSpy(m_source, &PacketSource::paused);
    QSignalSpy resumedSpy(m_source, &PacketSource::resumed);
    
    // Start the source first
    m_source->start();
    QCOMPARE(m_source->getState(), PacketSource::State::Running);
    
    // Test pause
    m_source->pause();
    QCOMPARE(m_source->getState(), PacketSource::State::Paused);
    QVERIFY(m_source->wasPauseCalled());
    QCOMPARE(pausedSpy.count(), 1);
    
    // Test resume
    m_source->resume();
    QCOMPARE(m_source->getState(), PacketSource::State::Running);
    QVERIFY(m_source->wasResumeCalled());
    QCOMPARE(resumedSpy.count(), 1);
    
    // Test pause from stopped state (should do nothing)
    m_source->stop();
    m_source->resetCallFlags();
    m_source->pause();
    QVERIFY(!m_source->wasPauseCalled());
    
    // Test resume from stopped state (should do nothing)
    m_source->resume();
    QVERIFY(!m_source->wasResumeCalled());
}

void TestPacketSource::testStartFailure() {
    QSignalSpy errorSpy(m_source, &PacketSource::error);
    
    // Configure source to fail on start
    m_source->setStartResult(false);
    
    bool result = m_source->start();
    QVERIFY(!result);
    QCOMPARE(m_source->getState(), PacketSource::State::Error);
    QVERIFY(m_source->hasError());
    
    QCOMPARE(errorSpy.count(), 1);
    auto errorArgs = errorSpy.takeFirst();
    QVERIFY(!errorArgs.at(0).toString().isEmpty());
}

void TestPacketSource::testResumeFailure() {
    QSignalSpy errorSpy(m_source, &PacketSource::error);
    
    // Start and pause
    m_source->start();
    m_source->pause();
    QCOMPARE(m_source->getState(), PacketSource::State::Paused);
    
    // Configure to fail on resume
    m_source->setResumeResult(false);
    
    m_source->resume();
    QCOMPARE(m_source->getState(), PacketSource::State::Error);
    QCOMPARE(errorSpy.count(), 1);
}

void TestPacketSource::testStateChangeSignals() {
    QSignalSpy stateSpy(m_source, &PacketSource::stateChanged);
    
    // Test various state changes
    m_source->testSetState(PacketSource::State::Starting);
    m_source->testSetState(PacketSource::State::Running);
    m_source->testSetState(PacketSource::State::Paused);
    m_source->testSetState(PacketSource::State::Error);
    
    QCOMPARE(stateSpy.count(), 4);
    
    // Verify signal arguments
    for (int i = 0; i < stateSpy.count(); ++i) {
        auto args = stateSpy.at(i);
        QVERIFY(args.size() == 2);
        // Old and new states should be different
        QVERIFY(args.at(0).value<PacketSource::State>() != args.at(1).value<PacketSource::State>());
    }
}

void TestPacketSource::testStartStopSignals() {
    QSignalSpy startedSpy(m_source, &PacketSource::started);
    QSignalSpy stoppedSpy(m_source, &PacketSource::stopped);
    
    // Test multiple start/stop cycles
    for (int i = 0; i < 3; ++i) {
        m_source->start();
        m_source->stop();
    }
    
    QCOMPARE(startedSpy.count(), 3);
    QCOMPARE(stoppedSpy.count(), 3);
}

void TestPacketSource::testPauseResumeSignals() {
    QSignalSpy pausedSpy(m_source, &PacketSource::paused);
    QSignalSpy resumedSpy(m_source, &PacketSource::resumed);
    
    m_source->start();
    
    // Test multiple pause/resume cycles
    for (int i = 0; i < 3; ++i) {
        m_source->pause();
        m_source->resume();
    }
    
    QCOMPARE(pausedSpy.count(), 3);
    QCOMPARE(resumedSpy.count(), 3);
}

void TestPacketSource::testErrorSignals() {
    QSignalSpy errorSpy(m_source, &PacketSource::error);
    
    m_source->testReportError("Test error 1");
    m_source->testReportError("Test error 2");
    
    QCOMPARE(errorSpy.count(), 2);
    
    auto error1 = errorSpy.at(0).at(0).toString();
    auto error2 = errorSpy.at(1).at(0).toString();
    
    QCOMPARE(error1, QString("Test error 1"));
    QCOMPARE(error2, QString("Test error 2"));
}

void TestPacketSource::testPacketSignals() {
    QSignalSpy packetSpy(m_source, &PacketSource::packetReady);
    
    // Create and deliver test packets
    auto result1 = m_packetFactory->createPacket(1, nullptr, 100);
    auto result2 = m_packetFactory->createPacket(2, nullptr, 200);
    
    QVERIFY(result1.success);
    QVERIFY(result2.success);
    
    m_source->testDeliverPacket(result1.packet);
    m_source->testDeliverPacket(result2.packet);
    
    QCOMPARE(packetSpy.count(), 2);
    
    // Verify signal arguments
    auto packet1 = packetSpy.at(0).at(0).value<PacketPtr>();
    auto packet2 = packetSpy.at(1).at(0).value<PacketPtr>();
    
    QVERIFY(packet1 != nullptr);
    QVERIFY(packet2 != nullptr);
    QCOMPARE(packet1->id(), PacketId(1));
    QCOMPARE(packet2->id(), PacketId(2));
}

void TestPacketSource::testPacketDelivery() {
    auto result = m_packetFactory->createPacket(42, nullptr, 256);
    QVERIFY(result.success);
    
    // Check initial statistics
    const auto& stats = m_source->getStatistics();
    uint64_t initialDelivered = stats.packetsDelivered.load();
    uint64_t initialBytes = stats.bytesGenerated.load();
    
    // Deliver packet
    m_source->testDeliverPacket(result.packet);
    
    // Check updated statistics
    QCOMPARE(stats.packetsDelivered.load(), initialDelivered + 1);
    QVERIFY(stats.bytesGenerated.load() > initialBytes);
    
    // Last packet time should be recent
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - stats.lastPacketTime).count();
    QVERIFY(elapsed < 100); // Less than 100ms ago
}

void TestPacketSource::testPacketCallback() {
    bool callbackCalled = false;
    PacketPtr callbackPacket = nullptr;
    
    m_source->setPacketCallback([&](PacketPtr packet) {
        callbackCalled = true;
        callbackPacket = packet;
    });
    
    auto result = m_packetFactory->createPacket(99, nullptr, 128);
    QVERIFY(result.success);
    
    m_source->testDeliverPacket(result.packet);
    
    QVERIFY(callbackCalled);
    QVERIFY(callbackPacket != nullptr);
    QCOMPARE(callbackPacket->id(), PacketId(99));
}

void TestPacketSource::testPacketStatistics() {
    const auto& stats = m_source->getStatistics();
    
    // Initial state
    QCOMPARE(stats.packetsDelivered.load(), uint64_t(0));
    QCOMPARE(stats.bytesGenerated.load(), uint64_t(0));
    
    // Deliver several packets
    std::vector<size_t> sizes = {100, 200, 300, 400};
    uint64_t expectedBytes = 0;
    
    for (size_t size : sizes) {
        auto result = m_packetFactory->createPacket(1, nullptr, size);
        QVERIFY(result.success);
        
        m_source->testDeliverPacket(result.packet);
        expectedBytes += result.packet->totalSize();
    }
    
    // Check statistics
    QCOMPARE(stats.packetsDelivered.load(), uint64_t(4));
    QCOMPARE(stats.bytesGenerated.load(), expectedBytes);
    
    // Check rate calculations
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    double packetRate = stats.getPacketRate();
    double byteRate = stats.getByteRate();
    
    QVERIFY(packetRate > 0);
    QVERIFY(byteRate > 0);
}

void TestPacketSource::testNullPacketHandling() {
    const auto& stats = m_source->getStatistics();
    uint64_t initialErrors = stats.errorCount.load();
    
    // Deliver null packet
    m_source->testDeliverPacket(nullptr);
    
    // Error count should increase
    QCOMPARE(stats.errorCount.load(), initialErrors + 1);
}

void TestPacketSource::testErrorReporting() {
    QSignalSpy errorSpy(m_source, &PacketSource::error);
    
    const auto& stats = m_source->getStatistics();
    uint64_t initialErrors = stats.errorCount.load();
    
    std::string errorMessage = "Critical system failure";
    m_source->testReportError(errorMessage);
    
    // Check state change
    QCOMPARE(m_source->getState(), PacketSource::State::Error);
    QVERIFY(m_source->hasError());
    
    // Check error count
    QCOMPARE(stats.errorCount.load(), initialErrors + 1);
    
    // Check signal emission
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.first().first().toString(), QString::fromStdString(errorMessage));
}

void TestPacketSource::testErrorCallback() {
    bool callbackCalled = false;
    std::string callbackError;
    
    m_source->setErrorCallback([&](const std::string& error) {
        callbackCalled = true;
        callbackError = error;
    });
    
    std::string testError = "Callback test error";
    m_source->testReportError(testError);
    
    QVERIFY(callbackCalled);
    QCOMPARE(callbackError, testError);
}

void TestPacketSource::testErrorStateTransition() {
    // Start source
    m_source->start();
    QCOMPARE(m_source->getState(), PacketSource::State::Running);
    
    // Report error
    m_source->testReportError("Runtime error");
    
    // Should transition to error state
    QCOMPARE(m_source->getState(), PacketSource::State::Error);
    QVERIFY(m_source->hasError());
    
    // Should not be able to start from error state
    bool result = m_source->start();
    QVERIFY(!result);
}

void TestPacketSource::testRateLimiting() {
    // Create source with rate limit
    auto config = createTestConfig("RateLimitedSource");
    config.maxPacketRate = 10; // 10 packets/second
    
    TestablePacketSource rateLimitedSource(config, this);
    
    // Initially should not throttle
    QVERIFY(!rateLimitedSource.testShouldThrottle());
    
    // TODO: This test needs time-based simulation to properly test rate limiting
    // For now, we test that the throttling method exists and returns false initially
}

void TestPacketSource::testThrottling() {
    // Test throttling logic
    QVERIFY(!m_source->testShouldThrottle()); // Should not throttle initially
    
    // Test with unlimited rate (0)
    auto config = createTestConfig("UnlimitedSource");
    config.maxPacketRate = 0;
    
    TestablePacketSource unlimitedSource(config, this);
    QVERIFY(!unlimitedSource.testShouldThrottle()); // Should never throttle
}

void TestPacketSource::testUnlimitedRate() {
    auto config = createTestConfig("UnlimitedSource");
    config.maxPacketRate = 0; // Unlimited
    
    TestablePacketSource unlimitedSource(config, this);
    
    // Should never throttle with unlimited rate
    QVERIFY(!unlimitedSource.testShouldThrottle());
}

void TestPacketSource::testConcurrentStateChanges() {
    const int numThreads = 4;
    const int operationsPerThread = 100;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, operationsPerThread]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                // Randomly choose operation
                int operation = j % 4;
                switch (operation) {
                    case 0: m_source->start(); break;
                    case 1: m_source->stop(); break;
                    case 2: m_source->pause(); break;
                    case 3: m_source->resume(); break;
                }
                std::this_thread::yield();
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Source should be in a valid state after concurrent operations
    auto finalState = m_source->getState();
    QVERIFY(finalState != static_cast<PacketSource::State>(-1)); // Should be valid state
}

void TestPacketSource::testConcurrentPacketDelivery() {
    const int numThreads = 4;
    const int packetsPerThread = 250;
    
    std::vector<std::thread> threads;
    std::atomic<int> totalPackets(0);
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, packetsPerThread, &totalPackets]() {
            for (int j = 0; j < packetsPerThread; ++j) {
                auto result = m_packetFactory->createPacket(i * 1000 + j, nullptr, 64);
                if (result.success) {
                    m_source->testDeliverPacket(result.packet);
                    totalPackets++;
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Check that all packets were processed
    const auto& stats = m_source->getStatistics();
    QCOMPARE(stats.packetsDelivered.load(), static_cast<uint64_t>(totalPackets.load()));
    
    qDebug() << "Concurrent packet delivery: Delivered" << totalPackets.load() 
             << "packets across" << numThreads << "threads";
}

void TestPacketSource::testConcurrentStatisticsAccess() {
    const int numThreads = 4;
    const int accessesPerThread = 1000;
    
    std::vector<std::thread> threads;
    
    // One thread delivers packets while others read statistics
    threads.emplace_back([this]() {
        for (int i = 0; i < 100; ++i) {
            auto result = m_packetFactory->createPacket(i, nullptr, 128);
            if (result.success) {
                m_source->testDeliverPacket(result.packet);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    // Reader threads
    for (int i = 0; i < numThreads - 1; ++i) {
        threads.emplace_back([this, accessesPerThread]() {
            for (int j = 0; j < accessesPerThread; ++j) {
                const auto& stats = m_source->getStatistics();
                volatile uint64_t packets = stats.packetsDelivered.load();
                volatile uint64_t bytes = stats.bytesGenerated.load();
                volatile double rate = stats.getPacketRate();
                (void)packets; (void)bytes; (void)rate; // Prevent optimization
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Should not crash - this tests thread safety of statistics access
    QVERIFY(true);
}

void TestPacketSource::testInvalidStateTransitions() {
    // Test starting from invalid states
    m_source->testSetState(PacketSource::State::Error);
    bool result = m_source->start();
    QVERIFY(!result); // Should fail to start from error state
    
    m_source->testSetState(PacketSource::State::Starting);
    result = m_source->start();
    QVERIFY(!result); // Should fail to start from starting state
    
    m_source->testSetState(PacketSource::State::Stopping);
    result = m_source->start();
    QVERIFY(!result); // Should fail to start from stopping state
}

void TestPacketSource::testStatisticsOverflow() {
    // Test with very large numbers (simulate overflow conditions)
    const auto& stats = m_source->getStatistics();
    
    // Set near-max values
    // Note: We can't directly set atomic values, so we simulate by delivering many packets
    // This is more of an interface test
    
    for (int i = 0; i < 1000; ++i) {
        auto result = m_packetFactory->createPacket(i, nullptr, 1000);
        if (result.success) {
            m_source->testDeliverPacket(result.packet);
        }
    }
    
    // Statistics should still be coherent
    QVERIFY(stats.packetsDelivered.load() <= 1000);
    QVERIFY(stats.bytesGenerated.load() > 0);
}

void TestPacketSource::testLongRunningOperation() {
    QSignalSpy statisticsSpy(m_source, &PacketSource::statisticsUpdated);
    
    // Simulate long-running packet delivery (triggers periodic statistics updates)
    for (int i = 0; i < 2000; ++i) {
        auto result = m_packetFactory->createPacket(i, nullptr, 100);
        if (result.success) {
            m_source->testDeliverPacket(result.packet);
        }
    }
    
    // Should have triggered statistics updates (every 1000 packets)
    QVERIFY(statisticsSpy.count() >= 1);
    
    if (statisticsSpy.count() > 0) {
        auto statsArgs = statisticsSpy.last();
        QVERIFY(statsArgs.size() >= 1);
    }
}

QTEST_MAIN(TestPacketSource)
#include "test_packet_source.moc"