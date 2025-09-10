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
 * @brief JSON deserializer for C++ structure definitions
 * 
 * This class converts JSON representations back into C++ structure
 * definitions and their layout information. It supports the complete
 * Monitor Application structure schema and provides validation.
 */
class JSONDeserializer {
public:
    struct DeserializationOptions {
        bool validateSchema = true;
        bool strictValidation = true;
        bool reconstructLayouts = true;
        bool validateLayouts = true;
        bool allowPartialReconstruction = false;
        bool skipInvalidEntries = false;
        
        DeserializationOptions() {}
    };
    
    struct DeserializationResult {
        std::vector<std::unique_ptr<AST::StructDeclaration>> structures;
        std::unordered_map<std::string, Layout::LayoutCalculator::StructLayout> structLayouts;
        std::vector<std::unique_ptr<AST::UnionDeclaration>> unions;
        std::unordered_map<std::string, Layout::LayoutCalculator::UnionLayout> unionLayouts;
        std::vector<std::unique_ptr<AST::TypedefDeclaration>> typedefs;
        
        bool success = false;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        size_t totalItems = 0;
        size_t successfulItems = 0;
        std::chrono::milliseconds deserializationTime{0};
        
        double getSuccessRatio() const {
            return totalItems > 0 ? static_cast<double>(successfulItems) / totalItems : 0.0;
        }
        
        void addError(const std::string& error) {
            errors.push_back(error);
            success = false;
        }
        
        void addWarning(const std::string& warning) {
            warnings.push_back(warning);
        }
    };
    
    explicit JSONDeserializer(const DeserializationOptions& options = DeserializationOptions());
    ~JSONDeserializer() = default;
    
    // Main deserialization methods
    std::unique_ptr<AST::StructDeclaration> deserializeStruct(const nlohmann::json& json);
    Layout::LayoutCalculator::StructLayout deserializeStructLayout(const nlohmann::json& json);
    
    std::unique_ptr<AST::UnionDeclaration> deserializeUnion(const nlohmann::json& json);
    Layout::LayoutCalculator::UnionLayout deserializeUnionLayout(const nlohmann::json& json);
    
    std::unique_ptr<AST::TypedefDeclaration> deserializeTypedef(const nlohmann::json& json);
    
    // Workspace deserialization
    DeserializationResult deserializeWorkspace(const nlohmann::json& workspace);
    DeserializationResult deserializeFromFile(const std::string& filePath);
    
    // Individual component deserialization
    std::unique_ptr<AST::FieldDeclaration> deserializeField(const nlohmann::json& fieldJson);
    std::unique_ptr<AST::BitfieldDeclaration> deserializeBitfield(const nlohmann::json& bitfieldJson);
    std::unique_ptr<AST::TypeNode> deserializeType(const nlohmann::json& typeJson);
    std::unique_ptr<AST::PragmaDirective> deserializePragma(const nlohmann::json& pragmaJson);
    
    // Layout deserialization
    Layout::LayoutCalculator::FieldLayout deserializeFieldLayout(const nlohmann::json& layoutJson);
    
    // Validation methods
    bool validateSchema(const nlohmann::json& json);
    bool validateWorkspace(const nlohmann::json& workspace);
    bool validateStruct(const nlohmann::json& structJson);
    bool validateUnion(const nlohmann::json& unionJson);
    bool validateField(const nlohmann::json& fieldJson);
    bool validateType(const nlohmann::json& typeJson);
    
    // Configuration
    void setOptions(const DeserializationOptions& options) { m_options = options; }
    const DeserializationOptions& getOptions() const { return m_options; }
    
    // Error handling and diagnostics
    struct DeserializationError {
        std::string message;
        std::string jsonPath;
        std::string expectedType;
        std::string actualValue;
        size_t line = 0;
        size_t column = 0;
        
        DeserializationError(const std::string& msg, const std::string& path = "",
                           const std::string& expected = "", const std::string& actual = "")
            : message(msg), jsonPath(path), expectedType(expected), actualValue(actual) {}
        
        std::string toString() const;
    };
    
    const std::vector<DeserializationError>& getErrors() const { return m_errors; }
    const std::vector<std::string>& getWarnings() const { return m_warnings; }
    bool hasErrors() const { return !m_errors.empty(); }
    void clearErrors() { m_errors.clear(); m_warnings.clear(); }
    
    // Statistics
    struct DeserializationStatistics {
        size_t structsDeserialized = 0;
        size_t unionsDeserialized = 0;
        size_t fieldsDeserialized = 0;
        size_t bitfieldsDeserialized = 0;
        size_t typedefsDeserialized = 0;
        size_t layoutsReconstructed = 0;
        size_t validationFailures = 0;
        size_t totalJsonNodes = 0;
        std::chrono::milliseconds validationTime{0};
        std::chrono::milliseconds reconstructionTime{0};
        std::chrono::milliseconds totalTime{0};
        
        size_t getTotalItems() const {
            return structsDeserialized + unionsDeserialized + fieldsDeserialized +
                   bitfieldsDeserialized + typedefsDeserialized;
        }
        
        void reset() {
            structsDeserialized = 0;
            unionsDeserialized = 0;
            fieldsDeserialized = 0;
            bitfieldsDeserialized = 0;
            typedefsDeserialized = 0;
            layoutsReconstructed = 0;
            validationFailures = 0;
            totalJsonNodes = 0;
            validationTime = std::chrono::milliseconds{0};
            reconstructionTime = std::chrono::milliseconds{0};
            totalTime = std::chrono::milliseconds{0};
        }
    };
    
