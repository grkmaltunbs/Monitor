#include <QtTest/QTest>
#include <QtCore/QObject>
#include <QtTest/QSignalSpy>
#include <QtCore/QThread>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include <QtCore/QMutexLocker>
#include "../../../src/threading/thread_worker.h"
#include "../../../src/threading/thread_pool.h"
#include <atomic>
#include <chrono>
#include <thread>

class TestThreadWorker : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Basic functionality tests
    void testThreadWorkerCreation();
    void testTaskExecution();
    void testTaskPriority();
    void testTaskQueueing();
    void testWorkerStartStop();
    void testWorkerStatistics();
    
    // Performance tests
    void testTaskThroughput();
    void testTaskLatency();
    void testQueueCapacity();
    
    // Work stealing tests
    void testWorkStealing();
    void testTaskStealingStats();
    
    // CPU affinity tests
    void testCpuAffinity();
    
    // Thread safety tests
    void testConcurrentTaskSubmission();
    void testWorkerLifecycle();
    
    // Error handling tests
    void testInvalidTasks();
    void testQueueOverflow();
    void testExceptionHandling();
    
    // Signal emission tests
    void testSignalEmission();

private:
    Monitor::Threading::ThreadPool* m_threadPool = nullptr;
    Monitor::Threading::ThreadWorker* m_worker = nullptr;
    
    void waitForSignal(QSignalSpy& spy, int timeout = 1000);
};

void TestThreadWorker::initTestCase()
{
    // Create a thread pool for testing
    m_threadPool = new Monitor::Threading::ThreadPool(this);
    QVERIFY(m_threadPool->initialize(2));
}

void TestThreadWorker::cleanupTestCase()
{
    if (m_threadPool) {
        m_threadPool->shutdown();
        delete m_threadPool;
        m_threadPool = nullptr;
    }
}

void TestThreadWorker::init()
{
    // Create a fresh worker for each test
    m_worker = new Monitor::Threading::ThreadWorker(0, m_threadPool, this);
    QVERIFY(m_worker != nullptr);
}

void TestThreadWorker::cleanup()
{
    if (m_worker) {
        if (m_worker->isRunning()) {
            m_worker->stop();
            m_worker->wait(1000);
        }
        delete m_worker;
        m_worker = nullptr;
    }
}

void TestThreadWorker::testThreadWorkerCreation()
{
    // Test basic creation
    QVERIFY(m_worker != nullptr);
    QCOMPARE(m_worker->getTasksProcessed(), size_t(0));
    QCOMPARE(m_worker->getTasksStolen(), size_t(0));
    QCOMPARE(m_worker->getQueueSize(), size_t(0));
    QVERIFY(m_worker->isIdle());
    QVERIFY(!m_worker->isRunning());
    QCOMPARE(m_worker->getCpuAffinity(), -1); // No affinity set
}

void TestThreadWorker::testTaskExecution()
{
    std::atomic<int> counter{0};
    std::atomic<bool> taskExecuted{false};
    
    // Create a simple task
    auto task = std::make_shared<Monitor::Threading::Task>(
        [&counter, &taskExecuted]() {
            counter.fetch_add(1);
            taskExecuted = true;
        }, 0, 1
    );
    
    // Start worker and add task
    m_worker->start();
    QVERIFY(m_worker->addTask(task));
    
    // Wait for task execution
    int attempts = 0;
    while (!taskExecuted.load() && attempts < 100) {
        QThread::msleep(10);
        attempts++;
    }
    
    QVERIFY(taskExecuted.load());
    QCOMPARE(counter.load(), 1);
    QCOMPARE(m_worker->getTasksProcessed(), size_t(1));
}

