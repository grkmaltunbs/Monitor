#include <QtTest/QtTest>
#include <QtCore/QCoreApplication>
#include <thread>
#include <chrono>

#include "threading/thread_pool.h"

using namespace Monitor::Threading;

class TestThreadPoolSimple : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testBasicInitialization();
    void testSimpleTaskSubmission();
    void testStartPauseResume();
    void testSchedulingPolicySetGet();

private:
    ThreadPool* m_threadPool;
};

void TestThreadPoolSimple::initTestCase()
{
    qDebug() << "Starting simple ThreadPool tests";
}

void TestThreadPoolSimple::cleanupTestCase()
{
    qDebug() << "Simple ThreadPool tests completed";
}

void TestThreadPoolSimple::init()
{
    m_threadPool = new ThreadPool(this);
    QVERIFY(m_threadPool != nullptr);
}

void TestThreadPoolSimple::cleanup()
{
    if (m_threadPool) {
        m_threadPool->shutdown();
        delete m_threadPool;
        m_threadPool = nullptr;
    }
}

void TestThreadPoolSimple::testBasicInitialization()
{
    QVERIFY(m_threadPool->initialize(4));
    QCOMPARE(m_threadPool->getNumThreads(), static_cast<size_t>(4));
    
    m_threadPool->start();
    QVERIFY(m_threadPool->isRunning());
    QVERIFY(!m_threadPool->isPaused());
}

void TestThreadPoolSimple::testSimpleTaskSubmission()
{
    m_threadPool->initialize(2);
    m_threadPool->start();
    
    std::atomic<int> counter{0};
    
    // Submit a simple task
    TaskFunction task = [&counter]() {
        counter.fetch_add(1);
    };
    
    QVERIFY(m_threadPool->submitTask(task));
    
    // Wait a bit for task completion
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    QCOMPARE(counter.load(), 1);
}

void TestThreadPoolSimple::testStartPauseResume()
{
    m_threadPool->initialize(2);
    
    // Test start
    m_threadPool->start();
    QVERIFY(m_threadPool->isRunning());
    QVERIFY(!m_threadPool->isPaused());
    
    // Test pause
    m_threadPool->pause();
    QVERIFY(m_threadPool->isPaused());
    
    // Test resume
    m_threadPool->resume();
    QVERIFY(!m_threadPool->isPaused());
}

void TestThreadPoolSimple::testSchedulingPolicySetGet()
{
    // Test default policy
    ThreadPool::SchedulingPolicy defaultPolicy = m_threadPool->getSchedulingPolicy();
    QVERIFY(defaultPolicy == ThreadPool::SchedulingPolicy::RoundRobin ||
            defaultPolicy == ThreadPool::SchedulingPolicy::LeastLoaded ||
            defaultPolicy == ThreadPool::SchedulingPolicy::Random ||
            defaultPolicy == ThreadPool::SchedulingPolicy::WorkStealing);
    
    // Test setting different policies
    m_threadPool->setSchedulingPolicy(ThreadPool::SchedulingPolicy::WorkStealing);
    QVERIFY(m_threadPool->getSchedulingPolicy() == ThreadPool::SchedulingPolicy::WorkStealing);
    
    m_threadPool->setSchedulingPolicy(ThreadPool::SchedulingPolicy::LeastLoaded);
    QVERIFY(m_threadPool->getSchedulingPolicy() == ThreadPool::SchedulingPolicy::LeastLoaded);
}

QTEST_MAIN(TestThreadPoolSimple)
#include "test_thread_pool_simple.moc"