#pragma once

#include <cstdint>
#include <chrono>

namespace Monitor {
namespace Packet {

/**
 * @brief Packet ID type - used to identify packet types
 */
using PacketId = uint32_t;

/**
 * @brief Sequence number for packet ordering
 */
using SequenceNumber = uint32_t;

/**
 * @brief Common packet header structure
 * 
 * This header is present at the beginning of every packet and contains
 * essential routing and identification information. The structure is 
 * designed to be compatible with common network packet formats.
 */
struct PacketHeader {
    PacketId id;                    ///< Packet type identifier (4 bytes)
    SequenceNumber sequence;        ///< Sequence number for ordering (4 bytes)
    uint64_t timestamp;             ///< Nanosecond timestamp (8 bytes)
    uint32_t payloadSize;           ///< Size of packet payload in bytes (4 bytes)
    uint32_t flags;                 ///< Packet flags and metadata (4 bytes)
    
    // Total header size: 24 bytes
    
    /**
     * @brief Header flag definitions
     */
    enum Flags : uint32_t {
        None = 0x00000000,
        Compressed = 0x00000001,    ///< Payload is compressed
        Fragmented = 0x00000002,    ///< Part of fragmented packet
        Priority = 0x00000004,      ///< High priority packet
        Encrypted = 0x00000008,     ///< Payload is encrypted
        TestData = 0x00000010,      ///< Generated test data
        Simulation = 0x00000020,    ///< Simulation mode packet
        Offline = 0x00000040,       ///< Offline/replay packet
        Network = 0x00000080,       ///< Network-received packet
        
        // User-defined flags (bits 8-15)
        UserFlag0 = 0x00000100,
        UserFlag1 = 0x00000200,
        UserFlag2 = 0x00000400,
        UserFlag3 = 0x00000800,
        UserFlag4 = 0x00001000,
        UserFlag5 = 0x00002000,
        UserFlag6 = 0x00004000,
        UserFlag7 = 0x00008000,
        
        // Reserved for future use (bits 16-31)
        Reserved = 0xFFFF0000
    };
    
    PacketHeader() 
        : id(0)
        , sequence(0)
        , timestamp(0)
        , payloadSize(0)
        , flags(Flags::None)
    {
    }
    
    PacketHeader(PacketId packetId, SequenceNumber seq = 0, uint32_t size = 0, uint32_t headerFlags = Flags::None)
        : id(packetId)
        , sequence(seq)
        , timestamp(getCurrentTimestampNs())
        , payloadSize(size)
        , flags(headerFlags)
    {
    }
    
    /**
     * @brief Check if a specific flag is set
     */
    bool hasFlag(Flags flag) const {
        return (flags & static_cast<uint32_t>(flag)) != 0;
    }
    
    /**
     * @brief Set a specific flag
     */
    void setFlag(Flags flag) {
        flags |= static_cast<uint32_t>(flag);
    }
    
    /**
     * @brief Clear a specific flag
     */
    void clearFlag(Flags flag) {
        flags &= ~static_cast<uint32_t>(flag);
    }
    
    /**
     * @brief Get current timestamp in nanoseconds
     */
    static uint64_t getCurrentTimestampNs() {
        auto now = std::chrono::high_resolution_clock::now();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch()).count()
        );
    }
    
    /**
     * @brief Convert timestamp to time_point
     */
    std::chrono::high_resolution_clock::time_point getTimestamp() const {
        return std::chrono::high_resolution_clock::time_point{
            std::chrono::nanoseconds{timestamp}
        };
    }
    
    /**
     * @brief Get packet age in nanoseconds
     */
    uint64_t getAgeNs() const {
        return getCurrentTimestampNs() - timestamp;
    }
    
    /**
     * @brief Validate header integrity
     */
    bool isValid() const {
        // Basic validation checks
        return timestamp > 0 &&                      // Must have valid timestamp
               payloadSize <= MAX_PAYLOAD_SIZE &&
               (flags & Reserved) == 0;
    }
    
    // Constants
    static constexpr uint32_t MAX_PAYLOAD_SIZE = 64 * 1024; // 64KB max payload
    
} __attribute__((packed)); // Ensure no padding

static_assert(sizeof(PacketHeader) == 24, "PacketHeader must be exactly 24 bytes");

// Header size constant (must be defined after struct)
static constexpr size_t PACKET_HEADER_SIZE = 24;

} // namespace Packet
} // namespace Monitor