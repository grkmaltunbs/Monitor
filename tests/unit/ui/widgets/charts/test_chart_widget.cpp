#include <QtTest/QtTest>
#include <QApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <memory>

#include "ui/widgets/charts/chart_widget.h"
#include "ui/widgets/charts/chart_common.h"

/**
 * @brief Test implementation of ChartWidget for testing base functionality
 */
class TestChartWidgetImpl : public ChartWidget
{
    Q_OBJECT

public:
    explicit TestChartWidgetImpl(const QString& widgetId, QWidget* parent = nullptr)
        : ChartWidget(widgetId, "Test Chart", parent) {}

protected:
    void createChart() override {
        m_chart = new QChart();
        m_chart->setTitle("Test Chart Implementation");
    }

    void updateSeriesData() override {
        updateDataCallCount++;
        lastUpdateTime = std::chrono::steady_clock::now();
    }

    void configureSeries(const QString& fieldPath, const SeriesConfig& config) override {
        Q_UNUSED(fieldPath);
        Q_UNUSED(config);
        configureSeriesCallCount++;
    }

    QAbstractSeries* createSeriesForField(const QString& fieldPath, const SeriesConfig& config) override {
        Q_UNUSED(fieldPath);
        Q_UNUSED(config);
        createSeriesCallCount++;
        return nullptr; // Mock implementation
    }

    void removeSeriesForField(const QString& fieldPath) override {
        Q_UNUSED(fieldPath);
        removeSeriesCallCount++;
    }

    void updateFieldDisplay(const QString& fieldPath, const QVariant& value) override {
        Q_UNUSED(fieldPath);
        Q_UNUSED(value);
        updateFieldDisplayCallCount++;
    }

    void clearFieldDisplay(const QString& fieldPath) override {
        Q_UNUSED(fieldPath);
        clearFieldDisplayCallCount++;
    }

    void refreshAllDisplays() override {
        refreshAllDisplaysCallCount++;
    }

public:
    // Test counters
    int updateDataCallCount = 0;
    int configureSeriesCallCount = 0;
    int createSeriesCallCount = 0;
    int removeSeriesCallCount = 0;
    int updateFieldDisplayCallCount = 0;
    int clearFieldDisplayCallCount = 0;
    int refreshAllDisplaysCallCount = 0;
    std::chrono::steady_clock::time_point lastUpdateTime;
};

class TestChartWidget : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testConstruction();
    void testChartCreation();
    void testWidgetIdentity();
    
    // Configuration tests
    void testChartConfiguration();
    void testThemeConfiguration();
    void testPerformanceConfiguration();
    
    // Series management tests
    void testSeriesManagement();
    void testSeriesConfiguration();
    void testSeriesRemoval();
    
    // Settings persistence tests
    void testSettingsSave();
    void testSettingsRestore();
    void testSettingsRoundTrip();
    
    // Update and performance tests
    void testUpdateThrottling();
    void testPerformanceMonitoring();
    void testFPSTracking();
    
    // Export functionality tests
    void testChartExport();
    void testExportFormats();
    
    // Interaction tests
    void testZoomPanFunctionality();
    void testAxisManagement();
    void testAutoScaling();
    
    // Error handling tests
    void testInvalidOperations();
    void testNullPointerSafety();

private:
    QApplication* app;
    std::unique_ptr<TestChartWidgetImpl> widget;
};

void TestChartWidget::initTestCase()
{
    // Initialize Qt application if not already done
    if (!QApplication::instance()) {
        static int argc = 1;
        static char* argv[] = {const_cast<char*>("test")};
        app = new QApplication(argc, argv);
    }
}

void TestChartWidget::cleanupTestCase()
{
    // Application cleanup is handled by Qt
}

void TestChartWidget::init()
{
    // Create fresh widget for each test
    widget = std::make_unique<TestChartWidgetImpl>("test_chart_widget");
    widget->initializeWidget();
}

void TestChartWidget::cleanup()
{
    widget.reset();
}

void TestChartWidget::testConstruction()
{
    // Test basic construction
    QVERIFY(widget != nullptr);
    QCOMPARE(widget->getWidgetId(), QString("test_chart_widget"));
    QCOMPARE(widget->getWindowTitle(), QString("Test Chart"));
    
    // Test that chart is created
    QVERIFY(widget->getChart() != nullptr);
    QVERIFY(widget->getChartView() != nullptr);
    
    // Test initial state
    QVERIFY(widget->isAutoScale());
    QVERIFY(!widget->isPerformanceOptimized());
    QCOMPARE(widget->getCurrentPointCount(), 0);
}

