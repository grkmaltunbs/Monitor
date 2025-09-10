#include "chart_widget.h"
#include <QtCharts/QValueAxis>
#include <QtCharts/QLogValueAxis>
#include <QtCharts/QCategoryAxis>
#include <QApplication>
#include <QStyle>
#include <QToolTip>
#include <QGraphicsProxyWidget>
#include <QGraphicsTextItem>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <chrono>


ChartWidget::ChartWidget(const QString& widgetId, const QString& windowTitle, QWidget* parent)
    : DisplayWidget(widgetId, windowTitle, parent)
    , m_chart(nullptr)
    , m_chartView(nullptr)
    , m_mainLayout(nullptr)
    , m_toolbar(nullptr)
    , m_autoScale(true)
    , m_performanceOptimized(false)
    , m_updateTimer(new QTimer(this))
    , m_fpsTimer(new QTimer(this))
    , m_frameCount(0)
    , m_lastFPSUpdate(std::chrono::steady_clock::now())
    , m_currentFPS(0.0)
    , m_currentPointCount(0)
    , m_currentInteractionMode(InteractionMode::PanZoom)
    , m_rubberBand(nullptr)
    , m_isRubberBandActive(false)
    , m_nextColorIndex(0)
{
    // Initialize chart configuration with defaults
    m_chartConfig = ChartConfig();
    
    // Setup update timer
    m_updateTimer->setSingleShot(false);
    m_updateTimer->setInterval(16); // ~60 FPS
    connect(m_updateTimer, &QTimer::timeout, this, &ChartWidget::onUpdateTimerTimeout);
    
    // Setup performance monitoring
    initializePerformanceTracking();
}

ChartWidget::~ChartWidget() {
    // Clean up chart components
    if (m_chart) {
        m_chart->removeAllSeries();
    }
    
    // Clean up rubber band
    if (m_rubberBand) {
        delete m_rubberBand;
    }
}

void ChartWidget::initializeWidget() {
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(2, 2, 2, 2);
    m_mainLayout->setSpacing(2);
    
    // Setup toolbar
    setupToolbar();
    
    // Create chart (implemented by derived classes)
    createChart();
    
    // Setup chart view
    setupChartView();
    
    // Setup interaction
    setupInteraction();
    
    // Apply initial configuration
    applyChartConfig();
    
    // Start performance monitoring
    m_fpsTimer->start(1000); // Update FPS every second
    
    // Start update timer if needed
    if (m_chartConfig.updateMode != UpdateMode::Immediate) {
        m_updateTimer->start();
    }
}

void ChartWidget::setupChartView() {
    if (!m_chart) {
        qWarning() << "ChartWidget::setupChartView: Chart not created";
        return;
    }
    
    // Create chart view
    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing, true);
    m_chartView->setRubberBand(QChartView::RectangleRubberBand);
    m_chartView->setInteractive(true);
    
    // Apply performance settings
    updatePerformanceSettings();
    
    // Add to layout
    m_mainLayout->addWidget(m_chartView);
    
    // Connect chart view signals
    connect(m_chartView, &QChartView::rubberBandChanged, 
            this, &ChartWidget::onChartInteraction);
}

