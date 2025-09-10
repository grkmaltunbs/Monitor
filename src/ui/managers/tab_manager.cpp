#include "tab_manager.h"
#include "../windows/struct_window.h"
#include "window_manager.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QMdiArea>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QApplication>
#include <QtCore/QLoggingCategory>

Q_LOGGING_CATEGORY(tabManager, "Monitor.TabManager")

TabManager::TabManager(QWidget *parent)
    : QObject(parent)
    , m_tabWidget(nullptr)
    , m_contextMenu(nullptr)
    , m_contextMenuIndex(-1)
    , m_inlineEditor(nullptr)
    , m_editingIndex(-1)
    , m_maxTabs(20)
    , m_tabCounter(0)
    , m_defaultTabPrefix("Tab")
    , m_allowTabReorder(true)
    , m_allowTabClose(true)
{
    setupTabWidget();
    setupContextMenu();
    
    qCInfo(tabManager) << "TabManager initialized";
}

TabManager::~TabManager()
{
    qCInfo(tabManager) << "TabManager destroyed";
}

void TabManager::setupTabWidget()
{
    m_tabWidget = new QTabWidget();
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setUsesScrollButtons(true);
    
    // Connect signals
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &TabManager::onCurrentTabChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &TabManager::onTabCloseRequested);
    connect(m_tabWidget->tabBar(), &QTabBar::tabMoved, this, &TabManager::onTabMoved);
    
    // Connect standard tab bar signals for enhanced functionality
    connect(m_tabWidget->tabBar(), &QTabBar::tabBarDoubleClicked, this, &TabManager::onTabBarDoubleClicked);
    
    // Install event filter for context menu
    m_tabWidget->tabBar()->installEventFilter(this);
    
    qCDebug(tabManager) << "Tab widget setup completed";
}

void TabManager::setupContextMenu()
{
    m_contextMenu = new QMenu();
    
    // Tab management actions
    QAction *renameAction = m_contextMenu->addAction("Rename Tab");
    connect(renameAction, &QAction::triggered, this, &TabManager::onRenameTabAction);
    
    QAction *duplicateAction = m_contextMenu->addAction("Duplicate Tab");
    connect(duplicateAction, &QAction::triggered, this, &TabManager::onDuplicateTabAction);
    
    m_contextMenu->addSeparator();
    
    QAction *closeAction = m_contextMenu->addAction("Close Tab");
    connect(closeAction, &QAction::triggered, this, &TabManager::onCloseTabAction);
    
    QAction *closeOthersAction = m_contextMenu->addAction("Close Other Tabs");
    connect(closeOthersAction, &QAction::triggered, this, &TabManager::onCloseOtherTabsAction);
    
    QAction *closeAllAction = m_contextMenu->addAction("Close All Tabs");
    connect(closeAllAction, &QAction::triggered, this, &TabManager::onCloseAllTabsAction);
    
    qCDebug(tabManager) << "Context menu setup completed";
}

QString TabManager::createTab(const QString &name)
{
    if (!canCreateTab()) {
        qCWarning(tabManager) << "Cannot create tab: maximum limit reached";
        return QString();
    }
    
    QString tabId = generateTabId();
    QString tabName = name.isEmpty() ? generateDefaultTabName() : name;
    
    // Ensure unique name
    tabName = getUniqueTabName(tabName);
    
    // Create tab data
    TabData tabData;
    tabData.id = tabId;
    tabData.name = tabName;
    tabData.isDefault = (m_tabs.isEmpty());
    
    // Create tab content
    tabData.content = createTabContent(tabId);
    tabData.structWindow = createStructWindow(tabId);
    tabData.windowManager = createWindowManager(tabId);
    
    // Add to tab widget
    int index = m_tabWidget->addTab(tabData.content, tabName);
    m_tabs[tabId] = tabData;
    m_indexToId[index] = tabId;
    
    // Set as active if it's the first tab
    if (m_tabs.count() == 1) {
        m_tabWidget->setCurrentIndex(index);
    }
    
    emit tabCreated(tabId, tabName);
    emit tabCountChanged(m_tabs.count());
    
    qCDebug(tabManager) << "Created tab:" << tabName << "with ID:" << tabId;
    return tabId;
}

