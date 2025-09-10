#include <QtTest/QtTest>
#include <QSignalSpy>
#include <memory>

#include "../../../src/parser/layout/layout_calculator.h"
#include "../../../src/parser/layout/alignment_rules.h"
#include "../../../src/parser/ast/ast_nodes.h"

class TestLayoutCalculator : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic layout tests
    void testSimpleStruct();
    void testEmptyStruct();
    void testBasicTypes();
    void testTypeAlignment();

    // Compiler-specific tests
    void testMSVCLayout();
    void testGCCLayout();
    void testClangLayout();
    void testCrossCompilerConsistency();

    // Padding and alignment tests
    void testFieldPadding();
    void testStructPadding();
    void testAlignmentRequirements();
    void testPackedStructs();

    // Array layout tests
    void testFixedArrays();
    void testMultiDimensionalArrays();
    void testArrayAlignment();
    void testLargeArrays();

    // Bitfield layout tests
    void testBasicBitfields();
    void testBitfieldPacking();
    void testCrossBoundaryBitfields();
    void testZeroWidthBitfields();
    void testMixedBitfields();
    void testBitfieldAlignment();

    // Union layout tests
    void testBasicUnions();
    void testUnionAlignment();
    void testComplexUnions();
    void testUnionWithBitfields();

    // Nested structure tests
    void testNestedStructs();
    void testDeepNesting();
    void testNestedAlignment();
    void testNestedPacking();

    // Advanced features
    void testCustomAlignment();
    void testPragmaPack();
    void testAttributePacked();
    void testAlignmentSpecifiers();

    // Edge cases
    void testZeroSizedStructs();
    void testLargeStructs();
    void testCircularReferences();
    void testFlexibleArrayMembers();

    // Performance tests
    void testCalculationSpeed();
    void testLargeNumberOfStructs();
    void testDeepNestingPerformance();

    // Real-world scenarios
    void testNetworkProtocolLayout();
    void testSystemStructLayout();
    void testEmbeddedRegisterMap();

private:
    std::unique_ptr<LayoutCalculator> calculator;
    AlignmentRules msvcRules;
    AlignmentRules gccRules;
    AlignmentRules clangRules;
    
    // Helper methods
    std::unique_ptr<StructNode> createSimpleStruct();
    std::unique_ptr<StructNode> createBitfieldStruct();
    std::unique_ptr<StructNode> createNestedStruct();
    std::unique_ptr<StructNode> createUnionStruct();
    void verifyLayout(const StructNode* node, size_t expectedSize, size_t expectedAlignment);
    void verifyFieldOffset(const StructNode* node, const QString& fieldName, size_t expectedOffset);
    void compareCompilerLayouts(const StructNode* node);
};

void TestLayoutCalculator::initTestCase()
{
    calculator = std::make_unique<LayoutCalculator>();
    
    // Initialize compiler-specific alignment rules
    msvcRules = AlignmentRules::getMSVCRules();
    gccRules = AlignmentRules::getGCCRules();
    clangRules = AlignmentRules::getClangRules();
}

void TestLayoutCalculator::cleanupTestCase()
{
    calculator.reset();
}

void TestLayoutCalculator::init()
{
    // Reset calculator state if needed
}

void TestLayoutCalculator::cleanup()
{
    // Cleanup after each test
}

void TestLayoutCalculator::testSimpleStruct()
{
    auto structNode = std::make_unique<StructNode>("SimpleStruct");
    structNode->addField(std::make_unique<FieldNode>("x", "int"));
    structNode->addField(std::make_unique<FieldNode>("y", "int"));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 8, 4); // 2 ints, 4-byte aligned
    verifyFieldOffset(structNode.get(), "x", 0);
    verifyFieldOffset(structNode.get(), "y", 4);
}

void TestLayoutCalculator::testEmptyStruct()
{
    auto structNode = std::make_unique<StructNode>("EmptyStruct");
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    // Empty struct typically has size 1 in C++ (implementation-defined in C)
    verifyLayout(structNode.get(), 1, 1);
}

void TestLayoutCalculator::testBasicTypes()
{
    auto structNode = std::make_unique<StructNode>("BasicTypes");
    structNode->addField(std::make_unique<FieldNode>("boolVal", "bool"));
    structNode->addField(std::make_unique<FieldNode>("charVal", "char"));
    structNode->addField(std::make_unique<FieldNode>("shortVal", "short"));
    structNode->addField(std::make_unique<FieldNode>("intVal", "int"));
    structNode->addField(std::make_unique<FieldNode>("longVal", "long"));
    structNode->addField(std::make_unique<FieldNode>("floatVal", "float"));
    structNode->addField(std::make_unique<FieldNode>("doubleVal", "double"));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    // Expected layout with padding:
    // bool (1) + char (1) + padding (2) + short (2) + padding (2) + int (4) + long (8) + float (4) + padding (4) + double (8)
    // Assuming 64-bit platform where long = 8 bytes
    size_t expectedSize = 32; // Platform dependent
    verifyLayout(structNode.get(), expectedSize, 8); // Aligned to largest member (double)
    
    verifyFieldOffset(structNode.get(), "boolVal", 0);
    verifyFieldOffset(structNode.get(), "charVal", 1);
    verifyFieldOffset(structNode.get(), "shortVal", 2);
    verifyFieldOffset(structNode.get(), "intVal", 4);
    // Remaining offsets are platform-dependent
}

void TestLayoutCalculator::testTypeAlignment()
{
    struct TypeTest {
        QString typeName;
        size_t expectedSize;
        size_t expectedAlignment;
    };
    
    QList<TypeTest> typeTests = {
        {"char", 1, 1},
        {"short", 2, 2},
        {"int", 4, 4},
        {"float", 4, 4},
        {"double", 8, 8},
        {"int8_t", 1, 1},
        {"int16_t", 2, 2},
        {"int32_t", 4, 4},
        {"int64_t", 8, 8},
        {"uint8_t", 1, 1},
        {"uint16_t", 2, 2},
        {"uint32_t", 4, 4},
        {"uint64_t", 8, 8}
    };
    
    for (const auto& test : typeTests) {
        auto structNode = std::make_unique<StructNode>("TypeTest");
        structNode->addField(std::make_unique<FieldNode>("field", test.typeName));
        
        calculator->calculateLayout(structNode.get(), msvcRules);
        
        verifyLayout(structNode.get(), test.expectedSize, test.expectedAlignment);
        verifyFieldOffset(structNode.get(), "field", 0);
    }
}

