#pragma once

#include "thread_worker.h"
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QAtomicInteger>
#include <QtCore/QTimer>
#include <vector>
#include <memory>
#include <atomic>
#include <random>

namespace Monitor {
namespace Threading {

class ThreadPool : public QObject
{
    Q_OBJECT

public:
    enum class SchedulingPolicy {
        RoundRobin,
        LeastLoaded,
        Random,
        WorkStealing
    };

    explicit ThreadPool(QObject* parent = nullptr);
    ~ThreadPool() override;
    
    // Configuration
    bool initialize(size_t numThreads = 0); // 0 = auto-detect CPU cores
    void shutdown();
    
    void setSchedulingPolicy(SchedulingPolicy policy) { m_schedulingPolicy = policy; }
    SchedulingPolicy getSchedulingPolicy() const { return m_schedulingPolicy; }
    
    void setWorkStealingEnabled(bool enabled) { m_workStealingEnabled = enabled; }
    bool isWorkStealingEnabled() const { return m_workStealingEnabled; }
    
    // Task submission
    bool submitTask(TaskFunction function, int priority = 0);
    bool submitTask(TaskPtr task);
    
    template<typename F, typename... Args>
    auto submitTask(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;
    
    // Batch operations
    bool submitTasks(const std::vector<TaskPtr>& tasks);
    
    // Pool management
    void start();
    void pause();
    void resume();
    bool isRunning() const { return m_isRunning.loadRelaxed(); }
    bool isPaused() const { return m_isPaused.loadRelaxed(); }
    
    // Statistics
    size_t getNumThreads() const { return m_workers.size(); }
    size_t getTotalQueueSize() const;
    size_t getTotalTasksProcessed() const;
    size_t getTotalTasksStolen() const;
    double getAverageTaskTime() const;
    std::vector<size_t> getWorkerQueueSizes() const;
    std::vector<bool> getWorkerIdleStates() const;
    
    // CPU affinity management
    void setCpuAffinityPattern(const std::vector<int>& coreIds);
    void setWorkerCpuAffinity(size_t workerIndex, int coreId);
    
    // Load balancing
    void enableLoadBalancing(bool enabled) { m_loadBalancingEnabled = enabled; }
    void setLoadBalanceInterval(int intervalMs);

signals:
    void taskCompleted(size_t taskId, qint64 executionTimeUs);
    void workStealingOccurred(int fromWorkerId, int toWorkerId);
    void poolSaturated(size_t totalQueueSize);
    void poolIdle();

private slots:
    void onTaskCompleted(size_t taskId, qint64 executionTimeUs);
    void onWorkerIdle();
    void onWorkerBusy();
    void onTaskStolen(int fromWorkerId, int toWorkerId);
    void performLoadBalancing();

private:
    ThreadWorker* selectWorker(int priority = 0);
    ThreadWorker* findLeastLoadedWorker();
    void initializeWorkStealing();
    void attemptWorkStealing(ThreadWorker* idleWorker);
    void checkPoolState();
    
    std::vector<std::unique_ptr<ThreadWorker>> m_workers;
    SchedulingPolicy m_schedulingPolicy;
    
    mutable QMutex m_statsMutex;
    QAtomicInteger<bool> m_isRunning;
    QAtomicInteger<bool> m_isPaused;
    QAtomicInteger<size_t> m_nextTaskId;
    QAtomicInteger<size_t> m_nextWorkerIndex;
    
    // Work stealing
    bool m_workStealingEnabled;
    std::atomic<size_t> m_stealingAttempts;
    std::atomic<size_t> m_successfulSteals;
    
    // Load balancing
    bool m_loadBalancingEnabled;
    QTimer* m_loadBalanceTimer;
    
    // Random number generator for random scheduling
    std::random_device m_randomDevice;
    std::mt19937 m_randomGenerator;
    std::uniform_int_distribution<size_t> m_randomDistribution;
    
    // Pool state tracking
    std::atomic<size_t> m_idleWorkerCount;
    std::atomic<size_t> m_totalTasksCompleted;
    
    static constexpr size_t MIN_THREADS = 1;
    static constexpr size_t MAX_THREADS = 64;
    static constexpr int DEFAULT_LOAD_BALANCE_INTERVAL_MS = 100;
    static constexpr size_t SATURATION_THRESHOLD = 500;
};

template<typename F, typename... Args>
auto ThreadPool::submitTask(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
{
    using ReturnType = std::invoke_result_t<F, Args...>;
    
    auto taskPromise = std::make_shared<std::promise<ReturnType>>();
    auto future = taskPromise->get_future();
    
    auto task = std::make_shared<Task>([taskPromise, f = std::forward<F>(f), args...]() mutable {
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                f(args...);
                taskPromise->set_value();
            } else {
                auto result = f(args...);
                taskPromise->set_value(std::move(result));
            }
        } catch (...) {
            taskPromise->set_exception(std::current_exception());
        }
    }, 0, m_nextTaskId.fetchAndAddRelaxed(1));
    
    if (!submitTask(task)) {
        // If submission fails, set exception
        taskPromise->set_exception(std::make_exception_ptr(
            std::runtime_error("Failed to submit task to thread pool")));
    }
    
    return future;
}

} // namespace Threading  
} // namespace Monitor