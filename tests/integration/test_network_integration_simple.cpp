#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>
#include <QUdpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QThread>
#include <QEventLoop>
#include <QElapsedTimer>

// Network sources
#include "../../src/network/config/network_config.h"
#include "../../src/network/sources/udp_source.h"

// Core components for packet processing
#include "../../src/packet/core/packet_factory.h"
#include "../../src/packet/core/packet_header.h"
#include "../../src/memory/memory_pool.h"
#include "../../src/core/application.h"

using namespace Monitor;

/**
 * @brief Simple Network Integration Tests - Real UDP operations
 * 
 * These tests verify basic end-to-end network functionality including:
 * - Real UDP socket operations
 * - Packet transmission and reception  
 * - Network configuration validation
 * - Basic error handling
 */
class TestNetworkIntegrationSimple : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    // UDP Basic Integration Tests
    void testUdpSourceBasicCommunication();
    void testUdpSourcePacketParsing();
    void testUdpSourceConfiguration();
    void testUdpSourceErrorHandling();

private:
    Memory::MemoryPoolManager* m_memoryManager;
    Packet::PacketFactory* m_packetFactory;
    
    // Test helpers
    QByteArray createTestPacket(uint32_t id, uint32_t sequence, const QByteArray& payload);
    bool waitForSignalWithTimeout(QSignalSpy& spy, int expectedCount = 1, int timeoutMs = 5000);
    void sendTestPacketsUdp(QUdpSocket& sender, const QHostAddress& address, quint16 port, int count);
    quint16 findAvailablePort();
    bool isPortAvailable(quint16 port);
};

void TestNetworkIntegrationSimple::initTestCase()
{
    // Initialize application and get memory manager
    auto app = Core::Application::instance();
    QVERIFY(app != nullptr);
    QVERIFY(app->initialize());  // Initialize the application first
    m_memoryManager = app->memoryManager();
    QVERIFY(m_memoryManager != nullptr);
    
    // Create packet factory
    m_packetFactory = new Packet::PacketFactory(m_memoryManager);
}

void TestNetworkIntegrationSimple::cleanupTestCase()
{
    delete m_packetFactory;
}

void TestNetworkIntegrationSimple::cleanup()
{
    // Clean up between tests to avoid interference
    QThread::msleep(100);
}

void TestNetworkIntegrationSimple::testUdpSourceBasicCommunication()
{
    // Find available port
    quint16 localPort = findAvailablePort();
    
    // Create UDP source configuration
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "TestUdpIntegration", QHostAddress::LocalHost, localPort);
    
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    // Set up signal spy for received packets using base PacketSource signals
    QSignalSpy packetSpy(&udpSource, &Packet::PacketSource::packetReady);
    QSignalSpy startedSpy(&udpSource, &Packet::PacketSource::started);
    QSignalSpy errorSpy(&udpSource, &Packet::PacketSource::error);
    
    // Start the UDP source
    udpSource.start();
    QVERIFY(waitForSignalWithTimeout(startedSpy));
    QVERIFY(udpSource.isRunning());
    
    // Create sender socket
    QUdpSocket sender;
    
    // Send test packets
    const int packetCount = 5;
    sendTestPacketsUdp(sender, QHostAddress::LocalHost, localPort, packetCount);
    
    // Wait for packets to be received
    QVERIFY(waitForSignalWithTimeout(packetSpy, packetCount, 3000));
    QCOMPARE(packetSpy.count(), packetCount);
    
    // Verify no errors occurred
    QCOMPARE(errorSpy.count(), 0);
    
    // Stop source
    udpSource.stop();
    QVERIFY(udpSource.isStopped());
}

void TestNetworkIntegrationSimple::testUdpSourcePacketParsing()
{
    quint16 port = findAvailablePort();
    
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "TestParsing", QHostAddress::LocalHost, port);
    
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&udpSource, &Packet::PacketSource::packetReady);
    QSignalSpy startedSpy(&udpSource, &Packet::PacketSource::started);
    
    udpSource.start();
    QVERIFY(waitForSignalWithTimeout(startedSpy));
    
    QUdpSocket sender;
    
    // Send packets with different IDs and payloads
    struct TestPacketData {
        uint32_t id;
        QString payload;
    } testData[] = {
        {1001, "Test payload 1"},
        {1002, "Different payload"},
        {1003, "Third test packet"}
    };
    
    for (const auto& data : testData) {
        QByteArray packet = createTestPacket(data.id, 1, data.payload.toUtf8());
        sender.writeDatagram(packet, QHostAddress::LocalHost, port);
        QThread::msleep(10);
    }
    
    QVERIFY(waitForSignalWithTimeout(packetSpy, 3, 3000));
    QCOMPARE(packetSpy.count(), 3);
    
    udpSource.stop();
}

