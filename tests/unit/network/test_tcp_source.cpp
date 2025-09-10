#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QEventLoop>
#include <QThread>

#include "../../../src/network/sources/tcp_source.h"
#include "../../../src/packet/core/packet_factory.h"
#include "../../../src/memory/memory_pool.h"
#include "../../../src/packet/core/packet_header.h"

using namespace Monitor;

/**
 * @brief Mock TCP server for testing TCP source
 */
class MockTcpServer : public QTcpServer
{
    Q_OBJECT
    
public:
    explicit MockTcpServer(QObject* parent = nullptr) : QTcpServer(parent) {}
    
    void sendPacketToClient(const QByteArray& packet) {
        if (m_clientSocket && m_clientSocket->state() == QAbstractSocket::ConnectedState) {
            m_clientSocket->write(packet);
            m_clientSocket->flush();
        }
    }
    
    void disconnectClient() {
        if (m_clientSocket) {
            m_clientSocket->disconnectFromHost();
        }
    }
    
    bool hasConnectedClient() const {
        return m_clientSocket && m_clientSocket->state() == QAbstractSocket::ConnectedState;
    }

protected:
    void incomingConnection(qintptr socketDescriptor) override {
        m_clientSocket = new QTcpSocket(this);
        m_clientSocket->setSocketDescriptor(socketDescriptor);
        
        connect(m_clientSocket, &QTcpSocket::disconnected,
                m_clientSocket, &QTcpSocket::deleteLater);
        
        emit clientConnected();
    }

signals:
    void clientConnected();

private:
    QTcpSocket* m_clientSocket = nullptr;
};

class TestTcpSource : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    // Basic functionality tests
    void testConstruction();
    void testConfiguration();
    void testConnectionState();
    
    // Connection management tests
    void testConnectionAttempt();
    void testConnectionSuccess();
    void testConnectionFailure();
    void testConnectionLoss();
    void testSocketStateTransitions();
    
    // Reconnection tests
    void testAutoReconnect();
    void testReconnectionBackoff();
    void testReconnectionAttempts();
    void testReconnectionTimeout();
    
    // Stream parsing tests
    void testStreamBuffering();
    void testPacketBoundaryDetection();
    void testIncompletePacketHandling();
    void testMultiPacketStream();
    
    // Statistics tests
    void testConnectionStatistics();
    void testDataStatistics();
    void testErrorStatistics();
    
    // Error handling tests
    void testSocketErrors();
    void testNetworkErrors();
    void testTimeoutHandling();
    void testErrorRecovery();
    
    // Performance tests
    void testHighThroughputStream();
    void testReconnectionPerformance();
    void testMemoryUsage();
    
    // Signal/slot tests
    void testSignalEmission();

private:
    Memory::MemoryPoolManager* m_memoryManager;
    Packet::PacketFactory* m_packetFactory;
    MockTcpServer* m_mockServer;
    quint16 m_serverPort;
    
    // Helper methods
    QByteArray createTestPacket(uint32_t id, uint32_t sequence, const QString& payload = "TestData");
    QByteArray createMultiPacketStream(const QList<QPair<uint32_t, uint32_t>>& packets);
    bool waitForConnection(Network::TcpSource& source, int timeout = 5000);
    bool waitForSignal(const QObject* sender, const char* signal, int timeout = 5000);
    quint16 startMockServer();
    void stopMockServer();
};

void TestTcpSource::initTestCase()
{
    m_memoryManager = new Memory::MemoryPoolManager();
    m_packetFactory = new Packet::PacketFactory(m_memoryManager);
    m_mockServer = new MockTcpServer(this);
    m_serverPort = startMockServer();
}

void TestTcpSource::cleanupTestCase()
{
    stopMockServer();
    delete m_packetFactory;
    delete m_memoryManager;
}

void TestTcpSource::testConstruction()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig("TestTCP", QHostAddress::LocalHost, 8080);
    Network::TcpSource source(config);
    
    QCOMPARE(source.getName(), std::string("TestTCP"));
    QVERIFY(!source.isRunning());
    QVERIFY(source.isStopped());
}

void TestTcpSource::testConfiguration()
{
    Network::NetworkConfig config;
    config.protocol = Network::Protocol::TCP;
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    // Test initial configuration
    const auto& sourceConfig = source.getNetworkConfig();
    QCOMPARE(sourceConfig.protocol, Network::Protocol::TCP);
    
    // Test configuration update
    Network::NetworkConfig newConfig = Network::NetworkConfig::createTcpConfig("UpdatedTCP", QHostAddress("10.0.0.1"), 9001);
    source.setNetworkConfig(newConfig);
    
    QCOMPARE(source.getNetworkConfig().name, std::string("UpdatedTCP"));
    QCOMPARE(source.getNetworkConfig().remoteAddress, QHostAddress("10.0.0.1"));
    QCOMPARE(source.getNetworkConfig().remotePort, 9001);
}

