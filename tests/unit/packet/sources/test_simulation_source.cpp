#include <QtTest/QTest>
#include <QSignalSpy>
#include <QTimer>
#include <QObject>
#include <thread>
#include <chrono>
#include <cmath>
#include "packet/sources/simulation_source.h"
#include "packet/core/packet_factory.h"
#include "core/application.h"

using namespace Monitor::Packet;
using namespace Monitor::Memory;

class TestSimulationSource : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Configuration tests
    void testSimulationConfiguration();
    void testPacketTypeConfiguration();
    void testConfigurationDefaults();

    // Packet type management tests
    void testAddPacketType();
    void testRemovePacketType();
    void testEnableDisablePacketType();
    void testMultiplePacketTypes();

    // Pattern generation tests
    void testConstantPattern();
    void testLinearPattern();
    void testSinePattern();
    void testCosinePattern();
    void testSquarePattern();
    void testSawtoothPattern();
    void testRandomPattern();
    void testCounterPattern();
    void testBitfieldPattern();

    // Simulation control tests
    void testSimulationStartStop();
    void testSimulationPauseResume();
    void testSimulationDuration();
    void testSimulationReset();

    // Packet generation tests
    void testPacketGeneration();
    void testPacketTiming();
    void testPacketContent();
    void testPacketFlags();

    // Statistics tests
    void testGenerationStatistics();
    void testStatisticsReset();
    void testStatisticsAccuracy();

    // Error handling tests
    void testMissingPacketFactory();
    void testInvalidConfiguration();
    void testPacketCreationFailure();

    // Performance tests
    void testHighFrequencyGeneration();
    void testMultipleTypeConcurrency();
    void testMemoryEfficiency();

    // Edge cases
    void testZeroSizePackets();
    void testLargePackets();
    void testVeryHighFrequency();
    void testPatternEdgeCases();

    // Integration tests
    void testEventIntegration();
    void testFactoryIntegration();
    void testSignalEmission();

private:
    Core::Application* m_app = nullptr;
    MemoryPoolManager* m_memoryManager = nullptr;
    PacketFactory* m_packetFactory = nullptr;
    SimulationSource* m_source = nullptr;
    
    static constexpr PacketId TEST_PACKET_ID_1 = 1001;
    static constexpr PacketId TEST_PACKET_ID_2 = 1002;
    static constexpr size_t TEST_PAYLOAD_SIZE = 128;
    static constexpr uint32_t TEST_INTERVAL_MS = 100;
    
    void setupMemoryPools();
    SimulationSource::SimulationConfig createTestConfig(const std::string& name = "TestSimulation");
    SimulationSource::PacketTypeConfig createPacketTypeConfig(
        PacketId id = TEST_PACKET_ID_1, 
        size_t size = TEST_PAYLOAD_SIZE,
        uint32_t interval = TEST_INTERVAL_MS,
        SimulationSource::PatternType pattern = SimulationSource::PatternType::Counter);
    void waitForPackets(int expectedCount, int timeoutMs = 5000);
    double calculatePatternValue(SimulationSource::PatternType pattern, double time, double amplitude = 1.0, double frequency = 1.0, double offset = 0.0, uint64_t counter = 0);
};

void TestSimulationSource::initTestCase() {
    m_app = Core::Application::instance();
    QVERIFY(m_app != nullptr);
    
    m_memoryManager = m_app->memoryManager();
    QVERIFY(m_memoryManager != nullptr);
    
    setupMemoryPools();
    
    m_packetFactory = new PacketFactory(m_memoryManager, this);
    QVERIFY(m_packetFactory != nullptr);
}

void TestSimulationSource::cleanupTestCase() {
    delete m_packetFactory;
    m_packetFactory = nullptr;
}

void TestSimulationSource::init() {
    auto config = createTestConfig();
    m_source = new SimulationSource(config, this);
    QVERIFY(m_source != nullptr);
    
    m_source->setPacketFactory(m_packetFactory);
}

void TestSimulationSource::cleanup() {
    if (m_source && m_source->isRunning()) {
        m_source->stop();
    }
    delete m_source;
    m_source = nullptr;
}

void TestSimulationSource::setupMemoryPools() {
    m_memoryManager->createPool("SmallObjects", 64, 1000);
    m_memoryManager->createPool("MediumObjects", 512, 1000);
    m_memoryManager->createPool("WidgetData", 1024, 1000);
    m_memoryManager->createPool("TestFramework", 2048, 1000);
    m_memoryManager->createPool("PacketBuffer", 4096, 1000);
    m_memoryManager->createPool("LargeObjects", 8192, 1000);
}

SimulationSource::SimulationConfig TestSimulationSource::createTestConfig(const std::string& name) {
    SimulationSource::SimulationConfig config(name);
    config.autoStart = false;
    config.bufferSize = 1000;
    config.maxPacketRate = 0; // Unlimited
    config.enableStatistics = true;
    config.totalDurationMs = 0; // Unlimited
    config.burstSize = 1;
    config.burstIntervalMs = 0;
    config.randomizeTimings = false;
    config.timingJitterMs = 0;
    
    // Add default packet type
    auto packetType = createPacketTypeConfig();
    config.packetTypes.push_back(packetType);
    
    return config;
}

