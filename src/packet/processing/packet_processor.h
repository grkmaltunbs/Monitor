#pragma once

#include "../core/packet.h"
#include "field_extractor.h"
#include "data_transformer.h"
#include "statistics_calculator.h"
#include "../../parser/manager/structure_manager.h"
#include "../../threading/thread_pool.h"
#include "../../events/event_dispatcher.h"
#include "../../logging/logger.h"
#include "../../profiling/profiler.h"

#include <QtCore/QObject>
#include <QString>
#include <memory>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <future>

namespace Monitor {
namespace Packet {

/**
 * @brief Main packet processing pipeline coordinator
 * 
 * This class orchestrates the complete packet processing pipeline,
 * including field extraction, data transformation, and statistics
 * calculation. It provides a high-level interface for processing
 * packets and delivers results to subscribers.
 */
class PacketProcessor : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Processing configuration
     */
    struct Configuration {
        bool enableFieldExtraction;      ///< Enable field extraction
        bool enableTransformation;       ///< Enable data transformation
        bool enableStatistics;           ///< Enable statistics calculation
        bool enableParallelProcessing;   ///< Use thread pool for processing
        bool enableResultCaching;       ///< Cache processing results
        uint32_t maxCacheSize;          ///< Maximum cache entries
        uint32_t processingTimeoutMs;    ///< Processing timeout per packet
        
        // Component configurations
        StatisticsCalculator::Configuration statisticsConfig;
        
        Configuration() 
            : enableFieldExtraction(true)
            , enableTransformation(true)
            , enableStatistics(true)
            , enableParallelProcessing(true)
            , enableResultCaching(false)
            , maxCacheSize(1000)
            , processingTimeoutMs(100)
        {}
    };
    
    /**
     * @brief Processing result for a packet
     */
    struct ProcessingResult {
        PacketPtr packet;                       ///< Original packet
        std::unordered_map<std::string, FieldExtractor::ExtractionResult> extractedFields;
        std::unordered_map<std::string, DataTransformer::TransformationResult> transformedFields;
        std::chrono::nanoseconds processingTime{0};
        bool success = false;
        std::string error;
        
        ProcessingResult() = default;
        ProcessingResult(PacketPtr pkt) : packet(pkt), success(true) {}
        ProcessingResult(PacketPtr pkt, const std::string& err) : packet(pkt), success(false), error(err) {}
    };
    
    /**
     * @brief Processing statistics
     */
    struct Statistics {
        std::atomic<uint64_t> packetsProcessed{0};
        std::atomic<uint64_t> packetsDropped{0};
        std::atomic<uint64_t> processingFailures{0};
        std::atomic<uint64_t> averageProcessingTimeNs{0};
        std::atomic<uint64_t> maxProcessingTimeNs{0};
        std::atomic<uint64_t> cacheHits{0};
        std::atomic<uint64_t> cacheMisses{0};
        
        std::chrono::steady_clock::time_point startTime;
        
        Statistics() : startTime(std::chrono::steady_clock::now()) {}
        
        // Copy constructor
        Statistics(const Statistics& other) : startTime(other.startTime) {
            packetsProcessed.store(other.packetsProcessed.load());
            packetsDropped.store(other.packetsDropped.load());
            processingFailures.store(other.processingFailures.load());
            averageProcessingTimeNs.store(other.averageProcessingTimeNs.load());
            maxProcessingTimeNs.store(other.maxProcessingTimeNs.load());
            cacheHits.store(other.cacheHits.load());
            cacheMisses.store(other.cacheMisses.load());
        }
        
        // Assignment operator
        Statistics& operator=(const Statistics& other) {
            if (this != &other) {
                packetsProcessed.store(other.packetsProcessed.load());
                packetsDropped.store(other.packetsDropped.load());
                processingFailures.store(other.processingFailures.load());
                averageProcessingTimeNs.store(other.averageProcessingTimeNs.load());
                maxProcessingTimeNs.store(other.maxProcessingTimeNs.load());
                cacheHits.store(other.cacheHits.load());
                cacheMisses.store(other.cacheMisses.load());
                startTime = other.startTime;
            }
            return *this;
        }
        
        double getProcessingRate() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed == 0) return 0.0;
            return static_cast<double>(packetsProcessed.load()) / elapsed;
        }
        
        double getFailureRate() const {
            uint64_t total = packetsProcessed.load();
            if (total == 0) return 0.0;
            return static_cast<double>(processingFailures.load()) / total;
        }
        
