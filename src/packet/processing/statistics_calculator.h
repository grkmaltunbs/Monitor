#pragma once

#include "field_extractor.h"
#include "../../logging/logger.h"
#include "../../profiling/profiler.h"

#include <QString>
#include <memory>
#include <unordered_map>
#include <vector>
#include <deque>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <atomic>
#include <shared_mutex>
#include <mutex>

namespace Monitor {
namespace Packet {

/**
 * @brief Real-time statistics calculator for packet field values
 * 
 * This class provides running statistical calculations for field values,
 * maintaining efficient incremental updates and supporting windowed
 * statistics with configurable window sizes.
 */
class StatisticsCalculator {
public:
    /**
     * @brief Statistical metrics for a field
     */
    struct FieldStatistics {
        // Basic statistics
        std::atomic<uint64_t> sampleCount{0};
        std::atomic<double> sum{0.0};
        std::atomic<double> sumSquared{0.0};
        std::atomic<double> min{std::numeric_limits<double>::max()};
        std::atomic<double> max{std::numeric_limits<double>::lowest()};
        
        // Current values
        std::atomic<double> current{0.0};
        std::atomic<double> previous{0.0};
        
        // Computed statistics (updated periodically)
        std::atomic<double> mean{0.0};
        std::atomic<double> variance{0.0};
        std::atomic<double> standardDeviation{0.0};
        std::atomic<double> range{0.0};
        
        // Rate statistics
        std::atomic<double> rate{0.0};              ///< Samples per second
        std::atomic<uint64_t> lastUpdateTime{0};    ///< Nanoseconds since epoch
        
        // Timestamps
        std::chrono::steady_clock::time_point firstSample;
        std::chrono::steady_clock::time_point lastSample;
        
        FieldStatistics() {
            auto now = std::chrono::steady_clock::now();
            firstSample = now;
            lastSample = now;
        }
        
        // Copy constructor
        FieldStatistics(const FieldStatistics& other) {
            sampleCount.store(other.sampleCount.load());
            sum.store(other.sum.load());
            sumSquared.store(other.sumSquared.load());
            min.store(other.min.load());
            max.store(other.max.load());
            current.store(other.current.load());
            previous.store(other.previous.load());
            mean.store(other.mean.load());
            variance.store(other.variance.load());
            standardDeviation.store(other.standardDeviation.load());
            range.store(other.range.load());
            rate.store(other.rate.load());
            lastUpdateTime.store(other.lastUpdateTime.load());
            firstSample = other.firstSample;
            lastSample = other.lastSample;
        }
        
        // Assignment operator
        FieldStatistics& operator=(const FieldStatistics& other) {
            if (this != &other) {
                sampleCount.store(other.sampleCount.load());
                sum.store(other.sum.load());
                sumSquared.store(other.sumSquared.load());
                min.store(other.min.load());
                max.store(other.max.load());
                current.store(other.current.load());
                previous.store(other.previous.load());
                mean.store(other.mean.load());
                variance.store(other.variance.load());
                standardDeviation.store(other.standardDeviation.load());
                range.store(other.range.load());
                rate.store(other.rate.load());
                lastUpdateTime.store(other.lastUpdateTime.load());
                firstSample = other.firstSample;
                lastSample = other.lastSample;
            }
            return *this;
        }
        
        void reset() {
            sampleCount = 0;
            sum = 0.0;
            sumSquared = 0.0;
            min = std::numeric_limits<double>::max();
            max = std::numeric_limits<double>::lowest();
            current = 0.0;
            previous = 0.0;
            mean = 0.0;
            variance = 0.0;
            standardDeviation = 0.0;
            range = 0.0;
            rate = 0.0;
            lastUpdateTime = 0;
            
            auto now = std::chrono::steady_clock::now();
            firstSample = now;
            lastSample = now;
        }
    };
    
    /**
     * @brief Windowed statistics for moving calculations
     */
    struct WindowedStatistics {
        std::deque<double> values;
        std::deque<std::chrono::steady_clock::time_point> timestamps;
        size_t maxWindowSize;
        std::chrono::milliseconds timeWindow;
        
        // Computed windowed statistics
        double windowMean = 0.0;
        double windowMin = std::numeric_limits<double>::max();
        double windowMax = std::numeric_limits<double>::lowest();
        double windowStdDev = 0.0;
        double windowMedian = 0.0;
        
