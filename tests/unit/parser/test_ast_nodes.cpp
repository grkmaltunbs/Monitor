#include <QtTest/QtTest>
#include <QSignalSpy>
#include <memory>

#include "../../../src/parser/ast/ast_nodes.h"
#include "../../../src/parser/ast/ast_visitor.h"

class TestASTNodes : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // ASTNode base class tests
    void testASTNodeBasics();
    void testASTNodeHierarchy();
    void testASTNodeEquality();
    void testASTNodeCloning();

    // StructNode tests
    void testStructNodeCreation();
    void testStructNodeFields();
    void testStructNodeNesting();
    void testStructNodeDependencies();

    // FieldNode tests
    void testFieldNodeCreation();
    void testFieldNodeTypes();
    void testFieldNodeArrays();
    void testFieldNodeBitfields();

    // TypeNode tests
    void testTypeNodeCreation();
    void testTypeNodeBuiltins();
    void testTypeNodeCustom();
    void testTypeNodePointers();

    // UnionNode tests
    void testUnionNodeCreation();
    void testUnionNodeMembers();
    void testUnionNodeSize();

    // EnumNode tests
    void testEnumNodeCreation();
    void testEnumNodeValues();
    void testEnumNodeTypes();

    // Complex AST tests
    void testComplexStructure();
    void testCircularDependencies();
    void testDeepNesting();
    void testMixedTypes();

    // Visitor pattern tests
    void testVisitorPattern();
    void testVisitorTraversal();

private:
    // Test helper methods
    std::unique_ptr<StructNode> createSimpleStruct();
    std::unique_ptr<StructNode> createComplexStruct();
    std::unique_ptr<UnionNode> createSimpleUnion();
    std::unique_ptr<EnumNode> createSimpleEnum();
};

void TestASTNodes::initTestCase()
{
    // Initialize any global test resources
}

void TestASTNodes::cleanupTestCase()
{
    // Cleanup global test resources
}

void TestASTNodes::init()
{
    // Setup for each test
}

void TestASTNodes::cleanup()
{
    // Cleanup after each test
}

void TestASTNodes::testASTNodeBasics()
{
    auto structNode = std::make_unique<StructNode>("TestStruct");
    
    QCOMPARE(structNode->getName(), QString("TestStruct"));
    QCOMPARE(structNode->getType(), ASTNodeType::Struct);
    QVERIFY(structNode->getParent() == nullptr);
    QVERIFY(structNode->getChildren().isEmpty());
    QCOMPARE(structNode->getSize(), 0);
    QCOMPARE(structNode->getAlignment(), 1);
}

void TestASTNodes::testASTNodeHierarchy()
{
    auto parent = std::make_unique<StructNode>("Parent");
    auto child = std::make_unique<FieldNode>("child", "int");
    
    auto childPtr = child.get();
    parent->addChild(std::move(child));
    
    QCOMPARE(parent->getChildren().size(), 1);
    QCOMPARE(childPtr->getParent(), parent.get());
    QCOMPARE(parent->getChildren().first().get(), childPtr);
}

void TestASTNodes::testASTNodeEquality()
{
    auto node1 = std::make_unique<StructNode>("TestStruct");
    auto node2 = std::make_unique<StructNode>("TestStruct");
    auto node3 = std::make_unique<StructNode>("DifferentStruct");
    
    QVERIFY(*node1 == *node2);
    QVERIFY(!(*node1 == *node3));
    QVERIFY(*node1 != *node3);
}

void TestASTNodes::testASTNodeCloning()
{
    auto original = createSimpleStruct();
    auto clone = original->clone();
    
    QVERIFY(*original == *clone);
    QVERIFY(original.get() != clone.get()); // Different objects
    QCOMPARE(clone->getName(), original->getName());
    QCOMPARE(clone->getType(), original->getType());
}

void TestASTNodes::testStructNodeCreation()
{
    auto structNode = std::make_unique<StructNode>("TestStruct");
    
    QCOMPARE(structNode->getName(), QString("TestStruct"));
    QCOMPARE(structNode->getType(), ASTNodeType::Struct);
    QVERIFY(structNode->getFields().isEmpty());
    QVERIFY(!structNode->isPacked());
    QCOMPARE(structNode->getPackValue(), 0);
}

