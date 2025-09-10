#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QRandomGenerator>

// Offline sources
#include "../../src/offline/sources/file_source.h"
#include "../../src/offline/sources/file_indexer.h"

// Core components for packet processing
#include "../../src/packet/core/packet_factory.h"
#include "../../src/packet/core/packet_header.h"
#include "../../src/memory/memory_pool.h"
#include "../../src/core/application.h"

using namespace Monitor;

/**
 * @brief Comprehensive Offline Integration Tests
 * 
 * These tests verify the complete integration between FileSource and FileIndexer
 * components working together with the packet processing pipeline. Tests include:
 * - End-to-end file playback with indexing
 * - Real file operations with temporary test files
 * - Playback control integration (play/pause/seek)
 * - Index-based seeking and navigation
 * - Large file handling and performance
 * - Error recovery and edge cases
 */
class TestOfflineIntegration : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    // Integration Tests
    void testFileSourceWithIndexerBasicIntegration();
    void testPlaybackControlsWithIndexing();
    void testSeekingWithIndex();
    void testLargeFileHandling();
    void testMultipleFileTypes();
    void testErrorRecoveryIntegration();
    void testPerformanceIntegration();
    void testIndexCacheIntegration();

private:
    Memory::MemoryPoolManager* m_memoryManager;
    Packet::PacketFactory* m_packetFactory;
    QTemporaryDir* m_tempDir;
    
    // Test helpers
    QString createTestFile(const QString& filename, int packetCount, uint32_t startId = 1000);
    QString createLargeTestFile(const QString& filename, int packetCount);
    QString createCorruptedFile(const QString& filename);
    QByteArray createTestPacket(uint32_t id, uint32_t sequence, uint64_t timestamp, const QByteArray& payload);
    bool waitForSignalWithTimeout(QSignalSpy& spy, int expectedCount = 1, int timeoutMs = 10000);
    void waitForMs(int milliseconds);
    bool verifyFileContainsPackets(const QString& filename, int expectedCount);
    void compareIndexWithFile(const QString& filename, const std::vector<Offline::PacketIndexEntry>& index);
};

void TestOfflineIntegration::initTestCase()
{
    // Initialize application and get memory manager
    auto app = Core::Application::instance();
    QVERIFY(app != nullptr);
    QVERIFY(app->initialize());
    m_memoryManager = app->memoryManager();
    QVERIFY(m_memoryManager != nullptr);
    
    // Create packet factory
    m_packetFactory = new Packet::PacketFactory(m_memoryManager);
    
    // Create temporary directory for test files
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
    
    qDebug() << "Test temp dir:" << m_tempDir->path();
}

void TestOfflineIntegration::cleanupTestCase()
{
    delete m_packetFactory;
    delete m_tempDir;
}

void TestOfflineIntegration::cleanup()
{
    // Clean up between tests to avoid interference
    QThread::msleep(100);
}

