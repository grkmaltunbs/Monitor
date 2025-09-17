#include "grid_widget.h"
#include "../../logging/logger.h"
#include "../../profiling/profiler.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QColorDialog>
#include <QHeaderView>
#include <QScrollBar>
#include <QTextStream>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QEasingCurve>
#include <QStyledItemDelegate>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QStyle>

GridWidget::GridWidget(const QString& widgetId, QWidget* parent)
    : DisplayWidget(widgetId, "Grid Widget", parent)
    , m_table(nullptr)
    , m_mainLayout(nullptr)
    , m_lastSortColumn(-1)
    , m_lastSortOrder(Qt::AscendingOrder)
    , m_delayedUpdateTimer(new QTimer(this))
    , m_updatePending(false)
    , m_editDisplayNameAction(nullptr)
    , m_setIconAction(nullptr)
    , m_removeFieldAction(nullptr)
    , m_copyValueAction(nullptr)
    , m_showHistoryAction(nullptr)
    , m_toggleGridLinesAction(nullptr)
    , m_toggleRowColorsAction(nullptr)
    , m_resetColumnWidthsAction(nullptr)
    , m_exportClipboardAction(nullptr)
    , m_exportFileAction(nullptr)
    , m_lastTableUpdate(std::chrono::steady_clock::now())
    , m_updateCount(0)
    , m_maxVisibleRows(1000)
    , m_dropIndicator(nullptr)
    , m_showDropIndicator(false)
{
    PROFILE_SCOPE("GridWidget::constructor");
    
    setupLayout();
    setupTable();
    setupConnections();
    setupContextMenu();
    
    // Enable drag and drop
    setAcceptDrops(true);
    
    // Setup delayed update timer
    m_delayedUpdateTimer->setSingleShot(true);
    m_delayedUpdateTimer->setInterval(16); // ~60 FPS
    connect(m_delayedUpdateTimer, &QTimer::timeout, this, &GridWidget::updateVisibleRows);
    
    Monitor::Logging::Logger::instance()->debug("GridWidget", 
        QString("Grid widget '%1' created").arg(widgetId));
}

GridWidget::~GridWidget() {
    PROFILE_SCOPE("GridWidget::destructor");
    
    // Cleanup handled by Qt's parent-child relationship and RAII
    
    Monitor::Logging::Logger::instance()->debug("GridWidget", 
        QString("Grid widget '%1' destroyed").arg(getWidgetId()));
}

void GridWidget::setGridOptions(const GridOptions& options) {
    if (m_gridOptions.showGridLines != options.showGridLines ||
        m_gridOptions.alternatingRowColors != options.alternatingRowColors ||
        m_gridOptions.sortingEnabled != options.sortingEnabled) {
        
        m_gridOptions = options;
        applyGridOptions();
        
        emit gridOptionsChanged();
    }
}

void GridWidget::setFieldDisplayName(const QString& fieldPath, const QString& displayName) {
    m_customDisplayNames[fieldPath] = displayName;
    
    // Update table if field exists
    FieldRow* row = findFieldRow(fieldPath);
    if (row && row->nameItem) {
        row->nameItem->setText(displayName.isEmpty() ? formatFieldName(fieldPath) : displayName);
    }
    
    Monitor::Logging::Logger::instance()->debug("GridWidget", 
        QString("Display name for field '%1' set to '%2'").arg(fieldPath).arg(displayName));
}

QString GridWidget::getFieldDisplayName(const QString& fieldPath) const {
    return m_customDisplayNames.value(fieldPath, formatFieldName(fieldPath));
}

void GridWidget::setFieldIcon(const QString& fieldPath, const QIcon& icon) {
    m_customIcons[fieldPath] = icon;
    
    // Update table if field exists
    FieldRow* row = findFieldRow(fieldPath);
    if (row && row->nameItem) {
        row->nameItem->setIcon(icon.isNull() ? getFieldTypeIcon(fieldPath) : icon);
    }
}

QIcon GridWidget::getFieldIcon(const QString& fieldPath) const {
    return m_customIcons.value(fieldPath, getFieldTypeIcon(fieldPath));
}

void GridWidget::setColumnWidth(int column, int width) {
    if (m_table && column >= 0 && column < m_table->columnCount()) {
        m_table->setColumnWidth(column, width);
    }
}

int GridWidget::getColumnWidth(int column) const {
    if (m_table && column >= 0 && column < m_table->columnCount()) {
        return m_table->columnWidth(column);
    }
    return 0;
}

void GridWidget::resetColumnWidths() {
    if (m_table) {
        m_table->resizeColumnsToContents();
        
        // Ensure minimum widths
        int nameMinWidth = 150;
        int valueMinWidth = 100;
        
        if (m_table->columnWidth(0) < nameMinWidth) {
            m_table->setColumnWidth(0, nameMinWidth);
        }
        if (m_table->columnWidth(1) < valueMinWidth) {
            m_table->setColumnWidth(1, valueMinWidth);
        }
    }
}

void GridWidget::setRowHeight(int height) {
    if (m_table) {
        m_table->verticalHeader()->setDefaultSectionSize(height);
    }
}

int GridWidget::getRowHeight() const {
    return m_table ? m_table->verticalHeader()->defaultSectionSize() : 0;
}

