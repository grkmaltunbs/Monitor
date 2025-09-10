#include "bar_chart_widget.h"
#include <QtCharts/QValueAxis>
#include <QApplication>
#include <QDebug>
#include <algorithm>
#include <numeric>
#include <cmath>


// BarSeriesConfig implementation
BarChartWidget::BarSeriesConfig::BarSeriesConfig(const QJsonObject& json) {
    barColor = QColor(json.value("barColor").toString());
    borderColor = QColor(json.value("borderColor").toString("#000000"));
    borderWidth = json.value("borderWidth").toInt(1);
    opacity = json.value("opacity").toDouble(1.0);
    labelFormat = json.value("labelFormat").toString("%.2f");
    showLabels = json.value("showLabels").toBool(true);
    labelColor = QColor(json.value("labelColor").toString("#000000"));
    useGradient = json.value("useGradient").toBool(false);
    gradientStartColor = QColor(json.value("gradientStartColor").toString());
    gradientEndColor = QColor(json.value("gradientEndColor").toString());
    gradientDirection = static_cast<Qt::Orientation>(json.value("gradientDirection").toInt());
}

QJsonObject BarChartWidget::BarSeriesConfig::toJson() const {
    QJsonObject json;
    json["barColor"] = barColor.name();
    json["borderColor"] = borderColor.name();
    json["borderWidth"] = borderWidth;
    json["opacity"] = opacity;
    json["labelFormat"] = labelFormat;
    json["showLabels"] = showLabels;
    json["labelColor"] = labelColor.name();
    json["useGradient"] = useGradient;
    json["gradientStartColor"] = gradientStartColor.name();
    json["gradientEndColor"] = gradientEndColor.name();
    json["gradientDirection"] = static_cast<int>(gradientDirection);
    return json;
}

// BarSeriesData implementation
void BarChartWidget::BarSeriesData::addValue(const QString& category, double value, 
                                           BarChartConfig::AggregationMethod method) {
    switch (method) {
        case BarChartConfig::AggregationMethod::Last:
            categoryValues[category] = value;
            break;
            
        case BarChartConfig::AggregationMethod::Sum:
        case BarChartConfig::AggregationMethod::Average:
        case BarChartConfig::AggregationMethod::Count:
        case BarChartConfig::AggregationMethod::Min:
        case BarChartConfig::AggregationMethod::Max:
            categoryHistory[category].push_back(value);
            categoryValues[category] = getAggregatedValue(category, method);
            break;
    }
    needsUpdate = true;
}

void BarChartWidget::BarSeriesData::clearData() {
    categoryValues.clear();
    categoryHistory.clear();
    needsUpdate = true;
}

double BarChartWidget::BarSeriesData::getAggregatedValue(const QString& category, 
                                                        BarChartConfig::AggregationMethod method) const {
    auto historyIt = categoryHistory.find(category);
    if (historyIt == categoryHistory.end() || historyIt->second.empty()) {
        return 0.0;
    }
    
    const std::vector<double>& values = historyIt->second;
    
    switch (method) {
        case BarChartConfig::AggregationMethod::Last:
            return values.back();
            
        case BarChartConfig::AggregationMethod::Sum:
            return std::accumulate(values.begin(), values.end(), 0.0);
            
        case BarChartConfig::AggregationMethod::Average:
            return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
            
        case BarChartConfig::AggregationMethod::Count:
            return static_cast<double>(values.size());
            
        case BarChartConfig::AggregationMethod::Min:
            return *std::min_element(values.begin(), values.end());
            
        case BarChartConfig::AggregationMethod::Max:
            return *std::max_element(values.begin(), values.end());
    }
    
    return 0.0;
}

// BarChartWidget implementation
BarChartWidget::BarChartWidget(const QString& widgetId, QWidget* parent)
    : ChartWidget(widgetId, "Bar Chart", parent)
    , m_barSeries(nullptr)
    , m_categoryAxis(nullptr)
    , m_valueAxis(nullptr)
    , m_realTimeTimer(new QTimer(this))
    , m_chartTypeCombo(nullptr)
    , m_orientationCombo(nullptr)
    , m_realTimeModeCheckBox(nullptr)
    , m_valueLabelsCheckBox(nullptr)
    , m_maxCategoriesSpin(nullptr)
{
    // Initialize bar chart configuration
    m_barConfig = BarChartConfig();
    
    // Setup real-time update timer
    m_realTimeTimer->setSingleShot(false);
    m_realTimeTimer->setInterval(m_barConfig.updateInterval);
    connect(m_realTimeTimer, &QTimer::timeout, this, &BarChartWidget::onRealTimeUpdate);
    
    // Start real-time timer if enabled
    if (m_barConfig.enableRealTimeMode) {
        m_realTimeTimer->start();
    }
}

BarChartWidget::~BarChartWidget() {
    // Clean up series data
    m_seriesData.clear();
    m_barSeriesConfigs.clear();
}

void BarChartWidget::createChart() {
    // Create the chart
    m_chart = new QChart();
    m_chart->setTitle("Bar Chart");
    m_chart->setAnimationOptions(m_barConfig.enableAnimations ? 
        QChart::SeriesAnimations : QChart::NoAnimation);
    
    // Create initial bar series
    recreateBarSeries();
    
    // Setup axes
    updateCategoryAxis();
    updateValueAxis();
    
    // Setup toolbar extensions
    setupToolbarExtensions();
    
    // Apply initial configuration
    applyChartConfig();
}

