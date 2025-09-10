#include "line_chart_widget.h"
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <QApplication>
#include <QGraphicsProxyWidget>
#include <QDebug>
#include <algorithm>
#include <numeric>
#include <cmath>


// LineSeriesConfig implementation
LineChartWidget::LineSeriesConfig::LineSeriesConfig(const QJsonObject& json) {
    lineStyle = static_cast<LineChartConfig::LineStyle>(json.value("lineStyle").toInt());
    pointStyle = static_cast<LineChartConfig::PointStyle>(json.value("pointStyle").toInt());
    interpolation = static_cast<LineChartConfig::InterpolationMethod>(json.value("interpolation").toInt());
    lineWidth = json.value("lineWidth").toInt(2);
    pointSize = json.value("pointSize").toDouble(6.0);
    showPoints = json.value("showPoints").toBool(false);
    connectPoints = json.value("connectPoints").toBool(true);
    fillColor = QColor(json.value("fillColor").toString());
    fillOpacity = json.value("fillOpacity").toDouble(0.3);
    enableSmoothing = json.value("enableSmoothing").toBool(false);
    smoothingWindow = json.value("smoothingWindow").toInt(5);
}

QJsonObject LineChartWidget::LineSeriesConfig::toJson() const {
    QJsonObject json;
    json["lineStyle"] = static_cast<int>(lineStyle);
    json["pointStyle"] = static_cast<int>(pointStyle);
    json["interpolation"] = static_cast<int>(interpolation);
    json["lineWidth"] = lineWidth;
    json["pointSize"] = pointSize;
    json["showPoints"] = showPoints;
    json["connectPoints"] = connectPoints;
    json["fillColor"] = fillColor.name();
    json["fillOpacity"] = fillOpacity;
    json["enableSmoothing"] = enableSmoothing;
    json["smoothingWindow"] = smoothingWindow;
    return json;
}

// SeriesData implementation
void LineChartWidget::SeriesData::addPoint(const QPointF& point, const QVariant& rawValue) {
    points.push_back(point);
    rawValues.push_back(rawValue);
    timestamps.push_back(std::chrono::steady_clock::now());
    needsUpdate = true;
}

void LineChartWidget::SeriesData::addPoint(double x, double y, const QVariant& rawValue) {
    addPoint(QPointF(x, y), rawValue);
}

void LineChartWidget::SeriesData::clearData() {
    points.clear();
    rawValues.clear();
    timestamps.clear();
    needsUpdate = true;
}

std::vector<QPointF> LineChartWidget::SeriesData::getPointsInRange(double xMin, double xMax) const {
    std::vector<QPointF> result;
    
    for (const auto& point : points) {
        if (point.x() >= xMin && point.x() <= xMax) {
            result.push_back(point);
        }
    }
    
    return result;
}

void LineChartWidget::SeriesData::limitDataPoints(int maxPoints) {
    if (static_cast<int>(points.size()) > maxPoints) {
        int toRemove = points.size() - maxPoints;
        points.erase(points.begin(), points.begin() + toRemove);
        rawValues.erase(rawValues.begin(), rawValues.begin() + toRemove);
        timestamps.erase(timestamps.begin(), timestamps.begin() + toRemove);
        needsUpdate = true;
    }
}

// LineChartWidget implementation
LineChartWidget::LineChartWidget(const QString& widgetId, QWidget* parent)
    : ChartWidget(widgetId, "Line Chart", parent)
    , m_realTimeTimer(new QTimer(this))
    , m_realTimeModeCheckBox(nullptr)
    , m_interpolationCombo(nullptr)
    , m_maxPointsSpin(nullptr)
    , m_packetSequence(0)
    , m_showingTooltip(false)
{
    // Initialize line chart configuration
    m_lineConfig = LineChartConfig();
    
    // Setup real-time update timer
    m_realTimeTimer->setSingleShot(false);
    m_realTimeTimer->setInterval(16); // ~60 FPS
    connect(m_realTimeTimer, &QTimer::timeout, this, &LineChartWidget::onRealTimeUpdate);
    
    // Start real-time timer if enabled
    if (m_lineConfig.enableRealTimeMode) {
        m_realTimeTimer->start();
    }
}

LineChartWidget::~LineChartWidget() {
    // Clean up series data
    m_seriesData.clear();
    m_lineSeriesConfigs.clear();
}

void LineChartWidget::createChart() {
    // Create the chart
    m_chart = new QChart();
    m_chart->setTitle("Line Chart");
    m_chart->setAnimationOptions(QChart::SeriesAnimations);
    
    // Setup toolbar extensions
    setupToolbarExtensions();
    
    // Apply initial configuration
    applyChartConfig();
}

