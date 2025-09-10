#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QDateTime>
#include <memory>

namespace Monitor {
namespace Events {

enum class Priority : int {
    Lowest = 0,
    Low = 1,
    Normal = 2,
    High = 3,
    Critical = 4
};

class Event : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString type READ type CONSTANT)
    Q_PROPERTY(Priority priority READ priority CONSTANT)
    Q_PROPERTY(QDateTime timestamp READ timestamp CONSTANT)
    Q_PROPERTY(QVariantMap data READ data CONSTANT)

public:
    explicit Event(const QString& type, Priority priority = Priority::Normal, QObject* parent = nullptr);
    virtual ~Event() = default;

    const QString& type() const { return m_type; }
    Priority priority() const { return m_priority; }
    const QDateTime& timestamp() const { return m_timestamp; }
    const QVariantMap& data() const { return m_data; }
    
    void setData(const QString& key, const QVariant& value);
    QVariant getData(const QString& key, const QVariant& defaultValue = QVariant()) const;
    
    bool isConsumed() const { return m_consumed; }
    void consume() { m_consumed = true; }
    
    virtual QString toString() const;

protected:
    QString m_type;
    Priority m_priority;
    QDateTime m_timestamp;
    QVariantMap m_data;
    bool m_consumed;
};

class ApplicationEvent : public Event
{
    Q_OBJECT

public:
    enum Type {
        Startup,
        Shutdown,
        ConfigChanged,
        WorkspaceLoaded,
        WorkspaceSaved,
        ErrorOccurred
    };
    Q_ENUM(Type)

    explicit ApplicationEvent(Type eventType, Priority priority = Priority::Normal, QObject* parent = nullptr);
    
    Type eventType() const { return m_eventType; }

private:
    Type m_eventType;
};

class MemoryEvent : public Event
{
    Q_OBJECT

public:
    enum Type {
        AllocationFailed,
        MemoryPressure,
        PoolExhausted,
        LeakDetected
    };
    Q_ENUM(Type)

    explicit MemoryEvent(Type eventType, Priority priority = Priority::High, QObject* parent = nullptr);
    
    Type eventType() const { return m_eventType; }
    
    void setPoolName(const QString& poolName) { setData("poolName", poolName); }
    void setUtilization(double utilization) { setData("utilization", utilization); }
    void setBlockSize(size_t blockSize) { setData("blockSize", static_cast<qulonglong>(blockSize)); }

private:
    Type m_eventType;
};

class PerformanceEvent : public Event
{
    Q_OBJECT

public:
    enum Type {
        LatencyThresholdExceeded,
        ThroughputDropped,
        FrameRateDropped,
        CPUThresholdExceeded,
        MemoryThresholdExceeded
    };
    Q_ENUM(Type)

    explicit PerformanceEvent(Type eventType, Priority priority = Priority::High, QObject* parent = nullptr);
    
    Type eventType() const { return m_eventType; }
    
    void setLatency(qint64 microseconds) { setData("latency_us", microseconds); }
    void setThroughput(double packetsPerSecond) { setData("throughput_pps", packetsPerSecond); }
    void setFrameRate(double fps) { setData("frame_rate", fps); }
    void setCpuUsage(double percentage) { setData("cpu_usage", percentage); }
    void setMemoryUsage(qulonglong bytes) { setData("memory_usage", bytes); }

private:
    Type m_eventType;
};

using EventPtr = std::shared_ptr<Event>;
using ApplicationEventPtr = std::shared_ptr<ApplicationEvent>;
using MemoryEventPtr = std::shared_ptr<MemoryEvent>;
using PerformanceEventPtr = std::shared_ptr<PerformanceEvent>;

} // namespace Events
} // namespace Monitor