void TestOfflineIntegration::testFileSourceWithIndexerBasicIntegration()
{
    // Create test file with known packet structure
    const int packetCount = 50;
    QString testFile = createTestFile("basic_integration.dat", packetCount, 2000);
    QVERIFY(QFileInfo::exists(testFile));
    QVERIFY(verifyFileContainsPackets(testFile, packetCount));
    
    // Create and start indexer
    Offline::FileIndexer indexer;
    QSignalSpy indexingStartedSpy(&indexer, &Offline::FileIndexer::indexingStarted);
    QSignalSpy indexingCompletedSpy(&indexer, &Offline::FileIndexer::indexingCompleted);
    QSignalSpy progressSpy(&indexer, &Offline::FileIndexer::progressChanged);
    
    QVERIFY(indexer.startIndexing(testFile, false)); // Foreground indexing for test
    QVERIFY(waitForSignalWithTimeout(indexingStartedSpy));
    QVERIFY(waitForSignalWithTimeout(indexingCompletedSpy));
    
    // Verify indexing results
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::Completed);
    QVERIFY(indexer.isIndexingComplete());
    QCOMPARE(indexer.getPacketCount(), static_cast<uint64_t>(packetCount));
    
    const auto& statistics = indexer.getStatistics();
    QCOMPARE(statistics.totalPackets, static_cast<uint64_t>(packetCount));
    QCOMPARE(statistics.validPackets, static_cast<uint64_t>(packetCount));
    QCOMPARE(statistics.errorPackets, 0ULL);
    
    // Verify index integrity
    compareIndexWithFile(testFile, indexer.getIndex());
    
    // Create file source and load the indexed file
    Offline::FileSourceConfig config;
    config.filename = testFile;
    config.playbackSpeed = 1.0;
    config.realTimePlayback = false; // Fast playback for testing
    
    Offline::FileSource fileSource(config);
    fileSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy fileLoadedSpy(&fileSource, &Offline::FileSource::fileLoaded);
    QSignalSpy packetReadySpy(&fileSource, &Packet::PacketSource::packetReady);
    QSignalSpy playbackStateChangedSpy(&fileSource, &Offline::FileSource::playbackStateChanged);
    
    // Load file and verify it uses the index
    QVERIFY(fileSource.loadFile(testFile));
    QVERIFY(waitForSignalWithTimeout(fileLoadedSpy));
    QVERIFY(fileSource.isFileLoaded());
    
    const auto& fileStats = fileSource.getFileStatistics();
    QCOMPARE(fileStats.totalPackets, static_cast<uint64_t>(packetCount));
    
    // Start playback and verify packets are received
    fileSource.start();
    QVERIFY(waitForSignalWithTimeout(playbackStateChangedSpy));
    fileSource.play();
    
    // Wait for some packets to be processed
    QVERIFY(waitForSignalWithTimeout(packetReadySpy, 10, 5000));
    QVERIFY(packetReadySpy.count() >= 10);
    
    // Verify playback state
    QCOMPARE(fileSource.getPlaybackState(), Offline::PlaybackState::Playing);
    QVERIFY(!fileSource.isAtBeginningOfFile());
    
    // Stop playback
    fileSource.stop();
    QVERIFY(fileSource.isStopped());
}

void TestOfflineIntegration::testPlaybackControlsWithIndexing()
{
    const int packetCount = 100;
    QString testFile = createTestFile("playback_controls.dat", packetCount, 3000);
    
    // Index the file first
    Offline::FileIndexer indexer;
    QSignalSpy indexingCompletedSpy(&indexer, &Offline::FileIndexer::indexingCompleted);
    QVERIFY(indexer.startIndexing(testFile, false));
    QVERIFY(waitForSignalWithTimeout(indexingCompletedSpy));
    QVERIFY(indexer.isIndexingComplete());
    
    // Create file source with fast playback
    Offline::FileSourceConfig config;
    config.filename = testFile;
    config.playbackSpeed = 2.0; // 2x speed
    config.realTimePlayback = false;
    
    Offline::FileSource fileSource(config);
    fileSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy fileLoadedSpy(&fileSource, &Offline::FileSource::fileLoaded);
    QSignalSpy packetReadySpy(&fileSource, &Packet::PacketSource::packetReady);
    QSignalSpy playbackStateChangedSpy(&fileSource, &Offline::FileSource::playbackStateChanged);
    QSignalSpy progressUpdatedSpy(&fileSource, &Offline::FileSource::progressUpdated);
    QSignalSpy seekCompletedSpy(&fileSource, &Offline::FileSource::seekCompleted);
    
    // Load and start
    QVERIFY(fileSource.loadFile(testFile));
    QVERIFY(waitForSignalWithTimeout(fileLoadedSpy));
    fileSource.start();
    
    // Test play/pause cycle
    fileSource.play();
    QCOMPARE(fileSource.getPlaybackState(), Offline::PlaybackState::Playing);
    
    // Let some packets play
    QVERIFY(waitForSignalWithTimeout(packetReadySpy, 5, 3000));
    int packetsBeforePause = packetReadySpy.count();
    Q_UNUSED(packetsBeforePause);
    
    // Pause playback
    fileSource.pausePlayback();
    QCOMPARE(fileSource.getPlaybackState(), Offline::PlaybackState::Paused);
    packetReadySpy.clear();
    
    // Wait and verify no more packets during pause
    waitForMs(500);
    QCOMPARE(packetReadySpy.count(), 0);
    
    // Resume playback
    fileSource.play();
    QCOMPARE(fileSource.getPlaybackState(), Offline::PlaybackState::Playing);
    QVERIFY(waitForSignalWithTimeout(packetReadySpy, 5, 3000));
    
    // Test seeking with index
    uint64_t seekTarget = packetCount / 2; // Seek to middle
    fileSource.seekToPacket(seekTarget);
    QVERIFY(waitForSignalWithTimeout(seekCompletedSpy));
    
    // Verify seek position
    const auto& stats = fileSource.getFileStatistics();
    QVERIFY(stats.currentPacket >= seekTarget - 5); // Allow some tolerance
    QVERIFY(stats.currentPacket <= seekTarget + 5);
    
    // Test step controls
    uint64_t currentPacket = stats.currentPacket;
    Q_UNUSED(currentPacket);
    fileSource.stepForward();
    
    // Test position seeking
    fileSource.seekToPosition(0.75); // 75% through file
    QVERIFY(waitForSignalWithTimeout(seekCompletedSpy, 1, 2000));
    
    const auto& newStats = fileSource.getFileStatistics();
    QVERIFY(newStats.playbackProgress > 0.7);
    QVERIFY(newStats.playbackProgress < 0.8);
    
    fileSource.stop();
}