void LineChartWidget::setupToolbarExtensions() {
    if (!m_toolbar) return;
    
    // Add line chart specific toolbar items
    m_toolbar->addSeparator();
    
    // Real-time mode toggle
    m_realTimeModeCheckBox = new QCheckBox("Real-time");
    m_realTimeModeCheckBox->setChecked(m_lineConfig.enableRealTimeMode);
    connect(m_realTimeModeCheckBox, &QCheckBox::toggled, 
            this, &LineChartWidget::onToggleRealTimeMode);
    m_toolbar->addWidget(m_realTimeModeCheckBox);
    
    // Interpolation method
    m_toolbar->addWidget(new QLabel("Interpolation:"));
    m_interpolationCombo = new QComboBox();
    m_interpolationCombo->addItems({"Linear", "Spline", "Step"});
    m_interpolationCombo->setCurrentIndex(static_cast<int>(m_lineConfig.interpolation));
    connect(m_interpolationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineChartWidget::onChangeInterpolationMethod);
    m_toolbar->addWidget(m_interpolationCombo);
    
    // Max points spinner
    m_toolbar->addWidget(new QLabel("Max Points:"));
    m_maxPointsSpin = new QSpinBox();
    m_maxPointsSpin->setRange(100, 100000);
    m_maxPointsSpin->setValue(m_lineConfig.maxDataPoints);
    m_maxPointsSpin->setSingleStep(1000);
    connect(m_maxPointsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int value) {
                m_lineConfig.maxDataPoints = value;
                // Update existing series data limits
                for (auto& pair : m_seriesData) {
                    pair.second->limitDataPoints(value);
                }
            });
    m_toolbar->addWidget(m_maxPointsSpin);
    
    m_toolbar->addSeparator();
    
    // Add utility actions
    QAction* clearDataAction = m_toolbar->addAction("Clear Data");
    connect(clearDataAction, &QAction::triggered, this, &LineChartWidget::onClearData);
    
    QAction* scrollToLatestAction = m_toolbar->addAction("Latest");
    connect(scrollToLatestAction, &QAction::triggered, this, &LineChartWidget::onScrollToLatest);
}

void LineChartWidget::setLineChartConfig(const LineChartConfig& config) {
    m_lineConfig = config;
    updateRealTimeSettings();
    updateAxes();
    
    // Update toolbar controls
    if (m_realTimeModeCheckBox) {
        m_realTimeModeCheckBox->setChecked(m_lineConfig.enableRealTimeMode);
    }
    if (m_interpolationCombo) {
        m_interpolationCombo->setCurrentIndex(static_cast<int>(m_lineConfig.interpolation));
    }
    if (m_maxPointsSpin) {
        m_maxPointsSpin->setValue(m_lineConfig.maxDataPoints);
    }
    
    // Update all series appearances
    for (const auto& pair : m_seriesData) {
        updateSeriesAppearance(pair.first);
    }
}

void LineChartWidget::resetLineChartConfig() {
    m_lineConfig = LineChartConfig();
    setLineChartConfig(m_lineConfig);
}

bool LineChartWidget::addLineSeries(const QString& fieldPath, const QString& seriesName,
                                   const QColor& color, const LineSeriesConfig& config) {
    SeriesConfig baseConfig;
    baseConfig.fieldPath = fieldPath;
    baseConfig.seriesName = seriesName.isEmpty() ? fieldPath : seriesName;
    baseConfig.color = color.isValid() ? color : Monitor::Charts::ColorPalette::getColor(m_nextColorIndex);
    
    // Store line-specific configuration
    m_lineSeriesConfigs[fieldPath] = config;
    
    return addSeries(fieldPath, baseConfig);
}

void LineChartWidget::setLineSeriesConfig(const QString& fieldPath, const LineSeriesConfig& config) {
    auto it = m_lineSeriesConfigs.find(fieldPath);
    if (it == m_lineSeriesConfigs.end()) {
        return;
    }
    
    it->second = config;
    updateSeriesAppearance(fieldPath);
}

LineChartWidget::LineSeriesConfig LineChartWidget::getLineSeriesConfig(const QString& fieldPath) const {
    auto it = m_lineSeriesConfigs.find(fieldPath);
    return (it != m_lineSeriesConfigs.end()) ? it->second : LineSeriesConfig();
}

QAbstractSeries* LineChartWidget::createSeriesForField(const QString& fieldPath, const SeriesConfig& config) {
    // Get line-specific configuration
    auto lineConfigIt = m_lineSeriesConfigs.find(fieldPath);
    LineSeriesConfig lineConfig = (lineConfigIt != m_lineSeriesConfigs.end()) ? 
        lineConfigIt->second : LineSeriesConfig();
    
    // Create series data storage
    auto seriesData = std::make_unique<SeriesData>();
    seriesData->config = lineConfig;
    
    // Create the appropriate Qt series based on interpolation method
    QAbstractSeries* series = nullptr;
    
    switch (lineConfig.interpolation) {
        case LineChartConfig::InterpolationMethod::Linear: {
            QLineSeries* lineSeries = new QLineSeries();
            lineSeries->setName(config.seriesName);
            applyLineSeriesConfig(lineSeries, lineConfig);
            seriesData->lineSeries = lineSeries;
            series = lineSeries;
            
            // Connect signals
            connect(lineSeries, &QLineSeries::hovered, this, &LineChartWidget::onSeriesHovered);
            connect(lineSeries, &QLineSeries::clicked, this, &LineChartWidget::onSeriesClicked);
            break;
        }
        
        case LineChartConfig::InterpolationMethod::Spline: {
            QSplineSeries* splineSeries = new QSplineSeries();
            splineSeries->setName(config.seriesName);
            applySplineSeriesConfig(splineSeries, lineConfig);
            seriesData->splineSeries = splineSeries;
            series = splineSeries;
            
            // Connect signals
            connect(splineSeries, &QSplineSeries::hovered, this, &LineChartWidget::onSeriesHovered);
            connect(splineSeries, &QSplineSeries::clicked, this, &LineChartWidget::onSeriesClicked);
            break;
        }
        
        case LineChartConfig::InterpolationMethod::Step: {
            // For step interpolation, use line series with step-like data points
            QLineSeries* lineSeries = new QLineSeries();
            lineSeries->setName(config.seriesName);
            applyLineSeriesConfig(lineSeries, lineConfig);
            seriesData->lineSeries = lineSeries;
            series = lineSeries;
            
            // Connect signals
            connect(lineSeries, &QLineSeries::hovered, this, &LineChartWidget::onSeriesHovered);
            connect(lineSeries, &QLineSeries::clicked, this, &LineChartWidget::onSeriesClicked);
            break;
        }
    }
    
    // Create scatter series for points if needed
    if (lineConfig.showPoints && series) {
        QScatterSeries* pointSeries = new QScatterSeries();
        pointSeries->setName(config.seriesName + " Points");
        pointSeries->setColor(config.color);
        pointSeries->setMarkerSize(lineConfig.pointSize);
        applyScatterSeriesConfig(pointSeries, lineConfig);
        seriesData->pointSeries = pointSeries;
        
        // Add point series to chart separately
        m_chart->addSeries(pointSeries);
    }
    
    // Store series data
    m_seriesData[fieldPath] = std::move(seriesData);
    
    return series;
}

