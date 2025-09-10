#ifndef LINE_CHART_WIDGET_H
#define LINE_CHART_WIDGET_H

#include "chart_widget.h"
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QDateTime>
#include <QTimer>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointF>
#include <vector>
#include <deque>
#include <unordered_map>


/**
 * @brief Line chart widget for displaying time series and numeric data
 * 
 * LineChartWidget provides comprehensive line chart functionality:
 * - Multiple series support with independent configuration
 * - Real-time data streaming with configurable history depth
 * - Various line styles (solid, dashed, dotted, spline)
 * - Point markers with customizable shapes and sizes
 * - Auto-scaling and manual axis configuration
 * - Zoom and pan with data decimation for performance
 * - X-axis support: numeric, time-based, or packet sequence
 * - Interactive tooltips and crosshair
 * - Data export and chart export functionality
 * 
 * Performance Features:
 * - Data decimation for datasets > 10K points
 * - Viewport-based rendering optimization
 * - Efficient incremental updates
 * - Adaptive performance scaling based on FPS
 * - Memory-efficient circular buffers
 * 
 * Interactive Features:
 * - Zoom with mouse wheel or rubber band selection
 * - Pan with mouse drag
 * - Series visibility toggle via legend
 * - Point value tooltips on hover
 * - Crosshair cursor for precise value reading
 */
class LineChartWidget : public ChartWidget
{
    Q_OBJECT

public:
    /**
     * @brief Line chart specific configuration
     */
    struct LineChartConfig {
        // Line appearance
        enum class LineStyle {
            Solid,
            Dash,
            Dot,
            DashDot,
            DashDotDot
        };
        
        enum class PointStyle {
            None,
            Circle,
            Square,
            Triangle,
            Diamond,
            Cross,
            Plus
        };
        
        enum class InterpolationMethod {
            Linear,         // Straight lines between points
            Spline,         // Smooth curved lines
            Step           // Step function (constant value until next point)
        };
        
        // Data management
        int maxDataPoints = 10000;     // Maximum points per series
        bool rollingData = true;       // Remove old points when limit reached
        int historyDepth = 1000;       // Points to keep in memory for analysis
        
        // X-axis configuration
        enum class XAxisType {
            PacketSequence,    // Use packet sequence numbers
            Timestamp,         // Use packet timestamps
            FieldValue        // Use value of specified field
        };
        XAxisType xAxisType = XAxisType::PacketSequence;
        QString xAxisFieldPath;        // Field path for FieldValue mode
        
        // Visual appearance
        LineStyle defaultLineStyle = LineStyle::Solid;
        PointStyle defaultPointStyle = PointStyle::None;
        InterpolationMethod interpolation = InterpolationMethod::Linear;
        int defaultLineWidth = 2;
        double defaultPointSize = 6.0;
        bool showPoints = false;
        bool connectPoints = true;
        
        // Auto-scaling
        bool autoScaleX = true;
        bool autoScaleY = true;
        double yAxisMarginPercent = 5.0;   // Margin around Y data
        double xAxisMarginPercent = 2.0;   // Margin around X data
        
        // Real-time features
        bool enableRealTimeMode = true;    // Auto-scroll to show latest data
        bool enableCrosshair = false;      // Show crosshair cursor
        bool enableValueLabels = false;    // Show value labels on points
        
        LineChartConfig() = default;
    };
    
    /**
     * @brief Series-specific configuration for line charts
     */
    struct LineSeriesConfig {
        LineChartConfig::LineStyle lineStyle;
        LineChartConfig::PointStyle pointStyle;
        LineChartConfig::InterpolationMethod interpolation;
        int lineWidth;
        double pointSize;
        bool showPoints;
        bool connectPoints;
        QColor fillColor;
        double fillOpacity;
        
        // Data processing
        bool enableSmoothing;
        int smoothingWindow;
        
        LineSeriesConfig() {
            lineStyle = LineChartConfig::LineStyle::Solid;
            pointStyle = LineChartConfig::PointStyle::None;
            interpolation = LineChartConfig::InterpolationMethod::Linear;
            lineWidth = 2;
            pointSize = 6.0;
            showPoints = false;
            connectPoints = true;
            fillColor = QColor(0, 0, 0, 0);    // Transparent = no fill
            fillOpacity = 0.3;
            enableSmoothing = false;
            smoothingWindow = 5;
        }
        LineSeriesConfig(const QJsonObject& json);
        QJsonObject toJson() const;
    };

    explicit LineChartWidget(const QString& widgetId, QWidget* parent = nullptr);
    ~LineChartWidget() override;

    // Line chart specific configuration
    void setLineChartConfig(const LineChartConfig& config);
    LineChartConfig getLineChartConfig() const { return m_lineConfig; }
    void resetLineChartConfig();

    // Series management
    bool addLineSeries(const QString& fieldPath, const QString& seriesName = QString(),
                      const QColor& color = QColor(), const LineSeriesConfig& config = LineSeriesConfig());
    void setLineSeriesConfig(const QString& fieldPath, const LineSeriesConfig& config);
    LineSeriesConfig getLineSeriesConfig(const QString& fieldPath) const;

    // Data access
    std::vector<QPointF> getSeriesData(const QString& fieldPath) const;
    std::vector<QPointF> getSeriesDataInRange(const QString& fieldPath, double xMin, double xMax) const;
    QPointF getLastDataPoint(const QString& fieldPath) const;
    int getSeriesPointCount(const QString& fieldPath) const;

