#ifndef STRUCT_WINDOW_H
#define STRUCT_WINDOW_H

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QHeaderView>
#include <QSplitter>
#include <QGroupBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QIcon>
#include <QPixmap>
#include <QPainter>

// Forward declarations
class StructureManager;
class TabManager;

/**
 * @brief Widget for displaying and managing structure hierarchies
 * 
 * The StructWindow provides:
 * - Tree view of all saved structures (reusable and packet structs)
 * - Expandable/collapsible hierarchical display
 * - Two-column layout (Field Name | Type)
 * - Drag-and-drop source for field assignment to widgets
 * - Structure filtering and search
 * - Context menus for structure operations
 * - Integration with Phase 2 structure management system
 */
class StructWindow : public QWidget
{
    Q_OBJECT

public:
    explicit StructWindow(QWidget *parent = nullptr);
    ~StructWindow();

    // Structure management
    void refreshStructures();
    void addStructure(const QString &structName, const QJsonObject &structData);
    void removeStructure(const QString &structName);
    void updateStructure(const QString &structName, const QJsonObject &structData);
    
    // Tree operations
    void expandAll();
    void collapseAll();
    void expandItem(const QString &path);
    void collapseItem(const QString &path);
    
    // Selection and navigation
    QStringList getSelectedFields() const;
    QString getSelectedStructure() const;
    void selectField(const QString &fieldPath);
    void clearSelection();
    
    // Search and filtering
    void setSearchFilter(const QString &filter);
    void clearSearchFilter();
    void setStructureTypeFilter(const QStringList &types);
    
    // Settings and persistence
    QJsonObject saveState() const;
    bool restoreState(const QJsonObject &state);
    
    // Drag and drop configuration
    void setDragEnabled(bool enabled);
    bool isDragEnabled() const { return m_dragEnabled; }

    // Custom tree widget item for structure data
    class StructureTreeItem : public QTreeWidgetItem
    {
    public:
        enum ItemType {
            StructureType = QTreeWidgetItem::UserType + 1,
            FieldType,
            ArrayType,
            UnionType,
            BitfieldType
        };
        
        explicit StructureTreeItem(ItemType type = StructureType);
        StructureTreeItem(QTreeWidget *parent, ItemType type = StructureType);
        StructureTreeItem(QTreeWidgetItem *parent, ItemType type = StructureType);
        
        // Data storage
        void setFieldData(const QJsonObject &data);
        QJsonObject getFieldData() const;
        void setFieldPath(const QString &path);
        QString getFieldPath() const;
        void setFieldType(const QString &type);
        QString getFieldType() const;
        
        // Visual customization
        void updateAppearance();
        void setExpansionState(bool expanded);
        
        // Drag support
        bool isDraggable() const;
        QMimeData* createDragData() const;
        
        ItemType getItemType() const { return static_cast<ItemType>(type()); }
        
    private:
        QJsonObject m_fieldData;
        QString m_fieldPath;
        QString m_fieldType;
        bool m_isExpanded;
    };

public slots:
    void onStructureAdded(const QString &name);
    void onStructureRemoved(const QString &name);
    void onStructureUpdated(const QString &name);

signals:
    // Drag and drop signals
    void fieldDragStarted(const QString &fieldPath, const QJsonObject &fieldInfo);
    void fieldDragFinished(const QString &fieldPath, bool successful);
    
    // Selection signals
    void fieldSelected(const QString &fieldPath, const QJsonObject &fieldInfo);
    void structureSelected(const QString &structName, const QJsonObject &structInfo);
    void selectionCleared();
    
    // Context menu signals
    void addStructureRequested();
    void editStructureRequested(const QString &structName);
    void deleteStructureRequested(const QString &structName);
    void duplicateStructureRequested(const QString &structName);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // Tree widget events
    void onItemExpanded(QTreeWidgetItem *item);
    void onItemCollapsed(QTreeWidgetItem *item);
    void onItemSelectionChanged();
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onItemClicked(QTreeWidgetItem *item, int column);
    
