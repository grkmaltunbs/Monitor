#include <QtTest/QtTest>
#include <QtCore/QElapsedTimer>
#include <QtCore/QSignalSpy>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <memory>

#include "messaging/message_channel.h"
#include "messaging/message.h"

using namespace Monitor::Messaging;

class TestMessageChannel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // SPSC Channel tests
    void testSPSCChannelCreation();
    void testSPSCChannelBasicOperations();
    void testSPSCChannelPerformance();
    void testSPSCChannelThreadSafety();
    void testSPSCChannelOverflow();
    
    // MPSC Channel tests
    void testMPSCChannelCreation();
    void testMPSCChannelMultipleProducers();
    void testMPSCChannelPerformance();
    void testMPSCChannelBackpressure();
    void testMPSCChannelFairness();
    
    // Buffered Channel tests
    void testBufferedChannelCreation();
    void testBufferedChannelQueueing();
    void testBufferedChannelThreadSafety();
    void testBufferedChannelPerformance();
    void testBufferedChannelOverflow();
    
    // Signal/Slot integration tests
    void testMessageReceivedSignal();
    void testErrorSignals();
    void testStatisticsSignals();
    void testSignalThreadSafety();
    
    // Configuration tests
    void testChannelConfiguration();
    void testConfigurationValidation();
    void testRuntimeConfigurationChanges();
    
    // Performance tests
    void testHighThroughputSPSC();
    void testHighThroughputMPSC();
    void testLowLatencyOperations();
    void testMemoryEfficiency();
    
    // Statistics and monitoring tests
    void testStatisticsAccuracy();
    void testPerformanceMetrics();
    void testErrorTracking();
    void testCapacityMonitoring();
    
    // Error handling tests
    void testInvalidOperations();
    void testResourceExhaustionHandling();
    void testRecoveryAfterErrors();
    void testConcurrentErrorHandling();
    
    // Stress tests
    void testLongRunningOperations();
    void testMassiveMessageThroughput();
    void testConcurrentChannelOperations();
    void testMemoryPressureHandling();
    
    // Edge cases
    void testEmptyMessages();
    void testLargeMessages();
    void testMaxCapacityOperations();
    void testChannelDestruction();

private:
    // Helper methods
    void sendTestMessages(MessageChannel* channel, int count, int baseValue = 0);
    void receiveTestMessages(MessageChannel* channel, int expectedCount, 
                           std::vector<MessagePtr>& received, int timeoutMs = 5000);
    void verifyMessageOrder(const std::vector<MessagePtr>& messages, int expectedStart, int expectedCount);
    void runMultipleProducers(MessageChannel* channel, int numProducers, int messagesPerProducer);
    void measureChannelLatency(MessageChannel* channel, int iterations, double& avgLatencyNs);
    MessagePtr createTestMessage(uint32_t type, int value);
    
    // Test constants
    static constexpr int PERFORMANCE_ITERATIONS = 10000;
    static constexpr int STRESS_ITERATIONS = 50000;
};

void TestMessageChannel::initTestCase()
{
    qDebug() << "Starting MessageChannel comprehensive tests";
}

void TestMessageChannel::cleanupTestCase()
{
    qDebug() << "MessageChannel tests completed";
}

void TestMessageChannel::init()
{
    // Fresh state for each test
}

void TestMessageChannel::cleanup()
{
    // Cleanup after each test
}

void TestMessageChannel::testSPSCChannelCreation()
{
    // Test default SPSC channel
    auto channel1 = MessageChannel::createSPSCChannel("TestSPSC1");
    QVERIFY(channel1 != nullptr);
    QCOMPARE(channel1->getName(), QString("TestSPSC1"));
    QVERIFY(channel1->getType() == ChannelType::SPSC);
    QVERIFY(channel1->capacity() > 0);
    QCOMPARE(channel1->size(), static_cast<size_t>(0));
    QVERIFY(channel1->isEmpty());
    QVERIFY(!channel1->isFull());
    
    // Test SPSC channel with specific capacity
    auto channel2 = MessageChannel::createSPSCChannel("TestSPSC2", 512);
    QVERIFY(channel2 != nullptr);
    QCOMPARE(channel2->capacity(), static_cast<size_t>(512));
    
    // Test SPSC channel with configuration
    ChannelConfig config;
    config.capacity = 1024;
    config.enableStatistics = true;
    config.dropOnOverflow = false;
    
    auto channel3 = MessageChannel::createSPSCChannel("TestSPSC3", config);
    QVERIFY(channel3 != nullptr);
    QCOMPARE(channel3->capacity(), static_cast<size_t>(1024));
}

