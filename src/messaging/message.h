#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QVariant>
#include <QtCore/QMetaType>
#include <memory>
#include <chrono>
#include <cstdint>
#include <type_traits>
#include <any>
#include <optional>
#include <unordered_map>
#include <atomic>

namespace Monitor {
namespace Messaging {

using MessageId = uint64_t;
using ThreadId = uint32_t;
using Priority = int32_t;

/**
 * @brief Message priority levels
 */
enum class MessagePriority : Priority {
    Critical = 1000,
    High = 500,
    Normal = 0,
    Low = -500,
    Background = -1000
};

/**
 * @brief Message routing information
 */
struct MessageRoute {
    ThreadId senderId;
    ThreadId receiverId;
    QString topic;
    QString channel;
    
    MessageRoute() = default;
    MessageRoute(ThreadId sender, ThreadId receiver, const QString& t = {}, const QString& c = {})
        : senderId(sender), receiverId(receiver), topic(t), channel(c) {}
};

/**
 * @brief Message timing information
 */
struct MessageTiming {
    std::chrono::steady_clock::time_point createdAt;
    std::chrono::steady_clock::time_point sentAt;
    std::chrono::steady_clock::time_point receivedAt;
    std::chrono::nanoseconds timeToLive{0};
    
    MessageTiming() : createdAt(std::chrono::steady_clock::now()) {}
    
    void markSent() { sentAt = std::chrono::steady_clock::now(); }
    void markReceived() { receivedAt = std::chrono::steady_clock::now(); }
    
    std::chrono::nanoseconds getAge() const {
        return std::chrono::steady_clock::now() - createdAt;
    }
    
    std::chrono::nanoseconds getLatency() const {
        if (sentAt.time_since_epoch().count() > 0 && receivedAt.time_since_epoch().count() > 0) {
            return receivedAt - sentAt;
        }
        return std::chrono::nanoseconds{0};
    }
    
    bool isExpired() const {
        return timeToLive.count() > 0 && getAge() > timeToLive;
    }
};

/**
 * @brief Message metadata container
 */
struct MessageMetadata {
    QString type;
    QString description;
    std::unordered_map<std::string, std::any> attributes;
    
    template<typename T>
    void setAttribute(const std::string& key, T&& value) {
        attributes[key] = std::forward<T>(value);
    }
    
    template<typename T>
    std::optional<T> getAttribute(const std::string& key) const {
        auto it = attributes.find(key);
        if (it != attributes.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                // Type mismatch
            }
        }
        return std::nullopt;
    }
    
    bool hasAttribute(const std::string& key) const {
        return attributes.find(key) != attributes.end();
    }
};

/**
 * @brief Base message class with metadata and payload management
 */
class Message
{
public:
    /**
     * @brief Constructs a new message
     * @param messageType Type identifier for the message
     * @param priority Message priority
     */
    explicit Message(const QString& messageType = {}, MessagePriority priority = MessagePriority::Normal);
    
    /**
     * @brief Virtual destructor
     */
    virtual ~Message() = default;
    
    // Non-copyable but movable
    Message(const Message&) = delete;
    Message& operator=(const Message&) = delete;
    Message(Message&&) = default;
    Message& operator=(Message&&) = default;
    
    // Basic properties
    MessageId getId() const { return m_id; }
    QString getType() const { return m_metadata.type; }
    MessagePriority getPriority() const { return m_priority; }
    
    void setPriority(MessagePriority priority) { m_priority = priority; }
    void setDescription(const QString& desc) { m_metadata.description = desc; }
    QString getDescription() const { return m_metadata.description; }
    
    // Routing information
    const MessageRoute& getRoute() const { return m_route; }
    void setRoute(const MessageRoute& route) { m_route = route; }
    void setRoute(ThreadId sender, ThreadId receiver, const QString& topic = {}, const QString& channel = {}) {
        m_route = MessageRoute(sender, receiver, topic, channel);
    }
    
    // Timing information
    const MessageTiming& getTiming() const { return m_timing; }
    MessageTiming& getTiming() { return m_timing; }
    
    void setTimeToLive(std::chrono::nanoseconds ttl) { m_timing.timeToLive = ttl; }
    bool isExpired() const { return m_timing.isExpired(); }
    
    // Metadata management
    MessageMetadata& getMetadata() { return m_metadata; }
    const MessageMetadata& getMetadata() const { return m_metadata; }
    
    template<typename T>
    void setAttribute(const std::string& key, T&& value) {
        m_metadata.setAttribute(key, std::forward<T>(value));
    }
    
    template<typename T>
    std::optional<T> getAttribute(const std::string& key) const {
        return m_metadata.getAttribute<T>(key);
    }
    
    // Payload management (to be overridden by derived classes)
    virtual size_t getPayloadSize() const = 0;
    virtual bool hasPayload() const = 0;
    virtual void clearPayload() = 0;
    
    // Serialization support
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    
    // Statistics
    static MessageId getNextMessageId();
    static size_t getTotalMessageCount();
    static void resetMessageCounter();

protected:
    MessageId m_id;
    MessagePriority m_priority;
    MessageRoute m_route;
    MessageTiming m_timing;
    MessageMetadata m_metadata;
    
private:
    static std::atomic<MessageId> s_nextMessageId;
    static std::atomic<size_t> s_totalMessageCount;
};

/**
 * @brief Typed message with specific payload type
 */
template<typename PayloadType>
class TypedMessage : public Message
{
public:
    using PayloadT = PayloadType;
    
