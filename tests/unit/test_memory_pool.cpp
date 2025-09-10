#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <QtCore/QObject>
#include "../../src/memory/memory_pool.h"

class TestMemoryPool : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Basic functionality tests
    void testPoolCreation();
    void testAllocation();
    void testDeallocation();
    void testPoolExhaustion();
    void testInvalidPointerDeallocation();
    void testUtilizationCalculation();
    void testPoolReset();
    
    // Thread safety tests
    void testConcurrentAllocation();
    void testConcurrentDeallocation();
    void testConcurrentAllocationDeallocation();
    
    // Performance tests
    void testAllocationPerformance();
    void testDeallocationPerformance();
    
    // Memory pool manager tests
    void testPoolManagerCreation();
    void testPoolManagerAllocation();
    void testPoolManagerUtilization();
    void testPoolManagerCleanup();
    
    // Signal tests
    void testMemoryPressureSignal();
    void testAllocationFailedSignal();

private:
    Monitor::Memory::MemoryPool* m_pool = nullptr;
    Monitor::Memory::MemoryPoolManager* m_manager = nullptr;
    
    static constexpr size_t TEST_BLOCK_SIZE = 64;
    static constexpr size_t TEST_BLOCK_COUNT = 100;
};

void TestMemoryPool::initTestCase()
{
    // This will be called once before the first test function is executed
}

void TestMemoryPool::cleanupTestCase()
{
    // This will be called once after the last test function was executed
}

void TestMemoryPool::init()
{
    // Called before each test function
    m_pool = new Monitor::Memory::MemoryPool(TEST_BLOCK_SIZE, TEST_BLOCK_COUNT);
    m_manager = new Monitor::Memory::MemoryPoolManager();
}

void TestMemoryPool::cleanup()
{
    // Called after each test function
    delete m_pool;
    delete m_manager;
    m_pool = nullptr;
    m_manager = nullptr;
}

void TestMemoryPool::testPoolCreation()
{
    QVERIFY(m_pool != nullptr);
    QCOMPARE(m_pool->getBlockSize(), TEST_BLOCK_SIZE);
    QCOMPARE(m_pool->getBlockCount(), TEST_BLOCK_COUNT);
    QCOMPARE(m_pool->getUsedBlocks(), size_t(0));
    QCOMPARE(m_pool->getAvailableBlocks(), TEST_BLOCK_COUNT);
    QCOMPARE(m_pool->getUtilization(), 0.0);
}

void TestMemoryPool::testAllocation()
{
    void* ptr1 = m_pool->allocate();
    QVERIFY(ptr1 != nullptr);
    QCOMPARE(m_pool->getUsedBlocks(), size_t(1));
    QCOMPARE(m_pool->getAvailableBlocks(), TEST_BLOCK_COUNT - 1);
    
    void* ptr2 = m_pool->allocate();
    QVERIFY(ptr2 != nullptr);
    QVERIFY(ptr1 != ptr2);
    QCOMPARE(m_pool->getUsedBlocks(), size_t(2));
    
    // Verify memory is zeroed
    char* charPtr = static_cast<char*>(ptr1);
    for (size_t i = 0; i < TEST_BLOCK_SIZE; ++i) {
        QCOMPARE(charPtr[i], char(0));
    }
    
    m_pool->deallocate(ptr1);
    m_pool->deallocate(ptr2);
}

void TestMemoryPool::testDeallocation()
{
    void* ptr = m_pool->allocate();
    QVERIFY(ptr != nullptr);
    QCOMPARE(m_pool->getUsedBlocks(), size_t(1));
    
    m_pool->deallocate(ptr);
    QCOMPARE(m_pool->getUsedBlocks(), size_t(0));
    QCOMPARE(m_pool->getAvailableBlocks(), TEST_BLOCK_COUNT);
    
    // Should be able to deallocate nullptr without crashing
    m_pool->deallocate(nullptr);
    QCOMPARE(m_pool->getUsedBlocks(), size_t(0));
}

void TestMemoryPool::testPoolExhaustion()
{
    std::vector<void*> allocations;
    
    // Allocate all blocks
    for (size_t i = 0; i < TEST_BLOCK_COUNT; ++i) {
        void* ptr = m_pool->allocate();
        QVERIFY(ptr != nullptr);
        allocations.push_back(ptr);
    }
    
    QCOMPARE(m_pool->getUsedBlocks(), TEST_BLOCK_COUNT);
    QCOMPARE(m_pool->getAvailableBlocks(), size_t(0));
    
    // Next allocation should fail
    void* ptr = m_pool->allocate();
    QVERIFY(ptr == nullptr);
    
    // Clean up
    for (void* p : allocations) {
        m_pool->deallocate(p);
    }
}

