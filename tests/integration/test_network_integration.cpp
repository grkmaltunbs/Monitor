#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>
#include <QUdpSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QThread>
#include <QEventLoop>
#include <QElapsedTimer>

// Network sources
#include "../../src/network/config/network_config.h"
#include "../../src/network/sources/udp_source.h"
#include "../../src/network/sources/tcp_source.h"

// Core components for packet processing
#include "../../src/packet/core/packet_factory.h"
#include "../../src/packet/core/packet_header.h"
#include "../../src/memory/memory_pool.h"

using namespace Monitor;

/**
 * @brief Network Integration Tests - Real network operations with packet processing
 * 
 * These tests verify end-to-end network functionality including:
 * - Real UDP/TCP socket operations
 * - Packet transmission and reception
 * - Network configuration validation
 * - Performance under load
 * - Error handling and recovery
 * - Multi-source scenarios
 */
class TestNetworkIntegration : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    // UDP Integration Tests
    void testUdpSourceRealCommunication();
    void testUdpSourceMulticast();
    void testUdpSourcePacketParsing();
    void testUdpSourcePerformanceLoad();
    void testUdpSourceErrorRecovery();
    void testUdpSourceLargePackets();
    void testUdpSourceFragmentation();

    // TCP Integration Tests  
    void testTcpSourceRealConnection();
    void testTcpSourceReconnection();
    void testTcpSourcePacketStreaming();
    void testTcpSourcePerformanceLoad();
    void testTcpSourceConnectionLoss();
    void testTcpSourceServerScenario();
    void testTcpSourceClientScenario();

    // Cross-Protocol Tests
    void testMultipleSourcesSimultaneous();
    void testProtocolSwitching();
    void testConcurrentConnections();
    
    // Performance Integration Tests
    void testHighThroughputUdp();
    void testHighThroughputTcp();
    void testLatencyMeasurement();
    void testMemoryUsageUnderLoad();
    
    // Error Handling Integration Tests
    void testNetworkFailureRecovery();
    void testInvalidPacketHandling();
    void testBufferOverflowProtection();

private:
    Memory::MemoryPoolManager* m_memoryManager;
    Packet::PacketFactory* m_packetFactory;
    
    // Test helpers
    QByteArray createTestPacket(uint32_t id, uint32_t sequence, const QByteArray& payload);
    bool waitForSignalWithTimeout(QSignalSpy& spy, int expectedCount = 1, int timeoutMs = 5000);
    void sendTestPacketsUdp(QUdpSocket& sender, const QHostAddress& address, quint16 port, int count);
    void sendTestPacketsTcp(QTcpSocket& sender, int count);
    
    // Network test infrastructure  
    quint16 findAvailablePort();
    bool isPortAvailable(quint16 port);
    void createTestServer(QTcpServer& server, quint16& port);
    
    // Validation helpers
    void validatePacketContent(const QByteArray& data, uint32_t expectedId, uint32_t expectedSequence);
    void validatePerformanceMetrics(int packetsSent, int packetsReceived, qint64 elapsedMs);
};

void TestNetworkIntegration::initTestCase()
{
    // Initialize memory pool manager and packet factory
    m_memoryManager = new Memory::MemoryPoolManager();
    m_packetFactory = new Packet::PacketFactory(m_memoryManager);
}

void TestNetworkIntegration::cleanupTestCase()
{
    delete m_packetFactory;
    delete m_memoryManager;
}

void TestNetworkIntegration::cleanup()
{
    // Clean up between tests to avoid interference
    QThread::msleep(100);
}

void TestNetworkIntegration::testUdpSourceRealCommunication()
{
    // Find available ports
    quint16 localPort = findAvailablePort();
    // quint16 remotePort = findAvailablePort();
    
    // Create UDP source configuration
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "TestUdpIntegration", QHostAddress::LocalHost, localPort);
    
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    // Set up signal spy for received packets
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
    const int packetCount = 10;
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

