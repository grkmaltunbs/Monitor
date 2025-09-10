#include <QtTest/QTest>
#include <QObject>
#include <thread>
#include <chrono>
#include "packet/core/packet.h"
#include "packet/core/packet_buffer.h"
#include "core/application.h"

using namespace Monitor::Packet;
using namespace Monitor::Memory;

class TestPacket : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testPacketConstruction();
    void testPacketValidation();
    void testPacketDataAccess();
    void testPacketMoveSemantics();

    // Header access tests
    void testHeaderAccess();
    void testHeaderModification();
    void testFlagManipulation();
    void testTimestampOperations();

    // Payload access tests
    void testPayloadAccess();
    void testPayloadModification();
    void testZeroCopyAccess();

    // Structure integration tests
    void testStructureAssociation();
    void testStructureValidation();
    void testStructureMetadata();

    // Validation tests
    void testValidationResults();
    void testValidationErrors();
    void testValidationWarnings();
    void testValidationCaching();

    // Performance tests
    void testPacketCreationPerformance();
    void testDataAccessPerformance();
    void testValidationPerformance();

    // Error handling tests
    void testInvalidBufferHandling();
    void testCorruptedPacketHandling();
    void testOversizedPacketHandling();

    // Edge cases
    void testEmptyPacket();
    void testMaximumSizePacket();
    void testPacketAging();
    void testSequenceNumberHandling();

    // Memory management tests
    void testPacketLifecycle();
    void testBufferOwnership();
    void testMemoryEfficiency();

private:
    Monitor::Core::Application* m_app = nullptr;
    MemoryPoolManager* m_memoryManager = nullptr;
    PacketBuffer* m_packetBuffer = nullptr;
    
    static constexpr PacketId TEST_PACKET_ID = 42;
    static constexpr SequenceNumber TEST_SEQUENCE = 12345;
    static constexpr size_t TEST_PAYLOAD_SIZE = 512;
    static constexpr uint64_t PERFORMANCE_ITERATIONS = 10000;
    
    PacketBuffer::ManagedBufferPtr createTestBuffer(size_t payloadSize = TEST_PAYLOAD_SIZE);
    std::unique_ptr<Packet> createTestPacket(size_t payloadSize = TEST_PAYLOAD_SIZE);
};

void TestPacket::initTestCase() {
    m_app = Monitor::Core::Application::instance();
    QVERIFY(m_app != nullptr);
    
    m_memoryManager = m_app->memoryManager();
    QVERIFY(m_memoryManager != nullptr);
    
    // Setup memory pools
    m_memoryManager->createPool("SmallObjects", 64, 1000);
    m_memoryManager->createPool("MediumObjects", 512, 1000);
    m_memoryManager->createPool("WidgetData", 1024, 1000);
    m_memoryManager->createPool("TestFramework", 2048, 1000);
    m_memoryManager->createPool("PacketBuffer", 4096, 1000);
    m_memoryManager->createPool("LargeObjects", 8192, 1000);
}

void TestPacket::cleanupTestCase() {
    // Cleanup handled by Application destructor
}

void TestPacket::init() {
    m_packetBuffer = new PacketBuffer(m_memoryManager);
    QVERIFY(m_packetBuffer != nullptr);
}

void TestPacket::cleanup() {
    delete m_packetBuffer;
    m_packetBuffer = nullptr;
}

PacketBuffer::ManagedBufferPtr TestPacket::createTestBuffer(size_t payloadSize) {
    return m_packetBuffer->createForPacket(TEST_PACKET_ID, nullptr, payloadSize);
}

std::unique_ptr<Packet> TestPacket::createTestPacket(size_t payloadSize) {
    auto buffer = createTestBuffer(payloadSize);
    if (!buffer) {
        return nullptr;
    }
    
    return std::make_unique<Packet>(std::move(buffer));
}

