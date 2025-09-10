#include <QtTest/QtTest>
#include <QtCore/QCoreApplication>
#include <QtCore/QThread>
#include <QtCore/QElapsedTimer>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <memory>

#include "threading/thread_pool.h"

using namespace Monitor::Threading;

class TestThreadPool : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testConstruction();
    void testStartStop();
    void testBasicTaskSubmission();
    void testTaskExecution();
    void testTaskReturnValue();
    
    // Scheduling policy tests
    void testRoundRobinScheduling();
    void testLeastLoadedScheduling();
    void testRandomScheduling();
    void testWorkStealingScheduling();
    
    // Performance tests
    void testHighThroughputTasks();
    void testLowLatencySubmission();
    void testConcurrentSubmission();
    void testLoadBalancing();
    
    // Resource management tests
    void testThreadGrowthShrinking();
    void testMemoryManagement();
    void testTaskQueueManagement();
    
    // Error handling tests
    void testTaskException();
    void testInvalidParameters();
    void testResourceExhaustion();
    void testThreadFailure();
    
    // Stress tests
    void testMassiveTaskSubmission();
    void testLongRunningTasks();
    void testMixedWorkloadStress();
    void testShutdownWithPendingTasks();
    
    // Statistics and monitoring tests
    void testStatisticsAccuracy();
    void testPerformanceMetrics();

private:
    std::unique_ptr<ThreadPool> m_threadPool;
    
    // Helper methods
    void waitForTaskCompletion(int expectedTasks, int timeoutMs = 5000);
    bool verifyLoadBalancing(const std::vector<std::atomic<int>>& counters, double tolerance = 0.2);
};

void TestThreadPool::initTestCase()
{
    // Initialize test environment
}

void TestThreadPool::cleanupTestCase()
{
    // Cleanup test environment
}

void TestThreadPool::init()
{
    // Create fresh thread pool for each test
    m_threadPool = std::make_unique<ThreadPool>("TestPool", 4);
}

void TestThreadPool::cleanup()
{
    if (m_threadPool) {
        m_threadPool->stop();
        m_threadPool.reset();
    }
}

void TestThreadPool::testConstruction()
{
    // Test default construction
    ThreadPool pool1("DefaultPool");
    QCOMPARE(pool1.getName(), QString("DefaultPool"));
    QVERIFY(pool1.getThreadCount() > 0);
    QVERIFY(pool1.getThreadCount() <= std::thread::hardware_concurrency());
    
    // Test construction with specific thread count
    ThreadPool pool2("FixedPool", 8);
    QCOMPARE(pool2.getName(), QString("FixedPool"));
    QCOMPARE(pool2.getThreadCount(), 8);
    
    // Test construction with configuration
    ThreadPoolConfig config;
    config.minThreads = 2;
    config.maxThreads = 16;
    config.policy = SchedulingPolicy::WorkStealing;
    config.enableCpuAffinity = false;
    
    ThreadPool pool3("ConfigPool", config);
    QCOMPARE(pool3.getName(), QString("ConfigPool"));
    QCOMPARE(pool3.getMinThreads(), 2);
    QCOMPARE(pool3.getMaxThreads(), 16);
    QVERIFY(pool3.getSchedulingPolicy() == SchedulingPolicy::WorkStealing);
}

void TestThreadPool::testStartStop()
{
    QVERIFY(!m_threadPool->isRunning());
    
    // Test start
    QVERIFY(m_threadPool->start());
    QVERIFY(m_threadPool->isRunning());
    
    // Test double start (should return true but not restart)
    QVERIFY(m_threadPool->start());
    QVERIFY(m_threadPool->isRunning());
    
    // Test stop
    m_threadPool->stop();
    QVERIFY(!m_threadPool->isRunning());
    
    // Test double stop (should be safe)
    m_threadPool->stop();
    QVERIFY(!m_threadPool->isRunning());
    
    // Test restart
    QVERIFY(m_threadPool->start());
    QVERIFY(m_threadPool->isRunning());
}

void TestThreadPool::testBasicTaskSubmission()
{
    m_threadPool->start();
    
    std::atomic<int> counter{0};
    
    // Submit simple task
    auto task = [&counter]() {
        counter.fetch_add(1);
    };
    
    QVERIFY(m_threadPool->submit(task).valid());
    
    // Wait for task completion
    waitForTaskCompletion(1);
    QCOMPARE(counter.load(), 1);
}

