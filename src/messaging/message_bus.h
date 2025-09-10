#pragma once

#include "message.h"
#include "message_channel.h"
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMutex>
#include <QtCore/QReadWriteLock>
#include <QtCore/QAtomicInteger>
#include <QtCore/QTimer>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <regex>

namespace Monitor {
namespace Messaging {

/**
 * @brief Subscription identifier
 */
using SubscriptionId = uint64_t;

/**
 * @brief Message filter function type
 */
using MessageFilter = std::function<bool(const MessagePtr&)>;

/**
 * @brief Topic-based message subscription
 */
struct Subscription {
    SubscriptionId id;
    ThreadId subscriberId;
    QString topic;
    QString pattern; // For pattern-based subscriptions
    MessageFilter filter;
    std::shared_ptr<MessageChannel> channel;
    int priority = 0; // Higher priority subscriptions get messages first
    bool isActive = true;
    std::chrono::steady_clock::time_point createdAt;
    size_t messagesReceived = 0;
    
    Subscription(SubscriptionId subId, ThreadId subscriber, const QString& t)
        : id(subId)
        , subscriberId(subscriber)
        , topic(t)
        , createdAt(std::chrono::steady_clock::now())
    {}
};

using SubscriptionPtr = std::shared_ptr<Subscription>;

/**
 * @brief Message bus statistics
 */
struct MessageBusStatistics {
    size_t totalMessagesPublished = 0;
    size_t totalMessagesDelivered = 0;
    size_t totalSubscriptions = 0;
    size_t activeSubscriptions = 0;
    size_t failedDeliveries = 0;
    std::unordered_map<std::string, size_t> messagesPerTopic;
    std::unordered_map<std::string, size_t> subscribersPerTopic;
    double averageDeliveryTimeUs = 0.0;
    qint64 peakDeliveryTimeUs = 0;
};

/**
 * @brief Message routing configuration
 */
struct RoutingConfig {
    bool enableTopicHierarchy = true; // Support hierarchical topics like "sensor/temperature/room1"
    bool enablePatternMatching = true; // Support wildcards like "sensor/*/room1"
    bool enableMessageFiltering = true;
    bool enablePriorityRouting = true;
    size_t maxSubscriptionsPerTopic = 1000;
    size_t maxTopics = 10000;
    std::chrono::nanoseconds deliveryTimeout{100000}; // 100us
    bool dropOnTimeout = false;
    bool enableStatistics = true;
};

/**
 * @brief High-performance topic-based message bus
 * 
 * Provides publish-subscribe messaging with:
 * - Topic-based routing
 * - Pattern-based subscriptions with wildcards
 * - Message filtering
 * - Priority-based delivery
 * - Multicast support
 * - Thread-safe operations
 */
class MessageBus : public QObject
{
    Q_OBJECT

public:
    explicit MessageBus(const QString& busName, const RoutingConfig& config = {}, QObject* parent = nullptr);
    ~MessageBus() override;
    
    // Configuration
    const QString& getName() const { return m_busName; }
    const RoutingConfig& getConfig() const { return m_config; }
    void setConfig(const RoutingConfig& config);
    
    // Topic management
    bool createTopic(const QString& topic);
    bool deleteTopic(const QString& topic);
    QStringList getTopics() const;
    bool topicExists(const QString& topic) const;
    
    // Publishing
    bool publish(const QString& topic, MessagePtr message);
    bool publish(MessagePtr message); // Uses message's route topic
    
    // Bulk publishing
    bool publishBatch(const QString& topic, std::vector<MessagePtr> messages);
    
    // Subscription management
    SubscriptionId subscribe(const QString& topic, std::shared_ptr<MessageChannel> channel, 
                           ThreadId subscriberId = 0, int priority = 0);
    SubscriptionId subscribePattern(const QString& pattern, std::shared_ptr<MessageChannel> channel,
                                  ThreadId subscriberId = 0, int priority = 0);
    SubscriptionId subscribeWithFilter(const QString& topic, std::shared_ptr<MessageChannel> channel,
                                     MessageFilter filter, ThreadId subscriberId = 0, int priority = 0);
    
    bool unsubscribe(SubscriptionId subscriptionId);
    bool unsubscribeAll(ThreadId subscriberId);
    bool unsubscribeFromTopic(const QString& topic, ThreadId subscriberId);
    
    // Subscription queries
    std::vector<SubscriptionPtr> getSubscriptions(const QString& topic) const;
    std::vector<SubscriptionPtr> getSubscriptions(ThreadId subscriberId) const;
    size_t getSubscriptionCount(const QString& topic) const;
    size_t getTotalSubscriptionCount() const;
    
    // Message routing control
    void pauseSubscription(SubscriptionId subscriptionId);
    void resumeSubscription(SubscriptionId subscriptionId);
    void setSubscriptionPriority(SubscriptionId subscriptionId, int priority);
    
    // Statistics
    MessageBusStatistics getStatistics() const;
    void resetStatistics();
    
    // Advanced features
    void setGlobalMessageFilter(MessageFilter filter);
    void removeGlobalMessageFilter();
    
    // Debugging and diagnostics
    void enableDebugLogging(bool enabled) { m_debugLogging = enabled; }
    QStringList getDebugInfo() const;
    