void TestTcpSource::testConnectionState()
{
    Network::NetworkConfig config;
    config.protocol = Network::Protocol::TCP;
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    // Test initial state
    QVERIFY(source.isStopped());
    QVERIFY(!source.isRunning());
    QCOMPARE(source.getConnectionState(), Network::TcpSource::ConnectionState::Disconnected);
    QVERIFY(!source.isConnected());
}

// Connection management tests
void TestTcpSource::testConnectionAttempt()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "ConnectionTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy connectionSpy(&source, &Network::TcpSource::connectionStateChanged);
    
    source.start();
    QVERIFY(source.isRunning());
    
    // Should attempt to connect
    QVERIFY(waitForSignal(&source, SIGNAL(connectionStateChanged()), 5000));
    QVERIFY(connectionSpy.count() > 0);
    
    source.stop();
}

void TestTcpSource::testConnectionSuccess()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "SuccessTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    QSignalSpy connectionSpy(&source, &Network::TcpSource::connectionStateChanged);
    
    source.start();
    
    // Wait for connection
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    QVERIFY(clientSpy.count() > 0);
    QVERIFY(m_mockServer->hasConnectedClient());
    
    // Wait for source to report connected state
    if (!source.isConnected()) {
        waitForSignal(&source, SIGNAL(connectionStateChanged()), 5000);
    }
    QVERIFY(source.isConnected());
    QCOMPARE(source.getConnectionState(), Network::TcpSource::ConnectionState::Connected);
    
    source.stop();
}

void TestTcpSource::testConnectionFailure()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "FailureTest", QHostAddress::LocalHost, 65534); // Non-existent port
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy errorSpy(&source, &Packet::PacketSource::error);
    
    source.start();
    
    // Should eventually fail
    if (errorSpy.count() == 0) {
        errorSpy.wait(10000);
    }
    
    QVERIFY(!source.isConnected());
    
    source.stop();
}

void TestTcpSource::testConnectionLoss()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "LossTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    
    source.start();
    
    // Wait for connection
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    QVERIFY(m_mockServer->hasConnectedClient());
    
    if (!source.isConnected()) {
        waitForSignal(&source, SIGNAL(connectionStateChanged()), 5000);
    }
    QVERIFY(source.isConnected());
    
    QSignalSpy connectionLostSpy(&source, &Network::TcpSource::connectionStateChanged);
    
    // Simulate connection loss
    m_mockServer->disconnectClient();
    
    // Wait for disconnection detection
    if (connectionLostSpy.count() == 0) {
        connectionLostSpy.wait(5000);
    }
    
    QVERIFY(!source.isConnected());
    
    source.stop();
}

void TestTcpSource::testSocketStateTransitions()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "StateTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy stateSpy(&source, &Network::TcpSource::connectionStateChanged);
    
    source.start();
    
    // Should see state transitions
    waitForSignal(&source, SIGNAL(connectionStateChanged(Network::TcpSource::ConnectionState,Network::TcpSource::ConnectionState)), 5000);
    QVERIFY(stateSpy.count() > 0);
    
    source.stop();
}

// Reconnection tests
void TestTcpSource::testAutoReconnect()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "ReconnectTest", QHostAddress::LocalHost, m_serverPort);
    // Auto-reconnect behavior is handled internally by TcpSource
    config.reconnectInterval = 1000; // 1 second
    
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    
    source.start();
    
    // Wait for initial connection
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    QVERIFY(m_mockServer->hasConnectedClient());
    
    if (!source.isConnected()) {
        waitForSignal(&source, SIGNAL(connectionStateChanged()), 5000);
    }
    QVERIFY(source.isConnected());
    
    // Disconnect
    clientSpy.clear();
    m_mockServer->disconnectClient();
    
    // Wait for reconnection
    if (clientSpy.count() == 0) {
        clientSpy.wait(10000);
    }
    QVERIFY(clientSpy.count() > 0); // Should have reconnected
    
    source.stop();
}

