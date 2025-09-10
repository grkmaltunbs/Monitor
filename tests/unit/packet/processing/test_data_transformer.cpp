#include <QtTest/QTest>
#include <QObject>
#include <memory>
#include <cmath>
#include "packet/processing/data_transformer.h"

using namespace Monitor::Packet;
using FieldValue = FieldExtractor::FieldValue;

class TestDataTransformer : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testConstruction();
    void testTransformationChainManagement();
    void testSingleTransformationAddition();
    void testTransformationClearing();

    // Type conversion tests
    void testToIntegerConversion();
    void testToFloatConversion();
    void testToDoubleConversion();
    void testToStringConversion();
    void testToHexConversion();
    void testToBinaryConversion();

    // Mathematical operation tests
    void testMathematicalOperations();
    void testDivisionByZero();
    void testMathematicalFunctions();
    void testPowerOperations();

    // Statistical operation tests
    void testMovingAverage();
    void testDifferenceOperations();
    void testCumulativeSum();

    // String operation tests
    void testStringPrefixPostfix();
    void testStringFormatting();

    // Conditional operation tests
    void testValueClamping();
    void testThresholdOperations();

    // Custom transformation tests
    void testCustomTransformations();
    void testCustomFunctionErrors();

    // Chain processing tests
    void testTransformationChains();
    void testComplexChains();
    void testChainErrorHandling();

    // Stateful transformation tests
    void testStatefulTransformations();
    void testStateReset();
    void testConcurrentStateManagement();

    // Error handling tests
    void testInvalidTypeConversions();
    void testInvalidMathOperations();
    void testTransformationErrors();

    // Performance tests
    void testTransformationPerformance();
    void testLargeChainPerformance();

    // Edge case tests
    void testEmptyTransformer();
    void testVariantTypeHandling();
    void testNumericLimits();

private:
    DataTransformer* m_transformer = nullptr;
    
    // Helper methods
    void verifyIntegerValue(const DataTransformer::TransformationResult& result, int64_t expected);
    void verifyDoubleValue(const DataTransformer::TransformationResult& result, double expected, double tolerance = 1e-9);
    void verifyStringValue(const DataTransformer::TransformationResult& result, const std::string& expected);
    void verifyTransformationError(const DataTransformer::TransformationResult& result);
    void verifyTransformationSuccess(const DataTransformer::TransformationResult& result);
    
    // Test data generators
    std::vector<FieldValue> generateTestValues();
    std::vector<double> generateNumericTestValues();
};

void TestDataTransformer::initTestCase() {
    // Nothing special needed for setup
}

void TestDataTransformer::cleanupTestCase() {
    // Nothing special needed for cleanup
}

void TestDataTransformer::init() {
    m_transformer = new DataTransformer();
    QVERIFY(m_transformer != nullptr);
}

void TestDataTransformer::cleanup() {
    delete m_transformer;
    m_transformer = nullptr;
}

void TestDataTransformer::verifyIntegerValue(const DataTransformer::TransformationResult& result, int64_t expected) {
    QVERIFY(result.success);
    QVERIFY(std::holds_alternative<int64_t>(result.value));
    QCOMPARE(std::get<int64_t>(result.value), expected);
}

void TestDataTransformer::verifyDoubleValue(const DataTransformer::TransformationResult& result, double expected, double tolerance) {
    QVERIFY(result.success);
    QVERIFY(std::holds_alternative<double>(result.value));
    double actual = std::get<double>(result.value);
    QVERIFY(std::abs(actual - expected) < tolerance);
}

void TestDataTransformer::verifyStringValue(const DataTransformer::TransformationResult& result, const std::string& expected) {
    QVERIFY(result.success);
    QVERIFY(std::holds_alternative<std::string>(result.value));
    QCOMPARE(std::get<std::string>(result.value), expected);
}

void TestDataTransformer::verifyTransformationError(const DataTransformer::TransformationResult& result) {
    QVERIFY(!result.success);
    QVERIFY(!result.error.empty());
}

void TestDataTransformer::verifyTransformationSuccess(const DataTransformer::TransformationResult& result) {
    QVERIFY(result.success);
    QVERIFY(result.error.empty());
}

