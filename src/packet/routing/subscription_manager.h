#pragma once

#include "../core/packet.h"
#include "../../logging/logger.h"

#include <QtCore/QObject>
#include <QString>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <shared_mutex>
#include <atomic>

namespace Monitor {
namespace Packet {

/**
 * @brief Manages packet subscriptions for widgets and processors
 * 
 * This class maintains a registry of subscribers for each packet type,
 * enabling efficient multicast distribution of packets to interested
 * components. It uses thread-safe operations to support concurrent
 * access from multiple threads.
 */
class SubscriptionManager : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Subscriber identifier type
     */
    using SubscriberId = uint64_t;
    
    /**
     * @brief Packet delivery callback
     */
    using PacketCallback = std::function<void(PacketPtr packet)>;
    
    /**
     * @brief Subscription information
     */
    struct Subscription {
        SubscriberId id;
        std::string name;               ///< Human-readable subscriber name
        PacketId packetId;              ///< Subscribed packet ID
        PacketCallback callback;        ///< Delivery callback
        uint32_t priority;              ///< Delivery priority (lower = first, 0 = highest)
        bool enabled;                   ///< Enable/disable subscription
        std::chrono::steady_clock::time_point createdAt;
        
        // Statistics
        std::atomic<uint64_t> packetsReceived{0};
        std::atomic<uint64_t> packetsDropped{0};
        std::atomic<uint64_t> lastDeliveryTime{0};
        
        Subscription(SubscriberId subId, const std::string& subName, 
                    PacketId pktId, PacketCallback cb, uint32_t prio = 0)
            : id(subId), name(subName), packetId(pktId), callback(cb), 
              priority(prio), enabled(true), createdAt(std::chrono::steady_clock::now())
        {
        }
    };
    
    /**
     * @brief Subscription statistics
     */
    struct Statistics {
        std::atomic<uint64_t> totalSubscriptions{0};
        std::atomic<uint64_t> activeSubscriptions{0};
        std::atomic<uint64_t> packetsDistributed{0};
        std::atomic<uint64_t> deliveryFailures{0};
        std::atomic<uint64_t> averageDeliveryTimeNs{0};
        
        std::unordered_map<PacketId, uint64_t> subscriptionsPerPacketType;
        std::chrono::steady_clock::time_point startTime;
        
        Statistics() : startTime(std::chrono::steady_clock::now()) {}
        
        double getDistributionRate() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed == 0) return 0.0;
            return static_cast<double>(packetsDistributed.load()) / elapsed;
        }
        
        double getFailureRate() const {
            uint64_t total = packetsDistributed.load();
            if (total == 0) return 0.0;
            return static_cast<double>(deliveryFailures.load()) / total;
        }
    };

private:
    // Thread-safe containers
    std::unordered_map<SubscriberId, std::shared_ptr<Subscription>> m_subscriptions;
    std::unordered_map<PacketId, std::vector<std::shared_ptr<Subscription>>> m_packetSubscriptions;
    mutable std::shared_mutex m_subscriptionMutex;
    
    // Statistics
    Statistics m_stats;
    
    // Utilities
    Logging::Logger* m_logger;
    std::atomic<SubscriberId> m_nextSubscriberId{1};

