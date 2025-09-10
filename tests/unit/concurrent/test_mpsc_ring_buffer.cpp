#include <QtTest/QtTest>
#include <QtCore/QElapsedTimer>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <memory>
#include <random>

#include "concurrent/mpsc_ring_buffer.h"

using namespace Monitor::Concurrent;

class TestMPSCRingBuffer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testConstruction();
    void testBasicProduceConsume();
    void testCapacityAndSize();
    void testEmptyAndFull();
    void testClear();
    
    // Single producer tests
    void testSingleProducerSequential();
    void testSingleProducerBatch();
    void testSingleProducerOverflow();
    
    // Multiple producer tests
    void testMultipleProducersConcurrent();
    void testMultipleProducersHighContention();
    void testMultipleProducersWithBackpressure();
    void testMixedProducerWorkloads();
    
    // Consumer tests
    void testConsumerBasicOperations();
    void testConsumerBatchOperations();
    void testConsumerWithEmptyBuffer();
    void testConsumerUnderProducerStress();
    
    // Performance tests
    void testHighThroughputMultipleProducers();
    void testLowLatencyOperations();
    void testMemoryEfficiency();
    void testCacheLineAlignmentPerformance();
    
    // Back-pressure tests
    void testBackpressureHandling();
    void testBackpressureRecovery();
    void testBackpressureFairness();
    
    // Stress tests
    void testLongRunningStress();
    void testMemoryStressWithLargeItems();
    void testMaxCapacityStress();
    void testConcurrentStressTest();
    
    // Edge case tests
    void testZeroCapacityBuffer();
    void testSingleItemBuffer();
    void testLargeCapacityBuffer();
    void testDataIntegrity();
    
    // Alignment and memory tests
    void testCacheLineAlignment();
    void testMemoryOrdering();
    void testFalseSharing();

private:
    // Test data structures
    struct TestItem {
        int value;
        std::chrono::steady_clock::time_point timestamp;
        
        TestItem() : value(0) {}
        TestItem(int v) : value(v), timestamp(std::chrono::steady_clock::now()) {}
        
        bool operator==(const TestItem& other) const {
            return value == other.value;
        }
    };
    
    struct LargeTestItem {
        int data[64]; // 256 bytes
        
        LargeTestItem(int value = 0) {
            for (int i = 0; i < 64; ++i) {
                data[i] = value + i;
            }
        }
        
        bool operator==(const LargeTestItem& other) const {
            return std::equal(data, data + 64, other.data);
        }
    };
    
    // Helper methods
    void runMultipleProducers(MPSCRingBuffer<TestItem>& buffer, int numProducers, 
                             int itemsPerProducer, std::atomic<int>& totalProduced);
    void runConsumer(MPSCRingBuffer<TestItem>& buffer, int expectedItems,
                    std::atomic<int>& totalConsumed, int timeoutMs = 5000);
    void verifySequentialData(const std::vector<TestItem>& items, int expectedStart, int expectedCount);
    bool verifyBackpressureHandling(MPSCRingBuffer<TestItem>& buffer, int numProducers);
    void measureLatency(MPSCRingBuffer<TestItem>& buffer, int iterations, double& avgLatencyNs);
};

void TestMPSCRingBuffer::initTestCase()
{
    qDebug() << "Starting MPSC Ring Buffer comprehensive tests";
    qDebug() << "Cache line size: 64 bytes (assumed)"; // getCacheLineSize not available
}

void TestMPSCRingBuffer::cleanupTestCase()
{
    qDebug() << "MPSC Ring Buffer tests completed";
}

void TestMPSCRingBuffer::init()
{
    // Fresh state for each test
}

void TestMPSCRingBuffer::cleanup()
{
    // Cleanup after each test
}

void TestMPSCRingBuffer::testConstruction()
{
    // Test power of 2 capacity
    MPSCRingBuffer<int> buffer1(16);
    QCOMPARE(buffer1.capacity(), static_cast<size_t>(16));
    QCOMPARE(buffer1.size(), static_cast<size_t>(0));
    QVERIFY(buffer1.empty());
    QVERIFY(!buffer1.full());
    
    // Test non-power of 2 capacity (should round up)
    MPSCRingBuffer<int> buffer2(15);
    QCOMPARE(buffer2.capacity(), static_cast<size_t>(16)); // Rounded up to next power of 2
    
    // Test large capacity
    MPSCRingBuffer<int> buffer3(1024);
    QCOMPARE(buffer3.capacity(), static_cast<size_t>(1024));
    
    // Test minimum capacity
    MPSCRingBuffer<int> buffer4(1);
    QCOMPARE(buffer4.capacity(), static_cast<size_t>(2)); // Minimum power of 2
}