void ChartWidget::setupToolbar() {
    m_toolbar = new QToolBar("Chart Tools", this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setIconSize(QSize(16, 16));
    
    // Reset zoom action
    m_resetZoomAction = m_toolbar->addAction(
        QApplication::style()->standardIcon(QStyle::SP_BrowserReload),
        "Reset Zoom");
    connect(m_resetZoomAction, &QAction::triggered, this, &ChartWidget::onResetZoom);
    
    // Toggle legend action
    m_toggleLegendAction = m_toolbar->addAction("Legend");
    m_toggleLegendAction->setCheckable(true);
    m_toggleLegendAction->setChecked(m_chartConfig.showLegend);
    connect(m_toggleLegendAction, &QAction::triggered, this, &ChartWidget::onToggleLegend);
    
    // Toggle grid action
    m_toggleGridAction = m_toolbar->addAction("Grid");
    m_toggleGridAction->setCheckable(true);
    m_toggleGridAction->setChecked(m_chartConfig.showGrid);
    connect(m_toggleGridAction, &QAction::triggered, this, &ChartWidget::onToggleGrid);
    
    m_toolbar->addSeparator();
    
    // Theme selector
    m_toolbar->addWidget(new QLabel("Theme:"));
    m_themeCombo = new QComboBox();
    populateThemeCombo();
    m_themeCombo->setCurrentIndex(static_cast<int>(m_chartConfig.theme));
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChartWidget::onChangeTheme);
    m_toolbar->addWidget(m_themeCombo);
    
    m_toolbar->addSeparator();
    
    // Export action
    m_exportAction = m_toolbar->addAction(
        QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton),
        "Export");
    connect(m_exportAction, &QAction::triggered, this, &ChartWidget::onExportChart);
    
    // Settings action
    m_settingsAction = m_toolbar->addAction(
        QApplication::style()->standardIcon(QStyle::SP_ComputerIcon),
        "Settings");
    connect(m_settingsAction, &QAction::triggered, this, &ChartWidget::onShowChartSettings);
    
    // Add toolbar to layout
    m_mainLayout->addWidget(m_toolbar);
}

void ChartWidget::setupInteraction() {
    if (!m_chartView) return;
    
    // Set interaction mode
    enableZoomPan(m_chartConfig.interactionMode == InteractionMode::PanZoom ||
                  m_chartConfig.interactionMode == InteractionMode::Zoom ||
                  m_chartConfig.interactionMode == InteractionMode::Pan);
    
    // Install event filter for custom interactions
    m_chartView->installEventFilter(this);
}

void ChartWidget::setChartConfig(const ChartConfig& config) {
    m_chartConfig = config;
    applyChartConfig();
}

void ChartWidget::resetChartConfig() {
    m_chartConfig = ChartConfig();
    applyChartConfig();
}

void ChartWidget::applyChartConfig() {
    if (!m_chart) return;
    
    // Apply title
    m_chart->setTitle(m_chartConfig.title);
    
    // Apply legend settings
    m_chart->legend()->setVisible(m_chartConfig.showLegend);
    m_chart->legend()->setAlignment(m_chartConfig.legendAlignment);
    
    // Apply theme
    applyTheme();
    
    // Apply performance settings
    updatePerformanceSettings();
    
    // Update toolbar state
    if (m_toggleLegendAction) {
        m_toggleLegendAction->setChecked(m_chartConfig.showLegend);
    }
    if (m_toggleGridAction) {
        m_toggleGridAction->setChecked(m_chartConfig.showGrid);
    }
    if (m_themeCombo) {
        m_themeCombo->setCurrentIndex(static_cast<int>(m_chartConfig.theme));
    }
    
    // Apply update timer settings
    if (m_chartConfig.updateMode != UpdateMode::Immediate) {
        int interval = 16; // Default 60 FPS
        if (m_chartConfig.performanceLevel == Monitor::Charts::PerformanceLevel::Fast) {
            interval = 33; // 30 FPS
        }
        m_updateTimer->setInterval(interval);
        if (!m_updateTimer->isActive()) {
            m_updateTimer->start();
        }
    } else {
        m_updateTimer->stop();
    }
}

void ChartWidget::applyTheme() {
    if (!m_chart) return;
    
    Monitor::Charts::ChartThemeConfig themeConfig = getCurrentThemeConfig();
    themeConfig.applyToChart(m_chart);
    
    // Apply grid settings to axes
    foreach (QAbstractAxis* axis, m_chart->axes()) {
        if (QValueAxis* valueAxis = qobject_cast<QValueAxis*>(axis)) {
            valueAxis->setGridLineVisible(m_chartConfig.showGrid);
            valueAxis->setGridLineColor(themeConfig.gridLineColor);
            valueAxis->setLabelsColor(themeConfig.axisLabelColor);
            valueAxis->setTitleBrush(QBrush(themeConfig.axisLabelColor));
        }
    }
}