void TestThreadPool::testTaskExecution()
{
    m_threadPool->start();
    
    std::atomic<int> executedTasks{0};
    std::atomic<bool> taskExecuted{false};
    
    const int numTasks = 10;
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < numTasks; ++i) {
        auto future = m_threadPool->submit([&executedTasks, &taskExecuted, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            executedTasks.fetch_add(1);
            if (i == 5) {
                taskExecuted.store(true);
            }
        });
        
        QVERIFY(future.valid());
        futures.push_back(std::move(future));
    }
    
    // Wait for all tasks
    for (auto& future : futures) {
        future.wait();
    }
    
    QCOMPARE(executedTasks.load(), numTasks);
    QVERIFY(taskExecuted.load());
}

void TestThreadPool::testTaskReturnValue()
{
    m_threadPool->start();
    
    // Test task with return value
    auto future1 = m_threadPool->submit([]() -> int {
        return 42;
    });
    
    QVERIFY(future1.valid());
    QCOMPARE(future1.get(), 42);
    
    // Test task with string return value
    auto future2 = m_threadPool->submit([]() -> std::string {
        return "Hello, ThreadPool!";
    });
    
    QVERIFY(future2.valid());
    QCOMPARE(QString::fromStdString(future2.get()), QString("Hello, ThreadPool!"));
}

