#ifndef GRID_LOGGER_WIDGET_H
#define GRID_LOGGER_WIDGET_H

#include "display_widget.h"
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollBar>
#include <QTimer>
#include <QCheckBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QQueue>
#include <chrono>
#include <deque>
#include <atomic>

/**
 * @brief Grid Logger widget for displaying packet field history in table format
 * 
 * The GridLoggerWidget displays packet field values as they arrive over time:
 * - Columns represent different fields
 * - Rows represent packets/timestamps
 * - Each new packet creates a new row
 * - Configurable row limits with auto-scroll or auto-delete
 * - Real-time logging with timestamp columns
 * - Export capabilities (CSV, Excel, JSON)
 * - Search and filter functionality
 * - Row highlighting based on conditions
 * - Auto-save functionality
 * 
 * Performance Features:
 * - Efficient row insertion and removal
 * - Viewport culling for large datasets
 * - Background auto-save without blocking UI
 * - Memory-efficient string storage
 * - Batch updates for high-frequency data
 * 
 * Display Features:
 * - Timestamp column with configurable format
 * - Sortable columns
 * - Resizable columns with persistence
 * - Row highlighting based on field values
 * - Alternating row colors
 * - Progress indicator for large operations
 */
class GridLoggerWidget : public DisplayWidget
{
    Q_OBJECT

public:
    /**
     * @brief Logger configuration options
     */
    struct LoggerOptions {
        int maxRows = 10000;                    ///< Maximum number of rows to keep
        bool autoScroll = true;                 ///< Auto-scroll to newest data
        bool autoDeleteOldest = true;          ///< Auto-delete oldest rows when max reached
        bool showTimestamp = true;             ///< Show timestamp column
        QString timestampFormat = "hh:mm:ss.zzz"; ///< Timestamp display format
        bool enableAutoSave = false;           ///< Enable auto-save functionality
        int autoSaveInterval = 60000;          ///< Auto-save interval in ms (60 seconds)
        QString autoSaveFile;                  ///< Auto-save file path
        bool highlightNewRows = true;          ///< Highlight newly added rows
        int highlightDuration = 2000;          ///< Row highlight duration in ms
        QColor highlightColor = QColor(144, 238, 144); ///< Light green highlight
        
        LoggerOptions() = default;
    };

    /**
     * @brief Row highlighting rule
     */
    struct HighlightRule {
        QString name;               ///< Rule name for identification
        QString fieldPath;          ///< Field to evaluate
        QString condition;          ///< Condition expression (e.g., "> 100")
        QColor backgroundColor;     ///< Row background color
        QColor textColor;           ///< Row text color
        bool enabled = true;        ///< Rule enabled state
        
        HighlightRule() = default;
        HighlightRule(const QString& ruleName, const QString& field, const QString& cond, const QColor& bgColor)
            : name(ruleName), fieldPath(field), condition(cond), backgroundColor(bgColor), textColor(Qt::black) {}
    };

    /**
     * @brief Packet row data for efficient storage
     */
    struct PacketRow {
        std::chrono::steady_clock::time_point timestamp;
        Monitor::Packet::PacketId packetId;
        QHash<QString, QVariant> fieldValues;  ///< fieldPath -> value
        int tableRow;                          ///< Current row index in table
        bool isHighlighted = false;            ///< Row highlighting state
        
        PacketRow() : packetId(0), tableRow(-1) {}
        PacketRow(Monitor::Packet::PacketId id) 
            : timestamp(std::chrono::steady_clock::now()), packetId(id), tableRow(-1) {}
    };

    explicit GridLoggerWidget(const QString& widgetId, QWidget* parent = nullptr);
    ~GridLoggerWidget() override;

    // Logger-specific configuration
    void setLoggerOptions(const LoggerOptions& options);
    LoggerOptions getLoggerOptions() const { return m_loggerOptions; }

    // Row management
    void clearAllRows();
    void setMaxRows(int maxRows);
    int getMaxRows() const { return m_loggerOptions.maxRows; }
    int getCurrentRowCount() const;