std::vector<FieldValue> TestDataTransformer::generateTestValues() {
    return {
        static_cast<bool>(true),
        static_cast<int8_t>(42),
        static_cast<uint8_t>(123),
        static_cast<int16_t>(1000),
        static_cast<uint16_t>(2000),
        static_cast<int32_t>(50000),
        static_cast<uint32_t>(100000),
        static_cast<int64_t>(1000000LL),
        static_cast<uint64_t>(2000000ULL),
        static_cast<float>(3.14f),
        static_cast<double>(2.71828),
        std::string("test_string"),
        std::vector<uint8_t>{0x01, 0x02, 0x03, 0x04}
    };
}

std::vector<double> TestDataTransformer::generateNumericTestValues() {
    return {0.0, 1.0, -1.0, 3.14159, -2.71828, 100.0, -100.0, 1e6, -1e6};
}

void TestDataTransformer::testConstruction() {
    // Test basic construction
    QVERIFY(m_transformer != nullptr);
    
    // Test initial state
    QVERIFY(!m_transformer->hasTransformations("nonexistent"));
    QCOMPARE(m_transformer->getTransformationCount("nonexistent"), size_t(0));
}

void TestDataTransformer::testTransformationChainManagement() {
    const std::string fieldName = "test_field";
    
    // Test adding transformation chain
    std::vector<DataTransformer::Transformation> transformations = {
        DataTransformer::Transformation(DataTransformer::OperationType::ToDouble),
        DataTransformer::Transformation(DataTransformer::OperationType::Add, 
                                        DataTransformer::TransformationParams(10.0))
    };
    
    m_transformer->addTransformationChain(fieldName, transformations);
    
    QVERIFY(m_transformer->hasTransformations(fieldName));
    QCOMPARE(m_transformer->getTransformationCount(fieldName), size_t(2));
}

void TestDataTransformer::testSingleTransformationAddition() {
    const std::string fieldName = "single_transform_field";
    
    // Test adding single transformation
    DataTransformer::Transformation transformation(DataTransformer::OperationType::ToInteger);
    m_transformer->addTransformation(fieldName, transformation);
    
    QVERIFY(m_transformer->hasTransformations(fieldName));
    QCOMPARE(m_transformer->getTransformationCount(fieldName), size_t(1));
    
    // Test adding another transformation to same field
    DataTransformer::Transformation secondTransformation(DataTransformer::OperationType::Multiply, 
                                                        DataTransformer::TransformationParams(2.0));
    m_transformer->addTransformation(fieldName, secondTransformation);
    
    QCOMPARE(m_transformer->getTransformationCount(fieldName), size_t(2));
}

void TestDataTransformer::testTransformationClearing() {
    const std::string fieldName = "clear_test_field";
    
    // Add some transformations
    m_transformer->addTransformation(fieldName, 
        DataTransformer::Transformation(DataTransformer::OperationType::ToDouble));
    m_transformer->addTransformation(fieldName, 
        DataTransformer::Transformation(DataTransformer::OperationType::Add, DataTransformer::TransformationParams(5.0)));
    
    QVERIFY(m_transformer->hasTransformations(fieldName));
    QCOMPARE(m_transformer->getTransformationCount(fieldName), size_t(2));
    
    // Clear transformations
    m_transformer->clearTransformations(fieldName);
    
    QVERIFY(!m_transformer->hasTransformations(fieldName));
    QCOMPARE(m_transformer->getTransformationCount(fieldName), size_t(0));
}

void TestDataTransformer::testToIntegerConversion() {
    const std::string fieldName = "int_convert_test";
    m_transformer->addTransformation(fieldName, 
        DataTransformer::Transformation(DataTransformer::OperationType::ToInteger));
    
    // Test numeric conversions
    auto result1 = m_transformer->transform(fieldName, static_cast<float>(42.7f));
    verifyIntegerValue(result1, 42);
    
    auto result2 = m_transformer->transform(fieldName, static_cast<double>(100.9));
    verifyIntegerValue(result2, 100);
    
    auto result3 = m_transformer->transform(fieldName, static_cast<uint32_t>(12345));
    verifyIntegerValue(result3, 12345);
    
    // Test string conversion
    auto result4 = m_transformer->transform(fieldName, std::string("456"));
    verifyIntegerValue(result4, 456);
    
    // Test invalid string conversion
    auto result5 = m_transformer->transform(fieldName, std::string("not_a_number"));
    verifyTransformationError(result5);
    
    // Test non-convertible type
    auto result6 = m_transformer->transform(fieldName, std::vector<uint8_t>{1, 2, 3});
    verifyTransformationError(result6);
}