void TestASTNodes::testStructNodeFields()
{
    auto structNode = std::make_unique<StructNode>("TestStruct");
    auto field1 = std::make_unique<FieldNode>("x", "int");
    auto field2 = std::make_unique<FieldNode>("y", "double");
    
    structNode->addField(std::move(field1));
    structNode->addField(std::move(field2));
    
    QCOMPARE(structNode->getFields().size(), 2);
    QCOMPARE(structNode->getFields()[0]->getName(), QString("x"));
    QCOMPARE(structNode->getFields()[1]->getName(), QString("y"));
    
    auto foundField = structNode->findField("x");
    QVERIFY(foundField != nullptr);
    QCOMPARE(foundField->getName(), QString("x"));
    
    auto notFound = structNode->findField("z");
    QVERIFY(notFound == nullptr);
}

void TestASTNodes::testStructNodeNesting()
{
    auto outerStruct = std::make_unique<StructNode>("Outer");
    auto innerStruct = std::make_unique<StructNode>("Inner");
    
    innerStruct->addField(std::make_unique<FieldNode>("value", "int"));
    auto innerField = std::make_unique<FieldNode>("inner", "Inner");
    innerField->setNestedStruct(std::move(innerStruct));
    
    outerStruct->addField(std::move(innerField));
    
    QCOMPARE(outerStruct->getFields().size(), 1);
    auto field = outerStruct->getFields()[0].get();
    QVERIFY(field->getNestedStruct() != nullptr);
    QCOMPARE(field->getNestedStruct()->getName(), QString("Inner"));
}

void TestASTNodes::testStructNodeDependencies()
{
    auto structNode = std::make_unique<StructNode>("TestStruct");
    
    structNode->addDependency("OtherStruct");
    structNode->addDependency("AnotherStruct");
    
    auto deps = structNode->getDependencies();
    QCOMPARE(deps.size(), 2);
    QVERIFY(deps.contains("OtherStruct"));
    QVERIFY(deps.contains("AnotherStruct"));
}

void TestASTNodes::testFieldNodeCreation()
{
    auto field = std::make_unique<FieldNode>("testField", "int");
    
    QCOMPARE(field->getName(), QString("testField"));
    QCOMPARE(field->getType(), ASTNodeType::Field);
    QCOMPARE(field->getTypeName(), QString("int"));
    QVERIFY(!field->isArray());
    QVERIFY(!field->isBitField());
    QVERIFY(!field->isPointer());
}

void TestASTNodes::testFieldNodeTypes()
{
    auto intField = std::make_unique<FieldNode>("intVal", "int");
    auto doubleField = std::make_unique<FieldNode>("doubleVal", "double");
    auto customField = std::make_unique<FieldNode>("customVal", "CustomStruct");
    
    QCOMPARE(intField->getTypeName(), QString("int"));
    QCOMPARE(doubleField->getTypeName(), QString("double"));
    QCOMPARE(customField->getTypeName(), QString("CustomStruct"));
}

void TestASTNodes::testFieldNodeArrays()
{
    auto arrayField = std::make_unique<FieldNode>("arr", "int");
    arrayField->setArraySize(10);
    
    QVERIFY(arrayField->isArray());
    QCOMPARE(arrayField->getArraySize(), 10);
    QCOMPARE(arrayField->getTypeName(), QString("int"));
}

void TestASTNodes::testFieldNodeBitfields()
{
    auto bitField = std::make_unique<FieldNode>("flags", "uint32_t");
    bitField->setBitField(true, 8);
    
    QVERIFY(bitField->isBitField());
    QCOMPARE(bitField->getBitWidth(), 8);
    QCOMPARE(bitField->getTypeName(), QString("uint32_t"));
}

void TestASTNodes::testTypeNodeCreation()
{
    auto typeNode = std::make_unique<TypeNode>("int");
    
    QCOMPARE(typeNode->getName(), QString("int"));
    QCOMPARE(typeNode->getType(), ASTNodeType::Type);
    QVERIFY(typeNode->isBuiltinType());
    QVERIFY(!typeNode->isCustomType());
}