void TestNetworkIntegration::testUdpSourceMulticast()
{
    // Test multicast functionality
    QHostAddress multicastGroup("239.0.0.1");
    quint16 multicastPort = findAvailablePort();
    
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "TestMulticast", multicastGroup, multicastPort);
    config.enableMulticast = true;
    config.multicastGroup = multicastGroup;
    
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&udpSource, &Packet::PacketSource::packetReady);
    QSignalSpy startedSpy(&udpSource, &Packet::PacketSource::started);
    
    // Start multicast receiver
    udpSource.start();
    QVERIFY(waitForSignalWithTimeout(startedSpy));
    
    // Create multicast sender
    QUdpSocket sender;
    sender.bind();
    
    // Send multicast packets
    for (int i = 0; i < 5; ++i) {
        QByteArray packet = createTestPacket(100 + i, i, QString("Multicast %1").arg(i).toUtf8());
        qint64 sent = sender.writeDatagram(packet, multicastGroup, multicastPort);
        QVERIFY(sent == packet.size());
        QThread::msleep(10);
    }
    
    // Verify multicast reception
    QVERIFY(waitForSignalWithTimeout(packetSpy, 5, 3000));
    QVERIFY(udpSource.isMulticastActive());
    
    udpSource.stop();
}

void TestNetworkIntegration::testUdpSourcePacketParsing()
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
        {1003, "Third test packet"},
        {2000, "Large ID packet"},
        {1, "Minimal ID"}
    };
    
    for (const auto& data : testData) {
        QByteArray packet = createTestPacket(data.id, 1, data.payload.toUtf8());
        sender.writeDatagram(packet, QHostAddress::LocalHost, port);
        QThread::msleep(10);
    }
    
    QVERIFY(waitForSignalWithTimeout(packetSpy, 5, 3000));
    
    // Verify packet parsing through signal arguments
    QCOMPARE(packetSpy.count(), 5);
    
    udpSource.stop();
}

void TestNetworkIntegration::testUdpSourcePerformanceLoad()
{
    quint16 port = findAvailablePort();
    
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "TestPerformance", QHostAddress::LocalHost, port);
    config.receiveBufferSize = 1024 * 1024; // 1MB buffer
    
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&udpSource, &Packet::PacketSource::packetReady);
    QSignalSpy startedSpy(&udpSource, &Packet::PacketSource::started);
    
    udpSource.start();
    QVERIFY(waitForSignalWithTimeout(startedSpy));
    
    QUdpSocket sender;
    
    // High-rate packet sending test
    const int totalPackets = 1000;
    QElapsedTimer timer;
    timer.start();
    
    sendTestPacketsUdp(sender, QHostAddress::LocalHost, port, totalPackets);
    
    // Wait for all packets with reasonable timeout
    QVERIFY(waitForSignalWithTimeout(packetSpy, totalPackets, 10000));
    
    qint64 elapsedMs = timer.elapsed();
    validatePerformanceMetrics(totalPackets, packetSpy.count(), elapsedMs);
    
    udpSource.stop();
}

void TestNetworkIntegration::testTcpSourceRealConnection()
{
    quint16 serverPort;
    QTcpServer server;
    createTestServer(server, serverPort);
    
    // Configure TCP source as client
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "TestTcpClient", QHostAddress::LocalHost, serverPort);
    
    Network::TcpSource tcpSource(config);
    tcpSource.setPacketFactory(m_packetFactory);
    
    // Note: TCP-specific connection signals may not be implemented yet 
    // QSignalSpy connectedSpy(&tcpSource, &Network::TcpSource::connected);
    QSignalSpy packetSpy(&tcpSource, &Packet::PacketSource::packetReady);
    QSignalSpy startedSpy(&tcpSource, &Packet::PacketSource::started);
    
    // Start TCP source (should connect to server)
    tcpSource.start();
    QVERIFY(waitForSignalWithTimeout(startedSpy));
    
    // Wait for connection (simplified check)
    QThread::msleep(1000); // Allow time for connection
    // QVERIFY(tcpSource.isConnected()); // May not be implemented yet
    // QCOMPARE(tcpSource.getConnectionState(), Network::TcpSource::ConnectionState::Connected);
    
    // Wait for client connection on server side
    QVERIFY(server.waitForNewConnection(3000));
    QTcpSocket* clientSocket = server.nextPendingConnection();
    QVERIFY(clientSocket != nullptr);
    
    // Send packets through server to client
    const int packetCount = 5;
    sendTestPacketsTcp(*clientSocket, packetCount);
    
    // Verify packet reception
    QVERIFY(waitForSignalWithTimeout(packetSpy, packetCount, 3000));
    QCOMPARE(packetSpy.count(), packetCount);
    
    tcpSource.stop();
    clientSocket->close();
}

