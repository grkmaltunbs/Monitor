#include <QtTest/QTest>
#include <QObject>
#include <QSignalSpy>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include "packet/routing/subscription_manager.h"
#include "packet/core/packet_factory.h"
#include "core/application.h"

using namespace Monitor::Packet;

class TestSubscriptionManager : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testConstruction();
    void testBasicSubscription();
    void testUnsubscription();
    void testEnableDisableSubscription();
    void testMultipleSubscriptionsForPacket();

    // Packet distribution tests
    void testPacketDistribution();
    void testDistributionToMultipleSubscribers();
    void testDistributionPriorityOrdering();
    void testDistributionWithDisabledSubscriptions();
    void testDistributionWithInvalidPacket();

    // Error handling tests
    void testCallbackExceptions();

    // Qt integration tests
    void testSignalEmission();

    // Performance tests
    void testDistributionPerformance();

    // Thread safety tests
    void testConcurrentSubscribe();
    void testConcurrentDistribution();

    // Edge case tests
    void testEmptyManager();
    void testClearAllSubscriptions();
    void testLargeNumberOfSubscribers();

private:
    SubscriptionManager* m_manager = nullptr;
    PacketFactory* m_packetFactory = nullptr;
    Monitor::Core::Application* m_app = nullptr;
    
    static constexpr PacketId TEST_PACKET_ID_1 = 100;
    static constexpr PacketId TEST_PACKET_ID_2 = 200;
    static constexpr size_t PERFORMANCE_ITERATIONS = 10000;
    static constexpr size_t STRESS_SUBSCRIBER_COUNT = 1000;
    
    // Test data
    std::atomic<int> m_callbackCounter{0};
    std::atomic<int> m_exceptionCounter{0};
    PacketPtr m_lastReceivedPacket{nullptr};
    
    // Helper methods
    PacketPtr createTestPacket(PacketId id = TEST_PACKET_ID_1, size_t payloadSize = 512);
    void resetCounters();
    void basicCallback(PacketPtr packet);
    void countingCallback(PacketPtr packet);
    void exceptionThrowingCallback(PacketPtr packet);
    void priorityCallback(PacketPtr packet);
};

void TestSubscriptionManager::initTestCase() {
    // Initialize application for memory management
    m_app = Monitor::Core::Application::instance();
    QVERIFY(m_app != nullptr);
    
    auto memoryManager = m_app->memoryManager();
    QVERIFY(memoryManager != nullptr);
    
    m_packetFactory = new PacketFactory(memoryManager);
    QVERIFY(m_packetFactory != nullptr);
}

void TestSubscriptionManager::cleanupTestCase() {
    delete m_packetFactory;
    m_packetFactory = nullptr;
    // Application cleanup handled by destructor
}

void TestSubscriptionManager::init() {
    // Create fresh manager for each test
    m_manager = new SubscriptionManager();
    QVERIFY(m_manager != nullptr);
    
    resetCounters();
}

void TestSubscriptionManager::cleanup() {
    delete m_manager;
    m_manager = nullptr;
    m_lastReceivedPacket.reset();
}

PacketPtr TestSubscriptionManager::createTestPacket(PacketId id, size_t payloadSize) {
    const char* testData = "Test packet payload data";
    size_t dataSize = std::min(payloadSize, strlen(testData));
    
    auto result = m_packetFactory->createPacket(id, testData, dataSize);
    return result.packet;
}

void TestSubscriptionManager::resetCounters() {
    m_callbackCounter = 0;
    m_exceptionCounter = 0;
    m_lastReceivedPacket.reset();
}

void TestSubscriptionManager::basicCallback(PacketPtr packet) {
    m_lastReceivedPacket = packet;
}

void TestSubscriptionManager::countingCallback(PacketPtr packet) {
    m_callbackCounter++;
    m_lastReceivedPacket = packet;
}

void TestSubscriptionManager::exceptionThrowingCallback(PacketPtr /*packet*/) {
    m_exceptionCounter++;
    throw std::runtime_error("Test exception from callback");
}

void TestSubscriptionManager::priorityCallback(PacketPtr packet) {
    // Used for priority ordering tests
    m_callbackCounter++;
    m_lastReceivedPacket = packet;
}

