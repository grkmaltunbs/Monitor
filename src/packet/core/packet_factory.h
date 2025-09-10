#pragma once

#include "packet.h"
#include "packet_buffer.h"
#include "../../parser/manager/structure_manager.h"
#include "../../events/event_dispatcher.h"
#include "../../logging/logger.h"
#include "../../profiling/profiler.h"

#include <QtCore/QObject>
#include <QString>
#include <memory>
#include <unordered_map>
#include <functional>
#include <atomic>

namespace Monitor {
namespace Packet {

/**
 * @brief Factory for creating and managing packets
 * 
 * This factory provides high-level interface for packet creation,
 * integrates with Phase 2 structure parsing, and maintains packet
 * statistics. It uses Phase 1 memory pools for efficient allocation.
 */
class PacketFactory : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Packet creation result
     */
    struct CreationResult {
        PacketPtr packet;
        bool success = false;
        std::string error;
        std::chrono::nanoseconds creationTime{0};
        
        CreationResult() = default;
        CreationResult(PacketPtr pkt) : packet(pkt), success(pkt != nullptr) {}
        CreationResult(const std::string& err) : success(false), error(err) {}
    };
    
    /**
     * @brief Factory statistics
     */
    struct Statistics {
        std::atomic<uint64_t> packetsCreated{0};
        std::atomic<uint64_t> packetsFromRawData{0};
        std::atomic<uint64_t> packetsFromStructure{0};
        std::atomic<uint64_t> packetsWithErrors{0};
        std::atomic<uint64_t> totalBytesAllocated{0};
        std::atomic<uint64_t> averageCreationTimeNs{0};
        
        std::chrono::steady_clock::time_point startTime;
        
        Statistics() : startTime(std::chrono::steady_clock::now()) {}
        
        double getCreationRate() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed == 0) return 0.0;
            return static_cast<double>(packetsCreated.load()) / elapsed;
        }
        
        double getErrorRate() const {
            uint64_t total = packetsCreated.load();
            if (total == 0) return 0.0;
            return static_cast<double>(packetsWithErrors.load()) / total;
        }
    };

private:
    std::unique_ptr<PacketBuffer> m_packetBuffer;
    Parser::StructureManager* m_structureManager;
    Events::EventDispatcher* m_eventDispatcher;
    Logging::Logger* m_logger;
    Profiling::Profiler* m_profiler;
    
    // Structure cache for fast packet ID to structure mapping
    std::unordered_map<PacketId, std::shared_ptr<Parser::AST::StructDeclaration>> m_structureCache;
    mutable std::shared_mutex m_cacheMutex;
    
    // Statistics
    mutable Statistics m_stats;
    
    // Sequence number generator
    std::atomic<SequenceNumber> m_nextSequence{1};

