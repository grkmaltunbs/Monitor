#include <gtest/gtest.h>
#include <QApplication>
#include <QTimer>
#include <QEventLoop>
#include <QSignalSpy>
#include <memory>

#include "../../src/packet/sources/simulation_source.h"
#include "../../src/packet/sources/test_packet_structures.h"
#include "../../src/packet/core/packet_factory.h"
#include "../../src/memory/memory_pool_manager.h"
#include "../../src/events/event_dispatcher.h"
#include "../../src/logging/logger.h"

using namespace Monitor;

class PacketPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize Qt application if needed
        if (!QApplication::instance()) {
            int argc = 1;
            char* argv[] = {const_cast<char*>("test")};
            app = new QApplication(argc, argv);
        }

        // Initialize core components
        memoryManager = std::make_unique<Memory::MemoryPoolManager>();
        eventDispatcher = std::make_unique<Events::EventDispatcher>();
        logger = Logging::Logger::instance();
        packetFactory = std::make_unique<Packet::PacketFactory>(memoryManager.get());
    }

    void TearDown() override {
        simulationSource.reset();
        packetFactory.reset();
        eventDispatcher.reset();
        memoryManager.reset();
    }

    std::unique_ptr<Memory::MemoryPoolManager> memoryManager;
    std::unique_ptr<Events::EventDispatcher> eventDispatcher;
    std::unique_ptr<Packet::PacketFactory> packetFactory;
    std::shared_ptr<Packet::SimulationSource> simulationSource;
    Logging::Logger* logger;
    QApplication* app = nullptr;
};

TEST_F(PacketPipelineTest, SignalTestPacketGeneration) {
    // Create simulation config with SignalTestPacket
    auto config = Packet::SimulationSource::createDefaultConfig();
    
    // Add SignalTestPacket configuration
    Packet::SimulationSource::PacketTypeConfig signalConfig;
    signalConfig.id = 1001; // SIGNAL_TEST_PACKET_ID
    signalConfig.name = "SignalTestPacket";
    signalConfig.size = sizeof(SignalTestPacket);
    signalConfig.rate = 10.0; // 10 Hz
    signalConfig.enabled = true;
    
    config.packetTypes.clear();
    config.packetTypes.push_back(signalConfig);

    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Track generated packets
    std::vector<PacketPtr> generatedPackets;
    connect(simulationSource.get(), &Packet::PacketSource::packetReady,
            [&](PacketPtr packet) {
                generatedPackets.push_back(packet);
            });

    // Start simulation
    ASSERT_TRUE(simulationSource->start());

    // Wait for packets
    QEventLoop loop;
    QTimer::singleShot(1500, &loop, &QEventLoop::quit); // 1.5 seconds
    loop.exec();

    simulationSource->stop();

    // Verify packets were generated
    EXPECT_GT(generatedPackets.size(), 5); // Should have multiple packets

    // Analyze first few packets for pattern validation
    for (size_t i = 0; i < std::min(generatedPackets.size(), size_t(5)); ++i) {
        auto packet = generatedPackets[i];
        ASSERT_TRUE(packet != nullptr);
        
        auto header = packet->getHeader();
        EXPECT_EQ(header.packet_id, 1001u);
        EXPECT_EQ(packet->dataSize(), sizeof(SignalTestPacket) - sizeof(TestHeader));
        
        // Verify packet contains expected data structure
        EXPECT_GE(packet->totalSize(), sizeof(SignalTestPacket));
    }

    logger->info("PacketPipeline", 
                QString("Generated %1 SignalTestPackets in 1.5 seconds")
                .arg(generatedPackets.size()));
}