void GridWidget::setAlternatingRowColors(bool enabled) {
    m_gridOptions.alternatingRowColors = enabled;
    if (m_table) {
        m_table->setAlternatingRowColors(enabled);
    }
}

bool GridWidget::hasAlternatingRowColors() const {
    return m_gridOptions.alternatingRowColors;
}

void GridWidget::setSortingEnabled(bool enabled) {
    m_gridOptions.sortingEnabled = enabled;
    if (m_table) {
        m_table->setSortingEnabled(enabled);
    }
}

bool GridWidget::isSortingEnabled() const {
    return m_gridOptions.sortingEnabled;
}

void GridWidget::sortByColumn(int column, Qt::SortOrder order) {
    if (m_table && column >= 0 && column < m_table->columnCount()) {
        m_lastSortColumn = column;
        m_lastSortOrder = order;
        m_table->sortItems(column, order);
    }
}

void GridWidget::clearSort() {
    m_lastSortColumn = -1;
    refreshTableStructure();
}

void GridWidget::refreshGrid() {
    PROFILE_SCOPE("GridWidget::refreshGrid");
    
    refreshTableStructure();
    updateVisibleRows();
    
    Monitor::Logging::Logger::instance()->debug("GridWidget", 
        QString("Grid widget '%1' refreshed").arg(getWidgetId()));
}

void GridWidget::resizeColumnsToContents() {
    if (m_table) {
        m_table->resizeColumnsToContents();
    }
}

void GridWidget::selectField(const QString& fieldPath) {
    FieldRow* row = findFieldRow(fieldPath);
    if (row && row->row >= 0) {
        m_table->selectRow(row->row);
        ensureRowVisible(row->row);
        emit fieldSelected(fieldPath);
    }
}

void GridWidget::scrollToField(const QString& fieldPath) {
    FieldRow* row = findFieldRow(fieldPath);
    if (row && row->row >= 0) {
        ensureRowVisible(row->row);
    }
}

void GridWidget::updateFieldDisplay(const QString& fieldPath, const QVariant& value) {
    PROFILE_SCOPE("GridWidget::updateFieldDisplay");
    
    QMutexLocker locker(&m_fieldRowMutex);
    
    FieldRow* row = findFieldRow(fieldPath);
    if (!row) {
        // Create new row for field
        int rowIndex = addFieldRow(fieldPath);
        Q_UNUSED(rowIndex); // Used for row creation, index not needed afterward
        row = findFieldRow(fieldPath);
        if (!row) {
            Monitor::Logging::Logger::instance()->error("GridWidget", 
                QString("Failed to create row for field '%1'").arg(fieldPath));
            return;
        }
    }
    
    updateFieldRow(fieldPath, value);
    
    // Schedule delayed update if many updates are happening
    if (!m_updatePending) {
        m_updatePending = true;
        scheduleDelayedUpdate();
    }
    
    m_updateCount++;
}

void GridWidget::clearFieldDisplay(const QString& fieldPath) {
    QMutexLocker locker(&m_fieldRowMutex);
    removeFieldRow(fieldPath);
}

void GridWidget::refreshAllDisplays() {
    PROFILE_SCOPE("GridWidget::refreshAllDisplays");
    
    QMutexLocker locker(&m_fieldRowMutex);
    
    // Clear all rows
    m_table->setRowCount(0);
    m_fieldRows.clear();
    m_rowToField.clear();
    
    // Recreate rows for all assigned fields
    QStringList fields = getAssignedFields();
    for (const QString& fieldPath : fields) {
        addFieldRow(fieldPath);
    }
    
    // Update table structure
    refreshTableStructure();
}

QJsonObject GridWidget::saveWidgetSpecificSettings() const {
    QJsonObject settings = DisplayWidget::saveWidgetSpecificSettings();
    
    // Grid options
    QJsonObject gridOptions;
    gridOptions["showGridLines"] = m_gridOptions.showGridLines;
    gridOptions["alternatingRowColors"] = m_gridOptions.alternatingRowColors;
    gridOptions["sortingEnabled"] = m_gridOptions.sortingEnabled;
    gridOptions["resizableColumns"] = m_gridOptions.resizableColumns;
    gridOptions["showFieldIcons"] = m_gridOptions.showFieldIcons;
    gridOptions["animateValueChanges"] = m_gridOptions.animateValueChanges;
    gridOptions["valueChangeHighlightDuration"] = m_gridOptions.valueChangeHighlightDuration;
    gridOptions["highlightColor"] = m_gridOptions.highlightColor.name();
    settings["gridOptions"] = gridOptions;
    
    // Column widths
    if (m_table && m_table->columnCount() > 0) {
        QJsonObject columnWidths;
        for (int i = 0; i < m_table->columnCount(); ++i) {
            columnWidths[QString::number(i)] = m_table->columnWidth(i);
        }
        settings["columnWidths"] = columnWidths;
    }
    
    // Custom display names
    if (!m_customDisplayNames.isEmpty()) {
        QJsonObject displayNames;
        for (auto it = m_customDisplayNames.begin(); it != m_customDisplayNames.end(); ++it) {
            displayNames[it.key()] = it.value();
        }
        settings["customDisplayNames"] = displayNames;
    }
    
    // Sorting state
    if (m_lastSortColumn >= 0) {
        settings["sortColumn"] = m_lastSortColumn;
        settings["sortOrder"] = static_cast<int>(m_lastSortOrder);
    }
    
    return settings;
}

