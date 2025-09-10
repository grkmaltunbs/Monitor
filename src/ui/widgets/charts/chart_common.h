#ifndef CHART_COMMON_H
#define CHART_COMMON_H

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLegend>
#include <QColor>
#include <QFont>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QJsonObject>
#include <QEasingCurve>
#include <vector>
#include <memory>

/**
 * @brief Common utilities and constants for chart widgets
 * 
 * This header provides shared functionality for all chart widget implementations:
 * - Color palette management
 * - Chart theme configuration
 * - Data conversion utilities
 * - Performance optimization helpers
 * - Export functionality
 * 
 * All chart widgets should use these utilities to maintain consistency
 * across the Monitor application.
 */
namespace Monitor {
namespace Charts {

/**
 * @brief Chart rendering performance levels
 */
enum class PerformanceLevel {
    High,           // Maximum quality, slower rendering
    Balanced,       // Good quality, reasonable performance
    Fast,          // Lower quality, maximum performance
    Adaptive       // Automatically adjust based on data size
};

/**
 * @brief Chart theme styles
 */
enum class ChartTheme {
    Light,
    Dark,
    BlueCerulean,
    Custom
};

/**
 * @brief Data point decimation strategies for large datasets
 */
enum class DecimationStrategy {
    None,           // No decimation
    Uniform,        // Uniform sampling
    MinMax,         // Min-max preserving
    LTTB,          // Largest Triangle Three Buckets
    Adaptive       // Adaptive based on zoom level
};

/**
 * @brief Chart export formats
 */
enum class ExportFormat {
    PNG,
    SVG,
    PDF,
    JPEG
};

/**
 * @brief Default color palette for chart series
 */
class ColorPalette {
public:
    static const std::vector<QColor>& getDefaultColors() {
        static const std::vector<QColor> colors = {
            QColor(31, 119, 180),   // Blue
            QColor(255, 127, 14),   // Orange
            QColor(44, 160, 44),    // Green
            QColor(214, 39, 40),    // Red
            QColor(148, 103, 189),  // Purple
            QColor(140, 86, 75),    // Brown
            QColor(227, 119, 194),  // Pink
            QColor(127, 127, 127),  // Gray
            QColor(188, 189, 34),   // Olive
            QColor(23, 190, 207),   // Cyan
            QColor(174, 199, 232),  // Light Blue
            QColor(255, 187, 120),  // Light Orange
            QColor(152, 223, 138),  // Light Green
            QColor(255, 152, 150),  // Light Red
            QColor(197, 176, 213),  // Light Purple
        };
        return colors;
    }

    static QColor getColor(int index) {
        const auto& colors = getDefaultColors();
        return colors[index % colors.size()];
    }

    static int getColorCount() {
        return static_cast<int>(getDefaultColors().size());
    }
};

/**
 * @brief Chart theme configuration
 */
struct ChartThemeConfig {
    QColor backgroundColor = QColor(255, 255, 255);
    QColor plotAreaColor = QColor(255, 255, 255);
    QColor gridLineColor = QColor(200, 200, 200);
    QColor axisLineColor = QColor(0, 0, 0);
    QColor axisLabelColor = QColor(0, 0, 0);
    QColor titleColor = QColor(0, 0, 0);
    QColor legendColor = QColor(0, 0, 0);
    QFont titleFont = QFont("Arial", 14, QFont::Bold);
    QFont labelFont = QFont("Arial", 10);
    QFont legendFont = QFont("Arial", 9);

    static ChartThemeConfig getTheme(ChartTheme theme);
    void applyToChart(QChart* chart) const;
};

/**
 * @brief Performance optimization configuration
 */
struct PerformanceConfig {
    PerformanceLevel level = PerformanceLevel::Balanced;
    int maxDataPoints = 10000;        // Points before decimation
    int maxSeriesCount = 20;          // Series before optimization
    bool useOpenGL = true;            // Use OpenGL acceleration
    bool enableAnimations = true;     // Chart animations
    DecimationStrategy decimation = DecimationStrategy::Adaptive;
    int updateThrottleMs = 16;        // ~60 FPS update limit

    static PerformanceConfig getConfig(PerformanceLevel level);
    void applyToChart(QChart* chart) const;
    void applyToView(QChartView* view) const;
};

/**
 * @brief Data conversion utilities
 */
class DataConverter {
public:
    // Convert QVariant to double for numeric charts
    static double toDouble(const QVariant& value, bool* ok = nullptr);
    
    // Convert QVariant to string for labels
    static QString toString(const QVariant& value);
    
    // Validate numeric data
    static bool isNumeric(const QVariant& value);
    
    // Get appropriate axis type for data
    static QString getAxisType(const QVariant& sampleValue);
    
    // Format value for display
    static QString formatValue(const QVariant& value, int decimalPlaces = 2);

    // Decimate data using specified strategy
    static std::vector<QPointF> decimateData(const std::vector<QPointF>& data, 
                                           int maxPoints, 
                                           DecimationStrategy strategy = DecimationStrategy::LTTB);

    // Statistical functions for data analysis
    static double calculateMean(const std::vector<double>& data);
    static double calculateStdDev(const std::vector<double>& data);
    static std::pair<double, double> calculateMinMax(const std::vector<double>& data);
};

/**
 * @brief Chart export utilities
 */
class ChartExporter {
public:
    // Export chart to file
    static bool exportChart(QChart* chart, const QString& filePath, ExportFormat format);
    
    // Export chart to image with custom size
    static bool exportChart(QChart* chart, const QString& filePath, 
                          ExportFormat format, const QSize& size);
    
    // Get supported file extensions for format
    static QStringList getFileExtensions(ExportFormat format);
    
    // Get file filter string for dialogs
    static QString getFileFilter();

private:
    static bool exportToPNG(QChart* chart, const QString& filePath, const QSize& size);
    static bool exportToSVG(QChart* chart, const QString& filePath, const QSize& size);
    static bool exportToPDF(QChart* chart, const QString& filePath, const QSize& size);
    static bool exportToJPEG(QChart* chart, const QString& filePath, const QSize& size);
};

/**
 * @brief Series factory for creating chart series
 */
class SeriesFactory {
public:
    // Create line series with configuration
    static QLineSeries* createLineSeries(const QString& name, const QColor& color);
    
    // Create bar series with configuration  
    static QBarSeries* createBarSeries(const QString& name);
    
    // Create pie series with configuration
    static QPieSeries* createPieSeries(const QString& name);
    
    // Configure series appearance
    static void configureLineSeries(QLineSeries* series, const QJsonObject& config);
    static void configureBarSeries(QBarSeries* series, const QJsonObject& config);
    static void configurePieSeries(QPieSeries* series, const QJsonObject& config);
};

/**
 * @brief Axis factory for creating chart axes
 */
class AxisFactory {
public:
    // Create value axis for numeric data
    static QValueAxis* createValueAxis(const QString& title, double min = 0, double max = 100);
    
    // Create category axis for categorical data
    static QCategoryAxis* createCategoryAxis(const QString& title, const QStringList& categories);
    
    // Create datetime axis for time series data
    static QDateTimeAxis* createDateTimeAxis(const QString& title);
    
    // Auto-configure axis range
    static void autoScaleAxis(QValueAxis* axis, const std::vector<double>& data, 
                            double marginPercent = 5.0);
};

} // namespace Charts
} // namespace Monitor

#endif // CHART_COMMON_H