    // Highlighting rules
    void addHighlightRule(const HighlightRule& rule);
    void removeHighlightRule(const QString& ruleName);
    void clearHighlightRules();
    QList<HighlightRule> getHighlightRules() const;

    // Export functionality
    bool exportToCSV(const QString& fileName) const;
    bool exportToJSON(const QString& fileName) const;
    QString getClipboardText() const;

    // Search and filter
    void setSearchFilter(const QString& searchText);
    void clearSearchFilter();
    void setFieldFilter(const QString& fieldPath, const QVariant& value);
    void clearFieldFilters();

    // Auto-save functionality
    void enableAutoSave(bool enabled, const QString& fileName = QString());
    bool isAutoSaveEnabled() const { return m_loggerOptions.enableAutoSave; }
    QString getAutoSaveFile() const { return m_loggerOptions.autoSaveFile; }

    // Test helpers
    QMenu* getContextMenuForTesting() const { return getContextMenu(); }
    void performAutoSaveForTesting() { performAutoSave(); }
    bool restoreWidgetSpecificSettingsForTesting(const QJsonObject& settings) { return restoreWidgetSpecificSettings(settings); }
    void updateFieldDisplayForTesting(const QString& fieldPath, const QVariant& value) { updateFieldDisplay(fieldPath, value); }
    void processPendingUpdatesForTesting() { processPendingUpdates(); }

public slots:
    void scrollToTop();
    void scrollToBottom();
    void jumpToRow(int row);
    void resizeColumnsToContents();
    void refreshTable();

signals:
    void rowAdded(int row);
    void rowsCleared();
    void maxRowsReached();
    void autoSaveCompleted(const QString& fileName);
    void autoSaveError(const QString& error);
    void rowHighlighted(int row, const QString& ruleName);

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

    // Drag and drop enhancements
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    // Table interactions
    void onCellClicked(int row, int column);
    void onCellDoubleClicked(int row, int column);
    void onHeaderClicked(int logicalIndex);
    void onCustomContextMenuRequested(const QPoint& pos);
    void onVerticalScrollChanged(int value);
    void onHorizontalScrollChanged(int value);
    
    // Auto-save functionality
    void onAutoSaveTimer();
    void performAutoSave();
    
    // Column management
    void onColumnVisibilityChanged();
    void onTimestampFormatChanged();
    
    // Context menu actions
    void onClearAllRows();
    void onExportToCSV();
    void onExportToJSON();
    void onCopyAllToClipboard();
    void onConfigureHighlightRules();
    void onToggleAutoSave();
    void onJumpToRow();
    void onSearchRows();
    void onConfigureColumns();
    
    // Batch processing
    void processPendingUpdates();

private:
    // UI setup
    void setupTable();
    void setupLayout();
    void setupConnections();
    void setupAutoSave();
    void setupToolbar();

    // Row management
    void addPacketRow(Monitor::Packet::PacketId packetId, const QHash<QString, QVariant>& fieldValues);
    void removeOldestRows(int count);
    void updateTableStructure();
    void rebuildTableFromData();

    // Column management
    void updateColumnHeaders();
    void addFieldColumn(const QString& fieldPath);
    void removeFieldColumn(const QString& fieldPath);
    int findColumnIndex(const QString& fieldPath) const;
    QString getFieldPathFromColumn(int column) const;

    // Data processing
    void processPacketData();
    void applyHighlightRules(int tableRow, const PacketRow& packetRow);
    bool evaluateHighlightCondition(const HighlightRule& rule, const QHash<QString, QVariant>& fieldValues);

    // Performance optimizations
    void optimizeDisplay();
    void updateVisibleRows();
    bool isRowVisible(int row) const;
    void scheduleUpdate();

    // Auto-save implementation
    bool writeToFile(const QString& fileName, const QString& format) const;
    QString formatRowDataAsCSV(const PacketRow& row) const;
    QString formatRowDataAsJSON(const PacketRow& row) const;

    // Search and filter
    void applySearchFilter();
    bool rowMatchesSearch(const PacketRow& row, const QString& searchText) const;
    bool rowMatchesFilters(const PacketRow& row) const;