void TestMemoryPool::testInvalidPointerDeallocation()
{
    char invalidPtr[TEST_BLOCK_SIZE];
    
    // Should not crash when deallocating invalid pointer
    m_pool->deallocate(invalidPtr);
    QCOMPARE(m_pool->getUsedBlocks(), size_t(0));
    
    // Test isValidPointer function
    void* validPtr = m_pool->allocate();
    QVERIFY(m_pool->isValidPointer(validPtr));
    QVERIFY(!m_pool->isValidPointer(invalidPtr));
    QVERIFY(!m_pool->isValidPointer(nullptr));
    
    m_pool->deallocate(validPtr);
}

void TestMemoryPool::testUtilizationCalculation()
{
    QCOMPARE(m_pool->getUtilization(), 0.0);
    
    void* ptr1 = m_pool->allocate();
    double expectedUtilization = 1.0 / TEST_BLOCK_COUNT;
    QCOMPARE(m_pool->getUtilization(), expectedUtilization);
    
    void* ptr2 = m_pool->allocate();
    expectedUtilization = 2.0 / TEST_BLOCK_COUNT;
    QCOMPARE(m_pool->getUtilization(), expectedUtilization);
    
    m_pool->deallocate(ptr1);
    expectedUtilization = 1.0 / TEST_BLOCK_COUNT;
    QCOMPARE(m_pool->getUtilization(), expectedUtilization);
    
    m_pool->deallocate(ptr2);
    QCOMPARE(m_pool->getUtilization(), 0.0);
}

void TestMemoryPool::testPoolReset()
{
    // Allocate some blocks
    void* ptr1 = m_pool->allocate();
    void* ptr2 = m_pool->allocate();
    QVERIFY(ptr1 != nullptr);
    QVERIFY(ptr2 != nullptr);
    QCOMPARE(m_pool->getUsedBlocks(), size_t(2));
    
    // Reset pool
    m_pool->reset();
    QCOMPARE(m_pool->getUsedBlocks(), size_t(0));
    QCOMPARE(m_pool->getAvailableBlocks(), TEST_BLOCK_COUNT);
    QCOMPARE(m_pool->getUtilization(), 0.0);
    
    // Should be able to allocate again
    void* ptr3 = m_pool->allocate();
    QVERIFY(ptr3 != nullptr);
    m_pool->deallocate(ptr3);
}

void TestMemoryPool::testConcurrentAllocation()
{
    const int numThreads = 4;
    std::vector<std::vector<void*>> threadAllocations(numThreads);
    
    QVector<QThread*> threads;
    QVector<bool> results(numThreads, true);
    
    for (int i = 0; i < numThreads; ++i) {
        QThread* thread = QThread::create([this, i, &threadAllocations, &results]() {
            int allocationsPerThread = TEST_BLOCK_COUNT / 4; // Recalculate to avoid capture
            for (int j = 0; j < allocationsPerThread; ++j) {
                void* ptr = m_pool->allocate();
                if (ptr == nullptr) {
                    results[i] = false;
                    return;
                }
                threadAllocations[i].push_back(ptr);
                
                // Small delay to increase chance of race conditions
                QThread::usleep(1);
            }
        });
        threads.append(thread);
    }
    
    // Start all threads
    for (QThread* thread : threads) {
        thread->start();
    }
    
    // Wait for all threads to complete
    for (QThread* thread : threads) {
        QVERIFY(thread->wait(5000)); // 5 second timeout
        delete thread;
    }
    
    // Verify all allocations succeeded
    for (bool result : results) {
        QVERIFY(result);
    }
    
    // Verify total allocations
    size_t totalAllocated = 0;
    for (const auto& allocations : threadAllocations) {
        totalAllocated += allocations.size();
    }
    
    QCOMPARE(m_pool->getUsedBlocks(), totalAllocated);
    
    // Clean up
    for (const auto& allocations : threadAllocations) {
        for (void* ptr : allocations) {
            m_pool->deallocate(ptr);
        }
    }
}

