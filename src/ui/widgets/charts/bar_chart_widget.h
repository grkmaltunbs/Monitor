#ifndef BAR_CHART_WIDGET_H
#define BAR_CHART_WIDGET_H

#include "chart_widget.h"
#include <QtCharts/QBarSeries>
#include <QtCharts/QStackedBarSeries>
#include <QtCharts/QPercentBarSeries>
#include <QtCharts/QHorizontalBarSeries>
#include <QtCharts/QHorizontalStackedBarSeries>
#include <QtCharts/QHorizontalPercentBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QTimer>
#include <QEasingCurve>
#include <QActionGroup>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>
#include <unordered_map>
#include <memory>


/**
 * @brief Bar chart widget for displaying categorical and numeric data
 * 
 * BarChartWidget provides comprehensive bar chart functionality:
 * - Multiple bar series support with grouping and stacking
 * - Horizontal and vertical orientations
 * - Real-time data updates with smooth animations
 * - Category-based and value-based X-axis modes
 * - Custom bar colors, spacing, and styling
 * - Value labels on bars with formatting options
 * - Interactive tooltips and bar selection
 * - Data aggregation functions (sum, average, count)
 * 
 * Bar Chart Types:
 * - Grouped: Multiple series shown side by side
 * - Stacked: Series stacked on top of each other
 * - Percent: Stacked bars normalized to 100%
 * - Horizontal: All above types rotated 90 degrees
 * 
 * Performance Features:
 * - Efficient bar updates without full redraw
 * - Configurable animation duration and easing
 * - Automatic color management for large datasets
 * - Memory-efficient category management
 */
class BarChartWidget : public ChartWidget
{
    Q_OBJECT

public:
    /**
     * @brief Bar chart specific configuration
     */
    struct BarChartConfig {
        // Chart type and orientation
        enum class ChartType {
            Grouped,           // Side-by-side bars
            Stacked,          // Stacked bars
            Percent           // 100% stacked bars
        };
        
        enum class Orientation {
            Vertical,         // Standard vertical bars
            Horizontal        // Horizontal bars
        };
        
        enum class CategoryMode {
            FieldBased,       // Categories from field values
            PacketBased,      // Categories from packet IDs/types
            TimeBased,        // Categories from time intervals
            Custom            // User-defined categories
        };
        
        // Chart appearance
        ChartType chartType = ChartType::Grouped;
        Orientation orientation = Orientation::Vertical;
        CategoryMode categoryMode = CategoryMode::FieldBased;
        
        // Bar appearance
        double barWidth = 0.8;           // Bar width as fraction of category width
        double barSpacing = 0.2;         // Spacing between bar groups
        bool showValueLabels = true;     // Show values on bars
        bool showBarBorders = true;      // Show bar outlines
        QColor barBorderColor = QColor(Qt::black);
        int barBorderWidth = 1;
        
        // Categories
        QStringList customCategories;    // For Custom category mode
        QString categoryFieldPath;       // Field path for category values
        int maxCategories = 50;          // Maximum number of categories to display
        bool autoSortCategories = true;  // Auto-sort categories alphabetically
        
        // Aggregation (when multiple values per category)
        enum class AggregationMethod {
            Last,             // Use last value
            Sum,              // Sum all values
            Average,          // Average all values
            Count,            // Count of values
            Min,              // Minimum value
            Max               // Maximum value
        };
        AggregationMethod aggregation = AggregationMethod::Last;
        
        // Animations
        bool enableAnimations = true;
        int animationDuration = 500;     // Animation duration in milliseconds
        QEasingCurve::Type animationEasing = QEasingCurve::OutCubic;
        
        // Real-time updates
        bool enableRealTimeMode = true;
        int updateInterval = 100;        // Update interval in milliseconds
        
        BarChartConfig() = default;
    };
    
    /**
     * @brief Series-specific configuration for bar charts
     */
    struct BarSeriesConfig {
        QColor barColor;                 // Bar fill color
        QColor borderColor = QColor(Qt::black);  // Bar border color
        int borderWidth = 1;             // Border width
        double opacity = 1.0;            // Bar opacity (0.0 - 1.0)
        QString labelFormat = "%.2f";    // Value label format
        bool showLabels = true;          // Show value labels on bars
        QColor labelColor = QColor(Qt::black);   // Label text color
        
        // Gradient fill options
        bool useGradient = false;
        QColor gradientStartColor;
        QColor gradientEndColor;
        Qt::Orientation gradientDirection = Qt::Vertical;
        
        BarSeriesConfig() : barColor(Monitor::Charts::ColorPalette::getColor(0)) {
            gradientStartColor = barColor.lighter(150);
            gradientEndColor = barColor.darker(150);
        }
        
        BarSeriesConfig(const QJsonObject& json);
        QJsonObject toJson() const;
    };

    explicit BarChartWidget(const QString& widgetId, QWidget* parent = nullptr);
    ~BarChartWidget() override;

    // Bar chart specific configuration
    void setBarChartConfig(const BarChartConfig& config);
    BarChartConfig getBarChartConfig() const { return m_barConfig; }
    void resetBarChartConfig();