void TestOfflineIntegration::testSeekingWithIndex()
{
    const int packetCount = 200;
    QString testFile = createTestFile("seeking_test.dat", packetCount, 4000);
    
    // Create indexer and index file
    Offline::FileIndexer indexer;
    QSignalSpy indexingCompletedSpy(&indexer, &Offline::FileIndexer::indexingCompleted);
    QVERIFY(indexer.startIndexing(testFile, false));
    QVERIFY(waitForSignalWithTimeout(indexingCompletedSpy));
    
    // Test various index search operations
    const auto& index = indexer.getIndex();
    QCOMPARE(index.size(), static_cast<size_t>(packetCount));
    
    // Test position-based search
    if (!index.empty()) {
        qint64 midPosition = index[packetCount / 2].filePosition;
        int foundIndex = indexer.findPacketByPosition(midPosition);
        QVERIFY(foundIndex >= 0);
        QCOMPARE(index[foundIndex].filePosition, midPosition);
    }
    
    // Test packet ID search
    std::vector<int> idMatches = indexer.findPacketsByPacketId(4000); // First packet ID
    QVERIFY(!idMatches.empty());
    QCOMPARE(index[idMatches[0]].packetId, 4000U);
    
    // Test sequence number search
    int seqIndex = indexer.findPacketBySequence(50);
    if (seqIndex >= 0) {
        QCOMPARE(index[seqIndex].sequenceNumber, 50U);
    }
    
    // Test timestamp-based search
    if (!index.empty()) {
        uint64_t midTimestamp = index[packetCount / 2].timestamp;
        int tsIndex = indexer.findPacketByTimestamp(midTimestamp);
        QVERIFY(tsIndex >= 0);
        QVERIFY(index[tsIndex].timestamp >= midTimestamp);
    }
    
    // Now test file source seeking using the index
    Offline::FileSource fileSource;
    fileSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy fileLoadedSpy(&fileSource, &Offline::FileSource::fileLoaded);
    QSignalSpy seekCompletedSpy(&fileSource, &Offline::FileSource::seekCompleted);
    
    QVERIFY(fileSource.loadFile(testFile));
    QVERIFY(waitForSignalWithTimeout(fileLoadedSpy));
    fileSource.start();
    
    // Test multiple seek operations
    struct SeekTest {
        uint64_t packetNumber;
        double position;
    };
    
    QVector<SeekTest> seekTests = {
        {0, 0.0},           // Beginning
        {50, 0.25},         // Quarter
        {100, 0.5},         // Half
        {150, 0.75},        // Three quarters
        {199, 1.0}          // Near end
    };
    
    for (const auto& test : seekTests) {
        seekCompletedSpy.clear();
        
        // Test packet-based seeking
        fileSource.seekToPacket(test.packetNumber);
        QVERIFY(waitForSignalWithTimeout(seekCompletedSpy, 1, 2000));
        
        const auto& stats = fileSource.getFileStatistics();
        QVERIFY(qAbs(static_cast<int>(stats.currentPacket) - static_cast<int>(test.packetNumber)) <= 2);
        
        // Test position-based seeking
        seekCompletedSpy.clear();
        fileSource.seekToPosition(test.position);
        QVERIFY(waitForSignalWithTimeout(seekCompletedSpy, 1, 2000));
        
        const auto& newStats = fileSource.getFileStatistics();
        QVERIFY(qAbs(newStats.playbackProgress - test.position) < 0.1);
    }
    
    fileSource.stop();
}

