#pragma once

#include "../../logging/logger.h"
#include <QString>
#include <QDateTime>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <vector>
#include <memory>
#include <atomic>

namespace Monitor {
namespace Offline {

/**
 * @brief Packet index entry for fast file seeking
 */
struct PacketIndexEntry {
    qint64 filePosition;        ///< Position in file (bytes)
    uint32_t packetSize;        ///< Size of packet (bytes)
    uint64_t timestamp;         ///< Packet timestamp (microseconds since epoch)
    uint32_t packetId;          ///< Packet ID from header
    uint32_t sequenceNumber;    ///< Packet sequence number
    
    PacketIndexEntry() 
        : filePosition(0), packetSize(0), timestamp(0), packetId(0), sequenceNumber(0) {}
    
    PacketIndexEntry(qint64 pos, uint32_t size, uint64_t ts, uint32_t id, uint32_t seq)
        : filePosition(pos), packetSize(size), timestamp(ts), packetId(id), sequenceNumber(seq) {}
};

/**
 * @brief Index statistics and metadata
 */
struct IndexStatistics {
    QString filename;
    qint64 fileSize;
    uint64_t totalPackets;
    uint64_t indexedPackets;
    uint64_t validPackets;
    uint64_t errorPackets;
    QDateTime indexStartTime;
    QDateTime indexEndTime;
    int indexingTimeMs;
    double packetsPerSecond;
    
    IndexStatistics()
        : fileSize(0), totalPackets(0), indexedPackets(0)
        , validPackets(0), errorPackets(0), indexingTimeMs(0), packetsPerSecond(0.0) {}
};

/**
 * @brief High-performance file indexer for packet files
 * 
 * Builds comprehensive index of packet files for fast seeking and navigation.
 * Supports background indexing with progress reporting and can handle very
 * large files efficiently using memory-mapped access and streaming.
 * 
 * Key Features:
 * - Background indexing in separate thread
 * - Memory-efficient streaming indexer
 * - Progress reporting during indexing
 * - Index caching and persistence
 * - Fast binary search for seeking
 * - Timestamp-based navigation
 * - Packet ID and sequence filtering
 * - Error recovery and validation
 */
class FileIndexer : public QThread {
    Q_OBJECT
    
public:
    /**
     * @brief Indexing status enumeration
     */
    enum class IndexStatus {
        NotStarted,     ///< Indexing not started
        InProgress,     ///< Currently indexing
        Completed,      ///< Indexing completed successfully
        Failed,         ///< Indexing failed
        Cancelled       ///< Indexing was cancelled
    };
    
    /**
     * @brief Construct file indexer
     * @param parent Parent object
     */
    explicit FileIndexer(QObject* parent = nullptr);
    
    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~FileIndexer() override;
    
    /**
     * @brief Start indexing file
     * @param filename File to index
     * @param background True to index in background thread
     * @return True if indexing started successfully
     */
    bool startIndexing(const QString& filename, bool background = true);
    
    /**
     * @brief Cancel ongoing indexing operation
     */
    void cancelIndexing();
    
    /**
     * @brief Check if indexing is complete
     */
    bool isIndexingComplete() const { return m_status == IndexStatus::Completed; }
    
    /**
     * @brief Get current indexing status
     */
    IndexStatus getStatus() const { return m_status; }
    
    /**
     * @brief Get indexing statistics
     */
    const IndexStatistics& getStatistics() const { return m_statistics; }
    
    /**
     * @brief Get packet index entries
     */
    const std::vector<PacketIndexEntry>& getIndex() const { return m_index; }
    
    /**
     * @brief Find packet by position in file
     * @param position File position
     * @return Index of packet at position, or -1 if not found
     */
    int findPacketByPosition(qint64 position) const;
    
    /**
     * @brief Find packet by timestamp
     * @param timestamp Target timestamp
     * @return Index of packet at or after timestamp, or -1 if not found
     */
    int findPacketByTimestamp(uint64_t timestamp) const;
    
    /**
     * @brief Find packets by packet ID
     * @param packetId Target packet ID
     * @return Vector of indices for packets with matching ID
     */
    std::vector<int> findPacketsByPacketId(uint32_t packetId) const;
    
    /**
     * @brief Find packet by sequence number
     * @param sequenceNumber Target sequence number
     * @return Index of packet with matching sequence, or -1 if not found
     */
    int findPacketBySequence(uint32_t sequenceNumber) const;
    
    /**
     * @brief Get packet entry by index
     * @param index Packet index
     * @return Packet entry, or nullptr if index invalid
     */
    const PacketIndexEntry* getPacketEntry(int index) const;
    