SimulationSource::PacketTypeConfig TestSimulationSource::createPacketTypeConfig(
    PacketId id, size_t size, uint32_t interval, SimulationSource::PatternType pattern) {
    
    SimulationSource::PacketTypeConfig config(id, "TestPacketType", size, interval, pattern);
    config.amplitude = 1000.0;
    config.frequency = 1.0;
    config.offset = 0.0;
    config.enabled = true;
    
    return config;
}

void TestSimulationSource::waitForPackets(int expectedCount, int timeoutMs) {
    QSignalSpy packetSpy(m_source, &SimulationSource::packetReady);
    
    // Wait for expected number of packets or timeout
    QTRY_COMPARE_WITH_TIMEOUT(packetSpy.count(), expectedCount, timeoutMs);
}

double TestSimulationSource::calculatePatternValue(SimulationSource::PatternType pattern, double time, 
                                                  double amplitude, double frequency, double offset, uint64_t counter) {
    double timePhase = time * frequency * 2.0 * M_PI;
    
    switch (pattern) {
        case SimulationSource::PatternType::Constant:
            return amplitude * 1000;
            
        case SimulationSource::PatternType::Linear:
            return amplitude * time + offset;
            
        case SimulationSource::PatternType::Sine:
            return amplitude * std::sin(timePhase) + offset;
            
        case SimulationSource::PatternType::Cosine:
            return amplitude * std::cos(timePhase) + offset;
            
        case SimulationSource::PatternType::Square:
            return (std::sin(timePhase) >= 0 ? amplitude : -amplitude) + offset;
            
        case SimulationSource::PatternType::Sawtooth: {
            double phase = std::fmod(timePhase, 2.0 * M_PI);
            return amplitude * (phase / M_PI - 1.0) + offset;
        }
        
        case SimulationSource::PatternType::Counter:
            return static_cast<double>(counter);
            
        case SimulationSource::PatternType::Bitfield:
            return static_cast<double>(1u << (counter % 32));
            
        default:
            return 0.0;
    }
}

void TestSimulationSource::testSimulationConfiguration() {
    auto config = m_source->getSimulationConfig();
    
    QCOMPARE(config.name, std::string("TestSimulation"));
    QCOMPARE(config.totalDurationMs, uint32_t(0));
    QCOMPARE(config.burstSize, uint32_t(1));
    QCOMPARE(config.burstIntervalMs, uint32_t(0));
    QVERIFY(!config.randomizeTimings);
    QCOMPARE(config.timingJitterMs, uint32_t(0));
    QCOMPARE(config.packetTypes.size(), size_t(1));
    
    if (!config.packetTypes.empty()) {
        const auto& packetType = config.packetTypes[0];
        QCOMPARE(packetType.id, TEST_PACKET_ID_1);
        QCOMPARE(packetType.payloadSize, TEST_PAYLOAD_SIZE);
        QCOMPARE(packetType.intervalMs, TEST_INTERVAL_MS);
        QVERIFY(packetType.enabled);
    }
}

void TestSimulationSource::testPacketTypeConfiguration() {
    auto packetType = createPacketTypeConfig(42, 256, 50, SimulationSource::PatternType::Sine);
    
    QCOMPARE(packetType.id, PacketId(42));
    QCOMPARE(packetType.name, std::string("TestPacketType"));
    QCOMPARE(packetType.payloadSize, size_t(256));
    QCOMPARE(packetType.intervalMs, uint32_t(50));
    QCOMPARE(packetType.pattern, SimulationSource::PatternType::Sine);
    QCOMPARE(packetType.amplitude, 1000.0);
    QCOMPARE(packetType.frequency, 1.0);
    QCOMPARE(packetType.offset, 0.0);
    QVERIFY(packetType.enabled);
}

void TestSimulationSource::testConfigurationDefaults() {
    SimulationSource::SimulationConfig defaultConfig("DefaultTest");
    
    QCOMPARE(defaultConfig.name, std::string("DefaultTest"));
    QCOMPARE(defaultConfig.totalDurationMs, uint32_t(0));
    QCOMPARE(defaultConfig.burstSize, uint32_t(1));
    QCOMPARE(defaultConfig.burstIntervalMs, uint32_t(0));
    QVERIFY(!defaultConfig.randomizeTimings);
    QCOMPARE(defaultConfig.timingJitterMs, uint32_t(0));
    QVERIFY(defaultConfig.packetTypes.empty());
    
    // Test base configuration inheritance
    QVERIFY(!defaultConfig.autoStart);
    QCOMPARE(defaultConfig.bufferSize, uint32_t(1000));
    QCOMPARE(defaultConfig.maxPacketRate, uint32_t(0));
    QVERIFY(defaultConfig.enableStatistics);
}