void TestLayoutCalculator::testMSVCLayout()
{
    auto structNode = std::make_unique<StructNode>("MSVCTest");
    structNode->addField(std::make_unique<FieldNode>("a", "char"));
    structNode->addField(std::make_unique<FieldNode>("b", "int"));
    structNode->addField(std::make_unique<FieldNode>("c", "char"));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    // MSVC layout: char(1) + padding(3) + int(4) + char(1) + padding(3) = 12 bytes
    verifyLayout(structNode.get(), 12, 4);
    verifyFieldOffset(structNode.get(), "a", 0);
    verifyFieldOffset(structNode.get(), "b", 4);
    verifyFieldOffset(structNode.get(), "c", 8);
}

void TestLayoutCalculator::testGCCLayout()
{
    auto structNode = std::make_unique<StructNode>("GCCTest");
    structNode->addField(std::make_unique<FieldNode>("a", "char"));
    structNode->addField(std::make_unique<FieldNode>("b", "int"));
    structNode->addField(std::make_unique<FieldNode>("c", "char"));
    
    calculator->calculateLayout(structNode.get(), gccRules);
    
    // GCC layout should be same as MSVC for this simple case
    verifyLayout(structNode.get(), 12, 4);
    verifyFieldOffset(structNode.get(), "a", 0);
    verifyFieldOffset(structNode.get(), "b", 4);
    verifyFieldOffset(structNode.get(), "c", 8);
}

void TestLayoutCalculator::testClangLayout()
{
    auto structNode = std::make_unique<StructNode>("ClangTest");
    structNode->addField(std::make_unique<FieldNode>("a", "char"));
    structNode->addField(std::make_unique<FieldNode>("b", "int"));
    structNode->addField(std::make_unique<FieldNode>("c", "char"));
    
    calculator->calculateLayout(structNode.get(), clangRules);
    
    // Clang layout should be same as GCC for this simple case
    verifyLayout(structNode.get(), 12, 4);
    verifyFieldOffset(structNode.get(), "a", 0);
    verifyFieldOffset(structNode.get(), "b", 4);
    verifyFieldOffset(structNode.get(), "c", 8);
}

void TestLayoutCalculator::testCrossCompilerConsistency()
{
    auto structNode = std::make_unique<StructNode>("ConsistencyTest");
    structNode->addField(std::make_unique<FieldNode>("a", "char"));
    structNode->addField(std::make_unique<FieldNode>("b", "short"));
    structNode->addField(std::make_unique<FieldNode>("c", "int"));
    structNode->addField(std::make_unique<FieldNode>("d", "double"));
    
    // Calculate with different compilers
    auto msvcNode = std::unique_ptr<StructNode>(static_cast<StructNode*>(structNode->clone().release()));
    auto gccNode = std::unique_ptr<StructNode>(static_cast<StructNode*>(structNode->clone().release()));
    auto clangNode = std::unique_ptr<StructNode>(static_cast<StructNode*>(structNode->clone().release()));
    
    calculator->calculateLayout(msvcNode.get(), msvcRules);
    calculator->calculateLayout(gccNode.get(), gccRules);
    calculator->calculateLayout(clangNode.get(), clangRules);
    
    // For standard types, layout should be consistent
    QCOMPARE(msvcNode->getSize(), gccNode->getSize());
    QCOMPARE(gccNode->getSize(), clangNode->getSize());
    
    QCOMPARE(msvcNode->getAlignment(), gccNode->getAlignment());
    QCOMPARE(gccNode->getAlignment(), clangNode->getAlignment());
}

void TestLayoutCalculator::testFieldPadding()
{
    auto structNode = std::make_unique<StructNode>("PaddingTest");
    structNode->addField(std::make_unique<FieldNode>("a", "char"));   // offset 0, size 1
    structNode->addField(std::make_unique<FieldNode>("b", "int"));    // offset 4, size 4 (padded)
    structNode->addField(std::make_unique<FieldNode>("c", "char"));   // offset 8, size 1
    structNode->addField(std::make_unique<FieldNode>("d", "short"));  // offset 10, size 2 (padded)
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 12, 4); // Final padding to align to 4 bytes
    verifyFieldOffset(structNode.get(), "a", 0);
    verifyFieldOffset(structNode.get(), "b", 4);  // 3 bytes padding after 'a'
    verifyFieldOffset(structNode.get(), "c", 8);
    verifyFieldOffset(structNode.get(), "d", 10); // 1 byte padding after 'c'
}

void TestLayoutCalculator::testStructPadding()
{
    auto structNode = std::make_unique<StructNode>("StructPaddingTest");
    structNode->addField(std::make_unique<FieldNode>("a", "double")); // 8 bytes, 8-byte aligned
    structNode->addField(std::make_unique<FieldNode>("b", "char"));   // 1 byte
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 16, 8); // 8 + 1 + 7 padding = 16, aligned to 8
    verifyFieldOffset(structNode.get(), "a", 0);
    verifyFieldOffset(structNode.get(), "b", 8);
}

