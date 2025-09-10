#pragma once

#include "event.h"
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QAtomicInteger>
#include <QtCore/QMetaMethod>
#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>
#include <chrono>

namespace Monitor {
namespace Events {

using EventHandler = std::function<void(EventPtr)>;
using EventFilter = std::function<bool(EventPtr)>;

struct EventQueueEntry {
    EventPtr event;
    std::chrono::steady_clock::time_point enqueueTime;
    
    EventQueueEntry(EventPtr evt) 
        : event(std::move(evt))
        , enqueueTime(std::chrono::steady_clock::now())
    {}
};

struct EventQueueComparator {
    bool operator()(const EventQueueEntry& a, const EventQueueEntry& b) const {
        return static_cast<int>(a.event->priority()) < static_cast<int>(b.event->priority());
    }
};

using EventQueue = std::priority_queue<EventQueueEntry, std::vector<EventQueueEntry>, EventQueueComparator>;

class EventDispatcher : public QObject
{
    Q_OBJECT

public:
    explicit EventDispatcher(QObject* parent = nullptr);
    ~EventDispatcher() override;
    
    void subscribe(const QString& eventType, EventHandler handler);
    void subscribe(const QString& eventType, QObject* receiver, const char* slot);
    void unsubscribe(const QString& eventType, QObject* receiver);
    void unsubscribeAll(QObject* receiver);
    
    void setEventFilter(const QString& eventType, EventFilter filter);
    void removeEventFilter(const QString& eventType);
    
    void post(EventPtr event);
    void postDelayed(EventPtr event, int delayMs);
    
    bool processEvent(EventPtr event);
    void processQueuedEvents();
    void processQueuedEventsFor(const QString& eventType);
    
    void start();
    void stop();
    void pause();
    void resume();
    
    bool isRunning() const { return m_isRunning.loadRelaxed(); }
    bool isPaused() const { return m_isPaused.loadRelaxed(); }
    
    size_t getQueueSize() const;
    size_t getQueueSize(const QString& eventType) const;
    double getAverageProcessingTime() const;
    qint64 getEventsProcessed() const { return m_eventsProcessed.loadRelaxed(); }
    
    void setMaxQueueSize(size_t maxSize) { m_maxQueueSize = maxSize; }
    size_t getMaxQueueSize() const { return m_maxQueueSize; }
    
    void setProcessingTimeout(int timeoutMs) { m_processingTimeoutMs = timeoutMs; }
    int getProcessingTimeout() const { return m_processingTimeoutMs; }

signals:
    void eventProcessed(const QString& eventType, qint64 processingTimeUs);
    void queueOverflow(const QString& eventType, size_t queueSize);
    void processingTimeout(const QString& eventType, qint64 timeoutUs);
    void errorOccurred(const QString& error);

private slots:
    void processDelayedEvents();

private:
    struct HandlerInfo {
        EventHandler handler;
        QObject* receiver;
        QMetaMethod method;
        bool isQObjectReceiver;
        
        HandlerInfo(EventHandler h) : handler(std::move(h)), receiver(nullptr), isQObjectReceiver(false) {}
        HandlerInfo(QObject* r, const QMetaMethod& m) : receiver(r), method(m), isQObjectReceiver(true) {}
    };
    
    struct DelayedEvent {
        EventPtr event;
        QDateTime executeAt;
        
        DelayedEvent(EventPtr evt, const QDateTime& when) : event(std::move(evt)), executeAt(when) {}
    };
    
    void processEventInternal(EventPtr event);
    bool invokeHandler(const HandlerInfo& handler, EventPtr event);
    void cleanupDisconnectedReceivers();
    
    mutable QMutex m_queueMutex;
    mutable QMutex m_handlersMutex;
    mutable QMutex m_statsMutex;
    
    std::unordered_map<QString, EventQueue> m_eventQueues;
    std::unordered_map<QString, std::vector<HandlerInfo>> m_handlers;
    std::unordered_map<QString, EventFilter> m_filters;
    
    std::vector<DelayedEvent> m_delayedEvents;
    QTimer* m_delayedEventTimer;
    
    QAtomicInteger<bool> m_isRunning;
    QAtomicInteger<bool> m_isPaused;
    QAtomicInteger<qint64> m_eventsProcessed;
    
    size_t m_maxQueueSize;
    int m_processingTimeoutMs;
    
    std::chrono::steady_clock::time_point m_totalProcessingTimeStart;
    std::chrono::nanoseconds m_totalProcessingTime;
    
    static constexpr size_t DEFAULT_MAX_QUEUE_SIZE = 10000;
    static constexpr int DEFAULT_PROCESSING_TIMEOUT_MS = 1000;
    static constexpr int DELAYED_EVENT_TIMER_INTERVAL_MS = 10;
};

class ScopedEventSubscription
{
public:
    ScopedEventSubscription(EventDispatcher* dispatcher, const QString& eventType, EventHandler handler);
    ScopedEventSubscription(EventDispatcher* dispatcher, const QString& eventType, QObject* receiver, const char* slot);
    ~ScopedEventSubscription();
    
    ScopedEventSubscription(const ScopedEventSubscription&) = delete;
    ScopedEventSubscription& operator=(const ScopedEventSubscription&) = delete;
    
    ScopedEventSubscription(ScopedEventSubscription&& other) noexcept;
    ScopedEventSubscription& operator=(ScopedEventSubscription&& other) noexcept;

private:
    EventDispatcher* m_dispatcher;
    QString m_eventType;
    QObject* m_receiver;
    bool m_isValid;
};

} // namespace Events
} // namespace Monitor