#include <QtTest/QtTest>
#include <memory>

#include "../../../src/parser/ast/ast_nodes.h"

using namespace Monitor::Parser::AST;

class TestPhase2Minimal : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testSourceLocation();
    void testPrimitiveTypeBasic();
    void testStructDeclarationBasic();

private:
    // Helper methods
};

void TestPhase2Minimal::initTestCase()
{
    // Initialize any global test resources
}

void TestPhase2Minimal::cleanupTestCase()
{
    // Cleanup global test resources
}

void TestPhase2Minimal::init()
{
    // Setup for each test
}

void TestPhase2Minimal::cleanup()
{
    // Cleanup after each test
}

void TestPhase2Minimal::testSourceLocation()
{
    SourceLocation loc(10, 20, 100, "test.cpp");
    
    QCOMPARE(loc.line, static_cast<size_t>(10));
    QCOMPARE(loc.column, static_cast<size_t>(20));
    QCOMPARE(loc.position, static_cast<size_t>(100));
    QCOMPARE(loc.filename, std::string("test.cpp"));
}

void TestPhase2Minimal::testPrimitiveTypeBasic()
{
    auto primitiveType = std::make_unique<PrimitiveType>(PrimitiveType::Kind::INT);
    
    QVERIFY(primitiveType != nullptr);
    QCOMPARE(primitiveType->getNodeType(), ASTNode::NodeType::PRIMITIVE_TYPE);
    QVERIFY(primitiveType->isPrimitive());
    QVERIFY(!primitiveType->isPointer());
    QVERIFY(!primitiveType->isArray());
    
    // These methods should exist and return reasonable values
    QVERIFY(primitiveType->getSize() > 0);
    QVERIFY(primitiveType->getAlignment() > 0);
    QVERIFY(!primitiveType->getTypeName().empty());
}

void TestPhase2Minimal::testStructDeclarationBasic()
{
    auto structDecl = std::make_unique<StructDeclaration>("TestStruct");
    
    QVERIFY(structDecl != nullptr);
    QCOMPARE(structDecl->getNodeType(), ASTNode::NodeType::STRUCT_DECLARATION);
    QVERIFY(structDecl->getFields().empty());
    QVERIFY(!structDecl->isPacked());
}

QTEST_MAIN(TestPhase2Minimal)
#include "test_phase2_minimal.moc"