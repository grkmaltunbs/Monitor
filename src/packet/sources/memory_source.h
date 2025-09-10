#pragma once

#include "packet_source.h"
#include <QtCore/QTimer>
#include <QString>
#include <vector>
#include <queue>

namespace Monitor {
namespace Packet {

/**
 * @brief Memory-based packet source for testing
 * 
 * This source delivers pre-loaded packets from memory, useful for
 * unit testing, integration testing, and replaying specific packet
 * sequences.
 */
class MemorySource : public PacketSource {
    Q_OBJECT
    
public:
    /**
     * @brief Memory source configuration
     */
    struct MemoryConfig : public Configuration {
        bool repeatSequence = false;    ///< Repeat packet sequence when finished
        uint32_t intervalMs = 100;      ///< Interval between packets in ms
        bool randomizeOrder = false;    ///< Randomize packet delivery order
        
        MemoryConfig(const std::string& name) : Configuration(name) {}
    };

private:
    MemoryConfig m_memConfig;
    std::vector<std::vector<uint8_t>> m_packetData;
    std::queue<size_t> m_deliveryQueue;
    std::unique_ptr<QTimer> m_deliveryTimer;
    size_t m_currentIndex;
    bool m_sequenceComplete;

public:
    explicit MemorySource(const MemoryConfig& config, QObject* parent = nullptr)
        : PacketSource(config, parent)
        , m_memConfig(config)
        , m_currentIndex(0)
        , m_sequenceComplete(false)
    {
        m_config = config; // Copy base configuration
        
        m_deliveryTimer = std::make_unique<QTimer>(this);
        m_deliveryTimer->setInterval(static_cast<int>(m_memConfig.intervalMs));
        connect(m_deliveryTimer.get(), &QTimer::timeout, this, &MemorySource::deliverNextPacket);
    }
    
    /**
     * @brief Add packet data to the source
     */
    void addPacket(const void* data, size_t size) {
        if (!data || size == 0) {
            return;
        }
        
        std::vector<uint8_t> packetData(size);
        std::memcpy(packetData.data(), data, size);
        m_packetData.push_back(std::move(packetData));
        
        m_logger->debug("MemorySource", 
            QString("Added packet %1: %2 bytes")
            .arg(m_packetData.size()).arg(size));
    }
    
    /**
     * @brief Add packet from existing packet
     */
    void addPacket(const PacketPtr& packet) {
        if (!packet || !packet->isValid()) {
            return;
        }
        
        addPacket(packet->data(), packet->totalSize());
    }
    
    /**
     * @brief Clear all packet data
     */
    void clearPackets() {
        m_packetData.clear();
        std::queue<size_t> empty;
        m_deliveryQueue.swap(empty);
        m_currentIndex = 0;
        m_sequenceComplete = false;
    }
    
    /**
     * @brief Get number of packets in source
     */
    size_t getPacketCount() const {
        return m_packetData.size();
    }
    
    /**
     * @brief Check if sequence is complete
     */
    bool isSequenceComplete() const {
        return m_sequenceComplete;
    }
    
    /**
     * @brief Get memory configuration
     */
    const MemoryConfig& getMemoryConfig() const {
        return m_memConfig;
    }

protected:
    bool doStart() override {
        m_logger->info("MemorySource", 
            QString("Starting memory source with %1 packets").arg(m_packetData.size()));
        
        if (!m_packetFactory) {
            reportError("No packet factory available");
            return false;
        }
        
        if (m_packetData.empty()) {
            reportError("No packet data available");
            return false;
        }
        
        // Reset state
        m_currentIndex = 0;
        m_sequenceComplete = false;
        std::queue<size_t> empty;
        m_deliveryQueue.swap(empty);
        
        // Prepare delivery queue
        prepareDeliveryQueue();
        
        // Start delivery timer
        m_deliveryTimer->start();
        
        return true;
    }
    
    void doStop() override {
        m_logger->info("MemorySource", "Stopping memory source");
        m_deliveryTimer->stop();
    }
    
    void doPause() override {
        m_deliveryTimer->stop();
    }
    
    bool doResume() override {
        m_deliveryTimer->start();
        return true;
    }

private slots:
    void deliverNextPacket() {
        if (m_deliveryQueue.empty()) {
            if (m_memConfig.repeatSequence && !m_packetData.empty()) {
                prepareDeliveryQueue();
            } else {
                m_sequenceComplete = true;
                stop();
                return;
            }
        }
        
        if (m_deliveryQueue.empty()) {
            return; // Still empty after preparation
        }
        
        // Get next packet index
        size_t packetIndex = m_deliveryQueue.front();
        m_deliveryQueue.pop();
        
        if (packetIndex >= m_packetData.size()) {
            reportError("Invalid packet index");
            return;
        }
        
        // Create packet from stored data
        const auto& packetData = m_packetData[packetIndex];
        auto result = m_packetFactory->createFromRawData(packetData.data(), packetData.size());
        
        if (!result.success) {
            reportError("Failed to create packet from memory data: " + result.error);
            return;
        }
        
        // Mark as test data
        result.packet->setFlag(PacketHeader::Flags::TestData);
        
        // Update statistics
        m_stats.packetsGenerated++;
        
        // Deliver packet
        deliverPacket(result.packet);
        
        m_logger->debug("MemorySource", 
            QString("Delivered packet %1: ID=%2, size=%3")
            .arg(packetIndex).arg(result.packet->id()).arg(result.packet->totalSize()));
    }

private:
    /**
     * @brief Prepare delivery queue based on configuration
     */
    void prepareDeliveryQueue() {
        std::queue<size_t> empty;
        m_deliveryQueue.swap(empty);
        
        if (m_memConfig.randomizeOrder) {
            // Create randomized order
            std::vector<size_t> indices(m_packetData.size());
            std::iota(indices.begin(), indices.end(), 0);
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::shuffle(indices.begin(), indices.end(), gen);
            
            for (size_t index : indices) {
                m_deliveryQueue.push(index);
            }
        } else {
            // Sequential order
            for (size_t i = 0; i < m_packetData.size(); ++i) {
                m_deliveryQueue.push(i);
            }
        }
    }
};

} // namespace Packet
} // namespace Monitor