#pragma once

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QAtomicInteger>
#include <cstddef>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Monitor {
namespace Memory {

class MemoryPool : public QObject
{
    Q_OBJECT

public:
    explicit MemoryPool(size_t blockSize, size_t blockCount, QObject* parent = nullptr);
    ~MemoryPool() override;

    void* allocate();
    void deallocate(void* ptr);
    
    size_t getBlockSize() const { return m_blockSize; }
    size_t getBlockCount() const { return m_blockCount; }
    size_t getUsedBlocks() const { return m_usedBlocks.loadRelaxed(); }
    size_t getAvailableBlocks() const { return m_blockCount - m_usedBlocks.loadRelaxed(); }
    
    double getUtilization() const;
    void reset();
    
    bool isValidPointer(void* ptr) const;

signals:
    void memoryPressure(double utilization);
    void allocationFailed();

private:
    struct Block {
        Block* next;
    };
    
    void initializePool();
    
    const size_t m_blockSize;
    const size_t m_blockCount;
    
    std::unique_ptr<char[]> m_pool;
    Block* m_freeList;
    
    mutable QMutex m_mutex;
    QAtomicInteger<size_t> m_usedBlocks;
    
    char* m_poolStart;
    char* m_poolEnd;
    
public:
    static constexpr double PRESSURE_THRESHOLD = 0.8;
};

class MemoryPoolManager : public QObject
{
    Q_OBJECT

public:
    explicit MemoryPoolManager(QObject* parent = nullptr);
    ~MemoryPoolManager() override;
    
    MemoryPool* createPool(const QString& name, size_t blockSize, size_t blockCount);
    MemoryPool* getPool(const QString& name);
    void removePool(const QString& name);
    
    void* allocate(const QString& poolName);
    void deallocate(const QString& poolName, void* ptr);
    
    QStringList getPoolNames() const;
    double getTotalUtilization() const;
    size_t getTotalMemoryUsed() const;

signals:
    void poolCreated(const QString& name);
    void poolRemoved(const QString& name);
    void globalMemoryPressure(double utilization);

private slots:
    void onPoolMemoryPressure(double utilization);

private:
    std::unordered_map<std::string, std::unique_ptr<MemoryPool>> m_pools;
    mutable QMutex m_poolsMutex;
};

} // namespace Memory
} // namespace Monitor