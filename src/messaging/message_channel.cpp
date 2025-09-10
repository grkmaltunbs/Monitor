#include "message_channel.h"
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <future>
#include <thread>
#include <queue>

namespace Monitor {
namespace Messaging {

// MessageChannel base implementation

MessageChannel::MessageChannel(const QString& channelName, const ChannelConfig& config, QObject* parent)
    : QObject(parent)
    , m_channelName(channelName)
    , m_config(config)
    , m_isOpen(false)
    , m_lastThroughputUpdate(std::chrono::steady_clock::now())
{
}

MessageChannel::~MessageChannel()
{
    // Don't call virtual functions in destructor
    // Derived classes should handle cleanup in their own destructors
}

void MessageChannel::setConfig(const ChannelConfig& config)
{
    m_config = config;
}

std::future<bool> MessageChannel::sendAsync(MessagePtr message)
{
    return std::async(std::launch::async, [this, msg = std::move(message)]() mutable {
        return send(std::move(msg));
    });
}

std::future<MessagePtr> MessageChannel::receiveAsync(int timeoutMs)
{
    return std::async(std::launch::async, [this, timeoutMs]() {
        return receive(timeoutMs);
    });
}

ChannelStatistics MessageChannel::getStatistics() const
{
    QMutexLocker locker(&m_statsMutex);
    return m_statistics;
}

void MessageChannel::resetStatistics()
{
    QMutexLocker locker(&m_statsMutex);
    m_statistics = ChannelStatistics{};
}

void MessageChannel::updateSendStatistics(bool success, const MessagePtr& message)
{
    if (!m_config.enableStatistics) {
        return;
    }
    
    QMutexLocker locker(&m_statsMutex);
    
    if (success) {
        m_statistics.messagesSent++;
        message->getTiming().markSent();
    } else {
        m_statistics.messagesDropped++;
    }
    
    updateThroughputStatistics();
}

void MessageChannel::updateReceiveStatistics(const MessagePtr& message)
{
    if (!m_config.enableStatistics || !message) {
        return;
    }
    
    QMutexLocker locker(&m_statsMutex);
    
    m_statistics.messagesReceived++;
    message->getTiming().markReceived();
    
    // Update latency statistics
    auto latency = message->getTiming().getLatency();
    if (latency.count() > 0) {
        auto latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(latency).count();
        
        // Simple moving average
        if (m_statistics.messagesReceived == 1) {
            m_statistics.averageLatencyUs = latencyUs;
        } else {
            m_statistics.averageLatencyUs = 
                (m_statistics.averageLatencyUs * 0.9) + (latencyUs * 0.1);
        }
        
        m_statistics.peakLatencyUs = std::max(m_statistics.peakLatencyUs, latencyUs);
    }
    
    updateThroughputStatistics();
}

void MessageChannel::emitError(const QString& error)
{
    qWarning() << "MessageChannel" << m_channelName << "error:" << error;
    
    if (m_errorHandler) {
        m_errorHandler(error);
    }
    
    emit errorOccurred(error);
}

bool MessageChannel::isMessageExpired(const MessagePtr& message) const
{
    if (!message) {
        return true;
    }
    
    // Check message-specific TTL first
    if (message->isExpired()) {
        return true;
    }
    
    // Check channel-wide TTL
    if (m_config.messageTimeToLive.count() > 0) {
        auto age = message->getTiming().getAge();
        return age > m_config.messageTimeToLive;
    }
    
    return false;
}

void MessageChannel::updateThroughputStatistics()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastThroughputUpdate).count();
    
    if (elapsed >= THROUGHPUT_UPDATE_INTERVAL_MS) {
        m_statistics.throughputMsgPerSec = 
            (m_messagesInLastPeriod * 1000.0) / elapsed;
        m_messagesInLastPeriod = 0;
        m_lastThroughputUpdate = now;
    } else {
        m_messagesInLastPeriod++;
    }
}

// SPSCMessageChannel implementation

SPSCMessageChannel::SPSCMessageChannel(const QString& channelName, const ChannelConfig& config, QObject* parent)
    : MessageChannel(channelName, config, parent)
{
    try {
        m_buffer = std::make_unique<MessageRingBuffer>(config.bufferSize);
    } catch (const std::exception& e) {
        emitError(QString("Failed to create SPSC buffer: %1").arg(e.what()));
        throw;
    }
}

SPSCMessageChannel::~SPSCMessageChannel()
{
    close();
}

