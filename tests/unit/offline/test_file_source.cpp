#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QElapsedTimer>
#include <QDir>
#include <QStandardPaths>
#include <QThread>

#include "../../../src/offline/sources/file_source.h"
#include "../../../src/packet/core/packet_factory.h"
#include "../../../src/memory/memory_pool.h"
#include "../../../src/packet/core/packet_header.h"

using namespace Monitor;

class TestFileSource : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Basic functionality tests
    void testConstruction();
    void testConfiguration();
    void testPlaybackState();
    
    // File loading and format detection tests
    void testFileLoading();
    void testFileLoadingInvalidFile();
    void testFileFormatDetection();
    void testFileStatistics();
    void testFileIndexing();
    
    // Playback control tests
    void testPlayPauseStop();
    void testPlaybackStateTransitions();
    void testPlaybackSignals();
    void testLoopPlayback();
    void testRealTimePlayback();
    void testNonRealTimePlayback();
    
    // Speed control tests
    void testPlaybackSpeed();
    void testPlaybackSpeedLimits();
    void testPlaybackSpeedSignals();
    
    // Seeking functionality tests
    void testSeekToPacket();
    void testSeekToPosition();
    void testSeekToTimestamp();
    void testSeekBoundaryConditions();
    void testSeekSignals();
    
    // Step navigation tests
    void testStepForward();
    void testStepBackward();
    void testStepAtBoundaries();
    
    // Progress tracking tests
    void testProgressTracking();
    void testProgressUpdates();
    void testProgressSignals();
    
    // Performance and stress tests
    void testLargeFileLoading();
    void testHighSpeedPlayback();
    void testSeekPerformance();
    void testMemoryUsage();
    void testLongRunningPlayback();
    
    // Error handling tests
    void testFileAccessErrors();
    void testCorruptedPackets();
    void testSeekErrors();
    void testConfigurationErrors();

private:
    // Helper methods for test setup and packet creation
    QByteArray createTestPacket(uint32_t id, uint64_t timestamp, const QByteArray& payload);
    QString createTestFile(const QString& suffix, const QList<QByteArray>& packets);
    QString createLargeTestFile(int packetCount);
    void verifyFileStatistics(const Offline::FileSource& source, uint64_t expectedPackets);
    void waitForSignal(QSignalSpy& spy, int timeout = 1000);
    void verifyPlaybackState(const Offline::FileSource& source, Offline::PlaybackState expectedState);
    
    // Test infrastructure
    Memory::MemoryPool* m_memoryPool;
    Packet::PacketFactory* m_packetFactory;
    QTemporaryDir* m_tempDir;
    QString m_testDataDir;
    QStringList m_createdFiles;
    
    // Test constants
    static const int SMALL_FILE_PACKET_COUNT = 100;
    static const int MEDIUM_FILE_PACKET_COUNT = 1000;
    static const int LARGE_FILE_PACKET_COUNT = 10000;
    static const int TEST_TIMEOUT_MS = 5000;
};

// Static member definitions
const int TestFileSource::SMALL_FILE_PACKET_COUNT;
const int TestFileSource::MEDIUM_FILE_PACKET_COUNT;
const int TestFileSource::LARGE_FILE_PACKET_COUNT;
const int TestFileSource::TEST_TIMEOUT_MS;

void TestFileSource::initTestCase()
{
    // Create a single memory pool for testing
    m_memoryPool = new Memory::MemoryPool(4096, 100); // 4KB blocks, 100 blocks
    m_packetFactory = nullptr; // Will be mocked in individual tests
    
    // Create temporary directory for test files
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
    m_testDataDir = m_tempDir->path();
    
    qDebug() << "Test data directory:" << m_testDataDir;
}

void TestFileSource::cleanupTestCase()
{
    delete m_packetFactory;
    delete m_memoryPool;
    delete m_tempDir;
    
    // Clean up any remaining test files
    for (const QString& filename : m_createdFiles) {
        QFile::remove(filename);
    }
}

void TestFileSource::init()
{
    // Clean up any files from previous test
    for (const QString& filename : m_createdFiles) {
        QFile::remove(filename);
    }
    m_createdFiles.clear();
}

void TestFileSource::cleanup()
{
    // Cleanup after each test
}

QByteArray TestFileSource::createTestPacket(uint32_t id, uint64_t timestamp, const QByteArray& payload)
{
    Packet::PacketHeader header;
    header.id = id;
    header.sequence = 0;
    header.timestamp = timestamp;
    header.payloadSize = static_cast<uint32_t>(payload.size());
    header.flags = Packet::PacketHeader::Flags::TestData;
    
    QByteArray packet;
    packet.append(reinterpret_cast<const char*>(&header), sizeof(header));
    packet.append(payload);
    
    return packet;
}

