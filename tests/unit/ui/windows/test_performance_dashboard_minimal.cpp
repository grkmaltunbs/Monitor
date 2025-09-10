#include <QtTest/QtTest>
#include <QApplication>
#include <memory>

// Application includes
#include "../../../../src/ui/windows/performance_dashboard.h"

/**
 * @brief Minimal Performance Dashboard Tests
 * 
 * Basic tests for PerformanceDashboard creation and instantiation.
 */
class TestPerformanceDashboardMinimal : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        if (!QApplication::instance()) {
            int argc = 1;
            char argv0[] = "test";
            char* argv[] = {argv0};
            static QApplication app(argc, argv);
        }
    }

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

    void testBasicOperations() {
        auto dashboard = std::make_unique<PerformanceDashboard>();
        
        // Test that basic operations don't crash
        dashboard->clearAlerts();
        dashboard->resetThresholds();
        
        // Dashboard should still be valid
        QVERIFY(dashboard != nullptr);
        QVERIFY(!dashboard->isMonitoring());
    }
};

QTEST_MAIN(TestPerformanceDashboardMinimal)
#include "test_performance_dashboard_minimal.moc"