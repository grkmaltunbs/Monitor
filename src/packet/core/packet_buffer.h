#pragma once

#include "packet_header.h"
#include "../../memory/memory_pool.h"
#include "../../logging/logger.h"

#include <memory>
#include <cstring>
#include <stdexcept>
#include <QString>

namespace Monitor {
namespace Packet {

/**
 * @brief Zero-copy packet buffer management using Phase 1 memory pools
 * 
 * This class manages packet data storage using the existing memory pool
 * infrastructure. It provides zero-copy semantics where possible and
 * ensures proper memory alignment for high-performance packet processing.
 */
class PacketBuffer {
public:
    /**
     * @brief Buffer allocation result
     */
    struct BufferAllocation {
        void* data = nullptr;           ///< Pointer to allocated memory
        size_t size = 0;                ///< Size of allocated buffer
        size_t capacity = 0;            ///< Total capacity of buffer
        QString poolName;               ///< Name of memory pool used
        bool success = false;           ///< Allocation success flag
        
        BufferAllocation() = default;
        BufferAllocation(void* ptr, size_t sz, size_t cap, const QString& pool)
            : data(ptr), size(sz), capacity(cap), poolName(pool), success(ptr != nullptr)
        {
        }
    };
    
    /**
     * @brief Smart pointer for packet buffer with automatic cleanup
     */
    class ManagedBuffer {
    private:
        void* m_data;
        size_t m_size;
        size_t m_capacity;
        QString m_poolName;
        Memory::MemoryPoolManager* m_manager;
        
    public:
        ManagedBuffer(void* data, size_t size, size_t capacity, 
                     const QString& poolName, Memory::MemoryPoolManager* manager)
            : m_data(data), m_size(size), m_capacity(capacity), 
              m_poolName(poolName), m_manager(manager)
        {
        }
        
        ~ManagedBuffer() {
            if (m_data && m_manager) {
                m_manager->deallocate(m_poolName, m_data);
            }
        }
        
        // Move constructor
        ManagedBuffer(ManagedBuffer&& other) noexcept
            : m_data(other.m_data), m_size(other.m_size), 
              m_capacity(other.m_capacity), m_poolName(other.m_poolName),
              m_manager(other.m_manager)
        {
            other.m_data = nullptr;
            other.m_manager = nullptr;
        }
        
        // Move assignment
        ManagedBuffer& operator=(ManagedBuffer&& other) noexcept {
            if (this != &other) {
                if (m_data && m_manager) {
                    m_manager->deallocate(m_poolName, m_data);
                }
                
                m_data = other.m_data;
                m_size = other.m_size;
                m_capacity = other.m_capacity;
                m_poolName = other.m_poolName;
                m_manager = other.m_manager;
                
                other.m_data = nullptr;
                other.m_manager = nullptr;
            }
            return *this;
        }
        
        // Disable copy
        ManagedBuffer(const ManagedBuffer&) = delete;
        ManagedBuffer& operator=(const ManagedBuffer&) = delete;
        
        void* data() const { return m_data; }
        size_t size() const { return m_size; }
        size_t capacity() const { return m_capacity; }
        const QString& poolName() const { return m_poolName; }
        
        bool isValid() const { return m_data != nullptr && m_manager != nullptr; }
        
        template<typename T>
        T* as() const {
            return static_cast<T*>(m_data);
        }
        
        uint8_t* bytes() const {
            return static_cast<uint8_t*>(m_data);
        }
    };
    
    using ManagedBufferPtr = std::unique_ptr<ManagedBuffer>;
    
private:
    Memory::MemoryPoolManager* m_memoryManager;
    Logging::Logger* m_logger;
    
public:
    explicit PacketBuffer(Memory::MemoryPoolManager* manager)
        : m_memoryManager(manager)
        , m_logger(Logging::Logger::instance())
    {
        if (!m_memoryManager) {
            throw std::invalid_argument("Memory manager cannot be null");
        }
    }
    