        double getCacheHitRate() const {
            uint64_t totalRequests = cacheHits.load() + cacheMisses.load();
            if (totalRequests == 0) return 0.0;
            return static_cast<double>(cacheHits.load()) / totalRequests;
        }
    };
    
    /**
     * @brief Field processing configuration for specific packet types
     */
    struct FieldProcessingConfig {
        std::vector<std::string> fieldsToExtract;   ///< Fields to extract (empty = all)
        std::vector<std::string> fieldsToTransform; ///< Fields to transform (empty = all)
        bool enableStatistics = true;               ///< Enable statistics for this packet type
        
        FieldProcessingConfig() = default;
        FieldProcessingConfig(const std::vector<std::string>& extract, 
                            const std::vector<std::string>& transform)
            : fieldsToExtract(extract), fieldsToTransform(transform) {}
    };
    
    /**
     * @brief Result callback type
     */
    using ResultCallback = std::function<void(const ProcessingResult& result)>;

private:
    Configuration m_config;
    
    // Processing components
    std::unique_ptr<FieldExtractor> m_fieldExtractor;
    std::unique_ptr<DataTransformer> m_dataTransformer;
    std::unique_ptr<StatisticsCalculator> m_statisticsCalculator;
    
    // External dependencies
    Parser::StructureManager* m_structureManager;
    Threading::ThreadPool* m_threadPool;
    Events::EventDispatcher* m_eventDispatcher;
    Logging::Logger* m_logger;
    Profiling::Profiler* m_profiler;
    
    // Field processing configuration per packet type
    std::unordered_map<PacketId, FieldProcessingConfig> m_fieldConfigs;
    mutable std::shared_mutex m_configMutex;
    
    // Result callbacks
    std::vector<ResultCallback> m_resultCallbacks;
    mutable std::shared_mutex m_callbackMutex;
    
    // Results cache (if enabled)
    std::unordered_map<size_t, ProcessingResult> m_resultCache; // hash -> result
    mutable std::shared_mutex m_cacheMutex;
    
    // Statistics
    Statistics m_stats;
    
    // Processing state
    std::atomic<bool> m_initialized{false};

