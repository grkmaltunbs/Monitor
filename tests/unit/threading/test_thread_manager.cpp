#include <QtTest/QtTest>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <memory>

#include "threading/thread_manager.h"

using namespace Monitor::Threading;

class TestThreadManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testSingletonAccess();
    void testInitializationShutdown();
    void testPoolCreation();
    void testPoolRetrieval();
    void testPoolRemoval();
    
    // Configuration tests
    void testDefaultConfiguration();
    void testCustomConfiguration();
    void testConfigurationValidation();
    
    // Resource management tests
    void testSystemResourceTracking();
    void testMemoryPressureHandling();
    void testCpuLoadMonitoring();
    void testThreadLimitEnforcement();
    
    // Performance tests
    void testHighPerformanceTaskDistribution();
    void testLoadBalancingAcrossPools();
    void testResourceOptimization();
    void testScalabilityUnderLoad();
    
    // Monitoring and statistics tests
    void testGlobalStatistics();
    void testPerPoolStatistics();
    void testPerformanceMetrics();
    void testResourceUtilization();
    
    // Error handling tests
    void testInvalidPoolNames();
    void testResourceExhaustionHandling();
    void testPoolFailureRecovery();
    void testConcurrentAccessSafety();
    
    // Stress tests
    void testMassivePoolCreation();
    void testConcurrentPoolOperations();
    void testLongRunningOperations();
    void testSystemLimitStress();
    
    // Integration tests
    void testMultiplePoolCoordination();
    void testTaskMigrationBetweenPools();
    void testSystemResourceSharing();

private:
    // Helper methods
    void waitForStatisticsUpdate(int timeoutMs = 1000);
    bool verifyResourceConstraints();
    void simulateSystemLoad();
    void cleanupAllPools();
};

void TestThreadManager::initTestCase()
{
    // Initialize test environment
    qDebug() << "Starting ThreadManager comprehensive tests";
}

void TestThreadManager::cleanupTestCase()
{
    // Cleanup test environment
    auto& manager = ThreadManager::instance();
    manager.shutdown();
    qDebug() << "ThreadManager tests completed";
}

void TestThreadManager::init()
{
    // Clean state for each test
    cleanupAllPools();
}

void TestThreadManager::cleanup()
{
    // Cleanup after each test
    cleanupAllPools();
}

