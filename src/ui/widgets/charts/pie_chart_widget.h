#ifndef PIE_CHART_WIDGET_H
#define PIE_CHART_WIDGET_H

#include "chart_widget.h"
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>
#include <unordered_map>
#include <memory>


/**
 * @brief Pie chart widget for displaying proportional data
 * 
 * PieChartWidget provides comprehensive pie chart functionality:
 * - Standard pie charts and donut charts (with configurable hole size)
 * - Real-time slice updates with smooth animations
 * - Interactive slice selection and explosion effects
 * - Customizable slice colors, labels, and borders
 * - Value and percentage display options
 * - Multiple data aggregation methods for dynamic categories
 * - Slice threshold for grouping small values into "Others"
 * - Label positioning (inside, outside, or with leader lines)
 * - Rotation animations and auto-rotation modes
 * 
 * Visual Features:
 * - Gradient fills for slices
 * - Drop shadow effects
 * - Customizable slice borders
 * - Animated slice explosions
 * - Label formatting options
 * - Legend integration
 * 
 * Interactive Features:
 * - Click to explode/implode slices
 * - Hover highlighting
 * - Tooltips with detailed information
 * - Slice visibility toggle
 * - Auto-rotation mode
 */
class PieChartWidget : public ChartWidget
{
    Q_OBJECT

public:
    /**
     * @brief Pie chart specific configuration
     */
    struct PieChartConfig {
        // Chart appearance
        double holeSize = 0.0;              // 0.0 = pie, >0.0 = donut (0.0-0.9)
        double startAngle = 0.0;            // Starting angle in degrees
        double endAngle = 360.0;            // Ending angle in degrees
        
        // Slice appearance
        bool showSliceBorders = true;       // Show slice borders
        QColor sliceBorderColor = QColor(Qt::white);
        int sliceBorderWidth = 2;
        double sliceOpacity = 1.0;          // Slice opacity (0.0-1.0)
        
        // Labels
        enum class LabelPosition {
            Inside,                         // Labels inside slices
            Outside,                        // Labels outside slices
            LeaderLines                     // Labels with leader lines
        };
        
        enum class LabelContent {
            None,                           // No labels
            Value,                          // Show values only
            Percentage,                     // Show percentages only
            Label,                          // Show field names only
            ValueAndPercentage,             // Show both values and percentages
            LabelAndPercentage,             // Show labels and percentages
            All                             // Show label, value, and percentage
        };
        
        LabelPosition labelPosition = LabelPosition::Outside;
        LabelContent labelContent = LabelContent::LabelAndPercentage;
        QFont labelFont = QFont("Arial", 9);
        QColor labelColor = QColor(Qt::black);
        double labelDistance = 1.15;       // Distance multiplier for outside labels
        
        // Data processing
        enum class AggregationMethod {
            Last,                           // Use last value
            Sum,                            // Sum all values
            Average,                        // Average all values
            Count,                          // Count of values
            Max                             // Maximum value
        };
        AggregationMethod aggregation = AggregationMethod::Sum;
        
        // Small slice handling
        double minSliceThreshold = 0.02;   // Minimum slice size (2% default)
        QString otherSliceName = "Others";  // Name for combined small slices
        QColor otherSliceColor = QColor(128, 128, 128);
        bool combineSmallSlices = true;    // Combine slices below threshold
        
        // Animations
        bool enableAnimations = true;
        int animationDuration = 1000;      // Animation duration in milliseconds
        QEasingCurve::Type animationEasing = QEasingCurve::OutBounce;
        bool enableSliceExplosion = true;  // Allow slice explosion on click
        double explosionDistance = 0.1;    // Explosion distance (0.0-0.5)
        
        // Auto-rotation
        bool enableAutoRotation = false;   // Continuous rotation
        int rotationSpeed = 30;            // Degrees per second
        
        // Real-time updates
        bool enableRealTimeMode = true;
        int updateInterval = 200;          // Update interval in milliseconds
        
        PieChartConfig() = default;
    };
    
    /**
     * @brief Slice-specific configuration
     */
    struct SliceConfig {
        QColor color;                       // Slice color
        QColor borderColor = QColor(Qt::white);
        int borderWidth = 2;
        double opacity = 1.0;
        bool visible = true;
        bool exploded = false;              // Is slice exploded
        
        // Gradient options
        bool useGradient = false;
        QColor gradientStartColor;
        QColor gradientEndColor;
        
        // Shadow effects
        bool dropShadow = false;
        QColor shadowColor = QColor(0, 0, 0, 100);
        QPointF shadowOffset = QPointF(3, 3);
        
        SliceConfig() : color(Monitor::Charts::ColorPalette::getColor(0)) {
            gradientStartColor = color.lighter(150);
            gradientEndColor = color.darker(120);
        }
        
        SliceConfig(const QJsonObject& json);
        QJsonObject toJson() const;
    };

    explicit PieChartWidget(const QString& widgetId, QWidget* parent = nullptr);
    ~PieChartWidget() override;

    // Pie chart specific configuration
    void setPieChartConfig(const PieChartConfig& config);
    PieChartConfig getPieChartConfig() const { return m_pieConfig; }
    void resetPieChartConfig();

    // Slice management
    bool addSlice(const QString& fieldPath, const QString& label = QString(),
                 double value = 0.0, const SliceConfig& config = SliceConfig());
    void setSliceConfig(const QString& fieldPath, const SliceConfig& config);
    SliceConfig getSliceConfig(const QString& fieldPath) const;
    void removeSlice(const QString& fieldPath);