void BarChartWidget::setupToolbarExtensions() {
    if (!m_toolbar) return;
    
    // Add bar chart specific toolbar items
    m_toolbar->addSeparator();
    
    // Chart type selector
    m_toolbar->addWidget(new QLabel("Type:"));
    m_chartTypeCombo = new QComboBox();
    m_chartTypeCombo->addItems({"Grouped", "Stacked", "Percent"});
    m_chartTypeCombo->setCurrentIndex(static_cast<int>(m_barConfig.chartType));
    connect(m_chartTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BarChartWidget::onChartTypeChanged);
    m_toolbar->addWidget(m_chartTypeCombo);
    
    // Orientation selector
    m_toolbar->addWidget(new QLabel("Orient:"));
    m_orientationCombo = new QComboBox();
    m_orientationCombo->addItems({"Vertical", "Horizontal"});
    m_orientationCombo->setCurrentIndex(static_cast<int>(m_barConfig.orientation));
    connect(m_orientationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BarChartWidget::onOrientationChanged);
    m_toolbar->addWidget(m_orientationCombo);
    
    // Real-time mode toggle
    m_realTimeModeCheckBox = new QCheckBox("Real-time");
    m_realTimeModeCheckBox->setChecked(m_barConfig.enableRealTimeMode);
    connect(m_realTimeModeCheckBox, &QCheckBox::toggled, 
            this, &BarChartWidget::onToggleRealTimeMode);
    m_toolbar->addWidget(m_realTimeModeCheckBox);
    
    // Value labels toggle
    m_valueLabelsCheckBox = new QCheckBox("Labels");
    m_valueLabelsCheckBox->setChecked(m_barConfig.showValueLabels);
    connect(m_valueLabelsCheckBox, &QCheckBox::toggled,
            this, &BarChartWidget::onToggleValueLabels);
    m_toolbar->addWidget(m_valueLabelsCheckBox);
    
    // Max categories spinner
    m_toolbar->addWidget(new QLabel("Max Cat:"));
    m_maxCategoriesSpin = new QSpinBox();
    m_maxCategoriesSpin->setRange(1, 200);
    m_maxCategoriesSpin->setValue(m_barConfig.maxCategories);
    connect(m_maxCategoriesSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int value) {
                m_barConfig.maxCategories = value;
                limitCategories();
                updateBarSetData();
            });
    m_toolbar->addWidget(m_maxCategoriesSpin);
    
    m_toolbar->addSeparator();
    
    // Add utility actions
    QAction* clearDataAction = m_toolbar->addAction("Clear");
    connect(clearDataAction, &QAction::triggered, this, &BarChartWidget::onClearData);
    
    QAction* sortCategoriesAction = m_toolbar->addAction("Sort");
    connect(sortCategoriesAction, &QAction::triggered, this, &BarChartWidget::onSortCategories);
}

void BarChartWidget::setBarChartConfig(const BarChartConfig& config) {
    bool needsRecreate = (config.chartType != m_barConfig.chartType || 
                         config.orientation != m_barConfig.orientation);
    
    m_barConfig = config;
    
    if (needsRecreate) {
        recreateBarSeries();
    }
    
    updateRealTimeSettings();
    updateCategoryAxis();
    updateValueAxis();
    
    // Update toolbar controls
    if (m_chartTypeCombo) {
        m_chartTypeCombo->setCurrentIndex(static_cast<int>(m_barConfig.chartType));
    }
    if (m_orientationCombo) {
        m_orientationCombo->setCurrentIndex(static_cast<int>(m_barConfig.orientation));
    }
    if (m_realTimeModeCheckBox) {
        m_realTimeModeCheckBox->setChecked(m_barConfig.enableRealTimeMode);
    }
    if (m_valueLabelsCheckBox) {
        m_valueLabelsCheckBox->setChecked(m_barConfig.showValueLabels);
    }
    if (m_maxCategoriesSpin) {
        m_maxCategoriesSpin->setValue(m_barConfig.maxCategories);
    }
    
    // Update chart animations
    if (m_chart) {
        m_chart->setAnimationOptions(m_barConfig.enableAnimations ? 
            QChart::SeriesAnimations : QChart::NoAnimation);
    }
}

void BarChartWidget::resetBarChartConfig() {
    m_barConfig = BarChartConfig();
    setBarChartConfig(m_barConfig);
}

bool BarChartWidget::addBarSeries(const QString& fieldPath, const QString& seriesName,
                                 const QColor& color, const BarSeriesConfig& config) {
    SeriesConfig baseConfig;
    baseConfig.fieldPath = fieldPath;
    baseConfig.seriesName = seriesName.isEmpty() ? fieldPath : seriesName;
    baseConfig.color = color.isValid() ? color : Monitor::Charts::ColorPalette::getColor(m_nextColorIndex);
    
    // Store bar-specific configuration
    BarSeriesConfig barConfig = config;
    barConfig.barColor = baseConfig.color;
    m_barSeriesConfigs[fieldPath] = barConfig;
    
    return addSeries(fieldPath, baseConfig);
}

