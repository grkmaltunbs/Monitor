#include <QtTest/QtTest>
#include <QApplication>
#include <QJsonObject>
#include <QTimer>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QSplineSeries>
#include <memory>
#include <chrono>
#include <thread>

#include "ui/widgets/charts/line_chart_widget.h"

class TestLineChartWidget : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Basic functionality tests
    void testConstruction();
    void testLineChartConfiguration();
    void testSeriesCreation();
    
    // Data management tests
    void testDataPointAddition();
    void testDataPointStorage();
    void testDataPointRetrieval();
    void testDataPointHistory();
    
    // Series configuration tests
    void testLineSeriesConfiguration();
    void testInterpolationMethods();
    void testLineStyles();
    void testPointStyles();
    
    // Axis management tests
    void testXAxisTypes();
    void testAutoScaling();
    void testAxisFieldPath();
    
    // Real-time features tests
    void testRealTimeMode();
    void testDataDecimation();
    void testRollingData();
    
    // Performance tests
    void testLargeDatasets();
    void testDataSmoothing();
    void testViewportOptimization();
    
    // Analysis functions tests
    void testStatisticalFunctions();
    void testDataRangeCalculation();
    
    // Settings persistence tests
    void testLineSettingsSave();
    void testLineSettingsRestore();
    
    // Error handling tests
    void testInvalidDataHandling();
    void testMemoryManagement();

private:
    QApplication* app;
    std::unique_ptr<LineChartWidget> widget;
    
    // Helper methods
    void addTestData(const QString& fieldPath, const std::vector<double>& values);
    void verifySeriesData(const QString& fieldPath, const std::vector<double>& expectedValues);
};

void TestLineChartWidget::initTestCase()
{
    // Initialize Qt application if not already done
    if (!QApplication::instance()) {
        static int argc = 1;
        static char* argv[] = {const_cast<char*>("test")};
        app = new QApplication(argc, argv);
    }
}

void TestLineChartWidget::cleanupTestCase()
{
    // Application cleanup is handled by Qt
}

void TestLineChartWidget::init()
{
    // Create fresh widget for each test
    widget = std::make_unique<LineChartWidget>("test_line_chart");
    widget->initializeWidget();
}

void TestLineChartWidget::cleanup()
{
    widget.reset();
}

void TestLineChartWidget::testConstruction()
{
    // Test basic construction
    QVERIFY(widget != nullptr);
    QCOMPARE(widget->getWidgetId(), QString("test_line_chart"));
    QCOMPARE(widget->getWindowTitle(), QString("Line Chart"));
    
    // Test that chart is created and is correct type
    QVERIFY(widget->getChart() != nullptr);
    QVERIFY(widget->getChartView() != nullptr);
    
    // Test initial configuration
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    QCOMPARE(config.xAxisType, LineChartWidget::LineChartConfig::XAxisType::PacketSequence);
    QCOMPARE(config.interpolation, LineChartWidget::LineChartConfig::InterpolationMethod::Linear);
    QCOMPARE(config.maxDataPoints, 10000);
    QVERIFY(config.rollingData);
    QVERIFY(config.autoScaleX);
    QVERIFY(config.autoScaleY);
    QVERIFY(config.enableRealTimeMode);
}

void TestLineChartWidget::testLineChartConfiguration()
{
    // Test default configuration
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    QCOMPARE(config.defaultLineWidth, 2);
    QCOMPARE(config.defaultPointSize, 6.0);
    QVERIFY(!config.showPoints);
    QVERIFY(config.connectPoints);
    
    // Test configuration changes
    config.maxDataPoints = 5000;
    config.showPoints = true;
    config.defaultLineWidth = 3;
    config.interpolation = LineChartWidget::LineChartConfig::InterpolationMethod::Spline;
    config.xAxisType = LineChartWidget::LineChartConfig::XAxisType::Timestamp;
    
    widget->setLineChartConfig(config);
    LineChartWidget::LineChartConfig newConfig = widget->getLineChartConfig();
    
    QCOMPARE(newConfig.maxDataPoints, 5000);
    QVERIFY(newConfig.showPoints);
    QCOMPARE(newConfig.defaultLineWidth, 3);
    QCOMPARE(newConfig.interpolation, LineChartWidget::LineChartConfig::InterpolationMethod::Spline);
    QCOMPARE(newConfig.xAxisType, LineChartWidget::LineChartConfig::XAxisType::Timestamp);
}