void TestLayoutCalculator::testAlignmentRequirements()
{
    // Test various alignment scenarios
    struct AlignmentTest {
        QString name;
        QStringList types;
        size_t expectedAlignment;
    };
    
    QList<AlignmentTest> tests = {
        {"CharOnly", {"char", "char", "char"}, 1},
        {"ShortMax", {"char", "short", "char"}, 2},
        {"IntMax", {"char", "int", "char"}, 4},
        {"DoubleMax", {"char", "double", "char"}, 8},
        {"Mixed", {"char", "short", "int", "double"}, 8}
    };
    
    for (const auto& test : tests) {
        auto structNode = std::make_unique<StructNode>(test.name);
        
        for (int i = 0; i < test.types.size(); ++i) {
            structNode->addField(std::make_unique<FieldNode>(
                QString("field%1").arg(i), test.types[i]));
        }
        
        calculator->calculateLayout(structNode.get(), msvcRules);
        
        QCOMPARE(structNode->getAlignment(), test.expectedAlignment);
    }
}

void TestLayoutCalculator::testPackedStructs()
{
    auto structNode = std::make_unique<StructNode>("PackedTest");
    structNode->addField(std::make_unique<FieldNode>("a", "char"));
    structNode->addField(std::make_unique<FieldNode>("b", "int"));
    structNode->addField(std::make_unique<FieldNode>("c", "char"));
    
    // Test with pack(1)
    structNode->setPacked(true, 1);
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 6, 1); // No padding, 1-byte alignment
    verifyFieldOffset(structNode.get(), "a", 0);
    verifyFieldOffset(structNode.get(), "b", 1); // No padding
    verifyFieldOffset(structNode.get(), "c", 5);
    
    // Test with pack(2)
    structNode->setPacked(true, 2);
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 8, 2); // Limited padding, 2-byte alignment
    verifyFieldOffset(structNode.get(), "a", 0);
    verifyFieldOffset(structNode.get(), "b", 2); // Aligned to min(4, 2) = 2
    verifyFieldOffset(structNode.get(), "c", 6);
}

void TestLayoutCalculator::testFixedArrays()
{
    auto structNode = std::make_unique<StructNode>("ArrayTest");
    
    auto charArray = std::make_unique<FieldNode>("charArray", "char");
    charArray->setArraySize(10);
    structNode->addField(std::move(charArray));
    
    auto intArray = std::make_unique<FieldNode>("intArray", "int");
    intArray->setArraySize(5);
    structNode->addField(std::move(intArray));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    // char[10] + padding + int[5] = 10 + 2 + 20 = 32 bytes
    verifyLayout(structNode.get(), 32, 4);
    verifyFieldOffset(structNode.get(), "charArray", 0);
    verifyFieldOffset(structNode.get(), "intArray", 12); // Aligned to int boundary
}

void TestLayoutCalculator::testMultiDimensionalArrays()
{
    auto structNode = std::make_unique<StructNode>("MultiArrayTest");
    
    auto matrix = std::make_unique<FieldNode>("matrix", "int");
    matrix->setArraySize(3 * 4); // Representing int[3][4]
    matrix->setMultiDimensional(true);
    matrix->setDimensions({3, 4});
    structNode->addField(std::move(matrix));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 48, 4); // 3 * 4 * 4 = 48 bytes
    verifyFieldOffset(structNode.get(), "matrix", 0);
}

void TestLayoutCalculator::testArrayAlignment()
{
    auto structNode = std::make_unique<StructNode>("ArrayAlignTest");
    structNode->addField(std::make_unique<FieldNode>("prefix", "char"));
    
    auto doubleArray = std::make_unique<FieldNode>("doubles", "double");
    doubleArray->setArraySize(3);
    structNode->addField(std::move(doubleArray));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 32, 8); // char + 7 padding + 3*8 doubles = 32
    verifyFieldOffset(structNode.get(), "prefix", 0);
    verifyFieldOffset(structNode.get(), "doubles", 8); // Aligned to double boundary
}

void TestLayoutCalculator::testLargeArrays()
{
    auto structNode = std::make_unique<StructNode>("LargeArrayTest");
    
    auto largeArray = std::make_unique<FieldNode>("data", "char");
    largeArray->setArraySize(65536); // 64KB
    structNode->addField(std::move(largeArray));
    
    structNode->addField(std::make_unique<FieldNode>("trailer", "int"));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 65540, 4); // 65536 + 4 = 65540
    verifyFieldOffset(structNode.get(), "data", 0);
    verifyFieldOffset(structNode.get(), "trailer", 65536);
}

void TestLayoutCalculator::testBasicBitfields()
{
    auto structNode = std::make_unique<StructNode>("BitfieldTest");
    
    auto flag1 = std::make_unique<FieldNode>("flag1", "unsigned int");
    flag1->setBitField(true, 1);
    structNode->addField(std::move(flag1));
    
    auto flag2 = std::make_unique<FieldNode>("flag2", "unsigned int");
    flag2->setBitField(true, 1);
    structNode->addField(std::move(flag2));
    
    auto value = std::make_unique<FieldNode>("value", "unsigned int");
    value->setBitField(true, 6);
    structNode->addField(std::move(value));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 4, 4); // All fit in one int
    
    // Bitfield offsets are in bits, not bytes
    QCOMPARE(structNode->getFields()[0]->getBitOffset(), 0);
    QCOMPARE(structNode->getFields()[1]->getBitOffset(), 1);
    QCOMPARE(structNode->getFields()[2]->getBitOffset(), 2);
}

void TestLayoutCalculator::testBitfieldPacking()
{
    auto structNode = std::make_unique<StructNode>("BitfieldPackTest");
    
    // These should pack into a single unsigned int (32 bits)
    auto a = std::make_unique<FieldNode>("a", "unsigned int");
    a->setBitField(true, 8);
    structNode->addField(std::move(a));
    
    auto b = std::make_unique<FieldNode>("b", "unsigned int");
    b->setBitField(true, 8);
    structNode->addField(std::move(b));
    
    auto c = std::make_unique<FieldNode>("c", "unsigned int");
    c->setBitField(true, 16);
    structNode->addField(std::move(c));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 4, 4); // All pack into one int
    QCOMPARE(structNode->getFields()[0]->getBitOffset(), 0);
    QCOMPARE(structNode->getFields()[1]->getBitOffset(), 8);
    QCOMPARE(structNode->getFields()[2]->getBitOffset(), 16);
}

