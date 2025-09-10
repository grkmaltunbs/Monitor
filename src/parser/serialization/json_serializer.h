#pragma once

#include "../ast/ast_nodes.h"
#include "../layout/layout_calculator.h"
#include "json_mock.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace Monitor {
namespace Parser {
namespace Serialization {

/**
 * @brief JSON serializer for C++ structure definitions
 * 
 * This class converts parsed C++ structures and their layout information
 * into JSON format for persistence and data exchange. It supports the
 * complete Monitor Application structure schema.
 */
class JSONSerializer {
public:
    struct SerializationOptions {
        bool includeComments = true;
        bool includeSourceLocations = false;
        bool prettyPrint = true;
        bool includeBitMasks = true;
        bool includeLayoutCalculations = true;
        bool includePaddingInfo = true;
        bool includeStatistics = true;
        bool includeCompilerInfo = true;
        bool validateOnSerialize = true;
        int indentSize = 2;
        
        SerializationOptions() {}
    };
    
    explicit JSONSerializer(const SerializationOptions& options = SerializationOptions());
    ~JSONSerializer() = default;
    
    // Main serialization methods
    nlohmann::json serialize(const AST::StructDeclaration& structDecl,
                           const Layout::LayoutCalculator::StructLayout& layout) const;
    
    nlohmann::json serialize(const AST::UnionDeclaration& unionDecl,
                           const Layout::LayoutCalculator::UnionLayout& layout) const;
    
    nlohmann::json serialize(const AST::TypedefDeclaration& typedefDecl) const;
    
    // Batch serialization for complete workspaces
    nlohmann::json serializeWorkspace(
        const std::vector<std::unique_ptr<AST::StructDeclaration>>& structures,
        const std::unordered_map<std::string, Layout::LayoutCalculator::StructLayout>& structLayouts,
        const std::vector<std::unique_ptr<AST::UnionDeclaration>>& unions = {},
        const std::unordered_map<std::string, Layout::LayoutCalculator::UnionLayout>& unionLayouts = {},
        const std::vector<std::unique_ptr<AST::TypedefDeclaration>>& typedefs = {}) const;
    
    // Individual component serialization
    nlohmann::json serializeField(const AST::FieldDeclaration& field,
                                const Layout::LayoutCalculator::FieldLayout& layout) const;
    
    nlohmann::json serializeBitfield(const AST::BitfieldDeclaration& bitfield,
                                   const Layout::LayoutCalculator::FieldLayout& layout) const;
    
    nlohmann::json serializeType(const AST::TypeNode& type) const;
    
    nlohmann::json serializePragmas(const std::vector<AST::PragmaDirective*>& pragmas) const;
    
    // Layout-specific serialization
    nlohmann::json serializeStructLayout(const Layout::LayoutCalculator::StructLayout& layout,
                                       const std::string& structName) const;
    
    nlohmann::json serializeUnionLayout(const Layout::LayoutCalculator::UnionLayout& layout,
                                      const std::string& unionName) const;
    
    nlohmann::json serializeFieldLayout(const Layout::LayoutCalculator::FieldLayout& layout,
                                      const std::string& fieldName) const;
    
    // Compiler and platform information
    nlohmann::json serializeCompilerInfo(const Layout::AlignmentRules& rules) const;
    
    // Configuration
    void setOptions(const SerializationOptions& options) { m_options = options; }
    const SerializationOptions& getOptions() const { return m_options; }
    
    // Validation
    bool validateJSON(const nlohmann::json& json) const;
    std::vector<std::string> getValidationErrors(const nlohmann::json& json) const;
    
    // Schema generation
    static nlohmann::json generateSchema();
    
    // Statistics and diagnostics
    struct SerializationStatistics {
        size_t structsSerialized = 0;
        size_t unionsSerialized = 0;
        size_t fieldsSerialized = 0;
        size_t bitfieldsSerialized = 0;
        size_t typedefsSerialized = 0;
        size_t totalNodes = 0;
        size_t jsonSizeBytes = 0;
        std::chrono::milliseconds serializationTime{0};
        
        void reset() {
            structsSerialized = 0;
            unionsSerialized = 0;
            fieldsSerialized = 0;
            bitfieldsSerialized = 0;
            typedefsSerialized = 0;
            totalNodes = 0;
            jsonSizeBytes = 0;
            serializationTime = std::chrono::milliseconds{0};
        }
    };
    
