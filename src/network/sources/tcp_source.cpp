#include "tcp_source.h"
#include "../../packet/core/packet_header.h"
#include <QThread>
#include <QCoreApplication>
#include <QRandomGenerator>
#include <algorithm>

namespace Monitor {
namespace Network {

TcpSource::TcpSource(const NetworkConfig& config, QObject* parent)
    : PacketSource(Configuration(config.name), parent)
    , m_networkConfig(config)
    , m_socket(nullptr)
    , m_connectionState(ConnectionState::Disconnected)
    , m_shouldReconnect(false)
    , m_reconnectAttempt(0)
    , m_expectedPacketSize(0)
    , m_parsingHeader(true)
    , m_pauseRequested(false)
    , m_consecutiveErrors(0)
    , m_connectionFailures(0)
{
    // Initialize timers
    m_reconnectTimer = std::make_unique<QTimer>(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer.get(), &QTimer::timeout, 
            this, &TcpSource::onReconnectTimer);
    
    m_keepAliveTimer = std::make_unique<QTimer>(this);
    connect(m_keepAliveTimer.get(), &QTimer::timeout,
            this, &TcpSource::onKeepAliveTimer);
    
    m_statisticsTimer = std::make_unique<QTimer>(this);
    connect(m_statisticsTimer.get(), &QTimer::timeout, 
            this, &TcpSource::onStatisticsTimer);
    
    // Validate configuration
    if (!m_networkConfig.isValid()) {
        m_logger->warning("TcpSource", 
            QString("Invalid network configuration for source: %1")
            .arg(QString::fromStdString(m_networkConfig.name)));
    }
}

TcpSource::~TcpSource() {
    stop();
}

void TcpSource::setNetworkConfig(const NetworkConfig& config) {
    if (isRunning()) {
        m_logger->warning("TcpSource", 
            "Cannot change network configuration while source is running");
        return;
    }
    
    m_networkConfig = config;
    m_config.name = config.name;
}

QString TcpSource::getConnectionStateString() const {
    return connectionStateToString(m_connectionState);
}

QString TcpSource::getSocketState() const {
    if (!m_socket) {
        return "Not Initialized";
    }
    
    switch (m_socket->state()) {
        case QAbstractSocket::UnconnectedState: return "Unconnected";
        case QAbstractSocket::ConnectingState: return "Connecting";
        case QAbstractSocket::ConnectedState: return "Connected";
        case QAbstractSocket::BoundState: return "Bound";
        case QAbstractSocket::ClosingState: return "Closing";
        default: return "Unknown";
    }
}

bool TcpSource::doStart() {
    m_logger->info("TcpSource", 
        QString("Starting TCP source: %1 connecting to %2:%3")
        .arg(QString::fromStdString(m_networkConfig.name))
        .arg(m_networkConfig.remoteAddress.toString())
        .arg(m_networkConfig.remotePort));
    
    // Reset statistics and state
    m_networkStats.reset();
    m_consecutiveErrors = 0;
    m_connectionFailures = 0;
    m_reconnectAttempt = 0;
    m_pauseRequested = false;
    m_shouldReconnect = true;
    
    // Initialize socket
    if (!initializeSocket()) {
        return false;
    }
    
    // Start connection
    if (!connectToHost()) {
        return false;
    }
    
    // Start statistics timer
    m_statisticsTimer->start(STATISTICS_UPDATE_INTERVAL);
    
    m_logger->info("TcpSource", 
        QString("TCP source started successfully: %1")
        .arg(QString::fromStdString(m_networkConfig.name)));
    
    return true;
}

void TcpSource::doStop() {
    m_logger->info("TcpSource", 
        QString("Stopping TCP source: %1")
        .arg(QString::fromStdString(m_networkConfig.name)));
    
    m_shouldReconnect = false;
    
    // Stop all timers
    if (m_statisticsTimer) m_statisticsTimer->stop();
    if (m_reconnectTimer) m_reconnectTimer->stop();
    if (m_keepAliveTimer) m_keepAliveTimer->stop();
    
    // Disconnect from host
    disconnectFromHost();
    
    // Clean up socket
    if (m_socket) {
        m_socket.reset();
    }
    
    setConnectionState(ConnectionState::Disconnected);
    
    m_logger->info("TcpSource", 
        QString("TCP source stopped: %1")
        .arg(QString::fromStdString(m_networkConfig.name)));
}

void TcpSource::doPause() {
    m_pauseRequested = true;
    m_logger->info("TcpSource", 
        QString("TCP source paused: %1")
        .arg(QString::fromStdString(m_networkConfig.name)));
}

bool TcpSource::doResume() {
    m_pauseRequested = false;
    m_logger->info("TcpSource", 
        QString("TCP source resumed: %1")
        .arg(QString::fromStdString(m_networkConfig.name)));
    return true;
}

bool TcpSource::initializeSocket() {
    // Create socket
    m_socket = std::make_unique<QTcpSocket>(this);
    
    // Connect signals
    connect(m_socket.get(), &QTcpSocket::connected,
            this, &TcpSource::onConnected);
    connect(m_socket.get(), &QTcpSocket::disconnected,
            this, &TcpSource::onDisconnected);
    connect(m_socket.get(), &QTcpSocket::readyRead,
            this, &TcpSource::onDataReady);
    connect(m_socket.get(), QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &TcpSource::onSocketError);
    connect(m_socket.get(), &QAbstractSocket::stateChanged,
            this, &TcpSource::onSocketStateChanged);
    
    // Configure socket options
    configureSocketOptions();
    
    // Reset stream state
    resetStreamState();
    
    return true;
}

void TcpSource::configureSocketOptions() {
    if (!m_socket) return;
    
    // Set receive buffer size
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 
                             m_networkConfig.receiveBufferSize);
    
