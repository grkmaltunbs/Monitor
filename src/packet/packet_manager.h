#pragma once

#include "core/packet_factory.h"
#include "sources/packet_source.h"
#include "sources/simulation_source.h"
#include "sources/memory_source.h"
#include "routing/packet_dispatcher.h"
#include "processing/packet_processor.h"
#include "../parser/manager/structure_manager.h"
#include "../threading/thread_manager.h"
#include "../events/event_dispatcher.h"
#include "../memory/memory_pool.h"
#include "../logging/logger.h"

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QString>
#include <memory>
#include <vector>
#include <unordered_map>

namespace Monitor {
namespace Packet {

/**
 * @brief High-level packet management system with Qt integration
 * 
 * This class provides a unified interface to the entire packet processing
 * system, integrating all components and providing Qt signals/slots for
 * UI interaction. It serves as the main API for the Monitor Application.
 */
class PacketManager : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief System configuration
     */
    struct Configuration {
        PacketDispatcher::Configuration dispatcherConfig;
        PacketProcessor::Configuration processorConfig;
        
        // Integration settings
        bool autoStart;                 ///< Start system automatically
        uint32_t statisticsUpdateIntervalMs; ///< Statistics update interval
        bool enablePerformanceMonitoring;    ///< Enable performance monitoring
        
        Configuration() 
            : autoStart(false)
            , statisticsUpdateIntervalMs(1000)
            , enablePerformanceMonitoring(true)
        {}
    };
    
    /**
     * @brief System state
     */
    enum class State {
        Uninitialized,
        Initializing,
        Ready,
        Starting,
        Running,
        Stopping,
        Error
    };
    
    /**
     * @brief System statistics combining all components
     */
    struct SystemStatistics {
        // Dispatcher statistics
        PacketDispatcher::Statistics dispatcherStats;
        
        // Processor statistics
        PacketProcessor::Statistics processorStats;
        
        // Source statistics
        std::unordered_map<std::string, PacketSource::Statistics> sourceStats;
        
        // Overall system metrics
        double totalThroughput = 0.0;           ///< Total packets/second
        double totalLatency = 0.0;              ///< Average end-to-end latency (ms)
        uint64_t totalMemoryUsage = 0;          ///< Total memory usage (bytes)
        
        std::chrono::steady_clock::time_point lastUpdate;
        
        SystemStatistics() : lastUpdate(std::chrono::steady_clock::now()) {}
    };

private:
    Configuration m_config;
    State m_state;
    
    // Core components
    std::unique_ptr<PacketFactory> m_packetFactory;
    std::unique_ptr<PacketDispatcher> m_packetDispatcher;
    std::unique_ptr<PacketProcessor> m_packetProcessor;
    
    // External dependencies
    Parser::StructureManager* m_structureManager;
    Threading::ThreadManager* m_threadManager;
    Events::EventDispatcher* m_eventDispatcher;
    Memory::MemoryPoolManager* m_memoryManager;
    Logging::Logger* m_logger;
    
    // Source management
    std::unordered_map<std::string, std::unique_ptr<PacketSource>> m_sources;
    
    // Statistics and monitoring
    SystemStatistics m_systemStats;
    std::unique_ptr<QTimer> m_statisticsTimer;
    
    // Error tracking
    mutable std::vector<std::string> m_errors;
    mutable std::chrono::steady_clock::time_point m_lastError;

public:
    explicit PacketManager(const Configuration& config = Configuration(), QObject* parent = nullptr)
        : QObject(parent)
        , m_config(config)
        , m_state(State::Uninitialized)
        , m_structureManager(nullptr)
        , m_threadManager(nullptr)
        , m_eventDispatcher(nullptr)
        , m_memoryManager(nullptr)
        , m_logger(Logging::Logger::instance())
    {
        // Create statistics timer
        m_statisticsTimer = std::make_unique<QTimer>(this);
        m_statisticsTimer->setInterval(static_cast<int>(m_config.statisticsUpdateIntervalMs));
        connect(m_statisticsTimer.get(), &QTimer::timeout, this, &PacketManager::updateStatistics);
    }
    