public:
    explicit PacketFactory(Memory::MemoryPoolManager* memoryManager, QObject* parent = nullptr)
        : QObject(parent)
        , m_packetBuffer(std::make_unique<PacketBuffer>(memoryManager))
        , m_structureManager(nullptr)
        , m_eventDispatcher(nullptr)
        , m_logger(Logging::Logger::instance())
        , m_profiler(Profiling::Profiler::instance())
    {
        if (!memoryManager) {
            throw std::invalid_argument("Memory manager cannot be null");
        }
    }
    
    /**
     * @brief Set structure manager for packet type resolution
     */
    void setStructureManager(Parser::StructureManager* manager) {
        m_structureManager = manager;
        
        if (m_structureManager) {
            // Connect to structure updates to invalidate cache
            // TODO: Add these signals to StructureManager
            // connect(m_structureManager, &Parser::StructureManager::structureAdded,
            //         this, &PacketFactory::onStructureAdded);
            // connect(m_structureManager, &Parser::StructureManager::structureRemoved,
            //         this, &PacketFactory::onStructureRemoved);
        }
    }
    
    /**
     * @brief Set event dispatcher for packet events
     */
    void setEventDispatcher(Events::EventDispatcher* dispatcher) {
        m_eventDispatcher = dispatcher;
    }
    
    /**
     * @brief Create packet from raw data
     */
    CreationResult createFromRawData(const void* data, size_t size) {
        PROFILE_FUNCTION();
        auto startTime = std::chrono::high_resolution_clock::now();
        
        if (!data || size < PACKET_HEADER_SIZE) {
            QString error = "Invalid raw data or size too small";
            m_logger->error("PacketFactory", error);
            m_stats.packetsWithErrors++;
            return CreationResult(error.toStdString());
        }
        
        // Create buffer and copy data
        auto buffer = m_packetBuffer->createFromData(data, size);
        if (!buffer) {
            std::string error = "Failed to allocate buffer for packet";
            m_logger->error("PacketFactory", error.c_str());
            m_stats.packetsWithErrors++;
            return CreationResult(error);
        }
        
        // Create packet
        auto packet = std::make_shared<Packet>(std::move(buffer));
        if (!packet->isValid()) {
            std::string error = "Created invalid packet";
            m_logger->error("PacketFactory", error.c_str());
            m_stats.packetsWithErrors++;
            return CreationResult(error);
        }
        
        // Try to associate with structure
        associateStructure(packet);
        
        // Update statistics
        updateCreationStats(startTime, size, CreationType::FromRawData);
        
        m_logger->debug("PacketFactory", 
            QString("Created packet from raw data: ID=%1, size=%2 bytes")
            .arg(packet->id()).arg(size));
        
        return CreationResult(packet);
    }
    
    /**
     * @brief Create packet for specific packet ID with payload
     */
    CreationResult createPacket(PacketId id, const void* payload = nullptr, size_t payloadSize = 0) {
        PROFILE_FUNCTION();
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Create buffer
        auto buffer = m_packetBuffer->createForPacket(id, payload, payloadSize);
        if (!buffer) {
            std::string error = "Failed to allocate buffer for packet";
            m_logger->error("PacketFactory", error.c_str());
            m_stats.packetsWithErrors++;
            return CreationResult(error);
        }
        
        // Create packet
        auto packet = std::make_shared<Packet>(std::move(buffer));
        if (!packet->isValid()) {
            std::string error = "Created invalid packet";
            m_logger->error("PacketFactory", error.c_str());
            m_stats.packetsWithErrors++;
            return CreationResult(error);
        }
        
        // Set sequence number
        packet->setSequence(m_nextSequence++);
        
        // Try to associate with structure
        associateStructure(packet);
        
        // Update statistics
        updateCreationStats(startTime, PACKET_HEADER_SIZE + payloadSize, CreationType::New);
        
        m_logger->debug("PacketFactory", 
            QString("Created new packet: ID=%1, payload size=%2 bytes")
            .arg(id).arg(payloadSize));
        
        return CreationResult(packet);
    }
    
    /**
     * @brief Create packet using structure definition
     */
    CreationResult createFromStructure(PacketId id, const std::string& structureName, 
                                      const void* data = nullptr, size_t dataSize = 0) {
        PROFILE_FUNCTION();
        auto startTime = std::chrono::high_resolution_clock::now();
        
        if (!m_structureManager) {
            std::string error = "Structure manager not available";
            m_logger->error("PacketFactory", error.c_str());
            m_stats.packetsWithErrors++;
            return CreationResult(error);
        }
        
        // Get structure definition
        auto structure = m_structureManager->getStructure(structureName);
        if (!structure) {
            std::string error = "Structure not found: " + structureName;
            m_logger->error("PacketFactory", error.c_str());
            m_stats.packetsWithErrors++;
            return CreationResult(error);
        }
        
        size_t structureSize = structure->getTotalSize();
        size_t actualPayloadSize = dataSize > 0 ? dataSize : structureSize;
        
        // Create packet
        auto result = createPacket(id, data, actualPayloadSize);
        if (!result.success) {
            return result;
        }
        
        // Associate with structure (cast away const for compatibility)
        auto nonConstStructure = std::const_pointer_cast<Parser::AST::StructDeclaration>(structure);
        result.packet->setStructure(nonConstStructure);
        
        // Cache structure for future lookups
        cacheStructure(id, nonConstStructure);
        
        // Update statistics
        updateCreationStats(startTime, PACKET_HEADER_SIZE + actualPayloadSize, CreationType::FromStructure);
        
        m_logger->debug("PacketFactory", 
            QString("Created structured packet: ID=%1, structure=%2, size=%3 bytes")
            .arg(id).arg(QString::fromStdString(structureName)).arg(actualPayloadSize));
        
        return result;
    }
    
    /**
     * @brief Clone existing packet
     */
    CreationResult clonePacket(const PacketPtr& original) {
        if (!original || !original->isValid()) {
            std::string error = "Invalid original packet for cloning";
            m_logger->error("PacketFactory", error.c_str());
            m_stats.packetsWithErrors++;
            return CreationResult(error);
        }
        
        return createFromRawData(original->data(), original->totalSize());
    }
    
    /**
     * @brief Get factory statistics
     */
    const Statistics& getStatistics() const {
        return m_stats;
    }
    
    /**
     * @brief Reset statistics
     */
    void resetStatistics() {
        m_stats.packetsCreated = 0;
        m_stats.packetsFromRawData = 0;
        m_stats.packetsFromStructure = 0;
        m_stats.packetsWithErrors = 0;
        m_stats.totalBytesAllocated = 0;
        m_stats.averageCreationTimeNs = 0;
        m_stats.startTime = std::chrono::steady_clock::now();
    }
    
    /**
     * @brief Get next sequence number
     */
    SequenceNumber getNextSequence() const {
        return m_nextSequence.load();
    }
    
    /**
     * @brief Check if structure is cached for packet ID
     */
    bool hasStructureForPacketId(PacketId id) const {
        std::shared_lock lock(m_cacheMutex);
        return m_structureCache.find(id) != m_structureCache.end();
    }

