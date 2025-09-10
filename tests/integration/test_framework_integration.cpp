#include <QtTest>
#include <QObject>
#include <QSignalSpy>
#include <QTimer>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <memory>
#include <thread>
#include <chrono>

// Test Framework includes
#include "../../src/test_framework/core/test_definition.h"
#include "../../src/test_framework/core/test_result.h"
#include "../../src/test_framework/execution/expression_evaluator.h"
#include "../../src/test_framework/execution/test_scheduler.h"
#include "../../src/test_framework/execution/result_collector.h"
#include "../../src/test_framework/execution/alert_manager.h"

// Packet Processing includes (if available)
#include "../../src/packet/core/packet.h"

using namespace Monitor::TestFramework;

/**
 * @brief Integration tests for Test Framework with packet pipeline and UI components
 * 
 * These tests validate the complete end-to-end functionality of the test framework
 * including packet processing integration, UI component interaction, and full workflow
 */
class TestFrameworkIntegration : public QObject
{
    Q_OBJECT

private:
    // Helper method to create test packet data
    QJsonObject createTestPacket(const QString& packetId, double velocity_x, double velocity_y, 
                                double acceleration, int status, double timestamp) {
        QJsonObject packet;
        packet["packet_id"] = packetId;
        packet["timestamp"] = timestamp;
        
        QJsonObject velocity;
        velocity["x"] = velocity_x;
        velocity["y"] = velocity_y;
        packet["velocity"] = velocity;
        
        packet["acceleration"] = acceleration;
        packet["status"] = status;
        
        return packet;
    }
    
    // Helper method to create evaluation context from packet
    EvaluationContext createContextFromPacket(const QJsonObject& packet) {
        EvaluationContext context;
        context.setVariable("velocity_x", QVariant(packet["velocity"].toObject()["x"].toDouble()));
        context.setVariable("velocity_y", QVariant(packet["velocity"].toObject()["y"].toDouble()));
        context.setVariable("acceleration", QVariant(packet["acceleration"].toDouble()));
        context.setVariable("status", QVariant(packet["status"].toInt()));
        context.setVariable("timestamp", QVariant(packet["timestamp"].toDouble()));
        return context;
    }

private slots:
    void initTestCase() {
        qDebug() << "=== Test Framework Integration Tests ===";
        qDebug() << "Testing end-to-end test framework functionality";
        qDebug() << "Including packet processing and UI integration";
        qDebug() << "";
    }
    
    void cleanupTestCase() {
        qDebug() << "";
        qDebug() << "=== Integration Tests Completed ===";
    }

    // Test 1: Basic packet processing integration
    void testPacketProcessingIntegration() {
        qDebug() << "--- Test 1: Packet Processing Integration ---";
        
        // Create test definition for velocity validation
        auto testDef = std::make_shared<TestDefinition>("velocity_test");
        testDef->setName("Validate velocity is within range");
        testDef->setExpression("velocity_x >= -100 && velocity_x <= 100");
        testDef->setEnabled(true);
        
        // Create test packets
        QJsonObject validPacket = createTestPacket("test_001", 50.0, 25.0, 10.0, 1, 1000.0);
        QJsonObject invalidPacket = createTestPacket("test_002", 150.0, 25.0, 10.0, 1, 2000.0);
        
        // Test with valid packet
        EvaluationContext validContext = createContextFromPacket(validPacket);
        QVariant result1 = ExpressionEvaluator::evaluateString(testDef->getExpression(), validContext);
        QVERIFY(result1.toBool() == true);
        
        // Test with invalid packet  
        EvaluationContext invalidContext = createContextFromPacket(invalidPacket);
        QVariant result2 = ExpressionEvaluator::evaluateString(testDef->getExpression(), invalidContext);
        QVERIFY(result2.toBool() == false);
        
        qDebug() << "✅ Packet processing integration successful";
    }

    // Test 2: Test scheduler integration with packet stream
    void testSchedulerPacketIntegration() {
        qDebug() << "\n--- Test 2: Scheduler Packet Integration ---";
        
        TestScheduler scheduler;
        QSignalSpy readySpy(&scheduler, &TestScheduler::testReadyForExecution);
        
        // Schedule test to run every 3 packets
        TriggerConfig trigger = TriggerConfigFactory::everyNPackets(3);
        scheduler.scheduleTest("packet_count_test", trigger);
        
        scheduler.start();
        
        // Send packets and verify trigger
        for (int i = 1; i <= 5; ++i) {
            QJsonObject packet = createTestPacket(QString("packet_%1").arg(i), 
                                                 i * 10.0, i * 5.0, i * 2.0, 1, i * 1000.0);
            scheduler.onPacketReceived("packet_count_test", packet);
        }
        
        // Should trigger after packet 3 (1 trigger)
        QCOMPARE(readySpy.count(), 1);
        QCOMPARE(readySpy.at(0).at(0).toString(), QString("packet_count_test"));
        
        // Send 2 more packets to trigger again
        for (int i = 6; i <= 7; ++i) {
            QJsonObject packet = createTestPacket(QString("packet_%1").arg(i), 
                                                 i * 10.0, i * 5.0, i * 2.0, 1, i * 1000.0);
            scheduler.onPacketReceived("packet_count_test", packet);
        }
        
        // Should trigger after packet 6 (2nd trigger)
        QCOMPARE(readySpy.count(), 2);
        
        scheduler.stop();
        qDebug() << "✅ Scheduler packet integration successful";
    }

