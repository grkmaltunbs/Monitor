#include "event_dispatcher.h"
#include <QtCore/QLoggingCategory>
#include <QtCore/QMetaMethod>
#include <QtCore/QDebug>
#include <algorithm>

Q_LOGGING_CATEGORY(eventDispatcher, "Monitor.Events.Dispatcher")

namespace Monitor {
namespace Events {

Event::Event(const QString& type, Priority priority, QObject* parent)
    : QObject(parent)
    , m_type(type)
    , m_priority(priority)
    , m_timestamp(QDateTime::currentDateTime())
    , m_consumed(false)
{
}

void Event::setData(const QString& key, const QVariant& value)
{
    m_data[key] = value;
}

QVariant Event::getData(const QString& key, const QVariant& defaultValue) const
{
    return m_data.value(key, defaultValue);
}

QString Event::toString() const
{
    return QString("Event{type='%1', priority=%2, timestamp='%3', data=%4}")
           .arg(m_type)
           .arg(static_cast<int>(m_priority))
           .arg(m_timestamp.toString(Qt::ISODate))
           .arg(m_data.size());
}

ApplicationEvent::ApplicationEvent(Type eventType, Priority priority, QObject* parent)
    : Event(QString("Application.%1").arg(QMetaEnum::fromType<Type>().valueToKey(eventType)), priority, parent)
    , m_eventType(eventType)
{
}

MemoryEvent::MemoryEvent(Type eventType, Priority priority, QObject* parent)
    : Event(QString("Memory.%1").arg(QMetaEnum::fromType<Type>().valueToKey(eventType)), priority, parent)
    , m_eventType(eventType)
{
}

PerformanceEvent::PerformanceEvent(Type eventType, Priority priority, QObject* parent)
    : Event(QString("Performance.%1").arg(QMetaEnum::fromType<Type>().valueToKey(eventType)), priority, parent)
    , m_eventType(eventType)
{
}

EventDispatcher::EventDispatcher(QObject* parent)
    : QObject(parent)
    , m_delayedEventTimer(new QTimer(this))
    , m_isRunning(false)
    , m_isPaused(false)
    , m_eventsProcessed(0)
    , m_maxQueueSize(DEFAULT_MAX_QUEUE_SIZE)
    , m_processingTimeoutMs(DEFAULT_PROCESSING_TIMEOUT_MS)
    , m_totalProcessingTime(std::chrono::nanoseconds::zero())
{
    m_delayedEventTimer->setSingleShot(false);
    m_delayedEventTimer->setInterval(DELAYED_EVENT_TIMER_INTERVAL_MS);
    connect(m_delayedEventTimer, &QTimer::timeout, this, &EventDispatcher::processDelayedEvents);
    
    qCInfo(eventDispatcher) << "Event dispatcher created";
}

EventDispatcher::~EventDispatcher()
{
    stop();
    qCInfo(eventDispatcher) << "Event dispatcher destroyed";
}

void EventDispatcher::subscribe(const QString& eventType, EventHandler handler)
{
    QMutexLocker locker(&m_handlersMutex);
    m_handlers[eventType].emplace_back(std::move(handler));
    qCDebug(eventDispatcher) << "Subscribed function handler to event type:" << eventType;
}

void EventDispatcher::subscribe(const QString& eventType, QObject* receiver, const char* slot)
{
    if (!receiver || !slot) {
        qCWarning(eventDispatcher) << "Invalid receiver or slot for event type:" << eventType;
        return;
    }
    
    const QMetaObject* metaObject = receiver->metaObject();
    
    // Handle SLOT() macro which prepends a "1" to the signature
    const char* normalizedSlot = slot;
    if (slot && slot[0] == '1') {
        normalizedSlot = slot + 1; // Skip the "1" prefix from SLOT() macro
    }
    
    QMetaMethod method = metaObject->method(metaObject->indexOfSlot(normalizedSlot));
    
    if (!method.isValid()) {
        qCWarning(eventDispatcher) << "Invalid slot" << slot << "for receiver" << receiver << "event type:" << eventType;
        return;
    }
    
    QMutexLocker locker(&m_handlersMutex);
    m_handlers[eventType].emplace_back(receiver, method);
    qCDebug(eventDispatcher) << "Subscribed QObject slot to event type:" << eventType << "receiver:" << receiver;
}

void EventDispatcher::unsubscribe(const QString& eventType, QObject* receiver)
{
    QMutexLocker locker(&m_handlersMutex);
    
    auto it = m_handlers.find(eventType);
    if (it != m_handlers.end()) {
        auto& handlers = it->second;
        handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
                                    [receiver](const HandlerInfo& handler) {
                                        return handler.isQObjectReceiver && handler.receiver == receiver;
                                    }), handlers.end());
        