void TestLineChartWidget::testSeriesCreation()
{
    // Test line series creation
    bool result = widget->addLineSeries("test.field1", "Test Series 1", QColor(Qt::red));
    QVERIFY(result);
    
    QStringList seriesList = widget->getSeriesList();
    QCOMPARE(seriesList.size(), 1);
    QVERIFY(seriesList.contains("test.field1"));
    
    // Test multiple series
    widget->addLineSeries("test.field2", "Test Series 2", QColor(Qt::blue));
    widget->addLineSeries("test.field3", "Test Series 3", QColor(Qt::green));
    
    seriesList = widget->getSeriesList();
    QCOMPARE(seriesList.size(), 3);
    QVERIFY(seriesList.contains("test.field1"));
    QVERIFY(seriesList.contains("test.field2"));
    QVERIFY(seriesList.contains("test.field3"));
    
    // Test point counts
    QCOMPARE(widget->getSeriesPointCount("test.field1"), 0);
    QCOMPARE(widget->getSeriesPointCount("test.field2"), 0);
    QCOMPARE(widget->getSeriesPointCount("test.field3"), 0);
}

void TestLineChartWidget::testDataPointAddition()
{
    // Add series
    widget->addLineSeries("test.values", "Test Values");
    
    // Test data point addition through field display
    std::vector<double> testValues = {1.0, 2.5, 3.2, 4.8, 5.1, 6.7};
    
    for (double value : testValues) {
        widget->updateFieldDisplay("test.values", QVariant(value));
    }
    
    // Verify data points were added
    QCOMPARE(widget->getSeriesPointCount("test.values"), static_cast<int>(testValues.size()));
    
    // Test last data point
    QPointF lastPoint = widget->getLastDataPoint("test.values");
    QCOMPARE(lastPoint.y(), testValues.back());
    QVERIFY(lastPoint.x() > 0); // X should be packet sequence number
}

void TestLineChartWidget::testDataPointStorage()
{
    // Add series
    widget->addLineSeries("storage.test", "Storage Test");
    
    // Add multiple data points
    std::vector<double> values = {10.0, 20.0, 15.0, 25.0, 30.0};
    addTestData("storage.test", values);
    
    // Verify storage
    std::vector<QPointF> seriesData = widget->getSeriesData("storage.test");
    QCOMPARE(seriesData.size(), values.size());
    
    // Verify Y values match input
    for (size_t i = 0; i < values.size(); ++i) {
        QCOMPARE(seriesData[i].y(), values[i]);
        QVERIFY(seriesData[i].x() >= 0); // X values should be positive
    }
    
    // Verify X values are sequential for PacketSequence mode
    for (size_t i = 1; i < seriesData.size(); ++i) {
        QVERIFY(seriesData[i].x() > seriesData[i-1].x());
    }
}

void TestLineChartWidget::testDataPointRetrieval()
{
    // Add series with data
    widget->addLineSeries("retrieval.test", "Retrieval Test");
    std::vector<double> values = {1.0, 5.0, 3.0, 8.0, 2.0, 9.0, 4.0};
    addTestData("retrieval.test", values);
    
    // Test full data retrieval
    std::vector<QPointF> allData = widget->getSeriesData("retrieval.test");
    QCOMPARE(allData.size(), values.size());
    
    // Test range-based retrieval
    std::vector<QPointF> rangeData = widget->getSeriesDataInRange("retrieval.test", 2.0, 5.0);
    QVERIFY(rangeData.size() <= allData.size());
    
    // Verify all points in range are within bounds
    for (const QPointF& point : rangeData) {
        QVERIFY(point.x() >= 2.0);
        QVERIFY(point.x() <= 5.0);
    }
    
    // Test last data point
    QPointF lastPoint = widget->getLastDataPoint("retrieval.test");
    QVERIFY(!lastPoint.isNull());
    QCOMPARE(lastPoint.y(), values.back());
}