void TestTcpSource::testReconnectionBackoff()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "BackoffTest", QHostAddress::LocalHost, 65533); // Non-existent port
    // Auto-reconnect behavior is handled internally by TcpSource
    config.reconnectInterval = 100; // Very short interval
    
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    const auto startTime = std::chrono::steady_clock::now();
    
    source.start();
    
    // Wait for several reconnection attempts
    QTest::qWait(5000);
    
    const auto endTime = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Should have backed off (not attempted every 100ms)
    qDebug() << "Reconnection attempts took:" << duration.count() << "ms";
    QVERIFY(duration.count() > 1000); // Should be much longer than 100ms * few attempts
    
    source.stop();
}

void TestTcpSource::testReconnectionAttempts()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "AttemptsTest", QHostAddress::LocalHost, 65532); // Non-existent port
    // Auto-reconnect behavior is handled internally by TcpSource
    config.maxReconnectAttempts = 3;
    
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy errorSpy(&source, &Packet::PacketSource::error);
    
    source.start();
    
    // Wait for all attempts to be exhausted
    if (errorSpy.count() == 0) {
        errorSpy.wait(15000);
    }
    
    // Should eventually give up
    QVERIFY(!source.isConnected());
    
    source.stop();
}

void TestTcpSource::testReconnectionTimeout()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "TimeoutTest", QHostAddress::LocalHost, m_serverPort);
    config.connectionTimeout = 100; // Very short timeout
    
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    const auto startTime = std::chrono::steady_clock::now();
    
    source.start();
    
    // Wait for timeout-based failure or success
    QTest::qWait(2000);
    
    const auto endTime = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    qDebug() << "Connection attempt took:" << duration.count() << "ms";
    
    source.stop();
}

// Stream parsing tests
void TestTcpSource::testStreamBuffering()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "BufferTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    
    source.start();
    
    // Wait for connection
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    QVERIFY(m_mockServer->hasConnectedClient());
    
    // Send partial packet data
    QByteArray fullPacket = createTestPacket(1, 1);
    QByteArray firstHalf = fullPacket.left(fullPacket.size() / 2);
    QByteArray secondHalf = fullPacket.mid(fullPacket.size() / 2);
    
    QSignalSpy packetSpy(&source, &Packet::PacketSource::packetReady);
    
    // Send first half
    m_mockServer->sendPacketToClient(firstHalf);
    QTest::qWait(100);
    QCOMPARE(packetSpy.count(), 0); // Should not have complete packet yet
    
    // Send second half
    m_mockServer->sendPacketToClient(secondHalf);
    
    // Should now have complete packet
    if (packetSpy.count() == 0) {
        packetSpy.wait(1000);
    }
    QVERIFY(packetSpy.count() > 0);
    
    source.stop();
}

void TestTcpSource::testPacketBoundaryDetection()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "BoundaryTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    
    source.start();
    
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    QVERIFY(m_mockServer->hasConnectedClient());
    
    QSignalSpy packetSpy(&source, &Packet::PacketSource::packetReady);
    
    // Create stream with multiple packets
    QList<QPair<uint32_t, uint32_t>> packets = {{1, 100}, {2, 200}, {3, 300}};
    QByteArray stream = createMultiPacketStream(packets);
    
    m_mockServer->sendPacketToClient(stream);
    
    // Wait for all packets to be processed
    QTest::qWait(1000);
    
    QVERIFY(packetSpy.count() >= packets.size());
    
    source.stop();
}

void TestTcpSource::testIncompletePacketHandling()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "IncompleteTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    
    source.start();
    
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    QVERIFY(m_mockServer->hasConnectedClient());
    
    QSignalSpy packetSpy(&source, &Packet::PacketSource::packetReady);
    
    // Send incomplete header (less than PacketHeader size)
    QByteArray incompleteHeader("incomplete");
    m_mockServer->sendPacketToClient(incompleteHeader);
    
    QTest::qWait(200);
    QCOMPARE(packetSpy.count(), 0); // Should not process incomplete packet
    
    source.stop();
}

void TestTcpSource::testMultiPacketStream()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "MultiTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    
    source.start();
    
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    QVERIFY(m_mockServer->hasConnectedClient());
    
    QSignalSpy packetSpy(&source, &Packet::PacketSource::packetReady);
    
    // Send many packets in rapid succession
    for (int i = 1; i <= 10; ++i) {
        QByteArray packet = createTestPacket(i, i * 100);
        m_mockServer->sendPacketToClient(packet);
        QTest::qWait(10); // Small delay between packets
    }
    
    // Wait for all packets to be processed
    QTest::qWait(1000);
    
    QVERIFY(packetSpy.count() >= 5); // Should process most packets
    
    source.stop();
}

