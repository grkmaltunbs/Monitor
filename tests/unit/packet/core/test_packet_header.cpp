#include <QtTest/QTest>
#include <QObject>
#include <chrono>
#include <thread>
#include "packet/core/packet_header.h"

using namespace Monitor::Packet;

class TestPacketHeader : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic construction tests
    void testDefaultConstruction();
    void testParameterizedConstruction();
    void testHeaderSize();

    // Flag manipulation tests
    void testFlagOperations();
    void testFlagCombinations();
    void testReservedFlagProtection();

    // Timestamp tests
    void testTimestampGeneration();
    void testTimestampConversion();
    void testAgeCalculation();

    // Validation tests
    void testHeaderValidation();
    void testPayloadSizeValidation();
    void testInvalidHeaders();

    // Performance tests
    void testConstructionPerformance();
    void testFlagOperationPerformance();
    void testTimestampPerformance();

    // Edge case tests
    void testMaxPayloadSize();
    void testZeroValues();
    void testTimestampAccuracy();

private:
    static constexpr PacketId TEST_PACKET_ID = 12345;
    static constexpr SequenceNumber TEST_SEQUENCE = 98765;
    static constexpr uint32_t TEST_PAYLOAD_SIZE = 1024;
    static constexpr uint64_t PERFORMANCE_ITERATIONS = 100000;
};

void TestPacketHeader::initTestCase() {
    // Setup before all tests
}

void TestPacketHeader::cleanupTestCase() {
    // Cleanup after all tests
}

void TestPacketHeader::init() {
    // Setup before each test
}

void TestPacketHeader::cleanup() {
    // Cleanup after each test
}

void TestPacketHeader::testDefaultConstruction() {
    PacketHeader header;
    
    // Verify default values
    QCOMPARE(header.id, PacketId(0));
    QCOMPARE(header.sequence, SequenceNumber(0));
    QCOMPARE(header.timestamp, uint64_t(0));
    QCOMPARE(header.payloadSize, uint32_t(0));
    QCOMPARE(header.flags, uint32_t(PacketHeader::Flags::None));
    
    // Header should not be valid with default values
    QVERIFY(!header.isValid());
}

void TestPacketHeader::testParameterizedConstruction() {
    // Record time before construction for timestamp validation
    auto beforeTime = std::chrono::high_resolution_clock::now();
    
    PacketHeader header(TEST_PACKET_ID, TEST_SEQUENCE, TEST_PAYLOAD_SIZE, 
                       PacketHeader::Flags::Priority | PacketHeader::Flags::TestData);
    
    auto afterTime = std::chrono::high_resolution_clock::now();
    
    // Verify parameter assignment
    QCOMPARE(header.id, TEST_PACKET_ID);
    QCOMPARE(header.sequence, TEST_SEQUENCE);
    QCOMPARE(header.payloadSize, TEST_PAYLOAD_SIZE);
    QVERIFY(header.hasFlag(PacketHeader::Flags::Priority));
    QVERIFY(header.hasFlag(PacketHeader::Flags::TestData));
    
    // Verify timestamp is reasonable (between before and after)
    uint64_t beforeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        beforeTime.time_since_epoch()).count();
    uint64_t afterNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        afterTime.time_since_epoch()).count();
    
    QVERIFY(header.timestamp >= beforeNs);
    QVERIFY(header.timestamp <= afterNs);
    
    // Header should be valid
    QVERIFY(header.isValid());
}

void TestPacketHeader::testHeaderSize() {
    // Verify header size is exactly 24 bytes
    QCOMPARE(sizeof(PacketHeader), size_t(24));
    QCOMPARE(PACKET_HEADER_SIZE, size_t(24));
    
    // Verify field offsets (assuming standard packing)
    PacketHeader header;
    uint8_t* base = reinterpret_cast<uint8_t*>(&header);
    
    QCOMPARE(reinterpret_cast<uint8_t*>(&header.id) - base, ptrdiff_t(0));
    QCOMPARE(reinterpret_cast<uint8_t*>(&header.sequence) - base, ptrdiff_t(4));
    QCOMPARE(reinterpret_cast<uint8_t*>(&header.timestamp) - base, ptrdiff_t(8));
    QCOMPARE(reinterpret_cast<uint8_t*>(&header.payloadSize) - base, ptrdiff_t(16));
    QCOMPARE(reinterpret_cast<uint8_t*>(&header.flags) - base, ptrdiff_t(20));
}

