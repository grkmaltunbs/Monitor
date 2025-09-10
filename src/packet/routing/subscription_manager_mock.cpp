#include "subscription_manager_mock.h"
#include "../../logging/logger.h"

#include <algorithm>

namespace Monitor {
namespace Packet {

SubscriptionManagerMock::SubscriptionManagerMock(QObject* parent)
    : QObject(parent)
    , m_logger(Logging::Logger::instance())
{
    m_logger->debug("SubscriptionManagerMock", "Mock SubscriptionManager created for Phase 6 testing");
}

SubscriptionManagerMock::~SubscriptionManagerMock() {
    m_logger->debug("SubscriptionManagerMock", "Mock SubscriptionManager destroyed");
}

SubscriptionManagerMock::SubscriberId SubscriptionManagerMock::subscribe(
    const std::string& subscriberName, PacketId packetId)
{
    if (subscriberName.empty() || packetId == 0) {
        m_logger->warning("SubscriptionManagerMock", 
            QString("Invalid subscription: name='%1', packetId=%2")
            .arg(QString::fromStdString(subscriberName)).arg(packetId));
        return 0;
    }
    
    SubscriberId id = m_nextSubscriberId++;
    
    // Create subscription
    MockSubscription subscription(id, subscriberName, packetId);
    m_subscriptions[id] = subscription;
    
    // Add to packet-specific tracking
    m_packetSubscriptions[packetId].push_back(id);
    
    m_logger->info("SubscriptionManagerMock", 
        QString("Mock subscriber '%1' registered for packet ID %2 (subscription ID %3)")
        .arg(QString::fromStdString(subscriberName)).arg(packetId).arg(id));
    
    emit subscriptionAdded(id, QString::fromStdString(subscriberName), packetId);
    
    return id;
}

bool SubscriptionManagerMock::unsubscribe(SubscriberId id) {
    auto it = m_subscriptions.find(id);
    if (it == m_subscriptions.end()) {
        m_logger->warning("SubscriptionManagerMock", 
            QString("Subscription ID %1 not found").arg(id));
        return false;
    }
    
    const MockSubscription& subscription = it->second;
    PacketId packetId = subscription.packetId;
    std::string name = subscription.name;
    
    // Remove from main map
    m_subscriptions.erase(it);
    
    // Remove from packet-specific map
    auto& packetSubs = m_packetSubscriptions[packetId];
    auto subIt = std::remove(packetSubs.begin(), packetSubs.end(), id);
    packetSubs.erase(subIt, packetSubs.end());
    
    // Clean up empty packet subscription lists
    if (packetSubs.empty()) {
        m_packetSubscriptions.erase(packetId);
    }
    
    m_logger->info("SubscriptionManagerMock", 
        QString("Mock subscriber '%1' unsubscribed from packet ID %2")
        .arg(QString::fromStdString(name)).arg(packetId));
    
    emit subscriptionRemoved(id, QString::fromStdString(name), packetId);
    
    return true;
}

bool SubscriptionManagerMock::enableSubscription(SubscriberId id, bool enabled) {
    auto it = m_subscriptions.find(id);
    if (it == m_subscriptions.end()) {
        return false;
    }
    
    it->second.enabled = enabled;
    
    m_logger->debug("SubscriptionManagerMock", 
        QString("Mock subscription %1 %2").arg(id).arg(enabled ? "enabled" : "disabled"));
    
    return true;
}

bool SubscriptionManagerMock::hasSubscribers(PacketId packetId) const {
    auto it = m_packetSubscriptions.find(packetId);
    return (it != m_packetSubscriptions.end() && !it->second.empty());
}

size_t SubscriptionManagerMock::getSubscriberCount(PacketId packetId) const {
    auto it = m_packetSubscriptions.find(packetId);
    return (it != m_packetSubscriptions.end()) ? it->second.size() : 0;
}

std::vector<PacketId> SubscriptionManagerMock::getSubscribedPacketIds() const {
    std::vector<PacketId> packetIds;
    packetIds.reserve(m_packetSubscriptions.size());
    
    for (const auto& pair : m_packetSubscriptions) {
        packetIds.push_back(pair.first);
    }
    
    return packetIds;
}

std::vector<SubscriptionManagerMock::MockSubscription> SubscriptionManagerMock::getSubscriptions() const {
    std::vector<MockSubscription> subscriptions;
    subscriptions.reserve(m_subscriptions.size());
    
    for (const auto& pair : m_subscriptions) {
        subscriptions.push_back(pair.second);
    }
    
    return subscriptions;
}

void SubscriptionManagerMock::clearSubscriptions() {
    m_subscriptions.clear();
    m_packetSubscriptions.clear();
    m_logger->debug("SubscriptionManagerMock", "Cleared all mock subscriptions");
}

} // namespace Packet
} // namespace Monitor

// Note: moc file is automatically handled by CMake