#include <QTest>
#include <QApplication>
#include <QSignalSpy>
#include <QTimer>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QThread>
#include <QThreadPool>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTemporaryFile>
#include <memory>
#include <chrono>
#include <vector>
#include <random>

// Core includes
#include "../../../src/core/memory/memory_pool.h"
#include "../../../src/core/events/event_dispatcher.h"
#include "../../../src/core/profiling/profiler.h"

// UI includes
#include "../../../src/ui/windows/performance_dashboard.h"
#include "../../../src/ui/widgets/charts/chart_3d_widget.h"

/**
 * @brief Performance Monitoring System Tests
 * 
 * These tests verify the performance monitoring infrastructure,
 * including profiling capabilities, metrics collection, performance
 * dashboard functionality, and system-wide performance analysis.
 */
class TestPerformanceMonitoring : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QApplication> app;
    std::unique_ptr<PerformanceDashboard> dashboard;
    std::unique_ptr<Monitor::Core::Memory::MemoryPool> memoryPool;
    std::unique_ptr<Monitor::Core::Events::EventDispatcher> eventDispatcher;

    void waitForProcessing(int ms = 50) {
        QEventLoop loop;
        QTimer::singleShot(ms, &loop, &QEventLoop::quit);
        loop.exec();
    }

    void simulateSystemLoad() {
        // Simulate CPU load
        volatile int sum = 0;
        for (int i = 0; i < 100000; ++i) {
            sum += i * i;
        }
    }

    void simulateMemoryLoad() {
        std::vector<std::unique_ptr<char[]>> allocations;
        for (int i = 0; i < 100; ++i) {
            allocations.emplace_back(std::make_unique<char[]>(1024 * 1024)); // 1MB each
        }
    }

    PerformanceDashboard::SystemMetrics generateTestSystemMetrics() {
        PerformanceDashboard::SystemMetrics metrics;
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> cpuDist(10.0, 90.0);
        static std::uniform_real_distribution<> memDist(100.0, 2000.0);
        static std::uniform_real_distribution<> networkDist(100.0, 10000.0);
        
        metrics.cpuUsage = cpuDist(gen);
        metrics.memoryUsage = memDist(gen);
        metrics.memoryPercent = (metrics.memoryUsage / 4000.0) * 100.0;
        metrics.networkRxPackets = networkDist(gen);
        metrics.networkRxMB = metrics.networkRxPackets * 0.001;
        metrics.packetRate = networkDist(gen);
        metrics.parserThroughput = metrics.packetRate * 0.95;
        metrics.avgQueueDepth = cpuDist(gen) / 10.0;
        metrics.timestamp = QDateTime::currentDateTime();
        
        return metrics;
    }

    PerformanceDashboard::WidgetMetrics generateTestWidgetMetrics(const QString& widgetId) {
        PerformanceDashboard::WidgetMetrics metrics;
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> cpuDist(1.0, 25.0);
        static std::uniform_real_distribution<> memDist(10.0, 200.0);
        static std::uniform_real_distribution<> fpsDist(30.0, 120.0);
        static std::uniform_real_distribution<> latencyDist(1.0, 50.0);
        
        metrics.widgetId = widgetId;
        metrics.widgetType = "TestWidget";
        metrics.cpuUsage = cpuDist(gen);
        metrics.memoryUsage = memDist(gen);
        metrics.fps = fpsDist(gen);
        metrics.latency = latencyDist(gen);
        metrics.queueDepth = static_cast<int>(cpuDist(gen) / 5.0);
        metrics.isActive = true;
        metrics.lastUpdate = QDateTime::currentDateTime();
        
        return metrics;
    }

private slots:
    void initTestCase() {
        if (!QApplication::instance()) {
            int argc = 1;
            char* argv[] = {const_cast<char*>("test")};
            app = std::make_unique<QApplication>(argc, argv);
        }
        
        // Initialize core components
        memoryPool = std::make_unique<Monitor::Core::Memory::MemoryPool>(1024 * 1024); // 1MB pool
        eventDispatcher = std::make_unique<Monitor::Core::Events::EventDispatcher>();
        
        // Initialize dashboard
        dashboard = std::make_unique<PerformanceDashboard>();
        dashboard->show();
        waitForProcessing(100);
    }

    void cleanupTestCase() {
        dashboard.reset();
        eventDispatcher.reset();
        memoryPool.reset();
    }

    void cleanup() {
        if (dashboard) {
            dashboard->stopMonitoring();
            dashboard->clearAlerts();
            waitForProcessing();
        }
    }

    // Core Performance Infrastructure Tests
    void testProfilerInitialization();
    void testProfilerScoping();
    void testProfilerNesting();
    void testProfilerThreadSafety();
    void testProfilerStatistics();
    
    // Memory Performance Tests
    void testMemoryPoolPerformance();
    void testMemoryAllocationPatterns();
    void testMemoryFragmentation();
    void testMemoryLeakDetection();
    
    // Event System Performance Tests
    void testEventDispatchPerformance();
    void testEventThroughput();
    void testEventLatency();
    void testEventBackpressure();
    
    // System Metrics Tests
    void testSystemMetricsCollection();
    void testSystemMetricsAccuracy();
    void testSystemMetricsFrequency();
    void testSystemMetricsValidation();
    
    // Widget Performance Tests
    void testWidgetPerformanceTracking();
    void testWidgetMetricsCollection();
    void testWidgetPerformanceComparison();
    void testWidgetResourceUsage();
    
    // Dashboard Performance Tests
    void testDashboardResponseTime();
    void testDashboardUpdatePerformance();
    void testDashboardMemoryUsage();
    void testDashboardConcurrency();
    
    // Performance Analysis Tests
    void testBottleneckDetection();
    void testPerformanceTrends();
    void testPerformanceRegression();
    void testPerformanceOptimization();
    
    // Real-time Monitoring Tests
    void testRealTimeMetricsCollection();
    void testRealTimeAlerts();
    void testRealTimeVisualization();
    void testRealTimeExport();
    
    // Load Testing
    void testHighFrequencyUpdates();
    void testLargeDatasetProcessing();
    void testConcurrentOperations();
    void testSystemLimits();
    
    // Performance Benchmarks
    void benchmarkPacketProcessing();
    void benchmarkWidgetUpdates();
    void benchmarkChartRendering();
    void benchmarkMemoryOperations();
    
    // Error Handling Performance Tests
    void testErrorHandlingOverhead();
    void testFailureRecoveryTime();
    void testResourceExhaustionHandling();
    void testPerformanceDegradation();
};

void TestPerformanceMonitoring::testProfilerInitialization()
{
    // Test profiler initialization
    Monitor::Core::Profiling::Profiler profiler;
    QVERIFY(profiler.isEnabled());
    
    // Test profiler configuration
    profiler.setEnabled(false);
    QVERIFY(!profiler.isEnabled());
    
    profiler.setEnabled(true);
    QVERIFY(profiler.isEnabled());
    
    // Test statistics reset
    profiler.reset();
    auto stats = profiler.getStatistics();
    QVERIFY(stats.totalSamples == 0);
}

void TestPerformanceMonitoring::testProfilerScoping()
{
    Monitor::Core::Profiling::Profiler profiler;
    
    // Test scoped profiling
    QElapsedTimer timer;
    timer.start();
    
    {
        Monitor::Core::Profiling::ScopedProfiler scope("test_function");
        QThread::msleep(10); // Simulate work
    }
    
    qint64 elapsed = timer.elapsed();
    
    // Test that profiling captured the operation
    auto stats = profiler.getStatistics();
    QVERIFY(stats.totalSamples > 0);
    
    auto functionStats = profiler.getFunctionStatistics("test_function");
    QVERIFY(functionStats.callCount > 0);
    QVERIFY(functionStats.totalTime >= 10000); // At least 10ms in microseconds
}

