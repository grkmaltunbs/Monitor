#pragma once

#include "alignment_rules.h"
#include "../ast/ast_nodes.h"
#include <vector>
#include <string>
#include <cstdint>

namespace Monitor {
namespace Parser {
namespace Layout {

/**
 * @brief Specialized handler for C++ bitfield layout calculation
 * 
 * This class handles the complex rules for bitfield allocation,
 * including compiler-specific differences, bit boundary handling,
 * and storage unit management.
 */
class BitfieldHandler {
public:
    struct BitfieldInfo {
        std::string name;
        std::string baseType;       // uint32_t, int, etc.
        uint32_t bitWidth;          // Number of bits
        uint32_t bitOffset;         // Bit position within storage unit
        size_t byteOffset;          // Byte offset from struct start
        uint64_t extractionMask;    // Mask for bit extraction
        bool isSigned;
        
        BitfieldInfo() : bitWidth(0), bitOffset(0), byteOffset(0), 
                        extractionMask(0), isSigned(false) {}
        
        BitfieldInfo(const std::string& n, const std::string& type, uint32_t width)
            : name(n), baseType(type), bitWidth(width), bitOffset(0), 
              byteOffset(0), extractionMask(0), isSigned(false) {}
    };
    
    struct BitfieldGroup {
        std::string baseType;       // Common base type (uint32_t, int, etc.)
        size_t baseTypeSize;        // Size of base type in bytes
        uint32_t totalBits;         // Total bits used in this group
        size_t startByteOffset;     // Byte offset where this group starts
        std::vector<BitfieldInfo> fields;
        
        BitfieldGroup(const std::string& type, size_t size, size_t offset)
            : baseType(type), baseTypeSize(size), totalBits(0), startByteOffset(offset) {}
        
        bool canFit(uint32_t additionalBits) const {
            return (totalBits + additionalBits) <= (baseTypeSize * 8);
        }
        
        bool isEmpty() const { return fields.empty(); }
        
        uint32_t getUsedBits() const { return totalBits; }
        uint32_t getRemainingBits() const { 
            return (static_cast<uint32_t>(baseTypeSize) * 8) - totalBits; 
        }
    };
    
    struct AllocationResult {
        std::vector<BitfieldGroup> groups;
        size_t totalSizeBytes = 0;
        size_t totalFields = 0;
        bool success = false;
        std::vector<std::string> errors;
        
        AllocationResult() = default;
        
        void addError(const std::string& error) {
            errors.push_back(error);
            success = false;
        }
        
        bool hasErrors() const { return !errors.empty(); }
    };
    
    explicit BitfieldHandler(std::shared_ptr<AlignmentRules> alignmentRules);
    ~BitfieldHandler() = default;
    
    // Main bitfield allocation methods
    AllocationResult allocateBitfields(const std::vector<AST::BitfieldDeclaration*>& bitfields,
                                     size_t startOffset = 0,
                                     uint8_t packValue = 0);
    
    // Individual bitfield processing
    BitfieldInfo processBitfield(const AST::BitfieldDeclaration& bitfield,
                               BitfieldGroup& currentGroup,
                               uint8_t packValue = 0);
    
    // Group management
    BitfieldGroup createGroup(const std::string& baseType, size_t offset);
    bool canCombineInGroup(const BitfieldGroup& group, 
                          const AST::BitfieldDeclaration& bitfield) const;
    
    // Bit manipulation utilities
    uint64_t generateBitMask(uint32_t bitOffset, uint32_t bitWidth) const;
    uint32_t calculateBitOffset(const BitfieldGroup& group, uint32_t bitWidth) const;
    
    // Compiler-specific allocation strategies
    void allocateWithMSVCRules(BitfieldGroup& group);
    void allocateWithGCCRules(BitfieldGroup& group);
    void allocateWithClangRules(BitfieldGroup& group);
    
    // Validation methods
    bool validateBitfieldDeclaration(const AST::BitfieldDeclaration& bitfield) const;
    bool validateBaseType(const std::string& baseType) const;
    bool validateBitWidth(uint32_t bitWidth, const std::string& baseType) const;
    
