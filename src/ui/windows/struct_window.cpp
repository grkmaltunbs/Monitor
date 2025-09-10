#include "struct_window.h"
#include "../../parser/manager/structure_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QJsonObject>
#include <QJsonArray>
#include <QMimeData>
#include <QDrag>
#include <QApplication>
#include <QStyle>
#include <QTimer>
#include <QtCore/QLoggingCategory>

Q_LOGGING_CATEGORY(structWindow, "Monitor.StructWindow")

StructWindow::StructWindow(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_searchLayout(nullptr)
    , m_toolbarLayout(nullptr)
    , m_searchEdit(nullptr)
    , m_clearSearchButton(nullptr)
    , m_filterButton(nullptr)
    , m_resultCountLabel(nullptr)
    , m_treeWidget(nullptr)
    , m_contextMenu(nullptr)
    , m_dragEnabled(true)
    , m_dragItem(nullptr)
    , m_dragTimer(nullptr)
    , m_structureManager(nullptr)
    , m_iconSize(16)
    , m_batchUpdate(false)
    , m_updateTimer(nullptr)
{
    setupUI();
    setupContextMenu();
    
    // Initialize update timer for performance optimization
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(100);
    connect(m_updateTimer, &QTimer::timeout, this, &StructWindow::populateTree);
    
    qCInfo(structWindow) << "StructWindow initialized";
}

StructWindow::~StructWindow()
{
    qCInfo(structWindow) << "StructWindow destroyed";
}

void StructWindow::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(4);
    
    // Setup search bar
    setupSearchBar();
    
    // Setup toolbar
    setupToolbar();
    
    // Setup tree widget
    setupTreeWidget();
    
    // Apply styling
    applyTreeStyling();
}

void StructWindow::setupSearchBar()
{
    m_searchLayout = new QHBoxLayout();
    m_searchLayout->setSpacing(4);
    
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search structures and fields...");
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &StructWindow::onSearchTextChanged);
    
    m_clearSearchButton = new QPushButton("Clear", this);
    m_clearSearchButton->setMaximumWidth(60);
    connect(m_clearSearchButton, &QPushButton::clicked, this, &StructWindow::onClearSearchClicked);
    
    m_filterButton = new QPushButton("Filter", this);
    m_filterButton->setMaximumWidth(60);
    connect(m_filterButton, &QPushButton::clicked, this, &StructWindow::onFilterButtonClicked);
    
    m_resultCountLabel = new QLabel("0 results", this);
    m_resultCountLabel->setStyleSheet("QLabel { color: gray; }");
    
    m_searchLayout->addWidget(m_searchEdit);
    m_searchLayout->addWidget(m_clearSearchButton);
    m_searchLayout->addWidget(m_filterButton);
    m_searchLayout->addWidget(m_resultCountLabel);
    
    m_mainLayout->addLayout(m_searchLayout);
}

void StructWindow::setupToolbar()
{
    m_toolbarLayout = new QHBoxLayout();
    m_toolbarLayout->setSpacing(4);
    
    // Expand/Collapse buttons
    m_expandAllButton = new QToolButton(this);
    m_expandAllButton->setText("Expand All");
    m_expandAllButton->setToolTip("Expand all structure nodes");
    m_expandAllButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    connect(m_expandAllButton, &QToolButton::clicked, this, &StructWindow::onExpandAllAction);
    
    m_collapseAllButton = new QToolButton(this);
    m_collapseAllButton->setText("Collapse All");
    m_collapseAllButton->setToolTip("Collapse all structure nodes");
    m_collapseAllButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    connect(m_collapseAllButton, &QToolButton::clicked, this, &StructWindow::onCollapseAllAction);
    
    m_refreshButton = new QToolButton(this);
    m_refreshButton->setText("Refresh");
    m_refreshButton->setToolTip("Refresh structure list");
    m_refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    connect(m_refreshButton, &QToolButton::clicked, this, &StructWindow::onRefreshAction);
    
    m_addStructButton = new QToolButton(this);
    m_addStructButton->setText("Add");
    m_addStructButton->setToolTip("Add new structure");
    m_addStructButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    connect(m_addStructButton, &QToolButton::clicked, this, &StructWindow::onAddStructureAction);
    
    m_toolbarLayout->addWidget(m_expandAllButton);
    m_toolbarLayout->addWidget(m_collapseAllButton);
    m_toolbarLayout->addWidget(m_refreshButton);
    m_toolbarLayout->addWidget(m_addStructButton);
    m_toolbarLayout->addStretch();
    
    m_mainLayout->addLayout(m_toolbarLayout);
}

