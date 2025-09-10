#include "profiler.h"
#include <QtCore/QThread>
#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtCore/QDebug>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#include <sys/resource.h>
#elif defined(Q_OS_MACOS)
#include <mach/mach.h>
#include <mach/task.h>
#endif

Q_LOGGING_CATEGORY(profiler, "Monitor.Profiling.Profiler")

namespace Monitor {
namespace Profiling {

ProfileSample::ProfileSample(const QString& n, TimePoint start, TimePoint end)
    : name(n)
    , startTime(start)
    , endTime(end)
    , duration(std::chrono::duration_cast<Duration>(end - start))
    , threadId(reinterpret_cast<qint64>(QThread::currentThreadId()))
{
}

double ProfileSample::durationMs() const
{
    return std::chrono::duration<double, std::milli>(duration).count();
}

double ProfileSample::durationUs() const
{
    return std::chrono::duration<double, std::micro>(duration).count();
}

qint64 ProfileSample::durationNs() const
{
    return duration.count();
}

void ProfileStats::addSample(const ProfileSample& sample)
{
    callCount++;
    totalTime += sample.duration;
    
    if (sample.duration < minTime) {
        minTime = sample.duration;
    }
    
    if (sample.duration > maxTime) {
        maxTime = sample.duration;
    }
    
    avgTime = totalTime / callCount;
}

void ProfileStats::reset()
{
    callCount = 0;
    totalTime = Duration::zero();
    minTime = Duration::max();
    maxTime = Duration::zero();
    avgTime = Duration::zero();
}

double ProfileStats::totalTimeMs() const
{
    return std::chrono::duration<double, std::milli>(totalTime).count();
}

double ProfileStats::totalTimeUs() const
{
    return std::chrono::duration<double, std::micro>(totalTime).count();
}

double ProfileStats::minTimeUs() const
{
    return minTime == Duration::max() ? 0.0 : std::chrono::duration<double, std::micro>(minTime).count();
}

double ProfileStats::maxTimeUs() const
{
    return std::chrono::duration<double, std::micro>(maxTime).count();
}

double ProfileStats::avgTimeUs() const
{
    return std::chrono::duration<double, std::micro>(avgTime).count();
}

Profiler* Profiler::s_instance = nullptr;
QMutex Profiler::s_instanceMutex;

Profiler::Profiler(QObject* parent)
    : QObject(parent)
    , m_autoReportTimer(new QTimer(this))
    , m_enabled(true)
    , m_totalSamples(0)
    , m_autoReportEnabled(false)
{
    m_autoReportTimer->setSingleShot(false);
    m_autoReportTimer->setInterval(DEFAULT_AUTO_REPORT_INTERVAL_MS);
    connect(m_autoReportTimer, &QTimer::timeout, this, &Profiler::onAutoReportTimer);
    
    qCInfo(profiler) << "Profiler created";
}

Profiler::~Profiler()
{
    if (m_autoReportEnabled) {
        generateAutoReport();
    }
    qCInfo(profiler) << "Profiler destroyed";
}

Profiler* Profiler::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new Profiler(QCoreApplication::instance());
        }
    }
    return s_instance;
}

void Profiler::beginProfile(const QString& name)
{
    if (!m_enabled.loadRelaxed()) {
        return;
    }
    
    qint64 threadId = reinterpret_cast<qint64>(QThread::currentThreadId());
    TimePoint now = std::chrono::steady_clock::now();
    
    QMutexLocker locker(&m_activeProfilesMutex);
    m_activeProfiles[threadId].emplace_back(name, now);
}

void Profiler::endProfile(const QString& name)
{
    if (!m_enabled.loadRelaxed()) {
        return;
    }
    
    TimePoint endTime = std::chrono::steady_clock::now();
    qint64 threadId = reinterpret_cast<qint64>(QThread::currentThreadId());
    
    QMutexLocker locker(&m_activeProfilesMutex);
    
    auto threadProfilesIt = m_activeProfiles.find(threadId);
    if (threadProfilesIt == m_activeProfiles.end()) {
        qCWarning(profiler) << "No active profiles found for thread" << threadId;
        return;
    }
    
    auto& threadProfiles = threadProfilesIt.value();
    
    // Find the most recent profile with matching name (LIFO order for nested calls)
    for (auto it = threadProfiles.rbegin(); it != threadProfiles.rend(); ++it) {
        if (it->name == name) {
            ProfileSample sample(name, it->startTime, endTime);
            sample.metadata = it->metadata;
            
            // Remove from active profiles
            threadProfiles.erase(std::next(it).base());
            
            locker.unlock();
            
            addSample(sample);
            return;
        }
    }
    
    qCWarning(profiler) << "No matching active profile found for" << name << "in thread" << threadId;
}

