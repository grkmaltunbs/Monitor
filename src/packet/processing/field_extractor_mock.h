#pragma once

#include "../core/packet.h"
#include "../../logging/logger.h"

#include <unordered_set>
#include <vector>

namespace Monitor {
namespace Packet {

/**
 * @brief Mock FieldExtractor for Phase 6 widget testing
 * 
 * This is a lightweight mock implementation that provides just enough
 * functionality to allow Phase 6 widgets to work without the full
 * Phase 4 packet processing system. It simply tracks packet IDs and
 * always returns true for hasFieldMap() for valid packet IDs.
 * 
 * This mock will be replaced with the full implementation when
 * Phase 4 packet processing is completed.
 */
class FieldExtractorMock {
public:
    FieldExtractorMock();
    ~FieldExtractorMock();

    /**
     * @brief Check if packet type has field map (mock always returns true for packetId > 0)
     */
    bool hasFieldMap(PacketId packetId) const;

    /**
     * @brief Register a packet ID for mock field extraction
     */
    void registerPacketId(PacketId packetId);

    /**
     * @brief Unregister a packet ID
     */
    void unregisterPacketId(PacketId packetId);

    /**
     * @brief Get all registered packet IDs
     */
    std::vector<PacketId> getRegisteredPacketIds() const;

    /**
     * @brief Get count of registered packet IDs
     */
    size_t getRegisteredPacketCount() const;

    /**
     * @brief Check if packet ID is registered
     */
    bool isPacketRegistered(PacketId packetId) const;

    /**
     * @brief Clear all registered packets
     */
    void clearRegisteredPackets();

private:
    std::unordered_set<PacketId> m_registeredPacketIds;
    Logging::Logger* m_logger;
};

} // namespace Packet
} // namespace Monitor