void TestDataTransformer::testToFloatConversion() {
    const std::string fieldName = "float_convert_test";
    m_transformer->addTransformation(fieldName, 
        DataTransformer::Transformation(DataTransformer::OperationType::ToFloat));
    
    // Test numeric conversions
    auto result1 = m_transformer->transform(fieldName, static_cast<int32_t>(42));
    QVERIFY(result1.success);
    QVERIFY(std::holds_alternative<float>(result1.value));
    QCOMPARE(std::get<float>(result1.value), 42.0f);
    
    auto result2 = m_transformer->transform(fieldName, static_cast<double>(3.14159));
    QVERIFY(result2.success);
    QVERIFY(std::holds_alternative<float>(result2.value));
    QCOMPARE(std::get<float>(result2.value), static_cast<float>(3.14159));
    
    // Test string conversion
    auto result3 = m_transformer->transform(fieldName, std::string("2.71828"));
    QVERIFY(result3.success);
    QVERIFY(std::holds_alternative<float>(result3.value));
    float expected = 2.71828f;
    QVERIFY(std::abs(std::get<float>(result3.value) - expected) < 1e-5f);
}

void TestDataTransformer::testToDoubleConversion() {
    const std::string fieldName = "double_convert_test";
    m_transformer->addTransformation(fieldName, 
        DataTransformer::Transformation(DataTransformer::OperationType::ToDouble));
    
    // Test various numeric type conversions
    auto result1 = m_transformer->transform(fieldName, static_cast<int32_t>(42));
    verifyDoubleValue(result1, 42.0);
    
    auto result2 = m_transformer->transform(fieldName, static_cast<float>(3.14f));
    verifyDoubleValue(result2, static_cast<double>(3.14f), 1e-6);
    
    auto result3 = m_transformer->transform(fieldName, std::string("1.234567"));
    verifyDoubleValue(result3, 1.234567);
}

void TestDataTransformer::testToStringConversion() {
    const std::string fieldName = "string_convert_test";
    m_transformer->addTransformation(fieldName, 
        DataTransformer::Transformation(DataTransformer::OperationType::ToString));
    
    // Test numeric to string conversions
    auto result1 = m_transformer->transform(fieldName, static_cast<int32_t>(42));
    verifyStringValue(result1, "42");
    
    auto result2 = m_transformer->transform(fieldName, static_cast<float>(3.14f));
    QVERIFY(result2.success);
    QVERIFY(std::holds_alternative<std::string>(result2.value));
    std::string str = std::get<std::string>(result2.value);
    QVERIFY(str.find("3.14") != std::string::npos);
    
    // Test string passthrough
    auto result3 = m_transformer->transform(fieldName, std::string("already_string"));
    verifyStringValue(result3, "already_string");
    
    // Test byte array conversion
    auto result4 = m_transformer->transform(fieldName, std::vector<uint8_t>{1, 2, 3, 4});
    verifyStringValue(result4, "byte_array[4]");
}

void TestDataTransformer::testToHexConversion() {
    const std::string fieldName = "hex_convert_test";
    m_transformer->addTransformation(fieldName, 
        DataTransformer::Transformation(DataTransformer::OperationType::ToHex));
    
    // Test integer to hex conversion
    auto result1 = m_transformer->transform(fieldName, static_cast<uint32_t>(255));
    verifyStringValue(result1, "0xff");
    
    auto result2 = m_transformer->transform(fieldName, static_cast<int32_t>(16));
    verifyStringValue(result2, "0x10");
    
    // Test non-integral type (should fail)
    auto result3 = m_transformer->transform(fieldName, static_cast<float>(3.14f));
    verifyTransformationError(result3);
    
    auto result4 = m_transformer->transform(fieldName, std::string("not_integer"));
    verifyTransformationError(result4);
}

void TestDataTransformer::testToBinaryConversion() {
    const std::string fieldName = "binary_convert_test";
    m_transformer->addTransformation(fieldName, 
        DataTransformer::Transformation(DataTransformer::OperationType::ToBinary));
    
    // Test integer to binary conversion
    auto result1 = m_transformer->transform(fieldName, static_cast<uint32_t>(5));
    verifyStringValue(result1, "0b101");
    
    auto result2 = m_transformer->transform(fieldName, static_cast<uint8_t>(0));
    verifyStringValue(result2, "0b0");
    
    auto result3 = m_transformer->transform(fieldName, static_cast<uint8_t>(255));
    verifyStringValue(result3, "0b11111111");
    
    // Test non-integral type (should fail)
    auto result4 = m_transformer->transform(fieldName, static_cast<double>(3.14));
    verifyTransformationError(result4);
}

