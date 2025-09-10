#pragma once

#include <QString>
#include <QHostAddress>
#include <QJsonObject>
#include <string>
#include <memory>

namespace Monitor {
namespace Network {

/**
 * @brief Network protocol enumeration
 */
enum class Protocol {
    UDP,    ///< User Datagram Protocol
    TCP     ///< Transmission Control Protocol
};

/**
 * @brief Network configuration for packet sources
 * 
 * Defines all parameters needed to establish network connections
 * for packet reception, including addressing, protocol settings,
 * and performance tuning options.
 */
struct NetworkConfig {
    // Connection parameters
    std::string name;                   ///< Configuration profile name
    Protocol protocol;                  ///< Network protocol (UDP/TCP)
    QHostAddress localAddress;         ///< Local interface address
    quint16 localPort;                 ///< Local port number
    QHostAddress remoteAddress;        ///< Remote device address
    quint16 remotePort;                ///< Remote device port
    QString networkInterface;          ///< Network interface name (optional)
    
    // Multicast settings (UDP only)
    bool enableMulticast;              ///< Enable multicast reception
    QHostAddress multicastGroup;       ///< Multicast group address
    int multicastTTL;                  ///< Time-to-live for multicast packets
    
    // Performance settings
    int receiveBufferSize;             ///< Socket receive buffer size (bytes)
    int socketTimeout;                 ///< Socket timeout (milliseconds)
    int maxPacketSize;                 ///< Maximum expected packet size
    bool enableTimestamping;           ///< Enable high-precision timestamping
    
    // Quality of Service
    int typeOfService;                 ///< IP Type of Service field
    int priority;                      ///< Socket priority
    
    // Connection settings (TCP only)
    bool enableKeepAlive;              ///< Enable TCP keep-alive
    int keepAliveInterval;             ///< Keep-alive interval (seconds)
    int connectionTimeout;             ///< Connection timeout (milliseconds)
    int maxReconnectAttempts;          ///< Maximum reconnection attempts
    int reconnectInterval;             ///< Reconnection interval (milliseconds)
    
    /**
     * @brief Default constructor with sensible defaults
     */
    NetworkConfig()
        : name("Default")
        , protocol(Protocol::UDP)
        , localAddress(QHostAddress::Any)
        , localPort(8080)
        , remoteAddress(QHostAddress::LocalHost)
        , remotePort(8081)
        , enableMulticast(false)
        , multicastGroup(QHostAddress("224.0.0.1"))
        , multicastTTL(1)
        , receiveBufferSize(1048576)  // 1MB
        , socketTimeout(1000)         // 1 second
        , maxPacketSize(65536)        // 64KB
        , enableTimestamping(true)
        , typeOfService(0)
        , priority(0)
        , enableKeepAlive(true)
        , keepAliveInterval(30)
        , connectionTimeout(5000)
        , maxReconnectAttempts(3)
        , reconnectInterval(1000)
    {}
    
    /**
     * @brief Named constructor for UDP configuration
     */
    static NetworkConfig createUdpConfig(const std::string& configName, 
                                        const QHostAddress& localAddr, 
                                        quint16 localP) {
        NetworkConfig config;
        config.name = configName;
        config.protocol = Protocol::UDP;
        config.localAddress = localAddr;
        config.localPort = localP;
        return config;
    }
    
    /**
     * @brief Named constructor for TCP configuration
     */
    static NetworkConfig createTcpConfig(const std::string& configName,
                                        const QHostAddress& remoteAddr,
                                        quint16 remoteP) {
        NetworkConfig config;
        config.name = configName;
        config.protocol = Protocol::TCP;
        config.remoteAddress = remoteAddr;
        config.remotePort = remoteP;
        return config;
    }
    
    /**
     * @brief Named constructor for multicast UDP configuration
     */
    static NetworkConfig createMulticastConfig(const std::string& configName,
                                              const QHostAddress& multicastAddr,
                                              quint16 port) {
        NetworkConfig config;
        config.name = configName;
        config.protocol = Protocol::UDP;
        config.enableMulticast = true;
        config.multicastGroup = multicastAddr;
        config.localPort = port;
        config.remotePort = port;
        return config;
    }
    
