#pragma once

#include "../../packet/sources/packet_source.h"
#include "../config/network_config.h"
#include "../../concurrent/spsc_ring_buffer.h"
#include "../../logging/logger.h"

#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include <memory>
#include <atomic>
#include <deque>

namespace Monitor {
namespace Network {

/**
 * @brief TCP packet source for network-based packet reception
 * 
 * High-performance TCP packet source that receives packets from TCP streams
 * with automatic connection management, stream parsing, and reconnection logic.
 * Handles packet boundary detection and partial packet assembly.
 * 
 * Key Features:
 * - Event-driven TCP connection management
 * - Stream-based packet boundary detection
 * - Automatic reconnection with exponential backoff
 * - Partial packet assembly and buffering
 * - Connection state monitoring
 * - Comprehensive error recovery
 */
class TcpSource : public Packet::PacketSource {
    Q_OBJECT
    
public:
    /**
     * @brief Connection state enumeration
     */
    enum class ConnectionState {
        Disconnected,    ///< Not connected
        Connecting,      ///< Connection in progress
        Connected,       ///< Successfully connected
        Reconnecting,    ///< Attempting to reconnect
        Failed           ///< Connection failed permanently
    };
    
    /**
     * @brief Construct TCP source with network configuration
     * @param config Network configuration parameters
     * @param parent Qt parent object
     */
    explicit TcpSource(const NetworkConfig& config, QObject* parent = nullptr);
    
    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~TcpSource() override;
    
    /**
     * @brief Get network configuration
     */
    const NetworkConfig& getNetworkConfig() const { return m_networkConfig; }
    
    /**
     * @brief Update network configuration (requires restart)
     */
    void setNetworkConfig(const NetworkConfig& config);
    
    /**
     * @brief Get network-specific statistics
     */
    const NetworkStatistics& getNetworkStatistics() const { return m_networkStats; }
    
    /**
     * @brief Get current connection state
     */
    ConnectionState getConnectionState() const { return m_connectionState; }
    
    /**
     * @brief Get connection state as string
     */
    QString getConnectionStateString() const;
    
    /**
     * @brief Check if connected to remote host
     */
    bool isConnected() const { return m_connectionState == ConnectionState::Connected; }
    
    /**
     * @brief Get current socket state for diagnostics
     */
    QString getSocketState() const;

public slots:
    /**
     * @brief Manually trigger connection attempt
     */
    bool connectToHost();
    
    /**
     * @brief Manually disconnect from host
     */
    void disconnectFromHost();
    
    /**
     * @brief Reset connection and clear buffers
     */
    void resetConnection();

signals:
    /**
     * @brief Emitted when connection state changes
     */
    void connectionStateChanged(ConnectionState oldState, ConnectionState newState);
    
    /**
     * @brief Emitted when successfully connected to host
     */
    void connected();
    
    /**
     * @brief Emitted when disconnected from host
     */
    void disconnected();
    
    /**
     * @brief Emitted when connection fails
     */
    void connectionFailed(const QString& reason);
    
    /**
     * @brief Emitted when network statistics are updated
     */
    void networkStatisticsUpdated(const NetworkStatistics& stats);

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
     * @brief Handle successful connection
     */
    void onConnected();
    
    /**
     * @brief Handle disconnection
     */
    void onDisconnected();
    
    /**
     * @brief Handle incoming data
     */
    void onDataReady();
    
    /**
     * @brief Handle socket errors
     */
    void onSocketError(QAbstractSocket::SocketError error);
    
    /**
     * @brief Handle socket state changes
     */
    void onSocketStateChanged(QAbstractSocket::SocketState state);
    
    /**
     * @brief Handle reconnection timer
     */
    void onReconnectTimer();
    
    /**
     * @brief Handle keep-alive timer
     */
    void onKeepAliveTimer();
    
    /**
     * @brief Periodic statistics update
     */
    void onStatisticsTimer();

private:
    /**
     * @brief Initialize TCP socket with configuration
     */
    bool initializeSocket();
    
    /**
     * @brief Configure socket options
     */
    void configureSocketOptions();
    
    /**
     * @brief Process incoming data stream
     */
    void processIncomingData();
    
    /**
     * @brief Parse packet from stream buffer
     */
    bool parsePacketFromStream();
    
    /**
     * @brief Create packet from parsed data
     */
    Packet::PacketPtr createPacketFromData(const QByteArray& data);
    
    /**
     * @brief Handle connection establishment
     */
    void handleConnectionEstablished();
    
    /**
     * @brief Handle connection loss
     */
    void handleConnectionLost();
    
    /**
     * @brief Start reconnection process
     */
    void startReconnection();
    
    /**
     * @brief Calculate next reconnection delay
     */
    int calculateReconnectDelay();
    
    /**
     * @brief Set connection state and emit signal
     */
    void setConnectionState(ConnectionState newState);
    
    /**
     * @brief Reset stream parsing state
     */
    void resetStreamState();
    
    /**
     * @brief Update network statistics
     */
    void updateNetworkStatistics();
    
    /**
     * @brief Find packet boundary in stream
     */
    int findPacketBoundary(const QByteArray& data, int startPos) const;
    
    /**
     * @brief Validate packet size and structure
     */
    bool isValidPacket(const QByteArray& data) const;
    
    // Network configuration
    NetworkConfig m_networkConfig;
    
    // Socket and connection management
    std::unique_ptr<QTcpSocket> m_socket;
    ConnectionState m_connectionState;
    std::atomic<bool> m_shouldReconnect;
    int m_reconnectAttempt;
    std::unique_ptr<QTimer> m_reconnectTimer;
    std::unique_ptr<QTimer> m_keepAliveTimer;
    
    // Stream processing
    QByteArray m_streamBuffer;
    int m_expectedPacketSize;
    bool m_parsingHeader;
    std::atomic<bool> m_pauseRequested;
    
    // Statistics and monitoring
    NetworkStatistics m_networkStats;
    std::unique_ptr<QTimer> m_statisticsTimer;
    
    // Error handling
    std::atomic<uint32_t> m_consecutiveErrors;
    std::atomic<uint32_t> m_connectionFailures;
    static constexpr uint32_t MAX_CONSECUTIVE_ERRORS = 10;
    static constexpr uint32_t MAX_CONNECTION_FAILURES = 5;
    
    // Performance tuning
    static constexpr int STATISTICS_UPDATE_INTERVAL = 1000;    // 1 second
    static constexpr int STREAM_BUFFER_MAX_SIZE = 1048576;     // 1MB
    static constexpr int MIN_PACKET_SIZE = 24;                 // Minimum packet size
    static constexpr int MAX_PACKET_SIZE = 65536;              // Maximum packet size (64KB)
    static constexpr int BASE_RECONNECT_DELAY = 1000;          // Base delay: 1 second
    static constexpr int MAX_RECONNECT_DELAY = 60000;          // Max delay: 60 seconds
};

/**
 * @brief Convert connection state to string
 */
inline QString connectionStateToString(TcpSource::ConnectionState state) {
    switch (state) {
        case TcpSource::ConnectionState::Disconnected: return "Disconnected";
        case TcpSource::ConnectionState::Connecting: return "Connecting";
        case TcpSource::ConnectionState::Connected: return "Connected";
        case TcpSource::ConnectionState::Reconnecting: return "Reconnecting";
        case TcpSource::ConnectionState::Failed: return "Failed";
        default: return "Unknown";
    }
}

} // namespace Network
} // namespace Monitor