bool ChartWidget::addSeries(const QString& fieldPath, const SeriesConfig& config) {
    if (fieldPath.isEmpty() || m_seriesConfigs.find(fieldPath) != m_seriesConfigs.end()) {
        return false;
    }
    
    // Create series through derived class implementation
    QAbstractSeries* series = createSeriesForField(fieldPath, config);
    if (!series) {
        return false;
    }
    
    // Store configuration
    m_seriesConfigs[fieldPath] = config;
    m_seriesMap[fieldPath] = series;
    
    // Add to chart
    m_chart->addSeries(series);
    
    // Configure series
    configureSeries(fieldPath, config);
    
    // Create default axes if needed
    if (m_chart->axes().isEmpty()) {
        createDefaultAxes();
    }
    
    // Attach axes
    if (!m_chart->axes(Qt::Horizontal).isEmpty() && !m_chart->axes(Qt::Vertical).isEmpty()) {
        series->attachAxis(m_chart->axes(Qt::Horizontal).first());
        series->attachAxis(m_chart->axes(Qt::Vertical).first());
    }
    
    // Update color index for next series
    m_nextColorIndex++;
    
    // Add field to base widget
    return addField(fieldPath, 0, QJsonObject());
}

bool ChartWidget::removeSeries(const QString& fieldPath) {
    auto configIt = m_seriesConfigs.find(fieldPath);
    auto seriesIt = m_seriesMap.find(fieldPath);
    
    if (configIt == m_seriesConfigs.end() || seriesIt == m_seriesMap.end()) {
        return false;
    }
    
    // Remove from chart
    if (m_chart) {
        m_chart->removeSeries(seriesIt->second);
    }
    
    // Clean up series through derived class
    removeSeriesForField(fieldPath);
    
    // Remove from maps
    m_seriesConfigs.erase(configIt);
    m_seriesMap.erase(seriesIt);
    
    // Remove field from base widget
    return removeField(fieldPath);
}

void ChartWidget::clearSeries() {
    // Remove all series
    if (m_chart) {
        m_chart->removeAllSeries();
    }
    
    // Clear maps
    m_seriesConfigs.clear();
    m_seriesMap.clear();
    m_nextColorIndex = 0;
    
    // Clear fields in base widget
    clearFields();
}

QStringList ChartWidget::getSeriesList() const {
    QStringList result;
    for (const auto& pair : m_seriesConfigs) {
        result.append(pair.first);
    }
    return result;
}

ChartWidget::SeriesConfig ChartWidget::getSeriesConfig(const QString& fieldPath) const {
    auto it = m_seriesConfigs.find(fieldPath);
    return (it != m_seriesConfigs.end()) ? it->second : SeriesConfig();
}

void ChartWidget::setSeriesConfig(const QString& fieldPath, const SeriesConfig& config) {
    auto it = m_seriesConfigs.find(fieldPath);
    if (it == m_seriesConfigs.end()) {
        return;
    }
    
    it->second = config;
    configureSeries(fieldPath, config);
}

void ChartWidget::setAutoScale(bool enabled) {
    m_autoScale = enabled;
    if (enabled) {
        updateAxisRange();
    }
}

void ChartWidget::resetZoom() {
    if (!m_chartView) return;
    
    m_chartView->chart()->zoomReset();
    emit zoomChanged(m_chart->plotArea());
}

void ChartWidget::setAxisRange(Qt::Orientation orientation, double min, double max) {
    if (!m_chart) return;
    
    QList<QAbstractAxis*> axes = m_chart->axes(orientation);
    if (axes.isEmpty()) return;
    
    if (QValueAxis* valueAxis = qobject_cast<QValueAxis*>(axes.first())) {
        valueAxis->setRange(min, max);
    }
}

QPair<double, double> ChartWidget::getAxisRange(Qt::Orientation orientation) const {
    if (!m_chart) return {0.0, 0.0};
    
    QList<QAbstractAxis*> axes = m_chart->axes(orientation);
    if (axes.isEmpty()) return {0.0, 0.0};
    
    if (QValueAxis* valueAxis = qobject_cast<QValueAxis*>(axes.first())) {
        return {valueAxis->min(), valueAxis->max()};
    }
    
    return {0.0, 0.0};
}

