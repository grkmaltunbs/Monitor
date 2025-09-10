#include "thread_manager.h"
#include <QtCore/QDebug>
#include <QtCore/QSysInfo>
#include <thread>
#include <algorithm>

#ifdef Q_OS_LINUX
#include <sys/sysinfo.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <Windows.h>
#include <Psapi.h>
#elif defined(Q_OS_MAC)
#include <sys/sysctl.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#endif

namespace Monitor {
namespace Threading {

ThreadManager::ThreadManager(QObject* parent)
    : QObject(parent)
    , m_defaultPoolName("default")
    , m_maxTotalThreads(DEFAULT_MAX_TOTAL_THREADS)
    , m_performanceMonitoringEnabled(false)
    , m_performanceTimer(nullptr)
    , m_resourceTimer(nullptr)
    , m_emergencyStop(false)
{
    initializeSystemInfo();
    
    // Setup performance monitoring timer
    m_performanceTimer = new QTimer(this);
    m_performanceTimer->setInterval(DEFAULT_MONITORING_INTERVAL_MS);
    connect(m_performanceTimer, &QTimer::timeout, this, &ThreadManager::updatePerformanceMetrics);
    
    // Setup resource monitoring timer
    m_resourceTimer = new QTimer(this);
    m_resourceTimer->setInterval(DEFAULT_RESOURCE_CHECK_INTERVAL_MS);
    connect(m_resourceTimer, &QTimer::timeout, this, &ThreadManager::checkResourceUsage);
    m_resourceTimer->start();
    
    // Initialize global stats
    m_globalStats.lastUpdateTime = std::chrono::steady_clock::now();
}

ThreadManager::~ThreadManager()
{
    shutdownAll();
}

bool ThreadManager::createThreadPool(const QString& name, size_t numThreads)
{
    if (name.isEmpty()) {
        qWarning() << "ThreadManager: Empty pool name not allowed";
        return false;
    }
    
    QMutexLocker locker(&m_poolsMutex);
    
    std::string nameStr = name.toStdString();
    if (m_threadPools.find(nameStr) != m_threadPools.end()) {
        qWarning() << "ThreadManager: Thread pool" << name << "already exists";
        return false;
    }
    
    // Check if we would exceed the total thread limit
    size_t currentThreads = getCurrentTotalThreads();
    if (numThreads == 0) {
        numThreads = detectOptimalThreadCount();
    }
    
    if (currentThreads + numThreads > m_maxTotalThreads) {
        qWarning() << "ThreadManager: Creating pool" << name << "would exceed max total threads"
                  << "(" << currentThreads + numThreads << ">" << m_maxTotalThreads << ")";
        return false;
    }
    
    try {
        auto threadPool = std::make_unique<ThreadPool>(this);
        
        // Connect pool signals
        connect(threadPool.get(), &ThreadPool::poolSaturated, 
                this, &ThreadManager::onThreadPoolSaturated);
        connect(threadPool.get(), &ThreadPool::poolIdle, 
                this, &ThreadManager::onThreadPoolIdle);
        
        if (!threadPool->initialize(numThreads)) {
            qWarning() << "ThreadManager: Failed to initialize thread pool" << name;
            return false;
        }
        
        m_threadPools[nameStr] = std::move(threadPool);
        
        emit threadPoolCreated(name);
        qInfo() << "ThreadManager: Created thread pool" << name << "with" << numThreads << "threads";
        return true;
        
    } catch (const std::exception& e) {
        qCritical() << "ThreadManager: Exception creating thread pool" << name << ":" << e.what();
        return false;
    }
}

bool ThreadManager::removeThreadPool(const QString& name)
{
    if (name == m_defaultPoolName) {
        qWarning() << "ThreadManager: Cannot remove default thread pool";
        return false;
    }
    
    QMutexLocker locker(&m_poolsMutex);
    
    std::string nameStr = name.toStdString();
    auto it = m_threadPools.find(nameStr);
    if (it == m_threadPools.end()) {
        qWarning() << "ThreadManager: Thread pool" << name << "not found";
        return false;
    }
    
    // Shutdown the pool
    it->second->shutdown();
    m_threadPools.erase(it);
    
    emit threadPoolRemoved(name);
    qInfo() << "ThreadManager: Removed thread pool" << name;
    return true;
}

ThreadPool* ThreadManager::getThreadPool(const QString& name)
{
    QMutexLocker locker(&m_poolsMutex);
    
    std::string nameStr = name.toStdString();
    auto it = m_threadPools.find(nameStr);
    return (it != m_threadPools.end()) ? it->second.get() : nullptr;
}

QStringList ThreadManager::getThreadPoolNames() const
{
    QMutexLocker locker(&m_poolsMutex);
    
    QStringList names;
    for (const auto& pair : m_threadPools) {
        names.append(QString::fromStdString(pair.first));
    }
    return names;
}

ThreadPool* ThreadManager::getDefaultThreadPool()
{
    return getThreadPool(m_defaultPoolName);
}

bool ThreadManager::initializeDefaultThreadPool(size_t numThreads)
{
    return createThreadPool(m_defaultPoolName, numThreads);
}

bool ThreadManager::submitTask(const QString& poolName, TaskFunction function, int priority)
{
    ThreadPool* pool = getThreadPool(poolName);
    return pool ? pool->submitTask(function, priority) : false;
}

bool ThreadManager::submitTask(TaskFunction function, int priority)
{
    ThreadPool* defaultPool = getDefaultThreadPool();
    return defaultPool ? defaultPool->submitTask(function, priority) : false;
}

void ThreadManager::startAll()
{
    if (m_emergencyStop.loadRelaxed()) {
        qWarning() << "ThreadManager: Cannot start - in emergency stop state";
        return;
    }
    
    QMutexLocker locker(&m_poolsMutex);
    
    for (auto& pair : m_threadPools) {
        pair.second->start();
    }
    
    if (m_performanceMonitoringEnabled) {
        m_performanceTimer->start();
    }
    
    qInfo() << "ThreadManager: Started all thread pools";
}

void ThreadManager::pauseAll()
{
    QMutexLocker locker(&m_poolsMutex);
    
    for (auto& pair : m_threadPools) {
        pair.second->pause();
    }
    
    m_performanceTimer->stop();
    qInfo() << "ThreadManager: Paused all thread pools";
}

void ThreadManager::resumeAll()
{
    if (m_emergencyStop.loadRelaxed()) {
        qWarning() << "ThreadManager: Cannot resume - in emergency stop state";
        return;
    }
    
    QMutexLocker locker(&m_poolsMutex);
    
    for (auto& pair : m_threadPools) {
        pair.second->resume();
    }
    
    if (m_performanceMonitoringEnabled) {
        m_performanceTimer->start();
    }
    
    qInfo() << "ThreadManager: Resumed all thread pools";
}

void ThreadManager::shutdownAll()
{
    qInfo() << "ThreadManager: Shutting down all thread pools";
    
    m_performanceTimer->stop();
    m_resourceTimer->stop();
    
    QMutexLocker locker(&m_poolsMutex);
    
    for (auto& pair : m_threadPools) {
        pair.second->shutdown();
    }
    
    m_threadPools.clear();
    qInfo() << "ThreadManager: All thread pools shut down";
}

ThreadPoolStats ThreadManager::getThreadPoolStats(const QString& name) const
{
    ThreadPoolStats stats;
    ThreadPool* pool = const_cast<ThreadManager*>(this)->getThreadPool(name);
    
    if (!pool) {
        return stats; // Return empty stats
    }
    
    stats.numThreads = pool->getNumThreads();
    stats.totalQueueSize = pool->getTotalQueueSize();
    stats.totalTasksProcessed = pool->getTotalTasksProcessed();
    stats.totalTasksStolen = pool->getTotalTasksStolen();
    stats.averageTaskTime = pool->getAverageTaskTime();
    stats.workerQueueSizes = pool->getWorkerQueueSizes();
    stats.workerIdleStates = pool->getWorkerIdleStates();
    
    // Calculate utilization
    size_t idleWorkers = std::count(stats.workerIdleStates.begin(), 
                                   stats.workerIdleStates.end(), true);
    stats.utilizationPercent = stats.numThreads > 0 ? 
        (1.0 - static_cast<double>(idleWorkers) / stats.numThreads) * 100.0 : 0.0;
    
    return stats;
}

std::unordered_map<std::string, ThreadPoolStats> ThreadManager::getAllThreadPoolStats() const
{
    std::unordered_map<std::string, ThreadPoolStats> allStats;
    
    QStringList poolNames = getThreadPoolNames();
    for (const QString& name : poolNames) {
        allStats[name.toStdString()] = getThreadPoolStats(name);
    }
    
    return allStats;
}

SystemResourceInfo ThreadManager::getSystemResourceInfo() const
{
    SystemResourceInfo info = m_systemInfo;
    info.cpuUsagePercent = calculateCpuUsage();
    
    // Update memory usage
#ifdef Q_OS_LINUX
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.totalMemoryMB = si.totalram / (1024 * 1024);
        info.availableMemoryMB = si.freeram / (1024 * 1024);
        info.memoryUsagePercent = (1.0 - static_cast<double>(si.freeram) / si.totalram) * 100.0;
    }
#elif defined(Q_OS_WIN)
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        info.totalMemoryMB = memStatus.ullTotalPhys / (1024 * 1024);
        info.availableMemoryMB = memStatus.ullAvailPhys / (1024 * 1024);
        info.memoryUsagePercent = static_cast<double>(memStatus.dwMemoryLoad);
    }
#elif defined(Q_OS_MAC)
    vm_size_t page_size;
    vm_statistics64_data_t vm_stats;
    mach_msg_type_number_t host_count = HOST_VM_INFO64_COUNT;
    
