#include "pie_chart_widget.h"
#include <QApplication>
#include <QDebug>
#include <QRandomGenerator>
#include <algorithm>
#include <numeric>
#include <cmath>


// SliceConfig implementation
PieChartWidget::SliceConfig::SliceConfig(const QJsonObject& json) {
    color = QColor(json.value("color").toString());
    borderColor = QColor(json.value("borderColor").toString("#ffffff"));
    borderWidth = json.value("borderWidth").toInt(2);
    opacity = json.value("opacity").toDouble(1.0);
    visible = json.value("visible").toBool(true);
    exploded = json.value("exploded").toBool(false);
    useGradient = json.value("useGradient").toBool(false);
    gradientStartColor = QColor(json.value("gradientStartColor").toString());
    gradientEndColor = QColor(json.value("gradientEndColor").toString());
    dropShadow = json.value("dropShadow").toBool(false);
    shadowColor = QColor(json.value("shadowColor").toString("#64000000"));
    
    QJsonObject offsetObj = json.value("shadowOffset").toObject();
    shadowOffset = QPointF(offsetObj.value("x").toDouble(3.0), 
                          offsetObj.value("y").toDouble(3.0));
}

QJsonObject PieChartWidget::SliceConfig::toJson() const {
    QJsonObject json;
    json["color"] = color.name();
    json["borderColor"] = borderColor.name();
    json["borderWidth"] = borderWidth;
    json["opacity"] = opacity;
    json["visible"] = visible;
    json["exploded"] = exploded;
    json["useGradient"] = useGradient;
    json["gradientStartColor"] = gradientStartColor.name();
    json["gradientEndColor"] = gradientEndColor.name();
    json["dropShadow"] = dropShadow;
    json["shadowColor"] = shadowColor.name();
    
    QJsonObject offsetObj;
    offsetObj["x"] = shadowOffset.x();
    offsetObj["y"] = shadowOffset.y();
    json["shadowOffset"] = offsetObj;
    
    return json;
}

// SliceData implementation
PieChartWidget::SliceData::~SliceData() {
    if (explosionAnimation) {
        explosionAnimation->stop();
        delete explosionAnimation;
    }
}

void PieChartWidget::SliceData::addValue(double newValue, PieChartConfig::AggregationMethod method) {
    switch (method) {
        case PieChartConfig::AggregationMethod::Last:
            value = newValue;
            break;
            
        case PieChartConfig::AggregationMethod::Sum:
        case PieChartConfig::AggregationMethod::Average:
        case PieChartConfig::AggregationMethod::Count:
        case PieChartConfig::AggregationMethod::Max:
            valueHistory.push_back(newValue);
            value = getAggregatedValue(method);
            break;
    }
    needsUpdate = true;
}

void PieChartWidget::SliceData::clearData() {
    value = 0.0;
    valueHistory.clear();
    needsUpdate = true;
}

double PieChartWidget::SliceData::getAggregatedValue(PieChartConfig::AggregationMethod method) const {
    if (valueHistory.empty()) {
        return 0.0;
    }
    
    switch (method) {
        case PieChartConfig::AggregationMethod::Last:
            return valueHistory.back();
            
        case PieChartConfig::AggregationMethod::Sum:
            return std::accumulate(valueHistory.begin(), valueHistory.end(), 0.0);
            
        case PieChartConfig::AggregationMethod::Average:
            return std::accumulate(valueHistory.begin(), valueHistory.end(), 0.0) / valueHistory.size();
            
        case PieChartConfig::AggregationMethod::Count:
            return static_cast<double>(valueHistory.size());
            
        case PieChartConfig::AggregationMethod::Max:
            return *std::max_element(valueHistory.begin(), valueHistory.end());
    }
    
    return 0.0;
}

// PieChartWidget implementation
PieChartWidget::PieChartWidget(const QString& widgetId, QWidget* parent)
    : ChartWidget(widgetId, "Pie Chart", parent)
    , m_totalValue(0.0)
    , m_pieSeries(nullptr)
    , m_realTimeTimer(new QTimer(this))
    , m_rotationTimer(new QTimer(this))
    , m_holeSizeSlider(nullptr)
    , m_rotationSpeedSpin(nullptr)
    , m_autoRotationCheckBox(nullptr)
    , m_realTimeModeCheckBox(nullptr)
    , m_sliceLabelsCheckBox(nullptr)
    , m_labelContentCombo(nullptr)
    , m_labelPositionCombo(nullptr)
    , m_animationGroup(new QParallelAnimationGroup(this))
    , m_currentRotation(0.0)
{
    // Initialize pie chart configuration
    m_pieConfig = PieChartConfig();
    
    // Setup real-time update timer
    m_realTimeTimer->setSingleShot(false);
    m_realTimeTimer->setInterval(m_pieConfig.updateInterval);
    connect(m_realTimeTimer, &QTimer::timeout, this, &PieChartWidget::onRealTimeUpdate);
    
    // Setup rotation timer
    m_rotationTimer->setSingleShot(false);
    m_rotationTimer->setInterval(16); // ~60 FPS for smooth rotation
    connect(m_rotationTimer, &QTimer::timeout, this, &PieChartWidget::onAutoRotationUpdate);
    
    // Start timers if enabled
    if (m_pieConfig.enableRealTimeMode) {
        m_realTimeTimer->start();
    }
    if (m_pieConfig.enableAutoRotation) {
        m_rotationTimer->start();
    }
}

PieChartWidget::~PieChartWidget() {
    // Clean up slice data
    m_sliceData.clear();
    m_sliceConfigs.clear();
}