void Profiler::addSample(const ProfileSample& sample)
{
    if (!m_enabled.loadRelaxed()) {
        return;
    }
    
    updateStats(sample.name, sample);
    
    m_totalSamples.fetchAndAddRelaxed(1);
    emit sampleAdded(sample.name, sample);
}

void Profiler::addSample(const QString& name, Duration duration)
{
    TimePoint now = std::chrono::steady_clock::now();
    TimePoint start = now - duration;
    ProfileSample sample(name, start, now);
    addSample(sample);
}

void Profiler::addSample(const QString& name, TimePoint start, TimePoint end)
{
    ProfileSample sample(name, start, end);
    addSample(sample);
}

ProfileStats Profiler::getStats(const QString& name) const
{
    QMutexLocker locker(&m_statsMutex);
    return m_stats.value(name, ProfileStats{name});
}

QHash<QString, ProfileStats> Profiler::getAllStats() const
{
    QMutexLocker locker(&m_statsMutex);
    return m_stats;
}

void Profiler::resetStats()
{
    QMutexLocker locker(&m_statsMutex);
    m_stats.clear();
    m_totalSamples.storeRelaxed(0);
    qCInfo(profiler) << "All profiling stats reset";
}

void Profiler::resetStats(const QString& name)
{
    QMutexLocker locker(&m_statsMutex);
    auto it = m_stats.find(name);
    if (it != m_stats.end()) {
        it->reset();
        qCInfo(profiler) << "Profiling stats reset for" << name;
    }
}

void Profiler::setEnabled(bool enabled)
{
    m_enabled.storeRelaxed(enabled);
    qCInfo(profiler) << "Profiler" << (enabled ? "enabled" : "disabled");
}

void Profiler::setAutoReport(bool enabled, int intervalMs)
{
    m_autoReportEnabled = enabled;
    
    if (enabled) {
        m_autoReportTimer->setInterval(intervalMs);
        m_autoReportTimer->start();
        qCInfo(profiler) << "Auto-report enabled with interval" << intervalMs << "ms";
    } else {
        m_autoReportTimer->stop();
        qCInfo(profiler) << "Auto-report disabled";
    }
}

QStringList Profiler::getProfileNames() const
{
    QMutexLocker locker(&m_statsMutex);
    return m_stats.keys();
}

void Profiler::dumpReport() const
{
    QString report = generateReport();
    qCInfo(profiler).noquote() << report;
}

QString Profiler::generateReport() const
{
    QMutexLocker locker(&m_statsMutex);
    
    QString report;
    QTextStream stream(&report);
    
    stream << "=== Performance Profile Report ===" << Qt::endl;
    stream << "Total samples: " << m_totalSamples.loadRelaxed() << Qt::endl;
    stream << "Unique profiles: " << m_stats.size() << Qt::endl;
    stream << Qt::endl;
    
    if (m_stats.isEmpty()) {
        stream << "No profiling data available." << Qt::endl;
        return report;
    }
    
    // Sort by total time (descending)
    std::vector<std::pair<QString, ProfileStats>> sortedStats;
    for (auto it = m_stats.begin(); it != m_stats.end(); ++it) {
        sortedStats.emplace_back(it.key(), it.value());
    }
    
    std::sort(sortedStats.begin(), sortedStats.end(),
              [](const auto& a, const auto& b) {
                  return a.second.totalTime > b.second.totalTime;
              });
    
    stream << QString("%1 %2 %3 %4 %5 %6")
                 .arg("Name", -30)
                 .arg("Calls", 8)
                 .arg("Total(ms)", 12)
                 .arg("Avg(μs)", 10)
                 .arg("Min(μs)", 10)
                 .arg("Max(μs)", 10) << Qt::endl;
    
    stream << QString(86, '-') << Qt::endl;
    
    for (const auto& [name, stats] : sortedStats) {
        stream << QString("%1 %2 %3 %4 %5 %6")
                     .arg(name, -30)
                     .arg(stats.callCount, 8)
                     .arg(stats.totalTimeMs(), 12, 'f', 3)
                     .arg(stats.avgTimeUs(), 10, 'f', 1)
                     .arg(stats.minTimeUs(), 10, 'f', 1)
                     .arg(stats.maxTimeUs(), 10, 'f', 1) << Qt::endl;
    }
    
    return report;
}

