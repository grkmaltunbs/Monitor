#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QTimer>
#include <QEventLoop>
#include <memory>

// Application includes
#include "../../../../src/ui/windows/performance_dashboard.h"
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <memory>
#include <chrono>

#include "../../../../../src/ui/windows/performance_dashboard.h"

class TestPerformanceDashboard : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Dashboard creation and initialization tests
    void testDashboardCreation();
    void testDashboardInitialization();
    void testMultipleDashboards();
    void testDashboardDestruction();

    // UI Component tests
    void testTabWidget();
    void testSystemOverviewTab();
    void testWidgetMetricsTab();
    void testPipelineTab();
    void testAlertsTab();
    void testHistoryTab();

    // System metrics tests
    void testSystemMetrics();
    void testCPUMetrics();
    void testMemoryMetrics();
    void testNetworkMetrics();
    void testDiskMetrics();
    void testCustomMetrics();

    // Widget metrics tests
    void testWidgetMetrics();
    void testWidgetCreation();
    void testWidgetDestruction();
    void testWidgetPerformance();
    void testWidgetMemoryUsage();

    // Pipeline monitoring tests
    void testPacketPipeline();
    void testNetworkReceiver();
    void testParserThroughput();
    void testWidgetDistribution();
    void testTestExecution();
    void testBottleneckDetection();

    // Performance alert tests
    void testPerformanceAlert();
    void testAlertTriggers();
    void testCriticalAlerts();
    void testAlertHistory();
    void testAlertAcknowledgment();
    void testAlertClearing();

    // Historical data tests
    void testHistoricalData();
    void testDataRetention();
    void testDataExport();
    void testTrendAnalysis();
    void testDataVisualization();

    // Real-time monitoring tests
    void testRealTimeUpdates();
    void testUpdateIntervals();
    void testDataCollection();
    void testMonitoringControls();
    void testPerformanceImpact();

    // Settings and configuration tests
    void testDashboardSettings();
    void testThresholdConfiguration();
    void testUpdateFrequency();
    void testDisplayOptions();
    void testAlertConfiguration();

    // Chart and visualization tests
    void testChartCreation();
    void testChartUpdates();
    void testMultipleCharts();
    void testChartPerformance();
    void testChartExport();

    // Signal emission tests
    void testAlertTriggeredSignal();
    void testCriticalAlertTriggeredSignal();
    void testAlertsGlearedSignal();
    void testMonitoringSignals();
    void testMetricsUpdatedSignal();

    // Error handling tests
    void testInvalidMetrics();
    void testResourceExhaustion();
    void testDataCorruption();
    void testRecoveryMechanisms();

    // Performance tests
    void testDashboardPerformance();
    void testMemoryUsage();
    void testLargeDataSets();
    void testLongRunningMonitoring();

    // Integration tests
    void testApplicationIntegration();
    void testWidgetIntegration();
    void testDataSourceIntegration();
    void testExportIntegration();

    // Advanced features tests
    void testCustomAlerts();
    void testAdvancedVisualization();
    void testReportGeneration();
    void testDataAnalysis();

private:
    std::unique_ptr<PerformanceDashboard> dashboard;
    QApplication* app;
    
    // Helper methods
    SystemMetrics createTestSystemMetrics();
    WidgetMetrics createTestWidgetMetrics();
    PerformanceAlert createTestAlert();
    void addTestData();
    void waitForUpdate();
    void simulateSystemLoad();
    void verifyDashboardState();
};

void TestPerformanceDashboard::initTestCase()
{
    // Ensure we have a QApplication for Qt widgets
    if (!QApplication::instance()) {
        int argc = 1;
        char* argv[] = {"test"};
        app = new QApplication(argc, argv);
    }
}

void TestPerformanceDashboard::cleanupTestCase()
{
    // QApplication cleanup handled by Qt
}

void TestPerformanceDashboard::init()
{
    dashboard = std::make_unique<PerformanceDashboard>();
}

void TestPerformanceDashboard::cleanup()
{
    dashboard.reset();
}

void TestPerformanceDashboard::testDashboardCreation()
{
    QVERIFY(dashboard != nullptr);
    QVERIFY(dashboard->isVisible() == false); // Not shown by default
    
    // Verify dialog properties
    QVERIFY(!dashboard->windowTitle().isEmpty());
    QVERIFY(dashboard->isModal() == false);
    
    // Verify basic functionality
    QVERIFY(dashboard->isMonitoringActive() == false);
}

void TestPerformanceDashboard::testDashboardInitialization()
{
    // Test that all UI components are properly initialized
    QVERIFY(dashboard->getTabWidget() != nullptr);
    QCOMPARE(dashboard->getTabCount(), 5); // System, Widgets, Pipeline, Alerts, History
    
    // Verify tabs are created
    QVERIFY(dashboard->hasSystemOverviewTab());
    QVERIFY(dashboard->hasWidgetMetricsTab());
    QVERIFY(dashboard->hasPipelineTab());
    QVERIFY(dashboard->hasAlertsTab());
    QVERIFY(dashboard->hasHistoryTab());
    
    // Verify monitoring state
    QVERIFY(dashboard->isInitialized());
    QVERIFY(!dashboard->isMonitoringActive());
}

void TestPerformanceDashboard::testMultipleDashboards()
{
    // Test creating multiple dashboard instances
    auto dashboard1 = std::make_unique<PerformanceDashboard>();
    auto dashboard2 = std::make_unique<PerformanceDashboard>();
    auto dashboard3 = std::make_unique<PerformanceDashboard>();
    
    QVERIFY(dashboard1->isInitialized());
    QVERIFY(dashboard2->isInitialized());
    QVERIFY(dashboard3->isInitialized());
    
    // Verify they have independent state
    dashboard1->onStartMonitoring();
    QVERIFY(dashboard1->isMonitoringActive());
    QVERIFY(!dashboard2->isMonitoringActive());
    QVERIFY(!dashboard3->isMonitoringActive());
}

void TestPerformanceDashboard::testDashboardDestruction()
{
    // Test proper cleanup during destruction
    auto tempDashboard = std::make_unique<PerformanceDashboard>();
    tempDashboard->onStartMonitoring();
    
    addTestData();
    waitForUpdate();
    
    // Destroy dashboard
    tempDashboard.reset();
    
    // Original dashboard should still work
    QVERIFY(dashboard->isInitialized());
}

void TestPerformanceDashboard::testTabWidget()
{
    auto tabWidget = dashboard->getTabWidget();
    QVERIFY(tabWidget != nullptr);
    
    QCOMPARE(tabWidget->count(), 5);
    
    // Test tab switching
    for (int i = 0; i < tabWidget->count(); ++i) {
        tabWidget->setCurrentIndex(i);
        QCOMPARE(tabWidget->currentIndex(), i);
        QVERIFY(tabWidget->currentWidget() != nullptr);
    }
    
    // Test tab labels
    QVERIFY(tabWidget->tabText(0).contains("System", Qt::CaseInsensitive));
    QVERIFY(tabWidget->tabText(1).contains("Widget", Qt::CaseInsensitive));
    QVERIFY(tabWidget->tabText(2).contains("Pipeline", Qt::CaseInsensitive));
    QVERIFY(tabWidget->tabText(3).contains("Alert", Qt::CaseInsensitive));
    QVERIFY(tabWidget->tabText(4).contains("History", Qt::CaseInsensitive));
}

void TestPerformanceDashboard::testSystemOverviewTab()
{
    auto systemTab = dashboard->getSystemOverviewTab();
    QVERIFY(systemTab != nullptr);
    
    // Verify system overview components
    QVERIFY(dashboard->getCPUGauge() != nullptr);
    QVERIFY(dashboard->getMemoryGauge() != nullptr);
    QVERIFY(dashboard->getNetworkGauge() != nullptr);
    QVERIFY(dashboard->getDiskGauge() != nullptr);
    
    // Test gauge updates
    SystemMetrics metrics = createTestSystemMetrics();
    dashboard->updateSystemMetrics(metrics);
    
    // Verify gauges reflect the metrics
    QCOMPARE(static_cast<int>(dashboard->getCPUGauge()->value()), static_cast<int>(metrics.cpuUsage));
    QCOMPARE(static_cast<int>(dashboard->getMemoryGauge()->value()), static_cast<int>(metrics.memoryUsage));
}

