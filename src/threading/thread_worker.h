#pragma once

#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QAtomicInteger>
#include <functional>
#include <queue>
#include <memory>
#include <chrono>

namespace Monitor {
namespace Threading {

using TaskFunction = std::function<void()>;

struct Task {
    TaskFunction function;
    int priority;
    std::chrono::steady_clock::time_point enqueueTime;
    size_t id;
    
    Task() = default;
    Task(TaskFunction func, int prio = 0, size_t taskId = 0)
        : function(std::move(func))
        , priority(prio)
        , enqueueTime(std::chrono::steady_clock::now())
        , id(taskId)
    {}
    
    bool operator<(const Task& other) const {
        // Higher priority values have higher precedence
        return priority < other.priority;
    }
};

using TaskPtr = std::shared_ptr<Task>;

class ThreadPool; // Forward declaration

class ThreadWorker : public QThread
{
    Q_OBJECT

public:
    explicit ThreadWorker(int workerId, ThreadPool* pool, QObject* parent = nullptr);
    ~ThreadWorker() override;
    
    void start(Priority priority = NormalPriority);
    void stop();
    void wakeUp();
    
    bool addTask(TaskPtr task);
    TaskPtr stealTask(); // For work stealing
    
    // Statistics
    size_t getQueueSize() const;
    size_t getTasksProcessed() const { return m_tasksProcessed.loadRelaxed(); }
    size_t getTasksStolen() const { return m_tasksStolen.loadRelaxed(); }
    double getAverageTaskTime() const;
    bool isIdle() const { return m_isIdle.loadRelaxed(); }
    
    // CPU affinity control
    void setCpuAffinity(int coreId);
    int getCpuAffinity() const { return m_cpuAffinity; }

signals:
    void taskCompleted(size_t taskId, qint64 executionTimeUs);
    void workerIdle();
    void workerBusy();
    void taskStolen(int fromWorkerId, int toWorkerId);

protected:
    void run() override;

private:
    void processTaskQueue();
    void updateIdleState(bool idle);
    
    struct TaskComparator {
        bool operator()(const TaskPtr& a, const TaskPtr& b) const {
            return *a < *b; // Use Task's operator<
        }
    };
    
    using TaskQueue = std::priority_queue<TaskPtr, std::vector<TaskPtr>, TaskComparator>;
    
    int m_workerId;
    ThreadPool* m_threadPool;
    
    mutable QMutex m_queueMutex;
    QWaitCondition m_wakeCondition;
    TaskQueue m_taskQueue;
    
    QAtomicInteger<bool> m_isRunning;
    QAtomicInteger<bool> m_shouldStop;
    QAtomicInteger<bool> m_isIdle;
    QAtomicInteger<size_t> m_tasksProcessed;
    QAtomicInteger<size_t> m_tasksStolen;
    
    int m_cpuAffinity;
    
    // Performance tracking
    std::chrono::nanoseconds m_totalTaskTime;
    mutable QMutex m_statsMutex;
    
    static constexpr size_t MAX_QUEUE_SIZE = 1000;
    static constexpr int IDLE_TIMEOUT_MS = 100;
};

} // namespace Threading  
} // namespace Monitor