void LineChartWidget::removeSeriesForField(const QString& fieldPath) {
    auto it = m_seriesData.find(fieldPath);
    if (it != m_seriesData.end()) {
        // Remove point series if it exists
        if (it->second->pointSeries && m_chart) {
            m_chart->removeSeries(it->second->pointSeries);
            delete it->second->pointSeries;
        }
        
        // Remove from storage
        m_seriesData.erase(it);
    }
    
    // Remove line configuration
    m_lineSeriesConfigs.erase(fieldPath);
}

void LineChartWidget::configureSeries(const QString& fieldPath, const SeriesConfig& config) {
    auto it = m_seriesData.find(fieldPath);
    if (it == m_seriesData.end()) {
        return;
    }
    
    SeriesData* data = it->second.get();
    
    // Update line series
    if (data->lineSeries) {
        data->lineSeries->setName(config.seriesName);
        data->lineSeries->setColor(config.color);
        data->lineSeries->setVisible(config.visible);
        applyLineSeriesConfig(data->lineSeries, data->config);
    }
    
    // Update spline series
    if (data->splineSeries) {
        data->splineSeries->setName(config.seriesName);
        data->splineSeries->setColor(config.color);
        data->splineSeries->setVisible(config.visible);
        applySplineSeriesConfig(data->splineSeries, data->config);
    }
    
    // Update point series
    if (data->pointSeries) {
        data->pointSeries->setName(config.seriesName + " Points");
        data->pointSeries->setColor(config.color);
        data->pointSeries->setVisible(config.visible && data->config.showPoints);
        applyScatterSeriesConfig(data->pointSeries, data->config);
    }
}

void LineChartWidget::updateSeriesData() {
    // Update all series that need updates
    for (auto& pair : m_seriesData) {
        const QString& fieldPath = pair.first;
        SeriesData* data = pair.second.get();
        
        if (!data->needsUpdate) continue;
        
        // Convert deque to vector for Qt Charts
        std::vector<QPointF> points(data->points.begin(), data->points.end());
        
        // Apply smoothing if enabled
        if (data->config.enableSmoothing && (int)points.size() > data->config.smoothingWindow) {
            points = smoothData(points, data->config.smoothingWindow);
        }
        
        // Handle step interpolation
        if (data->config.interpolation == LineChartConfig::InterpolationMethod::Step && points.size() > 1) {
            std::vector<QPointF> stepPoints;
            stepPoints.reserve(points.size() * 2);
            
            for (size_t i = 0; i < points.size() - 1; ++i) {
                stepPoints.push_back(points[i]);
                stepPoints.push_back(QPointF(points[i+1].x(), points[i].y())); // Horizontal line
            }
            stepPoints.push_back(points.back());
            points = stepPoints;
        }
        
        // Decimate data if needed for performance
        if (shouldDecimateData(fieldPath) && points.size() > static_cast<size_t>(m_lineConfig.maxDataPoints)) {
            points = Monitor::Charts::DataConverter::decimateData(
                points, m_lineConfig.maxDataPoints, 
                Monitor::Charts::DecimationStrategy::LTTB);
        }
        
        // Update line series
        if (data->lineSeries) {
            data->lineSeries->clear();
            for (const auto& point : points) {
                data->lineSeries->append(point);
            }
        }
        
        // Update spline series
        if (data->splineSeries) {
            data->splineSeries->clear();
            for (const auto& point : points) {
                data->splineSeries->append(point);
            }
        }
        
        // Update point series with original (non-step) points
        if (data->pointSeries && data->config.showPoints) {
            data->pointSeries->clear();
            std::vector<QPointF> originalPoints(data->points.begin(), data->points.end());
            for (const auto& point : originalPoints) {
                data->pointSeries->append(point);
            }
        }
        
        data->needsUpdate = false;
        
        // Emit signal for new data
        if (!data->points.empty()) {
            emit dataPointAdded(fieldPath, data->points.back());
        }
    }
    
    // Auto-scale axes if enabled
    if (m_lineConfig.autoScaleX || m_lineConfig.autoScaleY) {
        autoScaleAxes();
    }
    
    // Scroll to latest data in real-time mode
    if (m_lineConfig.enableRealTimeMode) {
        scrollToShowLatestData();
    }
    
    // Update current point count for performance monitoring
    m_currentPointCount = 0;
    for (const auto& pair : m_seriesData) {
        m_currentPointCount += pair.second->points.size();
    }
}

