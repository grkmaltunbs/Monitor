#include <QCoreApplication>
#include <QTest>
#include <QJsonDocument>
#include <QHostAddress>

#include "../../../src/network/config/network_config.h"

using namespace Monitor::Network;

/**
 * @brief Comprehensive test suite for NetworkConfig
 */
class TestNetworkConfig : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultConstruction();
    void testNamedConstructors();
    void testValidation();
    void testJsonSerialization();
    void testConnectionString();
    void testProtocolConversion();
    void testConfigurationCopying();

private:
    void verifyDefaultConfig(const NetworkConfig& config);
};

void TestNetworkConfig::testDefaultConstruction()
{
    NetworkConfig config;
    verifyDefaultConfig(config);
}

void TestNetworkConfig::testNamedConstructors()
{
    // Test UDP configuration
    auto udpConfig = NetworkConfig::createUdpConfig("UDPTest", QHostAddress::LocalHost, 9000);
    QCOMPARE(udpConfig.name, std::string("UDPTest"));
    QCOMPARE(udpConfig.protocol, Protocol::UDP);
    QCOMPARE(udpConfig.localAddress, QHostAddress::LocalHost);
    QCOMPARE(udpConfig.localPort, 9000);
    
    // Test TCP configuration
    auto tcpConfig = NetworkConfig::createTcpConfig("TCPTest", QHostAddress("192.168.1.100"), 8080);
    QCOMPARE(tcpConfig.name, std::string("TCPTest"));
    QCOMPARE(tcpConfig.protocol, Protocol::TCP);
    QCOMPARE(tcpConfig.remoteAddress, QHostAddress("192.168.1.100"));
    QCOMPARE(tcpConfig.remotePort, 8080);
    
    // Test multicast configuration
    auto multicastConfig = NetworkConfig::createMulticastConfig("MulticastTest", 
        QHostAddress("224.1.1.1"), 9001);
    QCOMPARE(multicastConfig.name, std::string("MulticastTest"));
    QCOMPARE(multicastConfig.protocol, Protocol::UDP);
    QVERIFY(multicastConfig.enableMulticast);
    QCOMPARE(multicastConfig.multicastGroup, QHostAddress("224.1.1.1"));
    QCOMPARE(multicastConfig.localPort, 9001);
    QCOMPARE(multicastConfig.remotePort, 9001);
}

void TestNetworkConfig::testValidation()
{
    NetworkConfig config;
    
    // Default config should be valid
    QVERIFY(config.isValid());
    
    // Test invalid receive buffer sizes
    config.receiveBufferSize = 512;  // Too small
    QVERIFY(!config.isValid());
    
    config.receiveBufferSize = 128 * 1024 * 1024;  // Too large (128MB)
    QVERIFY(!config.isValid());
    
    config.receiveBufferSize = 1048576;  // Valid 1MB
    QVERIFY(config.isValid());
    
    // Test invalid packet sizes
    config.maxPacketSize = 32;  // Too small
    QVERIFY(!config.isValid());
    
    config.maxPacketSize = 128 * 1024;  // Too large
    QVERIFY(!config.isValid());
    
    config.maxPacketSize = 1500;  // Valid Ethernet MTU
    QVERIFY(config.isValid());
    
    // Test invalid multicast settings
    config.enableMulticast = true;
    config.multicastGroup = QHostAddress("192.168.1.1");  // Not a multicast address
    QVERIFY(!config.isValid());
    
    config.multicastGroup = QHostAddress("224.0.0.1");  // Valid multicast
    QVERIFY(config.isValid());
    
    config.multicastGroup = QHostAddress("239.255.255.255");  // Valid multicast
    QVERIFY(config.isValid());
    
    config.multicastGroup = QHostAddress("240.0.0.1");  // Not multicast
    QVERIFY(!config.isValid());
}