QString TestFileSource::createTestFile(const QString& suffix, const QList<QByteArray>& packets)
{
    QString filename = m_testDataDir + "/test_" + suffix + ".dat";
    QFile file(filename);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qFatal("Could not create test file: %s", qPrintable(filename));
        return QString();
    }
    
    for (const QByteArray& packet : packets) {
        qint64 written = file.write(packet);
        if (written != packet.size()) {
            qFatal("Failed to write complete packet to file");
            return QString();
        }
    }
    
    file.close();
    m_createdFiles.append(filename);
    
    return filename;
}

QString TestFileSource::createLargeTestFile(int packetCount)
{
    QString filename = m_testDataDir + "/large_test.dat";
    QFile file(filename);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qFatal("Could not create large test file: %s", qPrintable(filename));
        return QString();
    }
    
    for (int i = 0; i < packetCount; ++i) {
        QByteArray payload = QString("Test packet %1 payload data").arg(i).toUtf8();
        QByteArray packet = createTestPacket(1, i * 1000, payload);
        qint64 written = file.write(packet);
        if (written != packet.size()) {
            qFatal("Failed to write packet %d to large test file", i);
            return QString();
        }
    }
    
    file.close();
    m_createdFiles.append(filename);
    
    return filename;
}

void TestFileSource::verifyFileStatistics(const Offline::FileSource& source, uint64_t expectedPackets)
{
    const auto& stats = source.getFileStatistics();
    QCOMPARE(stats.totalPackets, expectedPackets);
    QVERIFY(!stats.filename.isEmpty());
    QVERIFY(stats.fileSize > 0);
}

void TestFileSource::waitForSignal(QSignalSpy& spy, int timeout)
{
    QVERIFY(spy.wait(timeout));
}

void TestFileSource::verifyPlaybackState(const Offline::FileSource& source, Offline::PlaybackState expectedState)
{
    QCOMPARE(source.getPlaybackState(), expectedState);
}

// Basic Tests
void TestFileSource::testConstruction()
{
    Offline::FileSourceConfig config;
    config.playbackSpeed = 2.0;
    config.loopPlayback = true;
    config.realTimePlayback = false;
    
    Offline::FileSource source(config);
    
    QVERIFY(!source.isFileLoaded());
    QCOMPARE(source.getPlaybackState(), Offline::PlaybackState::Stopped);
    QCOMPARE(source.getFileConfig().playbackSpeed, 2.0);
    QVERIFY(source.getFileConfig().loopPlayback);
    QVERIFY(!source.getFileConfig().realTimePlayback);
}

void TestFileSource::testConfiguration()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Test initial configuration
    const auto& config = source.getFileConfig();
    QCOMPARE(config.playbackSpeed, 1.0);
    QVERIFY(!config.loopPlayback);
    QVERIFY(config.realTimePlayback);
    
    // Test configuration update
    Offline::FileSourceConfig newConfig;
    newConfig.playbackSpeed = 0.5;
    newConfig.loopPlayback = true;
    newConfig.realTimePlayback = false;
    
    source.setFileConfig(newConfig);
    
    const auto& updatedConfig = source.getFileConfig();
    QCOMPARE(updatedConfig.playbackSpeed, 0.5);
    QVERIFY(updatedConfig.loopPlayback);
    QVERIFY(!updatedConfig.realTimePlayback);
}

void TestFileSource::testPlaybackState()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Test initial state
    QVERIFY(!source.isFileLoaded());
    QCOMPARE(source.getPlaybackState(), Offline::PlaybackState::Stopped);
    QVERIFY(source.isAtBeginningOfFile());
    QVERIFY(source.isAtEndOfFile()); // True when no file loaded
    QCOMPARE(source.getPlaybackProgress(), 0.0);
    
    const auto& stats = source.getFileStatistics();
    QVERIFY(stats.filename.isEmpty());
    QCOMPARE(stats.totalPackets, 0ULL);
    QCOMPARE(stats.currentPacket, 0ULL);
}