void TestSubscriptionManager::testConstruction() {
    // Test basic construction
    QVERIFY(m_manager != nullptr);
    
    // Test initial state
    QCOMPARE(m_manager->getTotalSubscriberCount(), size_t(0));
    QCOMPARE(m_manager->getSubscriberCount(TEST_PACKET_ID_1), size_t(0));
    
    // Test initial statistics
    const auto& stats = m_manager->getStatistics();
    QCOMPARE(stats.totalSubscriptions.load(), uint64_t(0));
    QCOMPARE(stats.activeSubscriptions.load(), uint64_t(0));
    QCOMPARE(stats.packetsDistributed.load(), uint64_t(0));
    QCOMPARE(stats.deliveryFailures.load(), uint64_t(0));
    
    // Test that it's a proper QObject
    QVERIFY(qobject_cast<QObject*>(m_manager) != nullptr);
}

void TestSubscriptionManager::testBasicSubscription() {
    // Test valid subscription
    auto callback = [this](PacketPtr packet) { basicCallback(packet); };
    auto id = m_manager->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    
    QVERIFY(id != 0);
    QCOMPARE(m_manager->getTotalSubscriberCount(), size_t(1));
    QCOMPARE(m_manager->getSubscriberCount(TEST_PACKET_ID_1), size_t(1));
    
    // Test subscription retrieval
    auto subscription = m_manager->getSubscription(id);
    QVERIFY(subscription != nullptr);
    QCOMPARE(subscription->id, id);
    QCOMPARE(subscription->name, std::string("TestSubscriber"));
    QCOMPARE(subscription->packetId, TEST_PACKET_ID_1);
    QCOMPARE(subscription->priority, uint32_t(0));
    QVERIFY(subscription->enabled);
    QCOMPARE(subscription->packetsReceived.load(), uint64_t(0));
    QCOMPARE(subscription->packetsDropped.load(), uint64_t(0));
    
    // Test statistics update
    const auto& stats = m_manager->getStatistics();
    QCOMPARE(stats.totalSubscriptions.load(), uint64_t(1));
    QCOMPARE(stats.activeSubscriptions.load(), uint64_t(1));
    
    // Test subscription for different packet ID
    auto id2 = m_manager->subscribe("TestSubscriber2", TEST_PACKET_ID_2, callback);
    QVERIFY(id2 != 0);
    QVERIFY(id2 != id);
    QCOMPARE(m_manager->getTotalSubscriberCount(), size_t(2));
    QCOMPARE(m_manager->getSubscriberCount(TEST_PACKET_ID_2), size_t(1));
}

void TestSubscriptionManager::testUnsubscription() {
    // Create subscription first
    auto callback = [this](PacketPtr packet) { basicCallback(packet); };
    auto id = m_manager->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    QVERIFY(id != 0);
    QCOMPARE(m_manager->getTotalSubscriberCount(), size_t(1));
    
    // Test successful unsubscription
    QVERIFY(m_manager->unsubscribe(id));
    QCOMPARE(m_manager->getTotalSubscriberCount(), size_t(0));
    QCOMPARE(m_manager->getSubscriberCount(TEST_PACKET_ID_1), size_t(0));
    
    // Test subscription no longer exists
    auto subscription = m_manager->getSubscription(id);
    QVERIFY(subscription == nullptr);
    
    // Test unsubscribing non-existent ID
    QVERIFY(!m_manager->unsubscribe(9999));
    QVERIFY(!m_manager->unsubscribe(id)); // Already removed
    
    // Test statistics update
    const auto& stats = m_manager->getStatistics();
    QCOMPARE(stats.activeSubscriptions.load(), uint64_t(0));
}

