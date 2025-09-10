#include <QtTest/QTest>
#include <QtCore/QObject>
#include <QtTest/QSignalSpy>
#include <QtCore/QThread>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include "../../src/events/event_dispatcher.h"

class TestEventDispatcher : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Basic functionality tests
    void testEventCreation();
    void testEventDispatcherCreation();
    void testEventPosting();
    void testEventProcessing();
    void testEventSubscription();
    void testEventUnsubscription();
    void testEventConsumption();
    
    // Priority and filtering tests
    void testPriorityOrdering();
    void testEventFiltering();
    void testDelayedEvents();
    
    // Handler tests
    void testFunctionHandlers();
    void testQObjectSlotHandlers();
    void testScopedSubscription();
    
    // Performance tests
    void testEventThroughput();
    void testProcessingLatency();
    void testQueueCapacity();
    
    // Error handling tests
    void testQueueOverflow();
    void testInvalidHandlers();
    void testEventDispatcherLifecycle();
    
    // Threading tests
    void testThreadSafety();
    void testCrossThreadEventPosting();
    
public slots:
    // Test slots for QObject handler testing
    void onTestEvent(Monitor::Events::EventPtr event);
    void onApplicationEvent(Monitor::Events::EventPtr event);

private:
    Monitor::Events::EventDispatcher* m_dispatcher = nullptr;
    QStringList m_receivedEvents;
    int m_eventCallCount = 0;
    
    // Helper method to create test events
    Monitor::Events::EventPtr createTestEvent(const QString& type, Monitor::Events::Priority priority = Monitor::Events::Priority::Normal);
};

void TestEventDispatcher::initTestCase()
{
    // Global setup
}

void TestEventDispatcher::cleanupTestCase()
{
    // Global cleanup
}

void TestEventDispatcher::init()
{
    m_dispatcher = new Monitor::Events::EventDispatcher();
    m_receivedEvents.clear();
    m_eventCallCount = 0;
}

void TestEventDispatcher::cleanup()
{
    if (m_dispatcher) {
        m_dispatcher->stop();
        delete m_dispatcher;
        m_dispatcher = nullptr;
    }
}

Monitor::Events::EventPtr TestEventDispatcher::createTestEvent(const QString& type, Monitor::Events::Priority priority)
{
    return std::make_shared<Monitor::Events::Event>(type, priority);
}

void TestEventDispatcher::onTestEvent(Monitor::Events::EventPtr event)
{
    m_receivedEvents.append(event->type());
    m_eventCallCount++;
}

void TestEventDispatcher::onApplicationEvent(Monitor::Events::EventPtr event)
{
    m_receivedEvents.append(QString("App:%1").arg(event->type()));
    m_eventCallCount++;
}

void TestEventDispatcher::testEventCreation()
{
    auto event = createTestEvent("TestType", Monitor::Events::Priority::High);
    
    QVERIFY(event != nullptr);
    QCOMPARE(event->type(), QString("TestType"));
    QCOMPARE(event->priority(), Monitor::Events::Priority::High);
    QVERIFY(!event->isConsumed());
    QVERIFY(!event->timestamp().isNull());
    
    // Test event data
    event->setData("key1", "value1");
    event->setData("key2", 42);
    
    QCOMPARE(event->getData("key1").toString(), QString("value1"));
    QCOMPARE(event->getData("key2").toInt(), 42);
    QCOMPARE(event->getData("nonexistent", "default").toString(), QString("default"));
    
    // Test consumption
    event->consume();
    QVERIFY(event->isConsumed());
}

void TestEventDispatcher::testEventDispatcherCreation()
{
    QVERIFY(m_dispatcher != nullptr);
    QVERIFY(!m_dispatcher->isRunning());
    QVERIFY(!m_dispatcher->isPaused());
    QCOMPARE(m_dispatcher->getQueueSize(), size_t(0));
    QCOMPARE(m_dispatcher->getEventsProcessed(), qint64(0));
}

void TestEventDispatcher::testEventPosting()
{
    m_dispatcher->start();
    
    auto event1 = createTestEvent("Event1");
    auto event2 = createTestEvent("Event2");
    
    m_dispatcher->post(event1);
    m_dispatcher->post(event2);
    
    QVERIFY(m_dispatcher->getQueueSize() >= 1);
    
    // Test null event posting
    m_dispatcher->post(nullptr);
    
    m_dispatcher->stop();
}