void StructWindow::setupTreeWidget()
{
    m_treeWidget = new StructTreeWidget(this);
    m_treeWidget->setObjectName("StructureTreeWidget");
    m_treeWidget->setHeaderLabels(QStringList() << "Field Name" << "Type" << "Size");
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeWidget->setDragDropMode(QAbstractItemView::DragOnly);
    m_treeWidget->setDragEnabled(m_dragEnabled);
    
    // Configure header
    QHeaderView *header = m_treeWidget->header();
    header->setStretchLastSection(false);
    header->resizeSection(0, 200);  // Field Name
    header->resizeSection(1, 120);  // Type
    header->resizeSection(2, 80);   // Size
    
    // Connect signals
    connect(m_treeWidget, &QTreeWidget::itemExpanded, this, &StructWindow::onItemExpanded);
    connect(m_treeWidget, &QTreeWidget::itemCollapsed, this, &StructWindow::onItemCollapsed);
    connect(m_treeWidget, &QTreeWidget::itemSelectionChanged, this, &StructWindow::onItemSelectionChanged);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this, &StructWindow::onItemDoubleClicked);
    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &StructWindow::onItemClicked);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested, this, &StructWindow::onContextMenuRequested);
    
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    
    m_mainLayout->addWidget(m_treeWidget);
    
    // Initialize with some mock data for testing
    populateTree();
}

void StructWindow::setupContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    m_expandAllAction = m_contextMenu->addAction("Expand All");
    connect(m_expandAllAction, &QAction::triggered, this, &StructWindow::onExpandAllAction);
    
    m_collapseAllAction = m_contextMenu->addAction("Collapse All");
    connect(m_collapseAllAction, &QAction::triggered, this, &StructWindow::onCollapseAllAction);
    
    m_contextMenu->addSeparator();
    
    m_refreshAction = m_contextMenu->addAction("Refresh");
    connect(m_refreshAction, &QAction::triggered, this, &StructWindow::onRefreshAction);
    
    m_contextMenu->addSeparator();
    
    m_addStructureAction = m_contextMenu->addAction("Add Structure...");
    connect(m_addStructureAction, &QAction::triggered, this, &StructWindow::onAddStructureAction);
    
    m_editStructureAction = m_contextMenu->addAction("Edit Structure...");
    connect(m_editStructureAction, &QAction::triggered, this, &StructWindow::onEditStructureAction);
    
    m_deleteStructureAction = m_contextMenu->addAction("Delete Structure");
    connect(m_deleteStructureAction, &QAction::triggered, this, &StructWindow::onDeleteStructureAction);
    
    m_duplicateStructureAction = m_contextMenu->addAction("Duplicate Structure");
    connect(m_duplicateStructureAction, &QAction::triggered, this, &StructWindow::onDuplicateStructureAction);
    
    m_contextMenu->addSeparator();
    
    m_showDetailsAction = m_contextMenu->addAction("Show Details");
    connect(m_showDetailsAction, &QAction::triggered, this, &StructWindow::onShowDetailsAction);
}

void StructWindow::populateTree()
{
    m_treeWidget->clear();
    
    // Create some mock structures for testing
    createMockStructures();
    
    qCDebug(structWindow) << "Tree populated with structures";
}