signals:
    /**
     * @brief Emitted when a packet is successfully created
     */
    void packetCreated(PacketPtr packet);
    
    /**
     * @brief Emitted when packet creation fails
     */
    void packetCreationFailed(PacketId id, const QString& error);
    
    /**
     * @brief Emitted when statistics are updated
     */
    void statisticsUpdated(const Statistics& stats);

private slots:
    void onStructureAdded(const QString& name) {
        m_logger->debug("PacketFactory", QString("Structure added: %1").arg(name));
        // Structure cache will be updated on next packet creation
    }
    
    void onStructureRemoved(const QString& name) {
        m_logger->debug("PacketFactory", QString("Structure removed: %1").arg(name));
        // Invalidate cache entries that use this structure
        std::unique_lock lock(m_cacheMutex);
        for (auto it = m_structureCache.begin(); it != m_structureCache.end();) {
            if (it->second && it->second->getName() == name.toStdString()) {
                it = m_structureCache.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    enum class CreationType {
        FromRawData,
        FromStructure,
        New
    };
    
    /**
     * @brief Associate packet with structure definition if possible
     */
    void associateStructure(PacketPtr packet) {
        if (!packet || !m_structureManager) {
            return;
        }
        
        PacketId id = packet->id();
        
        // Check cache first
        {
            std::shared_lock lock(m_cacheMutex);
            auto it = m_structureCache.find(id);
            if (it != m_structureCache.end() && it->second) {
                packet->setStructure(it->second);
                return;
            }
        }
        
        // Try to find structure by packet ID
        // This would require extending StructureManager to support ID-based lookup
        // For now, we'll leave packets without structure association unless explicitly provided
    }
    
    /**
     * @brief Cache structure for packet ID
     */
    void cacheStructure(PacketId id, std::shared_ptr<Parser::AST::StructDeclaration> structure) {
        std::unique_lock lock(m_cacheMutex);
        m_structureCache[id] = structure;
    }
    
    /**
     * @brief Update creation statistics
     */
    void updateCreationStats(const std::chrono::high_resolution_clock::time_point& startTime, 
                           size_t bytes, CreationType type) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
        
        m_stats.packetsCreated++;
        m_stats.totalBytesAllocated += bytes;
        
        switch (type) {
            case CreationType::FromRawData:
                m_stats.packetsFromRawData++;
                break;
            case CreationType::FromStructure:
                m_stats.packetsFromStructure++;
                break;
            case CreationType::New:
                break;
        }
        
        // Update average creation time (simple rolling average)
        uint64_t currentAvg = m_stats.averageCreationTimeNs.load();
        uint64_t newAvg = (currentAvg + duration.count()) / 2;
        m_stats.averageCreationTimeNs.store(newAvg);
        
        // Emit statistics update periodically
        if (m_stats.packetsCreated % 1000 == 0) {
            emit statisticsUpdated(m_stats);
        }
    }
};

} // namespace Packet
} // namespace Monitor