bool SPSCMessageChannel::send(MessagePtr message)
{
    if (!m_isOpen.loadRelaxed()) {
        emitError("Channel is not open");
        return false;
    }
    
    if (!message) {
        emitError("Cannot send null message");
        return false;
    }
    
    // Check if message is expired
    if (isMessageExpired(message)) {
        QMutexLocker locker(&m_statsMutex);
        m_statistics.messagesExpired++;
        return false;
    }
    
    bool success = false;
    
    if (m_config.blockingSend) {
        // Blocking send with timeout
        auto startTime = std::chrono::steady_clock::now();
        
        while (!success && m_isOpen.loadRelaxed()) {
            success = m_buffer->tryPush(std::move(message));
            
            if (!success) {
                if (m_config.sendTimeoutMs > 0) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - startTime).count();
                    if (elapsed >= m_config.sendTimeoutMs) {
                        break;
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    } else {
        // Non-blocking send
        success = m_buffer->tryPush(std::move(message));
        
        if (!success && m_config.dropOnFull) {
            success = true; // Consider it successful since we're dropping by design
        }
    }
    
    updateSendStatistics(success, message);
    
    if (success) {
        // Wake up any waiting receivers
        QMutexLocker locker(&m_receiveMutex);
        m_receiveCondition.wakeOne();
        
        emit messageSent(message.get());
    } else {
        if (isFull()) {
            emit queueFull();
        }
    }
    
    return success;
}

MessagePtr SPSCMessageChannel::receive(int timeoutMs)
{
    if (!m_isOpen.loadRelaxed()) {
        return nullptr;
    }
    
    MessagePtr message;
    
    if (tryReceive(message)) {
        return message;
    }
    
    // Wait for message
    QMutexLocker locker(&m_receiveMutex);
    
    if (timeoutMs <= 0) {
        // Wait indefinitely
        while (!tryReceive(message) && m_isOpen.loadRelaxed()) {
            m_receiveCondition.wait(&m_receiveMutex);
        }
    } else {
        // Wait with timeout
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        
        while (!tryReceive(message) && m_isOpen.loadRelaxed()) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                break;
            }
            
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            if (!m_receiveCondition.wait(&m_receiveMutex, static_cast<unsigned long>(remaining))) {
                break; // Timeout
            }
        }
    }
    
    return message;
}

bool SPSCMessageChannel::tryReceive(MessagePtr& message)
{
    if (!m_isOpen.loadRelaxed()) {
        return false;
    }
    
    if (m_buffer->tryPop(message)) {
        // Check if message is expired
        if (isMessageExpired(message)) {
            QMutexLocker locker(&m_statsMutex);
            m_statistics.messagesExpired++;
            message.reset();
            return false;
        }
        
        updateReceiveStatistics(message);
        
        if (m_messageHandler) {
            m_messageHandler(message);
        }
        
        emit messageReceived(message.get());
        return true;
    }
    
    return false;
}

void SPSCMessageChannel::open()
{
    m_isOpen.storeRelaxed(true);
}

void SPSCMessageChannel::close()
{
    m_isOpen.storeRelaxed(false);
    
    // Wake up any waiting threads
    QMutexLocker locker(&m_receiveMutex);
    m_receiveCondition.wakeAll();
}

void SPSCMessageChannel::flush()
{
    MessagePtr dummy;
    while (tryReceive(dummy)) {
        // Drain all messages
    }
}

size_t SPSCMessageChannel::getQueueSize() const
{
    return m_buffer ? m_buffer->size() : 0;
}

bool SPSCMessageChannel::isEmpty() const
{
    return m_buffer ? m_buffer->empty() : true;
}

bool SPSCMessageChannel::isFull() const
{
    return m_buffer ? m_buffer->full() : false;
}

void SPSCMessageChannel::clear()
{
    if (m_buffer) {
        m_buffer->clear();
    }
}

// MPSCMessageChannel implementation

MPSCMessageChannel::MPSCMessageChannel(const QString& channelName, const ChannelConfig& config, QObject* parent)
    : MessageChannel(channelName, config, parent)
{
    try {
        m_buffer = std::make_unique<MessageRingBuffer>(config.bufferSize);
    } catch (const std::exception& e) {
        emitError(QString("Failed to create MPSC buffer: %1").arg(e.what()));
        throw;
    }
}

MPSCMessageChannel::~MPSCMessageChannel()
{
    close();
}

bool MPSCMessageChannel::send(MessagePtr message)
{
    if (!m_isOpen.loadRelaxed()) {
        emitError("Channel is not open");
        return false;
    }
    
    if (!message) {
        emitError("Cannot send null message");
        return false;
    }
    
    if (isMessageExpired(message)) {
        QMutexLocker locker(&m_statsMutex);
        m_statistics.messagesExpired++;
        return false;
    }
    
    bool success = false;
    
    if (m_config.blockingSend) {
        success = m_buffer->timedPush(std::move(message), m_config.sendTimeoutMs);
    } else {
        success = m_buffer->tryPush(std::move(message));
        
        if (!success && m_config.dropOnFull) {
            success = true;
        }
    }
    
    updateSendStatistics(success, message);
    
    if (success) {
        QMutexLocker locker(&m_receiveMutex);
        m_receiveCondition.wakeOne();
        
        emit messageSent(message.get());
    } else {
        if (isFull()) {
            emit queueFull();
        }
    }
    
    return success;
}

