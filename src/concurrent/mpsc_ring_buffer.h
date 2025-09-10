#pragma once

#include <QtCore/QAtomicInteger>
#include <atomic>
#include <memory>
#include <cstdint>
#include <type_traits>
#include <thread>
#include <chrono>

namespace Monitor {
namespace Concurrent {

/**
 * @brief High-performance Multi Producer Multi Consumer (MPMC) ring buffer
 * 
 * This is a lock-free ring buffer optimized for multiple producer threads
 * and multiple consumer threads. It uses compare-and-swap operations for
 * thread-safe access while maintaining high performance.
 * 
 * Key features:
 * - Lock-free operations for multiple producers
 * - Lock-free consumer operations with CAS-based slot claiming
 * - Cache-line aligned to prevent false sharing
 * - Power-of-2 size requirement for optimal performance
 * - Overflow detection and handling
 * - Back-pressure support
 */
template<typename T>
class MPSCRingBuffer
{
public:
    static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>,
                  "T must be move or copy constructible");
    
    /**
     * @brief Constructs an MPSC ring buffer
     * @param capacity Buffer capacity (must be power of 2)
     */
    explicit MPSCRingBuffer(size_t capacity);
    
    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~MPSCRingBuffer();
    
    // Non-copyable, non-assignable
    MPSCRingBuffer(const MPSCRingBuffer&) = delete;
    MPSCRingBuffer& operator=(const MPSCRingBuffer&) = delete;
    
    // Move operations
    MPSCRingBuffer(MPSCRingBuffer&& other) noexcept;
    MPSCRingBuffer& operator=(MPSCRingBuffer&& other) noexcept;
    
    /**
     * @brief Attempts to push an item (multiple producers)
     * @param item Item to push
     * @return true if successful, false if buffer is full
     */
    bool tryPush(const T& item);
    bool tryPush(T&& item);
    
    /**
     * @brief Blocking push with timeout (multiple producers)
     * @param item Item to push
     * @param timeoutMs Timeout in milliseconds (0 = no timeout)
     * @return true if successful, false if timeout or full
     */
    bool timedPush(const T& item, int timeoutMs = 0);
    bool timedPush(T&& item, int timeoutMs = 0);
    
    /**
     * @brief Attempts to pop an item (multi-consumer safe)
     * @param item Reference to store the popped item
     * @return true if successful, false if buffer is empty
     */
    bool tryPop(T& item);
    
    /**
     * @brief Batch pop operation for better throughput
     * @param items Array to store popped items
     * @param maxItems Maximum number of items to pop
     * @return Number of items actually popped
     */
    size_t tryPopBatch(T* items, size_t maxItems);
    
    /**
     * @brief Attempts to peek at the front item without removing it
     * @param item Reference to store the peeked item
     * @return true if successful, false if buffer is empty
     */
    bool tryPeek(T& item) const;
    
    /**
     * @brief Gets the current approximate size of the buffer
     * @return Number of items currently in the buffer (approximate)
     */
    size_t size() const noexcept;
    
    /**
     * @brief Checks if the buffer appears empty
     * @return true if buffer appears empty (approximate)
     */
    bool empty() const noexcept;
    
    /**
     * @brief Checks if the buffer appears full
     * @return true if buffer appears full (approximate)
     */
    bool full() const noexcept;
    
    /**
     * @brief Gets the capacity of the buffer
     * @return Buffer capacity
     */
    size_t capacity() const noexcept { return m_capacity; }
    
    /**
     * @brief Clears all items from the buffer (NOT thread-safe)
     * @note This should only be called when no producer/consumer is active
     */
    void clear() noexcept;
    
    /**
     * @brief Enable/disable back-pressure signaling
     * @param enabled If true, provides back-pressure when nearly full
     * @param threshold Threshold percentage (0.0-1.0) for back-pressure
     */
    void setBackPressureEnabled(bool enabled, double threshold = 0.8);
    
    /**
     * @brief Checks if back-pressure should be applied
     * @return true if producers should slow down
     */
    bool shouldApplyBackPressure() const noexcept;
    
    /**
     * @brief Buffer statistics
     */
    struct Statistics {
        size_t totalPushes = 0;
        size_t totalPops = 0;
        size_t pushFailures = 0;
        size_t popFailures = 0;
        size_t casFailures = 0; // Compare-and-swap failures
        size_t currentSize = 0;
        double utilizationPercent = 0.0;
        size_t backPressureEvents = 0;
    };
    
    Statistics getStatistics() const;
    void resetStatistics();

private:
    /**
     * @brief Buffer slot for storing items with metadata
     */
    struct alignas(64) Slot {
        std::atomic<size_t> sequence{0};
        T data;
        