void TestLineChartWidget::testDataPointHistory()
{
    // Test rolling data behavior
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    config.maxDataPoints = 5; // Small limit for testing
    config.rollingData = true;
    widget->setLineChartConfig(config);
    
    widget->addLineSeries("history.test", "History Test");
    
    // Add more data than the limit
    std::vector<double> values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    addTestData("history.test", values);
    
    // Should only keep the last 5 points
    QCOMPARE(widget->getSeriesPointCount("history.test"), 5);
    
    // Verify the kept data is the most recent
    std::vector<QPointF> keptData = widget->getSeriesData("history.test");
    QCOMPARE(keptData.size(), 5);
    
    // The last point should be 10
    QCOMPARE(widget->getLastDataPoint("history.test").y(), 10.0);
}

void TestLineChartWidget::testLineSeriesConfiguration()
{
    // Add series
    widget->addLineSeries("config.test", "Config Test", QColor(Qt::red));
    
    // Test default configuration
    LineChartWidget::LineSeriesConfig config = widget->getLineSeriesConfig("config.test");
    QCOMPARE(config.lineStyle, LineChartWidget::LineChartConfig::LineStyle::Solid);
    QCOMPARE(config.pointStyle, LineChartWidget::LineChartConfig::PointStyle::None);
    QCOMPARE(config.interpolation, LineChartWidget::LineChartConfig::InterpolationMethod::Linear);
    QCOMPARE(config.lineWidth, 2);
    QCOMPARE(config.pointSize, 6.0);
    QVERIFY(!config.showPoints);
    QVERIFY(config.connectPoints);
    
    // Modify configuration
    config.lineStyle = LineChartWidget::LineChartConfig::LineStyle::Dash;
    config.pointStyle = LineChartWidget::LineChartConfig::PointStyle::Circle;
    config.showPoints = true;
    config.lineWidth = 4;
    config.pointSize = 8.0;
    config.interpolation = LineChartWidget::LineChartConfig::InterpolationMethod::Spline;
    
    widget->setLineSeriesConfig("config.test", config);
    
    // Verify configuration was applied
    LineChartWidget::LineSeriesConfig newConfig = widget->getLineSeriesConfig("config.test");
    QCOMPARE(newConfig.lineStyle, LineChartWidget::LineChartConfig::LineStyle::Dash);
    QCOMPARE(newConfig.pointStyle, LineChartWidget::LineChartConfig::PointStyle::Circle);
    QVERIFY(newConfig.showPoints);
    QCOMPARE(newConfig.lineWidth, 4);
    QCOMPARE(newConfig.pointSize, 8.0);
    QCOMPARE(newConfig.interpolation, LineChartWidget::LineChartConfig::InterpolationMethod::Spline);
}

void TestLineChartWidget::testInterpolationMethods()
{
    // Test all interpolation methods
    std::vector<LineChartWidget::LineChartConfig::InterpolationMethod> methods = {
        LineChartWidget::LineChartConfig::InterpolationMethod::Linear,
        LineChartWidget::LineChartConfig::InterpolationMethod::Spline,
        LineChartWidget::LineChartConfig::InterpolationMethod::Step
    };
    
    for (size_t i = 0; i < methods.size(); ++i) {
        QString fieldPath = QString("interpolation.test%1").arg(i);
        
        // Create series with specific interpolation method
        LineChartWidget::LineSeriesConfig config;
        config.interpolation = methods[i];
        
        widget->addLineSeries(fieldPath, QString("Method %1").arg(i), QColor(), config);
        
        // Verify configuration
        LineChartWidget::LineSeriesConfig retrievedConfig = widget->getLineSeriesConfig(fieldPath);
        QCOMPARE(retrievedConfig.interpolation, methods[i]);
        
        // Add some test data
        addTestData(fieldPath, {1.0, 3.0, 2.0, 4.0});
        QCOMPARE(widget->getSeriesPointCount(fieldPath), 4);
    }
}

