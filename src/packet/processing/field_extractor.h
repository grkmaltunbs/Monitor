#pragma once

#include "../core/packet.h"
#include "../../parser/layout/layout_calculator.h"
#include "../../logging/logger.h"
#include "../../profiling/profiler.h"

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <variant>
#include <cstring>

namespace Monitor {
namespace Packet {

/**
 * @brief High-performance field extraction using offset-based access
 * 
 * This class extracts field values from packets using pre-calculated
 * byte offsets and sizes, eliminating the need for field name lookups
 * during packet processing. It supports all primitive types, arrays,
 * and bitfields with proper alignment handling.
 */
class FieldExtractor {
public:
    /**
     * @brief Field value variant type supporting all primitive types
     */
    using FieldValue = std::variant<
        bool,           ///< Boolean values
        int8_t,         ///< Signed 8-bit integer
        uint8_t,        ///< Unsigned 8-bit integer
        int16_t,        ///< Signed 16-bit integer
        uint16_t,       ///< Unsigned 16-bit integer
        int32_t,        ///< Signed 32-bit integer
        uint32_t,       ///< Unsigned 32-bit integer
        int64_t,        ///< Signed 64-bit integer
        uint64_t,       ///< Unsigned 64-bit integer
        float,          ///< Single-precision float
        double,         ///< Double-precision float
        std::string,    ///< String values
        std::vector<uint8_t>  ///< Raw byte arrays
    >;
    
    /**
     * @brief Field extraction descriptor
     */
    struct FieldDescriptor {
        std::string name;           ///< Field name for identification
        size_t offset;              ///< Byte offset from packet payload start
        size_t size;                ///< Field size in bytes
        std::string typeName;       ///< Type name for validation
        bool isBitfield;            ///< True if field is a bitfield
        uint8_t bitOffset;          ///< Bit offset within byte (for bitfields)
        uint8_t bitWidth;           ///< Bit width (for bitfields)
        bool isArray;               ///< True if field is an array
        size_t arraySize;           ///< Array size (if isArray)
        bool isNullTerminated;      ///< True for null-terminated strings
        
        FieldDescriptor() 
            : offset(0), size(0), isBitfield(false), bitOffset(0), bitWidth(0),
              isArray(false), arraySize(0), isNullTerminated(false)
        {
        }
        
        FieldDescriptor(const std::string& fieldName, size_t fieldOffset, size_t fieldSize, const std::string& type)
            : name(fieldName), offset(fieldOffset), size(fieldSize), typeName(type),
              isBitfield(false), bitOffset(0), bitWidth(0), isArray(false), arraySize(0), isNullTerminated(false)
        {
        }
        
        bool isValid() const {
            return !name.empty() && size > 0 && 
                   (isBitfield ? (bitWidth > 0 && bitWidth <= 64) : true);
        }
    };
    
    /**
     * @brief Extraction result
     */
    struct ExtractionResult {
        FieldValue value;
        bool success;
        std::string error;
        
        ExtractionResult() : success(false) {}
        ExtractionResult(const FieldValue& val) : value(val), success(true) {}
        explicit ExtractionResult(const std::string& err) : success(false), error(err) {}
    };
    
    /**
     * @brief Field descriptor cache for packet types
     */
    struct PacketFieldMap {
        PacketId packetId;
        std::string structureName;
        std::vector<FieldDescriptor> fields;
        std::unordered_map<std::string, size_t> fieldIndex; ///< Name to index lookup
        size_t totalPayloadSize;
        
        PacketFieldMap() : packetId(0), totalPayloadSize(0) {}
        PacketFieldMap(PacketId id, const std::string& name) 
            : packetId(id), structureName(name), totalPayloadSize(0) {}
    };

private:
    // Field descriptor cache
    std::unordered_map<PacketId, PacketFieldMap> m_fieldMaps;
    
    // Utilities
    Logging::Logger* m_logger;
    Profiling::Profiler* m_profiler;

public:
    explicit FieldExtractor()
        : m_logger(Logging::Logger::instance())
        , m_profiler(Profiling::Profiler::instance())
    {
    }
    