public:
    explicit SubscriptionManager(QObject* parent = nullptr)
        : QObject(parent)
        , m_logger(Logging::Logger::instance())
    {
    }
    
    /**
     * @brief Subscribe to packets of specific type
     */
    SubscriberId subscribe(const std::string& subscriberName, PacketId packetId, 
                          PacketCallback callback, uint32_t priority = 0) {
        if (!callback) {
            m_logger->error("SubscriptionManager", 
                QString("Null callback for subscriber %1").arg(QString::fromStdString(subscriberName)));
            return 0;
        }
        
        SubscriberId id = m_nextSubscriberId++;
        auto subscription = std::make_shared<Subscription>(id, subscriberName, packetId, callback, priority);
        
        {
            std::unique_lock lock(m_subscriptionMutex);
            
            // Add to main subscription map
            m_subscriptions[id] = subscription;
            
            // Add to packet-specific map
            auto& packetSubs = m_packetSubscriptions[packetId];
            packetSubs.push_back(subscription);
            
            // Sort by priority (lower value = higher priority, 0 = highest)
            std::sort(packetSubs.begin(), packetSubs.end(),
                [](const std::shared_ptr<Subscription>& a, const std::shared_ptr<Subscription>& b) {
                    return a->priority < b->priority;
                });
            
            // Update statistics
            m_stats.totalSubscriptions++;
            m_stats.activeSubscriptions++;
            m_stats.subscriptionsPerPacketType[packetId]++;
        }
        
        m_logger->info("SubscriptionManager", 
            QString("Subscriber '%1' registered for packet ID %2 (priority %3)")
            .arg(QString::fromStdString(subscriberName)).arg(packetId).arg(priority));
        
        emit subscriptionAdded(id, QString::fromStdString(subscriberName), packetId);
        
        return id;
    }
    
    /**
     * @brief Unsubscribe from packets
     */
    bool unsubscribe(SubscriberId id) {
        std::unique_lock lock(m_subscriptionMutex);
        
        auto it = m_subscriptions.find(id);
        if (it == m_subscriptions.end()) {
            m_logger->warning("SubscriptionManager", 
                QString("Subscription ID %1 not found").arg(id));
            return false;
        }
        
        auto subscription = it->second;
        PacketId packetId = subscription->packetId;
        
        // Remove from main map
        m_subscriptions.erase(it);
        
        // Remove from packet-specific map
        auto& packetSubs = m_packetSubscriptions[packetId];
        auto subIt = std::remove_if(packetSubs.begin(), packetSubs.end(),
            [id](const std::shared_ptr<Subscription>& sub) {
                return sub->id == id;
            });
        packetSubs.erase(subIt, packetSubs.end());
        
        // Clean up empty packet subscription lists
        if (packetSubs.empty()) {
            m_packetSubscriptions.erase(packetId);
        }
        
        // Update statistics
        m_stats.activeSubscriptions--;
        m_stats.subscriptionsPerPacketType[packetId]--;
        
        m_logger->info("SubscriptionManager", 
            QString("Subscriber '%1' unsubscribed from packet ID %2")
            .arg(QString::fromStdString(subscription->name)).arg(packetId));
        
        emit subscriptionRemoved(id, QString::fromStdString(subscription->name), packetId);
        
        return true;
    }
    
    /**
     * @brief Enable/disable subscription
     */
    bool enableSubscription(SubscriberId id, bool enabled) {
        std::shared_lock lock(m_subscriptionMutex);
        
        auto it = m_subscriptions.find(id);
        if (it == m_subscriptions.end()) {
            return false;
        }
        
        it->second->enabled = enabled;
        
        m_logger->debug("SubscriptionManager", 
            QString("Subscription %1 %2").arg(id).arg(enabled ? "enabled" : "disabled"));
        
        return true;
    }
    
    /**
     * @brief Distribute packet to all subscribers
     */
    size_t distributePacket(PacketPtr packet) {
        if (!packet || !packet->isValid()) {
            m_stats.deliveryFailures++;
            return 0;
        }
        
        PacketId packetId = packet->id();
        auto startTime = std::chrono::high_resolution_clock::now();
        
        std::shared_lock lock(m_subscriptionMutex);
        
        auto it = m_packetSubscriptions.find(packetId);
        if (it == m_packetSubscriptions.end()) {
            // No subscribers for this packet type
            return 0;
        }
        
        const auto& subscribers = it->second;
        size_t deliveredCount = 0;
        
        for (const auto& subscription : subscribers) {
            if (!subscription->enabled) {
                continue;
            }
            
            try {
                subscription->callback(packet);
                subscription->packetsReceived++;
                deliveredCount++;
                
                auto now = std::chrono::high_resolution_clock::now();
                auto deliveryTime = std::chrono::duration_cast<std::chrono::nanoseconds>(now - startTime).count();
                subscription->lastDeliveryTime = static_cast<uint64_t>(deliveryTime);
                
            } catch (const std::exception& e) {
                m_logger->error("SubscriptionManager", 
                    QString("Exception in subscriber '%1': %2")
                    .arg(QString::fromStdString(subscription->name)).arg(QString::fromStdString(e.what())));
                subscription->packetsDropped++;
                m_stats.deliveryFailures++;
            } catch (...) {
                m_logger->error("SubscriptionManager", 
                    QString("Unknown exception in subscriber '%1'")
                    .arg(QString::fromStdString(subscription->name)));
                subscription->packetsDropped++;
                m_stats.deliveryFailures++;
            }
        }
        
        // Update global statistics
        m_stats.packetsDistributed++;
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto totalDeliveryTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();
        
        // Update average delivery time
        uint64_t currentAvg = m_stats.averageDeliveryTimeNs.load();
        uint64_t newAvg = (currentAvg + static_cast<uint64_t>(totalDeliveryTime)) / 2;
        m_stats.averageDeliveryTimeNs.store(newAvg);
        
        if (deliveredCount > 0) {
            m_logger->debug("SubscriptionManager", 
                QString("Distributed packet ID %1 to %2 subscribers in %3 ns")
                .arg(packetId).arg(deliveredCount).arg(totalDeliveryTime));
        }
        
        return deliveredCount;
    }
    
    /**
     * @brief Get subscription information
     */
    std::shared_ptr<Subscription> getSubscription(SubscriberId id) const {
        std::shared_lock lock(m_subscriptionMutex);
        auto it = m_subscriptions.find(id);
        return (it != m_subscriptions.end()) ? it->second : nullptr;
    }
    
    /**
     * @brief Get all subscribers for packet type
     */
    std::vector<std::shared_ptr<Subscription>> getSubscribersForPacket(PacketId packetId) const {
        std::shared_lock lock(m_subscriptionMutex);
        auto it = m_packetSubscriptions.find(packetId);
        return (it != m_packetSubscriptions.end()) ? it->second : std::vector<std::shared_ptr<Subscription>>();
    }
    
    /**
     * @brief Get all active subscriptions
     */
    std::vector<std::shared_ptr<Subscription>> getAllSubscriptions() const {
        std::shared_lock lock(m_subscriptionMutex);
        std::vector<std::shared_ptr<Subscription>> result;
        result.reserve(m_subscriptions.size());
        
        for (const auto& pair : m_subscriptions) {
            result.push_back(pair.second);
        }
        
        return result;
    }
    
    /**
     * @brief Get subscription count for packet type
     */
    size_t getSubscriberCount(PacketId packetId) const {
        std::shared_lock lock(m_subscriptionMutex);
        auto it = m_packetSubscriptions.find(packetId);
        return (it != m_packetSubscriptions.end()) ? it->second.size() : 0;
    }
    
    /**
     * @brief Get total subscription count
     */
    size_t getTotalSubscriberCount() const {
        std::shared_lock lock(m_subscriptionMutex);
        return m_subscriptions.size();
    }
    
    /**
     * @brief Get subscription statistics
     */
    const Statistics& getStatistics() const {
        return m_stats;
    }
    
    /**
     * @brief Clear all subscriptions
     */
    void clearAllSubscriptions() {
        std::unique_lock lock(m_subscriptionMutex);
        
        size_t count = m_subscriptions.size();
        m_subscriptions.clear();
        m_packetSubscriptions.clear();
        
        m_stats.activeSubscriptions = 0;
        m_stats.subscriptionsPerPacketType.clear();
        
        m_logger->info("SubscriptionManager", 
            QString("Cleared %1 subscriptions").arg(count));
        
        emit allSubscriptionsCleared();
    }

signals:
    /**
     * @brief Emitted when subscription is added
     */
    void subscriptionAdded(SubscriberId id, const QString& name, PacketId packetId);
    
    /**
     * @brief Emitted when subscription is removed
     */
    void subscriptionRemoved(SubscriberId id, const QString& name, PacketId packetId);
    
    /**
     * @brief Emitted when all subscriptions are cleared
     */
    void allSubscriptionsCleared();
    
    /**
     * @brief Emitted when statistics are updated
     */
    void statisticsUpdated(const Statistics& stats);
};

} // namespace Packet
} // namespace Monitor