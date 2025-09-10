#include <QtTest/QTest>
#include <QtCore/QObject>
#include <QtTest/QSignalSpy>
#include <QtCore/QThread>
#include "../../src/profiling/profiler.h"

class TestProfiler : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // ProfileSample tests
    void testProfileSampleCreation();
    void testProfileSampleTiming();
    
    // ProfileStats tests
    void testProfileStatsAccumulation();
    void testProfileStatsReset();
    
    // Profiler core tests
    void testProfilerSingleton();
    void testBasicProfiling();
    void testNestedProfiling();
    void testScopeProfiler();
    void testProfilerState();
    
    // Performance tests
    void testProfilingOverhead();
    void testHighFrequencyProfiling();
    void testConcurrentProfiling();
    
    // Frame rate profiler tests
    void testFrameRateProfiler();
    void testFrameRateCalculation();
    
    // Memory profiler tests
    void testMemoryProfiler();
    void testMemorySnapshots();
    
    // Reporting tests
    void testReportGeneration();
    void testAutoReporting();

private:
    Monitor::Profiling::Profiler* m_profiler = nullptr;
    
    void simulateWork(int microseconds);
};

void TestProfiler::initTestCase()
{
    // Global setup
}

void TestProfiler::cleanupTestCase()
{
    // Global cleanup
}

void TestProfiler::init()
{
    m_profiler = Monitor::Profiling::Profiler::instance();
    m_profiler->resetStats();
    m_profiler->setEnabled(true);
}

void TestProfiler::cleanup()
{
    if (m_profiler) {
        m_profiler->resetStats();
    }
}

void TestProfiler::simulateWork(int microseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.nsecsElapsed() < microseconds * 1000) {
        // Busy wait
    }
}

void TestProfiler::testProfileSampleCreation()
{
    auto startTime = std::chrono::steady_clock::now();
    simulateWork(1000); // 1ms
    auto endTime = std::chrono::steady_clock::now();
    
    Monitor::Profiling::ProfileSample sample("TestSample", startTime, endTime);
    
    QCOMPARE(sample.name, QString("TestSample"));
    QVERIFY(sample.duration.count() > 0);
    QVERIFY(sample.durationMs() > 0.9 && sample.durationMs() < 2.0); // Should be around 1ms
    QVERIFY(sample.durationUs() > 900.0 && sample.durationUs() < 2000.0);
    QVERIFY(sample.durationNs() > 900000 && sample.durationNs() < 2000000);
    QVERIFY(sample.threadId != 0);
}

void TestProfiler::testProfileSampleTiming()
{
    const int expectedMicros = 2000; // 2ms
    
    auto startTime = std::chrono::steady_clock::now();
    simulateWork(expectedMicros);
    auto endTime = std::chrono::steady_clock::now();
    
    Monitor::Profiling::ProfileSample sample("TimingTest", startTime, endTime);
    
    double measuredMicros = sample.durationUs();
    
    // Should be within reasonable tolerance (±20%)
    QVERIFY(measuredMicros > expectedMicros * 0.8);
    QVERIFY(measuredMicros < expectedMicros * 1.2);
}

void TestProfiler::testProfileStatsAccumulation()
{
    Monitor::Profiling::ProfileStats stats;
    stats.name = "AccumulationTest";
    
    QCOMPARE(stats.callCount, qint64(0));
    QCOMPARE(stats.totalTime.count(), qint64(0));
    
    // Add some samples with known durations
    auto baseTime = std::chrono::steady_clock::now();
    
    Monitor::Profiling::ProfileSample sample1("Test", baseTime, baseTime + std::chrono::microseconds(1000));
    Monitor::Profiling::ProfileSample sample2("Test", baseTime, baseTime + std::chrono::microseconds(2000));
    Monitor::Profiling::ProfileSample sample3("Test", baseTime, baseTime + std::chrono::microseconds(3000));
    
    stats.addSample(sample1);
    QCOMPARE(stats.callCount, qint64(1));
    QVERIFY(stats.totalTimeUs() > 990.0 && stats.totalTimeUs() < 1010.0);
    QCOMPARE(stats.avgTimeUs(), stats.totalTimeUs());
    QCOMPARE(stats.minTimeUs(), stats.maxTimeUs());
    
    stats.addSample(sample2);
    stats.addSample(sample3);
    
    QCOMPARE(stats.callCount, qint64(3));
    QVERIFY(stats.totalTimeUs() > 5990.0 && stats.totalTimeUs() < 6010.0); // ~6000μs total
    QVERIFY(stats.avgTimeUs() > 1990.0 && stats.avgTimeUs() < 2010.0);     // ~2000μs average
    QVERIFY(stats.minTimeUs() > 990.0 && stats.minTimeUs() < 1010.0);      // ~1000μs min
    QVERIFY(stats.maxTimeUs() > 2990.0 && stats.maxTimeUs() < 3010.0);     // ~3000μs max
}

