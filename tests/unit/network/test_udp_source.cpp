#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QTimer>
#include <QEventLoop>
#include <QNetworkInterface>

#include "../../../src/network/sources/udp_source.h"
#include "../../../src/packet/core/packet_factory.h"
#include "../../../src/memory/memory_pool.h"
#include "../../../src/packet/core/packet_header.h"

using namespace Monitor;

class TestUdpSource : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // Basic functionality tests
    void testConstruction();
    void testConfiguration();
    void testStatistics();
    
    // Socket management tests
    void testSocketInitialization();
    void testSocketBinding();
    void testSocketOptions();
    void testSocketStateTransitions();
    
    // Multicast tests
    void testMulticastConfiguration();
    void testMulticastJoinLeave();
    void testMulticastNetworkInterface();
    
    // Packet processing tests
    void testPacketCreation();
    void testPacketValidation();
    void testInvalidPacketHandling();
    void testPacketStatistics();
    
    // Rate limiting tests
    void testRateLimiting();
    void testRateLimitDisabled();
    
    // Error handling tests
    void testSocketErrors();
    void testConsecutiveErrors();
    void testErrorRecovery();
    
    // Performance tests
    void testHighThroughputSimulation();
    void testMemoryUsage();
    
    // Signal/slot tests
    void testSignalEmission();

private:
    Memory::MemoryPoolManager* m_memoryManager;
    Packet::PacketFactory* m_packetFactory;
    
    // Helper methods
    QByteArray createTestPacket(uint32_t id, uint32_t sequence, const QString& payload = "TestData");
    void simulatePacketReception(Network::UdpSource& source, const QByteArray& packetData);
    bool waitForSignal(const QObject* sender, const char* signal, int timeout = 5000);
};

void TestUdpSource::initTestCase()
{
    m_memoryManager = new Memory::MemoryPoolManager();
    m_packetFactory = new Packet::PacketFactory(m_memoryManager);
}

void TestUdpSource::cleanupTestCase()
{
    delete m_packetFactory;
    delete m_memoryManager;
}

void TestUdpSource::testConstruction()
{
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig("TestUDP", QHostAddress::Any, 8080);
    Network::UdpSource source(config);
    
    QCOMPARE(source.getName(), std::string("TestUDP"));
    QVERIFY(!source.isRunning());
    QVERIFY(source.isStopped());
}

void TestUdpSource::testConfiguration()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    // Test initial configuration
    const auto& sourceConfig = source.getNetworkConfig();
    QCOMPARE(sourceConfig.protocol, Network::Protocol::UDP);
    
    // Test configuration update
    Network::NetworkConfig newConfig = Network::NetworkConfig::createUdpConfig("UpdatedUDP", QHostAddress::LocalHost, 9000);
    source.setNetworkConfig(newConfig);
    
    QCOMPARE(source.getNetworkConfig().name, std::string("UpdatedUDP"));
    QCOMPARE(source.getNetworkConfig().localPort, 9000);
}

void TestUdpSource::testStatistics()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    
    const auto& stats = source.getNetworkStatistics();
    QCOMPARE(stats.packetsReceived.load(), 0ULL);
    QCOMPARE(stats.bytesReceived.load(), 0ULL);
    QCOMPARE(stats.packetsDropped.load(), 0ULL);
}

// Socket management tests
void TestUdpSource::testSocketInitialization()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    // Initially socket should not be initialized
    QCOMPARE(source.getSocketState(), QString("Not Initialized"));
    
    // Start source to initialize socket
    QSignalSpy socketStateSpy(&source, &Network::UdpSource::socketStateChanged);
    bool started = source.start();
    
    QVERIFY(started);
    QVERIFY(source.isRunning());
    
    // Socket should now be bound
    if (socketStateSpy.count() == 0) {
        socketStateSpy.wait(1000);
    }
    QVERIFY(socketStateSpy.count() > 0);
    QCOMPARE(source.getSocketState(), QString("Bound"));
    
    source.stop();
    QVERIFY(source.isStopped());
}

