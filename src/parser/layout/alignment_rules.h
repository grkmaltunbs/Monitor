#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Monitor {
namespace Parser {
namespace Layout {

/**
 * @brief Compiler-specific alignment and size rules
 * 
 * This class encapsulates the different alignment and sizing rules
 * used by various C++ compilers. It provides a unified interface
 * for calculating type sizes and alignments across different
 * compilation targets.
 */
class AlignmentRules {
public:
    enum class CompilerType {
        MSVC,           // Microsoft Visual C++
        GCC,            // GNU Compiler Collection
        CLANG,          // LLVM Clang
        AUTO_DETECT     // Attempt to detect current compiler
    };
    
    enum class Architecture {
        X86,            // 32-bit x86
        X64,            // 64-bit x86-64
        ARM32,          // 32-bit ARM
        ARM64,          // 64-bit ARM (AArch64)
        AUTO_DETECT     // Detect current architecture
    };
    
    struct TypeInfo {
        size_t size;        // Size in bytes
        size_t alignment;   // Alignment requirement in bytes
        bool isPrimitive;   // Whether this is a primitive type
        bool isSigned;      // For integer types
        
        TypeInfo() : size(0), alignment(0), isPrimitive(false), isSigned(false) {}
        TypeInfo(size_t s, size_t a, bool p = false, bool sign = false)
            : size(s), alignment(a), isPrimitive(p), isSigned(sign) {}
    };
    
    explicit AlignmentRules(CompilerType compiler = CompilerType::AUTO_DETECT,
                           Architecture arch = Architecture::AUTO_DETECT);
    ~AlignmentRules() = default;
    
    // Compiler and architecture detection
    CompilerType detectCompiler() const;
    Architecture detectArchitecture() const;
    
    // Type information queries
    TypeInfo getTypeInfo(const std::string& typeName) const;
    size_t getTypeSize(const std::string& typeName) const;
    size_t getTypeAlignment(const std::string& typeName) const;
    bool isPrimitiveType(const std::string& typeName) const;
    bool isSignedType(const std::string& typeName) const;
    
    // Alignment calculations
    size_t calculateStructAlignment(const std::vector<size_t>& memberAlignments) const;
    size_t calculatePadding(size_t currentOffset, size_t alignment) const;
    size_t alignOffset(size_t offset, size_t alignment) const;
    size_t calculateStructPadding(size_t structSize, size_t structAlignment) const;
    
    // Pack pragma support
    size_t applyPackAlignment(size_t naturalAlignment, uint8_t packValue) const;
    bool isValidPackValue(uint8_t packValue) const;
    
    // Platform-specific information
    size_t getPointerSize() const;
    size_t getPointerAlignment() const;
    bool isLittleEndian() const;
    bool isBigEndian() const;
    
    // Maximum alignment constraints
    size_t getMaxAlignment() const;
    size_t getDefaultStructAlignment() const;
    
    // Compiler-specific behavior
    bool usesMSVCBitfieldPacking() const { return m_compilerType == CompilerType::MSVC; }
    bool usesGCCBitfieldPacking() const { 
        return m_compilerType == CompilerType::GCC || m_compilerType == CompilerType::CLANG; 
    }
    
    // Configuration
    void setCompilerType(CompilerType compiler);
    void setArchitecture(Architecture arch);
    CompilerType getCompilerType() const { return m_compilerType; }
    Architecture getArchitecture() const { return m_architecture; }
    
    // Custom type registration
    void registerCustomType(const std::string& typeName, const TypeInfo& info);
    void unregisterCustomType(const std::string& typeName);
    bool hasCustomType(const std::string& typeName) const;
    
    // Debugging and diagnostics
    std::string getCompilerName() const;
    std::string getArchitectureName() const;
    void printTypeTable() const;
    
private:
    CompilerType m_compilerType;
    Architecture m_architecture;
    
    // Type information tables
    std::unordered_map<std::string, TypeInfo> m_typeTable;
    std::unordered_map<std::string, TypeInfo> m_customTypes;
    
    // Platform-specific information
    size_t m_pointerSize;
    size_t m_pointerAlignment;
    bool m_isLittleEndian;
    size_t m_maxAlignment;
    
    // Initialization methods
    void initializeForMSVC();
    void initializeForGCC();
    void initializeForClang();
    void initializePlatformInfo();
    void populateTypeTable();
    
    // Helper methods
    static const std::unordered_map<std::string, TypeInfo>& getMSVCTypes();
    static const std::unordered_map<std::string, TypeInfo>& getGCCTypes();
    static const std::unordered_map<std::string, TypeInfo>& getClangTypes();
    
    // Validation methods
    bool validateTypeInfo(const TypeInfo& info) const;
    void validateConfiguration() const;
};

/**
 * @brief Helper class for pack pragma state management
 */
class PackState {
public:
    PackState() : m_packValue(0), m_isActive(false) {}
    
    void push(uint8_t packValue, const std::string& identifier = "");
    void pop(const std::string& identifier = "");
    void set(uint8_t packValue);
    void reset();
    
    uint8_t getCurrentPackValue() const { return m_packValue; }
    bool isActive() const { return m_isActive; }
    size_t getStackDepth() const { return m_packStack.size(); }
    
private:
    struct PackEntry {
        uint8_t packValue;
        std::string identifier;
        
        PackEntry(uint8_t value, const std::string& id = "")
            : packValue(value), identifier(id) {}
    };
    
    std::vector<PackEntry> m_packStack;
    uint8_t m_packValue;
    bool m_isActive;
};

} // namespace Layout
} // namespace Parser
} // namespace Monitor