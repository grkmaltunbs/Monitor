#include <QtTest/QtTest>
#include <QtCore/QElapsedTimer>
#include <QtCore/QByteArray>
#include <QtCore/QDataStream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <memory>

#include "messaging/message.h"

using namespace Monitor::Messaging;

class TestMessage : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic message tests
    void testBasicMessageCreation();
    void testMessageWithPayload();
    void testMessageMetadata();
    void testMessageCopy();
    void testMessageSerialization();
    
    // Zero-copy message tests
    void testZeroCopyMessageCreation();
    void testZeroCopyMessageOwnership();
    void testZeroCopyMessageAccess();
    void testZeroCopyMessageSafety();
    
    // Shared message tests
    void testSharedMessageCreation();
    void testSharedMessageSharing();
    void testSharedMessageThreadSafety();
    void testSharedMessageLifetime();
    
    // Performance tests
    void testMessageCreationPerformance();
    void testZeroCopyPerformance();
    void testSharedMessagePerformance();
    void testSerializationPerformance();
    
    // Memory management tests
    void testMemoryOwnership();
    void testMemoryLeakPrevention();
    void testLargePayloadHandling();
    void testPayloadAlignment();
    
    // Timing and metadata tests
    void testTimestampAccuracy();
    void testSequenceNumbers();
    void testRoutingInformation();
    void testPriorityHandling();
    
    // Thread safety tests
    void testConcurrentAccess();
    void testConcurrentSharedAccess();
    void testConcurrentSerialization();
    
    // Stress tests
    void testMassiveMessageCreation();
    void testLongRunningMessageOperations();
    void testMemoryPressureHandling();
    
    // Edge cases
    void testEmptyMessages();
    void testNullPayloads();
    void testMaxSizePayloads();
    void testInvalidOperations();

private:
    // Helper methods
    void verifyMessageIntegrity(const Message& message);
    void verifyZeroCopyIntegrity(const ZeroCopyMessage& message);
    void verifySharedMessageIntegrity(const SharedMessage& message);
    QByteArray createTestPayload(int size, char pattern = 0x55);
    void measureMessageOperationLatency(int iterations, const std::function<void()>& operation, 
                                       const QString& operationName);
    
    // Test constants
    static constexpr int STRESS_TEST_ITERATIONS = 10000;
    static constexpr int PERFORMANCE_ITERATIONS = 100000;
};

void TestMessage::initTestCase()
{
    qDebug() << "Starting Message system comprehensive tests";
}

void TestMessage::cleanupTestCase()
{
    qDebug() << "Message system tests completed";
}

void TestMessage::init()
{
    // Fresh state for each test
}

void TestMessage::cleanup()
{
    // Cleanup after each test
}

void TestMessage::testBasicMessageCreation()
{
    // Test default construction
    Message msg1;
    QCOMPARE(msg1.getType(), static_cast<uint32_t>(0));
    QCOMPARE(msg1.getSequenceNumber(), static_cast<uint64_t>(0));
    QVERIFY(msg1.getPayload().isEmpty());
    QVERIFY(msg1.getTimestamp().time_since_epoch().count() > 0);
    
    // Test construction with type
    Message msg2(42);
    QCOMPARE(msg2.getType(), static_cast<uint32_t>(42));
    QVERIFY(msg2.getSequenceNumber() > 0); // Should have auto-generated sequence
    QVERIFY(msg2.getPayload().isEmpty());
    
    // Test construction with type and payload
    QByteArray testPayload = createTestPayload(100);
    Message msg3(100, testPayload);
    QCOMPARE(msg3.getType(), static_cast<uint32_t>(100));
    QCOMPARE(msg3.getPayload(), testPayload);
    QVERIFY(msg3.getSequenceNumber() > 0);
    
    verifyMessageIntegrity(msg3);
}

void TestMessage::testMessageWithPayload()
{
    const int payloadSize = 1024;
    QByteArray payload = createTestPayload(payloadSize, 0xAB);
    
    Message msg(256, payload);
    
    QCOMPARE(msg.getType(), static_cast<uint32_t>(256));
    QCOMPARE(msg.getPayload().size(), payloadSize);
    QCOMPARE(msg.getPayload(), payload);
    
    // Verify payload content
    const QByteArray& msgPayload = msg.getPayload();
    for (int i = 0; i < payloadSize; ++i) {
        QCOMPARE(static_cast<unsigned char>(msgPayload[i]), static_cast<unsigned char>(0xAB));
    }
    
    verifyMessageIntegrity(msg);
}