void TestSubscriptionManager::testEnableDisableSubscription() {
    // Create subscription
    auto callback = [this](PacketPtr packet) { countingCallback(packet); };
    auto id = m_manager->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    QVERIFY(id != 0);
    
    // Test initial enabled state
    auto subscription = m_manager->getSubscription(id);
    QVERIFY(subscription->enabled);
    
    // Test packet delivery when enabled
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    size_t delivered = m_manager->distributePacket(packet);
    QCOMPARE(delivered, size_t(1));
    QCOMPARE(m_callbackCounter.load(), 1);
    
    // Test disabling subscription
    QVERIFY(m_manager->enableSubscription(id, false));
    QVERIFY(!subscription->enabled);
    
    // Test no packet delivery when disabled
    resetCounters();
    delivered = m_manager->distributePacket(packet);
    QCOMPARE(delivered, size_t(0)); // No delivery to disabled subscription
    QCOMPARE(m_callbackCounter.load(), 0);
    
    // Test re-enabling subscription
    QVERIFY(m_manager->enableSubscription(id, true));
    QVERIFY(subscription->enabled);
    
    // Test packet delivery resumed
    delivered = m_manager->distributePacket(packet);
    QCOMPARE(delivered, size_t(1));
    QCOMPARE(m_callbackCounter.load(), 1);
    
    // Test invalid subscription ID
    QVERIFY(!m_manager->enableSubscription(9999, true));
}

void TestSubscriptionManager::testMultipleSubscriptionsForPacket() {
    // Create multiple subscriptions for same packet ID
    auto callback1 = [this](PacketPtr packet) { countingCallback(packet); };
    auto callback2 = [this](PacketPtr packet) { countingCallback(packet); };
    auto callback3 = [this](PacketPtr packet) { countingCallback(packet); };
    
    auto id1 = m_manager->subscribe("Subscriber1", TEST_PACKET_ID_1, callback1, 0);
    auto id2 = m_manager->subscribe("Subscriber2", TEST_PACKET_ID_1, callback2, 1);
    auto id3 = m_manager->subscribe("Subscriber3", TEST_PACKET_ID_1, callback3, 2);
    
    QVERIFY(id1 != 0 && id2 != 0 && id3 != 0);
    QCOMPARE(m_manager->getSubscriberCount(TEST_PACKET_ID_1), size_t(3));
    
    // Test all subscribers receive packet
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    size_t delivered = m_manager->distributePacket(packet);
    QCOMPARE(delivered, size_t(3));
    QCOMPARE(m_callbackCounter.load(), 3);
    
    // Test getSubscribersForPacket
    auto subscribers = m_manager->getSubscribersForPacket(TEST_PACKET_ID_1);
    QCOMPARE(subscribers.size(), size_t(3));
    
    // Remove one subscription
    QVERIFY(m_manager->unsubscribe(id2));
    QCOMPARE(m_manager->getSubscriberCount(TEST_PACKET_ID_1), size_t(2));
    
    resetCounters();
    delivered = m_manager->distributePacket(packet);
    QCOMPARE(delivered, size_t(2));
    QCOMPARE(m_callbackCounter.load(), 2);
}

void TestSubscriptionManager::testPacketDistribution() {
    // Create subscription
    auto callback = [this](PacketPtr packet) { basicCallback(packet); };
    auto id = m_manager->subscribe("TestSubscriber", TEST_PACKET_ID_1, callback);
    
    // Test distribution to subscribed packet type
    auto packet = createTestPacket(TEST_PACKET_ID_1, 256);
    size_t delivered = m_manager->distributePacket(packet);
    QCOMPARE(delivered, size_t(1));
    QVERIFY(m_lastReceivedPacket != nullptr);
    QCOMPARE(m_lastReceivedPacket->id(), TEST_PACKET_ID_1);
    
    // Test no distribution to non-subscribed packet type
    resetCounters();
    auto otherPacket = createTestPacket(TEST_PACKET_ID_2, 256);
    delivered = m_manager->distributePacket(otherPacket);
    QCOMPARE(delivered, size_t(0));
    QVERIFY(m_lastReceivedPacket == nullptr);
    
    // Test subscription statistics
    auto subscription = m_manager->getSubscription(id);
    QCOMPARE(subscription->packetsReceived.load(), uint64_t(1));
    QCOMPARE(subscription->packetsDropped.load(), uint64_t(0));
    QVERIFY(subscription->lastDeliveryTime.load() > 0);
}