void PieChartWidget::createChart() {
    // Create the chart
    m_chart = new QChart();
    m_chart->setTitle("Pie Chart");
    m_chart->setAnimationOptions(m_pieConfig.enableAnimations ? 
        QChart::SeriesAnimations : QChart::NoAnimation);
    
    // Create pie series
    m_pieSeries = new QPieSeries();
    m_pieSeries->setHoleSize(m_pieConfig.holeSize);
    m_pieSeries->setPieStartAngle(m_pieConfig.startAngle);
    m_pieSeries->setPieEndAngle(m_pieConfig.endAngle);
    m_pieSeries->setLabelsVisible(m_pieConfig.labelContent != PieChartConfig::LabelContent::None);
    
    // Add series to chart
    m_chart->addSeries(m_pieSeries);
    
    // Setup toolbar extensions
    setupToolbarExtensions();
    
    // Apply initial configuration
    applyChartConfig();
}

void PieChartWidget::setupToolbarExtensions() {
    if (!m_toolbar) return;
    
    // Add pie chart specific toolbar items
    m_toolbar->addSeparator();
    
    // Hole size slider
    m_toolbar->addWidget(new QLabel("Hole:"));
    m_holeSizeSlider = new QSlider(Qt::Horizontal);
    m_holeSizeSlider->setRange(0, 90);  // 0-90% -> 0.0-0.9
    m_holeSizeSlider->setValue(static_cast<int>(m_pieConfig.holeSize * 100));
    m_holeSizeSlider->setMaximumWidth(80);
    connect(m_holeSizeSlider, &QSlider::valueChanged, this, &PieChartWidget::onHoleSizeChanged);
    m_toolbar->addWidget(m_holeSizeSlider);
    
    // Auto-rotation controls
    m_autoRotationCheckBox = new QCheckBox("Auto-rotate");
    m_autoRotationCheckBox->setChecked(m_pieConfig.enableAutoRotation);
    connect(m_autoRotationCheckBox, &QCheckBox::toggled, 
            this, &PieChartWidget::onToggleAutoRotation);
    m_toolbar->addWidget(m_autoRotationCheckBox);
    
    m_toolbar->addWidget(new QLabel("Speed:"));
    m_rotationSpeedSpin = new QSpinBox();
    m_rotationSpeedSpin->setRange(1, 180);  // 1-180 degrees per second
    m_rotationSpeedSpin->setValue(m_pieConfig.rotationSpeed);
    m_rotationSpeedSpin->setSuffix("°/s");
    m_rotationSpeedSpin->setMaximumWidth(70);
    connect(m_rotationSpeedSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PieChartWidget::onRotationSpeedChanged);
    m_toolbar->addWidget(m_rotationSpeedSpin);
    
    // Label controls
    m_sliceLabelsCheckBox = new QCheckBox("Labels");
    m_sliceLabelsCheckBox->setChecked(m_pieConfig.labelContent != PieChartConfig::LabelContent::None);
    connect(m_sliceLabelsCheckBox, &QCheckBox::toggled,
            this, &PieChartWidget::onToggleSliceLabels);
    m_toolbar->addWidget(m_sliceLabelsCheckBox);
    
    m_toolbar->addWidget(new QLabel("Content:"));
    m_labelContentCombo = new QComboBox();
    m_labelContentCombo->addItems({"None", "Value", "%", "Label", "Value+%", "Label+%", "All"});
    m_labelContentCombo->setCurrentIndex(static_cast<int>(m_pieConfig.labelContent));
    m_labelContentCombo->setMaximumWidth(80);
    connect(m_labelContentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                m_pieConfig.labelContent = static_cast<PieChartConfig::LabelContent>(index);
                updateSliceLabels();
            });
    m_toolbar->addWidget(m_labelContentCombo);
    
    // Real-time mode toggle
    m_realTimeModeCheckBox = new QCheckBox("Real-time");
    m_realTimeModeCheckBox->setChecked(m_pieConfig.enableRealTimeMode);
    connect(m_realTimeModeCheckBox, &QCheckBox::toggled, 
            this, &PieChartWidget::onToggleRealTimeMode);
    m_toolbar->addWidget(m_realTimeModeCheckBox);
    
    m_toolbar->addSeparator();
    
    // Add utility actions
    QAction* clearDataAction = m_toolbar->addAction("Clear");
    connect(clearDataAction, &QAction::triggered, this, &PieChartWidget::onClearData);
    
    QAction* explodeAllAction = m_toolbar->addAction("Explode");
    connect(explodeAllAction, &QAction::triggered, this, &PieChartWidget::onExplodeAllSlices);
    
    QAction* implodeAllAction = m_toolbar->addAction("Implode");
    connect(implodeAllAction, &QAction::triggered, this, &PieChartWidget::onImplodeAllSlices);
}

void PieChartWidget::setPieChartConfig(const PieChartConfig& config) {
    bool needsSeriesUpdate = (config.holeSize != m_pieConfig.holeSize ||
                             config.startAngle != m_pieConfig.startAngle ||
                             config.endAngle != m_pieConfig.endAngle ||
                             config.labelContent != m_pieConfig.labelContent ||
                             config.labelPosition != m_pieConfig.labelPosition);
    
    m_pieConfig = config;
    
    updateRealTimeSettings();
    updateAutoRotationSettings();
    
    if (m_pieSeries && needsSeriesUpdate) {
        m_pieSeries->setHoleSize(m_pieConfig.holeSize);
        m_pieSeries->setPieStartAngle(m_pieConfig.startAngle);
        m_pieSeries->setPieEndAngle(m_pieConfig.endAngle);
        m_pieSeries->setLabelsVisible(m_pieConfig.labelContent != PieChartConfig::LabelContent::None);
        updateSliceLabels();
    }
    
    // Update toolbar controls
    if (m_holeSizeSlider) {
        m_holeSizeSlider->setValue(static_cast<int>(m_pieConfig.holeSize * 100));
    }
    if (m_autoRotationCheckBox) {
        m_autoRotationCheckBox->setChecked(m_pieConfig.enableAutoRotation);
    }
    if (m_rotationSpeedSpin) {
        m_rotationSpeedSpin->setValue(m_pieConfig.rotationSpeed);
    }
    if (m_realTimeModeCheckBox) {
        m_realTimeModeCheckBox->setChecked(m_pieConfig.enableRealTimeMode);
    }
    if (m_sliceLabelsCheckBox) {
        m_sliceLabelsCheckBox->setChecked(m_pieConfig.labelContent != PieChartConfig::LabelContent::None);
    }
    if (m_labelContentCombo) {
        m_labelContentCombo->setCurrentIndex(static_cast<int>(m_pieConfig.labelContent));
    }
    
    // Update chart animations
    if (m_chart) {
        m_chart->setAnimationOptions(m_pieConfig.enableAnimations ? 
            QChart::SeriesAnimations : QChart::NoAnimation);
    }
}