void TestEventDispatcher::testEventProcessing()
{
    bool handlerCalled = false;
    QString receivedType;
    
    m_dispatcher->subscribe("ProcessTest", [&handlerCalled, &receivedType](Monitor::Events::EventPtr event) {
        handlerCalled = true;
        receivedType = event->type();
    });
    
    auto event = createTestEvent("ProcessTest");
    bool processed = m_dispatcher->processEvent(event);
    
    QVERIFY(processed);
    QVERIFY(handlerCalled);
    QCOMPARE(receivedType, QString("ProcessTest"));
    
    // Test processing null event
    QVERIFY(!m_dispatcher->processEvent(nullptr));
    
    // Test processing consumed event
    event->consume();
    QVERIFY(!m_dispatcher->processEvent(event));
}

void TestEventDispatcher::testEventSubscription()
{
    int callCount = 0;
    QString lastEventType;
    
    // Test function handler subscription
    m_dispatcher->subscribe("TestSubscription", [&callCount, &lastEventType](Monitor::Events::EventPtr event) {
        callCount++;
        lastEventType = event->type();
    });
    
    auto event = createTestEvent("TestSubscription");
    m_dispatcher->processEvent(event);
    
    QCOMPARE(callCount, 1);
    QCOMPARE(lastEventType, QString("TestSubscription"));
    
    // Test QObject slot subscription
    m_dispatcher->subscribe("QObjectTest", this, SLOT(onTestEvent(Monitor::Events::EventPtr)));
    
    auto qobjectEvent = createTestEvent("QObjectTest");
    m_dispatcher->processEvent(qobjectEvent);
    
    QCOMPARE(m_eventCallCount, 1);
    QVERIFY(m_receivedEvents.contains("QObjectTest"));
}

void TestEventDispatcher::testEventUnsubscription()
{
    // Subscribe to an event
    m_dispatcher->subscribe("UnsubTest", this, SLOT(onTestEvent(Monitor::Events::EventPtr)));
    
    auto event1 = createTestEvent("UnsubTest");
    m_dispatcher->processEvent(event1);
    QCOMPARE(m_eventCallCount, 1);
    
    // Unsubscribe
    m_dispatcher->unsubscribe("UnsubTest", this);
    
    auto event2 = createTestEvent("UnsubTest");
    m_dispatcher->processEvent(event2);
    QCOMPARE(m_eventCallCount, 1); // Should not have increased
    
    // Test unsubscribe all
    m_dispatcher->subscribe("Test1", this, SLOT(onTestEvent(Monitor::Events::EventPtr)));
    m_dispatcher->subscribe("Test2", this, SLOT(onApplicationEvent(Monitor::Events::EventPtr)));
    
    auto test1Event = createTestEvent("Test1");
    auto test2Event = createTestEvent("Test2");
    
    m_dispatcher->processEvent(test1Event);
    m_dispatcher->processEvent(test2Event);
    QCOMPARE(m_eventCallCount, 3); // 1 + 2 new events
    
    m_dispatcher->unsubscribeAll(this);
    
    auto test3Event = createTestEvent("Test1");
    m_dispatcher->processEvent(test3Event);
    QCOMPARE(m_eventCallCount, 3); // Should not have changed
}

void TestEventDispatcher::testEventConsumption()
{
    int handler1Calls = 0;
    int handler2Calls = 0;
    
    // First handler consumes the event
    m_dispatcher->subscribe("ConsumeTest", [&handler1Calls](Monitor::Events::EventPtr event) {
        handler1Calls++;
        event->consume();
    });
    
    // Second handler should not be called due to consumption
    m_dispatcher->subscribe("ConsumeTest", [&handler2Calls](Monitor::Events::EventPtr) {
        handler2Calls++;
    });
    
    auto event = createTestEvent("ConsumeTest");
    m_dispatcher->processEvent(event);
    
    QCOMPARE(handler1Calls, 1);
    QCOMPARE(handler2Calls, 0); // Should not be called
    QVERIFY(event->isConsumed());
}

