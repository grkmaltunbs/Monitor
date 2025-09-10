#pragma once

#include "../core/packet.h"
#include "../core/packet_factory.h"
#include "../../events/event_dispatcher.h"
#include "../../logging/logger.h"

#include <QtCore/QObject>
#include <QString>
#include <functional>
#include <memory>
#include <atomic>
#include <string>

namespace Monitor {
namespace Packet {

/**
 * @brief Abstract interface for packet sources
 * 
 * This interface defines the contract for all packet sources in the system,
 * including network sources, file sources, and simulation sources. It provides
 * event-driven packet delivery with comprehensive error handling and statistics.
 */
class PacketSource : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Source state enumeration
     */
    enum class State {
        Stopped,        ///< Source is stopped
        Starting,       ///< Source is starting up
        Running,        ///< Source is actively producing packets
        Pausing,        ///< Source is pausing
        Paused,         ///< Source is paused
        Stopping,       ///< Source is shutting down
        Error           ///< Source encountered an error
    };
    
    /**
     * @brief Source configuration
     */
    struct Configuration {
        std::string name;               ///< Source name for identification
        bool autoStart;         ///< Start automatically on creation
        uint32_t bufferSize;     ///< Internal buffer size (packets)
        uint32_t maxPacketRate;     ///< Maximum packets/second (0 = unlimited)
        bool enableStatistics;   ///< Enable performance statistics
        
        Configuration() 
            : autoStart(false)
            , bufferSize(1000)
            , maxPacketRate(0)
            , enableStatistics(true)
        {}
        Configuration(const std::string& sourceName) 
            : name(sourceName)
            , autoStart(false)
            , bufferSize(1000)
            , maxPacketRate(0)
            , enableStatistics(true) 
        {}
    };
    
    /**
     * @brief Source statistics
     */
    struct Statistics {
        std::atomic<uint64_t> packetsGenerated{0};
        std::atomic<uint64_t> packetsDelivered{0};
        std::atomic<uint64_t> packetsDropped{0};
        std::atomic<uint64_t> bytesGenerated{0};
        std::atomic<uint64_t> errorCount{0};
        
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point lastPacketTime;
        
        Statistics() : startTime(std::chrono::steady_clock::now()) {}
        
        // Copy constructor
        Statistics(const Statistics& other) : startTime(other.startTime), lastPacketTime(other.lastPacketTime) {
            packetsGenerated.store(other.packetsGenerated.load());
            packetsDelivered.store(other.packetsDelivered.load());
            packetsDropped.store(other.packetsDropped.load());
            bytesGenerated.store(other.bytesGenerated.load());
            errorCount.store(other.errorCount.load());
        }
        
        // Assignment operator
        Statistics& operator=(const Statistics& other) {
            if (this != &other) {
                packetsGenerated.store(other.packetsGenerated.load());
                packetsDelivered.store(other.packetsDelivered.load());
                packetsDropped.store(other.packetsDropped.load());
                bytesGenerated.store(other.bytesGenerated.load());
                errorCount.store(other.errorCount.load());
                startTime = other.startTime;
                lastPacketTime = other.lastPacketTime;
            }
            return *this;
        }
        
        double getPacketRate() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed == 0) return 0.0;
            return static_cast<double>(packetsDelivered.load()) / elapsed;
        }
        
        double getByteRate() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed == 0) return 0.0;
            return static_cast<double>(bytesGenerated.load()) / elapsed;
        }
        
        double getDropRate() const {
            uint64_t total = packetsGenerated.load();
            if (total == 0) return 0.0;
            return static_cast<double>(packetsDropped.load()) / total;
        }
    };
    
    /**
     * @brief Packet callback function type
     */
    using PacketCallback = std::function<void(PacketPtr packet)>;
    
    /**
     * @brief Error callback function type
     */
    using ErrorCallback = std::function<void(const std::string& error)>;

protected:
    Configuration m_config;
    State m_state;
    PacketFactory* m_packetFactory;
    Events::EventDispatcher* m_eventDispatcher;
    Logging::Logger* m_logger;
    
    Statistics m_stats;
    
    PacketCallback m_packetCallback;
    ErrorCallback m_errorCallback;

public:
    explicit PacketSource(const Configuration& config, QObject* parent = nullptr)
        : QObject(parent)
        , m_config(config)
        , m_state(State::Stopped)
        , m_packetFactory(nullptr)
        , m_eventDispatcher(nullptr)
        , m_logger(Logging::Logger::instance())
    {
    }
    
    virtual ~PacketSource() = default;
    
    /**
     * @brief Set packet factory for packet creation
     */
    void setPacketFactory(PacketFactory* factory) {
        m_packetFactory = factory;
    }
    
    /**
     * @brief Set event dispatcher for events
     */
    void setEventDispatcher(Events::EventDispatcher* dispatcher) {
        m_eventDispatcher = dispatcher;
    }
    
    /**
     * @brief Set packet callback
     */
    void setPacketCallback(const PacketCallback& callback) {
        m_packetCallback = callback;
    }
    
    /**
     * @brief Set error callback
     */
    void setErrorCallback(const ErrorCallback& callback) {
        m_errorCallback = callback;
    }
    
    /**
     * @brief Get current state
     */
    State getState() const { return m_state; }
    
    /**
     * @brief Get source name
     */
    const std::string& getName() const { return m_config.name; }
    
    /**
     * @brief Get configuration
     */
    const Configuration& getConfiguration() const { return m_config; }
    
    /**
     * @brief Get statistics
     */
    const Statistics& getStatistics() const { return m_stats; }
    
    /**
     * @brief Check if source is running
     */
    bool isRunning() const { return m_state == State::Running; }
    
    /**
     * @brief Check if source is stopped
     */
    bool isStopped() const { return m_state == State::Stopped; }
    
    /**
     * @brief Check if source has error
     */
    bool hasError() const { return m_state == State::Error; }