void Profiler::generateAutoReport()
{
    QString report = generateReport();
    emit reportGenerated(report);
}

void Profiler::onAutoReportTimer()
{
    generateAutoReport();
}

void Profiler::updateStats(const QString& name, const ProfileSample& sample)
{
    QMutexLocker locker(&m_statsMutex);
    
    auto it = m_stats.find(name);
    if (it == m_stats.end()) {
        it = m_stats.insert(name, ProfileStats{name});
    }
    
    it->addSample(sample);
    
    emit statsUpdated(name, *it);
}

ScopedProfiler::ScopedProfiler(const QString& name, Profiler* profiler)
    : m_profiler(profiler ? profiler : Profiler::instance())
    , m_name(name)
    , m_startTime(std::chrono::steady_clock::now())
    , m_isActive(true)
{
    if (m_profiler->isEnabled()) {
        m_profiler->beginProfile(m_name);
    }
}

ScopedProfiler::~ScopedProfiler()
{
    if (m_isActive && m_profiler && m_profiler->isEnabled()) {
        m_profiler->endProfile(m_name);
    }
}

void ScopedProfiler::setMetadata(const QString& key, const QVariant& value)
{
    m_metadata[key] = value;
}

FrameRateProfiler::FrameRateProfiler(const QString& name, QObject* parent)
    : QObject(parent)
    , m_name(name)
    , m_currentFPS(0.0)
    , m_averageFPS(0.0)
    , m_minFPS(std::numeric_limits<double>::max())
    , m_maxFPS(0.0)
    , m_frameCount(0)
    , m_totalFrameTime(Duration::zero())
    , m_lastFrameTime(Duration::zero())
    , m_windowIndex(0)
{
    m_frameTimeWindow.resize(FPS_WINDOW_SIZE, Duration::zero());
}

void FrameRateProfiler::frameStart()
{
    m_frameStartTime = std::chrono::steady_clock::now();
}

void FrameRateProfiler::frameEnd()
{
    TimePoint endTime = std::chrono::steady_clock::now();
    Duration frameTime = std::chrono::duration_cast<Duration>(endTime - m_frameStartTime);
    
    updateStats(frameTime);
    
    emit frameCompleted(m_currentFPS, frameTime);
}

void FrameRateProfiler::reset()
{
    m_currentFPS = 0.0;
    m_averageFPS = 0.0;
    m_minFPS = std::numeric_limits<double>::max();
    m_maxFPS = 0.0;
    m_frameCount = 0;
    m_totalFrameTime = Duration::zero();
    m_lastFrameTime = Duration::zero();
    m_windowIndex = 0;
    std::fill(m_frameTimeWindow.begin(), m_frameTimeWindow.end(), Duration::zero());
}

void FrameRateProfiler::updateStats(Duration frameTime)
{
    m_frameCount++;
    m_totalFrameTime += frameTime;
    m_lastFrameTime = frameTime;
    
    // Calculate current FPS based on this frame
    if (frameTime.count() > 0) {
        m_currentFPS = 1e9 / frameTime.count(); // nanoseconds to FPS
    }
    
    // Update rolling window
    m_frameTimeWindow[m_windowIndex] = frameTime;
    m_windowIndex = (m_windowIndex + 1) % FPS_WINDOW_SIZE;
    
    // Calculate windowed average FPS
    Duration windowTotal = Duration::zero();
    int validFrames = std::min(static_cast<int>(m_frameCount), FPS_WINDOW_SIZE);
    
    for (int i = 0; i < validFrames; ++i) {
        windowTotal += m_frameTimeWindow[i];
    }
    
    if (validFrames > 0 && windowTotal.count() > 0) {
        Duration avgFrameTime = windowTotal / validFrames;
        double windowFPS = 1e9 / avgFrameTime.count();
        
        m_averageFPS = windowFPS;
        
        if (windowFPS < m_minFPS) {
            m_minFPS = windowFPS;
        }
        
        if (windowFPS > m_maxFPS) {
            m_maxFPS = windowFPS;
        }
    }
    
    emit fpsUpdated(m_currentFPS, m_averageFPS);
}