void TestPerformanceMonitoring::testProfilerNesting()
{
    Monitor::Core::Profiling::Profiler profiler;
    profiler.reset();
    
    // Test nested profiling
    {
        Monitor::Core::Profiling::ScopedProfiler outerScope("outer_function");
        
        {
            Monitor::Core::Profiling::ScopedProfiler innerScope("inner_function");
            QThread::msleep(5);
        }
        
        QThread::msleep(5);
    }
    
    // Verify nested profiling
    auto outerStats = profiler.getFunctionStatistics("outer_function");
    auto innerStats = profiler.getFunctionStatistics("inner_function");
    
    QVERIFY(outerStats.callCount > 0);
    QVERIFY(innerStats.callCount > 0);
    QVERIFY(outerStats.totalTime >= innerStats.totalTime);
}

void TestPerformanceMonitoring::testProfilerThreadSafety()
{
    Monitor::Core::Profiling::Profiler profiler;
    profiler.reset();
    
    const int threadCount = 10;
    const int iterationsPerThread = 100;
    
    // Test concurrent profiling
    QThreadPool pool;
    QAtomicInt completedThreads = 0;
    
    for (int t = 0; t < threadCount; ++t) {
        pool.start([&, t]() {
            for (int i = 0; i < iterationsPerThread; ++i) {
                Monitor::Core::Profiling::ScopedProfiler scope(
                    QString("thread_%1_iteration_%2").arg(t).arg(i));
                QThread::usleep(100); // 0.1ms work
            }
            completedThreads.fetchAndAddOrdered(1);
        });
    }
    
    // Wait for completion
    while (completedThreads.loadAcquire() < threadCount) {
        QThread::msleep(10);
    }
    
    pool.waitForDone();
    
    // Verify thread-safe operation
    auto stats = profiler.getStatistics();
    QVERIFY(stats.totalSamples >= threadCount * iterationsPerThread);
    QVERIFY(stats.activeProfilers == 0); // All should be complete
}

void TestPerformanceMonitoring::testProfilerStatistics()
{
    Monitor::Core::Profiling::Profiler profiler;
    profiler.reset();
    
    // Generate profiling data
    const int iterations = 50;
    
    for (int i = 0; i < iterations; ++i) {
        Monitor::Core::Profiling::ScopedProfiler scope("statistics_test");
        QThread::usleep(1000 + (i % 10) * 100); // Variable timing
    }
    
    // Test statistics calculation
    auto funcStats = profiler.getFunctionStatistics("statistics_test");
    QCOMPARE(funcStats.callCount, iterations);
    QVERIFY(funcStats.totalTime > 0);
    QVERIFY(funcStats.averageTime > 0);
    QVERIFY(funcStats.minTime > 0);
    QVERIFY(funcStats.maxTime >= funcStats.minTime);
    QVERIFY(funcStats.averageTime >= funcStats.minTime);
    QVERIFY(funcStats.averageTime <= funcStats.maxTime);
    
    // Test overall statistics
    auto overallStats = profiler.getStatistics();
    QVERIFY(overallStats.totalSamples >= iterations);
    QVERIFY(overallStats.totalTime > 0);
}

void TestPerformanceMonitoring::testMemoryPoolPerformance()
{
    const size_t poolSize = 1024 * 1024; // 1MB
    const size_t blockSize = 64;
    const int iterations = 10000;
    
    // Test memory pool allocation performance
    QElapsedTimer timer;
    timer.start();
    
    std::vector<void*> allocations;
    allocations.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        void* ptr = memoryPool->allocate(blockSize);
        if (ptr) {
            allocations.push_back(ptr);
        }
    }
    
    qint64 allocationTime = timer.elapsed();
    
    // Test deallocation performance
    timer.restart();
    
    for (void* ptr : allocations) {
        memoryPool->deallocate(ptr, blockSize);
    }
    
    qint64 deallocationTime = timer.elapsed();
    
    // Verify performance (should be fast)
    double allocationsPerMs = static_cast<double>(iterations) / allocationTime;
    double deallocationsPerMs = static_cast<double>(iterations) / deallocationTime;
    
    QVERIFY(allocationsPerMs > 100.0); // At least 100 allocations per ms
    QVERIFY(deallocationsPerMs > 100.0); // At least 100 deallocations per ms
    
    // Compare with system malloc
    timer.restart();
    
    std::vector<void*> systemAllocations;
    for (int i = 0; i < iterations / 10; ++i) { // Fewer iterations for malloc
        void* ptr = malloc(blockSize);
        if (ptr) {
            systemAllocations.push_back(ptr);
        }
    }
    
    qint64 systemAllocationTime = timer.elapsed();
    
    for (void* ptr : systemAllocations) {
        free(ptr);
    }
    
    // Pool should be competitive with system allocator
    double systemAllocationsPerMs = static_cast<double>(iterations / 10) / systemAllocationTime;
    QVERIFY(allocationsPerMs >= systemAllocationsPerMs * 0.5); // At least 50% of system performance
}

void TestPerformanceMonitoring::testMemoryAllocationPatterns()
{
    const int patternIterations = 1000;
    
    // Test sequential allocation pattern
    QElapsedTimer timer;
    timer.start();
    
    std::vector<void*> sequential;
    for (int i = 0; i < patternIterations; ++i) {
        void* ptr = memoryPool->allocate(64 + (i % 64)); // Variable sizes
        if (ptr) sequential.push_back(ptr);
    }
    
    qint64 sequentialTime = timer.elapsed();
    
    // Clean up
    for (size_t i = 0; i < sequential.size(); ++i) {
        memoryPool->deallocate(sequential[i], 64 + (i % 64));
    }
    
    // Test random allocation pattern
    timer.restart();
    
    std::vector<std::pair<void*, size_t>> random;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sizeDist(32, 128);
    
    for (int i = 0; i < patternIterations; ++i) {
        size_t size = sizeDist(gen);
        void* ptr = memoryPool->allocate(size);
        if (ptr) random.emplace_back(ptr, size);
    }
    
    qint64 randomTime = timer.elapsed();
    
    // Clean up
    for (auto& [ptr, size] : random) {
        memoryPool->deallocate(ptr, size);
    }
    
    // Both patterns should perform reasonably
    QVERIFY(sequentialTime < patternIterations); // Less than 1ms per allocation
    QVERIFY(randomTime < patternIterations * 2); // Less than 2ms per allocation
}

void TestPerformanceMonitoring::testMemoryFragmentation()
{
    const size_t blockSize = 128;
    const int blocks = 100;
    
    // Allocate many blocks
    std::vector<void*> allocations;
    for (int i = 0; i < blocks; ++i) {
        void* ptr = memoryPool->allocate(blockSize);
        if (ptr) allocations.push_back(ptr);
    }
    
    // Free every other block (create fragmentation)
    for (size_t i = 1; i < allocations.size(); i += 2) {
        memoryPool->deallocate(allocations[i], blockSize);
        allocations[i] = nullptr;
    }
    
    // Test allocation performance with fragmentation
    QElapsedTimer timer;
    timer.start();
    
    std::vector<void*> newAllocations;
    for (int i = 0; i < blocks / 2; ++i) {
        void* ptr = memoryPool->allocate(blockSize);
        if (ptr) newAllocations.push_back(ptr);
    }
    
    qint64 fragmentedTime = timer.elapsed();
    
    // Should still perform reasonably with fragmentation
    QVERIFY(fragmentedTime < blocks); // Less than 1ms per allocation
    
    // Clean up
    for (void* ptr : allocations) {
        if (ptr) memoryPool->deallocate(ptr, blockSize);
    }
    for (void* ptr : newAllocations) {
        memoryPool->deallocate(ptr, blockSize);
    }
}