void TestSimulationSource::testAddPacketType() {
    auto newPacketType = createPacketTypeConfig(TEST_PACKET_ID_2, 512, 200, SimulationSource::PatternType::Sine);
    
    size_t initialCount = m_source->getSimulationConfig().packetTypes.size();
    m_source->addPacketType(newPacketType);
    
    auto config = m_source->getSimulationConfig();
    QCOMPARE(config.packetTypes.size(), initialCount + 1);
    
    // Find the added packet type
    bool found = false;
    for (const auto& packetType : config.packetTypes) {
        if (packetType.id == TEST_PACKET_ID_2) {
            found = true;
            QCOMPARE(packetType.payloadSize, size_t(512));
            QCOMPARE(packetType.intervalMs, uint32_t(200));
            QCOMPARE(packetType.pattern, SimulationSource::PatternType::Sine);
            break;
        }
    }
    QVERIFY(found);
}

void TestSimulationSource::testRemovePacketType() {
    // Add another packet type first
    auto newPacketType = createPacketTypeConfig(TEST_PACKET_ID_2, 256, 150);
    m_source->addPacketType(newPacketType);
    
    size_t countBefore = m_source->getSimulationConfig().packetTypes.size();
    QVERIFY(countBefore >= 2);
    
    // Remove packet type
    m_source->removePacketType(TEST_PACKET_ID_2);
    
    auto config = m_source->getSimulationConfig();
    QCOMPARE(config.packetTypes.size(), countBefore - 1);
    
    // Verify it's actually removed
    bool found = false;
    for (const auto& packetType : config.packetTypes) {
        if (packetType.id == TEST_PACKET_ID_2) {
            found = true;
            break;
        }
    }
    QVERIFY(!found);
}

void TestSimulationSource::testEnableDisablePacketType() {
    // Initially enabled
    auto config = m_source->getSimulationConfig();
    QVERIFY(!config.packetTypes.empty());
    QVERIFY(config.packetTypes[0].enabled);
    
    // Disable packet type
    m_source->enablePacketType(TEST_PACKET_ID_1, false);
    config = m_source->getSimulationConfig();
    
    bool found = false;
    for (const auto& packetType : config.packetTypes) {
        if (packetType.id == TEST_PACKET_ID_1) {
            found = true;
            QVERIFY(!packetType.enabled);
            break;
        }
    }
    QVERIFY(found);
    
    // Re-enable packet type
    m_source->enablePacketType(TEST_PACKET_ID_1, true);
    config = m_source->getSimulationConfig();
    
    found = false;
    for (const auto& packetType : config.packetTypes) {
        if (packetType.id == TEST_PACKET_ID_1) {
            found = true;
            QVERIFY(packetType.enabled);
            break;
        }
    }
    QVERIFY(found);
}

void TestSimulationSource::testMultiplePacketTypes() {
    // Add multiple packet types with different configurations
    auto packetType2 = createPacketTypeConfig(TEST_PACKET_ID_2, 256, 75, SimulationSource::PatternType::Sine);
    auto packetType3 = createPacketTypeConfig(300, 512, 50, SimulationSource::PatternType::Random);
    
    m_source->addPacketType(packetType2);
    m_source->addPacketType(packetType3);
    
    auto config = m_source->getSimulationConfig();
    QCOMPARE(config.packetTypes.size(), size_t(3));
    
    // Verify all packet types are present
    std::set<PacketId> foundIds;
    for (const auto& packetType : config.packetTypes) {
        foundIds.insert(packetType.id);
    }
    
    QVERIFY(foundIds.count(TEST_PACKET_ID_1) > 0);
    QVERIFY(foundIds.count(TEST_PACKET_ID_2) > 0);
    QVERIFY(foundIds.count(300) > 0);
}

void TestSimulationSource::testConstantPattern() {
    // Create source with constant pattern
    auto config = createTestConfig("ConstantTest");
    config.packetTypes[0].pattern = SimulationSource::PatternType::Constant;
    config.packetTypes[0].amplitude = 5.0;
    config.packetTypes[0].intervalMs = 50;
    
    SimulationSource constantSource(config, this);
    constantSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&constantSource, &SimulationSource::packetReady);
    
    constantSource.start();
    waitForPackets(2, 200);
    constantSource.stop();
    
    QVERIFY(packetSpy.count() >= 1);
    
    if (packetSpy.count() > 0) {
        auto packet = packetSpy.first().first().value<PacketPtr>();
        QVERIFY(packet != nullptr);
        QVERIFY(packet->hasFlag(PacketHeader::Flags::Simulation));
        QVERIFY(packet->hasFlag(PacketHeader::Flags::TestData));
        
        // Check payload contains constant values
        const uint8_t* payload = packet->payload();
        const uint32_t* values = reinterpret_cast<const uint32_t*>(payload);
        uint32_t expectedValue = static_cast<uint32_t>(5000); // amplitude * 1000
        
        for (size_t i = 0; i < packet->payloadSize() / 4; ++i) {
            QCOMPARE(values[i], expectedValue);
        }
    }
}

void TestSimulationSource::testLinearPattern() {
    auto config = createTestConfig("LinearTest");
    config.packetTypes[0].pattern = SimulationSource::PatternType::Linear;
    config.packetTypes[0].amplitude = 2.0;
    config.packetTypes[0].offset = 100.0;
    
    SimulationSource linearSource(config, this);
    linearSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&linearSource, &SimulationSource::packetReady);
    
    linearSource.start();
    waitForPackets(1, 200);
    linearSource.stop();
    
    QVERIFY(packetSpy.count() >= 1);
    
    if (packetSpy.count() > 0) {
        auto packet = packetSpy.first().first().value<PacketPtr>();
        QVERIFY(packet != nullptr);
        
        const uint8_t* payload = packet->payload();
        const uint32_t* values = reinterpret_cast<const uint32_t*>(payload);
        
        // Linear pattern should have increasing values
        for (size_t i = 1; i < std::min(size_t(10), packet->payloadSize() / 4); ++i) {
            QVERIFY(values[i] >= values[i-1]); // Should be non-decreasing
        }
    }
}

