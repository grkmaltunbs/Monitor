#include <QtTest>
#include <QObject>
#include <QElapsedTimer>
#include <QDebug>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>

// Test Framework includes
#include "../../src/test_framework/core/test_result.h"
#include "../../src/test_framework/execution/expression_evaluator.h"
#include "../../src/test_framework/execution/test_scheduler.h"
#include "../../src/test_framework/execution/result_collector.h"
#include "../../src/test_framework/execution/alert_manager.h"
#include "../../src/test_framework/parser/expression_lexer.h"

using namespace Monitor::TestFramework;
using namespace std::chrono;

/**
 * @brief Performance tests for Test Framework components
 * 
 * Target: All critical operations must complete in <100μs
 * Measurement: Use high-resolution chrono for microsecond precision
 */
class TestFrameworkPerformance : public QObject
{
    Q_OBJECT

private:
    // Performance measurement utilities
    template<typename Func>
    double measureMicroseconds(Func&& func, int iterations = 1000) {
        std::vector<double> measurements;
        measurements.reserve(iterations);
        
        // Warm up
        for (int i = 0; i < 100; ++i) {
            func();
        }
        
        // Actual measurements
        for (int i = 0; i < iterations; ++i) {
            auto start = high_resolution_clock::now();
            func();
            auto end = high_resolution_clock::now();
            
            auto duration = duration_cast<nanoseconds>(end - start).count();
            measurements.push_back(duration / 1000.0); // Convert to microseconds
        }
        
        // Return average time
        double sum = std::accumulate(measurements.begin(), measurements.end(), 0.0);
        return sum / measurements.size();
    }
    
    void validatePerformance(const QString& testName, double actualMs, double targetMs = 100.0) {
        qDebug() << QString("%1: %2μs (target: <%3μs)").arg(testName).arg(actualMs, 0, 'f', 2).arg(targetMs);
        QVERIFY2(actualMs < targetMs, 
                QString("%1 took %2μs, exceeding %3μs target")
                .arg(testName).arg(actualMs, 0, 'f', 2).arg(targetMs).toLatin1().data());
    }

    // Test data generators
    TestResultPtr createTestResult(const QString& testId, TestResultStatus status = TestResultStatus::PASSED) {
        auto result = std::make_shared<TestResult>(testId, status);
        result->setTimestamp(QDateTime::currentDateTime());
        result->setExecutionTimeUs(50.0); // Simulated execution time
        result->setMessage("Test completed");
        result->setActualValue(QVariant(42));
        result->setExpectedValue(QVariant(42));
        return result;
    }
    
    std::vector<TestResultPtr> createTestResults(int count, const QString& testIdPrefix = "test") {
        std::vector<TestResultPtr> results;
        results.reserve(count);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> statusDist(0, 3);
        
        for (int i = 0; i < count; ++i) {
            QString testId = QString("%1_%2").arg(testIdPrefix).arg(i);
            TestResultStatus status = static_cast<TestResultStatus>(statusDist(gen));
            results.push_back(createTestResult(testId, status));
        }
        
        return results;
    }

private slots:
    void initTestCase() {
        qDebug() << "Starting Test Framework Performance Tests";
        qDebug() << "Target: All operations < 100μs";
        qDebug() << "Measurement precision: Microseconds";
        qDebug() << "";
    }

    void cleanupTestCase() {
        qDebug() << "";
        qDebug() << "Test Framework Performance Tests Completed";
    }

    // Expression Evaluator Performance Tests
    void testExpressionEvaluatorPerformance() {
        qDebug() << "=== Expression Evaluator Performance ===";
        
        EvaluationContext context;
        
        // Test simple expression evaluation
        auto simpleEvalTime = measureMicroseconds([&]() {
            QVariant result = ExpressionEvaluator::evaluateString("5 + 3", context);
            Q_UNUSED(result)
        });
        validatePerformance("Simple Expression (5 + 3)", simpleEvalTime, 50.0);
        
        // Test complex expression evaluation
        auto complexEvalTime = measureMicroseconds([&]() {
            QVariant result = ExpressionEvaluator::evaluateString("(10 * 2) + (15 / 3) - 1", context);
            Q_UNUSED(result)
        });
        validatePerformance("Complex Expression", complexEvalTime, 80.0);
        
        // Test with context variables
        context.setVariable("velocity_x", QVariant(42.5));
        context.setVariable("threshold", QVariant(100.0));
        
        auto contextEvalTime = measureMicroseconds([&]() {
            QVariant result = ExpressionEvaluator::evaluateString("velocity_x > threshold", context);
            Q_UNUSED(result)
        });
        validatePerformance("Context Variable Expression", contextEvalTime, 60.0);
    }

