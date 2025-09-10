#pragma once

#include "alignment_rules.h"
#include "../ast/ast_nodes.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <chrono>

namespace Monitor {
namespace Parser {
namespace Layout {

/**
 * @brief Layout calculator for C++ structures
 * 
 * This class calculates the memory layout of C++ structures,
 * including field offsets, sizes, and padding. It handles
 * complex cases like bitfields, unions, and packed structures.
 */
class LayoutCalculator {
public:
    struct FieldLayout {
        size_t offset;              // Byte offset from struct start
        size_t size;                // Field size in bytes
        size_t alignment;           // Field alignment requirement
        
        // Bitfield-specific information
        uint32_t bitOffset = 0;     // Bit offset within storage unit
        uint32_t bitWidth = 0;      // Width in bits (0 = not a bitfield)
        uint64_t bitMask = 0;       // Extraction mask
        
        // Padding information
        size_t paddingBefore = 0;   // Padding bytes before this field
        size_t paddingAfter = 0;    // Padding bytes after this field
        
        FieldLayout() : offset(0), size(0), alignment(0) {}
        FieldLayout(size_t off, size_t sz, size_t align)
            : offset(off), size(sz), alignment(align) {}
        
        bool isBitfield() const { return bitWidth > 0; }
    };
    
    struct StructLayout {
        size_t totalSize;           // Total struct size including final padding
        size_t alignment;           // Struct alignment requirement
        size_t totalPadding;        // Total padding bytes
        
        std::unordered_map<std::string, FieldLayout> fieldLayouts;
        std::vector<size_t> paddingLocations;  // Offsets where padding occurs
        
        // Pack pragma information
        bool isPacked = false;
        uint8_t packValue = 0;
        
        // Statistics
        size_t fieldCount = 0;
        size_t bitfieldCount = 0;
        double paddingRatio = 0.0;  // padding / total size
        
        StructLayout() : totalSize(0), alignment(0), totalPadding(0) {}
    };
    
    struct UnionLayout {
        size_t totalSize;           // Size of largest member
        size_t alignment;           // Maximum member alignment
        
        std::unordered_map<std::string, FieldLayout> memberLayouts;
        size_t memberCount = 0;
        
        UnionLayout() : totalSize(0), alignment(0) {}
    };
    
    explicit LayoutCalculator(std::shared_ptr<AlignmentRules> alignmentRules = nullptr);
    ~LayoutCalculator() = default;
    
    // Main layout calculation methods
    StructLayout calculateStructLayout(const AST::StructDeclaration& structDecl);
    UnionLayout calculateUnionLayout(const AST::UnionDeclaration& unionDecl);
    
    // Individual field layout calculation
    FieldLayout calculateFieldLayout(const AST::FieldDeclaration& field, 
                                   size_t currentOffset,
                                   uint8_t packValue = 0);
    
    FieldLayout calculateBitfieldLayout(const AST::BitfieldDeclaration& bitfield,
                                      size_t currentOffset,
                                      uint32_t& currentBitOffset,
                                      const std::string& currentBitfieldType,
                                      uint8_t packValue = 0);
    
    // Type-specific calculations
    size_t calculateTypeSize(const AST::TypeNode* type);
    size_t calculateTypeAlignment(const AST::TypeNode* type);
    
    // Array and pointer calculations
    size_t calculateArraySize(const AST::ArrayType* arrayType);
    size_t calculatePointerSize() const;
    
    // Offset and padding calculations
    size_t calculateOffset(size_t currentOffset, size_t alignment, uint8_t packValue = 0) const;
    size_t calculatePadding(size_t currentOffset, size_t alignment, uint8_t packValue = 0) const;
    size_t calculateFinalPadding(size_t currentSize, size_t structAlignment) const;
    
    // Validation methods
    bool validateLayout(const StructLayout& layout) const;
    bool validateFieldAccess(const std::string& fieldPath, const StructLayout& layout) const;
    
    // Cache management
    void cacheStructLayout(const std::string& structName, const StructLayout& layout);
    bool getCachedStructLayout(const std::string& structName, StructLayout& layout) const;
    void invalidateCache(const std::string& structName = "");
    void clearCache();
    
    // Configuration
    void setAlignmentRules(std::shared_ptr<AlignmentRules> rules) { m_alignmentRules = rules; }
    std::shared_ptr<AlignmentRules> getAlignmentRules() const { return m_alignmentRules; }
    