void TestMessageChannel::testSPSCChannelBasicOperations()
{
    auto channel = MessageChannel::createSPSCChannel("BasicSPSC", 32);
    
    // Test sending single message
    auto msg1 = createTestMessage(100, 42);
    QVERIFY(channel->send(msg1));
    QCOMPARE(channel->size(), static_cast<size_t>(1));
    QVERIFY(!channel->isEmpty());
    
    // Test receiving single message
    auto received = channel->receive();
    QVERIFY(received != nullptr);
    QCOMPARE(received->getType(), static_cast<uint32_t>(100));
    QCOMPARE(channel->size(), static_cast<size_t>(0));
    QVERIFY(channel->isEmpty());
    
    // Test receive from empty channel
    auto empty = channel->receive();
    QVERIFY(empty == nullptr);
    
    // Test multiple messages
    const int numMessages = 10;
    for (int i = 0; i < numMessages; ++i) {
        auto msg = createTestMessage(200, i);
        QVERIFY(channel->send(msg));
    }
    
    QCOMPARE(channel->size(), static_cast<size_t>(numMessages));
    
    for (int i = 0; i < numMessages; ++i) {
        auto received = channel->receive();
        QVERIFY(received != nullptr);
        QCOMPARE(received->getType(), static_cast<uint32_t>(200));
    }
    
    QVERIFY(channel->isEmpty());
}