    // Enable low delay for real-time applications
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    
    // Configure keep-alive if enabled
    if (m_networkConfig.enableKeepAlive) {
        m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    }
}

bool TcpSource::connectToHost() {
    if (!m_socket) {
        return false;
    }
    
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_logger->warning("TcpSource", "Socket not in unconnected state");
        return false;
    }
    
    setConnectionState(ConnectionState::Connecting);
    
    m_socket->connectToHost(m_networkConfig.remoteAddress, 
                           m_networkConfig.remotePort);
    
    // Wait for connection with timeout
    if (!m_socket->waitForConnected(m_networkConfig.connectionTimeout)) {
        QString error = m_socket->errorString();
        m_logger->error("TcpSource", 
            QString("Connection timeout: %1").arg(error));
        handleConnectionLost();
        return false;
    }
    
    return true;
}

void TcpSource::disconnectFromHost() {
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        
        // Wait for disconnection
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(3000); // 3 second timeout
        }
    }
}

void TcpSource::resetConnection() {
    m_logger->info("TcpSource", "Resetting TCP connection");
    
    disconnectFromHost();
    resetStreamState();
    m_consecutiveErrors = 0;
    
    if (m_shouldReconnect && isRunning()) {
        startReconnection();
    }
}

void TcpSource::onConnected() {
    m_logger->info("TcpSource", 
        QString("TCP connection established to %1:%2")
        .arg(m_networkConfig.remoteAddress.toString())
        .arg(m_networkConfig.remotePort));
    
    handleConnectionEstablished();
}

void TcpSource::onDisconnected() {
    m_logger->info("TcpSource", 
        QString("TCP connection lost from %1:%2")
        .arg(m_networkConfig.remoteAddress.toString())
        .arg(m_networkConfig.remotePort));
    
    handleConnectionLost();
}

void TcpSource::onDataReady() {
    if (m_pauseRequested.load()) {
        return; // Skip processing while paused
    }
    
    processIncomingData();
}

void TcpSource::processIncomingData() {
    if (!m_socket) return;
    
    // Read available data
    QByteArray newData = m_socket->readAll();
    if (newData.isEmpty()) return;
    
    // Update statistics
    m_networkStats.bytesReceived += newData.size();
    
    // Append to stream buffer
    m_streamBuffer.append(newData);
    
    // Prevent buffer overflow
    if (m_streamBuffer.size() > STREAM_BUFFER_MAX_SIZE) {
        m_logger->warning("TcpSource", "Stream buffer overflow, resetting");
        resetStreamState();
        m_networkStats.packetErrors++;
        return;
    }
    
    // Parse packets from buffer
    while (parsePacketFromStream()) {
        // Continue parsing until no complete packets found
    }
}