void TestThreadManager::testSingletonAccess()
{
    // Test singleton pattern
    auto& manager1 = ThreadManager::instance();
    auto& manager2 = ThreadManager::instance();
    
    QVERIFY(&manager1 == &manager2);
    
    // Test thread safety of singleton access
    std::atomic<ThreadManager*> managerPtr{nullptr};
    std::vector<std::thread> threads;
    
    const int numThreads = 10;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&managerPtr]() {
            auto* manager = &ThreadManager::instance();
            ThreadManager* expected = nullptr;
            
            // Either set it or verify it's the same
            if (!managerPtr.compare_exchange_strong(expected, manager)) {
                // Already set, verify it's the same instance
                QVERIFY(managerPtr.load() == manager);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    QVERIFY(managerPtr.load() != nullptr);
    QVERIFY(managerPtr.load() == &manager1);
}

void TestThreadManager::testInitializationShutdown()
{
    auto& manager = ThreadManager::instance();
    
    // Test initialization
    QVERIFY(manager.initialize());
    QVERIFY(manager.isInitialized());
    
    // Test double initialization (should be safe)
    QVERIFY(manager.initialize());
    QVERIFY(manager.isInitialized());
    
    // Test shutdown
    manager.shutdown();
    QVERIFY(!manager.isInitialized());
    
    // Test double shutdown (should be safe)
    manager.shutdown();
    QVERIFY(!manager.isInitialized());
    
    // Test re-initialization
    QVERIFY(manager.initialize());
    QVERIFY(manager.isInitialized());
}

void TestThreadManager::testPoolCreation()
{
    auto& manager = ThreadManager::instance();
    manager.initialize();
    
    // Test basic pool creation
    auto pool1 = manager.createPool("TestPool1");
    QVERIFY(pool1 != nullptr);
    QCOMPARE(pool1->getName(), QString("TestPool1"));
    
    // Test pool creation with configuration
    ThreadPoolConfig config;
    config.minThreads = 2;
    config.maxThreads = 8;
    config.policy = SchedulingPolicy::WorkStealing;
    
    auto pool2 = manager.createPool("TestPool2", config);
    QVERIFY(pool2 != nullptr);
    QCOMPARE(pool2->getName(), QString("TestPool2"));
    QCOMPARE(pool2->getMinThreads(), 2);
    QCOMPARE(pool2->getMaxThreads(), 8);
    QVERIFY(pool2->getSchedulingPolicy() == SchedulingPolicy::WorkStealing);
    
    // Test duplicate pool name (should return existing)
    auto pool1_duplicate = manager.createPool("TestPool1");
    QVERIFY(pool1_duplicate == pool1);
    
    // Test pool count
    auto poolNames = manager.getPoolNames();
    QVERIFY(poolNames.contains("TestPool1"));
    QVERIFY(poolNames.contains("TestPool2"));
    QCOMPARE(poolNames.size(), 2);
}

void TestThreadManager::testPoolRetrieval()
{
    auto& manager = ThreadManager::instance();
    manager.initialize();
    
    // Create test pools
    auto pool1 = manager.createPool("RetrievalTest1");
    auto pool2 = manager.createPool("RetrievalTest2");
    
    // Test pool retrieval
    auto retrieved1 = manager.getPool("RetrievalTest1");
    auto retrieved2 = manager.getPool("RetrievalTest2");
    
    QVERIFY(retrieved1 == pool1);
    QVERIFY(retrieved2 == pool2);
    
    // Test non-existent pool
    auto nonExistent = manager.getPool("NonExistent");
    QVERIFY(nonExistent == nullptr);
    
    // Test case sensitivity
    auto caseTest = manager.getPool("retrievaltest1");
    QVERIFY(caseTest == nullptr); // Should be case sensitive
}

void TestThreadManager::testPoolRemoval()
{
    auto& manager = ThreadManager::instance();
    manager.initialize();
    
    // Create test pool
    auto pool = manager.createPool("RemovalTest");
    QVERIFY(pool != nullptr);
    QVERIFY(manager.getPool("RemovalTest") != nullptr);
    
    // Test pool removal
    QVERIFY(manager.removePool("RemovalTest"));
    QVERIFY(manager.getPool("RemovalTest") == nullptr);
    
    // Test removing non-existent pool
    QVERIFY(!manager.removePool("NonExistent"));
    
    // Verify pool names updated
    auto poolNames = manager.getPoolNames();
    QVERIFY(!poolNames.contains("RemovalTest"));
}

void TestThreadManager::testSystemResourceTracking()
{
    auto& manager = ThreadManager::instance();
    manager.initialize();
    
    // Get initial system info
    auto systemInfo = manager.getSystemInfo();
    
    QVERIFY(systemInfo.totalCpuCores > 0);
    QVERIFY(systemInfo.availableCpuCores > 0);
    QVERIFY(systemInfo.totalMemoryMB > 0);
    QVERIFY(systemInfo.availableMemoryMB > 0);
    QVERIFY(systemInfo.availableMemoryMB <= systemInfo.totalMemoryMB);
    
    // Create pools and verify resource tracking
    const int numPools = 4;
    std::vector<std::shared_ptr<ThreadPool>> pools;
    
    for (int i = 0; i < numPools; ++i) {
        ThreadPoolConfig config;
        config.minThreads = 2;
        config.maxThreads = 4;
        
        auto pool = manager.createPool(QString("ResourceTest%1").arg(i), config);
        QVERIFY(pool != nullptr);
        pool->start();
        pools.push_back(pool);
    }
    
    waitForStatisticsUpdate();
    
    // Verify resource tracking reflects created pools
    auto updatedSystemInfo = manager.getSystemInfo();
    QVERIFY(updatedSystemInfo.totalManagedThreads >= numPools * 2); // At least min threads
    
    qDebug() << "System info - CPU cores:" << systemInfo.totalCpuCores
             << "Memory:" << systemInfo.totalMemoryMB << "MB"
             << "Managed threads:" << updatedSystemInfo.totalManagedThreads;
}

void TestThreadManager::testGlobalStatistics()
{
    auto& manager = ThreadManager::instance();
    manager.initialize();
    
    // Create multiple pools
    auto pool1 = manager.createPool("StatsTest1");
    auto pool2 = manager.createPool("StatsTest2");
    
    pool1->start();
    pool2->start();
    
    // Submit tasks to generate statistics
    const int tasksPerPool = 50;
    std::atomic<int> totalCompleted{0};
    
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < tasksPerPool; ++i) {
        auto future1 = pool1->submit([&totalCompleted]() {
            totalCompleted.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
        
        auto future2 = pool2->submit([&totalCompleted]() {
            totalCompleted.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
        
        futures.push_back(std::move(future1));
        futures.push_back(std::move(future2));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    QCOMPARE(totalCompleted.load(), tasksPerPool * 2);
    
    waitForStatisticsUpdate();
    
    // Verify global statistics
    auto globalStats = manager.getGlobalStatistics();
    
    QVERIFY(globalStats.totalPools >= 2);
    QVERIFY(globalStats.totalTasks >= tasksPerPool * 2);
    QVERIFY(globalStats.completedTasks >= tasksPerPool * 2);
    QVERIFY(globalStats.totalExecutionTimeUs > 0);
    
    qDebug() << "Global stats - Pools:" << globalStats.totalPools
             << "Tasks:" << globalStats.totalTasks
             << "Completed:" << globalStats.completedTasks;
}

void TestThreadManager::testHighPerformanceTaskDistribution()
{
    auto& manager = ThreadManager::instance();
    manager.initialize();
    
    // Create specialized pools for different task types
    ThreadPoolConfig highPriorityConfig;
    highPriorityConfig.minThreads = 4;
    highPriorityConfig.maxThreads = 8;
    highPriorityConfig.policy = SchedulingPolicy::LeastLoaded;
    
    ThreadPoolConfig backgroundConfig;
    backgroundConfig.minThreads = 2;
    backgroundConfig.maxThreads = 4;
    backgroundConfig.policy = SchedulingPolicy::RoundRobin;
    
    auto highPriorityPool = manager.createPool("HighPriority", highPriorityConfig);
    auto backgroundPool = manager.createPool("Background", backgroundConfig);
    
    highPriorityPool->start();
    backgroundPool->start();
    
    const int highPriorityTasks = 1000;
    const int backgroundTasks = 2000;
    
    std::atomic<int> highPriorityCompleted{0};
    std::atomic<int> backgroundCompleted{0};
    
    QElapsedTimer timer;
    timer.start();
    
    // Submit high priority tasks
    std::vector<std::future<void>> highPriorityFutures;
    for (int i = 0; i < highPriorityTasks; ++i) {
        auto future = highPriorityPool->submit([&highPriorityCompleted]() {
            // Simulate high priority work
            highPriorityCompleted.fetch_add(1);
        });
        highPriorityFutures.push_back(std::move(future));
    }
    
    // Submit background tasks
    std::vector<std::future<void>> backgroundFutures;
    for (int i = 0; i < backgroundTasks; ++i) {
        auto future = backgroundPool->submit([&backgroundCompleted]() {
            // Simulate background work
            backgroundCompleted.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        });
        backgroundFutures.push_back(std::move(future));
    }
    
    // Wait for all tasks
    for (auto& future : highPriorityFutures) {
        future.wait();
    }
    for (auto& future : backgroundFutures) {
        future.wait();
    }
    
    qint64 elapsedMs = timer.elapsed();
    
    QCOMPARE(highPriorityCompleted.load(), highPriorityTasks);
    QCOMPARE(backgroundCompleted.load(), backgroundTasks);
    
    // Verify performance meets requirements
    double totalTasksPerSecond = ((highPriorityTasks + backgroundTasks) * 1000.0) / elapsedMs;
    QVERIFY(totalTasksPerSecond > 10000.0); // Target: 10K+ tasks/second
    
    qDebug() << "Task distribution performance:" << totalTasksPerSecond << "tasks/second";
}

void TestThreadManager::testConcurrentPoolOperations()
{
    auto& manager = ThreadManager::instance();
    manager.initialize();
    
    const int numThreads = 10;
    const int operationsPerThread = 20;
    
    std::atomic<int> createdPools{0};
    std::atomic<int> retrievedPools{0};
    std::atomic<int> removedPools{0};
    
    std::vector<std::thread> workers;
    
    // Concurrent pool operations
    for (int t = 0; t < numThreads; ++t) {
        workers.emplace_back([&, t]() {
            for (int op = 0; op < operationsPerThread; ++op) {
                QString poolName = QString("ConcurrentTest_%1_%2").arg(t).arg(op);
                
                // Create pool
                auto pool = manager.createPool(poolName);
                if (pool != nullptr) {
                    createdPools.fetch_add(1);
                    pool->start();
                    
                    // Submit a quick task
                    auto future = pool->submit([]() {
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    });
                    future.wait();
                    
                    // Retrieve pool
                    auto retrieved = manager.getPool(poolName);
                    if (retrieved == pool) {
                        retrievedPools.fetch_add(1);
                    }
                    
                    // Remove pool
                    if (manager.removePool(poolName)) {
                        removedPools.fetch_add(1);
                    }
                }
            }
        });
    }
    
    for (auto& worker : workers) {
        worker.join();
    }
    
    const int expectedOperations = numThreads * operationsPerThread;
    
    QCOMPARE(createdPools.load(), expectedOperations);
    QCOMPARE(retrievedPools.load(), expectedOperations);
    QCOMPARE(removedPools.load(), expectedOperations);
    
    // Verify no pools remain
    auto remainingPools = manager.getPoolNames();
    for (const auto& poolName : remainingPools) {
        if (poolName.startsWith("ConcurrentTest_")) {
            QFAIL(QString("Pool %1 was not properly removed").arg(poolName).toLatin1());
        }
    }
}

void TestThreadManager::testResourceOptimization()
{
    auto& manager = ThreadManager::instance();
    manager.initialize();
    
    // Test CPU affinity optimization
    ThreadPoolConfig affinityConfig;
    affinityConfig.enableCpuAffinity = true;
    affinityConfig.minThreads = 4;
    affinityConfig.maxThreads = 4;
    
    auto affinityPool = manager.createPool("AffinityTest", affinityConfig);
    QVERIFY(affinityPool != nullptr);
    affinityPool->start();
    
    // Submit CPU-intensive tasks
    const int cpuIntensiveTasks = 100;
    std::atomic<int> completedTasks{0};
    
    QElapsedTimer timer;
    timer.start();
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < cpuIntensiveTasks; ++i) {
        auto future = affinityPool->submit([&completedTasks]() {
            // Simulate CPU-intensive work
            volatile int sum = 0;
            for (int j = 0; j < 10000; ++j) {
                sum += j;
            }
            completedTasks.fetch_add(1);
        });
        futures.push_back(std::move(future));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    qint64 elapsedMs = timer.elapsed();
    
    QCOMPARE(completedTasks.load(), cpuIntensiveTasks);
    
    // With CPU affinity, should have better cache locality and performance
    double tasksPerSecond = (cpuIntensiveTasks * 1000.0) / elapsedMs;
    QVERIFY(tasksPerSecond > 100.0); // Reasonable performance expectation
    
    qDebug() << "CPU affinity optimization:" << tasksPerSecond << "tasks/second";
}

void TestThreadManager::testMemoryPressureHandling()
{
    auto& manager = ThreadManager::instance();
    manager.initialize();
    
    // Create pools that will use significant memory
    ThreadPoolConfig memoryIntensiveConfig;
    memoryIntensiveConfig.minThreads = 8;
    memoryIntensiveConfig.maxThreads = 16;
    
    auto memoryPool = manager.createPool("MemoryTest", memoryIntensiveConfig);
    memoryPool->start();
    
    // Get initial memory info
    auto initialSystemInfo = manager.getSystemInfo();
    
    // Submit memory-intensive tasks
    const int memoryTasks = 50;
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < memoryTasks; ++i) {
        auto future = memoryPool->submit([]() {
            // Allocate and use some memory
            std::vector<int> largeVector(100000, i);
            volatile int sum = 0;
            for (int val : largeVector) {
                sum += val;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
        futures.push_back(std::move(future));
    }
    
    // Monitor memory during execution
    waitForStatisticsUpdate();
    auto duringSystemInfo = manager.getSystemInfo();
    
    for (auto& future : futures) {
        future.wait();
    }
    
    // Check memory usage patterns
    QVERIFY(initialSystemInfo.availableMemoryMB > 0);
    QVERIFY(duringSystemInfo.availableMemoryMB > 0);
    
    // Memory should be reasonable (not exhausted)
    double memoryUsagePercent = 100.0 * (initialSystemInfo.totalMemoryMB - duringSystemInfo.availableMemoryMB) / initialSystemInfo.totalMemoryMB;
    
    qDebug() << "Memory usage during test:" << memoryUsagePercent << "%"
             << "Available:" << duringSystemInfo.availableMemoryMB << "MB";
    
    // Should not exhaust system memory
    QVERIFY(memoryUsagePercent < 90.0);
}

void TestThreadManager::testSystemLimitStress()
{
    auto& manager = ThreadManager::instance();
    manager.initialize();
    
    // Try to create many pools to test system limits
    const int maxAttempts = 50;
    std::vector<std::shared_ptr<ThreadPool>> pools;
    
    int successfulCreations = 0;
    
    for (int i = 0; i < maxAttempts; ++i) {
        ThreadPoolConfig config;
        config.minThreads = 1;
        config.maxThreads = 2;
        
        QString poolName = QString("StressTest_%1").arg(i);
        auto pool = manager.createPool(poolName, config);
        
        if (pool != nullptr) {
            pools.push_back(pool);
            pool->start();
            successfulCreations++;
            
            // Submit a task to verify pool is working
            auto future = pool->submit([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            });
            future.wait();
        } else {
            // Hit a system limit, which is expected
            break;
        }
    }
    
    QVERIFY(successfulCreations > 0);
    qDebug() << "Successfully created" << successfulCreations << "pools under stress test";
    
    // Verify system is still responsive
    auto systemInfo = manager.getSystemInfo();
    QVERIFY(systemInfo.totalManagedThreads >= successfulCreations);
    
    // Clean up
    for (auto& pool : pools) {
        manager.removePool(pool->getName());
    }
}

void TestThreadManager::testConfigurationValidation()
{
    auto& manager = ThreadManager::instance();
    manager.initialize();
    
    // Test invalid configurations
    ThreadPoolConfig invalidConfig1;
    invalidConfig1.minThreads = 10;
    invalidConfig1.maxThreads = 5; // max < min
    
    auto pool1 = manager.createPool("InvalidTest1", invalidConfig1);
    // Should either fix the config or return nullptr
    if (pool1 != nullptr) {
        // Config should be corrected
        QVERIFY(pool1->getMaxThreads() >= pool1->getMinThreads());
    }
    
    ThreadPoolConfig invalidConfig2;
    invalidConfig2.minThreads = 0; // Invalid minimum
    
    auto pool2 = manager.createPool("InvalidTest2", invalidConfig2);
    if (pool2 != nullptr) {
        // Should have corrected to valid minimum
        QVERIFY(pool2->getMinThreads() > 0);
    }
    
    // Test valid configuration
    ThreadPoolConfig validConfig;
    validConfig.minThreads = 2;
    validConfig.maxThreads = 8;
    validConfig.policy = SchedulingPolicy::WorkStealing;
    validConfig.enableCpuAffinity = true;
    
    auto pool3 = manager.createPool("ValidTest", validConfig);
    QVERIFY(pool3 != nullptr);
    QCOMPARE(pool3->getMinThreads(), 2);
    QCOMPARE(pool3->getMaxThreads(), 8);
}

// Helper methods implementation
void TestThreadManager::waitForStatisticsUpdate(int timeoutMs)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
}

void TestThreadManager::cleanupAllPools()
{
    auto& manager = ThreadManager::instance();
    if (manager.isInitialized()) {
        auto poolNames = manager.getPoolNames();
        for (const auto& poolName : poolNames) {
            manager.removePool(poolName);
        }
    }
}

QTEST_MAIN(TestThreadManager)
#include "test_thread_manager.moc"