        WindowedStatistics(size_t maxSize = 1000, std::chrono::milliseconds timeWin = std::chrono::milliseconds(60000))
            : maxWindowSize(maxSize), timeWindow(timeWin) {}
        
        void addValue(double value, std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now()) {
            values.push_back(value);
            timestamps.push_back(timestamp);
            
            // Remove old values based on window size
            while (values.size() > maxWindowSize) {
                values.pop_front();
                timestamps.pop_front();
            }
            
            // Remove old values based on time window
            auto cutoffTime = timestamp - timeWindow;
            while (!timestamps.empty() && timestamps.front() < cutoffTime) {
                values.pop_front();
                timestamps.pop_front();
            }
            
            updateWindowStatistics();
        }
        
        void updateWindowStatistics() {
            if (values.empty()) return;
            
            // Calculate mean
            double sum = 0.0;
            windowMin = std::numeric_limits<double>::max();
            windowMax = std::numeric_limits<double>::lowest();
            
            for (double val : values) {
                sum += val;
                windowMin = std::min(windowMin, val);
                windowMax = std::max(windowMax, val);
            }
            
            windowMean = sum / values.size();
            
            // Calculate standard deviation
            double variance = 0.0;
            for (double val : values) {
                double diff = val - windowMean;
                variance += diff * diff;
            }
            windowStdDev = std::sqrt(variance / values.size());
            
            // Calculate median
            std::vector<double> sortedValues(values.begin(), values.end());
            std::sort(sortedValues.begin(), sortedValues.end());
            size_t mid = sortedValues.size() / 2;
            if (sortedValues.size() % 2 == 0) {
                windowMedian = (sortedValues[mid - 1] + sortedValues[mid]) / 2.0;
            } else {
                windowMedian = sortedValues[mid];
            }
        }
        
        void clear() {
            values.clear();
            timestamps.clear();
            windowMean = 0.0;
            windowMin = std::numeric_limits<double>::max();
            windowMax = std::numeric_limits<double>::lowest();
            windowStdDev = 0.0;
            windowMedian = 0.0;
        }
    };
    
    /**
     * @brief Configuration for statistics calculation
     */
    struct Configuration {
        bool enableWindowed;             ///< Enable windowed statistics
        size_t windowSize;               ///< Maximum window size
        std::chrono::milliseconds timeWindow; ///< Time window (1 minute)
        bool enablePercentiles;        ///< Enable percentile calculations
        std::vector<double> percentiles; ///< Percentiles to calculate
        uint32_t updateIntervalMs;      ///< Statistics update interval
        
        Configuration() 
            : enableWindowed(true)
            , windowSize(1000)
            , timeWindow(std::chrono::milliseconds(60000))
            , enablePercentiles(false)
            , percentiles({25.0, 50.0, 75.0, 90.0, 95.0, 99.0})
            , updateIntervalMs(1000)
        {}
    };

private:
    Configuration m_config;
    
    // Field statistics
    std::unordered_map<std::string, FieldStatistics> m_fieldStats;
    std::unordered_map<std::string, WindowedStatistics> m_windowedStats;
    mutable std::shared_mutex m_statsMutex;
    
    // Update tracking
    std::chrono::steady_clock::time_point m_lastUpdate;
    std::atomic<uint64_t> m_totalSamples{0};
    
    // Utilities
    Logging::Logger* m_logger;
    Profiling::Profiler* m_profiler;

public:
    explicit StatisticsCalculator(const Configuration& config = Configuration())
        : m_config(config)
        , m_lastUpdate(std::chrono::steady_clock::now())
        , m_logger(Logging::Logger::instance())
        , m_profiler(Profiling::Profiler::instance())
    {
    }
    
