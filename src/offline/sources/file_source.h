#pragma once

#include "../../packet/sources/packet_source.h"
#include "../../concurrent/spsc_ring_buffer.h"
#include "../../logging/logger.h"

#include <QFile>
#include <QTimer>
#include <QString>
#include <QDateTime>
#include <memory>
#include <atomic>
#include <vector>

namespace Monitor {
namespace Offline {

/**
 * @brief Playback state enumeration
 */
enum class PlaybackState {
    Stopped,    ///< Playback stopped
    Playing,    ///< Playing packets
    Paused      ///< Playback paused
};

/**
 * @brief File source configuration
 */
struct FileSourceConfig {
    QString filename;                   ///< File to read from
    double playbackSpeed;              ///< Playback speed multiplier (0.1 - 10.0)
    bool loopPlayback;                 ///< Loop playback when reaching end
    bool realTimePlayback;             ///< Use original timing or play as fast as possible
    int bufferSize;                    ///< Internal packet buffer size
    
    FileSourceConfig()
        : playbackSpeed(1.0)
        , loopPlayback(false)
        , realTimePlayback(true)
        , bufferSize(1000)
    {}
};

/**
 * @brief File-based packet source for offline data playback
 * 
 * High-performance file source that reads packets from binary files
 * with support for playback controls including play/pause, seeking,
 * and variable speed playback. Supports both real-time playback
 * (matching original timing) and fast playback modes.
 * 
 * Key Features:
 * - Drag-and-drop file support
 * - Memory-mapped file access for large files
 * - Packet indexing for fast seeking
 * - Variable playback speed (0.1x - 10x)
 * - Step-by-step packet navigation
 * - Timeline scrubbing
 * - Loop playback support
 * - Progress tracking
 */
class FileSource : public Packet::PacketSource {
    Q_OBJECT
    
public:
    /**
     * @brief File format enumeration
     */
    enum class FileFormat {
        AutoDetect,     ///< Automatically detect format
        Binary,         ///< Raw binary packet data
        PCAP,           ///< PCAP capture format
        Custom          ///< Custom application format
    };
    
    /**
     * @brief File statistics
     */
    struct FileStatistics {
        QString filename;
        qint64 fileSize;
        uint64_t totalPackets;
        uint64_t currentPacket;
        QDateTime fileCreated;
        QDateTime playbackStarted;
        double playbackProgress;    // 0.0 - 1.0
        
        FileStatistics() 
            : fileSize(0)
            , totalPackets(0)
            , currentPacket(0)
            , playbackProgress(0.0)
        {}
    };
    