// File Loading Tests
void TestFileSource::testFileLoading()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Create test file with known packets
    QList<QByteArray> testPackets;
    for (int i = 0; i < 10; ++i) {
        QByteArray payload = QString("Packet %1").arg(i).toUtf8();
        testPackets.append(createTestPacket(1, i * 1000, payload));
    }
    
    QString testFile = createTestFile("loading", testPackets);
    
    QSignalSpy fileLoadedSpy(&source, &Offline::FileSource::fileLoaded);
    
    QVERIFY(source.loadFile(testFile));
    QVERIFY(source.isFileLoaded());
    
    waitForSignal(fileLoadedSpy);
    QCOMPARE(fileLoadedSpy.count(), 1);
    QCOMPARE(fileLoadedSpy.first().first().toString(), testFile);
    
    verifyFileStatistics(source, 10);
}

void TestFileSource::testFileLoadingInvalidFile()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Test non-existent file
    QString invalidFile = m_testDataDir + "/nonexistent.dat";
    QVERIFY(!source.loadFile(invalidFile));
    QVERIFY(!source.isFileLoaded());
    
    // Test empty file
    QString emptyFile = createTestFile("empty", QList<QByteArray>());
    QVERIFY(!source.loadFile(emptyFile)); // Should fail for empty file
    QVERIFY(!source.isFileLoaded());
}

void TestFileSource::testFileFormatDetection()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Create binary format test file
    QList<QByteArray> testPackets;
    testPackets.append(createTestPacket(1, 1000, "Binary data"));
    QString binaryFile = createTestFile("binary", testPackets);
    
    QVERIFY(source.loadFile(binaryFile, Offline::FileSource::FileFormat::Binary));
    QVERIFY(source.isFileLoaded());
    
    source.closeFile();
    QVERIFY(!source.isFileLoaded());
    
    // Test auto-detection
    QVERIFY(source.loadFile(binaryFile, Offline::FileSource::FileFormat::AutoDetect));
    QVERIFY(source.isFileLoaded());
}

void TestFileSource::testFileStatistics()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Test empty source
    const auto& emptyStats = source.getFileStatistics();
    QVERIFY(emptyStats.filename.isEmpty());
    QCOMPARE(emptyStats.totalPackets, 0ULL);
    QCOMPARE(emptyStats.currentPacket, 0ULL);
    
    // Create test file and verify statistics
    QList<QByteArray> testPackets;
    for (int i = 0; i < 50; ++i) {
        QByteArray payload = QString("Statistics test packet %1").arg(i).toUtf8();
        testPackets.append(createTestPacket(1, i * 2000, payload));
    }
    
    QString testFile = createTestFile("statistics", testPackets);
    
    QSignalSpy statsSpy(&source, &Offline::FileSource::fileStatisticsUpdated);
    
    QVERIFY(source.loadFile(testFile));
    
    const auto& stats = source.getFileStatistics();
    QCOMPARE(stats.filename, testFile);
    QCOMPARE(stats.totalPackets, 50ULL);
    QCOMPARE(stats.currentPacket, 0ULL);
    QVERIFY(stats.fileSize > 0);
    QCOMPARE(stats.playbackProgress, 0.0);
}

void TestFileSource::testFileIndexing()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Create file with packets of different sizes
    QList<QByteArray> testPackets;
    for (int i = 0; i < 25; ++i) {
        QString payloadText = QString("Packet %1 ").repeated(i + 1); // Variable size payloads
        testPackets.append(createTestPacket(1, i * 1500, payloadText.toUtf8()));
    }
    
    QString testFile = createTestFile("indexing", testPackets);
    
    QVERIFY(source.loadFile(testFile));
    verifyFileStatistics(source, 25);
    
    // Test seeking to different positions to verify indexing works
    source.seekToPacket(10);
    QCOMPARE(source.getFileStatistics().currentPacket, 10ULL);
    
    source.seekToPacket(20);
    QCOMPARE(source.getFileStatistics().currentPacket, 20ULL);
    
    source.seekToPacket(0);
    QCOMPARE(source.getFileStatistics().currentPacket, 0ULL);
}

// Playback Control Tests
void TestFileSource::testPlayPauseStop()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Create test file
    QList<QByteArray> testPackets;
    for (int i = 0; i < 20; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, QString("Packet %1").arg(i).toUtf8()));
    }
    
    QString testFile = createTestFile("playback", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    QSignalSpy stateChangedSpy(&source, &Offline::FileSource::playbackStateChanged);
    
    // Test initial state
    verifyPlaybackState(source, Offline::PlaybackState::Stopped);
    
    // Test play
    source.play();
    verifyPlaybackState(source, Offline::PlaybackState::Playing);
    
    // Test pause
    source.pausePlayback();
    verifyPlaybackState(source, Offline::PlaybackState::Paused);
    
    // Test resume
    source.play();
    verifyPlaybackState(source, Offline::PlaybackState::Playing);
    
    // Test stop
    source.stopPlayback();
    verifyPlaybackState(source, Offline::PlaybackState::Stopped);
    QCOMPARE(source.getFileStatistics().currentPacket, 0ULL);
    
    QVERIFY(stateChangedSpy.count() >= 4); // At least 4 state changes
}

