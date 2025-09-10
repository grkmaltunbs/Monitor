#include <QtTest>
#include <QObject>
#include <QDebug>

// Test Framework includes
#include "../../src/test_framework/execution/expression_evaluator.h"

using namespace Monitor::TestFramework;

/**
 * @brief Debug test for integration issues
 */
class TestIntegrationDebug : public QObject
{
    Q_OBJECT

private slots:
    void testExpressionDebug() {
        qDebug() << "=== Expression Debug Test ===";
        
        EvaluationContext context;
        context.setVariable("x", QVariant(10));
        context.setVariable("y", QVariant(5));
        
        // Test simple arithmetic
        QVariant result1 = ExpressionEvaluator::evaluateString("x + y", context);
        qDebug() << "Expression: 'x + y'";
        qDebug() << "Result valid:" << result1.isValid();
        qDebug() << "Result type:" << result1.typeName();
        qDebug() << "Result value:" << result1;
        qDebug() << "Result as double:" << result1.toDouble();
        qDebug() << "Result as string:" << result1.toString();
        
        // Test simple values
        QVariant result2 = ExpressionEvaluator::evaluateString("5 + 3", context);
        qDebug() << "Expression: '5 + 3'";
        qDebug() << "Result valid:" << result2.isValid();
        qDebug() << "Result value:" << result2;
        qDebug() << "Result as double:" << result2.toDouble();
        
        // Test comparison
        QVariant result3 = ExpressionEvaluator::evaluateString("10 > 5", context);
        qDebug() << "Expression: '10 > 5'";
        qDebug() << "Result valid:" << result3.isValid();
        qDebug() << "Result value:" << result3;
        qDebug() << "Result as bool:" << result3.toBool();
        
        qDebug() << "=== Debug Complete ===";
    }
};

QTEST_MAIN(TestIntegrationDebug)
#include "test_integration_debug.moc"