void TestEventDispatcher::testPriorityOrdering()
{
    m_dispatcher->start();
    
    QStringList processOrder;
    
    m_dispatcher->subscribe("PriorityTest", [&processOrder](Monitor::Events::EventPtr event) {
        processOrder.append(event->getData("id").toString());
    });
    
    // Post events in reverse priority order
    auto lowEvent = createTestEvent("PriorityTest", Monitor::Events::Priority::Low);
    lowEvent->setData("id", "Low");
    
    auto highEvent = createTestEvent("PriorityTest", Monitor::Events::Priority::High);
    highEvent->setData("id", "High");
    
    auto normalEvent = createTestEvent("PriorityTest", Monitor::Events::Priority::Normal);
    normalEvent->setData("id", "Normal");
    
    auto criticalEvent = createTestEvent("PriorityTest", Monitor::Events::Priority::Critical);
    criticalEvent->setData("id", "Critical");
    
    // Post in random order
    m_dispatcher->post(lowEvent);
    m_dispatcher->post(highEvent);
    m_dispatcher->post(normalEvent);
    m_dispatcher->post(criticalEvent);
    
    // Process all events
    m_dispatcher->processQueuedEventsFor("PriorityTest");
    
    // Verify priority order (highest first)
    QVERIFY(processOrder.size() >= 4);
    QVERIFY(processOrder.indexOf("Critical") < processOrder.indexOf("High"));
    QVERIFY(processOrder.indexOf("High") < processOrder.indexOf("Normal"));
    QVERIFY(processOrder.indexOf("Normal") < processOrder.indexOf("Low"));
    
    m_dispatcher->stop();
}

void TestEventDispatcher::testEventFiltering()
{
    int acceptedEvents = 0;
    int totalEvents = 0;
    
    // Set up filter to only accept events with specific data
    m_dispatcher->setEventFilter("FilterTest", [](Monitor::Events::EventPtr event) {
        return event->getData("accept").toBool();
    });
    
    m_dispatcher->subscribe("FilterTest", [&acceptedEvents](Monitor::Events::EventPtr) {
        acceptedEvents++;
    });
    
    // Create events, some acceptable, some not
    for (int i = 0; i < 10; ++i) {
        auto event = createTestEvent("FilterTest");
        event->setData("accept", i % 2 == 0); // Accept even numbered events
        m_dispatcher->processEvent(event);
        totalEvents++;
    }
    
    QCOMPARE(totalEvents, 10);
    QCOMPARE(acceptedEvents, 5); // Only half should be accepted
    
    // Remove filter and test again
    m_dispatcher->removeEventFilter("FilterTest");
    
    for (int i = 0; i < 5; ++i) {
        auto event = createTestEvent("FilterTest");
        event->setData("accept", false); // These would be filtered if filter was active
        m_dispatcher->processEvent(event);
    }
    
    QCOMPARE(acceptedEvents, 10); // All 5 new events should be accepted
}

void TestEventDispatcher::testDelayedEvents()
{
    m_dispatcher->start();
    
    QStringList receivedOrder;
    
    m_dispatcher->subscribe("DelayedTest", [&receivedOrder](Monitor::Events::EventPtr event) {
        receivedOrder.append(event->getData("id").toString());
    });
    
    // Post immediate event
    auto immediateEvent = createTestEvent("DelayedTest");
    immediateEvent->setData("id", "immediate");
    m_dispatcher->post(immediateEvent);
    
    // Post delayed event
    auto delayedEvent = createTestEvent("DelayedTest");
    delayedEvent->setData("id", "delayed");
    m_dispatcher->postDelayed(delayedEvent, 100); // 100ms delay
    
    // Process immediate events
    m_dispatcher->processQueuedEventsFor("DelayedTest");
    
    QCOMPARE(receivedOrder.size(), 1);
    QCOMPARE(receivedOrder[0], QString("immediate"));
    
    // Wait for delayed event
    QEventLoop loop;
    QTimer::singleShot(200, &loop, &QEventLoop::quit);
    loop.exec();
    
    // Process delayed events
    m_dispatcher->processQueuedEventsFor("DelayedTest");
    
    QCOMPARE(receivedOrder.size(), 2);
    QCOMPARE(receivedOrder[1], QString("delayed"));
    
    m_dispatcher->stop();
}

void TestEventDispatcher::testFunctionHandlers()
{
    int functionCallCount = 0;
    QString lastEventData;
    
    auto handler = [&functionCallCount, &lastEventData](Monitor::Events::EventPtr event) {
        functionCallCount++;
        lastEventData = event->getData("test").toString();
    };
    
    m_dispatcher->subscribe("FunctionTest", handler);
    
    auto event = createTestEvent("FunctionTest");
    event->setData("test", "function_data");
    
    m_dispatcher->processEvent(event);
    
    QCOMPARE(functionCallCount, 1);
    QCOMPARE(lastEventData, QString("function_data"));
}

