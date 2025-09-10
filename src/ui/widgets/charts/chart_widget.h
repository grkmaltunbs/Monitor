#ifndef CHART_WIDGET_H
#define CHART_WIDGET_H

#include "../display_widget.h"
#include "chart_common.h"
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QAbstractSeries>
#include <QtCharts/QAbstractAxis>
#include <QtCharts/QLegend>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <QRubberBand>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPointF>
#include <QRectF>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <QMessageBox>
#include <memory>
#include <unordered_map>

/**
 * @brief Abstract base class for all chart widgets
 * 
 * ChartWidget provides common functionality for all chart-based displays:
 * - Chart view management with zoom/pan support
 * - Series management and configuration
 * - Axis management and auto-scaling
 * - Legend and title configuration
 * - Theme and appearance management
 * - Performance optimization features
 * - Export functionality
 * - Interactive features (tooltips, selection)
 * 
 * This class implements the display-related template methods from DisplayWidget
 * while leaving chart-specific implementation to concrete chart widgets.
 * 
 * Performance Features:
 * - Data decimation for large datasets
 * - Viewport-based rendering optimization
 * - Efficient series updates
 * - OpenGL acceleration support
 * - Adaptive performance scaling
 */
class ChartWidget : public DisplayWidget
{
    Q_OBJECT

public:
    /**
     * @brief Chart interaction modes
     */
    enum class InteractionMode {
        None,           // No interaction
        Pan,           // Pan only
        Zoom,          // Zoom only
        PanZoom,       // Both pan and zoom
        Select         // Data selection mode
    };

    /**
     * @brief Chart update modes for performance
     */
    enum class UpdateMode {
        Immediate,     // Update immediately
        Buffered,      // Buffer updates and apply periodically
        Decimated,     // Decimate data before display
        Adaptive       // Automatically choose based on performance
    };

    /**
     * @brief Chart configuration settings
     */
    struct ChartConfig {
        // Appearance
        Monitor::Charts::ChartTheme theme = Monitor::Charts::ChartTheme::Light;
        QString title;
        bool showLegend = true;
        Qt::Alignment legendAlignment = Qt::AlignBottom;
        bool showGrid = true;
        bool showAxes = true;
        
        // Performance
        Monitor::Charts::PerformanceLevel performanceLevel = Monitor::Charts::PerformanceLevel::Balanced;
        int maxDataPoints = 10000;
        bool enableAnimations = true;
        UpdateMode updateMode = UpdateMode::Adaptive;
        
        // Interaction
        InteractionMode interactionMode = InteractionMode::PanZoom;
        bool enableTooltips = true;
        bool enableCrosshair = false;
        
        // Export
        QSize exportSize = QSize(1920, 1080);
        Monitor::Charts::ExportFormat defaultExportFormat = Monitor::Charts::ExportFormat::PNG;
    };

    /**
     * @brief Series configuration for field mapping
     */
    struct SeriesConfig {
        QString fieldPath;          // Field path for data
        QString seriesName;         // Display name
        QColor color;              // Series color
        bool visible = true;       // Series visibility
        int axisIndex = 0;         // Axis assignment (for multi-axis charts)
        QJsonObject chartSpecific; // Chart-specific configuration
        
        SeriesConfig() : color(Monitor::Charts::ColorPalette::getColor(0)) {}
        SeriesConfig(const QString& path, const QString& name) 
            : fieldPath(path), seriesName(name), color(Monitor::Charts::ColorPalette::getColor(0)) {}
    };

    explicit ChartWidget(const QString& widgetId, const QString& windowTitle, QWidget* parent = nullptr);
    ~ChartWidget() override;

    // Chart configuration
    void setChartConfig(const ChartConfig& config);
    ChartConfig getChartConfig() const { return m_chartConfig; }
    void resetChartConfig();

    // Series management
    virtual bool addSeries(const QString& fieldPath, const SeriesConfig& config);
    virtual bool removeSeries(const QString& fieldPath);
    virtual void clearSeries();
    virtual QStringList getSeriesList() const;
    virtual SeriesConfig getSeriesConfig(const QString& fieldPath) const;
    virtual void setSeriesConfig(const QString& fieldPath, const SeriesConfig& config);

    // Chart access
    QChart* getChart() const { return m_chart; }
    QChartView* getChartView() const { return m_chartView; }

    // Axis management
    virtual void setAutoScale(bool enabled);
    virtual bool isAutoScale() const { return m_autoScale; }
    virtual void resetZoom();
    virtual void setAxisRange(Qt::Orientation orientation, double min, double max);
    virtual QPair<double, double> getAxisRange(Qt::Orientation orientation) const;