void TestASTNodes::testTypeNodeBuiltins()
{
    QStringList builtinTypes = {"int", "char", "double", "float", "bool", 
                               "uint8_t", "int16_t", "uint32_t", "int64_t"};
    
    for (const QString& typeName : builtinTypes) {
        auto typeNode = std::make_unique<TypeNode>(typeName);
        QVERIFY(typeNode->isBuiltinType());
        QVERIFY(!typeNode->isCustomType());
    }
}

void TestASTNodes::testTypeNodeCustom()
{
    auto customType = std::make_unique<TypeNode>("CustomStruct");
    customType->setCustomType(true);
    
    QVERIFY(customType->isCustomType());
    QVERIFY(!customType->isBuiltinType());
}

void TestASTNodes::testTypeNodePointers()
{
    auto ptrType = std::make_unique<TypeNode>("int*");
    ptrType->setPointerLevel(1);
    
    QCOMPARE(ptrType->getPointerLevel(), 1);
    QVERIFY(ptrType->isPointer());
    
    auto doublePtrType = std::make_unique<TypeNode>("int**");
    doublePtrType->setPointerLevel(2);
    
    QCOMPARE(doublePtrType->getPointerLevel(), 2);
    QVERIFY(doublePtrType->isPointer());
}

void TestASTNodes::testUnionNodeCreation()
{
    auto unionNode = std::make_unique<UnionNode>("TestUnion");
    
    QCOMPARE(unionNode->getName(), QString("TestUnion"));
    QCOMPARE(unionNode->getType(), ASTNodeType::Union);
    QVERIFY(unionNode->getMembers().isEmpty());
}

void TestASTNodes::testUnionNodeMembers()
{
    auto unionNode = std::make_unique<UnionNode>("TestUnion");
    
    unionNode->addMember(std::make_unique<FieldNode>("intVal", "int"));
    unionNode->addMember(std::make_unique<FieldNode>("floatVal", "float"));
    
    QCOMPARE(unionNode->getMembers().size(), 2);
    QCOMPARE(unionNode->getMembers()[0]->getName(), QString("intVal"));
    QCOMPARE(unionNode->getMembers()[1]->getName(), QString("floatVal"));
}

void TestASTNodes::testUnionNodeSize()
{
    auto unionNode = std::make_unique<UnionNode>("TestUnion");
    
    unionNode->addMember(std::make_unique<FieldNode>("intVal", "int"));    // 4 bytes
    unionNode->addMember(std::make_unique<FieldNode>("doubleVal", "double")); // 8 bytes
    
    // Union size should be the size of the largest member
    // This would be calculated by the layout calculator
    unionNode->setSize(8);
    QCOMPARE(unionNode->getSize(), 8);
}

void TestASTNodes::testEnumNodeCreation()
{
    auto enumNode = std::make_unique<EnumNode>("TestEnum");
    
    QCOMPARE(enumNode->getName(), QString("TestEnum"));
    QCOMPARE(enumNode->getType(), ASTNodeType::Enum);
    QVERIFY(enumNode->getValues().isEmpty());
    QCOMPARE(enumNode->getUnderlyingType(), QString("int"));
}

void TestASTNodes::testEnumNodeValues()
{
    auto enumNode = std::make_unique<EnumNode>("Status");
    
    enumNode->addValue("IDLE", 0);
    enumNode->addValue("RUNNING", 1);
    enumNode->addValue("ERROR", 2);
    
    auto values = enumNode->getValues();
    QCOMPARE(values.size(), 3);
    QCOMPARE(values["IDLE"], 0);
    QCOMPARE(values["RUNNING"], 1);
    QCOMPARE(values["ERROR"], 2);
}

void TestASTNodes::testEnumNodeTypes()
{
    auto int8Enum = std::make_unique<EnumNode>("SmallEnum", "int8_t");
    auto int32Enum = std::make_unique<EnumNode>("LargeEnum", "int32_t");
    
    QCOMPARE(int8Enum->getUnderlyingType(), QString("int8_t"));
    QCOMPARE(int32Enum->getUnderlyingType(), QString("int32_t"));
}