void TestProfiler::testProfileStatsReset()
{
    Monitor::Profiling::ProfileStats stats;
    stats.name = "ResetTest";
    
    // Add a sample
    auto baseTime = std::chrono::steady_clock::now();
    Monitor::Profiling::ProfileSample sample("Test", baseTime, baseTime + std::chrono::milliseconds(1));
    stats.addSample(sample);
    
    QVERIFY(stats.callCount > 0);
    QVERIFY(stats.totalTime.count() > 0);
    
    // Reset
    stats.reset();
    
    QCOMPARE(stats.callCount, qint64(0));
    QCOMPARE(stats.totalTime.count(), qint64(0));
    QCOMPARE(stats.avgTime.count(), qint64(0));
}

void TestProfiler::testProfilerSingleton()
{
    Monitor::Profiling::Profiler* profiler1 = Monitor::Profiling::Profiler::instance();
    Monitor::Profiling::Profiler* profiler2 = Monitor::Profiling::Profiler::instance();
    
    QVERIFY(profiler1 != nullptr);
    QVERIFY(profiler1 == profiler2); // Same instance
}

void TestProfiler::testBasicProfiling()
{
    QVERIFY(m_profiler->isEnabled());
    QCOMPARE(m_profiler->getTotalSamples(), qint64(0));
    
    // Test begin/end profiling
    m_profiler->beginProfile("BasicTest");
    simulateWork(1000); // 1ms
    m_profiler->endProfile("BasicTest");
    
    QCOMPARE(m_profiler->getTotalSamples(), qint64(1));
    
    auto stats = m_profiler->getStats("BasicTest");
    QCOMPARE(stats.callCount, qint64(1));
    QVERIFY(stats.totalTimeUs() > 900.0); // Should be around 1000μs
    
    QStringList profileNames = m_profiler->getProfileNames();
    QVERIFY(profileNames.contains("BasicTest"));
    
    // Test multiple calls
    m_profiler->beginProfile("BasicTest");
    simulateWork(500); // 0.5ms
    m_profiler->endProfile("BasicTest");
    
    stats = m_profiler->getStats("BasicTest");
    QCOMPARE(stats.callCount, qint64(2));
    QVERIFY(stats.totalTimeUs() > 1400.0); // Should be around 1500μs total
}

void TestProfiler::testNestedProfiling()
{
    m_profiler->beginProfile("Outer");
    simulateWork(500);
    
    m_profiler->beginProfile("Inner");
    simulateWork(300);
    m_profiler->endProfile("Inner");
    
    simulateWork(200);
    m_profiler->endProfile("Outer");
    
    auto outerStats = m_profiler->getStats("Outer");
    auto innerStats = m_profiler->getStats("Inner");
    
    QCOMPARE(outerStats.callCount, qint64(1));
    QCOMPARE(innerStats.callCount, qint64(1));
    
    // Outer should include all time (500 + 300 + 200 = 1000μs)
    QVERIFY(outerStats.totalTimeUs() > 900.0);
    
    // Inner should only include inner time (~300μs)
    QVERIFY(innerStats.totalTimeUs() > 250.0 && innerStats.totalTimeUs() < 400.0);
}

void TestProfiler::testScopeProfiler()
{
    {
        Monitor::Profiling::ScopedProfiler scoped("ScopeTest");
        simulateWork(1000);
        
        scoped.setMetadata("test_key", "test_value");
    } // Profiler should end here
    
    auto stats = m_profiler->getStats("ScopeTest");
    QCOMPARE(stats.callCount, qint64(1));
    QVERIFY(stats.totalTimeUs() > 900.0);
    
    // Test nested scoped profilers
    {
        Monitor::Profiling::ScopedProfiler outer("ScopeOuter");
        simulateWork(200);
        
        {
            Monitor::Profiling::ScopedProfiler inner("ScopeInner");
            simulateWork(300);
        }
        
        simulateWork(100);
    }
    
    auto outerStats = m_profiler->getStats("ScopeOuter");
    auto innerStats = m_profiler->getStats("ScopeInner");
    
    QCOMPARE(outerStats.callCount, qint64(1));
    QCOMPARE(innerStats.callCount, qint64(1));
    QVERIFY(outerStats.totalTimeUs() > innerStats.totalTimeUs());
}

