#include <QtTest/QTest>
#include <QSignalSpy>
#include <QObject>
#include <thread>
#include <chrono>
#include "packet/core/packet_factory.h"
#include "packet/core/packet.h"
#include "core/application.h"

using namespace Monitor::Packet;
using namespace Monitor::Memory;

class TestPacketFactory : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testFactoryConstruction();
    void testPacketCreation();
    void testRawDataCreation();
    void testStructureCreation();
    void testPacketCloning();

    // Statistics tests
    void testStatisticsTracking();
    void testStatisticsReset();
    void testCreationRateCalculation();
    void testErrorRateCalculation();

    // Sequence number tests
    void testSequenceNumberGeneration();
    void testSequenceNumberThreadSafety();

    // Structure caching tests
    void testStructureCaching();
    void testCacheInvalidation();
    void testStructureAssociation();

    // Signal emission tests
    void testPacketCreatedSignal();
    void testPacketCreationFailedSignal();
    void testStatisticsUpdatedSignal();

    // Performance tests
    void testCreationPerformance();
    void testConcurrentCreation();
    void testMemoryEfficiency();

    // Error handling tests
    void testInvalidInputHandling();
    void testMemoryAllocationFailure();
    void testNullManagerHandling();

    // Edge cases
    void testEmptyPayloadCreation();
    void testMaximumSizeCreation();
    void testZeroSizeRawData();

    // Integration tests
    void testEventDispatcherIntegration();
    void testStructureManagerIntegration();
    void testMultiComponentIntegration();

private:
    Core::Application* m_app = nullptr;
    MemoryPoolManager* m_memoryManager = nullptr;
    PacketFactory* m_factory = nullptr;
    
    static constexpr PacketId TEST_PACKET_ID = 12345;
    static constexpr size_t TEST_PAYLOAD_SIZE = 256;
    static constexpr uint64_t PERFORMANCE_ITERATIONS = 10000;
    
    void setupMemoryPools();
    std::vector<uint8_t> createTestPayload(size_t size = TEST_PAYLOAD_SIZE);
};

void TestPacketFactory::initTestCase() {
    m_app = Core::Application::instance();
    QVERIFY(m_app != nullptr);
    
    m_memoryManager = m_app->memoryManager();
    QVERIFY(m_memoryManager != nullptr);
    
    setupMemoryPools();
}

void TestPacketFactory::cleanupTestCase() {
    // Cleanup handled by Application destructor
}

void TestPacketFactory::init() {
    m_factory = new PacketFactory(m_memoryManager, this);
    QVERIFY(m_factory != nullptr);
}

void TestPacketFactory::cleanup() {
    delete m_factory;
    m_factory = nullptr;
}

void TestPacketFactory::setupMemoryPools() {
    m_memoryManager->createPool("SmallObjects", 64, 1000);
    m_memoryManager->createPool("MediumObjects", 512, 1000);
    m_memoryManager->createPool("WidgetData", 1024, 1000);
    m_memoryManager->createPool("TestFramework", 2048, 1000);
    m_memoryManager->createPool("PacketBuffer", 4096, 1000);
    m_memoryManager->createPool("LargeObjects", 8192, 1000);
}

std::vector<uint8_t> TestPacketFactory::createTestPayload(size_t size) {
    std::vector<uint8_t> payload(size);
    for (size_t i = 0; i < size; ++i) {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }
    return payload;
}

void TestPacketFactory::testFactoryConstruction() {
    // Test valid construction
    QVERIFY(m_factory != nullptr);
    
    // Test construction with null memory manager should throw
    try {
        PacketFactory nullFactory(nullptr);
        QFAIL("Should have thrown exception with null memory manager");
    } catch (const std::invalid_argument& e) {
        QVERIFY(true); // Expected exception
    }
    
    // Test QObject parent relationship
    QCOMPARE(m_factory->parent(), this);
}

