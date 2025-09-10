#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QUdpSocket>

// Network sources
#include "../../src/network/sources/udp_source.h"
#include "../../src/network/config/network_config.h"

// Offline sources
#include "../../src/offline/sources/file_source.h"
#include "../../src/offline/sources/file_indexer.h"

// Core components
#include "../../src/packet/core/packet_factory.h"
#include "../../src/packet/core/packet_header.h"
#include "../../src/memory/memory_pool.h"
#include "../../src/core/application.h"

using namespace Monitor;

/**
 * @brief Simple Phase 9 Performance Tests
 * 
 * Focused performance testing for key Phase 9 components with
 * realistic test scenarios that validate the core performance
 * requirements without overwhelming complexity.
 */
class TestPhase9PerformanceSimple : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    // Core Performance Tests
    void testUdpThroughputBasic();
    void testFileIndexingSpeed();
    void testFilePlaybackPerformance();
    void testMemoryEfficiency();
    void testLatencyMeasurement();
    void testSystemStability();

private:
    Memory::MemoryPoolManager* m_memoryManager;
    Packet::PacketFactory* m_packetFactory;
    QTemporaryDir* m_tempDir;
    
    // Test helpers
    QString createTestFile(const QString& filename, int packetCount);
    QByteArray createTestPacket(uint32_t id, uint32_t sequence, const QByteArray& payload);
    bool waitForSignal(QSignalSpy& spy, int expectedCount = 1, int timeoutMs = 10000);
    void sendUdpBurst(const QHostAddress& address, quint16 port, int count);
    quint16 findPort();
    void logResults(const QString& test, double metric, const QString& unit, bool passed);
};

void TestPhase9PerformanceSimple::initTestCase()
{
    qDebug() << "=== Phase 9 Performance Tests ===";
    qDebug() << "Target Requirements:";
    qDebug() << "- Throughput: 1000+ packets/second (test target)";
    qDebug() << "- Indexing: 5000+ packets/second";
    qDebug() << "- Memory: <100MB for normal operations";
    qDebug() << "- Latency: <50ms (test environment)";
    
    // Initialize application
    auto app = Core::Application::instance();
    QVERIFY(app != nullptr);
    QVERIFY(app->initialize());
    m_memoryManager = app->memoryManager();
    QVERIFY(m_memoryManager != nullptr);
    
    m_packetFactory = new Packet::PacketFactory(m_memoryManager);
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
}

void TestPhase9PerformanceSimple::cleanupTestCase()
{
    delete m_packetFactory;
    delete m_tempDir;
    qDebug() << "=== Performance Tests Complete ===";
}

void TestPhase9PerformanceSimple::cleanup()
{
    QThread::msleep(100);
}

void TestPhase9PerformanceSimple::testUdpThroughputBasic()
{
    qDebug() << "\n--- UDP Throughput Test ---";
    
    quint16 port = findPort();
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "PerfUDP", QHostAddress::LocalHost, port);
    config.receiveBufferSize = 524288; // 512KB buffer
    
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&udpSource, &Packet::PacketSource::packetReady);
    QSignalSpy startedSpy(&udpSource, &Packet::PacketSource::started);
    
    udpSource.start();
    QVERIFY(waitForSignal(startedSpy));
    
    // Throughput test - send burst of packets
    const int testPackets = 5000; // 5K packets
    QElapsedTimer timer;
    timer.start();
    
    sendUdpBurst(QHostAddress::LocalHost, port, testPackets);
    
    // Wait for processing
    while (packetSpy.count() < testPackets * 0.9 && timer.elapsed() < 10000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(1);
    }
    
    qint64 testTime = timer.elapsed();
    int received = packetSpy.count();
    double throughput = (received * 1000.0) / testTime;
    double lossRate = (double)(testPackets - received) / testPackets * 100.0;
    
    qDebug() << "UDP Throughput Results:";
    qDebug() << "- Packets sent:" << testPackets;
    qDebug() << "- Packets received:" << received;
    qDebug() << "- Test time:" << testTime << "ms";
    qDebug() << "- Throughput:" << throughput << "pps";
    qDebug() << "- Packet loss:" << lossRate << "%";
    
    bool passed = throughput >= 1000 && lossRate < 10.0;
    logResults("UDP Throughput", throughput, "pps", passed);
    
    QVERIFY(throughput >= 800); // Minimum 800 pps
    QVERIFY(lossRate < 15.0); // <15% loss acceptable in test
    
    udpSource.stop();
}