void TestSubscriptionManager::testDistributionToMultipleSubscribers() {
    const int subscriberCount = 5;
    std::atomic<int> callbackCounts[subscriberCount];
    std::vector<SubscriptionManager::SubscriberId> subscriptions;
    
    // Initialize counters
    for (int i = 0; i < subscriberCount; ++i) {
        callbackCounts[i] = 0;
    }
    
    // Create multiple subscribers for same packet ID
    for (int i = 0; i < subscriberCount; ++i) {
        auto callback = [&callbackCounts, i](PacketPtr /*packet*/) {
            callbackCounts[i]++;
        };
        
        auto id = m_manager->subscribe(
            QString("Subscriber%1").arg(i).toStdString(), 
            TEST_PACKET_ID_1, 
            callback
        );
        subscriptions.push_back(id);
    }
    
    QCOMPARE(m_manager->getSubscriberCount(TEST_PACKET_ID_1), size_t(subscriberCount));
    
    // Distribute packet
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    size_t delivered = m_manager->distributePacket(packet);
    QCOMPARE(delivered, size_t(subscriberCount));
    
    // Verify all subscribers received packet
    for (int i = 0; i < subscriberCount; ++i) {
        QCOMPARE(callbackCounts[i].load(), 1);
    }
    
    // Test global statistics
    const auto& stats = m_manager->getStatistics();
    QCOMPARE(stats.packetsDistributed.load(), uint64_t(1));
    QVERIFY(stats.averageDeliveryTimeNs.load() > 0);
}

void TestSubscriptionManager::testDistributionPriorityOrdering() {
    std::vector<int> executionOrder;
    std::mutex orderMutex;
    
    // Create subscribers with different priorities
    auto lowPriorityCallback = [&](PacketPtr /*packet*/) {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(1); // Low priority
    };
    
    auto highPriorityCallback = [&](PacketPtr /*packet*/) {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(3); // High priority
    };
    
    auto mediumPriorityCallback = [&](PacketPtr /*packet*/) {
        std::lock_guard<std::mutex> lock(orderMutex);
        executionOrder.push_back(2); // Medium priority
    };
    
    // Subscribe in random order but with different priorities
    m_manager->subscribe("LowPriority", TEST_PACKET_ID_1, lowPriorityCallback, 1);   // Priority 1
    m_manager->subscribe("HighPriority", TEST_PACKET_ID_1, highPriorityCallback, 3); // Priority 3
    m_manager->subscribe("MediumPriority", TEST_PACKET_ID_1, mediumPriorityCallback, 2); // Priority 2
    
    // Distribute packet
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    size_t delivered = m_manager->distributePacket(packet);
    QCOMPARE(delivered, size_t(3));
    
    // Verify execution order (highest priority first)
    QCOMPARE(executionOrder.size(), size_t(3));
    QCOMPARE(executionOrder[0], 3); // High priority first
    QCOMPARE(executionOrder[1], 2); // Medium priority second
    QCOMPARE(executionOrder[2], 1); // Low priority last
}

void TestSubscriptionManager::testDistributionWithDisabledSubscriptions() {
    // Create multiple subscriptions
    auto callback1 = [this](PacketPtr packet) { countingCallback(packet); };
    auto callback2 = [this](PacketPtr packet) { countingCallback(packet); };
    auto callback3 = [this](PacketPtr packet) { countingCallback(packet); };
    
    /*auto id1 =*/ m_manager->subscribe("Sub1", TEST_PACKET_ID_1, callback1);
    auto id2 = m_manager->subscribe("Sub2", TEST_PACKET_ID_1, callback2);
    /*auto id3 =*/ m_manager->subscribe("Sub3", TEST_PACKET_ID_1, callback3);
    
    // Disable middle subscription
    QVERIFY(m_manager->enableSubscription(id2, false));
    
    // Distribute packet
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    size_t delivered = m_manager->distributePacket(packet);
    QCOMPARE(delivered, size_t(2)); // Only enabled subscriptions
    QCOMPARE(m_callbackCounter.load(), 2);
    
    // Verify disabled subscription didn't receive packet
    auto subscription2 = m_manager->getSubscription(id2);
    QCOMPARE(subscription2->packetsReceived.load(), uint64_t(0));
}

void TestSubscriptionManager::testDistributionWithInvalidPacket() {
    auto callback = [this](PacketPtr packet) { countingCallback(packet); };
    m_manager->subscribe("TestSub", TEST_PACKET_ID_1, callback);
    
    // Test with null packet
    size_t delivered = m_manager->distributePacket(nullptr);
    QCOMPARE(delivered, size_t(0));
    QCOMPARE(m_callbackCounter.load(), 0);
    
    // Test with invalid packet (would need actual invalid packet implementation)
    // For now, just test that we handle the case
    const auto& stats = m_manager->getStatistics();
    QVERIFY(stats.deliveryFailures.load() > 0); // Should have recorded failure
}

