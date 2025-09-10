#include "thread_worker.h"
#include "thread_pool.h"
#include <QtCore/QDebug>
#include <thread>

#ifdef Q_OS_LINUX
#include <pthread.h>
#include <sched.h>
#elif defined(Q_OS_WIN)
#include <Windows.h>
#elif defined(Q_OS_MAC)
#include <pthread.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#endif

namespace Monitor {
namespace Threading {

ThreadWorker::ThreadWorker(int workerId, ThreadPool* pool, QObject* parent)
    : QThread(parent)
    , m_workerId(workerId)
    , m_threadPool(pool)
    , m_isRunning(false)
    , m_shouldStop(false)
    , m_isIdle(true)
    , m_tasksProcessed(0)
    , m_tasksStolen(0)
    , m_cpuAffinity(-1)
    , m_totalTaskTime(0)
{
}

ThreadWorker::~ThreadWorker()
{
    stop();
    wait();
}

void ThreadWorker::start(Priority priority)
{
    m_shouldStop.storeRelaxed(false);
    m_isRunning.storeRelaxed(true);
    QThread::start(priority);
}

void ThreadWorker::stop()
{
    m_shouldStop.storeRelaxed(true);
    wakeUp();
}

void ThreadWorker::wakeUp()
{
    QMutexLocker locker(&m_queueMutex);
    m_wakeCondition.wakeOne();
}

bool ThreadWorker::addTask(TaskPtr task)
{
    if (!task || m_shouldStop.loadRelaxed()) {
        return false;
    }
    
    QMutexLocker locker(&m_queueMutex);
    
    if (m_taskQueue.size() >= MAX_QUEUE_SIZE) {
        return false; // Queue full
    }
    
    m_taskQueue.push(task);
    
    if (m_isIdle.loadRelaxed()) {
        m_wakeCondition.wakeOne();
    }
    
    return true;
}

TaskPtr ThreadWorker::stealTask()
{
    QMutexLocker locker(&m_queueMutex);
    
    if (m_taskQueue.empty()) {
        return nullptr;
    }
    
    // Create a temporary vector to extract tasks
    std::vector<TaskPtr> tasks;
    while (!m_taskQueue.empty()) {
        tasks.push_back(m_taskQueue.top());
        m_taskQueue.pop();
    }
    
    if (tasks.empty()) {
        return nullptr;
    }
    
    // Take the lowest priority task (for work stealing)
    TaskPtr stolenTask = tasks.back();
    tasks.pop_back();
    
    // Put the rest back
    for (auto& task : tasks) {
        m_taskQueue.push(task);
    }
    
    m_tasksStolen.fetchAndAddRelaxed(1);
    return stolenTask;
}

size_t ThreadWorker::getQueueSize() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_taskQueue.size();
}

double ThreadWorker::getAverageTaskTime() const
{
    QMutexLocker locker(&m_statsMutex);
    size_t processed = m_tasksProcessed.loadRelaxed();
    if (processed == 0) {
        return 0.0;
    }
    return static_cast<double>(m_totalTaskTime.count()) / processed;
}

void ThreadWorker::setCpuAffinity(int coreId)
{
    m_cpuAffinity = coreId;
    
    if (isRunning() && coreId >= 0) {
#ifdef Q_OS_LINUX
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(coreId, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#elif defined(Q_OS_WIN)
        SetThreadAffinityMask(GetCurrentThread(), 1ULL << coreId);
#elif defined(Q_OS_MAC)
        thread_affinity_policy_data_t policy = { coreId };
        thread_policy_set(pthread_mach_thread_np(pthread_self()),
                          THREAD_AFFINITY_POLICY,
                          (thread_policy_t)&policy,
                          THREAD_AFFINITY_POLICY_COUNT);
#endif
    }
}

void ThreadWorker::run()
{
    // Set CPU affinity if specified
    if (m_cpuAffinity >= 0) {
        setCpuAffinity(m_cpuAffinity);
    }
    
    m_isRunning.storeRelaxed(true);
    
    while (!m_shouldStop.loadRelaxed()) {
        processTaskQueue();
        
        if (!m_shouldStop.loadRelaxed()) {
            // Wait for new tasks or timeout
            QMutexLocker locker(&m_queueMutex);
            if (m_taskQueue.empty()) {
                updateIdleState(true);
                m_wakeCondition.wait(&m_queueMutex, IDLE_TIMEOUT_MS);
                updateIdleState(false);
            }
        }
    }
    
    m_isRunning.storeRelaxed(false);
}

void ThreadWorker::processTaskQueue()
{
    while (!m_shouldStop.loadRelaxed()) {
        TaskPtr currentTask;
        
        // Get next task
        {
            QMutexLocker locker(&m_queueMutex);
            if (m_taskQueue.empty()) {
                break;
            }
            currentTask = m_taskQueue.top();
            m_taskQueue.pop();
        }
        
        if (!currentTask || !currentTask->function) {
            continue;
        }
        
        // Execute task with timing
        auto startTime = std::chrono::high_resolution_clock::now();
        
        try {
            currentTask->function();
        } catch (const std::exception& e) {
            qWarning() << "Task execution error in worker" << m_workerId << ":" << e.what();
        } catch (...) {
            qWarning() << "Unknown task execution error in worker" << m_workerId;
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto executionTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
        auto executionTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(executionTime).count();
        
        // Update statistics
        {
            QMutexLocker locker(&m_statsMutex);
            m_totalTaskTime += executionTime;
        }
        
        m_tasksProcessed.fetchAndAddRelaxed(1);
        
        // Emit completion signal
        emit taskCompleted(currentTask->id, executionTimeUs);
    }
}

void ThreadWorker::updateIdleState(bool idle)
{
    bool wasIdle = m_isIdle.loadRelaxed();
    m_isIdle.storeRelaxed(idle);
    
    if (idle && !wasIdle) {
        emit workerIdle();
    } else if (!idle && wasIdle) {
        emit workerBusy();
    }
}

} // namespace Threading  
} // namespace Monitor