        Slot() = default;
        
        template<typename U>
        void store(U&& item, size_t seq) {
            if constexpr (std::is_move_assignable_v<T>) {
                data = std::forward<U>(item);
            } else {
                new (&data) T(std::forward<U>(item));
            }
            sequence.store(seq, std::memory_order_release);
        }
        
        bool tryLoad(T& item, size_t expectedSeq) {
            if (sequence.load(std::memory_order_acquire) != expectedSeq) {
                return false;
            }
            
            if constexpr (std::is_move_assignable_v<T>) {
                item = std::move(data);
            } else {
                item = data;
            }
            
            if constexpr (!std::is_trivially_destructible_v<T>) {
                data.~T();
            }
            
            return true;
        }
    };
    
    static bool isPowerOfTwo(size_t n) noexcept;
    static size_t nextPowerOfTwo(size_t n) noexcept;
    
    template<typename U>
    bool pushInternal(U&& item);
    
    // Cache line size for alignment
    static constexpr size_t CACHE_LINE_SIZE = 64;
    
    // Buffer storage
    std::unique_ptr<Slot[]> m_buffer;
    size_t m_capacity;
    size_t m_mask;
    
    // Producer side (cache-line aligned)
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> m_head{0};
    
    // Consumer side (cache-line aligned) 
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> m_tail{0};
    alignas(CACHE_LINE_SIZE) mutable std::atomic<size_t> m_cachedHead{0};
    
    // Back-pressure control
    alignas(CACHE_LINE_SIZE) std::atomic<bool> m_backPressureEnabled{false};
    std::atomic<size_t> m_backPressureThreshold{0};
    mutable std::atomic<size_t> m_backPressureEvents{0};
    