MessagePtr MPSCMessageChannel::receive(int timeoutMs)
{
    if (!m_isOpen.loadRelaxed()) {
        return nullptr;
    }
    
    MessagePtr message;
    
    if (tryReceive(message)) {
        return message;
    }
    
    // Wait for message
    QMutexLocker locker(&m_receiveMutex);
    
    if (timeoutMs <= 0) {
        while (!tryReceive(message) && m_isOpen.loadRelaxed()) {
            m_receiveCondition.wait(&m_receiveMutex);
        }
    } else {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        
        while (!tryReceive(message) && m_isOpen.loadRelaxed()) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                break;
            }
            
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            if (!m_receiveCondition.wait(&m_receiveMutex, static_cast<unsigned long>(remaining))) {
                break;
            }
        }
    }
    
    return message;
}

bool MPSCMessageChannel::tryReceive(MessagePtr& message)
{
    if (!m_isOpen.loadRelaxed()) {
        return false;
    }
    
    if (m_buffer->tryPop(message)) {
        if (isMessageExpired(message)) {
            QMutexLocker locker(&m_statsMutex);
            m_statistics.messagesExpired++;
            message.reset();
            return false;
        }
        
        updateReceiveStatistics(message);
        
        if (m_messageHandler) {
            m_messageHandler(message);
        }
        
        emit messageReceived(message.get());
        return true;
    }
    
    return false;
}

size_t MPSCMessageChannel::receiveBatch(std::vector<MessagePtr>& messages, size_t maxMessages, int timeoutMs)
{
    if (!m_isOpen.loadRelaxed() || maxMessages == 0) {
        return 0;
    }
    
    messages.clear();
    messages.reserve(maxMessages);
    
    // Try to get messages immediately
    std::vector<MessagePtr> tempMessages(maxMessages);
    size_t received = m_buffer->tryPopBatch(tempMessages.data(), maxMessages);
    
    for (size_t i = 0; i < received; ++i) {
        if (!isMessageExpired(tempMessages[i])) {
            updateReceiveStatistics(tempMessages[i]);
            messages.push_back(std::move(tempMessages[i]));
        }
    }
    
    // If we didn't get enough and timeout is specified, wait for more
    if (messages.size() < maxMessages && timeoutMs > 0) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        
        QMutexLocker locker(&m_receiveMutex);
        
        while (messages.size() < maxMessages && m_isOpen.loadRelaxed()) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                break;
            }
            
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            if (!m_receiveCondition.wait(&m_receiveMutex, static_cast<unsigned long>(remaining))) {
                break;
            }
            
            // Try to get more messages
            MessagePtr msg;
            if (tryReceive(msg)) {
                messages.push_back(std::move(msg));
            }
        }
    }
    
    return messages.size();
}

void MPSCMessageChannel::open()
{
    m_isOpen.storeRelaxed(true);
}

void MPSCMessageChannel::close()
{
    m_isOpen.storeRelaxed(false);
    
    QMutexLocker locker(&m_receiveMutex);
    m_receiveCondition.wakeAll();
}

void MPSCMessageChannel::flush()
{
    MessagePtr dummy;
    while (tryReceive(dummy)) {
        // Drain all messages
    }
}

size_t MPSCMessageChannel::getQueueSize() const
{
    return m_buffer ? m_buffer->size() : 0;
}

bool MPSCMessageChannel::isEmpty() const
{
    return m_buffer ? m_buffer->empty() : true;
}

bool MPSCMessageChannel::isFull() const
{
    return m_buffer ? m_buffer->full() : false;
}

void MPSCMessageChannel::clear()
{
    if (m_buffer) {
        m_buffer->clear();
    }
}

// BufferedMessageChannel implementation

BufferedMessageChannel::BufferedMessageChannel(const QString& channelName, const ChannelConfig& config, QObject* parent)
    : MessageChannel(channelName, config, parent)
    , m_maxQueueSize(config.bufferSize)
{
}

BufferedMessageChannel::~BufferedMessageChannel()
{
    close();
}