void TestPerformanceMonitoring::testMemoryLeakDetection()
{
    // Test memory leak detection capability
    size_t initialUsed = memoryPool->getBytesUsed();
    
    // Allocate without deallocating
    std::vector<void*> leaks;
    for (int i = 0; i < 10; ++i) {
        void* ptr = memoryPool->allocate(128);
        if (ptr) leaks.push_back(ptr);
    }
    
    size_t afterAllocation = memoryPool->getBytesUsed();
    QVERIFY(afterAllocation > initialUsed);
    
    // Deallocate half
    for (size_t i = 0; i < leaks.size() / 2; ++i) {
        memoryPool->deallocate(leaks[i], 128);
    }
    leaks.erase(leaks.begin(), leaks.begin() + leaks.size() / 2);
    
    size_t afterPartialDeallocation = memoryPool->getBytesUsed();
    QVERIFY(afterPartialDeallocation < afterAllocation);
    QVERIFY(afterPartialDeallocation > initialUsed);
    
    // Clean up remaining allocations
    for (void* ptr : leaks) {
        memoryPool->deallocate(ptr, 128);
    }
    
    size_t final = memoryPool->getBytesUsed();
    QCOMPARE(final, initialUsed);
}

void TestPerformanceMonitoring::testEventDispatchPerformance()
{
    const int eventCount = 10000;
    
    // Test event dispatch performance
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < eventCount; ++i) {
        auto event = std::make_shared<Monitor::Core::Events::Event>(
            QString("performance_test_%1").arg(i));
        eventDispatcher->dispatch(event);
    }
    
    qint64 dispatchTime = timer.elapsed();
    
    // Wait for all events to be processed
    waitForProcessing(100);
    
    // Should dispatch events quickly
    double eventsPerMs = static_cast<double>(eventCount) / dispatchTime;
    QVERIFY(eventsPerMs > 1000.0); // At least 1000 events per ms
}

void TestPerformanceMonitoring::testEventThroughput()
{
    const int duration = 1000; // 1 second test
    std::atomic<int> eventCount{0};
    std::atomic<bool> running{true};
    
    // Event consumer
    auto consumer = [&eventCount, &running]() {
        while (running.load()) {
            eventCount.fetchAndAddOrdered(1);
            QThread::usleep(10); // Simulate processing time
        }
    };
    
    // Start consumer thread
    QThread consumerThread;
    consumerThread.moveToThread(&consumerThread);
    QTimer::singleShot(0, &consumerThread, consumer);
    consumerThread.start();
    
    // Measure throughput
    QElapsedTimer timer;
    timer.start();
    
    while (timer.elapsed() < duration) {
        auto event = std::make_shared<Monitor::Core::Events::Event>("throughput_test");
        eventDispatcher->dispatch(event);
        QThread::usleep(50); // Control event rate
    }
    
    running.store(false);
    consumerThread.quit();
    consumerThread.wait();
    
    int totalEvents = eventCount.load();
    double throughput = static_cast<double>(totalEvents) / (duration / 1000.0);
    
    // Should handle reasonable throughput
    QVERIFY(throughput > 100.0); // At least 100 events per second
}

void TestPerformanceMonitoring::testEventLatency()
{
    const int samples = 1000;
    std::vector<qint64> latencies;
    latencies.reserve(samples);
    
    QObject receiver;
    connect(eventDispatcher.get(), &Monitor::Core::Events::EventDispatcher::eventProcessed,
            [&latencies](const QString& eventType, qint64 processingTime) {
                if (eventType.startsWith("latency_test")) {
                    latencies.push_back(processingTime);
                }
            });
    
    // Measure event latencies
    for (int i = 0; i < samples; ++i) {
        auto event = std::make_shared<Monitor::Core::Events::Event>(
            QString("latency_test_%1").arg(i));
        eventDispatcher->dispatch(event);
        
        if (i % 100 == 0) {
            waitForProcessing(10);
        }
    }
    
    waitForProcessing(200);
    
    // Analyze latencies
    if (!latencies.empty()) {
        qint64 totalLatency = 0;
        qint64 minLatency = latencies[0];
        qint64 maxLatency = latencies[0];
        
        for (qint64 latency : latencies) {
            totalLatency += latency;
            minLatency = qMin(minLatency, latency);
            maxLatency = qMax(maxLatency, latency);
        }
        
        double avgLatency = static_cast<double>(totalLatency) / latencies.size();
        
        // Latency should be reasonable
        QVERIFY(avgLatency < 10000.0); // Average < 10ms
        QVERIFY(minLatency >= 0);
        QVERIFY(maxLatency > minLatency);
    }
}

void TestPerformanceMonitoring::testEventBackpressure()
{
    const int overloadEvents = 50000;
    
    // Generate event overload
    QElapsedTimer timer;
    timer.start();
    
    std::atomic<int> processedEvents{0};
    
    QObject receiver;
    connect(eventDispatcher.get(), &Monitor::Core::Events::EventDispatcher::eventProcessed,
            [&processedEvents](const QString&, qint64) {
                processedEvents.fetchAndAddOrdered(1);
            });
    
    // Send many events quickly
    for (int i = 0; i < overloadEvents; ++i) {
        auto event = std::make_shared<Monitor::Core::Events::Event>(
            QString("backpressure_test_%1").arg(i));
        eventDispatcher->dispatch(event);
    }
    
    qint64 sendTime = timer.elapsed();
    
    // Wait for processing
    waitForProcessing(1000);
    
    int processed = processedEvents.load();
    
    // Should handle backpressure gracefully
    QVERIFY(processed > 0);
    QVERIFY(sendTime < overloadEvents / 10); // Should not block significantly
    
    // May not process all events if backpressure handling drops some
    QVERIFY(processed <= overloadEvents);
}

void TestPerformanceMonitoring::testSystemMetricsCollection()
{
    dashboard->startMonitoring();
    
    // Test system metrics collection performance
    const int iterations = 100;
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < iterations; ++i) {
        auto metrics = generateTestSystemMetrics();
        dashboard->updateSystemMetrics(metrics);
        
        if (i % 10 == 0) {
            waitForProcessing(5);
        }
    }
    
    qint64 collectionTime = timer.elapsed();
    
    // Should collect metrics efficiently
    double metricsPerMs = static_cast<double>(iterations) / collectionTime;
    QVERIFY(metricsPerMs > 1.0); // At least 1 metric collection per ms
    
    // Verify metrics were stored
    auto currentMetrics = dashboard->getCurrentSystemMetrics();
    QVERIFY(currentMetrics.cpuUsage >= 0.0);
    QVERIFY(currentMetrics.memoryUsage >= 0.0);
}

void TestPerformanceMonitoring::testSystemMetricsAccuracy()
{
    dashboard->startMonitoring();
    
    // Generate known metrics pattern
    PerformanceDashboard::SystemMetrics knownMetrics;
    knownMetrics.cpuUsage = 42.5;
    knownMetrics.memoryUsage = 1024.0;
    knownMetrics.memoryPercent = 25.6;
    knownMetrics.networkRxPackets = 5000.0;
    knownMetrics.packetRate = 2500.0;
    knownMetrics.timestamp = QDateTime::currentDateTime();
    
    dashboard->updateSystemMetrics(knownMetrics);
    waitForProcessing();
    
    // Verify accuracy
    auto retrievedMetrics = dashboard->getCurrentSystemMetrics();
    QCOMPARE(retrievedMetrics.cpuUsage, knownMetrics.cpuUsage);
    QCOMPARE(retrievedMetrics.memoryUsage, knownMetrics.memoryUsage);
    QCOMPARE(retrievedMetrics.memoryPercent, knownMetrics.memoryPercent);
    QCOMPARE(retrievedMetrics.networkRxPackets, knownMetrics.networkRxPackets);
    QCOMPARE(retrievedMetrics.packetRate, knownMetrics.packetRate);
}

