#include "memory_pool.h"
#include <QtCore/QLoggingCategory>
#include <QtCore/QDebug>
#include <algorithm>
#include <cstring>

Q_LOGGING_CATEGORY(memoryPool, "Monitor.Memory.Pool")

namespace Monitor {
namespace Memory {

MemoryPool::MemoryPool(size_t blockSize, size_t blockCount, QObject* parent)
    : QObject(parent)
    , m_blockSize(std::max(blockSize, sizeof(Block)))
    , m_blockCount(blockCount)
    , m_pool(nullptr)
    , m_freeList(nullptr)
    , m_usedBlocks(0)
    , m_poolStart(nullptr)
    , m_poolEnd(nullptr)
{
    if (m_blockCount == 0) {
        qCWarning(memoryPool) << "Block count cannot be zero";
        return;
    }
    
    initializePool();
    qCInfo(memoryPool) << "Created memory pool: blockSize =" << m_blockSize 
                       << "blockCount =" << m_blockCount 
                       << "total size =" << (m_blockSize * m_blockCount) << "bytes";
}

MemoryPool::~MemoryPool()
{
    if (m_usedBlocks.loadRelaxed() > 0) {
        qCWarning(memoryPool) << "Memory pool destroyed with" << m_usedBlocks.loadRelaxed() << "blocks still allocated";
    }
}

void MemoryPool::initializePool()
{
    const size_t totalSize = m_blockSize * m_blockCount;
    m_pool = std::make_unique<char[]>(totalSize);
    
    if (!m_pool) {
        qCCritical(memoryPool) << "Failed to allocate memory pool of size" << totalSize;
        return;
    }
    
    m_poolStart = m_pool.get();
    m_poolEnd = m_poolStart + totalSize;
    
    std::memset(m_poolStart, 0, totalSize);
    
    m_freeList = nullptr;
    char* current = m_poolStart;
    
    for (size_t i = 0; i < m_blockCount; ++i) {
        Block* block = reinterpret_cast<Block*>(current);
        block->next = m_freeList;
        m_freeList = block;
        current += m_blockSize;
    }
}

void* MemoryPool::allocate()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_freeList) {
        emit allocationFailed();
        qCWarning(memoryPool) << "Memory pool exhausted";
        return nullptr;
    }
    
    Block* block = m_freeList;
    m_freeList = m_freeList->next;
    
    m_usedBlocks.fetchAndAddRelaxed(1);
    
    const double utilization = getUtilization();
    if (utilization >= PRESSURE_THRESHOLD) {
        emit memoryPressure(utilization);
    }
    
    std::memset(block, 0, m_blockSize);
    return block;
}

void MemoryPool::deallocate(void* ptr)
{
    if (!ptr) {
        return;
    }
    
    if (!isValidPointer(ptr)) {
        qCWarning(memoryPool) << "Attempting to deallocate invalid pointer";
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    Block* block = static_cast<Block*>(ptr);
    block->next = m_freeList;
    m_freeList = block;
    
    m_usedBlocks.fetchAndSubRelaxed(1);
}

double MemoryPool::getUtilization() const
{
    if (m_blockCount == 0) {
        return 0.0;
    }
    return static_cast<double>(m_usedBlocks.loadRelaxed()) / static_cast<double>(m_blockCount);
}

void MemoryPool::reset()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_usedBlocks.loadRelaxed() > 0) {
        qCWarning(memoryPool) << "Resetting pool with" << m_usedBlocks.loadRelaxed() << "blocks still allocated";
    }
    
    initializePool();
    m_usedBlocks.storeRelaxed(0);
}

bool MemoryPool::isValidPointer(void* ptr) const
{
    if (!ptr || !m_poolStart || !m_poolEnd) {
        return false;
    }
    
    char* charPtr = static_cast<char*>(ptr);
    if (charPtr < m_poolStart || charPtr >= m_poolEnd) {
        return false;
    }
    
    const size_t offset = charPtr - m_poolStart;
    return (offset % m_blockSize) == 0;
}