    // Table item creation
    QTableWidgetItem* createTimestampItem(const std::chrono::steady_clock::time_point& timestamp);
    QTableWidgetItem* createFieldValueItem(const QVariant& value, const QString& fieldPath);
    void updateTableItem(QTableWidgetItem* item, const QVariant& value, const QString& fieldPath);

    // Main table widget
    QTableWidget* m_table;
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_toolbarLayout;

    // Configuration
    LoggerOptions m_loggerOptions;
    QList<HighlightRule> m_highlightRules;

    // Data storage
    std::deque<PacketRow> m_packetRows;        ///< Packet data storage
    QStringList m_fieldColumns;               ///< Ordered field columns
    QHash<QString, int> m_fieldToColumn;      ///< Field path to column index
    mutable QMutex m_dataMutex;               ///< Thread safety for data access

    // Pending updates for batch processing
    QQueue<QHash<QString, QVariant>> m_pendingUpdates;
    Monitor::Packet::PacketId m_lastPacketId = 0;
    QTimer* m_updateTimer;

    // Auto-save components
    QTimer* m_autoSaveTimer;
    QFile* m_autoSaveFile;
    std::atomic<bool> m_autoSaveInProgress{false};

    // Search and filter state
    QString m_currentSearchText;
    QHash<QString, QVariant> m_fieldFilters;
    bool m_filtersActive = false;

    // UI controls
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QCheckBox* m_autoScrollCheckBox;
    QSpinBox* m_maxRowsSpinBox;

    // Context menu actions
    QAction* m_clearRowsAction;
    QAction* m_exportCSVAction;
    QAction* m_exportJSONAction;
    QAction* m_copyAllAction;
    QAction* m_highlightRulesAction;
    QAction* m_autoSaveAction;
    QAction* m_jumpToRowAction;
    QAction* m_searchAction;
    QAction* m_configureColumnsAction;

    // Performance tracking
    std::chrono::steady_clock::time_point m_lastUpdate;
    std::atomic<int> m_updateCount{0};
    std::atomic<int> m_maxVisibleRows{500};

    // Visual state
    int m_lastHighlightedRow = -1;
    QTimer* m_highlightTimer;
    bool m_tableNeedsRebuild = false;
};

/**
 * @brief Custom table widget item for logger data
 */
class LoggerTableItem : public QTableWidgetItem
{
public:
    explicit LoggerTableItem(const QString& text = QString(), int type = Type);

    // Enhanced data storage
    void setFieldPath(const QString& path) { m_fieldPath = path; }
    QString getFieldPath() const { return m_fieldPath; }

    void setPacketId(Monitor::Packet::PacketId id) { m_packetId = id; }
    Monitor::Packet::PacketId getPacketId() const { return m_packetId; }

    void setTimestamp(const std::chrono::steady_clock::time_point& timestamp) { m_timestamp = timestamp; }
    std::chrono::steady_clock::time_point getTimestamp() const { return m_timestamp; }

    // Custom comparison for sorting
    bool operator<(const QTableWidgetItem& other) const override;

private:
    QString m_fieldPath;
    Monitor::Packet::PacketId m_packetId = 0;
    std::chrono::steady_clock::time_point m_timestamp;
};

/**
 * @brief Dialog for configuring highlight rules
 */
class HighlightRulesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HighlightRulesDialog(const QList<GridLoggerWidget::HighlightRule>& rules, 
                                 const QStringList& availableFields, QWidget* parent = nullptr);
    
    QList<GridLoggerWidget::HighlightRule> getHighlightRules() const;

private slots:
    void onAddRule();
    void onEditRule();
    void onDeleteRule();
    void onRuleSelectionChanged();
    void onTestRule();

private:
    void setupUI();
    void updateRulesList();
    void populateRuleEditor(const GridLoggerWidget::HighlightRule& rule);
    GridLoggerWidget::HighlightRule getRuleFromEditor() const;

    QList<GridLoggerWidget::HighlightRule> m_rules;
    QStringList m_availableFields;
    
    // UI components will be added as needed
};

#endif // GRID_LOGGER_WIDGET_H