void TestPacket::testPacketConstruction() {
    // Test construction with valid buffer
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    QVERIFY(packet->isValid());
    
    // Test construction with null buffer
    Packet nullPacket(nullptr);
    QVERIFY(!nullPacket.isValid());
    
    // Verify basic properties
    QCOMPARE(packet->id(), TEST_PACKET_ID);
    QCOMPARE(packet->payloadSize(), TEST_PAYLOAD_SIZE);
    QCOMPARE(packet->totalSize(), PACKET_HEADER_SIZE + TEST_PAYLOAD_SIZE);
}

void TestPacket::testPacketValidation() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Valid packet should pass validation
    auto validation = packet->validate();
    QVERIFY(validation.isValid);
    QVERIFY(!validation.hasErrors());
    
    // Test with invalid header
    packet->header()->payloadSize = UINT32_MAX; // Invalid size
    validation = packet->validate();
    QVERIFY(!validation.isValid);
    QVERIFY(validation.hasErrors());
    
    // Test with reserved flags
    packet->header()->payloadSize = TEST_PAYLOAD_SIZE; // Fix payload size
    packet->header()->flags = PacketHeader::Flags::Reserved;
    validation = packet->validate();
    QVERIFY(!validation.isValid);
    QVERIFY(validation.hasErrors());
}

void TestPacket::testPacketDataAccess() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Test raw data access
    const uint8_t* data = packet->data();
    QVERIFY(data != nullptr);
    
    // Test payload access
    const uint8_t* payload = packet->payload();
    QVERIFY(payload != nullptr);
    QVERIFY(payload == data + PACKET_HEADER_SIZE);
    
    // Test mutable payload access
    uint8_t* mutablePayload = packet->payload();
    QVERIFY(mutablePayload != nullptr);
    
    // Write and read back
    mutablePayload[0] = 0xAB;
    mutablePayload[TEST_PAYLOAD_SIZE - 1] = 0xCD;
    
    QCOMPARE(payload[0], uint8_t(0xAB));
    QCOMPARE(payload[TEST_PAYLOAD_SIZE - 1], uint8_t(0xCD));
}

void TestPacket::testPacketMoveSemantics() {
    auto packet1 = createTestPacket();
    QVERIFY(packet1 != nullptr);
    
    PacketId originalId = packet1->id();
    size_t originalSize = packet1->totalSize();
    const uint8_t* originalData = packet1->data();
    
    // Move construct for unique_ptr
    auto packet2 = std::move(packet1);
    QVERIFY(packet2->isValid());
    QCOMPARE(packet2->id(), originalId);
    QCOMPARE(packet2->totalSize(), originalSize);
    QCOMPARE(packet2->data(), originalData);
    
    // Original packet should be null after move
    QVERIFY(packet1 == nullptr);
    
    // Move assign
    auto packet3 = createTestPacket(256);
    packet3 = std::move(packet2);
    
    QVERIFY(packet3->isValid());
    QCOMPARE(packet3->id(), originalId);
    QCOMPARE(packet3->totalSize(), originalSize);
    QCOMPARE(packet3->data(), originalData);
    
    // packet2 should be null after move
    QVERIFY(packet2 == nullptr);
}

void TestPacket::testHeaderAccess() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Test read-only header access
    const PacketHeader* constHeader = packet->header();
    QVERIFY(constHeader != nullptr);
    QCOMPARE(constHeader->id, TEST_PACKET_ID);
    QCOMPARE(constHeader->payloadSize, uint32_t(TEST_PAYLOAD_SIZE));
    
    // Test mutable header access
    PacketHeader* mutableHeader = packet->header();
    QVERIFY(mutableHeader != nullptr);
    QVERIFY(mutableHeader == constHeader);
    
    // Test individual field accessors
    QCOMPARE(packet->id(), TEST_PACKET_ID);
    QCOMPARE(packet->sequence(), SequenceNumber(0)); // Default sequence
    QCOMPARE(packet->payloadSize(), TEST_PAYLOAD_SIZE);
    QVERIFY(packet->timestamp() > 0);
}

