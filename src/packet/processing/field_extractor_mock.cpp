#include "field_extractor_mock.h"
#include "../../logging/logger.h"

namespace Monitor {
namespace Packet {

FieldExtractorMock::FieldExtractorMock()
    : m_logger(Logging::Logger::instance())
{
    m_logger->debug("FieldExtractorMock", "Mock FieldExtractor created for Phase 6 testing");
}

FieldExtractorMock::~FieldExtractorMock() {
    m_logger->debug("FieldExtractorMock", "Mock FieldExtractor destroyed");
}

bool FieldExtractorMock::hasFieldMap(PacketId packetId) const {
    // For Phase 6 testing, always return true for any packet ID > 0
    // This allows widgets to add fields without Phase 4 packet processing
    bool hasMap = (packetId > 0);
    
    if (hasMap && m_registeredPacketIds.find(packetId) == m_registeredPacketIds.end()) {
        // Automatically register packet ID when first queried
        const_cast<FieldExtractorMock*>(this)->registerPacketId(packetId);
    }
    
    return hasMap;
}

void FieldExtractorMock::registerPacketId(PacketId packetId) {
    if (packetId > 0) {
        m_registeredPacketIds.insert(packetId);
        m_logger->debug("FieldExtractorMock", 
            QString("Registered packet ID %1 for mock field extraction").arg(packetId));
    }
}

void FieldExtractorMock::unregisterPacketId(PacketId packetId) {
    m_registeredPacketIds.erase(packetId);
    m_logger->debug("FieldExtractorMock", 
        QString("Unregistered packet ID %1 from mock field extraction").arg(packetId));
}

std::vector<PacketId> FieldExtractorMock::getRegisteredPacketIds() const {
    return std::vector<PacketId>(m_registeredPacketIds.begin(), m_registeredPacketIds.end());
}

size_t FieldExtractorMock::getRegisteredPacketCount() const {
    return m_registeredPacketIds.size();
}

bool FieldExtractorMock::isPacketRegistered(PacketId packetId) const {
    return m_registeredPacketIds.find(packetId) != m_registeredPacketIds.end();
}

void FieldExtractorMock::clearRegisteredPackets() {
    m_registeredPacketIds.clear();
    m_logger->debug("FieldExtractorMock", "Cleared all registered packet IDs");
}

} // namespace Packet
} // namespace Monitor