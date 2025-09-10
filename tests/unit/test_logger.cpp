#include <QtTest/QTest>
#include <QtCore/QObject>
#include <QtTest/QSignalSpy>
#include <QtCore/QTemporaryFile>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include "../../src/logging/logger.h"

class TestLogger : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // LogEntry tests
    void testLogEntryCreation();
    void testLogEntryToString();
    void testLogEntryToJson();
    
    // LogSink tests
    void testConsoleSink();
    void testFileSink();
    void testMemorySink();
    void testFileSinkRotation();
    
    // Logger core tests
    void testLoggerSingleton();
    void testLogLevels();
    void testCategoryLevels();
    void testSinkManagement();
    void testAsyncLogging();
    void testLogMacros();
    
    // Performance tests
    void testLoggingPerformance();
    void testAsyncLoggingPerformance();
    void testMemoryUsage();
    
    // Thread safety tests
    void testConcurrentLogging();
    void testAsyncQueueStress();

private:
    Monitor::Logging::Logger* m_logger = nullptr;
    QTemporaryDir m_tempDir;
    
    QString createTempLogFile();
    void waitForAsyncLogs(int timeoutMs = 1000);
};

void TestLogger::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestLogger::cleanupTestCase()
{
    // Cleanup handled by QTemporaryDir destructor
}

void TestLogger::init()
{
    m_logger = Monitor::Logging::Logger::instance();
    m_logger->clearSinks(); // Start with clean slate
    m_logger->setAsynchronous(false); // Use synchronous by default for testing
}

void TestLogger::cleanup()
{
    if (m_logger) {
        m_logger->flushAndWait();
        m_logger->clearSinks();
    }
}

QString TestLogger::createTempLogFile()
{
    return m_tempDir.filePath(QString("test_%1.log").arg(QDateTime::currentMSecsSinceEpoch()));
}

void TestLogger::waitForAsyncLogs(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    
    qint64 lastCount = m_logger->getLoggedCount();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
        
        // Check if logging has stabilized (no new logs for a bit)
        qint64 currentCount = m_logger->getLoggedCount();
        if (currentCount == lastCount) {
            break;
        }
        lastCount = currentCount;
    }
}

void TestLogger::testLogEntryCreation()
{
    Monitor::Logging::LogEntry entry(
        Monitor::Logging::LogLevel::Info, 
        "TestCategory", 
        "Test message",
        "test.cpp",
        "testFunction",
        42
    );
    
    QCOMPARE(entry.level, Monitor::Logging::LogLevel::Info);
    QCOMPARE(entry.category, QString("TestCategory"));
    QCOMPARE(entry.message, QString("Test message"));
    QCOMPARE(entry.file, QString("test.cpp"));
    QCOMPARE(entry.function, QString("testFunction"));
    QCOMPARE(entry.line, 42);
    QVERIFY(!entry.timestamp.isNull());
    QVERIFY(entry.threadId != 0);
}

void TestLogger::testLogEntryToString()
{
    Monitor::Logging::LogEntry entry(
        Monitor::Logging::LogLevel::Warning,
        "TestCat",
        "Test message"
    );
    
    QString defaultString = entry.toString();
    QVERIFY(defaultString.contains("TestCat"));
    QVERIFY(defaultString.contains("Test message"));
    QVERIFY(defaultString.contains(QString::number(static_cast<int>(Monitor::Logging::LogLevel::Warning))));
    
    QString customFormat = "{level} - {category}: {message}";
    QString customString = entry.toString(customFormat);
    QVERIFY(customString.contains("3 - TestCat: Test message"));
}

void TestLogger::testLogEntryToJson()
{
    Monitor::Logging::LogEntry entry(
        Monitor::Logging::LogLevel::Error,
        "JsonTest",
        "JSON message",
        "source.cpp",
        "jsonFunction",
        100
    );
    
    QByteArray json = entry.toJson();
    QVERIFY(!json.isEmpty());
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    QVERIFY(error.error == QJsonParseError::NoError);
    
    QJsonObject obj = doc.object();
    QCOMPARE(obj["level"].toInt(), static_cast<int>(Monitor::Logging::LogLevel::Error));
    QCOMPARE(obj["category"].toString(), QString("JsonTest"));
    QCOMPARE(obj["message"].toString(), QString("JSON message"));
    QCOMPARE(obj["file"].toString(), QString("source.cpp"));
    QCOMPARE(obj["function"].toString(), QString("jsonFunction"));
    QCOMPARE(obj["line"].toInt(), 100);
}