void TestProfiler::testProfilerState()
{
    QVERIFY(m_profiler->isEnabled());
    
    // Disable profiler
    m_profiler->setEnabled(false);
    QVERIFY(!m_profiler->isEnabled());
    
    // Profiling calls should be ignored when disabled
    qint64 initialSamples = m_profiler->getTotalSamples();
    m_profiler->beginProfile("DisabledTest");
    simulateWork(1000);
    m_profiler->endProfile("DisabledTest");
    
    QCOMPARE(m_profiler->getTotalSamples(), initialSamples);
    
    auto stats = m_profiler->getStats("DisabledTest");
    QCOMPARE(stats.callCount, qint64(0));
    
    // Re-enable profiler
    m_profiler->setEnabled(true);
    QVERIFY(m_profiler->isEnabled());
    
    // Should work again
    m_profiler->beginProfile("EnabledTest");
    simulateWork(500);
    m_profiler->endProfile("EnabledTest");
    
    QVERIFY(m_profiler->getTotalSamples() > initialSamples);
}

void TestProfiler::testProfilingOverhead()
{
    const int numCalls = 10000;
    
    // Measure overhead of profiling calls
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < numCalls; ++i) {
        m_profiler->beginProfile("OverheadTest");
        m_profiler->endProfile("OverheadTest");
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerCall = static_cast<double>(elapsedNs) / numCalls;
    
    qDebug() << "Profiling overhead:" << nsPerCall << "ns per begin/end pair";
    
    // Overhead should be minimal - less than 1000ns per call
    QVERIFY(nsPerCall < 1000.0);
    
    auto stats = m_profiler->getStats("OverheadTest");
    QCOMPARE(stats.callCount, qint64(numCalls));
}

void TestProfiler::testHighFrequencyProfiling()
{
    const int numCalls = 1000;
    const int workTime = 10; // 10μs per call
    
    for (int i = 0; i < numCalls; ++i) {
        m_profiler->beginProfile("HighFreq");
        simulateWork(workTime);
        m_profiler->endProfile("HighFreq");
    }
    
    auto stats = m_profiler->getStats("HighFreq");
    QCOMPARE(stats.callCount, qint64(numCalls));
    
    // Average should be close to workTime
    QVERIFY(stats.avgTimeUs() > workTime * 0.5);
    QVERIFY(stats.avgTimeUs() < workTime * 2.0);
    
    // Total time should be approximately numCalls * workTime
    double expectedTotal = numCalls * workTime;
    QVERIFY(stats.totalTimeUs() > expectedTotal * 0.8);
    QVERIFY(stats.totalTimeUs() < expectedTotal * 1.5);
}

void TestProfiler::testConcurrentProfiling()
{
    const int numThreads = 4;
    const int callsPerThread = 1000;
    
    QVector<QThread*> threads;
    
    for (int i = 0; i < numThreads; ++i) {
        QThread* thread = QThread::create([this, i]() {
            QString profileName = QString("ConcurrentTest_%1").arg(i);
            const int callsPerThread = 1000;
            
            for (int j = 0; j < callsPerThread; ++j) {
                m_profiler->beginProfile(profileName);
                simulateWork(10); // 10μs
                m_profiler->endProfile(profileName);
            }
        });
        threads.append(thread);
    }
    
    // Start all threads
    for (QThread* thread : threads) {
        thread->start();
    }
    
    // Wait for completion
    for (QThread* thread : threads) {
        QVERIFY(thread->wait(10000));
        delete thread;
    }
    
    // Verify each thread's results
    for (int i = 0; i < numThreads; ++i) {
        QString profileName = QString("ConcurrentTest_%1").arg(i);
        auto stats = m_profiler->getStats(profileName);
        QCOMPARE(stats.callCount, qint64(callsPerThread));
    }
    
    QCOMPARE(m_profiler->getTotalSamples(), qint64(numThreads * callsPerThread));
}