void TestChartWidget::testChartCreation()
{
    // Verify chart was created correctly
    QChart* chart = widget->getChart();
    QVERIFY(chart != nullptr);
    QCOMPARE(chart->title(), QString("Test Chart Implementation"));
    
    // Verify chart view
    QChartView* chartView = widget->getChartView();
    QVERIFY(chartView != nullptr);
    QCOMPARE(chartView->chart(), chart);
    QVERIFY(chartView->renderHints().testFlag(QPainter::Antialiasing));
}

void TestChartWidget::testWidgetIdentity()
{
    // Test widget ID and title
    QCOMPARE(widget->getWidgetId(), QString("test_chart_widget"));
    QCOMPARE(widget->getWindowTitle(), QString("Test Chart"));
    
    // Test title change
    widget->setWindowTitle("New Chart Title");
    QCOMPARE(widget->getWindowTitle(), QString("New Chart Title"));
}

void TestChartWidget::testChartConfiguration()
{
    // Test default configuration
    ChartWidget::ChartConfig config = widget->getChartConfig();
    QCOMPARE(config.theme, Monitor::Charts::ChartTheme::Light);
    QVERIFY(config.showLegend);
    QVERIFY(config.showGrid);
    QVERIFY(config.enableAnimations);
    
    // Test configuration changes
    config.theme = Monitor::Charts::ChartTheme::Dark;
    config.showLegend = false;
    config.showGrid = false;
    config.title = "Modified Chart";
    
    widget->setChartConfig(config);
    ChartWidget::ChartConfig newConfig = widget->getChartConfig();
    
    QCOMPARE(newConfig.theme, Monitor::Charts::ChartTheme::Dark);
    QVERIFY(!newConfig.showLegend);
    QVERIFY(!newConfig.showGrid);
    QCOMPARE(newConfig.title, QString("Modified Chart"));
}

void TestChartWidget::testThemeConfiguration()
{
    // Test theme application
    Monitor::Charts::ChartThemeConfig lightTheme = 
        Monitor::Charts::ChartThemeConfig::getTheme(Monitor::Charts::ChartTheme::Light);
    Monitor::Charts::ChartThemeConfig darkTheme = 
        Monitor::Charts::ChartThemeConfig::getTheme(Monitor::Charts::ChartTheme::Dark);
    
    QVERIFY(lightTheme.backgroundColor != darkTheme.backgroundColor);
    QVERIFY(lightTheme.axisLabelColor != darkTheme.axisLabelColor);
    
    // Test theme application to chart
    QChart* chart = widget->getChart();
    lightTheme.applyToChart(chart);
    
    // Verify theme was applied (basic checks)
    QVERIFY(chart != nullptr);
}

void TestChartWidget::testPerformanceConfiguration()
{
    // Test performance level configurations
    Monitor::Charts::PerformanceConfig highPerf = 
        Monitor::Charts::PerformanceConfig::getConfig(Monitor::Charts::PerformanceLevel::High);
    Monitor::Charts::PerformanceConfig fastPerf = 
        Monitor::Charts::PerformanceConfig::getConfig(Monitor::Charts::PerformanceLevel::Fast);
    
    QVERIFY(highPerf.maxDataPoints > fastPerf.maxDataPoints);
    QVERIFY(highPerf.enableAnimations && !fastPerf.enableAnimations);
    QVERIFY(highPerf.updateThrottleMs <= fastPerf.updateThrottleMs);
}

void TestChartWidget::testSeriesManagement()
{
    // Test series addition
    ChartWidget::SeriesConfig config;
    config.fieldPath = "test.field1";
    config.seriesName = "Test Series";
    config.color = QColor(Qt::red);
    
    bool result = widget->addSeries("test.field1", config);
    QVERIFY(result);
    QCOMPARE(widget->createSeriesCallCount, 1);
    
    // Test series listing
    QStringList seriesList = widget->getSeriesList();
    QCOMPARE(seriesList.size(), 1);
    QVERIFY(seriesList.contains("test.field1"));
    
    // Test series configuration retrieval
    ChartWidget::SeriesConfig retrievedConfig = widget->getSeriesConfig("test.field1");
    QCOMPARE(retrievedConfig.fieldPath, config.fieldPath);
    QCOMPARE(retrievedConfig.seriesName, config.seriesName);
    QCOMPARE(retrievedConfig.color, config.color);
}