void BarChartWidget::setBarSeriesConfig(const QString& fieldPath, const BarSeriesConfig& config) {
    auto it = m_barSeriesConfigs.find(fieldPath);
    if (it == m_barSeriesConfigs.end()) {
        return;
    }
    
    it->second = config;
    
    // Apply configuration to existing bar set
    auto dataIt = m_seriesData.find(fieldPath);
    if (dataIt != m_seriesData.end() && dataIt->second->barSet) {
        applyBarSeriesConfig(dataIt->second->barSet, config);
    }
}

BarChartWidget::BarSeriesConfig BarChartWidget::getBarSeriesConfig(const QString& fieldPath) const {
    auto it = m_barSeriesConfigs.find(fieldPath);
    return (it != m_barSeriesConfigs.end()) ? it->second : BarSeriesConfig();
}

QAbstractSeries* BarChartWidget::createSeriesForField(const QString& fieldPath, const SeriesConfig& config) {
    // Get bar-specific configuration
    auto barConfigIt = m_barSeriesConfigs.find(fieldPath);
    BarSeriesConfig barConfig = (barConfigIt != m_barSeriesConfigs.end()) ? 
        barConfigIt->second : BarSeriesConfig();
    
    // Create series data storage
    auto seriesData = std::make_unique<BarSeriesData>();
    seriesData->config = barConfig;
    
    // Create bar set
    QBarSet* barSet = new QBarSet(config.seriesName);
    applyBarSeriesConfig(barSet, barConfig);
    seriesData->barSet = barSet;
    
    // Connect bar set signals
    connect(barSet, &QBarSet::hovered, this, &BarChartWidget::onBarSetHovered);
    connect(barSet, &QBarSet::clicked, this, &BarChartWidget::onBarSetClicked);
    
    // Add bar set to current bar series
    if (m_barSeries) {
        m_barSeries->append(barSet);
    }
    
    // Store series data
    m_seriesData[fieldPath] = std::move(seriesData);
    
    // Return the bar series (not the bar set)
    return m_barSeries;
}

void BarChartWidget::removeSeriesForField(const QString& fieldPath) {
    auto it = m_seriesData.find(fieldPath);
    if (it != m_seriesData.end()) {
        // Remove bar set from series
        if (m_barSeries && it->second->barSet) {
            m_barSeries->remove(it->second->barSet);
            // Note: QBarSeries takes ownership of bar sets, so we don't delete manually
        }
        
        // Remove from storage
        m_seriesData.erase(it);
    }
    
    // Remove bar configuration
    m_barSeriesConfigs.erase(fieldPath);
}

void BarChartWidget::configureSeries(const QString& fieldPath, const SeriesConfig& config) {
    auto it = m_seriesData.find(fieldPath);
    if (it == m_seriesData.end()) {
        return;
    }
    
    BarSeriesData* data = it->second.get();
    
    // Update bar set
    if (data->barSet) {
        data->barSet->setLabel(config.seriesName);
        data->barSet->setColor(config.color);
        
        // Update bar-specific configuration
        BarSeriesConfig barConfig = data->config;
        barConfig.barColor = config.color;
        applyBarSeriesConfig(data->barSet, barConfig);
    }
    
    // Update series visibility
    if (m_barSeries) {
        m_barSeries->setVisible(config.visible);
    }
}

void BarChartWidget::updateSeriesData() {
    // Update all series that need updates
    bool hasUpdates = false;
    for (auto& pair : m_seriesData) {
        if (pair.second->needsUpdate) {
            hasUpdates = true;
            break;
        }
    }
    
    if (hasUpdates) {
        updateBarSetData();
        
        // Clear update flags
        for (auto& pair : m_seriesData) {
            pair.second->needsUpdate = false;
        }
    }
    
    // Update current point count for performance monitoring
    m_currentPointCount = m_categories.size() * m_seriesData.size();
}

void BarChartWidget::updateFieldDisplay(const QString& fieldPath, const QVariant& value) {
    auto it = m_seriesData.find(fieldPath);
    if (it == m_seriesData.end()) {
        return;
    }
    
    // Extract category from field value or use field path
    QString category = extractCategory(value, fieldPath);
    
    // Convert field value to numeric
    bool ok;
    double numericValue = Monitor::Charts::DataConverter::toDouble(value, &ok);
    if (!ok) {
        qWarning() << "BarChartWidget: Cannot convert value to double for field" << fieldPath;
        return;
    }
    
    // Add data point
    addDataPoint(fieldPath, category, numericValue);
    
    // For immediate update mode, update display immediately
    if (getChartConfig().updateMode == UpdateMode::Immediate) {
        updateBarSetData();
    }
}

void BarChartWidget::clearFieldDisplay(const QString& fieldPath) {
    auto it = m_seriesData.find(fieldPath);
    if (it != m_seriesData.end()) {
        it->second->clearData();
        it->second->needsUpdate = true;
        
        // Clear the bar set immediately
        if (it->second->barSet) {
            it->second->barSet->remove(0, it->second->barSet->count());
        }
    }
}

