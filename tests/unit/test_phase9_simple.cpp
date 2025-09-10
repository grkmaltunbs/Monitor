#include <QCoreApplication>
#include <QTest>
#include <QSignalSpy>
#include <QHostAddress>

// Network sources
#include "../../src/network/config/network_config.h"
#include "../../src/network/sources/udp_source.h"
#include "../../src/network/sources/tcp_source.h"

// Offline sources  
#include "../../src/offline/sources/file_source.h"
#include "../../src/offline/sources/file_indexer.h"

// Core components for packet factory
#include "../../src/packet/core/packet_factory.h"
#include "../../src/memory/memory_pool.h"
#include "../../src/core/application.h"

using namespace Monitor;

/**
 * @brief Simple test to verify Phase 9 components compile and basic functionality works
 */
class TestPhase9Simple : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Network configuration tests
    void testNetworkConfigDefault();
    void testNetworkConfigUdpCreation(); 
    void testNetworkConfigTcpCreation();
    void testNetworkConfigValidation();
    void testNetworkConfigJsonSerialization();

    // UDP source tests
    void testUdpSourceCreation();
    void testUdpSourceConfiguration();
    void testUdpSourceState();

    // TCP source tests
    void testTcpSourceCreation();
    void testTcpSourceConfiguration();
    void testTcpSourceState();

    // File source tests
    void testFileSourceCreation();
    void testFileSourceConfiguration();
    void testFileSourceState();

    // File indexer tests
    void testFileIndexerCreation();
    void testFileIndexerState();

private:
    Memory::MemoryPoolManager* m_memoryManager;
    Packet::PacketFactory* m_packetFactory;
};

void TestPhase9Simple::initTestCase()
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

void TestPhase9Simple::cleanupTestCase()
{
    delete m_packetFactory;
}

void TestPhase9Simple::testNetworkConfigDefault()
{
    Network::NetworkConfig config;
    
    // Test default values
    QVERIFY(!config.name.empty());
    QCOMPARE(config.protocol, Network::Protocol::UDP);
    QCOMPARE(config.localAddress, QHostAddress::Any);
    QCOMPARE(config.localPort, 8080);
    QCOMPARE(config.remoteAddress, QHostAddress::LocalHost);
    QCOMPARE(config.remotePort, 8081);
    QVERIFY(!config.enableMulticast);
    QVERIFY(config.receiveBufferSize > 0);
    QVERIFY(config.socketTimeout > 0);
    QVERIFY(config.maxPacketSize > 0);
    QVERIFY(config.enableTimestamping);
}

void TestPhase9Simple::testNetworkConfigUdpCreation()
{
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "TestUDP", QHostAddress::LocalHost, 9000);
    
    QCOMPARE(config.name, std::string("TestUDP"));
    QCOMPARE(config.protocol, Network::Protocol::UDP);
    QCOMPARE(config.localAddress, QHostAddress::LocalHost);
    QCOMPARE(config.localPort, 9000);
}

void TestPhase9Simple::testNetworkConfigTcpCreation()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "TestTCP", QHostAddress("192.168.1.100"), 9001);
    
    QCOMPARE(config.name, std::string("TestTCP"));
    QCOMPARE(config.protocol, Network::Protocol::TCP);
    QCOMPARE(config.remoteAddress, QHostAddress("192.168.1.100"));
    QCOMPARE(config.remotePort, 9001);
}

void TestPhase9Simple::testNetworkConfigValidation()
{
    Network::NetworkConfig config;
    
    // Default config should be valid
    QVERIFY(config.isValid());
    
    // Test invalid configurations
    config.receiveBufferSize = 0;
    QVERIFY(!config.isValid());
    
    config.receiveBufferSize = 1024;
    config.maxPacketSize = 32;  // Too small
    QVERIFY(!config.isValid());
    
    // Reset to valid
    config.maxPacketSize = 1024;
    QVERIFY(config.isValid());
}