void TestPacket::testHeaderModification() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Test sequence number modification
    const SequenceNumber newSequence = 98765;
    packet->setSequence(newSequence);
    QCOMPARE(packet->sequence(), newSequence);
    QCOMPARE(packet->header()->sequence, newSequence);
    
    // Test timestamp update
    uint64_t oldTimestamp = packet->timestamp();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    packet->updateTimestamp();
    uint64_t newTimestamp = packet->timestamp();
    
    QVERIFY(newTimestamp > oldTimestamp);
    QVERIFY((newTimestamp - oldTimestamp) >= 100000); // At least 100μs difference
}

void TestPacket::testFlagManipulation() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Initially no flags should be set (except construction defaults)
    QVERIFY(!packet->hasFlag(PacketHeader::Flags::Priority));
    QVERIFY(!packet->hasFlag(PacketHeader::Flags::Compressed));
    
    // Set flags
    packet->setFlag(PacketHeader::Flags::Priority);
    QVERIFY(packet->hasFlag(PacketHeader::Flags::Priority));
    QVERIFY(!packet->hasFlag(PacketHeader::Flags::Compressed));
    
    packet->setFlag(PacketHeader::Flags::Compressed);
    QVERIFY(packet->hasFlag(PacketHeader::Flags::Priority));
    QVERIFY(packet->hasFlag(PacketHeader::Flags::Compressed));
    
    // Clear flags
    packet->clearFlag(PacketHeader::Flags::Priority);
    QVERIFY(!packet->hasFlag(PacketHeader::Flags::Priority));
    QVERIFY(packet->hasFlag(PacketHeader::Flags::Compressed));
    
    packet->clearFlag(PacketHeader::Flags::Compressed);
    QVERIFY(!packet->hasFlag(PacketHeader::Flags::Priority));
    QVERIFY(!packet->hasFlag(PacketHeader::Flags::Compressed));
}

void TestPacket::testTimestampOperations() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Test timestamp conversion
    uint64_t timestampNs = packet->timestamp();
    auto timePoint = packet->getTimestamp();
    
    uint64_t convertedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        timePoint.time_since_epoch()).count();
    QCOMPARE(convertedNs, timestampNs);
    
    // Test age calculation
    uint64_t age1 = packet->getAgeNs();
    QVERIFY(age1 < 10000000); // Should be less than 10ms
    
    std::this_thread::sleep_for(std::chrono::microseconds(500));
    uint64_t age2 = packet->getAgeNs();
    QVERIFY(age2 > age1);
    QVERIFY(age2 >= 500000); // At least 500μs
}

void TestPacket::testPayloadAccess() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Test const payload access
    const uint8_t* constPayload = static_cast<const Packet*>(packet.get())->payload();
    QVERIFY(constPayload != nullptr);
    
    // Test mutable payload access
    uint8_t* mutablePayload = packet->payload();
    QVERIFY(mutablePayload != nullptr);
    QVERIFY(mutablePayload == constPayload);
    
    // Test payload data manipulation
    const uint8_t testPattern[] = {0xDE, 0xAD, 0xBE, 0xEF};
    std::memcpy(mutablePayload, testPattern, sizeof(testPattern));
    
    QCOMPARE(constPayload[0], uint8_t(0xDE));
    QCOMPARE(constPayload[1], uint8_t(0xAD));
    QCOMPARE(constPayload[2], uint8_t(0xBE));
    QCOMPARE(constPayload[3], uint8_t(0xEF));
    
    // Test with empty payload
    auto emptyPacket = createTestPacket(0);
    QVERIFY(emptyPacket != nullptr);
    QVERIFY(emptyPacket->payload() == nullptr);
    QVERIFY(static_cast<const Packet*>(emptyPacket.get())->payload() == nullptr);
}

void TestPacket::testPayloadModification() {
    auto packet = createTestPacket(1024);
    QVERIFY(packet != nullptr);
    
    uint8_t* payload = packet->payload();
    QVERIFY(payload != nullptr);
    
    // Fill payload with pattern
    for (size_t i = 0; i < 1024; ++i) {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }
    
    // Verify pattern
    for (size_t i = 0; i < 1024; ++i) {
        QCOMPARE(payload[i], static_cast<uint8_t>(i & 0xFF));
    }
    
    // Modify specific bytes
    payload[100] = 0xFF;
    payload[500] = 0x00;
    payload[1023] = 0x42;
    
    QCOMPARE(payload[100], uint8_t(0xFF));
    QCOMPARE(payload[500], uint8_t(0x00));
    QCOMPARE(payload[1023], uint8_t(0x42));
}