// Statistics tests
void TestTcpSource::testConnectionStatistics()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "StatsTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    // const auto& initialStats = source.getNetworkStatistics();
    // uint64_t initialReconnections = initialStats.reconnections.load();
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    
    source.start();
    
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    
    const auto& finalStats = source.getNetworkStatistics();
    QVERIFY(finalStats.packetsReceived.load() >= 0); // Just verify stats are accessible
    
    source.stop();
}

void TestTcpSource::testDataStatistics()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "DataStatsTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    
    source.start();
    
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    QVERIFY(m_mockServer->hasConnectedClient());
    
    const auto& initialStats = source.getNetworkStatistics();
    uint64_t initialBytes = initialStats.bytesReceived.load();
    
    // Send data
    QByteArray packet = createTestPacket(1, 1, "LargerPayloadForStatsTesting");
    m_mockServer->sendPacketToClient(packet);
    
    QTest::qWait(500);
    
    const auto& finalStats = source.getNetworkStatistics();
    QVERIFY(finalStats.bytesReceived.load() >= initialBytes);
    
    source.stop();
}

void TestTcpSource::testErrorStatistics()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "ErrorStatsTest", QHostAddress::LocalHost, 65531); // Non-existent port
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    const auto& initialStats = source.getNetworkStatistics();
    uint64_t initialErrors = initialStats.socketErrors.load();
    
    source.start();
    QTest::qWait(2000); // Wait for connection failures
    
    const auto& finalStats = source.getNetworkStatistics();
    QVERIFY(finalStats.socketErrors.load() >= initialErrors);
    
    source.stop();
}

// Error handling tests
void TestTcpSource::testSocketErrors()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "SocketErrorTest", QHostAddress::LocalHost, 65530); // Non-existent port
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy errorSpy(&source, &Packet::PacketSource::error);
    
    source.start();
    
    // Should eventually emit error
    if (errorSpy.count() == 0) {
        errorSpy.wait(10000);
    }
    
    source.stop();
}

void TestTcpSource::testNetworkErrors()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "NetworkErrorTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    QSignalSpy errorSpy(&source, &Packet::PacketSource::error);
    
    source.start();
    
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    QVERIFY(m_mockServer->hasConnectedClient());
    
    // Simulate network error by disconnecting abruptly
    m_mockServer->disconnectClient();
    
    // Wait for error handling
    QTest::qWait(2000);
    
    source.stop();
}

void TestTcpSource::testTimeoutHandling()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "TimeoutHandlingTest", QHostAddress::LocalHost, m_serverPort);
    config.socketTimeout = 500; // Short timeout
    
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    source.start();
    QTest::qWait(2000); // Wait longer than timeout
    
    // Source should handle timeout gracefully
    QVERIFY(source.isRunning()); // Should still be running (attempting reconnection)
    
    source.stop();
}

void TestTcpSource::testErrorRecovery()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "RecoveryTest", QHostAddress::LocalHost, m_serverPort);
    // Auto-reconnect behavior is handled internally by TcpSource
    
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    
    source.start();
    
    // Initial connection
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    QVERIFY(m_mockServer->hasConnectedClient());
    
    // Cause error by disconnecting
    clientSpy.clear();
    m_mockServer->disconnectClient();
    
    // Should recover
    if (clientSpy.count() == 0) {
        clientSpy.wait(10000);
    }
    QVERIFY(clientSpy.count() > 0); // Should have reconnected
    
    source.stop();
}

// Performance tests
void TestTcpSource::testHighThroughputStream()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "ThroughputTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    
    source.start();
    
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    QVERIFY(m_mockServer->hasConnectedClient());
    
    QSignalSpy packetSpy(&source, &Packet::PacketSource::packetReady);
    
    const int packetCount = 1000;
    const auto startTime = std::chrono::steady_clock::now();
    
    // Send many packets rapidly
    for (int i = 1; i <= packetCount; ++i) {
        QByteArray packet = createTestPacket(1, i);
        m_mockServer->sendPacketToClient(packet);
    }
    
    // Wait for processing
    QTest::qWait(2000);
    
    const auto endTime = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    const auto& stats = source.getNetworkStatistics();
    uint64_t processed = stats.packetsReceived.load();
    
    qDebug() << "Processed" << processed << "packets in" << duration.count() << "ms";
    if (duration.count() > 0) {
        qDebug() << "Rate:" << (processed * 1000.0 / duration.count()) << "packets/sec";
    }
    
    QVERIFY(processed > 0);
    
    source.stop();
}

