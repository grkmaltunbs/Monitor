#include <QtTest/QtTest>
#include <thread>
#include <chrono>
#include <atomic>

#include "concurrent/mpsc_ring_buffer.h"

using namespace Monitor::Concurrent;

class TestMPSCSimple : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testBasicConstruction();
    void testBasicPushPop();
    void testMultipleProducers();

};

void TestMPSCSimple::initTestCase()
{
    qDebug() << "Starting simple MPSC Ring Buffer tests";
}

void TestMPSCSimple::cleanupTestCase()
{
    qDebug() << "Simple MPSC Ring Buffer tests completed";
}

void TestMPSCSimple::testBasicConstruction()
{
    MPSCRingBuffer<int> buffer(16);
    QCOMPARE(buffer.capacity(), static_cast<size_t>(16));
    QCOMPARE(buffer.size(), static_cast<size_t>(0));
    QVERIFY(buffer.empty());
    QVERIFY(!buffer.full());
}

void TestMPSCSimple::testBasicPushPop()
{
    MPSCRingBuffer<int> buffer(8);
    
    // Test single push/pop
    QVERIFY(buffer.tryPush(42));
    QCOMPARE(buffer.size(), static_cast<size_t>(1));
    QVERIFY(!buffer.empty());
    
    int value;
    QVERIFY(buffer.tryPop(value));
    QCOMPARE(value, 42);
    QCOMPARE(buffer.size(), static_cast<size_t>(0));
    QVERIFY(buffer.empty());
    
    // Test pop from empty
    QVERIFY(!buffer.tryPop(value));
}

void TestMPSCSimple::testMultipleProducers()
{
    const size_t bufferSize = 256;
    const int numProducers = 4;
    const int itemsPerProducer = 100;
    
    MPSCRingBuffer<int> buffer(bufferSize);
    std::atomic<int> totalProduced{0};
    std::atomic<int> totalConsumed{0};
    
    // Start consumer
    std::thread consumer([&]() {
        int value;
        int consumed = 0;
        while (consumed < numProducers * itemsPerProducer) {
            if (buffer.tryPop(value)) {
                consumed++;
                totalConsumed.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    // Start producers
    std::vector<std::thread> producers;
    for (int p = 0; p < numProducers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < itemsPerProducer; ++i) {
                int value = p * 1000 + i;
                while (!buffer.tryPush(value)) {
                    std::this_thread::yield();
                }
                totalProduced.fetch_add(1);
            }
        });
    }
    
    // Wait for completion
    for (auto& producer : producers) {
        producer.join();
    }
    consumer.join();
    
    QCOMPARE(totalProduced.load(), numProducers * itemsPerProducer);
    QCOMPARE(totalConsumed.load(), numProducers * itemsPerProducer);
    QVERIFY(buffer.empty());
}

QTEST_MAIN(TestMPSCSimple)
#include "test_mpsc_simple.moc"