void PieChartWidget::resetPieChartConfig() {
    m_pieConfig = PieChartConfig();
    setPieChartConfig(m_pieConfig);
}

bool PieChartWidget::addSlice(const QString& fieldPath, const QString& label,
                             double value, const SliceConfig& config) {
    SeriesConfig baseConfig;
    baseConfig.fieldPath = fieldPath;
    baseConfig.seriesName = label.isEmpty() ? fieldPath : label;
    baseConfig.color = config.color;
    
    // Store slice-specific configuration
    m_sliceConfigs[fieldPath] = config;
    
    // Create slice data
    auto sliceData = std::make_unique<SliceData>();
    sliceData->value = value;
    sliceData->config = config;
    
    // Store slice data
    m_sliceData[fieldPath] = std::move(sliceData);
    
    return addSeries(fieldPath, baseConfig);
}

void PieChartWidget::setSliceConfig(const QString& fieldPath, const SliceConfig& config) {
    auto it = m_sliceConfigs.find(fieldPath);
    if (it == m_sliceConfigs.end()) {
        return;
    }
    
    it->second = config;
    
    // Apply configuration to existing slice
    auto dataIt = m_sliceData.find(fieldPath);
    if (dataIt != m_sliceData.end() && dataIt->second->pieSlice) {
        applySliceConfig(dataIt->second->pieSlice, config);
    }
}

PieChartWidget::SliceConfig PieChartWidget::getSliceConfig(const QString& fieldPath) const {
    auto it = m_sliceConfigs.find(fieldPath);
    return (it != m_sliceConfigs.end()) ? it->second : SliceConfig();
}

void PieChartWidget::removeSlice(const QString& fieldPath) {
    removeSeries(fieldPath);
}

QAbstractSeries* PieChartWidget::createSeriesForField(const QString& fieldPath, const SeriesConfig& config) {
    // Get slice-specific configuration
    auto sliceConfigIt = m_sliceConfigs.find(fieldPath);
    SliceConfig sliceConfig = (sliceConfigIt != m_sliceConfigs.end()) ? 
        sliceConfigIt->second : SliceConfig();
    
    // Get or create slice data
    auto dataIt = m_sliceData.find(fieldPath);
    if (dataIt == m_sliceData.end()) {
        auto sliceData = std::make_unique<SliceData>();
        sliceData->config = sliceConfig;
        dataIt = m_sliceData.emplace(fieldPath, std::move(sliceData)).first;
    }
    
    // Create pie slice
    QPieSlice* slice = new QPieSlice(config.seriesName, dataIt->second->value);
    applySliceConfig(slice, sliceConfig);
    
    // Connect slice signals
    connect(slice, &QPieSlice::hovered, this, &PieChartWidget::onSliceHovered);
    connect(slice, &QPieSlice::clicked, this, &PieChartWidget::onSliceClicked);
    
    // Add slice to series
    if (m_pieSeries) {
        m_pieSeries->append(slice);
        dataIt->second->pieSlice = slice;
        
        // Update total value
        updateTotalValue();
        
        // Recalculate percentages for all slices
        recalculatePercentages();
    }
    
    return m_pieSeries;
}

void PieChartWidget::removeSeriesForField(const QString& fieldPath) {
    auto it = m_sliceData.find(fieldPath);
    if (it != m_sliceData.end()) {
        // Remove slice from series
        if (m_pieSeries && it->second->pieSlice) {
            m_pieSeries->remove(it->second->pieSlice);
            // Note: QPieSeries takes ownership of slices, so we don't delete manually
        }
        
        // Remove from storage
        m_sliceData.erase(it);
    }
    
    // Remove slice configuration
    m_sliceConfigs.erase(fieldPath);
    
    // Update total value and percentages
    updateTotalValue();
    recalculatePercentages();
}

void PieChartWidget::configureSeries(const QString& fieldPath, const SeriesConfig& config) {
    auto it = m_sliceData.find(fieldPath);
    if (it == m_sliceData.end()) {
        return;
    }
    
    SliceData* data = it->second.get();
    
    // Update slice
    if (data->pieSlice) {
        data->pieSlice->setLabel(config.seriesName);
        data->pieSlice->setColor(config.color);
        
        // Update slice-specific configuration
        SliceConfig sliceConfig = data->config;
        sliceConfig.color = config.color;
        applySliceConfig(data->pieSlice, sliceConfig);
    }
    
    // Update visibility (handled by series visibility in base class)
    if (data->pieSlice) {
        //data->pieSlice->setVisible(config.visible); // Not available in Qt6
    }
}