    host_page_size(mach_host_self(), &page_size);
    host_statistics64(mach_host_self(), HOST_VM_INFO64, 
                     (host_info64_t)&vm_stats, &host_count);
    
    uint64_t totalMem = (vm_stats.free_count + vm_stats.active_count + 
                        vm_stats.inactive_count + vm_stats.wire_count) * page_size;
    uint64_t freeMem = vm_stats.free_count * page_size;
    
    info.totalMemoryMB = totalMem / (1024 * 1024);
    info.availableMemoryMB = freeMem / (1024 * 1024);
    info.memoryUsagePercent = (1.0 - static_cast<double>(freeMem) / totalMem) * 100.0;
#endif
    
    return info;
}

void ThreadManager::setGlobalSchedulingPolicy(ThreadPool::SchedulingPolicy policy)
{
    QMutexLocker locker(&m_poolsMutex);
    
    for (auto& pair : m_threadPools) {
        pair.second->setSchedulingPolicy(policy);
    }
}

void ThreadManager::setGlobalWorkStealingEnabled(bool enabled)
{
    QMutexLocker locker(&m_poolsMutex);
    
    for (auto& pair : m_threadPools) {
        pair.second->setWorkStealingEnabled(enabled);
    }
}

void ThreadManager::setGlobalLoadBalancingEnabled(bool enabled)
{
    QMutexLocker locker(&m_poolsMutex);
    
    for (auto& pair : m_threadPools) {
        pair.second->enableLoadBalancing(enabled);
    }
}