void TestPerformanceDashboard::testWidgetMetricsTab()
{
    auto widgetTab = dashboard->getWidgetMetricsTab();
    QVERIFY(widgetTab != nullptr);
    
    auto widgetTable = dashboard->getWidgetMetricsTable();
    QVERIFY(widgetTable != nullptr);
    
    // Test widget registration
    dashboard->onWidgetCreated("widget1", "GridWidget");
    dashboard->onWidgetCreated("widget2", "ChartWidget");
    
    QCOMPARE(widgetTable->rowCount(), 2);
    
    // Test widget metrics update
    WidgetMetrics metrics = createTestWidgetMetrics();
    dashboard->updateWidgetMetrics("widget1", metrics);
    
    // Verify table is updated
    QVERIFY(widgetTable->item(0, 0) != nullptr);
}

void TestPerformanceDashboard::testPipelineTab()
{
    auto pipelineTab = dashboard->getPipelineTab();
    QVERIFY(pipelineTab != nullptr);
    
    // Verify pipeline visualization components
    QVERIFY(dashboard->getPipelineChart() != nullptr);
    QVERIFY(dashboard->getBottleneckIndicator() != nullptr);
    
    // Test pipeline metrics
    PipelineMetrics metrics;
    metrics.networkReceiver.packetsPerSecond = 1000;
    metrics.networkReceiver.bytesPerSecond = 1024000;
    metrics.parser.packetsPerSecond = 950;
    metrics.parser.processingLatency = 5.0;
    
    dashboard->updatePipelineMetrics(metrics);
    
    // Verify pipeline display is updated
    QVERIFY(dashboard->getPipelineChart()->series().size() > 0);
}

void TestPerformanceDashboard::testAlertsTab()
{
    auto alertsTab = dashboard->getAlertsTab();
    QVERIFY(alertsTab != nullptr);
    
    auto alertsTable = dashboard->getAlertsTable();
    QVERIFY(alertsTable != nullptr);
    
    // Test alert creation
    PerformanceAlert alert = createTestAlert();
    
    QSignalSpy alertSpy(dashboard.get(), &PerformanceDashboard::alertTriggered);
    dashboard->triggerAlert(alert);
    
    QCOMPARE(alertSpy.count(), 1);
    QCOMPARE(alertsTable->rowCount(), 1);
    
    // Test alert acknowledgment
    dashboard->onAcknowledgeAlert();
    
    // Test alert clearing
    dashboard->onClearAlert();
}

void TestPerformanceDashboard::testHistoryTab()
{
    auto historyTab = dashboard->getHistoryTab();
    QVERIFY(historyTab != nullptr);
    
    auto historyChart = dashboard->getHistoryChart();
    QVERIFY(historyChart != nullptr);
    
    // Add historical data
    dashboard->onStartMonitoring();
    
    for (int i = 0; i < 10; ++i) {
        SystemMetrics metrics = createTestSystemMetrics();
        metrics.timestamp = std::chrono::steady_clock::now() + std::chrono::seconds(i);
        dashboard->updateSystemMetrics(metrics);
        waitForUpdate();
    }
    
    // Verify history is recorded
    QVERIFY(dashboard->getHistoryDataPoints() >= 10);
    QVERIFY(historyChart->series().size() > 0);
}

void TestPerformanceDashboard::testSystemMetrics()
{
    SystemMetrics metrics = createTestSystemMetrics();
    
    QSignalSpy metricsSpy(dashboard.get(), &PerformanceDashboard::metricsUpdated);
    dashboard->updateSystemMetrics(metrics);
    
    QCOMPARE(metricsSpy.count(), 1);
    
    auto retrievedMetrics = dashboard->getCurrentSystemMetrics();
    QCOMPARE(retrievedMetrics.cpuUsage, metrics.cpuUsage);
    QCOMPARE(retrievedMetrics.memoryUsage, metrics.memoryUsage);
    QCOMPARE(retrievedMetrics.networkBytesIn, metrics.networkBytesIn);
    QCOMPARE(retrievedMetrics.networkBytesOut, metrics.networkBytesOut);
}

void TestPerformanceDashboard::testCPUMetrics()
{
    SystemMetrics metrics;
    metrics.cpuUsage = 75.5;
    metrics.cpuCores = 8;
    metrics.cpuFrequency = 3200000000; // 3.2 GHz
    
    dashboard->updateSystemMetrics(metrics);
    
    auto retrievedMetrics = dashboard->getCurrentSystemMetrics();
    QCOMPARE(retrievedMetrics.cpuUsage, 75.5);
    QCOMPARE(retrievedMetrics.cpuCores, 8);
    QCOMPARE(retrievedMetrics.cpuFrequency, 3200000000ULL);
    
    // Verify CPU gauge reflects the values
    QCOMPARE(static_cast<int>(dashboard->getCPUGauge()->value()), 75);
}

void TestPerformanceDashboard::testMemoryMetrics()
{
    SystemMetrics metrics;
    metrics.memoryUsage = 60.0;
    metrics.memoryTotal = 16ULL * 1024 * 1024 * 1024; // 16 GB
    metrics.memoryUsed = static_cast<uint64_t>(metrics.memoryTotal * 0.6);
    metrics.memoryAvailable = metrics.memoryTotal - metrics.memoryUsed;
    
    dashboard->updateSystemMetrics(metrics);
    
    auto retrievedMetrics = dashboard->getCurrentSystemMetrics();
    QCOMPARE(retrievedMetrics.memoryUsage, 60.0);
    QCOMPARE(retrievedMetrics.memoryTotal, 16ULL * 1024 * 1024 * 1024);
    
    // Verify memory gauge
    QCOMPARE(static_cast<int>(dashboard->getMemoryGauge()->value()), 60);
}

void TestPerformanceDashboard::testNetworkMetrics()
{
    SystemMetrics metrics;
    metrics.networkBytesIn = 1024000; // 1 MB
    metrics.networkBytesOut = 512000;  // 512 KB
    metrics.networkPacketsIn = 1000;
    metrics.networkPacketsOut = 800;
    
    dashboard->updateSystemMetrics(metrics);
    
    auto retrievedMetrics = dashboard->getCurrentSystemMetrics();
    QCOMPARE(retrievedMetrics.networkBytesIn, 1024000ULL);
    QCOMPARE(retrievedMetrics.networkBytesOut, 512000ULL);
    QCOMPARE(retrievedMetrics.networkPacketsIn, 1000ULL);
    QCOMPARE(retrievedMetrics.networkPacketsOut, 800ULL);
}

void TestPerformanceDashboard::testDiskMetrics()
{
    SystemMetrics metrics;
    metrics.diskUsage = 45.0;
    metrics.diskTotal = 1000ULL * 1024 * 1024 * 1024; // 1 TB
    metrics.diskUsed = static_cast<uint64_t>(metrics.diskTotal * 0.45);
    metrics.diskReadBytes = 100000;
    metrics.diskWriteBytes = 50000;
    
    dashboard->updateSystemMetrics(metrics);
    
    auto retrievedMetrics = dashboard->getCurrentSystemMetrics();
    QCOMPARE(retrievedMetrics.diskUsage, 45.0);
    QCOMPARE(retrievedMetrics.diskTotal, 1000ULL * 1024 * 1024 * 1024);
    QCOMPARE(retrievedMetrics.diskReadBytes, 100000ULL);
    QCOMPARE(retrievedMetrics.diskWriteBytes, 50000ULL);
}

void TestPerformanceDashboard::testCustomMetrics()
{
    SystemMetrics metrics = createTestSystemMetrics();
    
    // Add custom metrics
    metrics.customMetrics["packets_processed"] = 12500;
    metrics.customMetrics["widgets_active"] = 8;
    metrics.customMetrics["tests_running"] = 3;
    
    dashboard->updateSystemMetrics(metrics);
    
    auto retrievedMetrics = dashboard->getCurrentSystemMetrics();
    QCOMPARE(retrievedMetrics.customMetrics["packets_processed"], 12500.0);
    QCOMPARE(retrievedMetrics.customMetrics["widgets_active"], 8.0);
    QCOMPARE(retrievedMetrics.customMetrics["tests_running"], 3.0);
}

void TestPerformanceDashboard::testWidgetMetrics()
{
    WidgetMetrics metrics = createTestWidgetMetrics();
    
    QSignalSpy widgetSpy(dashboard.get(), &PerformanceDashboard::widgetMetricsUpdated);
    dashboard->updateWidgetMetrics("test_widget", metrics);
    
    QCOMPARE(widgetSpy.count(), 1);
    
    auto retrievedMetrics = dashboard->getWidgetMetrics("test_widget");
    QCOMPARE(retrievedMetrics.cpuUsage, metrics.cpuUsage);
    QCOMPARE(retrievedMetrics.memoryUsage, metrics.memoryUsage);
    QCOMPARE(retrievedMetrics.updateRate, metrics.updateRate);
}