void TestProfiler::testFrameRateProfiler()
{
    Monitor::Profiling::FrameRateProfiler frameProfiler("TestFPS");
    
    QCOMPARE(frameProfiler.getFrameCount(), qint64(0));
    QCOMPARE(frameProfiler.getCurrentFPS(), 0.0);
    QCOMPARE(frameProfiler.getAverageFPS(), 0.0);
    
    QSignalSpy frameCompletedSpy(&frameProfiler, &Monitor::Profiling::FrameRateProfiler::frameCompleted);
    QSignalSpy fpsUpdatedSpy(&frameProfiler, &Monitor::Profiling::FrameRateProfiler::fpsUpdated);
    
    // Simulate 60 FPS (16.67ms per frame)
    const int targetFPS = 60;
    const int frameTimeMs = 1000 / targetFPS;
    const int numFrames = 10;
    
    for (int i = 0; i < numFrames; ++i) {
        frameProfiler.frameStart();
        simulateWork(frameTimeMs * 1000); // Convert to microseconds
        frameProfiler.frameEnd();
    }
    
    QCOMPARE(frameProfiler.getFrameCount(), qint64(numFrames));
    QCOMPARE(frameCompletedSpy.count(), numFrames);
    QVERIFY(fpsUpdatedSpy.count() > 0);
    
    // FPS should be close to target
    double measuredFPS = frameProfiler.getAverageFPS();
    QVERIFY(measuredFPS > targetFPS * 0.8);
    QVERIFY(measuredFPS < targetFPS * 1.2);
    
    // Test reset
    frameProfiler.reset();
    QCOMPARE(frameProfiler.getFrameCount(), qint64(0));
    QCOMPARE(frameProfiler.getCurrentFPS(), 0.0);
}

void TestProfiler::testFrameRateCalculation()
{
    Monitor::Profiling::FrameRateProfiler frameProfiler("FPSCalc");
    
    // Test different frame rates
    struct FPSTest {
        int targetFPS;
        int numFrames;
    } tests[] = {
        {30, 20},
        {60, 30},
        {120, 25}
    };
    
    for (const auto& test : tests) {
        frameProfiler.reset();
        
        int frameTimeUs = (1000 * 1000) / test.targetFPS; // Microseconds per frame
        
        for (int i = 0; i < test.numFrames; ++i) {
            frameProfiler.frameStart();
            simulateWork(frameTimeUs);
            frameProfiler.frameEnd();
        }
        
        double measuredFPS = frameProfiler.getAverageFPS();
        qDebug() << "Target FPS:" << test.targetFPS << "Measured FPS:" << measuredFPS;
        
        // Should be within 10% of target
        QVERIFY(measuredFPS > test.targetFPS * 0.9);
        QVERIFY(measuredFPS < test.targetFPS * 1.1);
    }
}

void TestProfiler::testMemoryProfiler()
{
    Monitor::Profiling::MemoryProfiler memProfiler;
    
    QVERIFY(!memProfiler.isAutoSnapshotEnabled());
    QCOMPARE(memProfiler.getPeakHeapUsage(), qint64(0));
    QCOMPARE(memProfiler.getPeakVirtualUsage(), qint64(0));
    
    QSignalSpy snapshotSpy(&memProfiler, &Monitor::Profiling::MemoryProfiler::snapshotTaken);
    QSignalSpy peakSpy(&memProfiler, &Monitor::Profiling::MemoryProfiler::memoryPeakUpdated);
    
    // Take manual snapshot
    memProfiler.takeSnapshot();
    
    QCOMPARE(snapshotSpy.count(), 1);
    QVERIFY(peakSpy.count() >= 1);
    
    auto snapshots = memProfiler.getSnapshots();
    QCOMPARE(snapshots.size(), size_t(1));
    
    const auto& snapshot = snapshots[0];
    QVERIFY(!snapshot.timestamp.isNull());
    // Memory values depend on system and can't be easily predicted
    
    // Test current snapshot
    auto currentSnapshot = memProfiler.getCurrentSnapshot();
    QVERIFY(!currentSnapshot.timestamp.isNull());
    
    // Test auto snapshot
    memProfiler.setAutoSnapshot(true, 100); // 100ms interval
    QVERIFY(memProfiler.isAutoSnapshotEnabled());
    
    // Wait for a few auto snapshots
    QEventLoop loop;
    QTimer::singleShot(350, &loop, &QEventLoop::quit); // Wait 350ms
    loop.exec();
    
    snapshots = memProfiler.getSnapshots();
    QVERIFY(snapshots.size() > 1); // Should have taken additional snapshots
    
    memProfiler.setAutoSnapshot(false);
    QVERIFY(!memProfiler.isAutoSnapshotEnabled());
    
    // Test clear
    memProfiler.clearSnapshots();
    snapshots = memProfiler.getSnapshots();
    QCOMPARE(snapshots.size(), size_t(0));
}