void TestPacketHeader::testFlagOperations() {
    PacketHeader header;
    
    // Test initial state - no flags set
    QVERIFY(!header.hasFlag(PacketHeader::Flags::Priority));
    QVERIFY(!header.hasFlag(PacketHeader::Flags::Compressed));
    QVERIFY(!header.hasFlag(PacketHeader::Flags::Fragmented));
    
    // Test setting individual flags
    header.setFlag(PacketHeader::Flags::Priority);
    QVERIFY(header.hasFlag(PacketHeader::Flags::Priority));
    QVERIFY(!header.hasFlag(PacketHeader::Flags::Compressed));
    
    header.setFlag(PacketHeader::Flags::Compressed);
    QVERIFY(header.hasFlag(PacketHeader::Flags::Priority));
    QVERIFY(header.hasFlag(PacketHeader::Flags::Compressed));
    
    // Test clearing flags
    header.clearFlag(PacketHeader::Flags::Priority);
    QVERIFY(!header.hasFlag(PacketHeader::Flags::Priority));
    QVERIFY(header.hasFlag(PacketHeader::Flags::Compressed));
    
    // Test clearing all flags
    header.clearFlag(PacketHeader::Flags::Compressed);
    QVERIFY(!header.hasFlag(PacketHeader::Flags::Priority));
    QVERIFY(!header.hasFlag(PacketHeader::Flags::Compressed));
    QCOMPARE(header.flags, uint32_t(PacketHeader::Flags::None));
}

void TestPacketHeader::testFlagCombinations() {
    PacketHeader header;
    
    // Test setting multiple flags at once
    uint32_t combinedFlags = PacketHeader::Flags::Priority | 
                           PacketHeader::Flags::Encrypted | 
                           PacketHeader::Flags::TestData;
    header.flags = combinedFlags;
    
    QVERIFY(header.hasFlag(PacketHeader::Flags::Priority));
    QVERIFY(header.hasFlag(PacketHeader::Flags::Encrypted));
    QVERIFY(header.hasFlag(PacketHeader::Flags::TestData));
    QVERIFY(!header.hasFlag(PacketHeader::Flags::Compressed));
    
    // Test user-defined flags
    header.setFlag(PacketHeader::Flags::UserFlag0);
    header.setFlag(PacketHeader::Flags::UserFlag7);
    
    QVERIFY(header.hasFlag(PacketHeader::Flags::UserFlag0));
    QVERIFY(header.hasFlag(PacketHeader::Flags::UserFlag7));
    QVERIFY(!header.hasFlag(PacketHeader::Flags::UserFlag1));
}

void TestPacketHeader::testReservedFlagProtection() {
    PacketHeader header(TEST_PACKET_ID, TEST_SEQUENCE, TEST_PAYLOAD_SIZE);
    
    // Initially valid header
    QVERIFY(header.isValid());
    
    // Setting reserved flags should make header invalid
    header.flags |= PacketHeader::Flags::Reserved;
    QVERIFY(!header.isValid());
    
    // Clear reserved flags
    header.flags &= ~PacketHeader::Flags::Reserved;
    QVERIFY(header.isValid());
}

void TestPacketHeader::testTimestampGeneration() {
    // Test static timestamp function
    uint64_t timestamp1 = PacketHeader::getCurrentTimestampNs();
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    uint64_t timestamp2 = PacketHeader::getCurrentTimestampNs();
    
    // Second timestamp should be later
    QVERIFY(timestamp2 > timestamp1);
    
    // Difference should be at least 100 microseconds (100,000 nanoseconds)
    QVERIFY((timestamp2 - timestamp1) >= 100000);
}

void TestPacketHeader::testTimestampConversion() {
    PacketHeader header(TEST_PACKET_ID);
    
    // Get timestamp as time_point
    auto timePoint = header.getTimestamp();
    uint64_t convertedBack = std::chrono::duration_cast<std::chrono::nanoseconds>(
        timePoint.time_since_epoch()).count();
    
    // Should match original timestamp
    QCOMPARE(convertedBack, header.timestamp);
    
    // Test with specific timestamp
    uint64_t specificTime = 1609459200000000000ULL; // 2021-01-01 00:00:00 UTC in nanoseconds
    header.timestamp = specificTime;
    
    auto specificTimePoint = header.getTimestamp();
    uint64_t specificConverted = std::chrono::duration_cast<std::chrono::nanoseconds>(
        specificTimePoint.time_since_epoch()).count();
    
    QCOMPARE(specificConverted, specificTime);
}

