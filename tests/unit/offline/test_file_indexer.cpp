#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QElapsedTimer>
#include <QDir>
#include <QThread>
#include <QRandomGenerator>

#include "../../../src/offline/sources/file_indexer.h"
#include "../../../src/packet/core/packet_header.h"

using namespace Monitor;

class TestFileIndexer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testConstruction();
    void testState();
    void testSearchFunctions();
    void testCacheFilename();
    
    // File indexing tests with real files
    void testFileIndexing();
    void testIndexingProgress();
    void testIndexingSignals();
    void testIndexingCancel();
    void testIndexingFailure();
    
    // Index search and navigation tests
    void testPositionSearch();
    void testTimestampSearch();
    void testPacketIdSearch();
    void testSequenceNumberSearch();
    void testPacketEntryAccess();
    
    // Cache management tests
    void testCacheCreation();
    void testCacheLoading();
    void testCacheValidation();
    void testCacheInvalidation();
    
    // Performance and stress tests
    void testLargeFileIndexing();
    void testIndexingPerformance();
    void testMemoryUsage();
    void testCorruptedFileHandling();
    
    // Background indexing tests
    void testBackgroundIndexing();
    void testConcurrentIndexing();
    void testIndexingWithInterruption();
    
    // Error handling and edge cases
    void testEmptyFileIndexing();
    void testInvalidFileIndexing();
    void testPartialPacketHandling();
    void testIndexingLimits();

private:
    // Helper methods for test setup and file creation
    QByteArray createTestPacket(uint32_t id, uint32_t sequence, uint64_t timestamp, const QByteArray& payload);
    QString createTestFile(const QString& suffix, int packetCount, bool addCorruption = false);
    QString createLargeTestFile(int packetCount);
    QString createVariableSizePacketFile(int packetCount);
    void verifyIndexEntry(const Offline::PacketIndexEntry& entry, uint32_t expectedId, qint64 expectedPos);
    void waitForIndexingCompletion(Offline::FileIndexer& indexer, int timeoutMs = 10000);
    bool verifyIndexConsistency(const Offline::FileIndexer& indexer);
    
    // Test infrastructure
    QTemporaryDir* m_tempDir;
    QString m_testDataDir;
    QStringList m_createdFiles;
    
    // Test constants
    static const int SMALL_FILE_PACKET_COUNT = 50;
    static const int MEDIUM_FILE_PACKET_COUNT = 500;
    static const int LARGE_FILE_PACKET_COUNT = 5000;
    static const int TEST_TIMEOUT_MS = 10000;
};

void TestFileIndexer::initTestCase()
{
    // Create temporary directory for test files
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
    m_testDataDir = m_tempDir->path();
    
    qDebug() << "File indexer test data directory:" << m_testDataDir;
}

void TestFileIndexer::cleanupTestCase()
{
    delete m_tempDir;
    
    // Clean up any remaining test files
    for (const QString& filename : m_createdFiles) {
        QFile::remove(filename);
    }
}

void TestFileIndexer::init()
{
    // Clean up any files from previous test
    for (const QString& filename : m_createdFiles) {
        QFile::remove(filename);
    }
    m_createdFiles.clear();
}

void TestFileIndexer::cleanup()
{
    // Cleanup after each test
}

QByteArray TestFileIndexer::createTestPacket(uint32_t id, uint32_t sequence, uint64_t timestamp, const QByteArray& payload)
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

QString TestFileIndexer::createTestFile(const QString& suffix, int packetCount, bool addCorruption)
{
    QString filename = m_testDataDir + "/indexer_test_" + suffix + ".dat";
    QFile file(filename);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qFatal("Could not create indexer test file: %s", qPrintable(filename));
        return QString();
    }
    
    for (int i = 0; i < packetCount; ++i) {
        QString payloadText = QString("Index test packet %1 payload data").arg(i);
        QByteArray payload = payloadText.toUtf8();
        uint64_t timestamp = 1000000ULL + (i * 10000ULL); // Microsecond timestamps
        
        QByteArray packet = createTestPacket(1 + (i % 5), i, timestamp, payload);
        
        // Add corruption to some packets if requested
        if (addCorruption && i > 0 && i % 10 == 0) {
            // Corrupt the header magic number
            if (packet.size() >= 4) {
                packet[0] = 0xFF;
                packet[1] = 0xFF;
            }
        }
        
        qint64 written = file.write(packet);
        if (written != packet.size()) {
            qFatal("Failed to write packet %d to indexer test file", i);
            return QString();
        }
    }
    
    file.close();
    m_createdFiles.append(filename);
    
    return filename;
}