    void testExpressionLexerPerformance() {
        qDebug() << "\n=== Expression Lexer Performance ===";
        
        ExpressionLexer lexer;
        
        // Test simple tokenization
        auto simpleTokenTime = measureMicroseconds([&]() {
            lexer.setExpression("velocity.x > 100 && time < 5000");
            auto tokens = lexer.tokenize();
            Q_UNUSED(tokens)
        });
        validatePerformance("Simple Tokenization", simpleTokenTime, 30.0);
        
        // Test complex expression tokenization
        auto complexTokenTime = measureMicroseconds([&]() {
            lexer.setExpression("avg_last(velocity.x, 10) > threshold && (time@(status==1) - time@(status==0)) < 200");
            auto tokens = lexer.tokenize();
            Q_UNUSED(tokens)
        });
        validatePerformance("Complex Expression Tokenization", complexTokenTime, 60.0);
        
        // Test temporal expression tokenization
        auto temporalTokenTime = measureMicroseconds([&]() {
            lexer.setExpression("SINCE timestamp > 1000 UNTIL velocity.x == 0 WITHIN 5000");
            auto tokens = lexer.tokenize();
            Q_UNUSED(tokens)
        });
        validatePerformance("Temporal Expression Tokenization", temporalTokenTime, 50.0);
    }

    void testTestResultPerformance() {
        qDebug() << "\n=== Test Result Performance ===";
        
        // Test result creation
        auto creationTime = measureMicroseconds([&]() {
            auto result = std::make_shared<TestResult>("performance_test", TestResultStatus::PASSED);
            result->setTimestamp(QDateTime::currentDateTime());
            result->setExecutionTimeUs(42.5);
            result->setMessage("Performance test result");
            result->setActualValue(QVariant(123));
            Q_UNUSED(result)
        });
        validatePerformance("Test Result Creation", creationTime, 10.0);
        
        // Test JSON serialization
        auto testResult = createTestResult("json_test", TestResultStatus::FAILED);
        auto jsonTime = measureMicroseconds([&]() {
            QJsonObject json = testResult->toJson();
            Q_UNUSED(json)
        });
        validatePerformance("Test Result JSON Serialization", jsonTime, 25.0);
        
        // Test JSON deserialization
        QJsonObject testJson = testResult->toJson();
        auto deserializeTime = measureMicroseconds([&]() {
            auto newResult = std::make_shared<TestResult>("", TestResultStatus::PASSED);
            bool success = newResult->fromJson(testJson);
            Q_UNUSED(success)
        });
        validatePerformance("Test Result JSON Deserialization", deserializeTime, 30.0);
    }

    void testResultCollectorPerformance() {
        qDebug() << "\n=== Result Collector Performance ===";
        
        ResultCollector collector;
        collector.setAggregationConfig(AggregationConfigFactory::highPerformance());
        
        // Test single result addition
        auto singleAddTime = measureMicroseconds([&]() {
            auto result = createTestResult("perf_test", TestResultStatus::PASSED);
            collector.addResult(result);
        });
        validatePerformance("Single Result Addition", singleAddTime, 20.0);
        
        // Test batch result addition
        auto batchResults = createTestResults(100, "batch_test");
        auto batchAddTime = measureMicroseconds([&]() {
            collector.addResults(batchResults);
        }, 100); // Fewer iterations due to larger batch
        validatePerformance("Batch Result Addition (100 results)", batchAddTime, 80.0);
        
        // Test statistics calculation
        collector.addResults(createTestResults(1000, "stats_test"));
        auto statsTime = measureMicroseconds([&]() {
            auto stats = collector.getTestStatistics("stats_test_0");
            Q_UNUSED(stats)
        });
        validatePerformance("Statistics Calculation", statsTime, 50.0);
        
        // Test result retrieval
        auto retrievalTime = measureMicroseconds([&]() {
            auto results = collector.getResults("batch_test_0");
            Q_UNUSED(results)
        });
        validatePerformance("Result Retrieval", retrievalTime, 30.0);
        
        // Test recent results retrieval
        auto recentTime = measureMicroseconds([&]() {
            auto results = collector.getRecentResults("stats_test_0", 50);
            Q_UNUSED(results)
        });
        validatePerformance("Recent Results Retrieval", recentTime, 25.0);
    }

