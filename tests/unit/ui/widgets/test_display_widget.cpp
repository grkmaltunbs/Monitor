#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QJsonObject>
#include <QVariant>
#include <QColor>
#include <QFont>
#include <memory>

// Include the widget under test
#include "../../../../src/ui/widgets/display_widget.h"

/**
 * @brief Mock implementation of DisplayWidget for testing
 */
class MockDisplayWidget : public DisplayWidget
{
    Q_OBJECT

public:
    explicit MockDisplayWidget(const QString& widgetId, QWidget* parent = nullptr)
        : DisplayWidget(widgetId, "Mock Display Widget", parent)
        , m_updateFieldDisplayCalled(false)
        , m_clearFieldDisplayCalled(false)
        , m_refreshAllDisplaysCalled(false)
    {
    }

    // Test accessors
    bool isUpdateFieldDisplayCalled() const { return m_updateFieldDisplayCalled; }
    bool isClearFieldDisplayCalled() const { return m_clearFieldDisplayCalled; }
    bool isRefreshAllDisplaysCalled() const { return m_refreshAllDisplaysCalled; }
    
    QString getLastUpdatedFieldPath() const { return m_lastUpdatedFieldPath; }
    QVariant getLastUpdatedValue() const { return m_lastUpdatedValue; }
    
    void resetFlags() {
        m_updateFieldDisplayCalled = false;
        m_clearFieldDisplayCalled = false;
        m_refreshAllDisplaysCalled = false;
        m_lastUpdatedFieldPath.clear();
        m_lastUpdatedValue = QVariant();
    }

protected:
    // DisplayWidget template method implementations
    void updateFieldDisplay(const QString& fieldPath, const QVariant& value) override {
        m_updateFieldDisplayCalled = true;
        m_lastUpdatedFieldPath = fieldPath;
        m_lastUpdatedValue = value;
    }

    void clearFieldDisplay(const QString& fieldPath) override {
        Q_UNUSED(fieldPath)
        m_clearFieldDisplayCalled = true;
    }

    void refreshAllDisplays() override {
        m_refreshAllDisplaysCalled = true;
    }

private:
    bool m_updateFieldDisplayCalled;
    bool m_clearFieldDisplayCalled;
    bool m_refreshAllDisplaysCalled;
    QString m_lastUpdatedFieldPath;
    QVariant m_lastUpdatedValue;
};

/**
 * @brief Unit tests for DisplayWidget class
 */
class TestDisplayWidget : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Core functionality tests
    void testConstructor();
    void testDisplayConfig();
    void testTriggerConditions();
    void testValueAccess();
    void testValueFormatting();
    void testTransformations();
    void testMathematicalOperations();
    void testFunctionalTransformations();
    void testSettingsPersistence();

    // Static helper method tests
    void testFormatValue();
    void testConvertValue();
    void testApplyMathOperation();
    void testApplyFunction();

    // Performance tests
    void testTransformationPerformance();

    // Edge case tests
    void testInvalidTransformations();
    void testEmptyValues();
    void testLargeNumbers();

private:
    QApplication* m_app;
    MockDisplayWidget* m_widget;
    QString m_testWidgetId;
};

void TestDisplayWidget::initTestCase()
{
    if (!QApplication::instance()) {
        int argc = 1;
        static const char* test_name = "test";
        const char* argv[] = {test_name};
        m_app = new QApplication(argc, const_cast<char**>(argv));
    } else {
        m_app = nullptr;
    }
    
    m_testWidgetId = "test_display_widget_001";
}

void TestDisplayWidget::cleanupTestCase()
{
    if (m_app) {
        delete m_app;
        m_app = nullptr;
    }
}

void TestDisplayWidget::init()
{
    m_widget = new MockDisplayWidget(m_testWidgetId);
    QVERIFY(m_widget != nullptr);
}

void TestDisplayWidget::cleanup()
{
    if (m_widget) {
        delete m_widget;
        m_widget = nullptr;
    }
}

void TestDisplayWidget::testConstructor()
{
    QVERIFY(m_widget != nullptr);
    QCOMPARE(m_widget->getWidgetId(), m_testWidgetId);
    QCOMPARE(m_widget->getWindowTitle(), QString("Mock Display Widget"));
    
    // Test trigger condition defaults
    auto trigger = m_widget->getTriggerCondition();
    QVERIFY(!trigger.enabled);
    QVERIFY(trigger.expression.isEmpty());
}