void TestLayoutCalculator::testCrossBoundaryBitfields()
{
    auto structNode = std::make_unique<StructNode>("CrossBoundaryTest");
    
    auto a = std::make_unique<FieldNode>("a", "unsigned int");
    a->setBitField(true, 24); // Uses 24 bits
    structNode->addField(std::move(a));
    
    auto b = std::make_unique<FieldNode>("b", "unsigned int");
    b->setBitField(true, 16); // Needs 16 bits, won't fit in remaining 8
    structNode->addField(std::move(b));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 8, 4); // Spans two ints
    QCOMPARE(structNode->getFields()[0]->getBitOffset(), 0);
    QCOMPARE(structNode->getFields()[1]->getBitOffset(), 32); // Starts new int
}

void TestLayoutCalculator::testZeroWidthBitfields()
{
    auto structNode = std::make_unique<StructNode>("ZeroWidthTest");
    
    auto a = std::make_unique<FieldNode>("a", "unsigned int");
    a->setBitField(true, 8);
    structNode->addField(std::move(a));
    
    // Zero-width bitfield forces alignment
    auto separator = std::make_unique<FieldNode>("", "unsigned int");
    separator->setBitField(true, 0);
    structNode->addField(std::move(separator));
    
    auto b = std::make_unique<FieldNode>("b", "unsigned int");
    b->setBitField(true, 8);
    structNode->addField(std::move(b));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 8, 4); // Forces b to start new int
    QCOMPARE(structNode->getFields()[0]->getBitOffset(), 0);
    QCOMPARE(structNode->getFields()[2]->getBitOffset(), 32); // After zero-width field
}

void TestLayoutCalculator::testMixedBitfields()
{
    auto structNode = std::make_unique<StructNode>("MixedBitfieldTest");
    
    auto normalField = std::make_unique<FieldNode>("normal", "int");
    structNode->addField(std::move(normalField));
    
    auto bitField = std::make_unique<FieldNode>("bits", "unsigned int");
    bitField->setBitField(true, 8);
    structNode->addField(std::move(bitField));
    
    auto anotherNormal = std::make_unique<FieldNode>("normal2", "char");
    structNode->addField(std::move(anotherNormal));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 12, 4); // int + int(bitfield) + char + padding
    verifyFieldOffset(structNode.get(), "normal", 0);
    verifyFieldOffset(structNode.get(), "bits", 4);
    verifyFieldOffset(structNode.get(), "normal2", 8);
}

void TestLayoutCalculator::testBitfieldAlignment()
{
    auto structNode = std::make_unique<StructNode>("BitfieldAlignTest");
    
    auto shortBits = std::make_unique<FieldNode>("shortBits", "unsigned short");
    shortBits->setBitField(true, 12);
    structNode->addField(std::move(shortBits));
    
    auto intBits = std::make_unique<FieldNode>("intBits", "unsigned int");
    intBits->setBitField(true, 20);
    structNode->addField(std::move(intBits));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    // Different base types should respect their alignment
    verifyLayout(structNode.get(), 8, 4); // short(2) + padding + int(4)
}

void TestLayoutCalculator::testBasicUnions()
{
    auto structNode = std::make_unique<StructNode>("UnionTest");
    
    auto unionField = std::make_unique<FieldNode>("data", "DataUnion");
    auto unionNode = std::make_unique<UnionNode>("DataUnion");
    
    unionNode->addMember(std::make_unique<FieldNode>("intVal", "int"));
    unionNode->addMember(std::make_unique<FieldNode>("floatVal", "float"));
    unionNode->addMember(std::make_unique<FieldNode>("charArray", "char"));
    unionNode->getMembers().back()->setArraySize(4);
    
    unionField->setUnion(std::move(unionNode));
    structNode->addField(std::move(unionField));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    // Union size is the size of largest member
    verifyLayout(structNode.get(), 4, 4);
    verifyFieldOffset(structNode.get(), "data", 0);
    
    // All union members should have offset 0
    auto unionPtr = structNode->getFields()[0]->getUnion();
    for (const auto& member : unionPtr->getMembers()) {
        QCOMPARE(member->getOffset(), 0);
    }
}

void TestLayoutCalculator::testUnionAlignment()
{
    auto structNode = std::make_unique<StructNode>("UnionAlignTest");
    structNode->addField(std::make_unique<FieldNode>("prefix", "char"));
    
    auto unionField = std::make_unique<FieldNode>("aligned", "AlignedUnion");
    auto unionNode = std::make_unique<UnionNode>("AlignedUnion");
    
    unionNode->addMember(std::make_unique<FieldNode>("doubleVal", "double"));
    unionNode->addMember(std::make_unique<FieldNode>("intVal", "int"));
    
    unionField->setUnion(std::move(unionNode));
    structNode->addField(std::move(unionField));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 16, 8); // char + 7 padding + double union
    verifyFieldOffset(structNode.get(), "prefix", 0);
    verifyFieldOffset(structNode.get(), "aligned", 8);
}

void TestLayoutCalculator::testComplexUnions()
{
    auto structNode = std::make_unique<StructNode>("ComplexUnionTest");
    
    auto unionField = std::make_unique<FieldNode>("complex", "ComplexUnion");
    auto unionNode = std::make_unique<UnionNode>("ComplexUnion");
    
    // Add nested struct member
    auto nestedStruct = std::make_unique<StructNode>("NestedStruct");
    nestedStruct->addField(std::make_unique<FieldNode>("x", "int"));
    nestedStruct->addField(std::make_unique<FieldNode>("y", "int"));
    
    auto structMember = std::make_unique<FieldNode>("structVal", "NestedStruct");
    structMember->setNestedStruct(std::move(nestedStruct));
    unionNode->addMember(std::move(structMember));
    
    // Add array member
    auto arrayMember = std::make_unique<FieldNode>("arrayVal", "short");
    arrayMember->setArraySize(6);
    unionNode->addMember(std::move(arrayMember));
    
    unionField->setUnion(std::move(unionNode));
    structNode->addField(std::move(unionField));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    // Union size is max(8, 12) = 12, but aligned to 4 = 12
    verifyLayout(structNode.get(), 12, 4);
}

