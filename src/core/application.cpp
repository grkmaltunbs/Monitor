#include "application.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QLoggingCategory>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <cstdio>

Q_LOGGING_CATEGORY(application, "Monitor.Core.Application")

namespace Monitor {
namespace Core {

Application* Application::s_instance = nullptr;
QMutex Application::s_instanceMutex;

const std::vector<Application::PoolConfig> Application::s_defaultPools = {
    {"PacketBuffer",    4096,   1000},   // 4KB blocks for packet data
    {"SmallObjects",    64,     10000},  // 64B blocks for small objects
    {"MediumObjects",   512,    5000},   // 512B blocks for medium objects
    {"LargeObjects",    8192,   500},    // 8KB blocks for large objects
    {"EventObjects",    256,    2000},   // 256B blocks for events
    {"StringCache",     128,    5000},   // 128B blocks for string caching
    {"WidgetData",      1024,   2000},   // 1KB blocks for widget data
    {"TestFramework",   2048,   1000}    // 2KB blocks for test framework
};

Application::Application(QObject* parent)
    : QObject(parent)
    , m_logger(Logging::Logger::instance())
    , m_profiler(Profiling::Profiler::instance())
    , m_version(VERSION)
    , m_buildDate(BUILD_DATE)
    , m_startTime(QDateTime::currentDateTime())
    , m_maintenanceTimer(new QTimer(this))
    , m_isInitialized(false)
    , m_isShuttingDown(false)
{
    qCInfo(application) << "Monitor Application created";
    qCInfo(application) << "Version:" << m_version;
    qCInfo(application) << "Build Date:" << m_buildDate;
    qCInfo(application) << "Start Time:" << m_startTime.toString(Qt::ISODate);
    
    m_maintenanceTimer->setSingleShot(false);
    m_maintenanceTimer->setInterval(MAINTENANCE_INTERVAL_MS);
    connect(m_maintenanceTimer, &QTimer::timeout, this, &Application::performPeriodicMaintenance);
}

Application::~Application()
{
    // Set shutdown flag FIRST to stop any new log handler access
    {
        QMutexLocker locker(&s_instanceMutex);
        m_isShuttingDown = true;
    }
    
    // Uninstall Qt message handler to prevent use-after-free during shutdown
    qInstallMessageHandler(nullptr);
    
    // Clear the static instance pointer to prevent access during destruction
    {
        QMutexLocker locker(&s_instanceMutex);
        if (s_instance == this) {
            s_instance = nullptr;
        }
    }
    
    // Now perform shutdown
    shutdown();
    
    // All done - use stderr directly to avoid Qt logging system
    fprintf(stderr, "Monitor Application destroyed\n");
}

Application* Application::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new Application(QCoreApplication::instance());
        }
    }
    return s_instance;
}

bool Application::initialize()
{
    if (m_isInitialized) {
        qCWarning(application) << "Application already initialized";
        return true;
    }
    
    PROFILE_FUNCTION();
    
    try {
        qCInfo(application) << "Initializing Monitor Application...";
        
        initializeCore();
        initializeMemoryPools();
        initializeLogging();
        initializeProfiling();
        initializeEventSystem();
        
        setupPeriodicTasks();
        
        m_isInitialized = true;
        emit initializationChanged(true);
        
        // Post initialization event
        auto initEvent = std::make_shared<Events::ApplicationEvent>(
            Events::ApplicationEvent::Startup, Events::Priority::High);
        initEvent->setData("version", m_version);
        initEvent->setData("buildDate", m_buildDate);
        initEvent->setData("startTime", m_startTime);
        
        if (m_eventDispatcher) {
            m_eventDispatcher->post(initEvent);
        }
        
        qCInfo(application) << "Monitor Application initialized successfully";
        return true;
        
    } catch (const std::exception& e) {
        qCCritical(application) << "Failed to initialize application:" << e.what();
        handleCriticalError(QString("Initialization failed: %1").arg(e.what()));
        return false;
    }
}

void Application::shutdown()
{
    if (m_isShuttingDown) {
        return;
    }
    
    qCInfo(application) << "Shutting down Monitor Application...";
    m_isShuttingDown = true;
    
    emit shutdownRequested();
    
    // Post shutdown event
    if (m_eventDispatcher) {
        auto shutdownEvent = std::make_shared<Events::ApplicationEvent>(
            Events::ApplicationEvent::Shutdown, Events::Priority::Critical);
        shutdownEvent->setData("uptime_ms", getUptimeMs());
        m_eventDispatcher->post(shutdownEvent);
        
        // Process any remaining events
        m_eventDispatcher->processQueuedEvents();
    }
    
    // Stop periodic tasks
    m_maintenanceTimer->stop();
    
    // Save configuration
    saveConfiguration();
    
    cleanupResources();
    
    m_isInitialized = false;
    m_isShuttingDown = false;  // Reset shutdown flag for next initialization
    emit initializationChanged(false);
    
    qCInfo(application) << "Monitor Application shutdown complete";
}

