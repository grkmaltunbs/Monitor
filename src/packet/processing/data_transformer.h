#pragma once

#include "field_extractor.h"
#include "../../logging/logger.h"
#include "../../profiling/profiler.h"

#include <memory>
#include <vector>
#include <functional>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>

namespace Monitor {
namespace Packet {

/**
 * @brief Data transformation pipeline for field values
 * 
 * This class applies various transformations to extracted field values,
 * including type conversions, mathematical operations, and formatting.
 * It supports chaining multiple transformations for complex processing.
 */
class DataTransformer {
public:
    /**
     * @brief Transformation operation types
     */
    enum class OperationType {
        // Type conversions
        ToInteger,
        ToFloat,
        ToDouble,
        ToString,
        ToHex,
        ToBinary,
        
        // Mathematical operations
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulo,
        Power,
        
        // Mathematical functions
        Abs,
        Sqrt,
        Log,
        Log10,
        Sin,
        Cos,
        Tan,
        
        // Statistical operations
        Min,
        Max,
        Average,
        MovingAverage,
        Diff,
        CumulativeSum,
        
        // String operations
        AddPrefix,
        AddPostfix,
        Format,
        
        // Conditional operations
        Clamp,
        Threshold,
        
        // Custom
        Custom
    };
    
    /**
     * @brief Transformation parameters
     */
    struct TransformationParams {
        double numericValue;      ///< Numeric parameter for operations
        std::string stringValue;        ///< String parameter for operations
        std::vector<double> arrayValue; ///< Array parameter for operations
        size_t windowSize;         ///< Window size for moving operations
        double minValue;          ///< Minimum value for clamping
        double maxValue;        ///< Maximum value for clamping
        
        TransformationParams() 
            : numericValue(0.0), windowSize(10), minValue(0.0), maxValue(100.0) {}
        explicit TransformationParams(double value) 
            : numericValue(value), windowSize(10), minValue(0.0), maxValue(100.0) {}
        explicit TransformationParams(const std::string& str) 
            : numericValue(0.0), stringValue(str), windowSize(10), minValue(0.0), maxValue(100.0) {}
    };
    
    /**
     * @brief Single transformation step
     */
    struct Transformation {
        OperationType operation;
        TransformationParams params;
        std::function<FieldExtractor::FieldValue(const FieldExtractor::FieldValue&, const TransformationParams&)> customFunc;
        
        Transformation(OperationType op, const TransformationParams& p = TransformationParams())
            : operation(op), params(p) {}
        
        Transformation(std::function<FieldExtractor::FieldValue(const FieldExtractor::FieldValue&, const TransformationParams&)> func,
                      const TransformationParams& p = TransformationParams())
            : operation(OperationType::Custom), params(p), customFunc(func) {}
    };
    
    /**
     * @brief Transformation chain for a field
     */
    struct TransformationChain {
        std::string fieldName;
        std::vector<Transformation> transformations;
        
        // State for stateful transformations
        mutable std::vector<double> history;
        mutable double cumulativeValue = 0.0;
        mutable bool initialized = false;
        
        TransformationChain() : fieldName("") {}
        TransformationChain(const std::string& name) : fieldName(name) {}
    };
    
    /**
     * @brief Transformation result
     */
    struct TransformationResult {
        FieldExtractor::FieldValue value;
        bool success;
        std::string error;
        
        TransformationResult() : success(false) {}
        TransformationResult(const FieldExtractor::FieldValue& val) : value(val), success(true) {}
        explicit TransformationResult(const std::string& err) : success(false), error(err) {}
    };

private:
    std::unordered_map<std::string, TransformationChain> m_transformationChains;
    Logging::Logger* m_logger;
    Profiling::Profiler* m_profiler;

public:
    explicit DataTransformer()
        : m_logger(Logging::Logger::instance())
        , m_profiler(Profiling::Profiler::instance())
    {
    }
    
    /**
     * @brief Add transformation chain for field
     */
    void addTransformationChain(const std::string& fieldName, const std::vector<Transformation>& transformations) {
        m_transformationChains[fieldName] = TransformationChain(fieldName);
        m_transformationChains[fieldName].transformations = transformations;
        
        m_logger->debug("DataTransformer", 
            QString("Added transformation chain for field '%1' with %2 transformations")
                .arg(QString::fromStdString(fieldName)).arg(transformations.size()));
    }
    
    /**
     * @brief Add single transformation to field
     */
    void addTransformation(const std::string& fieldName, const Transformation& transformation) {
        auto& chain = m_transformationChains[fieldName];
        chain.fieldName = fieldName;
        chain.transformations.push_back(transformation);
        
        m_logger->debug("DataTransformer", 
            QString("Added transformation to field '%1'")
                .arg(QString::fromStdString(fieldName)));
    }
    