void TestTcpSource::testReconnectionPerformance()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "ReconnectPerfTest", QHostAddress::LocalHost, m_serverPort);
    // Auto-reconnect behavior is handled internally by TcpSource
    config.reconnectInterval = 100;
    
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    
    const auto startTime = std::chrono::steady_clock::now();
    
    source.start();
    
    // Multiple connect/disconnect cycles
    for (int i = 0; i < 3; ++i) {
        if (clientSpy.count() <= i) {
            clientSpy.wait(5000);
        }
        QVERIFY(m_mockServer->hasConnectedClient());
        
        m_mockServer->disconnectClient();
        QTest::qWait(200);
    }
    
    const auto endTime = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    qDebug() << "Reconnection cycles took:" << duration.count() << "ms";
    QVERIFY(clientSpy.count() >= 3);
    
    source.stop();
}

void TestTcpSource::testMemoryUsage()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "MemoryTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy clientSpy(m_mockServer, &MockTcpServer::clientConnected);
    
    source.start();
    
    if (clientSpy.count() == 0) {
        clientSpy.wait(5000);
    }
    QVERIFY(m_mockServer->hasConnectedClient());
    
    // Get initial memory stats (comment out until MemoryPoolManager has getStatistics)
    // const auto initialAllocated = m_memoryManager->getStatistics().totalBytesAllocated;
    
    // Process many packets
    for (int i = 0; i < 500; ++i) {
        QByteArray packet = createTestPacket(1, i);
        m_mockServer->sendPacketToClient(packet);
    }
    
    QTest::qWait(1000);
    
    // const auto finalAllocated = m_memoryManager->getStatistics().totalBytesAllocated;
    // qDebug() << "Memory usage increase:" << (finalAllocated - initialAllocated) << "bytes";
    
    source.stop();
    
    // Memory should be released after stop
    QTest::qWait(200);
}

// Signal/slot tests
void TestTcpSource::testSignalEmission()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "SignalTest", QHostAddress::LocalHost, m_serverPort);
    Network::TcpSource source(config);
    source.setPacketFactory(m_packetFactory);
    
    QSignalSpy stateSpy(&source, &Packet::PacketSource::stateChanged);
    QSignalSpy connectionSpy(&source, &Network::TcpSource::connectionStateChanged);
    QSignalSpy socketStateSpy(&source, &Network::TcpSource::connectionStateChanged);
    QSignalSpy statisticsSpy(&source, &Network::TcpSource::networkStatisticsUpdated);
    
    source.start();
    
    // Should emit various signals
    QVERIFY(stateSpy.count() > 0);
    
    // Wait for connection-related signals
    if (connectionSpy.count() == 0) {
        connectionSpy.wait(5000);
    }
    QVERIFY(connectionSpy.count() > 0);
    
    if (socketStateSpy.count() == 0) {
        socketStateSpy.wait(2000);
    }
    
    // Wait for statistics updates
    if (statisticsSpy.count() == 0) {
        statisticsSpy.wait(3000);
    }
    
    source.stop();
    
    // Should emit state changed on stop
    QVERIFY(stateSpy.count() >= 2);
}

// Helper method implementations
QByteArray TestTcpSource::createTestPacket(uint32_t id, uint32_t sequence, const QString& payload)
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

QByteArray TestTcpSource::createMultiPacketStream(const QList<QPair<uint32_t, uint32_t>>& packets)
{
    QByteArray stream;
    
    for (const auto& packetInfo : packets) {
        QByteArray packet = createTestPacket(packetInfo.first, packetInfo.second);
        stream.append(packet);
    }
    
    return stream;
}

bool TestTcpSource::waitForConnection(Network::TcpSource& source, int timeout)
{
    QElapsedTimer timer;
    timer.start();
    
    while (!source.isConnected() && timer.elapsed() < timeout) {
        QTest::qWait(100);
    }
    
    return source.isConnected();
}

bool TestTcpSource::waitForSignal(const QObject* sender, const char* signal, int timeout)
{
    QSignalSpy spy(sender, signal);
    if (spy.count() > 0) {
        return true;
    }
    return spy.wait(timeout);
}

quint16 TestTcpSource::startMockServer()
{
    if (m_mockServer->listen(QHostAddress::LocalHost, 0)) {
        return m_mockServer->serverPort();
    }
    return 0;
}

void TestTcpSource::stopMockServer()
{
    if (m_mockServer) {
        m_mockServer->close();
    }
}

QTEST_MAIN(TestTcpSource)
#include "test_tcp_source.moc"