    // Axis control
    void setXAxisFieldPath(const QString& fieldPath);
    QString getXAxisFieldPath() const { return m_lineConfig.xAxisFieldPath; }
    void setXAxisType(LineChartConfig::XAxisType type);
    LineChartConfig::XAxisType getXAxisType() const { return m_lineConfig.xAxisType; }

    // Real-time control
    void setRealTimeMode(bool enabled);
    bool isRealTimeMode() const { return m_lineConfig.enableRealTimeMode; }
    void scrollToLatest();
    void clearSeriesData(const QString& fieldPath);
    void clearAllData();

    // Analysis functions
    QPair<double, double> getYRange() const;
    QPair<double, double> getXRange() const;
    double getSeriesMean(const QString& fieldPath) const;
    double getSeriesStdDev(const QString& fieldPath) const;

public slots:
    void onToggleRealTimeMode(bool enabled);
    void onClearData();
    void onScrollToLatest();
    void onToggleCrosshair(bool enabled);
    void onChangeInterpolationMethod(int method);

signals:
    void dataPointAdded(const QString& fieldPath, QPointF point);
    void seriesDataCleared(const QString& fieldPath);
    void realTimeModeChanged(bool enabled);
    void crosshairPositionChanged(QPointF position);

protected:
    // ChartWidget implementation
    void createChart() override;
    void updateSeriesData() override;
    void configureSeries(const QString& fieldPath, const SeriesConfig& config) override;
    QAbstractSeries* createSeriesForField(const QString& fieldPath, const SeriesConfig& config) override;
    void removeSeriesForField(const QString& fieldPath) override;

    // DisplayWidget implementation  
    void updateFieldDisplay(const QString& fieldPath, const QVariant& value) override;
    void clearFieldDisplay(const QString& fieldPath) override;
    void refreshAllDisplays() override;

    // Settings implementation
    QJsonObject saveWidgetSpecificSettings() const override;
    bool restoreWidgetSpecificSettings(const QJsonObject& settings) override;
    
    // Event handling
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

    // Context menu implementation
    void setupContextMenu() override;


private slots:
    void onSeriesHovered(const QPointF& point, bool state);
    void onRealTimeUpdate();
    void onSeriesClicked(const QPointF& point);

private:
    /**
     * @brief Internal data storage for each series
     */
    struct SeriesData {
        std::deque<QPointF> points;                    // Point data (circular buffer)
        std::deque<QVariant> rawValues;               // Raw field values
        std::deque<std::chrono::steady_clock::time_point> timestamps; // Timestamps
        QLineSeries* lineSeries = nullptr;            // Qt line series
        QScatterSeries* pointSeries = nullptr;       // Qt scatter series for points
        QSplineSeries* splineSeries = nullptr;        // Qt spline series
        LineSeriesConfig config;                      // Series configuration
        double lastXValue = 0.0;                      // Last X value for sequence mode
        bool needsUpdate = false;                     // Flag for batched updates
        
        SeriesData() = default;
        ~SeriesData() = default;
        
        void addPoint(const QPointF& point, const QVariant& rawValue);
        void addPoint(double x, double y, const QVariant& rawValue);
        void clearData();
        std::vector<QPointF> getPointsInRange(double xMin, double xMax) const;
        void limitDataPoints(int maxPoints);
    };

    // Configuration
    LineChartConfig m_lineConfig;
    std::unordered_map<QString, LineSeriesConfig> m_lineSeriesConfigs;
    
    // Data storage
    std::unordered_map<QString, std::unique_ptr<SeriesData>> m_seriesData;
    
    // UI components
    QTimer* m_realTimeTimer;
    QCheckBox* m_realTimeModeCheckBox;
    QComboBox* m_interpolationCombo;
    QSpinBox* m_maxPointsSpin;
    
    // State
    int m_packetSequence;
    QPointF m_crosshairPosition;
    bool m_showingTooltip;
    
    // Helper methods
    void setupToolbarExtensions();
    void updateRealTimeSettings();
    void updateAxes();
    void updateSeriesAppearance(const QString& fieldPath);
    void applyLineSeriesConfig(QLineSeries* series, const LineSeriesConfig& config);
    void applyScatterSeriesConfig(QScatterSeries* series, const LineSeriesConfig& config);
    void applySplineSeriesConfig(QSplineSeries* series, const LineSeriesConfig& config);
    
    // Data processing
    double calculateXValue(const QVariant& fieldValue, const QString& fieldPath);
    QPointF createDataPoint(const QString& fieldPath, const QVariant& fieldValue);
    void decimateSeriesData(const QString& fieldPath);
    std::vector<QPointF> smoothData(const std::vector<QPointF>& data, int windowSize) const;
    
    // Auto-scaling
    void autoScaleAxes();
    QPair<double, double> calculateDataBounds(Qt::Orientation orientation) const;
    
    // Real-time features
    void scrollToShowLatestData();
    void updateCrosshair(const QPointF& position);
    void showPointTooltip(const QPointF& chartPos, const QString& fieldPath, const QPointF& dataPos);
    
    // Visual styling
    Qt::PenStyle lineStyleToPenStyle(LineChartConfig::LineStyle style) const;
    void configureSeriesMarkers(QLineSeries* series, LineChartConfig::PointStyle style) const;
    void configureSeriesMarkers(QScatterSeries* series, LineChartConfig::PointStyle style) const;
    
    // Performance optimization
    bool shouldDecimateData(const QString& fieldPath) const;
    void performDataDecimation();
    
    // Mouse interaction helpers
    void handleMouseMove(const QPoint& position);
    void handleMouseClick(const QPoint& position);
    void optimizeViewport();
};

#endif // LINE_CHART_WIDGET_H