    const DeserializationStatistics& getStatistics() const { return m_statistics; }
    void resetStatistics() { m_statistics.reset(); }
    
private:
    DeserializationOptions m_options;
    mutable std::vector<DeserializationError> m_errors;
    mutable std::vector<std::string> m_warnings;
    mutable DeserializationStatistics m_statistics;
    
    // JSON path tracking for error reporting
    std::string m_currentPath;
    
    // Helper methods for deserialization
    std::unique_ptr<AST::StructDeclaration> deserializeStructObject(const nlohmann::json& structObj);
    std::unique_ptr<AST::UnionDeclaration> deserializeUnionObject(const nlohmann::json& unionObj);
    std::unique_ptr<AST::FieldDeclaration> deserializeFieldObject(const nlohmann::json& fieldObj);
    std::unique_ptr<AST::BitfieldDeclaration> deserializeBitfieldObject(const nlohmann::json& bitfieldObj);
    
    // Type deserialization helpers
    std::unique_ptr<AST::PrimitiveType> deserializePrimitiveType(const nlohmann::json& typeObj);
    std::unique_ptr<AST::NamedType> deserializeNamedType(const nlohmann::json& typeObj);
    std::unique_ptr<AST::ArrayType> deserializeArrayType(const nlohmann::json& typeObj);
    std::unique_ptr<AST::PointerType> deserializePointerType(const nlohmann::json& typeObj);
    
    // Layout reconstruction helpers
    Layout::LayoutCalculator::StructLayout reconstructStructLayout(const nlohmann::json& layoutObj);
    Layout::LayoutCalculator::UnionLayout reconstructUnionLayout(const nlohmann::json& layoutObj);
    Layout::LayoutCalculator::FieldLayout reconstructFieldLayout(const nlohmann::json& layoutObj);
    
    // Validation helpers
    bool hasRequiredFields(const nlohmann::json& obj, const std::vector<std::string>& requiredFields);
    bool isValidType(const nlohmann::json& obj, const std::string& expectedType);
    bool isValidIdentifier(const std::string& identifier);
    bool isValidTypeName(const std::string& typeName);
    
    // JSON schema validation helpers
    bool validateStructSchema(const nlohmann::json& structObj);
    bool validateUnionSchema(const nlohmann::json& unionObj);
    bool validateFieldSchema(const nlohmann::json& fieldObj);
    bool validateTypeSchema(const nlohmann::json& typeObj);
    bool validateLayoutSchema(const nlohmann::json& layoutObj);
    
    // Error reporting helpers
    void addError(const std::string& message, const std::string& path = "");
    void addError(const std::string& message, const std::string& path,
                  const std::string& expected, const std::string& actual);
    void addWarning(const std::string& message, const std::string& path = "");
    
    // Path management for error reporting
    void enterPath(const std::string& segment);
    void exitPath();
    std::string getCurrentPath() const { return m_currentPath; }
    
    // Statistics tracking
    void trackDeserialization(AST::ASTNode::NodeType nodeType);
    void trackValidationFailure();
    void updateStatistics();
    
    // Utility methods
    std::string extractString(const nlohmann::json& obj, const std::string& key,
                            const std::string& defaultValue = "");
    size_t extractSize(const nlohmann::json& obj, const std::string& key, size_t defaultValue = 0);
    bool extractBool(const nlohmann::json& obj, const std::string& key, bool defaultValue = false);
    uint32_t extractUInt32(const nlohmann::json& obj, const std::string& key, uint32_t defaultValue = 0);
    uint64_t extractUInt64(const nlohmann::json& obj, const std::string& key, uint64_t defaultValue = 0);
    
    // JSON object type checking
    bool isStructObject(const nlohmann::json& obj);
    bool isUnionObject(const nlohmann::json& obj);
    bool isFieldObject(const nlohmann::json& obj);
    bool isBitfieldObject(const nlohmann::json& obj);
    bool isTypeObject(const nlohmann::json& obj);
    bool isLayoutObject(const nlohmann::json& obj);
    
    // Bitfield helpers
    uint64_t parseBitMask(const nlohmann::json& maskObj);
    AST::PragmaDirective::Type parsePragmaType(const std::string& typeStr);
    AST::PrimitiveType::Kind parsePrimitiveKind(const std::string& kindStr);
    
    // Source location reconstruction
    AST::SourceLocation reconstructSourceLocation(const nlohmann::json& locationObj);
    
    // Default value providers
    Layout::LayoutCalculator::StructLayout createDefaultStructLayout();
    Layout::LayoutCalculator::UnionLayout createDefaultUnionLayout();
    Layout::LayoutCalculator::FieldLayout createDefaultFieldLayout();
};

/**
 * @brief RAII helper class for JSON path tracking
 */
class ScopedJSONPath {
public:
    explicit ScopedJSONPath(JSONDeserializer& deserializer, const std::string& pathSegment);
    ~ScopedJSONPath();
    
    ScopedJSONPath(const ScopedJSONPath&) = delete;
    ScopedJSONPath& operator=(const ScopedJSONPath&) = delete;
    
private:
    JSONDeserializer& m_deserializer;
};

} // namespace Serialization
} // namespace Parser
} // namespace Monitor