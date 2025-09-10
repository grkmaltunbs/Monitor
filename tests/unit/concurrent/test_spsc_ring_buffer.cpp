#include <QtTest/QTest>
#include <QtCore/QObject>
#include <QtCore/QThread>
#include "../../../src/concurrent/spsc_ring_buffer.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <random>

class TestSPSCRingBuffer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // Basic functionality tests
    void testConstruction();
    void testBasicPushPop();
    void testSizeAndCapacity();
    void testEmptyAndFull();
    void testPowerOfTwoCapacity();
    void testClear();
    
    // Data integrity tests
    void testDataOrdering();
    void testPushPopSequence();
    void testPeekOperation();
    void testMoveSemantics();
    
    // Concurrency tests (SPSC specific)
    void testProducerConsumerBasic();
    void testHighThroughputSPSC();
    void testProducerConsumerStress();
    void testBufferOverflow();
    void testBufferUnderflow();
    
    // Performance tests
    void testLatency();
    void testThroughput();
    void testMemoryUsage();
    
    // Statistics tests
    void testStatistics();
    void testStatisticsReset();
    
    // Edge cases
    void testSingleElementBuffer();
    void testLargeBuffer();
    void testCustomTypes();
    void testExceptionSafety();

private:
    using IntBuffer = Monitor::Concurrent::SPSCRingBuffer<int>;
    using StringBuffer = Monitor::Concurrent::SPSCRingBuffer<QString>;
    
    struct TestData {
        int id;
        QString message;
        double value;
        
        TestData() : id(0), value(0.0) {}
        TestData(int i, const QString& msg, double v) : id(i), message(msg), value(v) {}
        
        bool operator==(const TestData& other) const {
            return id == other.id && message == other.message && 
                   qAbs(value - other.value) < 0.001;
        }
    };
    
    using TestDataBuffer = Monitor::Concurrent::SPSCRingBuffer<TestData>;
};

void TestSPSCRingBuffer::initTestCase()
{
    qDebug() << "Testing SPSC Ring Buffer implementation";
}

void TestSPSCRingBuffer::cleanupTestCase()
{
    qDebug() << "SPSC Ring Buffer tests completed";
}

void TestSPSCRingBuffer::testConstruction()
{
    // Test normal construction
    IntBuffer buffer(16);
    QCOMPARE(buffer.capacity(), size_t(16));
    QCOMPARE(buffer.mask(), size_t(15));
    QVERIFY(buffer.empty());
    QVERIFY(!buffer.full());
    QCOMPARE(buffer.size(), size_t(0));
    
    // Test power-of-2 adjustment
    IntBuffer buffer2(15); // Should round up to 16
    QCOMPARE(buffer2.capacity(), size_t(16));
    
    IntBuffer buffer3(17); // Should round up to 32
    QCOMPARE(buffer3.capacity(), size_t(32));
    
    // Test minimum size
    IntBuffer buffer4(0); // Should throw or use minimum
    QVERIFY(buffer4.capacity() > 0);
    
    // Test exception on invalid size
    try {
        IntBuffer largeBuffer(SIZE_MAX);
        QFAIL("Should have thrown exception for too large capacity");
    } catch (const std::exception&) {
        // Expected
    }
}

void TestSPSCRingBuffer::testBasicPushPop()
{
    IntBuffer buffer(8);
    
    // Test basic push
    QVERIFY(buffer.tryPush(42));
    QCOMPARE(buffer.size(), size_t(1));
    QVERIFY(!buffer.empty());
    QVERIFY(!buffer.full());
    
    // Test basic pop
    int value;
    QVERIFY(buffer.tryPop(value));
    QCOMPARE(value, 42);
    QCOMPARE(buffer.size(), size_t(0));
    QVERIFY(buffer.empty());
    QVERIFY(!buffer.full());
    
    // Test pop from empty buffer
    QVERIFY(!buffer.tryPop(value));
}