bool ChartWidget::exportChart(const QString& filePath) {
    QString path = filePath;
    if (path.isEmpty()) {
        path = getDefaultExportPath();
        path = QFileDialog::getSaveFileName(this, "Export Chart", path,
            Monitor::Charts::ChartExporter::getFileFilter());
        if (path.isEmpty()) {
            return false;
        }
    }
    
    Monitor::Charts::ExportFormat format = getFormatFromExtension(path);
    return Monitor::Charts::ChartExporter::exportChart(m_chart, path, format, m_chartConfig.exportSize);
}

bool ChartWidget::exportChart(const QString& filePath, Monitor::Charts::ExportFormat format, const QSize& size) {
    if (filePath.isEmpty() || !m_chart) return false;
    
    QSize exportSize = size.isValid() ? size : m_chartConfig.exportSize;
    return Monitor::Charts::ChartExporter::exportChart(m_chart, filePath, format, exportSize);
}

double ChartWidget::getCurrentFPS() const {
    return m_currentFPS;
}

int ChartWidget::getCurrentPointCount() const {
    return m_currentPointCount;
}

// DisplayWidget implementation
void ChartWidget::updateDisplay() {
    if (m_chartConfig.updateMode == UpdateMode::Immediate) {
        updateSeriesData();
        updateFPSCounter();
        checkPerformanceThresholds();
    }
    // For other modes, updates are handled by timer
}

void ChartWidget::handleFieldAdded(const FieldAssignment& field) {
    // Check if we need to create a series for this field
    if (m_seriesConfigs.find(field.fieldPath) == m_seriesConfigs.end()) {
        SeriesConfig config;
        config.fieldPath = field.fieldPath;
        config.seriesName = field.displayName;
        config.color = Monitor::Charts::ColorPalette::getColor(m_nextColorIndex);
        
        addSeries(field.fieldPath, config);
    }
}

void ChartWidget::handleFieldRemoved(const QString& fieldPath) {
    removeSeries(fieldPath);
}

void ChartWidget::handleFieldsCleared() {
    clearSeries();
}

// Settings implementation
QJsonObject ChartWidget::saveWidgetSpecificSettings() const {
    QJsonObject settings;
    
    // Chart configuration
    QJsonObject chartConfig;
    chartConfig["theme"] = static_cast<int>(m_chartConfig.theme);
    chartConfig["title"] = m_chartConfig.title;
    chartConfig["showLegend"] = m_chartConfig.showLegend;
    chartConfig["legendAlignment"] = static_cast<int>(m_chartConfig.legendAlignment);
    chartConfig["showGrid"] = m_chartConfig.showGrid;
    chartConfig["showAxes"] = m_chartConfig.showAxes;
    chartConfig["performanceLevel"] = static_cast<int>(m_chartConfig.performanceLevel);
    chartConfig["maxDataPoints"] = m_chartConfig.maxDataPoints;
    chartConfig["enableAnimations"] = m_chartConfig.enableAnimations;
    chartConfig["updateMode"] = static_cast<int>(m_chartConfig.updateMode);
    chartConfig["interactionMode"] = static_cast<int>(m_chartConfig.interactionMode);
    chartConfig["enableTooltips"] = m_chartConfig.enableTooltips;
    chartConfig["enableCrosshair"] = m_chartConfig.enableCrosshair;
    
    QJsonObject exportSize;
    exportSize["width"] = m_chartConfig.exportSize.width();
    exportSize["height"] = m_chartConfig.exportSize.height();
    chartConfig["exportSize"] = exportSize;
    chartConfig["defaultExportFormat"] = static_cast<int>(m_chartConfig.defaultExportFormat);
    
    settings["chartConfig"] = chartConfig;
    
    // Series configurations
    QJsonArray seriesArray;
    for (const auto& pair : m_seriesConfigs) {
        QJsonObject seriesObj;
        seriesObj["fieldPath"] = pair.first;
        seriesObj["seriesName"] = pair.second.seriesName;
        seriesObj["color"] = pair.second.color.name();
        seriesObj["visible"] = pair.second.visible;
        seriesObj["axisIndex"] = pair.second.axisIndex;
        seriesObj["chartSpecific"] = pair.second.chartSpecific;
        seriesArray.append(seriesObj);
    }
    settings["series"] = seriesArray;
    
    // State
    settings["autoScale"] = m_autoScale;
    
    return settings;
}