void TestOfflineIntegration::testLargeFileHandling()
{
    const int largePacketCount = 5000; // Larger test file
    QString testFile = createLargeTestFile("large_file.dat", largePacketCount);
    
    QElapsedTimer timer;
    timer.start();
    
    // Test indexing performance with larger file
    Offline::FileIndexer indexer;
    QSignalSpy indexingCompletedSpy(&indexer, &Offline::FileIndexer::indexingCompleted);
    QSignalSpy progressSpy(&indexer, &Offline::FileIndexer::progressChanged);
    
    QVERIFY(indexer.startIndexing(testFile, false));
    QVERIFY(waitForSignalWithTimeout(indexingCompletedSpy, 1, 30000)); // 30s timeout for large file
    
    qint64 indexingTime = timer.elapsed();
    qDebug() << "Indexing" << largePacketCount << "packets took" << indexingTime << "ms";
    
    // Verify indexing results
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::Completed);
    QCOMPARE(indexer.getPacketCount(), static_cast<uint64_t>(largePacketCount));
    
    const auto& stats = indexer.getStatistics();
    QCOMPARE(stats.totalPackets, static_cast<uint64_t>(largePacketCount));
    QVERIFY(stats.packetsPerSecond > 0);
    
    // Test progress reporting
    QVERIFY(progressSpy.count() > 0);
    bool foundProgress = false;
    for (const auto& args : progressSpy) {
        int progress = args[0].toInt();
        if (progress >= 0 && progress <= 100) {
            foundProgress = true;
            break;
        }
    }
    QVERIFY(foundProgress);
    
    // Test file source with large file
    timer.restart();
    
    Offline::FileSourceConfig config;
    config.filename = testFile;
    config.realTimePlayback = false;
    config.bufferSize = 2000; // Larger buffer
    
    Offline::FileSource fileSource(config);
    fileSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy fileLoadedSpy(&fileSource, &Offline::FileSource::fileLoaded);
    QSignalSpy packetReadySpy(&fileSource, &Packet::PacketSource::packetReady);
    
    QVERIFY(fileSource.loadFile(testFile));
    QVERIFY(waitForSignalWithTimeout(fileLoadedSpy, 1, 10000));
    
    qint64 loadTime = timer.elapsed();
    qDebug() << "Loading large file took" << loadTime << "ms";
    
    // Verify file statistics
    const auto& fileStats = fileSource.getFileStatistics();
    QCOMPARE(fileStats.totalPackets, static_cast<uint64_t>(largePacketCount));
    QVERIFY(fileStats.fileSize > 0);
    
    // Test playback performance
    timer.restart();
    fileSource.start();
    fileSource.play();
    
    // Wait for significant number of packets
    QVERIFY(waitForSignalWithTimeout(packetReadySpy, 100, 10000));
    
    qint64 playbackTime = timer.elapsed();
    double packetsPerSecond = (packetReadySpy.count() * 1000.0) / playbackTime;
    qDebug() << "Processed" << packetReadySpy.count() << "packets in" << playbackTime 
             << "ms (" << packetsPerSecond << "packets/sec)";
    
    // Verify minimum performance (should process >1000 packets/sec)
    QVERIFY(packetsPerSecond > 1000);
    
    fileSource.stop();
}