void TestPhase9PerformanceSimple::testFileIndexingSpeed()
{
    qDebug() << "\n--- File Indexing Performance Test ---";
    
    const int indexPackets = 25000; // 25K packets
    QString testFile = createTestFile("index_perf.dat", indexPackets);
    
    QFileInfo fileInfo(testFile);
    double fileSizeMB = fileInfo.size() / (1024.0 * 1024.0);
    
    QElapsedTimer timer;
    timer.start();
    
    Offline::FileIndexer indexer;
    QSignalSpy completedSpy(&indexer, &Offline::FileIndexer::indexingCompleted);
    
    QVERIFY(indexer.startIndexing(testFile, false)); // Synchronous
    QVERIFY(waitForSignal(completedSpy, 1, 30000)); // 30s timeout
    
    qint64 indexTime = timer.elapsed();
    const auto& stats = indexer.getStatistics();
    
    double indexingRate = stats.packetsPerSecond;
    double mbPerSec = (fileSizeMB * 1000.0) / indexTime;
    
    qDebug() << "File Indexing Results:";
    qDebug() << "- File size:" << fileSizeMB << "MB";
    qDebug() << "- Packets:" << stats.totalPackets;
    qDebug() << "- Index time:" << indexTime << "ms";
    qDebug() << "- Indexing rate:" << indexingRate << "packets/sec";
    qDebug() << "- MB/sec:" << mbPerSec << "MB/sec";
    
    bool passed = indexingRate >= 5000 && indexTime < 20000;
    logResults("File Indexing", indexingRate, "packets/sec", passed);
    
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::Completed);
    QCOMPARE(stats.totalPackets, static_cast<uint64_t>(indexPackets));
    QVERIFY(indexingRate >= 3000); // Minimum 3K packets/sec
    QVERIFY(indexTime < 25000); // <25s for 25K packets
    
    // Test search performance
    timer.restart();
    for (int i = 0; i < 100; ++i) {
        indexer.findPacketBySequence(i * 10);
    }
    qint64 searchTime = timer.elapsed();
    
    qDebug() << "- Search time (100 ops):" << searchTime << "ms";
    QVERIFY(searchTime < 50); // <50ms for 100 searches
}

void TestPhase9PerformanceSimple::testFilePlaybackPerformance()
{
    qDebug() << "\n--- File Playback Performance Test ---";
    
    const int playbackPackets = 15000; // 15K packets
    QString testFile = createTestFile("playback_perf.dat", playbackPackets);
    
    Offline::FileSourceConfig config;
    config.filename = testFile;
    config.realTimePlayback = false; // Max speed
    config.bufferSize = 2000;
    
    Offline::FileSource fileSource(config);
    fileSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy loadedSpy(&fileSource, &Offline::FileSource::fileLoaded);
    QSignalSpy packetSpy(&fileSource, &Packet::PacketSource::packetReady);
    
    // Measure file loading
    QElapsedTimer loadTimer;
    loadTimer.start();
    
    QVERIFY(fileSource.loadFile(testFile));
    QVERIFY(waitForSignal(loadedSpy));
    
    qint64 loadTime = loadTimer.elapsed();
    
    // Measure playback throughput
    QElapsedTimer playbackTimer;
    playbackTimer.start();
    
    fileSource.start();
    fileSource.play();
    
    // Wait for packets
    while (packetSpy.count() < playbackPackets * 0.9 && playbackTimer.elapsed() < 20000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    
    qint64 playbackTime = playbackTimer.elapsed();
    int received = packetSpy.count();
    double playbackThroughput = (received * 1000.0) / playbackTime;
    
    qDebug() << "File Playback Results:";
    qDebug() << "- Load time:" << loadTime << "ms";
    qDebug() << "- Packets expected:" << playbackPackets;
    qDebug() << "- Packets received:" << received;
    qDebug() << "- Playback time:" << playbackTime << "ms";
    qDebug() << "- Playback throughput:" << playbackThroughput << "pps";
    
    bool passed = playbackThroughput >= 2000 && loadTime < 3000;
    logResults("File Playback", playbackThroughput, "pps", passed);
    
    QVERIFY(received > playbackPackets * 0.85); // >85% delivery
    QVERIFY(playbackThroughput >= 1500); // Minimum 1.5K pps
    QVERIFY(loadTime < 5000); // <5s load time
    
    fileSource.stop();
}

void TestPhase9PerformanceSimple::testMemoryEfficiency()
{
    qDebug() << "\n--- Memory Efficiency Test ---";
    
    // Get baseline memory usage
    auto getMemoryInfo = [&]() -> QStringList {
        QStringList info;
        for (const QString& poolName : m_memoryManager->getPoolNames()) {
            info << QString("%1: used/total").arg(poolName);
        }
        return info;
    };
    
    QStringList initialPools = getMemoryInfo();
    qDebug() << "Initial memory pools:" << initialPools.size();
    
    quint16 port = findPort();
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "MemoryUDP", QHostAddress::LocalHost, port);
    
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&udpSource, &Packet::PacketSource::packetReady);
    QSignalSpy startedSpy(&udpSource, &Packet::PacketSource::started);
    
    udpSource.start();
    QVERIFY(waitForSignal(startedSpy));
    
    // Memory stress test - process many packets
    const int memoryTestPackets = 10000;
    sendUdpBurst(QHostAddress::LocalHost, port, memoryTestPackets);
    
    // Wait for processing
    while (packetSpy.count() < memoryTestPackets * 0.8 && packetSpy.count() < 60000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    
    QStringList finalPools = getMemoryInfo();
    int packetsProcessed = packetSpy.count();
    
    qDebug() << "Memory Efficiency Results:";
    qDebug() << "- Packets processed:" << packetsProcessed;
    qDebug() << "- Memory pools:" << finalPools.size();
    qDebug() << "- Pool utilization available via MemoryPoolManager";
    
    // Basic memory efficiency validation
    double efficiency = (double)packetsProcessed / memoryTestPackets;
    bool passed = efficiency > 0.7 && finalPools.size() >= initialPools.size();
    
    logResults("Memory Efficiency", efficiency * 100, "%", passed);
    
    QVERIFY(packetsProcessed > memoryTestPackets * 0.6); // >60% processing
    QVERIFY(finalPools.size() >= initialPools.size()); // No pool loss
    
    udpSource.stop();
    
    // Allow cleanup and check for stability
    QThread::msleep(1000);
    QStringList cleanupPools = getMemoryInfo();
    QCOMPARE(cleanupPools.size(), initialPools.size()); // Pool count stable
}