void TestDisplayWidget::testDisplayConfig()
{
    QString fieldPath = "test.config.field";
    
    // Test default config
    DisplayWidget::DisplayConfig defaultConfig = m_widget->getDisplayConfig(fieldPath);
    QCOMPARE(defaultConfig.conversion, DisplayWidget::ConversionType::NoConversion);
    QCOMPARE(defaultConfig.mathOp, DisplayWidget::MathOperation::None);
    QCOMPARE(defaultConfig.function, DisplayWidget::FunctionType::None);
    QCOMPARE(defaultConfig.decimalPlaces, 2);
    QVERIFY(defaultConfig.isVisible);
    
    // Set custom config
    DisplayWidget::DisplayConfig customConfig;
    customConfig.conversion = DisplayWidget::ConversionType::ToHexadecimal;
    customConfig.mathOp = DisplayWidget::MathOperation::Multiply;
    customConfig.mathOperand = 2.5;
    customConfig.function = DisplayWidget::FunctionType::MovingAverage;
    customConfig.functionWindow = 20;
    customConfig.prefix = "Value: ";
    customConfig.suffix = " units";
    customConfig.decimalPlaces = 3;
    customConfig.textColor = QColor(Qt::red);
    customConfig.backgroundColor = QColor(Qt::lightGray);
    
    m_widget->setDisplayConfig(fieldPath, customConfig);
    
    DisplayWidget::DisplayConfig retrievedConfig = m_widget->getDisplayConfig(fieldPath);
    QCOMPARE(retrievedConfig.conversion, DisplayWidget::ConversionType::ToHexadecimal);
    QCOMPARE(retrievedConfig.mathOp, DisplayWidget::MathOperation::Multiply);
    QCOMPARE(retrievedConfig.mathOperand, 2.5);
    QCOMPARE(retrievedConfig.function, DisplayWidget::FunctionType::MovingAverage);
    QCOMPARE(retrievedConfig.functionWindow, 20);
    QCOMPARE(retrievedConfig.prefix, QString("Value: "));
    QCOMPARE(retrievedConfig.suffix, QString(" units"));
    QCOMPARE(retrievedConfig.decimalPlaces, 3);
    QCOMPARE(retrievedConfig.textColor, QColor(Qt::red));
    QCOMPARE(retrievedConfig.backgroundColor, QColor(Qt::lightGray));
    
    // Test reset config
    m_widget->resetDisplayConfig(fieldPath);
    DisplayWidget::DisplayConfig resetConfig = m_widget->getDisplayConfig(fieldPath);
    QCOMPARE(resetConfig.conversion, DisplayWidget::ConversionType::NoConversion);
    QCOMPARE(resetConfig.mathOp, DisplayWidget::MathOperation::None);
}

void TestDisplayWidget::testTriggerConditions()
{
    // Test setting trigger condition
    DisplayWidget::TriggerCondition condition;
    condition.enabled = true;
    condition.expression = "test.field > 100";
    
    m_widget->setTriggerCondition(condition);
    
    DisplayWidget::TriggerCondition retrieved = m_widget->getTriggerCondition();
    QVERIFY(retrieved.enabled);
    QCOMPARE(retrieved.expression, QString("test.field > 100"));
    
    // Test clearing trigger condition
    m_widget->clearTriggerCondition();
    
    retrieved = m_widget->getTriggerCondition();
    QVERIFY(!retrieved.enabled);
    QVERIFY(retrieved.expression.isEmpty());
}

void TestDisplayWidget::testValueAccess()
{
    QString fieldPath = "test.value.field";
    QVariant testValue = 42.5;
    
    // Initially no value
    QVERIFY(!m_widget->getFieldValue(fieldPath).isValid());
    QVERIFY(!m_widget->getTransformedValue(fieldPath).isValid());
    QVERIFY(!m_widget->hasNewValue(fieldPath));
    
    // Note: Testing actual value updates would require integration with 
    // packet processing system, which is mocked in unit tests
}