void PieChartWidget::updateSeriesData() {
    // Update all slices that need updates
    bool hasUpdates = false;
    for (auto& pair : m_sliceData) {
        if (pair.second->needsUpdate) {
            hasUpdates = true;
            break;
        }
    }
    
    if (hasUpdates) {
        // Update slice values
        for (auto& pair : m_sliceData) {
            const QString& fieldPath = pair.first;
            SliceData* data = pair.second.get();
            
            if (data->needsUpdate && data->pieSlice) {
                // Animate value change if animations are enabled
                if (m_pieConfig.enableAnimations) {
                    animateSliceUpdate(fieldPath);
                } else {
                    data->pieSlice->setValue(data->value);
                }
                data->needsUpdate = false;
            }
        }
        
        // Update total value and percentages
        updateTotalValue();
        recalculatePercentages();
        
        // Combine small slices if enabled
        if (m_pieConfig.combineSmallSlices) {
            combineSmallSlices();
        }
        
        // Update slice labels
        updateSliceLabels();
        
        emit totalValueChanged(m_totalValue);
    }
    
    // Update current point count for performance monitoring
    m_currentPointCount = static_cast<int>(m_sliceData.size());
}

void PieChartWidget::updateFieldDisplay(const QString& fieldPath, const QVariant& value) {
    auto it = m_sliceData.find(fieldPath);
    if (it == m_sliceData.end()) {
        return;
    }
    
    // Convert field value to numeric
    bool ok;
    double numericValue = Monitor::Charts::DataConverter::toDouble(value, &ok);
    if (!ok) {
        qWarning() << "PieChartWidget: Cannot convert value to double for field" << fieldPath;
        return;
    }
    
    // Add data point
    addDataPoint(fieldPath, numericValue);
    
    // For immediate update mode, update display immediately
    if (getChartConfig().updateMode == UpdateMode::Immediate) {
        updateSeriesData();
    }
}

void PieChartWidget::clearFieldDisplay(const QString& fieldPath) {
    auto it = m_sliceData.find(fieldPath);
    if (it != m_sliceData.end()) {
        it->second->clearData();
        it->second->needsUpdate = true;
        
        // Clear the slice value immediately
        if (it->second->pieSlice) {
            it->second->pieSlice->setValue(0.0);
        }
        
        updateTotalValue();
        recalculatePercentages();
    }
}

void PieChartWidget::refreshAllDisplays() {
    // Mark all slices for update
    for (auto& pair : m_sliceData) {
        pair.second->needsUpdate = true;
    }
    
    updateSeriesData();
}

// Data access methods
double PieChartWidget::getSliceValue(const QString& fieldPath) const {
    auto it = m_sliceData.find(fieldPath);
    if (it != m_sliceData.end()) {
        return it->second->value;
    }
    return 0.0;
}

double PieChartWidget::getSlicePercentage(const QString& fieldPath) const {
    auto it = m_sliceData.find(fieldPath);
    if (it != m_sliceData.end() && it->second->pieSlice && m_totalValue > 0.0) {
        return (it->second->value / m_totalValue) * 100.0;
    }
    return 0.0;
}

double PieChartWidget::getTotalValue() const {
    return m_totalValue;
}

QStringList PieChartWidget::getSliceNames() const {
    QStringList names;
    for (const auto& pair : m_sliceData) {
        names.append(pair.first);
    }
    return names;
}

// Slice control methods
void PieChartWidget::explodeSlice(const QString& fieldPath, bool exploded) {
    auto it = m_sliceData.find(fieldPath);
    if (it != m_sliceData.end() && it->second->pieSlice) {
        if (m_pieConfig.enableAnimations) {
            animateSliceExplosion(fieldPath, exploded);
        } else {
            it->second->pieSlice->setExploded(exploded);
        }
        
        it->second->config.exploded = exploded;
        emit sliceExploded(fieldPath, exploded);
    }
}

void PieChartWidget::explodeAllSlices(bool exploded) {
    for (const auto& pair : m_sliceData) {
        explodeSlice(pair.first, exploded);
    }
}

bool PieChartWidget::isSliceExploded(const QString& fieldPath) const {
    auto it = m_sliceData.find(fieldPath);
    if (it != m_sliceData.end() && it->second->pieSlice) {
        return it->second->pieSlice->isExploded();
    }
    return false;
}

void PieChartWidget::setSliceVisible(const QString& fieldPath, bool visible) {
    auto it = m_sliceData.find(fieldPath);
    if (it != m_sliceData.end() && it->second->pieSlice) {
        //it->second->pieSlice->setVisible(visible); // Not available in Qt6
        it->second->config.visible = visible;
        
        updateTotalValue();
        recalculatePercentages();
        
        emit sliceVisibilityChanged(fieldPath, visible);
    }
}

bool PieChartWidget::isSliceVisible(const QString& fieldPath) const {
    auto it = m_sliceData.find(fieldPath);
    if (it != m_sliceData.end() && it->second->pieSlice) {
        //return it->second->pieSlice->isVisible(); // Not available in Qt6
        return true;
    }
    return false;
}

// Chart appearance methods
void PieChartWidget::setHoleSize(double holeSize) {
    holeSize = qBound(0.0, holeSize, 0.9);
    m_pieConfig.holeSize = holeSize;
    
    if (m_pieSeries) {
        m_pieSeries->setHoleSize(holeSize);
    }
    
    if (m_holeSizeSlider) {
        m_holeSizeSlider->setValue(static_cast<int>(holeSize * 100));
    }
}

void PieChartWidget::setStartAngle(double angle) {
    m_pieConfig.startAngle = angle;
    
    if (m_pieSeries) {
        m_pieSeries->setPieStartAngle(angle);
    }
}

void PieChartWidget::setEndAngle(double angle) {
    m_pieConfig.endAngle = angle;
    
    if (m_pieSeries) {
        m_pieSeries->setPieEndAngle(angle);
    }
}

