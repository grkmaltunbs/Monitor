#pragma once

#include "packet_header.h"
#include "packet_buffer.h"
#include "../../parser/ast/ast_nodes.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace Monitor {
namespace Packet {

/**
 * @brief Main packet class representing a complete data packet
 * 
 * This class combines header, payload, and metadata into a cohesive packet
 * representation. It provides zero-copy access to packet data and integrates
 * with the Phase 2 structure parsing system for typed field access.
 */
class Packet {
public:
    /**
     * @brief Packet validation result
     */
    struct ValidationResult {
        bool isValid = false;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        
        ValidationResult() = default;
        ValidationResult(bool valid) : isValid(valid) {}
        
        void addError(const std::string& error) {
            errors.push_back(error);
            isValid = false;
        }
        
        void addWarning(const std::string& warning) {
            warnings.push_back(warning);
        }
        
        bool hasErrors() const { return !errors.empty(); }
        bool hasWarnings() const { return !warnings.empty(); }
    };

private:
    PacketBuffer::ManagedBufferPtr m_buffer;    ///< Managed packet buffer
    mutable PacketHeader* m_header;              ///< Cached header pointer
    size_t m_totalSize;                         ///< Total packet size
    std::shared_ptr<Parser::AST::StructDeclaration> m_structure; ///< Associated structure definition
    
    // Cached metadata
    mutable bool m_metadataValid;
    mutable std::string m_structureName;
    mutable size_t m_payloadOffset;
    mutable ValidationResult m_lastValidation;

public:
    /**
     * @brief Construct packet from managed buffer
     */
    explicit Packet(PacketBuffer::ManagedBufferPtr buffer)
        : m_buffer(std::move(buffer))
        , m_header(nullptr)
        , m_totalSize(0)
        , m_metadataValid(false)
        , m_payloadOffset(PACKET_HEADER_SIZE)
    {
        if (m_buffer && m_buffer->isValid()) {
            m_header = m_buffer->as<PacketHeader>();
            m_totalSize = m_buffer->size();
        }
    }
    
    // Move constructor
    Packet(Packet&& other) noexcept
        : m_buffer(std::move(other.m_buffer))
        , m_header(other.m_header)
        , m_totalSize(other.m_totalSize)
        , m_structure(std::move(other.m_structure))
        , m_metadataValid(other.m_metadataValid)
        , m_structureName(std::move(other.m_structureName))
        , m_payloadOffset(other.m_payloadOffset)
        , m_lastValidation(std::move(other.m_lastValidation))
    {
        other.m_header = nullptr;
        other.m_totalSize = 0;
        other.m_metadataValid = false;
    }
    
    // Move assignment
    Packet& operator=(Packet&& other) noexcept {
        if (this != &other) {
            m_buffer = std::move(other.m_buffer);
            m_header = other.m_header;
            m_totalSize = other.m_totalSize;
            m_structure = std::move(other.m_structure);
            m_metadataValid = other.m_metadataValid;
            m_structureName = std::move(other.m_structureName);
            m_payloadOffset = other.m_payloadOffset;
            m_lastValidation = std::move(other.m_lastValidation);
            
            other.m_header = nullptr;
            other.m_totalSize = 0;
            other.m_metadataValid = false;
        }
        return *this;
    }
    
    // Disable copy
    Packet(const Packet&) = delete;
    Packet& operator=(const Packet&) = delete;
    
    /**
     * @brief Check if packet is valid
     */
    bool isValid() const {
        return m_buffer && 
               m_buffer->isValid() && 
               m_header && 
               m_header->isValid() &&
               m_totalSize >= PACKET_HEADER_SIZE;
    }
    
    /**
     * @brief Get packet header (read-only)
     */
    const PacketHeader* header() const { 
        return m_header; 
    }
    
    /**
     * @brief Get mutable packet header
     */
    PacketHeader* header() { 
        m_metadataValid = false; // Invalidate cache when header might change
        return m_header; 
    }
    
    /**
     * @brief Get packet ID
     */
    PacketId id() const {
        return m_header ? m_header->id : 0;
    }
    
    /**
     * @brief Get sequence number
     */
    SequenceNumber sequence() const {
        return m_header ? m_header->sequence : 0;
    }
    
    /**
     * @brief Get timestamp
     */
    uint64_t timestamp() const {
        return m_header ? m_header->timestamp : 0;
    }
    
    /**
     * @brief Get timestamp as time_point
     */
    std::chrono::high_resolution_clock::time_point getTimestamp() const {
        return m_header ? m_header->getTimestamp() : std::chrono::high_resolution_clock::time_point{};
    }
    
    /**
     * @brief Get packet age in nanoseconds
     */
    uint64_t getAgeNs() const {
        return m_header ? m_header->getAgeNs() : 0;
    }
    