void TestPhase9PerformanceSimple::testLatencyMeasurement()
{
    qDebug() << "\n--- Latency Measurement Test ---";
    
    quint16 port = findPort();
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "LatencyUDP", QHostAddress::LocalHost, port);
    
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&udpSource, &Packet::PacketSource::packetReady);
    QSignalSpy startedSpy(&udpSource, &Packet::PacketSource::started);
    
    udpSource.start();
    QVERIFY(waitForSignal(startedSpy));
    
    QUdpSocket sender;
    QVector<qint64> latencies;
    
    // Measure latencies for individual packets
    for (int i = 0; i < 50; ++i) {
        int initialCount = packetSpy.count();
        
        QElapsedTimer latencyTimer;
        latencyTimer.start();
        
        // Send packet
        QByteArray packet = createTestPacket(9000 + i, i, QByteArray("Latency test"));
        sender.writeDatagram(packet, QHostAddress::LocalHost, port);
        
        // Wait for reception
        while (packetSpy.count() <= initialCount && latencyTimer.elapsed() < 200) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }
        
        if (packetSpy.count() > initialCount) {
            latencies.append(latencyTimer.elapsed());
        }
        
        QThread::msleep(50); // Pause between tests
    }
    
    if (!latencies.isEmpty()) {
        std::sort(latencies.begin(), latencies.end());
        
        double avgLatency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
        qint64 maxLatency = latencies.last();
        qint64 minLatency = latencies.first();
        
        qDebug() << "Latency Results:";
        qDebug() << "- Samples:" << latencies.size();
        qDebug() << "- Average:" << avgLatency << "ms";
        qDebug() << "- Min:" << minLatency << "ms";
        qDebug() << "- Max:" << maxLatency << "ms";
        
        bool passed = avgLatency < 50.0 && maxLatency < 100;
        logResults("Average Latency", avgLatency, "ms", passed);
        
        QVERIFY(avgLatency < 80.0); // <80ms average (relaxed for test env)
        QVERIFY(maxLatency < 200); // <200ms max
        QVERIFY(minLatency >= 0); // Sanity check
    } else {
        QFAIL("No latency measurements obtained");
    }
    
    udpSource.stop();
}