// Auto-rotation control
void PieChartWidget::setAutoRotation(bool enabled) {
    m_pieConfig.enableAutoRotation = enabled;
    updateAutoRotationSettings();
    
    if (m_autoRotationCheckBox) {
        m_autoRotationCheckBox->setChecked(enabled);
    }
    
    emit autoRotationChanged(enabled);
}

void PieChartWidget::setRotationSpeed(int degreesPerSecond) {
    m_pieConfig.rotationSpeed = qBound(1, degreesPerSecond, 180);
    
    if (m_rotationSpeedSpin) {
        m_rotationSpeedSpin->setValue(m_pieConfig.rotationSpeed);
    }
}

// Real-time control
void PieChartWidget::setRealTimeMode(bool enabled) {
    m_pieConfig.enableRealTimeMode = enabled;
    updateRealTimeSettings();
    
    if (m_realTimeModeCheckBox) {
        m_realTimeModeCheckBox->setChecked(enabled);
    }
}

// Data operations
void PieChartWidget::clearAllData() {
    for (auto& pair : m_sliceData) {
        pair.second->clearData();
    }
    
    if (m_pieSeries) {
        m_pieSeries->clear();
    }
    
    m_sliceData.clear();
    m_sliceConfigs.clear();
    m_totalValue = 0.0;
    
    emit totalValueChanged(m_totalValue);
}

void PieChartWidget::updateSliceValue(const QString& fieldPath, double value) {
    auto it = m_sliceData.find(fieldPath);
    if (it != m_sliceData.end()) {
        it->second->value = value;
        it->second->needsUpdate = true;
        
        updateSeriesData();
    }
}

void PieChartWidget::normalizeValues() {
    if (m_totalValue <= 0.0) return;
    
    for (auto& pair : m_sliceData) {
        SliceData* data = pair.second.get();
        double normalizedValue = (data->value / m_totalValue) * 100.0;
        data->value = normalizedValue;
        data->needsUpdate = true;
        
        if (data->pieSlice) {
            data->pieSlice->setValue(normalizedValue);
        }
    }
    
    updateTotalValue();
}

// Settings implementation
QJsonObject PieChartWidget::saveWidgetSpecificSettings() const {
    QJsonObject settings = ChartWidget::saveWidgetSpecificSettings();
    
    // Pie chart configuration
    QJsonObject pieConfig;
    pieConfig["holeSize"] = m_pieConfig.holeSize;
    pieConfig["startAngle"] = m_pieConfig.startAngle;
    pieConfig["endAngle"] = m_pieConfig.endAngle;
    pieConfig["showSliceBorders"] = m_pieConfig.showSliceBorders;
    pieConfig["sliceBorderColor"] = m_pieConfig.sliceBorderColor.name();
    pieConfig["sliceBorderWidth"] = m_pieConfig.sliceBorderWidth;
    pieConfig["sliceOpacity"] = m_pieConfig.sliceOpacity;
    pieConfig["labelPosition"] = static_cast<int>(m_pieConfig.labelPosition);
    pieConfig["labelContent"] = static_cast<int>(m_pieConfig.labelContent);
    pieConfig["labelFont"] = m_pieConfig.labelFont.toString();
    pieConfig["labelColor"] = m_pieConfig.labelColor.name();
    pieConfig["labelDistance"] = m_pieConfig.labelDistance;
    pieConfig["aggregation"] = static_cast<int>(m_pieConfig.aggregation);
    pieConfig["minSliceThreshold"] = m_pieConfig.minSliceThreshold;
    pieConfig["otherSliceName"] = m_pieConfig.otherSliceName;
    pieConfig["otherSliceColor"] = m_pieConfig.otherSliceColor.name();
    pieConfig["combineSmallSlices"] = m_pieConfig.combineSmallSlices;
    pieConfig["enableAnimations"] = m_pieConfig.enableAnimations;
    pieConfig["animationDuration"] = m_pieConfig.animationDuration;
    pieConfig["animationEasing"] = static_cast<int>(m_pieConfig.animationEasing);
    pieConfig["enableSliceExplosion"] = m_pieConfig.enableSliceExplosion;
    pieConfig["explosionDistance"] = m_pieConfig.explosionDistance;
    pieConfig["enableAutoRotation"] = m_pieConfig.enableAutoRotation;
    pieConfig["rotationSpeed"] = m_pieConfig.rotationSpeed;
    pieConfig["enableRealTimeMode"] = m_pieConfig.enableRealTimeMode;
    pieConfig["updateInterval"] = m_pieConfig.updateInterval;
    
    settings["pieConfig"] = pieConfig;
    
    // Slice configurations
    QJsonArray sliceArray;
    for (const auto& pair : m_sliceConfigs) {
        QJsonObject sliceObj;
        sliceObj["fieldPath"] = pair.first;
        sliceObj["config"] = pair.second.toJson();
        sliceArray.append(sliceObj);
    }
    settings["sliceConfigs"] = sliceArray;
    
    // Current values
    QJsonArray valuesArray;
    for (const auto& pair : m_sliceData) {
        QJsonObject valueObj;
        valueObj["fieldPath"] = pair.first;
        valueObj["value"] = pair.second->value;
        valuesArray.append(valueObj);
    }
    settings["sliceValues"] = valuesArray;
    
    settings["totalValue"] = m_totalValue;
    
    return settings;
}