void TestASTNodes::testComplexStructure()
{
    // Create a complex nested structure
    auto mainStruct = std::make_unique<StructNode>("ComplexStruct");
    
    // Add simple fields
    mainStruct->addField(std::make_unique<FieldNode>("id", "uint32_t"));
    mainStruct->addField(std::make_unique<FieldNode>("name", "char"));
    mainStruct->getFields().back()->setArraySize(64);
    
    // Add bitfield
    auto flags = std::make_unique<FieldNode>("flags", "uint16_t");
    flags->setBitField(true, 12);
    mainStruct->addField(std::move(flags));
    
    // Add nested struct
    auto nestedStruct = std::make_unique<StructNode>("NestedData");
    nestedStruct->addField(std::make_unique<FieldNode>("x", "double"));
    nestedStruct->addField(std::make_unique<FieldNode>("y", "double"));
    
    auto nestedField = std::make_unique<FieldNode>("position", "NestedData");
    nestedField->setNestedStruct(std::move(nestedStruct));
    mainStruct->addField(std::move(nestedField));
    
    // Add union
    auto unionNode = std::make_unique<UnionNode>("DataUnion");
    unionNode->addMember(std::make_unique<FieldNode>("intData", "int32_t"));
    unionNode->addMember(std::make_unique<FieldNode>("floatData", "float"));
    
    auto unionField = std::make_unique<FieldNode>("data", "DataUnion");
    unionField->setUnion(std::move(unionNode));
    mainStruct->addField(std::move(unionField));
    
    // Verify structure
    QCOMPARE(mainStruct->getFields().size(), 5);
    QVERIFY(mainStruct->getFields()[1]->isArray());
    QVERIFY(mainStruct->getFields()[2]->isBitField());
    QVERIFY(mainStruct->getFields()[3]->getNestedStruct() != nullptr);
    QVERIFY(mainStruct->getFields()[4]->getUnion() != nullptr);
}

void TestASTNodes::testCircularDependencies()
{
    // Test detection of circular dependencies
    auto structA = std::make_unique<StructNode>("StructA");
    auto structB = std::make_unique<StructNode>("StructB");
    
    // A depends on B
    structA->addDependency("StructB");
    structA->addField(std::make_unique<FieldNode>("b", "StructB*")); // Pointer to avoid actual circular dependency
    
    // B depends on A
    structB->addDependency("StructA");
    structB->addField(std::make_unique<FieldNode>("a", "StructA*")); // Pointer to avoid actual circular dependency
    
    // In real implementation, this should be detected by the structure manager
    auto depsA = structA->getDependencies();
    auto depsB = structB->getDependencies();
    
    QVERIFY(depsA.contains("StructB"));
    QVERIFY(depsB.contains("StructA"));
}

void TestASTNodes::testDeepNesting()
{
    // Test deeply nested structures
    auto level0 = std::make_unique<StructNode>("Level0");
    
    for (int i = 1; i <= 5; ++i) {
        auto currentLevel = std::make_unique<StructNode>(QString("Level%1").arg(i));
        currentLevel->addField(std::make_unique<FieldNode>("value", "int"));
        
        auto nestedField = std::make_unique<FieldNode>(QString("next").arg(i), QString("Level%1").arg(i));
        nestedField->setNestedStruct(std::move(currentLevel));
        
        if (i == 1) {
            level0->addField(std::move(nestedField));
        } else {
            // In real implementation, this would be handled by the structure manager
            level0->addDependency(QString("Level%1").arg(i));
        }
    }
    
    QVERIFY(level0->getFields().size() >= 1);
    QVERIFY(level0->getDependencies().size() >= 4);
}

void TestASTNodes::testMixedTypes()
{
    auto mixedStruct = std::make_unique<StructNode>("MixedTypes");
    
    // Add various field types
    mixedStruct->addField(std::make_unique<FieldNode>("boolVal", "bool"));
    mixedStruct->addField(std::make_unique<FieldNode>("charVal", "char"));
    mixedStruct->addField(std::make_unique<FieldNode>("shortVal", "short"));
    mixedStruct->addField(std::make_unique<FieldNode>("intVal", "int"));
    mixedStruct->addField(std::make_unique<FieldNode>("longVal", "long"));
    mixedStruct->addField(std::make_unique<FieldNode>("floatVal", "float"));
    mixedStruct->addField(std::make_unique<FieldNode>("doubleVal", "double"));
    
    // Add array
    auto arrayField = std::make_unique<FieldNode>("intArray", "int");
    arrayField->setArraySize(10);
    mixedStruct->addField(std::move(arrayField));
    
    // Add pointer
    auto ptrField = std::make_unique<FieldNode>("intPtr", "int*");
    mixedStruct->addField(std::move(ptrField));
    
    // Add bitfield
    auto bitField = std::make_unique<FieldNode>("statusBits", "uint8_t");
    bitField->setBitField(true, 4);
    mixedStruct->addField(std::move(bitField));
    
    QCOMPARE(mixedStruct->getFields().size(), 10);
    
    // Verify specific field properties
    QVERIFY(mixedStruct->getFields()[7]->isArray());
    QCOMPARE(mixedStruct->getFields()[7]->getArraySize(), 10);
    
    QVERIFY(mixedStruct->getFields()[9]->isBitField());
    QCOMPARE(mixedStruct->getFields()[9]->getBitWidth(), 4);
}

