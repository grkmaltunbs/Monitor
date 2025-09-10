#pragma once

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QLoggingCategory>
#include <QtCore/QDateTime>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QAtomicInteger>
#include <memory>
#include <vector>
#include <queue>

namespace Monitor {
namespace Logging {

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5
};

struct LogEntry {
    QDateTime timestamp;
    LogLevel level;
    QString category;
    QString message;
    QString file;
    QString function;
    int line;
    qint64 threadId;
    
    LogEntry() = default;
    LogEntry(LogLevel lvl, const QString& cat, const QString& msg, 
             const QString& f = QString(), const QString& func = QString(), int ln = 0);
    
    QString toString(const QString& format = QString()) const;
    QByteArray toJson() const;
};

class LogSink : public QObject
{
    Q_OBJECT

public:
    explicit LogSink(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~LogSink() = default;
    
    virtual void write(const LogEntry& entry) = 0;
    virtual void flush() = 0;
    
    void setMinLevel(LogLevel level) { m_minLevel = level; }
    LogLevel getMinLevel() const { return m_minLevel; }
    
    bool shouldLog(LogLevel level) const { return level >= m_minLevel; }

protected:
    LogLevel m_minLevel = LogLevel::Info;
};

class ConsoleSink : public LogSink
{
    Q_OBJECT

public:
    explicit ConsoleSink(QObject* parent = nullptr);
    
    void write(const LogEntry& entry) override;
    void flush() override;
    
    void setUseColors(bool useColors) { m_useColors = useColors; }
    bool getUseColors() const { return m_useColors; }

private:
    QString colorizeLevel(LogLevel level) const;
    
    bool m_useColors;
    QTextStream m_stdout;
    QTextStream m_stderr;
    mutable QMutex m_writeMutex;
};

class FileSink : public LogSink
{
    Q_OBJECT

public:
    explicit FileSink(const QString& filePath, QObject* parent = nullptr);
    ~FileSink() override;
    
    void write(const LogEntry& entry) override;
    void flush() override;
    
    void setMaxFileSize(qint64 maxSize) { m_maxFileSize = maxSize; }
    qint64 getMaxFileSize() const { return m_maxFileSize; }
    
    void setMaxFiles(int maxFiles) { m_maxFiles = maxFiles; }
    int getMaxFiles() const { return m_maxFiles; }
    
    void setAutoFlush(bool autoFlush) { m_autoFlush = autoFlush; }
    bool getAutoFlush() const { return m_autoFlush; }
    
    QString getFilePath() const { return m_filePath; }
    qint64 getCurrentFileSize() const;

signals:
    void fileRotated(const QString& oldPath, const QString& newPath);
    void writeError(const QString& error);

private:
    void rotateFile();
    QString getRotatedFileName(int index) const;
    
    QString m_filePath;
    std::unique_ptr<QFile> m_file;
    std::unique_ptr<QTextStream> m_stream;
    
    qint64 m_maxFileSize;
    int m_maxFiles;
    bool m_autoFlush;
    
    mutable QMutex m_writeMutex;
};

class MemorySink : public LogSink
{
    Q_OBJECT

public:
    explicit MemorySink(size_t maxEntries = 10000, QObject* parent = nullptr);
    
    void write(const LogEntry& entry) override;
    void flush() override;
    
    std::vector<LogEntry> getEntries() const;
    std::vector<LogEntry> getEntriesByLevel(LogLevel level) const;
    std::vector<LogEntry> getEntriesByCategory(const QString& category) const;
    std::vector<LogEntry> getEntriesInRange(const QDateTime& start, const QDateTime& end) const;
    
    void clear();
    size_t getEntryCount() const;
    size_t getMaxEntries() const { return m_maxEntries; }

signals:
    void bufferFull();

private:
    mutable QMutex m_entriesMutex;
    std::queue<LogEntry> m_entries;
    size_t m_maxEntries;
};

class Logger : public QObject
{
    Q_OBJECT

public:
    explicit Logger(QObject* parent = nullptr);
    ~Logger() override;
    
    static Logger* instance();
    
    void addSink(std::shared_ptr<LogSink> sink);
    void removeSink(LogSink* sink);
    void clearSinks();
    