    /**
     * @brief Construct file source
     * @param config File source configuration
     * @param parent Qt parent object
     */
    explicit FileSource(const FileSourceConfig& config = FileSourceConfig(), QObject* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~FileSource() override;
    
    /**
     * @brief Load file for playback
     * @param filename File to load
     * @param format File format (AutoDetect to auto-detect)
     * @return True if file loaded successfully
     */
    bool loadFile(const QString& filename, FileFormat format = FileFormat::AutoDetect);
    
    /**
     * @brief Close currently loaded file
     */
    void closeFile();
    
    /**
     * @brief Check if file is loaded
     */
    bool isFileLoaded() const { return m_fileLoaded; }
    
    /**
     * @brief Get current playback state
     */
    PlaybackState getPlaybackState() const { return m_playbackState; }
    
    /**
     * @brief Get file statistics
     */
    const FileStatistics& getFileStatistics() const { return m_fileStats; }
    
    /**
     * @brief Get current file configuration
     */
    const FileSourceConfig& getFileConfig() const { return m_config; }
    
    /**
     * @brief Update file configuration
     */
    void setFileConfig(const FileSourceConfig& config);
    
    /**
     * @brief Get playback progress (0.0 - 1.0)
     */
    double getPlaybackProgress() const;
    
    /**
     * @brief Check if at end of file
     */
    bool isAtEndOfFile() const;
    
    /**
     * @brief Check if at beginning of file
     */
    bool isAtBeginningOfFile() const;

public slots:
    /**
     * @brief Start or resume playback
     */
    void play();
    
    /**
     * @brief Pause playback
     */
    void pausePlayback();
    
    /**
     * @brief Stop playback and return to beginning
     */
    void stopPlayback();
    
    /**
     * @brief Step forward one packet
     */
    void stepForward();
    
    /**
     * @brief Step backward one packet
     */
    void stepBackward();
    
    /**
     * @brief Seek to specific packet number
     * @param packetNumber Packet number (0-based)
     */
    void seekToPacket(uint64_t packetNumber);
    
    /**
     * @brief Seek to specific position (0.0 - 1.0)
     * @param position Position as percentage of file
     */
    void seekToPosition(double position);
    
    /**
     * @brief Seek to specific timestamp
     * @param timestamp Target timestamp
     */
    void seekToTimestamp(const QDateTime& timestamp);
    
    /**
     * @brief Set playback speed
     * @param speed Speed multiplier (0.1 - 10.0)
     */
    void setPlaybackSpeed(double speed);
    
    /**
     * @brief Toggle loop playback
     */
    void setLoopPlayback(bool loop);
    
    /**
     * @brief Set real-time playback mode
     */
    void setRealTimePlayback(bool realTime);

signals:
    /**
     * @brief Emitted when playback state changes
     */
    void playbackStateChanged(PlaybackState oldState, PlaybackState newState);
    
    /**
     * @brief Emitted when file is loaded
     */
    void fileLoaded(const QString& filename);
    
    /**
     * @brief Emitted when file is closed
     */
    void fileClosed();
    
    /**
     * @brief Emitted when seeking completes
     */
    void seekCompleted(uint64_t packetNumber);
    
    /**
     * @brief Emitted when playback reaches end
     */
    void endOfFileReached();
    
    /**
     * @brief Emitted when playback progress updates
     */
    void progressUpdated(double progress);
    
    /**
     * @brief Emitted when file statistics update
     */
    void fileStatisticsUpdated(const FileStatistics& stats);
    
    /**
     * @brief Emitted when playback speed changes
     */
    void playbackSpeedChanged(double speed);

protected:
    /**
     * @brief PacketSource interface implementation
     */
    bool doStart() override;
    void doStop() override;
    void doPause() override;
    bool doResume() override;

private slots:
    /**
     * @brief Handle playback timer
     */
    void onPlaybackTimer();
    
    /**
     * @brief Handle progress update timer
     */
    void onProgressTimer();

private:
    /**
     * @brief Initialize file reading
     */
    bool initializeFile();
    
    /**
     * @brief Detect file format
     */
    FileFormat detectFileFormat(const QString& filename) const;
    
    /**
     * @brief Index packets in file
     */
    bool indexPackets();
    
    /**
     * @brief Read next packet from file
     */
    bool readNextPacket();
    
    /**
     * @brief Read packet at specific position
     */
    Packet::PacketPtr readPacketAtPosition(qint64 position);
    
    /**
     * @brief Create packet from file data
     */
    Packet::PacketPtr createPacketFromData(const QByteArray& data);
    
    /**
     * @brief Calculate playback timing
     */
    int calculatePlaybackInterval() const;
    
    /**
     * @brief Update playback state
     */
    void setPlaybackState(PlaybackState newState);
    
    /**
     * @brief Update file statistics
     */
    void updateFileStatistics();
    
    /**
     * @brief Reset playback position
     */
    void resetPlaybackPosition();
    
    /**
     * @brief Handle end of file
     */
    void handleEndOfFile();
    
    /**
     * @brief Validate packet at current position
     */
    bool isValidPacketAtPosition(qint64 position) const;
    
    // File configuration and state
    FileSourceConfig m_config;
    QString m_currentFilename;
    FileFormat m_fileFormat;
    bool m_fileLoaded;
    PlaybackState m_playbackState;
    
    // File handling
    std::unique_ptr<QFile> m_file;
    qint64 m_fileSize;
    qint64 m_currentPosition;
    
    // Packet indexing
    struct PacketIndex {
        qint64 position;        ///< File position
        uint32_t size;          ///< Packet size
        uint64_t timestamp;     ///< Packet timestamp (if available)
        
        PacketIndex() : position(0), size(0), timestamp(0) {}
        PacketIndex(qint64 pos, uint32_t sz, uint64_t ts = 0) 
            : position(pos), size(sz), timestamp(ts) {}
    };
    
    std::vector<PacketIndex> m_packetIndex;
    uint64_t m_currentPacketIndex;
    bool m_indexBuilt;
    
    // Playback timing
    std::unique_ptr<QTimer> m_playbackTimer;
    std::unique_ptr<QTimer> m_progressTimer;
    QDateTime m_playbackStartTime;
    uint64_t m_packetsDelivered;
    
    // Statistics
    FileStatistics m_fileStats;
    
    // Performance settings
    static constexpr int MIN_PLAYBACK_INTERVAL = 1;      // 1ms minimum
    static constexpr int MAX_PLAYBACK_INTERVAL = 10000;  // 10 second maximum
    static constexpr int PROGRESS_UPDATE_INTERVAL = 100; // 100ms progress updates
    static constexpr int PACKET_BUFFER_SIZE = 1000;      // Packet buffer size
};

/**
 * @brief Convert playback state to string
 */
inline QString playbackStateToString(PlaybackState state) {
    switch (state) {
        case PlaybackState::Stopped: return "Stopped";
        case PlaybackState::Playing: return "Playing";
        case PlaybackState::Paused: return "Paused";
        default: return "Unknown";
    }
}

/**
 * @brief Convert file format to string
 */
inline QString fileFormatToString(FileSource::FileFormat format) {
    switch (format) {
        case FileSource::FileFormat::AutoDetect: return "Auto-Detect";
        case FileSource::FileFormat::Binary: return "Binary";
        case FileSource::FileFormat::PCAP: return "PCAP";
        case FileSource::FileFormat::Custom: return "Custom";
        default: return "Unknown";
    }
}

} // namespace Offline
} // namespace Monitor