void TestPhase9Simple::testNetworkConfigJsonSerialization()
{
    Network::NetworkConfig originalConfig;
    originalConfig.name = "TestConfig";
    originalConfig.protocol = Network::Protocol::TCP;
    originalConfig.localPort = 12345;
    originalConfig.enableMulticast = true;
    
    // Serialize to JSON
    QJsonObject jsonObj = originalConfig.toJson();
    QVERIFY(!jsonObj.isEmpty());
    QCOMPARE(jsonObj["name"].toString(), QString("TestConfig"));
    QCOMPARE(jsonObj["protocol"].toString(), QString("TCP"));
    
    // Deserialize from JSON
    Network::NetworkConfig deserializedConfig;
    QVERIFY(deserializedConfig.fromJson(jsonObj));
    QCOMPARE(deserializedConfig.name, originalConfig.name);
    QCOMPARE(deserializedConfig.protocol, originalConfig.protocol);
    QCOMPARE(deserializedConfig.localPort, originalConfig.localPort);
    QCOMPARE(deserializedConfig.enableMulticast, originalConfig.enableMulticast);
}

void TestPhase9Simple::testUdpSourceCreation()
{
    Network::NetworkConfig config = Network::NetworkConfig::createUdpConfig(
        "TestUDP", QHostAddress::Any, 8080);
    
    Network::UdpSource udpSource(config);
    
    QCOMPARE(udpSource.getName(), std::string("TestUDP"));
    QCOMPARE(udpSource.getNetworkConfig().protocol, Network::Protocol::UDP);
    QVERIFY(!udpSource.isRunning());
    QVERIFY(udpSource.isStopped());
    QVERIFY(!udpSource.hasError());
}

void TestPhase9Simple::testUdpSourceConfiguration()
{
    Network::NetworkConfig config;
    Network::UdpSource udpSource(config);
    
    // Test initial configuration
    const auto& sourceConfig = udpSource.getNetworkConfig();
    QCOMPARE(sourceConfig.protocol, Network::Protocol::UDP);
    
    // Test configuration update
    Network::NetworkConfig newConfig = Network::NetworkConfig::createUdpConfig(
        "UpdatedUDP", QHostAddress::LocalHost, 9000);
    udpSource.setNetworkConfig(newConfig);
    
    QCOMPARE(udpSource.getNetworkConfig().name, std::string("UpdatedUDP"));
    QCOMPARE(udpSource.getNetworkConfig().localPort, 9000);
}

void TestPhase9Simple::testUdpSourceState()
{
    Network::NetworkConfig config;
    Network::UdpSource udpSource(config);
    udpSource.setPacketFactory(m_packetFactory);
    
    // Test initial state
    QVERIFY(udpSource.isStopped());
    QVERIFY(!udpSource.isRunning());
    QCOMPARE(udpSource.getSocketState(), QString("Not Initialized"));
    QVERIFY(!udpSource.isMulticastActive());
}

void TestPhase9Simple::testTcpSourceCreation()
{
    Network::NetworkConfig config = Network::NetworkConfig::createTcpConfig(
        "TestTCP", QHostAddress::LocalHost, 8080);
    
    Network::TcpSource tcpSource(config);
    
    QCOMPARE(tcpSource.getName(), std::string("TestTCP"));
    QCOMPARE(tcpSource.getNetworkConfig().protocol, Network::Protocol::TCP);
    QVERIFY(!tcpSource.isRunning());
    QVERIFY(tcpSource.isStopped());
    QVERIFY(!tcpSource.hasError());
}

void TestPhase9Simple::testTcpSourceConfiguration()
{
    Network::NetworkConfig config;
    config.protocol = Network::Protocol::TCP;
    Network::TcpSource tcpSource(config);
    
    // Test initial configuration
    const auto& sourceConfig = tcpSource.getNetworkConfig();
    QCOMPARE(sourceConfig.protocol, Network::Protocol::TCP);
    
    // Test configuration update
    Network::NetworkConfig newConfig = Network::NetworkConfig::createTcpConfig(
        "UpdatedTCP", QHostAddress("10.0.0.1"), 9001);
    tcpSource.setNetworkConfig(newConfig);
    
    QCOMPARE(tcpSource.getNetworkConfig().name, std::string("UpdatedTCP"));
    QCOMPARE(tcpSource.getNetworkConfig().remoteAddress, QHostAddress("10.0.0.1"));
    QCOMPARE(tcpSource.getNetworkConfig().remotePort, 9001);
}