void Application::setWorkingDirectory(const QString& path)
{
    m_workingDirectory = path;
    QDir::setCurrent(path);
    qCInfo(application) << "Working directory set to:" << path;
}

void Application::setConfigPath(const QString& path)
{
    QMutexLocker locker(&m_configMutex);
    m_configPath = path;
    
    // Recreate settings with new path
    m_settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
    qCInfo(application) << "Configuration path set to:" << path;
}

void Application::setLogPath(const QString& path)
{
    m_logPath = path;
    qCInfo(application) << "Log path set to:" << path;
}

void Application::registerErrorHandler()
{
    // Set up Qt message handler to route to our logging system
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext& context, const QString& msg) {
        // Thread-safe access to logger - hold mutex during entire access
        Logging::Logger* logger = nullptr;
        
        {
            QMutexLocker locker(&s_instanceMutex);
            if (s_instance && !s_instance->isShuttingDown()) {
                logger = s_instance->logger();
            }
        }
        
        if (logger) {
            // We have a valid logger and we're not shutting down
            Logging::LogLevel level;
            switch (type) {
                case QtDebugMsg:    level = Logging::LogLevel::Debug; break;
                case QtInfoMsg:     level = Logging::LogLevel::Info; break;
                case QtWarningMsg:  level = Logging::LogLevel::Warning; break;
                case QtCriticalMsg: level = Logging::LogLevel::Error; break;
                case QtFatalMsg:    level = Logging::LogLevel::Critical; break;
            }
            
            logger->log(level, context.category ? context.category : "Qt",
                       msg, context.file, context.function, context.line);
            return; // Successfully logged, don't fall through to default handler
        }
        
        // Fallback: print to stderr if no logger available or during shutdown
        if (type != QtDebugMsg) { // Don't spam with debug messages during shutdown
            fprintf(stderr, "Qt[%s]: %s\n", 
                   context.category ? context.category : "Unknown", 
                   msg.toLocal8Bit().constData());
        }
        
        // Fallback to default handler for fatal messages
        if (type == QtFatalMsg) {
            abort();
        }
    });
}

void Application::handleCriticalError(const QString& error)
{
    qCCritical(application) << "Critical error:" << error;
    emit criticalError(error);
    
    // Post critical error event
    if (m_eventDispatcher) {
        auto errorEvent = std::make_shared<Events::ApplicationEvent>(
            Events::ApplicationEvent::ErrorOccurred, Events::Priority::Critical);
        errorEvent->setData("error", error);
        errorEvent->setData("timestamp", QDateTime::currentDateTime());
        m_eventDispatcher->post(errorEvent);
    }
    
    // Perform emergency save
    saveConfiguration();
    
    // Consider shutdown if this is a fatal error
    // For now, just log - could add auto-restart logic here
}

qint64 Application::getUptimeMs() const
{
    return m_startTime.msecsTo(QDateTime::currentDateTime());
}

void Application::requestShutdown()
{
    qCInfo(application) << "Shutdown requested";
    emit shutdownRequested();
    QCoreApplication::quit();
}

void Application::saveConfiguration()
{
    PROFILE_SCOPE("Application::saveConfiguration");
    
    QMutexLocker locker(&m_configMutex);
    
    if (!m_settings) {
        qCWarning(application) << "No settings object available for save";
        return;
    }
    
    m_settings->setValue("application/version", m_version);
    m_settings->setValue("application/lastRun", QDateTime::currentDateTime());
    m_settings->setValue("application/totalUptime", 
                        m_settings->value("application/totalUptime", 0).toLongLong() + getUptimeMs());
    
    m_settings->sync();
    
    if (m_settings->status() != QSettings::NoError) {
        qCWarning(application) << "Failed to save configuration";
    } else {
        qCDebug(application) << "Configuration saved successfully";
    }
    
    emit configurationChanged("", QVariant());
}

void Application::reloadConfiguration()
{
    PROFILE_SCOPE("Application::reloadConfiguration");
    
    QMutexLocker locker(&m_configMutex);
    
    if (!m_settings) {
        qCWarning(application) << "No settings object available for reload";
        return;
    }
    
    m_settings->sync();
    qCInfo(application) << "Configuration reloaded";
    
    emit configurationChanged("", QVariant());
}


void Application::onEventDispatcherError(const QString& error)
{
    qCWarning(application) << "Event dispatcher error:" << error;
    handleCriticalError(QString("Event system error: %1").arg(error));
}

void Application::onMemoryPressure(double utilization)
{
    qCWarning(application) << "Memory pressure detected:" << utilization;
    
    // Post memory pressure event
    auto memoryEvent = std::make_shared<Events::MemoryEvent>(
        Events::MemoryEvent::MemoryPressure, Events::Priority::High);
    memoryEvent->setUtilization(utilization);
    
    if (m_eventDispatcher) {
        m_eventDispatcher->post(memoryEvent);
    }
    
    // Could trigger garbage collection or other cleanup here
}