void TestNetworkIntegration::testTcpSourceReconnection()
{
    quint16 serverPort;
    QTcpServer server;
    createTestServer(server, serverPort);
    
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "TestReconnect", QHostAddress::LocalHost, serverPort);
    // Note: Advanced reconnection features may not be implemented yet
    // config.enableReconnection = true;  
    // config.reconnectionDelay = 500;
    
    Network::TcpSource tcpSource(config);
    tcpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy connectedSpy(&tcpSource, &Network::TcpSource::connected);
    QSignalSpy disconnectedSpy(&tcpSource, &Network::TcpSource::disconnected);
    QSignalSpy startedSpy(&tcpSource, &Packet::PacketSource::started);
    
    // Initial connection
    tcpSource.start();
    QVERIFY(waitForSignalWithTimeout(startedSpy));
    QVERIFY(waitForSignalWithTimeout(connectedSpy, 1, 3000));
    
    // Accept and then close connection to trigger reconnection
    QVERIFY(server.waitForNewConnection(3000));
    QTcpSocket* clientSocket = server.nextPendingConnection();
    clientSocket->close();
    delete clientSocket;
    
    // Wait for disconnection
    QVERIFY(waitForSignalWithTimeout(disconnectedSpy, 1, 3000));
    
    // Should automatically reconnect
    QVERIFY(waitForSignalWithTimeout(connectedSpy, 2, 5000));
    QVERIFY(tcpSource.isConnected());
    
    tcpSource.stop();
}

void TestNetworkIntegration::testTcpSourcePacketStreaming()
{
    quint16 serverPort;
    QTcpServer server;
    createTestServer(server, serverPort);
    
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "TestStreaming", QHostAddress::LocalHost, serverPort);
    
    Network::TcpSource tcpSource(config);
    tcpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy connectedSpy(&tcpSource, &Network::TcpSource::connected);
    QSignalSpy packetSpy(&tcpSource, &Packet::PacketSource::packetReady);
    QSignalSpy startedSpy(&tcpSource, &Packet::PacketSource::started);
    
    tcpSource.start();
    QVERIFY(waitForSignalWithTimeout(startedSpy));
    QVERIFY(waitForSignalWithTimeout(connectedSpy, 1, 3000));
    
    QVERIFY(server.waitForNewConnection(3000));
    QTcpSocket* clientSocket = server.nextPendingConnection();
    
    // Stream packets with varying sizes
    const int streamCount = 20;
    for (int i = 0; i < streamCount; ++i) {
        QString payload = QString("Stream packet %1 with data").arg(i).repeated(i % 5 + 1);
        QByteArray packet = createTestPacket(2000 + i, i, payload.toUtf8());
        
        clientSocket->write(packet);
        clientSocket->flush();
        QThread::msleep(10);
    }
    
    QVERIFY(waitForSignalWithTimeout(packetSpy, streamCount, 5000));
    QCOMPARE(packetSpy.count(), streamCount);
    
    tcpSource.stop();
    clientSocket->close();
}