MemoryProfiler::MemorySnapshot::MemorySnapshot()
    : timestamp(QDateTime::currentDateTime())
    , heapAllocated(0)
    , stackUsed(0)
    , virtualMemory(0)
    , residentMemory(0)
{
}

MemoryProfiler::MemoryProfiler(QObject* parent)
    : QObject(parent)
    , m_autoSnapshotTimer(new QTimer(this))
    , m_autoSnapshotEnabled(false)
    , m_peakHeapUsage(0)
    , m_peakVirtualUsage(0)
    , m_maxSnapshots(DEFAULT_MAX_SNAPSHOTS)
{
    m_autoSnapshotTimer->setSingleShot(false);
    m_autoSnapshotTimer->setInterval(DEFAULT_SNAPSHOT_INTERVAL_MS);
    connect(m_autoSnapshotTimer, &QTimer::timeout, this, &MemoryProfiler::takeAutoSnapshot);
}

void MemoryProfiler::takeSnapshot()
{
    MemorySnapshot snapshot = createSnapshot();
    
    {
        QMutexLocker locker(&m_snapshotsMutex);
        
        if (m_snapshots.size() >= m_maxSnapshots) {
            m_snapshots.erase(m_snapshots.begin());
        }
        
        m_snapshots.push_back(snapshot);
    }
    
    // Update peaks
    if (snapshot.heapAllocated > m_peakHeapUsage) {
        m_peakHeapUsage = snapshot.heapAllocated;
    }
    
    if (snapshot.virtualMemory > m_peakVirtualUsage) {
        m_peakVirtualUsage = snapshot.virtualMemory;
    }
    
    emit snapshotTaken(snapshot);
    emit memoryPeakUpdated(m_peakHeapUsage, m_peakVirtualUsage);
}

MemoryProfiler::MemorySnapshot MemoryProfiler::getCurrentSnapshot() const
{
    return createSnapshot();
}

std::vector<MemoryProfiler::MemorySnapshot> MemoryProfiler::getSnapshots() const
{
    QMutexLocker locker(&m_snapshotsMutex);
    return m_snapshots;
}

void MemoryProfiler::clearSnapshots()
{
    QMutexLocker locker(&m_snapshotsMutex);
    m_snapshots.clear();
}

void MemoryProfiler::setAutoSnapshot(bool enabled, int intervalMs)
{
    m_autoSnapshotEnabled = enabled;
    
    if (enabled) {
        m_autoSnapshotTimer->setInterval(intervalMs);
        m_autoSnapshotTimer->start();
    } else {
        m_autoSnapshotTimer->stop();
    }
}

void MemoryProfiler::takeAutoSnapshot()
{
    takeSnapshot();
}

MemoryProfiler::MemorySnapshot MemoryProfiler::createSnapshot() const
{
    MemorySnapshot snapshot;
    
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        snapshot.heapAllocated = pmc.WorkingSetSize;
        snapshot.virtualMemory = pmc.PagefileUsage;
        snapshot.residentMemory = pmc.WorkingSetSize;
    }
#elif defined(Q_OS_LINUX)
    // Linux-specific memory information
    FILE* file = fopen("/proc/self/status", "r");
    if (file) {
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "VmSize:", 7) == 0) {
                sscanf(line, "VmSize: %lld kB", &snapshot.virtualMemory);
                snapshot.virtualMemory *= 1024;
            } else if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line, "VmRSS: %lld kB", &snapshot.residentMemory);
                snapshot.residentMemory *= 1024;
            }
        }
        fclose(file);
    }
#elif defined(Q_OS_MACOS)
    task_basic_info_data_t info;
    mach_msg_type_number_t size = sizeof(info);
    kern_return_t kerr = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &size);
    if (kerr == KERN_SUCCESS) {
        snapshot.virtualMemory = info.virtual_size;
        snapshot.residentMemory = info.resident_size;
        snapshot.heapAllocated = info.resident_size; // Approximation
    }
#endif
    
    return snapshot;
}

} // namespace Profiling
} // namespace Monitor