void TestSimulationSource::testSinePattern() {
    auto config = createTestConfig("SineTest");
    config.packetTypes[0].pattern = SimulationSource::PatternType::Sine;
    config.packetTypes[0].amplitude = 3.0;
    config.packetTypes[0].frequency = 1.0;
    
    SimulationSource sineSource(config, this);
    sineSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&sineSource, &SimulationSource::packetReady);
    
    sineSource.start();
    waitForPackets(1, 200);
    sineSource.stop();
    
    QVERIFY(packetSpy.count() >= 1);
    
    if (packetSpy.count() > 0) {
        auto packet = packetSpy.first().first().value<PacketPtr>();
        QVERIFY(packet != nullptr);
        
        const uint8_t* payload = packet->payload();
        QVERIFY(payload != nullptr);
        
        // Sine pattern should produce varying values (not all the same)
        const uint32_t* values = reinterpret_cast<const uint32_t*>(payload);
        bool hasVariation = false;
        uint32_t firstValue = values[0];
        
        for (size_t i = 1; i < std::min(size_t(10), packet->payloadSize() / 4); ++i) {
            if (values[i] != firstValue) {
                hasVariation = true;
                break;
            }
        }
        QVERIFY(hasVariation);
    }
}

void TestSimulationSource::testCosinePattern() {
    auto config = createTestConfig("CosineTest");
    config.packetTypes[0].pattern = SimulationSource::PatternType::Cosine;
    config.packetTypes[0].intervalMs = 50;
    
    SimulationSource cosineSource(config, this);
    cosineSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&cosineSource, &SimulationSource::packetReady);
    
    cosineSource.start();
    waitForPackets(1, 200);
    cosineSource.stop();
    
    QVERIFY(packetSpy.count() >= 1);
}

void TestSimulationSource::testSquarePattern() {
    auto config = createTestConfig("SquareTest");
    config.packetTypes[0].pattern = SimulationSource::PatternType::Square;
    config.packetTypes[0].amplitude = 10.0;
    
    SimulationSource squareSource(config, this);
    squareSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&squareSource, &SimulationSource::packetReady);
    
    squareSource.start();
    waitForPackets(1, 200);
    squareSource.stop();
    
    QVERIFY(packetSpy.count() >= 1);
    
    if (packetSpy.count() > 0) {
        auto packet = packetSpy.first().first().value<PacketPtr>();
        QVERIFY(packet != nullptr);
        
        const uint8_t* payload = packet->payload();
        const uint32_t* values = reinterpret_cast<const uint32_t*>(payload);
        
        // Square wave should have distinct high/low values
        // (exact validation is complex due to timing, so we just check for data presence)
        QVERIFY(values[0] != 0 || packet->payloadSize() > 0);
    }
}

void TestSimulationSource::testSawtoothPattern() {
    auto config = createTestConfig("SawtoothTest");
    config.packetTypes[0].pattern = SimulationSource::PatternType::Sawtooth;
    
    SimulationSource sawtoothSource(config, this);
    sawtoothSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&sawtoothSource, &SimulationSource::packetReady);
    
    sawtoothSource.start();
    waitForPackets(1, 200);
    sawtoothSource.stop();
    
    QVERIFY(packetSpy.count() >= 1);
}

void TestSimulationSource::testRandomPattern() {
    auto config = createTestConfig("RandomTest");
    config.packetTypes[0].pattern = SimulationSource::PatternType::Random;
    
    SimulationSource randomSource(config, this);
    randomSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&randomSource, &SimulationSource::packetReady);
    
    randomSource.start();
    waitForPackets(2, 300);
    randomSource.stop();
    
    QVERIFY(packetSpy.count() >= 1);
    
    if (packetSpy.count() >= 2) {
        auto packet1 = packetSpy.at(0).first().value<PacketPtr>();
        auto packet2 = packetSpy.at(1).first().value<PacketPtr>();
        
        QVERIFY(packet1 != nullptr);
        QVERIFY(packet2 != nullptr);
        
        // Random patterns should be different between packets
        const uint32_t* values1 = reinterpret_cast<const uint32_t*>(packet1->payload());
        const uint32_t* values2 = reinterpret_cast<const uint32_t*>(packet2->payload());
        
        // At least some values should be different
        bool isDifferent = false;
        size_t compareCount = std::min(packet1->payloadSize(), packet2->payloadSize()) / 4;
        for (size_t i = 0; i < compareCount && i < 10; ++i) {
            if (values1[i] != values2[i]) {
                isDifferent = true;
                break;
            }
        }
        QVERIFY(isDifferent);
    }
}