void TestEventDispatcher::testQObjectSlotHandlers()
{
    m_dispatcher->subscribe("SlotTest", this, SLOT(onTestEvent(Monitor::Events::EventPtr)));
    
    auto event = createTestEvent("SlotTest");
    m_dispatcher->processEvent(event);
    
    QCOMPARE(m_eventCallCount, 1);
    QVERIFY(m_receivedEvents.contains("SlotTest"));
}

void TestEventDispatcher::testScopedSubscription()
{
    int callCount = 0;
    
    {
        Monitor::Events::ScopedEventSubscription subscription(
            m_dispatcher, "ScopedTest", 
            [&callCount](Monitor::Events::EventPtr) { callCount++; }
        );
        
        auto event = createTestEvent("ScopedTest");
        m_dispatcher->processEvent(event);
        QCOMPARE(callCount, 1);
        
    } // subscription goes out of scope here
    
    // Handler should no longer be active
    auto event2 = createTestEvent("ScopedTest");
    m_dispatcher->processEvent(event2);
    QCOMPARE(callCount, 1); // Should not have increased
}

void TestEventDispatcher::testEventThroughput()
{
    m_dispatcher->start();
    
    const int numEvents = 10000;
    QAtomicInt processedCount(0);
    
    m_dispatcher->subscribe("ThroughputTest", [&processedCount](Monitor::Events::EventPtr) {
        processedCount.fetchAndAddRelaxed(1);
    });
    
    QElapsedTimer timer;
    timer.start();
    
    // Post all events
    for (int i = 0; i < numEvents; ++i) {
        auto event = createTestEvent("ThroughputTest");
        event->setData("id", i);
        m_dispatcher->post(event);
    }
    
    // Process all events
    m_dispatcher->processQueuedEventsFor("ThroughputTest");
    
    qint64 elapsed = timer.elapsed();
    
    QCOMPARE(processedCount.loadRelaxed(), numEvents);
    
    double eventsPerSecond = (numEvents * 1000.0) / elapsed;
    qDebug() << "Event throughput:" << eventsPerSecond << "events/second";
    
    // Should process at least 100,000 events per second
    QVERIFY(eventsPerSecond > 100000.0);
    
    m_dispatcher->stop();
}

void TestEventDispatcher::testProcessingLatency()
{
    const int numSamples = 1000;
    QVector<qint64> latencies;
    
    m_dispatcher->subscribe("LatencyTest", [&latencies](Monitor::Events::EventPtr event) {
        auto startTime = event->getData("startTime").toLongLong();
        auto currentTime = QDateTime::currentMSecsSinceEpoch();
        latencies.append(currentTime - startTime);
    });
    
    for (int i = 0; i < numSamples; ++i) {
        auto event = createTestEvent("LatencyTest");
        event->setData("startTime", QDateTime::currentMSecsSinceEpoch());
        
        QElapsedTimer timer;
        timer.start();
        m_dispatcher->processEvent(event);
        qint64 processingTime = timer.nsecsElapsed();
        
        // Processing should be very fast - less than 100μs
        QVERIFY(processingTime < 100000); // 100 microseconds in nanoseconds
    }
    
    QCOMPARE(latencies.size(), numSamples);
    
    // Calculate average latency
    qint64 totalLatency = 0;
    for (qint64 latency : latencies) {
        totalLatency += latency;
    }
    double avgLatency = static_cast<double>(totalLatency) / numSamples;
    
    qDebug() << "Average processing latency:" << avgLatency << "ms";
    
    // Average latency should be very low
    QVERIFY(avgLatency < 10.0); // Less than 10ms on average
}

void TestEventDispatcher::testQueueCapacity()
{
    m_dispatcher->start();
    m_dispatcher->setMaxQueueSize(100);
    
    QSignalSpy overflowSpy(m_dispatcher, &Monitor::Events::EventDispatcher::queueOverflow);
    
    // Fill queue beyond capacity
    for (int i = 0; i < 150; ++i) {
        auto event = createTestEvent("CapacityTest");
        m_dispatcher->post(event);
    }
    
    // Should have received overflow signals
    QVERIFY(overflowSpy.count() > 0);
    
    // Queue size should not exceed maximum
    QVERIFY(m_dispatcher->getQueueSize() <= 100);
    
    m_dispatcher->stop();
}