    /**
     * @brief Initialize the packet manager with external dependencies
     */
    bool initialize(Parser::StructureManager* structureManager,
                   Threading::ThreadManager* threadManager,
                   Events::EventDispatcher* eventDispatcher,
                   Memory::MemoryPoolManager* memoryManager) {
        
        if (m_state != State::Uninitialized) {
            m_logger->warning("PacketManager", "Already initialized");
            return true;
        }
        
        setState(State::Initializing);
        
        // Store external dependencies
        m_structureManager = structureManager;
        m_threadManager = threadManager;
        m_eventDispatcher = eventDispatcher;
        m_memoryManager = memoryManager;
        
        if (!validateDependencies()) {
            setState(State::Error);
            return false;
        }
        
        try {
            // Initialize components in dependency order
            if (!initializePacketFactory()) {
                setState(State::Error);
                return false;
            }
            
            if (!initializePacketProcessor()) {
                setState(State::Error);
                return false;
            }
            
            if (!initializePacketDispatcher()) {
                setState(State::Error);
                return false;
            }
            
            // Create default simulation source
            createDefaultSimulationSource();
            
            setState(State::Ready);
            
            m_logger->info("PacketManager", "Packet manager initialized successfully");
            
            emit initialized();
            
            // Auto-start if configured
            if (m_config.autoStart) {
                return start();
            }
            
            return true;
            
        } catch (const std::exception& e) {
            m_logger->error("PacketManager", QString("Initialization failed: %1").arg(QString::fromStdString(e.what())));
            addError("Initialization failed: " + std::string(e.what()));
            setState(State::Error);
            return false;
        }
    }
    
    /**
     * @brief Start the packet processing system
     */
    bool start() {
        if (m_state != State::Ready && m_state != State::Running) {
            m_logger->error("PacketManager", QString("Cannot start from state %1").arg(static_cast<int>(m_state)));
            return false;
        }
        
        if (m_state == State::Running) {
            m_logger->info("PacketManager", "Already running");
            return true;
        }
        
        setState(State::Starting);
        
        m_logger->info("PacketManager", "Starting packet manager");
        
        try {
            // Start packet dispatcher
            if (!m_packetDispatcher->start()) {
                m_logger->error("PacketManager", "Failed to start packet dispatcher");
                setState(State::Error);
                return false;
            }
            
            // Start statistics timer
            m_statisticsTimer->start();
            
            setState(State::Running);
            
            m_logger->info("PacketManager", "Packet manager started successfully");
            
            emit started();
            
            return true;
            
        } catch (const std::exception& e) {
            m_logger->error("PacketManager", QString("Start failed: %1").arg(QString::fromStdString(e.what())));
            addError("Start failed: " + std::string(e.what()));
            setState(State::Error);
            return false;
        }
    }
    
    /**
     * @brief Stop the packet processing system
     */
    void stop() {
        if (m_state != State::Running) {
            return;
        }
        
        setState(State::Stopping);
        
        m_logger->info("PacketManager", "Stopping packet manager");
        
        // Stop statistics timer
        m_statisticsTimer->stop();
        
        // Stop packet dispatcher
        if (m_packetDispatcher) {
            m_packetDispatcher->stop();
        }
        
        setState(State::Ready);
        
        m_logger->info("PacketManager", "Packet manager stopped");
        
        emit stopped();
    }
    
    /**
     * @brief Create simulation source
     */
    bool createSimulationSource(const std::string& name, const SimulationSource::SimulationConfig& config) {
        if (m_sources.find(name) != m_sources.end()) {
            m_logger->error("PacketManager", QString("Source '%1' already exists").arg(QString::fromStdString(name)));
            return false;
        }
        
        auto source = std::make_unique<SimulationSource>(config);
        source->setPacketFactory(m_packetFactory.get());
        source->setEventDispatcher(m_eventDispatcher);
        
        // Register with dispatcher
        if (m_packetDispatcher && !m_packetDispatcher->registerSource(source.get())) {
            m_logger->error("PacketManager", QString("Failed to register simulation source '%1'").arg(QString::fromStdString(name)));
            return false;
        }
        
        m_sources[name] = std::move(source);
        
        m_logger->info("PacketManager", QString("Created simulation source: %1").arg(QString::fromStdString(name)));
        
        emit sourceAdded(QString::fromStdString(name), "Simulation");
        
        return true;
    }
    
    /**
     * @brief Create memory source
     */
    bool createMemorySource(const std::string& name, const MemorySource::MemoryConfig& config) {
        if (m_sources.find(name) != m_sources.end()) {
            m_logger->error("PacketManager", QString("Source '%1' already exists").arg(QString::fromStdString(name)));
            return false;
        }
        
        auto source = std::make_unique<MemorySource>(config);
        source->setPacketFactory(m_packetFactory.get());
        source->setEventDispatcher(m_eventDispatcher);
        
        // Register with dispatcher
        if (m_packetDispatcher && !m_packetDispatcher->registerSource(source.get())) {
            m_logger->error("PacketManager", QString("Failed to register memory source '%1'").arg(QString::fromStdString(name)));
            return false;
        }
        
        m_sources[name] = std::move(source);
        
        m_logger->info("PacketManager", QString("Created memory source: %1").arg(QString::fromStdString(name)));
        
        emit sourceAdded(QString::fromStdString(name), "Memory");
        
        return true;
    }
    