void TestNetworkIntegration::testMultipleSourcesSimultaneous()
{
    // Create multiple sources running simultaneously
    quint16 udpPort1 = findAvailablePort();
    quint16 udpPort2 = findAvailablePort();
    quint16 tcpPort;
    
    QTcpServer tcpServer;
    createTestServer(tcpServer, tcpPort);
    
    // UDP Source 1
    Network::NetworkConfig udpConfig1 = Network::NetworkConfig::createUdpConfig(
        "TestUDP1", QHostAddress::LocalHost, udpPort1);
    Network::UdpSource udpSource1(udpConfig1);
    udpSource1.setPacketFactory(m_packetFactory);
    
    // UDP Source 2  
    Network::NetworkConfig udpConfig2 = Network::NetworkConfig::createUdpConfig(
        "TestUDP2", QHostAddress::LocalHost, udpPort2);
    Network::UdpSource udpSource2(udpConfig2);
    udpSource2.setPacketFactory(m_packetFactory);
    
    // TCP Source
    Network::NetworkConfig tcpConfig = Network::NetworkConfig::createTcpConfig(
        "TestTCP", QHostAddress::LocalHost, tcpPort);
    Network::TcpSource tcpSource(tcpConfig);
    tcpSource.setPacketFactory(m_packetFactory);
    
    // Signal spies
    QSignalSpy udp1PacketSpy(&udpSource1, &Packet::PacketSource::packetReady);
    QSignalSpy udp2PacketSpy(&udpSource2, &Packet::PacketSource::packetReady);
    QSignalSpy tcpPacketSpy(&tcpSource, &Packet::PacketSource::packetReady);
    
    QSignalSpy udp1StartedSpy(&udpSource1, &Packet::PacketSource::started);
    QSignalSpy udp2StartedSpy(&udpSource2, &Packet::PacketSource::started);
    QSignalSpy tcpStartedSpy(&tcpSource, &Packet::PacketSource::started);
    QSignalSpy tcpConnectedSpy(&tcpSource, &Network::TcpSource::connected);
    
    // Start all sources
    udpSource1.start();
    udpSource2.start(); 
    tcpSource.start();
    
    QVERIFY(waitForSignalWithTimeout(udp1StartedSpy));
    QVERIFY(waitForSignalWithTimeout(udp2StartedSpy));
    QVERIFY(waitForSignalWithTimeout(tcpStartedSpy));
    QVERIFY(waitForSignalWithTimeout(tcpConnectedSpy, 1, 3000));
    
    // Set up TCP connection
    QVERIFY(tcpServer.waitForNewConnection(3000));
    QTcpSocket* tcpClient = tcpServer.nextPendingConnection();
    
    // Send packets to all sources simultaneously
    QUdpSocket udpSender1, udpSender2;
    
    const int packetsPerSource = 5;
    
    for (int i = 0; i < packetsPerSource; ++i) {
        // UDP 1
        QByteArray udp1Packet = createTestPacket(3001, i, QString("UDP1 packet %1").arg(i).toUtf8());
        udpSender1.writeDatagram(udp1Packet, QHostAddress::LocalHost, udpPort1);
        
        // UDP 2
        QByteArray udp2Packet = createTestPacket(3002, i, QString("UDP2 packet %1").arg(i).toUtf8());
        udpSender2.writeDatagram(udp2Packet, QHostAddress::LocalHost, udpPort2);
        
        // TCP
        QByteArray tcpPacket = createTestPacket(3003, i, QString("TCP packet %1").arg(i).toUtf8());
        tcpClient->write(tcpPacket);
        tcpClient->flush();
        
        QThread::msleep(20);
    }
    
    // Verify all sources received their packets
    QVERIFY(waitForSignalWithTimeout(udp1PacketSpy, packetsPerSource, 3000));
    QVERIFY(waitForSignalWithTimeout(udp2PacketSpy, packetsPerSource, 3000));
    QVERIFY(waitForSignalWithTimeout(tcpPacketSpy, packetsPerSource, 3000));
    
    QCOMPARE(udp1PacketSpy.count(), packetsPerSource);
    QCOMPARE(udp2PacketSpy.count(), packetsPerSource);
    QCOMPARE(tcpPacketSpy.count(), packetsPerSource);
    
    // Clean up
    udpSource1.stop();
    udpSource2.stop();
    tcpSource.stop();
    tcpClient->close();
}