void TestDisplayWidget::testValueFormatting()
{
    DisplayWidget::DisplayConfig config;
    
    // Test basic formatting
    config.decimalPlaces = 2;
    QString formatted = m_widget->formatValue(QVariant(3.14159), config);
    QCOMPARE(formatted, QString("3.14"));
    
    // Test with prefix and suffix
    config.prefix = "$";
    config.suffix = " USD";
    formatted = m_widget->formatValue(QVariant(123.45), config);
    QCOMPARE(formatted, QString("$123.45 USD"));
    
    // Test thousands separator
    config.prefix.clear();
    config.suffix.clear();
    config.useThousandsSeparator = true;
    formatted = m_widget->formatValue(QVariant(1234567), config);
    // Result depends on locale, but should contain separators
    QVERIFY(formatted.length() > 7); // More than just digits
    
    // Test scientific notation
    config.useThousandsSeparator = false;
    config.useScientificNotation = true;
    config.decimalPlaces = 3;
    formatted = m_widget->formatValue(QVariant(0.000123), config);
    QVERIFY(formatted.contains('e') || formatted.contains('E'));
}

void TestDisplayWidget::testTransformations()
{
    // Test type conversion
    QVariant intValue(42);
    QVariant converted = DisplayWidget::convertValue(intValue, DisplayWidget::ConversionType::ToDouble);
    QCOMPARE(converted.typeId(), QMetaType::Double);
    QCOMPARE(converted.toDouble(), 42.0);
    
    converted = DisplayWidget::convertValue(intValue, DisplayWidget::ConversionType::ToString);
    QCOMPARE(converted.typeId(), QMetaType::QString);
    QCOMPARE(converted.toString(), QString("42"));
    
    converted = DisplayWidget::convertValue(intValue, DisplayWidget::ConversionType::ToBoolean);
    QCOMPARE(converted.typeId(), QMetaType::Bool);
    QVERIFY(converted.toBool()); // 42 is truthy
    
    QVariant zeroValue(0);
    converted = DisplayWidget::convertValue(zeroValue, DisplayWidget::ConversionType::ToBoolean);
    QVERIFY(!converted.toBool()); // 0 is falsy
}

void TestDisplayWidget::testMathematicalOperations()
{
    QVariant input(10.0);
    
    // Test multiplication
    QVariant result = DisplayWidget::applyMathOperation(input, DisplayWidget::MathOperation::Multiply, 3.0);
    QCOMPARE(result.toDouble(), 30.0);
    
    // Test division
    result = DisplayWidget::applyMathOperation(input, DisplayWidget::MathOperation::Divide, 2.0);
    QCOMPARE(result.toDouble(), 5.0);
    
    // Test division by zero
    result = DisplayWidget::applyMathOperation(input, DisplayWidget::MathOperation::Divide, 0.0);
    QVERIFY(!result.isValid()); // Should return invalid QVariant
    
    // Test addition
    result = DisplayWidget::applyMathOperation(input, DisplayWidget::MathOperation::Add, 5.0);
    QCOMPARE(result.toDouble(), 15.0);
    
    // Test subtraction
    result = DisplayWidget::applyMathOperation(input, DisplayWidget::MathOperation::Subtract, 3.0);
    QCOMPARE(result.toDouble(), 7.0);
    
    // Test modulo
    result = DisplayWidget::applyMathOperation(input, DisplayWidget::MathOperation::Modulo, 3.0);
    QCOMPARE(result.toDouble(), 1.0);
    
    // Test power
    result = DisplayWidget::applyMathOperation(QVariant(2.0), DisplayWidget::MathOperation::Power, 3.0);
    QCOMPARE(result.toDouble(), 8.0);
    
    // Test absolute value
    result = DisplayWidget::applyMathOperation(QVariant(-5.0), DisplayWidget::MathOperation::Absolute, 0.0);
    QCOMPARE(result.toDouble(), 5.0);
    
    // Test negation
    result = DisplayWidget::applyMathOperation(QVariant(5.0), DisplayWidget::MathOperation::Negate, 0.0);
    QCOMPARE(result.toDouble(), -5.0);
}