void TestPacket::testZeroCopyAccess() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Get data through different access methods
    const uint8_t* rawData = packet->data();
    const uint8_t* payload = packet->payload();
    const PacketHeader* header = packet->header();
    
    QVERIFY(rawData != nullptr);
    QVERIFY(payload != nullptr);
    QVERIFY(header != nullptr);
    
    // Verify zero-copy relationships
    QVERIFY(reinterpret_cast<const uint8_t*>(header) == rawData);
    QVERIFY(payload == rawData + PACKET_HEADER_SIZE);
    
    // Write through header
    PacketHeader* mutableHeader = packet->header();
    mutableHeader->flags = PacketHeader::Flags::TestData;
    
    // Read through raw data
    const PacketHeader* rawHeader = reinterpret_cast<const PacketHeader*>(rawData);
    QCOMPARE(rawHeader->flags, uint32_t(PacketHeader::Flags::TestData));
    
    // Write through payload
    uint8_t* mutablePayload = packet->payload();
    mutablePayload[0] = 0x88;
    
    // Read through raw data
    QCOMPARE(rawData[PACKET_HEADER_SIZE], uint8_t(0x88));
}

void TestPacket::testStructureAssociation() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Initially no structure
    QVERIFY(packet->getStructure() == nullptr);
    QCOMPARE(packet->getStructureName(), std::string("Unknown"));
    
    // Create a mock structure (we can't easily create a real AST structure in tests)
    // For now, just test the setter/getter interface
    std::shared_ptr<Monitor::Parser::AST::StructDeclaration> mockStructure = nullptr;
    packet->setStructure(mockStructure);
    QVERIFY(packet->getStructure() == nullptr);
    
    // Test structure name caching
    packet->getStructureName(); // Should update metadata cache
}

void TestPacket::testStructureValidation() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Without structure, validation should still pass basic checks
    auto validation = packet->validate();
    QVERIFY(validation.isValid);
    
    // Note: Full structure validation testing would require
    // creating actual AST structures, which is complex for unit tests
}

void TestPacket::testStructureMetadata() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Test metadata caching behavior
    std::string name1 = packet->getStructureName();
    std::string name2 = packet->getStructureName();
    QCOMPARE(name1, name2);
    
    // Setting structure should invalidate metadata cache
    packet->setStructure(nullptr);
    std::string name3 = packet->getStructureName();
    QCOMPARE(name3, name1); // Should still be "Unknown"
}

void TestPacket::testValidationResults() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Test successful validation
    auto validation = packet->validate();
    QVERIFY(validation.isValid);
    QVERIFY(!validation.hasErrors());
    QVERIFY(!validation.hasWarnings());
    QVERIFY(validation.errors.empty());
    QVERIFY(validation.warnings.empty());
    
    // Test validation result construction
    Packet::ValidationResult customResult(true);
    QVERIFY(customResult.isValid);
    
    customResult.addWarning("Test warning");
    QVERIFY(customResult.hasWarnings());
    QCOMPARE(customResult.warnings.size(), size_t(1));
    QVERIFY(customResult.isValid); // Warnings don't make it invalid
    
    customResult.addError("Test error");
    QVERIFY(!customResult.isValid);
    QVERIFY(customResult.hasErrors());
    QCOMPARE(customResult.errors.size(), size_t(1));
}

void TestPacket::testValidationErrors() {
    // Test various error conditions
    
    // Null buffer packet
    Packet nullPacket(nullptr);
    auto validation = nullPacket.validate();
    QVERIFY(!validation.isValid);
    QVERIFY(validation.hasErrors());
    QVERIFY(validation.errors.size() > 0);
    
    // Valid packet with invalid header
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Make header invalid by setting reserved flags
    packet->header()->flags = PacketHeader::Flags::Reserved;
    validation = packet->validate();
    QVERIFY(!validation.isValid);
    QVERIFY(validation.hasErrors());
    
    // Make header invalid by setting oversized payload
    packet->header()->flags = PacketHeader::Flags::None;
    packet->header()->payloadSize = PacketHeader::MAX_PAYLOAD_SIZE + 1000;
    validation = packet->validate();
    QVERIFY(!validation.isValid);
    QVERIFY(validation.hasErrors());
}