    void log(LogLevel level, const QString& category, const QString& message,
             const QString& file = QString(), const QString& function = QString(), int line = 0);
    
    void trace(const QString& category, const QString& message);
    void debug(const QString& category, const QString& message);
    void info(const QString& category, const QString& message);
    void warning(const QString& category, const QString& message);
    void error(const QString& category, const QString& message);
    void critical(const QString& category, const QString& message);
    
    void setGlobalLogLevel(LogLevel level);
    LogLevel getGlobalLogLevel() const { return m_globalLevel; }
    
    void setCategoryLevel(const QString& category, LogLevel level);
    void removeCategoryLevel(const QString& category);
    LogLevel getCategoryLevel(const QString& category) const;
    
    void setAsynchronous(bool async);
    bool isAsynchronous() const { return m_isAsynchronous; }
    
    void flush();
    void flushAndWait();
    
    qint64 getLoggedCount() const { return m_loggedCount.loadRelaxed(); }
    qint64 getDroppedCount() const { return m_droppedCount.loadRelaxed(); }

signals:
    void logEntryCreated(const LogEntry& entry);
    void queueFull(size_t queueSize);

private slots:
    void processAsyncLogs();

private:
    void logSync(const LogEntry& entry);
    void logAsync(const LogEntry& entry);
    bool shouldLog(LogLevel level, const QString& category) const;
    
    static Logger* s_instance;
    static QMutex s_instanceMutex;
    
    mutable QMutex m_sinksMutex;
    mutable QMutex m_configMutex;
    mutable QMutex m_asyncMutex;
    
    std::vector<std::shared_ptr<LogSink>> m_sinks;
    std::queue<LogEntry> m_asyncQueue;
    
    QTimer* m_asyncTimer;
    
    LogLevel m_globalLevel;
    QHash<QString, LogLevel> m_categoryLevels;
    
    bool m_isAsynchronous;
    size_t m_maxAsyncQueueSize;
    
    QAtomicInteger<qint64> m_loggedCount;
    QAtomicInteger<qint64> m_droppedCount;
    
    static constexpr size_t DEFAULT_MAX_ASYNC_QUEUE_SIZE = 10000;
    static constexpr int ASYNC_TIMER_INTERVAL_MS = 10;
};

} // namespace Logging
} // namespace Monitor

#define LOG_TRACE(category, message) \
    Monitor::Logging::Logger::instance()->trace(category, message)

#define LOG_DEBUG(category, message) \
    Monitor::Logging::Logger::instance()->debug(category, message)

#define LOG_INFO(category, message) \
    Monitor::Logging::Logger::instance()->info(category, message)

#define LOG_WARNING(category, message) \
    Monitor::Logging::Logger::instance()->warning(category, message)

#define LOG_ERROR(category, message) \
    Monitor::Logging::Logger::instance()->error(category, message)

#define LOG_CRITICAL(category, message) \
    Monitor::Logging::Logger::instance()->critical(category, message)

#define LOG_TRACE_FL(category, message) \
    Monitor::Logging::Logger::instance()->log(Monitor::Logging::LogLevel::Trace, category, message, __FILE__, __FUNCTION__, __LINE__)

#define LOG_DEBUG_FL(category, message) \
    Monitor::Logging::Logger::instance()->log(Monitor::Logging::LogLevel::Debug, category, message, __FILE__, __FUNCTION__, __LINE__)

#define LOG_INFO_FL(category, message) \
    Monitor::Logging::Logger::instance()->log(Monitor::Logging::LogLevel::Info, category, message, __FILE__, __FUNCTION__, __LINE__)

#define LOG_WARNING_FL(category, message) \
    Monitor::Logging::Logger::instance()->log(Monitor::Logging::LogLevel::Warning, category, message, __FILE__, __FUNCTION__, __LINE__)

#define LOG_ERROR_FL(category, message) \
    Monitor::Logging::Logger::instance()->log(Monitor::Logging::LogLevel::Error, category, message, __FILE__, __FUNCTION__, __LINE__)

#define LOG_CRITICAL_FL(category, message) \
    Monitor::Logging::Logger::instance()->log(Monitor::Logging::LogLevel::Critical, category, message, __FILE__, __FUNCTION__, __LINE__)