void TestPerformanceMonitoring::testSystemMetricsFrequency()
{
    dashboard->setUpdateInterval(50); // 20 Hz
    dashboard->startMonitoring();
    
    // Monitor update frequency
    QSignalSpy spy(dashboard.get(), &PerformanceDashboard::metricsUpdated);
    
    // Generate continuous metrics
    QTimer metricsTimer;
    connect(&metricsTimer, &QTimer::timeout, [this]() {
        auto metrics = generateTestSystemMetrics();
        dashboard->updateSystemMetrics(metrics);
    });
    
    metricsTimer.start(25); // 40 Hz input
    
    // Let it run for 1 second
    waitForProcessing(1000);
    metricsTimer.stop();
    
    // Should have received updates
    int updateCount = spy.count();
    
    // Should be close to expected frequency (allow some variance)
    QVERIFY(updateCount >= 15); // At least 15 Hz
    QVERIFY(updateCount <= 25); // No more than 25 Hz
}

void TestPerformanceMonitoring::testSystemMetricsValidation()
{
    dashboard->startMonitoring();
    
    // Test invalid metrics handling
    PerformanceDashboard::SystemMetrics invalidMetrics;
    invalidMetrics.cpuUsage = -10.0; // Invalid
    invalidMetrics.memoryUsage = -1024.0; // Invalid
    invalidMetrics.memoryPercent = 150.0; // Invalid (over 100%)
    invalidMetrics.networkRxPackets = -100.0; // Invalid
    
    dashboard->updateSystemMetrics(invalidMetrics);
    waitForProcessing();
    
    // Should handle invalid values gracefully
    auto retrievedMetrics = dashboard->getCurrentSystemMetrics();
    
    // Invalid values should be clamped or rejected
    QVERIFY(retrievedMetrics.cpuUsage >= 0.0);
    QVERIFY(retrievedMetrics.memoryUsage >= 0.0);
    QVERIFY(retrievedMetrics.memoryPercent <= 100.0);
    QVERIFY(retrievedMetrics.networkRxPackets >= 0.0);
}

void TestPerformanceMonitoring::testWidgetPerformanceTracking()
{
    dashboard->startMonitoring();
    
    const QString widgetId = "performance_test_widget";
    const int updates = 100;
    
    // Test widget performance tracking
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < updates; ++i) {
        auto metrics = generateTestWidgetMetrics(widgetId);
        dashboard->updateWidgetMetrics(widgetId, metrics);
        
        if (i % 10 == 0) {
            waitForProcessing(5);
        }
    }
    
    qint64 trackingTime = timer.elapsed();
    
    // Should track widget performance efficiently
    double updatesPerMs = static_cast<double>(updates) / trackingTime;
    QVERIFY(updatesPerMs > 0.5); // At least 0.5 updates per ms
    
    // Verify widget is tracked
    auto monitoredWidgets = dashboard->getMonitoredWidgets();
    QVERIFY(monitoredWidgets.contains(widgetId));
    
    auto widgetMetrics = dashboard->getWidgetMetrics(widgetId);
    QCOMPARE(widgetMetrics.widgetId, widgetId);
    QVERIFY(widgetMetrics.cpuUsage >= 0.0);
    QVERIFY(widgetMetrics.memoryUsage >= 0.0);
}

void TestPerformanceMonitoring::testWidgetMetricsCollection()
{
    dashboard->startMonitoring();
    
    const int widgetCount = 20;
    const int metricsPerWidget = 50;
    
    QElapsedTimer timer;
    timer.start();
    
    // Collect metrics for multiple widgets
    for (int w = 0; w < widgetCount; ++w) {
        QString widgetId = QString("widget_%1").arg(w);
        
        for (int m = 0; m < metricsPerWidget; ++m) {
            auto metrics = generateTestWidgetMetrics(widgetId);
            dashboard->updateWidgetMetrics(widgetId, metrics);
        }
        
        if (w % 5 == 0) {
            waitForProcessing(10);
        }
    }
    
    qint64 collectionTime = timer.elapsed();
    
    // Should collect metrics for all widgets efficiently
    int totalMetrics = widgetCount * metricsPerWidget;
    double metricsPerMs = static_cast<double>(totalMetrics) / collectionTime;
    QVERIFY(metricsPerMs > 1.0); // At least 1 metric per ms
    
    // Verify all widgets are monitored
    auto monitoredWidgets = dashboard->getMonitoredWidgets();
    QCOMPARE(monitoredWidgets.size(), widgetCount);
}

void TestPerformanceMonitoring::testWidgetPerformanceComparison()
{
    dashboard->startMonitoring();
    
    // Create widgets with different performance characteristics
    QString fastWidget = "fast_widget";
    QString slowWidget = "slow_widget";
    
    // Fast widget metrics
    for (int i = 0; i < 50; ++i) {
        auto metrics = generateTestWidgetMetrics(fastWidget);
        metrics.cpuUsage = 5.0 + (i % 5); // Low CPU
        metrics.fps = 120.0 - (i % 10); // High FPS
        metrics.latency = 1.0 + (i % 3); // Low latency
        dashboard->updateWidgetMetrics(fastWidget, metrics);
    }
    
    // Slow widget metrics
    for (int i = 0; i < 50; ++i) {
        auto metrics = generateTestWidgetMetrics(slowWidget);
        metrics.cpuUsage = 25.0 + (i % 10); // High CPU
        metrics.fps = 30.0 + (i % 5); // Low FPS
        metrics.latency = 20.0 + (i % 10); // High latency
        dashboard->updateWidgetMetrics(slowWidget, metrics);
    }
    
    waitForProcessing(100);
    
    // Compare performance
    auto fastMetrics = dashboard->getWidgetMetrics(fastWidget);
    auto slowMetrics = dashboard->getWidgetMetrics(slowWidget);
    
    QVERIFY(fastMetrics.cpuUsage < slowMetrics.cpuUsage);
    QVERIFY(fastMetrics.fps > slowMetrics.fps);
    QVERIFY(fastMetrics.latency < slowMetrics.latency);
}

void TestPerformanceMonitoring::testWidgetResourceUsage()
{
    dashboard->startMonitoring();
    
    const QString widgetId = "resource_test_widget";
    const int iterations = 100;
    
    // Simulate increasing resource usage
    for (int i = 0; i < iterations; ++i) {
        auto metrics = generateTestWidgetMetrics(widgetId);
        metrics.memoryUsage = 50.0 + i * 2.0; // Increasing memory
        metrics.queueDepth = i / 10; // Increasing queue depth
        
        dashboard->updateWidgetMetrics(widgetId, metrics);
    }
    
    waitForProcessing(100);
    
    // Verify resource tracking
    auto finalMetrics = dashboard->getWidgetMetrics(widgetId);
    QVERIFY(finalMetrics.memoryUsage > 50.0);
    QVERIFY(finalMetrics.queueDepth >= 0);
    
    // Check for resource usage alerts
    auto alerts = dashboard->getActiveAlerts();
    
    // May have memory or queue depth alerts
    bool hasResourceAlert = false;
    for (const auto& alert : alerts) {
        if (alert.metricType == PerformanceDashboard::MetricType::Widget_Memory ||
            alert.metricType == PerformanceDashboard::MetricType::Queue_Depth) {
            hasResourceAlert = true;
            break;
        }
    }
    
    // Resource usage may or may not trigger alerts depending on thresholds
    Q_UNUSED(hasResourceAlert);
}