    /**
     * @brief Constructs a typed message
     * @param messageType Type identifier
     * @param priority Message priority
     */
    explicit TypedMessage(const QString& messageType = {}, MessagePriority priority = MessagePriority::Normal)
        : Message(messageType, priority)
        , m_hasPayload(false)
    {}
    
    /**
     * @brief Constructs a typed message with payload
     * @param payload The payload data
     * @param messageType Type identifier
     * @param priority Message priority
     */
    template<typename T>
    explicit TypedMessage(T&& payload, const QString& messageType = {}, MessagePriority priority = MessagePriority::Normal)
        : Message(messageType, priority)
        , m_payload(std::forward<T>(payload))
        , m_hasPayload(true)
    {}
    
    // Payload access
    const PayloadType& getPayload() const {
        if (!m_hasPayload) {
            throw std::runtime_error("Message has no payload");
        }
        return m_payload;
    }
    
    PayloadType& getPayload() {
        if (!m_hasPayload) {
            throw std::runtime_error("Message has no payload");
        }
        return m_payload;
    }
    
    template<typename T>
    void setPayload(T&& payload) {
        m_payload = std::forward<T>(payload);
        m_hasPayload = true;
    }
    
    // Move payload out of message
    PayloadType takePayload() {
        if (!m_hasPayload) {
            throw std::runtime_error("Message has no payload");
        }
        m_hasPayload = false;
        if constexpr (std::is_move_constructible_v<PayloadType>) {
            return std::move(m_payload);
        } else {
            return m_payload;
        }
    }
    
    // Base class overrides
    size_t getPayloadSize() const override {
        if constexpr (std::is_same_v<PayloadType, QByteArray>) {
            return m_hasPayload ? m_payload.size() : 0;
        } else if constexpr (std::is_same_v<PayloadType, QString>) {
            return m_hasPayload ? m_payload.size() * sizeof(QChar) : 0;
        } else {
            return m_hasPayload ? sizeof(PayloadType) : 0;
        }
    }
    
    bool hasPayload() const override {
        return m_hasPayload;
    }
    
    void clearPayload() override {
        if constexpr (std::is_default_constructible_v<PayloadType>) {
            m_payload = PayloadType{};
        }
        m_hasPayload = false;
    }

private:
    PayloadType m_payload;
    bool m_hasPayload;
};

// Common message types
using StringMessage = TypedMessage<QString>;
using BinaryMessage = TypedMessage<QByteArray>;
using VariantMessage = TypedMessage<QVariant>;
using VoidMessage = Message; // Message without payload

/**
 * @brief Zero-copy message for large payloads
 */
template<typename T>
class ZeroCopyMessage : public Message
{
public:
    explicit ZeroCopyMessage(const QString& messageType = {}, MessagePriority priority = MessagePriority::Normal)
        : Message(messageType, priority) {}
    
    template<typename... Args>
    explicit ZeroCopyMessage(std::unique_ptr<T> payload, const QString& messageType = {}, MessagePriority priority = MessagePriority::Normal)
        : Message(messageType, priority)
        , m_payload(std::move(payload))
    {}
    
    // Payload access
    const T* getPayload() const { return m_payload.get(); }
    T* getPayload() { return m_payload.get(); }
    
    void setPayload(std::unique_ptr<T> payload) { m_payload = std::move(payload); }
    std::unique_ptr<T> takePayload() { return std::move(m_payload); }
    
    // Base class overrides
    size_t getPayloadSize() const override {
        return m_payload ? sizeof(T) : 0;
    }
    
    bool hasPayload() const override {
        return static_cast<bool>(m_payload);
    }
    
    void clearPayload() override {
        m_payload.reset();
    }

private:
    std::unique_ptr<T> m_payload;
};

/**
 * @brief Shared message for broadcasting
 */
template<typename T>
class SharedMessage : public Message
{
public:
    explicit SharedMessage(const QString& messageType = {}, MessagePriority priority = MessagePriority::Normal)
        : Message(messageType, priority) {}
    
    explicit SharedMessage(std::shared_ptr<T> payload, const QString& messageType = {}, MessagePriority priority = MessagePriority::Normal)
        : Message(messageType, priority)
        , m_payload(std::move(payload))
    {}
    
    // Payload access (always const for shared access)
    const T* getPayload() const { return m_payload.get(); }
    std::shared_ptr<const T> getSharedPayload() const { return m_payload; }
    
    void setPayload(std::shared_ptr<T> payload) { m_payload = std::move(payload); }
    
    // Reference counting
    long getRefCount() const { return m_payload ? m_payload.use_count() : 0; }
    
    // Base class overrides
    size_t getPayloadSize() const override {
        return m_payload ? sizeof(T) : 0;
    }
    
    bool hasPayload() const override {
        return static_cast<bool>(m_payload);
    }
    
    void clearPayload() override {
        m_payload.reset();
    }

private:
    std::shared_ptr<T> m_payload;
};

// Convenience aliases
using MessagePtr = std::unique_ptr<Message>;
using SharedMessagePtr = std::shared_ptr<Message>;

} // namespace Messaging
} // namespace Monitor

// Make message types known to Qt's meta system
Q_DECLARE_METATYPE(Monitor::Messaging::MessagePtr)
Q_DECLARE_METATYPE(Monitor::Messaging::SharedMessagePtr)