QString TestFileIndexer::createLargeTestFile(int packetCount)
{
    QString filename = m_testDataDir + "/large_indexer_test.dat";
    QFile file(filename);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qFatal("Could not create large indexer test file");
        return QString();
    }
    
    for (int i = 0; i < packetCount; ++i) {
        // Vary the payload size for realistic testing
        int payloadSize = 100 + (i % 200); // 100-299 bytes
        QByteArray payload(payloadSize, 'A' + (i % 26));
        
        uint64_t timestamp = 2000000ULL + (i * 5000ULL);
        QByteArray packet = createTestPacket(1 + (i % 10), i, timestamp, payload);
        
        file.write(packet);
    }
    
    file.close();
    m_createdFiles.append(filename);
    
    return filename;
}

QString TestFileIndexer::createVariableSizePacketFile(int packetCount)
{
    QString filename = m_testDataDir + "/variable_size_test.dat";
    QFile file(filename);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qFatal("Could not create variable size test file");
        return QString();
    }
    
    for (int i = 0; i < packetCount; ++i) {
        // Create packets with highly variable sizes
        int payloadSize = (i * 37) % 1000 + 10; // 10-1009 bytes, pseudo-random
        QByteArray payload(payloadSize, static_cast<char>('A' + (i % 26)));
        
        // Add some structured data in payload
        QString structuredData = QString("Packet_%1_Size_%2_Content").arg(i).arg(payloadSize);
        payload.append(structuredData.toUtf8());
        
        uint64_t timestamp = 3000000ULL + (i * (1000ULL + (i % 5000ULL))); // Variable timing
        QByteArray packet = createTestPacket(1 + (i % 8), i, timestamp, payload);
        
        file.write(packet);
    }
    
    file.close();
    m_createdFiles.append(filename);
    
    return filename;
}

void TestFileIndexer::verifyIndexEntry(const Offline::PacketIndexEntry& entry, uint32_t expectedId, qint64 expectedPos)
{
    QCOMPARE(entry.packetId, expectedId);
    QVERIFY(entry.filePosition >= expectedPos);
    QVERIFY(entry.packetSize > 0);
    QVERIFY(entry.timestamp > 0);
}

void TestFileIndexer::waitForIndexingCompletion(Offline::FileIndexer& indexer, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    
    while (!indexer.isIndexingComplete() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
    
    QVERIFY2(indexer.isIndexingComplete(), "Indexing did not complete within timeout");
}

bool TestFileIndexer::verifyIndexConsistency(const Offline::FileIndexer& indexer)
{
    const auto& index = indexer.getIndex();
    const auto& stats = indexer.getStatistics();
    
    // Verify basic consistency
    if (index.size() != stats.validPackets) {
        qWarning() << "Index size mismatch:" << index.size() << "vs" << stats.validPackets;
        return false;
    }
    
    // Verify entries are sorted by position
    for (size_t i = 1; i < index.size(); ++i) {
        if (index[i].filePosition <= index[i-1].filePosition) {
            qWarning() << "Index not sorted by position at entry" << i;
            return false;
        }
    }
    
    // Verify timestamps are generally increasing (allowing some variance)
    for (size_t i = 1; i < index.size(); ++i) {
        if (index[i].timestamp < index[i-1].timestamp - 1000000) { // Allow 1 second variance
            qWarning() << "Timestamp order issue at entry" << i;
            return false;
        }
    }
    
    return true;
}

// Basic Tests
void TestFileIndexer::testConstruction()
{
    Offline::FileIndexer indexer;
    
    // Test initial state
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::NotStarted);
    QVERIFY(!indexer.isIndexingComplete());
    QCOMPARE(indexer.getPacketCount(), 0ULL);
    
    const auto& stats = indexer.getStatistics();
    QVERIFY(stats.filename.isEmpty());
    QCOMPARE(stats.totalPackets, 0ULL);
    QCOMPARE(stats.indexedPackets, 0ULL);
}