    /**
     * @brief Clear all transformations for field
     */
    void clearTransformations(const std::string& fieldName) {
        m_transformationChains.erase(fieldName);
    }
    
    /**
     * @brief Transform single field value
     */
    TransformationResult transform(const std::string& fieldName, const FieldExtractor::FieldValue& value) {
        auto it = m_transformationChains.find(fieldName);
        if (it == m_transformationChains.end()) {
            // No transformations, return original value
            return TransformationResult(value);
        }
        
        PROFILE_SCOPE("DataTransformer::transform");
        
        return applyTransformationChain(it->second, value);
    }
    
    /**
     * @brief Transform multiple field values
     */
    std::unordered_map<std::string, TransformationResult> transform(
        const std::unordered_map<std::string, FieldExtractor::ExtractionResult>& extractedValues) {
        
        std::unordered_map<std::string, TransformationResult> results;
        
        PROFILE_SCOPE("DataTransformer::transformMultiple");
        
        for (const auto& pair : extractedValues) {
            const std::string& fieldName = pair.first;
            const auto& extractionResult = pair.second;
            
            if (!extractionResult.success) {
                results[fieldName] = TransformationResult(std::string("Extraction failed: " + extractionResult.error));
                continue;
            }
            
            results[fieldName] = transform(fieldName, extractionResult.value);
        }
        
        return results;
    }
    
    /**
     * @brief Check if field has transformations
     */
    bool hasTransformations(const std::string& fieldName) const {
        return m_transformationChains.find(fieldName) != m_transformationChains.end();
    }
    
    /**
     * @brief Get transformation count for field
     */
    size_t getTransformationCount(const std::string& fieldName) const {
        auto it = m_transformationChains.find(fieldName);
        return (it != m_transformationChains.end()) ? it->second.transformations.size() : 0;
    }
    
    /**
     * @brief Reset stateful transformations
     */
    void resetState(const std::string& fieldName = "") {
        if (fieldName.empty()) {
            // Reset all
            for (auto& pair : m_transformationChains) {
                resetChainState(pair.second);
            }
        } else {
            // Reset specific field
            auto it = m_transformationChains.find(fieldName);
            if (it != m_transformationChains.end()) {
                resetChainState(it->second);
            }
        }
    }

private:
    /**
     * @brief Apply transformation chain to value
     */
    TransformationResult applyTransformationChain(TransformationChain& chain, const FieldExtractor::FieldValue& inputValue) {
        FieldExtractor::FieldValue currentValue = inputValue;
        
        for (const auto& transformation : chain.transformations) {
            auto result = applyTransformation(transformation, currentValue, chain);
            if (!result.success) {
                return result;
            }
            currentValue = result.value;
        }
        
        return TransformationResult(currentValue);
    }
    
    /**
     * @brief Apply single transformation
     */
    TransformationResult applyTransformation(const Transformation& transformation, 
                                            const FieldExtractor::FieldValue& value,
                                            TransformationChain& chain) {
        try {
            switch (transformation.operation) {
                case OperationType::ToInteger:
                    return convertToInteger(value);
                    
                case OperationType::ToFloat:
                    return convertToFloat(value);
                    
                case OperationType::ToDouble:
                    return convertToDouble(value);
                    
                case OperationType::ToString:
                    return convertToString(value);
                    
                case OperationType::ToHex:
                    return convertToHex(value);
                    
                case OperationType::ToBinary:
                    return convertToBinary(value);
                    
                case OperationType::Add:
                    return applyMath(value, transformation.params.numericValue, [](double a, double b) { return a + b; });
                    
                case OperationType::Subtract:
                    return applyMath(value, transformation.params.numericValue, [](double a, double b) { return a - b; });
                    
                case OperationType::Multiply:
                    return applyMath(value, transformation.params.numericValue, [](double a, double b) { return a * b; });
                    
                case OperationType::Divide:
                    if (transformation.params.numericValue == 0.0) {
                        return TransformationResult(std::string("Division by zero"));
                    }
                    return applyMath(value, transformation.params.numericValue, [](double a, double b) { return a / b; });
                    
                case OperationType::Modulo:
                    return applyMath(value, transformation.params.numericValue, [](double a, double b) { return std::fmod(a, b); });
                    
                case OperationType::Power:
                    return applyMath(value, transformation.params.numericValue, [](double a, double b) { return std::pow(a, b); });
                    
                case OperationType::Abs:
                    return applyMathFunction(value, [](double a) { return std::abs(a); });
                    
                case OperationType::Sqrt:
                    return applyMathFunction(value, [](double a) { return std::sqrt(a); });
                    
                case OperationType::Log:
                    return applyMathFunction(value, [](double a) { return std::log(a); });
                    
                case OperationType::Log10:
                    return applyMathFunction(value, [](double a) { return std::log10(a); });
                    
                case OperationType::Sin:
                    return applyMathFunction(value, [](double a) { return std::sin(a); });
                    
                case OperationType::Cos:
                    return applyMathFunction(value, [](double a) { return std::cos(a); });
                    
                case OperationType::Tan:
                    return applyMathFunction(value, [](double a) { return std::tan(a); });
                    
                case OperationType::MovingAverage:
                    return applyMovingAverage(value, chain, transformation.params.windowSize);
                    
                case OperationType::Diff:
                    return applyDifference(value, chain);
                    
                case OperationType::CumulativeSum:
                    return applyCumulativeSum(value, chain);
                    
                case OperationType::AddPrefix:
                    return addStringPrefix(value, transformation.params.stringValue);
                    
                case OperationType::AddPostfix:
                    return addStringPostfix(value, transformation.params.stringValue);
                    
                case OperationType::Clamp:
                    return applyClamp(value, transformation.params.minValue, transformation.params.maxValue);
                    
                case OperationType::Custom:
                    if (transformation.customFunc) {
                        return TransformationResult(transformation.customFunc(value, transformation.params));
                    } else {
                        return TransformationResult(std::string("No custom function provided"));
                    }
                    
                default:
                    return TransformationResult(std::string("Unknown transformation operation"));
            }
        } catch (const std::exception& e) {
            return TransformationResult(std::string("Transformation error: " + std::string(e.what())));
        }
    }
    
