#include "udp_source.h"
#include "../../packet/core/packet_header.h"
#include <QNetworkInterface>
#include <QNetworkDatagram>
#include <QThread>
#include <QCoreApplication>

namespace Monitor {
namespace Network {

UdpSource::UdpSource(const NetworkConfig& config, QObject* parent)
    : PacketSource(Configuration(config.name), parent)
    , m_networkConfig(config)
    , m_socket(nullptr)
    , m_socketBound(false)
    , m_multicastJoined(false)
    , m_networkThread(nullptr)
    , m_pauseRequested(false)
    , m_packetsSinceLastCheck(0)
    , m_consecutiveErrors(0)
{
    // Initialize timing
    m_lastPacketTime = std::chrono::steady_clock::now();
    m_lastRateCheck = m_lastPacketTime;
    
    // Setup statistics timer
    m_statisticsTimer = std::make_unique<QTimer>(this);
    connect(m_statisticsTimer.get(), &QTimer::timeout, 
            this, &UdpSource::onStatisticsTimer);
    
    // Validate configuration
    if (!m_networkConfig.isValid()) {
        m_logger->warning("UdpSource", 
            QString("Invalid network configuration for source: %1")
            .arg(QString::fromStdString(m_networkConfig.name)));
    }
}

UdpSource::~UdpSource() {
    stop();
}

void UdpSource::setNetworkConfig(const NetworkConfig& config) {
    if (isRunning()) {
        m_logger->warning("UdpSource", 
            "Cannot change network configuration while source is running");
        return;
    }
    
    m_networkConfig = config;
    m_config.name = config.name;
}

QString UdpSource::getSocketState() const {
    if (!m_socket) {
        return "Not Initialized";
    }
    
    switch (m_socket->state()) {
        case QAbstractSocket::UnconnectedState: return "Unconnected";
        case QAbstractSocket::BoundState: return "Bound";
        case QAbstractSocket::ConnectedState: return "Connected";
        case QAbstractSocket::ClosingState: return "Closing";
        default: return "Unknown";
    }
}

bool UdpSource::doStart() {
    m_logger->info("UdpSource", 
        QString("Starting UDP source: %1 on %2:%3")
        .arg(QString::fromStdString(m_networkConfig.name))
        .arg(m_networkConfig.localAddress.toString())
        .arg(m_networkConfig.localPort));
    
    // Reset statistics
    m_networkStats.reset();
    m_consecutiveErrors = 0;
    m_pauseRequested = false;
    
    // Initialize socket
    if (!initializeSocket()) {
        return false;
    }
    
    // Bind socket
    if (!bindSocket()) {
        return false;
    }
    
    // Setup multicast if enabled
    if (m_networkConfig.enableMulticast) {
        if (!setupMulticast()) {
            m_logger->warning("UdpSource", "Failed to setup multicast, continuing with unicast");
        }
    }
    
    // Start statistics timer
    m_statisticsTimer->start(STATISTICS_UPDATE_INTERVAL);
    
    m_logger->info("UdpSource", 
        QString("UDP source started successfully: %1")
        .arg(QString::fromStdString(m_networkConfig.name)));
    
    return true;
}

void UdpSource::doStop() {
    m_logger->info("UdpSource", 
        QString("Stopping UDP source: %1")
        .arg(QString::fromStdString(m_networkConfig.name)));
    
    // Stop statistics timer
    if (m_statisticsTimer) {
        m_statisticsTimer->stop();
    }
    
    // Cleanup multicast
    if (m_multicastJoined) {
        cleanupMulticast();
    }
    
    // Close socket
    if (m_socket) {
        m_socket->close();
        m_socket.reset();
    }
    
    m_socketBound = false;
    m_multicastJoined = false;
    
    m_logger->info("UdpSource", 
        QString("UDP source stopped: %1")
        .arg(QString::fromStdString(m_networkConfig.name)));
}

void UdpSource::doPause() {
    m_pauseRequested = true;
    m_logger->info("UdpSource", 
        QString("UDP source paused: %1")
        .arg(QString::fromStdString(m_networkConfig.name)));
}

bool UdpSource::doResume() {
    m_pauseRequested = false;
    m_logger->info("UdpSource", 
        QString("UDP source resumed: %1")
        .arg(QString::fromStdString(m_networkConfig.name)));
    return true;
}

bool UdpSource::initializeSocket() {
    // Create socket
    m_socket = std::make_unique<QUdpSocket>(this);
    
    // Connect signals
    connect(m_socket.get(), &QUdpSocket::readyRead,
            this, &UdpSource::onDatagramReady);
    connect(m_socket.get(), QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &UdpSource::onSocketError);
    connect(m_socket.get(), &QAbstractSocket::stateChanged,
            this, &UdpSource::onSocketStateChanged);
    
    // Configure socket options
    configureSocketOptions();
    
    return true;
}

void UdpSource::configureSocketOptions() {
    if (!m_socket) return;
    
    // Set receive buffer size
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 
                             m_networkConfig.receiveBufferSize);
    
