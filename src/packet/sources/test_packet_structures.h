#pragma once

#include "../core/packet_header.h"
#include <cstdint>

namespace Monitor {
namespace Packet {
namespace TestStructures {

// Pre-defined test structures for immediate use during development
// These structures are designed for simulation mode only

/**
 * @brief Standard test header for all simulation packets
 */
struct TestHeader {
    uint32_t packet_id;
    uint32_t sequence;
    uint64_t timestamp;
    
    TestHeader(uint32_t id = 0, uint32_t seq = 0) 
        : packet_id(id), sequence(seq), timestamp(0) {}
} __attribute__((packed));

/**
 * @brief Signal test packet - ID: 1001
 * Contains various mathematical patterns for testing chart widgets
 */
struct SignalTestPacket {
    TestHeader header;
    float sine_wave;        // Sine wave pattern
    float cosine_wave;      // Cosine wave pattern  
    float ramp;             // Linear ramp pattern
    uint32_t counter;       // Incrementing counter
    bool toggle;            // Boolean toggle pattern
    
    SignalTestPacket() : header(1001, 0), sine_wave(0.0f), cosine_wave(0.0f), 
                        ramp(0.0f), counter(0), toggle(false) {}
} __attribute__((packed));

/**
 * @brief Motion test packet - ID: 1002
 * Contains 3D motion data with position, velocity, and acceleration
 */
struct MotionTestPacket {
    TestHeader header;
    float x, y, z;          // Position coordinates
    float vx, vy, vz;       // Velocity components
    float ax, ay, az;       // Acceleration components
    
    MotionTestPacket() : header(1002, 0), 
                        x(0.0f), y(0.0f), z(0.0f),
                        vx(0.0f), vy(0.0f), vz(0.0f),
                        ax(0.0f), ay(0.0f), az(0.0f) {}
} __attribute__((packed));

/**
 * @brief System test packet - ID: 1003
 * Contains system status information with bitfields and arrays
 */
struct SystemTestPacket {
    TestHeader header;
    uint32_t status_flags : 8;    // System status flags (8 bits)
    uint32_t error_code : 8;      // Error code (8 bits)  
    uint32_t warning_flags : 16;  // Warning flags (16 bits)
    float temperatures[4];        // Temperature sensors
    float voltages[4];           // Voltage readings
    
    SystemTestPacket() : header(1003, 0), 
                        status_flags(0), error_code(0), warning_flags(0) {
        for (int i = 0; i < 4; i++) {
            temperatures[i] = 25.0f; // Default 25°C
            voltages[i] = 5.0f;      // Default 5V
        }
    }
} __attribute__((packed));

// Packet ID constants for easy reference
constexpr PacketId SIGNAL_TEST_PACKET_ID = 1001;
constexpr PacketId MOTION_TEST_PACKET_ID = 1002;
constexpr PacketId SYSTEM_TEST_PACKET_ID = 1003;

// Size constants
constexpr size_t SIGNAL_TEST_PACKET_SIZE = sizeof(SignalTestPacket);
constexpr size_t MOTION_TEST_PACKET_SIZE = sizeof(MotionTestPacket);
constexpr size_t SYSTEM_TEST_PACKET_SIZE = sizeof(SystemTestPacket);

} // namespace TestStructures
} // namespace Packet  
} // namespace Monitor