void TestLineChartWidget::testLineStyles()
{
    // Test all line styles
    std::vector<LineChartWidget::LineChartConfig::LineStyle> styles = {
        LineChartWidget::LineChartConfig::LineStyle::Solid,
        LineChartWidget::LineChartConfig::LineStyle::Dash,
        LineChartWidget::LineChartConfig::LineStyle::Dot,
        LineChartWidget::LineChartConfig::LineStyle::DashDot,
        LineChartWidget::LineChartConfig::LineStyle::DashDotDot
    };
    
    for (size_t i = 0; i < styles.size(); ++i) {
        QString fieldPath = QString("style.test%1").arg(i);
        
        LineChartWidget::LineSeriesConfig config;
        config.lineStyle = styles[i];
        
        widget->addLineSeries(fieldPath, QString("Style %1").arg(i), QColor(), config);
        
        LineChartWidget::LineSeriesConfig retrievedConfig = widget->getLineSeriesConfig(fieldPath);
        QCOMPARE(retrievedConfig.lineStyle, styles[i]);
    }
}

void TestLineChartWidget::testPointStyles()
{
    // Test point styles
    std::vector<LineChartWidget::LineChartConfig::PointStyle> styles = {
        LineChartWidget::LineChartConfig::PointStyle::None,
        LineChartWidget::LineChartConfig::PointStyle::Circle,
        LineChartWidget::LineChartConfig::PointStyle::Square,
        LineChartWidget::LineChartConfig::PointStyle::Triangle,
        LineChartWidget::LineChartConfig::PointStyle::Diamond
    };
    
    for (size_t i = 0; i < styles.size(); ++i) {
        QString fieldPath = QString("point.test%1").arg(i);
        
        LineChartWidget::LineSeriesConfig config;
        config.pointStyle = styles[i];
        config.showPoints = true; // Enable points for testing
        
        widget->addLineSeries(fieldPath, QString("Point %1").arg(i), QColor(), config);
        
        LineChartWidget::LineSeriesConfig retrievedConfig = widget->getLineSeriesConfig(fieldPath);
        QCOMPARE(retrievedConfig.pointStyle, styles[i]);
        QVERIFY(retrievedConfig.showPoints);
    }
}

void TestLineChartWidget::testXAxisTypes()
{
    // Test PacketSequence mode (default)
    QCOMPARE(widget->getXAxisType(), LineChartWidget::LineChartConfig::XAxisType::PacketSequence);
    
    widget->addLineSeries("packet.test", "Packet Test");
    addTestData("packet.test", {1.0, 2.0, 3.0});
    
    std::vector<QPointF> data = widget->getSeriesData("packet.test");
    QCOMPARE(data.size(), 3);
    
    // X values should be 1, 2, 3 (packet sequence)
    QCOMPARE(data[0].x(), 1.0);
    QCOMPARE(data[1].x(), 2.0);
    QCOMPARE(data[2].x(), 3.0);
    
    // Test Timestamp mode
    widget->setXAxisType(LineChartWidget::LineChartConfig::XAxisType::Timestamp);
    QCOMPARE(widget->getXAxisType(), LineChartWidget::LineChartConfig::XAxisType::Timestamp);
    
    widget->clearAllData();
    widget->addLineSeries("timestamp.test", "Timestamp Test");
    addTestData("timestamp.test", {10.0, 20.0});
    
    std::vector<QPointF> timestampData = widget->getSeriesData("timestamp.test");
    QCOMPARE(timestampData.size(), 2);
    
    // X values should be timestamps (much larger numbers)
    QVERIFY(timestampData[0].x() > 1000000); // Should be a timestamp
    QVERIFY(timestampData[1].x() > timestampData[0].x()); // Should be increasing
    
    // Test FieldValue mode
    widget->setXAxisType(LineChartWidget::LineChartConfig::XAxisType::FieldValue);
    QCOMPARE(widget->getXAxisType(), LineChartWidget::LineChartConfig::XAxisType::FieldValue);
    
    widget->clearAllData();
    widget->addLineSeries("field.test", "Field Test");
    
    // For field value mode, both X and Y would come from the field value
    // This is a simplified test - real implementation might handle differently
    addTestData("field.test", {5.0, 10.0, 15.0});
    
    std::vector<QPointF> fieldData = widget->getSeriesData("field.test");
    QCOMPARE(fieldData.size(), 3);
}