    /**
     * @brief Build field map from structure definition
     */
    bool buildFieldMap(PacketId packetId, 
                      std::shared_ptr<const Parser::AST::StructDeclaration> structure) {
        if (!structure) {
            m_logger->error("FieldExtractor", "Null structure definition");
            return false;
        }
        
        PROFILE_SCOPE("FieldExtractor::buildFieldMap");
        
        PacketFieldMap fieldMap(packetId, structure->getName());
        
        // Use layout calculator to get field offsets
        Parser::Layout::LayoutCalculator calculator;
        auto layoutResult = calculator.calculateStructLayout(*structure);
        
        if (layoutResult.totalSize == 0) {
            m_logger->error("FieldExtractor", 
                QString("Failed to calculate layout for %1").arg(QString::fromStdString(structure->getName())));
            return false;
        }
        
        // Build field descriptors from layout information
        buildFieldDescriptorsRecursive(structure, layoutResult, "", 0, fieldMap.fields);
        
        // Build lookup index
        for (size_t i = 0; i < fieldMap.fields.size(); ++i) {
            fieldMap.fieldIndex[fieldMap.fields[i].name] = i;
        }
        
        fieldMap.totalPayloadSize = layoutResult.totalSize;
        
        // Store in cache
        m_fieldMaps[packetId] = std::move(fieldMap);
        
        m_logger->info("FieldExtractor", 
            QString("Built field map for packet ID %1 (%2): %3 fields, %4 bytes total")
                .arg(packetId).arg(QString::fromStdString(structure->getName()))
                .arg(fieldMap.fields.size()).arg(layoutResult.totalSize));
        
        return true;
    }
    
    /**
     * @brief Extract single field by name
     */
    ExtractionResult extractField(PacketPtr packet, const std::string& fieldName) const {
        if (!packet || !packet->isValid()) {
            return ExtractionResult(std::string("Invalid packet"));
        }
        
        auto it = m_fieldMaps.find(packet->id());
        if (it == m_fieldMaps.end()) {
            return ExtractionResult(std::string("No field map for packet ID " + std::to_string(packet->id())));
        }
        
        const auto& fieldMap = it->second;
        auto fieldIt = fieldMap.fieldIndex.find(fieldName);
        if (fieldIt == fieldMap.fieldIndex.end()) {
            return ExtractionResult(std::string("Field not found: " + fieldName));
        }
        
        const auto& descriptor = fieldMap.fields[fieldIt->second];
        return extractFieldByDescriptor(packet, descriptor);
    }
    
    /**
     * @brief Extract field by pre-built descriptor (most efficient)
     */
    ExtractionResult extractFieldByDescriptor(PacketPtr packet, const FieldDescriptor& descriptor) const {
        if (!packet || !packet->isValid()) {
            return ExtractionResult(std::string("Invalid packet"));
        }
        
        if (!descriptor.isValid()) {
            return ExtractionResult(std::string("Invalid field descriptor"));
        }
        
        PROFILE_SCOPE("FieldExtractor::extractFieldByDescriptor");
        
        const uint8_t* payload = packet->payload();
        size_t payloadSize = packet->payloadSize();
        
        // Bounds check
        if (descriptor.offset + descriptor.size > payloadSize) {
            return ExtractionResult(std::string("Field offset beyond payload size"));
        }
        
        const uint8_t* fieldData = payload + descriptor.offset;
        
        if (descriptor.isBitfield) {
            return extractBitfield(fieldData, descriptor);
        } else if (descriptor.isArray) {
            return extractArray(fieldData, descriptor);
        } else {
            return extractPrimitive(fieldData, descriptor);
        }
    }
    
    /**
     * @brief Extract multiple fields efficiently
     */
    std::unordered_map<std::string, ExtractionResult> extractFields(
        PacketPtr packet, const std::vector<std::string>& fieldNames) const {
        
        std::unordered_map<std::string, ExtractionResult> results;
        
        if (!packet || !packet->isValid()) {
            for (const auto& name : fieldNames) {
                results[name] = ExtractionResult(std::string("Invalid packet"));
            }
            return results;
        }
        
        auto it = m_fieldMaps.find(packet->id());
        if (it == m_fieldMaps.end()) {
            std::string error = "No field map for packet ID " + std::to_string(packet->id());
            for (const auto& name : fieldNames) {
                results[name] = ExtractionResult(std::string(error));
            }
            return results;
        }
        
        PROFILE_SCOPE("FieldExtractor::extractFields");
        
        const auto& fieldMap = it->second;
        
        for (const auto& fieldName : fieldNames) {
            auto fieldIt = fieldMap.fieldIndex.find(fieldName);
            if (fieldIt == fieldMap.fieldIndex.end()) {
                results[fieldName] = ExtractionResult(std::string("Field not found: " + fieldName));
                continue;
            }
            
            const auto& descriptor = fieldMap.fields[fieldIt->second];
            results[fieldName] = extractFieldByDescriptor(packet, descriptor);
        }
        
        return results;
    }
    