void TestFileSource::testPlaybackStateTransitions()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 5; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, QString("Test %1").arg(i).toUtf8()));
    }
    
    QString testFile = createTestFile("transitions", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    QSignalSpy stateChangedSpy(&source, &Offline::FileSource::playbackStateChanged);
    
    // Test all possible state transitions
    QCOMPARE(source.getPlaybackState(), Offline::PlaybackState::Stopped);
    
    source.play();
    QCOMPARE(source.getPlaybackState(), Offline::PlaybackState::Playing);
    
    source.pausePlayback();
    QCOMPARE(source.getPlaybackState(), Offline::PlaybackState::Paused);
    
    source.stopPlayback();
    QCOMPARE(source.getPlaybackState(), Offline::PlaybackState::Stopped);
    
    // Direct stop from playing
    source.play();
    source.stopPlayback();
    QCOMPARE(source.getPlaybackState(), Offline::PlaybackState::Stopped);
    
    QVERIFY(stateChangedSpy.count() >= 5);
}

void TestFileSource::testPlaybackSignals()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 3; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, QString("Signal test %1").arg(i).toUtf8()));
    }
    
    QString testFile = createTestFile("signals", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    QSignalSpy stateChangedSpy(&source, &Offline::FileSource::playbackStateChanged);
    QSignalSpy progressSpy(&source, &Offline::FileSource::progressUpdated);
    QSignalSpy endOfFileSpy(&source, &Offline::FileSource::endOfFileReached);
    
    source.play();
    
    // Wait for some progress signals
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 2000 && progressSpy.count() < 3) {
        QCoreApplication::processEvents();
        QThread::msleep(50);
    }
    
    source.stopPlayback();
    
    QVERIFY(stateChangedSpy.count() >= 2);
    QVERIFY(progressSpy.count() >= 1);
}

void TestFileSource::testLoopPlayback()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Configure for loop playback
    Offline::FileSourceConfig config;
    config.loopPlayback = true;
    config.realTimePlayback = false; // Fast playback for testing
    source.setFileConfig(config);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 3; ++i) { // Small file for quick looping
        testPackets.append(createTestPacket(1, i * 1000, QString("Loop %1").arg(i).toUtf8()));
    }
    
    QString testFile = createTestFile("loop", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    QSignalSpy endOfFileSpy(&source, &Offline::FileSource::endOfFileReached);
    
    QVERIFY(source.getFileConfig().loopPlayback);
    
    source.play();
    
    // Let it run for a while to test looping
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
    
    source.stopPlayback();
    
    // In loop mode, we shouldn't get permanent end of file
    // The signal might be emitted but playback should continue
}

void TestFileSource::testRealTimePlayback()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    Offline::FileSourceConfig config;
    config.realTimePlayback = true;
    config.playbackSpeed = 1.0;
    source.setFileConfig(config);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 5; ++i) {
        testPackets.append(createTestPacket(1, i * 100000, "Realtime test"));
    }
    
    QString testFile = createTestFile("realtime", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    QVERIFY(source.getFileConfig().realTimePlayback);
    
    QElapsedTimer timer;
    timer.start();
    
    source.play();
    
    // Real-time playback should take actual time based on timestamps
    QThread::msleep(10); // Reduced from 200 // Wait a bit
    
    source.stopPlayback();
    
    // Verify it took some reasonable time (not instant)
    QVERIFY(timer.elapsed() >= 100);
}

void TestFileSource::testNonRealTimePlayback()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    Offline::FileSourceConfig config;
    config.realTimePlayback = false; // Fast playback
    source.setFileConfig(config);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 10; ++i) {
        testPackets.append(createTestPacket(1, i * 1000000, "Fast test"));
    }
    
    QString testFile = createTestFile("nonrealtime", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    QVERIFY(!source.getFileConfig().realTimePlayback);
    
    source.play();
    
    // Non-real-time should be much faster
    QThread::msleep(100);
    
    source.stopPlayback();
}