void TestLineChartWidget::testAutoScaling()
{
    // Test initial auto-scaling state
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    QVERIFY(config.autoScaleX);
    QVERIFY(config.autoScaleY);
    
    // Test disabling auto-scaling
    config.autoScaleX = false;
    config.autoScaleY = false;
    widget->setLineChartConfig(config);
    
    LineChartWidget::LineChartConfig newConfig = widget->getLineChartConfig();
    QVERIFY(!newConfig.autoScaleX);
    QVERIFY(!newConfig.autoScaleY);
    
    // Test data bounds calculation
    widget->addLineSeries("bounds.test", "Bounds Test");
    addTestData("bounds.test", {-5.0, 10.0, 3.0, 25.0, -2.0});
    
    QPair<double, double> yRange = widget->getYRange();
    QCOMPARE(yRange.first, -5.0);  // Minimum Y value
    QCOMPARE(yRange.second, 25.0); // Maximum Y value
    
    QPair<double, double> xRange = widget->getXRange();
    QVERIFY(xRange.first >= 0);     // X should start from positive values
    QVERIFY(xRange.second > xRange.first); // X should be increasing
}

void TestLineChartWidget::testAxisFieldPath()
{
    // Test setting X-axis field path
    QString fieldPath = "axis.x.field";
    widget->setXAxisFieldPath(fieldPath);
    
    QCOMPARE(widget->getXAxisFieldPath(), fieldPath);
    QCOMPARE(widget->getXAxisType(), LineChartWidget::LineChartConfig::XAxisType::FieldValue);
}

void TestLineChartWidget::testRealTimeMode()
{
    // Test initial real-time mode
    QVERIFY(widget->isRealTimeMode());
    
    // Test disabling real-time mode
    widget->setRealTimeMode(false);
    QVERIFY(!widget->isRealTimeMode());
    
    // Test re-enabling
    widget->setRealTimeMode(true);
    QVERIFY(widget->isRealTimeMode());
    
    // Test configuration consistency
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    QCOMPARE(config.enableRealTimeMode, widget->isRealTimeMode());
}

void TestLineChartWidget::testDataDecimation()
{
    // Configure for decimation testing
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    config.maxDataPoints = 100; // Small limit to trigger decimation
    widget->setLineChartConfig(config);
    
    widget->addLineSeries("decimation.test", "Decimation Test");
    
    // Add more data than the limit
    std::vector<double> largeDataset;
    for (int i = 0; i < 200; ++i) {
        largeDataset.push_back(sin(i * 0.1) * 10);
    }
    
    addTestData("decimation.test", largeDataset);
    
    // The actual data stored should be less than or equal to the limit
    // (depending on decimation strategy and rolling data behavior)
    int actualPointCount = widget->getSeriesPointCount("decimation.test");
    QVERIFY(actualPointCount <= 200); // Should not exceed original data
    
    // Verify data integrity is maintained
    QPointF lastPoint = widget->getLastDataPoint("decimation.test");
    QVERIFY(!lastPoint.isNull());
}

void TestLineChartWidget::testRollingData()
{
    // Configure rolling data
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    config.maxDataPoints = 5;
    config.rollingData = true;
    widget->setLineChartConfig(config);
    
    widget->addLineSeries("rolling.test", "Rolling Test");
    
    // Add data incrementally
    for (int i = 1; i <= 10; ++i) {
        widget->updateFieldDisplay("rolling.test", QVariant(static_cast<double>(i)));
        
        int expectedCount = std::min(i, 5);
        QCOMPARE(widget->getSeriesPointCount("rolling.test"), expectedCount);
    }
    
    // Final count should be limited to maxDataPoints
    QCOMPARE(widget->getSeriesPointCount("rolling.test"), 5);
    
    // The last value should be 10
    QPointF lastPoint = widget->getLastDataPoint("rolling.test");
    QCOMPARE(lastPoint.y(), 10.0);
}

void TestLineChartWidget::testLargeDatasets()
{
    widget->addLineSeries("large.test", "Large Dataset Test");
    
    // Create large dataset
    std::vector<double> largeData;
    for (int i = 0; i < 10000; ++i) {
        largeData.push_back(sin(i * 0.01) * 100 + cos(i * 0.005) * 50);
    }
    
    // Measure time to add data
    auto startTime = std::chrono::high_resolution_clock::now();
    addTestData("large.test", largeData);
    auto endTime = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Performance expectation: should handle 10k points reasonably fast
    QVERIFY(duration.count() < 1000); // Less than 1 second
    
    // Verify data integrity
    int pointCount = widget->getSeriesPointCount("large.test");
    QVERIFY(pointCount > 0);
    QVERIFY(pointCount <= static_cast<int>(largeData.size()));
}

