#pragma once

#include <QtCore/QObject>
#include <QtCore/QAtomicInteger>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace Monitor {
namespace Memory {

/**
 * @brief High-performance optimized memory pool with thread-local caching
 */
class OptimizedMemoryPool : public QObject {
    Q_OBJECT
    
public:
    explicit OptimizedMemoryPool(size_t blockSize, size_t blockCount, QObject* parent = nullptr);
    ~OptimizedMemoryPool() override;
    
    // Hot path - optimized for speed
    void* allocate() noexcept;
    void deallocate(void* ptr) noexcept;
    
    // Batch operations for improved throughput
    size_t allocateBatch(void** ptrs, size_t count) noexcept;
    void deallocateBatch(void** ptrs, size_t count) noexcept;
    
    // Statistics
    double getUtilization() const noexcept;
    size_t getUsedBlocks() const noexcept { return m_usedBlocks.load(std::memory_order_relaxed); }
    
private:
    struct alignas(64) Block {
        Block* next;
    };
    
    // Thread-local cache for lock-free allocation
    struct ThreadCache {
        static constexpr size_t CACHE_SIZE = 64;
        void* blocks[CACHE_SIZE];
        size_t count = 0;
        
        void* tryAllocate() noexcept {
            return count > 0 ? blocks[--count] : nullptr;
        }
        
        bool tryDeallocate(void* ptr) noexcept {
            if (count < CACHE_SIZE) {
                blocks[count++] = ptr;
                return true;
            }
            return false;
        }
    };
    
    ThreadCache& getThreadCache() noexcept;
    void fillThreadCache(ThreadCache& cache) noexcept;
    void drainThreadCache(ThreadCache& cache) noexcept;
    
    // Memory layout optimized for cache efficiency
    const size_t m_blockSize;
    const size_t m_blockCount;
    
    alignas(64) std::unique_ptr<char[]> m_pool;
    alignas(64) std::atomic<Block*> m_freeList{nullptr};
    alignas(64) std::atomic<size_t> m_usedBlocks{0};
    
    char* m_poolStart;
    char* m_poolEnd;
    
    // Statistics
    mutable std::atomic<size_t> m_allocations{0};
    mutable std::atomic<size_t> m_deallocations{0};
    mutable std::atomic<size_t> m_cacheHits{0};
    mutable std::atomic<size_t> m_cacheMisses{0};
    
public:
    static constexpr double PRESSURE_THRESHOLD = 0.8;
};

/**
 * @brief SIMD-optimized batch memory operations
 */
class SIMDMemoryUtils {
public:
    // Vectorized memory copy for packet data
    static void vectorizedCopy(void* dst, const void* src, size_t size) noexcept;
    
    // Parallel memory clearing
    static void parallelClear(void* ptr, size_t size) noexcept;
    
    // Optimized memory comparison
    static bool fastCompare(const void* a, const void* b, size_t size) noexcept;
};

} // namespace Memory
} // namespace Monitor