void TestNetworkConfig::testJsonSerialization()
{
    // Create a config with specific values
    NetworkConfig originalConfig;
    originalConfig.name = "TestConfig";
    originalConfig.protocol = Protocol::TCP;
    originalConfig.localAddress = QHostAddress("10.0.0.1");
    originalConfig.localPort = 12345;
    originalConfig.remoteAddress = QHostAddress("10.0.0.100");
    originalConfig.remotePort = 54321;
    originalConfig.networkInterface = "eth0";
    originalConfig.enableMulticast = true;
    originalConfig.multicastGroup = QHostAddress("224.1.2.3");
    originalConfig.multicastTTL = 5;
    originalConfig.receiveBufferSize = 2097152;  // 2MB
    originalConfig.socketTimeout = 2000;
    originalConfig.maxPacketSize = 2048;
    originalConfig.enableTimestamping = false;
    originalConfig.typeOfService = 10;
    originalConfig.priority = 3;
    originalConfig.enableKeepAlive = false;
    originalConfig.keepAliveInterval = 45;
    originalConfig.connectionTimeout = 10000;
    originalConfig.maxReconnectAttempts = 5;
    originalConfig.reconnectInterval = 2000;
    
    // Serialize to JSON
    QJsonObject json = originalConfig.toJson();
    QVERIFY(!json.isEmpty());
    
    // Verify JSON contents
    QCOMPARE(json["name"].toString(), QString::fromStdString(originalConfig.name));
    QCOMPARE(json["protocol"].toString(), QString("TCP"));
    QCOMPARE(json["localAddress"].toString(), originalConfig.localAddress.toString());
    QCOMPARE(json["localPort"].toInt(), originalConfig.localPort);
    QCOMPARE(json["remoteAddress"].toString(), originalConfig.remoteAddress.toString());
    QCOMPARE(json["remotePort"].toInt(), originalConfig.remotePort);
    QCOMPARE(json["networkInterface"].toString(), originalConfig.networkInterface);
    
    // Check nested objects
    QVERIFY(json.contains("multicast"));
    QJsonObject multicast = json["multicast"].toObject();
    QCOMPARE(multicast["enabled"].toBool(), true);
    QCOMPARE(multicast["group"].toString(), originalConfig.multicastGroup.toString());
    QCOMPARE(multicast["ttl"].toInt(), 5);
    
    QVERIFY(json.contains("performance"));
    QJsonObject performance = json["performance"].toObject();
    QCOMPARE(performance["receiveBufferSize"].toInt(), 2097152);
    QCOMPARE(performance["socketTimeout"].toInt(), 2000);
    QCOMPARE(performance["maxPacketSize"].toInt(), 2048);
    QCOMPARE(performance["enableTimestamping"].toBool(), false);
    
    // Deserialize from JSON
    NetworkConfig deserializedConfig;
    QVERIFY(deserializedConfig.fromJson(json));
    
    // Verify all fields match
    QCOMPARE(deserializedConfig.name, originalConfig.name);
    QCOMPARE(deserializedConfig.protocol, originalConfig.protocol);
    QCOMPARE(deserializedConfig.localAddress, originalConfig.localAddress);
    QCOMPARE(deserializedConfig.localPort, originalConfig.localPort);
    QCOMPARE(deserializedConfig.remoteAddress, originalConfig.remoteAddress);
    QCOMPARE(deserializedConfig.remotePort, originalConfig.remotePort);
    QCOMPARE(deserializedConfig.networkInterface, originalConfig.networkInterface);
    QCOMPARE(deserializedConfig.enableMulticast, originalConfig.enableMulticast);
    QCOMPARE(deserializedConfig.multicastGroup, originalConfig.multicastGroup);
    QCOMPARE(deserializedConfig.multicastTTL, originalConfig.multicastTTL);
    QCOMPARE(deserializedConfig.receiveBufferSize, originalConfig.receiveBufferSize);
    QCOMPARE(deserializedConfig.socketTimeout, originalConfig.socketTimeout);
    QCOMPARE(deserializedConfig.maxPacketSize, originalConfig.maxPacketSize);
    QCOMPARE(deserializedConfig.enableTimestamping, originalConfig.enableTimestamping);
    QCOMPARE(deserializedConfig.typeOfService, originalConfig.typeOfService);
    QCOMPARE(deserializedConfig.priority, originalConfig.priority);
    QCOMPARE(deserializedConfig.enableKeepAlive, originalConfig.enableKeepAlive);
    QCOMPARE(deserializedConfig.keepAliveInterval, originalConfig.keepAliveInterval);
    QCOMPARE(deserializedConfig.connectionTimeout, originalConfig.connectionTimeout);
    QCOMPARE(deserializedConfig.maxReconnectAttempts, originalConfig.maxReconnectAttempts);
    QCOMPARE(deserializedConfig.reconnectInterval, originalConfig.reconnectInterval);
}

void TestNetworkConfig::testConnectionString()
{
    // Test UDP connection string
    NetworkConfig udpConfig;
    udpConfig.protocol = Protocol::UDP;
    udpConfig.remoteAddress = QHostAddress("192.168.1.100");
    udpConfig.remotePort = 8080;
    
    QString udpConnStr = udpConfig.getConnectionString();
    QCOMPARE(udpConnStr, QString("udp://192.168.1.100:8080"));
    
    // Test TCP connection string
    NetworkConfig tcpConfig;
    tcpConfig.protocol = Protocol::TCP;
    tcpConfig.remoteAddress = QHostAddress("10.0.0.50");
    tcpConfig.remotePort = 9090;
    
    QString tcpConnStr = tcpConfig.getConnectionString();
    QCOMPARE(tcpConnStr, QString("tcp://10.0.0.50:9090"));
}

