#include "logger.h"
#include <QtCore/QThread>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <iostream>

namespace Monitor {
namespace Logging {

LogEntry::LogEntry(LogLevel lvl, const QString& cat, const QString& msg, 
                   const QString& f, const QString& func, int ln)
    : timestamp(QDateTime::currentDateTime())
    , level(lvl)
    , category(cat)
    , message(msg)
    , file(f)
    , function(func)
    , line(ln)
    , threadId(reinterpret_cast<qint64>(QThread::currentThreadId()))
{
}

QString LogEntry::toString(const QString& format) const
{
    if (format.isEmpty()) {
        return QString("[%1] [%2] [%3] %4")
               .arg(timestamp.toString(Qt::ISODateWithMs))
               .arg(static_cast<int>(level))
               .arg(category)
               .arg(message);
    }
    
    QString result = format;
    result.replace("{timestamp}", timestamp.toString(Qt::ISODateWithMs));
    result.replace("{level}", QString::number(static_cast<int>(level)));
    result.replace("{category}", category);
    result.replace("{message}", message);
    result.replace("{file}", file);
    result.replace("{function}", function);
    result.replace("{line}", QString::number(line));
    result.replace("{thread}", QString::number(threadId));
    
    return result;
}

QByteArray LogEntry::toJson() const
{
    QJsonObject obj;
    obj["timestamp"] = timestamp.toString(Qt::ISODateWithMs);
    obj["level"] = static_cast<int>(level);
    obj["category"] = category;
    obj["message"] = message;
    
    if (!file.isEmpty()) obj["file"] = file;
    if (!function.isEmpty()) obj["function"] = function;
    if (line > 0) obj["line"] = line;
    if (threadId != 0) obj["thread"] = threadId;
    
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

ConsoleSink::ConsoleSink(QObject* parent)
    : LogSink(parent)
    , m_useColors(true)
    , m_stdout(stdout)
    , m_stderr(stderr)
{
}

void ConsoleSink::write(const LogEntry& entry)
{
    if (!shouldLog(entry.level)) {
        return;
    }
    
    QMutexLocker locker(&m_writeMutex);
    
    QString formatted = QString("[%1] %2[%3]%4 [%5] %6")
                       .arg(entry.timestamp.toString(Qt::ISODateWithMs))
                       .arg(m_useColors ? colorizeLevel(entry.level) : QString())
                       .arg(static_cast<int>(entry.level))
                       .arg(m_useColors ? "\033[0m" : QString())
                       .arg(entry.category)
                       .arg(entry.message);
    
    if (entry.level >= LogLevel::Error) {
        m_stderr << formatted << Qt::endl;
    } else {
        m_stdout << formatted << Qt::endl;
    }
}

void ConsoleSink::flush()
{
    QMutexLocker locker(&m_writeMutex);
    m_stdout.flush();
    m_stderr.flush();
}

QString ConsoleSink::colorizeLevel(LogLevel level) const
{
    switch (level) {
        case LogLevel::Trace:    return "\033[37m";    // White
        case LogLevel::Debug:    return "\033[36m";    // Cyan
        case LogLevel::Info:     return "\033[32m";    // Green
        case LogLevel::Warning:  return "\033[33m";    // Yellow
        case LogLevel::Error:    return "\033[31m";    // Red
        case LogLevel::Critical: return "\033[35m";    // Magenta
        default:                 return "\033[0m";     // Reset
    }
}

FileSink::FileSink(const QString& filePath, QObject* parent)
    : LogSink(parent)
    , m_filePath(filePath)
    , m_maxFileSize(100 * 1024 * 1024) // 100MB default
    , m_maxFiles(10)
    , m_autoFlush(true)
{
    QDir dir = QFileInfo(m_filePath).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    m_file = std::make_unique<QFile>(m_filePath);
    if (m_file->open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_stream = std::make_unique<QTextStream>(m_file.get());
        m_stream->setEncoding(QStringConverter::Utf8);
    } else {
        emit writeError(QString("Failed to open log file: %1").arg(m_filePath));
    }
}

FileSink::~FileSink()
{
    if (m_stream) {
        m_stream->flush();
    }
    if (m_file && m_file->isOpen()) {
        m_file->close();
    }
}

void FileSink::write(const LogEntry& entry)
{
    if (!shouldLog(entry.level) || !m_stream) {
        return;
    }
    
    QMutexLocker locker(&m_writeMutex);
    
    QString formatted = QString("[%1] [%2] [%3] %4")
                       .arg(entry.timestamp.toString(Qt::ISODateWithMs))
                       .arg(static_cast<int>(entry.level))
                       .arg(entry.category)
                       .arg(entry.message);
    
    *m_stream << formatted << Qt::endl;
    
    if (m_autoFlush) {
        m_stream->flush();
    }
    
    if (m_file->size() >= m_maxFileSize) {
        rotateFile();
    }
}

void FileSink::flush()
{
    QMutexLocker locker(&m_writeMutex);
    if (m_stream) {
        m_stream->flush();
    }
}

qint64 FileSink::getCurrentFileSize() const
{
    QMutexLocker locker(&m_writeMutex);
    return m_file ? m_file->size() : 0;
}

void FileSink::rotateFile()
{
    if (!m_file || !m_stream) {
        return;
    }
    
    m_stream.reset();
    m_file->close();
    
    for (int i = m_maxFiles - 1; i >= 1; --i) {
        QString oldName = getRotatedFileName(i - 1);
        QString newName = getRotatedFileName(i);
        
        if (QFile::exists(oldName)) {
            if (QFile::exists(newName)) {
                QFile::remove(newName);
            }
            QFile::rename(oldName, newName);
        }
    }
    
    QString rotatedName = getRotatedFileName(0);
    if (QFile::exists(m_filePath)) {
        QFile::rename(m_filePath, rotatedName);
        emit fileRotated(m_filePath, rotatedName);
    }
    
    m_file = std::make_unique<QFile>(m_filePath);
    if (m_file->open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_stream = std::make_unique<QTextStream>(m_file.get());
        m_stream->setEncoding(QStringConverter::Utf8);
    } else {
        emit writeError(QString("Failed to reopen log file after rotation: %1").arg(m_filePath));
    }
}

QString FileSink::getRotatedFileName(int index) const
{
    QFileInfo info(m_filePath);
    return QString("%1/%2.%3.%4")
           .arg(info.path())
           .arg(info.baseName())
           .arg(index)
           .arg(info.suffix());
}

MemorySink::MemorySink(size_t maxEntries, QObject* parent)
    : LogSink(parent)
    , m_maxEntries(maxEntries)
{
}

void MemorySink::write(const LogEntry& entry)
{
    if (!shouldLog(entry.level)) {
        return;
    }
    
    QMutexLocker locker(&m_entriesMutex);
    
    if (m_entries.size() >= m_maxEntries) {
        m_entries.pop();
        emit bufferFull();
    }
    
    m_entries.push(entry);
}

void MemorySink::flush()
{
    // Memory sink doesn't need flushing
}

std::vector<LogEntry> MemorySink::getEntries() const
{
    QMutexLocker locker(&m_entriesMutex);
    
    std::vector<LogEntry> result;
    std::queue<LogEntry> temp = m_entries;
    
    while (!temp.empty()) {
        result.push_back(temp.front());
        temp.pop();
    }
    
    return result;
}

std::vector<LogEntry> MemorySink::getEntriesByLevel(LogLevel level) const
{
    std::vector<LogEntry> allEntries = getEntries();
    std::vector<LogEntry> result;
    
    std::copy_if(allEntries.begin(), allEntries.end(), std::back_inserter(result),
                 [level](const LogEntry& entry) { return entry.level == level; });
    
    return result;
}

std::vector<LogEntry> MemorySink::getEntriesByCategory(const QString& category) const
{
    std::vector<LogEntry> allEntries = getEntries();
    std::vector<LogEntry> result;
    
    std::copy_if(allEntries.begin(), allEntries.end(), std::back_inserter(result),
                 [&category](const LogEntry& entry) { return entry.category == category; });
    
    return result;
}

std::vector<LogEntry> MemorySink::getEntriesInRange(const QDateTime& start, const QDateTime& end) const
{
    std::vector<LogEntry> allEntries = getEntries();
    std::vector<LogEntry> result;
    
    std::copy_if(allEntries.begin(), allEntries.end(), std::back_inserter(result),
                 [&start, &end](const LogEntry& entry) { 
                     return entry.timestamp >= start && entry.timestamp <= end; 
                 });
    
    return result;
}

void MemorySink::clear()
{
    QMutexLocker locker(&m_entriesMutex);
    std::queue<LogEntry> empty;
    m_entries.swap(empty);
}

size_t MemorySink::getEntryCount() const
{
    QMutexLocker locker(&m_entriesMutex);
    return m_entries.size();
}

Logger* Logger::s_instance = nullptr;
QMutex Logger::s_instanceMutex;

Logger::Logger(QObject* parent)
    : QObject(parent)
    , m_asyncTimer(new QTimer(this))
    , m_globalLevel(LogLevel::Info)
    , m_isAsynchronous(true)
    , m_maxAsyncQueueSize(DEFAULT_MAX_ASYNC_QUEUE_SIZE)
    , m_loggedCount(0)
    , m_droppedCount(0)
{
    m_asyncTimer->setSingleShot(false);
    m_asyncTimer->setInterval(ASYNC_TIMER_INTERVAL_MS);
    connect(m_asyncTimer, &QTimer::timeout, this, &Logger::processAsyncLogs);
    
    if (m_isAsynchronous) {
        m_asyncTimer->start();
    }
}

Logger::~Logger()
{
    flushAndWait();
}

Logger* Logger::instance()
{
    if (!s_instance) {
        QMutexLocker locker(&s_instanceMutex);
        if (!s_instance) {
            s_instance = new Logger(QCoreApplication::instance());
            
            // Set up default console sink
            auto consoleSink = std::make_shared<ConsoleSink>();
            s_instance->addSink(consoleSink);
        }
    }
    return s_instance;
}

void Logger::addSink(std::shared_ptr<LogSink> sink)
{
    if (!sink) {
        return;
    }
    
    QMutexLocker locker(&m_sinksMutex);
    m_sinks.push_back(sink);
}

void Logger::removeSink(LogSink* sink)
{
    if (!sink) {
        return;
    }
    
    QMutexLocker locker(&m_sinksMutex);
    m_sinks.erase(std::remove_if(m_sinks.begin(), m_sinks.end(),
                                [sink](const std::weak_ptr<LogSink>& weak) {
                                    auto shared = weak.lock();
                                    return !shared || shared.get() == sink;
                                }), m_sinks.end());
}

void Logger::clearSinks()
{
    QMutexLocker locker(&m_sinksMutex);
    m_sinks.clear();
}

void Logger::log(LogLevel level, const QString& category, const QString& message,
                 const QString& file, const QString& function, int line)
{
    if (!shouldLog(level, category)) {
        return;
    }
    
    LogEntry entry(level, category, message, file, function, line);
    
    if (m_isAsynchronous) {
        logAsync(entry);
    } else {
        logSync(entry);
    }
}

void Logger::trace(const QString& category, const QString& message)
{
    log(LogLevel::Trace, category, message);
}

void Logger::debug(const QString& category, const QString& message)
{
    log(LogLevel::Debug, category, message);
}

void Logger::info(const QString& category, const QString& message)
{
    log(LogLevel::Info, category, message);
}

void Logger::warning(const QString& category, const QString& message)
{
    log(LogLevel::Warning, category, message);
}

void Logger::error(const QString& category, const QString& message)
{
    log(LogLevel::Error, category, message);
}

void Logger::critical(const QString& category, const QString& message)
{
    log(LogLevel::Critical, category, message);
}

void Logger::setGlobalLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_configMutex);
    m_globalLevel = level;
}

void Logger::setCategoryLevel(const QString& category, LogLevel level)
{
    QMutexLocker locker(&m_configMutex);
    m_categoryLevels[category] = level;
}

void Logger::removeCategoryLevel(const QString& category)
{
    QMutexLocker locker(&m_configMutex);
    m_categoryLevels.remove(category);
}

LogLevel Logger::getCategoryLevel(const QString& category) const
{
    QMutexLocker locker(&m_configMutex);
    return m_categoryLevels.value(category, m_globalLevel);
}

void Logger::setAsynchronous(bool async)
{
    if (m_isAsynchronous == async) {
        return;
    }
    
    if (!async) {
        flushAndWait();
    }
    
    m_isAsynchronous = async;
    
    if (async) {
        m_asyncTimer->start();
    } else {
        m_asyncTimer->stop();
    }
}

void Logger::flush()
{
    QMutexLocker sinkLocker(&m_sinksMutex);
    for (auto& sink : m_sinks) {
        sink->flush();
    }
}

void Logger::flushAndWait()
{
    if (m_isAsynchronous) {
        processAsyncLogs();
    }
    flush();
}

void Logger::logSync(const LogEntry& entry)
{
    emit logEntryCreated(entry);
    
    QMutexLocker locker(&m_sinksMutex);
    for (auto& sink : m_sinks) {
        sink->write(entry);
    }
    
    m_loggedCount.fetchAndAddRelaxed(1);
}

void Logger::logAsync(const LogEntry& entry)
{
    QMutexLocker locker(&m_asyncMutex);
    
    if (m_asyncQueue.size() >= m_maxAsyncQueueSize) {
        m_droppedCount.fetchAndAddRelaxed(1);
        emit queueFull(m_asyncQueue.size());
        return;
    }
    
    m_asyncQueue.push(entry);
}

bool Logger::shouldLog(LogLevel level, const QString& category) const
{
    LogLevel minLevel = getCategoryLevel(category);
    return level >= minLevel;
}

void Logger::processAsyncLogs()
{
    std::queue<LogEntry> toProcess;
    
    {
        QMutexLocker locker(&m_asyncMutex);
        toProcess.swap(m_asyncQueue);
    }
    
    while (!toProcess.empty()) {
        logSync(toProcess.front());
        toProcess.pop();
    }
}

} // namespace Logging
} // namespace Monitor

