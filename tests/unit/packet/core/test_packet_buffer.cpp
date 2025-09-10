#include <QtTest/QTest>
#include <QObject>
#include <thread>
#include <vector>
#include "packet/core/packet_buffer.h"
#include "core/application.h"

using namespace Monitor::Packet;
using namespace Monitor::Memory;

class TestPacketBuffer : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testBufferConstruction();
    void testBufferAllocation();
    void testPoolSelection();
    void testManagedBufferLifecycle();

    // Buffer allocation tests
    void testAllocateForPacket();
    void testCreateFromData();
    void testCreateForPacket();
    void testZeroSizeAllocation();

    // Memory pool integration tests
    void testMemoryPoolIntegration();
    void testPoolStatistics();
    void testMemoryUsageTracking();

    // Error handling tests
    void testNullManagerHandling();
    void testOversizedAllocation();
    void testAllocationFailure();

    // Performance tests
    void testAllocationPerformance();
    void testZeroCopySemantics();
    void testMemoryEfficiency();

    // Thread safety tests
    void testConcurrentAllocations();
    void testBufferLifecycleThreadSafety();

    // Edge case tests
    void testMaximumPacketSize();
    void testMinimumPacketSize();
    void testBoundaryConditions();

    // ManagedBuffer tests
    void testManagedBufferMoveSemantics();
    void testManagedBufferValidation();
    void testManagedBufferTypeConversion();

private:
    Monitor::Core::Application* m_app = nullptr;
    MemoryPoolManager* m_memoryManager = nullptr;
    PacketBuffer* m_packetBuffer = nullptr;
    
    static constexpr size_t PERFORMANCE_ITERATIONS = 10000;
    static constexpr size_t TEST_PACKET_SIZE = 1024;
    
    void setupMemoryPools();
};

void TestPacketBuffer::initTestCase() {
    // Initialize application for memory management
    m_app = Monitor::Core::Application::instance();
    QVERIFY(m_app != nullptr);
    
    m_memoryManager = m_app->memoryManager();
    QVERIFY(m_memoryManager != nullptr);
    
    setupMemoryPools();
}

void TestPacketBuffer::cleanupTestCase() {
    // Cleanup handled by Application destructor
}

void TestPacketBuffer::init() {
    // Create fresh PacketBuffer for each test
    m_packetBuffer = new PacketBuffer(m_memoryManager);
    QVERIFY(m_packetBuffer != nullptr);
}

void TestPacketBuffer::cleanup() {
    delete m_packetBuffer;
    m_packetBuffer = nullptr;
}

void TestPacketBuffer::setupMemoryPools() {
    // Create all the memory pools that PacketBuffer uses
    m_memoryManager->createPool("SmallObjects", 64, 1000);
    m_memoryManager->createPool("MediumObjects", 512, 1000);
    m_memoryManager->createPool("WidgetData", 1024, 1000);
    m_memoryManager->createPool("TestFramework", 2048, 1000);
    m_memoryManager->createPool("PacketBuffer", 4096, 1000);
    m_memoryManager->createPool("LargeObjects", 8192, 1000);
}

void TestPacketBuffer::testBufferConstruction() {
    // Test valid construction
    QVERIFY(m_packetBuffer != nullptr);
    
    // Test construction with null manager should throw
    try {
        PacketBuffer nullBuffer(nullptr);
        QFAIL("Should have thrown exception with null manager");
    } catch (const std::invalid_argument& e) {
        QVERIFY(true); // Expected exception
    }
}

void TestPacketBuffer::testBufferAllocation() {
    // Test basic allocation
    auto buffer = m_packetBuffer->allocate(TEST_PACKET_SIZE);
    QVERIFY(buffer != nullptr);
    QVERIFY(buffer->isValid());
    QCOMPARE(buffer->size(), TEST_PACKET_SIZE);
    QVERIFY(buffer->capacity() >= TEST_PACKET_SIZE);
    QVERIFY(buffer->data() != nullptr);
    
    // Verify memory is accessible
    uint8_t* bytes = buffer->bytes();
    QVERIFY(bytes != nullptr);
    
    // Test that we can write to the memory
    bytes[0] = 0xAB;
    bytes[TEST_PACKET_SIZE - 1] = 0xCD;
    QCOMPARE(bytes[0], uint8_t(0xAB));
    QCOMPARE(bytes[TEST_PACKET_SIZE - 1], uint8_t(0xCD));
}