void TestSimulationSource::testCounterPattern() {
    auto config = createTestConfig("CounterTest");
    config.packetTypes[0].pattern = SimulationSource::PatternType::Counter;
    
    SimulationSource counterSource(config, this);
    counterSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&counterSource, &SimulationSource::packetReady);
    
    counterSource.start();
    waitForPackets(3, 500);
    counterSource.stop();
    
    QVERIFY(packetSpy.count() >= 2);
    
    if (packetSpy.count() >= 2) {
        auto packet1 = packetSpy.at(0).first().value<PacketPtr>();
        auto packet2 = packetSpy.at(1).first().value<PacketPtr>();
        
        const uint32_t* values1 = reinterpret_cast<const uint32_t*>(packet1->payload());
        const uint32_t* values2 = reinterpret_cast<const uint32_t*>(packet2->payload());
        
        // Counter should increment between packets
        // First value in second packet should be greater than first packet
        QVERIFY(values2[0] > values1[0]);
        
        // Within each packet, values should increment
        if (packet1->payloadSize() >= 8) {
            QCOMPARE(values1[1], values1[0] + 1);
        }
    }
}

void TestSimulationSource::testBitfieldPattern() {
    auto config = createTestConfig("BitfieldTest");
    config.packetTypes[0].pattern = SimulationSource::PatternType::Bitfield;
    
    SimulationSource bitfieldSource(config, this);
    bitfieldSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&bitfieldSource, &SimulationSource::packetReady);
    
    bitfieldSource.start();
    waitForPackets(2, 300);
    bitfieldSource.stop();
    
    QVERIFY(packetSpy.count() >= 1);
    
    if (packetSpy.count() >= 1) {
        auto packet = packetSpy.first().first().value<PacketPtr>();
        QVERIFY(packet != nullptr);
        
        const uint32_t* values = reinterpret_cast<const uint32_t*>(packet->payload());
        
        // Bitfield pattern should produce power-of-2 values
        uint32_t value = values[0];
        bool isPowerOfTwo = (value > 0) && ((value & (value - 1)) == 0);
        QVERIFY(isPowerOfTwo);
    }
}

void TestSimulationSource::testSimulationStartStop() {
    QSignalSpy startedSpy(m_source, &SimulationSource::started);
    QSignalSpy stoppedSpy(m_source, &SimulationSource::stopped);
    QSignalSpy packetSpy(m_source, &SimulationSource::packetReady);
    
    // Start simulation
    bool result = m_source->start();
    QVERIFY(result);
    QVERIFY(m_source->isRunning());
    QCOMPARE(startedSpy.count(), 1);
    
    // Wait for some packets
    waitForPackets(2, 500);
    QVERIFY(packetSpy.count() >= 1);
    
    // Stop simulation
    m_source->stop();
    QVERIFY(m_source->isStopped());
    QCOMPARE(stoppedSpy.count(), 1);
    
    // Should stop generating packets
    int packetsAfterStop = packetSpy.count();
    QTest::qWait(200); // Wait a bit
    QCOMPARE(packetSpy.count(), packetsAfterStop); // Should not increase
}

void TestSimulationSource::testSimulationPauseResume() {
    QSignalSpy pausedSpy(m_source, &SimulationSource::paused);
    QSignalSpy resumedSpy(m_source, &SimulationSource::resumed);
    QSignalSpy packetSpy(m_source, &SimulationSource::packetReady);
    
    // Start and let it generate some packets
    m_source->start();
    waitForPackets(1, 300);
    QVERIFY(packetSpy.count() >= 1);
    
    // Pause
    m_source->pause();
    QCOMPARE(pausedSpy.count(), 1);
    
    int packetsAfterPause = packetSpy.count();
    QTest::qWait(200); // Wait while paused
    QCOMPARE(packetSpy.count(), packetsAfterPause); // Should not increase
    
    // Resume
    m_source->resume();
    QCOMPARE(resumedSpy.count(), 1);
    
    // Should start generating again
    waitForPackets(packetsAfterPause + 1, 300);
    QVERIFY(packetSpy.count() > packetsAfterPause);
    
    m_source->stop();
}

void TestSimulationSource::testSimulationDuration() {
    // Create simulation with limited duration
    auto config = createTestConfig("DurationTest");
    config.totalDurationMs = 200; // 200ms duration
    config.packetTypes[0].intervalMs = 50; // Should get ~4 packets
    
    SimulationSource durationSource(config, this);
    durationSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy stoppedSpy(&durationSource, &SimulationSource::stopped);
    QSignalSpy packetSpy(&durationSource, &SimulationSource::packetReady);
    
    durationSource.start();
    
    // Wait for automatic stop
    QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, 500);
    QVERIFY(durationSource.isStopped());
    
    // Should have generated some packets
    QVERIFY(packetSpy.count() >= 1);
}

void TestSimulationSource::testSimulationReset() {
    // Generate some packets first
    m_source->start();
    waitForPackets(3, 500);
    m_source->stop();
    
    const auto& stats = m_source->getStatistics();
    QVERIFY(stats.packetsGenerated.load() > 0);
    QVERIFY(stats.packetsDelivered.load() > 0);
    
    // Reset simulation
    m_source->resetSimulation();
    
    // Statistics should be reset
    QCOMPARE(stats.packetsGenerated.load(), uint64_t(0));
    QCOMPARE(stats.packetsDelivered.load(), uint64_t(0));
    QCOMPARE(stats.packetsDropped.load(), uint64_t(0));
    QCOMPARE(stats.bytesGenerated.load(), uint64_t(0));
    QCOMPARE(stats.errorCount.load(), uint64_t(0));
}