void StructWindow::createMockStructures()
{
    // Mock Header Structure
    StructureTreeItem *headerItem = new StructureTreeItem(StructureTreeItem::StructureType);
    headerItem->setText(0, "S_HEADER");
    headerItem->setText(1, "struct");
    headerItem->setText(2, "24 bytes");
    headerItem->setFieldPath("S_HEADER");
    
    QJsonObject headerData;
    headerData["type"] = "struct";
    headerData["size"] = 24;
    headerData["isPacketHeader"] = true;
    headerItem->setFieldData(headerData);
    
    m_treeWidget->addTopLevelItem(headerItem);
    
    // Add header fields
    addMockField(headerItem, "packetId", "uint32_t", 4, "S_HEADER.packetId");
    addMockField(headerItem, "sequence", "uint32_t", 4, "S_HEADER.sequence");
    addMockField(headerItem, "timestamp", "uint64_t", 8, "S_HEADER.timestamp");
    addMockField(headerItem, "flags", "uint32_t", 4, "S_HEADER.flags");
    addMockField(headerItem, "length", "uint16_t", 2, "S_HEADER.length");
    addMockField(headerItem, "reserved", "uint16_t", 2, "S_HEADER.reserved");
    
    // Mock Field3D Structure
    StructureTreeItem *field3dItem = new StructureTreeItem(StructureTreeItem::StructureType);
    field3dItem->setText(0, "Field3D");
    field3dItem->setText(1, "struct");
    field3dItem->setText(2, "12 bytes");
    field3dItem->setFieldPath("Field3D");
    
    QJsonObject field3dData;
    field3dData["type"] = "struct";
    field3dData["size"] = 12;
    field3dData["isReusable"] = true;
    field3dItem->setFieldData(field3dData);
    
    m_treeWidget->addTopLevelItem(field3dItem);
    
    // Add Field3D fields
    addMockField(field3dItem, "x", "int32_t", 4, "Field3D.x");
    addMockField(field3dItem, "y", "int32_t", 4, "Field3D.y");
    addMockField(field3dItem, "z", "int32_t", 4, "Field3D.z");
    
    // Mock DUMMY Packet Structure
    StructureTreeItem *dummyItem = new StructureTreeItem(StructureTreeItem::StructureType);
    dummyItem->setText(0, "DUMMY");
    dummyItem->setText(1, "packet struct");
    dummyItem->setText(2, "52 bytes");
    dummyItem->setFieldPath("DUMMY");
    
    QJsonObject dummyData;
    dummyData["type"] = "packet_struct";
    dummyData["size"] = 52;
    dummyData["packetId"] = 1;
    dummyItem->setFieldData(dummyData);
    
    m_treeWidget->addTopLevelItem(dummyItem);
    
    // Add DUMMY fields
    StructureTreeItem *headerFieldItem = addMockField(dummyItem, "header", "S_HEADER", 24, "DUMMY.header");
    
    // Add nested header fields
    addMockField(headerFieldItem, "packetId", "uint32_t", 4, "DUMMY.header.packetId");
    addMockField(headerFieldItem, "sequence", "uint32_t", 4, "DUMMY.header.sequence");
    addMockField(headerFieldItem, "timestamp", "uint64_t", 8, "DUMMY.header.timestamp");
    
    StructureTreeItem *velocityItem = addMockField(dummyItem, "velocity", "Field3D", 12, "DUMMY.velocity");
    addMockField(velocityItem, "x", "int32_t", 4, "DUMMY.velocity.x");
    addMockField(velocityItem, "y", "int32_t", 4, "DUMMY.velocity.y");
    addMockField(velocityItem, "z", "int32_t", 4, "DUMMY.velocity.z");
    
    StructureTreeItem *accelItem = addMockField(dummyItem, "acceleration", "Field3D", 12, "DUMMY.acceleration");
    addMockField(accelItem, "x", "int32_t", 4, "DUMMY.acceleration.x");
    addMockField(accelItem, "y", "int32_t", 4, "DUMMY.acceleration.y");
    addMockField(accelItem, "z", "int32_t", 4, "DUMMY.acceleration.z");
    
    addMockField(dummyItem, "name", "char[4]", 4, "DUMMY.name");
    addMockField(dummyItem, "time", "float", 4, "DUMMY.time");
    
    // Update appearance for all items
    updateItemIcons();
    
    // Set initial expansion state
    headerItem->setExpanded(false);
    field3dItem->setExpanded(false);
    dummyItem->setExpanded(true);
}