void TestMessageChannel::testSPSCChannelThreadSafety()
{
    auto channel = MessageChannel::createSPSCChannel("ThreadSafeSPSC", 1024);
    
    const int numMessages = 10000;
    std::atomic<int> sentMessages{0};
    std::atomic<int> receivedMessages{0};
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < numMessages; ++i) {
            auto msg = createTestMessage(300, i);
            
            // Retry on full buffer
            while (!channel->send(msg)) {
                std::this_thread::yield();
            }
            sentMessages.fetch_add(1);
        }
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int received = 0;
        while (received < numMessages) {
            auto msg = channel->receive();
            if (msg != nullptr) {
                QCOMPARE(msg->getType(), static_cast<uint32_t>(300));
                received++;
                receivedMessages.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    QCOMPARE(sentMessages.load(), numMessages);
    QCOMPARE(receivedMessages.load(), numMessages);
    QVERIFY(channel->isEmpty());
}

void TestMessageChannel::testMPSCChannelMultipleProducers()
{
    auto channel = MessageChannel::createMPSCChannel("MultiProdMPSC", 2048);
    
    const int numProducers = 8;
    const int messagesPerProducer = 1000;
    const int totalMessages = numProducers * messagesPerProducer;
    
    std::atomic<int> totalSent{0};
    std::atomic<int> totalReceived{0};
    std::vector<std::atomic<int>> producerCounts(numProducers);
    
    // Multiple producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&, p]() {
            int sent = 0;
            for (int i = 0; i < messagesPerProducer; ++i) {
                auto msg = createTestMessage(400 + p, i);
                
                while (!channel->send(msg)) {
                    std::this_thread::yield();
                }
                sent++;
                producerCounts[p].fetch_add(1);
            }
            totalSent.fetch_add(sent);
        });
    }
    
    // Single consumer thread
    std::thread consumer([&]() {
        std::vector<int> typeCounters(numProducers, 0);
        int received = 0;
        
        while (received < totalMessages) {
            auto msg = channel->receive();
            if (msg != nullptr) {
                uint32_t type = msg->getType();
                if (type >= 400 && type < 400 + numProducers) {
                    typeCounters[type - 400]++;
                }
                received++;
                totalReceived.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
        
        // Verify we received messages from all producers
        for (int i = 0; i < numProducers; ++i) {
            QCOMPARE(typeCounters[i], messagesPerProducer);
        }
    });
    
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    
    QCOMPARE(totalSent.load(), totalMessages);
    QCOMPARE(totalReceived.load(), totalMessages);
    
    // Verify all producers sent their messages
    for (int p = 0; p < numProducers; ++p) {
        QCOMPARE(producerCounts[p].load(), messagesPerProducer);
    }
}

void TestMessageChannel::testBufferedChannelQueueing()
{
    ChannelConfig config;
    config.capacity = 16;
    config.enableQueueing = true;
    config.maxQueueSize = 1000;
    
    auto channel = MessageChannel::createBufferedChannel("QueuedBuffer", config);
    
    // Fill the buffer
    for (size_t i = 0; i < channel->capacity(); ++i) {
        auto msg = createTestMessage(500, static_cast<int>(i));
        QVERIFY(channel->send(msg));
    }
    
    QVERIFY(channel->isFull());
    
    // Send additional messages (should be queued)
    const int queuedMessages = 50;
    for (int i = 0; i < queuedMessages; ++i) {
        auto msg = createTestMessage(600, i);
        QVERIFY(channel->send(msg)); // Should succeed due to queueing
    }
    
    // Consume all messages
    int totalReceived = 0;
    int type500Count = 0, type600Count = 0;
    
    MessagePtr msg;
    while ((msg = channel->receive()) != nullptr) {
        if (msg->getType() == 500) type500Count++;
        else if (msg->getType() == 600) type600Count++;
        totalReceived++;
    }
    
    QCOMPARE(type500Count, static_cast<int>(channel->capacity()));
    QCOMPARE(type600Count, queuedMessages);
    QCOMPARE(totalReceived, static_cast<int>(channel->capacity()) + queuedMessages);
}

void TestMessageChannel::testMessageReceivedSignal()
{
    auto channel = MessageChannel::createSPSCChannel("SignalTest", 64);
    
    QSignalSpy spy(channel.get(), &MessageChannel::messageReceived);
    QVERIFY(spy.isValid());
    
    // Send some messages
    const int numMessages = 5;
    for (int i = 0; i < numMessages; ++i) {
        auto msg = createTestMessage(700, i);
        QVERIFY(channel->send(msg));
    }
    
    // Process events to ensure signals are emitted
    QCoreApplication::processEvents();
    
    // Should have received signals for each message
    QVERIFY(spy.count() >= numMessages);
    
    // Verify signal parameters
    for (int i = 0; i < spy.count() && i < numMessages; ++i) {
        QList<QVariant> arguments = spy.at(i);
        QCOMPARE(arguments.count(), 1);
        
        // The signal should contain a pointer to the message
        const Message* msgPtr = arguments.at(0).value<const Message*>();
        QVERIFY(msgPtr != nullptr);
        QCOMPARE(msgPtr->getType(), static_cast<uint32_t>(700));
    }
}

void TestMessageChannel::testHighThroughputSPSC()
{
    auto channel = MessageChannel::createSPSCChannel("HighThroughputSPSC", 4096);
    
    const int numMessages = PERFORMANCE_ITERATIONS;
    std::atomic<int> sentCount{0};
    std::atomic<int> receivedCount{0};
    
    QElapsedTimer timer;
    timer.start();
    
    // High-speed producer
    std::thread producer([&]() {
        for (int i = 0; i < numMessages; ++i) {
            auto msg = createTestMessage(800, i);
            while (!channel->send(msg)) {
                // Busy wait for maximum performance
            }
            sentCount.fetch_add(1);
        }
    });
    
    // High-speed consumer
    std::thread consumer([&]() {
        int received = 0;
        while (received < numMessages) {
            auto msg = channel->receive();
            if (msg != nullptr) {
                received++;
                receivedCount.fetch_add(1);
            }
            // No yield for maximum performance
        }
    });
    
    producer.join();
    consumer.join();
    
    qint64 elapsedMs = timer.elapsed();
    
    QCOMPARE(sentCount.load(), numMessages);
    QCOMPARE(receivedCount.load(), numMessages);
    
    double throughput = (numMessages * 1000.0) / elapsedMs;
    QVERIFY(throughput > 100000.0); // Should handle >100K messages/second
    
    qDebug() << "SPSC high throughput test:" << throughput << "messages/second";
}

void TestMessageChannel::testLowLatencyOperations()
{
    auto channel = MessageChannel::createSPSCChannel("LowLatencySPSC", 256);
    
    double avgLatencyNs = 0.0;
    measureChannelLatency(channel.get(), 1000, avgLatencyNs);
    
    // Target: < 10 microseconds for send-receive cycle
    QVERIFY(avgLatencyNs < 10000.0);
    
    qDebug() << "SPSC average send-receive latency:" << avgLatencyNs << "nanoseconds";
}

void TestMessageChannel::testStatisticsAccuracy()
{
    ChannelConfig config;
    config.capacity = 128;
    config.enableStatistics = true;
    
    auto channel = MessageChannel::createSPSCChannel("StatsSPSC", config);
    
    const int numMessages = 100;
    
    // Send messages
    for (int i = 0; i < numMessages; ++i) {
        auto msg = createTestMessage(900, i);
        QVERIFY(channel->send(msg));
    }
    
    auto stats = channel->getStatistics();
    QCOMPARE(stats.messagesSent, static_cast<size_t>(numMessages));
    QVERIFY(stats.totalSendTimeNs > 0);
    
    // Receive messages
    for (int i = 0; i < numMessages; ++i) {
        auto msg = channel->receive();
        QVERIFY(msg != nullptr);
    }
    
    stats = channel->getStatistics();
    QCOMPARE(stats.messagesReceived, static_cast<size_t>(numMessages));
    QVERIFY(stats.totalReceiveTimeNs > 0);
    QCOMPARE(stats.messagesDropped, static_cast<size_t>(0)); // No drops expected
    
    // Calculate averages
    double avgSendTimeNs = static_cast<double>(stats.totalSendTimeNs) / stats.messagesSent;
    double avgReceiveTimeNs = static_cast<double>(stats.totalReceiveTimeNs) / stats.messagesReceived;
    
    qDebug() << "Statistics - Avg send time:" << avgSendTimeNs << "ns"
             << "Avg receive time:" << avgReceiveTimeNs << "ns";
    
    QVERIFY(avgSendTimeNs > 0);
    QVERIFY(avgReceiveTimeNs > 0);
}

void TestMessageChannel::testInvalidOperations()
{
    auto channel = MessageChannel::createSPSCChannel("InvalidTest", 32);
    
    // Test sending null message
    MessagePtr nullMsg;
    QVERIFY(!channel->send(nullMsg));
    
    // Test operations on destroyed channel
    channel.reset();
    // channel is now null, but we can't easily test operations on it
    // since we'd get a crash rather than graceful handling
}

void TestMessageChannel::testLongRunningOperations()
{
    auto channel = MessageChannel::createMPSCChannel("LongRunningMPSC", 1024);
    
    const int durationSeconds = 3;
    const int numProducers = 4;
    
    std::atomic<bool> running{true};
    std::atomic<long> totalSent{0};
    std::atomic<long> totalReceived{0};
    
    // Multiple producers
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&, p]() {
            int counter = 0;
            while (running.load()) {
                auto msg = createTestMessage(1000 + p, counter++);
                if (channel->send(msg)) {
                    totalSent.fetch_add(1);
                }
                
                // Small delay to prevent overwhelming
                if (counter % 1000 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        });
    }
    
    // Consumer
    std::thread consumer([&]() {
        while (running.load() || !channel->isEmpty()) {
            auto msg = channel->receive();
            if (msg != nullptr) {
                totalReceived.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
    running.store(false);
    
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    
    QCOMPARE(totalSent.load(), totalReceived.load());
    QVERIFY(totalSent.load() > 0);
    
    double messagesPerSecond = totalSent.load() / static_cast<double>(durationSeconds);
    qDebug() << "Long running test - Total messages:" << totalSent.load()
             << "Rate:" << messagesPerSecond << "messages/second";
    
    // Should maintain reasonable throughput
    QVERIFY(messagesPerSecond > 1000.0);
}

void TestMessageChannel::testChannelDestruction()
{
    std::unique_ptr<MessageChannel> channel;
    
    // Test destruction with pending messages
    {
        channel = MessageChannel::createSPSCChannel("DestructionTest", 64);
        
        // Add some messages
        for (int i = 0; i < 10; ++i) {
            auto msg = createTestMessage(1100, i);
            channel->send(msg);
        }
        
        QVERIFY(!channel->isEmpty());
    } // Channel destroyed here with pending messages
    
    // Should not crash - RAII should handle cleanup
    channel.reset();
}

void TestMessageChannel::testMPSCChannelBackpressure()
{
    // Small buffer to trigger backpressure quickly
    auto channel = MessageChannel::createMPSCChannel("BackpressureMPSC", 32);
    
    const int numProducers = 8;
    const int attemptsPerProducer = 200;
    
    std::atomic<int> successfulSends{0};
    std::atomic<int> failedSends{0};
    
    // Fill buffer first
    while (!channel->isFull()) {
        auto msg = createTestMessage(1200, 0);
        if (!channel->send(msg)) break;
    }
    
    // Multiple producers trying to send to full buffer
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < attemptsPerProducer; ++i) {
                auto msg = createTestMessage(1200 + p, i);
                if (channel->send(msg)) {
                    successfulSends.fetch_add(1);
                } else {
                    failedSends.fetch_add(1);
                }
                
                // Small delay between attempts
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }
    
    // Start consumer after delay to create initial backpressure
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    std::thread consumer([&]() {
        MessagePtr msg;
        int received = 0;
        
        // Consume slowly to maintain some backpressure
        while ((msg = channel->receive()) != nullptr || 
               successfulSends.load() + failedSends.load() < numProducers * attemptsPerProducer) {
            if (msg != nullptr) {
                received++;
                std::this_thread::sleep_for(std::chrono::microseconds(5)); // Slow consumer
            }
        }
        
        qDebug() << "Backpressure test - Consumed:" << received << "messages";
    });
    
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    
    int totalAttempts = numProducers * attemptsPerProducer;
    QCOMPARE(successfulSends.load() + failedSends.load(), totalAttempts);
    QVERIFY(failedSends.load() > 0); // Should have experienced backpressure
    
    qDebug() << "Backpressure results - Successful:" << successfulSends.load()
             << "Failed:" << failedSends.load()
             << "Backpressure ratio:" << (100.0 * failedSends.load() / totalAttempts) << "%";
}

// Helper method implementations
MessagePtr TestMessageChannel::createTestMessage(uint32_t type, int value)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream << value;
    
    return std::make_unique<Message>(type, payload);
}

void TestMessageChannel::measureChannelLatency(MessageChannel* channel, int iterations, double& avgLatencyNs)
{
    std::vector<double> latencies;
    latencies.reserve(iterations);
    
    for (int i = 0; i < iterations; ++i) {
        auto msg = createTestMessage(9999, i);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        channel->send(msg);
        auto received = channel->receive();
        
        auto end = std::chrono::high_resolution_clock::now();
        
        QVERIFY(received != nullptr);
        QCOMPARE(received->getType(), static_cast<uint32_t>(9999));
        
        double latencyNs = std::chrono::duration<double, std::nano>(end - start).count();
        latencies.push_back(latencyNs);
    }
    
    double sum = 0.0;
    for (double latency : latencies) {
        sum += latency;
    }
    avgLatencyNs = sum / latencies.size();
}

QTEST_MAIN(TestMessageChannel)
#include "test_message_channel.moc"