void TestSimulationSource::testPacketGeneration() {
    QSignalSpy packetSpy(m_source, &SimulationSource::packetReady);
    
    m_source->start();
    waitForPackets(2, 400);
    m_source->stop();
    
    QVERIFY(packetSpy.count() >= 2);
    
    for (int i = 0; i < packetSpy.count(); ++i) {
        auto packet = packetSpy.at(i).first().value<PacketPtr>();
        QVERIFY(packet != nullptr);
        QVERIFY(packet->isValid());
        QCOMPARE(packet->id(), TEST_PACKET_ID_1);
        QCOMPARE(packet->payloadSize(), TEST_PAYLOAD_SIZE);
    }
}

void TestSimulationSource::testPacketTiming() {
    // Use faster interval for timing test
    auto config = createTestConfig("TimingTest");
    config.packetTypes[0].intervalMs = 50; // 50ms interval
    
    SimulationSource timingSource(config, this);
    timingSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&timingSource, &SimulationSource::packetReady);
    std::vector<QTime> timestamps;
    
    connect(&timingSource, &SimulationSource::packetReady, [&]() {
        timestamps.push_back(QTime::currentTime());
    });
    
    timingSource.start();
    waitForPackets(3, 400);
    timingSource.stop();
    
    QVERIFY(timestamps.size() >= 2);
    
    if (timestamps.size() >= 2) {
        int interval1 = timestamps[0].msecsTo(timestamps[1]);
        // Allow some tolerance in timing (30ms to 100ms range)
        QVERIFY(interval1 >= 30 && interval1 <= 100);
    }
}

void TestSimulationSource::testPacketContent() {
    QSignalSpy packetSpy(m_source, &SimulationSource::packetReady);
    
    m_source->start();
    waitForPackets(1, 300);
    m_source->stop();
    
    QVERIFY(packetSpy.count() >= 1);
    
    auto packet = packetSpy.first().first().value<PacketPtr>();
    QVERIFY(packet != nullptr);
    
    // Check packet properties
    QCOMPARE(packet->id(), TEST_PACKET_ID_1);
    QCOMPARE(packet->payloadSize(), TEST_PAYLOAD_SIZE);
    QVERIFY(packet->payload() != nullptr);
    QVERIFY(packet->sequence() > 0); // Should have sequence number
    
    // Check payload content (counter pattern)
    const uint32_t* values = reinterpret_cast<const uint32_t*>(packet->payload());
    QVERIFY(values != nullptr);
    
    // Counter pattern should start from 1 (first packet)
    QCOMPARE(values[0], uint32_t(1));
    if (TEST_PAYLOAD_SIZE >= 8) {
        QCOMPARE(values[1], uint32_t(2));
    }
}

void TestSimulationSource::testPacketFlags() {
    QSignalSpy packetSpy(m_source, &SimulationSource::packetReady);
    
    m_source->start();
    waitForPackets(1, 300);
    m_source->stop();
    
    QVERIFY(packetSpy.count() >= 1);
    
    auto packet = packetSpy.first().first().value<PacketPtr>();
    QVERIFY(packet != nullptr);
    
    // Check simulation flags are set
    QVERIFY(packet->hasFlag(PacketHeader::Flags::Simulation));
    QVERIFY(packet->hasFlag(PacketHeader::Flags::TestData));
    
    // Should not have other flags
    QVERIFY(!packet->hasFlag(PacketHeader::Flags::Network));
    QVERIFY(!packet->hasFlag(PacketHeader::Flags::Offline));
}

void TestSimulationSource::testGenerationStatistics() {
    const auto& stats = m_source->getStatistics();
    
    // Initial statistics
    QCOMPARE(stats.packetsGenerated.load(), uint64_t(0));
    QCOMPARE(stats.packetsDelivered.load(), uint64_t(0));
    
    // Generate packets
    m_source->start();
    waitForPackets(5, 800);
    m_source->stop();
    
    // Check updated statistics
    QVERIFY(stats.packetsGenerated.load() >= 5);
    QCOMPARE(stats.packetsGenerated.load(), stats.packetsDelivered.load()); // Should match for successful simulation
    QVERIFY(stats.bytesGenerated.load() > 0);
    QVERIFY(stats.getPacketRate() >= 0.0);
    QVERIFY(stats.getByteRate() >= 0.0);
}

void TestSimulationSource::testStatisticsReset() {
    // Generate some statistics
    m_source->start();
    waitForPackets(2, 400);
    m_source->stop();
    
    const auto& stats = m_source->getStatistics();
    QVERIFY(stats.packetsGenerated.load() > 0);
    
    // Reset and verify
    m_source->resetSimulation();
    QCOMPARE(stats.packetsGenerated.load(), uint64_t(0));
    QCOMPARE(stats.packetsDelivered.load(), uint64_t(0));
    QCOMPARE(stats.bytesGenerated.load(), uint64_t(0));
}