void TestMPSCRingBuffer::testBasicProduceConsume()
{
    MPSCRingBuffer<TestItem> buffer(8);
    
    // Test single produce/consume
    TestItem item1(42);
    QVERIFY(buffer.tryPush(item1));
    QCOMPARE(buffer.size(), static_cast<size_t>(1));
    QVERIFY(!buffer.empty());
    
    TestItem consumed;
    QVERIFY(buffer.tryPop(consumed));
    QCOMPARE(consumed.value, 42);
    QCOMPARE(buffer.size(), static_cast<size_t>(0));
    QVERIFY(buffer.empty());
    
    // Test consume from empty buffer
    TestItem empty;
    QVERIFY(!buffer.tryPop(empty));
}

void TestMPSCRingBuffer::testCapacityAndSize()
{
    MPSCRingBuffer<int> buffer(16);
    
    // Fill buffer
    for (int i = 0; i < 16; ++i) {
        QVERIFY(buffer.tryPush(i));
        QCOMPARE(buffer.size(), static_cast<size_t>(i + 1));
    }
    
    QVERIFY(buffer.full());
    QCOMPARE(buffer.size(), buffer.capacity());
    
    // Try to produce when full
    QVERIFY(!buffer.tryPush(99));
    QCOMPARE(buffer.size(), buffer.capacity());
    
    // Consume all
    for (int i = 0; i < 16; ++i) {
        int consumed;
        QVERIFY(buffer.tryPop(consumed));
        QCOMPARE(consumed, i);
        QCOMPARE(buffer.size(), static_cast<size_t>(16 - i - 1));
    }
    
    QVERIFY(buffer.empty());
}

void TestMPSCRingBuffer::testMultipleProducersConcurrent()
{
    const size_t bufferSize = 1024;
    const int numProducers = 8;
    const int itemsPerProducer = 1000;
    const int totalItems = numProducers * itemsPerProducer;
    
    MPSCRingBuffer<TestItem> buffer(bufferSize);
    std::atomic<int> totalProduced{0};
    std::atomic<int> totalConsumed{0};
    
    // Start consumer thread
    std::thread consumerThread([&]() {
        runConsumer(buffer, totalItems, totalConsumed);
    });
    
    // Start multiple producer threads
    std::vector<std::thread> producerThreads;
    QElapsedTimer timer;
    timer.start();
    
    for (int p = 0; p < numProducers; ++p) {
        producerThreads.emplace_back([&, p]() {
            int produced = 0;
            for (int i = 0; i < itemsPerProducer; ++i) {
                TestItem item(p * itemsPerProducer + i);
                
                // Retry on full buffer
                while (!buffer.tryPush(item)) {
                    std::this_thread::yield();
                }
                produced++;
            }
            totalProduced.fetch_add(produced);
        });
    }
    
    // Wait for producers
    for (auto& thread : producerThreads) {
        thread.join();
    }
    
    // Wait for consumer
    consumerThread.join();
    
    qint64 elapsedMs = timer.elapsed();
    
    QCOMPARE(totalProduced.load(), totalItems);
    QCOMPARE(totalConsumed.load(), totalItems);
    
    // Performance validation
    double itemsPerSecond = (totalItems * 1000.0) / elapsedMs;
    QVERIFY(itemsPerSecond > 100000.0); // Should handle >100K items/sec
    
    qDebug() << "Multiple producers test:" << itemsPerSecond << "items/second";
}

void TestMPSCRingBuffer::testHighThroughputMultipleProducers()
{
    const size_t bufferSize = 2048;
    const int numProducers = 16;
    const int itemsPerProducer = 10000;
    const int totalItems = numProducers * itemsPerProducer;
    
    MPSCRingBuffer<int> buffer(bufferSize);
    std::atomic<int> totalProduced{0};
    std::atomic<int> totalConsumed{0};
    
    QElapsedTimer timer;
    timer.start();
    
    // High-performance consumer
    std::thread consumerThread([&]() {
        int consumed = 0;
        while (consumed < totalItems) {
            int item;
            while (buffer.tryPop(item)) {
                consumed++;
                totalConsumed.fetch_add(1);
                
                if (consumed >= totalItems) break;
            }
            
            if (consumed < totalItems) {
                std::this_thread::yield();
            }
        }
    });
    
    // High-performance producers
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&, p]() {
            int baseValue = p * itemsPerProducer;
            int produced = 0;
            
            for (int i = 0; i < itemsPerProducer; ++i) {
                int retryCount = 0;
                while (!buffer.tryPush(baseValue + i)) {
                    if (++retryCount > 1000) {
                        std::this_thread::yield();
                        retryCount = 0;
                    }
                }
                produced++;
            }
            totalProduced.fetch_add(produced);
        });
    }
    
    for (auto& thread : producers) {
        thread.join();
    }
    
    consumerThread.join();
    
    qint64 elapsedMs = timer.elapsed();
    
    QCOMPARE(totalProduced.load(), totalItems);
    QCOMPARE(totalConsumed.load(), totalItems);
    
    double throughputMPS = (totalItems / 1000000.0) / (elapsedMs / 1000.0);
    QVERIFY(throughputMPS > 1.0); // Should handle >1M items/second
    
    qDebug() << "High throughput test:" << throughputMPS << "million items/second";
}