public:
    explicit PacketProcessor(const Configuration& config = Configuration(), QObject* parent = nullptr)
        : QObject(parent)
        , m_config(config)
        , m_structureManager(nullptr)
        , m_threadPool(nullptr)
        , m_eventDispatcher(nullptr)
        , m_logger(Logging::Logger::instance())
        , m_profiler(Profiling::Profiler::instance())
    {
        // Create processing components
        m_fieldExtractor = std::make_unique<FieldExtractor>();
        m_dataTransformer = std::make_unique<DataTransformer>();
        m_statisticsCalculator = std::make_unique<StatisticsCalculator>(m_config.statisticsConfig);
    }
    
    /**
     * @brief Initialize processor with external dependencies
     */
    bool initialize(Parser::StructureManager* structureManager,
                   Threading::ThreadPool* threadPool = nullptr,
                   Events::EventDispatcher* eventDispatcher = nullptr) {
        
        if (!structureManager) {
            m_logger->error("PacketProcessor", "Structure manager is required");
            return false;
        }
        
        m_structureManager = structureManager;
        m_threadPool = threadPool;
        m_eventDispatcher = eventDispatcher;
        
        // Initialize field extractor with known structures
        initializeFieldMaps();
        
        m_initialized = true;
        m_stats.startTime = std::chrono::steady_clock::now();
        
        m_logger->info("PacketProcessor", "Packet processor initialized");
        
        return true;
    }
    
    /**
     * @brief Process single packet
     */
    ProcessingResult processPacket(PacketPtr packet) {
        if (!packet || !packet->isValid()) {
            m_stats.packetsDropped++;
            return ProcessingResult(packet, "Invalid packet");
        }
        
        if (!m_initialized.load()) {
            m_stats.packetsDropped++;
            return ProcessingResult(packet, "Processor not initialized");
        }
        
        PROFILE_SCOPE("PacketProcessor::processPacket");
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Check cache if enabled
        if (m_config.enableResultCaching) {
            auto cachedResult = getCachedResult(packet);
            if (cachedResult.has_value()) {
                m_stats.cacheHits++;
                return cachedResult.value();
            }
            m_stats.cacheMisses++;
        }
        
        ProcessingResult result = processPacketInternal(packet);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto processingTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
        result.processingTime = processingTime;
        
        // Update statistics
        updateProcessingStatistics(processingTime, result.success);
        
        // Cache result if enabled
        if (m_config.enableResultCaching && result.success) {
            cacheResult(packet, result);
        }
        
        // Notify callbacks
        notifyResultCallbacks(result);
        
        return result;
    }
    
    /**
     * @brief Process packet asynchronously (if thread pool available)
     */
    std::future<ProcessingResult> processPacketAsync(PacketPtr packet) {
        if (!m_threadPool || !m_config.enableParallelProcessing) {
            // Process synchronously
            std::promise<ProcessingResult> promise;
            promise.set_value(processPacket(packet));
            return promise.get_future();
        }
        
        // Submit to thread pool
        return m_threadPool->submitTask([this, packet]() -> ProcessingResult {
            return processPacket(packet);
        });
    }
    
    /**
     * @brief Set field processing configuration for packet type
     */
    void setFieldProcessingConfig(PacketId packetId, const FieldProcessingConfig& config) {
        std::unique_lock lock(m_configMutex);
        m_fieldConfigs[packetId] = config;
        
        m_logger->debug("PacketProcessor", 
            QString("Set field config for packet ID %1: extract %2 fields, transform %3 fields")
                .arg(packetId).arg(config.fieldsToExtract.size()).arg(config.fieldsToTransform.size()));
    }
    
    /**
     * @brief Add data transformation for field
     */
    void addTransformation(const std::string& fieldName, const DataTransformer::Transformation& transformation) {
        m_dataTransformer->addTransformation(fieldName, transformation);
    }
    
    /**
     * @brief Add transformation chain for field
     */
    void addTransformationChain(const std::string& fieldName, const std::vector<DataTransformer::Transformation>& transformations) {
        m_dataTransformer->addTransformationChain(fieldName, transformations);
    }
    
    /**
     * @brief Add result callback
     */
    void addResultCallback(const ResultCallback& callback) {
        std::unique_lock lock(m_callbackMutex);
        m_resultCallbacks.push_back(callback);
    }
    
    /**
     * @brief Get field extractor
     */
    FieldExtractor* getFieldExtractor() const {
        return m_fieldExtractor.get();
    }
    
    /**
     * @brief Get data transformer
     */
    DataTransformer* getDataTransformer() const {
        return m_dataTransformer.get();
    }
    
    /**
     * @brief Get statistics calculator
     */
    StatisticsCalculator* getStatisticsCalculator() const {
        return m_statisticsCalculator.get();
    }
    
    /**
     * @brief Get processor statistics
     */
    const Statistics& getStatistics() const {
        return m_stats;
    }
    
    /**
     * @brief Reset processor statistics
     */
    void resetStatistics() {
        m_stats = Statistics();
        if (m_statisticsCalculator) {
            m_statisticsCalculator->resetAllStatistics();
        }
    }
    
    /**
     * @brief Clear result cache
     */
    void clearCache() {
        std::unique_lock lock(m_cacheMutex);
        m_resultCache.clear();
        m_logger->debug("PacketProcessor", "Result cache cleared");
    }

signals:
    /**
     * @brief Emitted when packet processing is complete
     */
    void packetProcessed(const ProcessingResult& result);
    
    /**
     * @brief Emitted when processing fails
     */
    void processingFailed(PacketPtr packet, const QString& error);
    
    /**
     * @brief Emitted when statistics are updated
     */
    void statisticsUpdated(const Statistics& stats);