    /**
     * @brief Allocate buffer for packet of given size
     * 
     * Selects appropriate memory pool based on packet size:
     * - SmallObjects (64B): Packets up to 64 bytes
     * - MediumObjects (512B): Packets up to 512 bytes  
     * - WidgetData (1KB): Packets up to 1KB
     * - TestFramework (2KB): Packets up to 2KB
     * - PacketBuffer (4KB): Packets up to 4KB
     * - LargeObjects (8KB): Packets up to 8KB
     */
    ManagedBufferPtr allocate(size_t totalSize) {
        if (totalSize == 0) {
            m_logger->warning("PacketBuffer", "Attempted to allocate zero-size buffer");
            return nullptr;
        }
        
        if (totalSize > PacketHeader::MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE) {
            m_logger->error("PacketBuffer", 
                QString("Requested size %1 exceeds maximum packet size %2")
                .arg(totalSize).arg(PacketHeader::MAX_PAYLOAD_SIZE + PACKET_HEADER_SIZE));
            return nullptr;
        }
        
        QString poolName = selectMemoryPool(totalSize);
        void* data = m_memoryManager->allocate(poolName);
        
        if (!data) {
            m_logger->error("PacketBuffer", 
                QString("Failed to allocate %1 bytes from pool %2")
                .arg(totalSize).arg(poolName));
            return nullptr;
        }
        
        size_t poolBlockSize = getPoolBlockSize(poolName);
        
        return std::make_unique<ManagedBuffer>(
            data, totalSize, poolBlockSize, poolName, m_memoryManager
        );
    }
    
    /**
     * @brief Allocate buffer for packet with specific header and payload size
     */
    ManagedBufferPtr allocateForPacket(size_t payloadSize) {
        return allocate(PACKET_HEADER_SIZE + payloadSize);
    }
    
    /**
     * @brief Create buffer from existing data (copy)
     */
    ManagedBufferPtr createFromData(const void* data, size_t size) {
        auto buffer = allocate(size);
        if (buffer && data) {
            std::memcpy(buffer->data(), data, size);
        }
        return buffer;
    }
    
    /**
     * @brief Create buffer for specific packet ID with payload size
     */
    ManagedBufferPtr createForPacket(PacketId id, const void* payload, size_t payloadSize) {
        auto buffer = allocateForPacket(payloadSize);
        if (buffer) {
            // Initialize header
            PacketHeader* header = buffer->as<PacketHeader>();
            *header = PacketHeader(id, 0, static_cast<uint32_t>(payloadSize));
            
            // Copy payload if provided
            if (payload && payloadSize > 0) {
                uint8_t* payloadPtr = buffer->bytes() + PACKET_HEADER_SIZE;
                std::memcpy(payloadPtr, payload, payloadSize);
            }
        }
        return buffer;
    }
    
    /**
     * @brief Get memory pool statistics
     */
    struct PoolStats {
        QString name;
        size_t blockSize;
        size_t totalBlocks;
        size_t usedBlocks;
        size_t freeBlocks;
        double utilization;
    };
    
    std::vector<PoolStats> getPoolStatistics() const {
        std::vector<PoolStats> stats;
        
        // Get statistics from all packet-related pools
        std::vector<QString> pools = {
            "SmallObjects", "MediumObjects", "WidgetData", 
            "TestFramework", "PacketBuffer", "LargeObjects"
        };
        
        for (const auto& poolName : pools) {
            // For now, just return basic stats since hasPool and getPoolStatistics
            // methods need to be added to MemoryPoolManager
            stats.push_back({
                poolName,
                getPoolBlockSize(poolName),
                0,  // totalBlocks - would need to be implemented
                0,  // usedBlocks - would need to be implemented
                0,  // freeBlocks - would need to be implemented
                0.0 // utilization - would need to be implemented
            });
        }
        
        return stats;
    }
    
    /**
     * @brief Get total memory usage across all packet pools
     */
    size_t getTotalMemoryUsage() const {
        size_t total = 0;
        auto stats = getPoolStatistics();
        for (const auto& stat : stats) {
            total += stat.usedBlocks * stat.blockSize;
        }
        return total;
    }
    
private:
    /**
     * @brief Select appropriate memory pool based on size
     */
    QString selectMemoryPool(size_t size) const {
        if (size <= 64) return "SmallObjects";
        if (size <= 512) return "MediumObjects";
        if (size <= 1024) return "WidgetData";
        if (size <= 2048) return "TestFramework";
        if (size <= 4096) return "PacketBuffer";
        return "LargeObjects"; // Up to 8KB
    }
    
    /**
     * @brief Get block size for memory pool
     */
    size_t getPoolBlockSize(const QString& poolName) const {
        if (poolName == "SmallObjects") return 64;
        if (poolName == "MediumObjects") return 512;
        if (poolName == "WidgetData") return 1024;
        if (poolName == "TestFramework") return 2048;
        if (poolName == "PacketBuffer") return 4096;
        if (poolName == "LargeObjects") return 8192;
        return 0;
    }
};

} // namespace Packet
} // namespace Monitor