    // Test 3: Result collector integration with test execution
    void testResultCollectorIntegration() {
        qDebug() << "\n--- Test 3: Result Collector Integration ---";
        
        ResultCollector collector;
        QSignalSpy resultSpy(&collector, &ResultCollector::resultAdded);
        QSignalSpy statsSpy(&collector, &ResultCollector::statisticsUpdated);
        
        // Configure for high performance
        AggregationConfig config = AggregationConfigFactory::highPerformance();
        collector.setAggregationConfig(config);
        
        // Create and add test results
        std::vector<TestResultPtr> results;
        for (int i = 0; i < 10; ++i) {
            auto result = std::make_shared<TestResult>(
                QString("integration_test_%1").arg(i),
                (i % 3 == 0) ? TestResultStatus::FAILED : TestResultStatus::PASSED
            );
            result->setTimestamp(QDateTime::currentDateTime().addMSecs(i * 100));
            result->setExecutionTimeUs(25.0 + i);
            result->setMessage(QString("Integration test result %1").arg(i));
            result->setActualValue(QVariant(i * 10));
            result->setExpectedValue(QVariant(50));
            
            results.push_back(result);
        }
        
        // Add results and verify signals
        collector.addResults(results);
        
        QCOMPARE(resultSpy.count(), 10); // One signal per result
        QVERIFY(statsSpy.count() >= 1);  // At least one statistics update
        
        // Verify statistics calculation
        auto stats = collector.getTestStatistics("integration_test_0");
        QVERIFY(!stats.testId.isEmpty());
        QVERIFY(stats.totalExecutions > 0);
        
        qDebug() << QString("✅ Result collector processed %1 results").arg(results.size());
    }

    // Test 4: Alert manager integration with test failures
    void testAlertManagerIntegration() {
        qDebug() << "\n--- Test 4: Alert Manager Integration ---";
        
        AlertManager alertManager;
        QSignalSpy alertSpy(&alertManager, &AlertManager::alertTriggered);
        QSignalSpy statsSpy(&alertManager, &AlertManager::statisticsUpdated);
        
        // Configure for silent mode (no actual notifications in tests)
        AlertDeliveryConfig config = AlertConfigFactory::silentMode();
        alertManager.setDeliveryConfig(config);
        
        // Add alert condition for failures
        AlertCondition condition = AlertConfigFactory::failureAlert("integration_test_*");
        alertManager.addAlertCondition(condition);
        
        // Create failing test results
        std::vector<TestResultPtr> results;
        for (int i = 0; i < 5; ++i) {
            auto result = std::make_shared<TestResult>(
                QString("integration_test_alert_%1").arg(i),
                TestResultStatus::FAILED
            );
            result->setTimestamp(QDateTime::currentDateTime().addMSecs(i * 200));
            result->setMessage(QString("Integration test failure %1").arg(i));
            
            results.push_back(result);
        }
        
        // Process results and verify alerts
        alertManager.processTestResults(results);
        
        QVERIFY(alertSpy.count() >= 1); // Should create alerts for failures
        
        // Verify alert statistics
        alertManager.updateStatistics();
        QVERIFY(statsSpy.count() >= 1);
        
        // Get unacknowledged alerts
        auto alerts = alertManager.getUnacknowledgedAlerts();
        QVERIFY(alerts.size() > 0);
        
        qDebug() << QString("✅ Alert manager processed %1 failures, created %2 alerts")
                    .arg(results.size()).arg(alerts.size());
    }

