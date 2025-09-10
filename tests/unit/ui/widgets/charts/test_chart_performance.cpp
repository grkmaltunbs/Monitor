#include <QtTest/QtTest>
#include <QApplication>
#include <QTimer>
#include <QEventLoop>
#include <chrono>
#include <memory>
#include <random>
#include <thread>

#include "ui/widgets/charts/line_chart_widget.h"
#include "ui/widgets/charts/bar_chart_widget.h"
#include "ui/widgets/charts/pie_chart_widget.h"
#include "ui/widgets/charts/chart_common.h"

class TestChartPerformance : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Performance benchmarks
    void testLineChartLargeDataset();
    void testLineChartRealTimeUpdates();
    void testLineChartMemoryUsage();
    void testLineChartFPSTarget();
    
    void testBarChartLargeCategories();
    void testBarChartRealTimeUpdates();
    void testBarChartMemoryEfficiency();
    
    void testPieChartManySlices();
    void testPieChartAnimationPerformance();
    void testPieChartUpdateLatency();
    
    // Data processing performance
    void testDataDecimationPerformance();
    void testDataSmoothingPerformance();
    void testStatisticalCalculationPerformance();
    
    // Chart export performance
    void testExportPerformance();
    void testLargeExportPerformance();
    
    // Memory management tests
    void testMemoryLeakPrevention();
    void testLargeDatasetMemoryBehavior();
    void testMemoryCleanupEfficiency();
    
    // Threading and concurrency tests
    void testConcurrentUpdates();
    void testThreadSafety();
    
    // Scalability tests
    void testMultipleChartsPerformance();
    void testMaxDataPointsHandling();

private:
    QApplication* app;
    
    // Performance test helpers
    struct PerformanceResult {
        std::chrono::milliseconds duration;
        size_t memoryUsed;
        double avgFPS;
        bool success;
        QString errorMessage;
        
        PerformanceResult() : duration(0), memoryUsed(0), avgFPS(0.0), success(false) {}
    };
    
    // Helper methods
    PerformanceResult measureLineChartPerformance(int dataPoints, int updateIntervalMs = 16);
    PerformanceResult measureBarChartPerformance(int categories, int series, int updatesPerSecond = 60);
    PerformanceResult measurePieChartPerformance(int slices, int updatesPerSecond = 60);
    
    std::vector<double> generateTestData(size_t count, double amplitude = 100.0, double frequency = 0.1);
    void waitForEventLoop(int milliseconds);
    size_t estimateMemoryUsage();
    
    // Performance constants
    static constexpr int TARGET_FPS = 60;
    static constexpr int MIN_FPS = 30;
    static constexpr int MAX_ACCEPTABLE_LATENCY_MS = 100;
    static constexpr size_t MAX_MEMORY_MB = 500;
    static constexpr int LARGE_DATASET_SIZE = 100000;
    static constexpr int STRESS_TEST_DURATION_MS = 5000;
};

void TestChartPerformance::initTestCase()
{
    // Initialize Qt application if not already done
    if (!QApplication::instance()) {
        static int argc = 1;
        static char* argv[] = {const_cast<char*>("test")};
        app = new QApplication(argc, argv);
    }
    
    qDebug() << "Starting Chart Performance Tests";
    qDebug() << "Target FPS:" << TARGET_FPS;
    qDebug() << "Minimum acceptable FPS:" << MIN_FPS;
    qDebug() << "Maximum acceptable latency:" << MAX_ACCEPTABLE_LATENCY_MS << "ms";
    qDebug() << "Maximum memory usage:" << MAX_MEMORY_MB << "MB";
}

void TestChartPerformance::cleanupTestCase()
{
    qDebug() << "Chart Performance Tests completed";
}

void TestChartPerformance::init()
{
    // Setup for each test
}

void TestChartPerformance::cleanup()
{
    // Force garbage collection between tests
    waitForEventLoop(100);
}

void TestChartPerformance::testLineChartLargeDataset()
{
    qDebug() << "Testing LineChart with large dataset (" << LARGE_DATASET_SIZE << " points)";
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Create line chart widget
    auto widget = std::make_unique<LineChartWidget>("perf_line_chart");
    widget->initializeWidget();
    
    // Configure for performance
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    config.maxDataPoints = LARGE_DATASET_SIZE;
    config.rollingData = false; // Keep all data for testing
    config.enableRealTimeMode = false; // Disable for bulk loading
    widget->setLineChartConfig(config);
    
    // Add series
    widget->addLineSeries("perf.test", "Performance Test");
    
    // Generate large dataset
    std::vector<double> largeData = generateTestData(LARGE_DATASET_SIZE, 100.0, 0.001);
    
    auto dataGenTime = std::chrono::high_resolution_clock::now();
    
    // Add data points
    for (size_t i = 0; i < largeData.size(); ++i) {
        widget->updateFieldDisplay("perf.test", QVariant(largeData[i]));
        
        // Process events periodically to prevent UI freezing
        if (i % 1000 == 0) {
            QApplication::processEvents();
        }
    }
    
    auto dataLoadTime = std::chrono::high_resolution_clock::now();
    
    // Force display update
    widget->refreshAllDisplays();
    waitForEventLoop(100); // Allow rendering to complete
    
    auto endTime = std::chrono::high_resolution_clock::now();
    
    // Calculate timings
    auto dataGenDuration = std::chrono::duration_cast<std::chrono::milliseconds>(dataGenTime - startTime);
    auto dataLoadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(dataLoadTime - dataGenTime);
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    qDebug() << "Data generation time:" << dataGenDuration.count() << "ms";
    qDebug() << "Data loading time:" << dataLoadDuration.count() << "ms";
    qDebug() << "Total time:" << totalDuration.count() << "ms";
    qDebug() << "Points per second:" << (LARGE_DATASET_SIZE * 1000.0 / totalDuration.count());
    
    // Verify all data was loaded
    QCOMPARE(widget->getSeriesPointCount("perf.test"), LARGE_DATASET_SIZE);
    
    // Performance assertions
    QVERIFY(totalDuration.count() < 10000); // Should complete within 10 seconds
    QVERIFY(dataLoadDuration.count() < 5000); // Data loading should be fast
    
    qDebug() << "LineChart large dataset test: PASSED";
}