        if (handlers.empty()) {
            m_handlers.erase(it);
        }
        qCDebug(eventDispatcher) << "Unsubscribed receiver" << receiver << "from event type:" << eventType;
    }
}

void EventDispatcher::unsubscribeAll(QObject* receiver)
{
    QMutexLocker locker(&m_handlersMutex);
    
    for (auto it = m_handlers.begin(); it != m_handlers.end();) {
        auto& handlers = it->second;
        handlers.erase(std::remove_if(handlers.begin(), handlers.end(),
                                    [receiver](const HandlerInfo& handler) {
                                        return handler.isQObjectReceiver && handler.receiver == receiver;
                                    }), handlers.end());
        
        if (handlers.empty()) {
            it = m_handlers.erase(it);
        } else {
            ++it;
        }
    }
    qCDebug(eventDispatcher) << "Unsubscribed receiver" << receiver << "from all event types";
}

void EventDispatcher::setEventFilter(const QString& eventType, EventFilter filter)
{
    QMutexLocker locker(&m_handlersMutex);
    m_filters[eventType] = std::move(filter);
    qCDebug(eventDispatcher) << "Set event filter for type:" << eventType;
}

void EventDispatcher::removeEventFilter(const QString& eventType)
{
    QMutexLocker locker(&m_handlersMutex);
    m_filters.erase(eventType);
    qCDebug(eventDispatcher) << "Removed event filter for type:" << eventType;
}

void EventDispatcher::post(EventPtr event)
{
    if (!event) {
        qCWarning(eventDispatcher) << "Attempting to post null event";
        return;
    }
    
    if (!m_isRunning.loadRelaxed()) {
        qCWarning(eventDispatcher) << "Event dispatcher not running, dropping event:" << event->type();
        return;
    }
    
    QMutexLocker locker(&m_queueMutex);
    auto& queue = m_eventQueues[event->type()];
    
    if (queue.size() >= m_maxQueueSize) {
        emit queueOverflow(event->type(), queue.size());
        qCWarning(eventDispatcher) << "Queue overflow for event type:" << event->type();
        return;
    }
    
    queue.emplace(event);
    qCDebug(eventDispatcher) << "Posted event:" << event->type() << "queue size:" << queue.size();
}

void EventDispatcher::postDelayed(EventPtr event, int delayMs)
{
    if (!event) {
        qCWarning(eventDispatcher) << "Attempting to post null delayed event";
        return;
    }
    
    QDateTime executeAt = QDateTime::currentDateTime().addMSecs(delayMs);
    
    QMutexLocker locker(&m_queueMutex);
    m_delayedEvents.emplace_back(event, executeAt);
    qCDebug(eventDispatcher) << "Posted delayed event:" << event->type() << "delay:" << delayMs << "ms";
}

bool EventDispatcher::processEvent(EventPtr event)
{
    if (!event || event->isConsumed()) {
        return false;
    }
    
    if (m_isPaused.loadRelaxed()) {
        return false;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        processEventInternal(event);
        
        auto endTime = std::chrono::steady_clock::now();
        auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        {
            QMutexLocker locker(&m_statsMutex);
            m_totalProcessingTime += processingTime;
        }
        
        m_eventsProcessed.fetchAndAddRelaxed(1);
        emit eventProcessed(event->type(), processingTime.count());
        
        return true;
    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        emit errorOccurred(QString("Exception processing event %1: %2").arg(event->type(), e.what()));
        emit processingTimeout(event->type(), processingTime.count());
        qCCritical(eventDispatcher) << "Exception processing event" << event->type() << ":" << e.what();
        return false;
    }
}

void EventDispatcher::processEventInternal(EventPtr event)
{
    QMutexLocker locker(&m_handlersMutex);
    
    auto filterIt = m_filters.find(event->type());
    if (filterIt != m_filters.end() && !filterIt->second(event)) {
        qCDebug(eventDispatcher) << "Event filtered out:" << event->type();
        return;
    }
    
    auto handlersIt = m_handlers.find(event->type());
    if (handlersIt == m_handlers.end()) {
        qCDebug(eventDispatcher) << "No handlers for event type:" << event->type();
        return;
    }
    
    const auto& handlers = handlersIt->second;
    for (const auto& handler : handlers) {
        if (event->isConsumed()) {
            break;
        }
        
        if (!invokeHandler(handler, event)) {
            qCWarning(eventDispatcher) << "Failed to invoke handler for event:" << event->type();
        }
    }
}

bool EventDispatcher::invokeHandler(const HandlerInfo& handler, EventPtr event)
{
    if (handler.isQObjectReceiver) {
        if (!handler.receiver) {
            return false;
        }
        
        return handler.method.invoke(handler.receiver, 
                                   Q_ARG(Monitor::Events::EventPtr, event));
    } else {
        handler.handler(event);
        return true;
    }
}