    /**
     * @brief Remove source
     */
    bool removeSource(const std::string& name) {
        auto it = m_sources.find(name);
        if (it == m_sources.end()) {
            m_logger->warning("PacketManager", QString("Source '%1' not found").arg(QString::fromStdString(name)));
            return false;
        }
        
        // Unregister from dispatcher
        if (m_packetDispatcher) {
            m_packetDispatcher->unregisterSource(name);
        }
        
        m_sources.erase(it);
        
        m_logger->info("PacketManager", QString("Removed source: %1").arg(QString::fromStdString(name)));
        
        emit sourceRemoved(QString::fromStdString(name));
        
        return true;
    }
    
    /**
     * @brief Subscribe to packet type
     */
    SubscriptionManager::SubscriberId subscribe(const std::string& subscriberName,
                                               PacketId packetId,
                                               SubscriptionManager::PacketCallback callback,
                                               uint32_t priority = 0) {
        if (!m_packetDispatcher) {
            return 0;
        }
        
        return m_packetDispatcher->subscribe(subscriberName, packetId, callback, priority);
    }
    
    /**
     * @brief Unsubscribe from packets
     */
    bool unsubscribe(SubscriptionManager::SubscriberId id) {
        if (!m_packetDispatcher) {
            return false;
        }
        
        return m_packetDispatcher->unsubscribe(id);
    }
    
    /**
     * @brief Get packet factory
     */
    PacketFactory* getPacketFactory() const {
        return m_packetFactory.get();
    }
    
    /**
     * @brief Get packet processor
     */
    PacketProcessor* getPacketProcessor() const {
        return m_packetProcessor.get();
    }
    
    /**
     * @brief Get packet dispatcher
     */
    PacketDispatcher* getPacketDispatcher() const {
        return m_packetDispatcher.get();
    }
    
    /**
     * @brief Get current state
     */
    State getState() const {
        return m_state;
    }
    
    /**
     * @brief Check if system is running
     */
    bool isRunning() const {
        return m_state == State::Running;
    }
    
    /**
     * @brief Get system statistics
     */
    const SystemStatistics& getSystemStatistics() const {
        return m_systemStats;
    }
    
    /**
     * @brief Get source names
     */
    std::vector<std::string> getSourceNames() const {
        std::vector<std::string> names;
        names.reserve(m_sources.size());
        
        for (const auto& pair : m_sources) {
            names.push_back(pair.first);
        }
        
        return names;
    }
    
    /**
     * @brief Get source by name
     */
    PacketSource* getSource(const std::string& name) const {
        auto it = m_sources.find(name);
        return (it != m_sources.end()) ? it->second.get() : nullptr;
    }
    
    /**
     * @brief Get recent errors
     */
    const std::vector<std::string>& getErrors() const {
        return m_errors;
    }
    
    /**
     * @brief Clear errors
     */
    void clearErrors() {
        m_errors.clear();
    }

signals:
    /**
     * @brief Emitted when manager is initialized
     */
    void initialized();
    
    /**
     * @brief Emitted when manager starts
     */
    void started();
    
    /**
     * @brief Emitted when manager stops
     */
    void stopped();
    
    /**
     * @brief Emitted when state changes
     */
    void stateChanged(State oldState, State newState);
    
    /**
     * @brief Emitted when source is added
     */
    void sourceAdded(const QString& name, const QString& type);
    
    /**
     * @brief Emitted when source is removed
     */
    void sourceRemoved(const QString& name);
    
    /**
     * @brief Emitted when statistics are updated
     */
    void statisticsUpdated(const SystemStatistics& stats);
    
    /**
     * @brief Emitted when error occurs
     */
    void errorOccurred(const QString& error);

private slots:
    /**
     * @brief Update system statistics
     */
    void updateStatistics() {
        if (m_state != State::Running) {
            return;
        }
        
        // Collect statistics from all components
        if (m_packetDispatcher) {
            m_systemStats.dispatcherStats = m_packetDispatcher->getStatistics();
        }
        
        if (m_packetProcessor) {
            m_systemStats.processorStats = m_packetProcessor->getStatistics();
        }
        
        // Collect source statistics
        m_systemStats.sourceStats.clear();
        for (const auto& pair : m_sources) {
            m_systemStats.sourceStats[pair.first] = pair.second->getStatistics();
        }
        
        // Calculate overall metrics
        m_systemStats.totalThroughput = m_systemStats.dispatcherStats.getTotalThroughput();
        m_systemStats.totalLatency = static_cast<double>(m_systemStats.processorStats.averageProcessingTimeNs.load()) / 1e6; // Convert to ms
        
        if (m_memoryManager) {
            m_systemStats.totalMemoryUsage = m_memoryManager->getTotalMemoryUsed();
        }
        
        m_systemStats.lastUpdate = std::chrono::steady_clock::now();
        
        emit statisticsUpdated(m_systemStats);
    }

private:
    /**
     * @brief Set system state
     */
    void setState(State newState) {
        State oldState = m_state;
        m_state = newState;
        
        if (oldState != newState) {
            m_logger->debug("PacketManager", QString("State changed: %1 -> %2").arg(static_cast<int>(oldState)).arg(static_cast<int>(newState)));
            emit stateChanged(oldState, newState);
        }
    }
    