void TestLayoutCalculator::testUnionWithBitfields()
{
    auto structNode = std::make_unique<StructNode>("UnionBitfieldTest");
    
    auto unionField = std::make_unique<FieldNode>("data", "BitfieldUnion");
    auto unionNode = std::make_unique<UnionNode>("BitfieldUnion");
    
    // Regular int member
    unionNode->addMember(std::make_unique<FieldNode>("intVal", "int"));
    
    // Bitfield struct member
    auto bitfieldStruct = std::make_unique<StructNode>("BitfieldStruct");
    auto bitField = std::make_unique<FieldNode>("flags", "unsigned int");
    bitField->setBitField(true, 16);
    bitfieldStruct->addField(std::move(bitField));
    
    auto structMember = std::make_unique<FieldNode>("bitfieldVal", "BitfieldStruct");
    structMember->setNestedStruct(std::move(bitfieldStruct));
    unionNode->addMember(std::move(structMember));
    
    unionField->setUnion(std::move(unionNode));
    structNode->addField(std::move(unionField));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 4, 4); // Both members are 4 bytes
}

void TestLayoutCalculator::testNestedStructs()
{
    auto outerStruct = std::make_unique<StructNode>("OuterStruct");
    
    // Create inner struct
    auto innerStruct = std::make_unique<StructNode>("InnerStruct");
    innerStruct->addField(std::make_unique<FieldNode>("x", "double"));
    innerStruct->addField(std::make_unique<FieldNode>("y", "int"));
    
    // Calculate inner struct layout first
    calculator->calculateLayout(innerStruct.get(), msvcRules);
    
    // Add inner struct as field
    auto innerField = std::make_unique<FieldNode>("inner", "InnerStruct");
    innerField->setNestedStruct(std::move(innerStruct));
    outerStruct->addField(std::move(innerField));
    
    outerStruct->addField(std::make_unique<FieldNode>("outer", "char"));
    
    calculator->calculateLayout(outerStruct.get(), msvcRules);
    
    verifyLayout(outerStruct.get(), 24, 8); // InnerStruct(16) + char + padding
    verifyFieldOffset(outerStruct.get(), "inner", 0);
    verifyFieldOffset(outerStruct.get(), "outer", 16);
}

void TestLayoutCalculator::testDeepNesting()
{
    // Create deeply nested structure
    auto level0 = std::make_unique<StructNode>("Level0");
    
    auto currentStruct = level0.get();
    for (int i = 1; i <= 5; ++i) {
        auto nextLevel = std::make_unique<StructNode>(QString("Level%1").arg(i));
        nextLevel->addField(std::make_unique<FieldNode>("data", "int"));
        
        // Calculate intermediate layout
        calculator->calculateLayout(nextLevel.get(), msvcRules);
        
        auto nestedField = std::make_unique<FieldNode>(QString("level%1").arg(i), QString("Level%1").arg(i));
        nestedField->setNestedStruct(std::move(nextLevel));
        currentStruct->addField(std::move(nestedField));
        
        currentStruct = currentStruct->getFields().last()->getNestedStruct();
    }
    
    calculator->calculateLayout(level0.get(), msvcRules);
    
    // Each level adds 4 bytes for int
    verifyLayout(level0.get(), 20, 4); // 5 levels * 4 bytes each
}

void TestLayoutCalculator::testNestedAlignment()
{
    auto outerStruct = std::make_unique<StructNode>("AlignedNested");
    outerStruct->addField(std::make_unique<FieldNode>("prefix", "char"));
    
    // Create aligned inner struct
    auto innerStruct = std::make_unique<StructNode>("AlignedInner");
    innerStruct->addField(std::make_unique<FieldNode>("alignedField", "double"));
    calculator->calculateLayout(innerStruct.get(), msvcRules);
    
    auto innerField = std::make_unique<FieldNode>("inner", "AlignedInner");
    innerField->setNestedStruct(std::move(innerStruct));
    outerStruct->addField(std::move(innerField));
    
    calculator->calculateLayout(outerStruct.get(), msvcRules);
    
    verifyLayout(outerStruct.get(), 16, 8); // char + 7 padding + double
    verifyFieldOffset(outerStruct.get(), "prefix", 0);
    verifyFieldOffset(outerStruct.get(), "inner", 8);
}

void TestLayoutCalculator::testNestedPacking()
{
    auto outerStruct = std::make_unique<StructNode>("PackedNested");
    outerStruct->setPacked(true, 1);
    
    // Create inner struct
    auto innerStruct = std::make_unique<StructNode>("InnerStruct");
    innerStruct->addField(std::make_unique<FieldNode>("a", "char"));
    innerStruct->addField(std::make_unique<FieldNode>("b", "int"));
    innerStruct->setPacked(true, 1); // Also packed
    
    calculator->calculateLayout(innerStruct.get(), msvcRules);
    
    auto innerField = std::make_unique<FieldNode>("inner", "InnerStruct");
    innerField->setNestedStruct(std::move(innerStruct));
    outerStruct->addField(std::move(innerField));
    
    outerStruct->addField(std::make_unique<FieldNode>("outer", "char"));
    
    calculator->calculateLayout(outerStruct.get(), msvcRules);
    
    verifyLayout(outerStruct.get(), 6, 1); // InnerStruct(5) + char(1) = 6, no padding
    verifyFieldOffset(outerStruct.get(), "inner", 0);
    verifyFieldOffset(outerStruct.get(), "outer", 5);
}

void TestLayoutCalculator::testCustomAlignment()
{
    auto structNode = std::make_unique<StructNode>("CustomAligned");
    structNode->setAlignment(16); // Custom 16-byte alignment
    
    structNode->addField(std::make_unique<FieldNode>("data", "int"));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 16, 16); // Padded to 16-byte boundary
    verifyFieldOffset(structNode.get(), "data", 0);
}

