#pragma once

#include "../core/packet.h"
#include "subscription_manager.h"
#include "../../concurrent/mpsc_ring_buffer.h"
#include "../../threading/thread_pool.h"
#include "../../events/event_dispatcher.h"
#include "../../logging/logger.h"
#include "../../profiling/profiler.h"

#include <QtCore/QObject>
#include <QString>
#include <memory>
#include <unordered_map>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <thread>

namespace Monitor {
namespace Packet {

/**
 * @brief High-performance packet router with priority queues
 * 
 * This router processes incoming packets and distributes them to subscribers
 * based on packet ID. It uses multiple priority queues and worker threads
 * for maximum throughput while maintaining packet ordering where required.
 */
class PacketRouter : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Router configuration
     */
    struct Configuration {
        uint32_t queueSize = 10000;         ///< Queue size per priority level
        uint32_t workerThreads = 0;         ///< Number of worker threads (0 = auto)
        uint32_t batchSize = 100;           ///< Packets to process per batch
        uint32_t maxLatencyMs = 5;          ///< Maximum acceptable routing latency
        bool maintainOrder = false;         ///< Maintain packet order for each ID
        bool enableProfiling = true;        ///< Enable detailed profiling
        
        Configuration() {
            // Auto-detect optimal worker thread count
            workerThreads = std::max(2u, std::thread::hardware_concurrency() / 2);
        }
    };
    
    /**
     * @brief Routing priority levels
     */
    enum class Priority : uint8_t {
        Critical = 0,   ///< System-critical packets (highest priority)
        High = 1,       ///< High priority packets
        Normal = 2,     ///< Normal priority packets (default)
        Low = 3,        ///< Low priority packets
        Background = 4  ///< Background/bulk packets (lowest priority)
    };
    
    static constexpr size_t PRIORITY_LEVELS = 5;
    
    /**
     * @brief Router statistics
     */
    struct Statistics {
        std::atomic<uint64_t> packetsReceived{0};
        std::atomic<uint64_t> packetsRouted{0};
        std::atomic<uint64_t> packetsDropped{0};
        std::atomic<uint64_t> queueOverflows{0};
        std::atomic<uint64_t> averageLatencyNs{0};
        std::atomic<uint64_t> maxLatencyNs{0};
        
        // Per-priority statistics
        std::array<std::atomic<uint64_t>, PRIORITY_LEVELS> packetsPerPriority;
        std::array<std::atomic<uint64_t>, PRIORITY_LEVELS> queueDepth;
        
        std::chrono::steady_clock::time_point startTime;
        
        Statistics() : startTime(std::chrono::steady_clock::now()) {
            for (auto& counter : packetsPerPriority) {
                counter = 0;
            }
            for (auto& depth : queueDepth) {
                depth = 0;
            }
        }
        
        double getRoutingRate() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed == 0) return 0.0;
            return static_cast<double>(packetsRouted.load()) / elapsed;
        }
        
        double getDropRate() const {
            uint64_t total = packetsReceived.load();
            if (total == 0) return 0.0;
            return static_cast<double>(packetsDropped.load()) / total;
        }
    };

private:
    /**
     * @brief Packet queue entry
     */
    struct QueueEntry {
        PacketPtr packet;
        std::chrono::high_resolution_clock::time_point arrivalTime;
        Priority priority;
        
        QueueEntry() = default;
        
        QueueEntry(PacketPtr pkt, Priority prio)
            : packet(pkt), arrivalTime(std::chrono::high_resolution_clock::now()), priority(prio)
        {
        }
        
        // Explicit move constructor
        QueueEntry(QueueEntry&& other) noexcept
            : packet(std::move(other.packet))
            , arrivalTime(std::move(other.arrivalTime))
            , priority(other.priority)
        {
        }
        
        // Explicit move assignment operator
        QueueEntry& operator=(QueueEntry&& other) noexcept {
            if (this != &other) {
                packet = std::move(other.packet);
                arrivalTime = std::move(other.arrivalTime);
                priority = other.priority;
            }
            return *this;
        }
        
        // Delete copy operations to ensure we use move
        QueueEntry(const QueueEntry&) = delete;
        QueueEntry& operator=(const QueueEntry&) = delete;
    };
    
    Configuration m_config;
    SubscriptionManager* m_subscriptionManager;
    Threading::ThreadPool* m_threadPool;
    Events::EventDispatcher* m_eventDispatcher;
    Logging::Logger* m_logger;
    Profiling::Profiler* m_profiler;
    
    // Priority queues using lock-free ring buffers
    std::array<std::unique_ptr<Concurrent::MPSCRingBuffer<QueueEntry>>, PRIORITY_LEVELS> m_priorityQueues;
    
    // Worker threads and synchronization
    std::vector<std::thread> m_workerThreads;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
    std::condition_variable m_workerCondition;
    std::mutex m_workerMutex;
    
    // Statistics
    Statistics m_stats;
    
    // Packet ordering (if enabled)
    std::unordered_map<PacketId, SequenceNumber> m_lastSequence;
    std::mutex m_orderingMutex;