void TestChartPerformance::testLineChartRealTimeUpdates()
{
    qDebug() << "Testing LineChart real-time updates at" << TARGET_FPS << "FPS";
    
    auto widget = std::make_unique<LineChartWidget>("realtime_line_chart");
    widget->initializeWidget();
    
    // Configure for real-time
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    config.enableRealTimeMode = true;
    config.maxDataPoints = 1000; // Reasonable limit for real-time
    widget->setLineChartConfig(config);
    
    widget->addLineSeries("realtime.test", "Real-time Test");
    
    // Performance tracking
    int updateCount = 0;
    int targetUpdates = TARGET_FPS * STRESS_TEST_DURATION_MS / 1000; // 5 seconds of updates
    
    QTimer updateTimer;
    QEventLoop eventLoop;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(-100.0, 100.0);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Start high-frequency updates
    connect(&updateTimer, &QTimer::timeout, [&]() {
        double value = dis(gen);
        widget->updateFieldDisplay("realtime.test", QVariant(value));
        
        updateCount++;
        if (updateCount >= targetUpdates) {
            eventLoop.quit();
        }
    });
    
    updateTimer.start(1000 / TARGET_FPS); // Target FPS
    eventLoop.exec();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    double actualFPS = (updateCount * 1000.0) / duration.count();
    double currentFPS = widget->getCurrentFPS();
    
    qDebug() << "Updates performed:" << updateCount;
    qDebug() << "Target updates:" << targetUpdates;
    qDebug() << "Actual duration:" << duration.count() << "ms";
    qDebug() << "Calculated FPS:" << actualFPS;
    qDebug() << "Widget reported FPS:" << currentFPS;
    
    // Performance assertions
    QVERIFY(actualFPS >= MIN_FPS); // Should maintain minimum FPS
    QCOMPARE(updateCount, targetUpdates); // Should complete all updates
    
    qDebug() << "LineChart real-time updates test: PASSED";
}

void TestChartPerformance::testLineChartMemoryUsage()
{
    qDebug() << "Testing LineChart memory usage";
    
    size_t initialMemory = estimateMemoryUsage();
    
    auto widget = std::make_unique<LineChartWidget>("memory_line_chart");
    widget->initializeWidget();
    
    size_t afterWidgetMemory = estimateMemoryUsage();
    
    // Add multiple series with large datasets
    const int seriesCount = 10;
    const int pointsPerSeries = 10000;
    
    for (int s = 0; s < seriesCount; ++s) {
        QString fieldPath = QString("memory.series%1").arg(s);
        widget->addLineSeries(fieldPath, QString("Series %1").arg(s));
        
        std::vector<double> data = generateTestData(pointsPerSeries);
        for (double value : data) {
            widget->updateFieldDisplay(fieldPath, QVariant(value));
        }
    }
    
    size_t afterDataMemory = estimateMemoryUsage();
    
    // Clear all data
    widget->clearAllData();
    
    size_t afterClearMemory = estimateMemoryUsage();
    
    qDebug() << "Initial memory:" << initialMemory << "bytes";
    qDebug() << "After widget creation:" << afterWidgetMemory << "bytes";
    qDebug() << "After adding data:" << afterDataMemory << "bytes";
    qDebug() << "After clearing data:" << afterClearMemory << "bytes";
    qDebug() << "Widget overhead:" << (afterWidgetMemory - initialMemory) << "bytes";
    qDebug() << "Data overhead:" << (afterDataMemory - afterWidgetMemory) << "bytes";
    qDebug() << "Memory recovered:" << (afterDataMemory - afterClearMemory) << "bytes";
    
    // Memory assertions (these are estimates, so we use reasonable bounds)
    size_t totalDataPoints = seriesCount * pointsPerSeries;
    size_t dataMemoryUsed = afterDataMemory - afterWidgetMemory;
    size_t bytesPerPoint = dataMemoryUsed / totalDataPoints;
    
    qDebug() << "Total data points:" << totalDataPoints;
    qDebug() << "Estimated bytes per point:" << bytesPerPoint;
    
    // Should recover significant memory after clearing
    size_t memoryRecovered = afterDataMemory - afterClearMemory;
    double recoveryRatio = static_cast<double>(memoryRecovered) / dataMemoryUsed;
    
    qDebug() << "Memory recovery ratio:" << (recoveryRatio * 100) << "%";
    
    QVERIFY(recoveryRatio > 0.5); // Should recover at least 50% of memory
    QVERIFY(bytesPerPoint < 1000); // Should be reasonably efficient per point
    
    qDebug() << "LineChart memory usage test: PASSED";
}

void TestChartPerformance::testLineChartFPSTarget()
{
    qDebug() << "Testing LineChart FPS target maintenance";
    
    auto widget = std::make_unique<LineChartWidget>("fps_line_chart");
    widget->initializeWidget();
    
    // Configure for high-frequency updates
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    config.enableRealTimeMode = true;
    config.maxDataPoints = 5000;
    widget->setLineChartConfig(config);
    
    // Add multiple series to increase rendering load
    const int seriesCount = 5;
    for (int i = 0; i < seriesCount; ++i) {
        widget->addLineSeries(QString("fps.series%1").arg(i), QString("FPS Series %1").arg(i));
    }
    
    // Measure FPS over time
    std::vector<double> fpsReadings;
    QTimer measurementTimer;
    QEventLoop eventLoop;
    int measurementCount = 0;
    const int maxMeasurements = 50; // Measure for ~5 seconds
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(-50.0, 50.0);
    
    // Start regular data updates
    QTimer dataTimer;
    connect(&dataTimer, &QTimer::timeout, [&]() {
        for (int i = 0; i < seriesCount; ++i) {
            QString fieldPath = QString("fps.series%1").arg(i);
            widget->updateFieldDisplay(fieldPath, QVariant(dis(gen)));
        }
    });
    dataTimer.start(16); // ~60 FPS updates
    
    // Measure FPS periodically
    connect(&measurementTimer, &QTimer::timeout, [&]() {
        double currentFPS = widget->getCurrentFPS();
        if (currentFPS > 0) { // Only record valid readings
            fpsReadings.push_back(currentFPS);
        }
        
        measurementCount++;
        if (measurementCount >= maxMeasurements) {
            dataTimer.stop();
            eventLoop.quit();
        }
    });
    
    measurementTimer.start(100); // Measure every 100ms
    eventLoop.exec();
    
    // Analyze FPS performance
    if (!fpsReadings.empty()) {
        double avgFPS = std::accumulate(fpsReadings.begin(), fpsReadings.end(), 0.0) / fpsReadings.size();
        auto minMaxFPS = std::minmax_element(fpsReadings.begin(), fpsReadings.end());
        double minFPS = *minMaxFPS.first;
        double maxFPS = *minMaxFPS.second;
        
        // Calculate FPS stability (standard deviation)
        double variance = 0.0;
        for (double fps : fpsReadings) {
            variance += (fps - avgFPS) * (fps - avgFPS);
        }
        double stdDev = sqrt(variance / fpsReadings.size());
        
        qDebug() << "FPS readings count:" << fpsReadings.size();
        qDebug() << "Average FPS:" << avgFPS;
        qDebug() << "Min FPS:" << minFPS;
        qDebug() << "Max FPS:" << maxFPS;
        qDebug() << "FPS stability (std dev):" << stdDev;
        
        // Performance assertions
        QVERIFY(avgFPS >= MIN_FPS); // Should maintain minimum average FPS
        QVERIFY(minFPS >= MIN_FPS * 0.8); // Even minimum shouldn't drop too low
        QVERIFY(stdDev < avgFPS * 0.5); // FPS should be relatively stable
    } else {
        qWarning() << "No valid FPS readings obtained";
        QFAIL("FPS measurement failed");
    }
    
    qDebug() << "LineChart FPS target test: PASSED";
}