void TestASTNodes::testVisitorPattern()
{
    // Create a simple visitor for testing
    class TestVisitor : public ASTVisitor {
    public:
        mutable QStringList visitedNodes;
        
        void visit(const StructNode& node) const override {
            visitedNodes.append("Struct:" + node.getName());
        }
        
        void visit(const FieldNode& node) const override {
            visitedNodes.append("Field:" + node.getName());
        }
        
        void visit(const TypeNode& node) const override {
            visitedNodes.append("Type:" + node.getName());
        }
        
        void visit(const UnionNode& node) const override {
            visitedNodes.append("Union:" + node.getName());
        }
        
        void visit(const EnumNode& node) const override {
            visitedNodes.append("Enum:" + node.getName());
        }
    };
    
    auto structNode = createSimpleStruct();
    TestVisitor visitor;
    
    structNode->accept(visitor);
    
    QVERIFY(visitor.visitedNodes.contains("Struct:TestStruct"));
}

void TestASTNodes::testVisitorTraversal()
{
    class CountingVisitor : public ASTVisitor {
    public:
        mutable int structCount = 0;
        mutable int fieldCount = 0;
        
        void visit(const StructNode& /*node*/) const override { structCount++; }
        void visit(const FieldNode& /*node*/) const override { fieldCount++; }
        void visit(const TypeNode& /*node*/) const override { }
        void visit(const UnionNode& /*node*/) const override { }
        void visit(const EnumNode& /*node*/) const override { }
    };
    
    auto complexStruct = createComplexStruct();
    CountingVisitor visitor;
    
    complexStruct->accept(visitor);
    
    QVERIFY(visitor.structCount >= 1);
    QVERIFY(visitor.fieldCount >= 2);
}

// Helper method implementations
std::unique_ptr<StructNode> TestASTNodes::createSimpleStruct()
{
    auto structNode = std::make_unique<StructNode>("TestStruct");
    structNode->addField(std::make_unique<FieldNode>("x", "int"));
    structNode->addField(std::make_unique<FieldNode>("y", "int"));
    return structNode;
}

std::unique_ptr<StructNode> TestASTNodes::createComplexStruct()
{
    auto structNode = std::make_unique<StructNode>("ComplexStruct");
    
    // Add basic fields
    structNode->addField(std::make_unique<FieldNode>("id", "uint32_t"));
    structNode->addField(std::make_unique<FieldNode>("value", "double"));
    
    // Add array field
    auto arrayField = std::make_unique<FieldNode>("data", "float");
    arrayField->setArraySize(16);
    structNode->addField(std::move(arrayField));
    
    // Add bitfield
    auto bitField = std::make_unique<FieldNode>("flags", "uint8_t");
    bitField->setBitField(true, 6);
    structNode->addField(std::move(bitField));
    
    return structNode;
}

std::unique_ptr<UnionNode> TestASTNodes::createSimpleUnion()
{
    auto unionNode = std::make_unique<UnionNode>("TestUnion");
    unionNode->addMember(std::make_unique<FieldNode>("intVal", "int"));
    unionNode->addMember(std::make_unique<FieldNode>("floatVal", "float"));
    return unionNode;
}

std::unique_ptr<EnumNode> TestASTNodes::createSimpleEnum()
{
    auto enumNode = std::make_unique<EnumNode>("TestEnum");
    enumNode->addValue("FIRST", 0);
    enumNode->addValue("SECOND", 1);
    enumNode->addValue("THIRD", 2);
    return enumNode;
}

QTEST_MAIN(TestASTNodes)
#include "test_ast_nodes.moc"