void LineChartWidget::updateFieldDisplay(const QString& fieldPath, const QVariant& value) {
    auto it = m_seriesData.find(fieldPath);
    if (it == m_seriesData.end()) {
        return;
    }
    
    // Calculate X value based on configuration
    double xValue = calculateXValue(value, fieldPath);
    
    // Convert field value to Y coordinate
    bool ok;
    double yValue = Monitor::Charts::DataConverter::toDouble(value, &ok);
    if (!ok) {
        qWarning() << "LineChartWidget: Cannot convert value to double for field" << fieldPath;
        return;
    }
    
    // Create data point
    QPointF point(xValue, yValue);
    
    // Add to series data
    it->second->addPoint(point, value);
    it->second->lastXValue = xValue;
    
    // Limit data points if needed
    if (m_lineConfig.rollingData) {
        it->second->limitDataPoints(m_lineConfig.maxDataPoints);
    }
    
    // For immediate update mode, update display immediately
    if (getChartConfig().updateMode == UpdateMode::Immediate) {
        it->second->needsUpdate = true;
        updateSeriesData();
    }
}

void LineChartWidget::clearFieldDisplay(const QString& fieldPath) {
    auto it = m_seriesData.find(fieldPath);
    if (it != m_seriesData.end()) {
        it->second->clearData();
        it->second->needsUpdate = true;
        
        // Clear the Qt series immediately
        if (it->second->lineSeries) {
            it->second->lineSeries->clear();
        }
        if (it->second->splineSeries) {
            it->second->splineSeries->clear();
        }
        if (it->second->pointSeries) {
            it->second->pointSeries->clear();
        }
        
        emit seriesDataCleared(fieldPath);
    }
}

void LineChartWidget::refreshAllDisplays() {
    // Mark all series for update
    for (auto& pair : m_seriesData) {
        pair.second->needsUpdate = true;
    }
    
    updateSeriesData();
}

// Data access methods
std::vector<QPointF> LineChartWidget::getSeriesData(const QString& fieldPath) const {
    auto it = m_seriesData.find(fieldPath);
    if (it != m_seriesData.end()) {
        return std::vector<QPointF>(it->second->points.begin(), it->second->points.end());
    }
    return {};
}

std::vector<QPointF> LineChartWidget::getSeriesDataInRange(const QString& fieldPath, double xMin, double xMax) const {
    auto it = m_seriesData.find(fieldPath);
    if (it != m_seriesData.end()) {
        return it->second->getPointsInRange(xMin, xMax);
    }
    return {};
}

QPointF LineChartWidget::getLastDataPoint(const QString& fieldPath) const {
    auto it = m_seriesData.find(fieldPath);
    if (it != m_seriesData.end() && !it->second->points.empty()) {
        return it->second->points.back();
    }
    return QPointF();
}

int LineChartWidget::getSeriesPointCount(const QString& fieldPath) const {
    auto it = m_seriesData.find(fieldPath);
    if (it != m_seriesData.end()) {
        return static_cast<int>(it->second->points.size());
    }
    return 0;
}

// Axis control
void LineChartWidget::setXAxisFieldPath(const QString& fieldPath) {
    m_lineConfig.xAxisFieldPath = fieldPath;
    m_lineConfig.xAxisType = LineChartConfig::XAxisType::FieldValue;
    updateAxes();
}

void LineChartWidget::setXAxisType(LineChartConfig::XAxisType type) {
    m_lineConfig.xAxisType = type;
    updateAxes();
}

// Real-time control
void LineChartWidget::setRealTimeMode(bool enabled) {
    m_lineConfig.enableRealTimeMode = enabled;
    updateRealTimeSettings();
    
    if (m_realTimeModeCheckBox) {
        m_realTimeModeCheckBox->setChecked(enabled);
    }
    
    emit realTimeModeChanged(enabled);
}

void LineChartWidget::scrollToLatest() {
    scrollToShowLatestData();
}

void LineChartWidget::clearSeriesData(const QString& fieldPath) {
    clearFieldDisplay(fieldPath);
}

void LineChartWidget::clearAllData() {
    for (auto& pair : m_seriesData) {
        pair.second->clearData();
        pair.second->needsUpdate = true;
    }
    m_packetSequence = 0;
    updateSeriesData();
}

// Analysis functions
QPair<double, double> LineChartWidget::getYRange() const {
    return calculateDataBounds(Qt::Vertical);
}

QPair<double, double> LineChartWidget::getXRange() const {
    return calculateDataBounds(Qt::Horizontal);
}

double LineChartWidget::getSeriesMean(const QString& fieldPath) const {
    auto it = m_seriesData.find(fieldPath);
    if (it == m_seriesData.end() || it->second->points.empty()) {
        return 0.0;
    }
    
    std::vector<double> values;
    values.reserve(it->second->points.size());
    for (const auto& point : it->second->points) {
        values.push_back(point.y());
    }
    
    return Monitor::Charts::DataConverter::calculateMean(values);
}

double LineChartWidget::getSeriesStdDev(const QString& fieldPath) const {
    auto it = m_seriesData.find(fieldPath);
    if (it == m_seriesData.end() || it->second->points.empty()) {
        return 0.0;
    }
    
    std::vector<double> values;
    values.reserve(it->second->points.size());
    for (const auto& point : it->second->points) {
        values.push_back(point.y());
    }
    
    return Monitor::Charts::DataConverter::calculateStdDev(values);
}

