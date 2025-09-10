#include <QtTest/QtTest>
#include <memory>

#include "../../../src/parser/ast/ast_nodes.h"

using namespace Monitor::Parser::AST;

class TestASTNodesSimple : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic AST node tests
    void testPrimitiveType();
    void testNamedType();
    void testArrayType();
    void testPointerType();
    void testFieldDeclaration();
    void testStructDeclaration();
    void testUnionDeclaration();
    void testTypedefDeclaration();
    void testPragmaDirective();
    void testSourceLocation();

private:
    // Helper methods for creating test nodes
    std::unique_ptr<PrimitiveType> createIntType();
    std::unique_ptr<FieldDeclaration> createSimpleField();
};

void TestASTNodesSimple::initTestCase()
{
    // Initialize any global test resources
}

void TestASTNodesSimple::cleanupTestCase()
{
    // Cleanup global test resources
}

void TestASTNodesSimple::init()
{
    // Setup for each test
}

void TestASTNodesSimple::cleanup()
{
    // Cleanup after each test
}

void TestASTNodesSimple::testPrimitiveType()
{
    auto primitiveType = std::make_unique<PrimitiveType>("int", 4, 4);
    
    QVERIFY(primitiveType != nullptr);
    QCOMPARE(primitiveType->getNodeType(), ASTNode::NodeType::PRIMITIVE_TYPE);
    QCOMPARE(primitiveType->getName(), std::string("int"));
    QCOMPARE(primitiveType->getSize(), static_cast<size_t>(4));
    QCOMPARE(primitiveType->getAlignment(), static_cast<size_t>(4));
}

void TestASTNodesSimple::testNamedType()
{
    auto namedType = std::make_unique<NamedType>("CustomStruct");
    
    QVERIFY(namedType != nullptr);
    QCOMPARE(namedType->getNodeType(), ASTNode::NodeType::NAMED_TYPE);
    QCOMPARE(namedType->getName(), std::string("CustomStruct"));
}

void TestASTNodesSimple::testArrayType()
{
    auto elementType = std::make_unique<PrimitiveType>("int", 4, 4);
    auto arrayType = std::make_unique<ArrayType>(std::move(elementType), 10);
    
    QVERIFY(arrayType != nullptr);
    QCOMPARE(arrayType->getNodeType(), ASTNode::NodeType::ARRAY_TYPE);
    QCOMPARE(arrayType->getSize(), static_cast<size_t>(10));
    QVERIFY(arrayType->getElementType() != nullptr);
}

void TestASTNodesSimple::testPointerType()
{
    auto targetType = std::make_unique<PrimitiveType>("int", 4, 4);
    auto pointerType = std::make_unique<PointerType>(std::move(targetType));
    
    QVERIFY(pointerType != nullptr);
    QCOMPARE(pointerType->getNodeType(), ASTNode::NodeType::POINTER_TYPE);
    QVERIFY(pointerType->getTargetType() != nullptr);
}

void TestASTNodesSimple::testFieldDeclaration()
{
    auto fieldType = std::make_unique<PrimitiveType>("int", 4, 4);
    auto field = std::make_unique<FieldDeclaration>("testField", std::move(fieldType));
    
    QVERIFY(field != nullptr);
    QCOMPARE(field->getNodeType(), ASTNode::NodeType::FIELD_DECLARATION);
    QCOMPARE(field->getName(), std::string("testField"));
    QVERIFY(field->getType() != nullptr);
    
    // Test default values
    QCOMPARE(field->getOffset(), static_cast<size_t>(0));
    QVERIFY(!field->isBitfield());
}

