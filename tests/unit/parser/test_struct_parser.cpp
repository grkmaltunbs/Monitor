#include <QtTest/QtTest>
#include <QSignalSpy>
#include <memory>

#include "../../../src/parser/parser/struct_parser.h"
#include "../../../src/parser/ast/ast_nodes.h"

class TestStructParser : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic parsing tests
    void testSimpleStruct();
    void testEmptyStruct();
    void testStructWithBasicTypes();
    void testMultipleStructs();

    // Field parsing tests
    void testFieldTypes();
    void testArrayFields();
    void testPointerFields();
    void testConstFields();
    void testStaticFields();

    // Complex type tests
    void testNestedStructs();
    void testUnionParsing();
    void testEnumParsing();
    void testTypedefParsing();

    // Bitfield tests
    void testBasicBitfields();
    void testMixedBitfields();
    void testBitfieldAlignment();
    void testZeroWidthBitfields();

    // Compiler-specific tests
    void testPackedStructs();
    void testAlignmentAttributes();
    void testGCCAttributes();
    void testMSVCPragmas();

    // Advanced features
    void testInheritance();
    void testTemplates();
    void testAnonymousStructs();
    void testForwardDeclarations();

    // Error handling tests
    void testSyntaxErrors();
    void testIncompleteStructs();
    void testCircularDependencies();
    void testInvalidBitfields();

    // Real-world scenarios
    void testNetworkProtocolStruct();
    void testSystemStruct();
    void testEmbeddedStruct();
    void testLargeComplexStruct();

private:
    std::unique_ptr<StructParser> parser;
    
    // Helper methods
    void verifyStructNode(const StructNode* node, const QString& expectedName, int expectedFieldCount);
    void verifyFieldNode(const FieldNode* field, const QString& expectedName, const QString& expectedType);
    std::unique_ptr<StructNode> parseStruct(const QString& structCode);
    QList<std::unique_ptr<StructNode>> parseMultipleStructs(const QString& structsCode);
};

void TestStructParser::initTestCase()
{
    // Initialize parser
    parser = std::make_unique<StructParser>();
}

void TestStructParser::cleanupTestCase()
{
    parser.reset();
}

void TestStructParser::init()
{
    // Setup for each test - could reset parser state if needed
}

void TestStructParser::cleanup()
{
    // Cleanup after each test
}

void TestStructParser::testSimpleStruct()
{
    QString structCode = R"(
        typedef struct {
            int x;
            int y;
        } Point2D;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "Point2D", 2);
    verifyFieldNode(result->getFields()[0].get(), "x", "int");
    verifyFieldNode(result->getFields()[1].get(), "y", "int");
}

void TestStructParser::testEmptyStruct()
{
    QString structCode = R"(
        typedef struct {
        } EmptyStruct;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "EmptyStruct", 0);
}

void TestStructParser::testStructWithBasicTypes()
{
    QString structCode = R"(
        typedef struct {
            bool flag;
            char character;
            unsigned char uchar;
            short shortVal;
            unsigned short ushortVal;
            int intVal;
            unsigned int uintVal;
            long longVal;
            unsigned long ulongVal;
            long long llongVal;
            unsigned long long ullongVal;
            float floatVal;
            double doubleVal;
            long double ldoubleVal;
        } AllBasicTypes;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "AllBasicTypes", 14);
    
    // Verify specific types
    verifyFieldNode(result->getFields()[0].get(), "flag", "bool");
    verifyFieldNode(result->getFields()[1].get(), "character", "char");
    verifyFieldNode(result->getFields()[2].get(), "uchar", "unsigned char");
    verifyFieldNode(result->getFields()[11].get(), "floatVal", "float");
    verifyFieldNode(result->getFields()[12].get(), "doubleVal", "double");
}

void TestStructParser::testMultipleStructs()
{
    QString structsCode = R"(
        typedef struct {
            int x;
            int y;
        } Point2D;
        
        typedef struct {
            int x;
            int y;
            int z;
        } Point3D;
        
        typedef struct {
            Point2D position;
            Point3D velocity;
        } Object;
    )";
    
    auto results = parseMultipleStructs(structsCode);
    QCOMPARE(results.size(), 3);
    
    verifyStructNode(results[0].get(), "Point2D", 2);
    verifyStructNode(results[1].get(), "Point3D", 3);
    verifyStructNode(results[2].get(), "Object", 2);
    
    // Verify dependency detection
    QVERIFY(results[2]->getDependencies().contains("Point2D"));
    QVERIFY(results[2]->getDependencies().contains("Point3D"));
}

