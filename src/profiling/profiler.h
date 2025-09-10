#pragma once

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QHash>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QAtomicInteger>
#include <chrono>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace Monitor {
namespace Profiling {

using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::nanoseconds;

struct ProfileSample {
    QString name;
    TimePoint startTime;
    TimePoint endTime;
    Duration duration;
    qint64 threadId;
    QVariantMap metadata;
    
    ProfileSample() = default;
    ProfileSample(const QString& n, TimePoint start, TimePoint end);
    
    double durationMs() const;
    double durationUs() const;
    qint64 durationNs() const;
};

struct ProfileStats {
    QString name;
    qint64 callCount = 0;
    Duration totalTime = Duration::zero();
    Duration minTime = Duration::max();
    Duration maxTime = Duration::zero();
    Duration avgTime = Duration::zero();
    
    void addSample(const ProfileSample& sample);
    void reset();
    
    double totalTimeMs() const;
    double totalTimeUs() const;
    double minTimeUs() const;
    double maxTimeUs() const;
    double avgTimeUs() const;
};

class Profiler : public QObject
{
    Q_OBJECT

public:
    explicit Profiler(QObject* parent = nullptr);
    ~Profiler() override;
    
    static Profiler* instance();
    
    void beginProfile(const QString& name);
    void endProfile(const QString& name);
    
    void addSample(const ProfileSample& sample);
    void addSample(const QString& name, Duration duration);
    void addSample(const QString& name, TimePoint start, TimePoint end);
    
    ProfileStats getStats(const QString& name) const;
    QHash<QString, ProfileStats> getAllStats() const;
    
    void resetStats();
    void resetStats(const QString& name);
    
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled.loadRelaxed(); }
    
    void setAutoReport(bool enabled, int intervalMs = 5000);
    bool isAutoReportEnabled() const { return m_autoReportEnabled; }
    
    qint64 getTotalSamples() const { return m_totalSamples.loadRelaxed(); }
    
    QStringList getProfileNames() const;
    
    void dumpReport() const;
    QString generateReport() const;
    void generateAutoReport();

signals:
    void sampleAdded(const QString& name, const ProfileSample& sample);
    void statsUpdated(const QString& name, const ProfileStats& stats);
    void reportGenerated(const QString& report);

private slots:
    void onAutoReportTimer();

private:
    struct ActiveProfile {
        QString name;
        TimePoint startTime;
        QVariantMap metadata;
        
        ActiveProfile(const QString& n, TimePoint start) : name(n), startTime(start) {}
    };
    
    void updateStats(const QString& name, const ProfileSample& sample);
    
    static Profiler* s_instance;
    static QMutex s_instanceMutex;
    
    mutable QMutex m_statsMutex;
    mutable QMutex m_activeProfilesMutex;
    
    QHash<QString, ProfileStats> m_stats;
    QHash<qint64, std::vector<ActiveProfile>> m_activeProfiles; // per thread
    
    QTimer* m_autoReportTimer;
    
    QAtomicInteger<bool> m_enabled;
    QAtomicInteger<qint64> m_totalSamples;
    
    bool m_autoReportEnabled;
    
    static constexpr int DEFAULT_AUTO_REPORT_INTERVAL_MS = 5000;
};

class ScopedProfiler
{
public:
    explicit ScopedProfiler(const QString& name, Profiler* profiler = nullptr);
    ~ScopedProfiler();
    
    ScopedProfiler(const ScopedProfiler&) = delete;
    ScopedProfiler& operator=(const ScopedProfiler&) = delete;
    
    void setMetadata(const QString& key, const QVariant& value);

private:
    Profiler* m_profiler;
    QString m_name;
    TimePoint m_startTime;
    QVariantMap m_metadata;
    bool m_isActive;
};

class FrameRateProfiler : public QObject
{
    Q_OBJECT

public:
    explicit FrameRateProfiler(const QString& name, QObject* parent = nullptr);
    
    void frameStart();
    void frameEnd();
    
    double getCurrentFPS() const { return m_currentFPS; }
    double getAverageFPS() const { return m_averageFPS; }
    double getMinFPS() const { return m_minFPS; }
    double getMaxFPS() const { return m_maxFPS; }
    
    qint64 getFrameCount() const { return m_frameCount; }
    Duration getLastFrameTime() const { return m_lastFrameTime; }
    
    void reset();

signals:
    void frameCompleted(double fps, Duration frameTime);
    void fpsUpdated(double currentFPS, double averageFPS);

private:
    void updateStats(Duration frameTime);
    
    QString m_name;
    TimePoint m_frameStartTime;
    
    double m_currentFPS;
    double m_averageFPS;
    double m_minFPS;
    double m_maxFPS;
    
    qint64 m_frameCount;
    Duration m_totalFrameTime;
    Duration m_lastFrameTime;
    
    static constexpr int FPS_WINDOW_SIZE = 60;
    std::vector<Duration> m_frameTimeWindow;
    size_t m_windowIndex;
};

class MemoryProfiler : public QObject
{
    Q_OBJECT

public:
    explicit MemoryProfiler(QObject* parent = nullptr);
    
    struct MemorySnapshot {
        QDateTime timestamp;
        qint64 heapAllocated;
        qint64 stackUsed;
        qint64 virtualMemory;
        qint64 residentMemory;
        QHash<QString, qint64> poolUsage;
        
        MemorySnapshot();
    };
    
    void takeSnapshot();
    MemorySnapshot getCurrentSnapshot() const;
    
    std::vector<MemorySnapshot> getSnapshots() const;
    void clearSnapshots();
    
    void setAutoSnapshot(bool enabled, int intervalMs = 1000);
    bool isAutoSnapshotEnabled() const { return m_autoSnapshotEnabled; }
    
    qint64 getPeakHeapUsage() const { return m_peakHeapUsage; }
    qint64 getPeakVirtualUsage() const { return m_peakVirtualUsage; }

signals:
    void snapshotTaken(const MemorySnapshot& snapshot);
    void memoryPeakUpdated(qint64 heapPeak, qint64 virtualPeak);

private slots:
    void takeAutoSnapshot();

private:
    MemorySnapshot createSnapshot() const;
    
    mutable QMutex m_snapshotsMutex;
    std::vector<MemorySnapshot> m_snapshots;
    
    QTimer* m_autoSnapshotTimer;
    
    bool m_autoSnapshotEnabled;
    qint64 m_peakHeapUsage;
    qint64 m_peakVirtualUsage;
    
    size_t m_maxSnapshots;
    
    static constexpr size_t DEFAULT_MAX_SNAPSHOTS = 1000;
    static constexpr int DEFAULT_SNAPSHOT_INTERVAL_MS = 1000;
};

} // namespace Profiling
} // namespace Monitor

#define PROFILE_SCOPE(name) \
    Monitor::Profiling::ScopedProfiler PROFILE_VAR_NAME(__LINE__)(name)

#define PROFILE_FUNCTION() \
    Monitor::Profiling::ScopedProfiler PROFILE_VAR_NAME(__LINE__)(__FUNCTION__)

#define PROFILE_VAR_NAME(line) PROFILE_VAR_NAME_IMPL(line)
#define PROFILE_VAR_NAME_IMPL(line) profile_##line

#define PROFILE_BEGIN(name) \
    Monitor::Profiling::Profiler::instance()->beginProfile(name)

#define PROFILE_END(name) \
    Monitor::Profiling::Profiler::instance()->endProfile(name)