void TestSPSCRingBuffer::testSizeAndCapacity()
{
    IntBuffer buffer(16);
    QCOMPARE(buffer.capacity(), size_t(16));
    QCOMPARE(buffer.size(), size_t(0));
    
    // Fill buffer partially
    for (int i = 0; i < 5; ++i) {
        QVERIFY(buffer.tryPush(i));
    }
    QCOMPARE(buffer.size(), size_t(5));
    
    // Pop some elements
    int value;
    QVERIFY(buffer.tryPop(value));
    QVERIFY(buffer.tryPop(value));
    QCOMPARE(buffer.size(), size_t(3));
}

void TestSPSCRingBuffer::testEmptyAndFull()
{
    IntBuffer buffer(4);
    
    // Initially empty
    QVERIFY(buffer.empty());
    QVERIFY(!buffer.full());
    
    // Fill to capacity (n-1 for ring buffer)
    for (int i = 0; i < 3; ++i) {
        QVERIFY(buffer.tryPush(i));
    }
    QVERIFY(!buffer.empty());
    QVERIFY(buffer.full());
    
    // Try to push to full buffer
    QVERIFY(!buffer.tryPush(999));
    
    // Pop all elements
    int value;
    while (buffer.tryPop(value)) {
        // Keep popping
    }
    QVERIFY(buffer.empty());
    QVERIFY(!buffer.full());
}

void TestSPSCRingBuffer::testPowerOfTwoCapacity()
{
    struct TestCase {
        size_t input;
        size_t expected;
    };
    
    std::vector<TestCase> testCases = {
        {1, 1}, {2, 2}, {3, 4}, {4, 4}, {7, 8}, {8, 8}, 
        {15, 16}, {16, 16}, {31, 32}, {32, 32}
    };
    
    for (const auto& testCase : testCases) {
        IntBuffer buffer(testCase.input);
        QCOMPARE(buffer.capacity(), testCase.expected);
    }
}

void TestSPSCRingBuffer::testClear()
{
    IntBuffer buffer(8);
    
    // Add some elements
    for (int i = 0; i < 5; ++i) {
        QVERIFY(buffer.tryPush(i));
    }
    QCOMPARE(buffer.size(), size_t(5));
    
    // Clear buffer
    buffer.clear();
    QCOMPARE(buffer.size(), size_t(0));
    QVERIFY(buffer.empty());
    
    // Should be able to add elements after clear
    QVERIFY(buffer.tryPush(999));
    int value;
    QVERIFY(buffer.tryPop(value));
    QCOMPARE(value, 999);
}

void TestSPSCRingBuffer::testDataOrdering()
{
    IntBuffer buffer(16);
    const int numElements = 10;
    
    // Push elements in order
    for (int i = 0; i < numElements; ++i) {
        QVERIFY(buffer.tryPush(i));
    }
    
    // Pop elements and verify order (FIFO)
    for (int i = 0; i < numElements; ++i) {
        int value;
        QVERIFY(buffer.tryPop(value));
        QCOMPARE(value, i);
    }
}

void TestSPSCRingBuffer::testPushPopSequence()
{
    IntBuffer buffer(8);
    const int iterations = 1000;
    
    // Alternating push/pop sequence
    for (int i = 0; i < iterations; ++i) {
        QVERIFY(buffer.tryPush(i));
        
        int value;
        QVERIFY(buffer.tryPop(value));
        QCOMPARE(value, i);
        QVERIFY(buffer.empty());
    }
}

void TestSPSCRingBuffer::testPeekOperation()
{
    IntBuffer buffer(8);
    
    // Test peek on empty buffer
    int value;
    QVERIFY(!buffer.tryPeek(value));
    
    // Add element and peek
    QVERIFY(buffer.tryPush(42));
    QVERIFY(buffer.tryPeek(value));
    QCOMPARE(value, 42);
    
    // Peek should not remove element
    QCOMPARE(buffer.size(), size_t(1));
    QVERIFY(buffer.tryPeek(value));
    QCOMPARE(value, 42);
    
    // Pop should still work
    QVERIFY(buffer.tryPop(value));
    QCOMPARE(value, 42);
    QVERIFY(buffer.empty());
}