    // Thread safety
    void enableThreadSafety(bool enabled);
    bool isThreadSafe() const { return m_threadSafetyEnabled; }

signals:
    void messagePublished(const QString& topic, const Message* message);
    void messageDelivered(const QString& topic, SubscriptionId subscriptionId);
    void deliveryFailed(const QString& topic, SubscriptionId subscriptionId, const QString& error);
    void subscriptionCreated(SubscriptionId subscriptionId, const QString& topic);
    void subscriptionRemoved(SubscriptionId subscriptionId, const QString& topic);
    void topicCreated(const QString& topic);
    void topicDeleted(const QString& topic);

private slots:
    void handleChannelError(const QString& error);
    void cleanupInactiveSubscriptions();

private:
    struct TopicNode {
        QString name;
        std::vector<SubscriptionPtr> subscriptions;
        std::unordered_map<std::string, std::unique_ptr<TopicNode>> children;
        mutable QMutex mutex; // Per-topic locking for better concurrency
        
        void addSubscription(SubscriptionPtr subscription);
        void removeSubscription(SubscriptionId subscriptionId);
        std::vector<SubscriptionPtr> getMatchingSubscriptions() const;
    };
    
    // Core routing logic
    void deliverMessage(const QString& topic, MessagePtr message);
    void deliverToSubscription(SubscriptionPtr subscription, MessagePtr message, const QString& topic);
    
    // Topic hierarchy management  
    TopicNode* findOrCreateTopicNode(const QString& topic);
    TopicNode* findTopicNode(const QString& topic) const;
    void splitTopicPath(const QString& topic, QStringList& parts) const;
    
    // Pattern matching
    bool matchesPattern(const QString& topic, const QString& pattern) const;
    std::vector<SubscriptionPtr> findPatternSubscriptions(const QString& topic) const;
    
    // Subscription management
    SubscriptionPtr createSubscription(const QString& topic, std::shared_ptr<MessageChannel> channel,
                                     ThreadId subscriberId, int priority, const QString& pattern = {},
                                     MessageFilter filter = {});
    void removeSubscriptionFromTopic(const QString& topic, SubscriptionId subscriptionId);
    
    // Statistics tracking
    void updatePublishStatistics(const QString& topic, bool success);
    void updateDeliveryStatistics(const QString& topic, bool success, qint64 deliveryTimeUs);
    
    // Cleanup
    void performMaintenance();
    
    QString m_busName;
    RoutingConfig m_config;
    
    // Topic hierarchy (thread-safe with per-topic locks)
    std::unique_ptr<TopicNode> m_rootTopic;
    mutable QReadWriteLock m_topicTreeLock;
    
    // Subscription management
    std::unordered_map<SubscriptionId, SubscriptionPtr> m_subscriptions;
    std::unordered_map<std::string, std::vector<SubscriptionPtr>> m_patternSubscriptions;
    mutable QReadWriteLock m_subscriptionsLock;
    
    // Global message filter
    MessageFilter m_globalFilter;
    mutable QMutex m_globalFilterMutex;
    
    // Statistics
    mutable QMutex m_statisticsMutex;
    MessageBusStatistics m_statistics;
    
    // Thread safety
    bool m_threadSafetyEnabled = true;
    bool m_debugLogging = false;
    
    // ID generation
    QAtomicInteger<SubscriptionId> m_nextSubscriptionId;
    
    // Maintenance
    QTimer* m_maintenanceTimer;
    
    // Performance optimization
    static constexpr size_t TOPIC_CACHE_SIZE = 1000;
    mutable std::unordered_map<std::string, TopicNode*> m_topicCache;
    mutable QMutex m_topicCacheMutex;
    
    // Batch operation support
    void deliverMessageBatch(const QString& topic, const std::vector<MessagePtr>& messages);
    
    // Wildcard pattern support
    static bool isWildcardPattern(const QString& pattern);
    static std::regex convertWildcardToRegex(const QString& pattern);
    
    // Topic hierarchy separator
    static constexpr const char* TOPIC_SEPARATOR = "/";
    static constexpr const char* WILDCARD_SINGLE = "*";
    static constexpr const char* WILDCARD_MULTI = "**";
};

/**
 * @brief Global message bus registry
 * 
 * Manages multiple message buses and provides global access
 */
class MessageBusRegistry : public QObject
{
    Q_OBJECT

public:
    static MessageBusRegistry* instance();
    
    // Bus management
    std::shared_ptr<MessageBus> createBus(const QString& name, const RoutingConfig& config = {});
    std::shared_ptr<MessageBus> getBus(const QString& name);
    bool removeBus(const QString& name);
    QStringList getBusNames() const;
    
    // Global operations
    bool publishGlobal(const QString& busName, const QString& topic, MessagePtr message);
    SubscriptionId subscribeGlobal(const QString& busName, const QString& topic, 
                                 std::shared_ptr<MessageChannel> channel, ThreadId subscriberId = 0);
    
    // Statistics
    std::unordered_map<std::string, MessageBusStatistics> getAllBusStatistics() const;

private:
    explicit MessageBusRegistry(QObject* parent = nullptr);
    ~MessageBusRegistry() override;
    
    std::unordered_map<std::string, std::shared_ptr<MessageBus>> m_buses;
    mutable QReadWriteLock m_busesLock;
    
    static MessageBusRegistry* s_instance;
    static QMutex s_instanceMutex;
};

// Convenience functions for global bus access
std::shared_ptr<MessageBus> getGlobalBus(const QString& name = "default");
bool publishToGlobalBus(const QString& topic, MessagePtr message, const QString& busName = "default");
SubscriptionId subscribeToGlobalBus(const QString& topic, std::shared_ptr<MessageChannel> channel, 
                                   const QString& busName = "default", ThreadId subscriberId = 0);

} // namespace Messaging
} // namespace Monitor