    /**
     * @brief Validate configuration parameters
     */
    bool isValid() const {
        // Check port ranges
        if (localPort == 0 && protocol == Protocol::UDP) return false;
        if (remotePort == 0 && protocol == Protocol::TCP) return false;
        
        // Check multicast settings
        if (enableMulticast) {
            if (!multicastGroup.isInSubnet(QHostAddress("224.0.0.0"), 4)) {
                return false; // Invalid multicast address
            }
        }
        
        // Check buffer sizes
        if (receiveBufferSize < 1024 || receiveBufferSize > 67108864) { // 1KB to 64MB
            return false;
        }
        
        if (maxPacketSize < 64 || maxPacketSize > 65536) { // 64B to 64KB
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Get protocol as string
     */
    QString getProtocolString() const {
        return (protocol == Protocol::UDP) ? "UDP" : "TCP";
    }
    
    /**
     * @brief Serialize to JSON
     */
    QJsonObject toJson() const;
    
    /**
     * @brief Deserialize from JSON
     */
    bool fromJson(const QJsonObject& json);
    
    /**
     * @brief Get connection string for display
     */
    QString getConnectionString() const {
        return QString("%1://%2:%3")
            .arg(getProtocolString().toLower())
            .arg(remoteAddress.toString())
            .arg(remotePort);
    }
};

/**
 * @brief Network statistics for monitoring
 */
struct NetworkStatistics {
    // Packet statistics
    std::atomic<uint64_t> packetsReceived{0};
    std::atomic<uint64_t> bytesReceived{0};
    std::atomic<uint64_t> packetsDropped{0};
    std::atomic<uint64_t> packetErrors{0};
    
    // Network statistics
    std::atomic<uint64_t> socketErrors{0};
    std::atomic<uint64_t> reconnections{0};
    std::atomic<uint32_t> connectionDrops{0};
    
    // Performance statistics
    std::atomic<double> averageLatency{0.0};
    std::atomic<double> packetRate{0.0};
    std::atomic<double> byteRate{0.0};
    
    // Timing
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastPacketTime;
    
    NetworkStatistics() 
        : startTime(std::chrono::steady_clock::now()) {}
    
    /**
     * @brief Calculate current packet rate (packets/second)
     */
    double getCurrentPacketRate() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
        if (elapsed.count() == 0) return 0.0;
        return static_cast<double>(packetsReceived.load()) / elapsed.count();
    }
    
    /**
     * @brief Calculate current byte rate (bytes/second)
     */
    double getCurrentByteRate() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
        if (elapsed.count() == 0) return 0.0;
        return static_cast<double>(bytesReceived.load()) / elapsed.count();
    }
    
    /**
     * @brief Calculate packet drop rate (percentage)
     */
    double getDropRate() const {
        uint64_t total = packetsReceived.load() + packetsDropped.load();
        if (total == 0) return 0.0;
        return (static_cast<double>(packetsDropped.load()) / total) * 100.0;
    }
    
    /**
     * @brief Reset all statistics
     */
    void reset() {
        packetsReceived = 0;
        bytesReceived = 0;
        packetsDropped = 0;
        packetErrors = 0;
        socketErrors = 0;
        reconnections = 0;
        connectionDrops = 0;
        averageLatency = 0.0;
        packetRate = 0.0;
        byteRate = 0.0;
        startTime = std::chrono::steady_clock::now();
    }
};

/**
 * @brief Convert protocol enum to string
 */
inline QString protocolToString(Protocol protocol) {
    switch (protocol) {
        case Protocol::UDP: return "UDP";
        case Protocol::TCP: return "TCP";
        default: return "Unknown";
    }
}

/**
 * @brief Convert string to protocol enum
 */
inline Protocol stringToProtocol(const QString& protocolStr) {
    if (protocolStr.toUpper() == "TCP") {
        return Protocol::TCP;
    }
    return Protocol::UDP; // Default to UDP
}

} // namespace Network
} // namespace Monitor