void TestMessage::testMessageMetadata()
{
    Message msg(123);
    
    // Test source and destination
    ThreadId sourceId = 0x1234567890ABCDEF;
    ThreadId destId = 0xFEDCBA0987654321;
    
    msg.setSource(sourceId);
    msg.setDestination(destId);
    
    QCOMPARE(msg.getSource(), sourceId);
    QCOMPARE(msg.getDestination(), destId);
    
    // Test priority
    msg.setPriority(Priority::High);
    QVERIFY(msg.getPriority() == Priority::High);
    
    // Test route
    QString route = "test/message/route";
    msg.setRoute(route);
    QCOMPARE(msg.getRoute(), route);
    
    // Test flags
    msg.setFlags(0x12345678);
    QCOMPARE(msg.getFlags(), static_cast<uint32_t>(0x12345678));
    
    verifyMessageIntegrity(msg);
}

void TestMessage::testMessageCopy()
{
    QByteArray payload = createTestPayload(500, 0xCC);
    Message original(789, payload);
    original.setSource(0x1111);
    original.setDestination(0x2222);
    original.setPriority(Priority::Critical);
    original.setRoute("original/route");
    
    // Test copy construction
    Message copy1(original);
    QCOMPARE(copy1.getType(), original.getType());
    QCOMPARE(copy1.getPayload(), original.getPayload());
    QCOMPARE(copy1.getSource(), original.getSource());
    QCOMPARE(copy1.getDestination(), original.getDestination());
    QVERIFY(copy1.getPriority() == original.getPriority());
    QCOMPARE(copy1.getRoute(), original.getRoute());
    
    // Sequence numbers should be different (each copy gets new sequence)
    QVERIFY(copy1.getSequenceNumber() != original.getSequenceNumber());
    
    // Test assignment
    Message copy2;
    copy2 = original;
    QCOMPARE(copy2.getType(), original.getType());
    QCOMPARE(copy2.getPayload(), original.getPayload());
    
    verifyMessageIntegrity(copy1);
    verifyMessageIntegrity(copy2);
}

void TestMessage::testMessageSerialization()
{
    // Create message with full metadata
    QByteArray payload = createTestPayload(256, 0xDD);
    Message original(555, payload);
    original.setSource(0xAAAAAAAA);
    original.setDestination(0xBBBBBBBB);
    original.setPriority(Priority::Low);
    original.setRoute("serialization/test");
    original.setFlags(0x87654321);
    
    // Serialize to QByteArray
    QByteArray serialized = original.serialize();
    QVERIFY(!serialized.isEmpty());
    QVERIFY(serialized.size() > payload.size()); // Should include metadata
    
    // Deserialize
    Message deserialized;
    QVERIFY(deserialized.deserialize(serialized));
    
    // Verify all fields
    QCOMPARE(deserialized.getType(), original.getType());
    QCOMPARE(deserialized.getPayload(), original.getPayload());
    QCOMPARE(deserialized.getSource(), original.getSource());
    QCOMPARE(deserialized.getDestination(), original.getDestination());
    QVERIFY(deserialized.getPriority() == original.getPriority());
    QCOMPARE(deserialized.getRoute(), original.getRoute());
    QCOMPARE(deserialized.getFlags(), original.getFlags());
    
    // Timestamps should be preserved
    QCOMPARE(deserialized.getTimestamp(), original.getTimestamp());
    
    verifyMessageIntegrity(deserialized);
}

void TestMessage::testZeroCopyMessageCreation()
{
    const int dataSize = 2048;
    std::unique_ptr<uint8_t[]> data(new uint8_t[dataSize]);
    std::fill_n(data.get(), dataSize, 0xEE);
    
    uint8_t* rawPtr = data.get();
    
    // Create zero-copy message (transfers ownership)
    ZeroCopyMessage zcMsg(42, std::move(data), dataSize);
    
    QCOMPARE(zcMsg.getType(), static_cast<uint32_t>(42));
    QCOMPARE(zcMsg.getDataSize(), static_cast<size_t>(dataSize));
    QVERIFY(zcMsg.getData() == rawPtr); // Should be same pointer
    QVERIFY(!data); // Original pointer should be null (ownership transferred)
    
    // Verify data content
    const uint8_t* msgData = zcMsg.getData();
    for (int i = 0; i < dataSize; ++i) {
        QCOMPARE(msgData[i], static_cast<uint8_t>(0xEE));
    }
    
    verifyZeroCopyIntegrity(zcMsg);
}