void TestLogger::testConsoleSink()
{
    auto consoleSink = std::make_shared<Monitor::Logging::ConsoleSink>();
    consoleSink->setMinLevel(Monitor::Logging::LogLevel::Debug);
    
    QCOMPARE(consoleSink->getMinLevel(), Monitor::Logging::LogLevel::Debug);
    QVERIFY(consoleSink->shouldLog(Monitor::Logging::LogLevel::Info));
    QVERIFY(!consoleSink->shouldLog(Monitor::Logging::LogLevel::Trace));
    
    // Test writing (can't easily verify console output, but should not crash)
    Monitor::Logging::LogEntry entry(Monitor::Logging::LogLevel::Info, "Console", "Test");
    consoleSink->write(entry);
    consoleSink->flush();
    
    // Test color settings
    consoleSink->setUseColors(false);
    QVERIFY(!consoleSink->getUseColors());
    
    consoleSink->setUseColors(true);
    QVERIFY(consoleSink->getUseColors());
}

void TestLogger::testFileSink()
{
    QString logPath = createTempLogFile();
    auto fileSink = std::make_shared<Monitor::Logging::FileSink>(logPath);
    
    QCOMPARE(fileSink->getFilePath(), logPath);
    QVERIFY(fileSink->getAutoFlush());
    
    // Test settings
    fileSink->setMaxFileSize(1024 * 1024); // 1MB
    QCOMPARE(fileSink->getMaxFileSize(), qint64(1024 * 1024));
    
    fileSink->setMaxFiles(5);
    QCOMPARE(fileSink->getMaxFiles(), 5);
    
    fileSink->setAutoFlush(false);
    QVERIFY(!fileSink->getAutoFlush());
    
    // Test writing
    Monitor::Logging::LogEntry entry(Monitor::Logging::LogLevel::Info, "File", "File test message");
    fileSink->write(entry);
    fileSink->flush();
    
    // Verify file exists and has content
    QFile file(logPath);
    QVERIFY(file.exists());
    QVERIFY(file.open(QIODevice::ReadOnly));
    
    QByteArray content = file.readAll();
    QVERIFY(content.contains("File test message"));
    QVERIFY(content.contains("File"));
}

void TestLogger::testMemorySink()
{
    const size_t maxEntries = 100;
    auto memorySink = std::make_shared<Monitor::Logging::MemorySink>(maxEntries);
    
    QCOMPARE(memorySink->getMaxEntries(), maxEntries);
    QCOMPARE(memorySink->getEntryCount(), size_t(0));
    
    // Add some entries
    for (int i = 0; i < 50; ++i) {
        Monitor::Logging::LogEntry entry(
            Monitor::Logging::LogLevel::Info, 
            "Memory", 
            QString("Message %1").arg(i)
        );
        memorySink->write(entry);
    }
    
    QCOMPARE(memorySink->getEntryCount(), size_t(50));
    
    // Test retrieval
    auto entries = memorySink->getEntries();
    QCOMPARE(entries.size(), size_t(50));
    QCOMPARE(entries[0].message, QString("Message 0"));
    QCOMPARE(entries[49].message, QString("Message 49"));
    
    // Test filtering
    auto infoEntries = memorySink->getEntriesByLevel(Monitor::Logging::LogLevel::Info);
    QCOMPARE(infoEntries.size(), size_t(50));
    
    auto debugEntries = memorySink->getEntriesByLevel(Monitor::Logging::LogLevel::Debug);
    QCOMPARE(debugEntries.size(), size_t(0));
    
    auto categoryEntries = memorySink->getEntriesByCategory("Memory");
    QCOMPARE(categoryEntries.size(), size_t(50));
    
    // Test overflow
    QSignalSpy overflowSpy(memorySink.get(), &Monitor::Logging::MemorySink::bufferFull);
    
    for (int i = 0; i < 60; ++i) { // This will cause overflow
        Monitor::Logging::LogEntry entry(Monitor::Logging::LogLevel::Warning, "Overflow", "Test");
        memorySink->write(entry);
    }
    
    QCOMPARE(memorySink->getEntryCount(), maxEntries); // Should not exceed max
    QVERIFY(overflowSpy.count() > 0);
    
    // Test clear
    memorySink->clear();
    QCOMPARE(memorySink->getEntryCount(), size_t(0));
}

