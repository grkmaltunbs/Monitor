#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <memory>

// Application includes
#include "../../../../src/ui/windows/performance_dashboard.h"

/**
 * @brief Simple Performance Dashboard Tests
 * 
 * Basic functionality tests for the PerformanceDashboard.
 */
class TestPerformanceDashboardSimple : public QObject
{
    Q_OBJECT

private slots:
    void testDashboardCreation() {
        auto dashboard = std::make_unique<PerformanceDashboard>();
        QVERIFY(dashboard != nullptr);
        
        // Basic state checks
        QVERIFY(!dashboard->isMonitoring());
        QVERIFY(dashboard->getUpdateInterval() > 0);
        QVERIFY(dashboard->getHistorySize() > 0);
    }

    void testMonitoringControls() {
        auto dashboard = std::make_unique<PerformanceDashboard>();
        
        // Test start/stop monitoring
        dashboard->startMonitoring();
        QVERIFY(dashboard->isMonitoring());
        
        dashboard->stopMonitoring();
        QVERIFY(!dashboard->isMonitoring());
    }

    void testBasicConfiguration() {
        auto dashboard = std::make_unique<PerformanceDashboard>();
        
        // Test update interval
        dashboard->setUpdateInterval(1000);
        QCOMPARE(dashboard->getUpdateInterval(), 1000);
        
        // Test history size
        dashboard->setHistorySize(10);
        QCOMPARE(dashboard->getHistorySize(), 10);
    }
};

QTEST_MAIN(TestPerformanceDashboardSimple)
#include "test_performance_dashboard_simple.moc"