void BarChartWidget::refreshAllDisplays() {
    // Mark all series for update
    for (auto& pair : m_seriesData) {
        pair.second->needsUpdate = true;
    }
    
    updateSeriesData();
}

// Category management
void BarChartWidget::addCategory(const QString& category) {
    ensureCategory(category);
}

void BarChartWidget::removeCategory(const QString& category) {
    int index = m_categories.indexOf(category);
    if (index >= 0) {
        m_categories.removeAt(index);
        
        // Remove from all series data
        for (auto& pair : m_seriesData) {
            pair.second->categoryValues.erase(category);
            pair.second->categoryHistory.erase(category);
            pair.second->needsUpdate = true;
        }
        
        rebuildCategoryIndices();
        updateCategoryAxis();
        updateBarSetData();
        
        emit categoryRemoved(category);
    }
}

void BarChartWidget::clearCategories() {
    m_categories.clear();
    m_categoryIndices.clear();
    
    // Clear all series data
    for (auto& pair : m_seriesData) {
        pair.second->clearData();
    }
    
    updateCategoryAxis();
    updateBarSetData();
}

QStringList BarChartWidget::getCategories() const {
    return m_categories;
}

void BarChartWidget::setCategoryFieldPath(const QString& fieldPath) {
    m_barConfig.categoryFieldPath = fieldPath;
    m_barConfig.categoryMode = BarChartConfig::CategoryMode::FieldBased;
}

// Data access
double BarChartWidget::getBarValue(const QString& fieldPath, const QString& category) const {
    auto it = m_seriesData.find(fieldPath);
    if (it != m_seriesData.end()) {
        auto valueIt = it->second->categoryValues.find(category);
        if (valueIt != it->second->categoryValues.end()) {
            return valueIt->second;
        }
    }
    return 0.0;
}

QStringList BarChartWidget::getSeriesCategories(const QString& fieldPath) const {
    auto it = m_seriesData.find(fieldPath);
    if (it != m_seriesData.end()) {
        QStringList categories;
        for (const auto& pair : it->second->categoryValues) {
            categories.append(pair.first);
        }
        return categories;
    }
    return QStringList();
}

int BarChartWidget::getCategoryCount() const {
    return m_categories.size();
}

// Chart type and orientation
void BarChartWidget::setChartType(BarChartConfig::ChartType type) {
    if (type != m_barConfig.chartType) {
        m_barConfig.chartType = type;
        recreateBarSeries();
        
        if (m_chartTypeCombo) {
            m_chartTypeCombo->setCurrentIndex(static_cast<int>(type));
        }
        
        emit chartTypeChanged(type);
    }
}

void BarChartWidget::setOrientation(BarChartConfig::Orientation orientation) {
    if (orientation != m_barConfig.orientation) {
        m_barConfig.orientation = orientation;
        recreateBarSeries();
        
        if (m_orientationCombo) {
            m_orientationCombo->setCurrentIndex(static_cast<int>(orientation));
        }
        
        emit orientationChanged(orientation);
    }
}

// Real-time control
void BarChartWidget::setRealTimeMode(bool enabled) {
    m_barConfig.enableRealTimeMode = enabled;
    updateRealTimeSettings();
    
    if (m_realTimeModeCheckBox) {
        m_realTimeModeCheckBox->setChecked(enabled);
    }
}

void BarChartWidget::clearSeriesData(const QString& fieldPath) {
    clearFieldDisplay(fieldPath);
}

void BarChartWidget::clearAllData() {
    for (auto& pair : m_seriesData) {
        pair.second->clearData();
    }
    clearCategories();
}

void BarChartWidget::sortCategories(bool ascending) {
    if (ascending) {
        std::sort(m_categories.begin(), m_categories.end());
    } else {
        std::sort(m_categories.begin(), m_categories.end(), std::greater<QString>());
    }
    
    rebuildCategoryIndices();
    updateCategoryAxis();
    updateBarSetData();
}

// Settings implementation
QJsonObject BarChartWidget::saveWidgetSpecificSettings() const {
    QJsonObject settings = ChartWidget::saveWidgetSpecificSettings();
    
    // Bar chart configuration
    QJsonObject barConfig;
    barConfig["chartType"] = static_cast<int>(m_barConfig.chartType);
    barConfig["orientation"] = static_cast<int>(m_barConfig.orientation);
    barConfig["categoryMode"] = static_cast<int>(m_barConfig.categoryMode);
    barConfig["barWidth"] = m_barConfig.barWidth;
    barConfig["barSpacing"] = m_barConfig.barSpacing;
    barConfig["showValueLabels"] = m_barConfig.showValueLabels;
    barConfig["showBarBorders"] = m_barConfig.showBarBorders;
    barConfig["barBorderColor"] = m_barConfig.barBorderColor.name();
    barConfig["barBorderWidth"] = m_barConfig.barBorderWidth;
    
    QJsonArray categoriesArray;
    for (const QString& category : m_barConfig.customCategories) {
        categoriesArray.append(category);
    }
    barConfig["customCategories"] = categoriesArray;
    barConfig["categoryFieldPath"] = m_barConfig.categoryFieldPath;
    barConfig["maxCategories"] = m_barConfig.maxCategories;
    barConfig["autoSortCategories"] = m_barConfig.autoSortCategories;
    barConfig["aggregation"] = static_cast<int>(m_barConfig.aggregation);
    barConfig["enableAnimations"] = m_barConfig.enableAnimations;
    barConfig["animationDuration"] = m_barConfig.animationDuration;
    barConfig["animationEasing"] = static_cast<int>(m_barConfig.animationEasing);
    barConfig["enableRealTimeMode"] = m_barConfig.enableRealTimeMode;
    barConfig["updateInterval"] = m_barConfig.updateInterval;
    
    settings["barConfig"] = barConfig;
    
    // Bar series configurations
    QJsonArray barSeriesArray;
    for (const auto& pair : m_barSeriesConfigs) {
        QJsonObject seriesObj;
        seriesObj["fieldPath"] = pair.first;
        seriesObj["config"] = pair.second.toJson();
        barSeriesArray.append(seriesObj);
    }
    settings["barSeriesConfigs"] = barSeriesArray;
    
    // Categories
    QJsonArray currentCategories;
    for (const QString& category : m_categories) {
        currentCategories.append(category);
    }
    settings["categories"] = currentCategories;
    
    return settings;
}