void ThreadManager::enablePerformanceMonitoring(bool enabled)
{
    m_performanceMonitoringEnabled = enabled;
    
    if (enabled) {
        m_performanceTimer->start();
    } else {
        m_performanceTimer->stop();
    }
}

void ThreadManager::setMonitoringInterval(int intervalMs)
{
    m_performanceTimer->setInterval(intervalMs);
}

qint64 ThreadManager::getGlobalMessagesPerSecond() const
{
    QMutexLocker locker(&m_statsMutex);
    return m_globalStats.messagesPerSecond;
}

qint64 ThreadManager::getGlobalAverageLatency() const
{
    QMutexLocker locker(&m_statsMutex);
    return m_globalStats.averageLatencyUs;
}

size_t ThreadManager::getCurrentTotalThreads() const
{
    QMutexLocker locker(&m_poolsMutex);
    
    size_t totalThreads = 0;
    for (const auto& pair : m_threadPools) {
        totalThreads += pair.second->getNumThreads();
    }
    return totalThreads;
}

void ThreadManager::emergencyStop()
{
    qCritical() << "ThreadManager: EMERGENCY STOP triggered";
    
    m_emergencyStop.storeRelaxed(true);
    emit emergencyStopTriggered();
    
    // Immediately shutdown all pools
    shutdownAll();
}