    const SerializationStatistics& getStatistics() const { return m_statistics; }
    void resetStatistics() { m_statistics.reset(); }
    
    // File operations
    bool saveToFile(const nlohmann::json& json, const std::string& filePath) const;
    bool saveWorkspaceToFile(
        const std::vector<std::unique_ptr<AST::StructDeclaration>>& structures,
        const std::unordered_map<std::string, Layout::LayoutCalculator::StructLayout>& layouts,
        const std::string& filePath) const;
    
    // Error handling
    struct SerializationError {
        std::string message;
        std::string context;
        std::string fieldName;
        size_t position;
        
        SerializationError(const std::string& msg, const std::string& ctx = "",
                         const std::string& field = "", size_t pos = 0)
            : message(msg), context(ctx), fieldName(field), position(pos) {}
    };
    
    const std::vector<SerializationError>& getErrors() const { return m_errors; }
    bool hasErrors() const { return !m_errors.empty(); }
    void clearErrors() { m_errors.clear(); }
    
private:
    SerializationOptions m_options;
    mutable SerializationStatistics m_statistics;
    mutable std::vector<SerializationError> m_errors;
    
    // Helper methods for serialization
    nlohmann::json createStructObject(const AST::StructDeclaration& structDecl,
                                    const Layout::LayoutCalculator::StructLayout& layout) const;
    
    nlohmann::json createUnionObject(const AST::UnionDeclaration& unionDecl,
                                   const Layout::LayoutCalculator::UnionLayout& layout) const;
    
    nlohmann::json createFieldObject(const AST::FieldDeclaration& field,
                                   const Layout::LayoutCalculator::FieldLayout& layout) const;
    
    nlohmann::json createBitfieldObject(const AST::BitfieldDeclaration& bitfield,
                                      const Layout::LayoutCalculator::FieldLayout& layout) const;
    
    nlohmann::json createTypeObject(const AST::TypeNode& type) const;
    
    nlohmann::json createPragmaObject(const AST::PragmaDirective& pragma) const;
    
    // Type-specific serialization helpers
    nlohmann::json serializePrimitiveType(const AST::PrimitiveType& primitiveType) const;
    nlohmann::json serializeNamedType(const AST::NamedType& namedType) const;
    nlohmann::json serializeArrayType(const AST::ArrayType& arrayType) const;
    nlohmann::json serializePointerType(const AST::PointerType& pointerType) const;
    
    // Layout information helpers
    nlohmann::json createLayoutObject(const Layout::LayoutCalculator::StructLayout& layout) const;
    nlohmann::json createFieldLayoutObject(const Layout::LayoutCalculator::FieldLayout& layout) const;
    nlohmann::json createBitfieldLayoutObject(const Layout::LayoutCalculator::FieldLayout& layout) const;
    nlohmann::json createPaddingArray(const std::vector<size_t>& paddingLocations) const;
    
    // Bitfield helpers
    nlohmann::json createBitMaskObject(uint64_t mask) const;
    std::string formatBitMask(uint64_t mask, uint32_t bitWidth) const;
    
    // Metadata and statistics
    nlohmann::json createMetadataObject() const;
    nlohmann::json createStatisticsObject(const SerializationStatistics& stats) const;
    
    // Validation helpers
    bool validateStructObject(const nlohmann::json& structObj) const;
    bool validateUnionObject(const nlohmann::json& unionObj) const;
    bool validateFieldObject(const nlohmann::json& fieldObj) const;
    bool validateTypeObject(const nlohmann::json& typeObj) const;
    
    // Error handling helpers
    void addError(const std::string& message, const std::string& context = "",
                  const std::string& fieldName = "", size_t position = 0) const;
    
    // Utility methods
    std::string formatComment(const std::string& comment) const;
    std::string formatSourceLocation(const AST::SourceLocation& location) const;
    std::string getTimestamp() const;
    
    // JSON formatting
    std::string formatJSON(const nlohmann::json& json) const;
    
    // Statistics tracking
    void updateStatistics(const nlohmann::json& json) const;
    void trackSerialization(AST::ASTNode::NodeType nodeType) const;
};

} // namespace Serialization
} // namespace Parser
} // namespace Monitor