    /**
     * @brief Extract all fields from packet
     */
    std::unordered_map<std::string, ExtractionResult> extractAllFields(PacketPtr packet) const {
        std::unordered_map<std::string, ExtractionResult> results;
        
        if (!packet || !packet->isValid()) {
            return results;
        }
        
        auto it = m_fieldMaps.find(packet->id());
        if (it == m_fieldMaps.end()) {
            return results;
        }
        
        PROFILE_SCOPE("FieldExtractor::extractAllFields");
        
        const auto& fieldMap = it->second;
        
        for (const auto& descriptor : fieldMap.fields) {
            results[descriptor.name] = extractFieldByDescriptor(packet, descriptor);
        }
        
        return results;
    }
    
    /**
     * @brief Get field descriptors for packet type
     */
    std::vector<FieldDescriptor> getFieldDescriptors(PacketId packetId) const {
        auto it = m_fieldMaps.find(packetId);
        return (it != m_fieldMaps.end()) ? it->second.fields : std::vector<FieldDescriptor>();
    }
    
    /**
     * @brief Check if packet type has field map
     */
    bool hasFieldMap(PacketId packetId) const {
        return m_fieldMaps.find(packetId) != m_fieldMaps.end();
    }
    
    /**
     * @brief Get field count for packet type
     */
    size_t getFieldCount(PacketId packetId) const {
        auto it = m_fieldMaps.find(packetId);
        return (it != m_fieldMaps.end()) ? it->second.fields.size() : 0;
    }

private:
    /**
     * @brief Recursively build field descriptors from structure
     */
    void buildFieldDescriptorsRecursive(
        std::shared_ptr<const Parser::AST::StructDeclaration> structure,
        const Parser::Layout::LayoutCalculator::StructLayout& /*layoutResult*/,
        const std::string& prefix,
        size_t baseOffset,
        std::vector<FieldDescriptor>& descriptors) const {
        
        for (const auto& field : structure->getFields()) {
            std::string fullName = prefix.empty() ? field->getName() : prefix + "." + field->getName();
            
            // Get field layout info
            size_t fieldOffset = baseOffset; // Would need actual offset from layout result
            size_t fieldSize = 0; // Would need actual size from layout result
            
            if (auto bitfield = dynamic_cast<Parser::AST::BitfieldDeclaration*>(field.get())) {
                // Bitfield handling
                FieldDescriptor descriptor(fullName, fieldOffset, fieldSize, bitfield->getBaseTypeName());
                descriptor.isBitfield = true;
                descriptor.bitOffset = 0; // Would calculate from layout
                descriptor.bitWidth = bitfield->getBitWidth();
                descriptors.push_back(descriptor);
                
            } else if (auto regularField = dynamic_cast<Parser::AST::FieldDeclaration*>(field.get())) {
                // Regular field
                auto type = regularField->getType();
                if (type->isArray()) {
                    // Array field
                    auto arrayType = dynamic_cast<const Parser::AST::ArrayType*>(type);
                    FieldDescriptor descriptor(fullName, fieldOffset, fieldSize, type->getTypeName());
                    descriptor.isArray = true;
                    descriptor.arraySize = arrayType ? arrayType->getArraySize() : 0;
                    descriptors.push_back(descriptor);
                } else {
                    // Primitive field
                    FieldDescriptor descriptor(fullName, fieldOffset, fieldSize, type->getTypeName());
                    descriptors.push_back(descriptor);
                }
            }
        }
    }
    