void TestChartWidget::testSeriesConfiguration()
{
    // Add a series first
    ChartWidget::SeriesConfig config;
    config.fieldPath = "test.field1";
    config.seriesName = "Original Series";
    config.color = QColor(Qt::blue);
    widget->addSeries("test.field1", config);
    
    // Modify configuration
    config.seriesName = "Modified Series";
    config.color = QColor(Qt::green);
    config.visible = false;
    
    widget->setSeriesConfig("test.field1", config);
    QCOMPARE(widget->configureSeriesCallCount, 2); // Once for add, once for set
    
    // Verify configuration was applied
    ChartWidget::SeriesConfig newConfig = widget->getSeriesConfig("test.field1");
    QCOMPARE(newConfig.seriesName, QString("Modified Series"));
    QCOMPARE(newConfig.color, QColor(Qt::green));
    QVERIFY(!newConfig.visible);
}

void TestChartWidget::testSeriesRemoval()
{
    // Add multiple series
    for (int i = 0; i < 3; ++i) {
        ChartWidget::SeriesConfig config;
        config.fieldPath = QString("test.field%1").arg(i);
        config.seriesName = QString("Series %1").arg(i);
        widget->addSeries(config.fieldPath, config);
    }
    
    QCOMPARE(widget->getSeriesList().size(), 3);
    
    // Remove one series
    bool result = widget->removeSeries("test.field1");
    QVERIFY(result);
    QCOMPARE(widget->removeSeriesCallCount, 1);
    QCOMPARE(widget->getSeriesList().size(), 2);
    QVERIFY(!widget->getSeriesList().contains("test.field1"));
    
    // Clear all series
    widget->clearSeries();
    QCOMPARE(widget->getSeriesList().size(), 0);
}

void TestChartWidget::testSettingsSave()
{
    // Configure widget
    ChartWidget::ChartConfig config = widget->getChartConfig();
    config.title = "Test Chart Settings";
    config.theme = Monitor::Charts::ChartTheme::Dark;
    config.showLegend = false;
    config.maxDataPoints = 5000;
    widget->setChartConfig(config);
    
    // Add some series
    ChartWidget::SeriesConfig seriesConfig;
    seriesConfig.fieldPath = "test.field1";
    seriesConfig.seriesName = "Test Series";
    seriesConfig.color = QColor(255, 0, 0);
    widget->addSeries("test.field1", seriesConfig);
    
    // Save settings
    QJsonObject settings = widget->saveSettings();
    
    // Verify basic structure
    QVERIFY(settings.contains("chartConfig"));
    QVERIFY(settings.contains("series"));
    
    // Verify chart config
    QJsonObject chartConfig = settings["chartConfig"].toObject();
    QCOMPARE(chartConfig["title"].toString(), QString("Test Chart Settings"));
    QCOMPARE(chartConfig["theme"].toInt(), static_cast<int>(Monitor::Charts::ChartTheme::Dark));
    QVERIFY(!chartConfig["showLegend"].toBool());
    QCOMPARE(chartConfig["maxDataPoints"].toInt(), 5000);
    
    // Verify series config
    QJsonArray seriesArray = settings["series"].toArray();
    QCOMPARE(seriesArray.size(), 1);
    QJsonObject seriesObj = seriesArray[0].toObject();
    QCOMPARE(seriesObj["fieldPath"].toString(), QString("test.field1"));
    QCOMPARE(seriesObj["seriesName"].toString(), QString("Test Series"));
    QCOMPARE(seriesObj["color"].toString(), QString("#ff0000"));
}