bool TcpSource::parsePacketFromStream() {
    if (m_streamBuffer.size() < MIN_PACKET_SIZE) {
        return false; // Not enough data
    }
    
    if (m_parsingHeader) {
        // Need at least header size
        if (m_streamBuffer.size() < static_cast<qsizetype>(sizeof(Packet::PacketHeader))) {
            return false;
        }
        
        // Extract packet size from header
        const Packet::PacketHeader* header = 
            reinterpret_cast<const Packet::PacketHeader*>(m_streamBuffer.constData());
        
        // Calculate total packet size (header + payload)
        m_expectedPacketSize = sizeof(Packet::PacketHeader) + header->payloadSize;
        
        // Validate packet size
        if (m_expectedPacketSize < MIN_PACKET_SIZE || 
            m_expectedPacketSize > MAX_PACKET_SIZE) {
            m_logger->warning("TcpSource", 
                QString("Invalid packet size in header: %1").arg(m_expectedPacketSize));
            resetStreamState();
            m_networkStats.packetErrors++;
            return false;
        }
        
        m_parsingHeader = false;
    }
    
    // Check if we have complete packet
    if (m_streamBuffer.size() < m_expectedPacketSize) {
        return false; // Wait for more data
    }
    
    // Extract packet data
    QByteArray packetData = m_streamBuffer.left(m_expectedPacketSize);
    m_streamBuffer.remove(0, m_expectedPacketSize);
    
    // Create packet
    auto packet = createPacketFromData(packetData);
    if (packet) {
        // Update statistics
        m_networkStats.packetsReceived++;
        m_networkStats.lastPacketTime = std::chrono::steady_clock::now();
        
        // Deliver packet
        deliverPacket(packet);
        
        m_consecutiveErrors = 0; // Reset error counter on success
    } else {
        m_networkStats.packetErrors++;
        m_consecutiveErrors++;
    }
    
    // Reset for next packet
    m_parsingHeader = true;
    m_expectedPacketSize = 0;
    
    // Check for too many consecutive errors
    if (m_consecutiveErrors > MAX_CONSECUTIVE_ERRORS) {
        m_logger->error("TcpSource", "Too many consecutive packet parsing errors");
        resetConnection();
        return false;
    }
    
    return true; // Continue parsing if more data available
}

Packet::PacketPtr TcpSource::createPacketFromData(const QByteArray& data) {
    if (!m_packetFactory) {
        m_logger->error("TcpSource", "Packet factory not set");
        return nullptr;
    }
    
    // Create packet using factory
    auto result = m_packetFactory->createFromRawData(data.constData(), data.size());
    if (!result.success) {
        m_logger->error("TcpSource", 
            QString("Failed to create packet: %1").arg(QString::fromStdString(result.error)));
        return nullptr;
    }
    
    auto packet = result.packet;
    
    return packet;
}

void TcpSource::onSocketError(QAbstractSocket::SocketError error) {
    m_networkStats.socketErrors++;
    
    QString errorString = m_socket ? m_socket->errorString() : "Unknown error";
    
    m_logger->error("TcpSource", 
        QString("Socket error in TCP source %1: %2 (%3)")
        .arg(QString::fromStdString(m_networkConfig.name))
        .arg(errorString)
        .arg(static_cast<int>(error)));
    
    // Handle specific errors
    switch (error) {
        case QAbstractSocket::ConnectionRefusedError:
        case QAbstractSocket::RemoteHostClosedError:
        case QAbstractSocket::HostNotFoundError:
        case QAbstractSocket::NetworkError:
            handleConnectionLost();
            break;
        default:
            reportError("Socket error: " + errorString.toStdString());
            break;
    }
}

void TcpSource::onSocketStateChanged(QAbstractSocket::SocketState /* state */) {
    m_logger->debug("TcpSource", 
        QString("Socket state changed to: %1").arg(getSocketState()));
}

void TcpSource::handleConnectionEstablished() {
    m_reconnectAttempt = 0;
    m_connectionFailures = 0;
    
    setConnectionState(ConnectionState::Connected);
    
    // Start keep-alive timer if enabled
    if (m_networkConfig.enableKeepAlive) {
        m_keepAliveTimer->start(m_networkConfig.keepAliveInterval * 1000);
    }
    
    emit connected();
}