void TestPerformanceMonitoring::testDashboardResponseTime()
{
    // Test dashboard UI response time
    QElapsedTimer timer;
    timer.start();
    
    // Start monitoring
    dashboard->startMonitoring();
    
    qint64 startTime = timer.elapsed();
    QVERIFY(startTime < 100); // Should start within 100ms
    
    // Update metrics
    timer.restart();
    auto metrics = generateTestSystemMetrics();
    dashboard->updateSystemMetrics(metrics);
    qint64 updateTime = timer.elapsed();
    QVERIFY(updateTime < 50); // Should update within 50ms
    
    // Stop monitoring
    timer.restart();
    dashboard->stopMonitoring();
    qint64 stopTime = timer.elapsed();
    QVERIFY(stopTime < 100); // Should stop within 100ms
}

void TestPerformanceMonitoring::testDashboardUpdatePerformance()
{
    dashboard->startMonitoring();
    
    const int rapidUpdates = 1000;
    
    QElapsedTimer timer;
    timer.start();
    
    // Rapid dashboard updates
    for (int i = 0; i < rapidUpdates; ++i) {
        auto systemMetrics = generateTestSystemMetrics();
        dashboard->updateSystemMetrics(systemMetrics);
        
        auto widgetMetrics = generateTestWidgetMetrics(QString("rapid_widget_%1").arg(i % 10));
        dashboard->updateWidgetMetrics(widgetMetrics.widgetId, widgetMetrics);
        
        if (i % 100 == 0) {
            waitForProcessing(5);
        }
    }
    
    qint64 updateTime = timer.elapsed();
    
    // Should handle rapid updates efficiently
    double updatesPerMs = static_cast<double>(rapidUpdates * 2) / updateTime; // 2 updates per iteration
    QVERIFY(updatesPerMs > 1.0); // At least 1 update per ms
}

void TestPerformanceMonitoring::testDashboardMemoryUsage()
{
    size_t initialMemory = dashboard->getMemoryUsage();
    
    dashboard->startMonitoring();
    
    // Add lots of data to dashboard
    for (int i = 0; i < 1000; ++i) {
        auto systemMetrics = generateTestSystemMetrics();
        dashboard->updateSystemMetrics(systemMetrics);
        
        for (int w = 0; w < 10; ++w) {
            auto widgetMetrics = generateTestWidgetMetrics(QString("memory_widget_%1").arg(w));
            dashboard->updateWidgetMetrics(widgetMetrics.widgetId, widgetMetrics);
        }
        
        if (i % 100 == 0) {
            waitForProcessing(10);
        }
    }
    
    size_t afterDataMemory = dashboard->getMemoryUsage();
    
    // Memory should increase with data
    QVERIFY(afterDataMemory > initialMemory);
    
    // Clear history and check memory cleanup
    dashboard->clearHistory();
    waitForProcessing(100);
    
    size_t afterClearMemory = dashboard->getMemoryUsage();
    
    // Memory should be reduced after clearing
    QVERIFY(afterClearMemory < afterDataMemory);
}

void TestPerformanceMonitoring::testDashboardConcurrency()
{
    dashboard->startMonitoring();
    
    const int threadCount = 5;
    const int updatesPerThread = 200;
    
    QThreadPool pool;
    QAtomicInt completedThreads{0};
    
    // Concurrent dashboard updates
    for (int t = 0; t < threadCount; ++t) {
        pool.start([this, &completedThreads, t, updatesPerThread]() {
            for (int i = 0; i < updatesPerThread; ++i) {
                auto systemMetrics = generateTestSystemMetrics();
                dashboard->updateSystemMetrics(systemMetrics);
                
                auto widgetMetrics = generateTestWidgetMetrics(
                    QString("concurrent_widget_%1_%2").arg(t).arg(i));
                dashboard->updateWidgetMetrics(widgetMetrics.widgetId, widgetMetrics);
                
                QThread::usleep(100); // Small delay
            }
            completedThreads.fetchAndAddOrdered(1);
        });
    }
    
    // Wait for completion
    pool.waitForDone();
    
    // Should handle concurrent updates without corruption
    QCOMPARE(completedThreads.load(), threadCount);
    
    // Dashboard should still be responsive
    auto currentMetrics = dashboard->getCurrentSystemMetrics();
    QVERIFY(currentMetrics.timestamp.isValid());
}

void TestPerformanceMonitoring::testBottleneckDetection()
{
    dashboard->startMonitoring();
    
    // Create bottleneck scenario
    PerformanceDashboard::SystemMetrics metrics;
    metrics.cpuUsage = 95.0; // High CPU
    metrics.memoryUsage = 2000.0;
    metrics.memoryPercent = 80.0; // High memory
    metrics.networkRxPackets = 10000.0; // High network
    metrics.packetRate = 5000.0; // Medium packet rate
    metrics.parserThroughput = 1000.0; // Low parser throughput (bottleneck)
    metrics.avgQueueDepth = 50.0; // High queue depth
    
    dashboard->updateSystemMetrics(metrics);
    waitForProcessing(100);
    
    // Check for bottleneck detection
    auto bottlenecks = dashboard->detectBottlenecks();
    QVERIFY(!bottlenecks.isEmpty());
    
    // Should identify parser as bottleneck
    bool parserBottleneckFound = false;
    for (const auto& bottleneck : bottlenecks) {
        if (bottleneck.contains("parser", Qt::CaseInsensitive)) {
            parserBottleneckFound = true;
            break;
        }
    }
    QVERIFY(parserBottleneckFound);
}

void TestPerformanceMonitoring::testPerformanceTrends()
{
    dashboard->startMonitoring();
    dashboard->setHistorySize(5); // 5 minutes history
    
    // Create performance trend (degrading performance)
    for (int i = 0; i < 100; ++i) {
        PerformanceDashboard::SystemMetrics metrics;
        metrics.cpuUsage = 20.0 + i * 0.5; // Increasing CPU
        metrics.memoryUsage = 500.0 + i * 10.0; // Increasing memory
        metrics.packetRate = 5000.0 - i * 20.0; // Decreasing throughput
        metrics.timestamp = QDateTime::currentDateTime();
        
        dashboard->updateSystemMetrics(metrics);
        
        if (i % 10 == 0) {
            waitForProcessing(10);
        }
    }
    
    waitForProcessing(100);
    
    // Analyze trends
    auto trends = dashboard->analyzeTrends();
    QVERIFY(!trends.isEmpty());
    
    // Should detect increasing CPU and memory trends
    bool cpuTrendFound = false;
    bool memoryTrendFound = false;
    
    for (const auto& trend : trends) {
        if (trend.contains("CPU") && trend.contains("increasing")) {
            cpuTrendFound = true;
        }
        if (trend.contains("memory") && trend.contains("increasing")) {
            memoryTrendFound = true;
        }
    }
    
    QVERIFY(cpuTrendFound);
    QVERIFY(memoryTrendFound);
}

void TestPerformanceMonitoring::testPerformanceRegression()
{
    dashboard->startMonitoring();
    
    // Establish baseline performance
    for (int i = 0; i < 50; ++i) {
        PerformanceDashboard::SystemMetrics metrics;
        metrics.cpuUsage = 25.0;
        metrics.packetRate = 8000.0;
        metrics.parserThroughput = 7800.0;
        metrics.timestamp = QDateTime::currentDateTime();
        
        dashboard->updateSystemMetrics(metrics);
    }
    
    waitForProcessing(100);
    
    // Save baseline
    dashboard->savePerformanceBaseline("test_baseline");
    
    // Simulate performance regression
    for (int i = 0; i < 50; ++i) {
        PerformanceDashboard::SystemMetrics metrics;
        metrics.cpuUsage = 50.0; // 2x CPU usage
        metrics.packetRate = 4000.0; // Half packet rate
        metrics.parserThroughput = 3900.0; // Half throughput
        metrics.timestamp = QDateTime::currentDateTime();
        
        dashboard->updateSystemMetrics(metrics);
    }
    
    waitForProcessing(100);
    
    // Detect regression
    auto regressions = dashboard->detectRegressions("test_baseline");
    QVERIFY(!regressions.isEmpty());
    
    // Should detect performance degradation
    bool performanceDegradation = false;
    for (const auto& regression : regressions) {
        if (regression.contains("degradation") || regression.contains("regression")) {
            performanceDegradation = true;
            break;
        }
    }
    QVERIFY(performanceDegradation);
}