bool BufferedMessageChannel::send(MessagePtr message)
{
    if (!m_isOpen.loadRelaxed()) {
        emitError("Channel is not open");
        return false;
    }
    
    if (!message) {
        emitError("Cannot send null message");
        return false;
    }
    
    if (isMessageExpired(message)) {
        QMutexLocker locker(&m_statsMutex);
        m_statistics.messagesExpired++;
        return false;
    }
    
    QMutexLocker locker(&m_queueMutex);
    
    // Check if queue is full
    if (m_messageQueue.size() >= m_maxQueueSize) {
        if (m_config.dropOnFull) {
            updateSendStatistics(true, message); // Consider it successful
            emit queueFull();
            return true;
        }
        
        if (m_config.blockingSend) {
            // Wait for space
            if (m_config.sendTimeoutMs > 0) {
                if (!m_sendCondition.wait(&m_queueMutex, m_config.sendTimeoutMs)) {
                    updateSendStatistics(false, message);
                    return false; // Timeout
                }
            } else {
                m_sendCondition.wait(&m_queueMutex);
            }
        } else {
            updateSendStatistics(false, message);
            emit queueFull();
            return false;
        }
    }
    
    m_messageQueue.push(std::move(message));
    updateSendStatistics(true, message);
    
    m_receiveCondition.wakeOne();
    emit messageSent(message.get());
    
    return true;
}

MessagePtr BufferedMessageChannel::receive(int timeoutMs)
{
    MessagePtr message;
    
    if (tryReceive(message)) {
        return message;
    }
    
    QMutexLocker locker(&m_queueMutex);
    
    if (timeoutMs <= 0) {
        while (m_messageQueue.empty() && m_isOpen.loadRelaxed()) {
            m_receiveCondition.wait(&m_queueMutex);
        }
    } else {
        if (m_messageQueue.empty() && m_isOpen.loadRelaxed()) {
            m_receiveCondition.wait(&m_queueMutex, timeoutMs);
        }
    }
    
    if (!m_messageQueue.empty()) {
        message = std::move(m_messageQueue.front());
        m_messageQueue.pop();
        
        m_sendCondition.wakeOne();
        
        if (!isMessageExpired(message)) {
            updateReceiveStatistics(message);
            
            if (m_messageHandler) {
                m_messageHandler(message);
            }
            
            emit messageReceived(message.get());
        } else {
            QMutexLocker statsLocker(&m_statsMutex);
            m_statistics.messagesExpired++;
            message.reset();
        }
    }
    
    return message;
}

bool BufferedMessageChannel::tryReceive(MessagePtr& message)
{
    if (!m_isOpen.loadRelaxed()) {
        return false;
    }
    
    QMutexLocker locker(&m_queueMutex);
    
    if (m_messageQueue.empty()) {
        return false;
    }
    
    message = std::move(m_messageQueue.front());
    m_messageQueue.pop();
    
    m_sendCondition.wakeOne();
    
    if (isMessageExpired(message)) {
        QMutexLocker statsLocker(&m_statsMutex);
        m_statistics.messagesExpired++;
        message.reset();
        return false;
    }
    
    updateReceiveStatistics(message);
    
    if (m_messageHandler) {
        m_messageHandler(message);
    }
    
    emit messageReceived(message.get());
    return true;
}

void BufferedMessageChannel::open()
{
    m_isOpen.storeRelaxed(true);
}

void BufferedMessageChannel::close()
{
    m_isOpen.storeRelaxed(false);
    
    QMutexLocker locker(&m_queueMutex);
    m_sendCondition.wakeAll();
    m_receiveCondition.wakeAll();
}

void BufferedMessageChannel::flush()
{
    QMutexLocker locker(&m_queueMutex);
    while (!m_messageQueue.empty()) {
        m_messageQueue.pop();
    }
    m_sendCondition.wakeAll();
}

size_t BufferedMessageChannel::getQueueSize() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_messageQueue.size();
}

bool BufferedMessageChannel::isEmpty() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_messageQueue.empty();
}

bool BufferedMessageChannel::isFull() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_messageQueue.size() >= m_maxQueueSize;
}

void BufferedMessageChannel::clear()
{
    flush();
}

// MessageChannelFactory implementation

std::unique_ptr<MessageChannel> MessageChannelFactory::createChannel(
    ChannelType type,
    const QString& name,
    const ChannelConfig& config,
    QObject* parent)
{
    switch (type) {
        case ChannelType::SPSC:
            return std::make_unique<SPSCMessageChannel>(name, config, parent);
        case ChannelType::MPSC:
            return std::make_unique<MPSCMessageChannel>(name, config, parent);
        case ChannelType::Buffered:
            return std::make_unique<BufferedMessageChannel>(name, config, parent);
    }
    
    return nullptr;
}

std::unique_ptr<MessageChannel> MessageChannelFactory::createOptimalChannel(
    const QString& name,
    int expectedProducers,
    int expectedConsumers,
    const ChannelConfig& config,
    QObject* parent)
{
    ChannelType optimalType;
    
    if (expectedProducers == 1 && expectedConsumers == 1) {
        optimalType = ChannelType::SPSC;
    } else if (expectedProducers > 1 && expectedConsumers == 1) {
        optimalType = ChannelType::MPSC;
    } else {
        optimalType = ChannelType::Buffered;
    }
    
    return createChannel(optimalType, name, config, parent);
}

} // namespace Messaging
} // namespace Monitor

