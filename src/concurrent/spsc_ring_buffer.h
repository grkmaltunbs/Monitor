#pragma once

#include <QtCore/QAtomicInteger>
#include <atomic>
#include <memory>
#include <cstdint>
#include <type_traits>

namespace Monitor {
namespace Concurrent {

/**
 * @brief High-performance Single Producer Single Consumer (SPSC) ring buffer
 * 
 * This is a lock-free, wait-free ring buffer optimized for single producer
 * and single consumer scenarios. It provides cache-line alignment to prevent
 * false sharing and uses memory ordering to ensure correctness.
 * 
 * Key features:
 * - Lock-free and wait-free operations
 * - Cache-line aligned head/tail pointers
 * - Power-of-2 size requirement for optimal performance
 * - Memory ordering guarantees
 * - Exception-safe operations
 */
template<typename T>
class SPSCRingBuffer
{
public:
    static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>,
                  "T must be move or copy constructible");
    
    /**
     * @brief Constructs an SPSC ring buffer
     * @param capacity Buffer capacity (must be power of 2)
     */
    explicit SPSCRingBuffer(size_t capacity);
    
    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~SPSCRingBuffer();
    
    // Non-copyable, non-assignable
    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;
    
    // Move operations
    SPSCRingBuffer(SPSCRingBuffer&& other) noexcept;
    SPSCRingBuffer& operator=(SPSCRingBuffer&& other) noexcept;
    
    /**
     * @brief Attempts to push an item (producer side)
     * @param item Item to push
     * @return true if successful, false if buffer is full
     */
    bool tryPush(const T& item);
    bool tryPush(T&& item);
    
    /**
     * @brief Attempts to pop an item (consumer side)
     * @param item Reference to store the popped item
     * @return true if successful, false if buffer is empty
     */
    bool tryPop(T& item);
    
    /**
     * @brief Attempts to peek at the front item without removing it
     * @param item Reference to store the peeked item
     * @return true if successful, false if buffer is empty
     */
    bool tryPeek(T& item) const;
    
    /**
     * @brief Gets the current size of the buffer
     * @return Number of items currently in the buffer
     * @note This is an approximation and may not be exact in concurrent scenarios
     */
    size_t size() const noexcept;
    
    /**
     * @brief Checks if the buffer is empty
     * @return true if buffer appears empty
     * @note This is an approximation and may not be exact in concurrent scenarios
     */
    bool empty() const noexcept;
    
    /**
     * @brief Checks if the buffer is full
     * @return true if buffer appears full
     * @note This is an approximation and may not be exact in concurrent scenarios
     */
    bool full() const noexcept;
    
    /**
     * @brief Gets the capacity of the buffer
     * @return Buffer capacity
     */
    size_t capacity() const noexcept { return m_capacity; }
    
    /**
     * @brief Gets the mask used for index wrapping
     * @return Index mask (capacity - 1)
     */
    size_t mask() const noexcept { return m_mask; }
    
    /**
     * @brief Clears all items from the buffer (NOT thread-safe)
     * @note This should only be called when no producer/consumer is active
     */
    void clear() noexcept;
    
    /**
     * @brief Gets statistics about buffer usage
     */
    struct Statistics {
        size_t totalPushes = 0;
        size_t totalPops = 0;
        size_t pushFailures = 0;
        size_t popFailures = 0;
        size_t currentSize = 0;
        double utilizationPercent = 0.0;
    };
    
    Statistics getStatistics() const;
    void resetStatistics();

private:
    /**
     * @brief Checks if a number is a power of 2
     */
    static bool isPowerOfTwo(size_t n) noexcept;
    
    /**
     * @brief Rounds up to the next power of 2
     */
    static size_t nextPowerOfTwo(size_t n) noexcept;
    
    // Cache line size for alignment
    static constexpr size_t CACHE_LINE_SIZE = 64;
    
    // Buffer storage
    std::unique_ptr<T[]> m_buffer;
    size_t m_capacity;
    size_t m_mask;
    
    // Producer variables (aligned to prevent false sharing)
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> m_head{0};
    alignas(CACHE_LINE_SIZE) mutable std::atomic<size_t> m_cachedTail{0};
    
    // Consumer variables (aligned to prevent false sharing)
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> m_tail{0};
    alignas(CACHE_LINE_SIZE) mutable std::atomic<size_t> m_cachedHead{0};
    