    /**
     * @brief Update statistics with new field value
     */
    void updateStatistics(const std::string& fieldName, const FieldExtractor::FieldValue& value) {
        // Convert value to double for calculations
        double numericValue = convertToNumeric(value);
        if (std::isnan(numericValue)) {
            return; // Skip non-numeric values
        }
        
        PROFILE_SCOPE("StatisticsCalculator::updateStatistics");
        
        auto now = std::chrono::steady_clock::now();
        
        {
            std::unique_lock lock(m_statsMutex);
            
            // Update basic statistics
            auto& stats = m_fieldStats[fieldName];
            updateBasicStatistics(stats, numericValue, now);
            
            // Update windowed statistics if enabled
            if (m_config.enableWindowed) {
                auto& windowedStats = m_windowedStats[fieldName];
                if (windowedStats.maxWindowSize == 0) {
                    // Initialize windowed statistics
                    windowedStats.maxWindowSize = m_config.windowSize;
                    windowedStats.timeWindow = m_config.timeWindow;
                }
                windowedStats.addValue(numericValue, now);
            }
        }
        
        m_totalSamples++;
        
        // Periodically update computed statistics
        if (now - m_lastUpdate > std::chrono::milliseconds(m_config.updateIntervalMs)) {
            updateComputedStatistics();
            m_lastUpdate = now;
        }
    }
    
    /**
     * @brief Update statistics with multiple field values
     */
    void updateStatistics(const std::unordered_map<std::string, FieldExtractor::ExtractionResult>& extractedValues) {
        PROFILE_SCOPE("StatisticsCalculator::updateMultipleStatistics");
        
        for (const auto& pair : extractedValues) {
            const std::string& fieldName = pair.first;
            const auto& extractionResult = pair.second;
            
            if (extractionResult.success) {
                updateStatistics(fieldName, extractionResult.value);
            }
        }
    }
    
    /**
     * @brief Get statistics for specific field
     */
    FieldStatistics getFieldStatistics(const std::string& fieldName) const {
        std::shared_lock lock(m_statsMutex);
        auto it = m_fieldStats.find(fieldName);
        return (it != m_fieldStats.end()) ? it->second : FieldStatistics();
    }
    
    /**
     * @brief Get windowed statistics for specific field
     */
    WindowedStatistics getWindowedStatistics(const std::string& fieldName) const {
        std::shared_lock lock(m_statsMutex);
        auto it = m_windowedStats.find(fieldName);
        return (it != m_windowedStats.end()) ? it->second : WindowedStatistics();
    }
    
    /**
     * @brief Get all field names with statistics
     */
    std::vector<std::string> getFieldNames() const {
        std::shared_lock lock(m_statsMutex);
        std::vector<std::string> names;
        names.reserve(m_fieldStats.size());
        
        for (const auto& pair : m_fieldStats) {
            names.push_back(pair.first);
        }
        
        return names;
    }
    
    /**
     * @brief Reset statistics for specific field
     */
    void resetFieldStatistics(const std::string& fieldName) {
        std::unique_lock lock(m_statsMutex);
        
        auto it = m_fieldStats.find(fieldName);
        if (it != m_fieldStats.end()) {
            it->second.reset();
        }
        
        auto windowIt = m_windowedStats.find(fieldName);
        if (windowIt != m_windowedStats.end()) {
            windowIt->second.clear();
        }
        
        m_logger->debug("StatisticsCalculator", QString("Reset statistics for field: %1").arg(QString::fromStdString(fieldName)));
    }
    
    /**
     * @brief Reset all statistics
     */
    void resetAllStatistics() {
        std::unique_lock lock(m_statsMutex);
        
        for (auto& pair : m_fieldStats) {
            pair.second.reset();
        }
        
        for (auto& pair : m_windowedStats) {
            pair.second.clear();
        }
        
        m_totalSamples = 0;
        m_lastUpdate = std::chrono::steady_clock::now();
        
        m_logger->info("StatisticsCalculator", "Reset all statistics");
    }
    
    /**
     * @brief Get total number of samples processed
     */
    uint64_t getTotalSamples() const {
        return m_totalSamples.load();
    }
    
    /**
     * @brief Get configuration
     */
    const Configuration& getConfiguration() const {
        return m_config;
    }
    
    /**
     * @brief Calculate percentile for field
     */
    double calculatePercentile(const std::string& fieldName, double percentile) const {
        std::shared_lock lock(m_statsMutex);
        
        auto it = m_windowedStats.find(fieldName);
        if (it == m_windowedStats.end() || it->second.values.empty()) {
            return 0.0;
        }
        
        const auto& values = it->second.values;
        std::vector<double> sortedValues(values.begin(), values.end());
        std::sort(sortedValues.begin(), sortedValues.end());
        
        double index = (percentile / 100.0) * (sortedValues.size() - 1);
        size_t lowerIndex = static_cast<size_t>(std::floor(index));
        size_t upperIndex = static_cast<size_t>(std::ceil(index));
        
        if (lowerIndex == upperIndex) {
            return sortedValues[lowerIndex];
        } else {
            double weight = index - lowerIndex;
            return sortedValues[lowerIndex] * (1.0 - weight) + sortedValues[upperIndex] * weight;
        }
    }
    