    // Export functionality
    bool exportChart(const QString& filePath = QString());
    bool exportChart(const QString& filePath, Monitor::Charts::ExportFormat format, const QSize& size = QSize());

    // Performance monitoring
    double getCurrentFPS() const;
    int getCurrentPointCount() const;
    bool isPerformanceOptimized() const { return m_performanceOptimized; }

public slots:
    // Interaction slots
    void onResetZoom();
    void onToggleLegend();
    void onToggleGrid();
    void onChangeTheme();
    void onExportChart();
    void onShowChartSettings();

signals:
    // Chart interaction signals
    void chartClicked(QPointF position);
    void chartDoubleClicked(QPointF position);
    void seriesHovered(QPointF position, bool state);
    void zoomChanged(QRectF zoomRect);
    void seriesVisibilityChanged(const QString& fieldPath, bool visible);

protected:
    // DisplayWidget implementation
    void initializeWidget() override;
    void updateDisplay() override;
    void handleFieldAdded(const FieldAssignment& field) override;
    void handleFieldRemoved(const QString& fieldPath) override;
    void handleFieldsCleared() override;

    // Settings implementation
    QJsonObject saveWidgetSpecificSettings() const override;
    bool restoreWidgetSpecificSettings(const QJsonObject& settings) override;

    // Context menu implementation
    void setupContextMenu() override;

    // Template methods for concrete chart widgets
    virtual void createChart() = 0;
    virtual void updateSeriesData() = 0;
    virtual void configureSeries(const QString& fieldPath, const SeriesConfig& config) = 0;
    virtual QAbstractSeries* createSeriesForField(const QString& fieldPath, const SeriesConfig& config) = 0;
    virtual void removeSeriesForField(const QString& fieldPath) = 0;

    // Chart setup helpers
    void setupChartView();
    void setupToolbar();
    void setupInteraction();
    void applyChartConfig();
    void applyTheme();
    void updatePerformanceSettings();

    // Data management helpers
    void addDataPoint(const QString& fieldPath, const QVariant& value);
    void updateSeriesVisibility();
    void optimizePerformance();

    // Interaction helpers
    void enableZoomPan(bool enable);
    void showTooltip(const QPointF& position, const QString& text);
    void hideTooltip();

    // Export helpers
    QString getDefaultExportPath() const;
    Monitor::Charts::ExportFormat getFormatFromExtension(const QString& filePath) const;

    // Access to configuration
    const std::unordered_map<QString, SeriesConfig>& getSeriesConfigs() const { return m_seriesConfigs; }

    // Performance monitoring
    void updateFPSCounter();
    void checkPerformanceThresholds();

protected:
    // Chart components
    QChart* m_chart;
    QChartView* m_chartView;

    // Layout
    QVBoxLayout* m_mainLayout;
    QToolBar* m_toolbar;

    // Configuration
    ChartConfig m_chartConfig;
    std::unordered_map<QString, SeriesConfig> m_seriesConfigs;
    std::unordered_map<QString, QAbstractSeries*> m_seriesMap;

    // State management
    bool m_autoScale;
    bool m_performanceOptimized;
    QTimer* m_updateTimer;
    QTimer* m_fpsTimer;

    // Performance tracking
    int m_frameCount;
    std::chrono::steady_clock::time_point m_lastFPSUpdate;
    double m_currentFPS;
    int m_currentPointCount;

    // Interaction state
    InteractionMode m_currentInteractionMode;
    QRubberBand* m_rubberBand;
    QPoint m_lastPanPoint;
    bool m_isRubberBandActive;

    // Toolbar actions
    QAction* m_resetZoomAction;
    QAction* m_toggleLegendAction;
    QAction* m_toggleGridAction;
    QAction* m_exportAction;
    QAction* m_settingsAction;
    QComboBox* m_themeCombo;

private slots:
    // Internal update management
    void onUpdateTimerTimeout();
    void onFPSTimerTimeout();
    void onChartInteraction();

private:
    // Helper methods
    void initializePerformanceTracking();
protected:
    void createDefaultAxes();
private:
    void updateAxisRange();
    bool shouldDecimateData() const;
    std::vector<QPointF> getDecimatedData(const QString& fieldPath) const;
    
    // Theme management
    void populateThemeCombo();
protected:
    Monitor::Charts::ChartThemeConfig getCurrentThemeConfig() const;
private:
    
    // Series color management
    QColor getNextSeriesColor() const;
protected:
    int m_nextColorIndex;
private:
};

#endif // CHART_WIDGET_H