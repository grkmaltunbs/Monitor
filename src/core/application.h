#pragma once

#include <memory>
#include <vector>
#include <type_traits>
#include <QtCore/QObject>
#include <QtCore/QSettings>
#include <QtCore/QTimer>
#include <QtCore/QMutex>

#include "../events/event_dispatcher.h"
#include "../memory/memory_pool.h"
#include "../logging/logger.h"
#include "../profiling/profiler.h"

namespace Monitor {
namespace Core {

class Application : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString buildDate READ buildDate CONSTANT)
    Q_PROPERTY(bool isInitialized READ isInitialized NOTIFY initializationChanged)

public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;
    
    static Application* instance();
    
    bool initialize();
    void shutdown();
    
    QString version() const { return m_version; }
    QString buildDate() const { return m_buildDate; }
    bool isInitialized() const { return m_isInitialized; }
    
    Events::EventDispatcher* eventDispatcher() const { return m_eventDispatcher.get(); }
    Memory::MemoryPoolManager* memoryManager() const { return m_memoryManager.get(); }
    Logging::Logger* logger() const { return m_logger; }
    Profiling::Profiler* profiler() const { return m_profiler; }
    
    QSettings* settings() const { return m_settings.get(); }
    
    void setWorkingDirectory(const QString& path);
    QString getWorkingDirectory() const { return m_workingDirectory; }
    
    void setConfigPath(const QString& path);
    QString getConfigPath() const { return m_configPath; }
    
    void setLogPath(const QString& path);
    QString getLogPath() const { return m_logPath; }
    
    void registerErrorHandler();
    void handleCriticalError(const QString& error);
    
    bool isShuttingDown() const { return m_isShuttingDown; }
    
    qint64 getUptimeMs() const;
    QDateTime getStartTime() const { return m_startTime; }

signals:
    void initializationChanged(bool initialized);
    void shutdownRequested();
    void criticalError(const QString& error);
    void configurationChanged(const QString& key, const QVariant& value);

public slots:
    void requestShutdown();
    void saveConfiguration();
    void reloadConfiguration();

private slots:
    void onEventDispatcherError(const QString& error);
    void onMemoryPressure(double utilization);
    void performPeriodicMaintenance();

private:
    void initializeCore();
    void initializeMemoryPools();
    void initializeLogging();
    void initializeProfiling();
    void initializeEventSystem();
    
    void setupDefaultMemoryPools();
    void setupDefaultLogSinks();
    void setupPeriodicTasks();
    
    void cleanupResources();
    
    static Application* s_instance;
    static QMutex s_instanceMutex;
    
    std::unique_ptr<Events::EventDispatcher> m_eventDispatcher;
    std::unique_ptr<Memory::MemoryPoolManager> m_memoryManager;
    Logging::Logger* m_logger; // Singleton, don't delete
    Profiling::Profiler* m_profiler; // Singleton, don't delete
    
    std::unique_ptr<QSettings> m_settings;
    
    QString m_version;
    QString m_buildDate;
    QString m_workingDirectory;
    QString m_configPath;
    QString m_logPath;
    
    QDateTime m_startTime;
    QTimer* m_maintenanceTimer;
    
    bool m_isInitialized;
    bool m_isShuttingDown;
    
    mutable QMutex m_configMutex;
    
    // Default pool configurations
    struct PoolConfig {
        QString name;
        size_t blockSize;
        size_t blockCount;
    };
    
    static const std::vector<PoolConfig> s_defaultPools;
    
    static constexpr const char* VERSION = "0.1.0";
    static constexpr const char* BUILD_DATE = __DATE__ " " __TIME__;
    static constexpr int MAINTENANCE_INTERVAL_MS = 30000; // 30 seconds
};

} // namespace Core
} // namespace Monitor