bool ChartWidget::restoreWidgetSpecificSettings(const QJsonObject& settings) {
    // Restore chart configuration
    if (settings.contains("chartConfig")) {
        QJsonObject chartConfig = settings["chartConfig"].toObject();
        
        m_chartConfig.theme = static_cast<Monitor::Charts::ChartTheme>(
            chartConfig.value("theme").toInt());
        m_chartConfig.title = chartConfig.value("title").toString();
        m_chartConfig.showLegend = chartConfig.value("showLegend").toBool(true);
        m_chartConfig.legendAlignment = static_cast<Qt::Alignment>(
            chartConfig.value("legendAlignment").toInt());
        m_chartConfig.showGrid = chartConfig.value("showGrid").toBool(true);
        m_chartConfig.showAxes = chartConfig.value("showAxes").toBool(true);
        m_chartConfig.performanceLevel = static_cast<Monitor::Charts::PerformanceLevel>(
            chartConfig.value("performanceLevel").toInt());
        m_chartConfig.maxDataPoints = chartConfig.value("maxDataPoints").toInt(10000);
        m_chartConfig.enableAnimations = chartConfig.value("enableAnimations").toBool(true);
        m_chartConfig.updateMode = static_cast<UpdateMode>(
            chartConfig.value("updateMode").toInt());
        m_chartConfig.interactionMode = static_cast<InteractionMode>(
            chartConfig.value("interactionMode").toInt());
        m_chartConfig.enableTooltips = chartConfig.value("enableTooltips").toBool(true);
        m_chartConfig.enableCrosshair = chartConfig.value("enableCrosshair").toBool(false);
        
        if (chartConfig.contains("exportSize")) {
            QJsonObject exportSize = chartConfig["exportSize"].toObject();
            m_chartConfig.exportSize = QSize(
                exportSize.value("width").toInt(1920),
                exportSize.value("height").toInt(1080));
        }
        m_chartConfig.defaultExportFormat = static_cast<Monitor::Charts::ExportFormat>(
            chartConfig.value("defaultExportFormat").toInt());
    }
    
    // Restore series configurations
    if (settings.contains("series")) {
        clearSeries();
        QJsonArray seriesArray = settings["series"].toArray();
        
        for (const QJsonValue& value : seriesArray) {
            QJsonObject seriesObj = value.toObject();
            SeriesConfig config;
            config.fieldPath = seriesObj.value("fieldPath").toString();
            config.seriesName = seriesObj.value("seriesName").toString();
            config.color = QColor(seriesObj.value("color").toString());
            config.visible = seriesObj.value("visible").toBool(true);
            config.axisIndex = seriesObj.value("axisIndex").toInt(0);
            config.chartSpecific = seriesObj.value("chartSpecific").toObject();
            
            addSeries(config.fieldPath, config);
        }
    }
    
    // Restore state
    m_autoScale = settings.value("autoScale").toBool(true);
    
    // Apply configuration
    applyChartConfig();
    
    return true;
}

void ChartWidget::setupContextMenu() {
    // Add chart-specific context menu items
    if (QMenu* menu = getContextMenu()) {
        menu->addSeparator();
        menu->addAction("Reset Zoom", this, &ChartWidget::onResetZoom);
        menu->addAction("Toggle Legend", this, &ChartWidget::onToggleLegend);
        menu->addAction("Toggle Grid", this, &ChartWidget::onToggleGrid);
        menu->addSeparator();
        menu->addAction("Export Chart...", this, &ChartWidget::onExportChart);
        menu->addAction("Chart Settings...", this, &ChartWidget::onShowChartSettings);
    }
}