void TestASTNodesSimple::testStructDeclaration()
{
    auto structDecl = std::make_unique<StructDeclaration>("TestStruct");
    
    QVERIFY(structDecl != nullptr);
    QCOMPARE(structDecl->getNodeType(), ASTNode::NodeType::STRUCT_DECLARATION);
    QCOMPARE(structDecl->getName(), std::string("TestStruct"));
    QVERIFY(structDecl->getFields().empty());
    QCOMPARE(structDecl->getSize(), static_cast<size_t>(0));
    QCOMPARE(structDecl->getAlignment(), static_cast<size_t>(1));
    QVERIFY(!structDecl->isPacked());
    
    // Add a field
    auto fieldType = std::make_unique<PrimitiveType>("int", 4, 4);
    auto field = std::make_unique<FieldDeclaration>("testField", std::move(fieldType));
    structDecl->addField(std::move(field));
    
    QCOMPARE(structDecl->getFields().size(), static_cast<size_t>(1));
    QCOMPARE(structDecl->getFields()[0]->getName(), std::string("testField"));
}

void TestASTNodesSimple::testUnionDeclaration()
{
    auto unionDecl = std::make_unique<UnionDeclaration>("TestUnion");
    
    QVERIFY(unionDecl != nullptr);
    QCOMPARE(unionDecl->getNodeType(), ASTNode::NodeType::UNION_DECLARATION);
    QCOMPARE(unionDecl->getName(), std::string("TestUnion"));
    QVERIFY(unionDecl->getMembers().empty());
    QCOMPARE(unionDecl->getSize(), static_cast<size_t>(0));
    
    // Add members
    auto intType = std::make_unique<PrimitiveType>("int", 4, 4);
    auto intField = std::make_unique<FieldDeclaration>("intVal", std::move(intType));
    unionDecl->addMember(std::move(intField));
    
    auto floatType = std::make_unique<PrimitiveType>("float", 4, 4);
    auto floatField = std::make_unique<FieldDeclaration>("floatVal", std::move(floatType));
    unionDecl->addMember(std::move(floatField));
    
    QCOMPARE(unionDecl->getMembers().size(), static_cast<size_t>(2));
}

void TestASTNodesSimple::testTypedefDeclaration()
{
    auto targetType = std::make_unique<PrimitiveType>("int", 4, 4);
    auto typedefDecl = std::make_unique<TypedefDeclaration>("CustomInt", std::move(targetType));
    
    QVERIFY(typedefDecl != nullptr);
    QCOMPARE(typedefDecl->getNodeType(), ASTNode::NodeType::TYPEDEF_DECLARATION);
    QCOMPARE(typedefDecl->getName(), std::string("CustomInt"));
    QVERIFY(typedefDecl->getTargetType() != nullptr);
}

void TestASTNodesSimple::testPragmaDirective()
{
    auto pragma = std::make_unique<PragmaDirective>("pack", "1");
    
    QVERIFY(pragma != nullptr);
    QCOMPARE(pragma->getNodeType(), ASTNode::NodeType::PRAGMA_DIRECTIVE);
    QCOMPARE(pragma->getName(), std::string("pack"));
    QCOMPARE(pragma->getValue(), std::string("1"));
}

void TestASTNodesSimple::testSourceLocation()
{
    SourceLocation loc(10, 20, 100, "test.cpp");
    
    QCOMPARE(loc.line, static_cast<size_t>(10));
    QCOMPARE(loc.column, static_cast<size_t>(20));
    QCOMPARE(loc.position, static_cast<size_t>(100));
    QCOMPARE(loc.filename, std::string("test.cpp"));
    
    // Test toString method
    std::string locStr = loc.toString();
    QVERIFY(!locStr.empty());
    QVERIFY(locStr.find("10") != std::string::npos);
    QVERIFY(locStr.find("20") != std::string::npos);
}

// Helper method implementations
std::unique_ptr<PrimitiveType> TestASTNodesSimple::createIntType()
{
    return std::make_unique<PrimitiveType>("int", 4, 4);
}

std::unique_ptr<FieldDeclaration> TestASTNodesSimple::createSimpleField()
{
    auto fieldType = createIntType();
    return std::make_unique<FieldDeclaration>("testField", std::move(fieldType));
}

QTEST_MAIN(TestASTNodesSimple)
#include "test_ast_nodes_simple.moc"