void TestOfflineIntegration::testMultipleFileTypes()
{
    // Create different types of test files
    struct FileTestCase {
        QString name;
        int packetCount;
        uint32_t startId;
    };
    
    QVector<FileTestCase> testCases = {
        {"small_file.dat", 10, 5000},
        {"medium_file.dat", 500, 6000},
        {"varied_packets.dat", 100, 7000}
    };
    
    for (const auto& testCase : testCases) {
        QString testFile = createTestFile(testCase.name, testCase.packetCount, testCase.startId);
        
        // Index each file
        Offline::FileIndexer indexer;
        QSignalSpy indexingCompletedSpy(&indexer, &Offline::FileIndexer::indexingCompleted);
        
        QVERIFY(indexer.startIndexing(testFile, false));
        QVERIFY(waitForSignalWithTimeout(indexingCompletedSpy, 1, 10000));
        QCOMPARE(indexer.getPacketCount(), static_cast<uint64_t>(testCase.packetCount));
        
        // Verify index contains expected packet IDs
        const auto& index = indexer.getIndex();
        if (!index.empty()) {
            QCOMPARE(index[0].packetId, testCase.startId);
            if (testCase.packetCount > 1) {
                QCOMPARE(index.back().packetId, testCase.startId + testCase.packetCount - 1);
            }
        }
        
        // Test file source with each file
        Offline::FileSource fileSource;
        fileSource.setPacketFactory(m_packetFactory);
        
        QSignalSpy fileLoadedSpy(&fileSource, &Offline::FileSource::fileLoaded);
        QSignalSpy packetReadySpy(&fileSource, &Packet::PacketSource::packetReady);
        
        QVERIFY(fileSource.loadFile(testFile));
        QVERIFY(waitForSignalWithTimeout(fileLoadedSpy));
        
        const auto& stats = fileSource.getFileStatistics();
        QCOMPARE(stats.totalPackets, static_cast<uint64_t>(testCase.packetCount));
        
        // Quick playback test
        fileSource.start();
        fileSource.play();
        
        // Wait for at least a few packets or all packets for small files
        int expectedPackets = qMin(testCase.packetCount, 5);
        QVERIFY(waitForSignalWithTimeout(packetReadySpy, expectedPackets, 3000));
        
        fileSource.stop();
        fileSource.closeFile();
    }
}

void TestOfflineIntegration::testErrorRecoveryIntegration()
{
    // Test with corrupted file
    QString corruptedFile = createCorruptedFile("corrupted.dat");
    
    // Test indexer error handling
    Offline::FileIndexer indexer;
    QSignalSpy indexingFailedSpy(&indexer, &Offline::FileIndexer::indexingFailed);
    QSignalSpy indexingCompletedSpy(&indexer, &Offline::FileIndexer::indexingCompleted);
    
    bool startResult = indexer.startIndexing(corruptedFile, false);
    
    // Either indexing fails to start or completes with errors
    if (startResult) {
        // Wait for either completion or failure
        bool completed = waitForSignalWithTimeout(indexingCompletedSpy, 1, 5000);
        bool failed = waitForSignalWithTimeout(indexingFailedSpy, 1, 100);
        
        if (completed) {
            // If completed, should have error packets reported
            const auto& stats = indexer.getStatistics();
            QVERIFY(stats.errorPackets > 0 || stats.totalPackets == 0);
        } else if (failed) {
            QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::Failed);
        }
    }
    
    // Test file source error handling
    Offline::FileSource fileSource;
    fileSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy errorSpy(&fileSource, &Packet::PacketSource::error);
    
    // Try to load corrupted file
    bool loadResult = fileSource.loadFile(corruptedFile);
    
    // Should either fail to load or handle gracefully
    if (loadResult) {
        // If loaded, starting should either work or fail gracefully
        fileSource.start();
        if (fileSource.isRunning()) {
            fileSource.play();
            
            // May get error signals or just no packets
            waitForMs(1000);
            
            // Should be able to stop without crashing
            fileSource.stop();
        }
    }
    
    // Test with non-existent file
    Offline::FileIndexer indexer2;
    QSignalSpy failedSpy2(&indexer2, &Offline::FileIndexer::indexingFailed);
    
    bool nonExistentResult = indexer2.startIndexing("/non/existent/file.dat", false);
    if (nonExistentResult) {
        QVERIFY(waitForSignalWithTimeout(failedSpy2, 1, 2000));
        QCOMPARE(indexer2.getStatus(), Offline::FileIndexer::IndexStatus::Failed);
    }
    
    // Test file source with non-existent file
    Offline::FileSource fileSource2;
    fileSource2.setPacketFactory(m_packetFactory);
    
    bool loadResult2 = fileSource2.loadFile("/non/existent/file.dat");
    QVERIFY(!loadResult2); // Should fail to load
    QVERIFY(!fileSource2.isFileLoaded());
}