void TestPacketFactory::testPacketCreation() {
    auto payload = createTestPayload(TEST_PAYLOAD_SIZE);
    
    auto result = m_factory->createPacket(TEST_PACKET_ID, payload.data(), payload.size());
    
    QVERIFY(result.success);
    QVERIFY(result.packet != nullptr);
    QVERIFY(result.packet->isValid());
    QVERIFY(result.error.empty());
    
    // Verify packet properties
    QCOMPARE(result.packet->id(), TEST_PACKET_ID);
    QCOMPARE(result.packet->payloadSize(), TEST_PAYLOAD_SIZE);
    QVERIFY(result.packet->sequence() > 0); // Should have assigned sequence number
    
    // Verify payload data
    const uint8_t* packetPayload = result.packet->payload();
    QVERIFY(packetPayload != nullptr);
    QCOMPARE(std::memcmp(packetPayload, payload.data(), payload.size()), 0);
    
    // Test creation without payload
    auto emptyResult = m_factory->createPacket(TEST_PACKET_ID);
    QVERIFY(emptyResult.success);
    QVERIFY(emptyResult.packet != nullptr);
    QCOMPARE(emptyResult.packet->payloadSize(), size_t(0));
}

void TestPacketFactory::testRawDataCreation() {
    // Create raw packet data (header + payload)
    auto payload = createTestPayload(TEST_PAYLOAD_SIZE);
    std::vector<uint8_t> rawData(PACKET_HEADER_SIZE + payload.size());
    
    // Initialize header
    PacketHeader* header = reinterpret_cast<PacketHeader*>(rawData.data());
    *header = PacketHeader(TEST_PACKET_ID, 100, static_cast<uint32_t>(payload.size()));
    
    // Copy payload
    std::memcpy(rawData.data() + PACKET_HEADER_SIZE, payload.data(), payload.size());
    
    auto result = m_factory->createFromRawData(rawData.data(), rawData.size());
    
    QVERIFY(result.success);
    QVERIFY(result.packet != nullptr);
    QVERIFY(result.packet->isValid());
    
    // Verify packet properties match raw data
    QCOMPARE(result.packet->id(), TEST_PACKET_ID);
    QCOMPARE(result.packet->sequence(), SequenceNumber(100));
    QCOMPARE(result.packet->payloadSize(), TEST_PAYLOAD_SIZE);
    
    // Verify data integrity
    const uint8_t* packetPayload = result.packet->payload();
    QVERIFY(packetPayload != nullptr);
    QCOMPARE(std::memcmp(packetPayload, payload.data(), payload.size()), 0);
}

void TestPacketFactory::testStructureCreation() {
    // Note: This test is limited because we don't have a real StructureManager
    // We'll test the error handling for missing structure manager
    
    auto result = m_factory->createFromStructure(TEST_PACKET_ID, "TestStructure");
    
    QVERIFY(!result.success);
    QVERIFY(result.packet == nullptr);
    QVERIFY(!result.error.empty());
    QVERIFY(result.error.find("Structure manager not available") != std::string::npos);
    
    // Test with mock structure manager (would need to implement mock)
    // For now, we verify the interface exists and handles missing manager correctly
}

void TestPacketFactory::testPacketCloning() {
    // Create original packet
    auto payload = createTestPayload(TEST_PAYLOAD_SIZE);
    auto originalResult = m_factory->createPacket(TEST_PACKET_ID, payload.data(), payload.size());
    QVERIFY(originalResult.success);
    
    // Clone the packet
    auto cloneResult = m_factory->clonePacket(originalResult.packet);
    
    QVERIFY(cloneResult.success);
    QVERIFY(cloneResult.packet != nullptr);
    QVERIFY(cloneResult.packet->isValid());
    QVERIFY(cloneResult.packet != originalResult.packet); // Different objects
    
    // Verify cloned packet properties
    QCOMPARE(cloneResult.packet->id(), originalResult.packet->id());
    QCOMPARE(cloneResult.packet->payloadSize(), originalResult.packet->payloadSize());
    QCOMPARE(cloneResult.packet->totalSize(), originalResult.packet->totalSize());
    
    // Verify payload data
    const uint8_t* originalPayload = originalResult.packet->payload();
    const uint8_t* clonedPayload = cloneResult.packet->payload();
    QCOMPARE(std::memcmp(originalPayload, clonedPayload, TEST_PAYLOAD_SIZE), 0);
    
    // Test cloning null packet
    auto nullResult = m_factory->clonePacket(nullptr);
    QVERIFY(!nullResult.success);
    QVERIFY(nullResult.packet == nullptr);
    QVERIFY(!nullResult.error.empty());
}