// Speed Control Tests
void TestFileSource::testPlaybackSpeed()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy speedChangedSpy(&source, &Offline::FileSource::playbackSpeedChanged);
    
    // Test speed changes
    source.setPlaybackSpeed(2.0);
    QCOMPARE(source.getFileConfig().playbackSpeed, 2.0);
    
    source.setPlaybackSpeed(0.5);
    QCOMPARE(source.getFileConfig().playbackSpeed, 0.5);
    
    source.setPlaybackSpeed(1.0);
    QCOMPARE(source.getFileConfig().playbackSpeed, 1.0);
    
    QCOMPARE(speedChangedSpy.count(), 3);
}

void TestFileSource::testPlaybackSpeedLimits()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Test boundary values
    source.setPlaybackSpeed(0.1);  // Minimum
    QCOMPARE(source.getFileConfig().playbackSpeed, 0.1);
    
    source.setPlaybackSpeed(10.0); // Maximum
    QCOMPARE(source.getFileConfig().playbackSpeed, 10.0);
    
    // Test invalid values (should be clamped)
    source.setPlaybackSpeed(0.05); // Too small
    QVERIFY(source.getFileConfig().playbackSpeed >= 0.1);
    
    source.setPlaybackSpeed(20.0); // Too large
    QVERIFY(source.getFileConfig().playbackSpeed <= 10.0);
}

void TestFileSource::testPlaybackSpeedSignals()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy speedChangedSpy(&source, &Offline::FileSource::playbackSpeedChanged);
    
    double testSpeeds[] = {0.5, 1.0, 2.0, 5.0, 1.0};
    
    for (double speed : testSpeeds) {
        source.setPlaybackSpeed(speed);
    }
    
    QCOMPARE(speedChangedSpy.count(), 5);
    
    // Verify signal parameters
    for (int i = 0; i < speedChangedSpy.count(); ++i) {
        double signalSpeed = speedChangedSpy.at(i).first().toDouble();
        QCOMPARE(signalSpeed, testSpeeds[i]);
    }
}

// Seeking Tests
void TestFileSource::testSeekToPacket()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 50; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, QString("Seek packet %1").arg(i).toUtf8()));
    }
    
    QString testFile = createTestFile("seek_packet", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    QSignalSpy seekCompletedSpy(&source, &Offline::FileSource::seekCompleted);
    
    // Test seeking to various packet numbers
    source.seekToPacket(25);
    QCOMPARE(source.getFileStatistics().currentPacket, 25ULL);
    waitForSignal(seekCompletedSpy);
    QCOMPARE(seekCompletedSpy.last().first().toULongLong(), 25ULL);
    
    source.seekToPacket(10);
    QCOMPARE(source.getFileStatistics().currentPacket, 10ULL);
    
    source.seekToPacket(45);
    QCOMPARE(source.getFileStatistics().currentPacket, 45ULL);
    
    source.seekToPacket(0);
    QCOMPARE(source.getFileStatistics().currentPacket, 0ULL);
    
    QVERIFY(seekCompletedSpy.count() >= 4);
}

void TestFileSource::testSeekToPosition()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 100; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, QString("Position %1").arg(i).toUtf8()));
    }
    
    QString testFile = createTestFile("seek_position", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    // Test seeking to different positions
    source.seekToPosition(0.5); // Middle
    QVERIFY(source.getFileStatistics().currentPacket >= 45 && source.getFileStatistics().currentPacket <= 55);
    
    source.seekToPosition(0.0); // Beginning
    QCOMPARE(source.getFileStatistics().currentPacket, 0ULL);
    
    source.seekToPosition(1.0); // End
    QCOMPARE(source.getFileStatistics().currentPacket, 99ULL);
    
    source.seekToPosition(0.25); // Quarter
    QVERIFY(source.getFileStatistics().currentPacket >= 20 && source.getFileStatistics().currentPacket <= 30);
}

void TestFileSource::testSeekToTimestamp()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    QDateTime baseTime = QDateTime::currentDateTime();
    
    for (int i = 0; i < 30; ++i) {
        uint64_t timestamp = baseTime.addSecs(i * 10).toSecsSinceEpoch() * 1000000; // Microseconds
        testPackets.append(createTestPacket(1, timestamp, QString("Timestamp %1").arg(i).toUtf8()));
    }
    
    QString testFile = createTestFile("seek_timestamp", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    // Test seeking to specific timestamps
    QDateTime targetTime = baseTime.addSecs(150); // Should be around packet 15
    source.seekToTimestamp(targetTime);
    
    // Should be close to packet 15 (±2 packets tolerance)
    uint64_t currentPacket = source.getFileStatistics().currentPacket;
    QVERIFY(currentPacket >= 13 && currentPacket <= 17);
}