void TestPerformanceDashboard::testWidgetCreation()
{
    QSignalSpy createdSpy(dashboard.get(), &PerformanceDashboard::widgetCreated);
    
    dashboard->onWidgetCreated("widget1", "GridWidget");
    dashboard->onWidgetCreated("widget2", "ChartWidget");
    dashboard->onWidgetCreated("widget3", "3DChartWidget");
    
    QCOMPARE(createdSpy.count(), 3);
    QCOMPARE(dashboard->getWidgetCount(), 3);
    
    auto widgetList = dashboard->getWidgetList();
    QVERIFY(widgetList.contains("widget1"));
    QVERIFY(widgetList.contains("widget2"));
    QVERIFY(widgetList.contains("widget3"));
}

void TestPerformanceDashboard::testWidgetDestruction()
{
    // Create widgets first
    dashboard->onWidgetCreated("widget1", "GridWidget");
    dashboard->onWidgetCreated("widget2", "ChartWidget");
    
    QCOMPARE(dashboard->getWidgetCount(), 2);
    
    QSignalSpy destroyedSpy(dashboard.get(), &PerformanceDashboard::widgetDestroyed);
    
    dashboard->onWidgetDestroyed("widget1");
    
    QCOMPARE(destroyedSpy.count(), 1);
    QCOMPARE(dashboard->getWidgetCount(), 1);
    
    auto widgetList = dashboard->getWidgetList();
    QVERIFY(!widgetList.contains("widget1"));
    QVERIFY(widgetList.contains("widget2"));
}

void TestPerformanceDashboard::testWidgetPerformance()
{
    dashboard->onWidgetCreated("perf_widget", "TestWidget");
    
    // Simulate widget performance data
    WidgetMetrics metrics;
    metrics.cpuUsage = 15.5;
    metrics.memoryUsage = 1024 * 1024; // 1 MB
    metrics.updateRate = 60.0; // 60 FPS
    metrics.processingLatency = 5.2; // 5.2 ms
    metrics.frameDrops = 0;
    
    dashboard->updateWidgetMetrics("perf_widget", metrics);
    
    auto retrievedMetrics = dashboard->getWidgetMetrics("perf_widget");
    QCOMPARE(retrievedMetrics.cpuUsage, 15.5);
    QCOMPARE(retrievedMetrics.memoryUsage, 1024ULL * 1024);
    QCOMPARE(retrievedMetrics.updateRate, 60.0);
    QCOMPARE(retrievedMetrics.processingLatency, 5.2);
    QCOMPARE(retrievedMetrics.frameDrops, 0ULL);
}

void TestPerformanceDashboard::testWidgetMemoryUsage()
{
    dashboard->onWidgetCreated("memory_widget", "MemoryTestWidget");
    
    // Track memory usage over time
    QList<uint64_t> memoryUsages = {
        1024 * 1024,      // 1 MB
        2 * 1024 * 1024,  // 2 MB
        3 * 1024 * 1024,  // 3 MB
        2.5 * 1024 * 1024 // 2.5 MB (memory freed)
    };
    
    for (uint64_t usage : memoryUsages) {
        WidgetMetrics metrics;
        metrics.memoryUsage = usage;
        dashboard->updateWidgetMetrics("memory_widget", metrics);
        waitForUpdate();
    }
    
    // Check if memory trend is tracked
    auto history = dashboard->getWidgetMetricsHistory("memory_widget");
    QVERIFY(history.size() >= 4);
    
    // Verify peak memory detection
    uint64_t peakMemory = dashboard->getWidgetPeakMemory("memory_widget");
    QCOMPARE(peakMemory, 3ULL * 1024 * 1024);
}

void TestPerformanceDashboard::testPacketPipeline()
{
    PipelineMetrics metrics;
    
    // Network receiver metrics
    metrics.networkReceiver.packetsPerSecond = 5000;
    metrics.networkReceiver.bytesPerSecond = 5000 * 512; // 512 bytes per packet
    metrics.networkReceiver.packetsDropped = 10;
    metrics.networkReceiver.bufferUtilization = 75.0;
    
    // Parser metrics
    metrics.parser.packetsPerSecond = 4950; // Slight loss in parsing
    metrics.parser.processingLatency = 2.5;
    metrics.parser.queueDepth = 50;
    metrics.parser.errorRate = 0.1;
    
    dashboard->updatePipelineMetrics(metrics);
    
    // Verify pipeline metrics
    auto retrievedMetrics = dashboard->getCurrentPipelineMetrics();
    QCOMPARE(retrievedMetrics.networkReceiver.packetsPerSecond, 5000ULL);
    QCOMPARE(retrievedMetrics.parser.packetsPerSecond, 4950ULL);
    QCOMPARE(retrievedMetrics.parser.processingLatency, 2.5);
}

void TestPerformanceDashboard::testNetworkReceiver()
{
    PipelineMetrics metrics;
    metrics.networkReceiver.packetsPerSecond = 10000;
    metrics.networkReceiver.bytesPerSecond = 10000 * 1024; // 1KB per packet
    metrics.networkReceiver.packetsDropped = 5;
    metrics.networkReceiver.bufferUtilization = 80.0;
    metrics.networkReceiver.connectionStatus = "Connected";
    
    dashboard->updatePipelineMetrics(metrics);
    
    auto retrieved = dashboard->getCurrentPipelineMetrics();
    QCOMPARE(retrieved.networkReceiver.packetsPerSecond, 10000ULL);
    QCOMPARE(retrieved.networkReceiver.bytesPerSecond, 10000ULL * 1024);
    QCOMPARE(retrieved.networkReceiver.packetsDropped, 5ULL);
    QCOMPARE(retrieved.networkReceiver.bufferUtilization, 80.0);
}

void TestPerformanceDashboard::testParserThroughput()
{
    PipelineMetrics metrics;
    metrics.parser.packetsPerSecond = 8500;
    metrics.parser.processingLatency = 1.8;
    metrics.parser.queueDepth = 25;
    metrics.parser.errorRate = 0.05;
    metrics.parser.structuresActive = 15;
    
    dashboard->updatePipelineMetrics(metrics);
    
    auto retrieved = dashboard->getCurrentPipelineMetrics();
    QCOMPARE(retrieved.parser.packetsPerSecond, 8500ULL);
    QCOMPARE(retrieved.parser.processingLatency, 1.8);
    QCOMPARE(retrieved.parser.queueDepth, 25ULL);
    QCOMPARE(retrieved.parser.errorRate, 0.05);
}

void TestPerformanceDashboard::testWidgetDistribution()
{
    PipelineMetrics metrics;
    metrics.widgetDistribution.packetsPerSecond = 8000;
    metrics.widgetDistribution.distributionLatency = 0.5;
    metrics.widgetDistribution.widgetsActive = 12;
    metrics.widgetDistribution.queueDepth = 10;
    
    dashboard->updatePipelineMetrics(metrics);
    
    auto retrieved = dashboard->getCurrentPipelineMetrics();
    QCOMPARE(retrieved.widgetDistribution.packetsPerSecond, 8000ULL);
    QCOMPARE(retrieved.widgetDistribution.distributionLatency, 0.5);
    QCOMPARE(retrieved.widgetDistribution.widgetsActive, 12ULL);
}

void TestPerformanceDashboard::testTestExecution()
{
    PipelineMetrics metrics;
    metrics.testExecution.testsPerSecond = 500;
    metrics.testExecution.executionLatency = 10.0;
    metrics.testExecution.testsActive = 25;
    metrics.testExecution.passRate = 98.5;
    metrics.testExecution.failureRate = 1.5;
    
    dashboard->updatePipelineMetrics(metrics);
    
    auto retrieved = dashboard->getCurrentPipelineMetrics();
    QCOMPARE(retrieved.testExecution.testsPerSecond, 500ULL);
    QCOMPARE(retrieved.testExecution.executionLatency, 10.0);
    QCOMPARE(retrieved.testExecution.testsActive, 25ULL);
    QCOMPARE(retrieved.testExecution.passRate, 98.5);
}

void TestPerformanceDashboard::testBottleneckDetection()
{
    // Create scenario with parser bottleneck
    PipelineMetrics metrics;
    metrics.networkReceiver.packetsPerSecond = 10000;
    metrics.parser.packetsPerSecond = 5000; // Bottleneck here
    metrics.widgetDistribution.packetsPerSecond = 4950;
    
    dashboard->updatePipelineMetrics(metrics);
    
    // Wait for bottleneck detection
    waitForUpdate();
    
    auto bottleneck = dashboard->getBottleneckStage();
    QCOMPARE(bottleneck, QString("Parser"));
    
    auto bottleneckIndicator = dashboard->getBottleneckIndicator();
    QVERIFY(bottleneckIndicator->isVisible());
}