bool TabManager::deleteTab(const QString &tabId)
{
    if (!m_tabs.contains(tabId)) {
        qCWarning(tabManager) << "Cannot delete tab: ID not found:" << tabId;
        return false;
    }
    
    if (m_tabs.count() == 1) {
        qCWarning(tabManager) << "Cannot delete last tab";
        return false;
    }
    
    TabData tabData = m_tabs[tabId];
    int index = getTabIndex(tabId);
    
    if (index >= 0) {
        // Remove from tab widget
        m_tabWidget->removeTab(index);
        
        // Clean up tab data
        if (tabData.content) {
            tabData.content->deleteLater();
        }
        if (tabData.structWindow) {
            tabData.structWindow->deleteLater();
        }
        if (tabData.windowManager) {
            tabData.windowManager->deleteLater();
        }
        
        // Remove from internal structures
        m_tabs.remove(tabId);
        updateTabIds();
        
        emit tabDeleted(tabId);
        emit tabCountChanged(m_tabs.count());
        
        qCDebug(tabManager) << "Deleted tab:" << tabData.name << "with ID:" << tabId;
        return true;
    }
    
    return false;
}

bool TabManager::renameTab(const QString &tabId, const QString &newName)
{
    if (!m_tabs.contains(tabId) || newName.isEmpty()) {
        return false;
    }
    
    if (!isValidTabName(newName, tabId)) {
        return false;
    }
    
    TabData &tabData = m_tabs[tabId];
    QString oldName = tabData.name;
    tabData.name = newName;
    
    int index = getTabIndex(tabId);
    if (index >= 0) {
        m_tabWidget->setTabText(index, newName);
        emit tabRenamed(tabId, oldName, newName);
        
        qCDebug(tabManager) << "Renamed tab from:" << oldName << "to:" << newName;
        return true;
    }
    
    return false;
}

QString TabManager::getActiveTabId() const
{
    int currentIndex = m_tabWidget->currentIndex();
    if (m_indexToId.contains(currentIndex)) {
        return m_indexToId[currentIndex];
    }
    return QString();
}

QStringList TabManager::getTabIds() const
{
    return m_tabs.keys();
}

QString TabManager::getTabName(const QString &tabId) const
{
    if (m_tabs.contains(tabId)) {
        return m_tabs[tabId].name;
    }
    return QString();
}

int TabManager::getTabCount() const
{
    return m_tabs.count();
}

int TabManager::getTabIndex(const QString &tabId) const
{
    for (auto it = m_indexToId.constBegin(); it != m_indexToId.constEnd(); ++it) {
        if (it.value() == tabId) {
            return it.key();
        }
    }
    return -1;
}

bool TabManager::canCreateTab() const
{
    return m_tabs.count() < m_maxTabs;
}

// Private helper methods
QWidget* TabManager::createTabContent(const QString &tabId)
{
    QWidget *content = new QWidget();
    content->setObjectName(QString("TabContent_%1").arg(tabId));
    
    // Create main splitter for the tab
    QSplitter *splitter = new QSplitter(Qt::Horizontal, content);
    
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(splitter);
    
    return content;
}

StructWindow* TabManager::createStructWindow(const QString &tabId)
{
    Q_UNUSED(tabId)
    // Create a basic StructWindow - will be fully implemented in future
    StructWindow *structWindow = new StructWindow();
    return structWindow;
}

WindowManager* TabManager::createWindowManager(const QString &tabId)
{
    WindowManager *windowManager = new WindowManager(tabId);
    return windowManager;
}

QString TabManager::generateTabId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString TabManager::generateDefaultTabName() const
{
    return QString("%1 %2").arg(m_defaultTabPrefix).arg(++m_tabCounter);
}

bool TabManager::isValidTabName(const QString &name, const QString &excludeId) const
{
    if (name.trimmed().isEmpty()) {
        return false;
    }
    
    // Check for duplicate names
    for (auto it = m_tabs.constBegin(); it != m_tabs.constEnd(); ++it) {
        if (it.key() != excludeId && it.value().name == name) {
            return false;
        }
    }
    
    return true;
}

QString TabManager::getUniqueTabName(const QString &baseName) const
{
    QString name = baseName;
    int counter = 1;
    
    while (!isValidTabName(name)) {
        name = QString("%1 (%2)").arg(baseName).arg(counter++);
    }
    
    return name;
}

void TabManager::updateTabIds()
{
    m_indexToId.clear();
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QWidget *widget = m_tabWidget->widget(i);
        for (auto it = m_tabs.constBegin(); it != m_tabs.constEnd(); ++it) {
            if (it.value().content == widget) {
                m_indexToId[i] = it.key();
                break;
            }
        }
    }
}

// Slot implementations
void TabManager::onCurrentTabChanged(int index)
{
    if (m_indexToId.contains(index)) {
        QString tabId = m_indexToId[index];
        emit activeTabChanged(tabId, index);
        qCDebug(tabManager) << "Active tab changed to:" << tabId;
    }
}