bool PieChartWidget::restoreWidgetSpecificSettings(const QJsonObject& settings) {
    // Restore base chart settings
    if (!ChartWidget::restoreWidgetSpecificSettings(settings)) {
        return false;
    }
    
    // Restore pie chart configuration
    if (settings.contains("pieConfig")) {
        QJsonObject pieConfig = settings["pieConfig"].toObject();
        
        m_pieConfig.holeSize = pieConfig.value("holeSize").toDouble(0.0);
        m_pieConfig.startAngle = pieConfig.value("startAngle").toDouble(0.0);
        m_pieConfig.endAngle = pieConfig.value("endAngle").toDouble(360.0);
        m_pieConfig.showSliceBorders = pieConfig.value("showSliceBorders").toBool(true);
        m_pieConfig.sliceBorderColor = QColor(pieConfig.value("sliceBorderColor").toString("#ffffff"));
        m_pieConfig.sliceBorderWidth = pieConfig.value("sliceBorderWidth").toInt(2);
        m_pieConfig.sliceOpacity = pieConfig.value("sliceOpacity").toDouble(1.0);
        m_pieConfig.labelPosition = static_cast<PieChartConfig::LabelPosition>(
            pieConfig.value("labelPosition").toInt());
        m_pieConfig.labelContent = static_cast<PieChartConfig::LabelContent>(
            pieConfig.value("labelContent").toInt());
        m_pieConfig.labelFont.fromString(pieConfig.value("labelFont").toString());
        m_pieConfig.labelColor = QColor(pieConfig.value("labelColor").toString("#000000"));
        m_pieConfig.labelDistance = pieConfig.value("labelDistance").toDouble(1.15);
        m_pieConfig.aggregation = static_cast<PieChartConfig::AggregationMethod>(
            pieConfig.value("aggregation").toInt());
        m_pieConfig.minSliceThreshold = pieConfig.value("minSliceThreshold").toDouble(0.02);
        m_pieConfig.otherSliceName = pieConfig.value("otherSliceName").toString("Others");
        m_pieConfig.otherSliceColor = QColor(pieConfig.value("otherSliceColor").toString("#808080"));
        m_pieConfig.combineSmallSlices = pieConfig.value("combineSmallSlices").toBool(true);
        m_pieConfig.enableAnimations = pieConfig.value("enableAnimations").toBool(true);
        m_pieConfig.animationDuration = pieConfig.value("animationDuration").toInt(1000);
        m_pieConfig.animationEasing = static_cast<QEasingCurve::Type>(
            pieConfig.value("animationEasing").toInt());
        m_pieConfig.enableSliceExplosion = pieConfig.value("enableSliceExplosion").toBool(true);
        m_pieConfig.explosionDistance = pieConfig.value("explosionDistance").toDouble(0.1);
        m_pieConfig.enableAutoRotation = pieConfig.value("enableAutoRotation").toBool(false);
        m_pieConfig.rotationSpeed = pieConfig.value("rotationSpeed").toInt(30);
        m_pieConfig.enableRealTimeMode = pieConfig.value("enableRealTimeMode").toBool(true);
        m_pieConfig.updateInterval = pieConfig.value("updateInterval").toInt(200);
    }
    
    // Restore slice configurations
    if (settings.contains("sliceConfigs")) {
        m_sliceConfigs.clear();
        QJsonArray sliceArray = settings["sliceConfigs"].toArray();
        
        for (const QJsonValue& value : sliceArray) {
            QJsonObject sliceObj = value.toObject();
            QString fieldPath = sliceObj.value("fieldPath").toString();
            SliceConfig config(sliceObj.value("config").toObject());
            m_sliceConfigs[fieldPath] = config;
        }
    }
    
    // Restore slice values
    if (settings.contains("sliceValues")) {
        QJsonArray valuesArray = settings["sliceValues"].toArray();
        
        for (const QJsonValue& value : valuesArray) {
            QJsonObject valueObj = value.toObject();
            QString fieldPath = valueObj.value("fieldPath").toString();
            double sliceValue = valueObj.value("value").toDouble();
            
            auto dataIt = m_sliceData.find(fieldPath);
            if (dataIt != m_sliceData.end()) {
                dataIt->second->value = sliceValue;
                dataIt->second->needsUpdate = true;
            }
        }
    }
    
    // Restore total value
    m_totalValue = settings.value("totalValue").toDouble(0.0);
    
    // Apply configuration
    setPieChartConfig(m_pieConfig);
    
    return true;
}

void PieChartWidget::setupContextMenu() {
    ChartWidget::setupContextMenu();
    
    // Add pie chart specific context menu items
    if (QMenu* menu = getContextMenu()) {
        menu->addSeparator();
        
        QAction* donutModeAction = menu->addAction("Donut Mode");
        donutModeAction->setCheckable(true);
        donutModeAction->setChecked(m_pieConfig.holeSize > 0.0);
        connect(donutModeAction, &QAction::triggered, [this](bool checked) {
            setHoleSize(checked ? 0.3 : 0.0);
        });
        
        QAction* autoRotateAction = menu->addAction("Auto-rotate");
        autoRotateAction->setCheckable(true);
        autoRotateAction->setChecked(m_pieConfig.enableAutoRotation);
        connect(autoRotateAction, &QAction::triggered, this, &PieChartWidget::onToggleAutoRotation);
        
        menu->addSeparator();
        menu->addAction("Explode All", this, &PieChartWidget::onExplodeAllSlices);
        menu->addAction("Implode All", this, &PieChartWidget::onImplodeAllSlices);
        menu->addSeparator();
        menu->addAction("Clear All Data", this, &PieChartWidget::onClearData);
    }
}

// Slots implementation
void PieChartWidget::onHoleSizeChanged(int value) {
    setHoleSize(value / 100.0);  // Convert 0-90 to 0.0-0.9
}

void PieChartWidget::onToggleAutoRotation(bool enabled) {
    setAutoRotation(enabled);
}

void PieChartWidget::onRotationSpeedChanged(int speed) {
    setRotationSpeed(speed);
}

void PieChartWidget::onToggleRealTimeMode(bool enabled) {
    setRealTimeMode(enabled);
}