void TestPacketFactory::testStatisticsTracking() {
    const auto& stats = m_factory->getStatistics();
    
    // Initial statistics should be zero
    QCOMPARE(stats.packetsCreated.load(), uint64_t(0));
    QCOMPARE(stats.packetsFromRawData.load(), uint64_t(0));
    QCOMPARE(stats.packetsFromStructure.load(), uint64_t(0));
    QCOMPARE(stats.packetsWithErrors.load(), uint64_t(0));
    QCOMPARE(stats.totalBytesAllocated.load(), uint64_t(0));
    
    // Create some packets
    auto result1 = m_factory->createPacket(1, nullptr, 100);
    QVERIFY(result1.success);
    
    auto payload = createTestPayload(200);
    auto result2 = m_factory->createFromRawData(payload.data(), payload.size());
    // This will fail due to invalid header, but should still update error stats
    
    // Create a valid raw packet
    std::vector<uint8_t> rawData(PACKET_HEADER_SIZE + 50);
    PacketHeader* header = reinterpret_cast<PacketHeader*>(rawData.data());
    *header = PacketHeader(2, 0, 50);
    auto result3 = m_factory->createFromRawData(rawData.data(), rawData.size());
    QVERIFY(result3.success);
    
    // Check updated statistics
    const auto& updatedStats = m_factory->getStatistics();
    QVERIFY(updatedStats.packetsCreated.load() >= 2); // At least 2 successful
    QVERIFY(updatedStats.packetsFromRawData.load() >= 1);
    QVERIFY(updatedStats.totalBytesAllocated.load() > 0);
    
    if (!result2.success) {
        QVERIFY(updatedStats.packetsWithErrors.load() > 0);
    }
}

void TestPacketFactory::testStatisticsReset() {
    // Create some packets to populate statistics
    m_factory->createPacket(1, nullptr, 100);
    m_factory->createPacket(2, nullptr, 200);
    
    const auto& stats = m_factory->getStatistics();
    QVERIFY(stats.packetsCreated.load() > 0);
    
    // Reset statistics
    m_factory->resetStatistics();
    
    const auto& resetStats = m_factory->getStatistics();
    QCOMPARE(resetStats.packetsCreated.load(), uint64_t(0));
    QCOMPARE(resetStats.packetsFromRawData.load(), uint64_t(0));
    QCOMPARE(resetStats.packetsFromStructure.load(), uint64_t(0));
    QCOMPARE(resetStats.packetsWithErrors.load(), uint64_t(0));
    QCOMPARE(resetStats.totalBytesAllocated.load(), uint64_t(0));
    QCOMPARE(resetStats.averageCreationTimeNs.load(), uint64_t(0));
    
    // Start time should be updated
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - resetStats.startTime).count();
    QVERIFY(elapsed < 100); // Should be very recent
}

void TestPacketFactory::testCreationRateCalculation() {
    m_factory->resetStatistics();
    
    // Wait a short time to establish baseline
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Create several packets
    for (int i = 0; i < 10; ++i) {
        m_factory->createPacket(i, nullptr, 50);
    }
    
    const auto& stats = m_factory->getStatistics();
    double rate = stats.getCreationRate();
    
    // Rate should be positive if time has elapsed
    if (rate > 0) {
        QVERIFY(rate <= 10000); // Reasonable upper bound (10k packets/sec)
    }
    
    // Test with zero elapsed time (edge case)
    m_factory->resetStatistics();
    double immediateRate = m_factory->getStatistics().getCreationRate();
    QCOMPARE(immediateRate, 0.0);
}