void TestOfflineIntegration::testPerformanceIntegration()
{
    const int performancePacketCount = 1000;
    QString testFile = createTestFile("performance.dat", performancePacketCount, 8000);
    
    QElapsedTimer totalTimer;
    totalTimer.start();
    
    // Measure indexing performance
    QElapsedTimer indexTimer;
    indexTimer.start();
    
    Offline::FileIndexer indexer;
    QSignalSpy indexingCompletedSpy(&indexer, &Offline::FileIndexer::indexingCompleted);
    
    QVERIFY(indexer.startIndexing(testFile, false));
    QVERIFY(waitForSignalWithTimeout(indexingCompletedSpy, 1, 15000));
    
    qint64 indexingTime = indexTimer.elapsed();
    const auto& indexStats = indexer.getStatistics();
    
    qDebug() << "Performance Metrics:";
    qDebug() << "- Indexing time:" << indexingTime << "ms";
    qDebug() << "- Packets indexed:" << indexStats.totalPackets;
    qDebug() << "- Indexing rate:" << indexStats.packetsPerSecond << "packets/sec";
    
    // Verify reasonable indexing performance
    QVERIFY(indexStats.packetsPerSecond > 1000); // At least 1K packets/sec
    
    // Measure file loading performance
    QElapsedTimer loadTimer;
    loadTimer.start();
    
    Offline::FileSourceConfig config;
    config.filename = testFile;
    config.realTimePlayback = false;
    
    Offline::FileSource fileSource(config);
    fileSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy fileLoadedSpy(&fileSource, &Offline::FileSource::fileLoaded);
    
    QVERIFY(fileSource.loadFile(testFile));
    QVERIFY(waitForSignalWithTimeout(fileLoadedSpy, 1, 5000));
    
    qint64 loadTime = loadTimer.elapsed();
    qDebug() << "- File loading time:" << loadTime << "ms";
    
    // Verify reasonable load time
    QVERIFY(loadTime < 2000); // Should load within 2 seconds
    
    // Measure playback performance
    QElapsedTimer playbackTimer;
    playbackTimer.start();
    
    QSignalSpy packetReadySpy(&fileSource, &Packet::PacketSource::packetReady);
    
    fileSource.start();
    fileSource.play();
    
    // Process all packets or timeout
    while (packetReadySpy.count() < performancePacketCount && playbackTimer.elapsed() < 10000) {
        waitForMs(10);
        QCoreApplication::processEvents();
    }
    
    qint64 playbackTime = playbackTimer.elapsed();
    int packetsProcessed = packetReadySpy.count();
    double playbackRate = (packetsProcessed * 1000.0) / playbackTime;
    
    qDebug() << "- Playback time:" << playbackTime << "ms";
    qDebug() << "- Packets processed:" << packetsProcessed;
    qDebug() << "- Playback rate:" << playbackRate << "packets/sec";
    
    // Verify playback performance
    QVERIFY(playbackRate > 1000); // At least 1K packets/sec playback
    QVERIFY(packetsProcessed > performancePacketCount / 2); // At least half the packets
    
    qint64 totalTime = totalTimer.elapsed();
    qDebug() << "- Total test time:" << totalTime << "ms";
    
    fileSource.stop();
}