void TestLayoutCalculator::testPragmaPack()
{
    // Test various pack values
    QList<int> packValues = {1, 2, 4, 8, 16};
    
    for (int packValue : packValues) {
        auto structNode = std::make_unique<StructNode>(QString("Pack%1Test").arg(packValue));
        structNode->setPacked(true, packValue);
        
        structNode->addField(std::make_unique<FieldNode>("a", "char"));
        structNode->addField(std::make_unique<FieldNode>("b", "double"));
        structNode->addField(std::make_unique<FieldNode>("c", "char"));
        
        calculator->calculateLayout(structNode.get(), msvcRules);
        
        // Verify pack value affects alignment
        QVERIFY(structNode->getAlignment() <= packValue);
        QCOMPARE(structNode->getPackValue(), packValue);
    }
}

void TestLayoutCalculator::testAttributePacked()
{
    auto structNode = std::make_unique<StructNode>("AttributePacked");
    structNode->setAttribute("packed");
    structNode->setPacked(true, 1);
    
    structNode->addField(std::make_unique<FieldNode>("a", "char"));
    structNode->addField(std::make_unique<FieldNode>("b", "int"));
    structNode->addField(std::make_unique<FieldNode>("c", "char"));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 6, 1); // No padding
    verifyFieldOffset(structNode.get(), "a", 0);
    verifyFieldOffset(structNode.get(), "b", 1);
    verifyFieldOffset(structNode.get(), "c", 5);
}

void TestLayoutCalculator::testAlignmentSpecifiers()
{
    auto structNode = std::make_unique<StructNode>("AlignmentSpecifiers");
    
    auto alignedField = std::make_unique<FieldNode>("aligned", "int");
    alignedField->setAlignment(16);
    structNode->addField(std::move(alignedField));
    
    structNode->addField(std::make_unique<FieldNode>("normal", "char"));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 32, 16); // Aligned to 16 bytes
    verifyFieldOffset(structNode.get(), "aligned", 0);
    verifyFieldOffset(structNode.get(), "normal", 4);
}

void TestLayoutCalculator::testZeroSizedStructs()
{
    auto structNode = std::make_unique<StructNode>("ZeroSized");
    // No fields
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    // Empty structs typically have size 1
    verifyLayout(structNode.get(), 1, 1);
}

void TestLayoutCalculator::testLargeStructs()
{
    auto structNode = std::make_unique<StructNode>("LargeStruct");
    
    // Add many fields
    for (int i = 0; i < 1000; ++i) {
        structNode->addField(std::make_unique<FieldNode>(
            QString("field%1").arg(i), "int"));
    }
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    verifyLayout(structNode.get(), 4000, 4); // 1000 * 4 = 4000
    
    // Verify field offsets
    for (int i = 0; i < 10; ++i) { // Check first 10
        verifyFieldOffset(structNode.get(), QString("field%1").arg(i), i * 4);
    }
}

void TestLayoutCalculator::testCircularReferences()
{
    // Create structures with circular pointer references
    auto structA = std::make_unique<StructNode>("StructA");
    auto structB = std::make_unique<StructNode>("StructB");
    
    structA->addField(std::make_unique<FieldNode>("bPtr", "StructB*"));
    structA->addField(std::make_unique<FieldNode>("data", "int"));
    
    structB->addField(std::make_unique<FieldNode>("aPtr", "StructA*"));
    structB->addField(std::make_unique<FieldNode>("value", "double"));
    
    calculator->calculateLayout(structA.get(), msvcRules);
    calculator->calculateLayout(structB.get(), msvcRules);
    
    // Pointers should be calculated properly
    size_t ptrSize = sizeof(void*);
    verifyLayout(structA.get(), ptrSize + 8, 8); // Assuming 64-bit pointers
    verifyLayout(structB.get(), ptrSize + 8, 8);
}

void TestLayoutCalculator::testFlexibleArrayMembers()
{
    auto structNode = std::make_unique<StructNode>("FlexibleArray");
    structNode->addField(std::make_unique<FieldNode>("size", "int"));
    
    auto flexArray = std::make_unique<FieldNode>("data", "char");
    flexArray->setArraySize(0); // Flexible array member
    structNode->addField(std::move(flexArray));
    
    calculator->calculateLayout(structNode.get(), msvcRules);
    
    // Flexible array member doesn't contribute to struct size
    verifyLayout(structNode.get(), 4, 4); // Just the int field
    verifyFieldOffset(structNode.get(), "size", 0);
    verifyFieldOffset(structNode.get(), "data", 4);
}

void TestLayoutCalculator::testCalculationSpeed()
{
    const int numStructs = 1000;
    QElapsedTimer timer;
    
    timer.start();
    
    for (int i = 0; i < numStructs; ++i) {
        auto structNode = std::make_unique<StructNode>(QString("SpeedTest%1").arg(i));
        structNode->addField(std::make_unique<FieldNode>("a", "char"));
        structNode->addField(std::make_unique<FieldNode>("b", "int"));
        structNode->addField(std::make_unique<FieldNode>("c", "double"));
        
        calculator->calculateLayout(structNode.get(), msvcRules);
    }
    
    qint64 elapsed = timer.elapsed();
    
    // Should complete 1000 simple structs in reasonable time (< 100ms)
    QVERIFY(elapsed < 100);
    
    qDebug() << "Calculated" << numStructs << "structs in" << elapsed << "ms";
}

void TestLayoutCalculator::testLargeNumberOfStructs()
{
    const int numStructs = 100;
    QList<std::unique_ptr<StructNode>> structs;
    
    // Create many structs with dependencies
    for (int i = 0; i < numStructs; ++i) {
        auto structNode = std::make_unique<StructNode>(QString("Struct%1").arg(i));
        structNode->addField(std::make_unique<FieldNode>("id", "int"));
        
        if (i > 0) {
            // Add dependency on previous struct
            structNode->addField(std::make_unique<FieldNode>("prev", QString("Struct%1*").arg(i-1)));
        }
        
        calculator->calculateLayout(structNode.get(), msvcRules);
        structs.append(std::move(structNode));
    }
    
    // Verify all calculations are correct
    for (int i = 0; i < numStructs; ++i) {
        size_t expectedSize = (i == 0) ? 4 : (4 + sizeof(void*));
        verifyLayout(structs[i].get(), expectedSize, qMax(size_t(4), sizeof(void*)));
    }
}