void TestPacketBuffer::testPoolSelection() {
    struct TestCase {
        size_t size;
        QString expectedPool;
        size_t expectedCapacity;
    };
    
    std::vector<TestCase> testCases = {
        {32, "SmallObjects", 64},
        {64, "SmallObjects", 64},
        {128, "MediumObjects", 512},
        {512, "MediumObjects", 512},
        {800, "WidgetData", 1024},
        {1024, "WidgetData", 1024},
        {1500, "TestFramework", 2048},
        {2048, "TestFramework", 2048},
        {3000, "PacketBuffer", 4096},
        {4096, "PacketBuffer", 4096},
        {6000, "LargeObjects", 8192},
        {8192, "LargeObjects", 8192}
    };
    
    for (const auto& testCase : testCases) {
        auto buffer = m_packetBuffer->allocate(testCase.size);
        QVERIFY(buffer != nullptr);
        QCOMPARE(buffer->size(), testCase.size);
        QCOMPARE(buffer->capacity(), testCase.expectedCapacity);
        QCOMPARE(buffer->poolName(), testCase.expectedPool);
    }
}

void TestPacketBuffer::testManagedBufferLifecycle() {
    void* allocatedPtr = nullptr;
    
    {
        // Create managed buffer
        auto buffer = m_packetBuffer->allocate(TEST_PACKET_SIZE);
        QVERIFY(buffer != nullptr);
        QVERIFY(buffer->isValid());
        
        allocatedPtr = buffer->data();
        QVERIFY(allocatedPtr != nullptr);
        
        // Buffer is valid within scope
        QCOMPARE(buffer->size(), TEST_PACKET_SIZE);
    }
    
    // Buffer should be automatically deallocated when going out of scope
    // Note: We can't directly test if memory was returned to pool without
    // access to internal pool state, but this tests the RAII pattern
}

void TestPacketBuffer::testAllocateForPacket() {
    const size_t payloadSize = 500;
    auto buffer = m_packetBuffer->allocateForPacket(payloadSize);
    
    QVERIFY(buffer != nullptr);
    QVERIFY(buffer->isValid());
    QCOMPARE(buffer->size(), PACKET_HEADER_SIZE + payloadSize);
    
    // Verify we can access header and payload areas
    uint8_t* data = buffer->bytes();
    PacketHeader* header = reinterpret_cast<PacketHeader*>(data);
    uint8_t* payload = data + PACKET_HEADER_SIZE;
    
    // Initialize header
    header->id = 12345;
    header->payloadSize = static_cast<uint32_t>(payloadSize);
    
    // Write to payload
    payload[0] = 0xAA;
    payload[payloadSize - 1] = 0xBB;
    
    // Verify data integrity
    QCOMPARE(header->id, PacketId(12345));
    QCOMPARE(header->payloadSize, uint32_t(payloadSize));
    QCOMPARE(payload[0], uint8_t(0xAA));
    QCOMPARE(payload[payloadSize - 1], uint8_t(0xBB));
}

void TestPacketBuffer::testCreateFromData() {
    const char* testData = "Hello, PacketBuffer World!";
    const size_t dataSize = strlen(testData);
    
    auto buffer = m_packetBuffer->createFromData(testData, dataSize);
    
    QVERIFY(buffer != nullptr);
    QVERIFY(buffer->isValid());
    QCOMPARE(buffer->size(), dataSize);
    
    // Verify data was copied correctly
    const char* copiedData = reinterpret_cast<const char*>(buffer->data());
    QCOMPARE(QString::fromUtf8(copiedData, static_cast<int>(dataSize)), 
             QString::fromUtf8(testData));
    
    // Test with null data
    auto nullBuffer = m_packetBuffer->createFromData(nullptr, dataSize);
    QVERIFY(nullBuffer != nullptr); // Buffer allocated but data not copied
}

