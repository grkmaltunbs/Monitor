#pragma once

#include "packet_source.h"
#include <QtCore/QTimer>
#include <QString>
#include <random>
#include <vector>
#include <cmath>

namespace Monitor {
namespace Packet {

/**
 * @brief Simulation packet source for testing and development
 * 
 * This source generates synthetic packets with configurable patterns,
 * data types, and transmission rates. It's designed for system testing,
 * performance benchmarking, and demonstration purposes.
 */
class SimulationSource : public PacketSource {
    Q_OBJECT
    
public:
    /**
     * @brief Data generation pattern types
     */
    enum class PatternType {
        Constant,       ///< Constant values
        Linear,         ///< Linear ramp
        Sine,           ///< Sine wave
        Cosine,         ///< Cosine wave
        Square,         ///< Square wave
        Sawtooth,       ///< Sawtooth wave
        Random,         ///< Random values
        Counter,        ///< Incrementing counter
        Bitfield        ///< Rotating bit patterns
    };
    
    /**
     * @brief Packet type definition for simulation
     */
    struct PacketTypeConfig {
        PacketId id;
        std::string name;
        size_t payloadSize;
        uint32_t intervalMs;        ///< Generation interval in milliseconds
        PatternType pattern;
        double amplitude = 1.0;     ///< Pattern amplitude
        double frequency = 1.0;     ///< Pattern frequency (Hz)
        double offset = 0.0;        ///< Pattern offset
        bool enabled = true;        ///< Enable/disable this packet type
        
        PacketTypeConfig(PacketId packetId, const std::string& typeName, 
                        size_t size, uint32_t interval, PatternType pat = PatternType::Random)
            : id(packetId), name(typeName), payloadSize(size), intervalMs(interval), pattern(pat)
        {
        }
    };
    
    /**
     * @brief Simulation configuration
     */
    struct SimulationConfig : public Configuration {
        std::vector<PacketTypeConfig> packetTypes;
        uint32_t totalDurationMs = 0;      ///< Total simulation duration (0 = unlimited)
        uint32_t burstSize = 1;            ///< Packets per burst
        uint32_t burstIntervalMs = 0;      ///< Interval between bursts (0 = no bursts)
        bool randomizeTimings = false;     ///< Add timing jitter
        uint32_t timingJitterMs = 0;       ///< Maximum timing jitter in ms
        
        SimulationConfig(const std::string& name) : Configuration(name) {}
    };

private:
    SimulationConfig m_simConfig;
    std::vector<std::unique_ptr<QTimer>> m_packetTimers;
    std::unique_ptr<QTimer> m_durationTimer;
    
    // Pattern generation state
    std::mt19937 m_randomGenerator;
    std::uniform_real_distribution<double> m_uniformDist;
    std::normal_distribution<double> m_normalDist;
    
    std::unordered_map<PacketId, uint64_t> m_counters;
    std::unordered_map<PacketId, double> m_phases;
    
    uint64_t m_globalCounter;
    std::chrono::steady_clock::time_point m_simulationStart;

public:
    explicit SimulationSource(const SimulationConfig& config, QObject* parent = nullptr);
    ~SimulationSource() override;
    
    /**
     * @brief Add packet type to simulation
     */
    void addPacketType(const PacketTypeConfig& packetType) {
        m_simConfig.packetTypes.push_back(packetType);
        m_counters[packetType.id] = 0;
        m_phases[packetType.id] = 0.0;
    }
    
    /**
     * @brief Remove packet type from simulation
     */
    void removePacketType(PacketId id) {
        auto it = std::remove_if(m_simConfig.packetTypes.begin(), m_simConfig.packetTypes.end(),
            [id](const PacketTypeConfig& config) { return config.id == id; });
        m_simConfig.packetTypes.erase(it, m_simConfig.packetTypes.end());
        
        m_counters.erase(id);
        m_phases.erase(id);
    }
    
    /**
     * @brief Enable/disable specific packet type
     */
    void enablePacketType(PacketId id, bool enabled) {
        for (auto& packetType : m_simConfig.packetTypes) {
            if (packetType.id == id) {
                packetType.enabled = enabled;
                break;
            }
        }
    }
    
    /**
     * @brief Get simulation configuration
     */
    const SimulationConfig& getSimulationConfig() const {
        return m_simConfig;
    }
    
    /**
     * @brief Reset simulation counters and state
     */
    void resetSimulation() {
        for (auto& pair : m_counters) {
            pair.second = 0;
        }
        for (auto& pair : m_phases) {
            pair.second = 0.0;
        }
        m_globalCounter = 0;
        // Reset statistics individually since atomic members can't be copy-assigned
        m_stats.packetsGenerated = 0;
        m_stats.packetsDelivered = 0;
        m_stats.packetsDropped = 0;
        m_stats.bytesGenerated = 0;
        m_stats.errorCount = 0;
    }

    // Static factory methods for creating common simulation configurations
    static SimulationConfig createDefaultConfig();
    static SimulationConfig createStressTestConfig();

protected:
    bool doStart() override;
    void doStop() override;
    void doPause() override;
    bool doResume() override;

private:
    void generatePacket(const PacketTypeConfig& config);
    
    // Packet generation methods for specific test packets
    QByteArray generateSignalTestPacket(const PacketTypeConfig& config);
    QByteArray generateMotionTestPacket(const PacketTypeConfig& config);
    QByteArray generateSystemTestPacket(const PacketTypeConfig& config);
    QByteArray generateGenericPacket(const PacketTypeConfig& config);
    
    // Pattern data generation
    void generatePatternData(uint8_t* data, size_t size, const PacketTypeConfig& config);

};

} // namespace Packet
} // namespace Monitor