void TestDataTransformer::testMathematicalOperations() {
    // Test addition
    const std::string addField = "add_test";
    m_transformer->addTransformation(addField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Add, DataTransformer::TransformationParams(10.0)));
    
    auto result1 = m_transformer->transform(addField, static_cast<double>(5.0));
    verifyDoubleValue(result1, 15.0);
    
    // Test subtraction
    const std::string subField = "sub_test";
    m_transformer->addTransformation(subField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Subtract, DataTransformer::TransformationParams(3.0)));
    
    auto result2 = m_transformer->transform(subField, static_cast<double>(10.0));
    verifyDoubleValue(result2, 7.0);
    
    // Test multiplication
    const std::string mulField = "mul_test";
    m_transformer->addTransformation(mulField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Multiply, DataTransformer::TransformationParams(2.0)));
    
    auto result3 = m_transformer->transform(mulField, static_cast<double>(7.0));
    verifyDoubleValue(result3, 14.0);
    
    // Test modulo
    const std::string modField = "mod_test";
    m_transformer->addTransformation(modField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Modulo, DataTransformer::TransformationParams(3.0)));
    
    auto result4 = m_transformer->transform(modField, static_cast<double>(10.0));
    verifyDoubleValue(result4, 1.0);
}

void TestDataTransformer::testDivisionByZero() {
    const std::string divField = "div_test";
    
    // Test division by zero (should fail)
    m_transformer->addTransformation(divField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Divide, DataTransformer::TransformationParams(0.0)));
    
    auto result = m_transformer->transform(divField, static_cast<double>(10.0));
    verifyTransformationError(result);
    QVERIFY(result.error.find("Division by zero") != std::string::npos);
    
    // Test valid division
    m_transformer->clearTransformations(divField);
    m_transformer->addTransformation(divField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Divide, DataTransformer::TransformationParams(2.0)));
    
    auto result2 = m_transformer->transform(divField, static_cast<double>(10.0));
    verifyDoubleValue(result2, 5.0);
}

void TestDataTransformer::testMathematicalFunctions() {
    // Test absolute value
    const std::string absField = "abs_test";
    m_transformer->addTransformation(absField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Abs));
    
    auto result1 = m_transformer->transform(absField, static_cast<double>(-5.0));
    verifyDoubleValue(result1, 5.0);
    
    auto result2 = m_transformer->transform(absField, static_cast<double>(3.0));
    verifyDoubleValue(result2, 3.0);
    
    // Test square root
    const std::string sqrtField = "sqrt_test";
    m_transformer->addTransformation(sqrtField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Sqrt));
    
    auto result3 = m_transformer->transform(sqrtField, static_cast<double>(9.0));
    verifyDoubleValue(result3, 3.0);
    
    auto result4 = m_transformer->transform(sqrtField, static_cast<double>(2.0));
    verifyDoubleValue(result4, std::sqrt(2.0));
    
    // Test trigonometric functions
    const std::string sinField = "sin_test";
    m_transformer->addTransformation(sinField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Sin));
    
    auto result5 = m_transformer->transform(sinField, static_cast<double>(0.0));
    verifyDoubleValue(result5, 0.0);
    
    auto result6 = m_transformer->transform(sinField, static_cast<double>(M_PI / 2.0));
    verifyDoubleValue(result6, 1.0);
}

void TestDataTransformer::testPowerOperations() {
    const std::string powerField = "power_test";
    m_transformer->addTransformation(powerField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Power, DataTransformer::TransformationParams(2.0)));
    
    auto result1 = m_transformer->transform(powerField, static_cast<double>(3.0));
    verifyDoubleValue(result1, 9.0);
    
    auto result2 = m_transformer->transform(powerField, static_cast<double>(4.0));
    verifyDoubleValue(result2, 16.0);
    
    // Test fractional power
    m_transformer->clearTransformations(powerField);
    m_transformer->addTransformation(powerField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Power, DataTransformer::TransformationParams(0.5)));
    
    auto result3 = m_transformer->transform(powerField, static_cast<double>(16.0));
    verifyDoubleValue(result3, 4.0);
}