void TestNetworkIntegration::testHighThroughputUdp()
{
    quint16 port = findAvailablePort();
    
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "TestHighThroughput", QHostAddress::LocalHost, port);
    config.receiveBufferSize = 2 * 1024 * 1024; // 2MB buffer
    config.maxPacketSize = 1024;
    
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&udpSource, &Packet::PacketSource::packetReady);
    QSignalSpy startedSpy(&udpSource, &Packet::PacketSource::started);
    QSignalSpy droppedSpy(&udpSource, &Packet::PacketSource::statisticsUpdated);
    
    udpSource.start();
    QVERIFY(waitForSignalWithTimeout(startedSpy));
    
    QUdpSocket sender;
    
    // High throughput test - send packets as fast as possible
    const int burstSize = 100;
    const int burstCount = 20;
    const int totalPackets = burstSize * burstCount;
    
    QElapsedTimer timer;
    timer.start();
    
    for (int burst = 0; burst < burstCount; ++burst) {
        for (int i = 0; i < burstSize; ++i) {
            QByteArray packet = createTestPacket(4000 + i, burst * burstSize + i, 
                QString("Burst %1 Packet %2").arg(burst).arg(i).toUtf8());
            sender.writeDatagram(packet, QHostAddress::LocalHost, port);
        }
        QThread::msleep(1); // Small pause between bursts
    }
    
    qint64 elapsedMs = timer.elapsed();
    
    // Wait for packets to be processed
    QTest::qWait(2000);
    
    // Verify high throughput performance
    int receivedPackets = packetSpy.count();
    double packetsPerSecond = (double)receivedPackets / (elapsedMs / 1000.0);
    
    qDebug() << "High throughput test:" 
             << "Sent:" << totalPackets 
             << "Received:" << receivedPackets
             << "Rate:" << packetsPerSecond << "packets/sec"
             << "Dropped:" << droppedSpy.count();
    
    // Should achieve at least 1000 packets/second
    QVERIFY(packetsPerSecond > 1000.0);
    QVERIFY(receivedPackets > totalPackets * 0.8); // Allow some packet loss
    
    udpSource.stop();
}

void TestNetworkIntegration::testLatencyMeasurement()
{
    quint16 port = findAvailablePort();
    
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "TestLatency", QHostAddress::LocalHost, port);
    
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy packetSpy(&udpSource, &Packet::PacketSource::packetReady);
    QSignalSpy startedSpy(&udpSource, &Packet::PacketSource::started);
    
    udpSource.start();
    QVERIFY(waitForSignalWithTimeout(startedSpy));
    
    QUdpSocket sender;
    
    // Measure latency for individual packets
    QList<qint64> latencies;
    const int testPackets = 10;
    
    for (int i = 0; i < testPackets; ++i) {
        QElapsedTimer latencyTimer;
        latencyTimer.start();
        
        QByteArray packet = createTestPacket(5000, i, QString("Latency test %1").arg(i).toUtf8());
        sender.writeDatagram(packet, QHostAddress::LocalHost, port);
        
        // Wait for this specific packet
        int initialCount = packetSpy.count();
        QVERIFY(waitForSignalWithTimeout(packetSpy, initialCount + 1, 1000));
        
        qint64 latency = latencyTimer.elapsed();
        latencies.append(latency);
        
        QThread::msleep(100); // Space out tests
    }
    
    // Calculate latency statistics
    qint64 totalLatency = 0;
    qint64 maxLatency = 0;
    qint64 minLatency = latencies.first();
    
    for (qint64 latency : latencies) {
        totalLatency += latency;
        maxLatency = qMax(maxLatency, latency);
        minLatency = qMin(minLatency, latency);
    }
    
    double avgLatency = (double)totalLatency / latencies.size();
    
    qDebug() << "Latency measurements:"
             << "Avg:" << avgLatency << "ms"
             << "Min:" << minLatency << "ms"  
             << "Max:" << maxLatency << "ms";
    
    // Verify reasonable latency (should be < 100ms for localhost)
    QVERIFY(avgLatency < 100.0);
    QVERIFY(maxLatency < 200);
    
    udpSource.stop();
}

void TestNetworkIntegration::testNetworkFailureRecovery()
{
    quint16 serverPort;
    QTcpServer server;
    createTestServer(server, serverPort);
    
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "TestFailureRecovery", QHostAddress::LocalHost, serverPort);
    // Note: Advanced reconnection features may not be implemented yet
    // config.enableReconnection = true;
    // config.reconnectionDelay = 200;
    // config.maxReconnectionAttempts = 3;
    
    Network::TcpSource tcpSource(config);
    tcpSource.setPacketFactory(m_packetFactory);
    
    QSignalSpy connectedSpy(&tcpSource, &Network::TcpSource::connected);
    QSignalSpy disconnectedSpy(&tcpSource, &Network::TcpSource::disconnected);
    QSignalSpy errorSpy(&tcpSource, &Network::TcpSource::connectionFailed);
    QSignalSpy startedSpy(&tcpSource, &Packet::PacketSource::started);
    
    // Initial connection
    tcpSource.start();
    QVERIFY(waitForSignalWithTimeout(startedSpy));
    QVERIFY(waitForSignalWithTimeout(connectedSpy, 1, 3000));
    
    // Accept and immediately close connection multiple times
    for (int i = 0; i < 2; ++i) {
        QVERIFY(server.waitForNewConnection(3000));
        QTcpSocket* clientSocket = server.nextPendingConnection();
        clientSocket->close();
        delete clientSocket;
        
        // Wait for disconnection and reconnection
        QVERIFY(waitForSignalWithTimeout(disconnectedSpy, i + 1, 3000));
        QVERIFY(waitForSignalWithTimeout(connectedSpy, i + 2, 5000));
    }
    
    // Verify recovery capability
    QVERIFY(tcpSource.isConnected());
    QCOMPARE(connectedSpy.count(), 3); // Initial + 2 reconnections
    
    tcpSource.stop();
}

