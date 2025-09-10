#pragma once

namespace Monitor {
namespace Parser {
namespace AST {

// Forward declarations
class StructDeclaration;
class UnionDeclaration;
class FieldDeclaration;
class BitfieldDeclaration;
class ArrayType;
class PointerType;
class PrimitiveType;
class NamedType;
class TypedefDeclaration;
class PragmaDirective;

/**
 * @brief Abstract visitor interface for AST nodes
 * 
 * This implements the visitor pattern to allow different operations
 * on the AST without modifying the node classes themselves.
 */
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    
    // Structure and union declarations
    virtual void visit(StructDeclaration& node) = 0;
    virtual void visit(UnionDeclaration& node) = 0;
    
    // Field declarations
    virtual void visit(FieldDeclaration& node) = 0;
    virtual void visit(BitfieldDeclaration& node) = 0;
    
    // Type nodes
    virtual void visit(PrimitiveType& node) = 0;
    virtual void visit(NamedType& node) = 0;
    virtual void visit(ArrayType& node) = 0;
    virtual void visit(PointerType& node) = 0;
    
    // Other declarations
    virtual void visit(TypedefDeclaration& node) = 0;
    virtual void visit(PragmaDirective& node) = 0;
    
protected:
    // Helper methods for common visitor patterns
    virtual void beforeVisit() {}
    virtual void afterVisit() {}
    virtual bool shouldContinue() { return true; }
};

/**
 * @brief Base visitor with default implementations
 * 
 * Provides no-op implementations of all visit methods,
 * allowing derived classes to only override what they need.
 */
class DefaultASTVisitor : public ASTVisitor {
public:
    void visit(StructDeclaration&) override {}
    void visit(UnionDeclaration&) override {}
    void visit(FieldDeclaration&) override {}
    void visit(BitfieldDeclaration&) override {}
    void visit(PrimitiveType&) override {}
    void visit(NamedType&) override {}
    void visit(ArrayType&) override {}
    void visit(PointerType&) override {}
    void visit(TypedefDeclaration&) override {}
    void visit(PragmaDirective&) override {}
};

/**
 * @brief Visitor that recursively traverses the entire AST
 * 
 * This visitor automatically visits all child nodes, making it
 * easy to implement operations that need to process the entire tree.
 */
class RecursiveASTVisitor : public ASTVisitor {
public:
    void visit(StructDeclaration& node) override;
    void visit(UnionDeclaration& node) override;
    void visit(FieldDeclaration& node) override;
    void visit(BitfieldDeclaration& node) override;
    void visit(PrimitiveType& node) override;
    void visit(NamedType& node) override;
    void visit(ArrayType& node) override;
    void visit(PointerType& node) override;
    void visit(TypedefDeclaration& node) override;
    void visit(PragmaDirective& node) override;
    
protected:
    // Override these in derived classes to add custom behavior
    virtual void visitStructDeclaration(StructDeclaration&) {}
    virtual void visitUnionDeclaration(UnionDeclaration&) {}
    virtual void visitFieldDeclaration(FieldDeclaration&) {}
    virtual void visitBitfieldDeclaration(BitfieldDeclaration&) {}
    virtual void visitPrimitiveType(PrimitiveType&) {}
    virtual void visitNamedType(NamedType&) {}
    virtual void visitArrayType(ArrayType&) {}
    virtual void visitPointerType(PointerType&) {}
    virtual void visitTypedefDeclaration(TypedefDeclaration&) {}
    virtual void visitPragmaDirective(PragmaDirective&) {}
};

} // namespace AST
} // namespace Parser
} // namespace Monitor