void TabManager::onTabCloseRequested(int index)
{
    if (m_indexToId.contains(index)) {
        QString tabId = m_indexToId[index];
        deleteTab(tabId);
    }
}

void TabManager::onTabBarDoubleClicked(int index)
{
    startInlineEdit(index);
}

void TabManager::onTabMoved(int from, int to)
{
    Q_UNUSED(from)
    Q_UNUSED(to)
    updateTabIds();
    // Additional reorder logic can be implemented here
}

bool TabManager::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_tabWidget->tabBar()) {
        if (event->type() == QEvent::ContextMenu) {
            QContextMenuEvent *contextEvent = static_cast<QContextMenuEvent*>(event);
            showContextMenu(contextEvent->pos());
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void TabManager::showContextMenu(const QPoint &position)
{
    if (m_contextMenu && m_tabWidget) {
        QTabBar *tabBar = m_tabWidget->tabBar();
        m_contextMenuIndex = tabBar->tabAt(position);
        if (m_contextMenuIndex >= 0) {
            m_contextMenu->popup(tabBar->mapToGlobal(position));
        }
    }
}

// Context menu action slots (stubs for now)
void TabManager::onRenameTabAction()
{
    if (m_contextMenuIndex >= 0) {
        startInlineEdit(m_contextMenuIndex);
    }
}

void TabManager::onCloseTabAction()
{
    if (m_contextMenuIndex >= 0 && m_indexToId.contains(m_contextMenuIndex)) {
        QString tabId = m_indexToId[m_contextMenuIndex];
        deleteTab(tabId);
    }
}

void TabManager::onCloseOtherTabsAction()
{
    // TODO: Implement
    qCDebug(tabManager) << "Close other tabs action triggered";
}

void TabManager::onCloseAllTabsAction()
{
    // TODO: Implement  
    qCDebug(tabManager) << "Close all tabs action triggered";
}

void TabManager::onDuplicateTabAction()
{
    // TODO: Implement
    qCDebug(tabManager) << "Duplicate tab action triggered";
}

// Inline editing (simplified implementation)
void TabManager::startInlineEdit(int index)
{
    Q_UNUSED(index)
    // TODO: Implement inline tab name editing
    qCDebug(tabManager) << "Start inline edit for tab at index:" << index;
}

void TabManager::finishInlineEdit()
{
    // TODO: Implement
}

void TabManager::cancelInlineEdit()
{
    // TODO: Implement
}

// Settings and persistence (stubs)
QJsonObject TabManager::saveState() const
{
    QJsonObject state;
    state["activeTab"] = getActiveTabId();
    state["tabCount"] = getTabCount();
    
    QJsonArray tabsArray;
    for (auto it = m_tabs.constBegin(); it != m_tabs.constEnd(); ++it) {
        QJsonObject tabObject;
        tabObject["id"] = it.key();
        tabObject["name"] = it.value().name;
        tabObject["isDefault"] = it.value().isDefault;
        tabsArray.append(tabObject);
    }
    state["tabs"] = tabsArray;
    
    return state;
}

bool TabManager::restoreState(const QJsonObject &state)
{
    Q_UNUSED(state)
    // TODO: Implement state restoration
    qCDebug(tabManager) << "Restore state called";
    return true;
}

// CustomTabBar implementation
CustomTabBar::CustomTabBar(QWidget *parent)
    : QTabBar(parent)
    , m_dragInProgress(false)
{
}

void CustomTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = event->pos();
    }
    QTabBar::mousePressEvent(event);
}

void CustomTabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        QTabBar::mouseMoveEvent(event);
        return;
    }
    
    if ((event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance()) {
        QTabBar::mouseMoveEvent(event);
        return;
    }
    
    m_dragInProgress = true;
    QTabBar::mouseMoveEvent(event);
}

void CustomTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragInProgress = false;
    QTabBar::mouseReleaseEvent(event);
}

void CustomTabBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    int index = tabAt(event->pos());
    if (index >= 0) {
        emit tabDoubleClicked(index);
    }
    QTabBar::mouseDoubleClickEvent(event);
}

void CustomTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    emit contextMenuRequested(event->pos());
}

void CustomTabBar::paintEvent(QPaintEvent *event)
{
    // Use default painting for now
    QTabBar::paintEvent(event);
}

// Missing TabManager slot implementations
void TabManager::setActiveTab(const QString &tabId)
{
    int index = getTabIndex(tabId);
    if (index >= 0) {
        m_tabWidget->setCurrentIndex(index);
    }
}

void TabManager::setActiveTab(int index)
{
    if (index >= 0 && index < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(index);
    }
}

void TabManager::onTabDoubleClicked(int index)
{
    startInlineEdit(index);
}