TEST_F(PacketPipelineTest, MotionTestPacketGeneration) {
    // Create simulation config with MotionTestPacket
    auto config = Packet::SimulationSource::createDefaultConfig();
    
    Packet::SimulationSource::PacketTypeConfig motionConfig;
    motionConfig.id = 1002; // MOTION_TEST_PACKET_ID
    motionConfig.name = "MotionTestPacket";
    motionConfig.size = sizeof(MotionTestPacket);
    motionConfig.rate = 20.0; // 20 Hz
    motionConfig.enabled = true;
    
    config.packetTypes.clear();
    config.packetTypes.push_back(motionConfig);

    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Track packets with motion data analysis
    std::vector<MotionTestPacket> motionData;
    connect(simulationSource.get(), &Packet::PacketSource::packetReady,
            [&](PacketPtr packet) {
                if (packet->getHeader().packet_id == 1002) {
                    // Extract motion data for analysis
                    const char* data = packet->getData();
                    if (packet->dataSize() >= sizeof(MotionTestPacket) - sizeof(TestHeader)) {
                        MotionTestPacket motionPacket;
                        std::memcpy(&motionPacket.header, &packet->getHeader(), sizeof(TestHeader));
                        std::memcpy(&motionPacket.x, data, packet->dataSize());
                        motionData.push_back(motionPacket);
                    }
                }
            });

    // Start simulation
    ASSERT_TRUE(simulationSource->start());

    // Wait for motion packets
    QEventLoop loop;
    QTimer::singleShot(1000, &loop, &QEventLoop::quit); // 1 second
    loop.exec();

    simulationSource->stop();

    // Verify motion packets were generated
    EXPECT_GT(motionData.size(), 10); // Should have multiple motion packets

    // Analyze motion data patterns
    if (motionData.size() >= 2) {
        float totalDeltaX = 0.0f;
        for (size_t i = 1; i < motionData.size(); ++i) {
            float deltaX = std::abs(motionData[i].x - motionData[i-1].x);
            totalDeltaX += deltaX;
        }
        
        // Motion data should show variation (not constant)
        EXPECT_GT(totalDeltaX, 0.1f) << "Motion data should vary between packets";
    }

    logger->info("PacketPipeline",
                QString("Generated %1 MotionTestPackets with motion analysis")
                .arg(motionData.size()));
}

TEST_F(PacketPipelineTest, SystemTestPacketGeneration) {
    // Create simulation config with SystemTestPacket
    auto config = Packet::SimulationSource::createDefaultConfig();
    
    Packet::SimulationSource::PacketTypeConfig systemConfig;
    systemConfig.id = 1003; // SYSTEM_TEST_PACKET_ID
    systemConfig.name = "SystemTestPacket";
    systemConfig.size = sizeof(SystemTestPacket);
    systemConfig.rate = 5.0; // 5 Hz
    systemConfig.enabled = true;
    
    config.packetTypes.clear();
    config.packetTypes.push_back(systemConfig);

    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Track system packets
    std::vector<PacketPtr> systemPackets;
    connect(simulationSource.get(), &Packet::PacketSource::packetReady,
            [&](PacketPtr packet) {
                if (packet->getHeader().packet_id == 1003) {
                    systemPackets.push_back(packet);
                }
            });

    // Start simulation
    ASSERT_TRUE(simulationSource->start());

    // Wait for system packets
    QEventLoop loop;
    QTimer::singleShot(2000, &loop, &QEventLoop::quit); // 2 seconds
    loop.exec();

    simulationSource->stop();

    // Verify system packets were generated
    EXPECT_GT(systemPackets.size(), 5); // Should have multiple system packets

    // Verify packet structure
    for (auto packet : systemPackets) {
        EXPECT_EQ(packet->getHeader().packet_id, 1003u);
        EXPECT_GE(packet->totalSize(), sizeof(SystemTestPacket));
    }

    logger->info("PacketPipeline",
                QString("Generated %1 SystemTestPackets")
                .arg(systemPackets.size()));
}

TEST_F(PacketPipelineTest, MultiPacketTypeSimulation) {
    // Create comprehensive simulation with all packet types
    auto config = Packet::SimulationSource::createStressTestConfig();

    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Track all packet types
    std::map<uint32_t, int> packetCounts;
    std::map<uint32_t, size_t> totalBytes;

    connect(simulationSource.get(), &Packet::PacketSource::packetReady,
            [&](PacketPtr packet) {
                uint32_t id = packet->getHeader().packet_id;
                packetCounts[id]++;
                totalBytes[id] += packet->totalSize();
            });

    // Start comprehensive simulation
    ASSERT_TRUE(simulationSource->start());

    // Run for extended period
    QEventLoop loop;
    QTimer::singleShot(3000, &loop, &QEventLoop::quit); // 3 seconds
    loop.exec();

    simulationSource->stop();

    // Verify multiple packet types were generated
    EXPECT_GE(packetCounts.size(), 2); // At least 2 different packet types

    // Verify reasonable packet distribution
    int totalPackets = 0;
    for (const auto& [id, count] : packetCounts) {
        EXPECT_GT(count, 0) << "Packet type " << id << " should generate packets";
        totalPackets += count;
        
        logger->info("PacketPipeline",
                    QString("Packet type %1: %2 packets, %3 bytes total")
                    .arg(id).arg(count).arg(totalBytes[id]));
    }

    EXPECT_GT(totalPackets, 50) << "Should generate substantial number of packets";

    // Verify simulation statistics
    auto stats = simulationSource->getStatistics();
    EXPECT_GT(stats.packetsDelivered.load(), 0u);
    EXPECT_GT(stats.bytesGenerated.load(), 0u);
    
    logger->info("PacketPipeline",
                QString("Simulation stats: %1 packets delivered, %2 bytes generated")
                .arg(stats.packetsDelivered.load())
                .arg(stats.bytesGenerated.load()));
}

