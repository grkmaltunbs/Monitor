#pragma once

#include "../core/packet.h"
#include "../../logging/logger.h"

#include <QtCore/QObject>
#include <QString>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <atomic>

namespace Monitor {
namespace Packet {

/**
 * @brief Mock SubscriptionManager for Phase 6 widget testing
 * 
 * This is a lightweight mock implementation that provides just enough
 * functionality to allow Phase 6 widgets to register subscriptions
 * without the full Phase 4 packet processing system. It tracks
 * subscriptions but doesn't actually distribute packets.
 * 
 * This mock will be replaced with the full implementation when
 * Phase 4 packet processing is completed.
 */
class SubscriptionManagerMock : public QObject {
    Q_OBJECT
    
public:
    using SubscriberId = uint64_t;
    
    /**
     * @brief Simple subscription info for mock
     */
    struct MockSubscription {
        SubscriberId id;
        std::string name;
        PacketId packetId;
        bool enabled;
        
        MockSubscription() : id(0), packetId(0), enabled(true) {}
        
        MockSubscription(SubscriberId subId, const std::string& subName, PacketId pktId)
            : id(subId), name(subName), packetId(pktId), enabled(true)
        {
        }
    };

public:
    explicit SubscriptionManagerMock(QObject* parent = nullptr);
    ~SubscriptionManagerMock();

    /**
     * @brief Mock subscribe method (always succeeds for valid input)
     */
    SubscriberId subscribe(const std::string& subscriberName, PacketId packetId);

    /**
     * @brief Mock unsubscribe method
     */
    bool unsubscribe(SubscriberId id);

    /**
     * @brief Enable/disable subscription
     */
    bool enableSubscription(SubscriberId id, bool enabled);

    /**
     * @brief Check if there are subscribers for packet type
     */
    bool hasSubscribers(PacketId packetId) const;

    /**
     * @brief Get subscriber count for packet type
     */
    size_t getSubscriberCount(PacketId packetId) const;

    /**
     * @brief Get all subscribed packet IDs
     */
    std::vector<PacketId> getSubscribedPacketIds() const;

    /**
     * @brief Get subscription info
     */
    std::vector<MockSubscription> getSubscriptions() const;

    /**
     * @brief Clear all subscriptions
     */
    void clearSubscriptions();

signals:
    void subscriptionAdded(SubscriberId id, const QString& name, PacketId packetId);
    void subscriptionRemoved(SubscriberId id, const QString& name, PacketId packetId);

private:
    std::unordered_map<SubscriberId, MockSubscription> m_subscriptions;
    std::unordered_map<PacketId, std::vector<SubscriberId>> m_packetSubscriptions;
    
    Logging::Logger* m_logger;
    std::atomic<SubscriberId> m_nextSubscriberId{1};
};

} // namespace Packet
} // namespace Monitor