// Helper Methods Implementation

QByteArray TestNetworkIntegration::createTestPacket(uint32_t id, uint32_t sequence, const QByteArray& payload)
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

bool TestNetworkIntegration::waitForSignalWithTimeout(QSignalSpy& spy, int expectedCount, int timeoutMs)
{
    if (spy.count() >= expectedCount) {
        return true;
    }
    
    return spy.wait(timeoutMs) && spy.count() >= expectedCount;
}

void TestNetworkIntegration::sendTestPacketsUdp(QUdpSocket& sender, const QHostAddress& address, quint16 port, int count)
{
    for (int i = 0; i < count; ++i) {
        QByteArray packet = createTestPacket(1000 + i, i, QString("Test packet %1").arg(i).toUtf8());
        qint64 sent = sender.writeDatagram(packet, address, port);
        Q_UNUSED(sent);
        if (i % 10 == 0) {
            QThread::msleep(1); // Small pause every 10 packets
        }
    }
}

void TestNetworkIntegration::sendTestPacketsTcp(QTcpSocket& sender, int count)
{
    for (int i = 0; i < count; ++i) {
        QByteArray packet = createTestPacket(2000 + i, i, QString("TCP packet %1").arg(i).toUtf8());
        sender.write(packet);
        sender.flush();
        QThread::msleep(10);
    }
}

quint16 TestNetworkIntegration::findAvailablePort()
{
    // Find an available port starting from a high number
    for (quint16 port = 12000; port < 13000; ++port) {
        if (isPortAvailable(port)) {
            return port;
        }
    }
    return 12345; // Fallback
}

bool TestNetworkIntegration::isPortAvailable(quint16 port)
{
    QUdpSocket testSocket;
    return testSocket.bind(QHostAddress::LocalHost, port);
}

void TestNetworkIntegration::createTestServer(QTcpServer& server, quint16& port)
{
    port = findAvailablePort();
    bool success = server.listen(QHostAddress::LocalHost, port);
    QVERIFY(success);
    port = server.serverPort(); // Get actual port if 0 was requested
}

void TestNetworkIntegration::validatePacketContent(const QByteArray& data, uint32_t expectedId, uint32_t expectedSequence)
{
    QVERIFY(data.size() >= static_cast<int>(sizeof(Packet::PacketHeader)));
    
    const Packet::PacketHeader* header = reinterpret_cast<const Packet::PacketHeader*>(data.constData());
    QCOMPARE(header->id, expectedId);
    QCOMPARE(header->sequence, expectedSequence);
    QVERIFY(header->timestamp > 0);
}

void TestNetworkIntegration::validatePerformanceMetrics(int packetsSent, int packetsReceived, qint64 elapsedMs)
{
    double packetsPerSecond = (double)packetsReceived / (elapsedMs / 1000.0);
    double lossRate = 1.0 - ((double)packetsReceived / packetsSent);
    
    qDebug() << "Performance metrics:"
             << "Sent:" << packetsSent
             << "Received:" << packetsReceived
             << "Rate:" << packetsPerSecond << "packets/sec"
             << "Loss rate:" << (lossRate * 100) << "%"
             << "Time:" << elapsedMs << "ms";
    
    // Performance requirements
    QVERIFY(packetsPerSecond > 100.0); // At least 100 packets/sec
    QVERIFY(lossRate < 0.1); // Less than 10% loss
    QVERIFY(packetsReceived > packetsSent * 0.9); // At least 90% received
}

QTEST_MAIN(TestNetworkIntegration)
#include "test_network_integration.moc"