void TestFileSource::testSeekBoundaryConditions()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 20; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, "Boundary test"));
    }
    
    QString testFile = createTestFile("seek_boundary", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    // Test boundary conditions
    source.seekToPacket(100); // Beyond end - should clamp to last packet
    QCOMPARE(source.getFileStatistics().currentPacket, 19ULL);
    
    source.seekToPosition(-0.5); // Negative position - should clamp to 0
    QCOMPARE(source.getFileStatistics().currentPacket, 0ULL);
    
    source.seekToPosition(1.5); // Beyond 1.0 - should clamp to end
    QCOMPARE(source.getFileStatistics().currentPacket, 19ULL);
}

void TestFileSource::testSeekSignals()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 15; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, "Signal test"));
    }
    
    QString testFile = createTestFile("seek_signals", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    QSignalSpy seekCompletedSpy(&source, &Offline::FileSource::seekCompleted);
    QSignalSpy progressSpy(&source, &Offline::FileSource::progressUpdated);
    
    source.seekToPacket(5);
    source.seekToPacket(10);
    source.seekToPacket(7);
    
    QVERIFY(seekCompletedSpy.count() >= 3);
    QVERIFY(progressSpy.count() >= 3); // Should update progress on seek
}

// Step Navigation Tests
void TestFileSource::testStepForward()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 10; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, QString("Step %1").arg(i).toUtf8()));
    }
    
    QString testFile = createTestFile("step_forward", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    QCOMPARE(source.getFileStatistics().currentPacket, 0ULL);
    
    // Test stepping forward
    source.stepForward();
    QCOMPARE(source.getFileStatistics().currentPacket, 1ULL);
    
    source.stepForward();
    QCOMPARE(source.getFileStatistics().currentPacket, 2ULL);
    
    source.stepForward();
    QCOMPARE(source.getFileStatistics().currentPacket, 3ULL);
}

void TestFileSource::testStepBackward()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 10; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, QString("Back %1").arg(i).toUtf8()));
    }
    
    QString testFile = createTestFile("step_backward", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    // Start from middle
    source.seekToPacket(5);
    QCOMPARE(source.getFileStatistics().currentPacket, 5ULL);
    
    // Test stepping backward
    source.stepBackward();
    QCOMPARE(source.getFileStatistics().currentPacket, 4ULL);
    
    source.stepBackward();
    QCOMPARE(source.getFileStatistics().currentPacket, 3ULL);
    
    source.stepBackward();
    QCOMPARE(source.getFileStatistics().currentPacket, 2ULL);
}

void TestFileSource::testStepAtBoundaries()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 5; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, "Boundary"));
    }
    
    QString testFile = createTestFile("step_boundary", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    // Test stepping backward at beginning
    QCOMPARE(source.getFileStatistics().currentPacket, 0ULL);
    source.stepBackward(); // Should stay at 0
    QCOMPARE(source.getFileStatistics().currentPacket, 0ULL);
    
    // Go to end and test stepping forward
    source.seekToPacket(4); // Last packet
    QCOMPARE(source.getFileStatistics().currentPacket, 4ULL);
    source.stepForward(); // Should stay at 4
    QCOMPARE(source.getFileStatistics().currentPacket, 4ULL);
}

// Progress Tracking Tests
void TestFileSource::testProgressTracking()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 100; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, "Progress test"));
    }
    
    QString testFile = createTestFile("progress", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    // Test progress at different positions
    QCOMPARE(source.getPlaybackProgress(), 0.0);
    
    source.seekToPacket(25);
    double progress = source.getPlaybackProgress();
    QVERIFY(progress >= 0.20 && progress <= 0.30); // Should be around 0.25
    
    source.seekToPacket(50);
    progress = source.getPlaybackProgress();
    QVERIFY(progress >= 0.45 && progress <= 0.55); // Should be around 0.50
    
    source.seekToPacket(99);
    progress = source.getPlaybackProgress();
    QVERIFY(progress >= 0.95 && progress <= 1.0); // Should be close to 1.0
}

void TestFileSource::testProgressUpdates()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 20; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, "Update test"));
    }
    
    QString testFile = createTestFile("progress_update", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    QSignalSpy progressSpy(&source, &Offline::FileSource::progressUpdated);
    
    // Seek to different positions and verify progress updates
    source.seekToPacket(5);
    source.seekToPacket(10);
    source.seekToPacket(15);
    
    QVERIFY(progressSpy.count() >= 3);
    
    // Verify progress values are reasonable
    for (const QList<QVariant>& signal : progressSpy) {
        double progress = signal.first().toDouble();
        QVERIFY(progress >= 0.0 && progress <= 1.0);
    }
}