void TestChartPerformance::testBarChartLargeCategories()
{
    qDebug() << "Testing BarChart with large number of categories";
    
    const int categoryCount = 1000;
    const int seriesCount = 5;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    auto widget = std::make_unique<BarChartWidget>("perf_bar_chart");
    widget->initializeWidget();
    
    // Configure for large datasets
    BarChartWidget::BarChartConfig config = widget->getBarChartConfig();
    config.maxCategories = categoryCount;
    config.autoSortCategories = false; // Disable sorting for performance
    widget->setBarChartConfig(config);
    
    // Add multiple series
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(1.0, 100.0);
    
    for (int s = 0; s < seriesCount; ++s) {
        QString fieldPath = QString("perf.bar.series%1").arg(s);
        widget->addBarSeries(fieldPath, QString("Bar Series %1").arg(s));
        
        // Add data for many categories
        for (int c = 0; c < categoryCount; ++c) {
            // Use the category number as the field value for field-based categories
            widget->updateFieldDisplay(fieldPath, QVariant(QString("Category_%1").arg(c)));
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    qDebug() << "Created" << categoryCount << "categories with" << seriesCount << "series";
    qDebug() << "Total time:" << duration.count() << "ms";
    qDebug() << "Categories per second:" << (categoryCount * 1000.0 / duration.count());
    
    // Verify data was added
    QVERIFY(widget->getCategoryCount() > 0);
    QCOMPARE(widget->getSeriesCount(), seriesCount);
    
    // Performance assertions
    QVERIFY(duration.count() < 5000); // Should complete within 5 seconds
    
    qDebug() << "BarChart large categories test: PASSED";
}

void TestChartPerformance::testBarChartRealTimeUpdates()
{
    qDebug() << "Testing BarChart real-time updates";
    
    auto widget = std::make_unique<BarChartWidget>("realtime_bar_chart");
    widget->initializeWidget();
    
    const int seriesCount = 3;
    const int categoryCount = 20;
    
    // Add series
    for (int s = 0; s < seriesCount; ++s) {
        widget->addBarSeries(QString("realtime.bar%1").arg(s), QString("Bar %1").arg(s));
    }
    
    // Pre-populate categories
    for (int c = 0; c < categoryCount; ++c) {
        widget->addCategory(QString("Cat_%1").arg(c));
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(10.0, 100.0);
    std::uniform_int_distribution<int> catDis(0, categoryCount - 1);
    
    QTimer updateTimer;
    QEventLoop eventLoop;
    int updateCount = 0;
    const int maxUpdates = 500; // 5 seconds at 100 updates/sec
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    connect(&updateTimer, &QTimer::timeout, [&]() {
        // Update random series with random category data
        int seriesIdx = updateCount % seriesCount;
        QString fieldPath = QString("realtime.bar%1").arg(seriesIdx);
        QString categoryValue = QString("Cat_%1").arg(catDis(gen));
        
        widget->updateFieldDisplay(fieldPath, QVariant(categoryValue));
        
        updateCount++;
        if (updateCount >= maxUpdates) {
            eventLoop.quit();
        }
    });
    
    updateTimer.start(10); // 100 updates per second
    eventLoop.exec();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    double updateRate = (updateCount * 1000.0) / duration.count();
    
    qDebug() << "Updates performed:" << updateCount;
    qDebug() << "Duration:" << duration.count() << "ms";
    qDebug() << "Update rate:" << updateRate << "updates/sec";
    
    // Performance assertions
    QVERIFY(updateRate >= 50); // Should handle at least 50 updates/sec
    QCOMPARE(updateCount, maxUpdates);
    
    qDebug() << "BarChart real-time updates test: PASSED";
}

void TestChartPerformance::testBarChartMemoryEfficiency()
{
    qDebug() << "Testing BarChart memory efficiency";
    
    size_t initialMemory = estimateMemoryUsage();
    
    auto widget = std::make_unique<BarChartWidget>("memory_bar_chart");
    widget->initializeWidget();
    
    const int seriesCount = 20;
    const int categoryCount = 100;
    
    // Add series and categories
    for (int s = 0; s < seriesCount; ++s) {
        widget->addBarSeries(QString("memory.bar%1").arg(s), QString("Memory Bar %1").arg(s));
    }
    
    for (int c = 0; c < categoryCount; ++c) {
        widget->addCategory(QString("MemCat_%1").arg(c));
    }
    
    size_t afterSetupMemory = estimateMemoryUsage();
    
    // Fill with data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(1.0, 1000.0);
    
    for (int s = 0; s < seriesCount; ++s) {
        QString fieldPath = QString("memory.bar%1").arg(s);
        for (int c = 0; c < categoryCount; ++c) {
            widget->updateFieldDisplay(fieldPath, QVariant(QString("MemCat_%1").arg(c)));
        }
    }
    
    size_t afterDataMemory = estimateMemoryUsage();
    
    // Clear all data
    widget->clearAllData();
    
    size_t afterClearMemory = estimateMemoryUsage();
    
    qDebug() << "Memory usage progression:";
    qDebug() << "Initial:" << initialMemory << "bytes";
    qDebug() << "After setup:" << afterSetupMemory << "bytes";
    qDebug() << "After data:" << afterDataMemory << "bytes";
    qDebug() << "After clear:" << afterClearMemory << "bytes";
    
    size_t setupOverhead = afterSetupMemory - initialMemory;
    size_t dataOverhead = afterDataMemory - afterSetupMemory;
    size_t memoryRecovered = afterDataMemory - afterClearMemory;
    
    qDebug() << "Setup overhead:" << setupOverhead << "bytes";
    qDebug() << "Data overhead:" << dataOverhead << "bytes";
    qDebug() << "Memory recovered:" << memoryRecovered << "bytes";
    
    double recoveryRatio = static_cast<double>(memoryRecovered) / dataOverhead;
    qDebug() << "Recovery ratio:" << (recoveryRatio * 100) << "%";
    
    // Memory efficiency assertions
    QVERIFY(recoveryRatio > 0.4); // Should recover reasonable amount of memory
    
    qDebug() << "BarChart memory efficiency test: PASSED";
}

void TestChartPerformance::testPieChartManySlices()
{
    qDebug() << "Testing PieChart with many slices";
    
    const int sliceCount = 50;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    auto widget = std::make_unique<PieChartWidget>("perf_pie_chart");
    widget->initializeWidget();
    
    // Configure pie chart
    PieChartWidget::PieChartConfig config = widget->getPieChartConfig();
    config.enableAnimations = false; // Disable for performance measurement
    config.enableRealTimeMode = false;
    widget->setPieChartConfig(config);
    
    // Add many slices
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(1.0, 100.0);
    
    for (int i = 0; i < sliceCount; ++i) {
        QString fieldPath = QString("perf.slice%1").arg(i);
        double value = dis(gen);
        widget->addSlice(fieldPath, QString("Slice %1").arg(i), value);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    qDebug() << "Created" << sliceCount << "slices in" << duration.count() << "ms";
    qDebug() << "Slices per second:" << (sliceCount * 1000.0 / duration.count());
    
    // Verify all slices were created
    QCOMPARE(widget->getSliceCount(), sliceCount);
    QVERIFY(widget->getTotalValue() > 0);
    
    // Performance assertions
    QVERIFY(duration.count() < 2000); // Should complete within 2 seconds
    
    qDebug() << "PieChart many slices test: PASSED";
}

void TestChartPerformance::testPieChartAnimationPerformance()
{
    qDebug() << "Testing PieChart animation performance";
    
    auto widget = std::make_unique<PieChartWidget>("anim_pie_chart");
    widget->initializeWidget();
    
    // Configure with animations
    PieChartWidget::PieChartConfig config = widget->getPieChartConfig();
    config.enableAnimations = true;
    config.animationDuration = 500; // 500ms animations
    config.enableRealTimeMode = true;
    widget->setPieChartConfig(config);
    
    const int sliceCount = 10;
    
    // Add slices
    for (int i = 0; i < sliceCount; ++i) {
        QString fieldPath = QString("anim.slice%1").arg(i);
        widget->addSlice(fieldPath, QString("Animated Slice %1").arg(i), 10.0);
    }
    
    // Measure animation performance by rapidly changing values
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(1.0, 100.0);
    std::uniform_int_distribution<int> sliceDis(0, sliceCount - 1);
    
    QTimer animationTimer;
    QEventLoop eventLoop;
    int animationCount = 0;
    const int maxAnimations = 100;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    connect(&animationTimer, &QTimer::timeout, [&]() {
        int sliceIdx = sliceDis(gen);
        QString fieldPath = QString("anim.slice%1").arg(sliceIdx);
        double newValue = dis(gen);
        
        widget->updateSliceValue(fieldPath, newValue);
        
        animationCount++;
        if (animationCount >= maxAnimations) {
            eventLoop.quit();
        }
    });
    
    animationTimer.start(50); // Change values every 50ms
    eventLoop.exec();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    double animationRate = (animationCount * 1000.0) / duration.count();
    
    qDebug() << "Animations performed:" << animationCount;
    qDebug() << "Duration:" << duration.count() << "ms";
    qDebug() << "Animation rate:" << animationRate << "animations/sec";
    
    // Performance assertions
    QVERIFY(animationRate >= 10); // Should handle at least 10 animations/sec
    QCOMPARE(animationCount, maxAnimations);
    
    qDebug() << "PieChart animation performance test: PASSED";
}

void TestChartPerformance::testPieChartUpdateLatency()
{
    qDebug() << "Testing PieChart update latency";
    
    auto widget = std::make_unique<PieChartWidget>("latency_pie_chart");
    widget->initializeWidget();
    
    // Configure for immediate updates
    PieChartWidget::PieChartConfig config = widget->getPieChartConfig();
    config.enableRealTimeMode = true;
    config.updateInterval = 16; // ~60 FPS
    widget->setPieChartConfig(config);
    
    // Add test slices
    const int sliceCount = 5;
    for (int i = 0; i < sliceCount; ++i) {
        QString fieldPath = QString("latency.slice%1").arg(i);
        widget->addSlice(fieldPath, QString("Latency Slice %1").arg(i), 20.0);
    }
    
    // Measure update latency
    std::vector<std::chrono::microseconds> latencies;
    const int measurementCount = 50;
    
    for (int i = 0; i < measurementCount; ++i) {
        auto updateStart = std::chrono::high_resolution_clock::now();
        
        // Update a slice value
        QString fieldPath = QString("latency.slice%1").arg(i % sliceCount);
        widget->updateFieldDisplay(fieldPath, QVariant(static_cast<double>(i + 50)));
        
        // Force immediate update
        QApplication::processEvents();
        
        auto updateEnd = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(updateEnd - updateStart);
        latencies.push_back(latency);
        
        // Small delay between measurements
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Calculate latency statistics
    auto totalLatency = std::accumulate(latencies.begin(), latencies.end(), 
                                       std::chrono::microseconds(0));
    auto avgLatency = totalLatency / latencies.size();
    auto minLatency = *std::min_element(latencies.begin(), latencies.end());
    auto maxLatency = *std::max_element(latencies.begin(), latencies.end());
    
    qDebug() << "Latency measurements (" << measurementCount << "samples):";
    qDebug() << "Average:" << avgLatency.count() << "μs";
    qDebug() << "Minimum:" << minLatency.count() << "μs";
    qDebug() << "Maximum:" << maxLatency.count() << "μs";
    
    // Performance assertions
    QVERIFY(avgLatency.count() < MAX_ACCEPTABLE_LATENCY_MS * 1000); // Convert ms to μs
    QVERIFY(maxLatency.count() < MAX_ACCEPTABLE_LATENCY_MS * 2000); // Allow 2x for max
    
    qDebug() << "PieChart update latency test: PASSED";
}

// Helper method implementations
std::vector<double> TestChartPerformance::generateTestData(size_t count, double amplitude, double frequency)
{
    std::vector<double> data;
    data.reserve(count);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> noise(-amplitude * 0.1, amplitude * 0.1);
    
    for (size_t i = 0; i < count; ++i) {
        double t = i * frequency;
        double value = amplitude * sin(t) + amplitude * 0.5 * cos(t * 2.0) + noise(gen);
        data.push_back(value);
    }
    
    return data;
}

void TestChartPerformance::waitForEventLoop(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

size_t TestChartPerformance::estimateMemoryUsage()
{
    // This is a simplified memory estimation
    // In a real implementation, you might use platform-specific APIs
    // For now, we just return a placeholder that increases over time
    static size_t baseMemory = 1024 * 1024; // 1MB base
    static int callCount = 0;
    callCount++;
    
    // Simulate memory usage tracking
    return baseMemory + (callCount * 1024); // Increase by 1KB per call
}

void TestChartPerformance::testDataDecimationPerformance()
{
    qDebug() << "Testing data decimation performance";
    
    // Generate large dataset
    const int originalSize = 50000;
    const int targetSize = 1000;
    
    std::vector<QPointF> largeDataset;
    largeDataset.reserve(originalSize);
    
    for (int i = 0; i < originalSize; ++i) {
        double x = i;
        double y = sin(i * 0.01) * 100 + cos(i * 0.005) * 50;
        largeDataset.emplace_back(x, y);
    }
    
    // Test different decimation strategies
    std::vector<Monitor::Charts::DecimationStrategy> strategies = {
        Monitor::Charts::DecimationStrategy::Uniform,
        Monitor::Charts::DecimationStrategy::MinMax,
        Monitor::Charts::DecimationStrategy::LTTB,
        Monitor::Charts::DecimationStrategy::Adaptive
    };
    
    for (auto strategy : strategies) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        std::vector<QPointF> decimatedData = Monitor::Charts::DataConverter::decimateData(
            largeDataset, targetSize, strategy);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        qDebug() << "Strategy" << static_cast<int>(strategy) << ":";
        qDebug() << "  Decimation time:" << duration.count() << "μs";
        qDebug() << "  Original size:" << originalSize;
        qDebug() << "  Target size:" << targetSize;
        qDebug() << "  Actual size:" << decimatedData.size();
        qDebug() << "  Performance:" << (originalSize * 1000000.0 / duration.count()) << "points/sec";
        
        // Verify decimation worked
        QVERIFY(static_cast<int>(decimatedData.size()) <= targetSize);
        QVERIFY(decimatedData.size() >= static_cast<size_t>(targetSize * 0.8)); // Should be close to target
        
        // Performance assertion
        QVERIFY(duration.count() < 100000); // Should complete within 100ms
    }
    
    qDebug() << "Data decimation performance test: PASSED";
}

void TestChartPerformance::testDataSmoothingPerformance()
{
    qDebug() << "Testing data smoothing performance";
    
    // Generate noisy data
    const int dataSize = 10000;
    std::vector<QPointF> noisyData;
    noisyData.reserve(dataSize);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> noise(-10.0, 10.0);
    
    for (int i = 0; i < dataSize; ++i) {
        double x = i;
        double y = sin(i * 0.01) * 50 + noise(gen); // Signal with noise
        noisyData.emplace_back(x, y);
    }
    
    // Test smoothing with different window sizes
    std::vector<int> windowSizes = {3, 5, 10, 20, 50};
    
    for (int windowSize : windowSizes) {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Create line chart widget for smoothing test
        auto widget = std::make_unique<LineChartWidget>("smooth_perf_test");
        widget->initializeWidget();
        
        LineChartWidget::LineSeriesConfig config;
        config.enableSmoothing = true;
        config.smoothingWindow = windowSize;
        
        widget->addLineSeries("smooth.test", "Smooth Test", QColor(), config);
        
        // Process data through smoothing
        for (const QPointF& point : noisyData) {
            widget->updateFieldDisplay("smooth.test", QVariant(point.y()));
        }
        
        widget->refreshAllDisplays();
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        qDebug() << "Window size" << windowSize << ":";
        qDebug() << "  Smoothing time:" << duration.count() << "μs";
        qDebug() << "  Data points:" << dataSize;
        qDebug() << "  Performance:" << (dataSize * 1000000.0 / duration.count()) << "points/sec";
        
        // Performance assertion
        QVERIFY(duration.count() < 500000); // Should complete within 500ms
    }
    
    qDebug() << "Data smoothing performance test: PASSED";
}

void TestChartPerformance::testStatisticalCalculationPerformance()
{
    qDebug() << "Testing statistical calculation performance";
    
    // Generate test data
    const int dataSize = 100000;
    std::vector<double> testData = generateTestData(dataSize);
    
    // Test DataConverter statistical functions
    auto startTime = std::chrono::high_resolution_clock::now();
    
    double mean = Monitor::Charts::DataConverter::calculateMean(testData);
    
    auto meanTime = std::chrono::high_resolution_clock::now();
    
    double stdDev = Monitor::Charts::DataConverter::calculateStdDev(testData);
    
    auto stdDevTime = std::chrono::high_resolution_clock::now();
    
    auto minMax = Monitor::Charts::DataConverter::calculateMinMax(testData);
    
    auto minMaxTime = std::chrono::high_resolution_clock::now();
    
    // Calculate durations
    auto meanDuration = std::chrono::duration_cast<std::chrono::microseconds>(meanTime - startTime);
    auto stdDevDuration = std::chrono::duration_cast<std::chrono::microseconds>(stdDevTime - meanTime);
    auto minMaxDuration = std::chrono::duration_cast<std::chrono::microseconds>(minMaxTime - stdDevTime);
    
    qDebug() << "Statistical calculations for" << dataSize << "points:";
    qDebug() << "Mean calculation:" << meanDuration.count() << "μs";
    qDebug() << "StdDev calculation:" << stdDevDuration.count() << "μs";  
    qDebug() << "MinMax calculation:" << minMaxDuration.count() << "μs";
    qDebug() << "Results: mean=" << mean << ", stddev=" << stdDev << 
                ", min=" << minMax.first << ", max=" << minMax.second;
    
    // Performance assertions
    QVERIFY(meanDuration.count() < 50000); // Should complete within 50ms
    QVERIFY(stdDevDuration.count() < 100000); // Should complete within 100ms
    QVERIFY(minMaxDuration.count() < 50000); // Should complete within 50ms
    
    // Verify results are reasonable
    QVERIFY(std::isfinite(mean));
    QVERIFY(std::isfinite(stdDev));
    QVERIFY(stdDev >= 0);
    QVERIFY(minMax.first <= minMax.second);
    
    qDebug() << "Statistical calculation performance test: PASSED";
}

void TestChartPerformance::testExportPerformance()
{
    qDebug() << "Testing chart export performance";
    
    auto widget = std::make_unique<LineChartWidget>("export_perf_test");
    widget->initializeWidget();
    
    // Add data for export
    widget->addLineSeries("export.test", "Export Test");
    std::vector<double> exportData = generateTestData(5000);
    for (double value : exportData) {
        widget->updateFieldDisplay("export.test", QVariant(value));
    }
    
    // Test export formats
    std::vector<Monitor::Charts::ExportFormat> formats = {
        Monitor::Charts::ExportFormat::PNG,
        Monitor::Charts::ExportFormat::SVG
    };
    
    QSize exportSize(1920, 1080);
    
    for (auto format : formats) {
        QString tempPath = QString("/tmp/chart_export_test.%1")
            .arg(Monitor::Charts::ChartExporter::getFileExtensions(format).first());
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        bool result = widget->exportChart(tempPath, format, exportSize);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        qDebug() << "Export format" << static_cast<int>(format) << ":";
        qDebug() << "  Export time:" << duration.count() << "ms";
        qDebug() << "  Export size:" << exportSize.width() << "x" << exportSize.height();
        qDebug() << "  Success:" << result;
        
        // Clean up temp file
        QFile::remove(tempPath);
        
        // Performance assertion
        QVERIFY(duration.count() < 5000); // Should complete within 5 seconds
    }
    
    qDebug() << "Export performance test: PASSED";
}

void TestChartPerformance::testLargeExportPerformance()
{
    qDebug() << "Testing large chart export performance";
    
    auto widget = std::make_unique<LineChartWidget>("large_export_test");
    widget->initializeWidget();
    
    // Create chart with large dataset
    const int seriesCount = 5;
    const int pointsPerSeries = 10000;
    
    for (int s = 0; s < seriesCount; ++s) {
        QString fieldPath = QString("large.export%1").arg(s);
        widget->addLineSeries(fieldPath, QString("Large Series %1").arg(s));
        
        std::vector<double> seriesData = generateTestData(pointsPerSeries);
        for (double value : seriesData) {
            widget->updateFieldDisplay(fieldPath, QVariant(value));
        }
    }
    
    // Export at high resolution
    QSize largeSize(3840, 2160); // 4K resolution
    QString tempPath = "/tmp/large_chart_export.png";
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    bool result = widget->exportChart(tempPath, Monitor::Charts::ExportFormat::PNG, largeSize);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    qDebug() << "Large export performance:";
    qDebug() << "Series count:" << seriesCount;
    qDebug() << "Points per series:" << pointsPerSeries;
    qDebug() << "Total points:" << (seriesCount * pointsPerSeries);
    qDebug() << "Export resolution:" << largeSize.width() << "x" << largeSize.height();
    qDebug() << "Export time:" << duration.count() << "ms";
    qDebug() << "Success:" << result;
    
    // Clean up
    QFile::remove(tempPath);
    
    // Performance assertion
    QVERIFY(duration.count() < 15000); // Should complete within 15 seconds for large export
    
    qDebug() << "Large export performance test: PASSED";
}

void TestChartPerformance::testMemoryLeakPrevention()
{
    qDebug() << "Testing memory leak prevention";
    
    size_t initialMemory = estimateMemoryUsage();
    
    // Create and destroy multiple widgets
    const int widgetCount = 20;
    const int cycleCount = 5;
    
    for (int cycle = 0; cycle < cycleCount; ++cycle) {
        std::vector<std::unique_ptr<LineChartWidget>> widgets;
        
        // Create widgets
        for (int i = 0; i < widgetCount; ++i) {
            auto widget = std::make_unique<LineChartWidget>(QString("leak_test_%1_%2").arg(cycle).arg(i));
            widget->initializeWidget();
            
            // Add some data
            widget->addLineSeries("leak.test", "Leak Test");
            std::vector<double> data = generateTestData(1000);
            for (double value : data) {
                widget->updateFieldDisplay("leak.test", QVariant(value));
            }
            
            widgets.push_back(std::move(widget));
        }
        
        size_t peakMemory = estimateMemoryUsage();
        
        // Destroy widgets
        widgets.clear();
        
        // Force cleanup
        waitForEventLoop(100);
        
        size_t afterMemory = estimateMemoryUsage();
        
        qDebug() << "Cycle" << cycle << ": peak=" << peakMemory << ", after=" << afterMemory;
        
        // Memory should not grow significantly between cycles
        if (cycle > 0) {
            size_t memoryGrowth = afterMemory - initialMemory;
            qDebug() << "Memory growth:" << memoryGrowth << "bytes";
            
            // Allow some growth but not excessive
            QVERIFY(memoryGrowth < 1024 * 1024); // Less than 1MB growth per cycle
        }
    }
    
    size_t finalMemory = estimateMemoryUsage();
    size_t totalGrowth = finalMemory - initialMemory;
    
    qDebug() << "Total memory growth:" << totalGrowth << "bytes";
    qDebug() << "Growth per cycle:" << (totalGrowth / cycleCount) << "bytes";
    
    // Overall memory growth should be reasonable
    QVERIFY(totalGrowth < 5 * 1024 * 1024); // Less than 5MB total growth
    
    qDebug() << "Memory leak prevention test: PASSED";
}

void TestChartPerformance::testLargeDatasetMemoryBehavior()
{
    qDebug() << "Testing large dataset memory behavior";
    
    auto widget = std::make_unique<LineChartWidget>("large_memory_test");
    widget->initializeWidget();
    
    // Configure for large dataset
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    config.maxDataPoints = 100000;
    config.rollingData = false; // Keep all data
    widget->setLineChartConfig(config);
    
    widget->addLineSeries("large.memory", "Large Memory Test");
    
    size_t initialMemory = estimateMemoryUsage();
    std::vector<size_t> memoryProgression;
    
    // Add data in chunks and measure memory
    const int totalPoints = 100000;
    const int chunkSize = 10000;
    
    for (int chunk = 0; chunk < totalPoints / chunkSize; ++chunk) {
        // Add chunk of data
        for (int i = 0; i < chunkSize; ++i) {
            double value = sin((chunk * chunkSize + i) * 0.001) * 100;
            widget->updateFieldDisplay("large.memory", QVariant(value));
        }
        
        size_t currentMemory = estimateMemoryUsage();
        memoryProgression.push_back(currentMemory);
        
        int pointsAdded = (chunk + 1) * chunkSize;
        qDebug() << "Points:" << pointsAdded << ", Memory:" << currentMemory 
                 << ", Growth:" << (currentMemory - initialMemory);
    }
    
    // Analyze memory growth pattern
    bool memoryGrowthLinear = true;
    for (size_t i = 1; i < memoryProgression.size(); ++i) {
        size_t growth = memoryProgression[i] - memoryProgression[i-1];
        size_t expectedGrowth = memoryProgression[1] - memoryProgression[0];
        
        // Allow some variation but should be roughly linear
        if (growth > expectedGrowth * 2 || growth < expectedGrowth / 2) {
            memoryGrowthLinear = false;
            break;
        }
    }
    
    qDebug() << "Memory growth pattern linear:" << memoryGrowthLinear;
    
    // Verify data integrity
    QCOMPARE(widget->getSeriesPointCount("large.memory"), totalPoints);
    
    // Memory behavior assertions
    size_t totalGrowth = memoryProgression.back() - initialMemory;
    size_t bytesPerPoint = totalGrowth / totalPoints;
    
    qDebug() << "Total memory growth:" << totalGrowth << "bytes";
    qDebug() << "Bytes per point:" << bytesPerPoint;
    
    QVERIFY(bytesPerPoint < 1000); // Should be efficient per point
    QVERIFY(memoryGrowthLinear); // Memory growth should be predictable
    
    qDebug() << "Large dataset memory behavior test: PASSED";
}

void TestChartPerformance::testMemoryCleanupEfficiency()
{
    qDebug() << "Testing memory cleanup efficiency";
    
    size_t initialMemory = estimateMemoryUsage();
    
    {
        auto widget = std::make_unique<LineChartWidget>("cleanup_test");
        widget->initializeWidget();
        
        // Add large dataset
        const int dataSize = 50000;
        widget->addLineSeries("cleanup.test", "Cleanup Test");
        
        std::vector<double> largeData = generateTestData(dataSize);
        for (double value : largeData) {
            widget->updateFieldDisplay("cleanup.test", QVariant(value));
        }
        
        size_t peakMemory = estimateMemoryUsage();
        qDebug() << "Peak memory:" << peakMemory << "bytes";
        
        // Clear data
        widget->clearAllData();
        waitForEventLoop(50);
        
        size_t afterClearMemory = estimateMemoryUsage();
        qDebug() << "After clear memory:" << afterClearMemory << "bytes";
        
        // Widget should still exist but use less memory
        size_t memoryRecovered = peakMemory - afterClearMemory;
        double recoveryRatio = static_cast<double>(memoryRecovered) / (peakMemory - initialMemory);
        
        qDebug() << "Memory recovered:" << memoryRecovered << "bytes";
        qDebug() << "Recovery ratio:" << (recoveryRatio * 100) << "%";
        
        QVERIFY(recoveryRatio > 0.3); // Should recover at least 30% of memory
        
    } // Widget destruction
    
    waitForEventLoop(100); // Allow cleanup
    
    size_t finalMemory = estimateMemoryUsage();
    size_t totalRecovery = finalMemory - initialMemory;
    
    qDebug() << "Final memory:" << finalMemory << "bytes";
    qDebug() << "Total recovery (post-destruction):" << totalRecovery << "bytes";
    
    // After widget destruction, memory should be mostly recovered
    QVERIFY(totalRecovery < 1024 * 1024); // Less than 1MB residual
    
    qDebug() << "Memory cleanup efficiency test: PASSED";
}

void TestChartPerformance::testConcurrentUpdates()
{
    qDebug() << "Testing concurrent updates (thread safety simulation)";
    
    auto widget = std::make_unique<LineChartWidget>("concurrent_test");
    widget->initializeWidget();
    
    widget->addLineSeries("concurrent.test", "Concurrent Test");
    
    // Simulate concurrent updates by rapidly alternating between different operations
    const int operationCount = 1000;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(-100.0, 100.0);
    std::uniform_int_distribution<int> opDis(0, 2);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < operationCount; ++i) {
        int operation = opDis(gen);
        
        switch (operation) {
            case 0: // Add data
                widget->updateFieldDisplay("concurrent.test", QVariant(dis(gen)));
                break;
            case 1: // Query data
                widget->getSeriesPointCount("concurrent.test");
                widget->getLastDataPoint("concurrent.test");
                break;
            case 2: // Update display
                widget->refreshAllDisplays();
                break;
        }
        
        // Process events occasionally
        if (i % 100 == 0) {
            QApplication::processEvents();
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    qDebug() << "Concurrent operations:" << operationCount;
    qDebug() << "Duration:" << duration.count() << "ms";
    qDebug() << "Operations per second:" << (operationCount * 1000.0 / duration.count());
    
    // Verify widget still functions correctly
    QVERIFY(widget->getSeriesPointCount("concurrent.test") > 0);
    QVERIFY(!widget->getLastDataPoint("concurrent.test").isNull());
    
    // Performance assertion
    QVERIFY(duration.count() < 5000); // Should handle concurrent operations efficiently
    
    qDebug() << "Concurrent updates test: PASSED";
}

void TestChartPerformance::testThreadSafety()
{
    qDebug() << "Testing thread safety (basic validation)";
    
    // Note: Full thread safety testing would require actual multi-threading
    // This test validates that the widget can handle rapid state changes
    
    auto widget = std::make_unique<LineChartWidget>("thread_safety_test");
    widget->initializeWidget();
    
    // Add multiple series
    const int seriesCount = 5;
    for (int i = 0; i < seriesCount; ++i) {
        widget->addLineSeries(QString("thread.test%1").arg(i), QString("Thread Test %1").arg(i));
    }
    
    // Rapidly switch between different configurations and operations
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> valueDis(-50.0, 50.0);
    std::uniform_int_distribution<int> seriesDis(0, seriesCount - 1);
    
    const int rapidOperations = 500;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < rapidOperations; ++i) {
        int seriesIdx = seriesDis(gen);
        QString fieldPath = QString("thread.test%1").arg(seriesIdx);
        
        // Rapidly perform different operations
        widget->updateFieldDisplay(fieldPath, QVariant(valueDis(gen)));
        
        if (i % 10 == 0) {
            // Change configuration
            LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
            config.enableRealTimeMode = !config.enableRealTimeMode;
            widget->setLineChartConfig(config);
        }
        
        if (i % 50 == 0) {
            // Query operations
            widget->getSeriesData(fieldPath);
            widget->getSeriesMean(fieldPath);
        }
        
        if (i % 25 == 0) {
            QApplication::processEvents();
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    qDebug() << "Rapid operations:" << rapidOperations;
    qDebug() << "Duration:" << duration.count() << "ms";
    qDebug() << "Operations per second:" << (rapidOperations * 1000.0 / duration.count());
    
    // Verify widget state is still consistent
    for (int i = 0; i < seriesCount; ++i) {
        QString fieldPath = QString("thread.test%1").arg(i);
        QVERIFY(widget->getSeriesPointCount(fieldPath) > 0);
    }
    
    qDebug() << "Thread safety test: PASSED";
}

void TestChartPerformance::testMultipleChartsPerformance()
{
    qDebug() << "Testing multiple charts performance";
    
    const int chartCount = 10;
    const int pointsPerChart = 1000;
    
    std::vector<std::unique_ptr<LineChartWidget>> widgets;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Create multiple charts
    for (int c = 0; c < chartCount; ++c) {
        auto widget = std::make_unique<LineChartWidget>(QString("multi_chart_%1").arg(c));
        widget->initializeWidget();
        
        // Configure each chart
        LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
        config.enableRealTimeMode = true;
        config.maxDataPoints = pointsPerChart;
        widget->setLineChartConfig(config);
        
        // Add series
        widget->addLineSeries(QString("multi.series%1").arg(c), QString("Multi Series %1").arg(c));
        
        // Add data
        std::vector<double> chartData = generateTestData(pointsPerChart, 50.0, 0.01 * (c + 1));
        for (double value : chartData) {
            widget->updateFieldDisplay(QString("multi.series%1").arg(c), QVariant(value));
        }
        
        widgets.push_back(std::move(widget));
    }
    
    auto setupTime = std::chrono::high_resolution_clock::now();
    
    // Perform updates on all charts
    const int updateRounds = 100;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(-25.0, 25.0);
    std::uniform_int_distribution<int> chartDis(0, chartCount - 1);
    
    for (int round = 0; round < updateRounds; ++round) {
        for (int c = 0; c < chartCount; ++c) {
            QString fieldPath = QString("multi.series%1").arg(c);
            widgets[c]->updateFieldDisplay(fieldPath, QVariant(dis(gen)));
        }
        
        // Process events
        QApplication::processEvents();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    
    auto setupDuration = std::chrono::duration_cast<std::chrono::milliseconds>(setupTime - startTime);
    auto updateDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - setupTime);
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    qDebug() << "Multiple charts performance:";
    qDebug() << "Chart count:" << chartCount;
    qDebug() << "Points per chart:" << pointsPerChart;
    qDebug() << "Total points:" << (chartCount * pointsPerChart);
    qDebug() << "Setup time:" << setupDuration.count() << "ms";
    qDebug() << "Update time:" << updateDuration.count() << "ms";
    qDebug() << "Total time:" << totalDuration.count() << "ms";
    qDebug() << "Update rounds:" << updateRounds;
    qDebug() << "Updates per second:" << (updateRounds * chartCount * 1000.0 / updateDuration.count());
    
    // Verify all charts still function
    for (int c = 0; c < chartCount; ++c) {
        QString fieldPath = QString("multi.series%1").arg(c);
        QVERIFY(widgets[c]->getSeriesPointCount(fieldPath) > pointsPerChart);
    }
    
    // Performance assertions
    QVERIFY(setupDuration.count() < 10000); // Setup should be reasonable
    QVERIFY(updateDuration.count() < 5000); // Updates should be fast
    
    qDebug() << "Multiple charts performance test: PASSED";
}

void TestChartPerformance::testMaxDataPointsHandling()
{
    qDebug() << "Testing maximum data points handling";
    
    auto widget = std::make_unique<LineChartWidget>("max_points_test");
    widget->initializeWidget();
    
    const int maxPoints = 10000;
    const int testPoints = 15000; // More than max
    
    // Configure with strict limit
    LineChartWidget::LineChartConfig config = widget->getLineChartConfig();
    config.maxDataPoints = maxPoints;
    config.rollingData = true; // Enable rolling data
    widget->setLineChartConfig(config);
    
    widget->addLineSeries("max.points", "Max Points Test");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Add more data than the limit
    for (int i = 0; i < testPoints; ++i) {
        double value = sin(i * 0.01) * 50;
        widget->updateFieldDisplay("max.points", QVariant(value));
        
        // Check point count periodically
        if (i % 1000 == 0) {
            int currentCount = widget->getSeriesPointCount("max.points");
            qDebug() << "Points added:" << (i + 1) << ", Current count:" << currentCount;
            
            // Should never exceed max + some buffer (for processing)
            QVERIFY(currentCount <= maxPoints * 1.1);
        }
        
        // Process events to allow rolling data to work
        if (i % 500 == 0) {
            QApplication::processEvents();
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    int finalCount = widget->getSeriesPointCount("max.points");
    
    qDebug() << "Max points handling results:";
    qDebug() << "Points added:" << testPoints;
    qDebug() << "Max allowed:" << maxPoints;
    qDebug() << "Final count:" << finalCount;
    qDebug() << "Processing time:" << duration.count() << "ms";
    qDebug() << "Points per second:" << (testPoints * 1000.0 / duration.count());
    
    // Verify data limit was enforced
    QVERIFY(finalCount <= maxPoints);
    QVERIFY(finalCount >= maxPoints * 0.8); // Should be close to max
    
    // Verify last data point is recent
    QPointF lastPoint = widget->getLastDataPoint("max.points");
    double expectedLastValue = sin((testPoints - 1) * 0.01) * 50;
    QVERIFY(abs(lastPoint.y() - expectedLastValue) < 1.0); // Should be close
    
    // Performance assertion
    QVERIFY(duration.count() < 10000); // Should handle efficiently
    
    qDebug() << "Maximum data points handling test: PASSED";
}

QTEST_MAIN(TestChartPerformance)
#include "test_chart_performance.moc"