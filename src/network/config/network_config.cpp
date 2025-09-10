#include "network_config.h"
#include <QJsonDocument>
#include <QDebug>

namespace Monitor {
namespace Network {

QJsonObject NetworkConfig::toJson() const {
    QJsonObject json;
    
    // Basic settings
    json["name"] = QString::fromStdString(name);
    json["protocol"] = protocolToString(protocol);
    
    // Addressing
    json["localAddress"] = localAddress.toString();
    json["localPort"] = static_cast<int>(localPort);
    json["remoteAddress"] = remoteAddress.toString();
    json["remotePort"] = static_cast<int>(remotePort);
    json["networkInterface"] = networkInterface;
    
    // Multicast settings
    QJsonObject multicast;
    multicast["enabled"] = enableMulticast;
    multicast["group"] = multicastGroup.toString();
    multicast["ttl"] = multicastTTL;
    json["multicast"] = multicast;
    
    // Performance settings
    QJsonObject performance;
    performance["receiveBufferSize"] = receiveBufferSize;
    performance["socketTimeout"] = socketTimeout;
    performance["maxPacketSize"] = maxPacketSize;
    performance["enableTimestamping"] = enableTimestamping;
    json["performance"] = performance;
    
    // Quality of Service
    QJsonObject qos;
    qos["typeOfService"] = typeOfService;
    qos["priority"] = priority;
    json["qos"] = qos;
    
    // Connection settings (TCP)
    QJsonObject connection;
    connection["enableKeepAlive"] = enableKeepAlive;
    connection["keepAliveInterval"] = keepAliveInterval;
    connection["connectionTimeout"] = connectionTimeout;
    connection["maxReconnectAttempts"] = maxReconnectAttempts;
    connection["reconnectInterval"] = reconnectInterval;
    json["connection"] = connection;
    
    return json;
}

bool NetworkConfig::fromJson(const QJsonObject& json) {
    try {
        // Basic settings
        if (json.contains("name")) {
            name = json["name"].toString().toStdString();
        }
        
        if (json.contains("protocol")) {
            protocol = stringToProtocol(json["protocol"].toString());
        }
        
        // Addressing
        if (json.contains("localAddress")) {
            localAddress = QHostAddress(json["localAddress"].toString());
        }
        
        if (json.contains("localPort")) {
            localPort = static_cast<quint16>(json["localPort"].toInt());
        }
        
        if (json.contains("remoteAddress")) {
            remoteAddress = QHostAddress(json["remoteAddress"].toString());
        }
        
        if (json.contains("remotePort")) {
            remotePort = static_cast<quint16>(json["remotePort"].toInt());
        }
        
        if (json.contains("networkInterface")) {
            networkInterface = json["networkInterface"].toString();
        }
        
        // Multicast settings
        if (json.contains("multicast")) {
            const QJsonObject multicast = json["multicast"].toObject();
            enableMulticast = multicast["enabled"].toBool();
            multicastGroup = QHostAddress(multicast["group"].toString());
            multicastTTL = multicast["ttl"].toInt();
        }
        
        // Performance settings
        if (json.contains("performance")) {
            const QJsonObject performance = json["performance"].toObject();
            receiveBufferSize = performance["receiveBufferSize"].toInt();
            socketTimeout = performance["socketTimeout"].toInt();
            maxPacketSize = performance["maxPacketSize"].toInt();
            enableTimestamping = performance["enableTimestamping"].toBool();
        }
        
        // Quality of Service
        if (json.contains("qos")) {
            const QJsonObject qos = json["qos"].toObject();
            typeOfService = qos["typeOfService"].toInt();
            priority = qos["priority"].toInt();
        }
        
        // Connection settings (TCP)
        if (json.contains("connection")) {
            const QJsonObject connection = json["connection"].toObject();
            enableKeepAlive = connection["enableKeepAlive"].toBool();
            keepAliveInterval = connection["keepAliveInterval"].toInt();
            connectionTimeout = connection["connectionTimeout"].toInt();
            maxReconnectAttempts = connection["maxReconnectAttempts"].toInt();
            reconnectInterval = connection["reconnectInterval"].toInt();
        }
        
        return isValid();
    } catch (...) {
        qWarning() << "Failed to deserialize NetworkConfig from JSON";
        return false;
    }
}

} // namespace Network
} // namespace Monitor