void TestFileSource::testProgressSignals()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 10; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, "Signal progress"));
    }
    
    QString testFile = createTestFile("progress_signals", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    QSignalSpy progressSpy(&source, &Offline::FileSource::progressUpdated);
    QSignalSpy statsSpy(&source, &Offline::FileSource::fileStatisticsUpdated);
    
    // Configure for non-realtime fast playback
    Offline::FileSourceConfig config;
    config.realTimePlayback = false;
    source.setFileConfig(config);
    
    source.play();
    
    // Let it run briefly
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 500 && source.getPlaybackState() == Offline::PlaybackState::Playing) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
    
    source.stopPlayback();
    
    // Should have received some progress updates during playback
    QVERIFY(progressSpy.count() >= 1);
}

// Performance Tests
void TestFileSource::testLargeFileLoading()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Create a large test file
    QString largeFile = createLargeTestFile(LARGE_FILE_PACKET_COUNT);
    
    QElapsedTimer timer;
    timer.start();
    
    QVERIFY(source.loadFile(largeFile));
    
    qint64 loadTime = timer.elapsed();
    qDebug() << "Large file loading time:" << loadTime << "ms for" << LARGE_FILE_PACKET_COUNT << "packets";
    
    // Should load within reasonable time (< 5 seconds for 10K packets)
    QVERIFY(loadTime < 5000);
    
    verifyFileStatistics(source, LARGE_FILE_PACKET_COUNT);
    
    // Test that seeking works quickly even with large file
    timer.restart();
    source.seekToPacket(LARGE_FILE_PACKET_COUNT / 2);
    qint64 seekTime = timer.elapsed();
    
    qDebug() << "Large file seek time:" << seekTime << "ms";
    QVERIFY(seekTime < 100); // Should seek quickly due to indexing
}

void TestFileSource::testHighSpeedPlayback()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 100; ++i) {
        testPackets.append(createTestPacket(1, i * 10000, "Speed test"));
    }
    
    QString testFile = createTestFile("high_speed", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    // Configure for high-speed playback
    Offline::FileSourceConfig config;
    config.playbackSpeed = 10.0; // Maximum speed
    config.realTimePlayback = false;
    source.setFileConfig(config);
    
    QElapsedTimer timer;
    timer.start();
    
    source.play();
    
    // Let it run for a short time
    while (timer.elapsed() < 200 && source.getPlaybackState() == Offline::PlaybackState::Playing) {
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }
    
    source.stopPlayback();
    
    qint64 elapsed = timer.elapsed();
    uint64_t processed = source.getFileStatistics().currentPacket;
    
    qDebug() << "High speed playback processed" << processed << "packets in" << elapsed << "ms";
    
    // Should have processed multiple packets quickly
    QVERIFY(processed > 5);
}

void TestFileSource::testSeekPerformance()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QString mediumFile = createLargeTestFile(MEDIUM_FILE_PACKET_COUNT);
    QVERIFY(source.loadFile(mediumFile));
    
    // Perform multiple seeks and measure performance
    QElapsedTimer timer;
    timer.start();
    
    const int numSeeks = 100;
    for (int i = 0; i < numSeeks; ++i) {
        uint64_t targetPacket = (i * MEDIUM_FILE_PACKET_COUNT) / numSeeks;
        source.seekToPacket(targetPacket);
    }
    
    qint64 totalTime = timer.elapsed();
    double avgSeekTime = static_cast<double>(totalTime) / numSeeks;
    
    qDebug() << "Seek performance:" << avgSeekTime << "ms per seek";
    
    // Each seek should be fast (< 10ms average)
    QVERIFY(avgSeekTime < 10.0);
}

void TestFileSource::testMemoryUsage()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Test that memory usage doesn't grow excessively with large files
    QString largeFile = createLargeTestFile(LARGE_FILE_PACKET_COUNT);
    
    // Get baseline memory usage
    size_t baselineUsed = m_memoryPool->getUsedBlocks();
    
    QVERIFY(source.loadFile(largeFile));
    
    // Memory usage after loading
    size_t loadedUsed = m_memoryPool->getUsedBlocks();
    
    // Perform various operations
    source.seekToPacket(1000);
    source.seekToPacket(5000);
    source.seekToPacket(9000);
    
    // Final memory usage
    size_t finalUsed = m_memoryPool->getUsedBlocks();
    
    qDebug() << "Memory blocks - Baseline:" << baselineUsed 
             << "After load:" << loadedUsed 
             << "After operations:" << finalUsed;
    
    // Memory usage should be reasonable and not leak
    size_t memoryIncrease = finalUsed - baselineUsed;
    QVERIFY(memoryIncrease < 50); // Less than 50 additional blocks
}