void TestLayoutCalculator::testDeepNestingPerformance()
{
    const int nestingDepth = 50;
    
    auto rootStruct = std::make_unique<StructNode>("Root");
    auto currentStruct = rootStruct.get();
    
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 1; i <= nestingDepth; ++i) {
        auto nestedStruct = std::make_unique<StructNode>(QString("Nested%1").arg(i));
        nestedStruct->addField(std::make_unique<FieldNode>("data", "int"));
        
        auto nestedField = std::make_unique<FieldNode>(QString("nested%1").arg(i), QString("Nested%1").arg(i));
        nestedField->setNestedStruct(std::move(nestedStruct));
        currentStruct->addField(std::move(nestedField));
        
        currentStruct = currentStruct->getFields().last()->getNestedStruct();
    }
    
    calculator->calculateLayout(rootStruct.get(), msvcRules);
    
    qint64 elapsed = timer.elapsed();
    
    // Should handle deep nesting efficiently
    QVERIFY(elapsed < 10);
    
    verifyLayout(rootStruct.get(), nestingDepth * 4, 4);
}

void TestLayoutCalculator::testNetworkProtocolLayout()
{
    auto ipHeader = std::make_unique<StructNode>("IPHeader");
    ipHeader->setPacked(true, 1);
    
    auto version = std::make_unique<FieldNode>("version", "uint8_t");
    version->setBitField(true, 4);
    ipHeader->addField(std::move(version));
    
    auto headerLength = std::make_unique<FieldNode>("headerLength", "uint8_t");
    headerLength->setBitField(true, 4);
    ipHeader->addField(std::move(headerLength));
    
    ipHeader->addField(std::make_unique<FieldNode>("typeOfService", "uint8_t"));
    ipHeader->addField(std::make_unique<FieldNode>("totalLength", "uint16_t"));
    ipHeader->addField(std::make_unique<FieldNode>("identification", "uint16_t"));
    
    auto flags = std::make_unique<FieldNode>("flags", "uint16_t");
    flags->setBitField(true, 3);
    ipHeader->addField(std::move(flags));
    
    auto fragOffset = std::make_unique<FieldNode>("fragmentOffset", "uint16_t");
    fragOffset->setBitField(true, 13);
    ipHeader->addField(std::move(fragOffset));
    
    ipHeader->addField(std::make_unique<FieldNode>("timeToLive", "uint8_t"));
    ipHeader->addField(std::make_unique<FieldNode>("protocol", "uint8_t"));
    ipHeader->addField(std::make_unique<FieldNode>("headerChecksum", "uint16_t"));
    ipHeader->addField(std::make_unique<FieldNode>("sourceAddress", "uint32_t"));
    ipHeader->addField(std::make_unique<FieldNode>("destinationAddress", "uint32_t"));
    
    calculator->calculateLayout(ipHeader.get(), msvcRules);
    
    verifyLayout(ipHeader.get(), 20, 1); // Standard IP header is 20 bytes, packed
    
    // Verify critical field offsets
    verifyFieldOffset(ipHeader.get(), "typeOfService", 1);
    verifyFieldOffset(ipHeader.get(), "totalLength", 2);
    verifyFieldOffset(ipHeader.get(), "sourceAddress", 12);
    verifyFieldOffset(ipHeader.get(), "destinationAddress", 16);
}

void TestLayoutCalculator::testSystemStructLayout()
{
    auto statStruct = std::make_unique<StructNode>("stat");
    
    statStruct->addField(std::make_unique<FieldNode>("st_dev", "dev_t"));
    statStruct->addField(std::make_unique<FieldNode>("st_ino", "ino_t"));
    statStruct->addField(std::make_unique<FieldNode>("st_mode", "mode_t"));
    statStruct->addField(std::make_unique<FieldNode>("st_nlink", "nlink_t"));
    statStruct->addField(std::make_unique<FieldNode>("st_uid", "uid_t"));
    statStruct->addField(std::make_unique<FieldNode>("st_gid", "gid_t"));
    statStruct->addField(std::make_unique<FieldNode>("st_size", "off_t"));
    statStruct->addField(std::make_unique<FieldNode>("st_atime", "time_t"));
    statStruct->addField(std::make_unique<FieldNode>("st_mtime", "time_t"));
    statStruct->addField(std::make_unique<FieldNode>("st_ctime", "time_t"));
    
    calculator->calculateLayout(statStruct.get(), msvcRules);
    
    // System struct sizes are platform-dependent, but should be consistent
    QVERIFY(statStruct->getSize() > 0);
    QVERIFY(statStruct->getAlignment() >= 4);
    
    // Verify field ordering
    size_t prevOffset = 0;
    for (const auto& field : statStruct->getFields()) {
        QVERIFY(field->getOffset() >= prevOffset);
        prevOffset = field->getOffset() + field->getSize();
    }
}