void TestMessage::testZeroCopyMessageOwnership()
{
    const int dataSize = 1024;
    std::unique_ptr<uint8_t[]> originalData(new uint8_t[dataSize]);
    uint8_t* rawPtr = originalData.get();
    std::fill_n(originalData.get(), dataSize, 0xFF);
    
    // Create zero-copy message
    ZeroCopyMessage msg1(100, std::move(originalData), dataSize);
    QVERIFY(!originalData); // Ownership transferred
    QVERIFY(msg1.getData() == rawPtr);
    
    // Move construction
    ZeroCopyMessage msg2(std::move(msg1));
    QVERIFY(msg2.getData() == rawPtr); // Ownership transferred to msg2
    QVERIFY(msg1.getData() == nullptr); // msg1 should be empty
    QCOMPARE(msg1.getDataSize(), static_cast<size_t>(0));
    
    // Verify msg2 still has the data
    QCOMPARE(msg2.getDataSize(), static_cast<size_t>(dataSize));
    for (int i = 0; i < dataSize; ++i) {
        QCOMPARE(msg2.getData()[i], static_cast<uint8_t>(0xFF));
    }
    
    verifyZeroCopyIntegrity(msg2);
}

void TestMessage::testSharedMessageCreation()
{
    QByteArray payload = createTestPayload(512, 0x77);
    
    // Create shared message
    auto sharedMsg = std::make_shared<Message>(333, payload);
    SharedMessage sm1(sharedMsg);
    
    QVERIFY(sm1.getMessage() != nullptr);
    QCOMPARE(sm1.getMessage()->getType(), static_cast<uint32_t>(333));
    QCOMPARE(sm1.getMessage()->getPayload(), payload);
    
    // Test reference counting
    QCOMPARE(sharedMsg.use_count(), 2L); // sharedMsg and sm1
    
    // Create another shared reference
    SharedMessage sm2(sm1);
    QCOMPARE(sharedMsg.use_count(), 3L); // sharedMsg, sm1, and sm2
    QVERIFY(sm2.getMessage() == sm1.getMessage()); // Same underlying message
    
    verifySharedMessageIntegrity(sm1);
    verifySharedMessageIntegrity(sm2);
}

void TestMessage::testSharedMessageThreadSafety()
{
    QByteArray payload = createTestPayload(1024, 0x99);
    auto sharedMsg = std::make_shared<Message>(444, payload);
    SharedMessage originalSM(sharedMsg);
    
    const int numThreads = 8;
    const int operationsPerThread = 1000;
    std::atomic<int> successfulReads{0};
    std::vector<std::thread> threads;
    
    // Multiple threads accessing shared message
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                // Create local shared reference
                SharedMessage localSM(originalSM);
                
                // Access message data
                if (localSM.getMessage() != nullptr) {
                    uint32_t type = localSM.getMessage()->getType();
                    const QByteArray& msgPayload = localSM.getMessage()->getPayload();
                    
                    if (type == 444 && msgPayload.size() == 1024) {
                        successfulReads.fetch_add(1);
                    }
                }
                
                // Simulate some work
                std::this_thread::yield();
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    QCOMPARE(successfulReads.load(), numThreads * operationsPerThread);
}

void TestMessage::testMessageCreationPerformance()
{
    const int iterations = PERFORMANCE_ITERATIONS;
    QByteArray testPayload = createTestPayload(256);
    
    measureMessageOperationLatency(iterations, [&]() {
        Message msg(123, testPayload);
        // Use the message to prevent optimization
        volatile uint32_t type = msg.getType();
        Q_UNUSED(type);
    }, "Basic Message Creation");
}

void TestMessage::testZeroCopyPerformance()
{
    const int iterations = 10000; // Fewer iterations due to memory allocation
    const int dataSize = 1024;
    
    measureMessageOperationLatency(iterations, [&]() {
        auto data = std::make_unique<uint8_t[]>(dataSize);
        std::fill_n(data.get(), dataSize, 0xAA);
        
        ZeroCopyMessage zcMsg(456, std::move(data), dataSize);
        // Access data to prevent optimization
        volatile const uint8_t* ptr = zcMsg.getData();
        Q_UNUSED(ptr);
    }, "Zero-Copy Message Creation");
}

void TestMessage::testSerializationPerformance()
{
    QByteArray payload = createTestPayload(512, 0xBB);
    Message msg(789, payload);
    msg.setSource(0x12345678);
    msg.setDestination(0x87654321);
    msg.setPriority(Priority::Normal);
    msg.setRoute("performance/test");
    
    const int iterations = 10000;
    
    // Test serialization performance
    measureMessageOperationLatency(iterations, [&]() {
        QByteArray serialized = msg.serialize();
        // Access data to prevent optimization
        volatile int size = serialized.size();
        Q_UNUSED(size);
    }, "Message Serialization");
    
    // Test deserialization performance
    QByteArray serialized = msg.serialize();
    measureMessageOperationLatency(iterations, [&]() {
        Message deserializedMsg;
        bool success = deserializedMsg.deserialize(serialized);
        Q_UNUSED(success);
    }, "Message Deserialization");
}