void TestThreadWorker::testTaskPriority()
{
    std::vector<int> executionOrder;
    QMutex orderMutex;
    
    // Create tasks with different priorities
    auto lowPriorityTask = std::make_shared<Monitor::Threading::Task>(
        [&executionOrder, &orderMutex]() {
            QMutexLocker locker(&orderMutex);
            executionOrder.push_back(1);
        }, -10, 1
    );
    
    auto highPriorityTask = std::make_shared<Monitor::Threading::Task>(
        [&executionOrder, &orderMutex]() {
            QMutexLocker locker(&orderMutex);
            executionOrder.push_back(2);
        }, 10, 2
    );
    
    auto normalPriorityTask = std::make_shared<Monitor::Threading::Task>(
        [&executionOrder, &orderMutex]() {
            QMutexLocker locker(&orderMutex);
            executionOrder.push_back(3);
        }, 0, 3
    );
    
    // Add tasks in reverse priority order
    QVERIFY(m_worker->addTask(lowPriorityTask));
    QVERIFY(m_worker->addTask(normalPriorityTask));
    QVERIFY(m_worker->addTask(highPriorityTask));
    
    // Start worker
    m_worker->start();
    
    // Wait for all tasks to complete
    int attempts = 0;
    while (executionOrder.size() < 3 && attempts < 100) {
        QThread::msleep(10);
        attempts++;
    }
    
    QCOMPARE(executionOrder.size(), size_t(3));
    // Higher priority should execute first
    QCOMPARE(executionOrder[0], 2); // High priority
    QCOMPARE(executionOrder[1], 3); // Normal priority
    QCOMPARE(executionOrder[2], 1); // Low priority
}

void TestThreadWorker::testTaskQueueing()
{
    const int numTasks = 10;
    std::atomic<int> completedTasks{0};
    
    // Add multiple tasks before starting worker
    for (int i = 0; i < numTasks; ++i) {
        auto task = std::make_shared<Monitor::Threading::Task>(
            [&completedTasks]() {
                completedTasks.fetch_add(1);
                QThread::msleep(1); // Small delay
            }, 0, i
        );
        QVERIFY(m_worker->addTask(task));
    }
    
    QCOMPARE(m_worker->getQueueSize(), size_t(numTasks));
    
    // Start worker
    m_worker->start();
    
    // Wait for all tasks to complete
    int attempts = 0;
    while (completedTasks.load() < numTasks && attempts < 1000) {
        QThread::msleep(10);
        attempts++;
    }
    
    QCOMPARE(completedTasks.load(), numTasks);
    QCOMPARE(m_worker->getTasksProcessed(), size_t(numTasks));
    QCOMPARE(m_worker->getQueueSize(), size_t(0));
}

void TestThreadWorker::testWorkerStartStop()
{
    // Test starting worker
    QVERIFY(!m_worker->isRunning());
    m_worker->start();
    
    // Give it time to start
    QThread::msleep(50);
    QVERIFY(m_worker->isRunning());
    
    // Test stopping worker
    m_worker->stop();
    m_worker->wait(1000);
    QVERIFY(!m_worker->isRunning());
}

void TestThreadWorker::testWorkerStatistics()
{
    const int numTasks = 5;
    std::atomic<int> completedTasks{0};
    
    m_worker->start();
    
    // Add tasks and measure
    for (int i = 0; i < numTasks; ++i) {
        auto task = std::make_shared<Monitor::Threading::Task>(
            [&completedTasks]() {
                completedTasks.fetch_add(1);
                QThread::msleep(5); // Small processing time
            }, 0, i
        );
        QVERIFY(m_worker->addTask(task));
    }
    
    // Wait for completion
    while (completedTasks.load() < numTasks) {
        QThread::msleep(10);
    }
    
    // Check statistics
    QCOMPARE(m_worker->getTasksProcessed(), size_t(numTasks));
    QVERIFY(m_worker->getAverageTaskTime() > 0.0); // Should have some processing time
}