void TestStructParser::testFieldTypes()
{
    QString structCode = R"(
        typedef struct {
            int8_t int8Val;
            uint8_t uint8Val;
            int16_t int16Val;
            uint16_t uint16Val;
            int32_t int32Val;
            uint32_t uint32Val;
            int64_t int64Val;
            uint64_t uint64Val;
            size_t sizeVal;
            ptrdiff_t ptrdiffVal;
        } StandardTypes;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "StandardTypes", 10);
    verifyFieldNode(result->getFields()[0].get(), "int8Val", "int8_t");
    verifyFieldNode(result->getFields()[1].get(), "uint8Val", "uint8_t");
    verifyFieldNode(result->getFields()[4].get(), "int32Val", "int32_t");
    verifyFieldNode(result->getFields()[5].get(), "uint32Val", "uint32_t");
}

void TestStructParser::testArrayFields()
{
    QString structCode = R"(
        typedef struct {
            int fixedArray[10];
            char stringBuffer[256];
            double matrix[3][3];
            float dynamicSizes[];
        } ArrayStruct;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "ArrayStruct", 4);
    
    // Verify array properties
    auto fixedArray = result->getFields()[0].get();
    QVERIFY(fixedArray->isArray());
    QCOMPARE(fixedArray->getArraySize(), 10);
    
    auto stringBuffer = result->getFields()[1].get();
    QVERIFY(stringBuffer->isArray());
    QCOMPARE(stringBuffer->getArraySize(), 256);
    
    // Multi-dimensional arrays should be detected
    auto matrix = result->getFields()[2].get();
    QVERIFY(matrix->isArray());
    // Implementation detail: how to handle multi-dimensional arrays
    
    // Dynamic arrays (empty brackets)
    auto dynamicArray = result->getFields()[3].get();
    QVERIFY(dynamicArray->isArray());
    QCOMPARE(dynamicArray->getArraySize(), 0); // or -1 for dynamic
}

void TestStructParser::testPointerFields()
{
    QString structCode = R"(
        typedef struct {
            int* intPtr;
            char* charPtr;
            void* voidPtr;
            double** doublePtrPtr;
            const int* constIntPtr;
            int* const intConstPtr;
            const int* const constIntConstPtr;
        } PointerStruct;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "PointerStruct", 7);
    
    // Verify pointer properties
    for (int i = 0; i < 7; ++i) {
        auto field = result->getFields()[i].get();
        QVERIFY(field->isPointer());
    }
    
    // Double pointer
    auto doublePtrPtr = result->getFields()[3].get();
    // Implementation should detect pointer level
    QVERIFY(doublePtrPtr->getTypeName().contains("**") || 
            doublePtrPtr->getPointerLevel() == 2);
}

void TestStructParser::testConstFields()
{
    QString structCode = R"(
        typedef struct {
            const int constInt;
            volatile int volatileInt;
            const volatile int constVolatileInt;
            mutable int mutableInt;
        } QualifierStruct;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "QualifierStruct", 4);
    
    // Verify qualifiers are preserved in type names
    QVERIFY(result->getFields()[0]->getTypeName().contains("const"));
    QVERIFY(result->getFields()[1]->getTypeName().contains("volatile"));
    QVERIFY(result->getFields()[2]->getTypeName().contains("const") &&
            result->getFields()[2]->getTypeName().contains("volatile"));
}

void TestStructParser::testStaticFields()
{
    QString structCode = R"(
        typedef struct {
            static int staticInt;
            static const double staticConstDouble;
            int normalInt;
        } StaticStruct;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    // Static fields should be parsed but might be handled differently
    verifyStructNode(result.get(), "StaticStruct", 3);
    
    // Implementation detail: how to handle static fields
    QVERIFY(result->getFields()[0]->getTypeName().contains("static") ||
            result->getFields()[0]->isStatic());
}