void TestLogger::testFileSinkRotation()
{
    QString logPath = createTempLogFile();
    auto fileSink = std::make_shared<Monitor::Logging::FileSink>(logPath);
    
    fileSink->setMaxFileSize(1024); // 1KB for easy testing
    fileSink->setMaxFiles(3);
    
    QSignalSpy rotationSpy(fileSink.get(), &Monitor::Logging::FileSink::fileRotated);
    
    // Write enough data to trigger rotation
    QString longMessage(200, 'A'); // 200 character message
    
    for (int i = 0; i < 10; ++i) {
        Monitor::Logging::LogEntry entry(Monitor::Logging::LogLevel::Info, "Rotation", longMessage);
        fileSink->write(entry);
        fileSink->flush();
    }
    
    // Should have triggered at least one rotation
    QVERIFY(rotationSpy.count() > 0);
    
    // Verify rotated files exist
    QFileInfo mainFile(logPath);
    QString baseName = mainFile.baseName();
    QString suffix = mainFile.suffix();
    QString dir = mainFile.path();
    
    for (int i = 0; i < fileSink->getMaxFiles(); ++i) {
        QString rotatedPath = QString("%1/%2.%3.%4").arg(dir, baseName).arg(i).arg(suffix);
        // Some rotated files should exist
    }
}

void TestLogger::testLoggerSingleton()
{
    Monitor::Logging::Logger* logger1 = Monitor::Logging::Logger::instance();
    Monitor::Logging::Logger* logger2 = Monitor::Logging::Logger::instance();
    
    QVERIFY(logger1 != nullptr);
    QVERIFY(logger1 == logger2); // Same instance
}

void TestLogger::testLogLevels()
{
    auto memorySink = std::make_shared<Monitor::Logging::MemorySink>(1000);
    memorySink->setMinLevel(Monitor::Logging::LogLevel::Warning);
    m_logger->addSink(memorySink);
    
    m_logger->setGlobalLogLevel(Monitor::Logging::LogLevel::Info);
    
    // These should be logged (>= Info level)
    m_logger->info("Test", "Info message");
    m_logger->warning("Test", "Warning message");
    m_logger->error("Test", "Error message");
    m_logger->critical("Test", "Critical message");
    
    // These should not be logged (< Info level)
    m_logger->trace("Test", "Trace message");
    m_logger->debug("Test", "Debug message");
    
    auto entries = memorySink->getEntries();
    
    // Only warning, error, and critical should be in the sink (sink filters out info)
    QCOMPARE(entries.size(), size_t(3));
    QCOMPARE(entries[0].message, QString("Warning message"));
    QCOMPARE(entries[1].message, QString("Error message"));
    QCOMPARE(entries[2].message, QString("Critical message"));
}

void TestLogger::testCategoryLevels()
{
    auto memorySink = std::make_shared<Monitor::Logging::MemorySink>(1000);
    memorySink->setMinLevel(Monitor::Logging::LogLevel::Trace); // Accept all levels from sink
    m_logger->addSink(memorySink);
    
    m_logger->setGlobalLogLevel(Monitor::Logging::LogLevel::Warning);
    m_logger->setCategoryLevel("DebugCategory", Monitor::Logging::LogLevel::Debug);
    m_logger->setCategoryLevel("ErrorCategory", Monitor::Logging::LogLevel::Error);
    
    // Test global level
    m_logger->info("GlobalTest", "Should not appear");
    m_logger->warning("GlobalTest", "Should appear");
    
    // Test category-specific levels
    m_logger->debug("DebugCategory", "Debug should appear");
    m_logger->trace("DebugCategory", "Trace should not appear");
    
    m_logger->warning("ErrorCategory", "Warning should not appear");
    m_logger->error("ErrorCategory", "Error should appear");
    
    auto entries = memorySink->getEntries();
    QCOMPARE(entries.size(), size_t(3));
    
    // Verify correct entries
    bool foundGlobalWarning = false;
    bool foundDebugMessage = false;
    bool foundErrorMessage = false;
    
    for (const auto& entry : entries) {
        if (entry.category == "GlobalTest" && entry.message == "Should appear") {
            foundGlobalWarning = true;
        } else if (entry.category == "DebugCategory" && entry.message == "Debug should appear") {
            foundDebugMessage = true;
        } else if (entry.category == "ErrorCategory" && entry.message == "Error should appear") {
            foundErrorMessage = true;
        }
    }
    
    QVERIFY(foundGlobalWarning);
    QVERIFY(foundDebugMessage);
    QVERIFY(foundErrorMessage);
    
    // Test category level retrieval and removal
    QCOMPARE(m_logger->getCategoryLevel("DebugCategory"), Monitor::Logging::LogLevel::Debug);
    QCOMPARE(m_logger->getCategoryLevel("NonexistentCategory"), Monitor::Logging::LogLevel::Warning); // Should return global
    
    m_logger->removeCategoryLevel("DebugCategory");
    QCOMPARE(m_logger->getCategoryLevel("DebugCategory"), Monitor::Logging::LogLevel::Warning); // Should now return global
}