bool GridWidget::restoreWidgetSpecificSettings(const QJsonObject& settings) {
    if (!DisplayWidget::restoreWidgetSpecificSettings(settings)) {
        return false;
    }
    
    // Restore grid options
    QJsonObject gridOptions = settings.value("gridOptions").toObject();
    if (!gridOptions.isEmpty()) {
        GridOptions options;
        QJsonValue showGridLinesValue = gridOptions.value("showGridLines");
        options.showGridLines = showGridLinesValue.isUndefined() ? true : showGridLinesValue.toBool();
        
        QJsonValue alternatingRowColorsValue = gridOptions.value("alternatingRowColors");
        options.alternatingRowColors = alternatingRowColorsValue.isUndefined() ? true : alternatingRowColorsValue.toBool();
        
        QJsonValue sortingEnabledValue = gridOptions.value("sortingEnabled");
        options.sortingEnabled = sortingEnabledValue.isUndefined() ? true : sortingEnabledValue.toBool();
        
        QJsonValue resizableColumnsValue = gridOptions.value("resizableColumns");
        options.resizableColumns = resizableColumnsValue.isUndefined() ? true : resizableColumnsValue.toBool();
        
        QJsonValue showFieldIconsValue = gridOptions.value("showFieldIcons");
        options.showFieldIcons = showFieldIconsValue.isUndefined() ? true : showFieldIconsValue.toBool();
        
        QJsonValue animateValueChangesValue = gridOptions.value("animateValueChanges");
        options.animateValueChanges = animateValueChangesValue.isUndefined() ? true : animateValueChangesValue.toBool();
        
        QJsonValue highlightDurationValue = gridOptions.value("valueChangeHighlightDuration");
        options.valueChangeHighlightDuration = highlightDurationValue.isUndefined() ? 1000 : highlightDurationValue.toInt();
        
        QJsonValue highlightColorValue = gridOptions.value("highlightColor");
        options.highlightColor = QColor(highlightColorValue.isUndefined() ? "#FFFF64" : highlightColorValue.toString());
        setGridOptions(options);
    }
    
    // Restore column widths
    QJsonObject columnWidths = settings.value("columnWidths").toObject();
    if (!columnWidths.isEmpty() && m_table) {
        for (auto it = columnWidths.begin(); it != columnWidths.end(); ++it) {
            int column = it.key().toInt();
            int width = it.value().toInt();
            setColumnWidth(column, width);
        }
    }
    
    // Restore custom display names
    QJsonObject displayNames = settings.value("customDisplayNames").toObject();
    m_customDisplayNames.clear();
    for (auto it = displayNames.begin(); it != displayNames.end(); ++it) {
        m_customDisplayNames[it.key()] = it.value().toString();
    }
    
    // Restore sorting
    if (settings.contains("sortColumn")) {
        int sortColumn = settings.value("sortColumn").toInt();
        Qt::SortOrder sortOrder = static_cast<Qt::SortOrder>(settings.value("sortOrder").toInt());
        QTimer::singleShot(100, this, [this, sortColumn, sortOrder]() {
            sortByColumn(sortColumn, sortOrder);
        });
    }
    
    return true;
}

void GridWidget::setupContextMenu() {
    DisplayWidget::setupContextMenu();
    
    if (!m_editDisplayNameAction) {
        getContextMenu()->addSeparator();
        
        m_editDisplayNameAction = getContextMenu()->addAction("Edit Field Name...", 
            this, &GridWidget::onEditFieldDisplayName);
        m_setIconAction = getContextMenu()->addAction("Set Field Icon...", 
            this, &GridWidget::onSetFieldIcon);
        m_removeFieldAction = getContextMenu()->addAction("Remove Field", 
            this, &GridWidget::onRemoveSelectedField);
        
        getContextMenu()->addSeparator();
        
        m_copyValueAction = getContextMenu()->addAction("Copy Value", 
            this, &GridWidget::onCopyFieldValue);
        m_showHistoryAction = getContextMenu()->addAction("Show History...", 
            this, &GridWidget::onShowFieldHistory);
        
        getContextMenu()->addSeparator();
        
        m_toggleGridLinesAction = getContextMenu()->addAction("Toggle Grid Lines", 
            this, &GridWidget::onToggleGridLines);
        m_toggleGridLinesAction->setCheckable(true);
        m_toggleGridLinesAction->setChecked(m_gridOptions.showGridLines);
        
        m_toggleRowColorsAction = getContextMenu()->addAction("Toggle Row Colors", 
            this, &GridWidget::onToggleRowColors);
        m_toggleRowColorsAction->setCheckable(true);
        m_toggleRowColorsAction->setChecked(m_gridOptions.alternatingRowColors);
        
        m_resetColumnWidthsAction = getContextMenu()->addAction("Reset Column Widths", 
            this, &GridWidget::onResetColumnWidths);
        
        getContextMenu()->addSeparator();
        
        m_exportClipboardAction = getContextMenu()->addAction("Export to Clipboard", 
            this, &GridWidget::onExportToClipboard);
        m_exportFileAction = getContextMenu()->addAction("Export to File...", 
            this, &GridWidget::onExportToFile);
    }
}