void TestPerformanceDashboard::testPerformanceAlert()
{
    PerformanceAlert alert = createTestAlert();
    
    QSignalSpy alertSpy(dashboard.get(), &PerformanceDashboard::alertTriggered);
    dashboard->triggerAlert(alert);
    
    QCOMPARE(alertSpy.count(), 1);
    
    auto arguments = alertSpy.first();
    auto receivedAlert = arguments.at(0).value<PerformanceAlert>();
    
    QCOMPARE(receivedAlert.type, alert.type);
    QCOMPARE(receivedAlert.severity, alert.severity);
    QCOMPARE(receivedAlert.message, alert.message);
    QCOMPARE(receivedAlert.widgetId, alert.widgetId);
}

void TestPerformanceDashboard::testAlertTriggers()
{
    // Test CPU threshold alert
    dashboard->setCPUThreshold(80.0);
    
    SystemMetrics metrics;
    metrics.cpuUsage = 85.0; // Above threshold
    
    QSignalSpy alertSpy(dashboard.get(), &PerformanceDashboard::alertTriggered);
    dashboard->updateSystemMetrics(metrics);
    
    QCOMPARE(alertSpy.count(), 1);
    
    // Test memory threshold alert
    dashboard->setMemoryThreshold(90.0);
    
    metrics.memoryUsage = 95.0; // Above threshold
    dashboard->updateSystemMetrics(metrics);
    
    QCOMPARE(alertSpy.count(), 2);
}

void TestPerformanceDashboard::testCriticalAlerts()
{
    PerformanceAlert criticalAlert;
    criticalAlert.type = PerformanceAlert::Type::SystemFailure;
    criticalAlert.severity = PerformanceAlert::Severity::Critical;
    criticalAlert.message = "System failure detected";
    criticalAlert.timestamp = std::chrono::steady_clock::now();
    
    QSignalSpy criticalSpy(dashboard.get(), &PerformanceDashboard::criticalAlertTriggered);
    dashboard->triggerAlert(criticalAlert);
    
    QCOMPARE(criticalSpy.count(), 1);
    
    // Verify critical alert is highlighted
    auto alertsTable = dashboard->getAlertsTable();
    QCOMPARE(alertsTable->rowCount(), 1);
    
    // Critical alerts should be styled differently
    auto item = alertsTable->item(0, 0);
    QVERIFY(item->background().color() == QColor(255, 0, 0, 50)); // Light red background
}

void TestPerformanceDashboard::testAlertHistory()
{
    // Generate multiple alerts
    for (int i = 0; i < 10; ++i) {
        PerformanceAlert alert;
        alert.type = PerformanceAlert::Type::HighCPU;
        alert.severity = (i < 5) ? PerformanceAlert::Severity::Warning : PerformanceAlert::Severity::Error;
        alert.message = QString("Test alert %1").arg(i);
        alert.timestamp = std::chrono::steady_clock::now() + std::chrono::seconds(i);
        
        dashboard->triggerAlert(alert);
    }
    
    auto alertHistory = dashboard->getAlertHistory();
    QCOMPARE(alertHistory.size(), 10);
    
    // Verify alerts are sorted by timestamp (newest first)
    for (int i = 1; i < alertHistory.size(); ++i) {
        QVERIFY(alertHistory[i-1].timestamp >= alertHistory[i].timestamp);
    }
}

void TestPerformanceDashboard::testAlertAcknowledgment()
{
    PerformanceAlert alert = createTestAlert();
    dashboard->triggerAlert(alert);
    
    QVERIFY(!alert.acknowledged);
    QVERIFY(dashboard->getPendingAlertsCount() == 1);
    
    dashboard->onAcknowledgeAlert();
    
    auto alertHistory = dashboard->getAlertHistory();
    QVERIFY(alertHistory[0].acknowledged);
    QVERIFY(dashboard->getPendingAlertsCount() == 0);
}

void TestPerformanceDashboard::testAlertClearing()
{
    // Add multiple alerts
    for (int i = 0; i < 5; ++i) {
        PerformanceAlert alert = createTestAlert();
        alert.message = QString("Alert %1").arg(i);
        dashboard->triggerAlert(alert);
    }
    
    QCOMPARE(dashboard->getAlertHistory().size(), 5);
    
    QSignalSpy clearedSpy(dashboard.get(), &PerformanceDashboard::alertsCleared);
    dashboard->onClearHistory();
    
    QCOMPARE(clearedSpy.count(), 1);
    QCOMPARE(dashboard->getAlertHistory().size(), 0);
}

void TestPerformanceDashboard::testHistoricalData()
{
    dashboard->onStartMonitoring();
    
    // Generate historical data over time
    for (int i = 0; i < 60; ++i) {
        SystemMetrics metrics = createTestSystemMetrics();
        metrics.cpuUsage = 50.0 + (i % 30); // Varying CPU usage
        metrics.timestamp = std::chrono::steady_clock::now() + std::chrono::seconds(i);
        
        dashboard->updateSystemMetrics(metrics);
        waitForUpdate();
    }
    
    auto historyData = dashboard->getHistoricalData();
    QVERIFY(historyData.size() >= 60);
    
    // Verify data is chronologically ordered
    for (int i = 1; i < historyData.size(); ++i) {
        QVERIFY(historyData[i].timestamp >= historyData[i-1].timestamp);
    }
}

void TestPerformanceDashboard::testDataRetention()
{
    dashboard->setDataRetentionPeriod(std::chrono::minutes(5));
    dashboard->onStartMonitoring();
    
    // Add data spanning longer than retention period
    auto baseTime = std::chrono::steady_clock::now() - std::chrono::minutes(10);
    
    for (int i = 0; i < 600; ++i) { // 10 minutes of data at 1 second intervals
        SystemMetrics metrics = createTestSystemMetrics();
        metrics.timestamp = baseTime + std::chrono::seconds(i);
        
        dashboard->addHistoricalData(metrics);
    }
    
    // Trigger cleanup
    dashboard->cleanupHistoricalData();
    
    auto historyData = dashboard->getHistoricalData();
    
    // Should only have last 5 minutes of data
    auto oldestTime = std::chrono::steady_clock::now() - std::chrono::minutes(5);
    for (const auto& data : historyData) {
        QVERIFY(data.timestamp >= oldestTime);
    }
}