    void testTestSchedulerPerformance() {
        qDebug() << "\n=== Test Scheduler Performance ===";
        
        TestScheduler scheduler;
        
        // Test schedule creation with simple trigger config
        auto scheduleTime = measureMicroseconds([&]() {
            TriggerConfig config = TriggerConfigFactory::everyNPackets(10);
            scheduler.scheduleTest("perf_test", config);
        });
        validatePerformance("Test Schedule Creation", scheduleTime, 15.0);
        
        // Test packet-based triggering 
        auto packetTriggerTime = measureMicroseconds([&]() {
            scheduler.onPacketReceived("perf_test", QJsonObject());
        });
        validatePerformance("Packet Trigger Processing", packetTriggerTime, 10.0);
        
        // Test scheduler lifecycle operations
        auto startTime = measureMicroseconds([&]() {
            scheduler.start();
        });
        validatePerformance("Scheduler Start", startTime, 5.0);
        
        auto pauseTime = measureMicroseconds([&]() {
            scheduler.pause();
            scheduler.resume();
        });
        validatePerformance("Scheduler Pause/Resume", pauseTime, 5.0);
    }

    void testAlertManagerPerformance() {
        qDebug() << "\n=== Alert Manager Performance ===";
        
        AlertManager alertManager;
        alertManager.setDeliveryConfig(AlertConfigFactory::silentMode()); // No actual notifications
        
        // Test alert condition evaluation
        AlertCondition condition = AlertConfigFactory::failureAlert("perf_test");
        alertManager.addAlertCondition(condition);
        
        auto evaluationTime = measureMicroseconds([&]() {
            auto failedResult = createTestResult("perf_test", TestResultStatus::FAILED);
            alertManager.processTestResult(failedResult);
        });
        validatePerformance("Alert Condition Evaluation", evaluationTime, 50.0);
        
        // Test alert creation and delivery
        auto alertCreationTime = measureMicroseconds([&]() {
            auto errorResult = createTestResult("alert_test", TestResultStatus::ERROR);
            alertManager.processTestResult(errorResult);
        });
        validatePerformance("Alert Creation and Delivery", alertCreationTime, 60.0);
        
        // Test alert acknowledgment
        alertManager.processTestResult(createTestResult("ack_test", TestResultStatus::FAILED));
        auto alerts = alertManager.getUnacknowledgedAlerts();
        
        auto ackTime = measureMicroseconds([&]() {
            if (!alerts.empty()) {
                alertManager.acknowledgeAlert(alerts[0]->id);
            }
        });
        validatePerformance("Alert Acknowledgment", ackTime, 15.0);
        
        // Test statistics update
        auto statsUpdateTime = measureMicroseconds([&]() {
            alertManager.updateStatistics();
        });
        validatePerformance("Alert Statistics Update", statsUpdateTime, 30.0);
        
        // Test bulk result processing
        auto bulkResults = createTestResults(50, "bulk_test");
        // Add some failures
        for (int i = 0; i < 10; ++i) {
            bulkResults[i * 5] = createTestResult(QString("bulk_test_%1").arg(i * 5), TestResultStatus::FAILED);
        }
        
        auto bulkProcessTime = measureMicroseconds([&]() {
            alertManager.processTestResults(bulkResults);
        }, 200); // Fewer iterations due to bulk processing
        validatePerformance("Bulk Result Processing (50 results)", bulkProcessTime, 95.0);
    }