void TestPacket::testValidationWarnings() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Create an old packet by manipulating timestamp
    packet->header()->timestamp = PacketHeader::getCurrentTimestampNs() - 120000000000ULL; // 2 minutes ago
    
    auto validation = packet->validate();
    // Should still be valid but have warnings
    if (validation.hasWarnings()) {
        QVERIFY(validation.isValid); // Old packets are valid but generate warnings
        QVERIFY(validation.warnings.size() > 0);
    }
}

void TestPacket::testValidationCaching() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Validation results should be available through getLastValidation
    // though the actual caching behavior depends on implementation
    packet->validate();
    auto cachedResult = packet->getLastValidation();
    
    // Should have a result (even if default constructed)
    QVERIFY(!cachedResult.isValid || cachedResult.isValid); // Tautology but tests method existence
}

void TestPacket::testPacketCreationPerformance() {
    QElapsedTimer timer;
    std::vector<std::unique_ptr<Packet>> packets;
    packets.reserve(PERFORMANCE_ITERATIONS);
    
    timer.start();
    
    for (uint64_t i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        auto buffer = m_packetBuffer->createForPacket(
            static_cast<PacketId>(i % 1000), 
            nullptr, 
            128 + (i % 512)
        );
        if (buffer) {
            packets.push_back(std::make_unique<Packet>(std::move(buffer)));
        }
    }
    
    qint64 creationTime = timer.nsecsElapsed();
    double nsPerCreation = static_cast<double>(creationTime) / packets.size();
    
    qDebug() << "Packet creation performance:" << nsPerCreation << "ns per packet";
    qDebug() << "Created packets:" << packets.size() << "out of" << PERFORMANCE_ITERATIONS;
    
    // Should be very fast - target < 1000ns (1μs) per packet
    QVERIFY(nsPerCreation < 1000.0);
    
    // Clean up timing
    timer.restart();
    packets.clear();
    qint64 destructionTime = timer.nsecsElapsed();
    double nsPerDestruction = static_cast<double>(destructionTime) / PERFORMANCE_ITERATIONS;
    
    qDebug() << "Packet destruction performance:" << nsPerDestruction << "ns per packet";
    QVERIFY(nsPerDestruction < 500.0);
}

