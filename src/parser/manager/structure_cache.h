#pragma once

#include "../ast/ast_nodes.h"
#include "../layout/layout_calculator.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <string>
#include <chrono>
#include <shared_mutex>

namespace Monitor {
namespace Parser {

/**
 * @brief High-performance cache for parsed structures and their layouts
 * 
 * This class provides efficient caching of parsed C++ structures and
 * their calculated layouts. It uses LRU eviction policy and supports
 * dependency-aware invalidation.
 */
class StructureCache {
public:
    struct CacheEntry {
        std::shared_ptr<AST::StructDeclaration> structure;
        Layout::LayoutCalculator::StructLayout layout;
        std::chrono::steady_clock::time_point creationTime;
        std::chrono::steady_clock::time_point lastAccessTime;
        size_t accessCount = 0;
        size_t sourceHash = 0;
        std::vector<std::string> dependencies;
        
        CacheEntry() = default;
        
        CacheEntry(std::shared_ptr<AST::StructDeclaration> struct_ptr,
                  const Layout::LayoutCalculator::StructLayout& struct_layout,
                  size_t hash)
            : structure(std::move(struct_ptr))
            , layout(struct_layout)
            , creationTime(std::chrono::steady_clock::now())
            , lastAccessTime(creationTime)
            , sourceHash(hash) {}
        
        void updateAccess() {
            lastAccessTime = std::chrono::steady_clock::now();
            ++accessCount;
        }
        
        std::chrono::milliseconds getAge() const {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - creationTime);
        }
        
        std::chrono::milliseconds getTimeSinceAccess() const {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - lastAccessTime);
        }
    };
    
    struct UnionCacheEntry {
        std::shared_ptr<AST::UnionDeclaration> unionDecl;
        Layout::LayoutCalculator::UnionLayout layout;
        std::chrono::steady_clock::time_point creationTime;
        std::chrono::steady_clock::time_point lastAccessTime;
        size_t accessCount = 0;
        size_t sourceHash = 0;
        std::vector<std::string> dependencies;
        
        UnionCacheEntry() = default;
        
        UnionCacheEntry(std::shared_ptr<AST::UnionDeclaration> union_ptr,
                       const Layout::LayoutCalculator::UnionLayout& union_layout,
                       size_t hash)
            : unionDecl(std::move(union_ptr))
            , layout(union_layout)
            , creationTime(std::chrono::steady_clock::now())
            , lastAccessTime(creationTime)
            , sourceHash(hash) {}
        
        void updateAccess() {
            lastAccessTime = std::chrono::steady_clock::now();
            ++accessCount;
        }
    };
    
    explicit StructureCache(size_t maxSize = 1000);
    ~StructureCache() = default;
    
    // Cache operations
    void put(const std::string& name, 
             std::shared_ptr<AST::StructDeclaration> structure,
             const Layout::LayoutCalculator::StructLayout& layout,
             size_t sourceHash);
    
    void putUnion(const std::string& name,
                  std::shared_ptr<AST::UnionDeclaration> unionDecl,
                  const Layout::LayoutCalculator::UnionLayout& layout,
                  size_t sourceHash);
    
    bool get(const std::string& name, 
             std::shared_ptr<AST::StructDeclaration>& structure,
             Layout::LayoutCalculator::StructLayout& layout);
    
    bool getUnion(const std::string& name,
                  std::shared_ptr<AST::UnionDeclaration>& unionDecl,
                  Layout::LayoutCalculator::UnionLayout& layout);
    
    // Cache queries
    bool contains(const std::string& name) const;
    bool containsUnion(const std::string& name) const;
    bool isValid(const std::string& name, size_t sourceHash) const;
    bool isUnionValid(const std::string& name, size_t sourceHash) const;
    
    // Cache management
    void invalidate(const std::string& name);
    void invalidateUnion(const std::string& name);
    void invalidateAll();
    void invalidateDependents(const std::string& changedStructName);
    
    // Size management
    void setMaxSize(size_t maxSize);
    size_t getMaxSize() const { return m_maxSize; }
    size_t getCurrentSize() const;
    void cleanup(); // Remove expired/least used entries
    void evictLRU(); // Evict least recently used entry
    
    // Statistics and diagnostics
    struct CacheStatistics {
        size_t hitCount = 0;
        size_t missCount = 0;
        size_t evictionCount = 0;
        size_t invalidationCount = 0;
        size_t totalRequests = 0;
        size_t currentEntries = 0;
        size_t maxEntriesReached = 0;
        std::chrono::milliseconds totalAccessTime{0};
        std::chrono::milliseconds averageAccessTime{0};
        
        double getHitRatio() const {
            return totalRequests > 0 ? static_cast<double>(hitCount) / totalRequests : 0.0;
        }
        
        double getMissRatio() const {
            return totalRequests > 0 ? static_cast<double>(missCount) / totalRequests : 0.0;
        }
        