void TestFileIndexer::testState()
{
    Offline::FileIndexer indexer;
    
    // Test state management
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::NotStarted);
    
    // Test state conversion
    QCOMPARE(Offline::indexStatusToString(Offline::FileIndexer::IndexStatus::NotStarted), QString("Not Started"));
    QCOMPARE(Offline::indexStatusToString(Offline::FileIndexer::IndexStatus::InProgress), QString("In Progress"));
    QCOMPARE(Offline::indexStatusToString(Offline::FileIndexer::IndexStatus::Completed), QString("Completed"));
    QCOMPARE(Offline::indexStatusToString(Offline::FileIndexer::IndexStatus::Failed), QString("Failed"));
    QCOMPARE(Offline::indexStatusToString(Offline::FileIndexer::IndexStatus::Cancelled), QString("Cancelled"));
}

void TestFileIndexer::testSearchFunctions()
{
    Offline::FileIndexer indexer;
    
    // Test search functions on empty index
    QCOMPARE(indexer.findPacketByPosition(0), -1);
    QCOMPARE(indexer.findPacketByTimestamp(123456), -1);
    QCOMPARE(indexer.findPacketBySequence(1), -1);
    QVERIFY(indexer.findPacketsByPacketId(0).empty());
    QCOMPARE(indexer.getPacketEntry(0), nullptr);
}

void TestFileIndexer::testCacheFilename()
{
    // Test cache filename generation
    QString testFile = "/path/to/test.dat";
    QString cacheFile = Offline::FileIndexer::getCacheFilename(testFile);
    QVERIFY(!cacheFile.isEmpty());
    QVERIFY(cacheFile.endsWith(".idx"));
    QVERIFY(cacheFile.contains("test"));
    
    // Test cache validity check for non-existent file
    QVERIFY(!Offline::FileIndexer::isCacheValid("/non/existent/file.dat"));
}

// File Indexing Tests
void TestFileIndexer::testFileIndexing()
{
    Offline::FileIndexer indexer;
    
    // Create test file with known packet structure
    QString testFile = createTestFile("basic", SMALL_FILE_PACKET_COUNT);
    QVERIFY(!testFile.isEmpty());
    
    // Start indexing
    QVERIFY(indexer.startIndexing(testFile, false)); // Synchronous indexing
    
    // Verify indexing completed
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::Completed);
    QVERIFY(indexer.isIndexingComplete());
    
    // Verify index was created
    QCOMPARE(static_cast<int>(indexer.getPacketCount()), SMALL_FILE_PACKET_COUNT);
    
    const auto& stats = indexer.getStatistics();
    QCOMPARE(stats.filename, testFile);
    QCOMPARE(stats.validPackets, static_cast<uint64_t>(SMALL_FILE_PACKET_COUNT));
    QVERIFY(stats.fileSize > 0);
    QVERIFY(stats.indexingTimeMs >= 0);
    
    // Verify index consistency
    QVERIFY(verifyIndexConsistency(indexer));
}

void TestFileIndexer::testIndexingProgress()
{
    Offline::FileIndexer indexer;
    QString testFile = createTestFile("progress", MEDIUM_FILE_PACKET_COUNT);
    
    QSignalSpy progressSpy(&indexer, &Offline::FileIndexer::progressChanged);
    QSignalSpy statsSpy(&indexer, &Offline::FileIndexer::statisticsUpdated);
    
    // Start background indexing
    QVERIFY(indexer.startIndexing(testFile, true));
    
    // Wait for completion
    waitForIndexingCompletion(indexer);
    
    // Verify progress signals were emitted
    QVERIFY(progressSpy.count() > 0);
    
    // Check progress values are reasonable (0-100)
    for (const QList<QVariant>& signal : progressSpy) {
        int progress = signal.first().toInt();
        QVERIFY(progress >= 0 && progress <= 100);
    }
    
    // Verify final progress is 100
    if (progressSpy.count() > 0) {
        int finalProgress = progressSpy.last().first().toInt();
        QCOMPARE(finalProgress, 100);
    }
}