void TestSPSCRingBuffer::testMoveSemantics()
{
    StringBuffer buffer(8);
    QString testString = "Hello, World!";
    
    // Test move push
    QString moveString = testString;
    QVERIFY(buffer.tryPush(std::move(moveString)));
    QVERIFY(moveString.isEmpty()); // Should be moved from
    
    // Test pop
    QString result;
    QVERIFY(buffer.tryPop(result));
    QCOMPARE(result, testString);
}

void TestSPSCRingBuffer::testProducerConsumerBasic()
{
    IntBuffer buffer(64);
    const int numItems = 1000;
    std::atomic<int> producedCount{0};
    std::atomic<int> consumedCount{0};
    std::atomic<bool> producerDone{false};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < numItems; ++i) {
            while (!buffer.tryPush(i)) {
                std::this_thread::yield();
            }
            producedCount++;
        }
        producerDone = true;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int value;
        int expectedValue = 0;
        
        while (consumedCount < numItems) {
            if (buffer.tryPop(value)) {
                QCOMPARE(value, expectedValue++);
                consumedCount++;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    QCOMPARE(producedCount.load(), numItems);
    QCOMPARE(consumedCount.load(), numItems);
    QVERIFY(buffer.empty());
}

void TestSPSCRingBuffer::testHighThroughputSPSC()
{
    IntBuffer buffer(1024);
    const int numItems = 100000;
    std::atomic<bool> testComplete{false};
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < numItems; ++i) {
            while (!buffer.tryPush(i)) {
                std::this_thread::yield();
            }
        }
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int value;
        int consumed = 0;
        
        while (consumed < numItems) {
            if (buffer.tryPop(value)) {
                consumed++;
            } else {
                std::this_thread::yield();
            }
        }
        testComplete = true;
    });
    
    producer.join();
    consumer.join();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    double throughput = (numItems * 1000000.0) / duration.count();
    qDebug() << "SPSC Throughput:" << throughput << "items/second";
    
    // Should achieve high throughput
    QVERIFY(throughput > 1000000.0); // At least 1M items/second
}

void TestSPSCRingBuffer::testProducerConsumerStress()
{
    IntBuffer buffer(256);
    const int testDuration = 1000; // 1 second
    std::atomic<int> itemsProduced{0};
    std::atomic<int> itemsConsumed{0};
    std::atomic<bool> stopTest{false};
    
    // Producer thread
    std::thread producer([&]() {
        int value = 0;
        while (!stopTest.load()) {
            if (buffer.tryPush(value++)) {
                itemsProduced++;
            }
            std::this_thread::yield();
        }
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int value;
        while (!stopTest.load()) {
            if (buffer.tryPop(value)) {
                itemsConsumed++;
            }
            std::this_thread::yield();
        }
        
        // Consume remaining items
        while (buffer.tryPop(value)) {
            itemsConsumed++;
        }
    });
    
    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::milliseconds(testDuration));
    stopTest = true;
    
    producer.join();
    consumer.join();
    
    qDebug() << "Stress test - Produced:" << itemsProduced.load() << "Consumed:" << itemsConsumed.load();
    
    // All produced items should be consumed
    QCOMPARE(itemsProduced.load(), itemsConsumed.load());
    QVERIFY(buffer.empty());
}

void TestSPSCRingBuffer::testBufferOverflow()
{
    IntBuffer buffer(4);
    
    // Fill buffer to capacity
    for (int i = 0; i < 3; ++i) { // 3 items for size-4 ring buffer
        QVERIFY(buffer.tryPush(i));
    }
    QVERIFY(buffer.full());
    
    // Try to push to full buffer
    QVERIFY(!buffer.tryPush(999));
    
    // Statistics should reflect failed push
    auto stats = buffer.getStatistics();
    QVERIFY(stats.pushFailures > 0);
}