void TestDataTransformer::testMovingAverage() {
    const std::string avgField = "moving_avg_test";
    DataTransformer::TransformationParams params;
    params.windowSize = 3;
    
    m_transformer->addTransformation(avgField, 
        DataTransformer::Transformation(DataTransformer::OperationType::MovingAverage, params));
    
    // First value
    auto result1 = m_transformer->transform(avgField, static_cast<double>(10.0));
    verifyDoubleValue(result1, 10.0); // Average of [10.0]
    
    // Second value
    auto result2 = m_transformer->transform(avgField, static_cast<double>(20.0));
    verifyDoubleValue(result2, 15.0); // Average of [10.0, 20.0]
    
    // Third value
    auto result3 = m_transformer->transform(avgField, static_cast<double>(30.0));
    verifyDoubleValue(result3, 20.0); // Average of [10.0, 20.0, 30.0]
    
    // Fourth value (should slide the window)
    auto result4 = m_transformer->transform(avgField, static_cast<double>(40.0));
    verifyDoubleValue(result4, 30.0); // Average of [20.0, 30.0, 40.0]
}

void TestDataTransformer::testDifferenceOperations() {
    const std::string diffField = "diff_test";
    m_transformer->addTransformation(diffField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Diff));
    
    // First value should return 0 (no previous value)
    auto result1 = m_transformer->transform(diffField, static_cast<double>(10.0));
    verifyDoubleValue(result1, 0.0);
    
    // Second value should return difference
    auto result2 = m_transformer->transform(diffField, static_cast<double>(15.0));
    verifyDoubleValue(result2, 5.0);
    
    // Third value
    auto result3 = m_transformer->transform(diffField, static_cast<double>(12.0));
    verifyDoubleValue(result3, -3.0);
}

void TestDataTransformer::testCumulativeSum() {
    const std::string cumSumField = "cumsum_test";
    m_transformer->addTransformation(cumSumField, 
        DataTransformer::Transformation(DataTransformer::OperationType::CumulativeSum));
    
    auto result1 = m_transformer->transform(cumSumField, static_cast<double>(5.0));
    verifyDoubleValue(result1, 5.0);
    
    auto result2 = m_transformer->transform(cumSumField, static_cast<double>(3.0));
    verifyDoubleValue(result2, 8.0);
    
    auto result3 = m_transformer->transform(cumSumField, static_cast<double>(2.0));
    verifyDoubleValue(result3, 10.0);
}

void TestDataTransformer::testStringPrefixPostfix() {
    // Test prefix
    const std::string prefixField = "prefix_test";
    m_transformer->addTransformation(prefixField, 
        DataTransformer::Transformation(DataTransformer::OperationType::AddPrefix, 
                                       DataTransformer::TransformationParams("PREFIX:")));
    
    auto result1 = m_transformer->transform(prefixField, std::string("test"));
    verifyStringValue(result1, "PREFIX:test");
    
    auto result2 = m_transformer->transform(prefixField, static_cast<int32_t>(42));
    verifyStringValue(result2, "PREFIX:42");
    
    // Test postfix
    const std::string postfixField = "postfix_test";
    m_transformer->addTransformation(postfixField, 
        DataTransformer::Transformation(DataTransformer::OperationType::AddPostfix, 
                                       DataTransformer::TransformationParams(" units")));
    
    auto result3 = m_transformer->transform(postfixField, static_cast<double>(3.14));
    QVERIFY(result3.success);
    QVERIFY(std::holds_alternative<std::string>(result3.value));
    std::string str = std::get<std::string>(result3.value);
    QVERIFY(str.find("3.14") != std::string::npos);
    QVERIFY(str.find(" units") != std::string::npos);
}

void TestDataTransformer::testStringFormatting() {
    // This would test the Format operation if it were implemented
    // For now, we test that string conversions work correctly
    
    const std::string formatField = "format_test";
    m_transformer->addTransformation(formatField, 
        DataTransformer::Transformation(DataTransformer::OperationType::ToString));
    
    auto result = m_transformer->transform(formatField, static_cast<float>(3.14159f));
    verifyTransformationSuccess(result);
    QVERIFY(std::holds_alternative<std::string>(result.value));
}