public slots:
    /**
     * @brief Start the packet source
     */
    virtual bool start() {
        if (m_state == State::Running) {
            return true;
        }
        
        if (m_state != State::Stopped && m_state != State::Paused) {
            m_logger->warning("PacketSource", 
                QString("Cannot start source %1 in state %2")
                .arg(QString::fromStdString(m_config.name)).arg(static_cast<int>(m_state)));
            return false;
        }
        
        setState(State::Starting);
        m_logger->info("PacketSource", 
            QString("Starting source: %1").arg(QString::fromStdString(m_config.name)));
        
        bool result = doStart();
        if (result) {
            setState(State::Running);
            m_stats.startTime = std::chrono::steady_clock::now();
            emit started();
        } else {
            setState(State::Error);
            emit error("Failed to start packet source");
        }
        
        return result;
    }
    
    /**
     * @brief Stop the packet source
     */
    virtual void stop() {
        if (m_state == State::Stopped) {
            return;
        }
        
        setState(State::Stopping);
        m_logger->info("PacketSource", 
            QString("Stopping source: %1").arg(QString::fromStdString(m_config.name)));
        
        doStop();
        setState(State::Stopped);
        emit stopped();
    }
    
    /**
     * @brief Pause the packet source
     */
    virtual void pause() {
        if (m_state != State::Running) {
            return;
        }
        
        setState(State::Pausing);
        m_logger->info("PacketSource", 
            QString("Pausing source: %1").arg(QString::fromStdString(m_config.name)));
        
        doPause();
        setState(State::Paused);
        emit paused();
    }
    
    /**
     * @brief Resume the packet source
     */
    virtual void resume() {
        if (m_state != State::Paused) {
            return;
        }
        
        m_logger->info("PacketSource", 
            QString("Resuming source: %1").arg(QString::fromStdString(m_config.name)));
        
        bool result = doResume();
        if (result) {
            setState(State::Running);
            emit resumed();
        } else {
            setState(State::Error);
            emit error("Failed to resume packet source");
        }
    }

signals:
    /**
     * @brief Emitted when source starts
     */
    void started();
    
    /**
     * @brief Emitted when source stops
     */
    void stopped();
    
    /**
     * @brief Emitted when source is paused
     */
    void paused();
    
    /**
     * @brief Emitted when source is resumed
     */
    void resumed();
    
    /**
     * @brief Emitted when a packet is available
     */
    void packetReady(PacketPtr packet);
    
    /**
     * @brief Emitted when an error occurs
     */
    void error(const QString& errorMessage);
    
    /**
     * @brief Emitted when state changes
     */
    void stateChanged(State oldState, State newState);
    
    /**
     * @brief Emitted periodically with statistics
     */
    void statisticsUpdated(const Statistics& stats);

protected:
    /**
     * @brief Derived classes must implement these methods
     */
    virtual bool doStart() = 0;
    virtual void doStop() = 0;
    virtual void doPause() = 0;
    virtual bool doResume() = 0;
    
    /**
     * @brief Set source state
     */
    void setState(State newState) {
        State oldState = m_state;
        m_state = newState;
        if (oldState != newState) {
            emit stateChanged(oldState, newState);
        }
    }
    
    /**
     * @brief Deliver packet to callbacks and emit signals
     */
    void deliverPacket(PacketPtr packet) {
        if (!packet) {
            m_stats.errorCount++;
            return;
        }
        
        m_stats.packetsDelivered++;
        m_stats.bytesGenerated += packet->totalSize();
        m_stats.lastPacketTime = std::chrono::steady_clock::now();
        
        // Call callback if set
        if (m_packetCallback) {
            m_packetCallback(packet);
        }
        
        // Emit signal
        emit packetReady(packet);
        
        // Emit statistics update periodically
        if (m_stats.packetsDelivered % 1000 == 0) {
            emit statisticsUpdated(m_stats);
        }
    }
    
    /**
     * @brief Report error to callbacks and emit signals
     */
    void reportError(const std::string& errorMessage) {
        m_stats.errorCount++;
        m_logger->error("PacketSource", 
            QString("Source %1 error: %2")
            .arg(QString::fromStdString(m_config.name))
            .arg(QString::fromStdString(errorMessage)));
        
        if (m_errorCallback) {
            m_errorCallback(errorMessage);
        }
        
        setState(State::Error);
        emit error(QString::fromStdString(errorMessage));
    }
    
    /**
     * @brief Check if packet rate limiting is enabled and should throttle
     */
    bool shouldThrottle() const {
        if (m_config.maxPacketRate == 0) {
            return false; // No rate limiting
        }
        
        double currentRate = m_stats.getPacketRate();
        return currentRate > m_config.maxPacketRate;
    }
};

/**
 * @brief Convert state to string for debugging
 */
inline const char* stateToString(PacketSource::State state) {
    switch (state) {
        case PacketSource::State::Stopped: return "Stopped";
        case PacketSource::State::Starting: return "Starting";
        case PacketSource::State::Running: return "Running";
        case PacketSource::State::Pausing: return "Pausing";
        case PacketSource::State::Paused: return "Paused";
        case PacketSource::State::Stopping: return "Stopping";
        case PacketSource::State::Error: return "Error";
        default: return "Unknown";
    }
}

} // namespace Packet
} // namespace Monitor