    /**
     * @brief Get payload size
     */
    size_t payloadSize() const {
        return m_header ? m_header->payloadSize : 0;
    }
    
    /**
     * @brief Get total packet size
     */
    size_t totalSize() const {
        return m_totalSize;
    }
    
    /**
     * @brief Get raw packet data
     */
    const uint8_t* data() const {
        return m_buffer ? m_buffer->bytes() : nullptr;
    }
    
    /**
     * @brief Get payload data (after header)
     */
    const uint8_t* payload() const {
        if (!m_buffer || m_totalSize <= PACKET_HEADER_SIZE) {
            return nullptr;
        }
        return m_buffer->bytes() + PACKET_HEADER_SIZE;
    }
    
    /**
     * @brief Get mutable payload data
     */
    uint8_t* payload() {
        if (!m_buffer || m_totalSize <= PACKET_HEADER_SIZE) {
            return nullptr;
        }
        return m_buffer->bytes() + PACKET_HEADER_SIZE;
    }
    
    /**
     * @brief Associate packet with structure definition
     */
    void setStructure(std::shared_ptr<Parser::AST::StructDeclaration> structure) {
        m_structure = structure;
        m_metadataValid = false;
    }
    
    /**
     * @brief Get associated structure definition
     */
    std::shared_ptr<Parser::AST::StructDeclaration> getStructure() const {
        return m_structure;
    }
    
    /**
     * @brief Get structure name
     */
    const std::string& getStructureName() const {
        updateMetadata();
        return m_structureName;
    }
    
    /**
     * @brief Check if packet has specific flag
     */
    bool hasFlag(PacketHeader::Flags flag) const {
        return m_header ? m_header->hasFlag(flag) : false;
    }
    
    /**
     * @brief Set packet flag
     */
    void setFlag(PacketHeader::Flags flag) {
        if (m_header) {
            m_header->setFlag(flag);
        }
    }
    
    /**
     * @brief Clear packet flag
     */
    void clearFlag(PacketHeader::Flags flag) {
        if (m_header) {
            m_header->clearFlag(flag);
        }
    }
    
    /**
     * @brief Validate packet integrity
     */
    ValidationResult validate() const {
        ValidationResult result;
        
        // Basic buffer validation
        if (!m_buffer || !m_buffer->isValid()) {
            result.addError("Invalid or null packet buffer");
            return result;
        }
        
        // Header validation
        if (!m_header) {
            result.addError("Null packet header");
            return result;
        }
        
        if (!m_header->isValid()) {
            result.addError("Invalid packet header");
        }
        
        // Size validation
        if (m_totalSize < PACKET_HEADER_SIZE) {
            result.addError("Packet size smaller than header size");
        }
        
        if (m_header->payloadSize > m_totalSize - PACKET_HEADER_SIZE) {
            result.addError("Header payload size exceeds actual payload size");
        }
        
        // Structure validation (if available)
        if (m_structure) {
            size_t expectedSize = m_structure->getTotalSize();
            if (expectedSize > 0 && m_header->payloadSize != expectedSize) {
                result.addWarning("Payload size mismatch with structure definition");
            }
        }
        
        // Age validation
        uint64_t ageMs = getAgeNs() / 1000000; // Convert to milliseconds
        if (ageMs > 60000) { // More than 1 minute old
            result.addWarning("Packet is older than 1 minute");
        }
        
        if (result.errors.empty()) {
            result.isValid = true;
        }
        
        return result;
    }
    
    /**
     * @brief Get cached validation result
     */
    const ValidationResult& getLastValidation() const {
        return m_lastValidation;
    }
    
    /**
     * @brief Update sequence number
     */
    void setSequence(SequenceNumber seq) {
        if (m_header) {
            m_header->sequence = seq;
        }
    }
    
    /**
     * @brief Update timestamp to current time
     */
    void updateTimestamp() {
        if (m_header) {
            m_header->timestamp = PacketHeader::getCurrentTimestampNs();
        }
    }
    
    /**
     * @brief Get buffer pool name
     */
    std::string getPoolName() const {
        return m_buffer ? m_buffer->poolName().toStdString() : "";
    }
    
    /**
     * @brief Get buffer capacity
     */
    size_t getBufferCapacity() const {
        return m_buffer ? m_buffer->capacity() : 0;
    }
    
private:
    /**
     * @brief Update cached metadata
     */
    void updateMetadata() const {
        if (m_metadataValid) {
            return;
        }
        
        if (m_structure) {
            m_structureName = m_structure->getName();
        } else {
            m_structureName = "Unknown";
        }
        
        m_metadataValid = true;
    }
};

/**
 * @brief Shared pointer to packet for efficient sharing
 */
using PacketPtr = std::shared_ptr<Packet>;

/**
 * @brief Weak pointer to packet to avoid circular references
 */
using PacketWeakPtr = std::weak_ptr<Packet>;

} // namespace Packet
} // namespace Monitor