#pragma once

#include "message.h"
#include "../concurrent/spsc_ring_buffer.h"
#include "../concurrent/mpsc_ring_buffer.h"
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QAtomicInteger>
#include <memory>
#include <functional>
#include <future>

namespace Monitor {
namespace Messaging {

/**
 * @brief Channel configuration options
 */
struct ChannelConfig {
    size_t bufferSize = 1024;
    bool dropOnFull = false;
    bool blockingSend = false;
    int sendTimeoutMs = 100;
    bool enableStatistics = true;
    std::chrono::nanoseconds messageTimeToLive{0}; // 0 = no expiration
    
    ChannelConfig() = default;
    
    static ChannelConfig createHighThroughput() {
        ChannelConfig config;
        config.bufferSize = 4096;
        config.dropOnFull = true;
        config.blockingSend = false;
        return config;
    }
    
    static ChannelConfig createReliable() {
        ChannelConfig config;
        config.bufferSize = 1024;
        config.dropOnFull = false;
        config.blockingSend = true;
        config.sendTimeoutMs = 1000;
        return config;
    }
};

/**
 * @brief Channel statistics
 */
struct ChannelStatistics {
    size_t messagesSent = 0;
    size_t messagesReceived = 0;
    size_t messagesDropped = 0;
    size_t messagesExpired = 0;
    size_t currentQueueSize = 0;
    double averageLatencyUs = 0.0;
    qint64 peakLatencyUs = 0;
    double throughputMsgPerSec = 0.0;
    std::chrono::steady_clock::time_point lastResetTime;
    