void GridWidget::resizeEvent(QResizeEvent* event) {
    DisplayWidget::resizeEvent(event);
    
    // Auto-resize columns if needed
    if (m_table && m_gridOptions.resizableColumns) {
        QTimer::singleShot(10, this, &GridWidget::updateVisibleRows);
    }
}

void GridWidget::showEvent(QShowEvent* event) {
    DisplayWidget::showEvent(event);
    
    // Initial column sizing
    QTimer::singleShot(100, this, &GridWidget::resetColumnWidths);
}

void GridWidget::paintEvent(QPaintEvent* event) {
    DisplayWidget::paintEvent(event);
    
    // Draw drop indicator if needed
    if (m_showDropIndicator && !m_dropPosition.isNull()) {
        QPainter painter(this);
        painter.setPen(QPen(QColor(0, 120, 215), 2));
        painter.drawLine(m_dropPosition.x() - 50, m_dropPosition.y(), 
                        m_dropPosition.x() + 50, m_dropPosition.y());
    }
}

void GridWidget::dragEnterEvent(QDragEnterEvent* event) {
    // Accept drops from the StructWindow
    if (event->mimeData()->hasFormat("application/x-monitor-field") ||
        event->mimeData()->hasFormat("application/json")) {
        event->acceptProposedAction();
        m_showDropIndicator = true;
        m_dropPosition = event->position().toPoint();
        update();
    } else {
        event->ignore();
    }
}

void GridWidget::dragMoveEvent(QDragMoveEvent* event) {
    // Accept moves from the StructWindow
    if (event->mimeData()->hasFormat("application/x-monitor-field") ||
        event->mimeData()->hasFormat("application/json")) {
        event->acceptProposedAction();
        m_dropPosition = event->position().toPoint();
        update();
    } else {
        event->ignore();
    }
}

void GridWidget::dropEvent(QDropEvent* event) {
    m_showDropIndicator = false;
    m_dropPosition = QPoint();
    update();
    
    // Handle the dropped data
    if (event->mimeData()->hasFormat("application/x-monitor-field")) {
        QString fieldPath = QString::fromUtf8(event->mimeData()->data("application/x-monitor-field"));
        
        // Also get the JSON data if available
        QJsonObject fieldData;
        if (event->mimeData()->hasFormat("application/json")) {
            QJsonDocument doc = QJsonDocument::fromJson(event->mimeData()->data("application/json"));
            fieldData = doc.object();
        }
        
        handleDroppedField(fieldPath, fieldData);
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void GridWidget::setupTable() {
    m_table = new QTableWidget(0, 2, this);
    
    // Set headers
    QStringList headers;
    headers << "Field" << "Value";
    m_table->setHorizontalHeaderLabels(headers);
    
    // Configure table appearance
    setupTableAppearance();
    
    // Configure selection behavior
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    
    // Configure context menu
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Hide vertical header (row numbers)
    m_table->verticalHeader()->hide();
    
    // Configure horizontal header
    QHeaderView* headerView = m_table->horizontalHeader();
    headerView->setStretchLastSection(true);
    headerView->setSectionResizeMode(0, QHeaderView::Interactive);
    headerView->setSectionResizeMode(1, QHeaderView::Stretch);
    
    // Set minimum column widths
    m_table->setColumnWidth(0, 150); // Field name column
    
    // Apply grid options
    applyGridOptions();
}

void GridWidget::setupLayout() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(2, 2, 2, 2);
    m_mainLayout->setSpacing(0);
    
    setLayout(m_mainLayout);
}

void GridWidget::setupConnections() {
    if (m_table) {
        connect(m_table, &QTableWidget::cellClicked, 
                this, &GridWidget::onCellClicked);
        connect(m_table, &QTableWidget::cellDoubleClicked, 
                this, &GridWidget::onCellDoubleClicked);
        connect(m_table, &QTableWidget::customContextMenuRequested, 
                this, &GridWidget::onCustomContextMenuRequested);
        connect(m_table, &QTableWidget::itemChanged, 
                this, &GridWidget::onItemChanged);
        
        // Header connections
        connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked, 
                this, &GridWidget::onHeaderClicked);
        
        // Scroll connections for performance optimization
        connect(m_table->verticalScrollBar(), &QScrollBar::valueChanged, 
                this, &GridWidget::onVerticalScrollChanged);
        connect(m_table->horizontalScrollBar(), &QScrollBar::valueChanged, 
                this, &GridWidget::onHorizontalScrollChanged);
    }
}

void GridWidget::setupTableAppearance() {
    if (!m_table) return;
    
    // Set default style with better contrast
    m_table->setStyleSheet(
        "QTableWidget {"
        "    gridline-color: #b0b0b0;"
        "    background-color: white;"
        "    alternate-background-color: #f8f8f8;"
        "    color: #000000;"  // Ensure black text
        "}"
        "QTableWidget::item {"
        "    padding: 4px;"
        "    border: none;"
        "    color: #000000;"  // Black text for all items
        "    background-color: white;"
        "}"
        "QTableWidget::item:alternate {"
        "    background-color: #f0f0f0;"  // Slightly darker alternate rows
        "}"
        "QTableWidget::item:selected {"
        "    background-color: #0078d4;"
        "    color: white;"
        "}"
        "QHeaderView::section {"
        "    background-color: #e0e0e0;"  // Darker header background
        "    color: #000000;"  // Black text for headers
        "    padding: 6px;"
        "    border: 1px solid #b0b0b0;"
        "    font-weight: bold;"
        "}"
    );
    
    // Set row height
    setRowHeight(24);
}