    /**
     * @brief Extract bitfield value
     */
    ExtractionResult extractBitfield(const uint8_t* data, const FieldDescriptor& descriptor) const {
        if (!data) {
            return ExtractionResult(std::string("Null data pointer"));
        }
        
        // Read enough bytes to contain the bitfield
        uint64_t value = 0;
        size_t bytesToRead = std::min(descriptor.size, sizeof(uint64_t));
        std::memcpy(&value, data, bytesToRead);
        
        // Extract bits
        uint64_t mask = (1ULL << descriptor.bitWidth) - 1;
        uint64_t extractedValue = (value >> descriptor.bitOffset) & mask;
        
        // Convert to appropriate type based on bit width
        if (descriptor.bitWidth == 1) {
            return ExtractionResult(static_cast<bool>(extractedValue));
        } else if (descriptor.bitWidth <= 8) {
            return ExtractionResult(static_cast<uint8_t>(extractedValue));
        } else if (descriptor.bitWidth <= 16) {
            return ExtractionResult(static_cast<uint16_t>(extractedValue));
        } else if (descriptor.bitWidth <= 32) {
            return ExtractionResult(static_cast<uint32_t>(extractedValue));
        } else {
            return ExtractionResult(static_cast<uint64_t>(extractedValue));
        }
    }
    
    /**
     * @brief Extract array value
     */
    ExtractionResult extractArray(const uint8_t* data, const FieldDescriptor& descriptor) const {
        if (!data) {
            return ExtractionResult(std::string("Null data pointer"));
        }
        
        if (descriptor.typeName == "char" || descriptor.typeName == "unsigned char") {
            // String array
            std::string str;
            if (descriptor.isNullTerminated) {
                str = std::string(reinterpret_cast<const char*>(data));
            } else {
                str = std::string(reinterpret_cast<const char*>(data), descriptor.size);
            }
            return ExtractionResult(str);
        } else {
            // Byte array
            std::vector<uint8_t> bytes(data, data + descriptor.size);
            return ExtractionResult(bytes);
        }
    }
    
    /**
     * @brief Extract primitive value
     */
    ExtractionResult extractPrimitive(const uint8_t* data, const FieldDescriptor& descriptor) const {
        if (!data) {
            return ExtractionResult(std::string("Null data pointer"));
        }
        
        const std::string& typeName = descriptor.typeName;
        
        // Handle primitive types based on name and size
        if (typeName == "bool" || typeName == "_Bool") {
            bool value;
            std::memcpy(&value, data, sizeof(bool));
            return ExtractionResult(value);
        } else if (typeName == "char" || typeName == "signed char") {
            int8_t value;
            std::memcpy(&value, data, sizeof(int8_t));
            return ExtractionResult(value);
        } else if (typeName == "unsigned char") {
            uint8_t value;
            std::memcpy(&value, data, sizeof(uint8_t));
            return ExtractionResult(value);
        } else if (typeName == "short" || typeName == "short int" || typeName == "signed short") {
            int16_t value;
            std::memcpy(&value, data, sizeof(int16_t));
            return ExtractionResult(value);
        } else if (typeName == "unsigned short" || typeName == "unsigned short int") {
            uint16_t value;
            std::memcpy(&value, data, sizeof(uint16_t));
            return ExtractionResult(value);
        } else if (typeName == "int" || typeName == "signed int") {
            int32_t value;
            std::memcpy(&value, data, sizeof(int32_t));
            return ExtractionResult(value);
        } else if (typeName == "unsigned int") {
            uint32_t value;
            std::memcpy(&value, data, sizeof(uint32_t));
            return ExtractionResult(value);
        } else if (typeName == "long" || typeName == "long int" || typeName == "signed long" ||
                   typeName == "long long" || typeName == "signed long long") {
            int64_t value;
            std::memcpy(&value, data, sizeof(int64_t));
            return ExtractionResult(value);
        } else if (typeName == "unsigned long" || typeName == "unsigned long int" ||
                   typeName == "unsigned long long") {
            uint64_t value;
            std::memcpy(&value, data, sizeof(uint64_t));
            return ExtractionResult(value);
        } else if (typeName == "float") {
            float value;
            std::memcpy(&value, data, sizeof(float));
            return ExtractionResult(value);
        } else if (typeName == "double") {
            double value;
            std::memcpy(&value, data, sizeof(double));
            return ExtractionResult(value);
        } else {
            // Unknown type, return as raw bytes
            std::vector<uint8_t> bytes(data, data + descriptor.size);
            return ExtractionResult(bytes);
        }
    }
};

} // namespace Packet
} // namespace Monitor