void TestFileIndexer::testIndexingSignals()
{
    Offline::FileIndexer indexer;
    QString testFile = createTestFile("signals", SMALL_FILE_PACKET_COUNT);
    
    QSignalSpy startedSpy(&indexer, &Offline::FileIndexer::indexingStarted);
    QSignalSpy completedSpy(&indexer, &Offline::FileIndexer::indexingCompleted);
    QSignalSpy statusSpy(&indexer, &Offline::FileIndexer::statusChanged);
    
    // Start indexing
    QVERIFY(indexer.startIndexing(testFile, true));
    
    // Wait for completion
    waitForIndexingCompletion(indexer);
    
    // Verify signals were emitted
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(startedSpy.first().first().toString(), testFile);
    
    QCOMPARE(completedSpy.count(), 1);
    
    QVERIFY(statusSpy.count() >= 2); // At least NotStarted -> InProgress and InProgress -> Completed
}

void TestFileIndexer::testIndexingCancel()
{
    Offline::FileIndexer indexer;
    QString testFile = createLargeTestFile(LARGE_FILE_PACKET_COUNT); // Large file for cancellation test
    
    QSignalSpy cancelledSpy(&indexer, &Offline::FileIndexer::indexingCancelled);
    QSignalSpy statusSpy(&indexer, &Offline::FileIndexer::statusChanged);
    
    // Start indexing in background
    QVERIFY(indexer.startIndexing(testFile, true));
    
    // Wait a short time then cancel
    QThread::msleep(100);
    indexer.cancelIndexing();
    
    // Wait for cancellation to complete
    QElapsedTimer timer;
    timer.start();
    while (indexer.getStatus() == Offline::FileIndexer::IndexStatus::InProgress && timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
    
    // Verify cancellation
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::Cancelled);
    QVERIFY(cancelledSpy.count() > 0);
}

void TestFileIndexer::testIndexingFailure()
{
    Offline::FileIndexer indexer;
    
    QSignalSpy failedSpy(&indexer, &Offline::FileIndexer::indexingFailed);
    
    // Try to index non-existent file
    QString invalidFile = m_testDataDir + "/nonexistent.dat";
    QVERIFY(!indexer.startIndexing(invalidFile, false));
    
    // Verify failure
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::Failed);
}

// Search Tests
void TestFileIndexer::testPositionSearch()
{
    Offline::FileIndexer indexer;
    QString testFile = createTestFile("position_search", MEDIUM_FILE_PACKET_COUNT);
    
    QVERIFY(indexer.startIndexing(testFile, false));
    QVERIFY(indexer.isIndexingComplete());
    
    const auto& index = indexer.getIndex();
    QVERIFY(!index.empty());
    
    // Test position search for various positions
    for (size_t i = 0; i < std::min(size_t(10), index.size()); ++i) {
        qint64 position = index[i].filePosition;
        int foundIndex = indexer.findPacketByPosition(position);
        QCOMPARE(foundIndex, static_cast<int>(i));
    }
    
    // Test position search for non-existent positions
    QCOMPARE(indexer.findPacketByPosition(-1), -1);
    QCOMPARE(indexer.findPacketByPosition(999999999), -1);
}

void TestFileIndexer::testTimestampSearch()
{
    Offline::FileIndexer indexer;
    QString testFile = createTestFile("timestamp_search", MEDIUM_FILE_PACKET_COUNT);
    
    QVERIFY(indexer.startIndexing(testFile, false));
    QVERIFY(indexer.isIndexingComplete());
    
    const auto& index = indexer.getIndex();
    QVERIFY(!index.empty());
    
    // Test timestamp search
    uint64_t firstTimestamp = index[0].timestamp;
    int foundIndex = indexer.findPacketByTimestamp(firstTimestamp);
    QVERIFY(foundIndex >= 0);
    
    if (index.size() > 1) {
        uint64_t middleTimestamp = index[index.size()/2].timestamp;
        foundIndex = indexer.findPacketByTimestamp(middleTimestamp);
        QVERIFY(foundIndex >= static_cast<int>(index.size()/2) - 1); // Allow some variance
    }
    
    // Test search for non-existent timestamps
    QCOMPARE(indexer.findPacketByTimestamp(0), -1);
    QCOMPARE(indexer.findPacketByTimestamp(UINT64_MAX), -1);
}