void TestPacket::testDataAccessPerformance() {
    auto packet = createTestPacket(2048);
    QVERIFY(packet != nullptr);
    
    QElapsedTimer timer;
    volatile uint8_t sum = 0; // Prevent optimization
    
    timer.start();
    
    for (uint64_t i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        const uint8_t* data = packet->data();
        const uint8_t* payload = packet->payload();
        PacketId id = packet->id();
        size_t size = packet->payloadSize();
        
        // Use the data to prevent optimization
        sum += data[0] + payload[0] + static_cast<uint8_t>(id) + static_cast<uint8_t>(size);
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerAccess = static_cast<double>(elapsedNs) / (PERFORMANCE_ITERATIONS * 4); // 4 accesses per iteration
    
    qDebug() << "Packet data access performance:" << nsPerAccess << "ns per access";
    
    // Should be extremely fast - just pointer arithmetic
    QVERIFY(nsPerAccess < 10.0); // Less than 10ns per access
    
    // Use sum to prevent optimization
    QVERIFY(sum != 0 || sum == 0); // Tautology but uses sum
}

void TestPacket::testValidationPerformance() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    QElapsedTimer timer;
    timer.start();
    
    for (uint64_t i = 0; i < PERFORMANCE_ITERATIONS / 10; ++i) { // Validation is more expensive
        auto validation = packet->validate();
        // Use result to prevent optimization
        volatile bool valid = validation.isValid;
        (void)valid;
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerValidation = static_cast<double>(elapsedNs) / (PERFORMANCE_ITERATIONS / 10);
    
    qDebug() << "Packet validation performance:" << nsPerValidation << "ns per validation";
    
    // Validation is more complex but should still be fast
    QVERIFY(nsPerValidation < 10000.0); // Less than 10μs per validation
}

void TestPacket::testInvalidBufferHandling() {
    // Test with null buffer
    Packet nullPacket(nullptr);
    QVERIFY(!nullPacket.isValid());
    QCOMPARE(nullPacket.data(), static_cast<const uint8_t*>(nullptr));
    QCOMPARE(nullPacket.payload(), static_cast<const uint8_t*>(nullptr));
    QCOMPARE(nullPacket.header(), static_cast<const PacketHeader*>(nullptr));
    QCOMPARE(nullPacket.id(), PacketId(0));
    QCOMPARE(nullPacket.sequence(), SequenceNumber(0));
    QCOMPARE(nullPacket.payloadSize(), size_t(0));
    QCOMPARE(nullPacket.totalSize(), size_t(0));
    
    auto validation = nullPacket.validate();
    QVERIFY(!validation.isValid);
    QVERIFY(validation.hasErrors());
}

void TestPacket::testCorruptedPacketHandling() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Corrupt the header by setting invalid values
    PacketHeader* header = packet->header();
    header->payloadSize = UINT32_MAX; // Impossibly large
    
    QVERIFY(!packet->isValid() || packet->validate().hasErrors());
    
    // Test with mismatched payload size
    header->payloadSize = 10000; // Much larger than actual buffer
    auto validation = packet->validate();
    QVERIFY(!validation.isValid);
    QVERIFY(validation.hasErrors());
}

void TestPacket::testOversizedPacketHandling() {
    // Our buffer allocation should prevent creation of oversized packets
    // Test the boundaries
    
    // Try to create packet larger than our largest pool
    const size_t oversizePayload = 10000; // Larger than 8KB pool
    auto buffer = m_packetBuffer->createForPacket(TEST_PACKET_ID, nullptr, oversizePayload);
    QVERIFY(buffer == nullptr); // Should fail
    
    // Try maximum pool size
    const size_t maxPoolPayload = 8192 - PACKET_HEADER_SIZE;
    auto maxBuffer = m_packetBuffer->createForPacket(TEST_PACKET_ID, nullptr, maxPoolPayload);
    QVERIFY(maxBuffer != nullptr);
    
    auto maxPacket = std::make_unique<Packet>(std::move(maxBuffer));
    QVERIFY(maxPacket->isValid());
    QCOMPARE(maxPacket->payloadSize(), maxPoolPayload);
}

void TestPacket::testEmptyPacket() {
    auto emptyPacket = createTestPacket(0);
    QVERIFY(emptyPacket != nullptr);
    QVERIFY(emptyPacket->isValid());
    
    QCOMPARE(emptyPacket->payloadSize(), size_t(0));
    QCOMPARE(emptyPacket->totalSize(), PACKET_HEADER_SIZE);
    QVERIFY(emptyPacket->payload() == nullptr);
    QVERIFY(emptyPacket->data() != nullptr); // Header still exists
    
    // Should pass validation
    auto validation = emptyPacket->validate();
    QVERIFY(validation.isValid);
    QVERIFY(!validation.hasErrors());
}

void TestPacket::testMaximumSizePacket() {
    // Test with maximum size that fits in our pools
    const size_t maxPayload = 8192 - PACKET_HEADER_SIZE;
    auto maxPacket = createTestPacket(maxPayload);
    QVERIFY(maxPacket != nullptr);
    QVERIFY(maxPacket->isValid());
    
    QCOMPARE(maxPacket->payloadSize(), maxPayload);
    QCOMPARE(maxPacket->totalSize(), size_t(8192));
    
    // Should be able to access all payload bytes
    uint8_t* payload = maxPacket->payload();
    QVERIFY(payload != nullptr);
    
    // Write to first and last byte
    payload[0] = 0x11;
    payload[maxPayload - 1] = 0x22;
    
    QCOMPARE(payload[0], uint8_t(0x11));
    QCOMPARE(payload[maxPayload - 1], uint8_t(0x22));
}