// Slots implementation
void ChartWidget::onResetZoom() {
    resetZoom();
}

void ChartWidget::onToggleLegend() {
    m_chartConfig.showLegend = !m_chartConfig.showLegend;
    if (m_chart && m_chart->legend()) {
        m_chart->legend()->setVisible(m_chartConfig.showLegend);
    }
    if (m_toggleLegendAction) {
        m_toggleLegendAction->setChecked(m_chartConfig.showLegend);
    }
}

void ChartWidget::onToggleGrid() {
    m_chartConfig.showGrid = !m_chartConfig.showGrid;
    applyTheme(); // Reapply theme to update grid settings
    if (m_toggleGridAction) {
        m_toggleGridAction->setChecked(m_chartConfig.showGrid);
    }
}

void ChartWidget::onChangeTheme() {
    if (m_themeCombo) {
        m_chartConfig.theme = static_cast<Monitor::Charts::ChartTheme>(m_themeCombo->currentIndex());
        applyTheme();
    }
}

void ChartWidget::onExportChart() {
    exportChart();
}

void ChartWidget::onShowChartSettings() {
    // TODO: Implement chart settings dialog
    QMessageBox::information(this, "Chart Settings", 
        "Chart settings dialog will be implemented in future update.");
}

// Private slots
void ChartWidget::onUpdateTimerTimeout() {
    updateSeriesData();
    updateFPSCounter();
    checkPerformanceThresholds();
}

void ChartWidget::onFPSTimerTimeout() {
    // FPS counter is updated in updateFPSCounter()
}

void ChartWidget::onChartInteraction() {
    // Handle chart interaction
    if (m_chartView) {
        emit zoomChanged(m_chart->plotArea());
    }
}

// Helper methods
void ChartWidget::initializePerformanceTracking() {
    m_fpsTimer->setSingleShot(false);
    m_fpsTimer->setInterval(1000); // Update every second
    connect(m_fpsTimer, &QTimer::timeout, this, &ChartWidget::onFPSTimerTimeout);
}

void ChartWidget::createDefaultAxes() {
    if (!m_chart || !m_chart->axes().isEmpty()) return;
    
    // Create default X and Y axes
    QValueAxis* xAxis = Monitor::Charts::AxisFactory::createValueAxis("X", 0, 100);
    QValueAxis* yAxis = Monitor::Charts::AxisFactory::createValueAxis("Y", 0, 100);
    
    m_chart->addAxis(xAxis, Qt::AlignBottom);
    m_chart->addAxis(yAxis, Qt::AlignLeft);
    
    // Apply theme to new axes
    Monitor::Charts::ChartThemeConfig themeConfig = getCurrentThemeConfig();
    xAxis->setGridLineVisible(m_chartConfig.showGrid);
    xAxis->setGridLineColor(themeConfig.gridLineColor);
    xAxis->setLabelsColor(themeConfig.axisLabelColor);
    xAxis->setTitleBrush(QBrush(themeConfig.axisLabelColor));
    
    yAxis->setGridLineVisible(m_chartConfig.showGrid);
    yAxis->setGridLineColor(themeConfig.gridLineColor);
    yAxis->setLabelsColor(themeConfig.axisLabelColor);
    yAxis->setTitleBrush(QBrush(themeConfig.axisLabelColor));
}

void ChartWidget::updateAxisRange() {
    if (!m_autoScale || !m_chart) return;
    
    // Auto-scale axes based on current data
    // This is a simplified implementation - derived classes can override for specific behavior
    foreach (QAbstractAxis* axis, m_chart->axes()) {
        if (QValueAxis* valueAxis = qobject_cast<QValueAxis*>(axis)) {
            // Find min/max values from all series
            double min = std::numeric_limits<double>::max();
            double max = std::numeric_limits<double>::lowest();
            bool hasData = false;
            
            // This is a placeholder - actual data collection depends on chart type
            // Derived classes should implement proper auto-scaling
            
            if (hasData) {
                double margin = (max - min) * 0.05; // 5% margin
                valueAxis->setRange(min - margin, max + margin);
            }
        }
    }
}