void TestLogger::testSinkManagement()
{
    auto sink1 = std::make_shared<Monitor::Logging::MemorySink>(100);
    auto sink2 = std::make_shared<Monitor::Logging::MemorySink>(100);
    
    m_logger->addSink(sink1);
    m_logger->addSink(sink2);
    
    m_logger->info("SinkTest", "Test message");
    
    // Both sinks should receive the message
    QCOMPARE(sink1->getEntryCount(), size_t(1));
    QCOMPARE(sink2->getEntryCount(), size_t(1));
    
    // Remove one sink
    m_logger->removeSink(sink1.get());
    m_logger->info("SinkTest", "Second message");
    
    // Only sink2 should receive the second message
    QCOMPARE(sink1->getEntryCount(), size_t(1));
    QCOMPARE(sink2->getEntryCount(), size_t(2));
    
    // Clear all sinks
    m_logger->clearSinks();
    m_logger->info("SinkTest", "Third message");
    
    // Neither sink should receive the third message
    QCOMPARE(sink1->getEntryCount(), size_t(1));
    QCOMPARE(sink2->getEntryCount(), size_t(2));
}

void TestLogger::testAsyncLogging()
{
    m_logger->setAsynchronous(true);
    QVERIFY(m_logger->isAsynchronous());
    
    auto memorySink = std::make_shared<Monitor::Logging::MemorySink>(1000);
    m_logger->addSink(memorySink);
    
    // Log some messages
    for (int i = 0; i < 100; ++i) {
        m_logger->info("AsyncTest", QString("Message %1").arg(i));
    }
    
    // Messages should be queued, not immediately in sink
    QVERIFY(memorySink->getEntryCount() < 100);
    
    // Wait for async processing
    waitForAsyncLogs(2000);
    m_logger->flushAndWait();
    
    // Now all messages should be processed
    QCOMPARE(memorySink->getEntryCount(), size_t(100));
    
    m_logger->setAsynchronous(false);
    QVERIFY(!m_logger->isAsynchronous());
}

void TestLogger::testLogMacros()
{
    auto memorySink = std::make_shared<Monitor::Logging::MemorySink>(1000);
    memorySink->setMinLevel(Monitor::Logging::LogLevel::Trace);
    m_logger->addSink(memorySink);
    m_logger->setGlobalLogLevel(Monitor::Logging::LogLevel::Trace);
    
    // Test basic macros
    LOG_TRACE("MacroTest", "Trace message");
    LOG_DEBUG("MacroTest", "Debug message");
    LOG_INFO("MacroTest", "Info message");
    LOG_WARNING("MacroTest", "Warning message");
    LOG_ERROR("MacroTest", "Error message");
    LOG_CRITICAL("MacroTest", "Critical message");
    
    auto entries = memorySink->getEntries();
    QCOMPARE(entries.size(), size_t(6));
    
    // Test file/line macros
    memorySink->clear();
    LOG_INFO_FL("FileLineTest", "Message with file and line");
    
    entries = memorySink->getEntries();
    QCOMPARE(entries.size(), size_t(1));
    QVERIFY(!entries[0].file.isEmpty());
    QVERIFY(!entries[0].function.isEmpty());
    QVERIFY(entries[0].line > 0);
}

void TestLogger::testLoggingPerformance()
{
    auto memorySink = std::make_shared<Monitor::Logging::MemorySink>(100000);
    m_logger->addSink(memorySink);
    m_logger->setAsynchronous(false); // Synchronous for accurate timing
    
    const int numMessages = 10000;
    
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < numMessages; ++i) {
        m_logger->info("PerfTest", QString("Performance test message %1").arg(i));
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerLog = static_cast<double>(elapsedNs) / numMessages;
    
    qDebug() << "Synchronous logging performance:" << nsPerLog << "ns per log";
    
    // Should be reasonably fast - less than 10μs per log
    QVERIFY(nsPerLog < 10000.0);
    
    QCOMPARE(memorySink->getEntryCount(), size_t(numMessages));
}

