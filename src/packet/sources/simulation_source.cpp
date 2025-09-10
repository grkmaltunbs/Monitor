#include "simulation_source.h"
#include "test_packet_structures.h"
#include "../core/packet_factory.h"
#include "../../events/event_dispatcher.h"
#include "../../logging/logger.h"

#include <QTimer>
#include <QtCore/QLoggingCategory>
#include <cstring>

Q_LOGGING_CATEGORY(simulationSource, "Monitor.Packet.SimulationSource")

namespace Monitor {
namespace Packet {

SimulationSource::SimulationSource(const SimulationConfig& config, QObject* parent)
    : PacketSource(config, parent)
    , m_simConfig(config)
    , m_randomGenerator(std::random_device{}())
    , m_uniformDist(0.0, 1.0)
    , m_normalDist(0.0, 1.0)
    , m_globalCounter(0)
    , m_simulationStart(std::chrono::steady_clock::now())
{
    // Initialize counters and phases for all packet types
    for (const auto& packetType : m_simConfig.packetTypes) {
        m_counters[packetType.id] = 0;
        m_phases[packetType.id] = 0.0;
    }
    
    qCInfo(simulationSource) << "SimulationSource created with" 
                           << m_simConfig.packetTypes.size() << "packet types";
}

SimulationSource::~SimulationSource()
{
    stop();
}

bool SimulationSource::doStart()
{
    if (getState() == State::Running) {
        qCWarning(simulationSource) << "Simulation source already running";
        return false;
    }
    
    qCInfo(simulationSource) << "Starting simulation with" << m_simConfig.packetTypes.size() << "packet types";
    
    // Initialize simulation state
    m_simulationStart = std::chrono::steady_clock::now();
    m_globalCounter = 0;
    
    // Clear existing timers
    m_packetTimers.clear();
    
    // Create timers for each packet type
    for (const auto& packetType : m_simConfig.packetTypes) {
        if (!packetType.enabled) {
            continue;
        }
        
        auto timer = std::make_unique<QTimer>();
        timer->setInterval(static_cast<int>(packetType.intervalMs));
        
        // Connect timer to packet generation
        connect(timer.get(), &QTimer::timeout, this, [this, packetType]() {
            generatePacket(packetType);
        });
        
        timer->start();
        m_packetTimers.push_back(std::move(timer));
        
        qCInfo(simulationSource) << "Started timer for packet type" << packetType.name.c_str() 
                                << "ID:" << packetType.id << "interval:" << packetType.intervalMs << "ms";
    }
    
    // Set up duration timer if specified
    if (m_simConfig.totalDurationMs > 0) {
        m_durationTimer = std::make_unique<QTimer>();
        m_durationTimer->setSingleShot(true);
        m_durationTimer->setInterval(static_cast<int>(m_simConfig.totalDurationMs));
        
        connect(m_durationTimer.get(), &QTimer::timeout, this, [this]() {
            qCInfo(simulationSource) << "Simulation duration expired, stopping";
            stop();
        });
        
        m_durationTimer->start();
    }
    
    qCInfo(simulationSource) << "Simulation source started successfully";
    return true;
}

void SimulationSource::doStop()
{
    qCInfo(simulationSource) << "Stopping simulation source";
    
    // Stop all timers
    for (auto& timer : m_packetTimers) {
        if (timer) {
            timer->stop();
        }
    }
    m_packetTimers.clear();
    
    if (m_durationTimer) {
        m_durationTimer->stop();
        m_durationTimer.reset();
    }
    
    qCInfo(simulationSource) << "Simulation source stopped";
}

void SimulationSource::doPause()
{
    qCInfo(simulationSource) << "Pausing simulation source";
    
    // Pause all timers
    for (auto& timer : m_packetTimers) {
        if (timer) {
            timer->stop();
        }
    }
    
    if (m_durationTimer) {
        m_durationTimer->stop();
    }
}

bool SimulationSource::doResume()
{
    qCInfo(simulationSource) << "Resuming simulation source";
    
    // Resume all timers
    for (size_t i = 0; i < m_packetTimers.size() && i < m_simConfig.packetTypes.size(); ++i) {
        if (m_simConfig.packetTypes[i].enabled && m_packetTimers[i]) {
            m_packetTimers[i]->start();
        }
    }
    
    if (m_durationTimer) {
        m_durationTimer->start();
    }
    
    return true;
}

void SimulationSource::generatePacket(const PacketTypeConfig& config)
{
    if (!m_packetFactory) {
        qCWarning(simulationSource) << "No packet factory available";
        return;
    }
    
    // Generate packet data based on the packet type
    QByteArray packetData;
    
    if (config.id == TestStructures::SIGNAL_TEST_PACKET_ID) {
        packetData = generateSignalTestPacket(config);
    } else if (config.id == TestStructures::MOTION_TEST_PACKET_ID) {
        packetData = generateMotionTestPacket(config);
    } else if (config.id == TestStructures::SYSTEM_TEST_PACKET_ID) {
        packetData = generateSystemTestPacket(config);
    } else {
        // Generate generic packet
        packetData = generateGenericPacket(config);
    }
    
    if (packetData.isEmpty()) {
        qCWarning(simulationSource) << "Failed to generate packet data for type" << config.id;
        return;
    }
    
    // Create packet using factory
    auto result = m_packetFactory->createFromRawData(
        reinterpret_cast<const uint8_t*>(packetData.constData()),
        static_cast<size_t>(packetData.size())
    );
    
    if (!result.success) {
        qCWarning(simulationSource) << "Failed to create packet:" << result.error.c_str();
        m_stats.errorCount++;
        return;
    }
    
    // Update statistics
    m_stats.packetsGenerated++;
    m_stats.bytesGenerated += packetData.size();
    m_stats.lastPacketTime = std::chrono::steady_clock::now();
    
    // Deliver packet
    if (m_packetCallback) {
        m_packetCallback(result.packet);
    }
    
    // Emit signal
    emit packetReady(result.packet);
    
    // Update delivery statistics
    m_stats.packetsDelivered++;
    
    m_globalCounter++;
}

QByteArray SimulationSource::generateSignalTestPacket(const PacketTypeConfig& config)
{
    TestStructures::SignalTestPacket packet;
    
    // Update sequence number
    packet.header.sequence = static_cast<uint32_t>(m_counters[config.id]++);
    packet.header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Get time since simulation start in seconds
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_simulationStart).count() / 1000.0;
    