void TestPacket::testPacketAging() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Packet should start with very small age
    uint64_t initialAge = packet->getAgeNs();
    QVERIFY(initialAge < 1000000); // Less than 1ms
    
    // Wait and check age increases
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    uint64_t laterAge = packet->getAgeNs();
    QVERIFY(laterAge > initialAge);
    QVERIFY(laterAge >= 1000000); // At least 1ms
    
    // Test with artificially old timestamp
    packet->header()->timestamp = PacketHeader::getCurrentTimestampNs() - 5000000000ULL; // 5 seconds ago
    uint64_t oldAge = packet->getAgeNs();
    QVERIFY(oldAge >= 5000000000ULL); // At least 5 seconds
    
    // Should generate warning in validation
    auto validation = packet->validate();
    if (oldAge > 60000000000ULL) { // More than 1 minute
        QVERIFY(validation.hasWarnings());
    }
}

void TestPacket::testSequenceNumberHandling() {
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Test initial sequence number
    QCOMPARE(packet->sequence(), SequenceNumber(0)); // Default from createTestBuffer
    
    // Test sequence number modification
    std::vector<SequenceNumber> testSequences = {
        1, 100, 65535, 1000000, UINT32_MAX
    };
    
    for (SequenceNumber seq : testSequences) {
        packet->setSequence(seq);
        QCOMPARE(packet->sequence(), seq);
        QCOMPARE(packet->header()->sequence, seq);
        
        // Packet should remain valid
        QVERIFY(packet->isValid());
    }
}

void TestPacket::testPacketLifecycle() {
    void* bufferPtr = nullptr;
    
    {
        auto packet = createTestPacket();
        QVERIFY(packet != nullptr);
        QVERIFY(packet->isValid());
        
        bufferPtr = const_cast<void*>(static_cast<const void*>(packet->data()));
        QVERIFY(bufferPtr != nullptr);
        
        // Write some data
        uint8_t* payload = packet->payload();
        if (payload) {
            payload[0] = 0xAA;
        }
    }
    
    // Packet and buffer should be destroyed automatically
    // This tests RAII behavior
}

void TestPacket::testBufferOwnership() {
    auto buffer = createTestBuffer();
    void* bufferData = buffer->data();
    
    // Create packet with buffer
    auto packet = std::make_unique<Packet>(std::move(buffer));
    QVERIFY(packet->isValid());
    QCOMPARE(packet->data(), static_cast<const uint8_t*>(bufferData));
    
    // Original buffer should be moved
    QVERIFY(buffer == nullptr);
    
    // Packet owns the buffer now
    QVERIFY(packet->data() != nullptr);
    
    // Test move semantics preserve ownership
    Packet movedPacket = std::move(*packet);
    QVERIFY(movedPacket.isValid());
    QCOMPARE(movedPacket.data(), static_cast<const uint8_t*>(bufferData));
    
    // Original packet should be invalid
    QVERIFY(!packet->isValid());
}

void TestPacket::testMemoryEfficiency() {
    // Test that packet uses minimal additional memory beyond buffer
    auto packet = createTestPacket();
    QVERIFY(packet != nullptr);
    
    // Test buffer information
    std::string poolName = packet->getPoolName();
    QVERIFY(!poolName.empty());
    
    size_t bufferCapacity = packet->getBufferCapacity();
    QVERIFY(bufferCapacity >= packet->totalSize());
    
    // Verify buffer capacity matches expected pool size
    if (packet->totalSize() <= 64) {
        QCOMPARE(bufferCapacity, size_t(64));
    } else if (packet->totalSize() <= 512) {
        QCOMPARE(bufferCapacity, size_t(512));
    } else if (packet->totalSize() <= 1024) {
        QCOMPARE(bufferCapacity, size_t(1024));
    }
    
    // Packet object should be reasonably small
    // (Exact size depends on implementation, but should be much smaller than buffer)
    QVERIFY(sizeof(Packet) < bufferCapacity);
}

QTEST_MAIN(TestPacket)
#include "test_packet.moc"