int GridWidget::addFieldRow(const QString& fieldPath) {
    if (m_fieldRows.contains(fieldPath)) {
        return m_fieldRows[fieldPath].row;
    }
    
    int row = m_table->rowCount();
    m_table->insertRow(row);
    
    // Create items
    QString displayName = getFieldDisplayName(fieldPath);
    QTableWidgetItem* nameItem = createFieldNameItem(fieldPath, displayName);
    QTableWidgetItem* valueItem = createFieldValueItem(QVariant(), getDisplayConfig(fieldPath));
    
    m_table->setItem(row, 0, nameItem);
    m_table->setItem(row, 1, valueItem);
    
    // Store field row information
    FieldRow fieldRow(row, nameItem, valueItem);
    m_fieldRows[fieldPath] = fieldRow;
    m_rowToField[row] = fieldPath;
    
    // Update row appearance
    updateRowAppearance(row);
    
    Monitor::Logging::Logger::instance()->debug("GridWidget", 
        QString("Added row %1 for field '%2'").arg(row).arg(fieldPath));
    
    return row;
}

void GridWidget::removeFieldRow(const QString& fieldPath) {
    auto it = m_fieldRows.find(fieldPath);
    if (it == m_fieldRows.end()) {
        return;
    }
    
    int row = it->row;
    
    // Remove from table
    m_table->removeRow(row);
    
    // Clean up tracking data
    m_fieldRows.erase(it);
    m_rowToField.remove(row);
    
    // Update row indices for remaining rows
    for (auto& fieldRow : m_fieldRows) {
        if (fieldRow.row > row) {
            fieldRow.row--;
        }
    }
    
    // Update row to field mapping
    QHash<int, QString> newRowToField;
    for (auto it = m_rowToField.begin(); it != m_rowToField.end(); ++it) {
        int rowIndex = it.key();
        if (rowIndex > row) {
            newRowToField[rowIndex - 1] = it.value();
        } else {
            newRowToField[rowIndex] = it.value();
        }
    }
    m_rowToField = newRowToField;
    
    Monitor::Logging::Logger::instance()->debug("GridWidget", 
        QString("Removed row for field '%1'").arg(fieldPath));
}

void GridWidget::updateFieldRow(const QString& fieldPath, const QVariant& value) {
    FieldRow* row = findFieldRow(fieldPath);
    if (!row || !row->valueItem) {
        return;
    }
    
    // Check if value actually changed
    if (row->lastValue == value) {
        return;
    }
    
    // Format the value
    QString formattedValue = formatValue(value, getDisplayConfig(fieldPath));
    
    // Update the item
    row->valueItem->setText(formattedValue);
    row->valueItem->setToolTip(formattedValue);
    
    // Store last value and timestamp
    row->lastValue = value;
    row->lastUpdate = std::chrono::steady_clock::now();
    
    // Animate value change if enabled
    if (m_gridOptions.animateValueChanges) {
        animateValueChange(row, value);
    }
    
    // Update row appearance
    updateRowAppearance(row->row);
}

GridWidget::FieldRow* GridWidget::findFieldRow(const QString& fieldPath) {
    auto it = m_fieldRows.find(fieldPath);
    return (it != m_fieldRows.end()) ? &it.value() : nullptr;
}

const GridWidget::FieldRow* GridWidget::findFieldRow(const QString& fieldPath) const {
    auto it = m_fieldRows.find(fieldPath);
    return (it != m_fieldRows.end()) ? &it.value() : nullptr;
}

void GridWidget::animateValueChange(FieldRow* row, const QVariant& newValue) {
    Q_UNUSED(newValue); // Value highlighting doesn't need the actual new value
    if (!row || !row->valueItem) {
        return;
    }
    
    // Simple highlight effect using background color change and QTimer
    if (m_gridOptions.animateValueChanges && row->valueItem) {
        // Set highlight color immediately
        row->valueItem->setBackground(QBrush(m_gridOptions.highlightColor));
        
        // Use QTimer to restore normal color after duration
        QTimer::singleShot(m_gridOptions.valueChangeHighlightDuration, [row]() {
            if (row && row->valueItem) {
                row->valueItem->setBackground(QBrush()); // Reset to default background
            }
        });
    }
}

void GridWidget::highlightRow(int row, const QColor& color, int duration) {
    if (!m_table || row < 0 || row >= m_table->rowCount()) {
        return;
    }
    
    // Highlight both items in the row
    for (int col = 0; col < m_table->columnCount(); ++col) {
        QTableWidgetItem* item = m_table->item(row, col);
        if (item) {
            item->setBackground(color);
        }
    }
    
    // Fade back to normal
    QTimer::singleShot(duration, this, [this, row]() {
        if (m_table && row >= 0 && row < m_table->rowCount()) {
            for (int col = 0; col < m_table->columnCount(); ++col) {
                QTableWidgetItem* item = m_table->item(row, col);
                if (item) {
                    item->setBackground(QBrush());
                }
            }
        }
    });
}

