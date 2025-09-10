#include "file_source.h"
#include "../../packet/core/packet_header.h"
#include <QFileInfo>
#include <QDateTime>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace Monitor {
namespace Offline {

FileSource::FileSource(const FileSourceConfig& config, QObject* parent)
    : PacketSource(Configuration("FileSource"), parent)
    , m_config(config)
    , m_fileFormat(FileFormat::AutoDetect)
    , m_fileLoaded(false)
    , m_playbackState(PlaybackState::Stopped)
    , m_file(nullptr)
    , m_fileSize(0)
    , m_currentPosition(0)
    , m_currentPacketIndex(0)
    , m_indexBuilt(false)
    , m_packetsDelivered(0)
{
    // Setup timers
    m_playbackTimer = std::make_unique<QTimer>(this);
    m_playbackTimer->setSingleShot(false);
    connect(m_playbackTimer.get(), &QTimer::timeout, this, &FileSource::onPlaybackTimer);
    
    m_progressTimer = std::make_unique<QTimer>(this);
    m_progressTimer->setInterval(PROGRESS_UPDATE_INTERVAL);
    connect(m_progressTimer.get(), &QTimer::timeout, this, &FileSource::onProgressTimer);
    
    // Load file if specified in config
    if (!m_config.filename.isEmpty()) {
        loadFile(m_config.filename);
    }
}

FileSource::~FileSource() {
    closeFile();
}

bool FileSource::loadFile(const QString& filename, FileFormat format) {
    // Close any currently loaded file
    closeFile();
    
    m_logger->info("FileSource", QString("Loading file: %1").arg(filename));
    
    // Check if file exists
    if (!QFileInfo::exists(filename)) {
        m_logger->error("FileSource", QString("File does not exist: %1").arg(filename));
        return false;
    }
    
    // Create file object
    m_file = std::make_unique<QFile>(filename);
    if (!m_file->open(QIODevice::ReadOnly)) {
        m_logger->error("FileSource", 
            QString("Failed to open file: %1 - %2").arg(filename).arg(m_file->errorString()));
        m_file.reset();
        return false;
    }
    
    // Store file information
    m_currentFilename = filename;
    m_fileSize = m_file->size();
    
    // Detect file format if needed
    m_fileFormat = (format == FileFormat::AutoDetect) ? detectFileFormat(filename) : format;
    
    // Initialize file reading
    if (!initializeFile()) {
        closeFile();
        return false;
    }
    
    // Build packet index
    if (!indexPackets()) {
        m_logger->warning("FileSource", "Failed to build packet index, seeking will be limited");
    }
    
    // Update file statistics
    QFileInfo fileInfo(filename);
    m_fileStats.filename = filename;
    m_fileStats.fileSize = m_fileSize;
    m_fileStats.fileCreated = fileInfo.birthTime();
    m_fileStats.totalPackets = m_packetIndex.size();
    m_fileStats.currentPacket = 0;
    m_fileStats.playbackProgress = 0.0;
    
    m_fileLoaded = true;
    resetPlaybackPosition();
    
    m_logger->info("FileSource", 
        QString("File loaded successfully: %1 packets, %2 bytes")
        .arg(m_fileStats.totalPackets).arg(m_fileSize));
    
    emit fileLoaded(filename);
    emit fileStatisticsUpdated(m_fileStats);
    
    return true;
}

void FileSource::closeFile() {
    if (!m_fileLoaded) {
        return;
    }
    
    // Stop playback
    stopPlayback();
    
    // Close file
    if (m_file) {
        m_file->close();
        m_file.reset();
    }
    
    // Clear state
    m_fileLoaded = false;
    m_currentFilename.clear();
    m_fileSize = 0;
    m_currentPosition = 0;
    m_currentPacketIndex = 0;
    m_indexBuilt = false;
    m_packetIndex.clear();
    m_packetsDelivered = 0;
    
    // Reset statistics
    m_fileStats = FileStatistics();
    
    m_logger->info("FileSource", "File closed");
    emit fileClosed();
}

bool FileSource::doStart() {
    if (!m_fileLoaded) {
        reportError("No file loaded for playback");
        return false;
    }
    
    m_logger->info("FileSource", "Starting file source");
    
    // Reset statistics
    m_stats.startTime = std::chrono::steady_clock::now();
    m_packetsDelivered = 0;
    
    // Start progress timer
    m_progressTimer->start();
    
    // If not already playing, start playback
    if (m_playbackState == PlaybackState::Stopped) {
        play();
    }
    
    return true;
}

void FileSource::doStop() {
    m_logger->info("FileSource", "Stopping file source");
    
    stopPlayback();
    
    // Stop progress timer
    if (m_progressTimer) {
        m_progressTimer->stop();
    }
}

void FileSource::doPause() {
    pausePlayback();
}

bool FileSource::doResume() {
    play();
    return true;
}

FileSource::FileFormat FileSource::detectFileFormat(const QString& filename) const {
    QMimeDatabase db;
    QMimeType mimeType = db.mimeTypeForFile(filename);
    
    QString mimeTypeName = mimeType.name();
    
    if (mimeTypeName == "application/vnd.tcpdump.pcap" || filename.endsWith(".pcap")) {
        return FileFormat::PCAP;
    }
    
    // Default to binary format
    return FileFormat::Binary;
}

bool FileSource::initializeFile() {
    if (!m_file) {
        return false;
    }
    
    // Reset position
    m_file->seek(0);
    m_currentPosition = 0;
    
    // Validate file has minimum size
    if (m_fileSize < static_cast<qint64>(sizeof(Packet::PacketHeader))) {
        m_logger->error("FileSource", "File too small to contain valid packets");
        return false;
    }
    
    return true;
}

bool FileSource::indexPackets() {
    if (!m_file) {
        return false;
    }
    
    m_logger->info("FileSource", "Building packet index...");
    
    m_packetIndex.clear();
    m_file->seek(0);
    
    qint64 position = 0;
    uint64_t packetCount = 0;
    
    while (position < m_fileSize) {
        // Check if we have enough data for header
        if (position + static_cast<qint64>(sizeof(Packet::PacketHeader)) > m_fileSize) {
            break;
        }
        
        // Read packet header to get size
        m_file->seek(position);
        QByteArray headerData = m_file->read(sizeof(Packet::PacketHeader));
        
        if (headerData.size() != sizeof(Packet::PacketHeader)) {
            break;
        }
        
        const Packet::PacketHeader* header = 
            reinterpret_cast<const Packet::PacketHeader*>(headerData.constData());
        
        // Calculate total packet size
        uint32_t totalSize = sizeof(Packet::PacketHeader) + header->payloadSize;
        
        // Validate packet header
        if (totalSize < sizeof(Packet::PacketHeader) || totalSize > 65536) { // Max 64KB packet
            m_logger->warning("FileSource", 
                QString("Invalid packet size at position %1: %2").arg(position).arg(totalSize));
            break;
        }
        
        // Check if complete packet is available
        if (position + totalSize > m_fileSize) {
            m_logger->warning("FileSource", 
                QString("Truncated packet at position %1").arg(position));
            break;
        }
        
        // Add to index
        m_packetIndex.emplace_back(position, totalSize, header->timestamp);
        
        // Move to next packet
        position += totalSize;
        packetCount++;
        
        // Progress feedback for large files
        if (packetCount % 10000 == 0) {
            m_logger->debug("FileSource", 
                QString("Indexed %1 packets...").arg(packetCount));
        }
    }
    
    m_indexBuilt = true;
    
    m_logger->info("FileSource", 
        QString("Packet index built: %1 packets").arg(m_packetIndex.size()));
    
    return true;
}

void FileSource::play() {
    if (!m_fileLoaded) {
        return;
    }
    
    if (m_playbackState == PlaybackState::Playing) {
        return; // Already playing
    }
    
    // If at end of file, reset to beginning (unless looping is handled elsewhere)
    if (isAtEndOfFile() && !m_config.loopPlayback) {
        seekToPacket(0);
    }
    
    setPlaybackState(PlaybackState::Playing);
    
    // Record playback start time
    m_playbackStartTime = QDateTime::currentDateTime();
    m_fileStats.playbackStarted = m_playbackStartTime;
    
    // Calculate playback interval
    int interval = calculatePlaybackInterval();
    m_playbackTimer->start(interval);
    
    m_logger->info("FileSource", 
        QString("Playback started at speed %1x").arg(m_config.playbackSpeed));
}

void FileSource::pausePlayback() {
    if (m_playbackState != PlaybackState::Playing) {
        return;
    }
    
    setPlaybackState(PlaybackState::Paused);
    m_playbackTimer->stop();
    
    m_logger->info("FileSource", "Playback paused");
}

void FileSource::stopPlayback() {
    if (m_playbackState == PlaybackState::Stopped) {
        return;
    }
    
    setPlaybackState(PlaybackState::Stopped);
    m_playbackTimer->stop();
    
    // Reset to beginning
    seekToPacket(0);
    
    m_logger->info("FileSource", "Playback stopped");
}

void FileSource::stepForward() {
    if (!m_fileLoaded || isAtEndOfFile()) {
        return;
    }
    
    // Pause if playing
    if (m_playbackState == PlaybackState::Playing) {
        pausePlayback();
    }
    
    // Read and deliver next packet
    readNextPacket();
    
    m_logger->debug("FileSource", 
        QString("Step forward to packet %1").arg(m_currentPacketIndex));
}

void FileSource::stepBackward() {
    if (!m_fileLoaded || isAtBeginningOfFile()) {
        return;
    }
    
    // Pause if playing
    if (m_playbackState == PlaybackState::Playing) {
        pausePlayback();
    }
    
    // Seek to previous packet
    if (m_currentPacketIndex > 0) {
        seekToPacket(m_currentPacketIndex - 1);
    }
    
    m_logger->debug("FileSource", 
        QString("Step backward to packet %1").arg(m_currentPacketIndex));
}

void FileSource::seekToPacket(uint64_t packetNumber) {
    if (!m_fileLoaded || !m_indexBuilt) {
        return;
    }
    
    if (packetNumber >= m_packetIndex.size()) {
        packetNumber = m_packetIndex.size() - 1;
    }
    
    const auto& index = m_packetIndex[packetNumber];
    m_currentPosition = index.position;
    m_currentPacketIndex = packetNumber;
    
    // Update statistics
    m_fileStats.currentPacket = packetNumber;
    m_fileStats.playbackProgress = static_cast<double>(packetNumber) / m_packetIndex.size();
    
    emit seekCompleted(packetNumber);
    emit fileStatisticsUpdated(m_fileStats);
    
    m_logger->debug("FileSource", 
        QString("Seeked to packet %1 at position %2").arg(packetNumber).arg(m_currentPosition));
}

void FileSource::seekToPosition(double position) {
    if (!m_fileLoaded || position < 0.0 || position > 1.0) {
        return;
    }
    
    uint64_t packetNumber = static_cast<uint64_t>(position * m_packetIndex.size());
    seekToPacket(packetNumber);
}

void FileSource::seekToTimestamp(const QDateTime& timestamp) {
    if (!m_fileLoaded || !m_indexBuilt) {
        return;
    }
    
    uint64_t targetTimestamp = timestamp.toMSecsSinceEpoch();
    
    // Binary search for closest timestamp
    auto it = std::lower_bound(m_packetIndex.begin(), m_packetIndex.end(), 
        PacketIndex(0, 0, targetTimestamp),
        [](const PacketIndex& a, const PacketIndex& b) {
            return a.timestamp < b.timestamp;
        });
    
    if (it != m_packetIndex.end()) {
        uint64_t packetNumber = std::distance(m_packetIndex.begin(), it);
        seekToPacket(packetNumber);
    }
}

void FileSource::setPlaybackSpeed(double speed) {
    speed = std::clamp(speed, 0.1, 10.0);
    
    if (std::abs(m_config.playbackSpeed - speed) > 0.01) {
        m_config.playbackSpeed = speed;
        
        // Update timer interval if playing
        if (m_playbackState == PlaybackState::Playing) {
            int interval = calculatePlaybackInterval();
            m_playbackTimer->setInterval(interval);
        }
        
        emit playbackSpeedChanged(speed);
        
        m_logger->debug("FileSource", 
            QString("Playback speed changed to %1x").arg(speed));
    }
}

void FileSource::setLoopPlayback(bool loop) {
    m_config.loopPlayback = loop;
}

void FileSource::setRealTimePlayback(bool realTime) {
    m_config.realTimePlayback = realTime;
    
    // Update timer if playing
    if (m_playbackState == PlaybackState::Playing) {
        int interval = calculatePlaybackInterval();
        m_playbackTimer->setInterval(interval);
    }
}

void FileSource::onPlaybackTimer() {
    if (m_playbackState != PlaybackState::Playing) {
        return;
    }
    
    if (!readNextPacket()) {
        // End of file reached
        handleEndOfFile();
    }
}

void FileSource::onProgressTimer() {
    updateFileStatistics();
    emit fileStatisticsUpdated(m_fileStats);
}

bool FileSource::readNextPacket() {
    if (!m_fileLoaded || isAtEndOfFile()) {
        return false;
    }
    
    // Use indexed position if available
    if (m_indexBuilt && m_currentPacketIndex < m_packetIndex.size()) {
        const auto& index = m_packetIndex[m_currentPacketIndex];
        auto packet = readPacketAtPosition(index.position);
        
        if (packet) {
            // Update position
            m_currentPosition = index.position + index.size;
            m_currentPacketIndex++;
            
            // Deliver packet
            deliverPacket(packet);
            m_packetsDelivered++;
            
            return true;
        }
    }
    
    return false;
}

Packet::PacketPtr FileSource::readPacketAtPosition(qint64 position) {
    if (!m_file || position >= m_fileSize) {
        return nullptr;
    }
    
    // Seek to position
    if (!m_file->seek(position)) {
        m_logger->error("FileSource", 
            QString("Failed to seek to position %1").arg(position));
        return nullptr;
    }
    
    // Read packet header
    QByteArray headerData = m_file->read(sizeof(Packet::PacketHeader));
    if (headerData.size() != sizeof(Packet::PacketHeader)) {
        return nullptr;
    }
    
    const Packet::PacketHeader* header = 
        reinterpret_cast<const Packet::PacketHeader*>(headerData.constData());
    
    // Calculate total packet size
    uint32_t totalSize = sizeof(Packet::PacketHeader) + header->payloadSize;
    
    // Validate packet size
    if (totalSize < sizeof(Packet::PacketHeader) || totalSize > 65536) {
        return nullptr;
    }
    
    // Read complete packet
    m_file->seek(position);
    QByteArray packetData = m_file->read(totalSize);
    if (packetData.size() != static_cast<int>(totalSize)) {
        return nullptr;
    }
    
    return createPacketFromData(packetData);
}

Packet::PacketPtr FileSource::createPacketFromData(const QByteArray& data) {
    if (!m_packetFactory) {
        m_logger->error("FileSource", "Packet factory not set");
        return nullptr;
    }
    
    // Create packet using factory
    auto result = m_packetFactory->createFromRawData(data.constData(), data.size());
    if (!result.success) {
        m_logger->error("FileSource", 
            QString("Failed to create packet: %1").arg(QString::fromStdString(result.error)));
        return nullptr;
    }
    
    auto packet = result.packet;
    
    return packet;
}

int FileSource::calculatePlaybackInterval() const {
    if (!m_config.realTimePlayback) {
        // Fast playback - just use minimum interval
        return MIN_PLAYBACK_INTERVAL;
    }
    
    // Calculate interval based on playback speed
    // Base interval for real-time playback (assuming 1000 Hz base rate)
    double baseInterval = 1.0; // 1ms base
    double interval = baseInterval / m_config.playbackSpeed;
    
    return std::clamp(static_cast<int>(interval), MIN_PLAYBACK_INTERVAL, MAX_PLAYBACK_INTERVAL);
}

void FileSource::setPlaybackState(PlaybackState newState) {
    PlaybackState oldState = m_playbackState;
    m_playbackState = newState;
    
    if (oldState != newState) {
        emit playbackStateChanged(oldState, newState);
    }
}

void FileSource::updateFileStatistics() {
    if (!m_fileLoaded) {
        return;
    }
    
    m_fileStats.currentPacket = m_currentPacketIndex;
    
    if (m_fileStats.totalPackets > 0) {
        m_fileStats.playbackProgress = 
            static_cast<double>(m_currentPacketIndex) / m_fileStats.totalPackets;
    }
    
    emit progressUpdated(m_fileStats.playbackProgress);
}

void FileSource::resetPlaybackPosition() {
    m_currentPosition = 0;
    m_currentPacketIndex = 0;
    m_packetsDelivered = 0;
    
    updateFileStatistics();
}

void FileSource::handleEndOfFile() {
    emit endOfFileReached();
    
    if (m_config.loopPlayback) {
        // Loop back to beginning
        seekToPacket(0);
        m_logger->debug("FileSource", "End of file reached, looping playback");
    } else {
        // Stop playback
        stopPlayback();
        m_logger->info("FileSource", "End of file reached, playback stopped");
    }
}

double FileSource::getPlaybackProgress() const {
    return m_fileStats.playbackProgress;
}

bool FileSource::isAtEndOfFile() const {
    if (!m_fileLoaded || !m_indexBuilt) {
        return true;
    }
    
    return m_currentPacketIndex >= m_packetIndex.size();
}

bool FileSource::isAtBeginningOfFile() const {
    return m_currentPacketIndex == 0;
}

void FileSource::setFileConfig(const FileSourceConfig& config) {
    m_config = config;
    
    // Update playback if currently playing
    if (m_playbackState == PlaybackState::Playing) {
        int interval = calculatePlaybackInterval();
        m_playbackTimer->setInterval(interval);
    }
}

bool FileSource::isValidPacketAtPosition(qint64 position) const {
    if (!m_file || position >= m_fileSize - static_cast<qint64>(sizeof(Packet::PacketHeader))) {
        return false;
    }
    
    // This is a simplified validation - real implementation would be more thorough
    return true;
}

} // namespace Offline
} // namespace Monitor