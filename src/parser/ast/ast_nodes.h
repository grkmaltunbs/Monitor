#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Monitor {
namespace Parser {
namespace AST {

// Forward declarations
class ASTVisitor;

// Source location information
struct SourceLocation {
    size_t line = 0;
    size_t column = 0;
    size_t position = 0;
    std::string filename;
    
    SourceLocation() = default;
    SourceLocation(size_t l, size_t c, size_t p, const std::string& f = "")
        : line(l), column(c), position(p), filename(f) {}
    
    std::string toString() const;
};

// Base AST node
class ASTNode {
public:
    enum class NodeType {
        STRUCT_DECLARATION,
        UNION_DECLARATION,
        FIELD_DECLARATION,
        BITFIELD_DECLARATION,
        ARRAY_TYPE,
        POINTER_TYPE,
        PRIMITIVE_TYPE,
        TYPEDEF_DECLARATION,
        PRAGMA_DIRECTIVE,
        NAMED_TYPE
    };

    explicit ASTNode(NodeType type) : m_nodeType(type) {}
    virtual ~ASTNode() = default;
    
    NodeType getNodeType() const { return m_nodeType; }
    virtual void accept(ASTVisitor& visitor) = 0;
    
    // Source location
    SourceLocation location;
    std::string comment;
    
protected:
    NodeType m_nodeType;
};

// Type node base class
class TypeNode : public ASTNode {
public:
    explicit TypeNode(NodeType type) : ASTNode(type) {}
    virtual ~TypeNode() = default;
    
    virtual size_t getSize() const = 0;
    virtual size_t getAlignment() const = 0;
    virtual std::string getTypeName() const = 0;
    virtual bool isPrimitive() const { return false; }
    virtual bool isPointer() const { return false; }
    virtual bool isArray() const { return false; }
    virtual bool isStruct() const { return false; }
    virtual bool isUnion() const { return false; }
};

// Primitive type node
class PrimitiveType : public TypeNode {
public:
    enum class Kind {
        VOID,
        CHAR,
        SIGNED_CHAR,
        UNSIGNED_CHAR,
        SHORT,
        UNSIGNED_SHORT,
        INT,
        UNSIGNED_INT,
        LONG,
        UNSIGNED_LONG,
        LONG_LONG,
        UNSIGNED_LONG_LONG,
        FLOAT,
        DOUBLE,
        LONG_DOUBLE,
        BOOL
    };
    
    explicit PrimitiveType(Kind kind);
    
    void accept(ASTVisitor& visitor) override;
    
    size_t getSize() const override;
    size_t getAlignment() const override;
    std::string getTypeName() const override;
    bool isPrimitive() const override { return true; }
    
    Kind getKind() const { return m_kind; }
    bool isSigned() const;
    bool isInteger() const;
    bool isFloatingPoint() const;
    
    static Kind stringToKind(const std::string& typeName);
    static std::string kindToString(Kind kind);
    
private:
    Kind m_kind;
};

// Named type (for structs, unions, typedefs)
class NamedType : public TypeNode {
public:
    explicit NamedType(const std::string& name);
    
    void accept(ASTVisitor& visitor) override;
    
    size_t getSize() const override { return m_size; }
    size_t getAlignment() const override { return m_alignment; }
    std::string getTypeName() const override { return m_name; }
    
    const std::string& getName() const { return m_name; }
    
    // These will be resolved during type resolution phase
    void setSize(size_t size) { m_size = size; }
    void setAlignment(size_t alignment) { m_alignment = alignment; }
    void setResolvedType(std::shared_ptr<ASTNode> type) { m_resolvedType = type; }
    std::shared_ptr<ASTNode> getResolvedType() const { return m_resolvedType; }
    
private:
    std::string m_name;
    size_t m_size = 0;
    size_t m_alignment = 0;
    std::shared_ptr<ASTNode> m_resolvedType;
};

// Array type node
class ArrayType : public TypeNode {
public:
    ArrayType(std::unique_ptr<TypeNode> elementType, size_t size);
    ArrayType(std::unique_ptr<TypeNode> elementType, const std::vector<size_t>& dimensions);
    
    void accept(ASTVisitor& visitor) override;
    
    size_t getSize() const override;
    size_t getAlignment() const override;
    std::string getTypeName() const override;
    bool isArray() const override { return true; }
    
    const TypeNode* getElementType() const { return m_elementType.get(); }
    size_t getArraySize() const { return m_arraySize; }
    const std::vector<size_t>& getDimensions() const { return m_dimensions; }
    bool isMultidimensional() const { return m_dimensions.size() > 1; }
    
private:
    std::unique_ptr<TypeNode> m_elementType;
    size_t m_arraySize;
    std::vector<size_t> m_dimensions;
};

// Pointer type node
class PointerType : public TypeNode {
public:
    explicit PointerType(std::unique_ptr<TypeNode> pointeeType);
    
    void accept(ASTVisitor& visitor) override;
    
    size_t getSize() const override;
    size_t getAlignment() const override;
    std::string getTypeName() const override;
    bool isPointer() const override { return true; }
    
    const TypeNode* getPointeeType() const { return m_pointeeType.get(); }
    
private:
    std::unique_ptr<TypeNode> m_pointeeType;
};

// Field declaration
class FieldDeclaration : public ASTNode {
public:
    FieldDeclaration(const std::string& name, std::unique_ptr<TypeNode> type);
    
    void accept(ASTVisitor& visitor) override;
    
    const std::string& getName() const { return m_name; }
    const TypeNode* getType() const { return m_type.get(); }
    