    // Statistics (aligned to prevent false sharing)
    alignas(CACHE_LINE_SIZE) mutable std::atomic<size_t> m_totalPushes{0};
    mutable std::atomic<size_t> m_totalPops{0};
    mutable std::atomic<size_t> m_pushFailures{0};
    mutable std::atomic<size_t> m_popFailures{0};
};

// Implementation

template<typename T>
SPSCRingBuffer<T>::SPSCRingBuffer(size_t capacity)
    : m_capacity(isPowerOfTwo(capacity) ? capacity : nextPowerOfTwo(capacity))
    , m_mask(m_capacity - 1)
{
    if (capacity == 0) {
        throw std::invalid_argument("SPSCRingBuffer capacity must be greater than 0");
    }
    
    if (m_capacity > SIZE_MAX / 2) {
        throw std::invalid_argument("SPSCRingBuffer capacity too large");
    }
    
    try {
        m_buffer = std::make_unique<T[]>(m_capacity);
    } catch (const std::bad_alloc&) {
        throw std::runtime_error("SPSCRingBuffer: Failed to allocate buffer memory");
    }
}

template<typename T>
SPSCRingBuffer<T>::~SPSCRingBuffer()
{
    clear();
}

template<typename T>
SPSCRingBuffer<T>::SPSCRingBuffer(SPSCRingBuffer&& other) noexcept
    : m_buffer(std::move(other.m_buffer))
    , m_capacity(other.m_capacity)
    , m_mask(other.m_mask)
    , m_head(other.m_head.load(std::memory_order_relaxed))
    , m_cachedTail(other.m_cachedTail.load(std::memory_order_relaxed))
    , m_tail(other.m_tail.load(std::memory_order_relaxed))
    , m_cachedHead(other.m_cachedHead.load(std::memory_order_relaxed))
    , m_totalPushes(other.m_totalPushes.load(std::memory_order_relaxed))
    , m_totalPops(other.m_totalPops.load(std::memory_order_relaxed))
    , m_pushFailures(other.m_pushFailures.load(std::memory_order_relaxed))
    , m_popFailures(other.m_popFailures.load(std::memory_order_relaxed))
{
    // Reset the moved-from object
    other.m_capacity = 0;
    other.m_mask = 0;
    other.m_head.store(0, std::memory_order_relaxed);
    other.m_cachedTail.store(0, std::memory_order_relaxed);
    other.m_tail.store(0, std::memory_order_relaxed);
    other.m_cachedHead.store(0, std::memory_order_relaxed);
}

template<typename T>
SPSCRingBuffer<T>& SPSCRingBuffer<T>::operator=(SPSCRingBuffer&& other) noexcept
{
    if (this != &other) {
        clear();
        
        m_buffer = std::move(other.m_buffer);
        m_capacity = other.m_capacity;
        m_mask = other.m_mask;
        m_head.store(other.m_head.load(std::memory_order_relaxed), std::memory_order_relaxed);
        m_cachedTail.store(other.m_cachedTail.load(std::memory_order_relaxed), std::memory_order_relaxed);
        m_tail.store(other.m_tail.load(std::memory_order_relaxed), std::memory_order_relaxed);
        m_cachedHead.store(other.m_cachedHead.load(std::memory_order_relaxed), std::memory_order_relaxed);
        
        // Reset the moved-from object
        other.m_capacity = 0;
        other.m_mask = 0;
        other.m_head.store(0, std::memory_order_relaxed);
        other.m_cachedTail.store(0, std::memory_order_relaxed);
        other.m_tail.store(0, std::memory_order_relaxed);
        other.m_cachedHead.store(0, std::memory_order_relaxed);
    }
    return *this;
}

template<typename T>
bool SPSCRingBuffer<T>::tryPush(const T& item)
{
    const size_t head = m_head.load(std::memory_order_relaxed);
    const size_t nextHead = (head + 1) & m_mask;
    
    // Check against cached tail first to avoid expensive atomic load
    if (nextHead == m_cachedTail.load(std::memory_order_relaxed)) {
        // Update cached tail with actual value
        m_cachedTail.store(m_tail.load(std::memory_order_acquire), std::memory_order_relaxed);
        
        if (nextHead == m_cachedTail.load(std::memory_order_relaxed)) {
            m_pushFailures.fetch_add(1, std::memory_order_relaxed);
            return false; // Buffer is full
        }
    }
    
    // Construct/assign the item
    if constexpr (std::is_copy_assignable_v<T>) {
        m_buffer[head] = item;
    } else {
        new (&m_buffer[head]) T(item);
    }
    
    // Make the item visible to consumer
    m_head.store(nextHead, std::memory_order_release);
    m_totalPushes.fetch_add(1, std::memory_order_relaxed);
    
    return true;
}

template<typename T>
bool SPSCRingBuffer<T>::tryPush(T&& item)
{
    const size_t head = m_head.load(std::memory_order_relaxed);
    const size_t nextHead = (head + 1) & m_mask;
    
    // Check against cached tail first
    if (nextHead == m_cachedTail.load(std::memory_order_relaxed)) {
        // Update cached tail with actual value
        m_cachedTail.store(m_tail.load(std::memory_order_acquire), std::memory_order_relaxed);
        
        if (nextHead == m_cachedTail.load(std::memory_order_relaxed)) {
            m_pushFailures.fetch_add(1, std::memory_order_relaxed);
            return false; // Buffer is full
        }
    }
    
    // Move the item
    if constexpr (std::is_move_assignable_v<T>) {
        m_buffer[head] = std::move(item);
    } else {
        new (&m_buffer[head]) T(std::move(item));
    }
    
    // Make the item visible to consumer
    m_head.store(nextHead, std::memory_order_release);
    m_totalPushes.fetch_add(1, std::memory_order_relaxed);
    
    return true;
}

template<typename T>
bool SPSCRingBuffer<T>::tryPop(T& item)
{
    const size_t tail = m_tail.load(std::memory_order_relaxed);
    
    // Check against cached head first
    if (tail == m_cachedHead.load(std::memory_order_relaxed)) {
        // Update cached head with actual value
        m_cachedHead.store(m_head.load(std::memory_order_acquire), std::memory_order_relaxed);
        
        if (tail == m_cachedHead.load(std::memory_order_relaxed)) {
            m_popFailures.fetch_add(1, std::memory_order_relaxed);
            return false; // Buffer is empty
        }
    }
    
    // Move/copy the item
    if constexpr (std::is_move_assignable_v<T>) {
        item = std::move(m_buffer[tail]);
    } else {
        item = m_buffer[tail];
    }
    
    // Destroy the item if necessary
    if constexpr (!std::is_trivially_destructible_v<T>) {
        m_buffer[tail].~T();
    }
    
    // Update tail to make space available to producer
    m_tail.store((tail + 1) & m_mask, std::memory_order_release);
    m_totalPops.fetch_add(1, std::memory_order_relaxed);
    
    return true;
}

template<typename T>
bool SPSCRingBuffer<T>::tryPeek(T& item) const
{
    const size_t tail = m_tail.load(std::memory_order_relaxed);
    
    // Check against cached head first
    if (tail == m_cachedHead.load(std::memory_order_relaxed)) {
        // Update cached head with actual value
        m_cachedHead.store(m_head.load(std::memory_order_acquire), std::memory_order_relaxed);
        
        if (tail == m_cachedHead.load(std::memory_order_relaxed)) {
            return false; // Buffer is empty
        }
    }
    
    // Copy the item without removing it
    item = m_buffer[tail];
    return true;
}

template<typename T>
size_t SPSCRingBuffer<T>::size() const noexcept
{
    const size_t head = m_head.load(std::memory_order_acquire);
    const size_t tail = m_tail.load(std::memory_order_acquire);
    return (head - tail) & m_mask;
}

template<typename T>
bool SPSCRingBuffer<T>::empty() const noexcept
{
    return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
}

template<typename T>
bool SPSCRingBuffer<T>::full() const noexcept
{
    const size_t head = m_head.load(std::memory_order_acquire);
    const size_t tail = m_tail.load(std::memory_order_acquire);
    return ((head + 1) & m_mask) == tail;
}

template<typename T>
void SPSCRingBuffer<T>::clear() noexcept
{
    if constexpr (!std::is_trivially_destructible_v<T>) {
        // Destroy all remaining items
        size_t tail = m_tail.load(std::memory_order_relaxed);
        const size_t head = m_head.load(std::memory_order_relaxed);
        
        while (tail != head) {
            m_buffer[tail].~T();
            tail = (tail + 1) & m_mask;
        }
    }
    
    m_head.store(0, std::memory_order_relaxed);
    m_tail.store(0, std::memory_order_relaxed);
    m_cachedHead.store(0, std::memory_order_relaxed);
    m_cachedTail.store(0, std::memory_order_relaxed);
}

template<typename T>
typename SPSCRingBuffer<T>::Statistics SPSCRingBuffer<T>::getStatistics() const
{
    Statistics stats;
    stats.totalPushes = m_totalPushes.load(std::memory_order_relaxed);
    stats.totalPops = m_totalPops.load(std::memory_order_relaxed);
    stats.pushFailures = m_pushFailures.load(std::memory_order_relaxed);
    stats.popFailures = m_popFailures.load(std::memory_order_relaxed);
    stats.currentSize = size();
    stats.utilizationPercent = (static_cast<double>(stats.currentSize) / m_capacity) * 100.0;
    return stats;
}

template<typename T>
void SPSCRingBuffer<T>::resetStatistics()
{
    m_totalPushes.store(0, std::memory_order_relaxed);
    m_totalPops.store(0, std::memory_order_relaxed);
    m_pushFailures.store(0, std::memory_order_relaxed);
    m_popFailures.store(0, std::memory_order_relaxed);
}

template<typename T>
bool SPSCRingBuffer<T>::isPowerOfTwo(size_t n) noexcept
{
    return n != 0 && (n & (n - 1)) == 0;
}

template<typename T>
size_t SPSCRingBuffer<T>::nextPowerOfTwo(size_t n) noexcept
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