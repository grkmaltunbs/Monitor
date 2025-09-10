#pragma once

#include "thread_pool.h"
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QAtomicInteger>
#include <memory>
#include <unordered_map>
#include <string>

namespace Monitor {
namespace Threading {

struct ThreadPoolStats {
    size_t numThreads = 0;
    size_t totalQueueSize = 0;
    size_t totalTasksProcessed = 0;
    size_t totalTasksStolen = 0;
    double averageTaskTime = 0.0;
    double utilizationPercent = 0.0;
    std::vector<size_t> workerQueueSizes;
    std::vector<bool> workerIdleStates;
    
    // Performance metrics
    qint64 messagesPerSecond = 0;
    qint64 averageLatencyUs = 0;
    qint64 peakLatencyUs = 0;
};

struct SystemResourceInfo {
    size_t numCpuCores = 0;
    size_t totalMemoryMB = 0;
    size_t availableMemoryMB = 0;
    double cpuUsagePercent = 0.0;
    double memoryUsagePercent = 0.0;
};

class ThreadManager : public QObject
{
    Q_OBJECT

public:
    explicit ThreadManager(QObject* parent = nullptr);
    ~ThreadManager() override;
    
    // Thread pool management
    bool createThreadPool(const QString& name, size_t numThreads = 0);
    bool removeThreadPool(const QString& name);
    ThreadPool* getThreadPool(const QString& name);
    QStringList getThreadPoolNames() const;
    
    // Default thread pool management
    ThreadPool* getDefaultThreadPool();
    bool initializeDefaultThreadPool(size_t numThreads = 0);
    
    // Task submission convenience methods
    template<typename F, typename... Args>
    auto submitTask(const QString& poolName, F&& f, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>>;
    
    template<typename F, typename... Args>
    auto submitTask(F&& f, Args&&... args) // Uses default pool
        -> std::future<std::invoke_result_t<F, Args...>>;
    
    bool submitTask(const QString& poolName, TaskFunction function, int priority = 0);
    bool submitTask(TaskFunction function, int priority = 0); // Uses default pool
    
    // Global thread management
    void startAll();
    void pauseAll();
    void resumeAll();
    void shutdownAll();
    
    // Statistics and monitoring
    ThreadPoolStats getThreadPoolStats(const QString& name) const;
    std::unordered_map<std::string, ThreadPoolStats> getAllThreadPoolStats() const;
    SystemResourceInfo getSystemResourceInfo() const;
    
    // Configuration
    void setGlobalSchedulingPolicy(ThreadPool::SchedulingPolicy policy);
    void setGlobalWorkStealingEnabled(bool enabled);
    void setGlobalLoadBalancingEnabled(bool enabled);
    
    // CPU affinity management
    void setOptimalCpuAffinityForPool(const QString& poolName);
    void setCpuAffinityPattern(const QString& poolName, const std::vector<int>& coreIds);
    
    // Performance monitoring
    void enablePerformanceMonitoring(bool enabled);
    void setMonitoringInterval(int intervalMs);
    qint64 getGlobalMessagesPerSecond() const;
    qint64 getGlobalAverageLatency() const;
    
    // Resource management
    void setMaxTotalThreads(size_t maxThreads) { m_maxTotalThreads = maxThreads; }
    size_t getMaxTotalThreads() const { return m_maxTotalThreads; }
    size_t getCurrentTotalThreads() const;
    
    // Thread naming and identification
    void setThreadNames(const QString& poolName, const QString& namePrefix);
    
    // Emergency controls
    void emergencyStop(); // Immediate stop of all operations
    bool isInEmergencyState() const { return m_emergencyStop.loadRelaxed(); }

signals:
    void threadPoolCreated(const QString& name);
    void threadPoolRemoved(const QString& name);
    void globalPerformanceUpdate(qint64 messagesPerSecond, qint64 averageLatencyUs);
    void resourcePressure(double cpuUsage, double memoryUsage);
    void emergencyStopTriggered();

private slots:
    void updatePerformanceMetrics();
    void checkResourceUsage();
    void onThreadPoolSaturated(size_t queueSize);
    void onThreadPoolIdle();

private:
    void initializeSystemInfo();
    size_t detectOptimalThreadCount() const;
    std::vector<int> generateOptimalCpuAffinityPattern(size_t numThreads) const;
    double calculateCpuUsage() const;
    void updateGlobalStats();
    
    std::unordered_map<std::string, std::unique_ptr<ThreadPool>> m_threadPools;
    mutable QMutex m_poolsMutex;
    
    QString m_defaultPoolName;
    size_t m_maxTotalThreads;
    
    // Performance monitoring
    bool m_performanceMonitoringEnabled;
    QTimer* m_performanceTimer;
    QTimer* m_resourceTimer;
    
    // Global statistics
    struct GlobalStats {
        qint64 totalMessagesProcessed = 0;
        qint64 totalLatencyUs = 0;
        qint64 peakLatencyUs = 0;
        std::chrono::steady_clock::time_point lastUpdateTime;
        qint64 messagesPerSecond = 0;
        qint64 averageLatencyUs = 0;
    };
    
    mutable QMutex m_statsMutex;
    GlobalStats m_globalStats;
    
    // System information
    SystemResourceInfo m_systemInfo;
    
    // Emergency controls
    QAtomicInteger<bool> m_emergencyStop;
    
    static constexpr int DEFAULT_MONITORING_INTERVAL_MS = 1000;
    static constexpr int DEFAULT_RESOURCE_CHECK_INTERVAL_MS = 5000;
    static constexpr size_t DEFAULT_MAX_TOTAL_THREADS = 128;
    static constexpr double RESOURCE_PRESSURE_CPU_THRESHOLD = 90.0;
    static constexpr double RESOURCE_PRESSURE_MEMORY_THRESHOLD = 90.0;
};

template<typename F, typename... Args>
auto ThreadManager::submitTask(const QString& poolName, F&& f, Args&&... args) 
    -> std::future<std::invoke_result_t<F, Args...>>
{
    ThreadPool* pool = getThreadPool(poolName);
    if (!pool) {
        using ReturnType = std::invoke_result_t<F, Args...>;
        auto promise = std::promise<ReturnType>();
        promise.set_exception(std::make_exception_ptr(
            std::runtime_error("Thread pool '" + poolName.toStdString() + "' not found")));
        return promise.get_future();
    }
    
    return pool->submitTask(std::forward<F>(f), std::forward<Args>(args)...);
}

template<typename F, typename... Args>
auto ThreadManager::submitTask(F&& f, Args&&... args) 
    -> std::future<std::invoke_result_t<F, Args...>>
{
    ThreadPool* defaultPool = getDefaultThreadPool();
    if (!defaultPool) {
        using ReturnType = std::invoke_result_t<F, Args...>;
        auto promise = std::promise<ReturnType>();
        promise.set_exception(std::make_exception_ptr(
            std::runtime_error("Default thread pool not initialized")));
        return promise.get_future();
    }
    
    return defaultPool->submitTask(std::forward<F>(f), std::forward<Args>(args)...);
}

} // namespace Threading  
} // namespace Monitor