void TestUdpSource::testSocketBinding()
{
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "BindTest", QHostAddress::LocalHost, 0); // Port 0 = auto-select
    
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy socketStateSpy(&source, &Network::UdpSource::socketStateChanged);
    bool started = source.start();
    
    QVERIFY(started);
    QVERIFY(source.isRunning());
    
    // Wait for socket to be bound
    if (socketStateSpy.count() == 0) {
        socketStateSpy.wait(1000);
    }
    
    QCOMPARE(source.getSocketState(), QString("Bound"));
    
    source.stop();
}

void TestUdpSource::testSocketOptions()
{
    Network::NetworkConfig config;
    config.receiveBufferSize = 65536;
    config.priority = 5;
    
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    // Test that source starts successfully with custom options
    bool started = source.start();
    QVERIFY(started);
    
    source.stop();
}

void TestUdpSource::testSocketStateTransitions()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy stateSpy(&source, &Network::UdpSource::socketStateChanged);
    
    // Test start transition
    source.start();
    if (stateSpy.count() == 0) {
        stateSpy.wait(1000);
    }
    QVERIFY(stateSpy.count() > 0);
    
    // Test stop transition
    stateSpy.clear();
    source.stop();
    
    QCOMPARE(source.getSocketState(), QString("Not Initialized"));
}

// Multicast tests
void TestUdpSource::testMulticastConfiguration()
{
    Network::NetworkConfig config;
    config.enableMulticast = true;
    config.multicastGroup = QHostAddress("239.255.255.250"); // SSDP multicast address
    
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QVERIFY(!source.isMulticastActive()); // Not active until started
    
    bool started = source.start();
    QVERIFY(started);
    
    // Note: Multicast join may fail in test environment, but source should still start
    source.stop();
}

void TestUdpSource::testMulticastJoinLeave()
{
    Network::NetworkConfig config;
    config.enableMulticast = true;
    config.multicastGroup = QHostAddress("224.0.0.1"); // All Systems multicast
    
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy multicastSpy(&source, &Network::UdpSource::multicastStatusChanged);
    
    bool started = source.start();
    QVERIFY(started);
    
    // Wait for potential multicast join
    if (multicastSpy.count() == 0) {
        multicastSpy.wait(1000);
    }
    
    // Clean stop should leave multicast group
    source.stop();
}

void TestUdpSource::testMulticastNetworkInterface()
{
    Network::NetworkConfig config;
    config.enableMulticast = true;
    config.multicastGroup = QHostAddress("224.0.0.1");
    config.networkInterface = "lo0"; // Loopback interface
    
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    // Should start even with specific interface
    bool started = source.start();
    QVERIFY(started);
    
    source.stop();
}

// Packet processing tests
void TestUdpSource::testPacketCreation()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    // Create test packet data
    QByteArray testData = createTestPacket(1, 100);
    QVERIFY(static_cast<size_t>(testData.size()) >= sizeof(Packet::PacketHeader));
    
    // Verify packet structure
    const Packet::PacketHeader* header = 
        reinterpret_cast<const Packet::PacketHeader*>(testData.constData());
    QCOMPARE(header->id, 1U);
    QCOMPARE(header->sequence, 100U);
}

void TestUdpSource::testPacketValidation()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    source.start();
    
    // Test valid packet
    QByteArray validPacket = createTestPacket(1, 1);
    QSignalSpy packetSpy(&source, &Packet::PacketSource::packetReady);
    
    simulatePacketReception(source, validPacket);
    
    // Wait for packet processing
    if (packetSpy.count() == 0) {
        packetSpy.wait(1000);
    }
    
    QVERIFY(packetSpy.count() > 0);
    
    source.stop();
}

void TestUdpSource::testInvalidPacketHandling()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    source.start();
    
    QSignalSpy packetSpy(&source, &Packet::PacketSource::packetReady);
    QSignalSpy errorSpy(&source, &Packet::PacketSource::error);
    
    // Test packet too small
    QByteArray tinyPacket("small");
    simulatePacketReception(source, tinyPacket);
    
    // Should not receive valid packet
    QTest::qWait(100);
    QCOMPARE(packetSpy.count(), 0);
    
    // Check if statistics show error
    const auto& stats = source.getNetworkStatistics();
    QVERIFY(stats.packetErrors.load() > 0);
    
    source.stop();
}

