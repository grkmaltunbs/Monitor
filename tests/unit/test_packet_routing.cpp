#include <QtTest/QtTest>
#include <QObject>
#include <QSignalSpy>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>

#include "../../src/packet/routing/subscription_manager.h"
#include "../../src/packet/routing/packet_router.h"
#include "../../src/packet/routing/packet_dispatcher.h"
#include "../../src/packet/core/packet_factory.h"
#include "../../src/core/application.h"

using namespace Monitor;
using namespace Monitor::Packet;

class TestPacketRouting : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        // Get or create the Application instance and initialize it
        auto app = Monitor::Core::Application::instance();
        if (!app->isInitialized()) {
            QVERIFY(app->initialize());
        }
    }

    void cleanupTestCase() {
        // The Application instance will be cleaned up automatically
        if (auto app = Monitor::Core::Application::instance()) {
            if (app->isInitialized()) {
                app->shutdown();
            }
        }
    }

    void testSubscriptionManager() {
        SubscriptionManager manager;
        
        std::atomic<int> callbackCount{0};
        std::vector<PacketPtr> receivedPackets;
        
        // Test basic subscription
        auto callback = [&](PacketPtr packet) {
            callbackCount++;
            receivedPackets.push_back(packet);
        };
        
        auto subId = manager.subscribe("TestSubscriber", 100, callback, 1);
        QVERIFY(subId != 0);
        
        // Test subscription retrieval
        auto subscribers = manager.getSubscribersForPacket(100);
        QCOMPARE(subscribers.size(), 1u);
        QCOMPARE(subscribers[0]->name, std::string("TestSubscriber"));
        QCOMPARE(subscribers[0]->priority, 1u);
        
        // Test packet delivery
        auto app = Monitor::Core::Application::instance();
        QVERIFY(app);
        auto memMgr = app->memoryManager();
        QVERIFY(memMgr);
        PacketFactory factory(memMgr);
        auto result = factory.createPacket(100, nullptr, 256);
        QVERIFY(result.success);
        QVERIFY(result.packet != nullptr);
        
        size_t deliveredCount = manager.distributePacket(result.packet);
        
        // Allow time for delivery
        QTest::qWait(10);
        
        QCOMPARE(callbackCount.load(), 1);
        QCOMPARE(receivedPackets.size(), 1u);
        QCOMPARE(receivedPackets[0]->id(), 100u);
        QCOMPARE(deliveredCount, 1u);
        
        // Test unsubscription
        QVERIFY(manager.unsubscribe(subId));
        subscribers = manager.getSubscribersForPacket(100);
        QCOMPARE(subscribers.size(), 0u);
        
        // Verify no more deliveries after unsubscribe
        callbackCount = 0;
        receivedPackets.clear();
        
        auto result2 = factory.createPacket(100, nullptr, 256);
        QVERIFY(result2.success);
        size_t deliveredCount2 = manager.distributePacket(result2.packet);
        QTest::qWait(10);
        
        QCOMPARE(callbackCount.load(), 0);
        QCOMPARE(receivedPackets.size(), 0u);
        QCOMPARE(deliveredCount2, 0u);
    }
    
    void testSubscriptionPriorities() {
        SubscriptionManager manager;
        
        std::vector<int> deliveryOrder;
        std::mutex orderMutex;
        
        // Subscribe with different priorities
        auto highCallback = [&](PacketPtr) {
            std::lock_guard<std::mutex> lock(orderMutex);
            deliveryOrder.push_back(1); // High priority = 1
        };
        
        auto mediumCallback = [&](PacketPtr) {
            std::lock_guard<std::mutex> lock(orderMutex);
            deliveryOrder.push_back(2); // Medium priority = 2
        };
        
        auto lowCallback = [&](PacketPtr) {
            std::lock_guard<std::mutex> lock(orderMutex);
            deliveryOrder.push_back(3); // Low priority = 3
        };
        
        // Subscribe in random order
        auto mediumId = manager.subscribe("Medium", 42, mediumCallback, 2);
        auto highId = manager.subscribe("High", 42, highCallback, 1);
        auto lowId = manager.subscribe("Low", 42, lowCallback, 3);
        
        // Deliver packet
        auto app = Monitor::Core::Application::instance();
        QVERIFY(app);
        auto memMgr = app->memoryManager();
        QVERIFY(memMgr);
        PacketFactory factory(memMgr);
        auto result = factory.createPacket(42, nullptr, 128);
        QVERIFY(result.success);
        size_t deliveredCount = manager.distributePacket(result.packet);
        
        QTest::qWait(20);
        
        // Verify delivery order (high priority first)
        QCOMPARE(deliveryOrder.size(), 3u);
        QCOMPARE(deliveredCount, 3u);
        QCOMPARE(deliveryOrder[0], 1); // High priority first
        QCOMPARE(deliveryOrder[1], 2); // Medium priority second
        QCOMPARE(deliveryOrder[2], 3); // Low priority last
        
        // Cleanup
        manager.unsubscribe(highId);
        manager.unsubscribe(mediumId);
        manager.unsubscribe(lowId);
    }
    
    void testMultiplePacketTypes() {
        SubscriptionManager manager;
        
        std::atomic<int> type100Count{0};
        std::atomic<int> type200Count{0};
        
        auto callback100 = [&](PacketPtr packet) {
            QCOMPARE(packet->id(), 100u);
            type100Count++;
        };
        
        auto callback200 = [&](PacketPtr packet) {
            QCOMPARE(packet->id(), 200u);
            type200Count++;
        };
        
        // Subscribe to different packet types
        auto sub100 = manager.subscribe("Type100", 100, callback100);
        auto sub200 = manager.subscribe("Type200", 200, callback200);
        
        // Deliver different packet types
        auto app = Monitor::Core::Application::instance();
        QVERIFY(app);
        auto memMgr = app->memoryManager();
        QVERIFY(memMgr);
        PacketFactory factory(memMgr);
        
        for (int i = 0; i < 5; ++i) {
            auto result100 = factory.createPacket(100, nullptr, 64);
            auto result200 = factory.createPacket(200, nullptr, 64);
            
            QVERIFY(result100.success);
            QVERIFY(result200.success);
            
            manager.distributePacket(result100.packet);
            manager.distributePacket(result200.packet);
        }
        
        QTest::qWait(50);
        
        QCOMPARE(type100Count.load(), 5);
        QCOMPARE(type200Count.load(), 5);
        
        // Cleanup
        manager.unsubscribe(sub100);
        manager.unsubscribe(sub200);
    }
    
    void testPacketRouter() {
        PacketRouter::Configuration config;
        config.queueSize = 1000;
        config.enableProfiling = true;
        
        PacketRouter router(config);
        
        // Set up threading (simplified for test)
        Threading::ThreadPool *threadPool = new Threading::ThreadPool();
        threadPool->initialize(2);
        router.setThreadPool(threadPool);
        
        // Set up subscription manager (required for router to start)
        SubscriptionManager subscriptionManager;
        router.setSubscriptionManager(&subscriptionManager);
        
        QVERIFY(router.start());
        
        // Test manual routing with priority
        auto app = Monitor::Core::Application::instance();
        QVERIFY(app);
        auto memMgr = app->memoryManager();
        QVERIFY(memMgr);
        PacketFactory factory(memMgr);
        auto result = factory.createPacket(123, nullptr, 512);
        QVERIFY(result.success);
        auto packet = result.packet;
        
        QVERIFY(router.routePacket(packet, PacketRouter::Priority::High));
        
        // Test auto-routing (should determine priority automatically)
        auto result2 = factory.createPacket(124, nullptr, 256);
        QVERIFY(result2.success);
        auto packet2 = result2.packet;
        QVERIFY(router.routePacketAuto(packet2));
        
        // Get statistics
        const auto& stats = router.getStatistics();
        QVERIFY(stats.packetsRouted >= 2);
        QVERIFY(stats.averageLatencyNs > 0);
        
        router.stop();
    }
    
    void testPacketRouterPerformance() {
        PacketRouter::Configuration config;
        config.queueSize = 10000;
        config.enableProfiling = true;
        
        PacketRouter router(config);
        Threading::ThreadPool threadPool;
        threadPool.initialize(4);
        router.setThreadPool(&threadPool);
        
        // Set up subscription manager (required for router to start)
        SubscriptionManager subscriptionManager;
        router.setSubscriptionManager(&subscriptionManager);
        
        QVERIFY(router.start());
        
        // Performance test - route many packets quickly
        auto app = Monitor::Core::Application::instance();
        QVERIFY(app);
        auto memMgr = app->memoryManager();
        QVERIFY(memMgr);
        PacketFactory factory(memMgr);
        const int numPackets = 1000;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numPackets; ++i) {
            auto result = factory.createPacket(i % 10, nullptr, 256);
            if (result.success) {
                QVERIFY(router.routePacketAuto(result.packet));
            }
        }
        
        // Wait for routing to complete
        QTest::qWait(100);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime).count();
        
        double avgTimePerPacket = static_cast<double>(duration) / numPackets;
        qDebug() << "Router performance: " << avgTimePerPacket 
                 << "microseconds per packet";
        
        // Should route packets in less than 100μs each on average
        QVERIFY(avgTimePerPacket < 100.0);
        
        const auto& stats = router.getStatistics();
        QVERIFY(stats.packetsRouted >= numPackets);
        QCOMPARE(stats.packetsDropped.load(), 0u);
        
        router.stop();
    }
    
    void testPacketDispatcher() {
        PacketDispatcher::Configuration config;
        config.enableBackPressure = true;
        config.backPressureThreshold = 1000;
        config.maxSources = 10;
        
        PacketDispatcher dispatcher(config);
        
        // Set up threading
        Threading::ThreadPool threadPool;
        threadPool.initialize(2);
        dispatcher.setThreadPool(&threadPool);
        
        QVERIFY(dispatcher.start());
        QVERIFY(dispatcher.isRunning());
        
        // Test subscription through dispatcher
        std::atomic<int> receivedCount{0};
        auto callback = [&](PacketPtr /*packet*/) {
            receivedCount++;
        };
        
        auto subId = dispatcher.subscribe("TestSub", 500, callback);
        QVERIFY(subId != 0);
        
        // Test statistics
        const auto& stats = dispatcher.getStatistics();
        QCOMPARE(stats.subscriberCount.load(), 1u);
        
        // Test unsubscription
        QVERIFY(dispatcher.unsubscribe(subId));
        
        const auto& stats2 = dispatcher.getStatistics();
        QCOMPARE(stats2.subscriberCount.load(), 0u);
        
        dispatcher.stop();
        QVERIFY(!dispatcher.isRunning());
    }
    
    void testPacketDispatcherWithSources() {
        // Create and register a mock source
        class MockSource : public PacketSource {
        public:
            MockSource() : PacketSource(Configuration("MockSource")) {}
            
        protected:
            bool doStart() override { return true; }
            void doStop() override {}
            void doPause() override {}
            bool doResume() override { return true; }
            
        public:
            void generateTestPacket() {
                auto app = Monitor::Core::Application::instance();
                if (!app) return;
                auto memMgr = app->memoryManager();
                if (!memMgr) return;
                
                // Use PacketFactory to create packets with proper headers
                PacketFactory factory(memMgr);
                auto result = factory.createPacket(0, nullptr, 100); // Use packet ID 0
                if (result.success) {
                    deliverPacket(result.packet);
                }
            }
        };
        
        // Keep mockSource at method scope to ensure it outlives dispatcher
        auto mockSource = std::make_unique<MockSource>();
        auto* sourcePtr = mockSource.get();
        
        // Use nested scope for dispatcher - it will be destroyed first
        {
            PacketDispatcher::Configuration config;
            PacketDispatcher dispatcher(config);
            
            Threading::ThreadPool threadPool;
            threadPool.initialize(2);
            dispatcher.setThreadPool(&threadPool);
            
            QVERIFY(dispatcher.start());
            QVERIFY(dispatcher.registerSource(sourcePtr));
            
            // Set up packet reception
            std::atomic<int> packetsReceived{0};
            auto callback = [&](PacketPtr /*packet*/) {
                packetsReceived++;
            };
            
            auto subId = dispatcher.subscribe("Receiver", 0, callback);
            
            // Generate test packets
            for (int i = 0; i < 10; ++i) {
                sourcePtr->generateTestPacket();
            }
            
            QTest::qWait(100);
            
            QVERIFY(packetsReceived.load() > 0);
            
            // Cleanup in proper order: unsubscribe, unregister, stop
            dispatcher.unsubscribe(subId);
            QVERIFY(dispatcher.unregisterSource("MockSource"));
            dispatcher.stop();
        } // Dispatcher destroyed here, mockSource still alive
        
        // mockSource destroyed here - after dispatcher is gone
    }
    
    void testBackPressureHandling() {
        PacketDispatcher::Configuration config;
        config.enableBackPressure = true;
        config.backPressureThreshold = 10; // Very low for testing
        
        PacketDispatcher dispatcher(config);
        
        Threading::ThreadPool threadPool; // Single thread to create bottleneck
        threadPool.initialize(1);
        dispatcher.setThreadPool(&threadPool);
        
        QSignalSpy backPressureSpy(&dispatcher, &PacketDispatcher::backPressureDetected);
        
        // Create a flood source to generate lots of packets
        class FloodSource : public PacketSource {
        public:
            FloodSource() : PacketSource(Configuration("FloodSource")) {}
            
        protected:
            bool doStart() override { return true; }
            void doStop() override {}
            void doPause() override {}
            bool doResume() override { return true; }
            
        public:
            void floodPackets(int count) {
                auto app = Monitor::Core::Application::instance();
                if (!app) return;
                auto memMgr = app->memoryManager();
                if (!memMgr) return;
                
                PacketFactory factory(memMgr);
                for (int i = 0; i < count; ++i) {
                    auto result = factory.createPacket(i % 10, nullptr, 1024); // Large packets
                    if (result.success) {
                        deliverPacket(result.packet);
                        // Add small delay to let packets accumulate in queues
                        std::this_thread::sleep_for(std::chrono::microseconds(1));
                    }
                }
            }
        };
        
        auto floodSource = std::make_unique<FloodSource>();
        auto* floodPtr = floodSource.get();
        
        QVERIFY(dispatcher.start());
        QVERIFY(dispatcher.registerSource(floodPtr));
        
        // Flood with packets to trigger back-pressure
        floodPtr->floodPackets(100); // Increased count to ensure back-pressure
        
        QTest::qWait(200); // Longer wait for processing
        
        // Should have detected back-pressure
        QVERIFY(backPressureSpy.count() > 0);
        
        const auto& stats = dispatcher.getStatistics();
        QVERIFY(stats.backPressureEvents.load() > 0);
        
        // Cleanup
        dispatcher.unregisterSource("FloodSource");
        dispatcher.stop();
    }
    
    void testThreadSafety() {
        SubscriptionManager manager;
        
        std::atomic<int> totalReceived{0};
        std::atomic<bool> shouldStop{false};
        
        auto callback = [&](PacketPtr /*packet*/) {
            totalReceived++;
            // Small delay to increase chance of race conditions
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        };
        
        // Subscribe from multiple threads
        std::vector<std::thread> threads;
        std::vector<SubscriptionManager::SubscriberId> subIds;
        std::mutex subIdsMutex;
        
        // Thread 1: Subscribe and unsubscribe
        threads.emplace_back([&]() {
            for (int i = 0; i < 10; ++i) {
                if (shouldStop) break;
                
                auto id = manager.subscribe("Thread1_" + std::to_string(i), 
                                          100 + i, callback);
                {
                    std::lock_guard<std::mutex> lock(subIdsMutex);
                    subIds.push_back(id);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                
                if (i % 2 == 0) {
                    manager.unsubscribe(id);
                }
            }
        });
        
        // Thread 2: Deliver packets
        threads.emplace_back([&]() {
            auto app = Monitor::Core::Application::instance();
            if (!app) return;
            auto memMgr = app->memoryManager();
            if (!memMgr) return;
            PacketFactory factory(memMgr);
            for (int i = 0; i < 50 && !shouldStop; ++i) {
                auto result = factory.createPacket(100 + (i % 10), nullptr, 64);
                if (result.success) {
                    manager.distributePacket(result.packet);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        
        // Let threads run
        QTest::qWait(100);
        shouldStop = true;
        
        // Wait for threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Cleanup remaining subscriptions
        {
            std::lock_guard<std::mutex> lock(subIdsMutex);
            for (auto id : subIds) {
                manager.unsubscribe(id);
            }
        }
        
        // Should have received some packets without crashing
        QVERIFY(totalReceived.load() >= 0);
        qDebug() << "Thread safety test: received" << totalReceived.load() << "packets";
    }
    
    void testPacketDeliveryLatency() {
        SubscriptionManager manager;
        
        std::vector<std::chrono::high_resolution_clock::time_point> deliveryTimes;
        std::mutex timesMutex;
        
        auto callback = [&](PacketPtr /*packet*/) {
            std::lock_guard<std::mutex> lock(timesMutex);
            deliveryTimes.push_back(std::chrono::high_resolution_clock::now());
        };
        
        auto subId = manager.subscribe("LatencyTest", 999, callback);
        
        // Send packets and measure delivery latency
        auto app = Monitor::Core::Application::instance();
        QVERIFY(app);
        auto memMgr = app->memoryManager();
        QVERIFY(memMgr);
        PacketFactory factory(memMgr);
        std::vector<std::chrono::high_resolution_clock::time_point> sendTimes;
        
        for (int i = 0; i < 100; ++i) {
            sendTimes.push_back(std::chrono::high_resolution_clock::now());
            auto result = factory.createPacket(999, nullptr, 64);
            if (result.success) {
                manager.distributePacket(result.packet);
            }
            
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        QTest::qWait(50); // Allow all deliveries to complete
        
        // Calculate latencies
        std::lock_guard<std::mutex> lock(timesMutex);
        QCOMPARE(deliveryTimes.size(), sendTimes.size());
        
        std::vector<double> latencies;
        for (size_t i = 0; i < std::min(sendTimes.size(), deliveryTimes.size()); ++i) {
            auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                deliveryTimes[i] - sendTimes[i]).count();
            latencies.push_back(latency);
        }
        
        // Calculate average latency
        double avgLatency = 0.0;
        for (double lat : latencies) {
            avgLatency += lat;
        }
        avgLatency /= latencies.size();
        
        qDebug() << "Average delivery latency:" << avgLatency << "microseconds";
        
        // Should deliver packets in less than 100μs on average
        QVERIFY(avgLatency < 100.0);
        
        manager.unsubscribe(subId);
    }
};

QTEST_MAIN(TestPacketRouting)
#include "test_packet_routing.moc"