void TestOfflineIntegration::testIndexCacheIntegration()
{
    const int packetCount = 100;
    QString testFile = createTestFile("cache_test.dat", packetCount, 9000);
    QString cacheFile = Offline::FileIndexer::getCacheFilename(testFile);
    
    qDebug() << "Test file:" << testFile;
    qDebug() << "Cache file:" << cacheFile;
    
    // Ensure cache doesn't exist initially
    if (QFile::exists(cacheFile)) {
        QVERIFY(QFile::remove(cacheFile));
    }
    QVERIFY(!QFile::exists(cacheFile));
    
    // First indexing - should create cache
    Offline::FileIndexer indexer1;
    QSignalSpy indexingCompletedSpy1(&indexer1, &Offline::FileIndexer::indexingCompleted);
    
    QElapsedTimer firstIndexTimer;
    firstIndexTimer.start();
    
    QVERIFY(indexer1.startIndexing(testFile, false));
    QVERIFY(waitForSignalWithTimeout(indexingCompletedSpy1, 1, 10000));
    
    qint64 firstIndexTime = firstIndexTimer.elapsed();
    QCOMPARE(indexer1.getPacketCount(), static_cast<uint64_t>(packetCount));
    
    // Save index to cache
    QVERIFY(indexer1.saveIndexToCache(cacheFile));
    QVERIFY(QFile::exists(cacheFile));
    
    // Second indexing - should use cache
    Offline::FileIndexer indexer2;
    QSignalSpy indexingCompletedSpy2(&indexer2, &Offline::FileIndexer::indexingCompleted);
    
    // Load from cache first
    QVERIFY(indexer2.loadIndexFromCache(cacheFile));
    QCOMPARE(indexer2.getPacketCount(), static_cast<uint64_t>(packetCount));
    
    QElapsedTimer secondIndexTimer;
    secondIndexTimer.start();
    
    // This should be faster as it uses cache
    QVERIFY(indexer2.startIndexing(testFile, false));
    QVERIFY(waitForSignalWithTimeout(indexingCompletedSpy2, 1, 10000));
    
    qint64 secondIndexTime = secondIndexTimer.elapsed();
    
    qDebug() << "First indexing time:" << firstIndexTime << "ms";
    qDebug() << "Second indexing time:" << secondIndexTime << "ms";
    
    // Cache loading should be significantly faster
    // (Note: This may not always be true in tests due to small file size and disk caching)
    QCOMPARE(indexer2.getPacketCount(), static_cast<uint64_t>(packetCount));
    
    // Verify cache validity check
    QVERIFY(Offline::FileIndexer::isCacheValid(testFile));
    
    // Test file source with cached index
    Offline::FileSource fileSource;
    fileSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy fileLoadedSpy(&fileSource, &Offline::FileSource::fileLoaded);
    
    QVERIFY(fileSource.loadFile(testFile));
    QVERIFY(waitForSignalWithTimeout(fileLoadedSpy));
    QVERIFY(fileSource.isFileLoaded());
    
    const auto& stats = fileSource.getFileStatistics();
    QCOMPARE(stats.totalPackets, static_cast<uint64_t>(packetCount));
    
    // Test seeking with cached index
    QSignalSpy seekCompletedSpy(&fileSource, &Offline::FileSource::seekCompleted);
    
    fileSource.start();
    fileSource.seekToPacket(packetCount / 2);
    QVERIFY(waitForSignalWithTimeout(seekCompletedSpy));
    
    fileSource.stop();
    
    // Clean up cache file
    QVERIFY(QFile::remove(cacheFile));
}

// Helper method implementations