StructWindow::StructureTreeItem* StructWindow::addMockField(StructureTreeItem *parent, 
                                                            const QString &name, 
                                                            const QString &type, 
                                                            int size, 
                                                            const QString &path)
{
    StructureTreeItem *item = new StructureTreeItem(StructureTreeItem::FieldType);
    item->setText(0, name);
    item->setText(1, type);
    item->setText(2, QString("%1 bytes").arg(size));
    item->setFieldPath(path);
    item->setFieldType(type);
    
    QJsonObject fieldData;
    fieldData["name"] = name;
    fieldData["type"] = type;
    fieldData["size"] = size;
    fieldData["path"] = path;
    item->setFieldData(fieldData);
    
    parent->addChild(item);
    return item;
}

void StructWindow::applyTreeStyling()
{
    m_treeWidget->setStyleSheet(R"(
        QTreeWidget {
            background-color: white;
            border: 1px solid #ccc;
            selection-background-color: #3a7bd4;
            selection-color: white;
        }
        QTreeWidget::item {
            padding: 2px;
            border: none;
        }
        QTreeWidget::item:hover {
            background-color: #f0f0f0;
        }
        QTreeWidget::item:selected {
            background-color: #3a7bd4;
        }
        QTreeWidget::branch:has-children:!has-siblings:closed,
        QTreeWidget::branch:closed:has-children:has-siblings {
            border-image: none;
            image: url(:/icons/branch-closed.png);
        }
        QTreeWidget::branch:open:has-children:!has-siblings,
        QTreeWidget::branch:open:has-children:has-siblings {
            border-image: none;
            image: url(:/icons/branch-open.png);
        }
    )");
}

void StructWindow::updateItemIcons()
{
    // TODO: Implement proper icons based on type
    // For now, just update the appearance
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_treeWidget->topLevelItem(i);
        if (auto structItem = dynamic_cast<StructureTreeItem*>(item)) {
            structItem->updateAppearance();
        }
    }
}

// Public interface methods (stubs for now)
void StructWindow::refreshStructures()
{
    populateTree();
}

void StructWindow::expandAll()
{
    m_treeWidget->expandAll();
}

void StructWindow::collapseAll()
{
    m_treeWidget->collapseAll();
}

QStringList StructWindow::getSelectedFields() const
{
    QStringList fields;
    QList<QTreeWidgetItem*> selectedItems = m_treeWidget->selectedItems();
    
    for (auto item : selectedItems) {
        if (auto structItem = dynamic_cast<StructureTreeItem*>(item)) {
            fields << structItem->getFieldPath();
        }
    }
    
    return fields;
}

void StructWindow::setDragEnabled(bool enabled)
{
    m_dragEnabled = enabled;
    m_treeWidget->setDragEnabled(enabled);
}

QJsonObject StructWindow::saveState() const
{
    QJsonObject state;
    state["searchFilter"] = m_currentFilter;
    // TODO: Save expansion state
    return state;
}

bool StructWindow::restoreState(const QJsonObject &state)
{
    if (state.contains("searchFilter")) {
        setSearchFilter(state["searchFilter"].toString());
    }
    // TODO: Restore expansion state
    return true;
}

// Slot implementations (mostly stubs)
void StructWindow::onItemExpanded(QTreeWidgetItem *item)
{
    Q_UNUSED(item)
    saveExpansionState();
}

void StructWindow::onItemCollapsed(QTreeWidgetItem *item)
{
    Q_UNUSED(item)
    saveExpansionState();
}

void StructWindow::onItemSelectionChanged()
{
    QList<QTreeWidgetItem*> selectedItems = m_treeWidget->selectedItems();
    
    if (!selectedItems.isEmpty()) {
        auto item = dynamic_cast<StructureTreeItem*>(selectedItems.first());
        if (item) {
            emit fieldSelected(item->getFieldPath(), item->getFieldData());
        }
    } else {
        emit selectionCleared();
    }
}