TEST_F(PacketPipelineTest, PacketTimingAndSequencing) {
    // Create simulation with known timing
    auto config = Packet::SimulationSource::createDefaultConfig();
    config.basePacketRate = 100.0; // 100 Hz for precise timing
    
    Packet::SimulationSource::PacketTypeConfig timingConfig;
    timingConfig.id = 1001;
    timingConfig.name = "TimingTestPacket";
    timingConfig.size = sizeof(SignalTestPacket);
    timingConfig.rate = 50.0; // 50 Hz
    timingConfig.enabled = true;
    
    config.packetTypes.clear();
    config.packetTypes.push_back(timingConfig);

    simulationSource = std::make_shared<Packet::SimulationSource>(config);
    simulationSource->setPacketFactory(packetFactory.get());
    simulationSource->setEventDispatcher(eventDispatcher.get());

    // Track packet timing
    struct TimingInfo {
        uint32_t sequence;
        std::chrono::steady_clock::time_point receivedTime;
    };
    std::vector<TimingInfo> timingData;
    auto startTime = std::chrono::steady_clock::now();

    connect(simulationSource.get(), &Packet::PacketSource::packetReady,
            [&](PacketPtr packet) {
                TimingInfo info;
                info.sequence = packet->getHeader().sequence_number;
                info.receivedTime = std::chrono::steady_clock::now();
                timingData.push_back(info);
            });

    // Start timing test
    ASSERT_TRUE(simulationSource->start());

    // Wait for timing data
    QEventLoop loop;
    QTimer::singleShot(2000, &loop, &QEventLoop::quit); // 2 seconds
    loop.exec();

    simulationSource->stop();

    // Analyze timing and sequencing
    EXPECT_GT(timingData.size(), 50); // Should have substantial timing data

    if (timingData.size() >= 10) {
        // Check sequence number progression
        bool sequenceIncreasing = true;
        for (size_t i = 1; i < std::min(timingData.size(), size_t(20)); ++i) {
            if (timingData[i].sequence <= timingData[i-1].sequence) {
                sequenceIncreasing = false;
                break;
            }
        }
        EXPECT_TRUE(sequenceIncreasing) << "Sequence numbers should increase";

        // Calculate average timing intervals
        std::vector<double> intervals;
        for (size_t i = 1; i < timingData.size(); ++i) {
            auto interval = std::chrono::duration_cast<std::chrono::microseconds>(
                timingData[i].receivedTime - timingData[i-1].receivedTime).count();
            intervals.push_back(interval);
        }

        if (!intervals.empty()) {
            double avgInterval = 0.0;
            for (double interval : intervals) {
                avgInterval += interval;
            }
            avgInterval /= intervals.size();
            
            // Expected interval for 50 Hz: 20000 microseconds (20ms)
            double expectedInterval = 1000000.0 / 50.0; // 20000 μs
            double tolerance = expectedInterval * 0.5; // 50% tolerance
            
            EXPECT_NEAR(avgInterval, expectedInterval, tolerance)
                << "Average packet interval should be approximately " << expectedInterval << " μs";
                
            logger->info("PacketPipeline",
                        QString("Timing analysis: avg interval = %1 μs (expected %2 μs)")
                        .arg(avgInterval).arg(expectedInterval));
        }
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    QApplication app(argc, argv);
    
    return RUN_ALL_TESTS();
}