void TestMessage::testTimestampAccuracy()
{
    auto before = std::chrono::steady_clock::now();
    Message msg(111);
    auto after = std::chrono::steady_clock::now();
    
    auto msgTimestamp = msg.getTimestamp();
    
    // Message timestamp should be between before and after
    QVERIFY(msgTimestamp >= before);
    QVERIFY(msgTimestamp <= after);
    
    // Test timestamp precision (should be at least microsecond precision)
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    Message msg2(222);
    
    auto timeDiff = msg2.getTimestamp() - msgTimestamp;
    auto timeDiffUs = std::chrono::duration_cast<std::chrono::microseconds>(timeDiff).count();
    
    QVERIFY(timeDiffUs >= 100); // Should capture the sleep duration
    
    qDebug() << "Timestamp accuracy test - time difference:" << timeDiffUs << "microseconds";
}

void TestMessage::testSequenceNumbers()
{
    std::vector<Message> messages;
    messages.reserve(1000);
    
    // Create many messages and verify sequence numbers
    for (int i = 0; i < 1000; ++i) {
        messages.emplace_back(i % 10); // Various types
    }
    
    // Verify all sequence numbers are unique and increasing
    std::set<uint64_t> sequences;
    uint64_t lastSequence = 0;
    
    for (const auto& msg : messages) {
        uint64_t seq = msg.getSequenceNumber();
        QVERIFY(seq > 0);
        QVERIFY(seq > lastSequence); // Should be increasing
        QVERIFY(sequences.find(seq) == sequences.end()); // Should be unique
        
        sequences.insert(seq);
        lastSequence = seq;
    }
    
    QCOMPARE(sequences.size(), static_cast<size_t>(1000));
}

void TestMessage::testLargePayloadHandling()
{
    // Test with large payload (1MB)
    const int largeSize = 1024 * 1024;
    QByteArray largePayload = createTestPayload(largeSize, 0x42);
    
    QElapsedTimer timer;
    timer.start();
    
    Message largeMsg(999, largePayload);
    
    qint64 creationTime = timer.elapsed();
    
    QCOMPARE(largeMsg.getPayload().size(), largeSize);
    QCOMPARE(largeMsg.getType(), static_cast<uint32_t>(999));
    
    // Verify payload integrity
    const QByteArray& msgPayload = largeMsg.getPayload();
    for (int i = 0; i < 100; ++i) { // Check first 100 bytes
        QCOMPARE(static_cast<unsigned char>(msgPayload[i]), static_cast<unsigned char>(0x42));
    }
    
    // Test serialization of large message
    timer.restart();
    QByteArray serialized = largeMsg.serialize();
    qint64 serializationTime = timer.elapsed();
    
    QVERIFY(!serialized.isEmpty());
    QVERIFY(serialized.size() >= largeSize);
    
    qDebug() << "Large payload test - Creation:" << creationTime << "ms"
             << "Serialization:" << serializationTime << "ms"
             << "Size:" << (largeSize / 1024) << "KB";
    
    verifyMessageIntegrity(largeMsg);
}

void TestMessage::testMassiveMessageCreation()
{
    const int massiveCount = STRESS_TEST_ITERATIONS;
    QByteArray payload = createTestPayload(128, 0x33);
    
    std::vector<Message> messages;
    messages.reserve(massiveCount);
    
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < massiveCount; ++i) {
        messages.emplace_back(i % 256, payload);
    }
    
    qint64 elapsedMs = timer.elapsed();
    
    QCOMPARE(static_cast<int>(messages.size()), massiveCount);
    
    // Verify some random messages
    for (int i = 0; i < 100; ++i) {
        int randomIndex = i * (massiveCount / 100);
        if (randomIndex < massiveCount) {
            const Message& msg = messages[randomIndex];
            QCOMPARE(msg.getType(), static_cast<uint32_t>(randomIndex % 256));
            QCOMPARE(msg.getPayload(), payload);
        }
    }
    
    double messagesPerSecond = (massiveCount * 1000.0) / elapsedMs;
    qDebug() << "Massive message creation:" << messagesPerSecond << "messages/second";
    
    // Should create at least 10K messages per second
    QVERIFY(messagesPerSecond > 10000.0);
}