    // Data access
    double getSliceValue(const QString& fieldPath) const;
    double getSlicePercentage(const QString& fieldPath) const;
    double getTotalValue() const;
    int getSliceCount() const { return static_cast<int>(m_sliceData.size()); }
    QStringList getSliceNames() const;

    // Slice control
    void explodeSlice(const QString& fieldPath, bool exploded = true);
    void explodeAllSlices(bool exploded = true);
    bool isSliceExploded(const QString& fieldPath) const;
    void setSliceVisible(const QString& fieldPath, bool visible);
    bool isSliceVisible(const QString& fieldPath) const;

    // Chart appearance
    void setHoleSize(double holeSize);              // 0.0-0.9
    double getHoleSize() const { return m_pieConfig.holeSize; }
    void setStartAngle(double angle);
    double getStartAngle() const { return m_pieConfig.startAngle; }
    void setEndAngle(double angle);
    double getEndAngle() const { return m_pieConfig.endAngle; }

    // Auto-rotation control
    void setAutoRotation(bool enabled);
    bool isAutoRotationEnabled() const { return m_pieConfig.enableAutoRotation; }
    void setRotationSpeed(int degreesPerSecond);
    int getRotationSpeed() const { return m_pieConfig.rotationSpeed; }

    // Real-time control
    void setRealTimeMode(bool enabled);
    bool isRealTimeMode() const { return m_pieConfig.enableRealTimeMode; }

    // Data operations
    void clearAllData();
    void updateSliceValue(const QString& fieldPath, double value);
    void normalizeValues();                         // Normalize all values to percentages

public slots:
    void onHoleSizeChanged(int value);             // Slider value (0-90 -> 0.0-0.9)
    void onToggleAutoRotation(bool enabled);
    void onRotationSpeedChanged(int speed);
    void onToggleRealTimeMode(bool enabled);
    void onToggleSliceLabels(bool enabled);
    void onClearData();
    void onExplodeAllSlices();
    void onImplodeAllSlices();

signals:
    void sliceClicked(const QString& fieldPath, double value, double percentage);
    void sliceHovered(const QString& fieldPath, double value, double percentage, bool state);
    void sliceExploded(const QString& fieldPath, bool exploded);
    void sliceVisibilityChanged(const QString& fieldPath, bool visible);
    void totalValueChanged(double totalValue);
    void autoRotationChanged(bool enabled);

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
    void onSliceHovered(bool state);
    void onSliceClicked();
    void onRealTimeUpdate();
    void onAutoRotationUpdate();
    void onSliceAnimationFinished();

private:
    /**
     * @brief Internal data storage for each slice
     */
    struct SliceData {
        double value = 0.0;                         // Current value
        std::vector<double> valueHistory;          // For aggregation
        QPieSlice* pieSlice = nullptr;             // Qt pie slice
        SliceConfig config;                        // Slice configuration
        bool needsUpdate = false;                  // Update flag
        
        // Animation state
        QPropertyAnimation* explosionAnimation = nullptr;
        bool isAnimating = false;
        
        SliceData() = default;
        ~SliceData();
        
        void addValue(double newValue, PieChartConfig::AggregationMethod method);
        void clearData();
        double getAggregatedValue(PieChartConfig::AggregationMethod method) const;
    };

    // Configuration
    PieChartConfig m_pieConfig;
    std::unordered_map<QString, SliceConfig> m_sliceConfigs;
    
    // Data storage
    std::unordered_map<QString, std::unique_ptr<SliceData>> m_sliceData;
    double m_totalValue;
    
    // Chart components
    QPieSeries* m_pieSeries;
    
    // UI components
    QTimer* m_realTimeTimer;
    QTimer* m_rotationTimer;
    QSlider* m_holeSizeSlider;
    QSpinBox* m_rotationSpeedSpin;
    QCheckBox* m_autoRotationCheckBox;
    QCheckBox* m_realTimeModeCheckBox;
    QCheckBox* m_sliceLabelsCheckBox;
    QComboBox* m_labelContentCombo;
    QComboBox* m_labelPositionCombo;
    
    // Animation management
    QParallelAnimationGroup* m_animationGroup;
    double m_currentRotation;
    
    // Helper methods
    void setupToolbarExtensions();
    void updateRealTimeSettings();
    void updateAutoRotationSettings();
    void applySliceConfig(QPieSlice* slice, const SliceConfig& config);
    void updateSliceLabels();
    void updateSliceColors();
    
    // Data processing
    void addDataPoint(const QString& fieldPath, double value);
    void recalculatePercentages();
    void combineSmallSlices();
    void updateTotalValue();
    
    // Animation helpers
    void animateSliceExplosion(const QString& fieldPath, bool explode);
    void animateSliceUpdate(const QString& fieldPath);
    void startRotationAnimation();
    void stopRotationAnimation();
    
    // Label formatting
    QString formatSliceLabel(const QString& fieldPath, double value, double percentage) const;
    QString formatValue(double value) const;
    QString formatPercentage(double percentage) const;
    
    // Visual effects
    void applySliceGradient(QPieSlice* slice, const SliceConfig& config);
    void applySliceShadow(QPieSlice* slice, const SliceConfig& config);
    void updateSliceOpacity();
    
    // Threshold handling
    bool shouldCombineSlice(double percentage) const;
    void createOtherSlice();
    void updateOtherSlice();
    
    // Color management
    QColor getNextSliceColor() const;
    void redistributeColors();
};

#endif // PIE_CHART_WIDGET_H