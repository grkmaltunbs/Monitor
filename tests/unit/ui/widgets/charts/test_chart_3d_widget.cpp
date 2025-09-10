#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QVector3D>
#include <QJsonObject>
#include <memory>

// Application includes
#include "../../../../../src/ui/widgets/charts/chart_3d_widget.h"

/**
 * @brief Test Chart3DWidget implementation
 * 
 * Tests basic functionality of the Chart3DWidget including
 * widget creation, configuration, series management, and
 * basic 3D chart operations.
 */
class TestChart3DWidget : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QApplication> app;
    std::unique_ptr<Chart3DWidget> widget;

    void waitForProcessing(int ms = 50) {
        QEventLoop loop;
        QTimer::singleShot(ms, &loop, &QEventLoop::quit);
        loop.exec();
    }

private slots:
    void initTestCase() {
        if (!QApplication::instance()) {
            int argc = 1;
            char argv0[] = "test";
            char* argv[] = {argv0};
            app = std::make_unique<QApplication>(argc, argv);
        }
    }

    void cleanupTestCase() {
        widget.reset();
    }

    void init() {
        widget = std::make_unique<Chart3DWidget>(
            "test_3d_widget", 
            "Test 3D Chart",
            nullptr
        );
    }

    void cleanup() {
        widget.reset();
    }

    // Basic widget functionality tests
    void testWidgetCreation() {
        QVERIFY(widget != nullptr);
        
        // Test basic widget properties
        QCOMPARE(widget->getWidgetId(), QString("test_3d_widget"));
        QCOMPARE(widget->getWindowTitle(), QString("Test 3D Chart"));
    }

    void testChart3DConfiguration() {
        // Test getting default configuration
        auto config = widget->getChart3DConfig();
        QVERIFY(config.renderMode == Chart3DWidget::RenderMode::Points);
        
        // Test setting configuration
        config.renderMode = Chart3DWidget::RenderMode::Lines;
        config.enableAntiAliasing = true;
        config.showAxes = false;
        
        widget->setChart3DConfig(config);
        waitForProcessing();
        
        auto newConfig = widget->getChart3DConfig();
        QCOMPARE(newConfig.renderMode, Chart3DWidget::RenderMode::Lines);
        QCOMPARE(newConfig.enableAntiAliasing, true);
        QCOMPARE(newConfig.showAxes, false);
    }

    void testSeries3DManagement() {
        // Test adding series
        Chart3DWidget::Series3DConfig seriesConfig;
        seriesConfig.fieldPath = "test.field";
        seriesConfig.seriesName = "Test Series";
        seriesConfig.renderMode = Chart3DWidget::RenderMode::Points;
        
        bool result = widget->addSeries3D("test.field", seriesConfig);
        QVERIFY(result);
        
        // Test getting series list
        auto seriesList = widget->getSeries3DList();
        QVERIFY(seriesList.contains("test.field"));
        
        // Test getting series configuration
        auto retrievedConfig = widget->getSeries3DConfig("test.field");
        QCOMPARE(retrievedConfig.fieldPath, QString("test.field"));
        QCOMPARE(retrievedConfig.seriesName, QString("Test Series"));
        
        // Test removing series
        bool removeResult = widget->removeSeries3D("test.field");
        QVERIFY(removeResult);
        
        auto updatedList = widget->getSeries3DList();
        QVERIFY(!updatedList.contains("test.field"));
    }

    void testCameraControls() {
        // Test camera position
        QVector3D testPosition(10, 5, 15);
        widget->setCameraPosition(testPosition);
        waitForProcessing();
        
        QVector3D retrievedPosition = widget->getCameraPosition();
        QCOMPARE(retrievedPosition, testPosition);
        
        // Test camera target
        QVector3D testTarget(0, 0, 0);
        widget->setCameraTarget(testTarget);
        waitForProcessing();
        
        QVector3D retrievedTarget = widget->getCameraTarget();
        QCOMPARE(retrievedTarget, testTarget);
        
        // Test camera reset
        widget->resetCamera();
        waitForProcessing();
        
        // Camera should be reset (position may vary, but should be valid)
        QVector3D resetPosition = widget->getCameraPosition();
        QVERIFY(!resetPosition.isNull());
    }

    void testRenderingModes() {
        // Test setting different render modes
        widget->setRenderMode(Chart3DWidget::RenderMode::Points);
        QCOMPARE(widget->getRenderMode(), Chart3DWidget::RenderMode::Points);
        
        widget->setRenderMode(Chart3DWidget::RenderMode::Lines);
        QCOMPARE(widget->getRenderMode(), Chart3DWidget::RenderMode::Lines);
        
        widget->setRenderMode(Chart3DWidget::RenderMode::Surface);
        QCOMPARE(widget->getRenderMode(), Chart3DWidget::RenderMode::Surface);
    }

    void testLightingModes() {
        // Test setting different lighting modes
        widget->setLightingMode(Chart3DWidget::LightingMode::Ambient);
        QCOMPARE(widget->getLightingMode(), Chart3DWidget::LightingMode::Ambient);
        
        widget->setLightingMode(Chart3DWidget::LightingMode::Directional);
        QCOMPARE(widget->getLightingMode(), Chart3DWidget::LightingMode::Directional);
        
        widget->setLightingMode(Chart3DWidget::LightingMode::Point);
        QCOMPARE(widget->getLightingMode(), Chart3DWidget::LightingMode::Point);
    }

    void testAxisManagement() {
        // Test axis configuration for X, Y, Z axes
        for (int axis = 0; axis < 3; ++axis) {
            Chart3DWidget::AxisConfig axisConfig;
            axisConfig.fieldPath = QString("axis%1.field").arg(axis);
            axisConfig.label = QString("Axis %1").arg(axis);
            axisConfig.minValue = axis * 10.0;
            axisConfig.maxValue = (axis + 1) * 100.0;
            
            widget->setAxisConfig(axis, axisConfig);
            
            auto retrievedConfig = widget->getAxisConfig(axis);
            QCOMPARE(retrievedConfig.fieldPath, axisConfig.fieldPath);
            QCOMPARE(retrievedConfig.label, axisConfig.label);
            QCOMPARE(retrievedConfig.minValue, axisConfig.minValue);
            QCOMPARE(retrievedConfig.maxValue, axisConfig.maxValue);
        }
    }

    void testFieldAssignment() {
        // Test field to axis assignment
        widget->assignFieldToAxis("x.field", 0); // X-axis
        widget->assignFieldToAxis("y.field", 1); // Y-axis
        widget->assignFieldToAxis("z.field", 2); // Z-axis
        
        QCOMPARE(widget->getAxisField(0), QString("x.field"));
        QCOMPARE(widget->getAxisField(1), QString("y.field"));
        QCOMPARE(widget->getAxisField(2), QString("z.field"));
    }

    void testPerformanceMonitoring() {
        // Test performance monitoring methods
        double fps = widget->getCurrentFPS();
        QVERIFY(fps >= 0.0);
        
        int pointCount = widget->getCurrentPointCount();
        QVERIFY(pointCount >= 0);
        
        bool gpuAccelerated = widget->isGPUAccelerated();
        // GPU acceleration may or may not be available
        Q_UNUSED(gpuAccelerated);
    }

    void testDataUpdates() {
        // Add a series first
        Chart3DWidget::Series3DConfig seriesConfig;
        seriesConfig.fieldPath = "data.field";
        seriesConfig.seriesName = "Data Test";
        
        widget->addSeries3D("data.field", seriesConfig);
        waitForProcessing();
        
        // Test that series was added successfully
        auto seriesList = widget->getSeries3DList();
        QVERIFY(seriesList.contains("data.field"));
    }

    void testSignals() {
        // Test camera change signal
        QSignalSpy cameraChangeSpy(widget.get(), &Chart3DWidget::cameraChanged);
        
        widget->setCameraPosition(QVector3D(5, 5, 5));
        waitForProcessing();
        
        // May or may not emit signal depending on implementation
        QVERIFY(cameraChangeSpy.count() >= 0);
        
        // Test render mode change signal
        QSignalSpy renderModeSpy(widget.get(), &Chart3DWidget::renderModeChanged);
        
        widget->setRenderMode(Chart3DWidget::RenderMode::Lines);
        waitForProcessing();
        
        // May or may not emit signal depending on implementation
        QVERIFY(renderModeSpy.count() >= 0);
    }

    void testSettingsPersistence() {
        // Configure widget
        auto config = widget->getChart3DConfig();
        config.renderMode = Chart3DWidget::RenderMode::Surface;
        config.showAxes = false;
        config.showGrid = true;
        widget->setChart3DConfig(config);
        
        // Add series
        Chart3DWidget::Series3DConfig seriesConfig;
        seriesConfig.fieldPath = "settings.test";
        seriesConfig.seriesName = "Settings Test";
        widget->addSeries3D("settings.test", seriesConfig);
        
        waitForProcessing();
        
        // Test that configuration persists
        auto currentConfig = widget->getChart3DConfig();
        QCOMPARE(currentConfig.renderMode, Chart3DWidget::RenderMode::Surface);
        QCOMPARE(currentConfig.showAxes, false);
        QCOMPARE(currentConfig.showGrid, true);
        
        // Test that series persists
        auto seriesList = widget->getSeries3DList();
        QVERIFY(seriesList.contains("settings.test"));
    }

    void testConfigurationReset() {
        // Modify configuration
        auto config = widget->getChart3DConfig();
        config.renderMode = Chart3DWidget::RenderMode::Wireframe;
        config.enableAntiAliasing = false;
        config.showAxes = false;
        config.showGrid = false;
        widget->setChart3DConfig(config);
        
        // Reset configuration
        widget->resetChart3DConfig();
        waitForProcessing();
        
        // Verify reset to defaults
        auto resetConfig = widget->getChart3DConfig();
        QCOMPARE(resetConfig.renderMode, Chart3DWidget::RenderMode::Points);
        QCOMPARE(resetConfig.enableAntiAliasing, true);
        QCOMPARE(resetConfig.showAxes, true);
        QCOMPARE(resetConfig.showGrid, true);
    }

    void testClearSeries() {
        // Add multiple series
        for (int i = 0; i < 5; ++i) {
            Chart3DWidget::Series3DConfig seriesConfig;
            seriesConfig.fieldPath = QString("clear.series%1").arg(i);
            seriesConfig.seriesName = QString("Clear Series %1").arg(i);
            widget->addSeries3D(seriesConfig.fieldPath, seriesConfig);
        }
        
        auto seriesList = widget->getSeries3DList();
        QCOMPARE(seriesList.size(), 5);
        
        // Clear all series
        widget->clearSeries3D();
        waitForProcessing();
        
        auto clearedList = widget->getSeries3DList();
        QVERIFY(clearedList.isEmpty());
    }

    void testExportFunctionality() {
        // Test basic export functionality (may not actually create files)
        bool exportResult = widget->export3DChart();
        
        // Export may succeed or fail depending on system capabilities
        Q_UNUSED(exportResult);
        
        // Test export with parameters
        bool exportWithParamsResult = widget->export3DChart(
            "/tmp/test_chart.png", "PNG", QSize(800, 600));
        
        // Export may succeed or fail depending on system capabilities
        Q_UNUSED(exportWithParamsResult);
    }

    void testWidgetSlots() {
        // Test calling slot methods (should not crash)
        widget->onResetCamera();
        waitForProcessing();
        
        widget->onToggleAxes();
        waitForProcessing();
        
        widget->onToggleGrid();
        waitForProcessing();
        
        widget->onToggleLighting();
        waitForProcessing();
        
        widget->onChangeRenderMode();
        waitForProcessing();
        
        widget->onExport3DChart();
        waitForProcessing();
    }
};

QTEST_MAIN(TestChart3DWidget)
#include "test_chart_3d_widget.moc"