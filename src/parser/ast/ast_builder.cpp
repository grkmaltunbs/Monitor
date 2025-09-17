#include "ast_builder.h"

namespace Monitor {
namespace Parser {
namespace AST {

ASTBuilder::ASTBuilder() {
    // Constructor
}

std::unique_ptr<StructDeclaration> ASTBuilder::createStruct(const std::string& name) {
    return std::make_unique<StructDeclaration>(name);
}

std::unique_ptr<UnionDeclaration> ASTBuilder::createUnion(const std::string& name) {
    return std::make_unique<UnionDeclaration>(name);
}

std::unique_ptr<FieldDeclaration> ASTBuilder::createField(const std::string& name,
                                                        std::unique_ptr<TypeNode> type) {
    return std::make_unique<FieldDeclaration>(name, std::move(type));
}

std::unique_ptr<BitfieldDeclaration> ASTBuilder::createBitfield(const std::string& name,
                                                              std::unique_ptr<TypeNode> type,
                                                              uint32_t bitWidth) {
    return std::make_unique<BitfieldDeclaration>(name, std::move(type), bitWidth);
}

std::unique_ptr<PrimitiveType> ASTBuilder::createPrimitiveType(PrimitiveType::Kind kind) {
    return std::make_unique<PrimitiveType>(kind);
}

std::unique_ptr<PrimitiveType> ASTBuilder::createPrimitiveType(const std::string& typeName) {
    PrimitiveType::Kind kind = PrimitiveType::stringToKind(typeName);
    return std::make_unique<PrimitiveType>(kind);
}

std::unique_ptr<NamedType> ASTBuilder::createNamedType(const std::string& name) {
    return std::make_unique<NamedType>(name);
}

std::unique_ptr<ArrayType> ASTBuilder::createArrayType(std::unique_ptr<TypeNode> elementType,
                                                     size_t size) {
    return std::make_unique<ArrayType>(std::move(elementType), size);
}

std::unique_ptr<ArrayType> ASTBuilder::createMultidimensionalArrayType(std::unique_ptr<TypeNode> elementType,
                                                                     const std::vector<size_t>& dimensions) {
    return std::make_unique<ArrayType>(std::move(elementType), dimensions);
}

std::unique_ptr<PointerType> ASTBuilder::createPointerType(std::unique_ptr<TypeNode> pointeeType) {
    return std::make_unique<PointerType>(std::move(pointeeType));
}

std::unique_ptr<TypedefDeclaration> ASTBuilder::createTypedef(const std::string& name,
                                                            std::unique_ptr<TypeNode> underlyingType) {
    return std::make_unique<TypedefDeclaration>(name, std::move(underlyingType));
}

// Placeholder for PragmaDirective - would need to be implemented if header defines it
std::unique_ptr<PragmaDirective> ASTBuilder::createPragma(PragmaDirective::Type type,
                                                         const std::vector<std::string>& arguments) {
    (void)type;
    (void)arguments;
    // This would need PragmaDirective class implementation
    // For now, return nullptr to avoid compilation errors
    return nullptr;
}

} // namespace AST
} // namespace Parser
} // namespace Monitor