void TestPhase9Simple::testTcpSourceState()
{
    Network::NetworkConfig config;
    config.protocol = Network::Protocol::TCP;
    Network::TcpSource tcpSource(config);
    tcpSource.setPacketFactory(m_packetFactory);
    
    // Test initial state  
    QVERIFY(tcpSource.isStopped());
    QVERIFY(!tcpSource.isRunning());
    QCOMPARE(tcpSource.getConnectionState(), Network::TcpSource::ConnectionState::Disconnected);
    QVERIFY(!tcpSource.isConnected());
    QCOMPARE(tcpSource.getSocketState(), QString("Not Initialized"));
}

void TestPhase9Simple::testFileSourceCreation()
{
    Offline::FileSourceConfig config;
    config.playbackSpeed = 2.0;
    config.loopPlayback = true;
    config.realTimePlayback = false;
    
    Offline::FileSource fileSource(config);
    
    QVERIFY(!fileSource.isFileLoaded());
    QCOMPARE(fileSource.getPlaybackState(), Offline::PlaybackState::Stopped);
    QCOMPARE(fileSource.getFileConfig().playbackSpeed, 2.0);
    QVERIFY(fileSource.getFileConfig().loopPlayback);
    QVERIFY(!fileSource.getFileConfig().realTimePlayback);
}

void TestPhase9Simple::testFileSourceConfiguration()
{
    Offline::FileSource fileSource;
    
    // Test initial configuration
    const auto& config = fileSource.getFileConfig();
    QCOMPARE(config.playbackSpeed, 1.0);
    QVERIFY(!config.loopPlayback);
    QVERIFY(config.realTimePlayback);
    
    // Test configuration update
    Offline::FileSourceConfig newConfig;
    newConfig.playbackSpeed = 0.5;
    newConfig.loopPlayback = true;
    newConfig.realTimePlayback = false;
    newConfig.bufferSize = 2000;
    
    fileSource.setFileConfig(newConfig);
    
    const auto& updatedConfig = fileSource.getFileConfig();
    QCOMPARE(updatedConfig.playbackSpeed, 0.5);
    QVERIFY(updatedConfig.loopPlayback);
    QVERIFY(!updatedConfig.realTimePlayback);
    QCOMPARE(updatedConfig.bufferSize, 2000);
}

void TestPhase9Simple::testFileSourceState()
{
    Offline::FileSource fileSource;
    fileSource.setPacketFactory(m_packetFactory);
    
    // Test initial state
    QVERIFY(!fileSource.isFileLoaded());
    QCOMPARE(fileSource.getPlaybackState(), Offline::PlaybackState::Stopped);
    QVERIFY(fileSource.isAtBeginningOfFile());
    QVERIFY(fileSource.isAtEndOfFile()); // True when no file loaded
    QCOMPARE(fileSource.getPlaybackProgress(), 0.0);
    
    const auto& stats = fileSource.getFileStatistics();
    QVERIFY(stats.filename.isEmpty());
    QCOMPARE(stats.totalPackets, 0ULL);
    QCOMPARE(stats.currentPacket, 0ULL);
}

void TestPhase9Simple::testFileIndexerCreation()
{
    Offline::FileIndexer indexer;
    
    // Test initial state
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::NotStarted);
    QVERIFY(!indexer.isIndexingComplete());
    QCOMPARE(indexer.getPacketCount(), 0ULL);
    
    const auto& stats = indexer.getStatistics();
    QVERIFY(stats.filename.isEmpty());
    QCOMPARE(stats.totalPackets, 0ULL);
    QCOMPARE(stats.indexedPackets, 0ULL);
}

void TestPhase9Simple::testFileIndexerState()
{
    Offline::FileIndexer indexer;
    
    // Test state management
    QCOMPARE(indexer.getStatus(), Offline::FileIndexer::IndexStatus::NotStarted);
    
    // Test search functions on empty index
    QCOMPARE(indexer.findPacketByPosition(0), -1);
    QCOMPARE(indexer.findPacketByTimestamp(123456), -1);
    QCOMPARE(indexer.findPacketBySequence(1), -1);
    QVERIFY(indexer.findPacketsByPacketId(0).empty());
    QCOMPARE(indexer.getPacketEntry(0), nullptr);
    
    // Test cache filename generation
    QString testFile = "/path/to/test.dat";
    QString cacheFile = Offline::FileIndexer::getCacheFilename(testFile);
    QVERIFY(!cacheFile.isEmpty());
    QVERIFY(cacheFile.endsWith(".idx"));
}

QTEST_MAIN(TestPhase9Simple)
#include "test_phase9_simple.moc"