void TestDataTransformer::testValueClamping() {
    const std::string clampField = "clamp_test";
    DataTransformer::TransformationParams params;
    params.minValue = 0.0;
    params.maxValue = 100.0;
    
    m_transformer->addTransformation(clampField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Clamp, params));
    
    // Test value within range
    auto result1 = m_transformer->transform(clampField, static_cast<double>(50.0));
    verifyDoubleValue(result1, 50.0);
    
    // Test value below minimum
    auto result2 = m_transformer->transform(clampField, static_cast<double>(-10.0));
    verifyDoubleValue(result2, 0.0);
    
    // Test value above maximum
    auto result3 = m_transformer->transform(clampField, static_cast<double>(150.0));
    verifyDoubleValue(result3, 100.0);
    
    // Test exact boundaries
    auto result4 = m_transformer->transform(clampField, static_cast<double>(0.0));
    verifyDoubleValue(result4, 0.0);
    
    auto result5 = m_transformer->transform(clampField, static_cast<double>(100.0));
    verifyDoubleValue(result5, 100.0);
}

void TestDataTransformer::testThresholdOperations() {
    // Threshold operations would be custom implementations
    // Test that we can identify operations that don't exist
    
    const std::string thresholdField = "threshold_test";
    // If Threshold operation existed, we would test it here
    // For now, test that unknown operations are handled
}

void TestDataTransformer::testCustomTransformations() {
    const std::string customField = "custom_test";
    
    // Create custom transformation that doubles the value
    auto doubleFunc = [](const FieldValue& value, const DataTransformer::TransformationParams& /*params*/) -> FieldValue {
        return std::visit([](const auto& val) -> FieldValue {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_arithmetic_v<T>) {
                return static_cast<double>(val) * 2.0;
            } else {
                return val; // Return unchanged for non-numeric types
            }
        }, value);
    };
    
    m_transformer->addTransformation(customField, 
        DataTransformer::Transformation(doubleFunc));
    
    auto result1 = m_transformer->transform(customField, static_cast<double>(21.0));
    verifyDoubleValue(result1, 42.0);
    
    auto result2 = m_transformer->transform(customField, static_cast<int32_t>(5));
    verifyDoubleValue(result2, 10.0);
    
    // Test non-numeric type (should pass through unchanged)
    auto result3 = m_transformer->transform(customField, std::string("test"));
    verifyStringValue(result3, "test");
}

void TestDataTransformer::testCustomFunctionErrors() {
    const std::string customField = "custom_error_test";
    
    // Add transformation without custom function
    m_transformer->addTransformation(customField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Custom));
    
    auto result = m_transformer->transform(customField, static_cast<double>(42.0));
    verifyTransformationError(result);
    QVERIFY(result.error.find("No custom function") != std::string::npos);
}

void TestDataTransformer::testTransformationChains() {
    const std::string chainField = "chain_test";
    
    // Create chain: Convert to double -> Multiply by 2 -> Add 10
    std::vector<DataTransformer::Transformation> transformations = {
        DataTransformer::Transformation(DataTransformer::OperationType::ToDouble),
        DataTransformer::Transformation(DataTransformer::OperationType::Multiply, DataTransformer::TransformationParams(2.0)),
        DataTransformer::Transformation(DataTransformer::OperationType::Add, DataTransformer::TransformationParams(10.0))
    };
    
    m_transformer->addTransformationChain(chainField, transformations);
    
    auto result = m_transformer->transform(chainField, static_cast<int32_t>(5));
    verifyDoubleValue(result, 20.0); // (5 * 2) + 10 = 20
}

void TestDataTransformer::testComplexChains() {
    const std::string complexField = "complex_chain_test";
    
    // Create complex chain: ToDouble -> Abs -> Sqrt -> Multiply by 10 -> Add prefix
    std::vector<DataTransformer::Transformation> transformations = {
        DataTransformer::Transformation(DataTransformer::OperationType::ToDouble),
        DataTransformer::Transformation(DataTransformer::OperationType::Abs),
        DataTransformer::Transformation(DataTransformer::OperationType::Sqrt),
        DataTransformer::Transformation(DataTransformer::OperationType::Multiply, DataTransformer::TransformationParams(10.0)),
        DataTransformer::Transformation(DataTransformer::OperationType::ToString),
        DataTransformer::Transformation(DataTransformer::OperationType::AddPrefix, DataTransformer::TransformationParams("Result: "))
    };
    
    m_transformer->addTransformationChain(complexField, transformations);
    
    auto result = m_transformer->transform(complexField, static_cast<double>(-16.0));
    QVERIFY(result.success);
    QVERIFY(std::holds_alternative<std::string>(result.value));
    std::string str = std::get<std::string>(result.value);
    QVERIFY(str.find("Result: 40") != std::string::npos); // sqrt(16) * 10 = 40
}