    // Set socket priority if specified
    if (m_networkConfig.priority > 0) {
        m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    }
    
    // Enable broadcast for UDP (needed for some multicast scenarios)
    m_socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 0);
}

bool UdpSource::bindSocket() {
    if (!m_socket) {
        return false;
    }
    
    // Bind to local address and port
    bool success = m_socket->bind(m_networkConfig.localAddress, 
                                  m_networkConfig.localPort,
                                  QUdpSocket::ReuseAddressHint);
    
    if (success) {
        m_socketBound = true;
        emit socketStateChanged("Bound");
        m_logger->info("UdpSource", 
            QString("Socket bound to %1:%2")
            .arg(m_networkConfig.localAddress.toString())
            .arg(m_networkConfig.localPort));
    } else {
        reportError("Failed to bind UDP socket: " + m_socket->errorString().toStdString());
        return false;
    }
    
    return true;
}

bool UdpSource::setupMulticast() {
    if (!m_socket || !m_networkConfig.enableMulticast) {
        return false;
    }
    
    return joinMulticastGroup();
}

bool UdpSource::joinMulticastGroup() {
    if (!m_socket || m_multicastJoined) {
        return false;
    }
    
    // Select network interface
    QNetworkInterface interface;
    if (!m_networkConfig.networkInterface.isEmpty()) {
        interface = QNetworkInterface::interfaceFromName(m_networkConfig.networkInterface);
        if (!interface.isValid()) {
            m_logger->warning("UdpSource", 
                QString("Specified network interface not found: %1, using default")
                .arg(m_networkConfig.networkInterface));
            interface = QNetworkInterface();
        }
    }
    
    // Join multicast group
    bool success = m_socket->joinMulticastGroup(m_networkConfig.multicastGroup, interface);
    
    if (success) {
        m_multicastJoined = true;
        emit multicastStatusChanged(true);
        m_logger->info("UdpSource", 
            QString("Joined multicast group: %1")
            .arg(m_networkConfig.multicastGroup.toString()));
    } else {
        m_logger->error("UdpSource", 
            QString("Failed to join multicast group: %1 - %2")
            .arg(m_networkConfig.multicastGroup.toString())
            .arg(m_socket->errorString()));
    }
    
    return success;
}

bool UdpSource::leaveMulticastGroup() {
    if (!m_socket || !m_multicastJoined) {
        return false;
    }
    
    bool success = m_socket->leaveMulticastGroup(m_networkConfig.multicastGroup);
    
    if (success) {
        m_multicastJoined = false;
        emit multicastStatusChanged(false);
        m_logger->info("UdpSource", 
            QString("Left multicast group: %1")
            .arg(m_networkConfig.multicastGroup.toString()));
    } else {
        m_logger->warning("UdpSource", 
            QString("Failed to leave multicast group: %1")
            .arg(m_networkConfig.multicastGroup.toString()));
    }
    
    return success;
}

void UdpSource::cleanupMulticast() {
    if (m_multicastJoined) {
        leaveMulticastGroup();
    }
}

void UdpSource::onDatagramReady() {
    if (m_pauseRequested.load()) {
        return; // Skip processing while paused
    }
    
    auto receiveTime = std::chrono::steady_clock::now();
    
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        
        if (datagram.isValid()) {
            processDatagram(datagram);
            updateLatencyStats(receiveTime);
        } else {
            m_networkStats.packetErrors++;
            m_consecutiveErrors++;
            
            if (m_consecutiveErrors > MAX_CONSECUTIVE_ERRORS) {
                reportError("Too many consecutive datagram errors");
                return;
            }
        }
    }
    
    // Reset consecutive error counter on successful processing
    m_consecutiveErrors = 0;
}