    // Generate pattern-based data
    double phase = m_phases[config.id];
    double frequency = config.frequency;
    double amplitude = config.amplitude;
    double offset = config.offset;
    
    // Generate sine and cosine waves
    packet.sine_wave = static_cast<float>(amplitude * std::sin(2 * M_PI * frequency * elapsed + phase) + offset);
    packet.cosine_wave = static_cast<float>(amplitude * std::cos(2 * M_PI * frequency * elapsed + phase) + offset);
    
    // Generate ramp (sawtooth) pattern
    double rampPhase = std::fmod(frequency * elapsed + phase / (2 * M_PI), 1.0);
    packet.ramp = static_cast<float>(amplitude * (2 * rampPhase - 1) + offset);
    
    // Simple counter
    packet.counter = static_cast<uint32_t>(m_counters[config.id]);
    
    // Toggle boolean every 10 packets
    packet.toggle = (m_counters[config.id] % 20) < 10;
    
    return QByteArray(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

QByteArray SimulationSource::generateMotionTestPacket(const PacketTypeConfig& config)
{
    TestStructures::MotionTestPacket packet;
    
    // Update sequence number
    packet.header.sequence = static_cast<uint32_t>(m_counters[config.id]++);
    packet.header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Get time since simulation start in seconds
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_simulationStart).count() / 1000.0;
    
    double phase = m_phases[config.id];
    double frequency = config.frequency;
    double amplitude = config.amplitude;
    
    // Generate circular motion
    packet.x = static_cast<float>(amplitude * std::cos(2 * M_PI * frequency * elapsed + phase));
    packet.y = static_cast<float>(amplitude * std::sin(2 * M_PI * frequency * elapsed + phase));
    packet.z = static_cast<float>(amplitude * 0.5 * std::sin(4 * M_PI * frequency * elapsed)); // Vertical oscillation
    
    // Generate velocity (derivative of position)
    packet.vx = static_cast<float>(-amplitude * 2 * M_PI * frequency * std::sin(2 * M_PI * frequency * elapsed + phase));
    packet.vy = static_cast<float>(amplitude * 2 * M_PI * frequency * std::cos(2 * M_PI * frequency * elapsed + phase));
    packet.vz = static_cast<float>(amplitude * 0.5 * 4 * M_PI * frequency * std::cos(4 * M_PI * frequency * elapsed));
    
    // Generate acceleration (derivative of velocity)  
    packet.ax = static_cast<float>(-amplitude * std::pow(2 * M_PI * frequency, 2) * std::cos(2 * M_PI * frequency * elapsed + phase));
    packet.ay = static_cast<float>(-amplitude * std::pow(2 * M_PI * frequency, 2) * std::sin(2 * M_PI * frequency * elapsed + phase));
    packet.az = static_cast<float>(-amplitude * 0.5 * std::pow(4 * M_PI * frequency, 2) * std::sin(4 * M_PI * frequency * elapsed));
    
    return QByteArray(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

QByteArray SimulationSource::generateSystemTestPacket(const PacketTypeConfig& config)
{
    TestStructures::SystemTestPacket packet;
    
    // Update sequence number
    packet.header.sequence = static_cast<uint32_t>(m_counters[config.id]++);
    packet.header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    uint32_t counter = static_cast<uint32_t>(m_counters[config.id]);
    
    // Generate rotating status flags (0-255)
    packet.status_flags = counter % 256;
    
    // Generate occasional error codes
    packet.error_code = (counter % 100 == 0) ? (counter / 100) % 256 : 0;
    
    // Generate warning flags with some pattern
    packet.warning_flags = static_cast<uint16_t>((counter % 16) << ((counter / 16) % 12));
    
    // Generate temperature data (20°C to 80°C with some variation)
    for (int i = 0; i < 4; i++) {
        double variation = 5.0 * std::sin(2 * M_PI * (counter + i * 90) / 360.0); // 360-step cycle
        packet.temperatures[i] = 25.0f + static_cast<float>(i * 15.0 + variation);
    }
    
    // Generate voltage data (4.5V to 5.5V with some variation)
    for (int i = 0; i < 4; i++) {
        double variation = 0.2 * std::cos(2 * M_PI * (counter + i * 45) / 180.0); // 180-step cycle
        packet.voltages[i] = 5.0f + static_cast<float>(variation);
    }
    
    return QByteArray(reinterpret_cast<const char*>(&packet), sizeof(packet));
}

QByteArray SimulationSource::generateGenericPacket(const PacketTypeConfig& config)
{
    // Create a simple packet header + payload structure
    size_t totalSize = sizeof(TestStructures::TestHeader) + config.payloadSize;
    QByteArray data(static_cast<int>(totalSize), 0);
    
    // Fill header
    TestStructures::TestHeader* header = reinterpret_cast<TestStructures::TestHeader*>(data.data());
    header->packet_id = config.id;
    header->sequence = static_cast<uint32_t>(m_counters[config.id]++);
    header->timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Fill payload with pattern data
    uint8_t* payload = reinterpret_cast<uint8_t*>(data.data() + sizeof(TestStructures::TestHeader));
    generatePatternData(payload, config.payloadSize, config);
    
    return data;
}

void SimulationSource::generatePatternData(uint8_t* data, size_t size, const PacketTypeConfig& config)
{
    double phase = m_phases[config.id];
    uint32_t counter = static_cast<uint32_t>(m_counters[config.id]);
    
    switch (config.pattern) {
        case PatternType::Constant:
            std::fill(data, data + size, static_cast<uint8_t>(config.offset));
            break;
            
        case PatternType::Linear:
            for (size_t i = 0; i < size; ++i) {
                data[i] = static_cast<uint8_t>((i * config.amplitude + config.offset)) % 256;
            }
            break;
            
        case PatternType::Counter:
            for (size_t i = 0; i < size; ++i) {
                data[i] = static_cast<uint8_t>((counter + i) % 256);
            }
            break;
            
        case PatternType::Random:
            for (size_t i = 0; i < size; ++i) {
                data[i] = static_cast<uint8_t>(m_uniformDist(m_randomGenerator) * 255);
            }
            break;
            
        case PatternType::Sine:
            for (size_t i = 0; i < size; ++i) {
                double value = config.amplitude * std::sin(2 * M_PI * config.frequency * i / size + phase) + config.offset;
                data[i] = static_cast<uint8_t>(std::max(0.0, std::min(255.0, value)));
            }
            break;
            
        case PatternType::Square:
            for (size_t i = 0; i < size; ++i) {
                double phase_val = std::fmod(config.frequency * i / size + phase / (2 * M_PI), 1.0);
                data[i] = static_cast<uint8_t>((phase_val < 0.5) ? config.offset : config.offset + config.amplitude);
            }
            break;
            
        default:
            // Fill with counter pattern as fallback
            for (size_t i = 0; i < size; ++i) {
                data[i] = static_cast<uint8_t>((counter + i) % 256);
            }
            break;
    }
}

// Static factory methods for creating common simulation configurations
SimulationSource::SimulationConfig SimulationSource::createDefaultConfig()
{
    SimulationConfig config("Default Simulation");
    
    // Add signal test packet - 10 Hz
    config.packetTypes.emplace_back(
        TestStructures::SIGNAL_TEST_PACKET_ID, 
        "Signal Test", 
        TestStructures::SIGNAL_TEST_PACKET_SIZE,
        100, // 100ms = 10 Hz
        PatternType::Sine
    );
    config.packetTypes.back().frequency = 0.5; // 0.5 Hz sine wave
    config.packetTypes.back().amplitude = 10.0;
    
    // Add motion test packet - 5 Hz
    config.packetTypes.emplace_back(
        TestStructures::MOTION_TEST_PACKET_ID,
        "Motion Test",
        TestStructures::MOTION_TEST_PACKET_SIZE, 
        200, // 200ms = 5 Hz
        PatternType::Sine
    );
    config.packetTypes.back().frequency = 0.2; // 0.2 Hz circular motion
    config.packetTypes.back().amplitude = 5.0;
    
    // Add system test packet - 1 Hz
    config.packetTypes.emplace_back(
        TestStructures::SYSTEM_TEST_PACKET_ID,
        "System Test",
        TestStructures::SYSTEM_TEST_PACKET_SIZE,
        1000, // 1000ms = 1 Hz
        PatternType::Counter
    );
    
    return config;
}

SimulationSource::SimulationConfig SimulationSource::createStressTestConfig()
{
    SimulationConfig config("Stress Test");
    
    // High-frequency signal packets - 100 Hz
    config.packetTypes.emplace_back(
        TestStructures::SIGNAL_TEST_PACKET_ID,
        "High Rate Signal",
        TestStructures::SIGNAL_TEST_PACKET_SIZE,
        10, // 10ms = 100 Hz
        PatternType::Sine
    );
    config.packetTypes.back().frequency = 2.0; // 2 Hz sine wave
    config.packetTypes.back().amplitude = 50.0;
    
    // Medium-frequency motion packets - 50 Hz
    config.packetTypes.emplace_back(
        TestStructures::MOTION_TEST_PACKET_ID,
        "High Rate Motion",
        TestStructures::MOTION_TEST_PACKET_SIZE,
        20, // 20ms = 50 Hz
        PatternType::Sine
    );
    config.packetTypes.back().frequency = 1.0; // 1 Hz motion
    config.packetTypes.back().amplitude = 100.0;
    
    // System status - 10 Hz
    config.packetTypes.emplace_back(
        TestStructures::SYSTEM_TEST_PACKET_ID,
        "System Status",
        TestStructures::SYSTEM_TEST_PACKET_SIZE,
        100, // 100ms = 10 Hz
        PatternType::Random
    );
    
    // Enable timing jitter for more realistic simulation
    config.randomizeTimings = true;
    config.timingJitterMs = 2; // ±2ms jitter
    
    return config;
}

} // namespace Packet
} // namespace Monitor