void TestThreadPool::testRoundRobinScheduling()
{
    ThreadPoolConfig config;
    config.policy = SchedulingPolicy::RoundRobin;
    config.minThreads = 4;
    config.maxThreads = 4;
    
    ThreadPool pool("RoundRobinPool", config);
    pool.start();
    
    std::vector<std::atomic<int>> threadCounters(4);
    const int tasksPerThread = 10;
    const int totalTasks = tasksPerThread * 4;
    
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < totalTasks; ++i) {
        auto future = pool.submit([&threadCounters, i]() {
            // Simple way to identify which thread is executing
            auto threadId = std::hash<std::thread::id>{}(std::this_thread::get_id()) % 4;
            threadCounters[threadId].fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
        futures.push_back(std::move(future));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    // With round-robin, each thread should have roughly equal work
    QVERIFY(verifyLoadBalancing(threadCounters, 0.3)); // 30% tolerance for round-robin
}

void TestThreadPool::testLeastLoadedScheduling()
{
    ThreadPoolConfig config;
    config.policy = SchedulingPolicy::LeastLoaded;
    config.minThreads = 4;
    config.maxThreads = 4;
    
    ThreadPool pool("LeastLoadedPool", config);
    pool.start();
    
    std::vector<std::atomic<int>> threadCounters(4);
    const int numTasks = 20;
    
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < numTasks; ++i) {
        auto future = pool.submit([&threadCounters]() {
            auto threadId = std::hash<std::thread::id>{}(std::this_thread::get_id()) % 4;
            threadCounters[threadId].fetch_add(1);
            // Variable task duration to test load balancing
            std::this_thread::sleep_for(std::chrono::milliseconds(1 + (threadId * 2)));
        });
        futures.push_back(std::move(future));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    // Least loaded should distribute work more evenly than simple round-robin
    QVERIFY(verifyLoadBalancing(threadCounters, 0.4)); // More tolerance due to variable duration
}

void TestThreadPool::testWorkStealingScheduling()
{
    ThreadPoolConfig config;
    config.policy = SchedulingPolicy::WorkStealing;
    config.minThreads = 4;
    config.maxThreads = 4;
    
    ThreadPool pool("WorkStealingPool", config);
    pool.start();
    
    std::atomic<int> totalTasks{0};
    std::vector<std::atomic<int>> threadCounters(4);
    const int numTasks = 100;
    
    std::vector<std::future<void>> futures;
    
    // Submit tasks with varying complexity
    for (int i = 0; i < numTasks; ++i) {
        auto future = pool.submit([&threadCounters, &totalTasks, i]() {
            auto threadId = std::hash<std::thread::id>{}(std::this_thread::get_id()) % 4;
            threadCounters[threadId].fetch_add(1);
            totalTasks.fetch_add(1);
            
            // Some tasks take longer to encourage work stealing
            if (i % 10 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
        futures.push_back(std::move(future));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    QCOMPARE(totalTasks.load(), numTasks);
    
    // Work stealing should achieve good load balancing
    QVERIFY(verifyLoadBalancing(threadCounters, 0.5)); // Higher tolerance due to stealing dynamics
}

void TestThreadPool::testHighThroughputTasks()
{
    m_threadPool->start();
    
    const int numTasks = 10000;
    std::atomic<int> completedTasks{0};
    
    QElapsedTimer timer;
    timer.start();
    
    std::vector<std::future<void>> futures;
    futures.reserve(numTasks);
    
    for (int i = 0; i < numTasks; ++i) {
        auto future = m_threadPool->submit([&completedTasks]() {
            completedTasks.fetch_add(1);
        });
        futures.push_back(std::move(future));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    qint64 elapsedMs = timer.elapsed();
    
    QCOMPARE(completedTasks.load(), numTasks);
    
    // Should process at least 1000 tasks per second
    double tasksPerSecond = (numTasks * 1000.0) / elapsedMs;
    QVERIFY(tasksPerSecond > 1000.0);
    
    qDebug() << "High throughput test:" << tasksPerSecond << "tasks/second";
}

void TestThreadPool::testLowLatencySubmission()
{
    m_threadPool->start();
    
    const int numSubmissions = 1000;
    QElapsedTimer timer;
    
    timer.start();
    for (int i = 0; i < numSubmissions; ++i) {
        auto future = m_threadPool->submit([]() {
            // Minimal task
        });
        future.wait();
    }
    qint64 elapsedUs = timer.nsecsElapsed() / 1000;
    
    double avgLatencyUs = static_cast<double>(elapsedUs) / numSubmissions;
    
    // Task submission + execution should be under 100 microseconds on average
    QVERIFY(avgLatencyUs < 100.0);
    
    qDebug() << "Average task latency:" << avgLatencyUs << "microseconds";
}

void TestThreadPool::testConcurrentSubmission()
{
    m_threadPool->start();
    
    const int numThreads = 8;
    const int tasksPerThread = 100;
    std::atomic<int> totalCompleted{0};
    
    std::vector<std::thread> submitterThreads;
    
    QElapsedTimer timer;
    timer.start();
    
    for (int t = 0; t < numThreads; ++t) {
        submitterThreads.emplace_back([this, &totalCompleted, tasksPerThread]() {
            std::vector<std::future<void>> futures;
            
            for (int i = 0; i < tasksPerThread; ++i) {
                auto future = m_threadPool->submit([&totalCompleted]() {
                    totalCompleted.fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                });
                futures.push_back(std::move(future));
            }
            
            for (auto& future : futures) {
                future.wait();
            }
        });
    }
    
    for (auto& thread : submitterThreads) {
        thread.join();
    }
    
    qint64 elapsedMs = timer.elapsed();
    
    QCOMPARE(totalCompleted.load(), numThreads * tasksPerThread);
    
    // Concurrent submission should handle all tasks efficiently
    double tasksPerSecond = (totalCompleted.load() * 1000.0) / elapsedMs;
    QVERIFY(tasksPerSecond > 500.0);
    
    qDebug() << "Concurrent submission:" << tasksPerSecond << "tasks/second";
}

void TestThreadPool::testThreadGrowthShrinking()
{
    ThreadPoolConfig config;
    config.minThreads = 2;
    config.maxThreads = 8;
    config.enableDynamicSizing = true;
    config.idleTimeoutMs = 100; // Short timeout for testing
    
    ThreadPool pool("DynamicPool", config);
    pool.start();
    
    // Initially should have minimum threads
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    QVERIFY(pool.getCurrentThreadCount() >= config.minThreads);
    
    // Submit many tasks to trigger growth
    const int manyTasks = 50;
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < manyTasks; ++i) {
        auto future = pool.submit([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
        futures.push_back(std::move(future));
    }
    
    // Should grow to handle the load
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int peakThreads = pool.getCurrentThreadCount();
    QVERIFY(peakThreads > config.minThreads);
    
    // Wait for tasks to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    // Should shrink back after idle timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int finalThreads = pool.getCurrentThreadCount();
    QVERIFY(finalThreads <= peakThreads);
    QVERIFY(finalThreads >= config.minThreads);
    
    qDebug() << "Thread count: min=" << config.minThreads 
             << "peak=" << peakThreads 
             << "final=" << finalThreads;
}

void TestThreadPool::testTaskException()
{
    m_threadPool->start();
    
    // Task that throws exception
    auto future = m_threadPool->submit([]() -> int {
        throw std::runtime_error("Test exception");
        return 42;
    });
    
    QVERIFY(future.valid());
    
    bool exceptionCaught = false;
    try {
        future.get();
    } catch (const std::runtime_error& e) {
        exceptionCaught = true;
        QCOMPARE(QString(e.what()), QString("Test exception"));
    }
    
    QVERIFY(exceptionCaught);
    
    // Pool should continue operating after exception
    std::atomic<bool> normalTaskExecuted{false};
    auto normalFuture = m_threadPool->submit([&normalTaskExecuted]() {
        normalTaskExecuted.store(true);
    });
    
    normalFuture.wait();
    QVERIFY(normalTaskExecuted.load());
}

void TestThreadPool::testMassiveTaskSubmission()
{
    m_threadPool->start();
    
    const int massiveTasks = 50000;
    std::atomic<int> completedTasks{0};
    
    QElapsedTimer timer;
    timer.start();
    
    // Submit in batches to avoid memory issues
    const int batchSize = 1000;
    for (int batch = 0; batch < massiveTasks / batchSize; ++batch) {
        std::vector<std::future<void>> batchFutures;
        
        for (int i = 0; i < batchSize; ++i) {
            auto future = m_threadPool->submit([&completedTasks]() {
                completedTasks.fetch_add(1);
            });
            batchFutures.push_back(std::move(future));
        }
        
        // Wait for batch completion
        for (auto& future : batchFutures) {
            future.wait();
        }
    }
    
    qint64 elapsedMs = timer.elapsed();
    
    QCOMPARE(completedTasks.load(), massiveTasks);
    
    double tasksPerSecond = (massiveTasks * 1000.0) / elapsedMs;
    QVERIFY(tasksPerSecond > 5000.0); // Should handle at least 5K tasks/second
    
    qDebug() << "Massive task test:" << tasksPerSecond << "tasks/second";
}

void TestThreadPool::testStatisticsAccuracy()
{
    m_threadPool->start();
    
    const int numTasks = 100;
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < numTasks; ++i) {
        auto future = m_threadPool->submit([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
        futures.push_back(std::move(future));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    auto stats = m_threadPool->getStatistics();
    
    QCOMPARE(stats.totalTasksSubmitted, static_cast<size_t>(numTasks));
    QCOMPARE(stats.totalTasksCompleted, static_cast<size_t>(numTasks));
    QCOMPARE(stats.totalTasksFailed, static_cast<size_t>(0));
    
    QVERIFY(stats.averageExecutionTimeUs > 0);
    QVERIFY(stats.totalExecutionTimeUs > 0);
    
    qDebug() << "Statistics - Submitted:" << stats.totalTasksSubmitted
             << "Completed:" << stats.totalTasksCompleted
             << "Avg time:" << stats.averageExecutionTimeUs << "us";
}

// Helper methods implementation
void TestThreadPool::waitForTaskCompletion(int expectedTasks, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    
    while (timer.elapsed() < timeoutMs) {
        auto stats = m_threadPool->getStatistics();
        if (stats.totalTasksCompleted >= static_cast<size_t>(expectedTasks)) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    QFAIL("Timeout waiting for task completion");
}

bool TestThreadPool::verifyLoadBalancing(const std::vector<std::atomic<int>>& counters, double tolerance)
{
    if (counters.empty()) return false;
    
    // Calculate total and average
    int total = 0;
    for (const auto& counter : counters) {
        total += counter.load();
    }
    
    double average = static_cast<double>(total) / counters.size();
    
    // Check if all values are within tolerance of average
    for (const auto& counter : counters) {
        double deviation = std::abs(counter.load() - average) / average;
        if (deviation > tolerance) {
            qDebug() << "Load balancing failed - counter:" << counter.load() 
                     << "average:" << average << "deviation:" << deviation;
            return false;
        }
    }
    
    return true;
}

QTEST_MAIN(TestThreadPool)
#include "test_thread_pool.moc"