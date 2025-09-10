#pragma once

#include "../core/packet.h"
#include "../sources/packet_source.h"
#include "packet_router.h"
#include "subscription_manager.h"
#include "../../threading/thread_pool.h"
#include "../../events/event_dispatcher.h"
#include "../../logging/logger.h"

#include <QtCore/QObject>
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>

namespace Monitor {
namespace Packet {

/**
 * @brief Central packet dispatcher coordinating the entire packet flow
 * 
 * This class serves as the main orchestrator for the packet processing
 * system, coordinating packet sources, routing, and subscription management.
 * It provides a unified interface for the packet processing pipeline.
 */
class PacketDispatcher : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Dispatcher configuration
     */
    struct Configuration {
        PacketRouter::Configuration routerConfig;
        bool enableBackPressure;        ///< Enable back-pressure handling
        uint32_t backPressureThreshold; ///< Queue threshold for back-pressure
        uint32_t maxSources;             ///< Maximum number of packet sources
        bool enableMetrics;             ///< Enable detailed metrics collection
        
        Configuration() 
            : enableBackPressure(true)
            , backPressureThreshold(8000)
            , maxSources(100)
            , enableMetrics(true)
        {}
    };
    
    /**
     * @brief Dispatcher statistics
     */
    struct Statistics {
        std::atomic<uint64_t> totalPacketsReceived{0};
        std::atomic<uint64_t> totalPacketsProcessed{0};
        std::atomic<uint64_t> totalPacketsDropped{0};
        std::atomic<uint64_t> backPressureEvents{0};
        std::atomic<uint64_t> sourceCount{0};
        std::atomic<uint64_t> subscriberCount{0};
        
        std::chrono::steady_clock::time_point startTime;
        
        Statistics() : startTime(std::chrono::steady_clock::now()) {}
        
        // Copy constructor
        Statistics(const Statistics& other) : startTime(other.startTime) {
            totalPacketsReceived.store(other.totalPacketsReceived.load());
            totalPacketsProcessed.store(other.totalPacketsProcessed.load());
            totalPacketsDropped.store(other.totalPacketsDropped.load());
            backPressureEvents.store(other.backPressureEvents.load());
            sourceCount.store(other.sourceCount.load());
            subscriberCount.store(other.subscriberCount.load());
        }
        
        // Assignment operator
        Statistics& operator=(const Statistics& other) {
            if (this != &other) {
                totalPacketsReceived.store(other.totalPacketsReceived.load());
                totalPacketsProcessed.store(other.totalPacketsProcessed.load());
                totalPacketsDropped.store(other.totalPacketsDropped.load());
                backPressureEvents.store(other.backPressureEvents.load());
                sourceCount.store(other.sourceCount.load());
                subscriberCount.store(other.subscriberCount.load());
                startTime = other.startTime;
            }
            return *this;
        }
        
        double getTotalThroughput() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed == 0) return 0.0;
            return static_cast<double>(totalPacketsProcessed.load()) / elapsed;
        }
        
        double getOverallDropRate() const {
            uint64_t total = totalPacketsReceived.load();
            if (total == 0) return 0.0;
            return static_cast<double>(totalPacketsDropped.load()) / total;
        }
    };
    
    /**
     * @brief Source registration information
     */
    struct SourceRegistration {
        std::string name;
        PacketSource* source;
        bool enabled;
        std::chrono::steady_clock::time_point registeredAt;
        
        SourceRegistration(const std::string& sourceName, PacketSource* src)
            : name(sourceName), source(src), enabled(true), 
              registeredAt(std::chrono::steady_clock::now())
        {
        }
    };

private:
    Configuration m_config;
    
    // Core components
    std::unique_ptr<SubscriptionManager> m_subscriptionManager;
    std::unique_ptr<PacketRouter> m_router;
    Threading::ThreadPool* m_threadPool;
    Events::EventDispatcher* m_eventDispatcher;
    Logging::Logger* m_logger;
    
    // Source management
    std::vector<SourceRegistration> m_registeredSources;
    std::unordered_map<std::string, size_t> m_sourceIndex;
    
    // State management
    std::atomic<bool> m_running{false};
    Statistics m_stats;