MemoryPoolManager::MemoryPoolManager(QObject* parent)
    : QObject(parent)
{
    qCInfo(memoryPool) << "Memory pool manager created";
}

MemoryPoolManager::~MemoryPoolManager()
{
    QMutexLocker locker(&m_poolsMutex);
    m_pools.clear();
    qCInfo(memoryPool) << "Memory pool manager destroyed";
}

MemoryPool* MemoryPoolManager::createPool(const QString& name, size_t blockSize, size_t blockCount)
{
    QMutexLocker locker(&m_poolsMutex);
    
    std::string stdName = name.toStdString();
    auto it = m_pools.find(stdName);
    if (it != m_pools.end()) {
        qCWarning(memoryPool) << "Pool with name" << name << "already exists";
        return it->second.get();
    }
    
    auto pool = std::make_unique<MemoryPool>(blockSize, blockCount, this);
    MemoryPool* poolPtr = pool.get();
    
    connect(poolPtr, &MemoryPool::memoryPressure,
            this, &MemoryPoolManager::onPoolMemoryPressure, Qt::QueuedConnection);
    
    m_pools[stdName] = std::move(pool);
    
    emit poolCreated(name);
    qCInfo(memoryPool) << "Created pool" << name << "with blockSize =" << blockSize 
                       << "blockCount =" << blockCount;
    
    return poolPtr;
}

MemoryPool* MemoryPoolManager::getPool(const QString& name)
{
    QMutexLocker locker(&m_poolsMutex);
    auto it = m_pools.find(name.toStdString());
    return (it != m_pools.end()) ? it->second.get() : nullptr;
}

void MemoryPoolManager::removePool(const QString& name)
{
    QMutexLocker locker(&m_poolsMutex);
    
    auto it = m_pools.find(name.toStdString());
    if (it != m_pools.end()) {
        m_pools.erase(it);
        emit poolRemoved(name);
        qCInfo(memoryPool) << "Removed pool" << name;
    }
}

void* MemoryPoolManager::allocate(const QString& poolName)
{
    MemoryPool* pool = getPool(poolName);
    return pool ? pool->allocate() : nullptr;
}

void MemoryPoolManager::deallocate(const QString& poolName, void* ptr)
{
    MemoryPool* pool = getPool(poolName);
    if (pool) {
        pool->deallocate(ptr);
    }
}

QStringList MemoryPoolManager::getPoolNames() const
{
    QMutexLocker locker(&m_poolsMutex);
    QStringList names;
    for (const auto& pair : m_pools) {
        names.append(QString::fromStdString(pair.first));
    }
    return names;
}

double MemoryPoolManager::getTotalUtilization() const
{
    QMutexLocker locker(&m_poolsMutex);
    
    if (m_pools.empty()) {
        return 0.0;
    }
    
    size_t totalUsed = 0;
    size_t totalAvailable = 0;
    
    for (const auto& pair : m_pools) {
        totalUsed += pair.second->getUsedBlocks();
        totalAvailable += pair.second->getBlockCount();
    }
    
    return totalAvailable > 0 ? static_cast<double>(totalUsed) / static_cast<double>(totalAvailable) : 0.0;
}

size_t MemoryPoolManager::getTotalMemoryUsed() const
{
    QMutexLocker locker(&m_poolsMutex);
    
    size_t total = 0;
    for (const auto& pair : m_pools) {
        total += pair.second->getUsedBlocks() * pair.second->getBlockSize();
    }
    
    return total;
}

void MemoryPoolManager::onPoolMemoryPressure(double /* utilization */)
{
    const double totalUtilization = getTotalUtilization();
    if (totalUtilization >= MemoryPool::PRESSURE_THRESHOLD) {
        emit globalMemoryPressure(totalUtilization);
    }
}

} // namespace Memory
} // namespace Monitor