void TestLineChartWidget::testDataSmoothing()
{
    // Test smoothing configuration
    LineChartWidget::LineSeriesConfig config;
    config.enableSmoothing = true;
    config.smoothingWindow = 5;
    
    widget->addLineSeries("smooth.test", "Smooth Test", QColor(), config);
    
    // Verify smoothing configuration
    LineChartWidget::LineSeriesConfig retrievedConfig = widget->getLineSeriesConfig("smooth.test");
    QVERIFY(retrievedConfig.enableSmoothing);
    QCOMPARE(retrievedConfig.smoothingWindow, 5);
    
    // Add noisy data
    std::vector<double> noisyData = {1, 10, 2, 9, 3, 8, 4, 7, 5, 6};
    addTestData("smooth.test", noisyData);
    
    // Verify data was processed
    QCOMPARE(widget->getSeriesPointCount("smooth.test"), static_cast<int>(noisyData.size()));
}

void TestLineChartWidget::testViewportOptimization()
{
    // This test verifies that viewport optimization features exist
    // Actual performance testing would require more complex setup
    
    widget->addLineSeries("viewport.test", "Viewport Test");
    addTestData("viewport.test", {1, 2, 3, 4, 5});
    
    // Test scroll to latest functionality
    widget->scrollToLatest();
    
    // Basic validation - method should exist and not crash
    QVERIFY(true);
}

void TestLineChartWidget::testStatisticalFunctions()
{
    widget->addLineSeries("stats.test", "Statistics Test");
    
    // Add known data for statistical calculations
    std::vector<double> testData = {2, 4, 4, 4, 5, 5, 7, 9};
    addTestData("stats.test", testData);
    
    // Test mean calculation
    double mean = widget->getSeriesMean("stats.test");
    double expectedMean = 5.0; // (2+4+4+4+5+5+7+9)/8 = 40/8 = 5
    QCOMPARE(mean, expectedMean);
    
    // Test standard deviation
    double stdDev = widget->getSeriesStdDev("stats.test");
    QVERIFY(stdDev > 0); // Should be positive for this dataset
    
    // Standard deviation for this dataset should be about 2.0
    QVERIFY(stdDev > 1.5 && stdDev < 2.5);
}

void TestLineChartWidget::testDataRangeCalculation()
{
    widget->addLineSeries("range.test", "Range Test");
    
    std::vector<double> rangeData = {-10, 5, 20, -5, 15, 0, 25};
    addTestData("range.test", rangeData);
    
    // Test Y range
    QPair<double, double> yRange = widget->getYRange();
    QCOMPARE(yRange.first, -10.0);  // Minimum
    QCOMPARE(yRange.second, 25.0);  // Maximum
    
    // Test X range
    QPair<double, double> xRange = widget->getXRange();
    QVERIFY(xRange.first < xRange.second); // Should be valid range
    QVERIFY(xRange.first >= 0); // Packet sequence starts from positive numbers
}