public:
    explicit PacketDispatcher(const Configuration& config, QObject* parent = nullptr)
        : QObject(parent)
        , m_config(config)
        , m_threadPool(nullptr)
        , m_eventDispatcher(nullptr)
        , m_logger(Logging::Logger::instance())
    {
        // Create subscription manager
        m_subscriptionManager = std::make_unique<SubscriptionManager>(this);
        
        // Create router with subscription manager
        m_router = std::make_unique<PacketRouter>(m_config.routerConfig, this);
        m_router->setSubscriptionManager(m_subscriptionManager.get());
        
        // Connect router signals
        connect(m_router.get(), &PacketRouter::packetRouted,
                this, &PacketDispatcher::onPacketRouted);
        connect(m_router.get(), &PacketRouter::packetDropped,
                this, &PacketDispatcher::onPacketDropped);
        
        // Connect subscription manager signals
        connect(m_subscriptionManager.get(), &SubscriptionManager::subscriptionAdded,
                this, &PacketDispatcher::subscriptionAdded);
        connect(m_subscriptionManager.get(), &SubscriptionManager::subscriptionRemoved,
                this, &PacketDispatcher::subscriptionRemoved);
    }
    
    ~PacketDispatcher() {
        stop();
    }
    
    /**
     * @brief Set thread pool for parallel processing
     */
    void setThreadPool(Threading::ThreadPool* threadPool) {
        m_threadPool = threadPool;
        if (m_router) {
            m_router->setThreadPool(threadPool);
        }
    }
    
    /**
     * @brief Set event dispatcher
     */
    void setEventDispatcher(Events::EventDispatcher* dispatcher) {
        m_eventDispatcher = dispatcher;
        if (m_router) {
            m_router->setEventDispatcher(dispatcher);
        }
    }
    
    /**
     * @brief Start the dispatcher
     */
    bool start() {
        if (m_running.load()) {
            return true;
        }
        
        m_logger->info("PacketDispatcher", "Starting packet dispatcher");
        
        // Start the router
        if (!m_router->start()) {
            m_logger->error("PacketDispatcher", "Failed to start packet router");
            return false;
        }
        
        m_running.store(true);
        m_stats.startTime = std::chrono::steady_clock::now();
        
        // Start all registered sources
        for (auto& registration : m_registeredSources) {
            if (registration.enabled && registration.source) {
                if (!registration.source->start()) {
                    m_logger->warning("PacketDispatcher", 
                        QString("Failed to start source: %1").arg(QString::fromStdString(registration.name)));
                }
            }
        }
        
        emit started();
        
        return true;
    }
    
    /**
     * @brief Stop the dispatcher
     */
    void stop() {
        if (!m_running.load()) {
            return;
        }
        
        m_logger->info("PacketDispatcher", "Stopping packet dispatcher");
        
        // Stop all sources first
        for (auto& registration : m_registeredSources) {
            if (registration.source) {
                registration.source->stop();
            }
        }
        
        // Stop the router
        if (m_router) {
            m_router->stop();
        }
        
        m_running.store(false);
        
        emit stopped();
    }
    
    /**
     * @brief Register packet source
     */
    bool registerSource(PacketSource* source) {
        if (!source) {
            m_logger->error("PacketDispatcher", "Cannot register null packet source");
            return false;
        }
        
        const std::string& name = source->getName();
        if (name.empty()) {
            m_logger->error("PacketDispatcher", "Packet source must have a name");
            return false;
        }
        
        // Check if source already registered
        if (m_sourceIndex.find(name) != m_sourceIndex.end()) {
            m_logger->warning("PacketDispatcher", 
                QString("Source '%1' already registered").arg(QString::fromStdString(name)));
            return false;
        }
        
        if (m_registeredSources.size() >= m_config.maxSources) {
            m_logger->error("PacketDispatcher", 
                QString("Maximum number of sources (%1) reached").arg(m_config.maxSources));
            return false;
        }
        
        // Register source
        size_t index = m_registeredSources.size();
        m_registeredSources.emplace_back(name, source);
        m_sourceIndex[name] = index;
        
        // Connect source signals to dispatcher
        connect(source, &PacketSource::packetReady,
                this, &PacketDispatcher::onPacketReceived);
        connect(source, &PacketSource::error,
                this, &PacketDispatcher::onSourceError);
        
        m_stats.sourceCount++;
        
        m_logger->info("PacketDispatcher", 
            QString("Registered packet source: %1").arg(QString::fromStdString(name)));
        
        emit sourceRegistered(QString::fromStdString(name));
        
        // Start source if dispatcher is running
        if (m_running.load()) {
            source->start();
        }
        
        return true;
    }
    
    /**
     * @brief Unregister packet source
     */
    bool unregisterSource(const std::string& name) {
        auto it = m_sourceIndex.find(name);
        if (it == m_sourceIndex.end()) {
            m_logger->warning("PacketDispatcher", 
                QString("Source '%1' not found").arg(QString::fromStdString(name)));
            return false;
        }
        
        size_t index = it->second;
        if (index >= m_registeredSources.size()) {
            return false;
        }
        
        auto& registration = m_registeredSources[index];
        
        // Stop and disconnect source
        if (registration.source) {
            registration.source->stop();
            registration.source->disconnect(this);
        }
        
        // Remove from containers (swap with last element for efficiency)
        if (index < m_registeredSources.size() - 1) {
            std::swap(m_registeredSources[index], m_registeredSources.back());
            // Update index for swapped element
            m_sourceIndex[m_registeredSources[index].name] = index;
        }
        
        m_registeredSources.pop_back();
        m_sourceIndex.erase(it);
        
        m_stats.sourceCount--;
        
        m_logger->info("PacketDispatcher", 
            QString("Unregistered packet source: %1").arg(QString::fromStdString(name)));
        
        emit sourceUnregistered(QString::fromStdString(name));
        
        return true;
    }
    
    /**
     * @brief Subscribe to packet type
     */
    SubscriptionManager::SubscriberId subscribe(const std::string& subscriberName, 
                                               PacketId packetId,
                                               SubscriptionManager::PacketCallback callback,
                                               uint32_t priority = 0) {
        auto id = m_subscriptionManager->subscribe(subscriberName, packetId, callback, priority);
        if (id != 0) {
            m_stats.subscriberCount++;
        }
        return id;
    }
    
    /**
     * @brief Unsubscribe from packets
     */
    bool unsubscribe(SubscriptionManager::SubscriberId id) {
        if (m_subscriptionManager->unsubscribe(id)) {
            m_stats.subscriberCount--;
            return true;
        }
        return false;
    }
    
    /**
     * @brief Get subscription manager
     */
    SubscriptionManager* getSubscriptionManager() const {
        return m_subscriptionManager.get();
    }
    
    /**
     * @brief Get packet router
     */
    PacketRouter* getPacketRouter() const {
        return m_router.get();
    }
    
    /**
     * @brief Get registered sources
     */
    const std::vector<SourceRegistration>& getRegisteredSources() const {
        return m_registeredSources;
    }
    
    /**
     * @brief Get dispatcher statistics
     */
    const Statistics& getStatistics() const {
        return m_stats;
    }
    
    /**
     * @brief Check if dispatcher is running
     */
    bool isRunning() const {
        return m_running.load();
    }
    
    /**
     * @brief Enable/disable source
     */
    bool enableSource(const std::string& name, bool enabled) {
        auto it = m_sourceIndex.find(name);
        if (it == m_sourceIndex.end()) {
            return false;
        }
        
        auto& registration = m_registeredSources[it->second];
        registration.enabled = enabled;
        
        if (registration.source) {
            if (enabled) {
                registration.source->start();
            } else {
                registration.source->stop();
            }
        }
        
        return true;
    }