void TestPacketFactory::testErrorRateCalculation() {
    m_factory->resetStatistics();
    
    // Create successful packets
    for (int i = 0; i < 5; ++i) {
        m_factory->createPacket(i, nullptr, 100);
    }
    
    // Create failed packets (invalid raw data)
    for (int i = 0; i < 2; ++i) {
        uint8_t invalidData[10]; // Too small for header
        m_factory->createFromRawData(invalidData, sizeof(invalidData));
    }
    
    const auto& stats = m_factory->getStatistics();
    double errorRate = stats.getErrorRate();
    
    if (stats.packetsCreated.load() > 0) {
        QVERIFY(errorRate >= 0.0);
        QVERIFY(errorRate <= 1.0);
        
        if (stats.packetsWithErrors.load() > 0) {
            QVERIFY(errorRate > 0.0);
        }
    } else {
        QCOMPARE(errorRate, 0.0);
    }
}

void TestPacketFactory::testSequenceNumberGeneration() {
    SequenceNumber initialSeq = m_factory->getNextSequence();
    
    // Create packets and verify sequence numbers increment
    auto result1 = m_factory->createPacket(1);
    QVERIFY(result1.success);
    SequenceNumber seq1 = result1.packet->sequence();
    
    auto result2 = m_factory->createPacket(2);
    QVERIFY(result2.success);
    SequenceNumber seq2 = result2.packet->sequence();
    
    // Sequence numbers should increment
    QVERIFY(seq2 > seq1);
    QCOMPARE(seq2, seq1 + 1);
    
    // Next sequence should be higher
    SequenceNumber currentNext = m_factory->getNextSequence();
    QVERIFY(currentNext > seq2);
}