// Settings implementation
QJsonObject LineChartWidget::saveWidgetSpecificSettings() const {
    QJsonObject settings = ChartWidget::saveWidgetSpecificSettings();
    
    // Line chart configuration
    QJsonObject lineConfig;
    lineConfig["maxDataPoints"] = m_lineConfig.maxDataPoints;
    lineConfig["rollingData"] = m_lineConfig.rollingData;
    lineConfig["historyDepth"] = m_lineConfig.historyDepth;
    lineConfig["xAxisType"] = static_cast<int>(m_lineConfig.xAxisType);
    lineConfig["xAxisFieldPath"] = m_lineConfig.xAxisFieldPath;
    lineConfig["defaultLineStyle"] = static_cast<int>(m_lineConfig.defaultLineStyle);
    lineConfig["defaultPointStyle"] = static_cast<int>(m_lineConfig.defaultPointStyle);
    lineConfig["interpolation"] = static_cast<int>(m_lineConfig.interpolation);
    lineConfig["defaultLineWidth"] = m_lineConfig.defaultLineWidth;
    lineConfig["defaultPointSize"] = m_lineConfig.defaultPointSize;
    lineConfig["showPoints"] = m_lineConfig.showPoints;
    lineConfig["connectPoints"] = m_lineConfig.connectPoints;
    lineConfig["autoScaleX"] = m_lineConfig.autoScaleX;
    lineConfig["autoScaleY"] = m_lineConfig.autoScaleY;
    lineConfig["yAxisMarginPercent"] = m_lineConfig.yAxisMarginPercent;
    lineConfig["xAxisMarginPercent"] = m_lineConfig.xAxisMarginPercent;
    lineConfig["enableRealTimeMode"] = m_lineConfig.enableRealTimeMode;
    lineConfig["enableCrosshair"] = m_lineConfig.enableCrosshair;
    lineConfig["enableValueLabels"] = m_lineConfig.enableValueLabels;
    
    settings["lineConfig"] = lineConfig;
    
    // Line series configurations
    QJsonArray lineSeriesArray;
    for (const auto& pair : m_lineSeriesConfigs) {
        QJsonObject seriesObj;
        seriesObj["fieldPath"] = pair.first;
        seriesObj["config"] = pair.second.toJson();
        lineSeriesArray.append(seriesObj);
    }
    settings["lineSeriesConfigs"] = lineSeriesArray;
    
    return settings;
}

bool LineChartWidget::restoreWidgetSpecificSettings(const QJsonObject& settings) {
    // Restore base chart settings
    if (!ChartWidget::restoreWidgetSpecificSettings(settings)) {
        return false;
    }
    
    // Restore line chart configuration
    if (settings.contains("lineConfig")) {
        QJsonObject lineConfig = settings["lineConfig"].toObject();
        
        m_lineConfig.maxDataPoints = lineConfig.value("maxDataPoints").toInt(10000);
        m_lineConfig.rollingData = lineConfig.value("rollingData").toBool(true);
        m_lineConfig.historyDepth = lineConfig.value("historyDepth").toInt(1000);
        m_lineConfig.xAxisType = static_cast<LineChartConfig::XAxisType>(
            lineConfig.value("xAxisType").toInt());
        m_lineConfig.xAxisFieldPath = lineConfig.value("xAxisFieldPath").toString();
        m_lineConfig.defaultLineStyle = static_cast<LineChartConfig::LineStyle>(
            lineConfig.value("defaultLineStyle").toInt());
        m_lineConfig.defaultPointStyle = static_cast<LineChartConfig::PointStyle>(
            lineConfig.value("defaultPointStyle").toInt());
        m_lineConfig.interpolation = static_cast<LineChartConfig::InterpolationMethod>(
            lineConfig.value("interpolation").toInt());
        m_lineConfig.defaultLineWidth = lineConfig.value("defaultLineWidth").toInt(2);
        m_lineConfig.defaultPointSize = lineConfig.value("defaultPointSize").toDouble(6.0);
        m_lineConfig.showPoints = lineConfig.value("showPoints").toBool(false);
        m_lineConfig.connectPoints = lineConfig.value("connectPoints").toBool(true);
        m_lineConfig.autoScaleX = lineConfig.value("autoScaleX").toBool(true);
        m_lineConfig.autoScaleY = lineConfig.value("autoScaleY").toBool(true);
        m_lineConfig.yAxisMarginPercent = lineConfig.value("yAxisMarginPercent").toDouble(5.0);
        m_lineConfig.xAxisMarginPercent = lineConfig.value("xAxisMarginPercent").toDouble(2.0);
        m_lineConfig.enableRealTimeMode = lineConfig.value("enableRealTimeMode").toBool(true);
        m_lineConfig.enableCrosshair = lineConfig.value("enableCrosshair").toBool(false);
        m_lineConfig.enableValueLabels = lineConfig.value("enableValueLabels").toBool(false);
    }
    
    // Restore line series configurations
    if (settings.contains("lineSeriesConfigs")) {
        m_lineSeriesConfigs.clear();
        QJsonArray lineSeriesArray = settings["lineSeriesConfigs"].toArray();
        
        for (const QJsonValue& value : lineSeriesArray) {
            QJsonObject seriesObj = value.toObject();
            QString fieldPath = seriesObj.value("fieldPath").toString();
            LineSeriesConfig config(seriesObj.value("config").toObject());
            m_lineSeriesConfigs[fieldPath] = config;
        }
    }
    
    // Apply configuration
    setLineChartConfig(m_lineConfig);
    
    return true;
}