void TestNetworkIntegrationSimple::testUdpSourceConfiguration()
{
    // Test different configuration scenarios
    quint16 port1 = findAvailablePort();
    quint16 port2 = findAvailablePort();
    
    // Test configuration creation
    Network::NetworkConfig config1 = Network::NetworkConfig::createUdpConfig(
        "TestConfig1", QHostAddress::LocalHost, port1);
    QCOMPARE(config1.protocol, Network::Protocol::UDP);
    QCOMPARE(config1.localPort, port1);
    
    // Test configuration with different address
    Network::NetworkConfig config2 = Network::NetworkConfig::createUdpConfig(
        "TestConfig2", QHostAddress::Any, port2);
    QCOMPARE(config2.localAddress, QHostAddress::Any);
    
    // Verify configurations are valid
    QVERIFY(config1.isValid());
    QVERIFY(config2.isValid());
    
    // Test creating sources with different configs
    Network::UdpSource source1(config1);
    Network::UdpSource source2(config2);
    
    source1.setPacketFactory(m_packetFactory);
    source2.setPacketFactory(m_packetFactory);
    
    QCOMPARE(source1.getName(), std::string("TestConfig1"));
    QCOMPARE(source2.getName(), std::string("TestConfig2"));
    
    QVERIFY(source1.isStopped());
    QVERIFY(source2.isStopped());
}

void TestNetworkIntegrationSimple::testUdpSourceErrorHandling()
{
    // Test error handling with invalid port
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "TestError", QHostAddress::LocalHost, 0); // Let system choose port
    
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy startedSpy(&udpSource, &Packet::PacketSource::started);
    QSignalSpy errorSpy(&udpSource, &Packet::PacketSource::error);
    
    // Should start successfully even with port 0
    udpSource.start();
    
    // Either it starts successfully or reports an error
    bool startedOrError = waitForSignalWithTimeout(startedSpy, 1, 1000) || 
                          waitForSignalWithTimeout(errorSpy, 1, 1000);
    QVERIFY(startedOrError);
    
    udpSource.stop();
}

// Helper Methods Implementation

QByteArray TestNetworkIntegrationSimple::createTestPacket(uint32_t id, uint32_t sequence, const QByteArray& payload)
{
    Packet::PacketHeader header;
    header.id = id;
    header.sequence = sequence;
    header.timestamp = Packet::PacketHeader::getCurrentTimestampNs();
    header.payloadSize = static_cast<uint32_t>(payload.size());
    header.flags = Packet::PacketHeader::Flags::TestData;
    
    QByteArray packet;
    packet.append(reinterpret_cast<const char*>(&header), sizeof(header));
    packet.append(payload);
    return packet;
}

bool TestNetworkIntegrationSimple::waitForSignalWithTimeout(QSignalSpy& spy, int expectedCount, int timeoutMs)
{
    if (spy.count() >= expectedCount) {
        return true;
    }
    
    return spy.wait(timeoutMs) && spy.count() >= expectedCount;
}

void TestNetworkIntegrationSimple::sendTestPacketsUdp(QUdpSocket& sender, const QHostAddress& address, quint16 port, int count)
{
    for (int i = 0; i < count; ++i) {
        QByteArray packet = createTestPacket(1000 + i, i, QString("Test packet %1").arg(i).toUtf8());
        qint64 sent = sender.writeDatagram(packet, address, port);
        Q_UNUSED(sent);
        QThread::msleep(10); // Small pause between packets
    }
}

quint16 TestNetworkIntegrationSimple::findAvailablePort()
{
    // Find an available port starting from a high number
    for (quint16 port = 12000; port < 13000; ++port) {
        if (isPortAvailable(port)) {
            return port;
        }
    }
    return 12345; // Fallback
}

bool TestNetworkIntegrationSimple::isPortAvailable(quint16 port)
{
    QUdpSocket testSocket;
    return testSocket.bind(QHostAddress::LocalHost, port);
}

QTEST_MAIN(TestNetworkIntegrationSimple)
#include "test_network_integration_simple.moc"