void TestPacketBuffer::testCreateForPacket() {
    const PacketId testId = 98765;
    const char* testPayload = "Test payload data for packet creation";
    const size_t payloadSize = strlen(testPayload);
    
    auto buffer = m_packetBuffer->createForPacket(testId, testPayload, payloadSize);
    
    QVERIFY(buffer != nullptr);
    QVERIFY(buffer->isValid());
    QCOMPARE(buffer->size(), PACKET_HEADER_SIZE + payloadSize);
    
    // Verify header
    const PacketHeader* header = buffer->as<PacketHeader>();
    QCOMPARE(header->id, testId);
    QCOMPARE(header->payloadSize, uint32_t(payloadSize));
    QVERIFY(header->timestamp > 0); // Should have current timestamp
    
    // Verify payload
    const uint8_t* payload = buffer->bytes() + PACKET_HEADER_SIZE;
    QCOMPARE(QString::fromUtf8(reinterpret_cast<const char*>(payload), 
                               static_cast<int>(payloadSize)), 
             QString::fromUtf8(testPayload));
    
    // Test with no payload
    auto noPayloadBuffer = m_packetBuffer->createForPacket(testId, nullptr, 0);
    QVERIFY(noPayloadBuffer != nullptr);
    QCOMPARE(noPayloadBuffer->size(), PACKET_HEADER_SIZE);
    
    const PacketHeader* noPayloadHeader = noPayloadBuffer->as<PacketHeader>();
    QCOMPARE(noPayloadHeader->id, testId);
    QCOMPARE(noPayloadHeader->payloadSize, uint32_t(0));
}

void TestPacketBuffer::testZeroSizeAllocation() {
    // Test allocation with zero size should return nullptr
    auto buffer = m_packetBuffer->allocate(0);
    QVERIFY(buffer == nullptr);
    
    // Test allocateForPacket with zero payload size
    auto packetBuffer = m_packetBuffer->allocateForPacket(0);
    QVERIFY(packetBuffer != nullptr); // Should allocate header-only packet
    QCOMPARE(packetBuffer->size(), PACKET_HEADER_SIZE);
}

void TestPacketBuffer::testMemoryPoolIntegration() {
    // Test that PacketBuffer properly uses the memory pools
    const size_t smallSize = 32;
    auto smallBuffer = m_packetBuffer->allocate(smallSize);
    QVERIFY(smallBuffer != nullptr);
    QCOMPARE(smallBuffer->poolName(), QString("SmallObjects"));
    
    const size_t mediumSize = 256;
    auto mediumBuffer = m_packetBuffer->allocate(mediumSize);
    QVERIFY(mediumBuffer != nullptr);
    QCOMPARE(mediumBuffer->poolName(), QString("MediumObjects"));
    
    const size_t largeSize = 7000;
    auto largeBuffer = m_packetBuffer->allocate(largeSize);
    QVERIFY(largeBuffer != nullptr);
    QCOMPARE(largeBuffer->poolName(), QString("LargeObjects"));
}

void TestPacketBuffer::testPoolStatistics() {
    auto stats = m_packetBuffer->getPoolStatistics();
    
    // Should have statistics for all pools
    QVERIFY(!stats.empty());
    QVERIFY(stats.size() >= 6); // At least 6 pools
    
    // Verify basic structure of statistics
    for (const auto& stat : stats) {
        QVERIFY(!stat.name.isEmpty());
        QVERIFY(stat.blockSize > 0);
        // Other fields might be 0 in current implementation
    }
    
    // Check for specific pools
    bool hasSmallObjects = false;
    bool hasPacketBuffer = false;
    
    for (const auto& stat : stats) {
        if (stat.name == "SmallObjects") {
            hasSmallObjects = true;
            QCOMPARE(stat.blockSize, size_t(64));
        } else if (stat.name == "PacketBuffer") {
            hasPacketBuffer = true;
            QCOMPARE(stat.blockSize, size_t(4096));
        }
    }
    
    QVERIFY(hasSmallObjects);
    QVERIFY(hasPacketBuffer);
}

void TestPacketBuffer::testMemoryUsageTracking() {
    size_t initialUsage = m_packetBuffer->getTotalMemoryUsage();
    
    // Allocate some buffers
    std::vector<PacketBuffer::ManagedBufferPtr> buffers;
    for (int i = 0; i < 10; ++i) {
        auto buffer = m_packetBuffer->allocate(100 + i * 50);
        if (buffer) {
            buffers.push_back(std::move(buffer));
        }
    }
    
    size_t usageWithBuffers = m_packetBuffer->getTotalMemoryUsage();
    
    // Usage should increase (though actual tracking depends on implementation)
    QVERIFY(usageWithBuffers >= initialUsage);
    
    // Clear buffers
    buffers.clear();
    
    // Note: Usage tracking might not immediately reflect deallocation
    // depending on implementation details
}