void TestPerformanceDashboard::testDataExport()
{
    addTestData();
    
    QString exportPath = QDir::temp().filePath("performance_export.json");
    
    bool result = dashboard->exportData(exportPath);
    QVERIFY(result);
    
    // Verify file was created
    QVERIFY(QFile::exists(exportPath));
    
    // Verify file contents
    QFile file(exportPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QVERIFY(!doc.isNull());
    
    auto root = doc.object();
    QVERIFY(root.contains("systemMetrics"));
    QVERIFY(root.contains("widgetMetrics"));
    QVERIFY(root.contains("pipelineMetrics"));
    QVERIFY(root.contains("alerts"));
    
    // Cleanup
    QFile::remove(exportPath);
}

void TestPerformanceDashboard::testTrendAnalysis()
{
    dashboard->onStartMonitoring();
    
    // Create trending data
    for (int i = 0; i < 100; ++i) {
        SystemMetrics metrics = createTestSystemMetrics();
        metrics.cpuUsage = 30.0 + i * 0.5; // Increasing trend
        metrics.memoryUsage = 40.0 + sin(i * 0.1) * 10; // Oscillating
        metrics.timestamp = std::chrono::steady_clock::now() + std::chrono::seconds(i);
        
        dashboard->updateSystemMetrics(metrics);
    }
    
    auto cpuTrend = dashboard->analyzeCPUTrend();
    QVERIFY(cpuTrend.direction == TrendAnalysis::Direction::Increasing);
    QVERIFY(cpuTrend.confidence > 0.8);
    
    auto memoryTrend = dashboard->analyzeMemoryTrend();
    QVERIFY(memoryTrend.direction == TrendAnalysis::Direction::Stable);
}

void TestPerformanceDashboard::testDataVisualization()
{
    addTestData();
    
    auto historyChart = dashboard->getHistoryChart();
    QVERIFY(historyChart != nullptr);
    
    // Verify chart has data series
    auto chart = historyChart->chart();
    QVERIFY(chart != nullptr);
    QVERIFY(chart->series().size() > 0);
    
    // Test chart updates
    QSignalSpy chartSpy(chart, &QChart::plotAreaChanged);
    
    SystemMetrics metrics = createTestSystemMetrics();
    dashboard->updateSystemMetrics(metrics);
    
    // Chart should update
    QVERIFY(chartSpy.count() > 0);
}

void TestPerformanceDashboard::testRealTimeUpdates()
{
    dashboard->onStartMonitoring();
    QVERIFY(dashboard->isMonitoringActive());
    
    QSignalSpy updateSpy(dashboard.get(), &PerformanceDashboard::metricsUpdated);
    
    // Start real-time updates
    dashboard->startRealTimeUpdates(100); // 100ms intervals
    
    // Wait for several updates
    QTest::qWait(500);
    
    // Should have received multiple updates
    QVERIFY(updateSpy.count() >= 4);
    
    // Stop monitoring
    dashboard->onStopMonitoring();
    QVERIFY(!dashboard->isMonitoringActive());
}

void TestPerformanceDashboard::testUpdateIntervals()
{
    // Test different update intervals
    QList<int> intervals = {50, 100, 500, 1000};
    
    for (int interval : intervals) {
        dashboard->setUpdateInterval(interval);
        QCOMPARE(dashboard->getUpdateInterval(), interval);
        
        dashboard->onStartMonitoring();
        QSignalSpy updateSpy(dashboard.get(), &PerformanceDashboard::metricsUpdated);
        
        QTest::qWait(interval * 3);
        
        // Should have received approximately 3 updates
        QVERIFY(updateSpy.count() >= 2 && updateSpy.count() <= 4);
        
        dashboard->onStopMonitoring();
    }
}

void TestPerformanceDashboard::testDataCollection()
{
    dashboard->onStartMonitoring();
    
    // Test that data collection is working
    QSignalSpy systemSpy(dashboard.get(), &PerformanceDashboard::metricsUpdated);
    QSignalSpy widgetSpy(dashboard.get(), &PerformanceDashboard::widgetMetricsUpdated);
    
    // Simulate data sources
    dashboard->onWidgetCreated("test_widget", "TestWidget");
    
    QTest::qWait(200);
    
    // Should have received system metrics
    QVERIFY(systemSpy.count() >= 1);
    
    // Update widget metrics
    WidgetMetrics metrics = createTestWidgetMetrics();
    dashboard->updateWidgetMetrics("test_widget", metrics);
    
    QCOMPARE(widgetSpy.count(), 1);
}

void TestPerformanceDashboard::testMonitoringControls()
{
    QSignalSpy startedSpy(dashboard.get(), &PerformanceDashboard::monitoringStarted);
    QSignalSpy stoppedSpy(dashboard.get(), &PerformanceDashboard::monitoringStopped);
    QSignalSpy pausedSpy(dashboard.get(), &PerformanceDashboard::monitoringPaused);
    QSignalSpy resumedSpy(dashboard.get(), &PerformanceDashboard::monitoringResumed);
    
    // Test start monitoring
    dashboard->onStartMonitoring();
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(dashboard->isMonitoringActive());
    
    // Test pause monitoring
    dashboard->onPauseMonitoring();
    QCOMPARE(pausedSpy.count(), 1);
    QVERIFY(dashboard->isMonitoringPaused());
    
    // Test resume monitoring
    dashboard->onResumeMonitoring();
    QCOMPARE(resumedSpy.count(), 1);
    QVERIFY(dashboard->isMonitoringActive());
    QVERIFY(!dashboard->isMonitoringPaused());
    
    // Test stop monitoring
    dashboard->onStopMonitoring();
    QCOMPARE(stoppedSpy.count(), 1);
    QVERIFY(!dashboard->isMonitoringActive());
}

void TestPerformanceDashboard::testPerformanceImpact()
{
    // Test that the dashboard itself doesn't significantly impact performance
    
    QElapsedTimer timer;
    timer.start();
    
    dashboard->onStartMonitoring();
    
    // Simulate high-frequency updates
    for (int i = 0; i < 1000; ++i) {
        SystemMetrics metrics = createTestSystemMetrics();
        dashboard->updateSystemMetrics(metrics);
        
        if (i % 100 == 0) {
            waitForUpdate();
        }
    }
    
    qint64 elapsed = timer.elapsed();
    
    // Dashboard should handle 1000 updates efficiently
    QVERIFY(elapsed < 1000); // Less than 1 second for 1000 updates
    
    dashboard->onStopMonitoring();
    
    qDebug() << "Dashboard processed 1000 updates in" << elapsed << "ms";
}

void TestPerformanceDashboard::testDashboardSettings()
{
    DashboardSettings settings;
    settings.updateInterval = 250;
    settings.dataRetentionMinutes = 30;
    settings.enableAlerts = true;
    settings.cpuThreshold = 85.0;
    settings.memoryThreshold = 90.0;
    settings.diskThreshold = 95.0;
    settings.enableSounds = false;
    settings.theme = "dark";
    
    dashboard->setSettings(settings);
    
    auto retrievedSettings = dashboard->getSettings();
    QCOMPARE(retrievedSettings.updateInterval, 250);
    QCOMPARE(retrievedSettings.dataRetentionMinutes, 30);
    QVERIFY(retrievedSettings.enableAlerts);
    QCOMPARE(retrievedSettings.cpuThreshold, 85.0);
    QCOMPARE(retrievedSettings.memoryThreshold, 90.0);
    QVERIFY(!retrievedSettings.enableSounds);
    QCOMPARE(retrievedSettings.theme, QString("dark"));
}

void TestPerformanceDashboard::testThresholdConfiguration()
{
    // Test threshold configuration and alerts
    dashboard->setCPUThreshold(70.0);
    dashboard->setMemoryThreshold(80.0);
    dashboard->setNetworkThreshold(1000000); // 1 MB/s
    
    QCOMPARE(dashboard->getCPUThreshold(), 70.0);
    QCOMPARE(dashboard->getMemoryThreshold(), 80.0);
    QCOMPARE(dashboard->getNetworkThreshold(), 1000000ULL);
    
    // Test threshold alerts
    QSignalSpy alertSpy(dashboard.get(), &PerformanceDashboard::alertTriggered);
    
    SystemMetrics metrics;
    metrics.cpuUsage = 75.0; // Above CPU threshold
    metrics.memoryUsage = 85.0; // Above memory threshold
    
    dashboard->updateSystemMetrics(metrics);
    
    // Should trigger 2 alerts
    QCOMPARE(alertSpy.count(), 2);
}

void TestPerformanceDashboard::testUpdateFrequency()
{
    // Test various update frequencies
    QList<int> frequencies = {10, 50, 100, 500, 1000}; // Hz to milliseconds
    
    for (int freq : frequencies) {
        dashboard->setUpdateFrequency(freq);
        QCOMPARE(dashboard->getUpdateInterval(), 1000 / freq);
    }
}

void TestPerformanceDashboard::testDisplayOptions()
{
    DisplayOptions options;
    options.showSystemTab = true;
    options.showWidgetTab = true;
    options.showPipelineTab = false;
    options.showAlertsTab = true;
    options.showHistoryTab = false;
    options.compactView = true;
    options.showTooltips = true;
    options.animateCharts = false;
    
    dashboard->setDisplayOptions(options);
    
    auto retrievedOptions = dashboard->getDisplayOptions();
    QVERIFY(retrievedOptions.showSystemTab);
    QVERIFY(retrievedOptions.showWidgetTab);
    QVERIFY(!retrievedOptions.showPipelineTab);
    QVERIFY(retrievedOptions.showAlertsTab);
    QVERIFY(!retrievedOptions.showHistoryTab);
    QVERIFY(retrievedOptions.compactView);
    QVERIFY(retrievedOptions.showTooltips);
    QVERIFY(!retrievedOptions.animateCharts);
    
    // Verify tabs are shown/hidden according to options
    auto tabWidget = dashboard->getTabWidget();
    for (int i = 0; i < tabWidget->count(); ++i) {
        bool shouldBeVisible = true; // Determine based on options
        
        if (i == 2 && !options.showPipelineTab) shouldBeVisible = false;
        if (i == 4 && !options.showHistoryTab) shouldBeVisible = false;
        
        QCOMPARE(tabWidget->isTabEnabled(i), shouldBeVisible);
    }
}

void TestPerformanceDashboard::testAlertConfiguration()
{
    AlertConfiguration config;
    config.enableCPUAlerts = true;
    config.enableMemoryAlerts = true;
    config.enableNetworkAlerts = false;
    config.enableDiskAlerts = true;
    config.enableSounds = false;
    config.enablePopups = true;
    config.enableEmail = false;
    config.alertCooldownSeconds = 30;
    
    dashboard->setAlertConfiguration(config);
    
    auto retrievedConfig = dashboard->getAlertConfiguration();
    QVERIFY(retrievedConfig.enableCPUAlerts);
    QVERIFY(retrievedConfig.enableMemoryAlerts);
    QVERIFY(!retrievedConfig.enableNetworkAlerts);
    QVERIFY(retrievedConfig.enableDiskAlerts);
    QVERIFY(!retrievedConfig.enableSounds);
    QVERIFY(retrievedConfig.enablePopups);
    QVERIFY(!retrievedConfig.enableEmail);
    QCOMPARE(retrievedConfig.alertCooldownSeconds, 30);
}

void TestPerformanceDashboard::testChartCreation()
{
    // Test that all required charts are created
    QVERIFY(dashboard->getSystemChart() != nullptr);
    QVERIFY(dashboard->getWidgetChart() != nullptr);
    QVERIFY(dashboard->getPipelineChart() != nullptr);
    QVERIFY(dashboard->getHistoryChart() != nullptr);
    
    // Test chart properties
    auto systemChart = dashboard->getSystemChart()->chart();
    QVERIFY(systemChart != nullptr);
    QVERIFY(!systemChart->title().isEmpty());
    QVERIFY(systemChart->legend() != nullptr);
}

void TestPerformanceDashboard::testChartUpdates()
{
    auto systemChart = dashboard->getSystemChart()->chart();
    QSignalSpy plotSpy(systemChart, &QChart::plotAreaChanged);
    
    // Add data that should update charts
    addTestData();
    
    // Charts should be updated
    QVERIFY(plotSpy.count() > 0);
    
    // Verify chart has data
    QVERIFY(systemChart->series().size() > 0);
    
    auto series = qobject_cast<QLineSeries*>(systemChart->series().first());
    if (series) {
        QVERIFY(series->count() > 0);
    }
}

void TestPerformanceDashboard::testMultipleCharts()
{
    addTestData();
    
    QList<QChartView*> charts = {
        dashboard->getSystemChart(),
        dashboard->getWidgetChart(),
        dashboard->getPipelineChart(),
        dashboard->getHistoryChart()
    };
    
    // Verify all charts are functional
    for (auto chartView : charts) {
        QVERIFY(chartView != nullptr);
        QVERIFY(chartView->chart() != nullptr);
        QVERIFY(chartView->chart()->plotArea().isValid());
    }
}

void TestPerformanceDashboard::testChartPerformance()
{
    dashboard->onStartMonitoring();
    
    QElapsedTimer timer;
    timer.start();
    
    // Add substantial amount of data
    for (int i = 0; i < 1000; ++i) {
        SystemMetrics metrics = createTestSystemMetrics();
        dashboard->updateSystemMetrics(metrics);
        
        if (i % 100 == 0) {
            waitForUpdate();
        }
    }
    
    qint64 elapsed = timer.elapsed();
    
    // Chart updates should be efficient
    QVERIFY(elapsed < 2000); // Less than 2 seconds for 1000 updates
    
    qDebug() << "Chart performance: 1000 updates in" << elapsed << "ms";
}

void TestPerformanceDashboard::testChartExport()
{
    addTestData();
    
    auto systemChart = dashboard->getSystemChart();
    QString exportPath = QDir::temp().filePath("chart_export.png");
    
    bool result = dashboard->exportChart(systemChart, exportPath);
    QVERIFY(result);
    
    // Verify file was created
    QVERIFY(QFile::exists(exportPath));
    
    // Verify it's a valid image
    QImage image(exportPath);
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 0);
    QVERIFY(image.height() > 0);
    
    // Cleanup
    QFile::remove(exportPath);
}