void TestPacketHeader::testAgeCalculation() {
    PacketHeader header(TEST_PACKET_ID);
    
    // Age should be very small immediately after creation
    uint64_t age1 = header.getAgeNs();
    QVERIFY(age1 < 1000000); // Less than 1ms
    
    // Wait a bit and check age again
    std::this_thread::sleep_for(std::chrono::microseconds(500));
    uint64_t age2 = header.getAgeNs();
    
    QVERIFY(age2 > age1);
    QVERIFY(age2 >= 500000); // At least 500 microseconds
    
    // Test with older timestamp
    header.timestamp = PacketHeader::getCurrentTimestampNs() - 5000000000ULL; // 5 seconds ago
    uint64_t oldAge = header.getAgeNs();
    QVERIFY(oldAge >= 5000000000ULL); // At least 5 seconds
}

void TestPacketHeader::testHeaderValidation() {
    // Valid header
    PacketHeader validHeader(TEST_PACKET_ID, TEST_SEQUENCE, 1000);
    QVERIFY(validHeader.isValid());
    
    // Test maximum payload size
    PacketHeader maxSizeHeader(TEST_PACKET_ID, 0, PacketHeader::MAX_PAYLOAD_SIZE);
    QVERIFY(maxSizeHeader.isValid());
    
    // Test oversized payload
    PacketHeader oversizedHeader(TEST_PACKET_ID, 0, PacketHeader::MAX_PAYLOAD_SIZE + 1);
    QVERIFY(!oversizedHeader.isValid());
    
    // Test with reserved flags
    PacketHeader reservedFlagHeader(TEST_PACKET_ID, 0, 1000, PacketHeader::Flags::Reserved);
    QVERIFY(!reservedFlagHeader.isValid());
}

void TestPacketHeader::testPayloadSizeValidation() {
    PacketHeader header;
    
    // Test valid payload sizes
    std::vector<uint32_t> validSizes = {0, 1, 100, 1024, 8192, 32768, PacketHeader::MAX_PAYLOAD_SIZE};
    
    for (uint32_t size : validSizes) {
        header.payloadSize = size;
        header.flags = PacketHeader::Flags::None; // Clear reserved flags
        header.timestamp = PacketHeader::getCurrentTimestampNs(); // Set valid timestamp
        QVERIFY(header.isValid());
    }
    
    // Test invalid payload sizes
    std::vector<uint32_t> invalidSizes = {
        PacketHeader::MAX_PAYLOAD_SIZE + 1,
        PacketHeader::MAX_PAYLOAD_SIZE + 1000,
        UINT32_MAX
    };
    
    for (uint32_t size : invalidSizes) {
        header.payloadSize = size;
        header.flags = PacketHeader::Flags::None;
        QVERIFY(!header.isValid());
    }
}

void TestPacketHeader::testInvalidHeaders() {
    // Test various invalid header configurations
    PacketHeader header;
    
    // Default constructed header is invalid
    QVERIFY(!header.isValid());
    
    // Header with only reserved flags
    header.flags = PacketHeader::Flags::Reserved;
    QVERIFY(!header.isValid());
    
    // Header with partial reserved flags
    header.flags = 0x00010000; // One reserved bit
    QVERIFY(!header.isValid());
    
    // Clear flags and set valid payload size and timestamp
    header.flags = PacketHeader::Flags::None;
    header.payloadSize = 1024;
    header.timestamp = PacketHeader::getCurrentTimestampNs(); // Set valid timestamp
    QVERIFY(header.isValid());
}