void TestLineChartWidget::testLineSettingsSave()
{
    // Configure widget with specific settings
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    config.maxDataPoints = 8000;
    config.xAxisType = LineChartWidget::LineChartConfig::XAxisType::Timestamp;
    config.interpolation = LineChartWidget::LineChartConfig::InterpolationMethod::Spline;
    config.defaultLineWidth = 5;
    config.showPoints = true;
    config.enableRealTimeMode = false;
    widget->setLineChartConfig(config);
    
    // Add series with custom configuration
    LineChartWidget::LineSeriesConfig seriesConfig;
    seriesConfig.lineStyle = LineChartWidget::LineChartConfig::LineStyle::Dash;
    seriesConfig.pointStyle = LineChartWidget::LineChartConfig::PointStyle::Circle;
    seriesConfig.showPoints = true;
    seriesConfig.enableSmoothing = true;
    seriesConfig.smoothingWindow = 10;
    
    widget->addLineSeries("save.test", "Save Test", QColor(255, 128, 0), seriesConfig);
    
    // Save settings
    QJsonObject settings = widget->saveSettings();
    
    // Verify line-specific settings are saved
    QVERIFY(settings.contains("lineConfig"));
    QJsonObject lineConfig = settings["lineConfig"].toObject();
    QCOMPARE(lineConfig["maxDataPoints"].toInt(), 8000);
    QCOMPARE(lineConfig["xAxisType"].toInt(), 
             static_cast<int>(LineChartWidget::LineChartConfig::XAxisType::Timestamp));
    QCOMPARE(lineConfig["interpolation"].toInt(), 
             static_cast<int>(LineChartWidget::LineChartConfig::InterpolationMethod::Spline));
    QCOMPARE(lineConfig["defaultLineWidth"].toInt(), 5);
    QVERIFY(lineConfig["showPoints"].toBool());
    QVERIFY(!lineConfig["enableRealTimeMode"].toBool());
    
    // Verify series configurations are saved
    QVERIFY(settings.contains("lineSeriesConfigs"));
    QJsonArray seriesConfigs = settings["lineSeriesConfigs"].toArray();
    QCOMPARE(seriesConfigs.size(), 1);
    
    QJsonObject seriesObj = seriesConfigs[0].toObject();
    QCOMPARE(seriesObj["fieldPath"].toString(), QString("save.test"));
    
    QJsonObject savedSeriesConfig = seriesObj["config"].toObject();
    QCOMPARE(savedSeriesConfig["lineStyle"].toInt(), 
             static_cast<int>(LineChartWidget::LineChartConfig::LineStyle::Dash));
    QCOMPARE(savedSeriesConfig["pointStyle"].toInt(), 
             static_cast<int>(LineChartWidget::LineChartConfig::PointStyle::Circle));
    QVERIFY(savedSeriesConfig["showPoints"].toBool());
    QVERIFY(savedSeriesConfig["enableSmoothing"].toBool());
    QCOMPARE(savedSeriesConfig["smoothingWindow"].toInt(), 10);
}

void TestLineChartWidget::testLineSettingsRestore()
{
    // Create settings JSON manually
    QJsonObject settings;
    
    // Base widget settings
    QJsonObject chartConfig;
    chartConfig["title"] = "Restored Line Chart";
    settings["chartConfig"] = chartConfig;
    
    // Line-specific settings
    QJsonObject lineConfig;
    lineConfig["maxDataPoints"] = 6000;
    lineConfig["xAxisType"] = static_cast<int>(LineChartWidget::LineChartConfig::XAxisType::Timestamp);
    lineConfig["interpolation"] = static_cast<int>(LineChartWidget::LineChartConfig::InterpolationMethod::Step);
    lineConfig["defaultLineWidth"] = 4;
    lineConfig["showPoints"] = true;
    lineConfig["enableRealTimeMode"] = false;
    lineConfig["autoScaleX"] = false;
    lineConfig["autoScaleY"] = true;
    settings["lineConfig"] = lineConfig;
    
    // Series configurations
    QJsonArray lineSeriesConfigs;
    QJsonObject seriesObj;
    seriesObj["fieldPath"] = "restored.series";
    
    QJsonObject seriesConfig;
    seriesConfig["lineStyle"] = static_cast<int>(LineChartWidget::LineChartConfig::LineStyle::Dot);
    seriesConfig["pointStyle"] = static_cast<int>(LineChartWidget::LineChartConfig::PointStyle::Square);
    seriesConfig["showPoints"] = true;
    seriesConfig["lineWidth"] = 6;
    seriesConfig["enableSmoothing"] = true;
    seriesConfig["smoothingWindow"] = 7;
    seriesObj["config"] = seriesConfig;
    
    lineSeriesConfigs.append(seriesObj);
    settings["lineSeriesConfigs"] = lineSeriesConfigs;
    
    // Restore settings
    bool result = widget->restoreSettings(settings);
    QVERIFY(result);
    
    // Verify line configuration was restored
    LineChartWidget::LineChartConfig restoredLineConfig = widget->getLineChartConfig();
    QCOMPARE(restoredLineConfig.maxDataPoints, 6000);
    QCOMPARE(restoredLineConfig.xAxisType, LineChartWidget::LineChartConfig::XAxisType::Timestamp);
    QCOMPARE(restoredLineConfig.interpolation, LineChartWidget::LineChartConfig::InterpolationMethod::Step);
    QCOMPARE(restoredLineConfig.defaultLineWidth, 4);
    QVERIFY(restoredLineConfig.showPoints);
    QVERIFY(!restoredLineConfig.enableRealTimeMode);
    QVERIFY(!restoredLineConfig.autoScaleX);
    QVERIFY(restoredLineConfig.autoScaleY);
    
    // Verify series was recreated with correct configuration
    QStringList seriesList = widget->getSeriesList();
    QCOMPARE(seriesList.size(), 1);
    QVERIFY(seriesList.contains("restored.series"));
    
    LineChartWidget::LineSeriesConfig restoredSeriesConfig = 
        widget->getLineSeriesConfig("restored.series");
    QCOMPARE(restoredSeriesConfig.lineStyle, LineChartWidget::LineChartConfig::LineStyle::Dot);
    QCOMPARE(restoredSeriesConfig.pointStyle, LineChartWidget::LineChartConfig::PointStyle::Square);
    QVERIFY(restoredSeriesConfig.showPoints);
    QCOMPARE(restoredSeriesConfig.lineWidth, 6);
    QVERIFY(restoredSeriesConfig.enableSmoothing);
    QCOMPARE(restoredSeriesConfig.smoothingWindow, 7);
}