void TestPerformanceDashboard::testAlertTriggeredSignal()
{
    QSignalSpy spy(dashboard.get(), &PerformanceDashboard::alertTriggered);
    
    PerformanceAlert alert = createTestAlert();
    dashboard->triggerAlert(alert);
    
    QCOMPARE(spy.count(), 1);
    
    auto arguments = spy.first();
    auto receivedAlert = arguments.at(0).value<PerformanceAlert>();
    
    QCOMPARE(receivedAlert.type, alert.type);
    QCOMPARE(receivedAlert.severity, alert.severity);
    QCOMPARE(receivedAlert.message, alert.message);
}

void TestPerformanceDashboard::testCriticalAlertTriggeredSignal()
{
    QSignalSpy spy(dashboard.get(), &PerformanceDashboard::criticalAlertTriggered);
    
    PerformanceAlert criticalAlert;
    criticalAlert.type = PerformanceAlert::Type::SystemFailure;
    criticalAlert.severity = PerformanceAlert::Severity::Critical;
    criticalAlert.message = "Critical system failure";
    
    dashboard->triggerAlert(criticalAlert);
    
    QCOMPARE(spy.count(), 1);
    
    auto arguments = spy.first();
    auto receivedAlert = arguments.at(0).value<PerformanceAlert>();
    
    QCOMPARE(receivedAlert.severity, PerformanceAlert::Severity::Critical);
}

void TestPerformanceDashboard::testAlertsGlearedSignal()
{
    // Add some alerts first
    for (int i = 0; i < 3; ++i) {
        PerformanceAlert alert = createTestAlert();
        alert.message = QString("Alert %1").arg(i);
        dashboard->triggerAlert(alert);
    }
    
    QSignalSpy spy(dashboard.get(), &PerformanceDashboard::alertsCleared);
    
    dashboard->onClearHistory();
    
    QCOMPARE(spy.count(), 1);
}

void TestPerformanceDashboard::testMonitoringSignals()
{
    QSignalSpy startedSpy(dashboard.get(), &PerformanceDashboard::monitoringStarted);
    QSignalSpy stoppedSpy(dashboard.get(), &PerformanceDashboard::monitoringStopped);
    QSignalSpy pausedSpy(dashboard.get(), &PerformanceDashboard::monitoringPaused);
    QSignalSpy resumedSpy(dashboard.get(), &PerformanceDashboard::monitoringResumed);
    
    dashboard->onStartMonitoring();
    QCOMPARE(startedSpy.count(), 1);
    
    dashboard->onPauseMonitoring();
    QCOMPARE(pausedSpy.count(), 1);
    
    dashboard->onResumeMonitoring();
    QCOMPARE(resumedSpy.count(), 1);
    
    dashboard->onStopMonitoring();
    QCOMPARE(stoppedSpy.count(), 1);
}

void TestPerformanceDashboard::testMetricsUpdatedSignal()
{
    QSignalSpy systemSpy(dashboard.get(), &PerformanceDashboard::metricsUpdated);
    QSignalSpy widgetSpy(dashboard.get(), &PerformanceDashboard::widgetMetricsUpdated);
    
    SystemMetrics systemMetrics = createTestSystemMetrics();
    dashboard->updateSystemMetrics(systemMetrics);
    
    QCOMPARE(systemSpy.count(), 1);
    
    dashboard->onWidgetCreated("test_widget", "TestWidget");
    WidgetMetrics widgetMetrics = createTestWidgetMetrics();
    dashboard->updateWidgetMetrics("test_widget", widgetMetrics);
    
    QCOMPARE(widgetSpy.count(), 1);
}

void TestPerformanceDashboard::testInvalidMetrics()
{
    // Test handling of invalid metrics
    SystemMetrics invalidMetrics;
    invalidMetrics.cpuUsage = -1.0; // Invalid
    invalidMetrics.memoryUsage = 150.0; // Invalid (over 100%)
    invalidMetrics.timestamp = std::chrono::steady_clock::time_point{}; // Invalid
    
    // Should handle gracefully
    dashboard->updateSystemMetrics(invalidMetrics);
    
    // Dashboard should still be functional
    QVERIFY(dashboard->isInitialized());
    
    // Should not have updated with invalid data
    auto currentMetrics = dashboard->getCurrentSystemMetrics();
    QVERIFY(currentMetrics.cpuUsage >= 0.0 && currentMetrics.cpuUsage <= 100.0);
    QVERIFY(currentMetrics.memoryUsage >= 0.0 && currentMetrics.memoryUsage <= 100.0);
}

void TestPerformanceDashboard::testResourceExhaustion()
{
    // Test handling of resource exhaustion
    dashboard->onStartMonitoring();
    
    try {
        // Generate extremely large amount of data
        for (int i = 0; i < 1000000; ++i) {
            SystemMetrics metrics = createTestSystemMetrics();
            metrics.timestamp = std::chrono::steady_clock::now() + std::chrono::microseconds(i);
            dashboard->updateSystemMetrics(metrics);
            
            // Check periodically that dashboard is still responsive
            if (i % 100000 == 0) {
                QVERIFY(dashboard->isInitialized());
                waitForUpdate();
            }
        }
    } catch (...) {
        // Should handle resource exhaustion gracefully
    }
    
    // Dashboard should still be functional
    QVERIFY(dashboard->isInitialized());
}