void TestFileIndexer::testPacketIdSearch()
{
    Offline::FileIndexer indexer;
    QString testFile = createTestFile("packetid_search", MEDIUM_FILE_PACKET_COUNT);
    
    QVERIFY(indexer.startIndexing(testFile, false));
    QVERIFY(indexer.isIndexingComplete());
    
    // Search for packets with ID 1 (we create packets with IDs 1-5)
    auto foundPackets = indexer.findPacketsByPacketId(1);
    QVERIFY(!foundPackets.empty());
    
    // Verify all found packets have the correct ID
    for (int index : foundPackets) {
        const auto* entry = indexer.getPacketEntry(index);
        QVERIFY(entry != nullptr);
        QCOMPARE(entry->packetId, 1U);
    }
    
    // Test search for non-existent packet ID
    auto nonExistentPackets = indexer.findPacketsByPacketId(999);
    QVERIFY(nonExistentPackets.empty());
}

void TestFileIndexer::testSequenceNumberSearch()
{
    Offline::FileIndexer indexer;
    QString testFile = createTestFile("sequence_search", SMALL_FILE_PACKET_COUNT);
    
    QVERIFY(indexer.startIndexing(testFile, false));
    QVERIFY(indexer.isIndexingComplete());
    
    // Search for specific sequence numbers
    int foundIndex = indexer.findPacketBySequence(0); // First packet
    QCOMPARE(foundIndex, 0);
    
    if (SMALL_FILE_PACKET_COUNT > 10) {
        foundIndex = indexer.findPacketBySequence(10);
        QCOMPARE(foundIndex, 10);
    }
    
    // Test search for non-existent sequence
    foundIndex = indexer.findPacketBySequence(999999);
    QCOMPARE(foundIndex, -1);
}

void TestFileIndexer::testPacketEntryAccess()
{
    Offline::FileIndexer indexer;
    QString testFile = createTestFile("entry_access", SMALL_FILE_PACKET_COUNT);
    
    QVERIFY(indexer.startIndexing(testFile, false));
    QVERIFY(indexer.isIndexingComplete());
    
    // Test valid entry access
    const auto* entry = indexer.getPacketEntry(0);
    QVERIFY(entry != nullptr);
    QVERIFY(entry->filePosition >= 0);
    QVERIFY(entry->packetSize > 0);
    QVERIFY(entry->timestamp > 0);
    
    // Test invalid entry access
    QCOMPARE(indexer.getPacketEntry(-1), nullptr);
    QCOMPARE(indexer.getPacketEntry(static_cast<int>(indexer.getPacketCount())), nullptr);
}

// Cache Tests
void TestFileIndexer::testCacheCreation()
{
    Offline::FileIndexer indexer;
    QString testFile = createTestFile("cache_create", SMALL_FILE_PACKET_COUNT);
    
    QVERIFY(indexer.startIndexing(testFile, false));
    QVERIFY(indexer.isIndexingComplete());
    
    // Create cache file
    QString cacheFile = Offline::FileIndexer::getCacheFilename(testFile);
    QVERIFY(indexer.saveIndexToCache(cacheFile));
    QVERIFY(QFile::exists(cacheFile));
    
    m_createdFiles.append(cacheFile);
    
    // Verify cache file has content
    QFileInfo cacheInfo(cacheFile);
    QVERIFY(cacheInfo.size() > 0);
}