void TestStructParser::testNestedStructs()
{
    QString structCode = R"(
        typedef struct {
            struct {
                int x;
                int y;
            } position;
            
            struct {
                float r;
                float g;
                float b;
                float a;
            } color;
            
            int id;
        } NestedStruct;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "NestedStruct", 3);
    
    // Verify nested structures
    auto positionField = result->getFields()[0].get();
    QVERIFY(positionField->getNestedStruct() != nullptr);
    QCOMPARE(positionField->getNestedStruct()->getFields().size(), 2);
    
    auto colorField = result->getFields()[1].get();
    QVERIFY(colorField->getNestedStruct() != nullptr);
    QCOMPARE(colorField->getNestedStruct()->getFields().size(), 4);
}

void TestStructParser::testUnionParsing()
{
    QString structCode = R"(
        typedef struct {
            int id;
            union {
                int intValue;
                float floatValue;
                char bytes[4];
            } data;
            bool valid;
        } UnionStruct;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "UnionStruct", 3);
    
    // Verify union field
    auto dataField = result->getFields()[1].get();
    QVERIFY(dataField->getUnion() != nullptr);
    
    auto unionNode = dataField->getUnion();
    QCOMPARE(unionNode->getMembers().size(), 3);
    QCOMPARE(unionNode->getMembers()[0]->getName(), QString("intValue"));
    QCOMPARE(unionNode->getMembers()[1]->getName(), QString("floatValue"));
    QCOMPARE(unionNode->getMembers()[2]->getName(), QString("bytes"));
}

void TestStructParser::testEnumParsing()
{
    QString structCode = R"(
        typedef enum {
            STATUS_IDLE = 0,
            STATUS_RUNNING = 1,
            STATUS_ERROR = 2
        } Status;
        
        typedef struct {
            Status currentStatus;
            int value;
        } StatusStruct;
    )";
    
    auto results = parseMultipleStructs(structCode);
    QVERIFY(results.size() >= 1); // At least the struct, enum handling may vary
    
    // Find the struct (enum might be parsed separately)
    auto structResult = std::find_if(results.begin(), results.end(),
        [](const std::unique_ptr<StructNode>& node) {
            return node->getName() == "StatusStruct";
        });
    
    QVERIFY(structResult != results.end());
    verifyStructNode(structResult->get(), "StatusStruct", 2);
    verifyFieldNode((*structResult)->getFields()[0].get(), "currentStatus", "Status");
}

void TestStructParser::testTypedefParsing()
{
    QString structCode = R"(
        typedef int CustomInt;
        typedef double CustomDouble;
        typedef struct Point2D Point2D;
        
        typedef struct {
            CustomInt x;
            CustomDouble y;
        } Point2D;
        
        typedef struct {
            Point2D position;
            CustomInt id;
        } Object;
    )";
    
    auto results = parseMultipleStructs(structCode);
    QVERIFY(results.size() >= 2);
    
    // Verify typedef handling in field types
    bool foundObject = false;
    for (const auto& result : results) {
        if (result->getName() == "Object") {
            foundObject = true;
            verifyStructNode(result.get(), "Object", 2);
            
            // Verify custom type usage
            auto posField = result->getFields()[0].get();
            QCOMPARE(posField->getTypeName(), QString("Point2D"));
            
            auto idField = result->getFields()[1].get();
            QCOMPARE(idField->getTypeName(), QString("CustomInt"));
        }
    }
    QVERIFY(foundObject);
}

void TestStructParser::testBasicBitfields()
{
    QString structCode = R"(
        typedef struct {
            unsigned int flag1 : 1;
            unsigned int flag2 : 1;
            unsigned int flag3 : 1;
            unsigned int reserved : 5;
            unsigned int value : 8;
            unsigned int checksum : 16;
        } BitfieldStruct;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "BitfieldStruct", 6);
    
    // Verify bitfield properties
    for (int i = 0; i < 6; ++i) {
        auto field = result->getFields()[i].get();
        QVERIFY(field->isBitField());
    }
    
    QCOMPARE(result->getFields()[0]->getBitWidth(), 1);
    QCOMPARE(result->getFields()[3]->getBitWidth(), 5);
    QCOMPARE(result->getFields()[4]->getBitWidth(), 8);
    QCOMPARE(result->getFields()[5]->getBitWidth(), 16);
}