    // Test 5: Complete end-to-end workflow
    void testEndToEndWorkflow() {
        qDebug() << "\n--- Test 5: End-to-End Workflow ---";
        
        // Create all components
        TestScheduler scheduler;
        ResultCollector collector;
        AlertManager alertManager;
        
        // Configure components
        collector.setAggregationConfig(AggregationConfigFactory::highPerformance());
        alertManager.setDeliveryConfig(AlertConfigFactory::silentMode());
        
        AlertCondition errorCondition = AlertConfigFactory::errorAlert("*");
        alertManager.addAlertCondition(errorCondition);
        
        // Schedule a test that runs on every packet
        TriggerConfig triggerConfig = TriggerConfigFactory::everyNPackets(1);
        scheduler.scheduleTest("e2e_test", triggerConfig);
        
        QSignalSpy testReadySpy(&scheduler, &TestScheduler::testReadyForExecution);
        QSignalSpy resultSpy(&collector, &ResultCollector::resultAdded);
        QSignalSpy alertSpy(&alertManager, &AlertManager::alertTriggered);
        
        scheduler.start();
        
        // Simulate packet processing workflow
        QElapsedTimer timer;
        timer.start();
        
        for (int i = 0; i < 20; ++i) {
            // 1. Packet arrives and triggers test
            QJsonObject packet = createTestPacket(QString("e2e_packet_%1").arg(i), 
                                                 (i % 7 == 0) ? 200.0 : 50.0, // Some invalid values
                                                 25.0, 10.0, 1, i * 100.0);
            
            scheduler.onPacketReceived("e2e_test", packet);
            
            // 2. Test executes and produces result
            EvaluationContext context = createContextFromPacket(packet);
            QVariant testResult = ExpressionEvaluator::evaluateString("velocity_x <= 100", context);
            
            auto result = std::make_shared<TestResult>(
                "e2e_test",
                testResult.toBool() ? TestResultStatus::PASSED : TestResultStatus::FAILED
            );
            result->setTimestamp(QDateTime::currentDateTime());
            result->setExecutionTimeUs(15.0 + (i % 10));
            result->setMessage("End-to-end test execution");
            result->setActualValue(QVariant(packet["velocity"].toObject()["x"].toDouble()));
            result->setExpectedValue(QVariant(100.0));
            
            // 3. Result is collected
            collector.addResult(result);
            
            // 4. Alert manager evaluates result
            if (result->getStatus() == TestResultStatus::FAILED) {
                alertManager.processTestResult(result);
            }
            
            // Small delay to prevent overwhelming the system
            QCoreApplication::processEvents();
        }
        
        qint64 elapsedMs = timer.elapsed();
        
        scheduler.stop();
        
        // Verify end-to-end workflow
        QCOMPARE(testReadySpy.count(), 20); // All packets should trigger tests
        QCOMPARE(resultSpy.count(), 20);    // All results should be collected
        QVERIFY(alertSpy.count() > 0);      // Some failures should create alerts
        
        // Verify performance (should complete quickly)
        QVERIFY(elapsedMs < 5000); // Should complete in less than 5 seconds
        
        // Verify statistics
        auto stats = collector.getTestStatistics("e2e_test");
        QVERIFY(stats.totalExecutions == 20);
        QVERIFY(stats.successRate >= 0.0 && stats.successRate <= 100.0);
        
        qDebug() << QString("✅ End-to-end workflow completed in %1ms").arg(elapsedMs);
        qDebug() << QString("   Processed: 20 packets, %1 results, %2 alerts")
                    .arg(resultSpy.count()).arg(alertSpy.count());
        qDebug() << QString("   Success rate: %1%").arg(stats.successRate, 0, 'f', 1);
    }