void Application::performPeriodicMaintenance()
{
    PROFILE_SCOPE("Application::performPeriodicMaintenance");
    
    // Cleanup disconnected event handlers
    if (m_eventDispatcher) {
        // Event dispatcher handles this internally
    }
    
    // Log statistics periodically
    if (m_profiler && m_profiler->isEnabled()) {
        static int profileReportCounter = 0;
        if (++profileReportCounter >= 10) { // Every 5 minutes
            m_profiler->generateAutoReport();
            profileReportCounter = 0;
        }
    }
    
    // Memory pool statistics
    if (m_memoryManager) {
        double utilization = m_memoryManager->getTotalUtilization();
        if (utilization > 0.9) { // 90% utilization
            qCWarning(application) << "High memory pool utilization:" << utilization;
        }
    }
    
    qCDebug(application) << "Periodic maintenance completed";
}

void Application::initializeCore()
{
    PROFILE_SCOPE("Application::initializeCore");
    
    // Set up working directory
    if (m_workingDirectory.isEmpty()) {
        m_workingDirectory = QDir::currentPath();
    }
    
    // Set up config path
    if (m_configPath.isEmpty()) {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        QDir().mkpath(configDir + "/Monitor");
        m_configPath = configDir + "/Monitor/config.ini";
    }
    
    // Set up log path
    if (m_logPath.isEmpty()) {
        QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(logDir + "/logs");
        m_logPath = logDir + "/logs";
    } else {
        // Ensure the log directory exists even if path was set explicitly
        QDir().mkpath(m_logPath);
    }
    
    // Initialize settings
    m_settings = std::make_unique<QSettings>(m_configPath, QSettings::IniFormat);
    
    qCInfo(application) << "Core initialization complete";
    qCInfo(application) << "Working directory:" << m_workingDirectory;
    qCInfo(application) << "Config path:" << m_configPath;
    qCInfo(application) << "Log path:" << m_logPath;
}

void Application::initializeMemoryPools()
{
    PROFILE_SCOPE("Application::initializeMemoryPools");
    
    m_memoryManager = std::make_unique<Memory::MemoryPoolManager>(this);
    
    connect(m_memoryManager.get(), &Memory::MemoryPoolManager::globalMemoryPressure,
            this, &Application::onMemoryPressure);
    
    setupDefaultMemoryPools();
    
    qCInfo(application) << "Memory pools initialized";
}

void Application::initializeLogging()
{
    PROFILE_SCOPE("Application::initializeLogging");
    
    setupDefaultLogSinks();
    registerErrorHandler();
    
    qCInfo(application) << "Logging system initialized";
}

void Application::initializeProfiling()
{
    PROFILE_SCOPE("Application::initializeProfiling");
    
    // Profiler is a singleton, already available
    m_profiler->setEnabled(true);
    m_profiler->setAutoReport(false); // We'll handle this manually
    
    qCInfo(application) << "Profiling system initialized";
}

void Application::initializeEventSystem()
{
    PROFILE_SCOPE("Application::initializeEventSystem");
    
    m_eventDispatcher = std::make_unique<Events::EventDispatcher>(this);
    
    connect(m_eventDispatcher.get(), &Events::EventDispatcher::errorOccurred,
            this, &Application::onEventDispatcherError);
    
    m_eventDispatcher->start();
    
    qCInfo(application) << "Event system initialized";
}

void Application::setupDefaultMemoryPools()
{
    for (const auto& config : s_defaultPools) {
        m_memoryManager->createPool(config.name, config.blockSize, config.blockCount);
        qCDebug(application) << "Created memory pool:" << config.name 
                            << "(" << config.blockSize << "B x" << config.blockCount << ")";
    }
}

void Application::setupDefaultLogSinks()
{
    // Console sink (already set up by Logger singleton)
    
    // File sink
    QString logFile = m_logPath + "/monitor.log";
    auto fileSink = std::make_shared<Logging::FileSink>(logFile);
    fileSink->setMinLevel(Logging::LogLevel::Debug);
    fileSink->setMaxFileSize(50 * 1024 * 1024); // 50MB
    fileSink->setMaxFiles(5);
    m_logger->addSink(fileSink);
    
    // Memory sink for runtime log viewing
    auto memorySink = std::make_shared<Logging::MemorySink>(10000);
    memorySink->setMinLevel(Logging::LogLevel::Info);
    m_logger->addSink(memorySink);
    
    qCDebug(application) << "Default log sinks configured";
}

void Application::setupPeriodicTasks()
{
    m_maintenanceTimer->start();
    qCDebug(application) << "Periodic tasks started";
}

void Application::cleanupResources()
{
    PROFILE_SCOPE("Application::cleanupResources");
    
    // Stop event system
    if (m_eventDispatcher) {
        m_eventDispatcher->stop();
        m_eventDispatcher.reset();
    }
    
    // Cleanup memory pools
    if (m_memoryManager) {
        m_memoryManager.reset();
    }
    
    // Flush all logs
    if (m_logger) {
        m_logger->flushAndWait();
    }
    
    qCDebug(application) << "Resources cleaned up";
}

} // namespace Core
} // namespace Monitor

