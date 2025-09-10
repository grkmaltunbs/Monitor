#include "message.h"
#include <QtCore/QDataStream>
#include <QtCore/QIODevice>
#include <QtCore/QDebug>

namespace Monitor {
namespace Messaging {

// Static member definitions
std::atomic<MessageId> Message::s_nextMessageId{1};
std::atomic<size_t> Message::s_totalMessageCount{0};

Message::Message(const QString& messageType, MessagePriority priority)
    : m_id(getNextMessageId())
    , m_priority(priority)
{
    m_metadata.type = messageType;
    s_totalMessageCount.fetch_add(1, std::memory_order_relaxed);
}

MessageId Message::getNextMessageId()
{
    return s_nextMessageId.fetch_add(1, std::memory_order_relaxed);
}

size_t Message::getTotalMessageCount()
{
    return s_totalMessageCount.load(std::memory_order_relaxed);
}

void Message::resetMessageCounter()
{
    s_nextMessageId.store(1, std::memory_order_relaxed);
    s_totalMessageCount.store(0, std::memory_order_relaxed);
}

QByteArray Message::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    
    // Write message header
    stream << static_cast<quint64>(m_id);
    stream << static_cast<qint32>(m_priority);
    stream << m_metadata.type;
    stream << m_metadata.description;
    
    // Write routing information
    stream << static_cast<quint32>(m_route.senderId);
    stream << static_cast<quint32>(m_route.receiverId);
    stream << m_route.topic;
    stream << m_route.channel;
    
    // Write timing information
    auto createdTime = m_timing.createdAt.time_since_epoch().count();
    auto sentTime = m_timing.sentAt.time_since_epoch().count();
    auto receivedTime = m_timing.receivedAt.time_since_epoch().count();
    auto ttl = m_timing.timeToLive.count();
    
    stream << static_cast<qint64>(createdTime);
    stream << static_cast<qint64>(sentTime);
    stream << static_cast<qint64>(receivedTime);
    stream << static_cast<qint64>(ttl);
    
    // Write metadata attributes count
    stream << static_cast<quint32>(m_metadata.attributes.size());
    
    // Write each attribute (simplified - only string values supported)
    for (const auto& [key, value] : m_metadata.attributes) {
        stream << QString::fromStdString(key);
        
        // Try to extract string representation
        try {
            if (auto strPtr = std::any_cast<std::string>(&value)) {
                stream << QString::fromStdString(*strPtr);
            } else if (auto qstrPtr = std::any_cast<QString>(&value)) {
                stream << *qstrPtr;
            } else {
                stream << QString(""); // Fallback for unsupported types
            }
        } catch (const std::bad_any_cast&) {
            stream << QString(""); // Fallback
        }
    }
    
    return data;
}

bool Message::deserialize(const QByteArray& data)
{
    QDataStream stream(data);
    
    try {
        // Read message header
        quint64 id;
        qint32 priority;
        stream >> id >> priority >> m_metadata.type >> m_metadata.description;
        
        m_id = static_cast<MessageId>(id);
        m_priority = static_cast<MessagePriority>(priority);
        
        // Read routing information
        quint32 senderId, receiverId;
        stream >> senderId >> receiverId >> m_route.topic >> m_route.channel;
        
        m_route.senderId = static_cast<ThreadId>(senderId);
        m_route.receiverId = static_cast<ThreadId>(receiverId);
        
        // Read timing information
        qint64 createdTime, sentTime, receivedTime, ttl;
        stream >> createdTime >> sentTime >> receivedTime >> ttl;
        
        m_timing.createdAt = std::chrono::steady_clock::time_point(
            std::chrono::steady_clock::duration(createdTime));
        m_timing.sentAt = std::chrono::steady_clock::time_point(
            std::chrono::steady_clock::duration(sentTime));
        m_timing.receivedAt = std::chrono::steady_clock::time_point(
            std::chrono::steady_clock::duration(receivedTime));
        m_timing.timeToLive = std::chrono::nanoseconds(ttl);
        
        // Read metadata attributes
        quint32 attributeCount;
        stream >> attributeCount;
        
        m_metadata.attributes.clear();
        for (quint32 i = 0; i < attributeCount; ++i) {
            QString key, value;
            stream >> key >> value;
            m_metadata.attributes[key.toStdString()] = value.toStdString();
        }
        
        return stream.status() == QDataStream::Ok;
        
    } catch (const std::exception& e) {
        qWarning() << "Message deserialization error:" << e.what();
        return false;
    }
}

} // namespace Messaging
} // namespace Monitor