void TestProfiler::testMemorySnapshots()
{
    Monitor::Profiling::MemoryProfiler memProfiler;
    
    // Take multiple snapshots
    const int numSnapshots = 5;
    
    for (int i = 0; i < numSnapshots; ++i) {
        memProfiler.takeSnapshot();
        QThread::msleep(10); // Small delay to ensure different timestamps
    }
    
    auto snapshots = memProfiler.getSnapshots();
    QCOMPARE(snapshots.size(), size_t(numSnapshots));
    
    // Verify timestamps are increasing
    for (size_t i = 1; i < snapshots.size(); ++i) {
        QVERIFY(snapshots[i].timestamp >= snapshots[i-1].timestamp);
    }
    
    // Test snapshot data integrity
    for (const auto& snapshot : snapshots) {
        QVERIFY(!snapshot.timestamp.isNull());
        // Memory values should be non-negative (though could be 0)
        QVERIFY(snapshot.heapAllocated >= 0);
        QVERIFY(snapshot.stackUsed >= 0);
        QVERIFY(snapshot.virtualMemory >= 0);
        QVERIFY(snapshot.residentMemory >= 0);
    }
}

void TestProfiler::testReportGeneration()
{
    // Create some profiling data
    m_profiler->beginProfile("Function1");
    simulateWork(2000); // 2ms
    m_profiler->endProfile("Function1");
    
    m_profiler->beginProfile("Function2");
    simulateWork(1000); // 1ms
    m_profiler->endProfile("Function2");
    
    m_profiler->beginProfile("Function1"); // Second call
    simulateWork(3000); // 3ms
    m_profiler->endProfile("Function1");
    
    QString report = m_profiler->generateReport();
    
    QVERIFY(!report.isEmpty());
    QVERIFY(report.contains("Performance Profile Report"));
    QVERIFY(report.contains("Function1"));
    QVERIFY(report.contains("Function2"));
    QVERIFY(report.contains("Total samples"));
    
    // Should show Function1 with 2 calls, Function2 with 1 call
    QStringList lines = report.split('\n');
    
    bool foundFunction1 = false;
    bool foundFunction2 = false;
    
    for (const QString& line : lines) {
        if (line.contains("Function1")) {
            foundFunction1 = true;
            // Should show 2 calls for Function1
        } else if (line.contains("Function2")) {
            foundFunction2 = true;
            // Should show 1 call for Function2
        }
    }
    
    QVERIFY(foundFunction1);
    QVERIFY(foundFunction2);
    
    // Test dumping report (should not crash)
    m_profiler->dumpReport();
}

void TestProfiler::testAutoReporting()
{
    QSignalSpy reportSpy(m_profiler, &Monitor::Profiling::Profiler::reportGenerated);
    
    // Enable auto-reporting with short interval
    m_profiler->setAutoReport(true, 200); // 200ms interval
    QVERIFY(m_profiler->isAutoReportEnabled());
    
    // Add some profiling data
    m_profiler->beginProfile("AutoReportTest");
    simulateWork(100);
    m_profiler->endProfile("AutoReportTest");
    
    // Wait for auto-report
    QEventLoop loop;
    QTimer::singleShot(400, &loop, &QEventLoop::quit);
    loop.exec();
    
    QVERIFY(reportSpy.count() > 0);
    
    // Disable auto-reporting
    m_profiler->setAutoReport(false);
    QVERIFY(!m_profiler->isAutoReportEnabled());
    
    int previousCount = reportSpy.count();
    
    // Wait a bit more - should not get additional reports
    QTimer::singleShot(300, &loop, &QEventLoop::quit);
    loop.exec();
    
    QCOMPARE(reportSpy.count(), previousCount);
}

QTEST_MAIN(TestProfiler)
#include "test_profiler.moc"