    // Context menu
    void onContextMenuRequested(const QPoint &position);
    void onExpandAllAction();
    void onCollapseAllAction();
    void onRefreshAction();
    void onAddStructureAction();
    void onEditStructureAction();
    void onDeleteStructureAction();
    void onDuplicateStructureAction();
    void onShowDetailsAction();
    
    // Search and filter
    void onSearchTextChanged(const QString &text);
    void onClearSearchClicked();
    void onFilterButtonClicked();

private:
    // UI setup
    void setupUI();
    void setupTreeWidget();
    void setupSearchBar();
    void setupToolbar();
    void setupContextMenu();
    
    // Tree population
    void populateTree();
    void addStructureToTree(const QString &name, const QJsonObject &structData, bool isPacketStruct = false);
    StructureTreeItem* addFieldToTree(StructureTreeItem *parent, const QString &fieldName, const QJsonObject &fieldData, const QString &basePath = QString());
    void updateTreeItem(StructureTreeItem *item, const QJsonObject &fieldData);
    
    // Drag and drop support
    void startDrag(StructureTreeItem *item);
    QPixmap createDragPixmap(StructureTreeItem *item);
    
    // Search and filtering
    void applySearchFilter();
    void filterTreeItem(QTreeWidgetItem *item, const QString &filter);
    bool itemMatchesFilter(QTreeWidgetItem *item, const QString &filter) const;
    void showFilterDialog();
    
    // Tree population helpers
    void createMockStructures();
    StructureTreeItem* addMockField(StructureTreeItem *parent, 
                                   const QString &name, 
                                   const QString &type, 
                                   int size, 
                                   const QString &path);
    
    // Tree state management
    void saveExpansionState();
    void restoreExpansionState();
    QString getItemPath(QTreeWidgetItem *item) const;
    QTreeWidgetItem* findItemByPath(const QString &path) const;
    
    // Visual enhancements
    void updateItemIcons();
    QIcon getIconForType(const QString &type) const;
    void applyTreeStyling();
    
    // Main layout components
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_searchLayout;
    QHBoxLayout *m_toolbarLayout;
    
    // Search and filter components
    QLineEdit *m_searchEdit;
    QPushButton *m_clearSearchButton;
    QPushButton *m_filterButton;
    QLabel *m_resultCountLabel;
    
    // Toolbar components
    QToolButton *m_expandAllButton;
    QToolButton *m_collapseAllButton;
    QToolButton *m_refreshButton;
    QToolButton *m_addStructButton;
    
    // Tree widget
    QTreeWidget *m_treeWidget;
    
    // Context menu
    QMenu *m_contextMenu;
    QAction *m_expandAllAction;
    QAction *m_collapseAllAction;
    QAction *m_refreshAction;
    QAction *m_addStructureAction;
    QAction *m_editStructureAction;
    QAction *m_deleteStructureAction;
    QAction *m_duplicateStructureAction;
    QAction *m_showDetailsAction;
    
    // Drag and drop state
    bool m_dragEnabled;
    StructureTreeItem *m_dragItem;
    QPoint m_dragStartPosition;
    QTimer *m_dragTimer;
    
    // Search and filter state
    QString m_currentFilter;
    QStringList m_typeFilters;
    QMap<QString, bool> m_expansionState;
    
    // Structure data integration
    StructureManager *m_structureManager;
    
    // Visual customization
    int m_iconSize;
    QHash<QString, QIcon> m_typeIcons;
    
    // Performance optimization
    bool m_batchUpdate;
    QTimer *m_updateTimer;
};

/**
 * @brief Custom QTreeWidget with enhanced drag-and-drop support
 */
class StructTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit StructTreeWidget(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;

signals:
    void itemDragStarted(QTreeWidgetItem *item);
    void itemDragFinished(QTreeWidgetItem *item, bool successful);

private:
    QPoint m_dragStartPosition;
    bool m_dragInProgress;
};

#endif // STRUCT_WINDOW_H