void StructWindow::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    if (auto structItem = dynamic_cast<StructureTreeItem*>(item)) {
        // Trigger edit action or expand/collapse
        qCDebug(structWindow) << "Double-clicked item:" << structItem->getFieldPath();
    }
}

void StructWindow::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(item)
    Q_UNUSED(column)
    // Handle single click if needed
}

void StructWindow::onContextMenuRequested(const QPoint &position)
{
    QTreeWidgetItem *item = m_treeWidget->itemAt(position);
    if (item && m_contextMenu) {
        // Enable/disable context menu actions based on selected item
        bool hasSelection = (item != nullptr);
        m_editStructureAction->setEnabled(hasSelection);
        m_deleteStructureAction->setEnabled(hasSelection);
        m_duplicateStructureAction->setEnabled(hasSelection);
        m_showDetailsAction->setEnabled(hasSelection);
        
        m_contextMenu->popup(m_treeWidget->mapToGlobal(position));
    }
}

// Action slots (stubs)
void StructWindow::onExpandAllAction()
{
    expandAll();
}

void StructWindow::onCollapseAllAction()
{
    collapseAll();
}

void StructWindow::onRefreshAction()
{
    refreshStructures();
}

void StructWindow::onAddStructureAction()
{
    emit addStructureRequested();
}

void StructWindow::onEditStructureAction()
{
    // TODO: Implement
    qCDebug(structWindow) << "Edit structure action triggered";
}

void StructWindow::onDeleteStructureAction()
{
    // TODO: Implement
    qCDebug(structWindow) << "Delete structure action triggered";
}

void StructWindow::onDuplicateStructureAction()
{
    // TODO: Implement
    qCDebug(structWindow) << "Duplicate structure action triggered";
}

void StructWindow::onShowDetailsAction()
{
    // TODO: Implement
    qCDebug(structWindow) << "Show details action triggered";
}

void StructWindow::onSearchTextChanged(const QString &text)
{
    m_currentFilter = text;
    applySearchFilter();
}

void StructWindow::onClearSearchClicked()
{
    m_searchEdit->clear();
    clearSearchFilter();
}

void StructWindow::onFilterButtonClicked()
{
    showFilterDialog();
}

void StructWindow::setSearchFilter(const QString &filter)
{
    m_searchEdit->setText(filter);
}

void StructWindow::clearSearchFilter()
{
    m_currentFilter.clear();
    applySearchFilter();
}

void StructWindow::applySearchFilter()
{
    // TODO: Implement search filtering
    qCDebug(structWindow) << "Applying search filter:" << m_currentFilter;
}

void StructWindow::showFilterDialog()
{
    // TODO: Implement filter dialog
    qCDebug(structWindow) << "Show filter dialog";
}

void StructWindow::saveExpansionState()
{
    // TODO: Implement expansion state saving
}

void StructWindow::restoreExpansionState()
{
    // TODO: Implement expansion state restoration
}

// StructureTreeItem implementation
StructWindow::StructureTreeItem::StructureTreeItem(ItemType type)
    : QTreeWidgetItem(type)
    , m_isExpanded(false)
{
    updateAppearance();
}

StructWindow::StructureTreeItem::StructureTreeItem(QTreeWidget *parent, ItemType type)
    : QTreeWidgetItem(parent, type)
    , m_isExpanded(false)
{
    updateAppearance();
}

StructWindow::StructureTreeItem::StructureTreeItem(QTreeWidgetItem *parent, ItemType type)
    : QTreeWidgetItem(parent, type)
    , m_isExpanded(false)
{
    updateAppearance();
}

void StructWindow::StructureTreeItem::setFieldData(const QJsonObject &data)
{
    m_fieldData = data;
}

QJsonObject StructWindow::StructureTreeItem::getFieldData() const
{
    return m_fieldData;
}

void StructWindow::StructureTreeItem::setFieldPath(const QString &path)
{
    m_fieldPath = path;
}