public:
    explicit PacketRouter(const Configuration& config, QObject* parent = nullptr)
        : QObject(parent)
        , m_config(config)
        , m_subscriptionManager(nullptr)
        , m_threadPool(nullptr)
        , m_eventDispatcher(nullptr)
        , m_logger(Logging::Logger::instance())
        , m_profiler(Profiling::Profiler::instance())
    {
        // Initialize priority queues
        for (size_t i = 0; i < PRIORITY_LEVELS; ++i) {
            m_priorityQueues[i] = std::make_unique<Concurrent::MPSCRingBuffer<QueueEntry>>(m_config.queueSize);
        }
    }
    
    ~PacketRouter() {
        stop();
    }
    
    /**
     * @brief Set subscription manager
     */
    void setSubscriptionManager(SubscriptionManager* manager) {
        m_subscriptionManager = manager;
    }
    
    /**
     * @brief Set thread pool for parallel processing
     */
    void setThreadPool(Threading::ThreadPool* threadPool) {
        m_threadPool = threadPool;
    }
    
    /**
     * @brief Set event dispatcher
     */
    void setEventDispatcher(Events::EventDispatcher* dispatcher) {
        m_eventDispatcher = dispatcher;
    }
    
    /**
     * @brief Start the router
     */
    bool start() {
        if (m_running.load()) {
            return true;
        }
        
        if (!m_subscriptionManager) {
            m_logger->error("PacketRouter", "No subscription manager available");
            return false;
        }
        
        m_logger->info("PacketRouter", 
            QString("Starting router with %1 worker threads").arg(m_config.workerThreads));
        
        m_running.store(true);
        m_stopRequested.store(false);
        
        // Start worker threads
        for (uint32_t i = 0; i < m_config.workerThreads; ++i) {
            m_workerThreads.emplace_back(&PacketRouter::workerThread, this, i);
        }
        
        m_stats.startTime = std::chrono::steady_clock::now();
        
        emit started();
        
        return true;
    }
    
    /**
     * @brief Stop the router
     */
    void stop() {
        if (!m_running.load()) {
            return;
        }
        
        m_logger->info("PacketRouter", "Stopping router");
        
        m_stopRequested.store(true);
        m_workerCondition.notify_all();
        
        // Wait for worker threads to finish
        for (auto& thread : m_workerThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        m_workerThreads.clear();
        
        m_running.store(false);
        
        emit stopped();
    }
    
    /**
     * @brief Route packet to appropriate subscribers
     */
    bool routePacket(PacketPtr packet, Priority priority = Priority::Normal) {
        if (!packet || !packet->isValid()) {
            m_stats.packetsDropped++;
            return false;
        }
        
        if (!m_running.load()) {
            m_logger->warning("PacketRouter", "Router not running, dropping packet");
            m_stats.packetsDropped++;
            return false;
        }
        
        PROFILE_SCOPE("PacketRouter::routePacket");
        
        // Update statistics
        m_stats.packetsReceived++;
        m_stats.packetsPerPriority[static_cast<size_t>(priority)]++;
        
        // Create queue entry
        QueueEntry entry(packet, priority);
        
        // Enqueue packet based on priority
        auto& queue = m_priorityQueues[static_cast<size_t>(priority)];
        if (!queue->tryPush(std::move(entry))) {
            m_logger->warning("PacketRouter", 
                QString("Priority queue %1 full, dropping packet ID %2")
                .arg(static_cast<int>(priority)).arg(packet->id()));
            m_stats.packetsDropped++;
            m_stats.queueOverflows++;
            return false;
        }
        
        // Update queue depth
        m_stats.queueDepth[static_cast<size_t>(priority)]++;
        
        // Notify worker threads
        m_workerCondition.notify_one();
        
        return true;
    }
    
    /**
     * @brief Route packet with automatic priority detection
     */
    bool routePacketAuto(PacketPtr packet) {
        Priority priority = detectPacketPriority(packet);
        return routePacket(std::move(packet), priority);
    }
    
    /**
     * @brief Get router statistics
     */
    const Statistics& getStatistics() const {
        return m_stats;
    }
    
    /**
     * @brief Get configuration
     */
    const Configuration& getConfiguration() const {
        return m_config;
    }
    
    /**
     * @brief Check if router is running
     */
    bool isRunning() const {
        return m_running.load();
    }

signals:
    /**
     * @brief Emitted when router starts
     */
    void started();
    
    /**
     * @brief Emitted when router stops
     */
    void stopped();
    
    /**
     * @brief Emitted when packet is successfully routed
     */
    void packetRouted(PacketPtr packet, Priority priority);
    
    /**
     * @brief Emitted when packet is dropped
     */
    void packetDropped(PacketPtr packet, const QString& reason);
    
    /**
     * @brief Emitted when statistics are updated
     */
    void statisticsUpdated(const Statistics& stats);

private:
    /**
     * @brief Worker thread main loop
     */
    void workerThread(uint32_t threadId) {
        m_logger->debug("PacketRouter", QString("Worker thread %1 started").arg(threadId));
        
        while (!m_stopRequested.load()) {
            bool processedAny = false;
            
            // Process packets from highest to lowest priority
            for (size_t priority = 0; priority < PRIORITY_LEVELS; ++priority) {
                auto& queue = m_priorityQueues[priority];
                
                // Process batch of packets from this priority
                for (uint32_t batch = 0; batch < m_config.batchSize; ++batch) {
                    QueueEntry entry;
                    if (!queue->tryPop(entry)) {
                        break; // No more packets at this priority
                    }
                    
                    processPacket(entry);
                    processedAny = true;
                    
                    // Update queue depth
                    m_stats.queueDepth[priority]--;
                }
                
                if (processedAny) {
                    break; // Process higher priority first
                }
            }
            
            // Wait for work if no packets processed
            if (!processedAny) {
                std::unique_lock<std::mutex> lock(m_workerMutex);
                m_workerCondition.wait_for(lock, std::chrono::milliseconds(1));
            }
        }
        
        m_logger->debug("PacketRouter", QString("Worker thread %1 stopped").arg(threadId));
    }
    
    /**
     * @brief Process individual packet
     */
    void processPacket(const QueueEntry& entry) {
        PROFILE_SCOPE("PacketRouter::processPacket");
        
        if (!entry.packet || !entry.packet->isValid()) {
            m_stats.packetsDropped++;
            return;
        }
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Check packet ordering if enabled
        if (m_config.maintainOrder) {
            if (!checkPacketOrdering(entry.packet)) {
                m_logger->warning("PacketRouter", 
                    QString("Out-of-order packet ID %1, sequence %2")
                    .arg(entry.packet->id()).arg(entry.packet->sequence()));
                // Still process the packet, but log the issue
            }
        }
        
        // Distribute packet to subscribers
        size_t subscriberCount = m_subscriptionManager->distributePacket(entry.packet);
        
        // Update statistics
        m_stats.packetsRouted++;
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - entry.arrivalTime).count();
        auto processingTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();
        
        // Update latency statistics
        uint64_t currentAvg = m_stats.averageLatencyNs.load();
        uint64_t newAvg = (currentAvg + static_cast<uint64_t>(latency)) / 2;
        m_stats.averageLatencyNs.store(newAvg);
        
        uint64_t currentMax = m_stats.maxLatencyNs.load();
        if (static_cast<uint64_t>(latency) > currentMax) {
            m_stats.maxLatencyNs.store(static_cast<uint64_t>(latency));
        }
        
        // Check latency threshold
        if (latency > m_config.maxLatencyMs * 1000000) { // Convert ms to ns
            m_logger->warning("PacketRouter", 
                QString("High routing latency: %1 ns for packet ID %2").arg(latency).arg(entry.packet->id()));
        }
        
        m_logger->debug("PacketRouter", 
            QString("Routed packet ID %1 to %2 subscribers in %3 ns (total latency: %4 ns)")
            .arg(entry.packet->id()).arg(subscriberCount).arg(processingTime).arg(latency));
        
        emit packetRouted(entry.packet, entry.priority);
        
        // Emit statistics update periodically
        if (m_stats.packetsRouted % 1000 == 0) {
            emit statisticsUpdated(m_stats);
        }
    }
    
    /**
     * @brief Detect packet priority based on header flags
     */
    Priority detectPacketPriority(PacketPtr packet) const {
        if (!packet || !packet->header()) {
            return Priority::Normal;
        }
        
        const PacketHeader* header = packet->header();
        
        if (header->hasFlag(PacketHeader::Flags::Priority)) {
            return Priority::High;
        }
        
        if (header->hasFlag(PacketHeader::Flags::TestData)) {
            return Priority::Low;
        }
        
        if (header->hasFlag(PacketHeader::Flags::Simulation)) {
            return Priority::Background;
        }
        
        return Priority::Normal;
    }
    
    /**
     * @brief Check packet ordering
     */
    bool checkPacketOrdering(PacketPtr packet) {
        if (!m_config.maintainOrder) {
            return true;
        }
        
        std::lock_guard<std::mutex> lock(m_orderingMutex);
        
        PacketId id = packet->id();
        SequenceNumber sequence = packet->sequence();
        
        auto it = m_lastSequence.find(id);
        if (it == m_lastSequence.end()) {
            m_lastSequence[id] = sequence;
            return true;
        }
        
        bool inOrder = (sequence > it->second) || 
                      (sequence == 0 && it->second > 0xFFFF0000); // Sequence wrap-around
        
        if (inOrder) {
            it->second = sequence;
        }
        
        return inOrder;
    }
};

/**
 * @brief Convert priority to string for debugging
 */
inline const char* priorityToString(PacketRouter::Priority priority) {
    switch (priority) {
        case PacketRouter::Priority::Critical: return "Critical";
        case PacketRouter::Priority::High: return "High";
        case PacketRouter::Priority::Normal: return "Normal";
        case PacketRouter::Priority::Low: return "Low";
        case PacketRouter::Priority::Background: return "Background";
        default: return "Unknown";
    }
}

} // namespace Packet
} // namespace Monitor