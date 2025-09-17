#ifndef GRID_WIDGET_H
#define GRID_WIDGET_H

#include "display_widget.h"
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QFrame>
#include <QSplitter>
#include <QScrollArea>
#include <QPainter>
#include <QStyleOption>
#include <QTimer>
#include <QGraphicsEffect>
#include <QHash>
#include <QMutex>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief Grid widget for displaying field values in a two-column table format
 * 
 * The GridWidget displays packet field values in a grid layout with:
 * - Two columns: Field Name | Field Value
 * - Real-time value updates with visual feedback
 * - Support for all data transformations from DisplayWidget
 * - Drag-and-drop field assignment from StructWindow
 * - Context menu for individual field operations
 * - Configurable appearance and formatting
 * - Performance optimizations for high-frequency updates
 * 
 * Visual Features:
 * - Alternating row colors for better readability
 * - Value change highlighting with fade animation
 * - Sortable columns
 * - Resizable columns with persistence
 * - Optional grid lines
 * - Custom field icons based on data type
 * 
 * Performance Characteristics:
 * - Supports 100+ fields at 60 FPS
 * - Efficient item updates without full refresh
 * - Lazy loading for off-screen items
 * - Memory-efficient string formatting
 */
class GridWidget : public DisplayWidget
{
    Q_OBJECT

public:
    /**
     * @brief Grid display options
     */
    struct GridOptions {
        bool showGridLines = true;
        bool alternatingRowColors = true;
        bool sortingEnabled = true;
        bool resizableColumns = true;
        bool showFieldIcons = true;
        bool animateValueChanges = true;
        int valueChangeHighlightDuration = 1000; // ms
        QColor highlightColor = QColor(255, 255, 0, 100); // Light yellow
        
        GridOptions() = default;
    };

    /**
     * @brief Field row information for efficient updates
     */
    struct FieldRow {
        int row;                    // Row index in table
        QTableWidgetItem* nameItem; // Field name item
        QTableWidgetItem* valueItem; // Field value item
        QVariant lastValue;         // Last displayed value
        std::chrono::steady_clock::time_point lastUpdate;
        // Animation now handled with QTimer, no persistent animation object needed
        
        FieldRow() : row(-1), nameItem(nullptr), valueItem(nullptr), 
              lastUpdate(std::chrono::steady_clock::now()) {}
        FieldRow(int r, QTableWidgetItem* name, QTableWidgetItem* value) 
            : row(r), nameItem(name), valueItem(value), 
              lastUpdate(std::chrono::steady_clock::now()) {}
    };

    explicit GridWidget(const QString& widgetId, QWidget* parent = nullptr);
    ~GridWidget() override;

    // Grid-specific configuration
    void setGridOptions(const GridOptions& options);
    GridOptions getGridOptions() const { return m_gridOptions; }

    // Field management
    void setFieldDisplayName(const QString& fieldPath, const QString& displayName);
    QString getFieldDisplayName(const QString& fieldPath) const;
    void setFieldIcon(const QString& fieldPath, const QIcon& icon);
    QIcon getFieldIcon(const QString& fieldPath) const;

    // Column management
    void setColumnWidth(int column, int width);
    int getColumnWidth(int column) const;
    void resetColumnWidths();

    // Visual customization
    void setRowHeight(int height);
    int getRowHeight() const;
    void setAlternatingRowColors(bool enabled);
    bool hasAlternatingRowColors() const;

    // Sorting and filtering
    void setSortingEnabled(bool enabled);
    bool isSortingEnabled() const;
    void sortByColumn(int column, Qt::SortOrder order = Qt::AscendingOrder);
    void clearSort();

    // Test helpers
    QMenu* getContextMenuForTesting() const { return getContextMenu(); }
    void onExportToClipboardForTesting() { onExportToClipboard(); }
    bool restoreWidgetSpecificSettingsForTesting(const QJsonObject& settings) { return restoreWidgetSpecificSettings(settings); }

public slots:
    void refreshGrid();
    void resizeColumnsToContents();
    void selectField(const QString& fieldPath);
    void scrollToField(const QString& fieldPath);

signals:
    void fieldSelected(const QString& fieldPath);
    void fieldDoubleClicked(const QString& fieldPath);
    void gridOptionsChanged();

protected:
    // BaseWidget implementation
    void initializeWidget() override;
    
    // DisplayWidget implementation
    void updateFieldDisplay(const QString& fieldPath, const QVariant& value) override;
    void clearFieldDisplay(const QString& fieldPath) override;
    void refreshAllDisplays() override;

    // BaseWidget settings implementation
    QJsonObject saveWidgetSpecificSettings() const override;
    bool restoreWidgetSpecificSettings(const QJsonObject& settings) override;

    // Context menu implementation
    void setupContextMenu() override;

