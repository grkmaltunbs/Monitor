#include "ast_nodes.h"
#include "ast_visitor.h"

namespace Monitor {
namespace Parser {
namespace AST {

// StructDeclaration implementation
StructDeclaration::StructDeclaration(const std::string& name)
    : ASTNode(NodeType::STRUCT_DECLARATION), m_name(name) {}

void StructDeclaration::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void StructDeclaration::addField(std::unique_ptr<FieldDeclaration> field) {
    if (field) {
        m_fields.push_back(std::move(field));
    }
}

const FieldDeclaration* StructDeclaration::findField(const std::string& name) const {
    for (const auto& field : m_fields) {
        if (field && field->getName() == name) {
            return field.get();
        }
    }
    return nullptr;
}

// UnionDeclaration implementation
UnionDeclaration::UnionDeclaration(const std::string& name)
    : ASTNode(NodeType::UNION_DECLARATION), m_name(name) {}

void UnionDeclaration::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void UnionDeclaration::addMember(std::unique_ptr<FieldDeclaration> member) {
    if (member) {
        m_members.push_back(std::move(member));
    }
}

// FieldDeclaration implementation
FieldDeclaration::FieldDeclaration(const std::string& name, std::unique_ptr<TypeNode> type)
    : ASTNode(NodeType::FIELD_DECLARATION), m_name(name), m_type(std::move(type)) {}

void FieldDeclaration::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

// BitfieldDeclaration implementation
BitfieldDeclaration::BitfieldDeclaration(const std::string& name, std::unique_ptr<TypeNode> type, uint32_t bitWidth)
    : FieldDeclaration(name, std::move(type)), m_bitWidth(bitWidth) {
    m_nodeType = NodeType::BITFIELD_DECLARATION;
}

void BitfieldDeclaration::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string BitfieldDeclaration::getBaseTypeName() const {
    // TODO: Implement proper base type name extraction
    // For now, return a placeholder to allow linking
    if (m_type) {
        // This would need to properly extract the type name from the TypeNode
        return "int"; // Placeholder - most bitfields are int-based
    }
    return "unknown";
}

// PrimitiveType implementation
PrimitiveType::PrimitiveType(Kind kind)
    : TypeNode(NodeType::PRIMITIVE_TYPE), m_kind(kind) {}

void PrimitiveType::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

size_t PrimitiveType::getSize() const {
    switch (m_kind) {
        case Kind::VOID: return 0;
        case Kind::CHAR: case Kind::UNSIGNED_CHAR: case Kind::BOOL: return 1;
        case Kind::SHORT: case Kind::UNSIGNED_SHORT: return 2;
        case Kind::INT: case Kind::UNSIGNED_INT: case Kind::FLOAT: return 4;
        case Kind::LONG: case Kind::UNSIGNED_LONG: case Kind::DOUBLE: return 8;
        case Kind::LONG_LONG: case Kind::UNSIGNED_LONG_LONG: return 8;
        case Kind::LONG_DOUBLE: return 16;
        default: return 0;
    }
}

size_t PrimitiveType::getAlignment() const {
    return getSize();
}

std::string PrimitiveType::getTypeName() const {
    switch (m_kind) {
        case Kind::VOID: return "void";
        case Kind::CHAR: return "char";
        case Kind::UNSIGNED_CHAR: return "unsigned char";
        case Kind::SHORT: return "short";
        case Kind::UNSIGNED_SHORT: return "unsigned short";
        case Kind::INT: return "int";
        case Kind::UNSIGNED_INT: return "unsigned int";
        case Kind::LONG: return "long";
        case Kind::UNSIGNED_LONG: return "unsigned long";
        case Kind::LONG_LONG: return "long long";
        case Kind::UNSIGNED_LONG_LONG: return "unsigned long long";
        case Kind::FLOAT: return "float";
        case Kind::DOUBLE: return "double";
        case Kind::LONG_DOUBLE: return "long double";
        case Kind::BOOL: return "bool";
        default: return "unknown";
    }
}

// ArrayType implementation
ArrayType::ArrayType(std::unique_ptr<TypeNode> elementType, size_t size)
    : TypeNode(NodeType::ARRAY_TYPE), m_elementType(std::move(elementType)), m_arraySize(size) {}

void ArrayType::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

size_t ArrayType::getSize() const {
    return m_elementType ? m_elementType->getSize() * m_arraySize : 0;
}

size_t ArrayType::getAlignment() const {
    return m_elementType ? m_elementType->getAlignment() : 1;
}

std::string ArrayType::getTypeName() const {
    if (m_elementType) {
        return m_elementType->getTypeName() + "[" + std::to_string(m_arraySize) + "]";
    }
    return "unknown[]";
}

// PointerType implementation
PointerType::PointerType(std::unique_ptr<TypeNode> pointeeType)
    : TypeNode(NodeType::POINTER_TYPE), m_pointeeType(std::move(pointeeType)) {}

void PointerType::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

size_t PointerType::getSize() const {
    return sizeof(void*);
}

size_t PointerType::getAlignment() const {
    return sizeof(void*);
}

std::string PointerType::getTypeName() const {
    if (m_pointeeType) {
        return m_pointeeType->getTypeName() + "*";
    }
    return "void*";
}

// NamedType implementation
NamedType::NamedType(const std::string& name)
    : TypeNode(NodeType::NAMED_TYPE), m_name(name) {}

void NamedType::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

} // namespace AST
} // namespace Parser
} // namespace Monitor