void TestNetworkConfig::testProtocolConversion()
{
    // Test Protocol to string conversion
    QCOMPARE(protocolToString(Protocol::UDP), QString("UDP"));
    QCOMPARE(protocolToString(Protocol::TCP), QString("TCP"));
    
    // Test string to Protocol conversion
    QCOMPARE(stringToProtocol("UDP"), Protocol::UDP);
    QCOMPARE(stringToProtocol("udp"), Protocol::UDP);
    QCOMPARE(stringToProtocol("TCP"), Protocol::TCP);
    QCOMPARE(stringToProtocol("tcp"), Protocol::TCP);
    QCOMPARE(stringToProtocol("InvalidProtocol"), Protocol::UDP);  // Default
    
    // Test getProtocolString method
    NetworkConfig udpConfig;
    udpConfig.protocol = Protocol::UDP;
    QCOMPARE(udpConfig.getProtocolString(), QString("UDP"));
    
    NetworkConfig tcpConfig;
    tcpConfig.protocol = Protocol::TCP;
    QCOMPARE(tcpConfig.getProtocolString(), QString("TCP"));
}

void TestNetworkConfig::testConfigurationCopying()
{
    NetworkConfig original;
    original.name = "OriginalConfig";
    original.protocol = Protocol::TCP;
    original.localPort = 11111;
    original.remotePort = 22222;
    original.enableMulticast = true;
    original.receiveBufferSize = 4194304;
    
    // Test copy constructor
    NetworkConfig copied(original);
    QCOMPARE(copied.name, original.name);
    QCOMPARE(copied.protocol, original.protocol);
    QCOMPARE(copied.localPort, original.localPort);
    QCOMPARE(copied.remotePort, original.remotePort);
    QCOMPARE(copied.enableMulticast, original.enableMulticast);
    QCOMPARE(copied.receiveBufferSize, original.receiveBufferSize);
    
    // Test assignment operator
    NetworkConfig assigned;
    assigned = original;
    QCOMPARE(assigned.name, original.name);
    QCOMPARE(assigned.protocol, original.protocol);
    QCOMPARE(assigned.localPort, original.localPort);
    QCOMPARE(assigned.remotePort, original.remotePort);
    QCOMPARE(assigned.enableMulticast, original.enableMulticast);
    QCOMPARE(assigned.receiveBufferSize, original.receiveBufferSize);
    
    // Modify original and verify copies are independent
    original.name = "ModifiedOriginal";
    original.localPort = 99999;
    
    QCOMPARE(copied.name, std::string("OriginalConfig"));
    QCOMPARE(copied.localPort, 11111);
    QCOMPARE(assigned.name, std::string("OriginalConfig"));
    QCOMPARE(assigned.localPort, 11111);
}

void TestNetworkConfig::verifyDefaultConfig(const NetworkConfig& config)
{
    QCOMPARE(config.name, std::string("Default"));
    QCOMPARE(config.protocol, Protocol::UDP);
    QCOMPARE(config.localAddress, QHostAddress::Any);
    QCOMPARE(config.localPort, 8080);
    QCOMPARE(config.remoteAddress, QHostAddress::LocalHost);
    QCOMPARE(config.remotePort, 8081);
    QVERIFY(config.networkInterface.isEmpty());
    QVERIFY(!config.enableMulticast);
    QCOMPARE(config.multicastGroup, QHostAddress("224.0.0.1"));
    QCOMPARE(config.multicastTTL, 1);
    QCOMPARE(config.receiveBufferSize, 1048576);  // 1MB
    QCOMPARE(config.socketTimeout, 1000);
    QCOMPARE(config.maxPacketSize, 65536);  // 64KB
    QVERIFY(config.enableTimestamping);
    QCOMPARE(config.typeOfService, 0);
    QCOMPARE(config.priority, 0);
    QVERIFY(config.enableKeepAlive);
    QCOMPARE(config.keepAliveInterval, 30);
    QCOMPARE(config.connectionTimeout, 5000);
    QCOMPARE(config.maxReconnectAttempts, 3);
    QCOMPARE(config.reconnectInterval, 1000);
    
    QVERIFY(config.isValid());
}

QTEST_MAIN(TestNetworkConfig)
#include "test_network_config.moc"