bool BarChartWidget::restoreWidgetSpecificSettings(const QJsonObject& settings) {
    // Restore base chart settings
    if (!ChartWidget::restoreWidgetSpecificSettings(settings)) {
        return false;
    }
    
    // Restore bar chart configuration
    if (settings.contains("barConfig")) {
        QJsonObject barConfig = settings["barConfig"].toObject();
        
        m_barConfig.chartType = static_cast<BarChartConfig::ChartType>(
            barConfig.value("chartType").toInt());
        m_barConfig.orientation = static_cast<BarChartConfig::Orientation>(
            barConfig.value("orientation").toInt());
        m_barConfig.categoryMode = static_cast<BarChartConfig::CategoryMode>(
            barConfig.value("categoryMode").toInt());
        m_barConfig.barWidth = barConfig.value("barWidth").toDouble(0.8);
        m_barConfig.barSpacing = barConfig.value("barSpacing").toDouble(0.2);
        m_barConfig.showValueLabels = barConfig.value("showValueLabels").toBool(true);
        m_barConfig.showBarBorders = barConfig.value("showBarBorders").toBool(true);
        m_barConfig.barBorderColor = QColor(barConfig.value("barBorderColor").toString("#000000"));
        m_barConfig.barBorderWidth = barConfig.value("barBorderWidth").toInt(1);
        
        if (barConfig.contains("customCategories")) {
            m_barConfig.customCategories.clear();
            QJsonArray categoriesArray = barConfig["customCategories"].toArray();
            for (const QJsonValue& value : categoriesArray) {
                m_barConfig.customCategories.append(value.toString());
            }
        }
        
        m_barConfig.categoryFieldPath = barConfig.value("categoryFieldPath").toString();
        m_barConfig.maxCategories = barConfig.value("maxCategories").toInt(50);
        m_barConfig.autoSortCategories = barConfig.value("autoSortCategories").toBool(true);
        m_barConfig.aggregation = static_cast<BarChartConfig::AggregationMethod>(
            barConfig.value("aggregation").toInt());
        m_barConfig.enableAnimations = barConfig.value("enableAnimations").toBool(true);
        m_barConfig.animationDuration = barConfig.value("animationDuration").toInt(500);
        m_barConfig.animationEasing = static_cast<QEasingCurve::Type>(
            barConfig.value("animationEasing").toInt());
        m_barConfig.enableRealTimeMode = barConfig.value("enableRealTimeMode").toBool(true);
        m_barConfig.updateInterval = barConfig.value("updateInterval").toInt(100);
    }
    
    // Restore bar series configurations
    if (settings.contains("barSeriesConfigs")) {
        m_barSeriesConfigs.clear();
        QJsonArray barSeriesArray = settings["barSeriesConfigs"].toArray();
        
        for (const QJsonValue& value : barSeriesArray) {
            QJsonObject seriesObj = value.toObject();
            QString fieldPath = seriesObj.value("fieldPath").toString();
            BarSeriesConfig config(seriesObj.value("config").toObject());
            m_barSeriesConfigs[fieldPath] = config;
        }
    }
    
    // Restore categories
    if (settings.contains("categories")) {
        m_categories.clear();
        QJsonArray categoriesArray = settings["categories"].toArray();
        for (const QJsonValue& value : categoriesArray) {
            m_categories.append(value.toString());
        }
        rebuildCategoryIndices();
    }
    
    // Apply configuration
    setBarChartConfig(m_barConfig);
    
    return true;
}