void TestFileIndexer::testCacheLoading()
{
    // First create and save an index
    Offline::FileIndexer indexer1;
    QString testFile = createTestFile("cache_load", SMALL_FILE_PACKET_COUNT);
    
    QVERIFY(indexer1.startIndexing(testFile, false));
    QVERIFY(indexer1.isIndexingComplete());
    
    QString cacheFile = Offline::FileIndexer::getCacheFilename(testFile);
    QVERIFY(indexer1.saveIndexToCache(cacheFile));
    m_createdFiles.append(cacheFile);
    
    uint64_t originalPacketCount = indexer1.getPacketCount();
    const auto& originalStats = indexer1.getStatistics();
    
    // Create new indexer and load from cache
    Offline::FileIndexer indexer2;
    QVERIFY(indexer2.loadIndexFromCache(cacheFile));
    
    // Verify loaded index matches original
    QCOMPARE(indexer2.getPacketCount(), originalPacketCount);
    QCOMPARE(indexer2.getStatus(), Offline::FileIndexer::IndexStatus::Completed);
    
    const auto& loadedStats = indexer2.getStatistics();
    QCOMPARE(loadedStats.validPackets, originalStats.validPackets);
    QCOMPARE(loadedStats.fileSize, originalStats.fileSize);
}

void TestFileIndexer::testCacheValidation()
{
    QString testFile = createTestFile("cache_validation", SMALL_FILE_PACKET_COUNT);
    
    // Test cache validation for existing file
    QVERIFY(!Offline::FileIndexer::isCacheValid(testFile)); // No cache exists yet
    
    // Create cache
    Offline::FileIndexer indexer;
    QVERIFY(indexer.startIndexing(testFile, false));
    
    QString cacheFile = Offline::FileIndexer::getCacheFilename(testFile);
    QVERIFY(indexer.saveIndexToCache(cacheFile));
    m_createdFiles.append(cacheFile);
    
    // Now cache should be valid
    QVERIFY(Offline::FileIndexer::isCacheValid(testFile));
}

void TestFileIndexer::testCacheInvalidation()
{
    QString testFile = createTestFile("cache_invalidation", SMALL_FILE_PACKET_COUNT);
    
    // Create cache
    Offline::FileIndexer indexer;
    QVERIFY(indexer.startIndexing(testFile, false));
    
    QString cacheFile = Offline::FileIndexer::getCacheFilename(testFile);
    QVERIFY(indexer.saveIndexToCache(cacheFile));
    m_createdFiles.append(cacheFile);
    
    QVERIFY(Offline::FileIndexer::isCacheValid(testFile));
    
    // Modify the original file to make cache invalid
    QThread::msleep(1100); // Ensure different timestamp
    QFile file(testFile);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append));
    file.write("invalidate");
    file.close();
    
    // Cache should now be invalid
    QVERIFY(!Offline::FileIndexer::isCacheValid(testFile));
}

// Performance Tests
void TestFileIndexer::testLargeFileIndexing()
{
    Offline::FileIndexer indexer;
    QString largeFile = createLargeTestFile(LARGE_FILE_PACKET_COUNT);
    
    QElapsedTimer timer;
    timer.start();
    
    QVERIFY(indexer.startIndexing(largeFile, false));
    
    qint64 indexingTime = timer.elapsed();
    qDebug() << "Large file indexing time:" << indexingTime << "ms for" << LARGE_FILE_PACKET_COUNT << "packets";
    
    // Should complete within reasonable time (< 10 seconds for 5K packets)
    QVERIFY(indexingTime < 10000);
    
    QVERIFY(indexer.isIndexingComplete());
    QCOMPARE(static_cast<int>(indexer.getPacketCount()), LARGE_FILE_PACKET_COUNT);
    
    // Verify indexing performance statistics
    const auto& stats = indexer.getStatistics();
    QVERIFY(stats.packetsPerSecond > 0);
    qDebug() << "Indexing performance:" << stats.packetsPerSecond << "packets/second";
    
    // Should achieve reasonable performance (>100 packets/second)
    QVERIFY(stats.packetsPerSecond > 100);
}