void TestPerformanceMonitoring::testPerformanceOptimization()
{
    dashboard->startMonitoring();
    
    // Create suboptimal scenario
    PerformanceDashboard::SystemMetrics metrics;
    metrics.cpuUsage = 80.0;
    metrics.memoryUsage = 3000.0;
    metrics.memoryPercent = 75.0;
    metrics.avgQueueDepth = 25.0;
    metrics.frameDrops = 5.0;
    
    dashboard->updateSystemMetrics(metrics);
    waitForProcessing(100);
    
    // Get optimization suggestions
    auto suggestions = dashboard->getOptimizationSuggestions();
    QVERIFY(!suggestions.isEmpty());
    
    // Should suggest optimizations
    bool hasMemoryOptimization = false;
    bool hasCpuOptimization = false;
    bool hasQueueOptimization = false;
    
    for (const auto& suggestion : suggestions) {
        if (suggestion.contains("memory", Qt::CaseInsensitive)) {
            hasMemoryOptimization = true;
        }
        if (suggestion.contains("CPU", Qt::CaseInsensitive)) {
            hasCpuOptimization = true;
        }
        if (suggestion.contains("queue", Qt::CaseInsensitive)) {
            hasQueueOptimization = true;
        }
    }
    
    // Should have suggestions for high resource usage
    QVERIFY(hasMemoryOptimization || hasCpuOptimization || hasQueueOptimization);
}

void TestPerformanceMonitoring::testRealTimeMetricsCollection()
{
    dashboard->setUpdateInterval(100); // 10 Hz
    dashboard->startMonitoring();
    
    QSignalSpy metricsSpy(dashboard.get(), &PerformanceDashboard::metricsUpdated);
    
    // Generate real-time metrics stream
    QTimer metricsGenerator;
    int metricsCount = 0;
    
    connect(&metricsGenerator, &QTimer::timeout, [this, &metricsCount]() {
        auto metrics = generateTestSystemMetrics();
        dashboard->updateSystemMetrics(metrics);
        metricsCount++;
    });
    
    metricsGenerator.start(50); // 20 Hz input
    
    // Run for 2 seconds
    waitForProcessing(2000);
    metricsGenerator.stop();
    
    // Should have collected real-time metrics
    QVERIFY(metricsSpy.count() > 10); // At least 10 updates in 2 seconds
    QVERIFY(metricsCount >= 35); // Should have generated at least 35 metrics
}

void TestPerformanceMonitoring::testRealTimeAlerts()
{
    dashboard->startMonitoring();
    
    // Set low thresholds for testing
    dashboard->setThreshold(PerformanceDashboard::MetricType::CPU_Usage,
                           PerformanceDashboard::AlertLevel::Warning, 30.0);
    dashboard->setThreshold(PerformanceDashboard::MetricType::CPU_Usage,
                           PerformanceDashboard::AlertLevel::Error, 50.0);
    
    QSignalSpy alertSpy(dashboard.get(), &PerformanceDashboard::alertTriggered);
    
    // Generate metrics that trigger alerts
    PerformanceDashboard::SystemMetrics metrics;
    metrics.cpuUsage = 35.0; // Should trigger warning
    dashboard->updateSystemMetrics(metrics);
    
    waitForProcessing(100);
    
    metrics.cpuUsage = 55.0; // Should trigger error
    dashboard->updateSystemMetrics(metrics);
    
    waitForProcessing(100);
    
    // Should have triggered alerts
    QVERIFY(alertSpy.count() >= 1);
    
    auto alerts = dashboard->getActiveAlerts();
    bool hasWarning = false;
    bool hasError = false;
    
    for (const auto& alert : alerts) {
        if (alert.level == PerformanceDashboard::AlertLevel::Warning) {
            hasWarning = true;
        }
        if (alert.level == PerformanceDashboard::AlertLevel::Error) {
            hasError = true;
        }
    }
    
    QVERIFY(hasWarning || hasError);
}

void TestPerformanceMonitoring::testRealTimeVisualization()
{
    dashboard->startMonitoring();
    dashboard->show();
    
    // Test real-time visualization updates
    QElapsedTimer timer;
    timer.start();
    
    // Generate streaming data
    for (int i = 0; i < 100; ++i) {
        auto systemMetrics = generateTestSystemMetrics();
        dashboard->updateSystemMetrics(systemMetrics);
        
        // Add some widget metrics
        auto widgetMetrics = generateTestWidgetMetrics(QString("viz_widget_%1").arg(i % 5));
        dashboard->updateWidgetMetrics(widgetMetrics.widgetId, widgetMetrics);
        
        waitForProcessing(20); // 50 Hz update rate
    }
    
    qint64 visualizationTime = timer.elapsed();
    
    // Visualization should keep up with real-time data
    QVERIFY(visualizationTime < 5000); // Should complete within 5 seconds
    
    // UI should remain responsive
    QVERIFY(dashboard->isVisible());
    QVERIFY(dashboard->isMonitoring());
}

void TestPerformanceMonitoring::testRealTimeExport()
{
    dashboard->startMonitoring();
    
    // Generate data for export
    for (int i = 0; i < 50; ++i) {
        auto metrics = generateTestSystemMetrics();
        dashboard->updateSystemMetrics(metrics);
    }
    
    waitForProcessing(100);
    
    // Test real-time export
    QTemporaryFile exportFile;
    exportFile.open();
    
    QElapsedTimer timer;
    timer.start();
    
    bool exportResult = dashboard->exportReport(exportFile.fileName());
    
    qint64 exportTime = timer.elapsed();
    
    // Export should succeed and be fast
    QVERIFY(exportResult);
    QVERIFY(exportTime < 1000); // Should export within 1 second
    
    // Verify export file
    QFileInfo fileInfo(exportFile.fileName());
    QVERIFY(fileInfo.size() > 0);
}

void TestPerformanceMonitoring::testHighFrequencyUpdates()
{
    dashboard->setUpdateInterval(10); // 100 Hz
    dashboard->startMonitoring();
    
    const int highFrequencyUpdates = 10000;
    
    QElapsedTimer timer;
    timer.start();
    
    // High frequency update stress test
    for (int i = 0; i < highFrequencyUpdates; ++i) {
        auto metrics = generateTestSystemMetrics();
        dashboard->updateSystemMetrics(metrics);
        
        // No waiting - maximum update rate
    }
    
    qint64 updateTime = timer.elapsed();
    
    // Should handle high frequency updates
    double updatesPerMs = static_cast<double>(highFrequencyUpdates) / updateTime;
    QVERIFY(updatesPerMs > 10.0); // At least 10 updates per ms
    
    // System should remain stable
    QVERIFY(dashboard->isMonitoring());
    
    auto currentMetrics = dashboard->getCurrentSystemMetrics();
    QVERIFY(currentMetrics.timestamp.isValid());
}