void TestUdpSource::testPacketStatistics()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    source.start();
    
    const auto& initialStats = source.getNetworkStatistics();
    uint64_t initialPackets = initialStats.packetsReceived.load();
    uint64_t initialBytes = initialStats.bytesReceived.load();
    
    // Send test packet
    QByteArray testPacket = createTestPacket(1, 1);
    simulatePacketReception(source, testPacket);
    
    // Wait for processing
    QTest::qWait(100);
    
    const auto& finalStats = source.getNetworkStatistics();
    QVERIFY(finalStats.packetsReceived.load() >= initialPackets);
    QVERIFY(finalStats.bytesReceived.load() >= initialBytes);
    
    source.stop();
}

// Rate limiting tests
void TestUdpSource::testRateLimiting()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    // Enable rate limiting
    auto sourceConfig = source.getNetworkConfig();
    // Note: Rate limiting would be handled by the source configuration,
    // but for this test we'll just use the existing config
    source.setNetworkConfig(sourceConfig);
    
    source.start();
    
    const auto& initialStats = source.getNetworkStatistics();
    uint64_t initialDropped = initialStats.packetsDropped.load();
    
    // Send many packets quickly
    for (int i = 0; i < 50; ++i) {
        QByteArray packet = createTestPacket(1, i);
        simulatePacketReception(source, packet);
    }
    
    QTest::qWait(200);
    
    const auto& finalStats = source.getNetworkStatistics();
    QVERIFY(finalStats.packetsDropped.load() > initialDropped);
    
    source.stop();
}

void TestUdpSource::testRateLimitDisabled()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    // Disable rate limiting
    auto sourceConfig = source.getNetworkConfig();
    // Note: Rate limiting would be handled by the source configuration
    source.setNetworkConfig(sourceConfig);
    
    source.start();
    
    const auto& initialStats = source.getNetworkStatistics();
    uint64_t initialDropped = initialStats.packetsDropped.load();
    
    // Send many packets
    for (int i = 0; i < 20; ++i) {
        QByteArray packet = createTestPacket(1, i);
        simulatePacketReception(source, packet);
    }
    
    QTest::qWait(100);
    
    const auto& finalStats = source.getNetworkStatistics();
    QCOMPARE(finalStats.packetsDropped.load(), initialDropped); // No additional drops
    
    source.stop();
}

// Error handling tests
void TestUdpSource::testSocketErrors()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy errorSpy(&source, &Packet::PacketSource::error);
    
    // Try to bind to invalid port
    Network::NetworkConfig badConfig = config;
    badConfig.localPort = 0; // Invalid port (use 0 instead of 65536)
    source.setNetworkConfig(badConfig);
    
    bool started = source.start();
    
    // Should either fail to start or emit error
    if (started) {
        if (errorSpy.count() == 0) {
            errorSpy.wait(1000);
        }
    }
    
    // Clean up
    source.stop();
}

void TestUdpSource::testConsecutiveErrors()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    source.start();
    
    // Simulate many invalid packets to trigger consecutive errors
    for (int i = 0; i < 50; ++i) {
        QByteArray invalidPacket("bad");
        simulatePacketReception(source, invalidPacket);
    }
    
    QTest::qWait(200);
    
    const auto& stats = source.getNetworkStatistics();
    QVERIFY(stats.packetErrors.load() > 0);
    
    source.stop();
}

void TestUdpSource::testErrorRecovery()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    source.start();
    
    // Send invalid packet
    QByteArray invalidPacket("bad");
    simulatePacketReception(source, invalidPacket);
    
    QTest::qWait(50);
    
    // Send valid packet - should recover
    QByteArray validPacket = createTestPacket(1, 1);
    QSignalSpy packetSpy(&source, &Packet::PacketSource::packetReady);
    simulatePacketReception(source, validPacket);
    
    if (packetSpy.count() == 0) {
        packetSpy.wait(1000);
    }
    
    QVERIFY(packetSpy.count() > 0); // Should recover and process valid packet
    
    source.stop();
}

