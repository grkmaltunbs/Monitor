#include <QtTest/QtTest>
#include <QApplication>
#include <memory>

// Application includes
#include "../../../src/ui/widgets/charts/chart_3d_widget.h"

/**
 * @brief Simple Qt3D Integration Tests
 * 
 * Basic integration tests for Qt3D framework with Chart3DWidget.
 */
class TestQt3DIntegrationSimple : public QObject
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

    void testQt3DWidgetCreation() {
        auto widget = std::make_unique<Chart3DWidget>(
            "qt3d_test", "Qt3D Test Widget");
        QVERIFY(widget != nullptr);
        
        // Basic widget properties
        QCOMPARE(widget->getWidgetId(), QString("qt3d_test"));
    }

    void testQt3DConfiguration() {
        auto widget = std::make_unique<Chart3DWidget>(
            "qt3d_config", "Qt3D Config Test");
        
        // Test getting configuration
        auto config = widget->getChart3DConfig();
        QVERIFY(config.renderMode == Chart3DWidget::RenderMode::Points);
        
        // Test setting configuration
        config.renderMode = Chart3DWidget::RenderMode::Lines;
        widget->setChart3DConfig(config);
        
        auto newConfig = widget->getChart3DConfig();
        QCOMPARE(newConfig.renderMode, Chart3DWidget::RenderMode::Lines);
    }

    void testQt3DBasicOperations() {
        auto widget = std::make_unique<Chart3DWidget>(
            "qt3d_ops", "Qt3D Operations Test");
        
        // Test basic operations
        widget->resetCamera();
        widget->setRenderMode(Chart3DWidget::RenderMode::Points);
        widget->setLightingMode(Chart3DWidget::LightingMode::Directional);
        
        // Should not crash
        QVERIFY(widget != nullptr);
    }
};

QTEST_MAIN(TestQt3DIntegrationSimple)
#include "test_qt3d_integration_simple.moc"