void TestPerformanceDashboard::testDataCorruption()
{
    addTestData();
    
    // Simulate data corruption scenarios
    dashboard->simulateDataCorruption();
    
    // Dashboard should detect and handle corruption
    QVERIFY(dashboard->isInitialized());
    QVERIFY(dashboard->hasDataIntegrityCheck());
    
    // Should be able to recover
    dashboard->repairData();
    QVERIFY(dashboard->verifyDataIntegrity());
}

void TestPerformanceDashboard::testRecoveryMechanisms()
{
    dashboard->onStartMonitoring();
    addTestData();
    
    // Simulate various failure scenarios
    dashboard->simulateRenderFailure();
    QVERIFY(dashboard->isInitialized());
    
    dashboard->simulateMemoryError();
    QVERIFY(dashboard->isInitialized());
    
    dashboard->simulateDataLoss();
    QVERIFY(dashboard->isInitialized());
    
    // Should be able to continue normal operation
    SystemMetrics metrics = createTestSystemMetrics();
    dashboard->updateSystemMetrics(metrics);
    
    QVERIFY(dashboard->isMonitoringActive());
}

void TestPerformanceDashboard::testDashboardPerformance()
{
    const int updateCount = 10000;
    
    QElapsedTimer timer;
    timer.start();
    
    dashboard->onStartMonitoring();
    
    for (int i = 0; i < updateCount; ++i) {
        SystemMetrics metrics = createTestSystemMetrics();
        dashboard->updateSystemMetrics(metrics);
        
        if (i % 1000 == 0) {
            waitForUpdate();
        }
    }
    
    qint64 elapsed = timer.elapsed();
    double updatesPerSecond = updateCount / (elapsed / 1000.0);
    
    // Should handle high-frequency updates efficiently
    QVERIFY(updatesPerSecond > 1000); // At least 1000 updates per second
    
    qDebug() << "Dashboard performance:" << updatesPerSecond << "updates/sec";
}

void TestPerformanceDashboard::testMemoryUsage()
{
    size_t initialMemory = dashboard->getMemoryUsage();
    
    // Add substantial data
    dashboard->onStartMonitoring();
    
    for (int i = 0; i < 10000; ++i) {
        SystemMetrics metrics = createTestSystemMetrics();
        dashboard->updateSystemMetrics(metrics);
        
        dashboard->onWidgetCreated(QString("widget_%1").arg(i), "TestWidget");
        
        WidgetMetrics widgetMetrics = createTestWidgetMetrics();
        dashboard->updateWidgetMetrics(QString("widget_%1").arg(i), widgetMetrics);
        
        if (i % 1000 == 0) {
            waitForUpdate();
        }
    }
    
    size_t afterAddMemory = dashboard->getMemoryUsage();
    
    // Clear data
    dashboard->onClearHistory();
    dashboard->onStopMonitoring();
    
    size_t afterClearMemory = dashboard->getMemoryUsage();
    
    // Memory should increase with data and decrease after clearing
    QVERIFY(afterAddMemory > initialMemory);
    QVERIFY(afterClearMemory < afterAddMemory);
    
    // Should not leak significant memory
    QVERIFY(afterClearMemory <= initialMemory * 1.2); // Allow 20% overhead
    
    qDebug() << "Memory usage: Initial:" << initialMemory 
             << "After add:" << afterAddMemory 
             << "After clear:" << afterClearMemory;
}

void TestPerformanceDashboard::testLargeDataSets()
{
    const int largeDataCount = 100000;
    
    dashboard->onStartMonitoring();
    
    QElapsedTimer timer;
    timer.start();
    
    // Add large dataset
    for (int i = 0; i < largeDataCount; ++i) {
        SystemMetrics metrics = createTestSystemMetrics();
        metrics.timestamp = std::chrono::steady_clock::now() + std::chrono::microseconds(i);
        dashboard->updateSystemMetrics(metrics);
        
        if (i % 10000 == 0) {
            waitForUpdate();
        }
    }
    
    qint64 addTime = timer.elapsed();
    
    // Test querying large dataset
    timer.restart();
    auto historyData = dashboard->getHistoricalData();
    qint64 queryTime = timer.elapsed();
    
    QVERIFY(addTime < 30000); // Should add 100k points in < 30 seconds
    QVERIFY(queryTime < 1000); // Should query in < 1 second
    
    qDebug() << "Large dataset: Add time:" << addTime << "ms, Query time:" << queryTime << "ms";
}

void TestPerformanceDashboard::testLongRunningMonitoring()
{
    // Test stability during long-running monitoring
    dashboard->onStartMonitoring();
    
    QElapsedTimer timer;
    timer.start();
    
    int updateCount = 0;
    
    // Simulate 10 seconds of continuous monitoring
    while (timer.elapsed() < 10000) {
        SystemMetrics metrics = createTestSystemMetrics();
        dashboard->updateSystemMetrics(metrics);
        
        updateCount++;
        
        if (updateCount % 100 == 0) {
            waitForUpdate();
            
            // Verify dashboard is still functional
            QVERIFY(dashboard->isMonitoringActive());
            QVERIFY(dashboard->isInitialized());
        }
        
        QTest::qWait(10); // 10ms between updates
    }
    
    dashboard->onStopMonitoring();
    
    QVERIFY(updateCount > 900); // Should have processed many updates
    QVERIFY(dashboard->isInitialized());
    
    qDebug() << "Long-running test: Processed" << updateCount << "updates in 10 seconds";
}

void TestPerformanceDashboard::testApplicationIntegration()
{
    // Test integration with main application
    // This would test actual integration points in a real scenario
    
    QVERIFY(dashboard->canIntegrateWithApplication());
    
    // Test receiving data from application components
    dashboard->registerApplicationInterface();
    
    // Simulate application events
    dashboard->onApplicationStarted();
    QVERIFY(dashboard->isApplicationConnected());
    
    dashboard->onApplicationStopped();
    QVERIFY(!dashboard->isApplicationConnected());
}

void TestPerformanceDashboard::testWidgetIntegration()
{
    // Test integration with actual widgets
    dashboard->onStartMonitoring();
    
    // Register multiple widgets
    QStringList widgetTypes = {"GridWidget", "ChartWidget", "3DChartWidget", "GridLoggerWidget"};
    
    for (int i = 0; i < widgetTypes.size(); ++i) {
        QString widgetId = QString("widget_%1").arg(i);
        dashboard->onWidgetCreated(widgetId, widgetTypes[i]);
        
        // Simulate widget metrics
        WidgetMetrics metrics = createTestWidgetMetrics();
        metrics.widgetType = widgetTypes[i];
        dashboard->updateWidgetMetrics(widgetId, metrics);
    }
    
    QCOMPARE(dashboard->getWidgetCount(), widgetTypes.size());
    
    // Test widget metrics aggregation
    auto aggregateMetrics = dashboard->getAggregateWidgetMetrics();
    QVERIFY(aggregateMetrics.totalCPUUsage > 0);
    QVERIFY(aggregateMetrics.totalMemoryUsage > 0);
}

void TestPerformanceDashboard::testDataSourceIntegration()
{
    // Test integration with various data sources
    dashboard->registerDataSource("SystemMetrics", dashboard.get());
    dashboard->registerDataSource("NetworkMetrics", dashboard.get());
    dashboard->registerDataSource("ApplicationMetrics", dashboard.get());
    
    QCOMPARE(dashboard->getDataSourceCount(), 3);
    
    // Test data source updates
    dashboard->onStartMonitoring();
    
    // Simulate data from different sources
    dashboard->updateFromDataSource("SystemMetrics", createTestSystemMetrics());
    dashboard->updateFromDataSource("NetworkMetrics", createTestSystemMetrics());
    
    QVERIFY(dashboard->hasDataFromAllSources());
}

void TestPerformanceDashboard::testExportIntegration()
{
    addTestData();
    
    // Test various export formats
    QStringList formats = {"json", "csv", "xml", "html"};
    
    for (const QString& format : formats) {
        QString exportPath = QDir::temp().filePath(QString("export.%1").arg(format));
        
        bool result = dashboard->exportData(exportPath, format);
        QVERIFY(result);
        QVERIFY(QFile::exists(exportPath));
        
        // Cleanup
        QFile::remove(exportPath);
    }
}