QIcon GridWidget::getFieldTypeIcon(const QString& fieldPath) const {
    // Get field type information
    QString typeName;
    const FieldAssignment* assignment = findFieldAssignment(fieldPath);
    if (assignment) {
        typeName = assignment->typeName.toLower();
    } else {
        // For Phase 6 testing, provide default type for fields without assignment
        typeName = "unknown";
    }
    
    // Create simple colored icon based on type
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QColor typeColor;
    if (typeName.contains("int")) {
        typeColor = QColor(70, 130, 180);  // Steel blue for integers
    } else if (typeName.contains("float") || typeName.contains("double")) {
        typeColor = QColor(255, 140, 0);   // Dark orange for floats
    } else if (typeName.contains("bool")) {
        typeColor = QColor(50, 205, 50);   // Lime green for booleans
    } else if (typeName.contains("char") || typeName.contains("string")) {
        typeColor = QColor(220, 20, 60);   // Crimson for strings
    } else {
        typeColor = QColor(128, 128, 128); // Gray for unknown types
    }
    
    painter.setBrush(typeColor);
    painter.setPen(typeColor.darker());
    painter.drawEllipse(2, 2, 12, 12);
    
    return QIcon(pixmap);
}

void GridWidget::updateRowAppearance(int row) {
    if (!m_table || row < 0 || row >= m_table->rowCount()) {
        return;
    }
    
    // Update alternating row colors if enabled
    if (m_gridOptions.alternatingRowColors) {
        QColor bgColor = (row % 2 == 0) ? QColor(255, 255, 255) : QColor(247, 247, 247);
        for (int col = 0; col < m_table->columnCount(); ++col) {
            QTableWidgetItem* item = m_table->item(row, col);
            if (item && item->background().color().alpha() == 0) {
                item->setBackground(bgColor);
            }
        }
    }
}

void GridWidget::refreshTableStructure() {
    if (!m_table) return;
    
    PROFILE_SCOPE("GridWidget::refreshTableStructure");
    
    // Update headers
    updateTableHeaders();
    
    // Apply current options
    applyGridOptions();
    
    // Resize columns
    m_table->resizeColumnsToContents();
    
    // Restore sorting if it was active
    if (m_lastSortColumn >= 0) {
        m_table->sortItems(m_lastSortColumn, m_lastSortOrder);
    }
}

void GridWidget::updateTableHeaders() {
    if (m_table) {
        QStringList headers;
        headers << "Field" << "Value";
        m_table->setHorizontalHeaderLabels(headers);
    }
}

void GridWidget::applyGridOptions() {
    if (!m_table) return;
    
    m_table->setShowGrid(m_gridOptions.showGridLines);
    m_table->setAlternatingRowColors(m_gridOptions.alternatingRowColors);
    m_table->setSortingEnabled(m_gridOptions.sortingEnabled);
    
    // Update column resize mode
    QHeaderView* header = m_table->horizontalHeader();
    if (m_gridOptions.resizableColumns) {
        header->setSectionResizeMode(0, QHeaderView::Interactive);
        header->setSectionResizeMode(1, QHeaderView::Stretch);
    } else {
        header->setSectionResizeMode(QHeaderView::Fixed);
    }
}

void GridWidget::optimizeTableDisplay() {
    // This could implement view culling for very large datasets
    updateVisibleRows();
}

void GridWidget::updateVisibleRows() {
    m_updatePending = false;
    m_lastTableUpdate = std::chrono::steady_clock::now();
    
    // For now, all rows are always updated
    // In the future, this could implement viewport culling
    
    if (m_table) {
        m_table->viewport()->update();
    }
}

bool GridWidget::isRowVisible(int row) const {
    if (!m_table || row < 0 || row >= m_table->rowCount()) {
        return false;
    }
    
    QRect visibleRect = m_table->viewport()->rect();
    QRect rowRect = m_table->visualRect(m_table->model()->index(row, 0));
    
    return visibleRect.intersects(rowRect);
}

void GridWidget::scheduleDelayedUpdate() {
    if (!m_delayedUpdateTimer->isActive()) {
        m_delayedUpdateTimer->start();
    }
}

QString GridWidget::getFieldNameFromRow(int row) const {
    return m_rowToField.value(row, QString());
}

QString GridWidget::formatFieldName(const QString& fieldPath) const {
    // Extract the last component after the last dot
    int lastDot = fieldPath.lastIndexOf('.');
    return (lastDot >= 0) ? fieldPath.mid(lastDot + 1) : fieldPath;
}

void GridWidget::ensureRowVisible(int row) {
    if (m_table && row >= 0 && row < m_table->rowCount()) {
        m_table->scrollToItem(m_table->item(row, 0));
    }
}