void PieChartWidget::onToggleSliceLabels(bool enabled) {
    if (enabled) {
        if (m_pieConfig.labelContent == PieChartConfig::LabelContent::None) {
            m_pieConfig.labelContent = PieChartConfig::LabelContent::LabelAndPercentage;
        }
    } else {
        m_pieConfig.labelContent = PieChartConfig::LabelContent::None;
    }
    
    if (m_pieSeries) {
        m_pieSeries->setLabelsVisible(enabled);
    }
    
    if (m_labelContentCombo) {
        m_labelContentCombo->setCurrentIndex(static_cast<int>(m_pieConfig.labelContent));
    }
    
    updateSliceLabels();
}

void PieChartWidget::onClearData() {
    clearAllData();
}

void PieChartWidget::onExplodeAllSlices() {
    explodeAllSlices(true);
}

void PieChartWidget::onImplodeAllSlices() {
    explodeAllSlices(false);
}

void PieChartWidget::onSliceHovered(bool state) {
    QPieSlice* slice = qobject_cast<QPieSlice*>(sender());
    if (!slice) return;
    
    // Find which field path this slice belongs to
    QString fieldPath;
    for (const auto& pair : m_sliceData) {
        if (pair.second->pieSlice == slice) {
            fieldPath = pair.first;
            break;
        }
    }
    
    if (!fieldPath.isEmpty()) {
        double value = slice->value();
        double percentage = getSlicePercentage(fieldPath);
        emit sliceHovered(fieldPath, value, percentage, state);
        
        if (state && getChartConfig().enableTooltips) {
            QString tooltip = QString("%1\nValue: %2\nPercentage: %3%")
                .arg(fieldPath)
                .arg(formatValue(value))
                .arg(formatPercentage(percentage));
            
            // Show tooltip at mouse position
            QPoint mousePos = QCursor::pos();
            QPoint localPos = m_chartView->mapFromGlobal(mousePos);
            showTooltip(QPointF(localPos), tooltip);
        } else {
            hideTooltip();
        }
    }
}

void PieChartWidget::onSliceClicked() {
    QPieSlice* slice = qobject_cast<QPieSlice*>(sender());
    if (!slice) return;
    
    // Find which field path this slice belongs to
    QString fieldPath;
    for (const auto& pair : m_sliceData) {
        if (pair.second->pieSlice == slice) {
            fieldPath = pair.first;
            break;
        }
    }
    
    if (!fieldPath.isEmpty()) {
        double value = slice->value();
        double percentage = getSlicePercentage(fieldPath);
        emit sliceClicked(fieldPath, value, percentage);
        
        // Toggle explosion if enabled
        if (m_pieConfig.enableSliceExplosion) {
            explodeSlice(fieldPath, !slice->isExploded());
        }
    }
}

void PieChartWidget::onRealTimeUpdate() {
    // This slot is called by the real-time timer
    // Update display if we have pending updates
    bool hasUpdates = false;
    for (const auto& pair : m_sliceData) {
        if (pair.second->needsUpdate) {
            hasUpdates = true;
            break;
        }
    }
    
    if (hasUpdates) {
        updateSeriesData();
    }
}

void PieChartWidget::onAutoRotationUpdate() {
    // Update rotation angle
    double deltaAngle = (m_pieConfig.rotationSpeed * 16) / 1000.0; // 16ms timer interval
    m_currentRotation += deltaAngle;
    if (m_currentRotation >= 360.0) {
        m_currentRotation -= 360.0;
    }
    
    // Apply rotation to pie series
    if (m_pieSeries) {
        double currentStart = m_pieSeries->pieStartAngle();
        m_pieSeries->setPieStartAngle(currentStart + deltaAngle);
    }
}

void PieChartWidget::onSliceAnimationFinished() {
    // Handle animation completion if needed
    QPropertyAnimation* animation = qobject_cast<QPropertyAnimation*>(sender());
    if (animation) {
        // Find the slice data that owns this animation
        for (auto& pair : m_sliceData) {
            if (pair.second->explosionAnimation == animation) {
                pair.second->isAnimating = false;
                break;
            }
        }
    }
}

// Helper method implementations
void PieChartWidget::updateRealTimeSettings() {
    m_realTimeTimer->setInterval(m_pieConfig.updateInterval);
    
    if (m_pieConfig.enableRealTimeMode) {
        if (!m_realTimeTimer->isActive()) {
            m_realTimeTimer->start();
        }
    } else {
        m_realTimeTimer->stop();
    }
}

void PieChartWidget::updateAutoRotationSettings() {
    if (m_pieConfig.enableAutoRotation) {
        if (!m_rotationTimer->isActive()) {
            m_rotationTimer->start();
        }
    } else {
        m_rotationTimer->stop();
    }
}

void PieChartWidget::applySliceConfig(QPieSlice* slice, const SliceConfig& config) {
    if (!slice) return;
    
    slice->setColor(config.color);
    slice->setBorderColor(config.borderColor);
    slice->setBorderWidth(config.borderWidth);
    //slice->setVisible(config.visible); // Not available in Qt6
    slice->setExploded(config.exploded);
    
    // Apply gradient if configured
    if (config.useGradient) {
        applySliceGradient(slice, config);
    }
    
    // Apply shadow if configured
    if (config.dropShadow) {
        applySliceShadow(slice, config);
    }
}

void PieChartWidget::updateSliceLabels() {
    if (!m_pieSeries) return;
    
    bool labelsVisible = (m_pieConfig.labelContent != PieChartConfig::LabelContent::None);
    m_pieSeries->setLabelsVisible(labelsVisible);
    
    if (labelsVisible) {
        // Update label format for all slices
        for (const auto& pair : m_sliceData) {
            const QString& fieldPath = pair.first;
            SliceData* data = pair.second.get();
            
            if (data->pieSlice) {
                double value = data->value;
                double percentage = getSlicePercentage(fieldPath);
                QString label = formatSliceLabel(fieldPath, value, percentage);
                data->pieSlice->setLabel(label);
            }
        }
    }
}

