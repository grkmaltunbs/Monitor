#ifndef TAB_MANAGER_H
#define TAB_MANAGER_H

#include <QObject>
#include <QTabWidget>
#include <QTabBar>
#include <QWidget>
#include <QString>
#include <QHash>
#include <QUuid>
#include <QJsonObject>
#include <QJsonArray>
#include <QLineEdit>
#include <QMenu>
#include <QContextMenuEvent>
#include <QMdiArea>

// Forward declarations
class StructWindow;
class WindowManager;

/**
 * @brief Manages the tab system for the Monitor Application
 * 
 * Provides comprehensive tab management including:
 * - Dynamic tab creation and deletion
 * - Tab renaming (double-click or context menu)
 * - Drag-to-reorder tabs
 * - Tab state persistence
 * - Window management within tabs
 */
class TabManager : public QObject
{
    Q_OBJECT

public:
    explicit TabManager(QWidget *parent = nullptr);
    ~TabManager();

    // Core tab operations
    QString createTab(const QString &name = QString());
    bool deleteTab(const QString &tabId);
    bool renameTab(const QString &tabId, const QString &newName);
    bool reorderTab(const QString &tabId, int newIndex);

    // Tab access and information
    QTabWidget* getTabWidget() const { return m_tabWidget; }
    QString getActiveTabId() const;
    QStringList getTabIds() const;
    QString getTabName(const QString &tabId) const;
    int getTabCount() const;
    int getTabIndex(const QString &tabId) const;
    
    // Tab content management
    StructWindow* getStructWindow(const QString &tabId) const;
    WindowManager* getWindowManager(const QString &tabId) const;
    QWidget* getTabContent(const QString &tabId) const;
    
    // Settings and persistence
    QJsonObject saveState() const;
    bool restoreState(const QJsonObject &state);
    
    // Tab limits and validation
    void setMaxTabs(int maxTabs) { m_maxTabs = maxTabs; }
    int getMaxTabs() const { return m_maxTabs; }
    bool canCreateTab() const;

public slots:
    void setActiveTab(const QString &tabId);
    void setActiveTab(int index);
    void showContextMenu(const QPoint &position);
    void onTabDoubleClicked(int index);
    void onTabCloseRequested(int index);

signals:
    void tabCreated(const QString &tabId, const QString &name);
    void tabDeleted(const QString &tabId);
    void tabRenamed(const QString &tabId, const QString &oldName, const QString &newName);
    void tabReordered(const QString &tabId, int oldIndex, int newIndex);
    void activeTabChanged(const QString &tabId, int index);
    void tabCountChanged(int count);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onCurrentTabChanged(int index);
    void onTabBarDoubleClicked(int index);
    void onTabMoved(int from, int to);
    
    // Context menu actions
    void onRenameTabAction();
    void onCloseTabAction();
    void onCloseOtherTabsAction();
    void onCloseAllTabsAction();
    void onDuplicateTabAction();

private:
    struct TabData {
        QString id;
        QString name;
        QWidget *content;
        StructWindow *structWindow;
        WindowManager *windowManager;
        QJsonObject settings;
        bool isDefault;
        
        TabData() : content(nullptr), structWindow(nullptr), 
                   windowManager(nullptr), isDefault(false) {}
    };

    // UI setup
    void setupTabWidget();
    void setupContextMenu();
    void enableTabReordering();
    
    // Tab content creation
    QWidget* createTabContent(const QString &tabId);
    StructWindow* createStructWindow(const QString &tabId);
    WindowManager* createWindowManager(const QString &tabId);
    
    // Tab management internals
    QString generateTabId() const;
    QString generateDefaultTabName() const;
    bool isValidTabName(const QString &name, const QString &excludeId = QString()) const;
    QString getUniqueTabName(const QString &baseName) const;
    void updateTabIds();
    
    // Inline editing support
    void startInlineEdit(int index);
    void finishInlineEdit();
    void cancelInlineEdit();
    
    // Tab widget and data
    QTabWidget *m_tabWidget;
    QHash<QString, TabData> m_tabs;
    QHash<int, QString> m_indexToId;
    
    // Context menu
    QMenu *m_contextMenu;
    int m_contextMenuIndex;
    
    // Inline editing
    QLineEdit *m_inlineEditor;
    int m_editingIndex;
    QString m_originalName;
    
    // Configuration
    int m_maxTabs;
    mutable int m_tabCounter;
    QString m_defaultTabPrefix;
    bool m_allowTabReorder;
    bool m_allowTabClose;
    
    // Style and appearance
    void applyTabStyles();
    void updateTabStyle(int index);
};

/**
 * @brief Custom tab bar with enhanced drag-and-drop support
 */
class CustomTabBar : public QTabBar
{
    Q_OBJECT

public:
    explicit CustomTabBar(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

signals:
    void tabDoubleClicked(int index);
    void contextMenuRequested(const QPoint &position);

private:
    QPoint m_dragStartPosition;
    bool m_dragInProgress;
};

#endif // TAB_MANAGER_H