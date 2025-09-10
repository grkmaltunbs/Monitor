#include "file_indexer.h"
#include "../../packet/core/packet_header.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <algorithm>

namespace Monitor {
namespace Offline {

FileIndexer::FileIndexer(QObject* parent)
    : QThread(parent)
    , m_status(IndexStatus::NotStarted)
    , m_cancelRequested(false)
    , m_lastProgressPercentage(-1)
    , m_logger(Logging::Logger::instance())
{
}

FileIndexer::~FileIndexer() {
    if (isRunning()) {
        cancelIndexing();
        wait(5000); // Wait up to 5 seconds for thread to finish
        if (isRunning()) {
            terminate();
            wait(1000);
        }
    }
}

bool FileIndexer::startIndexing(const QString& filename, bool background) {
    if (m_status == IndexStatus::InProgress) {
        m_logger->warning("FileIndexer", "Indexing already in progress");
        return false;
    }
    
    // Check if file exists
    if (!QFileInfo::exists(filename)) {
        m_logger->error("FileIndexer", QString("File does not exist: %1").arg(filename));
        return false;
    }
    
    m_filename = filename;
    m_cancelRequested = false;
    clearIndex();
    
    // Initialize statistics
    QFileInfo fileInfo(filename);
    m_statistics = IndexStatistics();
    m_statistics.filename = filename;
    m_statistics.fileSize = fileInfo.size();
    m_statistics.indexStartTime = QDateTime::currentDateTime();
    
    m_logger->info("FileIndexer", 
        QString("Starting indexing of file: %1 (%2 bytes)")
        .arg(filename).arg(m_statistics.fileSize));
    
    setStatus(IndexStatus::InProgress);
    emit indexingStarted(filename);
    
    if (background) {
        // Start background thread
        start();
        return true;
    } else {
        // Perform synchronous indexing
        bool success = performIndexing();
        
        if (success) {
            setStatus(IndexStatus::Completed);
            calculateStatistics();
            emit indexingCompleted(m_statistics);
        } else {
            setStatus(IndexStatus::Failed);
            emit indexingFailed("Synchronous indexing failed");
        }
        
        return success;
    }
}

void FileIndexer::cancelIndexing() {
    if (m_status == IndexStatus::InProgress) {
        m_logger->info("FileIndexer", "Cancelling indexing operation");
        m_cancelRequested = true;
    }
}

void FileIndexer::stopIndexing() {
    cancelIndexing();
    
    if (isRunning()) {
        wait(3000); // Wait up to 3 seconds
        if (isRunning()) {
            terminate();
            wait(1000);
        }
    }
}

void FileIndexer::run() {
    bool success = performIndexing();
    
    if (m_cancelRequested) {
        setStatus(IndexStatus::Cancelled);
        emit indexingCancelled();
    } else if (success) {
        setStatus(IndexStatus::Completed);
        calculateStatistics();
        emit indexingCompleted(m_statistics);
    } else {
        setStatus(IndexStatus::Failed);
        emit indexingFailed("Background indexing failed");
    }
}

bool FileIndexer::performIndexing() {
    QFile file(m_filename);
    if (!file.open(QIODevice::ReadOnly)) {
        m_logger->error("FileIndexer", 
            QString("Failed to open file: %1").arg(file.errorString()));
        return false;
    }
    
    qint64 fileSize = file.size();
    qint64 position = 0;
    uint64_t packetCount = 0;
    uint64_t errorCount = 0;
    
    QElapsedTimer timer;
    timer.start();
    
    m_logger->info("FileIndexer", 
        QString("Indexing file: %1 bytes").arg(fileSize));
    
    // Reserve space for index (estimate)
    uint64_t estimatedPackets = fileSize / 100; // Rough estimate
    m_index.reserve(estimatedPackets);
    
    while (position < fileSize && !m_cancelRequested) {
        PacketIndexEntry entry;
        
        if (readPacketAtPosition(file, position, entry)) {
            // Add valid packet to index
            {
                QMutexLocker locker(&m_indexMutex);
                m_index.push_back(entry);
            }
            
            // Move to next packet
            position += entry.packetSize;
            packetCount++;
            
            // Update statistics periodically
            if (packetCount % BATCH_SIZE == 0) {
                m_statistics.indexedPackets = packetCount;
                m_statistics.validPackets = packetCount;
                m_statistics.errorPackets = errorCount;
                
                updateProgress(position, fileSize);
                emit statisticsUpdated(m_statistics);
            }
            
        } else {
            // Invalid packet - try to find next valid packet
            errorCount++;
            
            // Skip ahead and look for next packet
            position = findNextValidPacket(file, position + 1, fileSize);
            if (position == -1) {
                break; // No more valid packets found
            }
        }
    }
    
    // Final statistics update
    m_statistics.indexedPackets = packetCount;
    m_statistics.validPackets = packetCount;
    m_statistics.errorPackets = errorCount;
    m_statistics.indexEndTime = QDateTime::currentDateTime();
    m_statistics.indexingTimeMs = timer.elapsed();
    
    file.close();
    
    if (m_cancelRequested) {
        m_logger->info("FileIndexer", "Indexing cancelled by request");
        return false;
    }
    
    m_logger->info("FileIndexer", 
        QString("Indexing completed: %1 packets, %2 errors, %3ms")
        .arg(packetCount).arg(errorCount).arg(m_statistics.indexingTimeMs));
    
    updateProgress(fileSize, fileSize); // 100% complete
    
    return true;
}

bool FileIndexer::readPacketAtPosition(QFile& file, qint64 position, PacketIndexEntry& entry) {
    // Seek to position
    if (!file.seek(position)) {
        return false;
    }
    
    // Read packet header
    QByteArray headerData = file.read(sizeof(Packet::PacketHeader));
    if (headerData.size() != sizeof(Packet::PacketHeader)) {
        return false;
    }
    
    // Validate header
    if (!validatePacketHeader(headerData)) {
        return false;
    }
    
    const Packet::PacketHeader* header = 
        reinterpret_cast<const Packet::PacketHeader*>(headerData.constData());
    
    // Calculate total packet size (header + payload)
    uint32_t totalSize = sizeof(Packet::PacketHeader) + header->payloadSize;
    
    // Check if complete packet is available
    if (position + totalSize > file.size()) {
        return false; // Truncated packet
    }
    
    // Fill index entry
    entry.filePosition = position;
    entry.packetSize = totalSize;
    entry.timestamp = header->timestamp;
    entry.packetId = header->id;
    entry.sequenceNumber = header->sequence;
    
    return true;
}

bool FileIndexer::validatePacketHeader(const QByteArray& headerData) const {
    if (headerData.size() != sizeof(Packet::PacketHeader)) {
        return false;
    }
    
    const Packet::PacketHeader* header = 
        reinterpret_cast<const Packet::PacketHeader*>(headerData.constData());
    
    // Calculate total packet size
    uint32_t totalSize = sizeof(Packet::PacketHeader) + header->payloadSize;
    
    // Validate size
    if (totalSize < MIN_PACKET_SIZE || totalSize > MAX_PACKET_SIZE) {
        return false;
    }
    
    // Use the isValid() method from PacketHeader
    return header->isValid();
}

qint64 FileIndexer::findNextValidPacket(QFile& file, qint64 startPosition, qint64 fileSize) {
    const int SEARCH_BUFFER_SIZE = 4096;
    QByteArray buffer;
    
    for (qint64 pos = startPosition; pos < fileSize - MIN_PACKET_SIZE; pos += SEARCH_BUFFER_SIZE / 2) {
        if (m_cancelRequested) {
            return -1;
        }
        
        // Read search buffer
        file.seek(pos);
        buffer = file.read(SEARCH_BUFFER_SIZE);
        
        // Look for valid packet headers
        for (size_t i = 0; i <= buffer.size() - sizeof(Packet::PacketHeader); ++i) {
            const Packet::PacketHeader* header = 
                reinterpret_cast<const Packet::PacketHeader*>(buffer.constData() + i);
            
            // Calculate total packet size
            uint32_t totalSize = sizeof(Packet::PacketHeader) + header->payloadSize;
            
            if (totalSize >= MIN_PACKET_SIZE &&
                totalSize <= MAX_PACKET_SIZE &&
                header->isValid()) {
                
                qint64 candidatePos = pos + i;
                
                // Verify this is actually a valid packet
                PacketIndexEntry testEntry;
                if (readPacketAtPosition(file, candidatePos, testEntry)) {
                    return candidatePos;
                }
            }
        }
    }
    
    return -1; // No valid packet found
}

int FileIndexer::findPacketByPosition(qint64 position) const {
    QMutexLocker locker(&m_indexMutex);
    
    auto it = std::lower_bound(m_index.begin(), m_index.end(), position,
        [](const PacketIndexEntry& entry, qint64 pos) {
            return entry.filePosition < pos;
        });
    
    if (it != m_index.end() && it->filePosition == position) {
        return std::distance(m_index.begin(), it);
    }
    
    return -1;
}

int FileIndexer::findPacketByTimestamp(uint64_t timestamp) const {
    QMutexLocker locker(&m_indexMutex);
    
    auto it = std::lower_bound(m_index.begin(), m_index.end(), timestamp,
        [](const PacketIndexEntry& entry, uint64_t ts) {
            return entry.timestamp < ts;
        });
    
    if (it != m_index.end()) {
        return std::distance(m_index.begin(), it);
    }
    
    return -1;
}

std::vector<int> FileIndexer::findPacketsByPacketId(uint32_t packetId) const {
    QMutexLocker locker(&m_indexMutex);
    
    std::vector<int> indices;
    
    for (size_t i = 0; i < m_index.size(); ++i) {
        if (m_index[i].packetId == packetId) {
            indices.push_back(static_cast<int>(i));
        }
    }
    
    return indices;
}

int FileIndexer::findPacketBySequence(uint32_t sequenceNumber) const {
    QMutexLocker locker(&m_indexMutex);
    
    auto it = std::find_if(m_index.begin(), m_index.end(),
        [sequenceNumber](const PacketIndexEntry& entry) {
            return entry.sequenceNumber == sequenceNumber;
        });
    
    if (it != m_index.end()) {
        return std::distance(m_index.begin(), it);
    }
    
    return -1;
}

const PacketIndexEntry* FileIndexer::getPacketEntry(int index) const {
    QMutexLocker locker(&m_indexMutex);
    
    if (index >= 0 && index < static_cast<int>(m_index.size())) {
        return &m_index[index];
    }
    
    return nullptr;
}

bool FileIndexer::saveIndexToCache(const QString& cacheFilename) const {
    QMutexLocker locker(&m_indexMutex);
    
    QJsonObject cacheObject;
    cacheObject["version"] = 1;
    cacheObject["filename"] = m_statistics.filename;
    cacheObject["fileSize"] = m_statistics.fileSize;
    cacheObject["totalPackets"] = static_cast<qint64>(m_statistics.totalPackets);
    cacheObject["indexedPackets"] = static_cast<qint64>(m_statistics.indexedPackets);
    cacheObject["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Create checksum of original file
    QFile originalFile(m_statistics.filename);
    if (originalFile.open(QIODevice::ReadOnly)) {
        QCryptographicHash hash(QCryptographicHash::Md5);
        hash.addData(originalFile.readAll());
        cacheObject["fileChecksum"] = QString(hash.result().toHex());
        originalFile.close();
    }
    
    // Save index entries (sample for large indices)
    QJsonArray indexArray;
    size_t maxEntries = 10000; // Limit cache size
    size_t step = std::max(static_cast<size_t>(1), m_index.size() / maxEntries);
    
    for (size_t i = 0; i < m_index.size(); i += step) {
        const auto& entry = m_index[i];
        QJsonObject entryObject;
        entryObject["position"] = entry.filePosition;
        entryObject["size"] = static_cast<int>(entry.packetSize);
        entryObject["timestamp"] = static_cast<qint64>(entry.timestamp);
        entryObject["packetId"] = static_cast<int>(entry.packetId);
        entryObject["sequence"] = static_cast<int>(entry.sequenceNumber);
        indexArray.append(entryObject);
    }
    cacheObject["index"] = indexArray;
    
    // Write cache file
    QFile cacheFile(cacheFilename);
    if (!cacheFile.open(QIODevice::WriteOnly)) {
        m_logger->error("FileIndexer", 
            QString("Failed to create cache file: %1").arg(cacheFile.errorString()));
        return false;
    }
    
    QJsonDocument doc(cacheObject);
    qint64 bytesWritten = cacheFile.write(doc.toJson(QJsonDocument::Compact));
    cacheFile.close();
    
    bool success = (bytesWritten > 0);
    
    if (success) {
        m_logger->info("FileIndexer", 
            QString("Index cache saved: %1").arg(cacheFilename));
    } else {
        m_logger->error("FileIndexer", 
            QString("Failed to write index cache: %1").arg(cacheFilename));
    }
    
    return success;
}

bool FileIndexer::loadIndexFromCache(const QString& cacheFilename) {
    QFile cacheFile(cacheFilename);
    if (!cacheFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(cacheFile.readAll(), &parseError);
    cacheFile.close();
    
    if (parseError.error != QJsonParseError::NoError) {
        m_logger->warning("FileIndexer", 
            QString("Cache file parse error: %1").arg(parseError.errorString()));
        return false;
    }
    
    QJsonObject cacheObject = doc.object();
    
    // Verify cache validity
    QString cachedFilename = cacheObject["filename"].toString();
    qint64 cachedFileSize = cacheObject["fileSize"].toVariant().toLongLong();
    
    if (cachedFilename != m_statistics.filename || 
        cachedFileSize != m_statistics.fileSize) {
        m_logger->warning("FileIndexer", "Cache file mismatch");
        return false;
    }
    
    // Load index entries
    QJsonArray indexArray = cacheObject["index"].toArray();
    
    QMutexLocker locker(&m_indexMutex);
    m_index.clear();
    m_index.reserve(indexArray.size());
    
    for (const auto& value : indexArray) {
        QJsonObject entryObject = value.toObject();
        PacketIndexEntry entry;
        entry.filePosition = entryObject["position"].toVariant().toLongLong();
        entry.packetSize = static_cast<uint32_t>(entryObject["size"].toInt());
        entry.timestamp = static_cast<uint64_t>(entryObject["timestamp"].toVariant().toLongLong());
        entry.packetId = static_cast<uint32_t>(entryObject["packetId"].toInt());
        entry.sequenceNumber = static_cast<uint32_t>(entryObject["sequence"].toInt());
        m_index.push_back(entry);
    }
    
    // Update statistics
    m_statistics.totalPackets = m_index.size();
    m_statistics.indexedPackets = m_index.size();
    m_statistics.validPackets = m_index.size();
    
    setStatus(IndexStatus::Completed);
    
    m_logger->info("FileIndexer", 
        QString("Index cache loaded: %1 packets").arg(m_index.size()));
    
    return true;
}

QString FileIndexer::getCacheFilename(const QString& dataFilename) {
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cacheDir);
    
    QFileInfo fileInfo(dataFilename);
    QString baseName = fileInfo.completeBaseName();
    QString hash = QCryptographicHash::hash(dataFilename.toUtf8(), QCryptographicHash::Md5).toHex();
    
    return QString("%1/%2_%3.idx").arg(cacheDir).arg(baseName).arg(QString(hash).left(8));
}

bool FileIndexer::isCacheValid(const QString& dataFilename) {
    QString cacheFilename = getCacheFilename(dataFilename);
    
    if (!QFileInfo::exists(cacheFilename)) {
        return false;
    }
    
    QFileInfo dataInfo(dataFilename);
    QFileInfo cacheInfo(cacheFilename);
    
    // Check if cache is newer than data file
    return cacheInfo.lastModified() >= dataInfo.lastModified();
}

void FileIndexer::setStatus(IndexStatus newStatus) {
    IndexStatus oldStatus = m_status;
    m_status = newStatus;
    
    if (oldStatus != newStatus) {
        emit statusChanged(oldStatus, newStatus);
    }
}

void FileIndexer::updateProgress(qint64 bytesProcessed, qint64 totalBytes) {
    int percentage = (totalBytes > 0) ? 
        static_cast<int>((bytesProcessed * 100) / totalBytes) : 0;
    
    percentage = std::clamp(percentage, 0, 100);
    
    // Only emit progress if it has changed and enough time has passed
    QDateTime now = QDateTime::currentDateTime();
    if (percentage != m_lastProgressPercentage && 
        (m_lastProgressUpdate.isNull() || 
         m_lastProgressUpdate.msecsTo(now) >= PROGRESS_UPDATE_INTERVAL)) {
        
        m_lastProgressPercentage = percentage;
        m_lastProgressUpdate = now;
        
        emit progressChanged(percentage);
    }
}

void FileIndexer::calculateStatistics() {
    if (m_statistics.indexingTimeMs > 0) {
        m_statistics.packetsPerSecond = 
            (static_cast<double>(m_statistics.indexedPackets) * 1000.0) / 
            m_statistics.indexingTimeMs;
    }
    
    m_statistics.totalPackets = m_statistics.indexedPackets;
}

void FileIndexer::clearIndex() {
    QMutexLocker locker(&m_indexMutex);
    m_index.clear();
    m_lastProgressPercentage = -1;
}

} // namespace Offline
} // namespace Monitor