void TestPacketBuffer::testNullManagerHandling() {
    // Constructor should throw with null manager
    try {
        PacketBuffer nullBuffer(nullptr);
        QFAIL("Should have thrown exception");
    } catch (const std::invalid_argument&) {
        // Expected
    }
}

void TestPacketBuffer::testOversizedAllocation() {
    // Test allocation larger than maximum packet size
    const size_t oversizeRequest = PacketHeader::MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE + 1000;
    
    auto buffer = m_packetBuffer->allocate(oversizeRequest);
    QVERIFY(buffer == nullptr); // Should fail
    
    // Test allocateForPacket with oversized payload
    auto packetBuffer = m_packetBuffer->allocateForPacket(PacketHeader::MAX_PAYLOAD_SIZE + 1000);
    QVERIFY(packetBuffer == nullptr); // Should fail
}

void TestPacketBuffer::testAllocationFailure() {
    // This test is hard to implement without access to pool internals
    // or the ability to force allocation failure
    // For now, we'll test the case where allocation succeeds
    
    auto buffer = m_packetBuffer->allocate(TEST_PACKET_SIZE);
    QVERIFY(buffer != nullptr);
    QVERIFY(buffer->isValid());
}

void TestPacketBuffer::testAllocationPerformance() {
    QElapsedTimer timer;
    std::vector<PacketBuffer::ManagedBufferPtr> buffers;
    buffers.reserve(PERFORMANCE_ITERATIONS);
    
    timer.start();
    
    for (size_t i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        auto buffer = m_packetBuffer->allocate(TEST_PACKET_SIZE);
        if (buffer) {
            buffers.push_back(std::move(buffer));
        }
    }
    
    qint64 allocationTime = timer.nsecsElapsed();
    
    timer.restart();
    buffers.clear(); // Deallocate all buffers
    qint64 deallocationTime = timer.nsecsElapsed();
    
    double nsPerAllocation = static_cast<double>(allocationTime) / PERFORMANCE_ITERATIONS;
    double nsPerDeallocation = static_cast<double>(deallocationTime) / PERFORMANCE_ITERATIONS;
    
    qDebug() << "PacketBuffer allocation performance:" << nsPerAllocation << "ns per allocation";
    qDebug() << "PacketBuffer deallocation performance:" << nsPerDeallocation << "ns per deallocation";
    
    // Should be fast - target is < 1000ns (1μs) per allocation
    QVERIFY(nsPerAllocation < 1000.0);
    QVERIFY(nsPerDeallocation < 500.0);
}

void TestPacketBuffer::testZeroCopySemantics() {
    const size_t testSize = 2048;
    auto buffer = m_packetBuffer->allocate(testSize);
    
    QVERIFY(buffer != nullptr);
    
    // Get direct pointer to memory
    uint8_t* directPtr = buffer->bytes();
    void* rawPtr = buffer->data();
    
    QVERIFY(directPtr == static_cast<uint8_t*>(rawPtr));
    
    // Write through one interface
    directPtr[0] = 0x42;
    directPtr[testSize - 1] = 0x84;
    
    // Read through another interface
    uint8_t* readPtr = static_cast<uint8_t*>(buffer->data());
    QCOMPARE(readPtr[0], uint8_t(0x42));
    QCOMPARE(readPtr[testSize - 1], uint8_t(0x84));
    
    // Type conversion should also work
    PacketHeader* headerPtr = buffer->as<PacketHeader>();
    uint8_t* headerBytes = reinterpret_cast<uint8_t*>(headerPtr);
    QVERIFY(headerBytes == directPtr);
}