void LineChartWidget::setupContextMenu() {
    ChartWidget::setupContextMenu();
    
    // Add line chart specific context menu items
    if (QMenu* menu = getContextMenu()) {
        menu->addSeparator();
        
        QAction* realTimeAction = menu->addAction("Real-time Mode");
        realTimeAction->setCheckable(true);
        realTimeAction->setChecked(m_lineConfig.enableRealTimeMode);
        connect(realTimeAction, &QAction::triggered, this, &LineChartWidget::onToggleRealTimeMode);
        
        QAction* crosshairAction = menu->addAction("Show Crosshair");
        crosshairAction->setCheckable(true);
        crosshairAction->setChecked(m_lineConfig.enableCrosshair);
        connect(crosshairAction, &QAction::triggered, this, &LineChartWidget::onToggleCrosshair);
        
        menu->addSeparator();
        menu->addAction("Clear All Data", this, &LineChartWidget::onClearData);
        menu->addAction("Scroll to Latest", this, &LineChartWidget::onScrollToLatest);
    }
}

// Slots implementation
void LineChartWidget::onToggleRealTimeMode(bool enabled) {
    setRealTimeMode(enabled);
}

void LineChartWidget::onClearData() {
    clearAllData();
}

void LineChartWidget::onScrollToLatest() {
    scrollToLatest();
}

void LineChartWidget::onToggleCrosshair(bool enabled) {
    m_lineConfig.enableCrosshair = enabled;
    // TODO: Implement crosshair display logic
}

void LineChartWidget::onChangeInterpolationMethod(int method) {
    m_lineConfig.interpolation = static_cast<LineChartConfig::InterpolationMethod>(method);
    
    // Update all series configurations
    for (auto& pair : m_lineSeriesConfigs) {
        pair.second.interpolation = m_lineConfig.interpolation;
    }
    
    // Recreate series with new interpolation method
    // This is a simplified approach - in practice, we might want to be more efficient
    std::vector<QString> fieldsToRecreate;
    for (const auto& pair : m_seriesData) {
        fieldsToRecreate.push_back(pair.first);
    }
    
    // Store data temporarily
    std::unordered_map<QString, std::vector<QPointF>> tempData;
    for (const QString& fieldPath : fieldsToRecreate) {
        tempData[fieldPath] = getSeriesData(fieldPath);
        removeSeries(fieldPath);
    }
    
    // Recreate series with new interpolation
    for (const QString& fieldPath : fieldsToRecreate) {
        SeriesConfig config;
        config.fieldPath = fieldPath;
        config.seriesName = fieldPath;
        config.color = Monitor::Charts::ColorPalette::getColor(0);
        
        addSeries(fieldPath, config);
        
        // Restore data
        auto it = m_seriesData.find(fieldPath);
        if (it != m_seriesData.end()) {
            for (const QPointF& point : tempData[fieldPath]) {
                it->second->addPoint(point, QVariant(point.y()));
            }
            it->second->needsUpdate = true;
        }
    }
    
    updateSeriesData();
}

void LineChartWidget::onSeriesHovered(const QPointF& point, bool state) {
    if (state && m_chartConfig.enableTooltips) {
        // Find which series was hovered
        QAbstractSeries* series = qobject_cast<QAbstractSeries*>(sender());
        if (series) {
            QString fieldPath;
            for (const auto& pair : m_seriesData) {
                if (pair.second->lineSeries == series || 
                    pair.second->splineSeries == series ||
                    pair.second->pointSeries == series) {
                    fieldPath = pair.first;
                    break;
                }
            }
            
            if (!fieldPath.isEmpty()) {
                showPointTooltip(point, fieldPath, point);
            }
        }
    } else {
        hideTooltip();
    }
}

void LineChartWidget::onRealTimeUpdate() {
    // This slot is called by the real-time timer
    // Update display if we have pending updates
    bool hasUpdates = false;
    for (const auto& pair : m_seriesData) {
        if (pair.second->needsUpdate) {
            hasUpdates = true;
            break;
        }
    }
    
    if (hasUpdates) {
        updateSeriesData();
    }
}

void LineChartWidget::onSeriesClicked(const QPointF& point) {
    emit chartClicked(point);
}

// Helper method implementations
void LineChartWidget::updateRealTimeSettings() {
    if (m_lineConfig.enableRealTimeMode) {
        if (!m_realTimeTimer->isActive()) {
            m_realTimeTimer->start();
        }
    } else {
        m_realTimeTimer->stop();
    }
}

void LineChartWidget::updateAxes() {
    if (!m_chart) return;
    
    // This is a simplified axis update - full implementation would handle
    // different axis types and configurations
    if (m_chart->axes().isEmpty()) {
        createDefaultAxes();
    }
    
    // Apply axis titles and formatting based on configuration
    QList<QAbstractAxis*> xAxes = m_chart->axes(Qt::Horizontal);
    QList<QAbstractAxis*> yAxes = m_chart->axes(Qt::Vertical);
    
    if (!xAxes.isEmpty()) {
        QString title = "X";
        switch (m_lineConfig.xAxisType) {
            case LineChartConfig::XAxisType::PacketSequence:
                title = "Packet Sequence";
                break;
            case LineChartConfig::XAxisType::Timestamp:
                title = "Time";
                break;
            case LineChartConfig::XAxisType::FieldValue:
                title = m_lineConfig.xAxisFieldPath;
                break;
        }
        xAxes.first()->setTitleText(title);
    }
    
    if (!yAxes.isEmpty()) {
        yAxes.first()->setTitleText("Value");
    }
}