void TestFileIndexer::testIndexingPerformance()
{
    QString testFile = createVariableSizePacketFile(MEDIUM_FILE_PACKET_COUNT);
    
    const int numRuns = 3;
    QList<qint64> times;
    
    for (int run = 0; run < numRuns; ++run) {
        Offline::FileIndexer indexer;
        
        QElapsedTimer timer;
        timer.start();
        
        QVERIFY(indexer.startIndexing(testFile, false));
        
        qint64 elapsed = timer.elapsed();
        times.append(elapsed);
        
        qDebug() << "Performance run" << (run + 1) << ":" << elapsed << "ms";
    }
    
    // Calculate average performance
    qint64 avgTime = 0;
    for (qint64 time : times) {
        avgTime += time;
    }
    avgTime /= numRuns;
    
    qDebug() << "Average indexing time:" << avgTime << "ms";
    
    // Performance should be consistent (< 5 seconds average)
    QVERIFY(avgTime < 5000);
}

void TestFileIndexer::testMemoryUsage()
{
    Offline::FileIndexer indexer;
    QString testFile = createLargeTestFile(LARGE_FILE_PACKET_COUNT);
    
    // Index the file
    QVERIFY(indexer.startIndexing(testFile, false));
    QVERIFY(indexer.isIndexingComplete());
    
    // Verify index was created without excessive memory usage
    const auto& index = indexer.getIndex();
    size_t indexMemoryUsage = index.size() * sizeof(Offline::PacketIndexEntry);
    
    qDebug() << "Index memory usage:" << (indexMemoryUsage / 1024) << "KB for" 
             << index.size() << "packets";
    
    // Should be reasonable memory usage (< 1MB for 5K packets)
    QVERIFY(indexMemoryUsage < 1024 * 1024);
    
    // Verify index efficiency (each entry should be < 100 bytes in memory)
    size_t avgEntrySize = indexMemoryUsage / index.size();
    QVERIFY(avgEntrySize < 100);
}

void TestFileIndexer::testCorruptedFileHandling()
{
    Offline::FileIndexer indexer;
    QString corruptedFile = createTestFile("corrupted", MEDIUM_FILE_PACKET_COUNT, true); // Add corruption
    
    // Should still be able to index, but with some error packets
    QVERIFY(indexer.startIndexing(corruptedFile, false));
    QVERIFY(indexer.isIndexingComplete());
    
    const auto& stats = indexer.getStatistics();
    
    // Should have some valid packets
    QVERIFY(stats.validPackets > 0);
    
    // Should have detected some errors
    QVERIFY(stats.errorPackets > 0);
    
    // Total should be less than expected due to corruption
    QVERIFY(static_cast<int>(stats.validPackets) < MEDIUM_FILE_PACKET_COUNT);
    
    qDebug() << "Corrupted file indexing - Valid:" << stats.validPackets 
             << "Error:" << stats.errorPackets;
}

// Background Indexing Tests
void TestFileIndexer::testBackgroundIndexing()
{
    Offline::FileIndexer indexer;
    QString testFile = createTestFile("background", MEDIUM_FILE_PACKET_COUNT);
    
    QSignalSpy progressSpy(&indexer, &Offline::FileIndexer::progressChanged);
    QSignalSpy completedSpy(&indexer, &Offline::FileIndexer::indexingCompleted);
    
    // Start background indexing
    QVERIFY(indexer.startIndexing(testFile, true));
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::InProgress);
    
    // Should be able to do other operations while indexing
    QVERIFY(indexer.getPacketCount() >= 0); // Should not crash
    
    // Wait for completion
    waitForIndexingCompletion(indexer);
    
    // Verify completion
    QVERIFY(completedSpy.count() > 0);
    QVERIFY(progressSpy.count() > 0);
    QCOMPARE(static_cast<int>(indexer.getPacketCount()), MEDIUM_FILE_PACKET_COUNT);
}

void TestFileIndexer::testConcurrentIndexing()
{
    Offline::FileIndexer indexer;
    QString testFile = createTestFile("concurrent", SMALL_FILE_PACKET_COUNT);
    
    // Start first indexing
    QVERIFY(indexer.startIndexing(testFile, true));
    
    // Try to start second indexing - should fail
    QString testFile2 = createTestFile("concurrent2", SMALL_FILE_PACKET_COUNT);
    QVERIFY(!indexer.startIndexing(testFile2, true)); // Should fail - already indexing
    
    // Wait for completion
    waitForIndexingCompletion(indexer);
    
    // Now should be able to start new indexing
    QVERIFY(indexer.startIndexing(testFile2, false));
    QVERIFY(indexer.isIndexingComplete());
}