    void enableCaching(bool enabled) { m_cachingEnabled = enabled; }
    bool isCachingEnabled() const { return m_cachingEnabled; }
    
    void setMaxCacheSize(size_t maxSize) { m_maxCacheSize = maxSize; }
    size_t getMaxCacheSize() const { return m_maxCacheSize; }
    
    // Statistics and diagnostics
    struct CalculationStatistics {
        size_t structsCalculated = 0;
        size_t unionsCalculated = 0;
        size_t fieldsProcessed = 0;
        size_t bitfieldsProcessed = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        std::chrono::milliseconds totalTime{0};
        std::chrono::milliseconds averageTime{0};
        
        double getCacheHitRatio() const {
            size_t total = cacheHits + cacheMisses;
            return total > 0 ? static_cast<double>(cacheHits) / total : 0.0;
        }
    };
    
    const CalculationStatistics& getStatistics() const { return m_statistics; }
    void resetStatistics() { m_statistics = CalculationStatistics(); }
    
    // Error handling
    struct LayoutError {
        std::string message;
        std::string structName;
        std::string fieldName;
        size_t offset;
        
        LayoutError(const std::string& msg, const std::string& sName = "",
                   const std::string& fName = "", size_t off = 0)
            : message(msg), structName(sName), fieldName(fName), offset(off) {}
    };
    
    const std::vector<LayoutError>& getErrors() const { return m_errors; }
    bool hasErrors() const { return !m_errors.empty(); }
    void clearErrors() { m_errors.clear(); }
    
    // Utility methods
    std::string generateLayoutReport(const StructLayout& layout, const std::string& structName) const;
    void printLayoutReport(const StructLayout& layout, const std::string& structName) const;
    
    // Field path resolution (for nested access like "structA.fieldB.subFieldC")
    bool resolveFieldPath(const std::string& fieldPath, 
                         const StructLayout& rootLayout,
                         size_t& finalOffset,
                         size_t& finalSize) const;
    
private:
    std::shared_ptr<AlignmentRules> m_alignmentRules;
    
    // Caching
    bool m_cachingEnabled;
    size_t m_maxCacheSize;
    std::unordered_map<std::string, StructLayout> m_structLayoutCache;
    std::unordered_map<std::string, UnionLayout> m_unionLayoutCache;
    
    // Statistics and errors
    mutable CalculationStatistics m_statistics;
    mutable std::vector<LayoutError> m_errors;
    
    // Bitfield state management
    struct BitfieldContext {
        std::string currentType;
        uint32_t currentBitOffset = 0;
        size_t currentByteOffset = 0;
        size_t storageUnitSize = 0;
        
        void reset() {
            currentType.clear();
            currentBitOffset = 0;
            currentByteOffset = 0;
            storageUnitSize = 0;
        }
        
        bool isActive() const { return !currentType.empty(); }
    };
    
    // Helper methods
    void updateStatistics(std::chrono::steady_clock::time_point startTime) const;
    void addError(const std::string& message, const std::string& structName = "",
                  const std::string& fieldName = "", size_t offset = 0) const;
    
    // Layout calculation helpers
    FieldLayout calculatePrimitiveLayout(const AST::PrimitiveType* primitiveType,
                                       size_t currentOffset, uint8_t packValue = 0);
    FieldLayout calculateNamedTypeLayout(const AST::NamedType* namedType,
                                       size_t currentOffset, uint8_t packValue = 0);
    FieldLayout calculateArrayLayout(const AST::ArrayType* arrayType,
                                   size_t currentOffset, uint8_t packValue = 0);
    FieldLayout calculatePointerLayout(const AST::PointerType* pointerType,
                                     size_t currentOffset, uint8_t packValue = 0);
    
    // Bitfield helpers
    bool canCombineBitfields(const std::string& newType, const std::string& currentType) const;
    void resetBitfieldContext(BitfieldContext& context) const;
    bool bitfieldFitsInCurrentUnit(uint32_t currentBitOffset, uint32_t bitWidth, 
                                  size_t storageUnitSize) const;
    
    // Cache management helpers
    void evictOldestFromCache();
    void updateCacheAccessTime(const std::string& key);
    
    // Validation helpers
    bool isValidOffset(size_t offset, size_t size, size_t structSize) const;
    bool hasValidAlignment(size_t offset, size_t alignment) const;
};

} // namespace Layout
} // namespace Parser
} // namespace Monitor