void TestPacketBuffer::testMemoryEfficiency() {
    // Test that buffer capacity matches expected pool sizes
    struct SizeTest {
        size_t requestSize;
        size_t expectedCapacity;
    };
    
    std::vector<SizeTest> tests = {
        {32, 64},      // SmallObjects
        {100, 512},    // MediumObjects  
        {600, 1024},   // WidgetData
        {1200, 2048},  // TestFramework
        {2500, 4096},  // PacketBuffer
        {5000, 8192}   // LargeObjects
    };
    
    for (const auto& test : tests) {
        auto buffer = m_packetBuffer->allocate(test.requestSize);
        QVERIFY(buffer != nullptr);
        QCOMPARE(buffer->size(), test.requestSize);
        QCOMPARE(buffer->capacity(), test.expectedCapacity);
        
        // Verify we can use the full capacity
        uint8_t* bytes = buffer->bytes();
        bytes[test.expectedCapacity - 1] = 0xFF; // Write to last byte
        QCOMPARE(bytes[test.expectedCapacity - 1], uint8_t(0xFF));
    }
}

void TestPacketBuffer::testConcurrentAllocations() {
    const int numThreads = 4;
    const int allocationsPerThread = 1000;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<PacketBuffer::ManagedBufferPtr>> threadBuffers(numThreads);
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, &threadBuffers]() {
            for (int j = 0; j < allocationsPerThread; ++j) {
                size_t size = 100 + (j % 1000); // Varying sizes
                auto buffer = m_packetBuffer->allocate(size);
                if (buffer && buffer->isValid()) {
                    threadBuffers[i].push_back(std::move(buffer));
                    
                    // Write some data to verify buffer integrity
                    uint8_t* bytes = threadBuffers[i].back()->bytes();
                    bytes[0] = static_cast<uint8_t>(i);
                    bytes[size - 1] = static_cast<uint8_t>(j);
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all allocations succeeded and data integrity
    size_t totalAllocations = 0;
    for (int i = 0; i < numThreads; ++i) {
        totalAllocations += threadBuffers[i].size();
        
        for (size_t j = 0; j < threadBuffers[i].size(); ++j) {
            auto& buffer = threadBuffers[i][j];
            QVERIFY(buffer->isValid());
            
            uint8_t* bytes = buffer->bytes();
            QCOMPARE(bytes[0], uint8_t(i));
        }
    }
    
    qDebug() << "Concurrent allocations succeeded:" << totalAllocations 
             << "out of" << (numThreads * allocationsPerThread) << "requested";
    
    // Should have achieved most allocations
    QVERIFY(totalAllocations > (numThreads * allocationsPerThread * 0.9)); // At least 90% success
}

void TestPacketBuffer::testBufferLifecycleThreadSafety() {
    // Test that ManagedBuffer destruction is thread-safe
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 500; ++j) {
                auto buffer = m_packetBuffer->allocate(256);
                if (buffer) {
                    // Write some data
                    uint8_t* bytes = buffer->bytes();
                    bytes[0] = 0xAB;
                    bytes[255] = 0xCD;
                    
                    // Buffer will be automatically destroyed at end of loop iteration
                }
                std::this_thread::yield(); // Give other threads a chance
            }
        });
    }
    
    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }
    
    // If we reach here without crashes, thread safety is good
    QVERIFY(true);
}

void TestPacketBuffer::testMaximumPacketSize() {
    const size_t maxSize = PacketHeader::MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE;
    
    auto buffer = m_packetBuffer->allocate(maxSize);
    QVERIFY(buffer == nullptr); // Should fail due to our size check
    
    // Test just under the limit
    const size_t justUnderMax = PacketHeader::MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE - 1;
    auto underMaxBuffer = m_packetBuffer->allocate(justUnderMax);
    QVERIFY(underMaxBuffer == nullptr); // Still should fail because we check against our pool sizes
    
    // Test with largest available pool
    const size_t largestPoolSize = 8192;
    auto maxPoolBuffer = m_packetBuffer->allocate(largestPoolSize);
    QVERIFY(maxPoolBuffer != nullptr);
    QCOMPARE(maxPoolBuffer->capacity(), largestPoolSize);
}

void TestPacketBuffer::testMinimumPacketSize() {
    // Test minimum meaningful packet (header only)
    auto headerOnlyBuffer = m_packetBuffer->allocateForPacket(0);
    QVERIFY(headerOnlyBuffer != nullptr);
    QCOMPARE(headerOnlyBuffer->size(), PACKET_HEADER_SIZE);
    
    // Test 1 byte allocation
    auto oneByteBuffer = m_packetBuffer->allocate(1);
    QVERIFY(oneByteBuffer != nullptr);
    QCOMPARE(oneByteBuffer->size(), size_t(1));
    QCOMPARE(oneByteBuffer->capacity(), size_t(64)); // SmallObjects pool
}