signals:
    /**
     * @brief Emitted when dispatcher starts
     */
    void started();
    
    /**
     * @brief Emitted when dispatcher stops
     */
    void stopped();
    
    /**
     * @brief Emitted when source is registered
     */
    void sourceRegistered(const QString& name);
    
    /**
     * @brief Emitted when source is unregistered
     */
    void sourceUnregistered(const QString& name);
    
    /**
     * @brief Forwarded from subscription manager
     */
    void subscriptionAdded(SubscriptionManager::SubscriberId id, const QString& name, PacketId packetId);
    void subscriptionRemoved(SubscriptionManager::SubscriberId id, const QString& name, PacketId packetId);
    
    /**
     * @brief Emitted when packet is processed
     */
    void packetProcessed(PacketPtr packet);
    
    /**
     * @brief Emitted when back-pressure occurs
     */
    void backPressureDetected(const QString& reason);
    
    /**
     * @brief Emitted when statistics are updated
     */
    void statisticsUpdated(const Statistics& stats);

private slots:
    /**
     * @brief Handle packet from source
     */
    void onPacketReceived(PacketPtr packet) {
        if (!packet || !packet->isValid()) {
            m_stats.totalPacketsDropped++;
            return;
        }
        
        m_stats.totalPacketsReceived++;
        
        // Check back-pressure
        if (m_config.enableBackPressure) {
            if (checkBackPressure()) {
                m_logger->warning("PacketDispatcher", 
                    QString("Back-pressure detected, dropping packet ID %1").arg(packet->id()));
                m_stats.totalPacketsDropped++;
                m_stats.backPressureEvents++;
                emit backPressureDetected("Queue overflow");
                return;
            }
        }
        
        // Route packet
        if (m_router->routePacketAuto(packet)) {
            m_stats.totalPacketsProcessed++;
        } else {
            m_stats.totalPacketsDropped++;
        }
        
        // Emit statistics update periodically
        if (m_stats.totalPacketsReceived % 1000 == 0) {
            emit statisticsUpdated(m_stats);
        }
    }
    
    /**
     * @brief Handle packet routing completion
     */
    void onPacketRouted(PacketPtr packet, PacketRouter::Priority priority) {
        Q_UNUSED(priority);
        emit packetProcessed(packet);
    }
    
    /**
     * @brief Handle packet drop
     */
    void onPacketDropped(PacketPtr packet, const QString& reason) {
        Q_UNUSED(packet);
        Q_UNUSED(reason);
        m_stats.totalPacketsDropped++;
    }
    
    /**
     * @brief Handle source error
     */
    void onSourceError(const QString& error) {
        PacketSource* source = qobject_cast<PacketSource*>(sender());
        if (source) {
            m_logger->error("PacketDispatcher", 
                QString("Source '%1' error: %2")
                .arg(QString::fromStdString(source->getName())).arg(error));
        }
    }

private:
    /**
     * @brief Check for back-pressure conditions
     */
    bool checkBackPressure() const {
        if (!m_router) {
            return false;
        }
        
        const auto& routerStats = m_router->getStatistics();
        
        // Check total queue depth across all priorities
        uint64_t totalQueueDepth = 0;
        for (size_t i = 0; i < PacketRouter::PRIORITY_LEVELS; ++i) {
            totalQueueDepth += routerStats.queueDepth[i].load();
        }
        
        return totalQueueDepth > m_config.backPressureThreshold;
    }
};

} // namespace Packet
} // namespace Monitor