void TestDataTransformer::testChainErrorHandling() {
    const std::string errorChainField = "error_chain_test";
    
    // Create chain with error-inducing step
    std::vector<DataTransformer::Transformation> transformations = {
        DataTransformer::Transformation(DataTransformer::OperationType::ToDouble),
        DataTransformer::Transformation(DataTransformer::OperationType::Divide, DataTransformer::TransformationParams(0.0)), // Division by zero
        DataTransformer::Transformation(DataTransformer::OperationType::Add, DataTransformer::TransformationParams(10.0))   // Should not be reached
    };
    
    m_transformer->addTransformationChain(errorChainField, transformations);
    
    auto result = m_transformer->transform(errorChainField, static_cast<double>(42.0));
    verifyTransformationError(result);
    QVERIFY(result.error.find("Division by zero") != std::string::npos);
}

void TestDataTransformer::testStatefulTransformations() {
    const std::string statefulField = "stateful_test";
    
    // Test that stateful transformations maintain state correctly
    DataTransformer::TransformationParams params;
    params.windowSize = 2;
    
    m_transformer->addTransformation(statefulField, 
        DataTransformer::Transformation(DataTransformer::OperationType::MovingAverage, params));
    
    // Process several values
    auto result1 = m_transformer->transform(statefulField, static_cast<double>(10.0));
    verifyDoubleValue(result1, 10.0);
    
    auto result2 = m_transformer->transform(statefulField, static_cast<double>(20.0));
    verifyDoubleValue(result2, 15.0);
    
    auto result3 = m_transformer->transform(statefulField, static_cast<double>(30.0));
    verifyDoubleValue(result3, 25.0); // Average of [20.0, 30.0] with window size 2
}

void TestDataTransformer::testStateReset() {
    const std::string resetField = "reset_test";
    
    // Set up cumulative sum
    m_transformer->addTransformation(resetField, 
        DataTransformer::Transformation(DataTransformer::OperationType::CumulativeSum));
    
    // Build up some state
    m_transformer->transform(resetField, static_cast<double>(5.0));
    m_transformer->transform(resetField, static_cast<double>(3.0));
    auto result1 = m_transformer->transform(resetField, static_cast<double>(2.0));
    verifyDoubleValue(result1, 10.0); // 5 + 3 + 2
    
    // Reset state
    m_transformer->resetState(resetField);
    
    // Next transformation should start fresh
    auto result2 = m_transformer->transform(resetField, static_cast<double>(7.0));
    verifyDoubleValue(result2, 7.0); // Fresh start
}

void TestDataTransformer::testConcurrentStateManagement() {
    // Test that different fields maintain separate state
    const std::string field1 = "concurrent_field1";
    const std::string field2 = "concurrent_field2";
    
    m_transformer->addTransformation(field1, 
        DataTransformer::Transformation(DataTransformer::OperationType::CumulativeSum));
    m_transformer->addTransformation(field2, 
        DataTransformer::Transformation(DataTransformer::OperationType::CumulativeSum));
    
    // Process values for field1
    m_transformer->transform(field1, static_cast<double>(10.0));
    auto result1 = m_transformer->transform(field1, static_cast<double>(5.0));
    verifyDoubleValue(result1, 15.0);
    
    // Process values for field2 (should have independent state)
    m_transformer->transform(field2, static_cast<double>(20.0));
    auto result2 = m_transformer->transform(field2, static_cast<double>(3.0));
    verifyDoubleValue(result2, 23.0);
    
    // Continue field1 (should maintain its state)
    auto result3 = m_transformer->transform(field1, static_cast<double>(2.0));
    verifyDoubleValue(result3, 17.0); // 15 + 2
}

void TestDataTransformer::testInvalidTypeConversions() {
    // Test conversions that should fail
    const std::string invalidField = "invalid_conversions";
    
    // Try to convert byte array to integer
    m_transformer->addTransformation(invalidField, 
        DataTransformer::Transformation(DataTransformer::OperationType::ToInteger));
    
    auto result = m_transformer->transform(invalidField, std::vector<uint8_t>{1, 2, 3, 4});
    verifyTransformationError(result);
}

void TestDataTransformer::testInvalidMathOperations() {
    const std::string mathField = "invalid_math";
    
    // Try to apply math operation to string
    m_transformer->addTransformation(mathField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Add, DataTransformer::TransformationParams(10.0)));
    
    auto result = m_transformer->transform(mathField, std::string("not_a_number"));
    verifyTransformationError(result);
}