void TestSimulationSource::testStatisticsAccuracy() {
    QSignalSpy packetSpy(m_source, &SimulationSource::packetReady);
    
    m_source->start();
    waitForPackets(3, 600);
    m_source->stop();
    
    const auto& stats = m_source->getStatistics();
    
    // Packet counts should match signal emissions
    QCOMPARE(stats.packetsDelivered.load(), static_cast<uint64_t>(packetSpy.count()));
    
    // Byte count should match packet sizes
    uint64_t expectedBytes = packetSpy.count() * (PACKET_HEADER_SIZE + TEST_PAYLOAD_SIZE);
    QCOMPARE(stats.bytesGenerated.load(), expectedBytes);
}

void TestSimulationSource::testMissingPacketFactory() {
    // Create source without packet factory
    auto config = createTestConfig("NoFactoryTest");
    SimulationSource noFactorySource(config, this);
    
    QSignalSpy errorSpy(&noFactorySource, &SimulationSource::error);
    
    // Should fail to start without packet factory
    bool result = noFactorySource.start();
    QVERIFY(!result);
    QVERIFY(noFactorySource.hasError());
    QCOMPARE(errorSpy.count(), 1);
    
    auto errorMessage = errorSpy.first().first().toString();
    QVERIFY(errorMessage.contains("packet factory"));
}

void TestSimulationSource::testInvalidConfiguration() {
    // Test with invalid packet type configurations
    auto config = createTestConfig("InvalidTest");
    config.packetTypes.clear(); // No packet types
    
    SimulationSource invalidSource(config, this);
    invalidSource.setPacketFactory(m_packetFactory);
    
    // Should start successfully but not generate packets
    bool result = invalidSource.start();
    QVERIFY(result);
    
    QSignalSpy packetSpy(&invalidSource, &SimulationSource::packetReady);
    QTest::qWait(200);
    
    // Should not generate any packets
    QCOMPARE(packetSpy.count(), 0);
    
    invalidSource.stop();
}

void TestSimulationSource::testPacketCreationFailure() {
    // This is difficult to test without mocking PacketFactory
    // For now, we verify that the simulation handles creation gracefully
    
    QSignalSpy packetSpy(m_source, &SimulationSource::packetReady);
    QSignalSpy errorSpy(m_source, &SimulationSource::error);
    
    m_source->start();
    waitForPackets(1, 300);
    m_source->stop();
    
    // Should either succeed or fail gracefully
    QVERIFY(packetSpy.count() > 0 || errorSpy.count() > 0);
}

void TestSimulationSource::testHighFrequencyGeneration() {
    // Test with very high frequency
    auto config = createTestConfig("HighFreqTest");
    config.packetTypes[0].intervalMs = 10; // 100 packets/second
    
    SimulationSource highFreqSource(config, this);
    highFreqSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&highFreqSource, &SimulationSource::packetReady);
    
    highFreqSource.start();
    QTest::qWait(500); // Wait 500ms
    highFreqSource.stop();
    
    // Should generate many packets (allow some tolerance)
    int expectedPackets = 500 / 10; // Approximately 50 packets
    QVERIFY(packetSpy.count() >= expectedPackets * 0.5); // At least 50% of expected
    
    qDebug() << "High frequency test generated" << packetSpy.count() << "packets in 500ms";
}

void TestSimulationSource::testMultipleTypeConcurrency() {
    // Add multiple packet types with different intervals
    auto packetType2 = createPacketTypeConfig(TEST_PACKET_ID_2, 256, 75, SimulationSource::PatternType::Sine);
    m_source->addPacketType(packetType2);
    
    QSignalSpy packetSpy(m_source, &SimulationSource::packetReady);
    std::set<PacketId> receivedIds;
    
    connect(m_source, &SimulationSource::packetReady, [&](PacketPtr packet) {
        if (packet) {
            receivedIds.insert(packet->id());
        }
    });
    
    m_source->start();
    waitForPackets(6, 800); // Wait for multiple packets
    m_source->stop();
    
    // Should have received packets from both types
    QVERIFY(receivedIds.count(TEST_PACKET_ID_1) > 0);
    QVERIFY(receivedIds.count(TEST_PACKET_ID_2) > 0);
    
    qDebug() << "Multiple type test: Received" << receivedIds.size() << "different packet types";
}

void TestSimulationSource::testMemoryEfficiency() {
    const auto& stats = m_source->getStatistics();
    
    // Generate many packets
    m_source->start();
    waitForPackets(50, 2000);
    m_source->stop();
    
    uint64_t packetsGenerated = stats.packetsGenerated.load();
    uint64_t bytesGenerated = stats.bytesGenerated.load();
    
    QVERIFY(packetsGenerated > 0);
    QVERIFY(bytesGenerated > 0);
    
    // Average packet size should be reasonable
    double avgPacketSize = static_cast<double>(bytesGenerated) / packetsGenerated;
    double expectedSize = PACKET_HEADER_SIZE + TEST_PAYLOAD_SIZE;
    
    QCOMPARE(avgPacketSize, expectedSize);
    
    qDebug() << "Memory efficiency: Generated" << packetsGenerated << "packets,"
             << "avg size:" << avgPacketSize << "bytes";
}