    // Statistics (cache-line aligned)
    alignas(CACHE_LINE_SIZE) mutable std::atomic<size_t> m_totalPushes{0};
    mutable std::atomic<size_t> m_totalPops{0};
    mutable std::atomic<size_t> m_pushFailures{0};
    mutable std::atomic<size_t> m_popFailures{0};
    mutable std::atomic<size_t> m_casFailures{0};
};

// Implementation

template<typename T>
MPSCRingBuffer<T>::MPSCRingBuffer(size_t capacity)
    : m_capacity(isPowerOfTwo(capacity) ? capacity : nextPowerOfTwo(capacity))
    , m_mask(m_capacity - 1)
{
    if (capacity == 0) {
        throw std::invalid_argument("MPSCRingBuffer capacity must be greater than 0");
    }
    
    if (m_capacity > SIZE_MAX / 2) {
        throw std::invalid_argument("MPSCRingBuffer capacity too large");
    }
    
    try {
        m_buffer = std::make_unique<Slot[]>(m_capacity);
    } catch (const std::bad_alloc&) {
        throw std::runtime_error("MPSCRingBuffer: Failed to allocate buffer memory");
    }
    
    // Initialize slots with proper sequence numbers
    for (size_t i = 0; i < m_capacity; ++i) {
        m_buffer[i].sequence.store(i, std::memory_order_relaxed);
    }
}

template<typename T>
MPSCRingBuffer<T>::~MPSCRingBuffer()
{
    clear();
}

template<typename T>
MPSCRingBuffer<T>::MPSCRingBuffer(MPSCRingBuffer&& other) noexcept
    : m_buffer(std::move(other.m_buffer))
    , m_capacity(other.m_capacity)
    , m_mask(other.m_mask)
    , m_head(other.m_head.load(std::memory_order_relaxed))
    , m_tail(other.m_tail.load(std::memory_order_relaxed))
    , m_cachedHead(other.m_cachedHead.load(std::memory_order_relaxed))
    , m_backPressureEnabled(other.m_backPressureEnabled.load(std::memory_order_relaxed))
    , m_backPressureThreshold(other.m_backPressureThreshold.load(std::memory_order_relaxed))
    , m_backPressureEvents(other.m_backPressureEvents.load(std::memory_order_relaxed))
    , m_totalPushes(other.m_totalPushes.load(std::memory_order_relaxed))
    , m_totalPops(other.m_totalPops.load(std::memory_order_relaxed))
    , m_pushFailures(other.m_pushFailures.load(std::memory_order_relaxed))
    , m_popFailures(other.m_popFailures.load(std::memory_order_relaxed))
    , m_casFailures(other.m_casFailures.load(std::memory_order_relaxed))
{
    // Reset the moved-from object
    other.m_capacity = 0;
    other.m_mask = 0;
    other.m_head.store(0, std::memory_order_relaxed);
    other.m_tail.store(0, std::memory_order_relaxed);
    other.m_cachedHead.store(0, std::memory_order_relaxed);
}

template<typename T>
MPSCRingBuffer<T>& MPSCRingBuffer<T>::operator=(MPSCRingBuffer&& other) noexcept
{
    if (this != &other) {
        clear();
        
        m_buffer = std::move(other.m_buffer);
        m_capacity = other.m_capacity;
        m_mask = other.m_mask;
        m_head.store(other.m_head.load(std::memory_order_relaxed), std::memory_order_relaxed);
        m_tail.store(other.m_tail.load(std::memory_order_relaxed), std::memory_order_relaxed);
        m_cachedHead.store(other.m_cachedHead.load(std::memory_order_relaxed), std::memory_order_relaxed);
        
        // Reset the moved-from object
        other.m_capacity = 0;
        other.m_mask = 0;
        other.m_head.store(0, std::memory_order_relaxed);
        other.m_tail.store(0, std::memory_order_relaxed);
        other.m_cachedHead.store(0, std::memory_order_relaxed);
    }
    return *this;
}

template<typename T>
bool MPSCRingBuffer<T>::tryPush(const T& item)
{
    return pushInternal(item);
}

template<typename T>
bool MPSCRingBuffer<T>::tryPush(T&& item)
{
    return pushInternal(std::move(item));
}

template<typename T>
template<typename U>
bool MPSCRingBuffer<T>::pushInternal(U&& item)
{
    size_t head = m_head.load(std::memory_order_relaxed);
    
    for (;;) {
        Slot& slot = m_buffer[head & m_mask];
        size_t sequence = slot.sequence.load(std::memory_order_acquire);
        intptr_t diff = static_cast<intptr_t>(sequence) - static_cast<intptr_t>(head);
        
        if (diff == 0) {
            // Slot is available, try to claim it
            if (m_head.compare_exchange_weak(head, head + 1, std::memory_order_relaxed)) {
                // Successfully claimed the slot
                slot.store(std::forward<U>(item), head + 1);
                m_totalPushes.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
            // CAS failed, retry with new head value
            m_casFailures.fetch_add(1, std::memory_order_relaxed);
        } else if (diff < 0) {
            // Buffer is full
            m_pushFailures.fetch_add(1, std::memory_order_relaxed);
            return false;
        } else {
            // Another producer is in the process of writing to this slot
            // Update head and try again
            head = m_head.load(std::memory_order_relaxed);
        }
    }
}

template<typename T>
bool MPSCRingBuffer<T>::timedPush(const T& item, int timeoutMs)
{
    // Simple implementation - for a full implementation, you'd use timed backoff
    constexpr int maxRetries = 1000;
    const int usleepTime = timeoutMs > 0 ? (timeoutMs * 1000) / maxRetries : 1;
    
    for (int retry = 0; retry < maxRetries; ++retry) {
        if (tryPush(item)) {
            return true;
        }
        
        if (timeoutMs > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(usleepTime));
        } else {
            std::this_thread::yield();
        }
    }
    
    return false;
}

template<typename T>
bool MPSCRingBuffer<T>::timedPush(T&& item, int timeoutMs)
{
    // For move semantics, we can only try once to avoid multiple moves
    // We could potentially retry with a timeout, but that would require copying
    (void)timeoutMs; // Suppress unused parameter warning
    return tryPush(std::move(item));
}

template<typename T>
bool MPSCRingBuffer<T>::tryPop(T& item)
{
    size_t tail = m_tail.load(std::memory_order_relaxed);
    
    for (;;) {
        Slot& slot = m_buffer[tail & m_mask];
        size_t sequence = slot.sequence.load(std::memory_order_acquire);
        intptr_t diff = static_cast<intptr_t>(sequence) - static_cast<intptr_t>(tail + 1);
        
        if (diff == 0) {
            // Item is available, try to claim it atomically
            if (m_tail.compare_exchange_weak(tail, tail + 1, std::memory_order_relaxed)) {
                // Successfully claimed the slot, now load the item
                if (slot.tryLoad(item, tail + 1)) {
                    slot.sequence.store(tail + m_capacity, std::memory_order_release);
                    m_totalPops.fetch_add(1, std::memory_order_relaxed);
                    return true;
                } else {
                    // This should not happen if sequence numbers are correct
                    // Reset tail and return failure
                    m_tail.store(tail, std::memory_order_relaxed);
                    return false;
                }
            }
            // CAS failed, retry with new tail value
            // tail was updated by compare_exchange_weak
        } else if (diff < 0) {
            // Buffer is empty
            m_popFailures.fetch_add(1, std::memory_order_relaxed);
            return false;
        } else {
            // Another consumer is in the process of reading this slot
            // Update tail and try again
            tail = m_tail.load(std::memory_order_relaxed);
        }
    }
}

template<typename T>
size_t MPSCRingBuffer<T>::tryPopBatch(T* items, size_t maxItems)
{
    size_t popped = 0;
    
    for (size_t i = 0; i < maxItems; ++i) {
        if (!tryPop(items[i])) {
            break;
        }
        ++popped;
    }
    
    return popped;
}

template<typename T>
bool MPSCRingBuffer<T>::tryPeek(T& item) const
{
    size_t tail = m_tail.load(std::memory_order_relaxed);
    const Slot& slot = m_buffer[tail & m_mask];
    size_t sequence = slot.sequence.load(std::memory_order_acquire);
    
    if (sequence == tail + 1) {
        item = slot.data;
        return true;
    }
    
    return false;
}

template<typename T>
size_t MPSCRingBuffer<T>::size() const noexcept
{
    size_t head = m_head.load(std::memory_order_acquire);
    size_t tail = m_tail.load(std::memory_order_acquire);
    return head - tail;
}

template<typename T>
bool MPSCRingBuffer<T>::empty() const noexcept
{
    return size() == 0;
}

template<typename T>
bool MPSCRingBuffer<T>::full() const noexcept
{
    return size() >= m_capacity;
}

template<typename T>
void MPSCRingBuffer<T>::clear() noexcept
{
    // Drain the buffer
    T dummy;
    while (tryPop(dummy)) {
        // Items are destroyed in tryPop
    }
    
    // Reset sequence numbers
    for (size_t i = 0; i < m_capacity; ++i) {
        m_buffer[i].sequence.store(i, std::memory_order_relaxed);
    }
    
    m_head.store(0, std::memory_order_relaxed);
    m_tail.store(0, std::memory_order_relaxed);
    m_cachedHead.store(0, std::memory_order_relaxed);
}

template<typename T>
void MPSCRingBuffer<T>::setBackPressureEnabled(bool enabled, double threshold)
{
    m_backPressureEnabled.store(enabled, std::memory_order_relaxed);
    m_backPressureThreshold.store(static_cast<size_t>(threshold * m_capacity), 
                                 std::memory_order_relaxed);
}

template<typename T>
bool MPSCRingBuffer<T>::shouldApplyBackPressure() const noexcept
{
    if (!m_backPressureEnabled.load(std::memory_order_relaxed)) {
        return false;
    }
    
    size_t currentSize = size();
    size_t threshold = m_backPressureThreshold.load(std::memory_order_relaxed);
    
    if (currentSize > threshold) {
        m_backPressureEvents.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    
    return false;
}

template<typename T>
typename MPSCRingBuffer<T>::Statistics MPSCRingBuffer<T>::getStatistics() const
{
    Statistics stats;
    stats.totalPushes = m_totalPushes.load(std::memory_order_relaxed);
    stats.totalPops = m_totalPops.load(std::memory_order_relaxed);
    stats.pushFailures = m_pushFailures.load(std::memory_order_relaxed);
    stats.popFailures = m_popFailures.load(std::memory_order_relaxed);
    stats.casFailures = m_casFailures.load(std::memory_order_relaxed);
    stats.currentSize = size();
    stats.utilizationPercent = (static_cast<double>(stats.currentSize) / m_capacity) * 100.0;
    stats.backPressureEvents = m_backPressureEvents.load(std::memory_order_relaxed);
    return stats;
}

template<typename T>
void MPSCRingBuffer<T>::resetStatistics()
{
    m_totalPushes.store(0, std::memory_order_relaxed);
    m_totalPops.store(0, std::memory_order_relaxed);
    m_pushFailures.store(0, std::memory_order_relaxed);
    m_popFailures.store(0, std::memory_order_relaxed);
    m_casFailures.store(0, std::memory_order_relaxed);
    m_backPressureEvents.store(0, std::memory_order_relaxed);
}

template<typename T>
bool MPSCRingBuffer<T>::isPowerOfTwo(size_t n) noexcept
{
    return n != 0 && (n & (n - 1)) == 0;
}

template<typename T>
size_t MPSCRingBuffer<T>::nextPowerOfTwo(size_t n) noexcept
{
    if (n <= 1) return 1;
    
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if constexpr (sizeof(size_t) > 4) {
        n |= n >> 32;
    }
    return ++n;
}

} // namespace Concurrent
} // namespace Monitor