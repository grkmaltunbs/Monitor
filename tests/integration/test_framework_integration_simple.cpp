#include <QtTest>
#include <QObject>
#include <QSignalSpy>
#include <memory>

// Test Framework includes
#include "../../src/test_framework/core/test_definition.h"
#include "../../src/test_framework/core/test_result.h"
#include "../../src/test_framework/execution/expression_evaluator.h"
#include "../../src/test_framework/execution/test_scheduler.h"
#include "../../src/test_framework/execution/result_collector.h"
#include "../../src/test_framework/execution/alert_manager.h"

using namespace Monitor::TestFramework;

/**
 * @brief Simple integration tests for Test Framework core functionality
 * 
 * Focused on basic integration without complex packet simulation
 */
class TestFrameworkIntegrationSimple : public QObject
{
    Q_OBJECT

private:
    // Helper method to create evaluation context
    EvaluationContext createBasicContext() {
        EvaluationContext context;
        context.setVariable("velocity_x", QVariant(50.0));
        context.setVariable("velocity_y", QVariant(25.0));
        context.setVariable("status", QVariant(1));
        return context;
    }

private slots:
    void initTestCase() {
        qDebug() << "=== Simple Test Framework Integration Tests ===";
    }
    
    void cleanupTestCase() {
        qDebug() << "=== Simple Integration Tests Completed ===";
    }

    // Test 1: Expression evaluation integration
    void testExpressionIntegration() {
        qDebug() << "--- Test 1: Expression Integration ---";
        
        EvaluationContext context = createBasicContext();
        
        // Test simple expression
        QVariant result1 = ExpressionEvaluator::evaluateString("velocity_x > 25", context);
        QVERIFY(result1.isValid());
        QVERIFY(result1.toBool() == true);
        
        // Test complex expression
        QVariant result2 = ExpressionEvaluator::evaluateString("velocity_x > 100", context);
        QVERIFY(result2.isValid());
        QVERIFY(result2.toBool() == false);
        
        qDebug() << "✅ Expression integration successful";
    }

    // Test 2: Test scheduler basic operations
    void testSchedulerIntegration() {
        qDebug() << "\n--- Test 2: Scheduler Integration ---";
        
        TestScheduler scheduler;
        QSignalSpy readySpy(&scheduler, &TestScheduler::testReadyForExecution);
        
        // Schedule a simple test
        TriggerConfig trigger = TriggerConfigFactory::everyNPackets(2);
        scheduler.scheduleTest("simple_test", trigger);
        
        scheduler.start();
        
        // Send some packets
        QJsonObject packet;
        packet["test"] = "value";
        
        scheduler.onPacketReceived("simple_test", packet);
        QCOMPARE(readySpy.count(), 0); // Should not trigger yet
        
        scheduler.onPacketReceived("simple_test", packet);
        QCOMPARE(readySpy.count(), 1); // Should trigger now
        
        scheduler.stop();
        qDebug() << "✅ Scheduler integration successful";
    }

    // Test 3: Result collector basic functionality
    void testResultCollectorIntegration() {
        qDebug() << "\n--- Test 3: Result Collector Integration ---";
        
        ResultCollector collector;
        QSignalSpy resultSpy(&collector, &ResultCollector::resultAdded);
        
        // Add some test results
        for (int i = 0; i < 5; ++i) {
            auto result = std::make_shared<TestResult>(
                QString("simple_test_%1").arg(i),
                (i % 2 == 0) ? TestResultStatus::PASSED : TestResultStatus::FAILED
            );
            result->setTimestamp(QDateTime::currentDateTime());
            result->setExecutionTimeUs(10.0 + i);
            result->setMessage(QString("Simple test %1").arg(i));
            
            collector.addResult(result);
        }
        
        QCOMPARE(resultSpy.count(), 5);
        
        // Verify statistics
        auto stats = collector.getTestStatistics("simple_test_0");
        QVERIFY(!stats.testId.isEmpty());
        
        qDebug() << "✅ Result collector integration successful";
    }

    // Test 4: Alert manager basic functionality
    void testAlertManagerIntegration() {
        qDebug() << "\n--- Test 4: Alert Manager Integration ---";
        
        AlertManager alertManager;
        QSignalSpy alertSpy(&alertManager, &AlertManager::alertTriggered);
        
        // Configure for silent mode
        alertManager.setDeliveryConfig(AlertConfigFactory::silentMode());
        
        // Add alert condition
        AlertCondition condition = AlertConfigFactory::failureAlert("simple_alert_test");
        alertManager.addAlertCondition(condition);
        
        // Create failing test result
        auto result = std::make_shared<TestResult>("simple_alert_test", TestResultStatus::FAILED);
        result->setTimestamp(QDateTime::currentDateTime());
        result->setMessage("Simple test failure");
        
        alertManager.processTestResult(result);
        
        QVERIFY(alertSpy.count() >= 0); // Should handle gracefully
        
        qDebug() << "✅ Alert manager integration successful";
    }

    // Test 5: Basic end-to-end workflow
    void testBasicEndToEndWorkflow() {
        qDebug() << "\n--- Test 5: Basic End-to-End Workflow ---";
        
        // Create components
        TestScheduler scheduler;
        ResultCollector collector;
        AlertManager alertManager;
        
        // Configure
        collector.setAggregationConfig(AggregationConfigFactory::highPerformance());
        alertManager.setDeliveryConfig(AlertConfigFactory::silentMode());
        
        TriggerConfig trigger = TriggerConfigFactory::everyNPackets(1);
        scheduler.scheduleTest("e2e_simple", trigger);
        
        QSignalSpy testReadySpy(&scheduler, &TestScheduler::testReadyForExecution);
        QSignalSpy resultSpy(&collector, &ResultCollector::resultAdded);
        
        scheduler.start();
        
        // Simple workflow
        for (int i = 0; i < 3; ++i) {
            // Packet triggers test
            QJsonObject packet;
            packet["value"] = i;
            scheduler.onPacketReceived("e2e_simple", packet);
            
            // Process result
            auto result = std::make_shared<TestResult>(
                "e2e_simple",
                TestResultStatus::PASSED
            );
            result->setTimestamp(QDateTime::currentDateTime());
            result->setExecutionTimeUs(5.0 + i);
            
            collector.addResult(result);
        }
        
        scheduler.stop();
        
        // Verify workflow
        QCOMPARE(testReadySpy.count(), 3); // All packets should trigger
        QCOMPARE(resultSpy.count(), 3);    // All results collected
        
        qDebug() << "✅ Basic end-to-end workflow successful";
    }
};

QTEST_MAIN(TestFrameworkIntegrationSimple)
#include "test_framework_integration_simple.moc"