void PieChartWidget::addDataPoint(const QString& fieldPath, double value) {
    auto it = m_sliceData.find(fieldPath);
    if (it != m_sliceData.end()) {
        it->second->addValue(value, m_pieConfig.aggregation);
    }
}

void PieChartWidget::recalculatePercentages() {
    // Percentages are automatically calculated by Qt Charts based on slice values
    // This method can be used for additional percentage-based operations if needed
}

void PieChartWidget::updateTotalValue() {
    m_totalValue = 0.0;
    for (const auto& pair : m_sliceData) {
        if (pair.second->config.visible) {
            m_totalValue += pair.second->value;
        }
    }
}

void PieChartWidget::animateSliceExplosion(const QString& fieldPath, bool explode) {
    auto it = m_sliceData.find(fieldPath);
    if (it == m_sliceData.end() || !it->second->pieSlice) {
        return;
    }
    
    SliceData* data = it->second.get();
    
    // Stop any existing animation
    if (data->explosionAnimation) {
        data->explosionAnimation->stop();
        delete data->explosionAnimation;
    }
    
    // Create new animation
    data->explosionAnimation = new QPropertyAnimation(data->pieSlice, "exploded");
    data->explosionAnimation->setDuration(m_pieConfig.animationDuration / 2); // Faster for explosion
    data->explosionAnimation->setEasingCurve(QEasingCurve::OutBounce);
    
    connect(data->explosionAnimation, &QPropertyAnimation::finished,
            this, &PieChartWidget::onSliceAnimationFinished);
    
    data->explosionAnimation->setStartValue(!explode);
    data->explosionAnimation->setEndValue(explode);
    data->isAnimating = true;
    data->explosionAnimation->start();
}

void PieChartWidget::animateSliceUpdate(const QString& fieldPath) {
    auto it = m_sliceData.find(fieldPath);
    if (it == m_sliceData.end() || !it->second->pieSlice) {
        return;
    }
    
    SliceData* data = it->second.get();
    QPieSlice* slice = data->pieSlice;
    
    // Create animation for value change
    QPropertyAnimation* valueAnimation = new QPropertyAnimation(slice, "value");
    valueAnimation->setDuration(m_pieConfig.animationDuration);
    valueAnimation->setEasingCurve(static_cast<QEasingCurve::Type>(m_pieConfig.animationEasing));
    valueAnimation->setStartValue(slice->value());
    valueAnimation->setEndValue(data->value);
    
    // Add to animation group
    m_animationGroup->addAnimation(valueAnimation);
    
    if (!(m_animationGroup->state() == QAbstractAnimation::Running)) {
        m_animationGroup->start();
    }
}

QString PieChartWidget::formatSliceLabel(const QString& fieldPath, double value, double percentage) const {
    switch (m_pieConfig.labelContent) {
        case PieChartConfig::LabelContent::None:
            return "";
        case PieChartConfig::LabelContent::Value:
            return formatValue(value);
        case PieChartConfig::LabelContent::Percentage:
            return formatPercentage(percentage);
        case PieChartConfig::LabelContent::Label:
            return fieldPath;
        case PieChartConfig::LabelContent::ValueAndPercentage:
            return QString("%1 (%2)").arg(formatValue(value), formatPercentage(percentage));
        case PieChartConfig::LabelContent::LabelAndPercentage:
            return QString("%1\n%2").arg(fieldPath, formatPercentage(percentage));
        case PieChartConfig::LabelContent::All:
            return QString("%1\n%2 (%3)").arg(fieldPath, formatValue(value), formatPercentage(percentage));
    }
    return "";
}

QString PieChartWidget::formatValue(double value) const {
    return QString::number(value, 'f', 2);
}

QString PieChartWidget::formatPercentage(double percentage) const {
    return QString::number(percentage, 'f', 1) + "%";
}

void PieChartWidget::applySliceGradient(QPieSlice* slice, const SliceConfig& config) {
    Q_UNUSED(slice);
    Q_UNUSED(config);
    // Gradient implementation would require custom drawing or QML
    // This is a placeholder for future enhancement
}

void PieChartWidget::applySliceShadow(QPieSlice* slice, const SliceConfig& config) {
    Q_UNUSED(slice);
    Q_UNUSED(config);
    // Shadow implementation would require custom drawing or QML
    // This is a placeholder for future enhancement
}

bool PieChartWidget::shouldCombineSlice(double percentage) const {
    return m_pieConfig.combineSmallSlices && 
           (percentage < m_pieConfig.minSliceThreshold * 100.0);
}

void PieChartWidget::combineSmallSlices() {
    // This is a simplified implementation
    // A full implementation would collect small slices and create a combined "Others" slice
    
    std::vector<QString> smallSlices;
    double combinedValue = 0.0; Q_UNUSED(combinedValue);
    
    for (const auto& pair : m_sliceData) {
        double percentage = getSlicePercentage(pair.first);
        if (shouldCombineSlice(percentage)) {
            smallSlices.push_back(pair.first);
            combinedValue += pair.second->value;
        }
    }
    
    if (smallSlices.size() > 1) {
        // Remove small slices and create "Others" slice
        // This is complex to implement properly with Qt Charts
        // For now, we just hide small slices
        for (const QString& fieldPath : smallSlices) {
            setSliceVisible(fieldPath, false);
        }
    }
}

QColor PieChartWidget::getNextSliceColor() const {
    return Monitor::Charts::ColorPalette::getColor(m_sliceData.size());
}