void TestDisplayWidget::testFunctionalTransformations()
{
    std::vector<QVariant> history;
    history.push_back(QVariant(1.0));
    history.push_back(QVariant(2.0));
    history.push_back(QVariant(3.0));
    history.push_back(QVariant(4.0));
    history.push_back(QVariant(5.0));
    
    // Test moving average
    QVariant result = DisplayWidget::applyFunction(history, DisplayWidget::FunctionType::MovingAverage);
    QCOMPARE(result.toDouble(), 3.0); // (1+2+3+4+5)/5 = 3
    
    // Test cumulative sum
    result = DisplayWidget::applyFunction(history, DisplayWidget::FunctionType::CumulativeSum);
    QCOMPARE(result.toDouble(), 15.0); // 1+2+3+4+5 = 15
    
    // Test minimum
    result = DisplayWidget::applyFunction(history, DisplayWidget::FunctionType::Minimum);
    QCOMPARE(result.toDouble(), 1.0);
    
    // Test maximum
    result = DisplayWidget::applyFunction(history, DisplayWidget::FunctionType::Maximum);
    QCOMPARE(result.toDouble(), 5.0);
    
    // Test range
    result = DisplayWidget::applyFunction(history, DisplayWidget::FunctionType::Range);
    QCOMPARE(result.toDouble(), 4.0); // 5 - 1 = 4
    
    // Test difference (requires at least 2 values)
    result = DisplayWidget::applyFunction(history, DisplayWidget::FunctionType::Difference);
    QCOMPARE(result.toDouble(), 1.0); // 5 - 4 = 1
    
    // Test standard deviation
    result = DisplayWidget::applyFunction(history, DisplayWidget::FunctionType::StandardDeviation);
    QVERIFY(result.toDouble() > 0); // Should be approximately sqrt(2.5) ≈ 1.58
    QVERIFY(qAbs(result.toDouble() - 1.58) < 0.1);
}

void TestDisplayWidget::testSettingsPersistence()
{
    QString fieldPath = "test.settings.field";
    
    // Configure display settings
    DisplayWidget::DisplayConfig config;
    config.conversion = DisplayWidget::ConversionType::ToHexadecimal;
    config.mathOp = DisplayWidget::MathOperation::Multiply;
    config.mathOperand = 1.5;
    config.prefix = "Test: ";
    config.suffix = " end";
    config.textColor = QColor(Qt::blue);
    
    m_widget->setDisplayConfig(fieldPath, config);
    
    // Configure trigger
    DisplayWidget::TriggerCondition trigger;
    trigger.enabled = true;
    trigger.expression = "field > 50";
    m_widget->setTriggerCondition(trigger);
    
    // Save settings
    QJsonObject settings = m_widget->saveSettings();
    
    QVERIFY(!settings.isEmpty());
    
    QJsonObject widgetSpecific = settings.value("widgetSpecific").toObject();
    QVERIFY(!widgetSpecific.isEmpty());
    
    QJsonObject displayConfigs = widgetSpecific.value("displayConfigs").toObject();
    QVERIFY(displayConfigs.contains(fieldPath));
    
    QJsonObject triggerObj = widgetSpecific.value("trigger").toObject();
    QVERIFY(triggerObj.value("enabled").toBool());
    QCOMPARE(triggerObj.value("expression").toString(), QString("field > 50"));
    
    // Restore in new widget
    MockDisplayWidget newWidget("restored_widget");
    bool restored = newWidget.restoreSettings(settings);
    
    QVERIFY(restored);
    
    DisplayWidget::DisplayConfig restoredConfig = newWidget.getDisplayConfig(fieldPath);
    QCOMPARE(restoredConfig.conversion, DisplayWidget::ConversionType::ToHexadecimal);
    QCOMPARE(restoredConfig.mathOp, DisplayWidget::MathOperation::Multiply);
    QCOMPARE(restoredConfig.mathOperand, 1.5);
    QCOMPARE(restoredConfig.prefix, QString("Test: "));
    QCOMPARE(restoredConfig.suffix, QString(" end"));
    QCOMPARE(restoredConfig.textColor, QColor(Qt::blue));
    
    DisplayWidget::TriggerCondition restoredTrigger = newWidget.getTriggerCondition();
    QVERIFY(restoredTrigger.enabled);
    QCOMPARE(restoredTrigger.expression, QString("field > 50"));
}