void TestChartWidget::testSettingsRestore()
{
    // Create settings JSON
    QJsonObject settings;
    
    QJsonObject chartConfig;
    chartConfig["title"] = "Restored Chart";
    chartConfig["theme"] = static_cast<int>(Monitor::Charts::ChartTheme::BlueCerulean);
    chartConfig["showLegend"] = true;
    chartConfig["showGrid"] = false;
    chartConfig["maxDataPoints"] = 8000;
    settings["chartConfig"] = chartConfig;
    
    QJsonArray seriesArray;
    QJsonObject seriesObj;
    seriesObj["fieldPath"] = "restored.field";
    seriesObj["seriesName"] = "Restored Series";
    seriesObj["color"] = "#00ff00";
    seriesObj["visible"] = true;
    seriesArray.append(seriesObj);
    settings["series"] = seriesArray;
    
    settings["autoScale"] = false;
    
    // Restore settings
    bool result = widget->restoreSettings(settings);
    QVERIFY(result);
    
    // Verify restoration
    ChartWidget::ChartConfig config = widget->getChartConfig();
    QCOMPARE(config.title, QString("Restored Chart"));
    QCOMPARE(config.theme, Monitor::Charts::ChartTheme::BlueCerulean);
    QVERIFY(config.showLegend);
    QVERIFY(!config.showGrid);
    QCOMPARE(config.maxDataPoints, 8000);
    
    QVERIFY(!widget->isAutoScale());
    
    // Check if series was recreated
    QStringList seriesList = widget->getSeriesList();
    QCOMPARE(seriesList.size(), 1);
    QVERIFY(seriesList.contains("restored.field"));
}

void TestChartWidget::testSettingsRoundTrip()
{
    // Configure widget with complex settings
    ChartWidget::ChartConfig config = widget->getChartConfig();
    config.title = "Round Trip Test";
    config.theme = Monitor::Charts::ChartTheme::Dark;
    config.showLegend = false;
    config.showGrid = true;
    config.maxDataPoints = 12000;
    config.enableAnimations = false;
    widget->setChartConfig(config);
    
    widget->setAutoScale(false);
    
    // Add multiple series
    for (int i = 0; i < 3; ++i) {
        ChartWidget::SeriesConfig seriesConfig;
        seriesConfig.fieldPath = QString("roundtrip.field%1").arg(i);
        seriesConfig.seriesName = QString("RoundTrip Series %1").arg(i);
        seriesConfig.color = QColor(i * 80, 100, 200 - i * 50);
        seriesConfig.visible = (i % 2 == 0);
        widget->addSeries(seriesConfig.fieldPath, seriesConfig);
    }
    
    // Save settings
    QJsonObject savedSettings = widget->saveSettings();
    
    // Create new widget and restore
    auto newWidget = std::make_unique<TestChartWidgetImpl>("roundtrip_test");
    newWidget->initializeWidget();
    
    bool result = newWidget->restoreSettings(savedSettings);
    QVERIFY(result);
    
    // Compare configurations
    ChartWidget::ChartConfig newConfig = newWidget->getChartConfig();
    QCOMPARE(newConfig.title, config.title);
    QCOMPARE(newConfig.theme, config.theme);
    QCOMPARE(newConfig.showLegend, config.showLegend);
    QCOMPARE(newConfig.showGrid, config.showGrid);
    QCOMPARE(newConfig.maxDataPoints, config.maxDataPoints);
    QCOMPARE(newConfig.enableAnimations, config.enableAnimations);
    
    QCOMPARE(newWidget->isAutoScale(), widget->isAutoScale());
    
    // Compare series
    QStringList originalSeries = widget->getSeriesList();
    QStringList restoredSeries = newWidget->getSeriesList();
    QCOMPARE(restoredSeries.size(), originalSeries.size());
    
    for (const QString& fieldPath : originalSeries) {
        QVERIFY(restoredSeries.contains(fieldPath));
        
        ChartWidget::SeriesConfig originalSeriesConfig = widget->getSeriesConfig(fieldPath);
        ChartWidget::SeriesConfig restoredSeriesConfig = newWidget->getSeriesConfig(fieldPath);
        
        QCOMPARE(restoredSeriesConfig.fieldPath, originalSeriesConfig.fieldPath);
        QCOMPARE(restoredSeriesConfig.seriesName, originalSeriesConfig.seriesName);
        QCOMPARE(restoredSeriesConfig.color, originalSeriesConfig.color);
        QCOMPARE(restoredSeriesConfig.visible, originalSeriesConfig.visible);
    }
}