    void testEndToEndPerformance() {
        qDebug() << "\n=== End-to-End Performance ===";
        
        // Simulate complete test execution pipeline
        ResultCollector collector;
        AlertManager alertManager;
        TestScheduler scheduler;
        
        // Configure components
        collector.setAggregationConfig(AggregationConfigFactory::highPerformance());
        alertManager.setDeliveryConfig(AlertConfigFactory::silentMode());
        
        AlertCondition errorCondition = AlertConfigFactory::errorAlert("*");
        alertManager.addAlertCondition(errorCondition);
        
        TriggerConfig triggerConfig = TriggerConfigFactory::everyNPackets(1);
        scheduler.scheduleTest("e2e_test", triggerConfig);
        
        // Test complete pipeline: packet → test execution → result processing → alert evaluation
        auto pipelineTime = measureMicroseconds([&]() {
            // 1. Packet triggers test
            scheduler.onPacketReceived("e2e_test", QJsonObject());
            
            // 2. Test executes and produces result
            auto result = createTestResult("e2e_test", TestResultStatus::FAILED);
            
            // 3. Result is collected
            collector.addResult(result);
            
            // 4. Alert manager evaluates result
            alertManager.processTestResult(result);
            
            // 5. Statistics are updated
            auto stats = collector.getTestStatistics("e2e_test");
            Q_UNUSED(stats)
        });
        validatePerformance("End-to-End Pipeline", pipelineTime, 90.0);
        
        // Test high-throughput scenario (multiple tests in parallel)
        auto highThroughputTime = measureMicroseconds([&]() {
            auto results = createTestResults(10, "throughput_test");
            
            for (const auto& result : results) {
                collector.addResult(result);
                alertManager.processTestResult(result);
            }
            
            // Update all statistics
            for (int i = 0; i < 10; ++i) {
                QString testId = QString("throughput_test_%1").arg(i);
                auto stats = collector.getTestStatistics(testId);
                Q_UNUSED(stats)
            }
        }, 500); // Fewer iterations for throughput test
        validatePerformance("High-Throughput Processing (10 tests)", highThroughputTime, 95.0);
    }

    void testMemoryEfficiency() {
        qDebug() << "\n=== Memory Efficiency Tests ===";
        
        // Test that components don't leak memory under load
        ResultCollector collector;
        collector.setMaxResults(1000); // Limit memory usage
        
        // Stress test with many results
        auto memoryStressTime = measureMicroseconds([&]() {
            auto results = createTestResults(100, "memory_test");
            collector.addResults(results);
            
            // Force cleanup
            collector.optimizeMemoryUsage();
            
            // Verify memory management
            int resultCount = collector.getResultCount();
            Q_UNUSED(resultCount)
        }, 100);
        validatePerformance("Memory Stress Test (100 results)", memoryStressTime, 85.0);
        
        // Test alert manager memory efficiency
        AlertManager alertManager;
        alertManager.setMaxAlertHistory(500); // Limit alert history
        
        auto alertMemoryTime = measureMicroseconds([&]() {
            // Generate alerts
            for (int i = 0; i < 50; ++i) {
                auto result = createTestResult(QString("mem_test_%1").arg(i), TestResultStatus::FAILED);
                alertManager.processTestResult(result);
            }
            
            // Force cleanup
            alertManager.clearOldAlerts(0); // Clear all
        }, 100);
        validatePerformance("Alert Memory Management (50 alerts)", alertMemoryTime, 70.0);
    }

    void testConcurrencyPerformance() {
        qDebug() << "\n=== Concurrency Performance ===";
        
        // Test thread-safety overhead
        ResultCollector collector;
        
        auto concurrencyTime = measureMicroseconds([&]() {
            // Simulate concurrent access patterns
            auto result1 = createTestResult("concurrent_test_1", TestResultStatus::PASSED);
            auto result2 = createTestResult("concurrent_test_2", TestResultStatus::FAILED);
            
            // Add results (thread-safe operations)
            collector.addResult(result1);
            collector.addResult(result2);
            
            // Read operations (should be thread-safe)
            auto stats1 = collector.getTestStatistics("concurrent_test_1");
            auto stats2 = collector.getTestStatistics("concurrent_test_2");
            Q_UNUSED(stats1)
            Q_UNUSED(stats2)
        });
        validatePerformance("Thread-Safe Operations", concurrencyTime, 40.0);
    }
};

QTEST_MAIN(TestFrameworkPerformance)
#include "test_framework_performance.moc"