void LineChartWidget::updateSeriesAppearance(const QString& fieldPath) {
    auto it = m_seriesData.find(fieldPath);
    if (it == m_seriesData.end()) {
        return;
    }
    
    SeriesData* data = it->second.get();
    
    // Update line series appearance
    if (data->lineSeries) {
        applyLineSeriesConfig(data->lineSeries, data->config);
    }
    
    // Update spline series appearance
    if (data->splineSeries) {
        applySplineSeriesConfig(data->splineSeries, data->config);
    }
    
    // Update point series appearance
    if (data->pointSeries) {
        applyScatterSeriesConfig(data->pointSeries, data->config);
        data->pointSeries->setVisible(data->config.showPoints);
    }
}

void LineChartWidget::applyLineSeriesConfig(QLineSeries* series, const LineSeriesConfig& config) {
    if (!series) return;
    
    QPen pen = series->pen();
    pen.setWidth(config.lineWidth);
    pen.setStyle(lineStyleToPenStyle(config.lineStyle));
    series->setPen(pen);
    
    // Handle fill area if specified
    if (config.fillColor.alpha() > 0) {
        QColor fillColor = config.fillColor;
        fillColor.setAlphaF(config.fillOpacity);
        // Note: QLineSeries doesn't directly support fill areas
        // This would require QAreaSeries for proper implementation
    }
}

void LineChartWidget::applyScatterSeriesConfig(QScatterSeries* series, const LineSeriesConfig& config) {
    if (!series) return;
    
    series->setMarkerSize(config.pointSize);
    configureSeriesMarkers(series, config.pointStyle);
}

void LineChartWidget::applySplineSeriesConfig(QSplineSeries* series, const LineSeriesConfig& config) {
    if (!series) return;
    
    QPen pen = series->pen();
    pen.setWidth(config.lineWidth);
    pen.setStyle(lineStyleToPenStyle(config.lineStyle));
    series->setPen(pen);
}

double LineChartWidget::calculateXValue(const QVariant& fieldValue, const QString& fieldPath) {
    Q_UNUSED(fieldPath);
    Q_UNUSED(fieldValue);
    
    switch (m_lineConfig.xAxisType) {
        case LineChartConfig::XAxisType::PacketSequence:
            return ++m_packetSequence;
            
        case LineChartConfig::XAxisType::Timestamp: {
            auto now = std::chrono::steady_clock::now();
            auto duration = now.time_since_epoch();
            auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
            return static_cast<double>(millis.count());
        }
        
        case LineChartConfig::XAxisType::FieldValue: {
            // Use the field value itself as X coordinate
            bool ok;
            double xValue = Monitor::Charts::DataConverter::toDouble(fieldValue, &ok);
            if (ok) {
                return xValue;
            } else {
                // Fall back to sequence if field value is not numeric
                return ++m_packetSequence;
            }
        }
    }
    
    return ++m_packetSequence;
}

QPointF LineChartWidget::createDataPoint(const QString& fieldPath, const QVariant& fieldValue) {
    double xValue = calculateXValue(fieldValue, fieldPath);
    
    bool ok;
    double yValue = Monitor::Charts::DataConverter::toDouble(fieldValue, &ok);
    if (!ok) {
        yValue = 0.0;
    }
    
    return QPointF(xValue, yValue);
}

std::vector<QPointF> LineChartWidget::smoothData(const std::vector<QPointF>& data, int windowSize) const {
    if (data.size() < static_cast<size_t>(windowSize) || windowSize <= 1) {
        return data;
    }
    
    std::vector<QPointF> smoothed;
    smoothed.reserve(data.size());
    
    int halfWindow = windowSize / 2;
    
    for (size_t i = 0; i < data.size(); ++i) {
        double sumX = 0.0, sumY = 0.0;
        int count = 0;
        
        int start = std::max(0, static_cast<int>(i) - halfWindow);
        int end = std::min(static_cast<int>(data.size()), static_cast<int>(i) + halfWindow + 1);
        
        for (int j = start; j < end; ++j) {
            sumX += data[j].x();
            sumY += data[j].y();
            count++;
        }
        
        if (count > 0) {
            smoothed.emplace_back(sumX / count, sumY / count);
        }
    }
    
    return smoothed;
}

void LineChartWidget::autoScaleAxes() {
    if (!m_chart) return;
    
    QList<QAbstractAxis*> xAxes = m_chart->axes(Qt::Horizontal);
    QList<QAbstractAxis*> yAxes = m_chart->axes(Qt::Vertical);
    
    if (m_lineConfig.autoScaleY && !yAxes.isEmpty()) {
        auto yRange = calculateDataBounds(Qt::Vertical);
        if (yRange.first != yRange.second) {
            double margin = (yRange.second - yRange.first) * (m_lineConfig.yAxisMarginPercent / 100.0);
            
            if (QValueAxis* axis = qobject_cast<QValueAxis*>(yAxes.first())) {
                axis->setRange(yRange.first - margin, yRange.second + margin);
            }
        }
    }
    
    if (m_lineConfig.autoScaleX && !xAxes.isEmpty()) {
        auto xRange = calculateDataBounds(Qt::Horizontal);
        if (xRange.first != xRange.second) {
            double margin = (xRange.second - xRange.first) * (m_lineConfig.xAxisMarginPercent / 100.0);
            
            if (QValueAxis* axis = qobject_cast<QValueAxis*>(xAxes.first())) {
                axis->setRange(xRange.first - margin, xRange.second + margin);
            }
        }
    }
}