void TestFileSource::testLongRunningPlayback()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    QList<QByteArray> testPackets;
    for (int i = 0; i < 200; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, "Long run test"));
    }
    
    QString testFile = createTestFile("long_run", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    // Configure for fast non-realtime playback with looping
    Offline::FileSourceConfig config;
    config.realTimePlayback = false;
    config.loopPlayback = true;
    source.setFileConfig(config);
    
    QElapsedTimer timer;
    timer.start();
    
    source.play();
    
    // Let it run for an extended period
    while (timer.elapsed() < 2000) { // 2 seconds
        QCoreApplication::processEvents();
        QThread::msleep(50);
    }
    
    source.stopPlayback();
    
    // Verify it continued running without issues
    QCOMPARE(source.getPlaybackState(), Offline::PlaybackState::Stopped);
    
    // Should have processed many packets due to looping
    uint64_t totalProcessed = source.getFileStatistics().currentPacket;
    qDebug() << "Long running playback processed" << totalProcessed << "packets";
}

// Error Handling Tests
void TestFileSource::testFileAccessErrors()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Test with invalid file paths
    QVERIFY(!source.loadFile("/invalid/path/file.dat"));
    QVERIFY(!source.isFileLoaded());
    
    // Test with directory instead of file
    QVERIFY(!source.loadFile(m_testDataDir));
    QVERIFY(!source.isFileLoaded());
    
    // Test with empty filename
    QVERIFY(!source.loadFile(""));
    QVERIFY(!source.isFileLoaded());
}

void TestFileSource::testCorruptedPackets()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Create file with some corrupted packet data
    QByteArray corruptedData;
    
    // Add a few valid packets
    corruptedData.append(createTestPacket(1, 1000, "Valid packet 1"));
    corruptedData.append(createTestPacket(1, 2000, "Valid packet 2"));
    
    // Add some garbage data
    corruptedData.append("GARBAGE_DATA_NOT_A_PACKET_HEADER");
    
    // Add another valid packet
    corruptedData.append(createTestPacket(1, 3000, "Valid packet 3"));
    
    QString corruptedFile = m_testDataDir + "/corrupted.dat";
    QFile file(corruptedFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(corruptedData);
    file.close();
    m_createdFiles.append(corruptedFile);
    
    // The source should handle corrupted data gracefully
    source.loadFile(corruptedFile);
    // May or may not load depending on how corruption is handled
    // The important thing is it doesn't crash
}

void TestFileSource::testSeekErrors()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Test seeking without loaded file
    source.seekToPacket(10);
    source.seekToPosition(0.5);
    // Should not crash
    
    // Load a file and test invalid seeks
    QList<QByteArray> testPackets;
    for (int i = 0; i < 10; ++i) {
        testPackets.append(createTestPacket(1, i * 1000, "Seek error test"));
    }
    
    QString testFile = createTestFile("seek_error", testPackets);
    QVERIFY(source.loadFile(testFile));
    
    // Test seeking to invalid positions
    source.seekToPacket(UINT64_MAX); // Very large packet number
    // Should clamp to valid range
    QVERIFY(source.getFileStatistics().currentPacket <= 9);
    
    source.seekToPosition(-100.0); // Negative position
    QCOMPARE(source.getFileStatistics().currentPacket, 0ULL);
}

void TestFileSource::testConfigurationErrors()
{
    Offline::FileSource source;
    source.setPacketFactory(m_packetFactory);
    
    // Test with null packet factory
    Offline::FileSource nullFactorySource;
    
    QList<QByteArray> testPackets;
    testPackets.append(createTestPacket(1, 1000, "Config error test"));
    
    QString testFile = createTestFile("config_error", testPackets);
    
    // Should fail gracefully without packet factory
    QVERIFY(!nullFactorySource.loadFile(testFile));
    
    // Test invalid configuration values
    Offline::FileSourceConfig config;
    config.playbackSpeed = -1.0; // Invalid speed
    config.bufferSize = -1; // Invalid buffer size
    
    source.setFileConfig(config);
    // Should handle invalid config gracefully
}

QTEST_MAIN(TestFileSource)
#include "test_file_source.moc"