void TestLayoutCalculator::testEmbeddedRegisterMap()
{
    auto regMap = std::make_unique<StructNode>("RegisterMap");
    
    // Control register
    auto ctrl = std::make_unique<FieldNode>("CTRL", "volatile uint32_t");
    regMap->addField(std::move(ctrl));
    
    // Status register with bitfields
    auto status = std::make_unique<FieldNode>("STATUS", "volatile uint32_t");
    regMap->addField(std::move(status));
    
    // Data array
    auto dataArray = std::make_unique<FieldNode>("DATA", "volatile uint32_t");
    dataArray->setArraySize(8);
    regMap->addField(std::move(dataArray));
    
    // Reserved space
    auto reserved = std::make_unique<FieldNode>("RESERVED", "uint32_t");
    reserved->setArraySize(4);
    regMap->addField(std::move(reserved));
    
    // Configuration union
    auto configField = std::make_unique<FieldNode>("CONFIG", "ConfigUnion");
    auto configUnion = std::make_unique<UnionNode>("ConfigUnion");
    
    configUnion->addMember(std::make_unique<FieldNode>("word", "volatile uint32_t"));
    
    auto bytesStruct = std::make_unique<StructNode>("BytesStruct");
    bytesStruct->addField(std::make_unique<FieldNode>("byte0", "volatile uint8_t"));
    bytesStruct->addField(std::make_unique<FieldNode>("byte1", "volatile uint8_t"));
    bytesStruct->addField(std::make_unique<FieldNode>("byte2", "volatile uint8_t"));
    bytesStruct->addField(std::make_unique<FieldNode>("byte3", "volatile uint8_t"));
    
    auto bytesMember = std::make_unique<FieldNode>("bytes", "BytesStruct");
    bytesMember->setNestedStruct(std::move(bytesStruct));
    configUnion->addMember(std::move(bytesMember));
    
    configField->setUnion(std::move(configUnion));
    regMap->addField(std::move(configField));
    
    calculator->calculateLayout(regMap.get(), msvcRules);
    
    // Expected: CTRL(4) + STATUS(4) + DATA(32) + RESERVED(16) + CONFIG(4) = 60 bytes
    verifyLayout(regMap.get(), 60, 4);
    
    verifyFieldOffset(regMap.get(), "CTRL", 0);
    verifyFieldOffset(regMap.get(), "STATUS", 4);
    verifyFieldOffset(regMap.get(), "DATA", 8);
    verifyFieldOffset(regMap.get(), "RESERVED", 40);
    verifyFieldOffset(regMap.get(), "CONFIG", 56);
}

// Helper method implementations
std::unique_ptr<StructNode> TestLayoutCalculator::createSimpleStruct()
{
    auto structNode = std::make_unique<StructNode>("SimpleStruct");
    structNode->addField(std::make_unique<FieldNode>("a", "int"));
    structNode->addField(std::make_unique<FieldNode>("b", "double"));
    return structNode;
}

std::unique_ptr<StructNode> TestLayoutCalculator::createBitfieldStruct()
{
    auto structNode = std::make_unique<StructNode>("BitfieldStruct");
    
    auto flag1 = std::make_unique<FieldNode>("flag1", "unsigned int");
    flag1->setBitField(true, 1);
    structNode->addField(std::move(flag1));
    
    auto value = std::make_unique<FieldNode>("value", "unsigned int");
    value->setBitField(true, 15);
    structNode->addField(std::move(value));
    
    return structNode;
}

std::unique_ptr<StructNode> TestLayoutCalculator::createNestedStruct()
{
    auto outerStruct = std::make_unique<StructNode>("NestedStruct");
    
    auto innerStruct = std::make_unique<StructNode>("InnerStruct");
    innerStruct->addField(std::make_unique<FieldNode>("x", "int"));
    innerStruct->addField(std::make_unique<FieldNode>("y", "int"));
    
    auto innerField = std::make_unique<FieldNode>("inner", "InnerStruct");
    innerField->setNestedStruct(std::move(innerStruct));
    outerStruct->addField(std::move(innerField));
    
    outerStruct->addField(std::make_unique<FieldNode>("outer", "double"));
    
    return outerStruct;
}

std::unique_ptr<StructNode> TestLayoutCalculator::createUnionStruct()
{
    auto structNode = std::make_unique<StructNode>("UnionStruct");
    
    auto unionField = std::make_unique<FieldNode>("data", "DataUnion");
    auto unionNode = std::make_unique<UnionNode>("DataUnion");
    
    unionNode->addMember(std::make_unique<FieldNode>("intVal", "int"));
    unionNode->addMember(std::make_unique<FieldNode>("floatVal", "float"));
    unionNode->addMember(std::make_unique<FieldNode>("doubleVal", "double"));
    
    unionField->setUnion(std::move(unionNode));
    structNode->addField(std::move(unionField));
    
    return structNode;
}

void TestLayoutCalculator::verifyLayout(const StructNode* node, size_t expectedSize, size_t expectedAlignment)
{
    QVERIFY(node != nullptr);
    QCOMPARE(node->getSize(), expectedSize);
    QCOMPARE(node->getAlignment(), expectedAlignment);
}

void TestLayoutCalculator::verifyFieldOffset(const StructNode* node, const QString& fieldName, size_t expectedOffset)
{
    QVERIFY(node != nullptr);
    
    auto field = node->findField(fieldName);
    QVERIFY2(field != nullptr, qPrintable(QString("Field '%1' not found").arg(fieldName)));
    QCOMPARE(field->getOffset(), expectedOffset);
}

void TestLayoutCalculator::compareCompilerLayouts(const StructNode* node)
{
    auto msvcNode = std::unique_ptr<StructNode>(static_cast<StructNode*>(node->clone().release()));
    auto gccNode = std::unique_ptr<StructNode>(static_cast<StructNode*>(node->clone().release()));
    auto clangNode = std::unique_ptr<StructNode>(static_cast<StructNode*>(node->clone().release()));
    
    calculator->calculateLayout(msvcNode.get(), msvcRules);
    calculator->calculateLayout(gccNode.get(), gccRules);
    calculator->calculateLayout(clangNode.get(), clangRules);
    
    qDebug() << "Compiler layout comparison for" << node->getName();
    qDebug() << "MSVC: size=" << msvcNode->getSize() << "alignment=" << msvcNode->getAlignment();
    qDebug() << "GCC:  size=" << gccNode->getSize() << "alignment=" << gccNode->getAlignment();
    qDebug() << "Clang:size=" << clangNode->getSize() << "alignment=" << clangNode->getAlignment();
}

QTEST_MAIN(TestLayoutCalculator)
#include "test_layout_calculator.moc"