    // Test 6: Performance under load
    void testIntegrationPerformance() {
        qDebug() << "\n--- Test 6: Integration Performance ---";
        
        TestScheduler scheduler;
        ResultCollector collector;
        AlertManager alertManager;
        
        // Configure for high performance
        collector.setAggregationConfig(AggregationConfigFactory::highPerformance());
        alertManager.setDeliveryConfig(AlertConfigFactory::silentMode());
        
        AlertCondition condition = AlertConfigFactory::failureAlert("perf_test");
        alertManager.addAlertCondition(condition);
        
        TriggerConfig trigger = TriggerConfigFactory::everyNPackets(1);
        scheduler.scheduleTest("perf_test", trigger);
        
        scheduler.start();
        
        QElapsedTimer timer;
        timer.start();
        
        // Process 1000 packets rapidly
        for (int i = 0; i < 1000; ++i) {
            // Packet processing
            QJsonObject packet = createTestPacket(QString("perf_packet_%1").arg(i), 
                                                 50.0 + (i % 20), 25.0, 10.0, 1, i * 10.0);
            
            scheduler.onPacketReceived("perf_test", packet);
            
            // Test execution
            EvaluationContext context = createContextFromPacket(packet);
            QVariant testResult = ExpressionEvaluator::evaluateString("velocity_x < 100", context);
            
            auto result = std::make_shared<TestResult>(
                "perf_test",
                testResult.toBool() ? TestResultStatus::PASSED : TestResultStatus::FAILED
            );
            result->setTimestamp(QDateTime::currentDateTime());
            result->setExecutionTimeUs(10.0 + (i % 5));
            
            // Result processing
            collector.addResult(result);
            
            if (result->getStatus() == TestResultStatus::FAILED) {
                alertManager.processTestResult(result);
            }
            
            // Process events occasionally to prevent blocking
            if (i % 100 == 0) {
                QCoreApplication::processEvents();
            }
        }
        
        qint64 elapsedMs = timer.elapsed();
        
        scheduler.stop();
        
        // Verify performance metrics
        double packetsPerSecond = 1000.0 / (elapsedMs / 1000.0);
        double avgTimePerPacket = elapsedMs / 1000.0;
        
        qDebug() << QString("✅ Performance test completed");
        qDebug() << QString("   Total time: %1ms").arg(elapsedMs);
        qDebug() << QString("   Throughput: %1 packets/second").arg(packetsPerSecond, 0, 'f', 1);
        qDebug() << QString("   Average time per packet: %1ms").arg(avgTimePerPacket, 0, 'f', 3);
        
        // Performance requirements (should handle at least 100 packets/second in tests)
        QVERIFY(packetsPerSecond > 100.0);
        QVERIFY(avgTimePerPacket < 10.0); // Less than 10ms per packet on average
        
        // Verify data integrity
        auto stats = collector.getTestStatistics("perf_test");
        QVERIFY(stats.totalExecutions == 1000);
    }

    // Test 7: Error handling and recovery
    void testErrorHandlingIntegration() {
        qDebug() << "\n--- Test 7: Error Handling Integration ---";
        
        // Test malformed expressions
        EvaluationContext context;
        context.setVariable("test_value", QVariant(42));
        
        // Should handle invalid expressions gracefully
        QVariant result1 = ExpressionEvaluator::evaluateString("invalid_syntax ++ ", context);
        QVERIFY(!result1.isValid() || result1.toBool() == false);
        
        // Test scheduler with invalid test
        TestScheduler scheduler;
        scheduler.scheduleTest("invalid_test", TriggerConfigFactory::everyNPackets(1));
        scheduler.start();
        
        // Should handle packet processing without crashing
        QJsonObject packet = createTestPacket("test", 50.0, 25.0, 10.0, 1, 1000.0);
        scheduler.onPacketReceived("invalid_test", packet); // Should not crash
        
        scheduler.stop();
        
        // Test result collector with invalid results
        ResultCollector collector;
        
        auto invalidResult = std::make_shared<TestResult>("", TestResultStatus::PASSED);
        // Missing required fields should be handled gracefully
        collector.addResult(invalidResult);
        
        // Test alert manager with malformed conditions
        AlertManager alertManager;
        alertManager.setDeliveryConfig(AlertConfigFactory::silentMode());
        
        AlertCondition invalidCondition;
        invalidCondition.testId = ""; // Empty pattern should be handled
        alertManager.addAlertCondition(invalidCondition);
        
        qDebug() << "✅ Error handling integration successful";
    }

    // Test 8: Concurrent operations
    void testConcurrentIntegration() {
        qDebug() << "\n--- Test 8: Concurrent Integration ---";
        
        ResultCollector collector;
        collector.setAggregationConfig(AggregationConfigFactory::highPerformance());
        
        QSignalSpy resultSpy(&collector, &ResultCollector::resultAdded);
        
        // Simulate concurrent test result additions
        const int numThreads = 4;
        const int resultsPerThread = 25;
        
        std::vector<std::thread> threads;
        
        for (int t = 0; t < numThreads; ++t) {
            threads.emplace_back([&collector, t]() {
                for (int i = 0; i < resultsPerThread; ++i) {
                    auto result = std::make_shared<TestResult>(
                        QString("concurrent_test_%1_%2").arg(t).arg(i),
                        (i % 2 == 0) ? TestResultStatus::PASSED : TestResultStatus::FAILED
                    );
                    result->setTimestamp(QDateTime::currentDateTime());
                    result->setExecutionTimeUs(20.0 + i);
                    
                    collector.addResult(result);
                    
                    // Small delay to simulate real processing
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Allow time for all signals to be processed
        QCoreApplication::processEvents();
        
        // Verify all results were processed
        QCOMPARE(resultSpy.count(), numThreads * resultsPerThread);
        
        qDebug() << QString("✅ Concurrent integration successful (%1 results from %2 threads)")
                    .arg(numThreads * resultsPerThread).arg(numThreads);
    }
};

QTEST_MAIN(TestFrameworkIntegration)
#include "test_framework_integration.moc"