void TestFileIndexer::testIndexingWithInterruption()
{
    Offline::FileIndexer indexer;
    QString testFile = createLargeTestFile(LARGE_FILE_PACKET_COUNT);
    
    // Start indexing
    QVERIFY(indexer.startIndexing(testFile, true));
    
    // Let it run for a bit
    QThread::msleep(200);
    
    // Stop the indexer thread
    indexer.stopIndexing();
    
    // Wait for stop to complete
    QElapsedTimer timer;
    timer.start();
    while (indexer.getStatus() == Offline::FileIndexer::IndexStatus::InProgress && timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
    
    // Should be stopped or cancelled
    QVERIFY(indexer.getStatus() != Offline::FileIndexer::IndexStatus::InProgress);
}

// Error Handling Tests
void TestFileIndexer::testEmptyFileIndexing()
{
    Offline::FileIndexer indexer;
    
    // Create empty file
    QString emptyFile = m_testDataDir + "/empty.dat";
    QFile file(emptyFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    m_createdFiles.append(emptyFile);
    
    // Try to index empty file
    QVERIFY(!indexer.startIndexing(emptyFile, false));
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::Failed);
}

void TestFileIndexer::testInvalidFileIndexing()
{
    Offline::FileIndexer indexer;
    
    // Test with non-existent file
    QVERIFY(!indexer.startIndexing("/non/existent/file.dat", false));
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::Failed);
    
    // Test with directory instead of file
    QVERIFY(!indexer.startIndexing(m_testDataDir, false));
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::Failed);
}

void TestFileIndexer::testPartialPacketHandling()
{
    // Create file with partial packet at end
    QString partialFile = m_testDataDir + "/partial.dat";
    QFile file(partialFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    
    // Write some complete packets
    for (int i = 0; i < 5; ++i) {
        QByteArray packet = createTestPacket(1, i, 1000000 + i * 1000, "test");
        file.write(packet);
    }
    
    // Write partial packet (just header, no payload)
    Packet::PacketHeader header;
    header.id = 99;
    header.sequence = 999;
    header.timestamp = 9999999;
    header.payloadSize = 100; // Claim 100 bytes but don't write them
    header.flags = 0;
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.close();
    m_createdFiles.append(partialFile);
    
    // Index should handle partial packet gracefully
    Offline::FileIndexer indexer;
    QVERIFY(indexer.startIndexing(partialFile, false));
    QVERIFY(indexer.isIndexingComplete());
    
    // Should have indexed the complete packets only
    QCOMPARE(static_cast<int>(indexer.getPacketCount()), 5);
    
    const auto& stats = indexer.getStatistics();
    QCOMPARE(stats.validPackets, 5ULL);
    QVERIFY(stats.errorPackets >= 1); // The partial packet should be detected as error
}

void TestFileIndexer::testIndexingLimits()
{
    Offline::FileIndexer indexer;
    
    // Create file with packets that have invalid sizes
    QString limitsFile = m_testDataDir + "/limits.dat";
    QFile file(limitsFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    
    // Write packet with excessive payload size claim
    Packet::PacketHeader header;
    header.id = 1;
    header.sequence = 0;
    header.timestamp = 1000000;
    header.payloadSize = 1000000; // Claim 1MB payload
    header.flags = 0;
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // Write some normal data (much less than claimed)
    QByteArray smallPayload(100, 'X');
    file.write(smallPayload);
    
    file.close();
    m_createdFiles.append(limitsFile);
    
    // Indexer should handle size mismatch gracefully
    QVERIFY(!indexer.startIndexing(limitsFile, false)); // Should fail due to invalid structure
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::Failed);
}

QTEST_MAIN(TestFileIndexer)
#include "test_file_indexer.moc"