    ChannelStatistics() : lastResetTime(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Base message channel interface
 */
class MessageChannel : public QObject
{
    Q_OBJECT

public:
    using MessageHandler = std::function<void(const MessagePtr&)>;
    using ErrorHandler = std::function<void(const QString&)>;
    
    explicit MessageChannel(const QString& channelName, const ChannelConfig& config = {}, QObject* parent = nullptr);
    virtual ~MessageChannel();
    
    // Configuration
    const QString& getName() const { return m_channelName; }
    const ChannelConfig& getConfig() const { return m_config; }
    void setConfig(const ChannelConfig& config);
    
    // Message operations
    virtual bool send(MessagePtr message) = 0;
    virtual MessagePtr receive(int timeoutMs = 0) = 0;
    virtual bool tryReceive(MessagePtr& message) = 0;
    
    // Async operations
    virtual std::future<bool> sendAsync(MessagePtr message);
    virtual std::future<MessagePtr> receiveAsync(int timeoutMs = 0);
    
    // State management
    virtual void open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual void flush() = 0;
    
    // Statistics
    ChannelStatistics getStatistics() const;
    void resetStatistics();
    
    // Event handlers
    void setMessageHandler(MessageHandler handler) { m_messageHandler = std::move(handler); }
    void setErrorHandler(ErrorHandler handler) { m_errorHandler = std::move(handler); }
    
    // Queue management
    virtual size_t getQueueSize() const = 0;
    virtual bool isEmpty() const = 0;
    virtual bool isFull() const = 0;
    virtual void clear() = 0;

signals:
    void messageReceived(const Message* message);
    void messageSent(const Message* message);
    void messageDropped(const Message* message);
    void queueFull();
    void errorOccurred(const QString& error);

protected:
    void updateSendStatistics(bool success, const MessagePtr& message);
    void updateReceiveStatistics(const MessagePtr& message);
    void emitError(const QString& error);
    bool isMessageExpired(const MessagePtr& message) const;
    
    QString m_channelName;
    ChannelConfig m_config;
    MessageHandler m_messageHandler;
    ErrorHandler m_errorHandler;
    
    mutable QMutex m_statsMutex;
    ChannelStatistics m_statistics;
    
    QAtomicInteger<bool> m_isOpen;
    
private:
    void updateThroughputStatistics();
    
    std::chrono::steady_clock::time_point m_lastThroughputUpdate;
    size_t m_messagesInLastPeriod = 0;
    
    static constexpr int THROUGHPUT_UPDATE_INTERVAL_MS = 1000;
};

/**
 * @brief Single Producer Single Consumer (SPSC) message channel
 * 
 * Optimized for scenarios with one sender and one receiver thread.
 * Provides the highest performance for point-to-point communication.
 */
class SPSCMessageChannel : public MessageChannel
{
    Q_OBJECT

public:
    explicit SPSCMessageChannel(const QString& channelName, const ChannelConfig& config = {}, QObject* parent = nullptr);
    ~SPSCMessageChannel() override;
    
    // MessageChannel interface
    bool send(MessagePtr message) override;
    MessagePtr receive(int timeoutMs = 0) override;
    bool tryReceive(MessagePtr& message) override;
    
    void open() override;
    void close() override;
    bool isOpen() const override { return m_isOpen.loadRelaxed(); }
    void flush() override;
    
    size_t getQueueSize() const override;
    bool isEmpty() const override;
    bool isFull() const override;
    void clear() override;

private:
    using MessageRingBuffer = Monitor::Concurrent::SPSCRingBuffer<MessagePtr>;
    std::unique_ptr<MessageRingBuffer> m_buffer;
    
    mutable QMutex m_receiveMutex;
    QWaitCondition m_receiveCondition;
};

/**
 * @brief Multi Producer Single Consumer (MPSC) message channel
 * 
 * Supports multiple sender threads with a single receiver thread.
 * Ideal for collecting messages from multiple sources.
 */
class MPSCMessageChannel : public MessageChannel
{
    Q_OBJECT

public:
    explicit MPSCMessageChannel(const QString& channelName, const ChannelConfig& config = {}, QObject* parent = nullptr);
    ~MPSCMessageChannel() override;
    
    // MessageChannel interface
    bool send(MessagePtr message) override;
    MessagePtr receive(int timeoutMs = 0) override;
    bool tryReceive(MessagePtr& message) override;
    
    void open() override;
    void close() override;
    bool isOpen() const override { return m_isOpen.loadRelaxed(); }
    void flush() override;
    
    size_t getQueueSize() const override;
    bool isEmpty() const override;
    bool isFull() const override;
    void clear() override;
    
    // Batch operations for better throughput
    size_t receiveBatch(std::vector<MessagePtr>& messages, size_t maxMessages, int timeoutMs = 0);

private:
    using MessageRingBuffer = Monitor::Concurrent::MPSCRingBuffer<MessagePtr>;
    std::unique_ptr<MessageRingBuffer> m_buffer;
    
    mutable QMutex m_receiveMutex;
    QWaitCondition m_receiveCondition;
};

/**
 * @brief Buffered message channel with overflow handling
 * 
 * Traditional thread-safe channel with configurable overflow behavior.
 * Supports multiple producers and multiple consumers.
 */
class BufferedMessageChannel : public MessageChannel
{
    Q_OBJECT

public:
    explicit BufferedMessageChannel(const QString& channelName, const ChannelConfig& config = {}, QObject* parent = nullptr);
    ~BufferedMessageChannel() override;
    
    // MessageChannel interface
    bool send(MessagePtr message) override;
    MessagePtr receive(int timeoutMs = 0) override;
    bool tryReceive(MessagePtr& message) override;
    
    void open() override;
    void close() override;
    bool isOpen() const override { return m_isOpen.loadRelaxed(); }
    void flush() override;
    
    size_t getQueueSize() const override;
    bool isEmpty() const override;
    bool isFull() const override;
    void clear() override;

private:
    std::queue<MessagePtr> m_messageQueue;
    mutable QMutex m_queueMutex;
    QWaitCondition m_sendCondition;
    QWaitCondition m_receiveCondition;
    size_t m_maxQueueSize;
};

/**
 * @brief Factory for creating message channels
 */
class MessageChannelFactory
{
public:
    enum class ChannelType {
        SPSC,           // Single producer, single consumer (fastest)
        MPSC,           // Multi producer, single consumer
        Buffered        // Multi producer, multi consumer (most flexible)
    };
    
    static std::unique_ptr<MessageChannel> createChannel(
        ChannelType type,
        const QString& name,
        const ChannelConfig& config = {},
        QObject* parent = nullptr);
    
    static std::unique_ptr<MessageChannel> createOptimalChannel(
        const QString& name,
        int expectedProducers,
        int expectedConsumers,
        const ChannelConfig& config = {},
        QObject* parent = nullptr);
};

} // namespace Messaging
} // namespace Monitor