void TestMemoryPool::testConcurrentDeallocation()
{
    // Pre-allocate blocks
    std::vector<void*> allocations;
    for (size_t i = 0; i < TEST_BLOCK_COUNT; ++i) {
        void* ptr = m_pool->allocate();
        QVERIFY(ptr != nullptr);
        allocations.push_back(ptr);
    }
    
    const int numThreads = 4;
    const size_t blocksPerThread = allocations.size() / numThreads;
    
    QVector<QThread*> threads;
    
    for (int i = 0; i < numThreads; ++i) {
        QThread* thread = QThread::create([this, i, blocksPerThread, &allocations]() {
            size_t start = i * blocksPerThread;
            size_t end = (i == 3) ? allocations.size() : (i + 1) * blocksPerThread; // Handle remainder
            
            for (size_t j = start; j < end; ++j) {
                m_pool->deallocate(allocations[j]);
                QThread::usleep(1);
            }
        });
        threads.append(thread);
    }
    
    // Start all threads
    for (QThread* thread : threads) {
        thread->start();
    }
    
    // Wait for all threads to complete
    for (QThread* thread : threads) {
        QVERIFY(thread->wait(5000));
        delete thread;
    }
    
    // Verify all blocks are deallocated
    QCOMPARE(m_pool->getUsedBlocks(), size_t(0));
}

void TestMemoryPool::testConcurrentAllocationDeallocation()
{
    
    QVector<QThread*> threads;
    
    // Allocator thread
    QThread* allocatorThread = QThread::create([this]() {
        const int numIterations = 1000; // Local constant
        std::vector<void*> allocated;
        for (int i = 0; i < numIterations; ++i) {
            void* ptr = m_pool->allocate();
            if (ptr) {
                allocated.push_back(ptr);
            }
            
            // Occasionally deallocate some blocks
            if (allocated.size() > 10 && (i % 10) == 0) {
                for (int j = 0; j < 5 && !allocated.empty(); ++j) {
                    m_pool->deallocate(allocated.back());
                    allocated.pop_back();
                }
            }
        }
        
        // Clean up remaining allocations
        for (void* ptr : allocated) {
            m_pool->deallocate(ptr);
        }
    });
    
    // Deallocator thread (allocates then immediately deallocates)
    QThread* deallocatorThread = QThread::create([this]() {
        const int numIterations = 1000; // Local constant
        for (int i = 0; i < numIterations; ++i) {
            void* ptr = m_pool->allocate();
            if (ptr) {
                m_pool->deallocate(ptr);
            }
        }
    });
    
    threads.append(allocatorThread);
    threads.append(deallocatorThread);
    
    // Start threads
    for (QThread* thread : threads) {
        thread->start();
    }
    
    // Wait for completion
    for (QThread* thread : threads) {
        QVERIFY(thread->wait(10000));
        delete thread;
    }
    
    // Both threads completed successfully
}

void TestMemoryPool::testAllocationPerformance()
{
    const int numAllocations = 10000;
    std::vector<void*> allocations;
    allocations.reserve(numAllocations);
    
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < numAllocations; ++i) {
        void* ptr = m_pool->allocate();
        if (ptr) {
            allocations.push_back(ptr);
        }
        if (allocations.size() >= TEST_BLOCK_COUNT) {
            break; // Pool exhausted
        }
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerAllocation = static_cast<double>(elapsedNs) / allocations.size();
    
    qDebug() << "Allocation performance:" << nsPerAllocation << "ns per allocation";
    
    // Should be very fast - less than 1000ns (1μs) per allocation
    QVERIFY(nsPerAllocation < 1000.0);
    
    // Clean up
    for (void* ptr : allocations) {
        m_pool->deallocate(ptr);
    }
}