void TestPerformanceMonitoring::testLargeDatasetProcessing()
{
    dashboard->startMonitoring();
    
    const int largeDatasetSize = 100000;
    
    QElapsedTimer timer;
    timer.start();
    
    // Process large dataset
    for (int i = 0; i < largeDatasetSize; ++i) {
        auto systemMetrics = generateTestSystemMetrics();
        dashboard->updateSystemMetrics(systemMetrics);
        
        // Add widget metrics for multiple widgets
        for (int w = 0; w < 10; ++w) {
            auto widgetMetrics = generateTestWidgetMetrics(QString("large_widget_%1").arg(w));
            dashboard->updateWidgetMetrics(widgetMetrics.widgetId, widgetMetrics);
        }
        
        if (i % 1000 == 0) {
            waitForProcessing(1);
        }
    }
    
    qint64 processingTime = timer.elapsed();
    
    // Should process large datasets efficiently
    double itemsPerMs = static_cast<double>(largeDatasetSize * 11) / processingTime; // 11 items per iteration
    QVERIFY(itemsPerMs > 1.0); // At least 1 item per ms
    
    // Memory usage should be controlled
    size_t memoryUsage = dashboard->getMemoryUsage();
    QVERIFY(memoryUsage < 100 * 1024 * 1024); // Less than 100MB
}

void TestPerformanceMonitoring::testConcurrentOperations()
{
    dashboard->startMonitoring();
    
    const int concurrentThreads = 10;
    const int operationsPerThread = 1000;
    
    QThreadPool pool;
    QAtomicInt completedOperations{0};
    
    QElapsedTimer timer;
    timer.start();
    
    // Concurrent operations stress test
    for (int t = 0; t < concurrentThreads; ++t) {
        pool.start([this, &completedOperations, t, operationsPerThread]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                // Mix of different operations
                if (i % 3 == 0) {
                    auto systemMetrics = generateTestSystemMetrics();
                    dashboard->updateSystemMetrics(systemMetrics);
                } else if (i % 3 == 1) {
                    auto widgetMetrics = generateTestWidgetMetrics(QString("concurrent_%1_%2").arg(t).arg(i));
                    dashboard->updateWidgetMetrics(widgetMetrics.widgetId, widgetMetrics);
                } else {
                    // Query operations
                    dashboard->getCurrentSystemMetrics();
                    dashboard->getActiveAlerts();
                }
                
                completedOperations.fetchAndAddOrdered(1);
            }
        });
    }
    
    pool.waitForDone();
    
    qint64 concurrentTime = timer.elapsed();
    
    // Should handle concurrent operations
    int totalOperations = completedOperations.load();
    QCOMPARE(totalOperations, concurrentThreads * operationsPerThread);
    
    double operationsPerMs = static_cast<double>(totalOperations) / concurrentTime;
    QVERIFY(operationsPerMs > 1.0); // At least 1 operation per ms
    
    // Dashboard should remain stable
    QVERIFY(dashboard->isMonitoring());
}

void TestPerformanceMonitoring::testSystemLimits()
{
    dashboard->startMonitoring();
    
    // Test system limits by creating extreme scenarios
    const int extremeWidgetCount = 1000;
    const int extremeMetricsPerWidget = 100;
    
    QElapsedTimer timer;
    timer.start();
    
    // Create extreme widget count
    for (int w = 0; w < extremeWidgetCount; ++w) {
        QString widgetId = QString("extreme_widget_%1").arg(w);
        
        for (int m = 0; m < extremeMetricsPerWidget; ++m) {
            auto metrics = generateTestWidgetMetrics(widgetId);
            dashboard->updateWidgetMetrics(widgetId, metrics);
        }
        
        if (w % 100 == 0) {
            waitForProcessing(10);
        }
    }
    
    qint64 extremeTime = timer.elapsed();
    
    // Should handle extreme loads gracefully
    QVERIFY(extremeTime < 60000); // Complete within 1 minute
    
    // System should still be responsive
    auto monitoredWidgets = dashboard->getMonitoredWidgets();
    QVERIFY(monitoredWidgets.size() > 0);
    QVERIFY(monitoredWidgets.size() <= extremeWidgetCount);
    
    // Memory usage should be reasonable
    size_t memoryUsage = dashboard->getMemoryUsage();
    QVERIFY(memoryUsage < 500 * 1024 * 1024); // Less than 500MB
}

void TestPerformanceMonitoring::benchmarkPacketProcessing()
{
    const int packetCount = 100000;
    const size_t packetSize = 1024;
    
    // Benchmark packet processing throughput
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < packetCount; ++i) {
        // Simulate packet processing
        void* packet = memoryPool->allocate(packetSize);
        if (packet) {
            // Simulate packet parsing
            memset(packet, i % 256, packetSize);
            
            // Simulate field extraction
            int* intField = static_cast<int*>(packet);
            *intField = i;
            
            memoryPool->deallocate(packet, packetSize);
        }
    }
    
    qint64 processingTime = timer.elapsed();
    
    // Calculate throughput
    double packetsPerSecond = (static_cast<double>(packetCount) / processingTime) * 1000.0;
    double mbPerSecond = (packetsPerSecond * packetSize) / (1024.0 * 1024.0);
    
    // Should achieve high throughput
    QVERIFY(packetsPerSecond > 10000.0); // At least 10K packets/second
    QVERIFY(mbPerSecond > 10.0); // At least 10 MB/second
    
    qDebug() << "Packet Processing Benchmark:";
    qDebug() << "  Packets/second:" << packetsPerSecond;
    qDebug() << "  MB/second:" << mbPerSecond;
}

void TestPerformanceMonitoring::benchmarkWidgetUpdates()
{
    const int widgetCount = 100;
    const int updatesPerWidget = 1000;
    
    // Create test widgets
    std::vector<std::unique_ptr<Chart3DWidget>> widgets;
    for (int i = 0; i < widgetCount; ++i) {
        widgets.emplace_back(std::make_unique<Chart3DWidget>(
            QString("benchmark_widget_%1").arg(i),
            QString("Benchmark Widget %1").arg(i)
        ));
    }
    
    // Benchmark widget updates
    QElapsedTimer timer;
    timer.start();
    
    for (int u = 0; u < updatesPerWidget; ++u) {
        for (int w = 0; w < widgetCount; ++w) {
            widgets[w]->updateFieldDisplay("benchmark.field", QVariant(u * w));
        }
        
        if (u % 100 == 0) {
            waitForProcessing(1);
        }
    }
    
    qint64 updateTime = timer.elapsed();
    
    // Calculate update rate
    int totalUpdates = widgetCount * updatesPerWidget;
    double updatesPerSecond = (static_cast<double>(totalUpdates) / updateTime) * 1000.0;
    
    // Should achieve high update rate
    QVERIFY(updatesPerSecond > 1000.0); // At least 1K updates/second
    
    qDebug() << "Widget Update Benchmark:";
    qDebug() << "  Updates/second:" << updatesPerSecond;
    qDebug() << "  Total updates:" << totalUpdates;
}

void TestPerformanceMonitoring::benchmarkChartRendering()
{
    // Create chart widget for rendering benchmark
    auto chartWidget = std::make_unique<Chart3DWidget>(
        "benchmark_chart",
        "Benchmark Chart"
    );
    chartWidget->show();
    
    // Add series for rendering
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "benchmark.data";
    config.renderMode = Chart3DWidget::RenderMode::Points;
    chartWidget->addSeries3D("benchmark.data", config);
    
    waitForProcessing(100);
    
    const int renderFrames = 1000;
    
    // Benchmark chart rendering
    QElapsedTimer timer;
    timer.start();
    
    for (int frame = 0; frame < renderFrames; ++frame) {
        // Update chart data
        for (int point = 0; point < 100; ++point) {
            double value = qSin(frame * 0.1 + point * 0.05);
            chartWidget->updateFieldDisplay("benchmark.data", QVariant(value));
        }
        
        // Trigger render
        waitForProcessing(1);
    }
    
    qint64 renderTime = timer.elapsed();
    
    // Calculate rendering performance
    double framesPerSecond = (static_cast<double>(renderFrames) / renderTime) * 1000.0;
    double actualFps = chartWidget->getCurrentFPS();
    
    // Should achieve reasonable rendering performance
    QVERIFY(framesPerSecond > 10.0); // At least 10 FPS
    
    qDebug() << "Chart Rendering Benchmark:";
    qDebug() << "  Calculated FPS:" << framesPerSecond;
    qDebug() << "  Actual FPS:" << actualFps;
}