    /**
     * @brief Validate external dependencies
     */
    bool validateDependencies() const {
        if (!m_structureManager) {
            addError("Structure manager is required");
            return false;
        }
        
        if (!m_memoryManager) {
            addError("Memory manager is required");
            return false;
        }
        
        if (!m_logger) {
            addError("Logger is required");
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Initialize packet factory
     */
    bool initializePacketFactory() {
        m_packetFactory = std::make_unique<PacketFactory>(m_memoryManager);
        m_packetFactory->setStructureManager(m_structureManager);
        m_packetFactory->setEventDispatcher(m_eventDispatcher);
        
        connect(m_packetFactory.get(), &PacketFactory::packetCreated,
                this, [](PacketPtr /*packet*/) {
                    // Could emit signal for packet creation monitoring
                });
        
        m_logger->debug("PacketManager", "Packet factory initialized");
        return true;
    }
    
    /**
     * @brief Initialize packet processor
     */
    bool initializePacketProcessor() {
        m_packetProcessor = std::make_unique<PacketProcessor>(m_config.processorConfig);
        
        Threading::ThreadPool* threadPool = m_threadManager ? m_threadManager->getDefaultThreadPool() : nullptr;
        
        if (!m_packetProcessor->initialize(m_structureManager, threadPool, m_eventDispatcher)) {
            addError("Failed to initialize packet processor");
            return false;
        }
        
        connect(m_packetProcessor.get(), &PacketProcessor::processingFailed,
                this, [this](PacketPtr packet, const QString& error) {
                    Q_UNUSED(packet);
                    addError("Processing failed: " + error.toStdString());
                });
        
        m_logger->debug("PacketManager", "Packet processor initialized");
        return true;
    }
    
    /**
     * @brief Initialize packet dispatcher
     */
    bool initializePacketDispatcher() {
        m_packetDispatcher = std::make_unique<PacketDispatcher>(m_config.dispatcherConfig);
        
        Threading::ThreadPool* threadPool = m_threadManager ? m_threadManager->getDefaultThreadPool() : nullptr;
        m_packetDispatcher->setThreadPool(threadPool);
        m_packetDispatcher->setEventDispatcher(m_eventDispatcher);
        
        // Connect packet processing
        connect(m_packetDispatcher.get(), &PacketDispatcher::packetProcessed,
                this, [this](PacketPtr packet) {
                    if (m_packetProcessor) {
                        m_packetProcessor->processPacket(packet);
                    }
                });
        
        m_logger->debug("PacketManager", "Packet dispatcher initialized");
        return true;
    }
    
    /**
     * @brief Create default simulation source
     */
    void createDefaultSimulationSource() {
        SimulationSource::SimulationConfig config("DefaultSimulation");
        
        // Add some default packet types for testing
        config.packetTypes = {
            {1001, "TestSignal", 64, 100, SimulationSource::PatternType::Sine},
            {1002, "TestMotion", 128, 50, SimulationSource::PatternType::Random},
            {1003, "TestStatus", 32, 200, SimulationSource::PatternType::Counter}
        };
        
        createSimulationSource("DefaultSimulation", config);
    }
    
    /**
     * @brief Add error message
     */
    void addError(const std::string& error) const {
        m_errors.push_back(error);
        m_lastError = std::chrono::steady_clock::now();
        
        // Limit error history
        const size_t maxErrors = 100;
        if (m_errors.size() > maxErrors) {
            m_errors.erase(m_errors.begin());
        }
        
        emit const_cast<PacketManager*>(this)->errorOccurred(QString::fromStdString(error));
    }
};

/**
 * @brief Convert state to string for debugging
 */
inline const char* stateToString(PacketManager::State state) {
    switch (state) {
        case PacketManager::State::Uninitialized: return "Uninitialized";
        case PacketManager::State::Initializing: return "Initializing";
        case PacketManager::State::Ready: return "Ready";
        case PacketManager::State::Starting: return "Starting";
        case PacketManager::State::Running: return "Running";
        case PacketManager::State::Stopping: return "Stopping";
        case PacketManager::State::Error: return "Error";
        default: return "Unknown";
    }
}

} // namespace Packet
} // namespace Monitor