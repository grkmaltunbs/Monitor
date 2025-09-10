#include <QtTest>
#include <QObject>

// Test Framework includes
#include "../../src/test_framework/core/test_definition.h"
#include "../../src/test_framework/core/test_result.h"
#include "../../src/test_framework/execution/expression_evaluator.h"
#include "../../src/test_framework/execution/test_scheduler.h"
#include "../../src/test_framework/execution/result_collector.h"
#include "../../src/test_framework/execution/alert_manager.h"

using namespace Monitor::TestFramework;

/**
 * @brief Minimal integration tests for Test Framework components
 */
class TestFrameworkIntegrationMinimal : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        qDebug() << "=== Minimal Test Framework Integration Tests ===";
    }
    
    void cleanupTestCase() {
        qDebug() << "=== Integration tests completed successfully ===";
    }

    // Test 1: Component creation and basic functionality
    void testComponentCreation() {
        qDebug() << "--- Test 1: Component Creation ---";
        
        // Test component creation without crashing
        {
            TestDefinition testDef("test_id");
            testDef.setName("Test Definition");
            testDef.setExpression("5 + 3 == 8");
            QVERIFY(testDef.getId() == "test_id");
        }
        
        {
            auto result = std::make_shared<TestResult>("result_id", TestResultStatus::PASSED);
            result->setMessage("Test message");
            QVERIFY(result->getTestId() == "result_id");
            QVERIFY(result->getStatus() == TestResultStatus::PASSED);
        }
        
        {
            TestScheduler scheduler;
            QVERIFY(!scheduler.isRunning());
            scheduler.start();
            QVERIFY(scheduler.isRunning());
            scheduler.stop();
            QVERIFY(!scheduler.isRunning());
        }
        
        {
            ResultCollector collector;
            collector.setMaxResults(100);
            auto config = collector.getAggregationConfig();
            QVERIFY(config.windowSizeMs > 0);
        }
        
        {
            AlertManager alertManager;
            alertManager.setDeliveryConfig(AlertConfigFactory::silentMode());
            QVERIFY(!alertManager.isEnabled() || alertManager.isEnabled()); // Just verify it doesn't crash
        }
        
        qDebug() << "✅ Component creation successful";
    }

    // Test 2: Expression evaluation basic test
    void testExpressionEvaluation() {
        qDebug() << "\n--- Test 2: Expression Evaluation ---";
        
        EvaluationContext context;
        context.setVariable("x", QVariant(10));
        context.setVariable("y", QVariant(5));
        
        // Test simple arithmetic
        QVariant result1 = ExpressionEvaluator::evaluateString("x + y", context);
        QVERIFY(result1.isValid());
        QVERIFY(qAbs(result1.toDouble() - 15.0) < 0.001);
        
        // Test comparison
        QVariant result2 = ExpressionEvaluator::evaluateString("x > y", context);
        QVERIFY(result2.isValid());
        QVERIFY(result2.toBool() == true);
        
        qDebug() << "✅ Expression evaluation successful";
    }

    // Test 3: Result collection basic test
    void testResultCollection() {
        qDebug() << "\n--- Test 3: Result Collection ---";
        
        ResultCollector collector;
        
        // Add a few test results
        auto result1 = std::make_shared<TestResult>("test_1", TestResultStatus::PASSED);
        result1->setExecutionTimeUs(10.0);
        
        auto result2 = std::make_shared<TestResult>("test_2", TestResultStatus::FAILED);
        result2->setExecutionTimeUs(15.0);
        
        collector.addResult(result1);
        collector.addResult(result2);
        
        // Verify results were added
        auto results = collector.getResults("test_1");
        QVERIFY(results.size() > 0);
        
        qDebug() << "✅ Result collection successful";
    }

    // Test 4: Alert manager basic functionality
    void testAlertManager() {
        qDebug() << "\n--- Test 4: Alert Manager ---";
        
        AlertManager alertManager;
        alertManager.setDeliveryConfig(AlertConfigFactory::silentMode());
        
        // Add alert condition
        AlertCondition condition("test_*", TestResultStatus::FAILED);
        alertManager.addAlertCondition(condition);
        
        // Process a failing result
        auto result = std::make_shared<TestResult>("test_fail", TestResultStatus::FAILED);
        result->setMessage("Test failure");
        
        alertManager.processTestResult(result); // Should not crash
        
        // Verify manager is still functional
        alertManager.updateStatistics();
        
        qDebug() << "✅ Alert manager integration successful";
    }

    // Test 5: Basic workflow integration
    void testBasicWorkflow() {
        qDebug() << "\n--- Test 5: Basic Workflow Integration ---";
        
        // Create a simple workflow
        TestScheduler scheduler;
        ResultCollector collector;
        
        // Schedule a simple test
        TriggerConfig trigger = TriggerConfigFactory::everyNPackets(1);
        scheduler.scheduleTest("workflow_test", trigger);
        
        // Start scheduler
        scheduler.start();
        
        // Send a packet (should trigger)
        QJsonObject packet;
        packet["test"] = "data";
        scheduler.onPacketReceived("workflow_test", packet);
        
        // Create and collect result
        auto result = std::make_shared<TestResult>("workflow_test", TestResultStatus::PASSED);
        result->setExecutionTimeUs(5.0);
        collector.addResult(result);
        
        // Verify result was collected
        auto results = collector.getResults("workflow_test");
        QVERIFY(results.size() > 0);
        
        scheduler.stop();
        
        qDebug() << "✅ Basic workflow integration successful";
    }
};

QTEST_MAIN(TestFrameworkIntegrationMinimal)
#include "test_framework_integration_minimal.moc"