void BarChartWidget::setupContextMenu() {
    ChartWidget::setupContextMenu();
    
    // Add bar chart specific context menu items
    if (QMenu* menu = getContextMenu()) {
        menu->addSeparator();
        
        // Chart type submenu
        QMenu* chartTypeMenu = menu->addMenu("Chart Type");
        QActionGroup* typeGroup = new QActionGroup(chartTypeMenu);
        
        QAction* groupedAction = chartTypeMenu->addAction("Grouped");
        groupedAction->setCheckable(true);
        groupedAction->setChecked(m_barConfig.chartType == BarChartConfig::ChartType::Grouped);
        typeGroup->addAction(groupedAction);
        connect(groupedAction, &QAction::triggered, 
                [this]() { setChartType(BarChartConfig::ChartType::Grouped); });
        
        QAction* stackedAction = chartTypeMenu->addAction("Stacked");
        stackedAction->setCheckable(true);
        stackedAction->setChecked(m_barConfig.chartType == BarChartConfig::ChartType::Stacked);
        typeGroup->addAction(stackedAction);
        connect(stackedAction, &QAction::triggered, 
                [this]() { setChartType(BarChartConfig::ChartType::Stacked); });
        
        QAction* percentAction = chartTypeMenu->addAction("Percent");
        percentAction->setCheckable(true);
        percentAction->setChecked(m_barConfig.chartType == BarChartConfig::ChartType::Percent);
        typeGroup->addAction(percentAction);
        connect(percentAction, &QAction::triggered, 
                [this]() { setChartType(BarChartConfig::ChartType::Percent); });
        
        // Orientation submenu
        QMenu* orientationMenu = menu->addMenu("Orientation");
        QActionGroup* orientationGroup = new QActionGroup(orientationMenu);
        
        QAction* verticalAction = orientationMenu->addAction("Vertical");
        verticalAction->setCheckable(true);
        verticalAction->setChecked(m_barConfig.orientation == BarChartConfig::Orientation::Vertical);
        orientationGroup->addAction(verticalAction);
        connect(verticalAction, &QAction::triggered, 
                [this]() { setOrientation(BarChartConfig::Orientation::Vertical); });
        
        QAction* horizontalAction = orientationMenu->addAction("Horizontal");
        horizontalAction->setCheckable(true);
        horizontalAction->setChecked(m_barConfig.orientation == BarChartConfig::Orientation::Horizontal);
        orientationGroup->addAction(horizontalAction);
        connect(horizontalAction, &QAction::triggered, 
                [this]() { setOrientation(BarChartConfig::Orientation::Horizontal); });
        
        menu->addSeparator();
        
        QAction* valueLabelsAction = menu->addAction("Show Value Labels");
        valueLabelsAction->setCheckable(true);
        valueLabelsAction->setChecked(m_barConfig.showValueLabels);
        connect(valueLabelsAction, &QAction::triggered, this, &BarChartWidget::onToggleValueLabels);
        
        menu->addSeparator();
        menu->addAction("Clear All Data", this, &BarChartWidget::onClearData);
        menu->addAction("Sort Categories", this, &BarChartWidget::onSortCategories);
    }
}

// Slots implementation
void BarChartWidget::onChartTypeChanged(int type) {
    setChartType(static_cast<BarChartConfig::ChartType>(type));
}

void BarChartWidget::onOrientationChanged(int orientation) {
    setOrientation(static_cast<BarChartConfig::Orientation>(orientation));
}

void BarChartWidget::onToggleRealTimeMode(bool enabled) {
    setRealTimeMode(enabled);
}

void BarChartWidget::onToggleValueLabels(bool enabled) {
    m_barConfig.showValueLabels = enabled;
    
    // Update all bar sets to show/hide labels
    if (m_barSeries) {
        m_barSeries->setLabelsVisible(enabled);
    }
}

void BarChartWidget::onClearData() {
    clearAllData();
}

void BarChartWidget::onSortCategories() {
    sortCategories(true);
}

void BarChartWidget::onBarSetHovered(bool state, int index) {
    QBarSet* barSet = qobject_cast<QBarSet*>(sender());
    if (!barSet || index < 0 || index >= m_categories.size()) {
        return;
    }
    
    // Find which field path this bar set belongs to
    QString fieldPath;
    for (const auto& pair : m_seriesData) {
        if (pair.second->barSet == barSet) {
            fieldPath = pair.first;
            break;
        }
    }
    
    if (!fieldPath.isEmpty()) {
        QString category = m_categories[index];
        double value = barSet->at(index);
        emit barHovered(fieldPath, category, value, state);
        
        if (state && getChartConfig().enableTooltips) {
            QString tooltip = QString("%1\nCategory: %2\nValue: %3")
                .arg(fieldPath)
                .arg(category)
                .arg(value, 0, 'f', 2);
            
            // Show tooltip at mouse position
            QPoint mousePos = QCursor::pos();
            QPoint localPos = m_chartView->mapFromGlobal(mousePos);
            showTooltip(QPointF(localPos), tooltip);
        } else {
            hideTooltip();
        }
    }
}

void BarChartWidget::onBarSetClicked(int index) {
    QBarSet* barSet = qobject_cast<QBarSet*>(sender());
    if (!barSet || index < 0 || index >= m_categories.size()) {
        return;
    }
    
    // Find which field path this bar set belongs to
    QString fieldPath;
    for (const auto& pair : m_seriesData) {
        if (pair.second->barSet == barSet) {
            fieldPath = pair.first;
            break;
        }
    }
    
    if (!fieldPath.isEmpty()) {
        QString category = m_categories[index];
        double value = barSet->at(index);
        emit barClicked(fieldPath, category, value);
    }
}