    // Event handling
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onCellClicked(int row, int column);
    void onCellDoubleClicked(int row, int column);
    void onHeaderClicked(int logicalIndex);
    void onCustomContextMenuRequested(const QPoint& pos);
    void onItemChanged(QTableWidgetItem* item);
    void onVerticalScrollChanged(int value);
    void onHorizontalScrollChanged(int value);
    
    // Field-specific actions
    void onEditFieldDisplayName();
    void onSetFieldIcon();
    void onRemoveSelectedField();
    void onCopyFieldValue();
    void onShowFieldHistory();

    // Grid actions
    void onToggleGridLines();
    void onToggleRowColors();
    void onResetColumnWidths();
    void onExportToClipboard();
    void onExportToFile();

private:
    // UI setup
    void setupTable();
    void setupLayout();
    void setupConnections();
    void setupTableAppearance();

    // Field row management
    int addFieldRow(const QString& fieldPath);
    void removeFieldRow(const QString& fieldPath);
    void updateFieldRow(const QString& fieldPath, const QVariant& value);
    FieldRow* findFieldRow(const QString& fieldPath);
    const FieldRow* findFieldRow(const QString& fieldPath) const;
    
    // Drag and drop helpers
    void handleDroppedField(const QString& fieldPath, const QJsonObject& fieldData);
    void extractPrimitiveFields(const QJsonObject& structData, const QString& basePath, QStringList& primitiveFields);

    // Visual effects
    void animateValueChange(FieldRow* row, const QVariant& newValue);
    void highlightRow(int row, const QColor& color, int duration);
    QIcon getFieldTypeIcon(const QString& fieldPath) const;
    void updateRowAppearance(int row);

    // Table operations
    void refreshTableStructure();
    void updateTableHeaders();
    void applyGridOptions();
    void optimizeTableDisplay();

    // Performance optimizations
    void updateVisibleRows();
    bool isRowVisible(int row) const;
    void scheduleDelayedUpdate();

    // Utility methods
    QString getFieldNameFromRow(int row) const;
    QString formatFieldName(const QString& fieldPath) const;
    void ensureRowVisible(int row);
    QTableWidgetItem* createFieldNameItem(const QString& fieldPath, const QString& displayName);
    QTableWidgetItem* createFieldValueItem(const QVariant& value, const DisplayConfig& config);

    // Main table widget
    QTableWidget* m_table;
    QVBoxLayout* m_mainLayout;

    // Field management
    QHash<QString, FieldRow> m_fieldRows; // fieldPath -> FieldRow
    QHash<int, QString> m_rowToField;     // row -> fieldPath
    QMutex m_fieldRowMutex; // Thread safety for field operations

    // Configuration
    GridOptions m_gridOptions;
    QHash<QString, QString> m_customDisplayNames; // fieldPath -> display name
    QHash<QString, QIcon> m_customIcons;          // fieldPath -> icon

    // Visual state
    int m_lastSortColumn;
    Qt::SortOrder m_lastSortOrder;
    QTimer* m_delayedUpdateTimer;
    bool m_updatePending;

    // Context menu actions
    QAction* m_editDisplayNameAction;
    QAction* m_setIconAction;
    QAction* m_removeFieldAction;
    QAction* m_copyValueAction;
    QAction* m_showHistoryAction;
    QAction* m_toggleGridLinesAction;
    QAction* m_toggleRowColorsAction;
    QAction* m_resetColumnWidthsAction;
    QAction* m_exportClipboardAction;
    QAction* m_exportFileAction;

    // Performance tracking
    std::chrono::steady_clock::time_point m_lastTableUpdate;
    int m_updateCount;
    int m_maxVisibleRows;

    // Drag and drop visual feedback
    QLabel* m_dropIndicator;
    bool m_showDropIndicator;
    QPoint m_dropPosition;

    // Animation management
    // Animation system simplified to use QTimer instead of QPropertyAnimation
};

/**
 * @brief Custom table widget item with enhanced features
 */
class GridTableItem : public QTableWidgetItem
{
public:
    explicit GridTableItem(const QString& text = QString(), int type = Type);

    // Enhanced data storage
    void setFieldPath(const QString& path) { m_fieldPath = path; }
    QString getFieldPath() const { return m_fieldPath; }

    void setTimestamp(const std::chrono::steady_clock::time_point& timestamp) { m_timestamp = timestamp; }
    std::chrono::steady_clock::time_point getTimestamp() const { return m_timestamp; }

    void setValueHistory(const QVariantList& history) { m_valueHistory = history; }
    QVariantList getValueHistory() const { return m_valueHistory; }

    // Custom comparison for sorting
    bool operator<(const QTableWidgetItem& other) const override;

private:
    QString m_fieldPath;
    std::chrono::steady_clock::time_point m_timestamp;
    QVariantList m_valueHistory;
};

#endif // GRID_WIDGET_H