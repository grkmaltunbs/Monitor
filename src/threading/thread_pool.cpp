#include "thread_pool.h"
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
#include <thread>
#include <algorithm>

namespace Monitor {
namespace Threading {

ThreadPool::ThreadPool(QObject* parent)
    : QObject(parent)
    , m_schedulingPolicy(SchedulingPolicy::WorkStealing)
    , m_isRunning(false)
    , m_isPaused(false)
    , m_nextTaskId(1)
    , m_nextWorkerIndex(0)
    , m_workStealingEnabled(true)
    , m_stealingAttempts(0)
    , m_successfulSteals(0)
    , m_loadBalancingEnabled(true)
    , m_loadBalanceTimer(nullptr)
    , m_randomGenerator(m_randomDevice())
    , m_idleWorkerCount(0)
    , m_totalTasksCompleted(0)
{
    m_loadBalanceTimer = new QTimer(this);
    m_loadBalanceTimer->setInterval(DEFAULT_LOAD_BALANCE_INTERVAL_MS);
    connect(m_loadBalanceTimer, &QTimer::timeout, this, &ThreadPool::performLoadBalancing);
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

bool ThreadPool::initialize(size_t numThreads)
{
    if (m_isRunning.loadRelaxed()) {
        qWarning() << "ThreadPool already initialized";
        return false;
    }
    
    if (numThreads == 0) {
        numThreads = std::max(MIN_THREADS, 
                            static_cast<size_t>(std::thread::hardware_concurrency()));
    }
    
    numThreads = std::clamp(numThreads, MIN_THREADS, MAX_THREADS);
    
    qInfo() << "Initializing ThreadPool with" << numThreads << "threads";
    
    try {
        m_workers.reserve(numThreads);
        
        for (size_t i = 0; i < numThreads; ++i) {
            auto worker = std::make_unique<ThreadWorker>(static_cast<int>(i), this, this);
            
            // Connect worker signals
            connect(worker.get(), &ThreadWorker::taskCompleted, 
                    this, &ThreadPool::onTaskCompleted);
            connect(worker.get(), &ThreadWorker::workerIdle, 
                    this, &ThreadPool::onWorkerIdle);
            connect(worker.get(), &ThreadWorker::workerBusy, 
                    this, &ThreadPool::onWorkerBusy);
            connect(worker.get(), &ThreadWorker::taskStolen,
                    this, &ThreadPool::onTaskStolen);
            
            m_workers.push_back(std::move(worker));
        }
        
        // Initialize random distribution
        m_randomDistribution = std::uniform_int_distribution<size_t>(0, numThreads - 1);
        
        qInfo() << "ThreadPool initialized successfully with" << numThreads << "workers";
        return true;
        
    } catch (const std::exception& e) {
        qCritical() << "Failed to initialize ThreadPool:" << e.what();
        m_workers.clear();
        return false;
    }
}

void ThreadPool::shutdown()
{
    if (!m_isRunning.loadRelaxed()) {
        return;
    }
    
    qInfo() << "Shutting down ThreadPool";
    
    m_isRunning.storeRelaxed(false);
    m_isPaused.storeRelaxed(false);
    
    if (m_loadBalanceTimer) {
        m_loadBalanceTimer->stop();
    }
    
    // Stop all workers
    for (auto& worker : m_workers) {
        if (worker) {
            worker->stop();
        }
    }
    
    // Wait for all workers to finish
    for (auto& worker : m_workers) {
        if (worker && worker->isRunning()) {
            worker->wait(5000); // 5 second timeout
        }
    }
    
    m_workers.clear();
    
    qInfo() << "ThreadPool shutdown complete";
}

void ThreadPool::start()
{
    if (m_workers.empty()) {
        qWarning() << "ThreadPool not initialized, cannot start";
        return;
    }
    
    if (m_isRunning.loadRelaxed()) {
        qWarning() << "ThreadPool already running";
        return;
    }
    
    qInfo() << "Starting ThreadPool";
    
    m_isRunning.storeRelaxed(true);
    m_isPaused.storeRelaxed(false);
    
    // Start all workers
    for (auto& worker : m_workers) {
        worker->start();
    }
    
    if (m_loadBalancingEnabled && m_loadBalanceTimer) {
        m_loadBalanceTimer->start();
    }
    
    qInfo() << "ThreadPool started with" << m_workers.size() << "workers";
}

void ThreadPool::pause()
{
    if (!m_isRunning.loadRelaxed()) {
        return;
    }
    
    m_isPaused.storeRelaxed(true);
    if (m_loadBalanceTimer) {
        m_loadBalanceTimer->stop();
    }
}

void ThreadPool::resume()
{
    if (!m_isRunning.loadRelaxed() || !m_isPaused.loadRelaxed()) {
        return;
    }
    
    m_isPaused.storeRelaxed(false);
    if (m_loadBalancingEnabled && m_loadBalanceTimer) {
        m_loadBalanceTimer->start();
    }
    
    // Wake up all workers
    for (auto& worker : m_workers) {
        worker->wakeUp();
    }
}

bool ThreadPool::submitTask(TaskFunction function, int priority)
{
    if (!function || !m_isRunning.loadRelaxed() || m_isPaused.loadRelaxed()) {
        return false;
    }
    
    auto task = std::make_shared<Task>(std::move(function), priority, 
                                     m_nextTaskId.fetchAndAddRelaxed(1));
    return submitTask(task);
}

bool ThreadPool::submitTask(TaskPtr task)
{
    if (!task || !m_isRunning.loadRelaxed() || m_isPaused.loadRelaxed()) {
        return false;
    }
    
    ThreadWorker* selectedWorker = selectWorker(task->priority);
    if (!selectedWorker) {
        qWarning() << "No available worker to submit task";
        return false;
    }
    
    bool success = selectedWorker->addTask(task);
    
    if (!success) {
        // If primary worker is full, try work stealing or alternative workers
        if (m_workStealingEnabled) {
            for (auto& worker : m_workers) {
                if (worker.get() != selectedWorker && worker->addTask(task)) {
                    success = true;
                    break;
                }
            }
        }
    }
    
    if (!success) {
        qWarning() << "Failed to submit task - all workers busy";
    }
    
    return success;
}

bool ThreadPool::submitTasks(const std::vector<TaskPtr>& tasks)
{
    if (tasks.empty() || !m_isRunning.loadRelaxed() || m_isPaused.loadRelaxed()) {
        return false;
    }
    
    bool allSubmitted = true;
    for (const auto& task : tasks) {
        if (!submitTask(task)) {
            allSubmitted = false;
        }
    }
    
    return allSubmitted;
}

size_t ThreadPool::getTotalQueueSize() const
{
    size_t totalSize = 0;
    for (const auto& worker : m_workers) {
        totalSize += worker->getQueueSize();
    }
    return totalSize;
}

size_t ThreadPool::getTotalTasksProcessed() const
{
    return m_totalTasksCompleted.load(std::memory_order_relaxed);
}

size_t ThreadPool::getTotalTasksStolen() const
{
    size_t totalStolen = 0;
    for (const auto& worker : m_workers) {
        totalStolen += worker->getTasksStolen();
    }
    return totalStolen;
}

double ThreadPool::getAverageTaskTime() const
{
    if (m_workers.empty()) {
        return 0.0;
    }
    
    double totalTime = 0.0;
    size_t totalTasks = 0;
    
    for (const auto& worker : m_workers) {
        size_t workerTasks = worker->getTasksProcessed();
        if (workerTasks > 0) {
            totalTime += worker->getAverageTaskTime() * workerTasks;
            totalTasks += workerTasks;
        }
    }
    
    return totalTasks > 0 ? totalTime / totalTasks : 0.0;
}

std::vector<size_t> ThreadPool::getWorkerQueueSizes() const
{
    std::vector<size_t> sizes;
    sizes.reserve(m_workers.size());
    
    for (const auto& worker : m_workers) {
        sizes.push_back(worker->getQueueSize());
    }
    
    return sizes;
}

std::vector<bool> ThreadPool::getWorkerIdleStates() const
{
    std::vector<bool> states;
    states.reserve(m_workers.size());
    
    for (const auto& worker : m_workers) {
        states.push_back(worker->isIdle());
    }
    
    return states;
}

void ThreadPool::setCpuAffinityPattern(const std::vector<int>& coreIds)
{
    for (size_t i = 0; i < m_workers.size() && i < coreIds.size(); ++i) {
        m_workers[i]->setCpuAffinity(coreIds[i]);
    }
}

void ThreadPool::setWorkerCpuAffinity(size_t workerIndex, int coreId)
{
    if (workerIndex < m_workers.size()) {
        m_workers[workerIndex]->setCpuAffinity(coreId);
    }
}

void ThreadPool::setLoadBalanceInterval(int intervalMs)
{
    if (m_loadBalanceTimer) {
        m_loadBalanceTimer->setInterval(intervalMs);
    }
}

ThreadWorker* ThreadPool::selectWorker(int priority)
{
    if (m_workers.empty()) {
        return nullptr;
    }
    
    (void)priority; // May be used for priority-based scheduling in the future
    
    switch (m_schedulingPolicy) {
        case SchedulingPolicy::RoundRobin: {
            size_t index = m_nextWorkerIndex.fetchAndAddRelaxed(1) % m_workers.size();
            return m_workers[index].get();
        }
        
        case SchedulingPolicy::LeastLoaded:
            return findLeastLoadedWorker();
        
        case SchedulingPolicy::Random: {
            size_t index = m_randomDistribution(m_randomGenerator);
            return m_workers[index].get();
        }
        
        case SchedulingPolicy::WorkStealing:
        default:
            // For work stealing, start with least loaded then rely on stealing
            return findLeastLoadedWorker();
    }
}

ThreadWorker* ThreadPool::findLeastLoadedWorker()
{
    if (m_workers.empty()) {
        return nullptr;
    }
    
    ThreadWorker* leastLoaded = m_workers[0].get();
    size_t minQueueSize = leastLoaded->getQueueSize();
    
    for (size_t i = 1; i < m_workers.size(); ++i) {
        size_t queueSize = m_workers[i]->getQueueSize();
        if (queueSize < minQueueSize) {
            minQueueSize = queueSize;
            leastLoaded = m_workers[i].get();
        }
    }
    
    return leastLoaded;
}

void ThreadPool::attemptWorkStealing(ThreadWorker* idleWorker)
{
    if (!m_workStealingEnabled || !idleWorker) {
        return;
    }
    
    m_stealingAttempts.fetch_add(1);
    
    // Find a worker with tasks to steal from
    for (auto& worker : m_workers) {
        if (worker.get() != idleWorker && worker->getQueueSize() > 1) {
            TaskPtr stolenTask = worker->stealTask();
            if (stolenTask && idleWorker->addTask(stolenTask)) {
                m_successfulSteals.fetch_add(1);
                break;
            }
        }
    }
}

void ThreadPool::checkPoolState()
{
    size_t totalQueueSize = getTotalQueueSize();
    size_t idleCount = m_idleWorkerCount.load(std::memory_order_relaxed);
    
    if (totalQueueSize > SATURATION_THRESHOLD) {
        emit poolSaturated(totalQueueSize);
    } else if (idleCount == m_workers.size() && totalQueueSize == 0) {
        emit poolIdle();
    }
}

void ThreadPool::onTaskCompleted(size_t taskId, qint64 executionTimeUs)
{
    m_totalTasksCompleted.fetch_add(1);
    emit taskCompleted(taskId, executionTimeUs);
    checkPoolState();
}

void ThreadPool::onWorkerIdle()
{
    ThreadWorker* worker = qobject_cast<ThreadWorker*>(sender());
    m_idleWorkerCount.fetch_add(1);
    
    if (m_workStealingEnabled && worker) {
        attemptWorkStealing(worker);
    }
    
    checkPoolState();
}

void ThreadPool::onWorkerBusy()
{
    m_idleWorkerCount.fetch_sub(1);
    checkPoolState();
}

void ThreadPool::onTaskStolen(int fromWorkerId, int toWorkerId)
{
    emit workStealingOccurred(fromWorkerId, toWorkerId);
}

void ThreadPool::performLoadBalancing()
{
    if (!m_loadBalancingEnabled || !m_isRunning.loadRelaxed() || m_isPaused.loadRelaxed()) {
        return;
    }
    
    // Simple load balancing: identify overloaded workers and help them
    auto queueSizes = getWorkerQueueSizes();
    if (queueSizes.empty()) {
        return;
    }
    
    // Find average queue size
    size_t totalSize = 0;
    for (size_t size : queueSizes) {
        totalSize += size;
    }
    double averageSize = static_cast<double>(totalSize) / queueSizes.size();
    
    // If imbalance is significant, trigger work stealing for overloaded workers
    for (size_t i = 0; i < queueSizes.size(); ++i) {
        if (queueSizes[i] > averageSize * 1.5 && queueSizes[i] > 10) {
            // Find an idle worker to help
            for (size_t j = 0; j < m_workers.size(); ++j) {
                if (i != j && m_workers[j]->isIdle()) {
                    attemptWorkStealing(m_workers[j].get());
                    break;
                }
            }
        }
    }
}

} // namespace Threading  
} // namespace Monitor