void TestChartWidget::testUpdateThrottling()
{
    // Test update rate limiting
    ChartWidget::ChartConfig config = widget->getChartConfig();
    config.updateMode = ChartWidget::UpdateMode::Buffered;
    widget->setChartConfig(config);
    
    // Reset counters
    widget->updateDataCallCount = 0;
    
    // Trigger multiple updates rapidly
    for (int i = 0; i < 10; ++i) {
        widget->updateDisplay();
    }
    
    // With immediate mode, should have 10 updates
    config.updateMode = ChartWidget::UpdateMode::Immediate;
    widget->setChartConfig(config);
    
    int initialCount = widget->updateDataCallCount;
    for (int i = 0; i < 5; ++i) {
        widget->updateDisplay();
    }
    
    QCOMPARE(widget->updateDataCallCount, initialCount + 5);
}

void TestChartWidget::testPerformanceMonitoring()
{
    // Test FPS tracking
    double initialFPS = widget->getCurrentFPS();
    QCOMPARE(initialFPS, 0.0); // Should be 0 initially
    
    // Test point count tracking
    QCOMPARE(widget->getCurrentPointCount(), 0);
    
    // Test performance optimization flag
    QVERIFY(!widget->isPerformanceOptimized());
}

void TestChartWidget::testFPSTracking()
{
    // This is a basic test - FPS tracking requires time-based updates
    widget->updateDisplay();
    
    // FPS should still be 0 for single update
    QCOMPARE(widget->getCurrentFPS(), 0.0);
}

void TestChartWidget::testChartExport()
{
    // Test export functionality (basic validation)
    QString testPath = "/tmp/test_chart_export.png";
    
    // This would require actual file system access
    // For unit test, we just verify the method exists and handles invalid paths gracefully
    bool result = widget->exportChart(""); // Empty path should return false
    QVERIFY(!result);
}

void TestChartWidget::testExportFormats()
{
    // Test export format detection
    QCOMPARE(Monitor::Charts::ChartExporter::getFileExtensions(Monitor::Charts::ExportFormat::PNG), 
             QStringList({"png"}));
    QCOMPARE(Monitor::Charts::ChartExporter::getFileExtensions(Monitor::Charts::ExportFormat::SVG), 
             QStringList({"svg"}));
    QCOMPARE(Monitor::Charts::ChartExporter::getFileExtensions(Monitor::Charts::ExportFormat::JPEG), 
             QStringList({"jpg", "jpeg"}));
    
    // Test file filter
    QString filter = Monitor::Charts::ChartExporter::getFileFilter();
    QVERIFY(filter.contains("PNG"));
    QVERIFY(filter.contains("SVG"));
    QVERIFY(filter.contains("JPEG"));
}

void TestChartWidget::testZoomPanFunctionality()
{
    // Test zoom reset
    widget->resetZoom();
    
    // Basic functionality test - more detailed testing would require UI interaction
    QVERIFY(true); // Placeholder - actual zoom/pan testing requires more complex setup
}

void TestChartWidget::testAxisManagement()
{
    // Test auto-scaling
    QVERIFY(widget->isAutoScale());
    
    widget->setAutoScale(false);
    QVERIFY(!widget->isAutoScale());
    
    // Test axis range (basic test)
    widget->setAxisRange(Qt::Horizontal, 0, 100);
    widget->setAxisRange(Qt::Vertical, -50, 50);
    
    // Basic validation - actual axis testing would require chart with axes
    QVERIFY(true);
}

void TestChartWidget::testAutoScaling()
{
    // Test auto-scaling toggle
    bool initialAutoScale = widget->isAutoScale();
    widget->setAutoScale(!initialAutoScale);
    QCOMPARE(widget->isAutoScale(), !initialAutoScale);
    
    widget->setAutoScale(initialAutoScale);
    QCOMPARE(widget->isAutoScale(), initialAutoScale);
}

void TestChartWidget::testInvalidOperations()
{
    // Test operations on non-existent series
    bool result = widget->removeSeries("non.existent.field");
    QVERIFY(!result);
    
    ChartWidget::SeriesConfig emptyConfig = widget->getSeriesConfig("non.existent.field");
    QVERIFY(emptyConfig.fieldPath.isEmpty());
    
    // Test null pointer safety is implicitly tested by not crashing
    QVERIFY(true);
}

void TestChartWidget::testNullPointerSafety()
{
    // Test that widget handles null chart gracefully
    // This is primarily tested by not crashing during operations
    
    // Most null pointer safety is handled internally
    // External API should not accept null pointers in normal usage
    QVERIFY(widget->getChart() != nullptr);
    QVERIFY(widget->getChartView() != nullptr);
}

QTEST_MAIN(TestChartWidget)
#include "test_chart_widget.moc"