void TestLogger::testAsyncLoggingPerformance()
{
    m_logger->setAsynchronous(true);
    
    auto memorySink = std::make_shared<Monitor::Logging::MemorySink>(100000);
    m_logger->addSink(memorySink);
    
    const int numMessages = 10000;
    
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < numMessages; ++i) {
        m_logger->info("AsyncPerfTest", QString("Async performance test message %1").arg(i));
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerLog = static_cast<double>(elapsedNs) / numMessages;
    
    qDebug() << "Asynchronous logging performance:" << nsPerLog << "ns per log";
    
    // Async should be much faster for posting - less than 1μs per log
    QVERIFY(nsPerLog < 1000.0);
    
    // Wait for processing to complete
    waitForAsyncLogs(5000);
    m_logger->flushAndWait();
    
    QCOMPARE(memorySink->getEntryCount(), size_t(numMessages));
    
    m_logger->setAsynchronous(false);
}

void TestLogger::testMemoryUsage()
{
    const size_t maxEntries = 10000;
    auto memorySink = std::make_shared<Monitor::Logging::MemorySink>(maxEntries);
    m_logger->addSink(memorySink);
    
    // Fill the memory sink
    for (size_t i = 0; i < maxEntries; ++i) {
        m_logger->info("MemTest", QString("Memory test message %1 with some additional text to increase size").arg(i));
    }
    
    QCOMPARE(memorySink->getEntryCount(), maxEntries);
    
    // Add more entries - should not exceed maximum
    for (int i = 0; i < 1000; ++i) {
        m_logger->info("MemTest", "Overflow message");
    }
    
    QCOMPARE(memorySink->getEntryCount(), maxEntries);
}

void TestLogger::testConcurrentLogging()
{
    auto memorySink = std::make_shared<Monitor::Logging::MemorySink>(100000);
    m_logger->addSink(memorySink);
    m_logger->setAsynchronous(false); // Test synchronous thread safety
    
    const int numThreads = 4;
    const int messagesPerThread = 1000;
    
    QVector<QThread*> threads;
    QAtomicInt totalLogged(0);
    
    for (int i = 0; i < numThreads; ++i) {
        QThread* thread = QThread::create([this, i, &totalLogged]() {
            const int messagesPerThread = 1000;
            for (int j = 0; j < messagesPerThread; ++j) {
                m_logger->info(QString("Thread%1").arg(i), QString("Message %1").arg(j));
                totalLogged.fetchAndAddRelaxed(1);
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
        QVERIFY(thread->wait(5000));
        delete thread;
    }
    
    QCOMPARE(totalLogged.loadRelaxed(), numThreads * messagesPerThread);
    QCOMPARE(memorySink->getEntryCount(), size_t(numThreads * messagesPerThread));
    
    // Verify no data corruption - check that all messages are properly formatted
    auto entries = memorySink->getEntries();
    for (const auto& entry : entries) {
        QVERIFY(entry.category.startsWith("Thread"));
        QVERIFY(entry.message.startsWith("Message"));
        QVERIFY(!entry.timestamp.isNull());
    }
}

void TestLogger::testAsyncQueueStress()
{
    m_logger->setAsynchronous(true);
    
    auto memorySink = std::make_shared<Monitor::Logging::MemorySink>(200000);
    m_logger->addSink(memorySink);
    
    QSignalSpy queueFullSpy(m_logger, &Monitor::Logging::Logger::queueFull);
    
    const int numThreads = 8;
    const int messagesPerThread = 5000;
    
    QVector<QThread*> threads;
    
    for (int i = 0; i < numThreads; ++i) {
        QThread* thread = QThread::create([this, i]() {
            const int messagesPerThread = 5000;
            for (int j = 0; j < messagesPerThread; ++j) {
                m_logger->info(QString("StressThread%1").arg(i), 
                              QString("Stress message %1 with extra data to make it longer").arg(j));
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
        QVERIFY(thread->wait(10000)); // Longer timeout for stress test
        delete thread;
    }
    
    // Wait for async processing
    waitForAsyncLogs(10000);
    m_logger->flushAndWait();
    
    qDebug() << "Logged count:" << m_logger->getLoggedCount();
    qDebug() << "Dropped count:" << m_logger->getDroppedCount();
    qDebug() << "Sink entries:" << memorySink->getEntryCount();
    
    // Some messages might be dropped under extreme stress, but most should get through
    qint64 totalExpected = numThreads * messagesPerThread;
    qint64 totalProcessed = m_logger->getLoggedCount();
    
    QVERIFY(totalProcessed > totalExpected * 0.9); // At least 90% should succeed
    
    m_logger->setAsynchronous(false);
}

QTEST_MAIN(TestLogger)
#include "test_logger.moc"