void ThreadManager::initializeSystemInfo()
{
    m_systemInfo.numCpuCores = std::thread::hardware_concurrency();
    
#ifdef Q_OS_LINUX
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        m_systemInfo.totalMemoryMB = si.totalram / (1024 * 1024);
    }
#elif defined(Q_OS_WIN)
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        m_systemInfo.totalMemoryMB = memStatus.ullTotalPhys / (1024 * 1024);
    }
#elif defined(Q_OS_MAC)
    uint64_t memsize;
    size_t size = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &size, nullptr, 0) == 0) {
        m_systemInfo.totalMemoryMB = memsize / (1024 * 1024);
    }
#endif
    
    qInfo() << "ThreadManager: System info - CPU cores:" << m_systemInfo.numCpuCores 
           << "Total memory:" << m_systemInfo.totalMemoryMB << "MB";
}

size_t ThreadManager::detectOptimalThreadCount() const
{
    // Use 75% of available cores, minimum 2, maximum 16 for default pools
    size_t cores = m_systemInfo.numCpuCores;
    size_t optimal = std::max(2UL, std::min(16UL, cores * 3 / 4));
    return optimal;
}

double ThreadManager::calculateCpuUsage() const
{
    // This is a simplified implementation
    // In a real implementation, you would track CPU usage over time
    return 0.0; // Placeholder
}

void ThreadManager::updatePerformanceMetrics()
{
    updateGlobalStats();
    
    QMutexLocker locker(&m_statsMutex);
    emit globalPerformanceUpdate(m_globalStats.messagesPerSecond, 
                                m_globalStats.averageLatencyUs);
}

void ThreadManager::checkResourceUsage()
{
    SystemResourceInfo info = getSystemResourceInfo();
    
    if (info.cpuUsagePercent > RESOURCE_PRESSURE_CPU_THRESHOLD ||
        info.memoryUsagePercent > RESOURCE_PRESSURE_MEMORY_THRESHOLD) {
        emit resourcePressure(info.cpuUsagePercent, info.memoryUsagePercent);
    }
}

void ThreadManager::updateGlobalStats()
{
    QMutexLocker locker(&m_statsMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_globalStats.lastUpdateTime).count();
    
    if (elapsed > 0) {
        // Calculate messages per second across all pools
        qint64 totalProcessed = 0;
        qint64 totalLatency = 0;
        size_t poolCount = 0;
        
        QMutexLocker poolsLocker(&m_poolsMutex);
        for (const auto& pair : m_threadPools) {
            totalProcessed += pair.second->getTotalTasksProcessed();
            totalLatency += static_cast<qint64>(pair.second->getAverageTaskTime());
            poolCount++;
        }
        
        qint64 newMessages = totalProcessed - m_globalStats.totalMessagesProcessed;
        m_globalStats.messagesPerSecond = (newMessages * 1000) / elapsed;
        m_globalStats.totalMessagesProcessed = totalProcessed;
        m_globalStats.averageLatencyUs = poolCount > 0 ? totalLatency / poolCount : 0;
        m_globalStats.lastUpdateTime = now;
    }
}

void ThreadManager::onThreadPoolSaturated(size_t queueSize)
{
    qWarning() << "ThreadManager: Thread pool saturated with queue size" << queueSize;
}

void ThreadManager::onThreadPoolIdle()
{
    // Pool is idle - could be used for optimization decisions
}

} // namespace Threading  
} // namespace Monitor