private:
    /**
     * @brief Internal packet processing implementation
     */
    ProcessingResult processPacketInternal(PacketPtr packet) {
        ProcessingResult result(packet);
        
        try {
            // Get field processing configuration
            auto config = getFieldProcessingConfig(packet->id());
            
            // Step 1: Field extraction
            if (m_config.enableFieldExtraction) {
                if (config.fieldsToExtract.empty()) {
                    result.extractedFields = m_fieldExtractor->extractAllFields(packet);
                } else {
                    result.extractedFields = m_fieldExtractor->extractFields(packet, config.fieldsToExtract);
                }
            }
            
            // Step 2: Data transformation
            if (m_config.enableTransformation) {
                std::vector<std::string> fieldsToTransform = config.fieldsToTransform;
                if (fieldsToTransform.empty()) {
                    // Transform all extracted fields
                    for (const auto& pair : result.extractedFields) {
                        fieldsToTransform.push_back(pair.first);
                    }
                }
                
                for (const auto& fieldName : fieldsToTransform) {
                    auto extractIt = result.extractedFields.find(fieldName);
                    if (extractIt != result.extractedFields.end() && extractIt->second.success) {
                        result.transformedFields[fieldName] = m_dataTransformer->transform(fieldName, extractIt->second.value);
                    }
                }
            }
            
            // Step 3: Statistics update
            if (m_config.enableStatistics && config.enableStatistics) {
                m_statisticsCalculator->updateStatistics(result.extractedFields);
            }
            
            result.success = true;
            
        } catch (const std::exception& e) {
            result.success = false;
            result.error = "Processing exception: " + std::string(e.what());
            m_logger->error("PacketProcessor", 
                QString("Processing error for packet ID %1: %2")
                .arg(packet->id()).arg(QString::fromStdString(e.what())));
        } catch (...) {
            result.success = false;
            result.error = "Unknown processing error";
            m_logger->error("PacketProcessor", QString("Unknown processing error for packet ID %1").arg(packet->id()));
        }
        
        return result;
    }
    
    /**
     * @brief Get field processing configuration
     */
    FieldProcessingConfig getFieldProcessingConfig(PacketId packetId) const {
        std::shared_lock lock(m_configMutex);
        auto it = m_fieldConfigs.find(packetId);
        return (it != m_fieldConfigs.end()) ? it->second : FieldProcessingConfig();
    }
    
    /**
     * @brief Initialize field maps from structure manager
     */
    void initializeFieldMaps() {
        if (!m_structureManager) {
            return;
        }
        
        // Get all packet structures and build field maps
        auto structureNames = m_structureManager->getStructureNames();
        for (const auto& name : structureNames) {
            auto structure = m_structureManager->getStructure(name);
            if (structure) {
                // For this implementation, we'll assume packet ID is derived from structure name
                // In a real implementation, there would be a mapping system
                PacketId packetId = std::hash<std::string>{}(name) % 10000; // Simple hash
                m_fieldExtractor->buildFieldMap(packetId, structure);
                
                m_logger->debug("PacketProcessor", 
                    QString("Built field map for structure %1 (packet ID %2)")
                        .arg(name).arg(packetId));
            }
        }
    }
    
    /**
     * @brief Get cached processing result
     */
    std::optional<ProcessingResult> getCachedResult(PacketPtr packet) const {
        std::shared_lock lock(m_cacheMutex);
        
        // Create hash from packet data
        size_t hash = std::hash<std::string>{}(std::string(reinterpret_cast<const char*>(packet->data()), packet->totalSize()));
        
        auto it = m_resultCache.find(hash);
        if (it != m_resultCache.end()) {
            return it->second;
        }
        
        return std::nullopt;
    }
    
    /**
     * @brief Cache processing result
     */
    void cacheResult(PacketPtr packet, const ProcessingResult& result) {
        std::unique_lock lock(m_cacheMutex);
        
        if (m_resultCache.size() >= m_config.maxCacheSize) {
            // Simple eviction: remove first entry
            auto it = m_resultCache.begin();
            m_resultCache.erase(it);
        }
        
        size_t hash = std::hash<std::string>{}(std::string(reinterpret_cast<const char*>(packet->data()), packet->totalSize()));
        m_resultCache[hash] = result;
    }
    
    /**
     * @brief Update processing statistics
     */
    void updateProcessingStatistics(std::chrono::nanoseconds processingTime, bool success) {
        if (success) {
            m_stats.packetsProcessed++;
        } else {
            m_stats.processingFailures++;
        }
        
        uint64_t timeNs = static_cast<uint64_t>(processingTime.count());
        
        // Update average processing time
        uint64_t currentAvg = m_stats.averageProcessingTimeNs.load();
        uint64_t newAvg = (currentAvg + timeNs) / 2;
        m_stats.averageProcessingTimeNs.store(newAvg);
        
        // Update max processing time
        uint64_t currentMax = m_stats.maxProcessingTimeNs.load();
        while (timeNs > currentMax && !m_stats.maxProcessingTimeNs.compare_exchange_weak(currentMax, timeNs)) {
            // Retry if another thread modified max
        }
        
        // Emit statistics update periodically
        if (m_stats.packetsProcessed % 100 == 0) {
            emit statisticsUpdated(m_stats);
        }
    }
    
    /**
     * @brief Notify result callbacks
     */
    void notifyResultCallbacks(const ProcessingResult& result) {
        std::shared_lock lock(m_callbackMutex);
        
        for (const auto& callback : m_resultCallbacks) {
            try {
                callback(result);
            } catch (const std::exception& e) {
                m_logger->warning("PacketProcessor", 
                    QString("Exception in result callback: %1").arg(QString::fromStdString(e.what())));
            } catch (...) {
                m_logger->warning("PacketProcessor", "Unknown exception in result callback");
            }
        }
        
        // Emit signal
        emit packetProcessed(result);
        
        if (!result.success) {
            emit processingFailed(result.packet, QString::fromStdString(result.error));
        }
    }
};

} // namespace Packet
} // namespace Monitor