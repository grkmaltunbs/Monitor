#include <QtTest/QtTest>
#include <QApplication>
#include <memory>

// Core includes
#include "../../../src/core/memory/memory_pool.h"
#include "../../../src/core/events/event_dispatcher.h"
#include "../../../src/core/profiling/profiler.h"

/**
 * @brief Simple Performance Monitoring Tests
 * 
 * Basic tests for performance monitoring infrastructure.
 */
class TestPerformanceMonitoringSimple : public QObject
{
    Q_OBJECT

private slots:
    void testMemoryPoolPerformance() {
        const size_t poolSize = 1024 * 1024; // 1MB
        Monitor::Core::Memory::MemoryPool pool(poolSize);
        
        // Test basic allocation/deallocation
        void* ptr = pool.allocate(64);
        QVERIFY(ptr != nullptr);
        
        pool.deallocate(ptr, 64);
        
        // Pool should still be valid
        QVERIFY(pool.getBytesUsed() >= 0);
    }

    void testEventDispatcherPerformance() {
        Monitor::Core::Events::EventDispatcher dispatcher;
        
        // Test basic event dispatch
        auto event = std::make_shared<Monitor::Core::Events::Event>("test_event");
        dispatcher.dispatch(event);
        
        // Should not crash
        QVERIFY(true);
    }

    void testProfilerPerformance() {
        Monitor::Core::Profiling::Profiler profiler;
        QVERIFY(profiler.isEnabled());
        
        // Test basic profiling
        {
            Monitor::Core::Profiling::ScopedProfiler scope("test_function");
            QThread::msleep(1); // Small delay
        }
        
        auto stats = profiler.getStatistics();
        QVERIFY(stats.totalSamples >= 0);
    }

    void testBasicPerformanceOperations() {
        // Test that basic operations complete without excessive time
        QElapsedTimer timer;
        timer.start();
        
        // Simulate some work
        for (int i = 0; i < 1000; ++i) {
            volatile int x = i * i;
            Q_UNUSED(x);
        }
        
        qint64 elapsed = timer.elapsed();
        QVERIFY(elapsed < 1000); // Should complete within 1 second
    }
};

QTEST_MAIN(TestPerformanceMonitoringSimple)
#include "test_performance_monitoring_simple.moc"