void TestPacketHeader::testConstructionPerformance() {
    QElapsedTimer timer;
    timer.start();
    
    // Test construction performance
    for (uint64_t i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        PacketHeader header(static_cast<PacketId>(i % 1000), 
                           static_cast<SequenceNumber>(i), 
                           static_cast<uint32_t>(i % 2000));
        // Prevent optimization
        volatile PacketId id = header.id;
        (void)id;
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerConstruction = static_cast<double>(elapsedNs) / PERFORMANCE_ITERATIONS;
    
    qDebug() << "PacketHeader construction performance:" << nsPerConstruction << "ns per header";
    
    // Should be very fast - less than 1000ns (1μs) per construction
    QVERIFY(nsPerConstruction < 1000.0);
}

void TestPacketHeader::testFlagOperationPerformance() {
    PacketHeader header;
    QElapsedTimer timer;
    
    // Test flag operation performance
    timer.start();
    
    for (uint64_t i = 0; i < PERFORMANCE_ITERATIONS; ++i) {
        header.setFlag(PacketHeader::Flags::Priority);
        bool hasFlag = header.hasFlag(PacketHeader::Flags::Priority);
        header.clearFlag(PacketHeader::Flags::Priority);
        
        // Prevent optimization
        volatile bool result = hasFlag;
        (void)result;
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerOperation = static_cast<double>(elapsedNs) / (PERFORMANCE_ITERATIONS * 3); // 3 ops per iteration
    
    qDebug() << "PacketHeader flag operation performance:" << nsPerOperation << "ns per operation";
    
    // Should be very fast - less than 100ns per operation
    QVERIFY(nsPerOperation < 100.0);
}

void TestPacketHeader::testTimestampPerformance() {
    QElapsedTimer timer;
    timer.start();
    
    // Test timestamp generation performance
    for (uint64_t i = 0; i < PERFORMANCE_ITERATIONS / 10; ++i) { // Divide by 10 since timestamp generation is slower
        uint64_t timestamp = PacketHeader::getCurrentTimestampNs();
        
        // Prevent optimization
        volatile uint64_t result = timestamp;
        (void)result;
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerTimestamp = static_cast<double>(elapsedNs) / (PERFORMANCE_ITERATIONS / 10);
    
    qDebug() << "PacketHeader timestamp generation performance:" << nsPerTimestamp << "ns per timestamp";
    
    // Timestamp generation is more expensive, allow up to 10μs
    QVERIFY(nsPerTimestamp < 10000.0);
}

void TestPacketHeader::testMaxPayloadSize() {
    // Test the maximum payload size constant
    QCOMPARE(PacketHeader::MAX_PAYLOAD_SIZE, uint32_t(64 * 1024)); // 64KB
    
    // Test that headers with max payload size are valid
    PacketHeader maxHeader(TEST_PACKET_ID, 0, PacketHeader::MAX_PAYLOAD_SIZE);
    QVERIFY(maxHeader.isValid());
    
    // Test that exceeding max size is invalid
    PacketHeader overMaxHeader(TEST_PACKET_ID, 0, PacketHeader::MAX_PAYLOAD_SIZE + 1);
    QVERIFY(!overMaxHeader.isValid());
}

void TestPacketHeader::testZeroValues() {
    // Test header with zero values (except flags)
    PacketHeader zeroHeader(0, 0, 0, PacketHeader::Flags::None);
    QVERIFY(zeroHeader.isValid()); // Should be valid even with zero values
    
    QCOMPARE(zeroHeader.id, PacketId(0));
    QCOMPARE(zeroHeader.sequence, SequenceNumber(0));
    QCOMPARE(zeroHeader.payloadSize, uint32_t(0));
    
    // Age should be small for newly created header
    QVERIFY(zeroHeader.getAgeNs() < 1000000); // Less than 1ms
}

void TestPacketHeader::testTimestampAccuracy() {
    // Test timestamp accuracy by comparing with system clock
    auto systemTime = std::chrono::high_resolution_clock::now();
    uint64_t headerTime = PacketHeader::getCurrentTimestampNs();
    auto systemTimeAfter = std::chrono::high_resolution_clock::now();
    
    uint64_t systemNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        systemTime.time_since_epoch()).count();
    uint64_t systemNsAfter = std::chrono::duration_cast<std::chrono::nanoseconds>(
        systemTimeAfter.time_since_epoch()).count();
    
    // Header timestamp should be within the bounds
    QVERIFY(headerTime >= systemNs);
    QVERIFY(headerTime <= systemNsAfter);
    
    // Test conversion back to time_point
    PacketHeader header;
    header.timestamp = headerTime;
    
    auto converted = header.getTimestamp();
    uint64_t convertedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        converted.time_since_epoch()).count();
    
    QCOMPARE(convertedNs, headerTime);
}

QTEST_MAIN(TestPacketHeader)
#include "test_packet_header.moc"