void TestMPSCRingBuffer::testLowLatencyOperations()
{
    const size_t bufferSize = 64;
    const int iterations = 10000;
    
    MPSCRingBuffer<TestItem> buffer(bufferSize);
    
    double avgLatencyNs = 0.0;
    measureLatency(buffer, iterations, avgLatencyNs);
    
    // Target: < 1000ns average latency for produce-consume cycle
    QVERIFY(avgLatencyNs < 1000.0);
    
    qDebug() << "Average produce-consume latency:" << avgLatencyNs << "nanoseconds";
}

void TestMPSCRingBuffer::testBackpressureHandling()
{
    const size_t smallBuffer = 32;
    const int numProducers = 8;
    
    MPSCRingBuffer<TestItem> buffer(smallBuffer);
    
    QVERIFY(verifyBackpressureHandling(buffer, numProducers));
}

void TestMPSCRingBuffer::testLongRunningStress()
{
    const size_t bufferSize = 512;
    const int numProducers = 4;
    const int duration_seconds = 5;
    
    MPSCRingBuffer<TestItem> buffer(bufferSize);
    std::atomic<bool> running{true};
    std::atomic<long> totalProduced{0};
    std::atomic<long> totalConsumed{0};
    
    // Consumer thread
    std::thread consumer([&]() {
        TestItem item;
        while (running.load() || !buffer.empty()) {
            if (buffer.tryPop(item)) {
                totalConsumed.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    // Producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&, p]() {
            int counter = 0;
            while (running.load()) {
                TestItem item(p * 1000000 + counter);
                if (buffer.tryPush(item)) {
                    totalProduced.fetch_add(1);
                    counter++;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    running.store(false);
    
    // Wait for threads
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    
    QCOMPARE(totalProduced.load(), totalConsumed.load());
    QVERIFY(totalProduced.load() > 0);
    
    double itemsPerSecond = totalProduced.load() / static_cast<double>(duration_seconds);
    
    qDebug() << "Long running stress test:"
             << "Total items:" << totalProduced.load()
             << "Rate:" << itemsPerSecond << "items/second";
    
    // Should maintain reasonable throughput
    QVERIFY(itemsPerSecond > 50000.0);
}

void TestMPSCRingBuffer::testCacheLineAlignment()
{
    MPSCRingBuffer<int> buffer(64);
    
    // Check that critical atomic variables are cache-line aligned
    auto headPtr = reinterpret_cast<uintptr_t>(&buffer) + offsetof(MPSCRingBuffer<int>, m_head);
    auto tailPtr = reinterpret_cast<uintptr_t>(&buffer) + offsetof(MPSCRingBuffer<int>, m_tail);
    
    const size_t cacheLineSize = 64; // Most common cache line size
    
    // Head and tail should be on different cache lines
    QVERIFY((headPtr / cacheLineSize) != (tailPtr / cacheLineSize));
    
    qDebug() << "Cache line alignment verified:"
             << "Head cache line:" << (headPtr / cacheLineSize)
             << "Tail cache line:" << (tailPtr / cacheLineSize);
}

void TestMPSCRingBuffer::testDataIntegrity()
{
    const size_t bufferSize = 256;
    const int numProducers = 8;
    const int itemsPerProducer = 1000;
    
    MPSCRingBuffer<TestItem> buffer(bufferSize);
    
    std::atomic<int> totalProduced{0};
    std::vector<std::vector<int>> producedValues(numProducers);
    std::vector<TestItem> consumedItems;
    consumedItems.reserve(numProducers * itemsPerProducer);
    
    // Producers with unique value ranges
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < itemsPerProducer; ++i) {
                int value = p * 1000000 + i; // Unique value per producer
                TestItem item(value);
                
                while (!buffer.tryPush(item)) {
                    std::this_thread::yield();
                }
                
                producedValues[p].push_back(value);
                totalProduced.fetch_add(1);
            }
        });
    }
    
    // Consumer
    std::thread consumer([&]() {
        TestItem item;
        int consumed = 0;
        while (consumed < numProducers * itemsPerProducer) {
            if (buffer.tryPop(item)) {
                consumedItems.push_back(item);
                consumed++;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    
    QCOMPARE(totalProduced.load(), numProducers * itemsPerProducer);
    QCOMPARE(static_cast<int>(consumedItems.size()), numProducers * itemsPerProducer);
    
    // Verify all produced values were consumed (order may vary)
    std::set<int> producedSet, consumedSet;
    
    for (const auto& producerValues : producedValues) {
        for (int value : producerValues) {
            producedSet.insert(value);
        }
    }
    
    for (const auto& item : consumedItems) {
        consumedSet.insert(item.value);
    }
    
    QCOMPARE(producedSet.size(), static_cast<size_t>(numProducers * itemsPerProducer));
    QCOMPARE(consumedSet.size(), static_cast<size_t>(numProducers * itemsPerProducer));
    QVERIFY(producedSet == consumedSet);
    
    qDebug() << "Data integrity verified: All" << (numProducers * itemsPerProducer) 
             << "unique values produced and consumed correctly";
}

void TestMPSCRingBuffer::testMemoryStressWithLargeItems()
{
    const size_t bufferSize = 128;
    const int numProducers = 4;
    const int itemsPerProducer = 500;
    
    MPSCRingBuffer<LargeTestItem> buffer(bufferSize);
    
    std::atomic<int> totalProduced{0};
    std::atomic<int> totalConsumed{0};
    
    // Consumer
    std::thread consumer([&]() {
        LargeTestItem item;
        while (totalConsumed.load() < numProducers * itemsPerProducer) {
            if (buffer.tryPop(item)) {
                // Verify item integrity
                LargeTestItem expected(item.data[0]);
                QVERIFY(item == expected);
                totalConsumed.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    // Producers
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < itemsPerProducer; ++i) {
                LargeTestItem item(p * 1000 + i);
                while (!buffer.tryPush(item)) {
                    std::this_thread::yield();
                }
                totalProduced.fetch_add(1);
            }
        });
    }
    
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    
    QCOMPARE(totalProduced.load(), numProducers * itemsPerProducer);
    QCOMPARE(totalConsumed.load(), numProducers * itemsPerProducer);
    
    qDebug() << "Memory stress test with large items completed successfully";
}

// Helper method implementations
void TestMPSCRingBuffer::runConsumer(MPSCRingBuffer<TestItem>& buffer, int expectedItems,
                                   std::atomic<int>& totalConsumed, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    
    TestItem item;
    int consumed = 0;
    
    while (consumed < expectedItems && timer.elapsed() < timeoutMs) {
        if (buffer.tryPop(item)) {
            consumed++;
            totalConsumed.fetch_add(1);
        } else {
            std::this_thread::yield();
        }
    }
}

bool TestMPSCRingBuffer::verifyBackpressureHandling(MPSCRingBuffer<TestItem>& buffer, int numProducers)
{
    const int itemsPerProducer = 100;
    std::atomic<int> totalProduced{0};
    std::atomic<int> backpressureEvents{0};
    
    // Fill buffer first
    while (!buffer.full()) {
        if (!buffer.tryPush(TestItem(0))) {
            break;
        }
    }
    
    // Start producers that will face backpressure
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < itemsPerProducer; ++i) {
                TestItem item(p * 1000 + i);
                int retryCount = 0;
                
                while (!buffer.tryPush(item)) {
                    backpressureEvents.fetch_add(1);
                    std::this_thread::yield();
                    
                    if (++retryCount > 1000) {
                        break; // Avoid infinite loop
                    }
                }
                
                if (retryCount <= 1000) {
                    totalProduced.fetch_add(1);
                }
            }
        });
    }
    
    // Start consumer after a delay to create backpressure
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    std::thread consumer([&]() {
        TestItem item;
        int consumed = 0;
        
        while (consumed < totalProduced.load() + static_cast<int>(buffer.capacity())) {
            if (buffer.tryPop(item)) {
                consumed++;
            } else {
                std::this_thread::yield();
            }
            
            // Slow down consumer slightly to create backpressure
            if (consumed % 10 == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
    });
    
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    
    qDebug() << "Backpressure test - Total produced:" << totalProduced.load()
             << "Backpressure events:" << backpressureEvents.load();
    
    return backpressureEvents.load() > 0; // Should have experienced backpressure
}

void TestMPSCRingBuffer::measureLatency(MPSCRingBuffer<TestItem>& buffer, int iterations, double& avgLatencyNs)
{
    std::vector<double> latencies;
    latencies.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        
        TestItem item(i);
        buffer.tryPush(item); // Assume buffer has space
        
        TestItem consumed;
        buffer.tryPop(consumed);
        
        auto end = std::chrono::high_resolution_clock::now();
        
        double latencyNs = std::chrono::duration<double, std::nano>(end - start).count();
        latencies.push_back(latencyNs);
        
        QCOMPARE(consumed.value, i);
    }
    
    double sum = 0.0;
    for (double latency : latencies) {
        sum += latency;
    }
    avgLatencyNs = sum / latencies.size();
}

QTEST_MAIN(TestMPSCRingBuffer)
#include "test_mpsc_ring_buffer.moc"