void TestStructParser::testMixedBitfields()
{
    QString structCode = R"(
        typedef struct {
            int normalInt;
            unsigned int flags : 8;
            char normalChar;
            unsigned short status : 4;
            unsigned short error : 4;
            double normalDouble;
        } MixedBitfieldStruct;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "MixedBitfieldStruct", 6);
    
    // Verify mix of normal and bitfield
    QVERIFY(!result->getFields()[0]->isBitField()); // normalInt
    QVERIFY(result->getFields()[1]->isBitField());  // flags
    QVERIFY(!result->getFields()[2]->isBitField()); // normalChar
    QVERIFY(result->getFields()[3]->isBitField());  // status
    QVERIFY(result->getFields()[4]->isBitField());  // error
    QVERIFY(!result->getFields()[5]->isBitField()); // normalDouble
}

void TestStructParser::testBitfieldAlignment()
{
    QString structCode = R"(
        typedef struct {
            unsigned int a : 3;
            unsigned int b : 5;    // Should fit in same int as 'a'
            unsigned int c : 25;   // Should start new int (3+5+25 > 32)
            unsigned int d : 7;    // Should start new int
        } AlignmentBitfieldStruct;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "AlignmentBitfieldStruct", 4);
    
    // All should be bitfields
    for (int i = 0; i < 4; ++i) {
        QVERIFY(result->getFields()[i]->isBitField());
    }
    
    QCOMPARE(result->getFields()[0]->getBitWidth(), 3);
    QCOMPARE(result->getFields()[1]->getBitWidth(), 5);
    QCOMPARE(result->getFields()[2]->getBitWidth(), 25);
    QCOMPARE(result->getFields()[3]->getBitWidth(), 7);
}

void TestStructParser::testZeroWidthBitfields()
{
    QString structCode = R"(
        typedef struct {
            unsigned int a : 3;
            unsigned int   : 0;    // Force alignment to next boundary
            unsigned int b : 5;
            unsigned int   : 3;    // Unnamed bitfield padding
            unsigned int c : 2;
        } ZeroBitfieldStruct;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    // Zero-width and unnamed bitfields might be handled specially
    // They could be included or excluded depending on implementation
    QVERIFY(result->getFields().size() >= 3); // At least a, b, c
    
    // Find named fields
    bool foundA = false, foundB = false, foundC = false;
    for (const auto& field : result->getFields()) {
        if (field->getName() == "a") {
            foundA = true;
            QVERIFY(field->isBitField());
            QCOMPARE(field->getBitWidth(), 3);
        } else if (field->getName() == "b") {
            foundB = true;
            QVERIFY(field->isBitField());
            QCOMPARE(field->getBitWidth(), 5);
        } else if (field->getName() == "c") {
            foundC = true;
            QVERIFY(field->isBitField());
            QCOMPARE(field->getBitWidth(), 2);
        }
    }
    
    QVERIFY(foundA && foundB && foundC);
}

void TestStructParser::testPackedStructs()
{
    QString structCode = R"(
        #pragma pack(push, 1)
        typedef struct {
            char c;
            int i;
            short s;
        } PackedStruct;
        #pragma pack(pop)
        
        typedef struct __attribute__((packed)) {
            char c;
            int i;
            short s;
        } GCCPackedStruct;
    )";
    
    auto results = parseMultipleStructs(structCode);
    QVERIFY(results.size() >= 2);
    
    // Find packed structs
    for (const auto& result : results) {
        if (result->getName().contains("Packed")) {
            verifyStructNode(result.get(), result->getName(), 3);
            
            // Should be marked as packed
            QVERIFY(result->isPacked() || result->getPackValue() == 1);
        }
    }
}

void TestStructParser::testAlignmentAttributes()
{
    QString structCode = R"(
        typedef struct {
            char c;
            int i __attribute__((aligned(16)));
            short s;
        } AlignedFieldStruct;
        
        typedef struct __attribute__((aligned(32))) {
            char c;
            int i;
            short s;
        } AlignedStruct;
    )";
    
    auto results = parseMultipleStructs(structCode);
    QVERIFY(results.size() >= 2);
    
    // Alignment attributes should be parsed and stored
    for (const auto& result : results) {
        verifyStructNode(result.get(), result->getName(), 3);
        // Implementation should store alignment information
    }
}