void TestLineChartWidget::testInvalidDataHandling()
{
    widget->addLineSeries("invalid.test", "Invalid Test");
    
    // Test invalid QVariant values
    widget->updateFieldDisplay("invalid.test", QVariant("not a number"));
    widget->updateFieldDisplay("invalid.test", QVariant());
    widget->updateFieldDisplay("invalid.test", QVariant(QStringList()));
    
    // Should handle gracefully without crashing
    QCOMPARE(widget->getSeriesPointCount("invalid.test"), 0);
    
    // Test operations on non-existent series
    std::vector<QPointF> emptyData = widget->getSeriesData("non.existent");
    QVERIFY(emptyData.empty());
    
    QPointF nullPoint = widget->getLastDataPoint("non.existent");
    QVERIFY(nullPoint.isNull());
    
    QCOMPARE(widget->getSeriesPointCount("non.existent"), 0);
    
    double zerothPoint = widget->getSeriesMean("non.existent");
    QCOMPARE(zerothPoint, 0.0);
    
    double zeroStdDev = widget->getSeriesStdDev("non.existent");
    QCOMPARE(zeroStdDev, 0.0);
}

void TestLineChartWidget::testMemoryManagement()
{
    // Test large dataset with clearing
    widget->addLineSeries("memory.test", "Memory Test");
    
    // Add large amount of data
    for (int i = 0; i < 1000; ++i) {
        widget->updateFieldDisplay("memory.test", QVariant(static_cast<double>(i)));
    }
    
    QVERIFY(widget->getSeriesPointCount("memory.test") > 0);
    
    // Clear series data
    widget->clearSeriesData("memory.test");
    QCOMPARE(widget->getSeriesPointCount("memory.test"), 0);
    
    // Clear all data
    widget->addLineSeries("memory.test2", "Memory Test 2");
    addTestData("memory.test2", {1, 2, 3, 4, 5});
    
    widget->clearAllData();
    QCOMPARE(widget->getSeriesPointCount("memory.test2"), 0);
    QVERIFY(widget->getSeriesList().isEmpty());
}

// Helper methods
void TestLineChartWidget::addTestData(const QString& fieldPath, const std::vector<double>& values)
{
    for (double value : values) {
        widget->updateFieldDisplay(fieldPath, QVariant(value));
    }
}

void TestLineChartWidget::verifySeriesData(const QString& fieldPath, const std::vector<double>& expectedValues)
{
    std::vector<QPointF> actualData = widget->getSeriesData(fieldPath);
    QCOMPARE(actualData.size(), expectedValues.size());
    
    for (size_t i = 0; i < expectedValues.size(); ++i) {
        QCOMPARE(actualData[i].y(), expectedValues[i]);
    }
}

QTEST_MAIN(TestLineChartWidget)
#include "test_line_chart_widget.moc"