        void updateAccessTime(std::chrono::milliseconds accessTime) {
            totalAccessTime += accessTime;
            if (totalRequests > 0) {
                averageAccessTime = totalAccessTime / static_cast<long long>(totalRequests);
            }
        }
        
        void reset() {
            hitCount = 0;
            missCount = 0;
            evictionCount = 0;
            invalidationCount = 0;
            totalRequests = 0;
            currentEntries = 0;
            maxEntriesReached = 0;
            totalAccessTime = std::chrono::milliseconds{0};
            averageAccessTime = std::chrono::milliseconds{0};
        }
    };
    
    const CacheStatistics& getStatistics() const { return m_statistics; }
    void resetStatistics() { m_statistics.reset(); }
    
    // Configuration
    void setMaxAge(std::chrono::milliseconds maxAge) { m_maxAge = maxAge; }
    std::chrono::milliseconds getMaxAge() const { return m_maxAge; }
    
    void setMaxIdleTime(std::chrono::milliseconds maxIdleTime) { m_maxIdleTime = maxIdleTime; }
    std::chrono::milliseconds getMaxIdleTime() const { return m_maxIdleTime; }
    
    void setCleanupInterval(std::chrono::milliseconds interval) { m_cleanupInterval = interval; }
    std::chrono::milliseconds getCleanupInterval() const { return m_cleanupInterval; }
    
    // Dependency management
    void addDependency(const std::string& structName, const std::string& dependsOn);
    void removeDependency(const std::string& structName, const std::string& dependsOn);
    std::vector<std::string> getDependencies(const std::string& structName) const;
    std::vector<std::string> getDependents(const std::string& structName) const;
    bool hasCyclicDependency(const std::string& structName) const;
    
    // Bulk operations
    std::vector<std::string> getAllStructNames() const;
    std::vector<std::string> getAllUnionNames() const;
    void prefetchDependencies(const std::string& structName);
    
    // Memory management
    size_t getMemoryUsage() const;
    void compactMemory();
    
    // Debugging and diagnostics
    void printStatistics() const;
    void printCacheContents() const;
    std::string generateReport() const;
    
private:
    // Cache storage
    std::unordered_map<std::string, CacheEntry> m_structCache;
    std::unordered_map<std::string, UnionCacheEntry> m_unionCache;
    
    // LRU tracking
    std::list<std::string> m_structAccessOrder;
    std::list<std::string> m_unionAccessOrder;
    std::unordered_map<std::string, std::list<std::string>::iterator> m_structAccessMap;
    std::unordered_map<std::string, std::list<std::string>::iterator> m_unionAccessMap;
    
    // Dependency tracking
    std::unordered_map<std::string, std::unordered_set<std::string>> m_dependencies;     // name -> depends on
    std::unordered_map<std::string, std::unordered_set<std::string>> m_dependents;      // name -> depended by
    
    // Configuration
    size_t m_maxSize;
    std::chrono::milliseconds m_maxAge;
    std::chrono::milliseconds m_maxIdleTime;
    std::chrono::milliseconds m_cleanupInterval;
    std::chrono::steady_clock::time_point m_lastCleanup;
    
    // Statistics
    mutable CacheStatistics m_statistics;
    
    // Thread safety
    mutable std::shared_mutex m_mutex;
    
    // Helper methods
    void updateStructAccess(const std::string& name);
    void updateUnionAccess(const std::string& name);
    void addToStructLRU(const std::string& name);
    void addToUnionLRU(const std::string& name);
    void removeFromStructLRU(const std::string& name);
    void removeFromUnionLRU(const std::string& name);
    void moveToStructLRUFront(const std::string& name);
    void moveToUnionLRUFront(const std::string& name);
    
    // Eviction helpers
    bool shouldEvict() const;
    std::string selectStructForEviction() const;
    std::string selectUnionForEviction() const;
    void evictStruct(const std::string& name);
    void evictUnion(const std::string& name);
    
    // Cleanup helpers
    bool isExpired(const CacheEntry& entry) const;
    bool isExpired(const UnionCacheEntry& entry) const;
    bool isIdle(const CacheEntry& entry) const;
    bool isIdle(const UnionCacheEntry& entry) const;
    void removeExpiredEntries();
    bool shouldRunCleanup() const;
    
    // Dependency helpers
    void updateDependencyGraph(const std::string& name, const std::vector<std::string>& deps);
    void removeDependencyNode(const std::string& name);
    bool hasCyclicDependencyHelper(const std::string& current, 
                                  const std::string& target,
                                  std::unordered_set<std::string>& visited) const;
    
    // Statistics helpers
    void recordHit() const;
    void recordMiss() const;
    void recordEviction() const;
    void recordInvalidation() const;
    void updateCurrentSize() const;
    
    // Memory estimation
    size_t estimateStructSize(const CacheEntry& entry) const;
    size_t estimateUnionSize(const UnionCacheEntry& entry) const;
    
    // Validation
    void validateCacheConsistency() const;
};

} // namespace Parser
} // namespace Monitor