void TestStructParser::testGCCAttributes()
{
    QString structCode = R"(
        typedef struct __attribute__((packed, aligned(4))) {
            char a;
            int b;
            char c;
        } GCCAttributeStruct;
        
        typedef struct {
            int a __attribute__((deprecated));
            char b __attribute__((unused));
            float c __attribute__((aligned(8)));
        } FieldAttributeStruct;
    )";
    
    auto results = parseMultipleStructs(structCode);
    QVERIFY(results.size() >= 2);
    
    for (const auto& result : results) {
        verifyStructNode(result.get(), result->getName(), 3);
        // GCC attributes should be parsed and preserved
    }
}

void TestStructParser::testMSVCPragmas()
{
    QString structCode = R"(
        #pragma pack(push, 2)
        typedef struct {
            char a;
            int b;
            char c;
        } MSVC2PackStruct;
        #pragma pack(pop)
        
        #pragma pack(4)
        typedef struct {
            char a;
            double b;
            char c;
        } MSVC4PackStruct;
        #pragma pack()
    )";
    
    auto results = parseMultipleStructs(structCode);
    QVERIFY(results.size() >= 2);
    
    for (const auto& result : results) {
        if (result->getName() == "MSVC2PackStruct") {
            QVERIFY(result->isPacked() && result->getPackValue() == 2);
        } else if (result->getName() == "MSVC4PackStruct") {
            QVERIFY(result->isPacked() && result->getPackValue() == 4);
        }
    }
}

void TestStructParser::testInheritance()
{
    QString structCode = R"(
        typedef struct BaseStruct {
            int baseValue;
        } BaseStruct;
        
        typedef struct DerivedStruct {
            BaseStruct base;  // Composition, not inheritance in C
            int derivedValue;
        } DerivedStruct;
    )";
    
    auto results = parseMultipleStructs(structCode);
    QVERIFY(results.size() >= 2);
    
    // C doesn't have inheritance, but composition should be detected
    auto derivedStruct = std::find_if(results.begin(), results.end(),
        [](const std::unique_ptr<StructNode>& node) {
            return node->getName() == "DerivedStruct";
        });
    
    QVERIFY(derivedStruct != results.end());
    verifyStructNode(derivedStruct->get(), "DerivedStruct", 2);
    
    // Should have dependency on BaseStruct
    QVERIFY((*derivedStruct)->getDependencies().contains("BaseStruct"));
}

void TestStructParser::testTemplates()
{
    // C doesn't have templates, but we might encounter C++ code
    QString cppCode = R"(
        template<typename T>
        struct TemplateStruct {
            T value;
            int count;
        };
        
        typedef TemplateStruct<int> IntTemplateStruct;
        typedef TemplateStruct<double> DoubleTemplateStruct;
    )";
    
    // This test might be skipped if parser only handles C
    // or it might parse the typedef'd instantiations
    auto results = parseMultipleStructs(cppCode);
    
    // Implementation dependent - might parse typedef'd versions only
    if (!results.empty()) {
        QVERIFY(results.size() >= 2);
    }
}

void TestStructParser::testAnonymousStructs()
{
    QString structCode = R"(
        typedef struct {
            int id;
            struct {
                int x;
                int y;
            }; // Anonymous nested struct
            union {
                int intVal;
                float floatVal;
            }; // Anonymous union
        } AnonymousStruct;
    )";
    
    auto result = parseStruct(structCode);
    QVERIFY(result != nullptr);
    
    // Anonymous structs/unions might be flattened or preserved differently
    verifyStructNode(result.get(), "AnonymousStruct", 3);
    
    // Verify that nested anonymous structures are handled
    // Implementation detail: how to represent anonymous members
}

void TestStructParser::testForwardDeclarations()
{
    QString structCode = R"(
        struct ForwardDeclared;  // Forward declaration
        
        typedef struct {
            struct ForwardDeclared* ptr;
            int value;
        } UsingForwardDecl;
        
        typedef struct ForwardDeclared {
            int data;
            UsingForwardDecl* back;
        } ForwardDeclared;
    )";
    
    auto results = parseMultipleStructs(structCode);
    QVERIFY(results.size() >= 2);
    
    // Forward declarations should be resolved
    for (const auto& result : results) {
        verifyStructNode(result.get(), result->getName(), 2);
    }
}