    /**
     * @brief Get total number of packets
     */
    uint64_t getPacketCount() const { return m_index.size(); }
    
    /**
     * @brief Save index to cache file
     * @param cacheFilename Cache file path
     * @return True if saved successfully
     */
    bool saveIndexToCache(const QString& cacheFilename) const;
    
    /**
     * @brief Load index from cache file
     * @param cacheFilename Cache file path
     * @return True if loaded successfully
     */
    bool loadIndexFromCache(const QString& cacheFilename);
    
    /**
     * @brief Get recommended cache filename for a data file
     * @param dataFilename Original data file path
     * @return Recommended cache file path
     */
    static QString getCacheFilename(const QString& dataFilename);
    
    /**
     * @brief Check if cache file exists and is valid
     * @param dataFilename Original data file path
     * @return True if valid cache exists
     */
    static bool isCacheValid(const QString& dataFilename);

public slots:
    /**
     * @brief Force stop indexing and cleanup
     */
    void stopIndexing();

signals:
    /**
     * @brief Emitted when indexing starts
     */
    void indexingStarted(const QString& filename);
    
    /**
     * @brief Emitted with indexing progress (0-100)
     */
    void progressChanged(int percentage);
    
    /**
     * @brief Emitted when indexing completes successfully
     */
    void indexingCompleted(const IndexStatistics& stats);
    
    /**
     * @brief Emitted when indexing fails
     */
    void indexingFailed(const QString& error);
    
    /**
     * @brief Emitted when indexing is cancelled
     */
    void indexingCancelled();
    
    /**
     * @brief Emitted with status updates during indexing
     */
    void statusChanged(IndexStatus oldStatus, IndexStatus newStatus);
    
    /**
     * @brief Emitted periodically with current statistics
     */
    void statisticsUpdated(const IndexStatistics& stats);

protected:
    /**
     * @brief Thread execution function
     */
    void run() override;

private:
    /**
     * @brief Perform the actual indexing
     */
    bool performIndexing();
    
    /**
     * @brief Read and validate packet at position
     * @param file File handle
     * @param position File position
     * @param entry Output packet entry
     * @return True if packet is valid
     */
    bool readPacketAtPosition(QFile& file, qint64 position, PacketIndexEntry& entry);
    
    /**
     * @brief Validate packet header
     * @param headerData Header bytes
     * @return True if header is valid
     */
    bool validatePacketHeader(const QByteArray& headerData) const;
    
    /**
     * @brief Set indexing status
     */
    void setStatus(IndexStatus newStatus);
    
    /**
     * @brief Update progress
     */
    void updateProgress(qint64 bytesProcessed, qint64 totalBytes);
    
    /**
     * @brief Calculate indexing statistics
     */
    void calculateStatistics();
    
    /**
     * @brief Clear all index data
     */
    void clearIndex();
    
    /**
     * @brief Find next valid packet starting from position
     * @param file File handle
     * @param startPosition Starting search position
     * @param fileSize Total file size
     * @return Position of next valid packet, or -1 if none found
     */
    qint64 findNextValidPacket(QFile& file, qint64 startPosition, qint64 fileSize);
    
    // File and status
    QString m_filename;
    IndexStatus m_status;
    std::atomic<bool> m_cancelRequested;
    
    // Index data
    std::vector<PacketIndexEntry> m_index;
    mutable QMutex m_indexMutex;
    
    // Statistics
    IndexStatistics m_statistics;
    
    // Progress tracking
    int m_lastProgressPercentage;
    QDateTime m_lastProgressUpdate;
    
    // Logging
    Logging::Logger* m_logger;
    
    // Performance settings
    static constexpr int PROGRESS_UPDATE_INTERVAL = 100;    // ms
    static constexpr int MAX_PACKET_SIZE = 65536;           // 64KB
    static constexpr int MIN_PACKET_SIZE = 24;              // Minimum header size
    static constexpr int BATCH_SIZE = 1000;                 // Packets per batch
};

/**
 * @brief Convert index status to string
 */
inline QString indexStatusToString(FileIndexer::IndexStatus status) {
    switch (status) {
        case FileIndexer::IndexStatus::NotStarted: return "Not Started";
        case FileIndexer::IndexStatus::InProgress: return "In Progress";
        case FileIndexer::IndexStatus::Completed: return "Completed";
        case FileIndexer::IndexStatus::Failed: return "Failed";
        case FileIndexer::IndexStatus::Cancelled: return "Cancelled";
        default: return "Unknown";
    }
}

} // namespace Offline
} // namespace Monitor