    /**
     * @brief Convert value to integer
     */
    TransformationResult convertToInteger(const FieldExtractor::FieldValue& value) {
        return std::visit([](const auto& val) -> TransformationResult {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T>) {
                return TransformationResult(static_cast<int64_t>(val));
            } else if constexpr (std::is_same_v<T, std::string>) {
                try {
                    return TransformationResult(static_cast<int64_t>(std::stoll(val)));
                } catch (...) {
                    return TransformationResult(std::string("Cannot convert string to integer"));
                }
            } else {
                return TransformationResult(std::string("Cannot convert to integer"));
            }
        }, value);
    }
    
    /**
     * @brief Convert value to float
     */
    TransformationResult convertToFloat(const FieldExtractor::FieldValue& value) {
        return std::visit([](const auto& val) -> TransformationResult {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T>) {
                return TransformationResult(static_cast<float>(val));
            } else if constexpr (std::is_same_v<T, std::string>) {
                try {
                    return TransformationResult(std::stof(val));
                } catch (...) {
                    return TransformationResult(std::string("Cannot convert string to float"));
                }
            } else {
                return TransformationResult(std::string("Cannot convert to float"));
            }
        }, value);
    }
    
    /**
     * @brief Convert value to double
     */
    TransformationResult convertToDouble(const FieldExtractor::FieldValue& value) {
        return std::visit([](const auto& val) -> TransformationResult {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T>) {
                return TransformationResult(static_cast<double>(val));
            } else if constexpr (std::is_same_v<T, std::string>) {
                try {
                    return TransformationResult(std::stod(val));
                } catch (...) {
                    return TransformationResult(std::string("Cannot convert string to double"));
                }
            } else {
                return TransformationResult(std::string("Cannot convert to double"));
            }
        }, value);
    }
    
    /**
     * @brief Convert value to string
     */
    TransformationResult convertToString(const FieldExtractor::FieldValue& value) {
        return std::visit([](const auto& val) -> TransformationResult {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return TransformationResult(val);
            } else if constexpr (std::is_arithmetic_v<T>) {
                return TransformationResult(std::to_string(val));
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                return TransformationResult("byte_array[" + std::to_string(val.size()) + "]");
            } else {
                return TransformationResult(std::string("unknown"));
            }
        }, value);
    }
    
    /**
     * @brief Convert value to hexadecimal string
     */
    TransformationResult convertToHex(const FieldExtractor::FieldValue& value) {
        return std::visit([](const auto& val) -> TransformationResult {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_integral_v<T>) {
                std::stringstream ss;
                ss << "0x" << std::hex << static_cast<uint64_t>(val);
                return TransformationResult(ss.str());
            } else {
                return TransformationResult(std::string("Cannot convert to hex"));
            }
        }, value);
    }
    
    /**
     * @brief Convert value to binary string
     */
    TransformationResult convertToBinary(const FieldExtractor::FieldValue& value) {
        return std::visit([](const auto& val) -> TransformationResult {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_integral_v<T>) {
                std::string binary;
                auto intVal = static_cast<uint64_t>(val);
                for (int i = 63; i >= 0; --i) {
                    binary += ((intVal >> i) & 1) ? '1' : '0';
                }
                // Remove leading zeros
                size_t firstOne = binary.find('1');
                if (firstOne != std::string::npos) {
                    binary = binary.substr(firstOne);
                } else {
                    binary = "0";
                }
                return TransformationResult("0b" + binary);
            } else {
                return TransformationResult(std::string("Cannot convert to binary"));
            }
        }, value);
    }
    
    /**
     * @brief Apply mathematical operation with parameter
     */
    TransformationResult applyMath(const FieldExtractor::FieldValue& value, double param, 
                                  std::function<double(double, double)> operation) {
        return std::visit([&](const auto& val) -> TransformationResult {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T>) {
                double result = operation(static_cast<double>(val), param);
                return TransformationResult(result);
            } else {
                return TransformationResult(std::string("Cannot apply mathematical operation"));
            }
        }, value);
    }
    
    /**
     * @brief Apply mathematical function
     */
    TransformationResult applyMathFunction(const FieldExtractor::FieldValue& value, 
                                          std::function<double(double)> function) {
        return std::visit([&](const auto& val) -> TransformationResult {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T>) {
                double result = function(static_cast<double>(val));
                return TransformationResult(result);
            } else {
                return TransformationResult(std::string("Cannot apply mathematical function"));
            }
        }, value);
    }
    
    /**
     * @brief Apply moving average
     */
    TransformationResult applyMovingAverage(const FieldExtractor::FieldValue& value, 
                                           TransformationChain& chain, size_t windowSize) {
        return std::visit([&](const auto& val) -> TransformationResult {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T>) {
                double numericValue = static_cast<double>(val);
                
                // Add to history
                chain.history.push_back(numericValue);
                if (chain.history.size() > windowSize) {
                    chain.history.erase(chain.history.begin());
                }
                
                // Calculate average
                double sum = 0.0;
                for (double histVal : chain.history) {
                    sum += histVal;
                }
                double average = sum / chain.history.size();
                
                return TransformationResult(average);
            } else {
                return TransformationResult(std::string("Cannot apply moving average to non-numeric value"));
            }
        }, value);
    }
    
    /**
     * @brief Apply difference from previous value
     */
    TransformationResult applyDifference(const FieldExtractor::FieldValue& value, 
                                        TransformationChain& chain) {
        return std::visit([&](const auto& val) -> TransformationResult {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T>) {
                double numericValue = static_cast<double>(val);
                
                if (!chain.initialized) {
                    chain.cumulativeValue = numericValue;
                    chain.initialized = true;
                    return TransformationResult(0.0); // First value, no difference
                }
                
                double diff = numericValue - chain.cumulativeValue;
                chain.cumulativeValue = numericValue;
                
                return TransformationResult(diff);
            } else {
                return TransformationResult(std::string("Cannot apply difference to non-numeric value"));
            }
        }, value);
    }
    
    /**
     * @brief Apply cumulative sum
     */
    TransformationResult applyCumulativeSum(const FieldExtractor::FieldValue& value, 
                                           TransformationChain& chain) {
        return std::visit([&](const auto& val) -> TransformationResult {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T>) {
                double numericValue = static_cast<double>(val);
                chain.cumulativeValue += numericValue;
                return TransformationResult(chain.cumulativeValue);
            } else {
                return TransformationResult(std::string("Cannot apply cumulative sum to non-numeric value"));
            }
        }, value);
    }
    
    /**
     * @brief Add string prefix
     */
    TransformationResult addStringPrefix(const FieldExtractor::FieldValue& value, const std::string& prefix) {
        auto stringResult = convertToString(value);
        if (!stringResult.success) {
            return stringResult;
        }
        
        std::string stringValue = std::get<std::string>(stringResult.value);
        return TransformationResult(prefix + stringValue);
    }
    
    /**
     * @brief Add string postfix
     */
    TransformationResult addStringPostfix(const FieldExtractor::FieldValue& value, const std::string& postfix) {
        auto stringResult = convertToString(value);
        if (!stringResult.success) {
            return stringResult;
        }
        
        std::string stringValue = std::get<std::string>(stringResult.value);
        return TransformationResult(stringValue + postfix);
    }
    
    /**
     * @brief Apply value clamping
     */
    TransformationResult applyClamp(const FieldExtractor::FieldValue& value, double minVal, double maxVal) {
        return std::visit([&](const auto& val) -> TransformationResult {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T>) {
                double numericValue = static_cast<double>(val);
                double clamped = std::max(minVal, std::min(maxVal, numericValue));
                return TransformationResult(clamped);
            } else {
                return TransformationResult(std::string("Cannot clamp non-numeric value"));
            }
        }, value);
    }
    
    /**
     * @brief Reset chain state
     */
    void resetChainState(TransformationChain& chain) {
        chain.history.clear();
        chain.cumulativeValue = 0.0;
        chain.initialized = false;
    }
};

} // namespace Packet
} // namespace Monitor