    // Boundary and alignment handling
    bool spansBoundary(uint32_t bitOffset, uint32_t bitWidth, size_t storageSize) const;
    size_t calculateGroupAlignment(const BitfieldGroup& group, uint8_t packValue = 0) const;
    size_t calculateGroupPadding(const BitfieldGroup& group, size_t currentOffset) const;
    
    // Zero-width bitfield handling (alignment markers)
    bool isZeroWidthBitfield(const AST::BitfieldDeclaration& bitfield) const;
    void handleZeroWidthBitfield(AllocationResult& result, 
                               const AST::BitfieldDeclaration& bitfield,
                               size_t currentOffset) const;
    
    // Endianness considerations
    uint32_t adjustBitOffsetForEndianness(uint32_t bitOffset, 
                                        uint32_t bitWidth, 
                                        size_t storageSize) const;
    
    // Configuration
    void setAlignmentRules(std::shared_ptr<AlignmentRules> rules) { m_alignmentRules = rules; }
    std::shared_ptr<AlignmentRules> getAlignmentRules() const { return m_alignmentRules; }
    
    // Diagnostics and reporting
    std::string generateBitfieldReport(const AllocationResult& result) const;
    void printBitfieldLayout(const AllocationResult& result) const;
    
    // Statistics
    struct BitfieldStatistics {
        size_t totalBitfields = 0;
        size_t totalGroups = 0;
        size_t totalBitsUsed = 0;
        size_t totalBytesUsed = 0;
        size_t wastedBits = 0;
        double packingEfficiency = 0.0;
        
        void calculateEfficiency() {
            size_t totalCapacity = totalGroups * sizeof(uint32_t) * 8; // Assume uint32_t base
            packingEfficiency = totalCapacity > 0 ? 
                              static_cast<double>(totalBitsUsed) / totalCapacity : 0.0;
        }
    };
    
    BitfieldStatistics calculateStatistics(const AllocationResult& result) const;
    
    // Error handling
    struct BitfieldError {
        std::string message;
        std::string fieldName;
        std::string baseType;
        uint32_t bitWidth;
        size_t position;
        
        BitfieldError(const std::string& msg, const std::string& field = "",
                     const std::string& type = "", uint32_t width = 0, size_t pos = 0)
            : message(msg), fieldName(field), baseType(type), bitWidth(width), position(pos) {}
    };
    
    const std::vector<BitfieldError>& getErrors() const { return m_errors; }
    bool hasErrors() const { return !m_errors.empty(); }
    void clearErrors() { m_errors.clear(); }
    
private:
    std::shared_ptr<AlignmentRules> m_alignmentRules;
    mutable std::vector<BitfieldError> m_errors;
    
    // Compiler-specific constants
    static constexpr uint32_t MSVC_MAX_BITFIELD_SIZE = 64;
    static constexpr uint32_t GCC_MAX_BITFIELD_SIZE = 64;
    
    // Helper methods
    void addError(const std::string& message, const std::string& fieldName = "",
                  const std::string& baseType = "", uint32_t bitWidth = 0, size_t position = 0) const;
    
    size_t getBaseTypeSize(const std::string& baseType) const;
    size_t getBaseTypeAlignment(const std::string& baseType) const;
    bool isSignedType(const std::string& baseType) const;
    bool isValidBitfieldBaseType(const std::string& baseType) const;
    
    // Bit manipulation helpers
    uint32_t getMaxBitWidth(const std::string& baseType) const;
    bool isLittleEndian() const;
    
    // Allocation strategy selection
    void allocateBasedOnCompiler(BitfieldGroup& group);
    
    // Group finalization
    void finalizeGroup(BitfieldGroup& group);
    void calculateMasks(BitfieldGroup& group);
    
    // Padding and alignment
    size_t calculateInterGroupPadding(const BitfieldGroup& prevGroup, 
                                    const BitfieldGroup& nextGroup) const;
    
    // Validation helpers
    bool validateGroupConsistency(const BitfieldGroup& group) const;
    bool validateBitOffsets(const BitfieldGroup& group) const;
    bool validateNoOverlaps(const BitfieldGroup& group) const;
};

} // namespace Layout
} // namespace Parser
} // namespace Monitor