QTableWidgetItem* GridWidget::createFieldNameItem(const QString& fieldPath, const QString& displayName) {
    auto* item = new GridTableItem(displayName);
    item->setFieldPath(fieldPath);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    
    // Set field name specific styling for better contrast
    QFont font = item->font();
    font.setBold(true);  // Make field names bold
    item->setFont(font);
    item->setBackground(QBrush(QColor(240, 240, 240)));  // Light gray background for field names
    item->setForeground(QBrush(QColor(0, 0, 0)));  // Ensure black text
    
    // Set icon if enabled
    if (m_gridOptions.showFieldIcons) {
        item->setIcon(getFieldIcon(fieldPath));
    }
    
    // Set tooltip
    item->setToolTip(QString("Field: %1\nPath: %2").arg(displayName).arg(fieldPath));
    
    return item;
}

QTableWidgetItem* GridWidget::createFieldValueItem(const QVariant& value, const DisplayConfig& config) {
    QString formattedValue = formatValue(value, config);
    auto* item = new GridTableItem(formattedValue);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    
    // Ensure good contrast for values
    item->setForeground(QBrush(QColor(0, 0, 0)));  // Black text for values
    item->setBackground(QBrush(QColor(255, 255, 255)));  // White background for values
    
    // Set tooltip
    item->setToolTip(formattedValue);
    
    return item;
}

// Slot implementations
void GridWidget::onCellClicked(int row, int column) {
    Q_UNUSED(column)
    
    QString fieldPath = getFieldNameFromRow(row);
    if (!fieldPath.isEmpty()) {
        emit fieldSelected(fieldPath);
    }
}

void GridWidget::onCellDoubleClicked(int row, int column) {
    Q_UNUSED(column)
    
    QString fieldPath = getFieldNameFromRow(row);
    if (!fieldPath.isEmpty()) {
        emit fieldDoubleClicked(fieldPath);
    }
}

void GridWidget::onHeaderClicked(int logicalIndex) {
    if (m_gridOptions.sortingEnabled) {
        Qt::SortOrder order = (m_lastSortColumn == logicalIndex && m_lastSortOrder == Qt::AscendingOrder) 
                              ? Qt::DescendingOrder : Qt::AscendingOrder;
        sortByColumn(logicalIndex, order);
    }
}

void GridWidget::onCustomContextMenuRequested(const QPoint& pos) {
    QTableWidgetItem* item = m_table->itemAt(pos);
    if (item) {
        // Enable field-specific actions
        m_editDisplayNameAction->setEnabled(true);
        m_setIconAction->setEnabled(true);
        m_removeFieldAction->setEnabled(true);
        m_copyValueAction->setEnabled(true);
        m_showHistoryAction->setEnabled(true);
    } else {
        // Disable field-specific actions
        m_editDisplayNameAction->setEnabled(false);
        m_setIconAction->setEnabled(false);
        m_removeFieldAction->setEnabled(false);
        m_copyValueAction->setEnabled(false);
        m_showHistoryAction->setEnabled(false);
    }
    
    showContextMenu(m_table->mapToParent(pos));
}

void GridWidget::onItemChanged(QTableWidgetItem* item) {
    // Handle any item changes if needed
    Q_UNUSED(item)
}

void GridWidget::onVerticalScrollChanged(int value) {
    Q_UNUSED(value)
    scheduleDelayedUpdate();
}

void GridWidget::onHorizontalScrollChanged(int value) {
    Q_UNUSED(value)
    scheduleDelayedUpdate();
}

// Context menu action implementations
void GridWidget::onEditFieldDisplayName() {
    int currentRow = m_table->currentRow();
    QString fieldPath = getFieldNameFromRow(currentRow);
    
    if (fieldPath.isEmpty()) return;
    
    QString currentName = getFieldDisplayName(fieldPath);
    bool ok;
    QString newName = QInputDialog::getText(this, "Edit Field Name",
                                           "Field display name:", QLineEdit::Normal,
                                           currentName, &ok);
    
    if (ok && !newName.isEmpty() && newName != currentName) {
        setFieldDisplayName(fieldPath, newName);
    }
}

void GridWidget::onSetFieldIcon() {
    // This would open an icon selection dialog
    QMessageBox::information(this, "Set Field Icon", "Icon selection dialog would be implemented here.");
}

void GridWidget::onRemoveSelectedField() {
    int currentRow = m_table->currentRow();
    QString fieldPath = getFieldNameFromRow(currentRow);
    
    if (!fieldPath.isEmpty()) {
        removeField(fieldPath);
    }
}

void GridWidget::onCopyFieldValue() {
    int currentRow = m_table->currentRow();
    if (currentRow >= 0 && m_table->columnCount() > 1) {
        QTableWidgetItem* valueItem = m_table->item(currentRow, 1);
        if (valueItem) {
            QApplication::clipboard()->setText(valueItem->text());
        }
    }
}

void GridWidget::onShowFieldHistory() {
    QMessageBox::information(this, "Field History", "Field history dialog would be implemented here.");
}

void GridWidget::onToggleGridLines() {
    m_gridOptions.showGridLines = !m_gridOptions.showGridLines;
    m_table->setShowGrid(m_gridOptions.showGridLines);
    m_toggleGridLinesAction->setChecked(m_gridOptions.showGridLines);
}

void GridWidget::onToggleRowColors() {
    m_gridOptions.alternatingRowColors = !m_gridOptions.alternatingRowColors;
    setAlternatingRowColors(m_gridOptions.alternatingRowColors);
    m_toggleRowColorsAction->setChecked(m_gridOptions.alternatingRowColors);
}