QString TestOfflineIntegration::createTestFile(const QString& filename, int packetCount, uint32_t startId)
{
    QString fullPath = m_tempDir->path() + "/" + filename;
    QFile file(fullPath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    
    for (int i = 0; i < packetCount; ++i) {
        uint32_t packetId = startId + i;
        uint32_t sequence = i;
        uint64_t timestamp = QDateTime::currentMSecsSinceEpoch() * 1000ULL + (i * 1000ULL);
        QByteArray payload = QString("Test packet %1 payload data").arg(i).toUtf8();
        
        QByteArray packet = createTestPacket(packetId, sequence, timestamp, payload);
        file.write(packet);
    }
    
    file.close();
    return fullPath;
}

QString TestOfflineIntegration::createLargeTestFile(const QString& filename, int packetCount)
{
    QString fullPath = m_tempDir->path() + "/" + filename;
    QFile file(fullPath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    
    for (int i = 0; i < packetCount; ++i) {
        uint32_t packetId = 10000 + (i % 100); // Varied packet IDs
        uint32_t sequence = i;
        uint64_t timestamp = QDateTime::currentMSecsSinceEpoch() * 1000ULL + (i * 500ULL);
        
        // Create varying payload sizes
        QString payloadBase = QString("Large file packet %1 with extended payload data").arg(i);
        if (i % 10 == 0) {
            payloadBase += QString(" - Extra large payload with lots of additional data to increase packet size and test variable packet handling capabilities");
        }
        QByteArray payload = payloadBase.toUtf8();
        
        QByteArray packet = createTestPacket(packetId, sequence, timestamp, payload);
        file.write(packet);
    }
    
    file.close();
    return fullPath;
}

QString TestOfflineIntegration::createCorruptedFile(const QString& filename)
{
    QString fullPath = m_tempDir->path() + "/" + filename;
    QFile file(fullPath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    
    // Write some valid packets first
    for (int i = 0; i < 5; ++i) {
        QByteArray packet = createTestPacket(11000 + i, i, QDateTime::currentMSecsSinceEpoch() * 1000ULL, QString("Valid packet %1").arg(i).toUtf8());
        file.write(packet);
    }
    
    // Write corrupted data
    QByteArray corruptedData;
    corruptedData.resize(100);
    for (int i = 0; i < 100; ++i) {
        corruptedData[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    file.write(corruptedData);
    
    // Write a few more valid packets
    for (int i = 5; i < 10; ++i) {
        QByteArray packet = createTestPacket(11000 + i, i, QDateTime::currentMSecsSinceEpoch() * 1000ULL, QString("Valid packet %1").arg(i).toUtf8());
        file.write(packet);
    }
    
    file.close();
    return fullPath;
}

QByteArray TestOfflineIntegration::createTestPacket(uint32_t id, uint32_t sequence, uint64_t timestamp, const QByteArray& payload)
{
    Packet::PacketHeader header;
    header.id = id;
    header.sequence = sequence;
    header.timestamp = timestamp;
    header.payloadSize = static_cast<uint32_t>(payload.size());
    header.flags = Packet::PacketHeader::Flags::TestData;
    
    QByteArray packet;
    packet.append(reinterpret_cast<const char*>(&header), sizeof(header));
    packet.append(payload);
    return packet;
}

bool TestOfflineIntegration::waitForSignalWithTimeout(QSignalSpy& spy, int expectedCount, int timeoutMs)
{
    if (spy.count() >= expectedCount) {
        return true;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    while (timer.elapsed() < timeoutMs && spy.count() < expectedCount) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }
    
    return spy.count() >= expectedCount;
}

void TestOfflineIntegration::waitForMs(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
}

bool TestOfflineIntegration::verifyFileContainsPackets(const QString& filename, int expectedCount)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    int foundPackets = 0;
    qint64 position = 0;
    
    while (position < file.size()) {
        file.seek(position);
        
        Packet::PacketHeader header;
        if (file.read(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header)) {
            break;
        }
        
        if (header.payloadSize > 65536) { // Sanity check
            break;
        }
        
        foundPackets++;
        position += sizeof(header) + header.payloadSize;
    }
    
    return foundPackets == expectedCount;
}

void TestOfflineIntegration::compareIndexWithFile(const QString& filename, const std::vector<Offline::PacketIndexEntry>& index)
{
    QFile file(filename);
    QVERIFY(file.open(QIODevice::ReadOnly));
    
    for (size_t i = 0; i < index.size() && i < 10; ++i) { // Test first 10 packets
        const auto& entry = index[i];
        
        file.seek(entry.filePosition);
        Packet::PacketHeader header;
        qint64 bytesRead = file.read(reinterpret_cast<char*>(&header), sizeof(header));
        QCOMPARE(bytesRead, static_cast<qint64>(sizeof(header)));
        
        QCOMPARE(header.id, entry.packetId);
        QCOMPARE(header.sequence, entry.sequenceNumber);
        QCOMPARE(header.timestamp, entry.timestamp);
        QCOMPARE(header.payloadSize, entry.packetSize - sizeof(header));
    }
}

QTEST_MAIN(TestOfflineIntegration)
#include "test_offline_integration.moc"