void TestPerformanceDashboard::testCustomAlerts()
{
    // Test custom alert definitions
    CustomAlert customAlert;
    customAlert.name = "High Widget CPU";
    customAlert.condition = "widget.cpu > 50 AND widget.type == 'ChartWidget'";
    customAlert.severity = PerformanceAlert::Severity::Warning;
    customAlert.message = "Chart widget using high CPU";
    
    dashboard->addCustomAlert(customAlert);
    
    QCOMPARE(dashboard->getCustomAlertCount(), 1);
    
    // Trigger condition
    dashboard->onWidgetCreated("chart_widget", "ChartWidget");
    
    WidgetMetrics metrics = createTestWidgetMetrics();
    metrics.cpuUsage = 60.0; // Above threshold
    
    QSignalSpy alertSpy(dashboard.get(), &PerformanceDashboard::alertTriggered);
    dashboard->updateWidgetMetrics("chart_widget", metrics);
    
    // Custom alert should be triggered
    QCOMPARE(alertSpy.count(), 1);
}

void TestPerformanceDashboard::testAdvancedVisualization()
{
    addTestData();
    
    // Test advanced visualization features
    dashboard->enableHeatMap(true);
    QVERIFY(dashboard->hasHeatMap());
    
    dashboard->enable3DVisualization(true);
    QVERIFY(dashboard->has3DVisualization());
    
    dashboard->enableRealTimeGraphs(true);
    QVERIFY(dashboard->hasRealTimeGraphs());
    
    // Test visualization updates
    SystemMetrics metrics = createTestSystemMetrics();
    dashboard->updateSystemMetrics(metrics);
    
    waitForUpdate();
    
    // Visualizations should be updated
    QVERIFY(dashboard->isHeatMapUpdated());
    QVERIFY(dashboard->is3DVisualizationUpdated());
}

void TestPerformanceDashboard::testReportGeneration()
{
    addTestData();
    dashboard->onStartMonitoring();
    
    // Let some data accumulate
    QTest::qWait(500);
    
    // Generate performance report
    QString reportPath = QDir::temp().filePath("performance_report.html");
    
    PerformanceReport report;
    report.includeSystemMetrics = true;
    report.includeWidgetMetrics = true;
    report.includePipelineMetrics = true;
    report.includeAlerts = true;
    report.includeCharts = true;
    report.timePeriod = std::chrono::minutes(5);
    
    bool result = dashboard->generateReport(report, reportPath);
    QVERIFY(result);
    QVERIFY(QFile::exists(reportPath));
    
    // Verify report contents
    QFile file(reportPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QString reportContent = file.readAll();
    
    QVERIFY(reportContent.contains("Performance Report"));
    QVERIFY(reportContent.contains("System Metrics"));
    QVERIFY(reportContent.contains("Widget Metrics"));
    
    // Cleanup
    QFile::remove(reportPath);
}

void TestPerformanceDashboard::testDataAnalysis()
{
    dashboard->onStartMonitoring();
    
    // Generate data with patterns
    for (int i = 0; i < 200; ++i) {
        SystemMetrics metrics = createTestSystemMetrics();
        
        // Create patterns in the data
        metrics.cpuUsage = 50.0 + 20.0 * sin(i * 0.1); // Sine wave pattern
        metrics.memoryUsage = 40.0 + i * 0.1; // Linear increase
        metrics.networkBytesIn = 1000 + (i % 50) * 100; // Periodic pattern
        
        dashboard->updateSystemMetrics(metrics);
        waitForUpdate();
    }
    
    // Perform data analysis
    auto analysis = dashboard->performDataAnalysis();
    
    QVERIFY(analysis.hasCPUPattern);
    QVERIFY(analysis.hasMemoryTrend);
    QVERIFY(analysis.hasNetworkSpikes);
    
    // Test specific analysis results
    QVERIFY(analysis.cpuPattern.type == PatternType::Periodic);
    QVERIFY(analysis.memoryTrend.direction == TrendDirection::Increasing);
    QVERIFY(analysis.networkPattern.type == PatternType::Spiky);
}

// Helper method implementations
SystemMetrics TestPerformanceDashboard::createTestSystemMetrics()
{
    SystemMetrics metrics;
    metrics.timestamp = std::chrono::steady_clock::now();
    metrics.cpuUsage = 45.5;
    metrics.memoryUsage = 62.3;
    metrics.diskUsage = 78.9;
    metrics.networkBytesIn = 1024000;
    metrics.networkBytesOut = 512000;
    metrics.networkPacketsIn = 1000;
    metrics.networkPacketsOut = 800;
    metrics.memoryTotal = 16ULL * 1024 * 1024 * 1024; // 16 GB
    metrics.memoryUsed = static_cast<uint64_t>(metrics.memoryTotal * 0.623);
    metrics.memoryAvailable = metrics.memoryTotal - metrics.memoryUsed;
    metrics.cpuCores = 8;
    metrics.cpuFrequency = 3200000000ULL;
    
    return metrics;
}

WidgetMetrics TestPerformanceDashboard::createTestWidgetMetrics()
{
    WidgetMetrics metrics;
    metrics.timestamp = std::chrono::steady_clock::now();
    metrics.widgetId = "test_widget";
    metrics.widgetType = "TestWidget";
    metrics.cpuUsage = 12.5;
    metrics.memoryUsage = 2 * 1024 * 1024; // 2 MB
    metrics.updateRate = 60.0; // 60 FPS
    metrics.processingLatency = 8.3; // 8.3 ms
    metrics.frameDrops = 2;
    metrics.dataPointsProcessed = 1500;
    
    return metrics;
}

PerformanceAlert TestPerformanceDashboard::createTestAlert()
{
    PerformanceAlert alert;
    alert.type = PerformanceAlert::Type::HighCPU;
    alert.severity = PerformanceAlert::Severity::Warning;
    alert.message = "CPU usage is high";
    alert.widgetId = "test_widget";
    alert.timestamp = std::chrono::steady_clock::now();
    alert.acknowledged = false;
    alert.value = 85.5;
    alert.threshold = 80.0;
    
    return alert;
}

void TestPerformanceDashboard::addTestData()
{
    dashboard->onStartMonitoring();
    
    // Add system metrics
    SystemMetrics systemMetrics = createTestSystemMetrics();
    dashboard->updateSystemMetrics(systemMetrics);
    
    // Add widget metrics
    dashboard->onWidgetCreated("test_widget_1", "GridWidget");
    dashboard->onWidgetCreated("test_widget_2", "ChartWidget");
    
    WidgetMetrics widgetMetrics1 = createTestWidgetMetrics();
    widgetMetrics1.widgetId = "test_widget_1";
    dashboard->updateWidgetMetrics("test_widget_1", widgetMetrics1);
    
    WidgetMetrics widgetMetrics2 = createTestWidgetMetrics();
    widgetMetrics2.widgetId = "test_widget_2";
    widgetMetrics2.cpuUsage = 8.2;
    dashboard->updateWidgetMetrics("test_widget_2", widgetMetrics2);
    
    // Add pipeline metrics
    PipelineMetrics pipelineMetrics;
    pipelineMetrics.networkReceiver.packetsPerSecond = 5000;
    pipelineMetrics.parser.packetsPerSecond = 4950;
    pipelineMetrics.widgetDistribution.packetsPerSecond = 4900;
    dashboard->updatePipelineMetrics(pipelineMetrics);
    
    waitForUpdate();
}

void TestPerformanceDashboard::waitForUpdate()
{
    QTest::qWait(50); // Wait for UI updates
}

void TestPerformanceDashboard::simulateSystemLoad()
{
    // Simulate varying system load over time
    for (int i = 0; i < 10; ++i) {
        SystemMetrics metrics = createTestSystemMetrics();
        metrics.cpuUsage = 30.0 + i * 5.0; // Increasing load
        metrics.memoryUsage = 50.0 + sin(i * 0.5) * 20.0; // Varying memory
        
        dashboard->updateSystemMetrics(metrics);
        waitForUpdate();
    }
}

void TestPerformanceDashboard::verifyDashboardState()
{
    QVERIFY(dashboard->isInitialized());
    QVERIFY(dashboard->getTabWidget() != nullptr);
    QVERIFY(dashboard->hasSystemOverviewTab());
    QVERIFY(dashboard->hasWidgetMetricsTab());
    QVERIFY(dashboard->hasPipelineTab());
    QVERIFY(dashboard->hasAlertsTab());
    QVERIFY(dashboard->hasHistoryTab());
}

QTEST_MAIN(TestPerformanceDashboard)
#include "test_performance_dashboard.moc"