void TestDisplayWidget::testFormatValue()
{
    DisplayWidget::DisplayConfig config;
    
    // Test integer formatting
    QString result = DisplayWidget::formatValue(QVariant(42), config);
    QCOMPARE(result, QString("42"));
    
    // Test double formatting
    result = DisplayWidget::formatValue(QVariant(3.14159), config);
    QCOMPARE(result, QString("3.14")); // Default 2 decimal places
    
    // Test boolean formatting
    result = DisplayWidget::formatValue(QVariant(true), config);
    QCOMPARE(result, QString("true"));
    
    result = DisplayWidget::formatValue(QVariant(false), config);
    QCOMPARE(result, QString("false"));
    
    // Test string formatting
    result = DisplayWidget::formatValue(QVariant("test string"), config);
    QCOMPARE(result, QString("test string"));
    
    // Test invalid value
    result = DisplayWidget::formatValue(QVariant(), config);
    QCOMPARE(result, QString("--"));
    
    // Test hexadecimal conversion
    config.conversion = DisplayWidget::ConversionType::ToHexadecimal;
    result = DisplayWidget::formatValue(QVariant(255), config);
    QCOMPARE(result, QString("0XFF"));
    
    // Test binary conversion
    config.conversion = DisplayWidget::ConversionType::ToBinary;
    result = DisplayWidget::formatValue(QVariant(7), config);
    QCOMPARE(result, QString("0b111"));
}

void TestDisplayWidget::testConvertValue()
{
    // Test no conversion
    QVariant input(42);
    QVariant result = DisplayWidget::convertValue(input, DisplayWidget::ConversionType::NoConversion);
    QCOMPARE(result, input);
    
    // Test to integer
    result = DisplayWidget::convertValue(QVariant(3.14), DisplayWidget::ConversionType::ToInteger);
    QCOMPARE(result.typeId(), QMetaType::LongLong);
    QCOMPARE(result.toLongLong(), 3LL);
    
    // Test to double
    result = DisplayWidget::convertValue(QVariant(42), DisplayWidget::ConversionType::ToDouble);
    QCOMPARE(result.typeId(), QMetaType::Double);
    QCOMPARE(result.toDouble(), 42.0);
    
    // Test to string
    result = DisplayWidget::convertValue(QVariant(123), DisplayWidget::ConversionType::ToString);
    QCOMPARE(result.typeId(), QMetaType::QString);
    QCOMPARE(result.toString(), QString("123"));
    
    // Test to boolean
    result = DisplayWidget::convertValue(QVariant(1), DisplayWidget::ConversionType::ToBoolean);
    QCOMPARE(result.typeId(), QMetaType::Bool);
    QVERIFY(result.toBool());
    
    result = DisplayWidget::convertValue(QVariant(0), DisplayWidget::ConversionType::ToBoolean);
    QVERIFY(!result.toBool());
}

void TestDisplayWidget::testApplyMathOperation()
{
    // Test no operation
    QVariant input(10.0);
    QVariant result = DisplayWidget::applyMathOperation(input, DisplayWidget::MathOperation::None, 0.0);
    QCOMPARE(result, input);
    
    // Test all operations (covered in testMathematicalOperations)
    // Additional edge cases here
    
    // Test with non-numeric input
    result = DisplayWidget::applyMathOperation(QVariant("abc"), DisplayWidget::MathOperation::Multiply, 2.0);
    QCOMPARE(result.toDouble(), 0.0); // String converts to 0.0
}

void TestDisplayWidget::testApplyFunction()
{
    // Test empty history
    std::vector<QVariant> emptyHistory;
    QVariant result = DisplayWidget::applyFunction(emptyHistory, DisplayWidget::FunctionType::MovingAverage);
    QVERIFY(!result.isValid());
    
    // Test single value
    std::vector<QVariant> singleValue;
    singleValue.push_back(QVariant(5.0));
    
    result = DisplayWidget::applyFunction(singleValue, DisplayWidget::FunctionType::MovingAverage);
    QCOMPARE(result.toDouble(), 5.0);
    
    result = DisplayWidget::applyFunction(singleValue, DisplayWidget::FunctionType::Minimum);
    QCOMPARE(result.toDouble(), 5.0);
    
    result = DisplayWidget::applyFunction(singleValue, DisplayWidget::FunctionType::Maximum);
    QCOMPARE(result.toDouble(), 5.0);
    
    // Test difference with insufficient data
    result = DisplayWidget::applyFunction(singleValue, DisplayWidget::FunctionType::Difference);
    QVERIFY(!result.isValid()); // Needs at least 2 values
    
    // Test standard deviation with insufficient data
    result = DisplayWidget::applyFunction(singleValue, DisplayWidget::FunctionType::StandardDeviation);
    QVERIFY(!result.isValid()); // Needs at least 2 values
}