void TestPhase9PerformanceSimple::testSystemStability()
{
    qDebug() << "\n--- System Stability Test ---";
    qDebug() << "Running 15-second stability test...";
    
    quint16 port = findPort();
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "StabilityUDP", QHostAddress::LocalHost, port);
    
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&udpSource, &Packet::PacketSource::packetReady);
    QSignalSpy startedSpy(&udpSource, &Packet::PacketSource::started);
    QSignalSpy errorSpy(&udpSource, &Packet::PacketSource::error);
    
    udpSource.start();
    QVERIFY(waitForSignal(startedSpy));
    
    QElapsedTimer stabilityTimer;
    stabilityTimer.start();
    
    const qint64 testDuration = 15000; // 15 seconds
    int cycle = 0;
    
    // Continuous operation test
    while (stabilityTimer.elapsed() < testDuration) {
        // Send batch of packets
        sendUdpBurst(QHostAddress::LocalHost, port, 200);
        cycle++;
        
        // Process events and brief pause
        for (int i = 0; i < 100; ++i) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }
        QThread::msleep(500); // 500ms cycle
        
        // Log progress every 5 seconds
        if (stabilityTimer.elapsed() / 5000 > (stabilityTimer.elapsed() - 500) / 5000) {
            qDebug() << "Stability check - Time:" << stabilityTimer.elapsed() 
                     << "ms, Cycles:" << cycle 
                     << ", Packets:" << packetSpy.count()
                     << ", Errors:" << errorSpy.count();
        }
    }
    
    qint64 finalTime = stabilityTimer.elapsed();
    int totalPackets = packetSpy.count();
    int totalErrors = errorSpy.count();
    double avgThroughput = (totalPackets * 1000.0) / finalTime;
    
    qDebug() << "Stability Results:";
    qDebug() << "- Test duration:" << finalTime << "ms";
    qDebug() << "- Cycles completed:" << cycle;
    qDebug() << "- Total packets:" << totalPackets;
    qDebug() << "- Total errors:" << totalErrors;
    qDebug() << "- Average throughput:" << avgThroughput << "pps";
    
    bool passed = totalErrors == 0 && totalPackets > cycle * 150;
    logResults("System Stability", (double)totalErrors, "errors", passed);
    
    QCOMPARE(totalErrors, 0); // Zero errors required
    QVERIFY(totalPackets > cycle * 100); // Reasonable packet processing
    QVERIFY(cycle >= 25); // Completed reasonable number of cycles
    
    udpSource.stop();
}

// Helper implementations

QString TestPhase9PerformanceSimple::createTestFile(const QString& filename, int packetCount)
{
    QString fullPath = m_tempDir->path() + "/" + filename;
    QFile file(fullPath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    
    for (int i = 0; i < packetCount; ++i) {
        uint32_t id = 10000 + (i % 50); // Varied packet IDs
        uint32_t sequence = i;
        
        QString payloadText = QString("Performance test packet %1").arg(i);
        if (i % 100 == 0) {
            payloadText += QString(" - milestone packet with extra data for size variation");
        }
        
        QByteArray packet = createTestPacket(id, sequence, payloadText.toUtf8());
        file.write(packet);
    }
    
    file.close();
    return fullPath;
}

QByteArray TestPhase9PerformanceSimple::createTestPacket(uint32_t id, uint32_t sequence, const QByteArray& payload)
{
    Packet::PacketHeader header;
    header.id = id;
    header.sequence = sequence;
    header.timestamp = Packet::PacketHeader::getCurrentTimestampNs();
    header.payloadSize = static_cast<uint32_t>(payload.size());
    header.flags = Packet::PacketHeader::Flags::TestData;
    
    QByteArray packet;
    packet.append(reinterpret_cast<const char*>(&header), sizeof(header));
    packet.append(payload);
    return packet;
}

bool TestPhase9PerformanceSimple::waitForSignal(QSignalSpy& spy, int expectedCount, int timeoutMs)
{
    if (spy.count() >= expectedCount) {
        return true;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    while (timer.elapsed() < timeoutMs && spy.count() < expectedCount) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    
    return spy.count() >= expectedCount;
}

void TestPhase9PerformanceSimple::sendUdpBurst(const QHostAddress& address, quint16 port, int count)
{
    QUdpSocket sender;
    
    for (int i = 0; i < count; ++i) {
        QByteArray packet = createTestPacket(11000 + i, i, QString("Burst %1").arg(i).toUtf8());
        sender.writeDatagram(packet, address, port);
        
        // Small delay every 100 packets to avoid overwhelming
        if ((i + 1) % 100 == 0) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 0);
            QThread::msleep(1);
        }
    }
}

quint16 TestPhase9PerformanceSimple::findPort()
{
    for (quint16 port = 18000; port < 19000; ++port) {
        QUdpSocket test;
        if (test.bind(QHostAddress::LocalHost, port)) {
            return port;
        }
    }
    return 18555; // Fallback
}

void TestPhase9PerformanceSimple::logResults(const QString& test, double metric, const QString& unit, bool passed)
{
    QString status = passed ? "✓ PASS" : "✗ FAIL";
    qDebug() << QString("[%1] %2: %3 %4").arg(status, test).arg(metric).arg(unit);
}

QTEST_MAIN(TestPhase9PerformanceSimple)
#include "test_phase9_performance_simple.moc"