void TestMemoryPool::testDeallocationPerformance()
{
    const size_t numBlocks = std::min(size_t(10000), TEST_BLOCK_COUNT);
    std::vector<void*> allocations;
    
    // Pre-allocate
    for (size_t i = 0; i < numBlocks; ++i) {
        void* ptr = m_pool->allocate();
        QVERIFY(ptr != nullptr);
        allocations.push_back(ptr);
    }
    
    QElapsedTimer timer;
    timer.start();
    
    for (void* ptr : allocations) {
        m_pool->deallocate(ptr);
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerDeallocation = static_cast<double>(elapsedNs) / allocations.size();
    
    qDebug() << "Deallocation performance:" << nsPerDeallocation << "ns per deallocation";
    
    // Should be very fast - less than 500ns per deallocation
    QVERIFY(nsPerDeallocation < 500.0);
}

void TestMemoryPool::testPoolManagerCreation()
{
    QVERIFY(m_manager != nullptr);
    
    auto pool = m_manager->createPool("TestPool", 128, 50);
    QVERIFY(pool != nullptr);
    QCOMPARE(pool->getBlockSize(), size_t(128));
    QCOMPARE(pool->getBlockCount(), size_t(50));
    
    // Test duplicate name handling
    auto duplicatePool = m_manager->createPool("TestPool", 256, 100);
    QVERIFY(duplicatePool == pool); // Should return existing pool
    
    // Clean up
    m_manager->removePool("TestPool");
}

void TestMemoryPool::testPoolManagerAllocation()
{
    m_manager->createPool("TestAlloc", 64, 10);
    
    void* ptr1 = m_manager->allocate("TestAlloc");
    QVERIFY(ptr1 != nullptr);
    
    void* ptr2 = m_manager->allocate("NonExistent");
    QVERIFY(ptr2 == nullptr);
    
    m_manager->deallocate("TestAlloc", ptr1);
    m_manager->deallocate("NonExistent", ptr2); // Should not crash
    
    m_manager->removePool("TestAlloc");
}

void TestMemoryPool::testPoolManagerUtilization()
{
    m_manager->createPool("Pool1", 64, 100);
    m_manager->createPool("Pool2", 128, 50);
    
    QCOMPARE(m_manager->getTotalUtilization(), 0.0);
    
    void* ptr1 = m_manager->allocate("Pool1");
    void* ptr2 = m_manager->allocate("Pool2");
    
    double expectedUtilization = 2.0 / (100 + 50); // 2 blocks used out of 150 total
    QCOMPARE(m_manager->getTotalUtilization(), expectedUtilization);
    
    m_manager->deallocate("Pool1", ptr1);
    m_manager->deallocate("Pool2", ptr2);
    
    QCOMPARE(m_manager->getTotalUtilization(), 0.0);
    
    m_manager->removePool("Pool1");
    m_manager->removePool("Pool2");
}

void TestMemoryPool::testPoolManagerCleanup()
{
    m_manager->createPool("TempPool1", 64, 10);
    m_manager->createPool("TempPool2", 128, 10);
    
    QStringList poolNames = m_manager->getPoolNames();
    QVERIFY(poolNames.contains("TempPool1"));
    QVERIFY(poolNames.contains("TempPool2"));
    QCOMPARE(poolNames.size(), 2);
    
    m_manager->removePool("TempPool1");
    poolNames = m_manager->getPoolNames();
    QVERIFY(!poolNames.contains("TempPool1"));
    QVERIFY(poolNames.contains("TempPool2"));
    QCOMPARE(poolNames.size(), 1);
    
    m_manager->removePool("TempPool2");
    poolNames = m_manager->getPoolNames();
    QVERIFY(poolNames.isEmpty());
}

void TestMemoryPool::testMemoryPressureSignal()
{
    QSignalSpy spy(m_pool, &Monitor::Memory::MemoryPool::memoryPressure);
    
    // Allocate until we hit the pressure threshold (80%)
    std::vector<void*> allocations;
    size_t pressureThreshold = static_cast<size_t>(TEST_BLOCK_COUNT * 0.8);
    
    for (size_t i = 0; i <= pressureThreshold; ++i) {
        void* ptr = m_pool->allocate();
        if (ptr) {
            allocations.push_back(ptr);
        }
    }
    
    // Should have received at least one pressure signal
    QVERIFY(spy.count() >= 1);
    
    // Clean up
    for (void* ptr : allocations) {
        m_pool->deallocate(ptr);
    }
}

void TestMemoryPool::testAllocationFailedSignal()
{
    QSignalSpy spy(m_pool, &Monitor::Memory::MemoryPool::allocationFailed);
    
    // Allocate all blocks
    std::vector<void*> allocations;
    for (size_t i = 0; i < TEST_BLOCK_COUNT; ++i) {
        void* ptr = m_pool->allocate();
        QVERIFY(ptr != nullptr);
        allocations.push_back(ptr);
    }
    
    // Next allocation should fail and emit signal
    void* failPtr = m_pool->allocate();
    QVERIFY(failPtr == nullptr);
    QCOMPARE(spy.count(), 1);
    
    // Clean up
    for (void* ptr : allocations) {
        m_pool->deallocate(ptr);
    }
}

QTEST_MAIN(TestMemoryPool)
#include "test_memory_pool.moc"