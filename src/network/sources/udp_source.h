#pragma once

#include "../../packet/sources/packet_source.h"
#include "../config/network_config.h"
#include "../../concurrent/spsc_ring_buffer.h"
#include "../../threading/thread_worker.h"
#include "../../logging/logger.h"

#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QThread>
#include <QTimer>
#include <QNetworkInterface>
#include <memory>
#include <atomic>

namespace Monitor {
namespace Network {

/**
 * @brief UDP packet source for network-based packet reception
 * 
 * High-performance UDP packet source that receives packets from network
 * interfaces with support for unicast and multicast reception. Implements
 * event-driven architecture with dedicated network thread for zero packet loss.
 * 
 * Key Features:
 * - Event-driven packet reception (no polling)
 * - Lock-free ring buffer for inter-thread communication
 * - Multicast group support
 * - High-precision packet timestamping
 * - Comprehensive network statistics
 * - Automatic error recovery
 */
class UdpSource : public Packet::PacketSource {
    Q_OBJECT
    
public:
    /**
     * @brief Construct UDP source with network configuration
     * @param config Network configuration parameters
     * @param parent Qt parent object
     */
    explicit UdpSource(const NetworkConfig& config, QObject* parent = nullptr);
    
    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~UdpSource() override;
    
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
     * @brief Check if multicast is enabled and active
     */
    bool isMulticastActive() const { return m_networkConfig.enableMulticast && m_multicastJoined; }
    
    /**
     * @brief Get current socket state for diagnostics
     */
    QString getSocketState() const;

public slots:
    /**
     * @brief Join multicast group (if configured)
     */
    bool joinMulticastGroup();
    
    /**
     * @brief Leave multicast group
     */
    bool leaveMulticastGroup();
    
    /**
     * @brief Update network statistics
     */
    void updateNetworkStatistics();

signals:
    /**
     * @brief Emitted when multicast group is joined/left
     */
    void multicastStatusChanged(bool joined);
    
    /**
     * @brief Emitted when network statistics are updated
     */
    void networkStatisticsUpdated(const NetworkStatistics& stats);
    
    /**
     * @brief Emitted when socket state changes
     */
    void socketStateChanged(const QString& state);

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
     * @brief Handle incoming UDP datagrams
     */
    void onDatagramReady();
    
    /**
     * @brief Handle socket errors
     */
    void onSocketError(QAbstractSocket::SocketError error);
    
    /**
     * @brief Handle socket state changes
     */
    void onSocketStateChanged(QAbstractSocket::SocketState state);
    
    /**
     * @brief Periodic statistics update
     */
    void onStatisticsTimer();

private:
    /**
     * @brief Initialize UDP socket with configuration
     */
    bool initializeSocket();
    
    /**
     * @brief Configure socket options
     */
    void configureSocketOptions();
    
    /**
     * @brief Process received datagram and create packet
     */
    void processDatagram(const QNetworkDatagram& datagram);
    
    /**
     * @brief Create packet from datagram data
     */
    Packet::PacketPtr createPacketFromDatagram(const QByteArray& data, 
                                               const QHostAddress& sender,
                                               quint16 senderPort);
    
    /**
     * @brief Handle socket binding
     */
    bool bindSocket();
    
    /**
     * @brief Handle multicast group operations
     */
    bool setupMulticast();
    void cleanupMulticast();
    
    /**
     * @brief Update latency statistics
     */
    void updateLatencyStats(const std::chrono::steady_clock::time_point& receiveTime);
    
    /**
     * @brief Check if packet rate limiting should be applied
     */
    bool shouldDropForRateLimit();
    
    // Network configuration
    NetworkConfig m_networkConfig;
    
    // Socket and networking
    std::unique_ptr<QUdpSocket> m_socket;
    bool m_socketBound;
    bool m_multicastJoined;
    QNetworkInterface m_networkInterface;
    
    // Statistics and monitoring
    NetworkStatistics m_networkStats;
    std::unique_ptr<QTimer> m_statisticsTimer;
    
    // Threading - socket operations in main thread (Qt best practice)
    QThread* m_networkThread;
    
    // Packet processing
    std::chrono::steady_clock::time_point m_lastPacketTime;
    std::atomic<bool> m_pauseRequested;
    
    // Rate limiting
    std::atomic<uint32_t> m_packetsSinceLastCheck;
    std::chrono::steady_clock::time_point m_lastRateCheck;
    
    // Error handling
    std::atomic<uint32_t> m_consecutiveErrors;
    static constexpr uint32_t MAX_CONSECUTIVE_ERRORS = 10;
    
    // Performance tuning
    static constexpr int STATISTICS_UPDATE_INTERVAL = 1000; // 1 second
    static constexpr int DATAGRAM_QUEUE_SIZE = 1000;        // Ring buffer size
};

} // namespace Network
} // namespace Monitor