void TestSPSCRingBuffer::testBufferUnderflow()
{
    IntBuffer buffer(8);
    
    // Try to pop from empty buffer
    int value;
    QVERIFY(!buffer.tryPop(value));
    
    // Statistics should reflect failed pop
    auto stats = buffer.getStatistics();
    QVERIFY(stats.popFailures > 0);
}

void TestSPSCRingBuffer::testLatency()
{
    IntBuffer buffer(64);
    const int numSamples = 1000;
    std::vector<std::chrono::nanoseconds> latencies;
    latencies.reserve(numSamples);
    
    // Measure push latency
    for (int i = 0; i < numSamples; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        buffer.tryPush(i);
        auto end = std::chrono::high_resolution_clock::now();
        
        latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start));
        
        // Pop to prevent buffer from filling
        int value;
        buffer.tryPop(value);
    }
    
    // Calculate average latency
    auto totalLatency = std::accumulate(latencies.begin(), latencies.end(), 
                                       std::chrono::nanoseconds{0});
    auto avgLatency = totalLatency / numSamples;
    
    qDebug() << "Average push latency:" << avgLatency.count() << "nanoseconds";
    
    // Should be very low latency (< 1 microsecond)
    QVERIFY(avgLatency.count() < 1000);
}

void TestSPSCRingBuffer::testThroughput()
{
    IntBuffer buffer(1024);
    const int numItems = 1000000;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Push items
    for (int i = 0; i < numItems; ++i) {
        while (!buffer.tryPush(i)) {
            // This should not happen with large buffer and single thread
            QFAIL("Unexpected push failure in throughput test");
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    double throughput = (numItems * 1000000.0) / duration.count();
    qDebug() << "Single-threaded push throughput:" << throughput << "items/second";
    
    // Should achieve very high throughput for single-threaded case
    QVERIFY(throughput > 10000000.0); // At least 10M items/second
}

void TestSPSCRingBuffer::testStatistics()
{
    IntBuffer buffer(8);
    
    // Initial statistics
    auto stats = buffer.getStatistics();
    QCOMPARE(stats.totalPushes, size_t(0));
    QCOMPARE(stats.totalPops, size_t(0));
    QCOMPARE(stats.pushFailures, size_t(0));
    QCOMPARE(stats.popFailures, size_t(0));
    QCOMPARE(stats.currentSize, size_t(0));
    QCOMPARE(stats.utilizationPercent, 0.0);
    
    // Add some elements
    const int numElements = 5;
    for (int i = 0; i < numElements; ++i) {
        QVERIFY(buffer.tryPush(i));
    }
    
    stats = buffer.getStatistics();
    QCOMPARE(stats.totalPushes, size_t(numElements));
    QCOMPARE(stats.currentSize, size_t(numElements));
    QVERIFY(stats.utilizationPercent > 0.0);
    
    // Pop some elements
    int value;
    QVERIFY(buffer.tryPop(value));
    QVERIFY(buffer.tryPop(value));
    
    stats = buffer.getStatistics();
    QCOMPARE(stats.totalPops, size_t(2));
    QCOMPARE(stats.currentSize, size_t(3));
    
    // Test failure statistics
    // Fill buffer
    while (!buffer.full()) {
        buffer.tryPush(999);
    }
    
    // Try to push to full buffer
    QVERIFY(!buffer.tryPush(999));
    stats = buffer.getStatistics();
    QVERIFY(stats.pushFailures > 0);
    
    // Empty buffer and try to pop
    buffer.clear();
    QVERIFY(!buffer.tryPop(value));
    stats = buffer.getStatistics();
    QVERIFY(stats.popFailures > 0);
}

void TestSPSCRingBuffer::testStatisticsReset()
{
    IntBuffer buffer(8);
    
    // Generate some activity
    for (int i = 0; i < 5; ++i) {
        buffer.tryPush(i);
    }
    
    int value;
    buffer.tryPop(value);
    
    // Verify statistics are non-zero
    auto stats = buffer.getStatistics();
    QVERIFY(stats.totalPushes > 0);
    QVERIFY(stats.totalPops > 0);
    
    // Reset statistics
    buffer.resetStatistics();
    
    // Verify statistics are reset
    stats = buffer.getStatistics();
    QCOMPARE(stats.totalPushes, size_t(0));
    QCOMPARE(stats.totalPops, size_t(0));
    QCOMPARE(stats.pushFailures, size_t(0));
    QCOMPARE(stats.popFailures, size_t(0));
}

void TestSPSCRingBuffer::testSingleElementBuffer()
{
    IntBuffer buffer(1);
    QCOMPARE(buffer.capacity(), size_t(1));
    
    // Should be able to push one element
    QVERIFY(buffer.tryPush(42));
    QVERIFY(buffer.full());
    
    // Should not be able to push another
    QVERIFY(!buffer.tryPush(99));
    
    // Should be able to pop
    int value;
    QVERIFY(buffer.tryPop(value));
    QCOMPARE(value, 42);
    QVERIFY(buffer.empty());
}

void TestSPSCRingBuffer::testLargeBuffer()
{
    const size_t largeSize = 65536; // 64K elements
    IntBuffer buffer(largeSize);
    QCOMPARE(buffer.capacity(), largeSize);
    
    // Fill half the buffer
    const int numElements = largeSize / 2;
    for (int i = 0; i < numElements; ++i) {
        QVERIFY(buffer.tryPush(i));
    }
    
    QCOMPARE(buffer.size(), size_t(numElements));
    
    // Pop all elements
    int value;
    for (int i = 0; i < numElements; ++i) {
        QVERIFY(buffer.tryPop(value));
        QCOMPARE(value, i);
    }
    
    QVERIFY(buffer.empty());
}

void TestSPSCRingBuffer::testCustomTypes()
{
    TestDataBuffer buffer(16);
    
    // Test with custom struct
    TestData data1(1, "Hello", 3.14);
    TestData data2(2, "World", 2.71);
    
    QVERIFY(buffer.tryPush(data1));
    QVERIFY(buffer.tryPush(data2));
    
    TestData result;
    QVERIFY(buffer.tryPop(result));
    QVERIFY(result == data1);
    
    QVERIFY(buffer.tryPop(result));
    QVERIFY(result == data2);
    
    QVERIFY(buffer.empty());
}

void TestSPSCRingBuffer::testExceptionSafety()
{
    // Test that buffer remains in valid state after exceptions
    static bool shouldThrow = false;
    
    struct ThrowingType {
        int value;
        
        ThrowingType(int v = 0) : value(v) {
            if (shouldThrow && v == 42) {
                throw std::runtime_error("Test exception");
            }
        }
        
        ThrowingType(const ThrowingType& other) : value(other.value) {
            if (shouldThrow && value == 42) {
                throw std::runtime_error("Copy exception");
            }
        }
        
        ThrowingType& operator=(const ThrowingType& other) {
            if (shouldThrow && other.value == 42) {
                throw std::runtime_error("Assignment exception");
            }
            value = other.value;
            return *this;
        }
    };
    
    shouldThrow = false;
    
    Monitor::Concurrent::SPSCRingBuffer<ThrowingType> buffer(8);
    
    // Add some normal elements
    QVERIFY(buffer.tryPush(ThrowingType(1)));
    QVERIFY(buffer.tryPush(ThrowingType(2)));
    
    // Enable throwing
    shouldThrow = true;
    
    // Try to add throwing element - should handle gracefully
    try {
        buffer.tryPush(ThrowingType(42));
    } catch (const std::exception&) {
        // Exception is expected
    }
    
    // Buffer should still be functional
    shouldThrow = false;
    QVERIFY(buffer.tryPush(ThrowingType(3)));
    
    ThrowingType result;
    QVERIFY(buffer.tryPop(result));
    QCOMPARE(result.value, 1);
}

QTEST_MAIN(TestSPSCRingBuffer)
#include "test_spsc_ring_buffer.moc"