#include <QtTest/QtTest>
#include <QApplication>
#include <memory>

// Application includes
#include "../../../../../src/ui/widgets/charts/chart_3d_widget.h"

/**
 * @brief Minimal Chart3DWidget Tests
 * 
 * Basic tests for Chart3DWidget creation and instantiation.
 */
class TestChart3DWidgetMinimal : public QObject
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

    void testWidgetCreation() {
        auto widget = std::make_unique<Chart3DWidget>(
            "test_3d_widget", 
            "Test 3D Chart"
        );
        QVERIFY(widget != nullptr);
        
        // Basic widget properties
        QCOMPARE(widget->getWidgetId(), QString("test_3d_widget"));
        QCOMPARE(widget->getWindowTitle(), QString("Test 3D Chart"));
    }

    void testChart3DConfiguration() {
        auto widget = std::make_unique<Chart3DWidget>(
            "config_test", 
            "Configuration Test"
        );
        
        // Test getting default configuration
        auto config = widget->getChart3DConfig();
        QVERIFY(config.renderMode == Chart3DWidget::RenderMode::Points);
        
        // Test setting configuration
        config.renderMode = Chart3DWidget::RenderMode::Lines;
        config.enableAntiAliasing = true;
        widget->setChart3DConfig(config);
        
        auto newConfig = widget->getChart3DConfig();
        QCOMPARE(newConfig.renderMode, Chart3DWidget::RenderMode::Lines);
        QCOMPARE(newConfig.enableAntiAliasing, true);
    }

    void testBasicOperations() {
        auto widget = std::make_unique<Chart3DWidget>(
            "ops_test", 
            "Operations Test"
        );
        
        // Test that basic operations don't crash
        widget->resetChart3DConfig();
        widget->clearSeries3D();
        
        // Widget should still be valid
        QVERIFY(widget != nullptr);
        QCOMPARE(widget->getWidgetId(), QString("ops_test"));
    }
};

QTEST_MAIN(TestChart3DWidgetMinimal)
#include "test_chart_3d_widget_minimal.moc"