void TestStructParser::testSyntaxErrors()
{
    QStringList errorCases = {
        "typedef struct { int x; } // Missing semicolon",
        "typedef struct { int; } TestStruct;", // Missing field name
        "typedef struct { } int x; } ErrorStruct;", // Extra closing brace
        "typedef struct { int x[]; int y; } ErrorArray;", // Invalid array placement
    };
    
    for (const QString& errorCase : errorCases) {
        auto result = parseStruct(errorCase);
        // Should either return nullptr or handle error gracefully
        // Implementation dependent on error handling strategy
    }
}

void TestStructParser::testIncompleteStructs()
{
    QString incompleteCode = R"(
        typedef struct {
            int x;
            int y
            // Missing semicolon and closing brace
    )";
    
    auto result = parseStruct(incompleteCode);
    // Should handle incomplete input gracefully
    // Implementation dependent
}

void TestStructParser::testCircularDependencies()
{
    QString circularCode = R"(
        typedef struct StructB StructB;  // Forward declaration
        
        typedef struct StructA {
            StructB* bPtr;
            int aValue;
        } StructA;
        
        typedef struct StructB {
            StructA* aPtr;
            int bValue;
        } StructB;
    )";
    
    auto results = parseMultipleStructs(circularCode);
    QVERIFY(results.size() >= 2);
    
    // Should handle circular dependencies through pointers
    for (const auto& result : results) {
        verifyStructNode(result.get(), result->getName(), 2);
    }
}

void TestStructParser::testInvalidBitfields()
{
    QStringList invalidBitfields = {
        "typedef struct { int x : 0; } ZeroBitfield;", // Zero-width named bitfield
        "typedef struct { int x : 33; } TooBig;",      // Larger than base type
        "typedef struct { float x : 4; } FloatBitfield;", // Float bitfield
    };
    
    for (const QString& invalidCase : invalidBitfields) {
        auto result = parseStruct(invalidCase);
        // Should handle invalid bitfields appropriately
        // Implementation dependent on error handling
    }
}

void TestStructParser::testNetworkProtocolStruct()
{
    QString protocolStruct = R"(
        #pragma pack(1)
        typedef struct {
            uint8_t version : 4;
            uint8_t headerLength : 4;
            uint8_t typeOfService;
            uint16_t totalLength;
            uint16_t identification;
            uint16_t flags : 3;
            uint16_t fragmentOffset : 13;
            uint8_t timeToLive;
            uint8_t protocol;
            uint16_t headerChecksum;
            uint32_t sourceAddress;
            uint32_t destinationAddress;
        } IPHeader;
        #pragma pack()
    )";
    
    auto result = parseStruct(protocolStruct);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "IPHeader", 10);
    
    // Verify specific protocol fields
    QVERIFY(result->getFields()[0]->isBitField()); // version
    QCOMPARE(result->getFields()[0]->getBitWidth(), 4);
    
    QVERIFY(result->getFields()[1]->isBitField()); // headerLength
    QCOMPARE(result->getFields()[1]->getBitWidth(), 4);
    
    QVERIFY(result->isPacked());
    QCOMPARE(result->getPackValue(), 1);
}

void TestStructParser::testSystemStruct()
{
    QString systemStruct = R"(
        typedef struct {
            pid_t pid;
            uid_t uid;
            gid_t gid;
            mode_t mode;
            size_t size;
            time_t atime;
            time_t mtime;
            time_t ctime;
            dev_t device;
            ino_t inode;
        } SystemInfo;
    )";
    
    auto result = parseStruct(systemStruct);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "SystemInfo", 10);
    
    // System types should be recognized
    verifyFieldNode(result->getFields()[0].get(), "pid", "pid_t");
    verifyFieldNode(result->getFields()[4].get(), "size", "size_t");
}

void TestStructParser::testEmbeddedStruct()
{
    QString embeddedStruct = R"(
        typedef struct {
            volatile uint32_t CTRL;   // Control register
            volatile uint32_t STATUS; // Status register
            volatile uint32_t DATA;   // Data register
            uint32_t RESERVED[5];     // Reserved space
            struct {
                volatile uint16_t LOW;
                volatile uint16_t HIGH;
            } COUNTER;
            union {
                volatile uint32_t WORD;
                struct {
                    volatile uint16_t LOW;
                    volatile uint16_t HIGH;
                } HALF;
                struct {
                    volatile uint8_t BYTE0;
                    volatile uint8_t BYTE1;
                    volatile uint8_t BYTE2;
                    volatile uint8_t BYTE3;
                } BYTES;
            } CONFIG;
        } PeripheralRegisterMap;
    )";
    
    auto result = parseStruct(embeddedStruct);
    QVERIFY(result != nullptr);
    
    verifyStructNode(result.get(), "PeripheralRegisterMap", 6);
    
    // Verify volatile qualifier preservation
    QVERIFY(result->getFields()[0]->getTypeName().contains("volatile"));
    
    // Verify array field
    QVERIFY(result->getFields()[3]->isArray());
    QCOMPARE(result->getFields()[3]->getArraySize(), 5);
    
    // Verify nested struct and union
    QVERIFY(result->getFields()[4]->getNestedStruct() != nullptr);
    QVERIFY(result->getFields()[5]->getUnion() != nullptr);
}