QString StructWindow::StructureTreeItem::getFieldPath() const
{
    return m_fieldPath;
}

void StructWindow::StructureTreeItem::setFieldType(const QString &type)
{
    m_fieldType = type;
}

QString StructWindow::StructureTreeItem::getFieldType() const
{
    return m_fieldType;
}

void StructWindow::StructureTreeItem::updateAppearance()
{
    // Update styling based on type
    switch (getItemType()) {
        case StructureType:
            setData(0, Qt::FontRole, QFont("", -1, QFont::Bold));
            break;
        case FieldType:
            setData(0, Qt::FontRole, QFont());
            break;
        default:
            break;
    }
}

bool StructWindow::StructureTreeItem::isDraggable() const
{
    return getItemType() == FieldType;
}

QMimeData* StructWindow::StructureTreeItem::createDragData() const
{
    QMimeData *mimeData = new QMimeData();
    
    // Set custom MIME type for structure fields
    mimeData->setData("application/x-monitor-field", m_fieldPath.toUtf8());
    mimeData->setText(m_fieldPath);
    
    // Include field data as JSON
    QJsonObject dragData = m_fieldData;
    dragData["fieldPath"] = m_fieldPath;
    QJsonDocument doc(dragData);
    mimeData->setData("application/json", doc.toJson());
    
    return mimeData;
}

// StructTreeWidget implementation
StructTreeWidget::StructTreeWidget(QWidget *parent)
    : QTreeWidget(parent)
    , m_dragInProgress(false)
{
}

void StructTreeWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = event->pos();
    }
    QTreeWidget::mousePressEvent(event);
}

void StructTreeWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        QTreeWidget::mouseMoveEvent(event);
        return;
    }
    
    if ((event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance()) {
        QTreeWidget::mouseMoveEvent(event);
        return;
    }
    
    QTreeWidgetItem *item = itemAt(m_dragStartPosition);
    if (item) {
        auto structItem = dynamic_cast<StructWindow::StructureTreeItem*>(item);
        if (structItem && structItem->isDraggable()) {
            m_dragInProgress = true;
            emit itemDragStarted(item);
            startDrag(Qt::CopyAction);
        }
    }
    
    QTreeWidget::mouseMoveEvent(event);
}

void StructTreeWidget::startDrag(Qt::DropActions supportedActions)
{
    QTreeWidgetItem *item = currentItem();
    if (!item) {
        return;
    }
    
    auto structItem = dynamic_cast<StructWindow::StructureTreeItem*>(item);
    if (!structItem || !structItem->isDraggable()) {
        return;
    }
    
    QMimeData *mimeData = structItem->createDragData();
    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    
    // Create drag pixmap (simplified)
    QPixmap pixmap(200, 20);
    pixmap.fill(Qt::white);
    drag->setPixmap(pixmap);
    
    Qt::DropAction result = drag->exec(supportedActions);
    emit itemDragFinished(item, result != Qt::IgnoreAction);
    
    m_dragInProgress = false;
}

void StructTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept our own drag events
    if (event->mimeData()->hasFormat("application/x-monitor-field")) {
        event->acceptProposedAction();
    } else {
        QTreeWidget::dragEnterEvent(event);
    }
}

void StructTreeWidget::dragMoveEvent(QDragMoveEvent *event)
{
    // Handle drag move
    QTreeWidget::dragMoveEvent(event);
}

void StructTreeWidget::dropEvent(QDropEvent *event)
{
    // Handle drop (for future internal reorganization)
    QTreeWidget::dropEvent(event);
}

// Missing StructWindow implementations
void StructWindow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
}

void StructWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Additional resize handling can be added here
}

void StructWindow::onStructureAdded(const QString &name)
{
    qCDebug(structWindow) << "Structure added:" << name;
    refreshStructures();
}

void StructWindow::onStructureRemoved(const QString &name)
{
    qCDebug(structWindow) << "Structure removed:" << name;
    refreshStructures();
}

void StructWindow::onStructureUpdated(const QString &name)
{
    qCDebug(structWindow) << "Structure updated:" << name;
    refreshStructures();
}