void GridWidget::onResetColumnWidths() {
    resetColumnWidths();
}

void GridWidget::onExportToClipboard() {
    if (!m_table) return;
    
    QString text;
    QTextStream stream(&text);
    
    // Add headers
    stream << "Field\tValue\n";
    
    // Add data
    for (int row = 0; row < m_table->rowCount(); ++row) {
        for (int col = 0; col < m_table->columnCount(); ++col) {
            QTableWidgetItem* item = m_table->item(row, col);
            if (item) {
                stream << item->text();
            }
            if (col < m_table->columnCount() - 1) {
                stream << "\t";
            }
        }
        stream << "\n";
    }
    
    QApplication::clipboard()->setText(text);
    
    Monitor::Logging::Logger::instance()->info("GridWidget", 
        QString("Grid data exported to clipboard (%1 rows)").arg(m_table->rowCount()));
}

void GridWidget::onExportToFile() {
    QString fileName = QFileDialog::getSaveFileName(this, "Export Grid Data",
                                                   QString("grid_export_%1.csv").arg(getWidgetId()),
                                                   "CSV Files (*.csv);;Text Files (*.txt)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Error", 
                           QString("Could not write to file: %1").arg(fileName));
        return;
    }
    
    QTextStream stream(&file);
    
    // Add headers
    stream << "Field,Value\n";
    
    // Add data
    for (int row = 0; row < m_table->rowCount(); ++row) {
        for (int col = 0; col < m_table->columnCount(); ++col) {
            QTableWidgetItem* item = m_table->item(row, col);
            if (item) {
                QString text = item->text();
                // Escape commas and quotes for CSV
                if (text.contains(',') || text.contains('"')) {
                    text = QString("\"%1\"").arg(text.replace('"', "\"\""));
                }
                stream << text;
            }
            if (col < m_table->columnCount() - 1) {
                stream << ",";
            }
        }
        stream << "\n";
    }
    
    Monitor::Logging::Logger::instance()->info("GridWidget", 
        QString("Grid data exported to file: %1 (%2 rows)").arg(fileName).arg(m_table->rowCount()));
}

// Add the table to layout in initializeWidget
void GridWidget::initializeWidget() {
    DisplayWidget::initializeWidget();
    
    if (m_table && m_mainLayout) {
        m_mainLayout->addWidget(m_table);
    }
}

// GridTableItem implementation
GridTableItem::GridTableItem(const QString& text, int type)
    : QTableWidgetItem(text, type)
    , m_timestamp(std::chrono::steady_clock::now())
{
}

bool GridTableItem::operator<(const QTableWidgetItem& other) const {
    // Try to convert to numbers for better sorting
    bool ok1, ok2;
    double val1 = text().toDouble(&ok1);
    double val2 = other.text().toDouble(&ok2);
    
    if (ok1 && ok2) {
        return val1 < val2;
    }
    
    // Fall back to string comparison
    return text() < other.text();
}

void GridWidget::handleDroppedField(const QString& fieldPath, const QJsonObject& fieldData) {
    PROFILE_SCOPE("GridWidget::handleDroppedField");
    
    // Check if this is a struct or a primitive field
    QString fieldType = fieldData.value("type").toString();
    
    if (fieldType == "struct" || fieldType == "packet struct") {
        // This is a struct - extract all primitive fields
        QStringList primitiveFields;
        extractPrimitiveFields(fieldData, fieldPath, primitiveFields);
        
        // Add each primitive field to the grid
        for (const QString& primFieldPath : primitiveFields) {
            // Use a default packet ID of 0 for now - this will be updated when packets arrive
            addField(primFieldPath, 0, QJsonObject());
        }
        
        Monitor::Logging::Logger::instance()->info("GridWidget",
            QString("Added %1 primitive fields from struct '%2'")
            .arg(primitiveFields.count()).arg(fieldPath));
    } else {
        // This is a primitive field - add it directly
        // Use a default packet ID of 0 for now - this will be updated when packets arrive
        addField(fieldPath, 0, QJsonObject());
        
        Monitor::Logging::Logger::instance()->info("GridWidget",
            QString("Added field '%1' to grid").arg(fieldPath));
    }
}

void GridWidget::extractPrimitiveFields(const QJsonObject& structData, const QString& basePath, 
                                       QStringList& primitiveFields) {
    // Get the fields/children of this struct
    QJsonArray fields = structData.value("fields").toArray();
    if (fields.isEmpty()) {
        fields = structData.value("children").toArray();
    }
    
    for (const QJsonValue& fieldValue : fields) {
        QJsonObject field = fieldValue.toObject();
        QString fieldName = field.value("name").toString();
        QString fieldType = field.value("type").toString();
        QString fieldPath = basePath.isEmpty() ? fieldName : basePath + "." + fieldName;
        
        // Check if this is a struct or a primitive type
        if (fieldType.contains("struct") && !fieldType.contains("*")) {
            // This is a nested struct - recurse into it
            extractPrimitiveFields(field, fieldPath, primitiveFields);
        } else if (!fieldType.isEmpty()) {
            // This is a primitive field - add it to the list
            primitiveFields.append(fieldPath);
        }
    }
}