    /**
     * @brief Get summary statistics as string
     */
    std::string getStatisticsSummary(const std::string& fieldName) const {
        auto stats = getFieldStatistics(fieldName);
        
        std::ostringstream oss;
        oss << "Field: " << fieldName << "\n";
        oss << "  Samples: " << stats.sampleCount.load() << "\n";
        oss << "  Current: " << stats.current.load() << "\n";
        oss << "  Mean: " << stats.mean.load() << "\n";
        oss << "  Min: " << stats.min.load() << "\n";
        oss << "  Max: " << stats.max.load() << "\n";
        oss << "  StdDev: " << stats.standardDeviation.load() << "\n";
        oss << "  Rate: " << stats.rate.load() << " samples/sec";
        
        return oss.str();
    }

private:
    /**
     * @brief Convert field value to numeric
     */
    double convertToNumeric(const FieldExtractor::FieldValue& value) const {
        return std::visit([](const auto& val) -> double {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T>) {
                return static_cast<double>(val);
            } else if constexpr (std::is_same_v<T, std::string>) {
                try {
                    return std::stod(val);
                } catch (...) {
                    return std::numeric_limits<double>::quiet_NaN();
                }
            } else {
                return std::numeric_limits<double>::quiet_NaN();
            }
        }, value);
    }
    
    /**
     * @brief Update basic statistics for a field
     */
    void updateBasicStatistics(FieldStatistics& stats, double value, 
                              std::chrono::steady_clock::time_point timestamp) {
        // Update basic counters
        uint64_t count = stats.sampleCount.load();
        stats.sampleCount = count + 1;
        
        // Update sum and sum of squares
        double currentSum = stats.sum.load();
        stats.sum = currentSum + value;
        
        double currentSumSquared = stats.sumSquared.load();
        stats.sumSquared = currentSumSquared + (value * value);
        
        // Update min/max
        double currentMin = stats.min.load();
        while (value < currentMin && !stats.min.compare_exchange_weak(currentMin, value)) {
            // Retry if another thread modified min
        }
        
        double currentMax = stats.max.load();
        while (value > currentMax && !stats.max.compare_exchange_weak(currentMax, value)) {
            // Retry if another thread modified max
        }
        
        // Update current/previous
        stats.previous = stats.current.load();
        stats.current = value;
        
        // Update timestamps
        stats.lastSample = timestamp;
        
        // Update rate calculation
        auto now = std::chrono::high_resolution_clock::now();
        uint64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        uint64_t lastTime = stats.lastUpdateTime.load();
        
        if (lastTime > 0) {
            uint64_t deltaTimeNs = nowNs - lastTime;
            if (deltaTimeNs > 0) {
                double deltaTimeS = deltaTimeNs / 1e9;
                stats.rate = 1.0 / deltaTimeS; // Instantaneous rate
            }
        }
        
        stats.lastUpdateTime = nowNs;
    }
    
    /**
     * @brief Update computed statistics
     */
    void updateComputedStatistics() {
        std::shared_lock lock(m_statsMutex);
        
        for (auto& pair : m_fieldStats) {
            auto& stats = pair.second;
            uint64_t count = stats.sampleCount.load();
            
            if (count > 0) {
                // Calculate mean
                double mean = stats.sum.load() / count;
                stats.mean = mean;
                
                // Calculate variance and standard deviation
                if (count > 1) {
                    double sumSquared = stats.sumSquared.load();
                    double variance = (sumSquared - (count * mean * mean)) / (count - 1);
                    stats.variance = variance;
                    stats.standardDeviation = std::sqrt(std::max(0.0, variance));
                } else {
                    stats.variance = 0.0;
                    stats.standardDeviation = 0.0;
                }
                
                // Calculate range
                stats.range = stats.max.load() - stats.min.load();
            }
        }
        
        m_logger->debug("StatisticsCalculator", 
            QString("Updated computed statistics for %1 fields").arg(m_fieldStats.size()));
    }
};

} // namespace Packet
} // namespace Monitor