void TestEventDispatcher::testQueueOverflow()
{
    m_dispatcher->start();
    m_dispatcher->setMaxQueueSize(10); // Very small queue for testing
    
    QSignalSpy overflowSpy(m_dispatcher, &Monitor::Events::EventDispatcher::queueOverflow);
    
    // Post more events than queue can handle
    for (int i = 0; i < 20; ++i) {
        auto event = createTestEvent("OverflowTest");
        m_dispatcher->post(event);
    }
    
    QVERIFY(overflowSpy.count() > 0);
    
    m_dispatcher->stop();
}

void TestEventDispatcher::testInvalidHandlers()
{
    // Test subscribing with null receiver
    m_dispatcher->subscribe("InvalidTest", nullptr, SLOT(onTestEvent(Monitor::Events::EventPtr)));
    
    // Should not crash when processing
    auto event = createTestEvent("InvalidTest");
    m_dispatcher->processEvent(event);
    
    // Test unsubscribing null receiver
    m_dispatcher->unsubscribe("InvalidTest", nullptr);
    m_dispatcher->unsubscribeAll(nullptr);
    
    // These should not crash
}

void TestEventDispatcher::testEventDispatcherLifecycle()
{
    QVERIFY(!m_dispatcher->isRunning());
    
    m_dispatcher->start();
    QVERIFY(m_dispatcher->isRunning());
    QVERIFY(!m_dispatcher->isPaused());
    
    m_dispatcher->pause();
    QVERIFY(m_dispatcher->isRunning());
    QVERIFY(m_dispatcher->isPaused());
    
    m_dispatcher->resume();
    QVERIFY(m_dispatcher->isRunning());
    QVERIFY(!m_dispatcher->isPaused());
    
    m_dispatcher->stop();
    QVERIFY(!m_dispatcher->isRunning());
}

void TestEventDispatcher::testThreadSafety()
{
    m_dispatcher->start();
    
    const int numThreads = 4;
    const int eventsPerThread = 1000;
    QAtomicInt totalProcessed(0);
    
    m_dispatcher->subscribe("ThreadTest", [&totalProcessed](Monitor::Events::EventPtr) {
        totalProcessed.fetchAndAddRelaxed(1);
    });
    
    QVector<QThread*> threads;
    
    // Create threads that post events concurrently
    for (int i = 0; i < numThreads; ++i) {
        QThread* thread = QThread::create([this, i]() {
            const int eventsPerThread = 1000;
            for (int j = 0; j < eventsPerThread; ++j) {
                auto event = createTestEvent("ThreadTest");
                event->setData("thread", i);
                event->setData("index", j);
                m_dispatcher->post(event);
            }
        });
        threads.append(thread);
    }
    
    // Start all threads
    for (QThread* thread : threads) {
        thread->start();
    }
    
    // Wait for all threads to complete
    for (QThread* thread : threads) {
        QVERIFY(thread->wait(5000));
        delete thread;
    }
    
    // Process all posted events
    m_dispatcher->processQueuedEventsFor("ThreadTest");
    
    // Verify all events were processed
    QCOMPARE(totalProcessed.loadRelaxed(), numThreads * eventsPerThread);
    
    m_dispatcher->stop();
}

void TestEventDispatcher::testCrossThreadEventPosting()
{
    m_dispatcher->start();
    
    QAtomicInt receivedFromOtherThread(0);
    qint64 mainThreadId = reinterpret_cast<qint64>(QThread::currentThreadId());
    
    m_dispatcher->subscribe("CrossThreadTest", [&receivedFromOtherThread, mainThreadId](Monitor::Events::EventPtr event) {
        qint64 eventThreadId = event->getData("threadId").toLongLong();
        if (eventThreadId != mainThreadId) {
            receivedFromOtherThread.fetchAndAddRelaxed(1);
        }
    });
    
    const int numEvents = 100;
    
    QThread* workerThread = QThread::create([this]() {
        qint64 workerThreadId = reinterpret_cast<qint64>(QThread::currentThreadId());
        const int numEvents = 100;
        
        for (int i = 0; i < numEvents; ++i) {
            auto event = createTestEvent("CrossThreadTest");
            event->setData("threadId", workerThreadId);
            event->setData("index", i);
            m_dispatcher->post(event);
        }
    });
    
    workerThread->start();
    QVERIFY(workerThread->wait(5000));
    delete workerThread;
    
    // Process events on main thread
    m_dispatcher->processQueuedEventsFor("CrossThreadTest");
    
    QCOMPARE(receivedFromOtherThread.loadRelaxed(), numEvents);
    
    m_dispatcher->stop();
}

QTEST_MAIN(TestEventDispatcher)
#include "test_event_dispatcher.moc"