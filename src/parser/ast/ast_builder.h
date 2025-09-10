#pragma once

#include "ast_nodes.h"
#include "../lexer/token_types.h"
#include <memory>
#include <vector>
#include <stack>

namespace Monitor {
namespace Parser {
namespace AST {

/**
 * @brief Builder class for constructing AST nodes
 * 
 * This class provides a convenient interface for creating and managing
 * AST nodes during the parsing process. It handles memory management
 * and provides validation for node construction.
 */
class ASTBuilder {
public:
    explicit ASTBuilder();
    ~ASTBuilder() = default;
    
    // Factory methods for creating AST nodes
    std::unique_ptr<StructDeclaration> createStruct(const std::string& name);
    std::unique_ptr<UnionDeclaration> createUnion(const std::string& name);
    
    std::unique_ptr<FieldDeclaration> createField(const std::string& name, 
                                                std::unique_ptr<TypeNode> type);
    std::unique_ptr<BitfieldDeclaration> createBitfield(const std::string& name,
                                                      std::unique_ptr<TypeNode> type,
                                                      uint32_t bitWidth);
    
    // Type node creation
    std::unique_ptr<PrimitiveType> createPrimitiveType(PrimitiveType::Kind kind);
    std::unique_ptr<PrimitiveType> createPrimitiveType(const std::string& typeName);
    std::unique_ptr<NamedType> createNamedType(const std::string& name);
    std::unique_ptr<ArrayType> createArrayType(std::unique_ptr<TypeNode> elementType, 
                                             size_t size);
    std::unique_ptr<ArrayType> createMultidimensionalArrayType(std::unique_ptr<TypeNode> elementType,
                                                             const std::vector<size_t>& dimensions);
    std::unique_ptr<PointerType> createPointerType(std::unique_ptr<TypeNode> pointeeType);
    
    std::unique_ptr<TypedefDeclaration> createTypedef(const std::string& name,
                                                    std::unique_ptr<TypeNode> underlyingType);
    
    std::unique_ptr<PragmaDirective> createPragma(PragmaDirective::Type type,
                                                 const std::vector<std::string>& arguments);
    
    // Context management for nested structures
    struct BuildContext {
        std::string currentStructName;
        std::stack<std::string> nestedStructs;
        std::vector<std::string> pragmaStack;
        uint8_t currentPackValue = 0;
        bool inPackedContext = false;
    };
    
    BuildContext& getContext() { return m_context; }
    const BuildContext& getContext() const { return m_context; }
    
    // Validation methods
    bool validateStructName(const std::string& name) const;
    bool validateFieldName(const std::string& name) const;
    bool validateBitfieldWidth(uint32_t width, const TypeNode* baseType) const;
    
    // Source location management
    void setCurrentLocation(const SourceLocation& location) { m_currentLocation = location; }
    const SourceLocation& getCurrentLocation() const { return m_currentLocation; }
    
    // Error handling
    struct BuildError {
        std::string message;
        SourceLocation location;
        std::string context;
        
        BuildError(const std::string& msg, const SourceLocation& loc, const std::string& ctx = "")
            : message(msg), location(loc), context(ctx) {}
    };
    
    const std::vector<BuildError>& getErrors() const { return m_errors; }
    bool hasErrors() const { return !m_errors.empty(); }
    void clearErrors() { m_errors.clear(); }
    
    // Statistics
    struct BuildStatistics {
        size_t structsCreated = 0;
        size_t unionsCreated = 0;
        size_t fieldsCreated = 0;
        size_t bitfieldsCreated = 0;
        size_t typesCreated = 0;
        size_t typedefsCreated = 0;
        size_t pragmasCreated = 0;
        
        size_t getTotalNodes() const {
            return structsCreated + unionsCreated + fieldsCreated + 
                   bitfieldsCreated + typesCreated + typedefsCreated + pragmasCreated;
        }
    };
    
    const BuildStatistics& getStatistics() const { return m_statistics; }
    void resetStatistics() { m_statistics = BuildStatistics(); }
    
private:
    BuildContext m_context;
    SourceLocation m_currentLocation;
    std::vector<BuildError> m_errors;
    BuildStatistics m_statistics;
    
    // Helper methods
    void addError(const std::string& message, const std::string& context = "");
    void updateStatistics(ASTNode::NodeType nodeType);
    bool isValidIdentifier(const std::string& identifier) const;
    bool isReservedKeyword(const std::string& identifier) const;
    
    // Type validation helpers
    bool isValidBitfieldBaseType(const TypeNode* type) const;
    uint32_t getMaxBitfieldWidth(const TypeNode* type) const;
    bool isPrimitiveTypeName(const std::string& name) const;
};

/**
 * @brief Helper class for automatic context management
 * 
 * RAII helper to automatically manage build context during
 * nested structure parsing.
 */
class ScopedBuildContext {
public:
    explicit ScopedBuildContext(ASTBuilder& builder, const std::string& contextName);
    ~ScopedBuildContext();
    
    ScopedBuildContext(const ScopedBuildContext&) = delete;
    ScopedBuildContext& operator=(const ScopedBuildContext&) = delete;
    
private:
    ASTBuilder& m_builder;
    std::string m_previousContext;
};

} // namespace AST
} // namespace Parser
} // namespace Monitor