void TestPacketFactory::testSequenceNumberThreadSafety() {
    const int numThreads = 4;
    const int packetsPerThread = 100;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<SequenceNumber>> threadSequences(numThreads);
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, packetsPerThread, &threadSequences]() {
            for (int j = 0; j < packetsPerThread; ++j) {
                auto result = m_factory->createPacket(i * 1000 + j, nullptr, 50);
                if (result.success) {
                    threadSequences[i].push_back(result.packet->sequence());
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Collect all sequence numbers
    std::set<SequenceNumber> allSequences;
    size_t totalPackets = 0;
    
    for (const auto& threadSeqs : threadSequences) {
        totalPackets += threadSeqs.size();
        for (SequenceNumber seq : threadSeqs) {
            allSequences.insert(seq);
        }
    }
    
    // All sequence numbers should be unique (no duplicates)
    QCOMPARE(allSequences.size(), totalPackets);
    
    qDebug() << "Thread safety test: Created" << totalPackets << "packets with unique sequences";
}

void TestPacketFactory::testStructureCaching() {
    PacketId testId = 999;
    
    // Initially should not have structure
    QVERIFY(!m_factory->hasStructureForPacketId(testId));
    
    // Note: Full structure caching test would require StructureManager integration
    // For now, we test the interface exists and behaves correctly
}

void TestPacketFactory::testCacheInvalidation() {
    // This test would require StructureManager integration to properly test
    // cache invalidation when structures are removed
    
    // For now, verify the slot exists and doesn't crash
    m_factory->onStructureRemoved("TestStructure");
    
    // Should not crash
    QVERIFY(true);
}

void TestPacketFactory::testStructureAssociation() {
    // Test structure association without StructureManager
    auto result = m_factory->createPacket(TEST_PACKET_ID);
    QVERIFY(result.success);
    
    // Without StructureManager, structure should be null
    QVERIFY(result.packet->getStructure() == nullptr);
    QCOMPARE(result.packet->getStructureName(), std::string("Unknown"));
}

void TestPacketFactory::testPacketCreatedSignal() {
    QSignalSpy spy(m_factory, &PacketFactory::packetCreated);
    
    auto result = m_factory->createPacket(TEST_PACKET_ID, nullptr, 100);
    QVERIFY(result.success);
    
    // Should have emitted packetCreated signal
    QTRY_COMPARE(spy.count(), 1);
    
    // Verify signal arguments
    QList<QVariant> arguments = spy.takeFirst();
    PacketPtr signalPacket = arguments.at(0).value<PacketPtr>();
    QVERIFY(signalPacket != nullptr);
    QCOMPARE(signalPacket->id(), TEST_PACKET_ID);
}

void TestPacketFactory::testPacketCreationFailedSignal() {
    QSignalSpy spy(m_factory, &PacketFactory::packetCreationFailed);
    
    // Try to create packet with invalid data
    uint8_t invalidData[5]; // Too small
    auto result = m_factory->createFromRawData(invalidData, sizeof(invalidData));
    QVERIFY(!result.success);
    
    // Should have emitted packetCreationFailed signal
    QTRY_COMPARE(spy.count(), 1);
    
    // Verify signal arguments
    QList<QVariant> arguments = spy.takeFirst();
    QVERIFY(!arguments.at(1).toString().isEmpty()); // Error message should not be empty
}

void TestPacketFactory::testStatisticsUpdatedSignal() {
    QSignalSpy spy(m_factory, &PacketFactory::statisticsUpdated);
    
    // Create many packets to trigger statistics update
    for (int i = 0; i < 1000; ++i) {
        m_factory->createPacket(i, nullptr, 50);
    }
    
    // Should have emitted statisticsUpdated signal at least once
    QTRY_VERIFY(spy.count() >= 1);
    
    if (spy.count() > 0) {
        QList<QVariant> arguments = spy.takeLast();
        // Verify signal contains statistics
        QVERIFY(arguments.size() >= 1);
    }
}

void TestPacketFactory::testCreationPerformance() {
    m_factory->resetStatistics();
    
    QElapsedTimer timer;
    std::vector<PacketPtr> packets;
    packets.reserve(PERFORMANCE_ITERATIONS);
    
    timer.start();
    
    for (uint64_t i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        auto result = m_factory->createPacket(static_cast<PacketId>(i % 1000), nullptr, 128);
        if (result.success) {
            packets.push_back(result.packet);
        }
    }
    
    qint64 creationTime = timer.nsecsElapsed();
    double nsPerCreation = static_cast<double>(creationTime) / packets.size();
    
    qDebug() << "PacketFactory creation performance:" << nsPerCreation << "ns per packet";
    qDebug() << "Successful creations:" << packets.size() << "out of" << PERFORMANCE_ITERATIONS;
    
    // Should be fast - target < 1000ns (1μs) per packet
    QVERIFY(nsPerCreation < 1000.0);
    
    // Verify statistics were updated
    const auto& stats = m_factory->getStatistics();
    QCOMPARE(stats.packetsCreated.load(), static_cast<uint64_t>(packets.size()));
    QVERIFY(stats.averageCreationTimeNs.load() > 0);
}

void TestPacketFactory::testConcurrentCreation() {
    const int numThreads = 4;
    const int packetsPerThread = 1000;
    
    m_factory->resetStatistics();
    
    std::vector<std::thread> threads;
    std::vector<std::vector<PacketPtr>> threadPackets(numThreads);
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, packetsPerThread, &threadPackets]() {
            for (int j = 0; j < packetsPerThread; ++j) {
                auto result = m_factory->createPacket(i * 1000 + j, nullptr, 64 + (j % 256));
                if (result.success) {
                    threadPackets[i].push_back(result.packet);
                }
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Count total successful packets
    size_t totalPackets = 0;
    for (const auto& packets : threadPackets) {
        totalPackets += packets.size();
    }
    
    qDebug() << "Concurrent creation: Created" << totalPackets 
             << "out of" << (numThreads * packetsPerThread) << "requested";
    
    // Should have high success rate
    QVERIFY(totalPackets > (numThreads * packetsPerThread * 0.9)); // At least 90%
    
    // Verify statistics
    const auto& stats = m_factory->getStatistics();
    QCOMPARE(stats.packetsCreated.load(), totalPackets);
}

void TestPacketFactory::testMemoryEfficiency() {
    const int numPackets = 1000;
    std::vector<PacketPtr> packets;
    packets.reserve(numPackets);
    
    size_t initialMemory = m_factory->getStatistics().totalBytesAllocated.load();
    
    // Create packets
    for (int i = 0; i < numPackets; ++i) {
        auto result = m_factory->createPacket(i, nullptr, 256);
        if (result.success) {
            packets.push_back(result.packet);
        }
    }
    
    size_t finalMemory = m_factory->getStatistics().totalBytesAllocated.load();
    size_t allocatedMemory = finalMemory - initialMemory;
    
    // Calculate expected memory usage
    size_t expectedPerPacket = PACKET_HEADER_SIZE + 256;
    size_t expectedTotal = packets.size() * expectedPerPacket;
    
    qDebug() << "Memory efficiency: Allocated" << allocatedMemory 
             << "bytes, expected" << expectedTotal << "bytes";
    
    // Should be reasonably close (allowing for pool overhead)
    QVERIFY(allocatedMemory >= expectedTotal);
    QVERIFY(allocatedMemory < expectedTotal * 2); // Not more than 2x overhead
}

void TestPacketFactory::testInvalidInputHandling() {
    // Test null raw data
    auto result1 = m_factory->createFromRawData(nullptr, 100);
    QVERIFY(!result1.success);
    QVERIFY(!result1.error.empty());
    
    // Test too small raw data
    uint8_t smallData[10];
    auto result2 = m_factory->createFromRawData(smallData, sizeof(smallData));
    QVERIFY(!result2.success);
    QVERIFY(!result2.error.empty());
    
    // Test zero size raw data
    uint8_t someData[100];
    auto result3 = m_factory->createFromRawData(someData, 0);
    QVERIFY(!result3.success);
    QVERIFY(!result3.error.empty());
    
    // Error statistics should be updated
    const auto& stats = m_factory->getStatistics();
    QVERIFY(stats.packetsWithErrors.load() >= 3);
}

void TestPacketFactory::testMemoryAllocationFailure() {
    // This is difficult to test without mocking memory allocation
    // For now, we test that very large allocations fail gracefully
    
    const size_t hugeSize = 100 * 1024 * 1024; // 100MB
    auto result = m_factory->createPacket(TEST_PACKET_ID, nullptr, hugeSize);
    
    QVERIFY(!result.success);
    QVERIFY(!result.error.empty());
}

void TestPacketFactory::testNullManagerHandling() {
    // Already tested in testFactoryConstruction
    // Constructor should throw with null memory manager
    try {
        PacketFactory nullFactory(nullptr);
        QFAIL("Should have thrown exception");
    } catch (const std::invalid_argument&) {
        QVERIFY(true);
    }
}

void TestPacketFactory::testEmptyPayloadCreation() {
    auto result = m_factory->createPacket(TEST_PACKET_ID, nullptr, 0);
    
    QVERIFY(result.success);
    QVERIFY(result.packet != nullptr);
    QVERIFY(result.packet->isValid());
    
    QCOMPARE(result.packet->payloadSize(), size_t(0));
    QCOMPARE(result.packet->totalSize(), PACKET_HEADER_SIZE);
    QVERIFY(result.packet->payload() == nullptr);
    
    // Test with explicit empty payload
    std::vector<uint8_t> emptyPayload;
    auto result2 = m_factory->createPacket(TEST_PACKET_ID, emptyPayload.data(), emptyPayload.size());
    QVERIFY(result2.success);
    QCOMPARE(result2.packet->payloadSize(), size_t(0));
}

void TestPacketFactory::testMaximumSizeCreation() {
    // Test with largest pool size minus header
    const size_t maxPayload = 8192 - PACKET_HEADER_SIZE;
    auto result = m_factory->createPacket(TEST_PACKET_ID, nullptr, maxPayload);
    
    QVERIFY(result.success);
    QVERIFY(result.packet != nullptr);
    QCOMPARE(result.packet->payloadSize(), maxPayload);
    QCOMPARE(result.packet->totalSize(), size_t(8192));
    
    // Test with size larger than any pool
    const size_t tooLarge = 10000;
    auto failResult = m_factory->createPacket(TEST_PACKET_ID, nullptr, tooLarge);
    QVERIFY(!failResult.success);
    QVERIFY(!failResult.error.empty());
}

void TestPacketFactory::testZeroSizeRawData() {
    uint8_t someData[100];
    auto result = m_factory->createFromRawData(someData, 0);
    
    QVERIFY(!result.success);
    QVERIFY(!result.error.empty());
    QVERIFY(result.packet == nullptr);
    
    // Error statistics should be updated
    const auto& stats = m_factory->getStatistics();
    QVERIFY(stats.packetsWithErrors.load() > 0);
}

void TestPacketFactory::testEventDispatcherIntegration() {
    // Test setting event dispatcher
    Events::EventDispatcher* dispatcher = m_app->eventDispatcher();
    m_factory->setEventDispatcher(dispatcher);
    
    // Factory should not crash and should still function
    auto result = m_factory->createPacket(TEST_PACKET_ID, nullptr, 100);
    QVERIFY(result.success);
    
    // Test with null dispatcher
    m_factory->setEventDispatcher(nullptr);
    auto result2 = m_factory->createPacket(TEST_PACKET_ID + 1, nullptr, 100);
    QVERIFY(result2.success); // Should still work
}

void TestPacketFactory::testStructureManagerIntegration() {
    // Test setting null structure manager
    m_factory->setStructureManager(nullptr);
    
    // Should handle gracefully
    auto result = m_factory->createFromStructure(TEST_PACKET_ID, "TestStruct");
    QVERIFY(!result.success);
    QVERIFY(!result.error.empty());
    QVERIFY(result.error.find("Structure manager not available") != std::string::npos);
    
    // Normal packet creation should still work
    auto normalResult = m_factory->createPacket(TEST_PACKET_ID, nullptr, 100);
    QVERIFY(normalResult.success);
}

void TestPacketFactory::testMultiComponentIntegration() {
    // Test that factory works with all components integrated
    Events::EventDispatcher* dispatcher = m_app->eventDispatcher();
    m_factory->setEventDispatcher(dispatcher);
    m_factory->setStructureManager(nullptr); // No structure manager for now
    
    QSignalSpy createdSpy(m_factory, &PacketFactory::packetCreated);
    QSignalSpy failedSpy(m_factory, &PacketFactory::packetCreationFailed);
    
    // Create successful packet
    auto successResult = m_factory->createPacket(1, nullptr, 100);
    QVERIFY(successResult.success);
    
    // Create failed packet
    uint8_t invalidData[5];
    auto failResult = m_factory->createFromRawData(invalidData, sizeof(invalidData));
    QVERIFY(!failResult.success);
    
    // Verify signals
    QTRY_COMPARE(createdSpy.count(), 1);
    QTRY_COMPARE(failedSpy.count(), 1);
    
    // Verify statistics
    const auto& stats = m_factory->getStatistics();
    QVERIFY(stats.packetsCreated.load() >= 1);
    QVERIFY(stats.packetsWithErrors.load() >= 1);
}

QTEST_MAIN(TestPacketFactory)
#include "test_packet_factory.moc"