    // Layout information (calculated during layout phase)
    size_t getOffset() const { return m_offset; }
    size_t getSize() const { return m_size; }
    size_t getAlignment() const { return m_alignment; }
    
    void setOffset(size_t offset) { m_offset = offset; }
    void setSize(size_t size) { m_size = size; }
    void setAlignment(size_t alignment) { m_alignment = alignment; }
    
    virtual bool isBitfield() const { return false; }
    
protected:
    std::string m_name;
    std::unique_ptr<TypeNode> m_type;
    size_t m_offset = 0;
    size_t m_size = 0;
    size_t m_alignment = 0;
};

// Bitfield declaration
class BitfieldDeclaration : public FieldDeclaration {
public:
    BitfieldDeclaration(const std::string& name, std::unique_ptr<TypeNode> type, uint32_t bitWidth);
    
    void accept(ASTVisitor& visitor) override;
    
    uint32_t getBitWidth() const { return m_bitWidth; }
    uint32_t getBitOffset() const { return m_bitOffset; }
    
    void setBitOffset(uint32_t bitOffset) { m_bitOffset = bitOffset; }
    
    bool isBitfield() const override { return true; }
    
    // Bit manipulation helpers
    uint64_t getBitMask() const;
    std::string getBaseTypeName() const;
    
private:
    uint32_t m_bitWidth;
    uint32_t m_bitOffset = 0;
};

// Structure declaration
class StructDeclaration : public ASTNode {
public:
    explicit StructDeclaration(const std::string& name);
    
    void accept(ASTVisitor& visitor) override;
    
    const std::string& getName() const { return m_name; }
    
    // Field management
    void addField(std::unique_ptr<FieldDeclaration> field);
    const std::vector<std::unique_ptr<FieldDeclaration>>& getFields() const { return m_fields; }
    size_t getFieldCount() const { return m_fields.size(); }
    
    const FieldDeclaration* findField(const std::string& name) const;
    
    // Layout information
    size_t getTotalSize() const { return m_totalSize; }
    size_t getAlignment() const { return m_alignment; }
    bool isPacked() const { return m_isPacked; }
    uint8_t getPackValue() const { return m_packValue; }
    
    void setTotalSize(size_t size) { m_totalSize = size; }
    void setAlignment(size_t alignment) { m_alignment = alignment; }
    void setPacked(bool packed) { m_isPacked = packed; }
    void setPackValue(uint8_t packValue) { m_packValue = packValue; }
    
    // Dependencies
    const std::vector<std::string>& getDependencies() const { return m_dependencies; }
    void addDependency(const std::string& typeName);
    
    bool isStruct() const { return true; }
    
private:
    std::string m_name;
    std::vector<std::unique_ptr<FieldDeclaration>> m_fields;
    std::vector<std::string> m_dependencies;
    
    // Layout information
    size_t m_totalSize = 0;
    size_t m_alignment = 0;
    bool m_isPacked = false;
    uint8_t m_packValue = 0;
};

// Union declaration
class UnionDeclaration : public ASTNode {
public:
    explicit UnionDeclaration(const std::string& name);
    
    void accept(ASTVisitor& visitor) override;
    
    const std::string& getName() const { return m_name; }
    
    // Member management
    void addMember(std::unique_ptr<FieldDeclaration> member);
    const std::vector<std::unique_ptr<FieldDeclaration>>& getMembers() const { return m_members; }
    size_t getMemberCount() const { return m_members.size(); }
    
    const FieldDeclaration* findMember(const std::string& name) const;
    
    // Layout information (union size = largest member size)
    size_t getTotalSize() const { return m_totalSize; }
    size_t getAlignment() const { return m_alignment; }
    
    void setTotalSize(size_t size) { m_totalSize = size; }
    void setAlignment(size_t alignment) { m_alignment = alignment; }
    
    // Dependencies
    const std::vector<std::string>& getDependencies() const { return m_dependencies; }
    void addDependency(const std::string& typeName);
    
    bool isUnion() const { return true; }
    
private:
    std::string m_name;
    std::vector<std::unique_ptr<FieldDeclaration>> m_members;
    std::vector<std::string> m_dependencies;
    
    size_t m_totalSize = 0;
    size_t m_alignment = 0;
};

// Typedef declaration
class TypedefDeclaration : public ASTNode {
public:
    TypedefDeclaration(const std::string& name, std::unique_ptr<TypeNode> underlyingType);
    
    void accept(ASTVisitor& visitor) override;
    
    const std::string& getName() const { return m_name; }
    const TypeNode* getUnderlyingType() const { return m_underlyingType.get(); }
    
private:
    std::string m_name;
    std::unique_ptr<TypeNode> m_underlyingType;
};

// Pragma directive
class PragmaDirective : public ASTNode {
public:
    enum class Type {
        PACK_PUSH,
        PACK_POP,
        PACK_SET,
        ATTRIBUTE_PACKED,
        ALIGNED,
        UNKNOWN
    };
    
    PragmaDirective(Type type, const std::vector<std::string>& arguments);
    
    void accept(ASTVisitor& visitor) override;
    
    Type getPragmaType() const { return m_pragmaType; }
    const std::vector<std::string>& getArguments() const { return m_arguments; }
    
    // Convenience methods for pack pragmas
    uint8_t getPackValue() const;
    std::string getPackIdentifier() const;
    
private:
    Type m_pragmaType;
    std::vector<std::string> m_arguments;
};

} // namespace AST
} // namespace Parser
} // namespace Monitor