void TestThreadWorker::testTaskThroughput()
{
    const int numTasks = 1000;
    std::atomic<int> completedTasks{0};
    
    m_worker->start();
    auto startTime = std::chrono::steady_clock::now();
    
    // Submit many simple tasks
    for (int i = 0; i < numTasks; ++i) {
        auto task = std::make_shared<Monitor::Threading::Task>(
            [&completedTasks]() {
                completedTasks.fetch_add(1);
            }, 0, i
        );
        QVERIFY(m_worker->addTask(task));
    }
    
    // Wait for completion
    while (completedTasks.load() < numTasks) {
        QThread::msleep(1);
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Calculate throughput (tasks per second)
    double throughput = (numTasks * 1000.0) / duration.count();
    qDebug() << "Task throughput:" << throughput << "tasks/second";
    
    // Should be able to process at least 1000 tasks/second for simple tasks
    QVERIFY(throughput > 1000.0);
}

void TestThreadWorker::testTaskLatency()
{
    std::atomic<qint64> latencySum{0};
    std::atomic<int> taskCount{0};
    const int numTasks = 100;
    
    m_worker->start();
    
    for (int i = 0; i < numTasks; ++i) {
        auto task = std::make_shared<Monitor::Threading::Task>(
            [&taskCount]() {
                // Simple task
                taskCount.fetch_add(1);
            }, 0, i
        );
        QVERIFY(m_worker->addTask(task));
    }
    
    // Wait for completion
    while (taskCount.load() < numTasks) {
        QThread::msleep(1);
    }
    
    // Average task time should be reasonable (< 1ms for simple tasks)
    double avgTime = m_worker->getAverageTaskTime();
    qDebug() << "Average task execution time:" << avgTime << "nanoseconds";
    
    // Should be less than 1 millisecond for simple tasks
    QVERIFY(avgTime < 1000000.0); // 1ms in nanoseconds
}

void TestThreadWorker::testWorkStealing()
{
    // Create a task that can be stolen
    std::atomic<bool> taskExecuted{false};
    auto task = std::make_shared<Monitor::Threading::Task>(
        [&taskExecuted]() {
            taskExecuted = true;
        }, 0, 1
    );
    
    QVERIFY(m_worker->addTask(task));
    QCOMPARE(m_worker->getQueueSize(), size_t(1));
    
    // Steal the task
    auto stolenTask = m_worker->stealTask();
    QVERIFY(stolenTask != nullptr);
    QCOMPARE(m_worker->getQueueSize(), size_t(0));
    QCOMPARE(m_worker->getTasksStolen(), size_t(1));
    
    // Execute the stolen task manually
    if (stolenTask && stolenTask->function) {
        stolenTask->function();
    }
    
    QVERIFY(taskExecuted.load());
}

void TestThreadWorker::testCpuAffinity()
{
    // Test setting CPU affinity
    m_worker->setCpuAffinity(0);
    QCOMPARE(m_worker->getCpuAffinity(), 0);
    
    m_worker->setCpuAffinity(1);
    QCOMPARE(m_worker->getCpuAffinity(), 1);
    
    m_worker->setCpuAffinity(-1); // Reset
    QCOMPARE(m_worker->getCpuAffinity(), -1);
}

void TestThreadWorker::testConcurrentTaskSubmission()
{
    const int numThreads = 4;
    const int tasksPerThread = 100;
    std::atomic<int> completedTasks{0};
    
    m_worker->start();
    
    // Create multiple threads submitting tasks concurrently
    std::vector<std::thread> submitterThreads;
    
    for (int t = 0; t < numThreads; ++t) {
        submitterThreads.emplace_back([this, &completedTasks, t]() {
            for (int i = 0; i < tasksPerThread; ++i) {
                auto task = std::make_shared<Monitor::Threading::Task>(
                    [&completedTasks]() {
                        completedTasks.fetch_add(1);
                    }, 0, t * tasksPerThread + i
                );
                
                // Keep trying to submit if queue is full
                while (!m_worker->addTask(task)) {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            }
        });
    }
    
    // Wait for all submitter threads to finish
    for (auto& thread : submitterThreads) {
        thread.join();
    }
    
    // Wait for all tasks to complete
    while (completedTasks.load() < numThreads * tasksPerThread) {
        QThread::msleep(10);
    }
    
    QCOMPARE(completedTasks.load(), numThreads * tasksPerThread);
    QCOMPARE(m_worker->getTasksProcessed(), size_t(numThreads * tasksPerThread));
}

void TestThreadWorker::testInvalidTasks()
{
    m_worker->start();
    
    // Test adding null task
    QVERIFY(!m_worker->addTask(nullptr));
    
    // Test adding task with null function
    auto invalidTask = std::make_shared<Monitor::Threading::Task>();
    QVERIFY(!m_worker->addTask(invalidTask));
}

void TestThreadWorker::testQueueOverflow()
{
    // Fill the queue to capacity
    const size_t maxQueueSize = 1000; // Assuming this is the max from the worker
    
    for (size_t i = 0; i < maxQueueSize; ++i) {
        auto task = std::make_shared<Monitor::Threading::Task>(
            []() {
                QThread::msleep(100); // Long-running task to fill queue
            }, 0, i
        );
        QVERIFY(m_worker->addTask(task));
    }
    
    // Try to add one more task - should fail
    auto overflowTask = std::make_shared<Monitor::Threading::Task>(
        []() {}, 0, maxQueueSize
    );
    QVERIFY(!m_worker->addTask(overflowTask));
}

void TestThreadWorker::testExceptionHandling()
{
    std::atomic<bool> exceptionTaskCompleted{false};
    std::atomic<bool> normalTaskCompleted{false};
    
    // Task that throws exception
    auto exceptionTask = std::make_shared<Monitor::Threading::Task>(
        [&exceptionTaskCompleted]() {
            exceptionTaskCompleted = true;
            throw std::runtime_error("Test exception");
        }, 0, 1
    );
    
    // Normal task after exception
    auto normalTask = std::make_shared<Monitor::Threading::Task>(
        [&normalTaskCompleted]() {
            normalTaskCompleted = true;
        }, 0, 2
    );
    
    m_worker->start();
    QVERIFY(m_worker->addTask(exceptionTask));
    QVERIFY(m_worker->addTask(normalTask));
    
    // Wait for both tasks to be processed
    int attempts = 0;
    while ((!exceptionTaskCompleted.load() || !normalTaskCompleted.load()) && attempts < 100) {
        QThread::msleep(10);
        attempts++;
    }
    
    // Both tasks should have been processed despite exception
    QVERIFY(exceptionTaskCompleted.load());
    QVERIFY(normalTaskCompleted.load());
    QCOMPARE(m_worker->getTasksProcessed(), size_t(2));
}

void TestThreadWorker::testSignalEmission()
{
    QSignalSpy taskCompletedSpy(m_worker, &Monitor::Threading::ThreadWorker::taskCompleted);
    QSignalSpy workerIdleSpy(m_worker, &Monitor::Threading::ThreadWorker::workerIdle);
    QSignalSpy workerBusySpy(m_worker, &Monitor::Threading::ThreadWorker::workerBusy);
    
    std::atomic<bool> taskExecuted{false};
    auto task = std::make_shared<Monitor::Threading::Task>(
        [&taskExecuted]() {
            taskExecuted = true;
        }, 0, 1
    );
    
    m_worker->start();
    QVERIFY(m_worker->addTask(task));
    
    // Wait for task completion
    waitForSignal(taskCompletedSpy);
    
    QCOMPARE(taskCompletedSpy.count(), 1);
    QVERIFY(taskExecuted.load());
    
    // Check signal parameters
    QList<QVariant> arguments = taskCompletedSpy.takeFirst();
    QCOMPARE(arguments.at(0).toULongLong(), qulonglong(1)); // Task ID
    QVERIFY(arguments.at(1).toLongLong() >= 0); // Execution time should be >= 0
}

void TestThreadWorker::waitForSignal(QSignalSpy& spy, int timeout)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(timeout);
    
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timer.start();
    
    // Wait for signals or timeout
    while (spy.count() < expectedSignals && timer.remainingTime() > 0) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    
    timer.stop();
}

QTEST_MAIN(TestThreadWorker)
#include "test_thread_worker.moc"