void TestSubscriptionManager::testCallbackExceptions() {
    // Create subscription with exception-throwing callback
    auto exceptionCallback = [this](PacketPtr packet) { exceptionThrowingCallback(packet); };
    auto normalCallback = [this](PacketPtr packet) { countingCallback(packet); };
    
    auto id1 = m_manager->subscribe("ExceptionSub", TEST_PACKET_ID_1, exceptionCallback);
    auto id2 = m_manager->subscribe("NormalSub", TEST_PACKET_ID_1, normalCallback);
    
    // Distribute packet
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    size_t delivered = m_manager->distributePacket(packet);
    
    // Should deliver to both, but one throws exception
    QCOMPARE(delivered, size_t(2));
    QCOMPARE(m_exceptionCounter.load(), 1);
    QCOMPARE(m_callbackCounter.load(), 1);
    
    // Verify exception statistics
    auto subscription1 = m_manager->getSubscription(id1);
    QCOMPARE(subscription1->packetsDropped.load(), uint64_t(1)); // Exception counted as dropped
    
    auto subscription2 = m_manager->getSubscription(id2);
    QCOMPARE(subscription2->packetsReceived.load(), uint64_t(1));
    QCOMPARE(subscription2->packetsDropped.load(), uint64_t(0));
    
    // Global failure statistics
    const auto& stats = m_manager->getStatistics();
    QVERIFY(stats.deliveryFailures.load() > 0);
}

void TestSubscriptionManager::testSignalEmission() {
    // Set up signal spies
    QSignalSpy addedSpy(m_manager, &SubscriptionManager::subscriptionAdded);
    QSignalSpy removedSpy(m_manager, &SubscriptionManager::subscriptionRemoved);
    QSignalSpy clearedSpy(m_manager, &SubscriptionManager::allSubscriptionsCleared);
    
    // Test subscription added signal
    auto callback = [](PacketPtr /*packet*/) {};
    auto id = m_manager->subscribe("TestSub", TEST_PACKET_ID_1, callback, 5);
    
    QCOMPARE(addedSpy.count(), 1);
    auto addedArgs = addedSpy.takeFirst();
    QCOMPARE(addedArgs.at(0).value<SubscriptionManager::SubscriberId>(), id);
    QCOMPARE(addedArgs.at(1).toString(), QString("TestSub"));
    QCOMPARE(addedArgs.at(2).value<PacketId>(), TEST_PACKET_ID_1);
    
    // Test subscription removed signal
    QVERIFY(m_manager->unsubscribe(id));
    
    QCOMPARE(removedSpy.count(), 1);
    auto removedArgs = removedSpy.takeFirst();
    QCOMPARE(removedArgs.at(0).value<SubscriptionManager::SubscriberId>(), id);
    QCOMPARE(removedArgs.at(1).toString(), QString("TestSub"));
    QCOMPARE(removedArgs.at(2).value<PacketId>(), TEST_PACKET_ID_1);
    
    // Test clear all subscriptions signal
    m_manager->subscribe("Sub1", TEST_PACKET_ID_1, callback);
    m_manager->subscribe("Sub2", TEST_PACKET_ID_2, callback);
    addedSpy.clear();
    
    m_manager->clearAllSubscriptions();
    QCOMPARE(clearedSpy.count(), 1);
    QCOMPARE(m_manager->getTotalSubscriberCount(), size_t(0));
}