void UdpSource::processDatagram(const QNetworkDatagram& datagram) {
    // Check rate limiting
    if (shouldDropForRateLimit()) {
        m_networkStats.packetsDropped++;
        return;
    }
    
    // Create packet from datagram
    auto packet = createPacketFromDatagram(datagram.data(),
                                          datagram.senderAddress(),
                                          datagram.senderPort());
    
    if (packet) {
        // Update statistics
        m_networkStats.packetsReceived++;
        m_networkStats.bytesReceived += datagram.data().size();
        m_networkStats.lastPacketTime = std::chrono::steady_clock::now();
        
        // Deliver packet
        deliverPacket(packet);
        
        m_packetsSinceLastCheck++;
    } else {
        m_networkStats.packetErrors++;
    }
}

Packet::PacketPtr UdpSource::createPacketFromDatagram(const QByteArray& data,
                                                     const QHostAddress& /* sender */,
                                                     quint16 /* senderPort */) {
    if (!m_packetFactory) {
        m_logger->error("UdpSource", "Packet factory not set");
        return nullptr;
    }
    
    // Ensure minimum packet size
    if (data.size() < static_cast<qsizetype>(sizeof(Packet::PacketHeader))) {
        m_logger->warning("UdpSource", 
            QString("Received datagram too small: %1 bytes").arg(data.size()));
        return nullptr;
    }
    
    // Create packet using factory
    auto result = m_packetFactory->createFromRawData(data.constData(), data.size());
    if (!result.success) {
        m_logger->error("UdpSource", 
            QString("Failed to create packet: %1").arg(QString::fromStdString(result.error)));
        return nullptr;
    }
    
    auto packet = result.packet;
    
    // Set network-specific metadata if needed
    // This could include sender information, network interface, etc.
    
    return packet;
}

void UdpSource::onSocketError(QAbstractSocket::SocketError error) {
    m_networkStats.socketErrors++;
    
    QString errorString = m_socket ? m_socket->errorString() : "Unknown error";
    
    m_logger->error("UdpSource", 
        QString("Socket error in UDP source %1: %2 (%3)")
        .arg(QString::fromStdString(m_networkConfig.name))
        .arg(errorString)
        .arg(static_cast<int>(error)));
    
    // Handle specific errors
    switch (error) {
        case QAbstractSocket::AddressInUseError:
        case QAbstractSocket::SocketAddressNotAvailableError:
            reportError("Network address error: " + errorString.toStdString());
            break;
        case QAbstractSocket::NetworkError:
            // Network might recover, log but don't stop
            m_logger->warning("UdpSource", "Network error, will continue monitoring");
            break;
        default:
            reportError("Socket error: " + errorString.toStdString());
            break;
    }
}

void UdpSource::onSocketStateChanged(QAbstractSocket::SocketState /* state */) {
    emit socketStateChanged(getSocketState());
    
    m_logger->debug("UdpSource", 
        QString("Socket state changed to: %1").arg(getSocketState()));
}

void UdpSource::onStatisticsTimer() {
    updateNetworkStatistics();
}

void UdpSource::updateNetworkStatistics() {
    // Calculate current rates
    double currentPacketRate = m_networkStats.getCurrentPacketRate();
    double currentByteRate = m_networkStats.getCurrentByteRate();
    
    // Update atomic rate values
    m_networkStats.packetRate.store(currentPacketRate);
    m_networkStats.byteRate.store(currentByteRate);
    
    // Emit statistics update
    emit networkStatisticsUpdated(m_networkStats);
    emit statisticsUpdated(getStatistics()); // Base class signal
}

void UdpSource::updateLatencyStats(const std::chrono::steady_clock::time_point& receiveTime) {
    if (m_networkConfig.enableTimestamping) {
        auto now = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(now - receiveTime);
        
        // Simple exponential moving average
        double currentLatency = m_networkStats.averageLatency.load();
        double newLatency = static_cast<double>(latency.count());
        double alpha = 0.1; // Smoothing factor
        
        double updatedLatency = (alpha * newLatency) + ((1.0 - alpha) * currentLatency);
        m_networkStats.averageLatency.store(updatedLatency);
    }
}

bool UdpSource::shouldDropForRateLimit() {
    if (m_config.maxPacketRate == 0) {
        return false; // No rate limiting
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRateCheck);
    
    // Check every 100ms
    if (elapsed.count() >= 100) {
        uint32_t packetsInInterval = m_packetsSinceLastCheck.exchange(0);
        double currentRate = (packetsInInterval * 1000.0) / elapsed.count();
        
        m_lastRateCheck = now;
        
        return currentRate > m_config.maxPacketRate;
    }
    
    return false;
}

} // namespace Network
} // namespace Monitor