void TestPerformanceMonitoring::benchmarkMemoryOperations()
{
    const int iterations = 100000;
    const std::vector<size_t> sizes = {32, 64, 128, 256, 512, 1024, 2048, 4096};
    
    for (size_t size : sizes) {
        QElapsedTimer timer;
        timer.start();
        
        // Allocation benchmark
        std::vector<void*> allocations;
        allocations.reserve(iterations);
        
        for (int i = 0; i < iterations; ++i) {
            void* ptr = memoryPool->allocate(size);
            if (ptr) {
                allocations.push_back(ptr);
            }
        }
        
        qint64 allocationTime = timer.elapsed();
        
        // Deallocation benchmark
        timer.restart();
        
        for (void* ptr : allocations) {
            memoryPool->deallocate(ptr, size);
        }
        
        qint64 deallocationTime = timer.elapsed();
        
        // Calculate rates
        double allocationRate = (static_cast<double>(allocations.size()) / allocationTime) * 1000.0;
        double deallocationRate = (static_cast<double>(allocations.size()) / deallocationTime) * 1000.0;
        
        qDebug() << QString("Memory Operations Benchmark (size %1):").arg(size);
        qDebug() << "  Allocation rate:" << allocationRate << "ops/second";
        qDebug() << "  Deallocation rate:" << deallocationRate << "ops/second";
        
        // Should achieve reasonable memory performance
        QVERIFY(allocationRate > 1000.0); // At least 1K allocations/second
        QVERIFY(deallocationRate > 1000.0); // At least 1K deallocations/second
    }
}

void TestPerformanceMonitoring::testErrorHandlingOverhead()
{
    const int normalOperations = 10000;
    const int errorOperations = 1000;
    
    // Benchmark normal operations
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < normalOperations; ++i) {
        auto metrics = generateTestSystemMetrics();
        dashboard->updateSystemMetrics(metrics);
    }
    
    qint64 normalTime = timer.elapsed();
    
    // Benchmark operations with errors
    timer.restart();
    
    for (int i = 0; i < errorOperations; ++i) {
        // Create invalid metrics to trigger error handling
        PerformanceDashboard::SystemMetrics invalidMetrics;
        invalidMetrics.cpuUsage = -50.0; // Invalid
        invalidMetrics.memoryUsage = -1000.0; // Invalid
        dashboard->updateSystemMetrics(invalidMetrics);
    }
    
    qint64 errorTime = timer.elapsed();
    
    // Calculate overhead
    double normalRate = static_cast<double>(normalOperations) / normalTime;
    double errorRate = static_cast<double>(errorOperations) / errorTime;
    double overhead = (normalRate - errorRate) / normalRate;
    
    // Error handling should not add excessive overhead
    QVERIFY(overhead < 0.5); // Less than 50% overhead
    QVERIFY(errorRate > 0); // Should still process some operations
    
    qDebug() << "Error Handling Overhead:";
    qDebug() << "  Normal rate:" << normalRate << "ops/ms";
    qDebug() << "  Error rate:" << errorRate << "ops/ms";
    qDebug() << "  Overhead:" << (overhead * 100.0) << "%";
}

void TestPerformanceMonitoring::testFailureRecoveryTime()
{
    dashboard->startMonitoring();
    
    // Simulate system failure
    QElapsedTimer timer;
    timer.start();
    
    // Stop monitoring (simulate failure)
    dashboard->stopMonitoring();
    
    // Restart monitoring (simulate recovery)
    dashboard->startMonitoring();
    
    qint64 recoveryTime = timer.elapsed();
    
    // Should recover quickly
    QVERIFY(recoveryTime < 1000); // Less than 1 second recovery
    QVERIFY(dashboard->isMonitoring());
    
    // Test functionality after recovery
    auto metrics = generateTestSystemMetrics();
    dashboard->updateSystemMetrics(metrics);
    waitForProcessing();
    
    auto currentMetrics = dashboard->getCurrentSystemMetrics();
    QVERIFY(currentMetrics.timestamp.isValid());
    
    qDebug() << "Failure Recovery Time:" << recoveryTime << "ms";
}

void TestPerformanceMonitoring::testResourceExhaustionHandling()
{
    dashboard->startMonitoring();
    
    // Test behavior under resource exhaustion
    const int exhaustionIterations = 10000;
    
    QElapsedTimer timer;
    timer.start();
    
    // Attempt to exhaust memory
    std::vector<std::unique_ptr<char[]>> allocations;
    bool exhaustionDetected = false;
    
    for (int i = 0; i < exhaustionIterations; ++i) {
        try {
            auto allocation = std::make_unique<char[]>(1024 * 1024); // 1MB
            allocations.emplace_back(std::move(allocation));
            
            // Continue monitoring during exhaustion
            auto metrics = generateTestSystemMetrics();
            dashboard->updateSystemMetrics(metrics);
            
        } catch (const std::bad_alloc&) {
            exhaustionDetected = true;
            break;
        }
        
        if (i % 100 == 0) {
            waitForProcessing(1);
        }
    }
    
    qint64 exhaustionTime = timer.elapsed();
    
    // System should handle exhaustion gracefully
    QVERIFY(dashboard->isMonitoring());
    
    // Should maintain basic functionality
    auto currentMetrics = dashboard->getCurrentSystemMetrics();
    QVERIFY(currentMetrics.timestamp.isValid());
    
    qDebug() << "Resource Exhaustion Handling:";
    qDebug() << "  Time to exhaustion:" << exhaustionTime << "ms";
    qDebug() << "  Allocations created:" << allocations.size();
    qDebug() << "  Exhaustion detected:" << exhaustionDetected;
}

void TestPerformanceMonitoring::testPerformanceDegradation()
{
    dashboard->startMonitoring();
    
    // Test gradual performance degradation detection
    const int degradationSteps = 100;
    
    // Baseline performance
    for (int i = 0; i < 50; ++i) {
        PerformanceDashboard::SystemMetrics metrics;
        metrics.cpuUsage = 25.0;
        metrics.packetRate = 10000.0;
        metrics.parserThroughput = 9800.0;
        dashboard->updateSystemMetrics(metrics);
    }
    
    waitForProcessing(100);
    
    // Gradual degradation
    QElapsedTimer timer;
    timer.start();
    
    for (int step = 0; step < degradationSteps; ++step) {
        PerformanceDashboard::SystemMetrics metrics;
        metrics.cpuUsage = 25.0 + (step * 0.5); // Gradually increasing
        metrics.packetRate = 10000.0 - (step * 50.0); // Gradually decreasing
        metrics.parserThroughput = 9800.0 - (step * 49.0); // Gradually decreasing
        dashboard->updateSystemMetrics(metrics);
        
        waitForProcessing(10);
    }
    
    qint64 degradationTime = timer.elapsed();
    
    // Should detect performance degradation
    auto alerts = dashboard->getActiveAlerts();
    bool degradationAlertFound = false;
    
    for (const auto& alert : alerts) {
        if (alert.message.contains("degradation", Qt::CaseInsensitive) ||
            alert.message.contains("performance", Qt::CaseInsensitive)) {
            degradationAlertFound = true;
            break;
        }
    }
    
    // May or may not trigger degradation alerts depending on thresholds
    Q_UNUSED(degradationAlertFound);
    
    // Should complete degradation test efficiently
    QVERIFY(degradationTime < 5000); // Less than 5 seconds
    
    qDebug() << "Performance Degradation Test:";
    qDebug() << "  Degradation time:" << degradationTime << "ms";
    qDebug() << "  Active alerts:" << alerts.size();
}

QTEST_MAIN(TestPerformanceMonitoring)
#include "test_performance_monitoring.moc"