void TestMessage::testConcurrentAccess()
{
    QByteArray payload = createTestPayload(256, 0x66);
    Message sharedMsg(777, payload);
    
    const int numThreads = 8;
    const int accessesPerThread = 1000;
    std::atomic<int> successfulAccesses{0};
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < accessesPerThread; ++i) {
                // Concurrent read access
                uint32_t type = sharedMsg.getType();
                const QByteArray& msgPayload = sharedMsg.getPayload();
                auto timestamp = sharedMsg.getTimestamp();
                uint64_t sequence = sharedMsg.getSequenceNumber();
                
                if (type == 777 && msgPayload.size() == 256 && 
                    timestamp.time_since_epoch().count() > 0 && sequence > 0) {
                    successfulAccesses.fetch_add(1);
                }
                
                std::this_thread::yield();
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    QCOMPARE(successfulAccesses.load(), numThreads * accessesPerThread);
}

void TestMessage::testEmptyMessages()
{
    // Test message with empty payload
    Message emptyMsg(123, QByteArray());
    QCOMPARE(emptyMsg.getType(), static_cast<uint32_t>(123));
    QVERIFY(emptyMsg.getPayload().isEmpty());
    QCOMPARE(emptyMsg.getPayload().size(), 0);
    
    // Test serialization of empty message
    QByteArray serialized = emptyMsg.serialize();
    QVERIFY(!serialized.isEmpty()); // Should have metadata even if payload is empty
    
    Message deserializedEmpty;
    QVERIFY(deserializedEmpty.deserialize(serialized));
    QCOMPARE(deserializedEmpty.getType(), static_cast<uint32_t>(123));
    QVERIFY(deserializedEmpty.getPayload().isEmpty());
    
    verifyMessageIntegrity(emptyMsg);
    verifyMessageIntegrity(deserializedEmpty);
}

void TestMessage::testInvalidOperations()
{
    // Test deserializing invalid data
    Message invalidMsg;
    QByteArray invalidData("this is not valid serialized data");
    QVERIFY(!invalidMsg.deserialize(invalidData));
    
    // Test deserializing empty data
    QByteArray emptyData;
    QVERIFY(!invalidMsg.deserialize(emptyData));
    
    // Test zero-copy with null data
    ZeroCopyMessage nullMsg(100, nullptr, 0);
    QVERIFY(nullMsg.getData() == nullptr);
    QCOMPARE(nullMsg.getDataSize(), static_cast<size_t>(0));
    
    // These should still be valid operations even with null data
    QCOMPARE(nullMsg.getType(), static_cast<uint32_t>(100));
    QVERIFY(nullMsg.getTimestamp().time_since_epoch().count() > 0);
}

// Helper method implementations
void TestMessage::verifyMessageIntegrity(const Message& message)
{
    QVERIFY(message.getTimestamp().time_since_epoch().count() > 0);
    QVERIFY(message.getSequenceNumber() > 0);
    // Type can be any value including 0
    // Payload can be empty or have content
    // Source and destination can be 0 (default)
}

void TestMessage::verifyZeroCopyIntegrity(const ZeroCopyMessage& message)
{
    QVERIFY(message.getTimestamp().time_since_epoch().count() > 0);
    QVERIFY(message.getSequenceNumber() > 0);
    // Data can be null for empty messages
    if (message.getData() != nullptr) {
        QVERIFY(message.getDataSize() > 0);
    } else {
        QCOMPARE(message.getDataSize(), static_cast<size_t>(0));
    }
}

void TestMessage::verifySharedMessageIntegrity(const SharedMessage& message)
{
    if (message.getMessage() != nullptr) {
        verifyMessageIntegrity(*message.getMessage());
    }
}

QByteArray TestMessage::createTestPayload(int size, char pattern)
{
    QByteArray payload(size, pattern);
    return payload;
}

void TestMessage::measureMessageOperationLatency(int iterations, 
                                                const std::function<void()>& operation,
                                                const QString& operationName)
{
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < iterations; ++i) {
        operation();
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double avgLatencyNs = static_cast<double>(elapsedNs) / iterations;
    
    qDebug() << operationName << "- Average latency:" << avgLatencyNs << "nanoseconds"
             << "(" << (iterations * 1000000000LL / elapsedNs) << "ops/second)";
    
    // All message operations should complete in reasonable time
    QVERIFY(avgLatencyNs < 50000.0); // Should be under 50 microseconds
}

QTEST_MAIN(TestMessage)
#include "test_message.moc"