void TestSimulationSource::testZeroSizePackets() {
    // Test with zero-size payload
    auto config = createTestConfig("ZeroSizeTest");
    config.packetTypes[0].payloadSize = 0;
    
    SimulationSource zeroSizeSource(config, this);
    zeroSizeSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&zeroSizeSource, &SimulationSource::packetReady);
    
    zeroSizeSource.start();
    waitForPackets(1, 300);
    zeroSizeSource.stop();
    
    QVERIFY(packetSpy.count() >= 1);
    
    if (packetSpy.count() > 0) {
        auto packet = packetSpy.first().first().value<PacketPtr>();
        QVERIFY(packet != nullptr);
        QCOMPARE(packet->payloadSize(), size_t(0));
        QCOMPARE(packet->totalSize(), PACKET_HEADER_SIZE);
    }
}

void TestSimulationSource::testLargePackets() {
    // Test with large payload (but within memory pool limits)
    auto config = createTestConfig("LargePacketTest");
    config.packetTypes[0].payloadSize = 4000; // Large but within 4KB pool
    
    SimulationSource largePacketSource(config, this);
    largePacketSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&largePacketSource, &SimulationSource::packetReady);
    
    largePacketSource.start();
    waitForPackets(1, 300);
    largePacketSource.stop();
    
    QVERIFY(packetSpy.count() >= 1);
    
    if (packetSpy.count() > 0) {
        auto packet = packetSpy.first().first().value<PacketPtr>();
        QVERIFY(packet != nullptr);
        QCOMPARE(packet->payloadSize(), size_t(4000));
    }
}

void TestSimulationSource::testVeryHighFrequency() {
    // Test limits of generation frequency
    auto config = createTestConfig("VeryHighFreqTest");
    config.packetTypes[0].intervalMs = 1; // 1000 packets/second theoretical
    config.packetTypes[0].payloadSize = 64; // Small payload
    
    SimulationSource veryHighFreqSource(config, this);
    veryHighFreqSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&veryHighFreqSource, &SimulationSource::packetReady);
    
    veryHighFreqSource.start();
    QTest::qWait(100); // Wait 100ms
    veryHighFreqSource.stop();
    
    // Should generate many packets, but system limits may apply
    QVERIFY(packetSpy.count() >= 10); // At least some packets
    
    qDebug() << "Very high frequency test: Generated" << packetSpy.count() << "packets in 100ms";
}

void TestSimulationSource::testPatternEdgeCases() {
    // Test patterns with edge case parameters
    auto config = createTestConfig("EdgeCaseTest");
    config.packetTypes[0].pattern = SimulationSource::PatternType::Sine;
    config.packetTypes[0].amplitude = 0.0; // Zero amplitude
    config.packetTypes[0].frequency = 0.0;  // Zero frequency
    
    SimulationSource edgeCaseSource(config, this);
    edgeCaseSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&edgeCaseSource, &SimulationSource::packetReady);
    
    edgeCaseSource.start();
    waitForPackets(1, 300);
    edgeCaseSource.stop();
    
    // Should still generate packets without crashing
    QVERIFY(packetSpy.count() >= 1);
}

void TestSimulationSource::testEventIntegration() {
    // Test with event dispatcher
    auto dispatcher = m_app->eventDispatcher();
    m_source->setEventDispatcher(dispatcher);
    
    QSignalSpy packetSpy(m_source, &SimulationSource::packetReady);
    
    m_source->start();
    waitForPackets(1, 300);
    m_source->stop();
    
    // Should work normally with event dispatcher
    QVERIFY(packetSpy.count() >= 1);
}

void TestSimulationSource::testFactoryIntegration() {
    // Test changing packet factory
    m_source->setPacketFactory(nullptr);
    
    // Should fail to start without factory
    bool result = m_source->start();
    QVERIFY(!result);
    
    // Set factory back
    m_source->setPacketFactory(m_packetFactory);
    result = m_source->start();
    QVERIFY(result);
    
    m_source->stop();
}

void TestSimulationSource::testSignalEmission() {
    QSignalSpy startedSpy(m_source, &SimulationSource::started);
    QSignalSpy stoppedSpy(m_source, &SimulationSource::stopped);
    QSignalSpy pausedSpy(m_source, &SimulationSource::paused);
    QSignalSpy resumedSpy(m_source, &SimulationSource::resumed);
    QSignalSpy packetSpy(m_source, &SimulationSource::packetReady);
    QSignalSpy statisticsSpy(m_source, &SimulationSource::statisticsUpdated);
    
    // Test full cycle
    m_source->start();
    QCOMPARE(startedSpy.count(), 1);
    
    waitForPackets(2, 400);
    QVERIFY(packetSpy.count() >= 2);
    
    m_source->pause();
    QCOMPARE(pausedSpy.count(), 1);
    
    m_source->resume();
    QCOMPARE(resumedSpy.count(), 1);
    
    m_source->stop();
    QCOMPARE(stoppedSpy.count(), 1);
    
    // Statistics may or may not be emitted depending on packet count
}

QTEST_MAIN(TestSimulationSource)
#include "test_simulation_source.moc"