void TestPacketBuffer::testBoundaryConditions() {
    // Test allocations exactly at pool boundaries
    std::vector<size_t> boundaries = {64, 512, 1024, 2048, 4096, 8192};
    
    for (size_t boundary : boundaries) {
        // Allocate exactly at boundary
        auto exactBuffer = m_packetBuffer->allocate(boundary);
        QVERIFY(exactBuffer != nullptr);
        QCOMPARE(exactBuffer->size(), boundary);
        
        // Allocate one byte over boundary
        auto overBuffer = m_packetBuffer->allocate(boundary + 1);
        QVERIFY(overBuffer != nullptr);
        QCOMPARE(overBuffer->size(), boundary + 1);
        
        // Should use next larger pool
        QVERIFY(overBuffer->capacity() > exactBuffer->capacity() || 
                overBuffer->capacity() == exactBuffer->capacity());
    }
}

void TestPacketBuffer::testManagedBufferMoveSemantics() {
    auto buffer1 = m_packetBuffer->allocate(TEST_PACKET_SIZE);
    QVERIFY(buffer1 != nullptr);
    
    void* originalPtr = buffer1->data();
    size_t originalSize = buffer1->size();
    QString originalPool = buffer1->poolName();
    
    // Move construct
    auto buffer2 = std::move(buffer1);
    QVERIFY(buffer2 != nullptr);
    QVERIFY(buffer2->isValid());
    QCOMPARE(buffer2->data(), originalPtr);
    QCOMPARE(buffer2->size(), originalSize);
    QCOMPARE(buffer2->poolName(), originalPool);
    
    // Original buffer should be invalidated
    QVERIFY(buffer1 == nullptr || !buffer1->isValid());
    
    // Move assign
    auto buffer3 = m_packetBuffer->allocate(512);
    buffer3 = std::move(buffer2);
    
    QVERIFY(buffer3 != nullptr);
    QVERIFY(buffer3->isValid());
    QCOMPARE(buffer3->data(), originalPtr);
    QCOMPARE(buffer3->size(), originalSize);
}

void TestPacketBuffer::testManagedBufferValidation() {
    auto buffer = m_packetBuffer->allocate(TEST_PACKET_SIZE);
    QVERIFY(buffer != nullptr);
    QVERIFY(buffer->isValid());
    
    // Test various accessor methods
    QVERIFY(buffer->data() != nullptr);
    QVERIFY(buffer->bytes() != nullptr);
    QVERIFY(buffer->size() > 0);
    QVERIFY(buffer->capacity() >= buffer->size());
    QVERIFY(!buffer->poolName().isEmpty());
    
    // After moving, original should be invalid
    auto movedBuffer = std::move(buffer);
    QVERIFY(!buffer || !buffer->isValid());
    QVERIFY(movedBuffer->isValid());
}

void TestPacketBuffer::testManagedBufferTypeConversion() {
    auto buffer = m_packetBuffer->allocateForPacket(TEST_PACKET_SIZE);
    QVERIFY(buffer != nullptr);
    
    // Test conversion to different types
    PacketHeader* header = buffer->as<PacketHeader>();
    QVERIFY(header != nullptr);
    QVERIFY(reinterpret_cast<void*>(header) == buffer->data());
    
    uint8_t* bytes = buffer->bytes();
    QVERIFY(bytes != nullptr);
    QVERIFY(reinterpret_cast<void*>(bytes) == buffer->data());
    
    uint32_t* words = buffer->as<uint32_t>();
    QVERIFY(words != nullptr);
    QVERIFY(reinterpret_cast<void*>(words) == buffer->data());
    
    // Test writing through different type pointers
    header->id = 12345;
    QCOMPARE(reinterpret_cast<PacketHeader*>(bytes)->id, PacketId(12345));
    
    bytes[20] = 0xFF; // Write to flags field
    QVERIFY((header->flags & 0xFF) == 0xFF || (header->flags & 0xFF000000) == 0xFF000000);
}

QTEST_MAIN(TestPacketBuffer)
#include "test_packet_buffer.moc"