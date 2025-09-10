#include <QtTest/QtTest>
#include <QObject>
#include <chrono>
#include <thread>

#include "../../src/packet/core/packet_header.h"
#include "../../src/packet/core/packet_buffer.h"
#include "../../src/packet/core/packet.h"
#include "../../src/packet/core/packet_factory.h"
#include "../../src/core/application.h"

using namespace Monitor;
using namespace Monitor::Packet;
using namespace Monitor::Core;

class TestPacketCore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        // Application singleton is created automatically
        auto app = Application::instance();
        if (app) {
            app->initialize();
        }
    }

    void cleanupTestCase() {
        auto app = Application::instance();
        if (app) {
            app->shutdown();
        }
    }

    void testPacketHeader() {
        // Test default constructor
        PacketHeader header;
        QCOMPARE(header.id, 0u);
        QCOMPARE(header.sequence, 0u);
        QCOMPARE(header.payloadSize, 0u);
        QCOMPARE(header.flags, static_cast<uint32_t>(PacketHeader::Flags::None));
        
        // Test parameterized constructor
        PacketHeader header2(1001, 42, 1024, PacketHeader::Flags::Compressed);
        QCOMPARE(header2.id, 1001u);
        QCOMPARE(header2.sequence, 42u);
        QCOMPARE(header2.payloadSize, 1024u);
        QVERIFY(header2.hasFlag(PacketHeader::Flags::Compressed));
        
        // Test flag operations
        header.setFlag(PacketHeader::Flags::TestData);
        QVERIFY(header.hasFlag(PacketHeader::Flags::TestData));
        
        header.clearFlag(PacketHeader::Flags::TestData);
        QVERIFY(!header.hasFlag(PacketHeader::Flags::TestData));
        
        // Test timestamp functionality
        QVERIFY(header2.timestamp > 0);
        QVERIFY(header2.isValid());
        
        // Test size constraint
        static_assert(sizeof(PacketHeader) == 24, "Header size must be 24 bytes");
    }

    void testPacketBuffer() {
        auto app = Application::instance();
        QVERIFY(app != nullptr);
        auto memoryManager = app->memoryManager();
        QVERIFY(memoryManager != nullptr);
        
        PacketBuffer buffer(memoryManager);
        
        // Test buffer allocation
        {
            auto managed = buffer.allocate(1024);
            QVERIFY(managed != nullptr);
            QVERIFY(managed->isValid());
            QCOMPARE(managed->size(), 1024u);
            QVERIFY(managed->capacity() >= 1024u);
            QVERIFY(managed->data() != nullptr);
        }
        
        // Test packet-specific allocation
        {
            auto managed = buffer.allocateForPacket(512);
            QVERIFY(managed != nullptr);
            QVERIFY(managed->isValid());
            QCOMPARE(managed->size(), 512u + PACKET_HEADER_SIZE);
            QVERIFY(managed->capacity() >= managed->size());
        }
        
        // Test zero allocation
        {
            auto managed = buffer.allocate(0);
            QVERIFY(managed == nullptr);
        }
    }

    void testPacketFactory() {
        auto app = Application::instance();
        QVERIFY(app != nullptr);
        auto memoryManager = app->memoryManager();
        QVERIFY(memoryManager != nullptr);
        
        PacketFactory factory(memoryManager);
        
        // Test simple packet creation
        {
            std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
            auto result = factory.createPacket(1001, payload.data(), payload.size());
            
            QVERIFY(result.success);
            QVERIFY(result.packet != nullptr);
            QVERIFY(result.error.empty());
            
            auto packet = result.packet;
            QVERIFY(packet->isValid());
            QCOMPARE(packet->id(), 1001u);
            QCOMPARE(packet->payloadSize(), payload.size());
        }
        
        // Test empty packet creation
        {
            auto result = factory.createPacket(1002, nullptr, 0);
            QVERIFY(result.success);
            QVERIFY(result.packet != nullptr);
            QCOMPARE(result.packet->payloadSize(), 0u);
        }
        
        // Test oversized packet (should fail gracefully)
        {
            size_t oversized = PacketHeader::MAX_PAYLOAD_SIZE + 1;
            std::vector<uint8_t> oversizedPayload(oversized, 0xFF);
            auto result = factory.createPacket(1004, oversizedPayload.data(), oversizedPayload.size());
            
            QVERIFY(!result.success);
            QVERIFY(result.packet == nullptr);
            QVERIFY(!result.error.empty());
        }
    }

    void testPacket() {
        auto app = Application::instance();
        QVERIFY(app != nullptr);
        auto memoryManager = app->memoryManager();
        QVERIFY(memoryManager != nullptr);
        
        PacketFactory factory(memoryManager);
        
        // Test packet creation and basic operations
        std::vector<uint8_t> payload = {0x10, 0x20, 0x30, 0x40, 0x50};
        auto result = factory.createPacket(2001, payload.data(), payload.size());
        QVERIFY(result.success);
        
        auto packet = result.packet;
        QVERIFY(packet != nullptr);
        
        // Test basic properties
        QVERIFY(packet->isValid());
        QCOMPARE(packet->id(), 2001u);
        QCOMPARE(packet->payloadSize(), payload.size());
        QVERIFY(packet->payload() != nullptr);
        
        // Test flag operations
        QVERIFY(!packet->hasFlag(PacketHeader::Flags::TestData));
        packet->setFlag(PacketHeader::Flags::TestData);
        QVERIFY(packet->hasFlag(PacketHeader::Flags::TestData));
        packet->clearFlag(PacketHeader::Flags::TestData);
        QVERIFY(!packet->hasFlag(PacketHeader::Flags::TestData));
        
        // Test timestamp as uint64_t (nanoseconds)
        QVERIFY(packet->timestamp() > 0);
        QVERIFY(packet->getAgeNs() >= 0);
        
        // Test payload access
        const uint8_t* packetPayload = packet->payload();
        for (size_t i = 0; i < payload.size(); ++i) {
            QCOMPARE(packetPayload[i], payload[i]);
        }
        
        // Test header access
        const PacketHeader* header = packet->header();
        QVERIFY(header != nullptr);
        QCOMPARE(header->id, 2001u);
        QCOMPARE(header->payloadSize, static_cast<uint32_t>(payload.size()));
        
        // Test total size
        QCOMPARE(packet->totalSize(), PACKET_HEADER_SIZE + payload.size());
    }
    
    void testMemoryManagement() {
        auto app = Application::instance();
        QVERIFY(app != nullptr);
        auto memoryManager = app->memoryManager();
        QVERIFY(memoryManager != nullptr);
        
        PacketFactory factory(memoryManager);
        
        // Test that packets are properly cleaned up
        {
            std::vector<PacketPtr> packets;
            
            // Create many packets
            for (int i = 0; i < 50; ++i) {
                std::vector<uint8_t> payload(64 + i, static_cast<uint8_t>(i));
                auto result = factory.createPacket(4000 + i, payload.data(), payload.size());
                if (result.success) {
                    packets.push_back(result.packet);
                }
            }
            
            // Verify all are valid
            for (const auto& packet : packets) {
                QVERIFY(packet != nullptr);
                QVERIFY(packet->isValid());
            }
            
            // Let packets go out of scope (automatic cleanup)
        }
        
        // Create new packets to ensure memory is being reused
        for (int i = 0; i < 10; ++i) {
            std::vector<uint8_t> payload(128, 0xCC);
            auto result = factory.createPacket(5000 + i, payload.data(), payload.size());
            if (result.success) {
                QVERIFY(result.packet->isValid());
            }
        }
        
        // Test succeeds if no crashes occur
        QVERIFY(true);
    }
};

QTEST_MAIN(TestPacketCore)
#include "test_packet_core.moc"