void EventDispatcher::processQueuedEvents()
{
    QMutexLocker locker(&m_queueMutex);
    
    for (auto& [eventType, queue] : m_eventQueues) {
        while (!queue.empty()) {
            auto entry = queue.top();
            queue.pop();
            
            locker.unlock();
            processEvent(entry.event);
            locker.relock();
        }
    }
}

void EventDispatcher::processQueuedEventsFor(const QString& eventType)
{
    QMutexLocker locker(&m_queueMutex);
    
    auto it = m_eventQueues.find(eventType);
    if (it != m_eventQueues.end()) {
        auto& queue = it->second;
        while (!queue.empty()) {
            auto entry = queue.top();
            queue.pop();
            
            locker.unlock();
            processEvent(entry.event);
            locker.relock();
        }
    }
}

void EventDispatcher::start()
{
    m_isRunning.storeRelaxed(true);
    m_delayedEventTimer->start();
    qCInfo(eventDispatcher) << "Event dispatcher started";
}

void EventDispatcher::stop()
{
    m_isRunning.storeRelaxed(false);
    m_delayedEventTimer->stop();
    
    processQueuedEvents();
    
    qCInfo(eventDispatcher) << "Event dispatcher stopped";
}

void EventDispatcher::pause()
{
    m_isPaused.storeRelaxed(true);
    qCInfo(eventDispatcher) << "Event dispatcher paused";
}

void EventDispatcher::resume()
{
    m_isPaused.storeRelaxed(false);
    qCInfo(eventDispatcher) << "Event dispatcher resumed";
}

size_t EventDispatcher::getQueueSize() const
{
    QMutexLocker locker(&m_queueMutex);
    size_t total = 0;
    for (const auto& [eventType, queue] : m_eventQueues) {
        total += queue.size();
    }
    return total;
}

size_t EventDispatcher::getQueueSize(const QString& eventType) const
{
    QMutexLocker locker(&m_queueMutex);
    auto it = m_eventQueues.find(eventType);
    return (it != m_eventQueues.end()) ? it->second.size() : 0;
}

double EventDispatcher::getAverageProcessingTime() const
{
    QMutexLocker locker(&m_statsMutex);
    qint64 processed = m_eventsProcessed.loadRelaxed();
    
    if (processed == 0) {
        return 0.0;
    }
    
    auto avgNanos = m_totalProcessingTime.count() / processed;
    return static_cast<double>(avgNanos) / 1000.0; // Convert to microseconds
}

void EventDispatcher::processDelayedEvents()
{
    QDateTime now = QDateTime::currentDateTime();
    
    QMutexLocker locker(&m_queueMutex);
    
    for (auto it = m_delayedEvents.begin(); it != m_delayedEvents.end();) {
        if (it->executeAt <= now) {
            post(it->event);
            it = m_delayedEvents.erase(it);
        } else {
            ++it;
        }
    }
}

ScopedEventSubscription::ScopedEventSubscription(EventDispatcher* dispatcher, const QString& eventType, EventHandler handler)
    : m_dispatcher(dispatcher)
    , m_eventType(eventType)
    , m_receiver(nullptr)
    , m_isValid(true)
{
    if (m_dispatcher) {
        m_dispatcher->subscribe(m_eventType, std::move(handler));
    }
}

ScopedEventSubscription::ScopedEventSubscription(EventDispatcher* dispatcher, const QString& eventType, QObject* receiver, const char* slot)
    : m_dispatcher(dispatcher)
    , m_eventType(eventType)
    , m_receiver(receiver)
    , m_isValid(true)
{
    if (m_dispatcher && m_receiver) {
        m_dispatcher->subscribe(m_eventType, m_receiver, slot);
    }
}

ScopedEventSubscription::~ScopedEventSubscription()
{
    if (m_isValid && m_dispatcher && m_receiver) {
        m_dispatcher->unsubscribe(m_eventType, m_receiver);
    }
}

ScopedEventSubscription::ScopedEventSubscription(ScopedEventSubscription&& other) noexcept
    : m_dispatcher(other.m_dispatcher)
    , m_eventType(std::move(other.m_eventType))
    , m_receiver(other.m_receiver)
    , m_isValid(other.m_isValid)
{
    other.m_isValid = false;
}

ScopedEventSubscription& ScopedEventSubscription::operator=(ScopedEventSubscription&& other) noexcept
{
    if (this != &other) {
        if (m_isValid && m_dispatcher && m_receiver) {
            m_dispatcher->unsubscribe(m_eventType, m_receiver);
        }
        
        m_dispatcher = other.m_dispatcher;
        m_eventType = std::move(other.m_eventType);
        m_receiver = other.m_receiver;
        m_isValid = other.m_isValid;
        
        other.m_isValid = false;
    }
    return *this;
}

} // namespace Events
} // namespace Monitor