QPair<double, double> LineChartWidget::calculateDataBounds(Qt::Orientation orientation) const {
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::lowest();
    bool hasData = false;
    
    for (const auto& pair : m_seriesData) {
        for (const auto& point : pair.second->points) {
            double value = (orientation == Qt::Vertical) ? point.y() : point.x();
            min = std::min(min, value);
            max = std::max(max, value);
            hasData = true;
        }
    }
    
    return hasData ? QPair<double, double>(min, max) : QPair<double, double>(0.0, 0.0);
}

void LineChartWidget::scrollToShowLatestData() {
    if (!m_chart || !m_lineConfig.enableRealTimeMode) return;
    
    // Find the latest X value across all series
    double latestX = std::numeric_limits<double>::lowest();
    bool hasData = false;
    
    for (const auto& pair : m_seriesData) {
        if (!pair.second->points.empty()) {
            latestX = std::max(latestX, pair.second->points.back().x());
            hasData = true;
        }
    }
    
    if (!hasData) return;
    
    // Scroll to show latest data
    QList<QAbstractAxis*> xAxes = m_chart->axes(Qt::Horizontal);
    if (!xAxes.isEmpty()) {
        if (QValueAxis* axis = qobject_cast<QValueAxis*>(xAxes.first())) {
            double range = axis->max() - axis->min();
            axis->setRange(latestX - range * 0.8, latestX + range * 0.2);
        }
    }
}

void LineChartWidget::showPointTooltip(const QPointF& chartPos, const QString& fieldPath, const QPointF& dataPos) {
    QString tooltip = QString("%1\nX: %2\nY: %3")
        .arg(fieldPath)
        .arg(dataPos.x(), 0, 'f', 2)
        .arg(dataPos.y(), 0, 'f', 2);
    
    showTooltip(chartPos, tooltip);
}

bool LineChartWidget::shouldDecimateData(const QString& fieldPath) const {
    auto it = m_seriesData.find(fieldPath);
    if (it == m_seriesData.end()) {
        return false;
    }
    
    // Decimate if we have too many points and performance optimization is enabled
    return isPerformanceOptimized() && 
           static_cast<int>(it->second->points.size()) > m_lineConfig.maxDataPoints;
}

Qt::PenStyle LineChartWidget::lineStyleToPenStyle(LineChartConfig::LineStyle style) const {
    switch (style) {
        case LineChartConfig::LineStyle::Solid: return Qt::SolidLine;
        case LineChartConfig::LineStyle::Dash: return Qt::DashLine;
        case LineChartConfig::LineStyle::Dot: return Qt::DotLine;
        case LineChartConfig::LineStyle::DashDot: return Qt::DashDotLine;
        case LineChartConfig::LineStyle::DashDotDot: return Qt::DashDotDotLine;
    }
    return Qt::SolidLine;
}

void LineChartWidget::configureSeriesMarkers(QLineSeries* series, LineChartConfig::PointStyle style) const {
    // Configure line series point markers based on style
    // Note: Qt Charts may not support all marker shapes in all versions
    switch (style) {
        case LineChartConfig::PointStyle::Circle:
        case LineChartConfig::PointStyle::Square:
            series->setPointsVisible(true);
            break;
        default:
            series->setPointsVisible(false);
            break;
    }
}

void LineChartWidget::configureSeriesMarkers(QScatterSeries* series, LineChartConfig::PointStyle style) const {
    // Configure scatter series point markers based on style
    if (!series) return;
    
    switch (style) {
        case LineChartConfig::PointStyle::Circle:
            // QScatterSeries shows points by default
            break;
        case LineChartConfig::PointStyle::Square:
            // Note: Qt Charts may not support custom marker shapes
            break;
        default:
            break;
    }
}

// Event handling implementations
void LineChartWidget::wheelEvent(QWheelEvent* event) {
    if (!m_chartView) {
        ChartWidget::wheelEvent(event);
        return;
    }
    
    // Handle zoom with mouse wheel
    const double scaleFactor = 1.15;
    if (event->angleDelta().y() > 0) {
        m_chartView->chart()->zoom(scaleFactor);
    } else {
        m_chartView->chart()->zoom(1.0 / scaleFactor);
    }
    
    event->accept();
}

bool LineChartWidget::eventFilter(QObject* watched, QEvent* event) {
    // Handle chart view events for tooltips and interactions
    if (watched == m_chartView && event->type() == QEvent::MouseMove) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        handleMouseMove(mouseEvent->pos());
        return false; // Don't consume the event
    }
    
    return ChartWidget::eventFilter(watched, event);
}

void LineChartWidget::mouseMoveEvent(QMouseEvent* event) {
    handleMouseMove(event->pos());
    ChartWidget::mouseMoveEvent(event);
}

void LineChartWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        handleMouseClick(event->pos());
    }
    ChartWidget::mousePressEvent(event);
}

void LineChartWidget::handleMouseMove(const QPoint& position) {
    // Handle mouse move for tooltips and crosshair
    if (!m_chartView || !m_chartView->chart()) return;
    
    // Convert widget position to chart coordinates
    QPointF chartPos = m_chartView->chart()->mapToValue(position);
    Q_UNUSED(chartPos);
    
    // Show tooltip if configured
    // TODO: Add showTooltips to LineChartConfig if needed
    // Find nearest data point and show tooltip
    // Implementation would find closest series point
    
    // Update crosshair if enabled  
    // TODO: Add showCrosshair to LineChartConfig if needed
    // emit crosshairPositionChanged(chartPos);
}

void LineChartWidget::handleMouseClick(const QPoint& position) {
    // Handle mouse click events
    Q_UNUSED(position);
    
    // Could be used for:
    // - Series selection
    // - Data point highlighting
    // - Context menu triggering
}