void TestDisplayWidget::testTransformationPerformance()
{
    const int iterations = 1000;
    
    QElapsedTimer timer;
    timer.start();
    
    // Test rapid conversions
    for (int i = 0; i < iterations; ++i) {
        QVariant input(i * 3.14159);
        
        DisplayWidget::convertValue(input, DisplayWidget::ConversionType::ToInteger);
        DisplayWidget::convertValue(input, DisplayWidget::ConversionType::ToString);
        DisplayWidget::applyMathOperation(input, DisplayWidget::MathOperation::Multiply, 2.0);
        
        DisplayWidget::DisplayConfig config;
        config.decimalPlaces = 3;
        config.prefix = "Value: ";
        DisplayWidget::formatValue(input, config);
    }
    
    qint64 elapsed = timer.elapsed();
    
    // Should complete in reasonable time
    QVERIFY(elapsed < 1000); // Less than 1 second for 1000 operations
    
    qDebug() << "Performed" << iterations << "transformation operations in" << elapsed << "ms";
}

void TestDisplayWidget::testInvalidTransformations()
{
    // Test invalid conversion types (handled by default cases)
    QVariant input(42);
    QVariant result = DisplayWidget::convertValue(input, static_cast<DisplayWidget::ConversionType>(999));
    QCOMPARE(result, input); // Should return input unchanged
    
    // Test invalid math operations
    result = DisplayWidget::applyMathOperation(input, static_cast<DisplayWidget::MathOperation>(999), 1.0);
    QCOMPARE(result, input); // Should return input unchanged
    
    // Test invalid function types
    std::vector<QVariant> history;
    history.push_back(QVariant(1.0));
    history.push_back(QVariant(2.0));
    
    result = DisplayWidget::applyFunction(history, static_cast<DisplayWidget::FunctionType>(999));
    QCOMPARE(result, history.back()); // Should return last value
}

void TestDisplayWidget::testEmptyValues()
{
    DisplayWidget::DisplayConfig config;
    
    // Test null variant formatting
    QString result = DisplayWidget::formatValue(QVariant(), config);
    QCOMPARE(result, QString("--"));
    
    // Test empty string formatting
    result = DisplayWidget::formatValue(QVariant(""), config);
    QCOMPARE(result, QString(""));
    
    // Test with prefix/suffix on empty value
    config.prefix = "Pre: ";
    config.suffix = " :Post";
    result = DisplayWidget::formatValue(QVariant(), config);
    QCOMPARE(result, QString("--")); // Prefix/suffix not applied to invalid values
    
    result = DisplayWidget::formatValue(QVariant(""), config);
    QCOMPARE(result, QString("Pre:  :Post"));
}

void TestDisplayWidget::testLargeNumbers()
{
    DisplayWidget::DisplayConfig config;
    
    // Test very large integer
    QVariant largeInt(9223372036854775807LL); // Max long long
    QString result = DisplayWidget::formatValue(largeInt, config);
    QVERIFY(!result.isEmpty());
    
    // Test very large double
    QVariant largeDouble(1.7976931348623157e+308); // Near max double
    result = DisplayWidget::formatValue(largeDouble, config);
    QVERIFY(!result.isEmpty());
    
    // Test scientific notation with large numbers
    config.useScientificNotation = true;
    config.decimalPlaces = 2;
    result = DisplayWidget::formatValue(QVariant(1234567890.0), config);
    QVERIFY(result.contains('e') || result.contains('E'));
    
    // Test mathematical operations on large numbers
    QVariant mathResult = DisplayWidget::applyMathOperation(largeDouble, DisplayWidget::MathOperation::Multiply, 0.5);
    QVERIFY(mathResult.isValid());
    QVERIFY(mathResult.toDouble() > 0);
}

// Include the moc file for the test class
#include "test_display_widget.moc"

QTEST_MAIN(TestDisplayWidget)