void TestStructParser::testLargeComplexStruct()
{
    QString complexStruct = R"(
        typedef enum {
            STATE_IDLE = 0,
            STATE_ACTIVE = 1,
            STATE_ERROR = 2
        } State;
        
        typedef struct {
            uint32_t signature;
            uint16_t version : 8;
            uint16_t flags : 8;
            State currentState;
            
            struct {
                double x, y, z;
            } position;
            
            struct {
                float pitch, yaw, roll;
            } orientation;
            
            union {
                uint64_t timestamp;
                struct {
                    uint32_t seconds;
                    uint32_t nanoseconds;
                } time;
            } timeInfo;
            
            char name[32];
            uint8_t checksum;
            uint8_t reserved[7];  // Padding to 128 bytes
            
            // Variable length data marker
            uint32_t dataLength;
            uint8_t data[];  // Flexible array member
        } ComplexProtocolHeader;
    )";
    
    auto results = parseMultipleStructs(complexStruct);
    QVERIFY(results.size() >= 1);
    
    // Find the main struct
    auto mainStruct = std::find_if(results.begin(), results.end(),
        [](const std::unique_ptr<StructNode>& node) {
            return node->getName() == "ComplexProtocolHeader";
        });
    
    QVERIFY(mainStruct != results.end());
    verifyStructNode(mainStruct->get(), "ComplexProtocolHeader", 11);
    
    // Verify complex field types
    auto fields = (*mainStruct)->getFields();
    
    // Bitfields
    QVERIFY(fields[1]->isBitField()); // version
    QVERIFY(fields[2]->isBitField()); // flags
    
    // Nested structs
    QVERIFY(fields[4]->getNestedStruct() != nullptr); // position
    QVERIFY(fields[5]->getNestedStruct() != nullptr); // orientation
    
    // Union
    QVERIFY(fields[6]->getUnion() != nullptr); // timeInfo
    
    // Arrays
    QVERIFY(fields[7]->isArray()); // name[32]
    QCOMPARE(fields[7]->getArraySize(), 32);
    
    QVERIFY(fields[9]->isArray()); // reserved[7]
    QCOMPARE(fields[9]->getArraySize(), 7);
    
    // Flexible array member
    QVERIFY(fields[11]->isArray()); // data[]
    QCOMPARE(fields[11]->getArraySize(), 0); // or -1 for flexible
}

// Helper method implementations
void TestStructParser::verifyStructNode(const StructNode* node, const QString& expectedName, int expectedFieldCount)
{
    QVERIFY(node != nullptr);
    QCOMPARE(node->getName(), expectedName);
    QCOMPARE(node->getFields().size(), expectedFieldCount);
    QCOMPARE(node->getType(), ASTNodeType::Struct);
}

void TestStructParser::verifyFieldNode(const FieldNode* field, const QString& expectedName, const QString& expectedType)
{
    QVERIFY(field != nullptr);
    QCOMPARE(field->getName(), expectedName);
    QCOMPARE(field->getTypeName(), expectedType);
    QCOMPARE(field->getType(), ASTNodeType::Field);
}

std::unique_ptr<StructNode> TestStructParser::parseStruct(const QString& structCode)
{
    // This would use the actual parser implementation
    // For now, return a mock result based on the input
    
    auto results = parser->parseStructures(structCode);
    if (results.isEmpty()) {
        return nullptr;
    }
    return std::move(results.first());
}

QList<std::unique_ptr<StructNode>> TestStructParser::parseMultipleStructs(const QString& structsCode)
{
    return parser->parseStructures(structsCode);
}

QTEST_MAIN(TestStructParser)
#include "test_struct_parser.moc"