void BarChartWidget::onRealTimeUpdate() {
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

// Helper method implementations
void BarChartWidget::updateRealTimeSettings() {
    m_realTimeTimer->setInterval(m_barConfig.updateInterval);
    
    if (m_barConfig.enableRealTimeMode) {
        if (!m_realTimeTimer->isActive()) {
            m_realTimeTimer->start();
        }
    } else {
        m_realTimeTimer->stop();
    }
}

void BarChartWidget::recreateBarSeries() {
    // Remove existing series
    if (m_barSeries && m_chart) {
        m_chart->removeSeries(m_barSeries);
        delete m_barSeries;
    }
    
    // Create new series based on configuration
    m_barSeries = createBarSeriesOfType(m_barConfig.chartType, m_barConfig.orientation);
    
    if (m_barSeries && m_chart) {
        m_chart->addSeries(m_barSeries);
        
        // Configure series
        m_barSeries->setLabelsVisible(m_barConfig.showValueLabels);
        
        // Reattach existing bar sets
        for (auto& pair : m_seriesData) {
            if (pair.second->barSet) {
                m_barSeries->append(pair.second->barSet);
            }
        }
        
        // Attach axes
        if (m_categoryAxis && m_valueAxis) {
            if (m_barConfig.orientation == BarChartConfig::Orientation::Vertical) {
                m_barSeries->attachAxis(m_categoryAxis);
                m_barSeries->attachAxis(m_valueAxis);
            } else {
                m_barSeries->attachAxis(m_valueAxis);
                m_barSeries->attachAxis(m_categoryAxis);
            }
        }
    }
}

void BarChartWidget::updateCategoryAxis() {
    if (!m_chart) return;
    
    // Remove existing category axis
    if (m_categoryAxis) {
        m_chart->removeAxis(m_categoryAxis);
        delete m_categoryAxis;
    }
    
    // Create new category axis
    m_categoryAxis = new QBarCategoryAxis();
    m_categoryAxis->append(m_categories);
    m_categoryAxis->setTitleText("Categories");
    
    // Add to chart
    Qt::Alignment alignment = (m_barConfig.orientation == BarChartConfig::Orientation::Vertical) ?
        Qt::AlignBottom : Qt::AlignLeft;
    m_chart->addAxis(m_categoryAxis, alignment);
    
    // Attach to series
    if (m_barSeries) {
        m_barSeries->attachAxis(m_categoryAxis);
    }
    
    // Apply theme
    Monitor::Charts::ChartThemeConfig themeConfig = getCurrentThemeConfig();
    m_categoryAxis->setGridLineVisible(getChartConfig().showGrid);
    m_categoryAxis->setLabelsColor(themeConfig.axisLabelColor);
    m_categoryAxis->setTitleBrush(QBrush(themeConfig.axisLabelColor));
}

void BarChartWidget::updateValueAxis() {
    if (!m_chart) return;
    
    // Remove existing value axis
    if (m_valueAxis) {
        m_chart->removeAxis(m_valueAxis);
        delete m_valueAxis;
    }
    
    // Create new value axis
    m_valueAxis = new QValueAxis();
    m_valueAxis->setTitleText("Values");
    m_valueAxis->setLabelFormat("%.2f");
    
    // Add to chart
    Qt::Alignment alignment = (m_barConfig.orientation == BarChartConfig::Orientation::Vertical) ?
        Qt::AlignLeft : Qt::AlignBottom;
    m_chart->addAxis(m_valueAxis, alignment);
    
    // Attach to series
    if (m_barSeries) {
        m_barSeries->attachAxis(m_valueAxis);
    }
    
    // Apply theme
    Monitor::Charts::ChartThemeConfig themeConfig = getCurrentThemeConfig();
    m_valueAxis->setGridLineVisible(getChartConfig().showGrid);
    m_valueAxis->setGridLineColor(themeConfig.gridLineColor);
    m_valueAxis->setLabelsColor(themeConfig.axisLabelColor);
    m_valueAxis->setTitleBrush(QBrush(themeConfig.axisLabelColor));
}

void BarChartWidget::applyBarSeriesConfig(QBarSet* barSet, const BarSeriesConfig& config) {
    if (!barSet) return;
    
    barSet->setColor(config.barColor);
    barSet->setBorderColor(config.borderColor);
    barSet->setPen(QPen(config.borderColor, config.borderWidth));
    
    // Apply gradient if configured
    if (config.useGradient) {
        QLinearGradient gradient;
        if (config.gradientDirection == Qt::Vertical) {
            gradient = QLinearGradient(0, 0, 0, 1);
        } else {
            gradient = QLinearGradient(0, 0, 1, 0);
        }
        gradient.setColorAt(0, config.gradientStartColor);
        gradient.setColorAt(1, config.gradientEndColor);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        barSet->setBrush(QBrush(gradient));
    }
    
    // Set label formatting
    //barSet->setLabelFormat(config.labelFormat); // Not available in Qt6
    barSet->setLabelColor(config.labelColor);
}

QString BarChartWidget::extractCategory(const QVariant& fieldValue, const QString& fieldPath) {
    Q_UNUSED(fieldPath);
    
    switch (m_barConfig.categoryMode) {
        case BarChartConfig::CategoryMode::FieldBased:
            return Monitor::Charts::DataConverter::toString(fieldValue);
            
        case BarChartConfig::CategoryMode::PacketBased:
            // TODO: Extract from packet ID/type
            return "Packet";
            
        case BarChartConfig::CategoryMode::TimeBased:
            // TODO: Create time-based categories
            return "Time";
            
        case BarChartConfig::CategoryMode::Custom:
            // Use first available custom category or create new one
            if (!m_barConfig.customCategories.isEmpty()) {
                return m_barConfig.customCategories.first();
            }
            return "Default";
    }
    
    return Monitor::Charts::DataConverter::toString(fieldValue);
}

void BarChartWidget::addDataPoint(const QString& fieldPath, const QString& category, double value) {
    auto it = m_seriesData.find(fieldPath);
    if (it != m_seriesData.end()) {
        ensureCategory(category);
        it->second->addValue(category, value, m_barConfig.aggregation);
    }
}

void BarChartWidget::updateBarSetData() {
    // Update all bar sets with current data
    for (auto& pair : m_seriesData) {
        const QString& fieldPath = pair.first; Q_UNUSED(fieldPath);
        BarSeriesData* data = pair.second.get();
        
        if (!data->barSet) continue;
        
        // Resize bar set to match categories
        while (data->barSet->count() < m_categories.size()) {
            data->barSet->append(0.0);
        }
        while (data->barSet->count() > m_categories.size()) {
            data->barSet->remove(data->barSet->count() - 1);
        }
        
        // Update values for each category
        for (int i = 0; i < m_categories.size(); ++i) {
            const QString& category = m_categories[i];
            auto valueIt = data->categoryValues.find(category);
            double value = (valueIt != data->categoryValues.end()) ? valueIt->second : 0.0;
            data->barSet->replace(i, value);
        }
    }
    
    // Auto-scale value axis
    if (isAutoScale() && m_valueAxis) {
        double minValue = 0.0;
        double maxValue = 0.0;
        bool hasData = false;
        
        for (const auto& pair : m_seriesData) {
            for (const auto& valuePair : pair.second->categoryValues) {
                if (!hasData) {
                    minValue = maxValue = valuePair.second;
                    hasData = true;
                } else {
                    minValue = std::min(minValue, valuePair.second);
                    maxValue = std::max(maxValue, valuePair.second);
                }
            }
        }
        
        if (hasData) {
            double margin = (maxValue - minValue) * 0.1; // 10% margin
            m_valueAxis->setRange(minValue - margin, maxValue + margin);
        }
    }
}

void BarChartWidget::ensureCategory(const QString& category) {
    if (!m_categories.contains(category)) {
        m_categories.append(category);
        
        // Sort if enabled
        if (m_barConfig.autoSortCategories) {
            std::sort(m_categories.begin(), m_categories.end());
        }
        
        // Limit categories if needed
        limitCategories();
        
        rebuildCategoryIndices();
        updateCategoryAxis();
        
        emit categoryAdded(category);
    }
}

void BarChartWidget::rebuildCategoryIndices() {
    m_categoryIndices.clear();
    for (int i = 0; i < m_categories.size(); ++i) {
        m_categoryIndices[m_categories[i]] = i;
    }
}

void BarChartWidget::limitCategories() {
    while (m_categories.size() > m_barConfig.maxCategories) {
        QString removedCategory = m_categories.takeFirst();
        
        // Remove from all series data
        for (auto& pair : m_seriesData) {
            pair.second->categoryValues.erase(removedCategory);
            pair.second->categoryHistory.erase(removedCategory);
        }
    }
}

QAbstractBarSeries* BarChartWidget::createBarSeriesOfType(BarChartConfig::ChartType type, 
                                                         BarChartConfig::Orientation orientation) {
    QAbstractBarSeries* series = nullptr;
    
    switch (type) {
        case BarChartConfig::ChartType::Grouped:
            if (orientation == BarChartConfig::Orientation::Vertical) {
                series = new QBarSeries();
            } else {
                series = new QHorizontalBarSeries();
            }
            break;
            
        case BarChartConfig::ChartType::Stacked:
            if (orientation == BarChartConfig::Orientation::Vertical) {
                series = new QStackedBarSeries();
            } else {
                series = new QHorizontalStackedBarSeries();
            }
            break;
            
        case BarChartConfig::ChartType::Percent:
            if (orientation == BarChartConfig::Orientation::Vertical) {
                series = new QPercentBarSeries();
            } else {
                series = new QHorizontalPercentBarSeries();
            }
            break;
    }
    
    return series;
}

double BarChartWidget::aggregateValues(const std::vector<double>& values, 
                                      BarChartConfig::AggregationMethod method) const {
    if (values.empty()) return 0.0;
    
    switch (method) {
        case BarChartConfig::AggregationMethod::Last:
            return values.back();
            
        case BarChartConfig::AggregationMethod::Sum:
            return std::accumulate(values.begin(), values.end(), 0.0);
            
        case BarChartConfig::AggregationMethod::Average:
            return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
            
        case BarChartConfig::AggregationMethod::Count:
            return static_cast<double>(values.size());
            
        case BarChartConfig::AggregationMethod::Min:
            return *std::min_element(values.begin(), values.end());
            
        case BarChartConfig::AggregationMethod::Max:
            return *std::max_element(values.begin(), values.end());
    }
    
    return 0.0;
}