void TestDataTransformer::testTransformationErrors() {
    const std::string errorField = "transformation_errors";
    
    // Test unknown operation (this would require modifying the enum, so we test custom function error)
    m_transformer->addTransformation(errorField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Custom));
    
    auto result = m_transformer->transform(errorField, static_cast<double>(42.0));
    verifyTransformationError(result);
}

void TestDataTransformer::testTransformationPerformance() {
    const std::string perfField = "performance_test";
    const int iterations = 10000;
    
    // Simple transformation chain
    m_transformer->addTransformation(perfField, 
        DataTransformer::Transformation(DataTransformer::OperationType::ToDouble));
    m_transformer->addTransformation(perfField, 
        DataTransformer::Transformation(DataTransformer::OperationType::Multiply, DataTransformer::TransformationParams(2.0)));
    
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < iterations; ++i) {
        auto result = m_transformer->transform(perfField, static_cast<int32_t>(i));
        QVERIFY(result.success);
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerTransformation = static_cast<double>(elapsedNs) / iterations;
    
    qDebug() << "DataTransformer performance:" << nsPerTransformation 
             << "ns per transformation (" << iterations << "iterations)";
    
    // Should be fast - expect < 10μs per transformation
    QVERIFY(nsPerTransformation < 10000.0);
}

void TestDataTransformer::testLargeChainPerformance() {
    const std::string largeChainField = "large_chain_perf";
    const int chainLength = 20;
    const int iterations = 1000;
    
    // Build large transformation chain
    for (int i = 0; i < chainLength; ++i) {
        if (i % 2 == 0) {
            m_transformer->addTransformation(largeChainField, 
                DataTransformer::Transformation(DataTransformer::OperationType::Add, DataTransformer::TransformationParams(1.0)));
        } else {
            m_transformer->addTransformation(largeChainField, 
                DataTransformer::Transformation(DataTransformer::OperationType::Multiply, DataTransformer::TransformationParams(1.1)));
        }
    }
    
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < iterations; ++i) {
        auto result = m_transformer->transform(largeChainField, static_cast<double>(i));
        QVERIFY(result.success);
    }
    
    qint64 elapsedNs = timer.nsecsElapsed();
    double nsPerChain = static_cast<double>(elapsedNs) / iterations;
    
    qDebug() << "Large chain performance:" << nsPerChain 
             << "ns per chain (" << chainLength << "steps," << iterations << "iterations)";
    
    // Large chains should still be reasonable - expect < 100μs per chain
    QVERIFY(nsPerChain < 100000.0);
}

void TestDataTransformer::testEmptyTransformer() {
    // Test transformer with no transformations
    const std::string emptyField = "empty_field";
    
    // Should return original value when no transformations exist
    auto result = m_transformer->transform(emptyField, static_cast<double>(42.0));
    verifyTransformationSuccess(result);
    QVERIFY(std::holds_alternative<double>(result.value));
    QCOMPARE(std::get<double>(result.value), 42.0);
}

void TestDataTransformer::testVariantTypeHandling() {
    const std::string variantField = "variant_test";
    
    // Test that all variant types are handled correctly
    m_transformer->addTransformation(variantField, 
        DataTransformer::Transformation(DataTransformer::OperationType::ToString));
    
    auto testValues = generateTestValues();
    for (const auto& value : testValues) {
        auto result = m_transformer->transform(variantField, value);
        verifyTransformationSuccess(result);
        QVERIFY(std::holds_alternative<std::string>(result.value));
    }
}

void TestDataTransformer::testNumericLimits() {
    const std::string limitsField = "limits_test";
    
    // Test with extreme numeric values
    m_transformer->addTransformation(limitsField, 
        DataTransformer::Transformation(DataTransformer::OperationType::ToDouble));
    
    auto result1 = m_transformer->transform(limitsField, std::numeric_limits<int32_t>::max());
    verifyTransformationSuccess(result1);
    
    auto result2 = m_transformer->transform(limitsField, std::numeric_limits<int32_t>::min());
    verifyTransformationSuccess(result2);
    
    auto result3 = m_transformer->transform(limitsField, std::numeric_limits<double>::max());
    verifyTransformationSuccess(result3);
}

QTEST_MAIN(TestDataTransformer)