void TcpSource::handleConnectionLost() {
    setConnectionState(ConnectionState::Disconnected);
    
    // Stop keep-alive timer
    if (m_keepAliveTimer) {
        m_keepAliveTimer->stop();
    }
    
    // Reset stream state
    resetStreamState();
    
    // Update statistics
    m_networkStats.connectionDrops++;
    
    emit disconnected();
    
    // Start reconnection if should reconnect and source is running
    if (m_shouldReconnect && isRunning()) {
        startReconnection();
    }
}

void TcpSource::startReconnection() {
    if (m_connectionFailures >= MAX_CONNECTION_FAILURES) {
        m_logger->error("TcpSource", 
            QString("Maximum connection failures reached (%1), giving up")
            .arg(MAX_CONNECTION_FAILURES));
        setConnectionState(ConnectionState::Failed);
        reportError("Maximum connection failures reached");
        return;
    }
    
    setConnectionState(ConnectionState::Reconnecting);
    
    int delay = calculateReconnectDelay();
    m_logger->info("TcpSource", 
        QString("Scheduling reconnection attempt %1 in %2ms")
        .arg(m_reconnectAttempt + 1).arg(delay));
    
    m_reconnectTimer->start(delay);
}

void TcpSource::onReconnectTimer() {
    if (!m_shouldReconnect || !isRunning()) {
        return;
    }
    
    m_reconnectAttempt++;
    m_logger->info("TcpSource", 
        QString("Reconnection attempt %1/%2")
        .arg(m_reconnectAttempt).arg(m_networkConfig.maxReconnectAttempts));
    
    // Reinitialize socket
    if (m_socket) {
        m_socket.reset();
    }
    
    if (!initializeSocket()) {
        m_logger->error("TcpSource", "Failed to reinitialize socket");
        m_connectionFailures++;
        startReconnection();
        return;
    }
    
    // Attempt connection
    if (!connectToHost()) {
        m_connectionFailures++;
        
        if (m_reconnectAttempt >= m_networkConfig.maxReconnectAttempts) {
            m_logger->error("TcpSource", 
                QString("Maximum reconnection attempts reached (%1)")
                .arg(m_networkConfig.maxReconnectAttempts));
            setConnectionState(ConnectionState::Failed);
            emit connectionFailed("Maximum reconnection attempts exceeded");
            return;
        }
        
        startReconnection();
    }
}

void TcpSource::onKeepAliveTimer() {
    // Send keep-alive by attempting to write 0 bytes
    // This will trigger error if connection is lost
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(QByteArray());
    }
}

void TcpSource::onStatisticsTimer() {
    updateNetworkStatistics();
}

int TcpSource::calculateReconnectDelay() {
    // Exponential backoff with jitter
    int delay = BASE_RECONNECT_DELAY * (1 << std::min(m_reconnectAttempt, 6));
    delay = std::min(delay, MAX_RECONNECT_DELAY);
    
    // Add jitter (±25%)
    int jitter = delay / 4;
    delay += (QRandomGenerator::global()->bounded(2 * jitter)) - jitter;
    
    return std::max(delay, BASE_RECONNECT_DELAY);
}

void TcpSource::setConnectionState(ConnectionState newState) {
    ConnectionState oldState = m_connectionState;
    m_connectionState = newState;
    
    if (oldState != newState) {
        emit connectionStateChanged(oldState, newState);
    }
}

void TcpSource::resetStreamState() {
    m_streamBuffer.clear();
    m_expectedPacketSize = 0;
    m_parsingHeader = true;
}

void TcpSource::updateNetworkStatistics() {
    // Calculate current rates
    double currentPacketRate = m_networkStats.getCurrentPacketRate();
    double currentByteRate = m_networkStats.getCurrentByteRate();
    
    // Update atomic rate values
    m_networkStats.packetRate.store(currentPacketRate);
    m_networkStats.byteRate.store(currentByteRate);
    
    // Update reconnection statistics
    m_networkStats.reconnections.store(m_reconnectAttempt);
    
    // Emit statistics update
    emit networkStatisticsUpdated(m_networkStats);
    emit statisticsUpdated(getStatistics()); // Base class signal
}

} // namespace Network
} // namespace Monitor