void ChartWidget::updatePerformanceSettings() {
    if (!m_chartView) return;
    
    Monitor::Charts::PerformanceConfig perfConfig = 
        Monitor::Charts::PerformanceConfig::getConfig(m_chartConfig.performanceLevel);
    
    perfConfig.applyToChart(m_chart);
    perfConfig.applyToView(m_chartView);
    
    // Update performance optimization flag
    m_performanceOptimized = (m_chartConfig.performanceLevel == Monitor::Charts::PerformanceLevel::Fast);
}

void ChartWidget::enableZoomPan(bool enable) {
    if (!m_chartView) return;
    
    if (enable) {
        m_chartView->setDragMode(QGraphicsView::RubberBandDrag);
        m_chartView->setRubberBand(QChartView::RectangleRubberBand);
    } else {
        m_chartView->setDragMode(QGraphicsView::NoDrag);
        m_chartView->setRubberBand(QChartView::NoRubberBand);
    }
}

void ChartWidget::showTooltip(const QPointF& position, const QString& text) {
    if (!m_chartConfig.enableTooltips || text.isEmpty()) return;
    
    QPoint globalPos = m_chartView->mapToGlobal(position.toPoint());
    QToolTip::showText(globalPos, text, m_chartView);
}

void ChartWidget::hideTooltip() {
    QToolTip::hideText();
}

QString ChartWidget::getDefaultExportPath() const {
    QString baseName = getWindowTitle();
    if (baseName.isEmpty()) {
        baseName = "chart";
    }
    baseName = baseName.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString fileName = QString("%1_%2.png").arg(baseName, timestamp);
    
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(documentsPath).filePath(fileName);
}

Monitor::Charts::ExportFormat ChartWidget::getFormatFromExtension(const QString& filePath) const {
    QString extension = QFileInfo(filePath).suffix().toLower();
    
    if (extension == "png") return Monitor::Charts::ExportFormat::PNG;
    if (extension == "svg") return Monitor::Charts::ExportFormat::SVG;
    if (extension == "pdf") return Monitor::Charts::ExportFormat::PDF;
    if (extension == "jpg" || extension == "jpeg") return Monitor::Charts::ExportFormat::JPEG;
    
    return m_chartConfig.defaultExportFormat;
}

void ChartWidget::updateFPSCounter() {
    m_frameCount++;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFPSUpdate);
    
    if (elapsed.count() >= 1000) { // Update every second
        m_currentFPS = m_frameCount * 1000.0 / elapsed.count();
        m_frameCount = 0;
        m_lastFPSUpdate = now;
    }
}

void ChartWidget::checkPerformanceThresholds() {
    // Adaptive performance scaling
    if (m_chartConfig.performanceLevel == Monitor::Charts::PerformanceLevel::Adaptive) {
        if (m_currentFPS < 30.0 && !m_performanceOptimized) {
            // Performance is poor, enable optimizations
            m_performanceOptimized = true;
            qDebug() << "ChartWidget: Enabling performance optimizations due to low FPS:" << m_currentFPS;
        } else if (m_currentFPS > 55.0 && m_performanceOptimized) {
            // Performance is good, disable some optimizations
            m_performanceOptimized = false;
            qDebug() << "ChartWidget: Disabling performance optimizations due to good FPS:" << m_currentFPS;
        }
    }
}

void ChartWidget::populateThemeCombo() {
    if (!m_themeCombo) return;
    
    m_themeCombo->clear();
    m_themeCombo->addItem("Light");
    m_themeCombo->addItem("Dark");
    m_themeCombo->addItem("Blue Cerulean");
    m_themeCombo->addItem("Custom");
}

Monitor::Charts::ChartThemeConfig ChartWidget::getCurrentThemeConfig() const {
    return Monitor::Charts::ChartThemeConfig::getTheme(m_chartConfig.theme);
}

QColor ChartWidget::getNextSeriesColor() const {
    return Monitor::Charts::ColorPalette::getColor(m_nextColorIndex);
}