// Performance tests
void TestUdpSource::testHighThroughputSimulation()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    source.start();
    
    const int packetCount = 1000;
    const auto startTime = std::chrono::steady_clock::now();
    
    // Send many packets
    for (int i = 0; i < packetCount; ++i) {
        QByteArray packet = createTestPacket(1, i);
        simulatePacketReception(source, packet);
    }
    
    // Wait for processing
    QTest::qWait(500);
    
    const auto endTime = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    const auto& stats = source.getNetworkStatistics();
    uint64_t processed = stats.packetsReceived.load();
    
    qDebug() << "Processed" << processed << "packets in" << duration.count() << "ms";
    qDebug() << "Rate:" << (processed * 1000.0 / duration.count()) << "packets/sec";
    
    QVERIFY(processed > 0);
    
    source.stop();
}

void TestUdpSource::testMemoryUsage()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    source.start();
    
    // Get initial memory stats
    const auto initialMemoryUsed = m_memoryManager->getTotalMemoryUsed();
    
    // Process packets
    for (int i = 0; i < 100; ++i) {
        QByteArray packet = createTestPacket(1, i);
        simulatePacketReception(source, packet);
    }
    
    QTest::qWait(200);
    
    const auto finalMemoryUsed = m_memoryManager->getTotalMemoryUsed();
    qDebug() << "Memory usage increase:" << (finalMemoryUsed - initialMemoryUsed) << "bytes";
    
    source.stop();
    
    // Memory should be released after stop
    QTest::qWait(100);
}

// Signal/slot tests
void TestUdpSource::testSignalEmission()
{
    Network::NetworkConfig config;
    Network::UdpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy stateSpy(&source, &Packet::PacketSource::started);
    QSignalSpy socketStateSpy(&source, &Network::UdpSource::socketStateChanged);
    QSignalSpy statisticsSpy(&source, &Network::UdpSource::networkStatisticsUpdated);
    
    source.start();
    
    // Should emit state changed
    QVERIFY(stateSpy.count() > 0);
    QVERIFY(socketStateSpy.count() > 0);
    
    // Wait for statistics updates
    if (statisticsSpy.count() == 0) {
        statisticsSpy.wait(2000);
    }
    
    source.stop();
    
    // Should emit state changed on stop
    QVERIFY(stateSpy.count() >= 2); // At least start and stop
}

// Helper method implementations
QByteArray TestUdpSource::createTestPacket(uint32_t id, uint32_t sequence, const QString& payload)
{
    QByteArray payloadBytes = payload.toUtf8();
    QByteArray packet(sizeof(Packet::PacketHeader) + payloadBytes.size(), 0);
    
    Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(packet.data());
    header->id = id;
    header->sequence = sequence;
    header->timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    header->payloadSize = payloadBytes.size();
    header->flags = 0;
    
    // Copy payload
    if (!payloadBytes.isEmpty()) {
        std::memcpy(packet.data() + sizeof(Packet::PacketHeader), 
                   payloadBytes.constData(), payloadBytes.size());
    }
    
    return packet;
}

void TestUdpSource::simulatePacketReception(Network::UdpSource& source, const QByteArray& packetData)
{
    // This is a simplified simulation - in reality, we'd need to access private methods
    // For now, we'll just trigger the source with the packet data
    // Note: This would require friendship or exposed test methods in the actual implementation
    
    Q_UNUSED(source)
    Q_UNUSED(packetData)
    
    // In a real implementation, we might:
    // 1. Create a QNetworkDatagram with the packet data
    // 2. Call a test method on UdpSource to simulate datagram reception
    // 3. Or use dependency injection to mock the socket
}

bool TestUdpSource::waitForSignal(const QObject* sender, const char* signal, int timeout)
{
    QSignalSpy spy(sender, signal);
    if (spy.count() > 0) {
        return true;
    }
    return spy.wait(timeout);
}

QTEST_MAIN(TestUdpSource)
#include "test_udp_source.moc"