void TestSubscriptionManager::testDistributionPerformance() {
    const size_t subscriberCount = 100;
    std::vector<std::atomic<int>> counters(subscriberCount);
    
    // Create many subscribers
    for (size_t i = 0; i < subscriberCount; ++i) {
        counters[i] = 0;
        auto callback = [&counters, i](PacketPtr /*packet*/) {
            counters[i]++;
        };
        
        m_manager->subscribe(
            QString("PerfSub%1").arg(i).toStdString(),
            TEST_PACKET_ID_1,
            callback
        );
    }
    
    // Performance test
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    QElapsedTimer timer;
    timer.start();
    
    for (size_t i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        size_t delivered = m_manager->distributePacket(packet);
        QCOMPARE(delivered, subscriberCount);
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerDistribution = static_cast<double>(elapsedNs) / PERFORMANCE_ITERATIONS;
    
    qDebug() << "SubscriptionManager distribution performance:" 
             << nsPerDistribution << "ns per distribution to"
             << subscriberCount << "subscribers";
    
    // Should be able to distribute to 100 subscribers in < 100μs
    QVERIFY(nsPerDistribution < 100000.0);
    
    // Verify all subscribers received all packets
    for (size_t i = 0; i < subscriberCount; ++i) {
        QCOMPARE(counters[i].load(), int(PERFORMANCE_ITERATIONS));
    }
}

void TestSubscriptionManager::testConcurrentSubscribe() {
    const int threadCount = 8;
    const int subscriptionsPerThread = 100;
    std::vector<std::thread> threads;
    std::vector<std::vector<SubscriptionManager::SubscriberId>> threadSubscriptions(threadCount);
    
    // Create concurrent subscriptions
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([this, t, &threadSubscriptions]() {
            for (int i = 0; i < subscriptionsPerThread; ++i) {
                auto callback = [](PacketPtr /*packet*/) {};
                auto id = m_manager->subscribe(
                    QString("Thread%1Sub%2").arg(t).arg(i).toStdString(),
                    TEST_PACKET_ID_1,
                    callback
                );
                threadSubscriptions[t].push_back(id);
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all subscriptions were created
    QCOMPARE(m_manager->getTotalSubscriberCount(), 
             size_t(threadCount * subscriptionsPerThread));
    QCOMPARE(m_manager->getSubscriberCount(TEST_PACKET_ID_1), 
             size_t(threadCount * subscriptionsPerThread));
    
    // Test concurrent unsubscription
    std::vector<std::thread> unsubThreads;
    for (int t = 0; t < threadCount; ++t) {
        unsubThreads.emplace_back([this, t, &threadSubscriptions]() {
            for (auto id : threadSubscriptions[t]) {
                QVERIFY(m_manager->unsubscribe(id));
            }
        });
    }
    
    for (auto& thread : unsubThreads) {
        thread.join();
    }
    
    // Verify all subscriptions were removed
    QCOMPARE(m_manager->getTotalSubscriberCount(), size_t(0));
}

void TestSubscriptionManager::testConcurrentDistribution() {
    const int subscriberCount = 50;
    std::atomic<int> totalCallbacks{0};
    
    // Create subscribers
    for (int i = 0; i < subscriberCount; ++i) {
        auto callback = [&totalCallbacks](PacketPtr /*packet*/) {
            totalCallbacks++;
        };
        
        m_manager->subscribe(
            QString("ConcSub%1").arg(i).toStdString(),
            TEST_PACKET_ID_1,
            callback
        );
    }
    
    const int distributionThreads = 4;
    const int distributionsPerThread = 100;
    std::vector<std::thread> threads;
    
    // Concurrent packet distribution
    for (int t = 0; t < distributionThreads; ++t) {
        threads.emplace_back([this]() {
            auto packet = createTestPacket(TEST_PACKET_ID_1);
            
            for (int i = 0; i < distributionsPerThread; ++i) {
                size_t delivered = m_manager->distributePacket(packet);
                QCOMPARE(delivered, size_t(subscriberCount));
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify total callbacks
    int expectedCallbacks = distributionThreads * distributionsPerThread * subscriberCount;
    QCOMPARE(totalCallbacks.load(), expectedCallbacks);
    
    // Verify statistics
    const auto& stats = m_manager->getStatistics();
    QCOMPARE(stats.packetsDistributed.load(), 
             uint64_t(distributionThreads * distributionsPerThread));
}

void TestSubscriptionManager::testEmptyManager() {
    // Test operations on empty manager
    QCOMPARE(m_manager->getTotalSubscriberCount(), size_t(0));
    QCOMPARE(m_manager->getSubscriberCount(TEST_PACKET_ID_1), size_t(0));
    
    auto subscription = m_manager->getSubscription(999);
    QVERIFY(subscription == nullptr);
    
    auto subscribers = m_manager->getSubscribersForPacket(TEST_PACKET_ID_1);
    QVERIFY(subscribers.empty());
    
    auto allSubs = m_manager->getAllSubscriptions();
    QVERIFY(allSubs.empty());
    
    // Test packet distribution to empty manager
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    size_t delivered = m_manager->distributePacket(packet);
    QCOMPARE(delivered, size_t(0));
    
    // Test clearing empty manager
    m_manager->clearAllSubscriptions(); // Should not crash
}

void TestSubscriptionManager::testClearAllSubscriptions() {
    // Create several subscriptions
    auto callback = [](PacketPtr /*packet*/) {};
    
    auto id1 = m_manager->subscribe("Sub1", TEST_PACKET_ID_1, callback);
    auto id2 = m_manager->subscribe("Sub2", TEST_PACKET_ID_1, callback);
    auto id3 = m_manager->subscribe("Sub3", TEST_PACKET_ID_2, callback);
    
    QCOMPARE(m_manager->getTotalSubscriberCount(), size_t(3));
    
    // Set up signal spy
    QSignalSpy clearedSpy(m_manager, &SubscriptionManager::allSubscriptionsCleared);
    
    // Clear all subscriptions
    m_manager->clearAllSubscriptions();
    
    // Verify everything is cleared
    QCOMPARE(m_manager->getTotalSubscriberCount(), size_t(0));
    QCOMPARE(m_manager->getSubscriberCount(TEST_PACKET_ID_1), size_t(0));
    QCOMPARE(m_manager->getSubscriberCount(TEST_PACKET_ID_2), size_t(0));
    
    // Verify subscriptions no longer exist
    QVERIFY(m_manager->getSubscription(id1) == nullptr);
    QVERIFY(m_manager->getSubscription(id2) == nullptr);
    QVERIFY(m_manager->getSubscription(id3) == nullptr);
    
    // Verify signal was emitted
    QCOMPARE(clearedSpy.count(), 1);
    
    // Verify statistics
    const auto& stats = m_manager->getStatistics();
    QCOMPARE(stats.activeSubscriptions.load(), uint64_t(0));
}

void TestSubscriptionManager::testLargeNumberOfSubscribers() {
    const size_t largeCount = STRESS_SUBSCRIBER_COUNT;
    std::vector<SubscriptionManager::SubscriberId> subscriptions;
    std::atomic<int> callbackCount{0};
    
    // Create many subscribers
    auto callback = [&callbackCount](PacketPtr /*packet*/) {
        callbackCount++;
    };
    
    QElapsedTimer timer;
    timer.start();
    
    for (size_t i = 0; i < largeCount; ++i) {
        auto id = m_manager->subscribe(
            QString("StressSub%1").arg(i).toStdString(),
            TEST_PACKET_ID_1,
            callback,
            i % 5 // Varying priorities
        );
        subscriptions.push_back(id);
    }
    
    qint64 subscriptionTime = timer.nsecsElapsed();
    qDebug() << "Created" << largeCount << "subscriptions in" 
             << (subscriptionTime / 1000000) << "ms";
    
    QCOMPARE(m_manager->getTotalSubscriberCount(), largeCount);
    
    // Test packet distribution to all subscribers
    auto packet = createTestPacket(TEST_PACKET_ID_1);
    timer.restart();
    
    size_t delivered = m_manager->distributePacket(packet);
    
    qint64 distributionTime = timer.nsecsElapsed();
    qDebug() << "Distributed packet to" << delivered << "subscribers in" 
             << (distributionTime / 1000000) << "ms";
    
    QCOMPARE(delivered, largeCount);
    QCOMPARE(callbackCount.load(), int(largeCount));
    
    // Should handle large number of subscribers efficiently
    QVERIFY(distributionTime < 100000000); // < 100ms
    
    // Clean up
    timer.restart();
    m_manager->clearAllSubscriptions();
    qint64 cleanupTime = timer.nsecsElapsed();
    qDebug() << "Cleared all subscriptions in" << (cleanupTime / 1000000) << "ms";
    
    QCOMPARE(m_manager->getTotalSubscriberCount(), size_t(0));
}

QTEST_MAIN(TestSubscriptionManager)
#include "test_subscription_manager.moc"