    // Series management
    bool addBarSeries(const QString& fieldPath, const QString& seriesName = QString(),
                     const QColor& color = QColor(), const BarSeriesConfig& config = BarSeriesConfig());
    void setBarSeriesConfig(const QString& fieldPath, const BarSeriesConfig& config);
    BarSeriesConfig getBarSeriesConfig(const QString& fieldPath) const;

    // Category management
    void addCategory(const QString& category);
    void removeCategory(const QString& category);
    void clearCategories();
    QStringList getCategories() const;
    void setCategoryFieldPath(const QString& fieldPath);
    QString getCategoryFieldPath() const { return m_barConfig.categoryFieldPath; }

    // Data access
    double getBarValue(const QString& fieldPath, const QString& category) const;
    QStringList getSeriesCategories(const QString& fieldPath) const;
    int getCategoryCount() const;
    int getSeriesCount() const { return static_cast<int>(m_barSeriesConfigs.size()); }

    // Chart type and orientation
    void setChartType(BarChartConfig::ChartType type);
    BarChartConfig::ChartType getChartType() const { return m_barConfig.chartType; }
    void setOrientation(BarChartConfig::Orientation orientation);
    BarChartConfig::Orientation getOrientation() const { return m_barConfig.orientation; }

    // Real-time control
    void setRealTimeMode(bool enabled);
    bool isRealTimeMode() const { return m_barConfig.enableRealTimeMode; }

    // Data operations
    void clearSeriesData(const QString& fieldPath);
    void clearAllData();
    void sortCategories(bool ascending = true);

public slots:
    void onChartTypeChanged(int type);
    void onOrientationChanged(int orientation);
    void onToggleRealTimeMode(bool enabled);
    void onToggleValueLabels(bool enabled);
    void onClearData();
    void onSortCategories();

signals:
    void barClicked(const QString& fieldPath, const QString& category, double value);
    void barHovered(const QString& fieldPath, const QString& category, double value, bool state);
    void categoryAdded(const QString& category);
    void categoryRemoved(const QString& category);
    void chartTypeChanged(BarChartConfig::ChartType type);
    void orientationChanged(BarChartConfig::Orientation orientation);

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

    // Context menu implementation
    void setupContextMenu() override;

private slots:
    void onBarSetHovered(bool state, int index);
    void onBarSetClicked(int index);
    void onRealTimeUpdate();

private:
    /**
     * @brief Internal data storage for each bar series
     */
    struct BarSeriesData {
        std::unordered_map<QString, double> categoryValues;  // Category -> Value mapping
        std::unordered_map<QString, std::vector<double>> categoryHistory; // For aggregation
        QBarSet* barSet = nullptr;                          // Qt bar set
        BarSeriesConfig config;                             // Series configuration
        bool needsUpdate = false;                           // Update flag
        
        BarSeriesData() = default;
        ~BarSeriesData() = default;
        
        void addValue(const QString& category, double value, BarChartConfig::AggregationMethod method);
        void clearData();
        double getAggregatedValue(const QString& category, BarChartConfig::AggregationMethod method) const;
    };

    // Configuration
    BarChartConfig m_barConfig;
    std::unordered_map<QString, BarSeriesConfig> m_barSeriesConfigs;
    
    // Data storage
    std::unordered_map<QString, std::unique_ptr<BarSeriesData>> m_seriesData;
    QStringList m_categories;
    std::unordered_map<QString, int> m_categoryIndices;
    
    // Chart components
    QAbstractBarSeries* m_barSeries;    // Current bar series (type depends on config)
    QBarCategoryAxis* m_categoryAxis;
    QValueAxis* m_valueAxis;
    
    // UI components
    QTimer* m_realTimeTimer;
    QComboBox* m_chartTypeCombo;
    QComboBox* m_orientationCombo;
    QCheckBox* m_realTimeModeCheckBox;
    QCheckBox* m_valueLabelsCheckBox;
    QSpinBox* m_maxCategoriesSpin;
    
    // Helper methods
    void setupToolbarExtensions();
    void updateRealTimeSettings();
    void recreateBarSeries();
    void updateCategoryAxis();
    void updateValueAxis();
    void applyBarSeriesConfig(QBarSet* barSet, const BarSeriesConfig& config);
    
    // Data processing
    QString extractCategory(const QVariant& fieldValue, const QString& fieldPath);
    void addDataPoint(const QString& fieldPath, const QString& category, double value);
    void updateBarSetData();
    
    // Category management
    void ensureCategory(const QString& category);
    void rebuildCategoryIndices();
    void limitCategories();
    
    // Chart type management
    QAbstractBarSeries* createBarSeriesOfType(BarChartConfig::ChartType type, 
                                             BarChartConfig::Orientation orientation);
    void transferDataToBars();
    
    // Animation and visual effects
    void setupBarAnimations();
    void animateBarUpdate(QBarSet* barSet, int index, double newValue);
    
    // Aggregation helpers
    double aggregateValues(const std::vector<double>& values, 
                         BarChartConfig::AggregationMethod method) const;
};

#endif // BAR_CHART_WIDGET_H