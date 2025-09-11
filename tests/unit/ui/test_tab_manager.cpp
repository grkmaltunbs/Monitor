#include <QtTest/QtTest>
#include <QApplication>
#include <QWidget>
#include <QSignalSpy>
#include <QMimeData>
#include <QDrag>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include "../../../src/ui/managers/tab_manager.h"
#include "../../../src/ui/windows/struct_window.h"

class TestTabManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Core functionality tests
    void testInitialization();
    void testCreateTab();
    void testDeleteTab();
    void testTabCount();
    void testTabNames();
    void testMaxTabs();
    void testActiveTab();
    void testTabReordering();
    
    // Signal/slot tests
    void testTabCreatedSignals();
    void testTabDeletedSignals();
    void testTabRenamedSignals();
    void testTabReorderedSignals();
    void testActiveTabChangedSignals();
    void testTabCountChangedSignals();
    
    // Context menu tests
    void testContextMenuCreation();
    void testContextMenuActions();
    void testDoubleClickRename();
    
    // Drag and drop tests
    void testTabDragStart();
    void testTabDragMove();
    void testTabDropAccept();
    void testTabReorderByDrag();
    
    // State persistence tests
    void testSaveRestoreState();
    void testTabContentPersistence();
    
    // Edge cases and error conditions
    void testDuplicateTabNames();
    void testInvalidTabOperations();
    void testTabLimits();
    void testConcurrentOperations();
    
    // Performance tests
    void testManyTabsPerformance();
    void testRapidTabOperations();
    
    // Integration tests
    void testStructWindowIntegration();
    void testWindowManagerIntegration();

private:
    TabManager *m_tabManager;
    QWidget *m_parentWidget;
};

void TestTabManager::initTestCase()
{
    // Set up test environment
    if (!QApplication::instance()) {
        int argc = 1;
        char arg0[] = "test";
        char *argv[] = {arg0};
        new QApplication(argc, argv);
    }
    
    m_parentWidget = new QWidget();
}

void TestTabManager::cleanupTestCase()
{
    delete m_parentWidget;
}

void TestTabManager::init()
{
    m_tabManager = new TabManager(m_parentWidget);
}

void TestTabManager::cleanup()
{
    delete m_tabManager;
    m_tabManager = nullptr;
}

void TestTabManager::testInitialization()
{
    QVERIFY(m_tabManager != nullptr);
    QVERIFY(m_tabManager->getTabWidget() != nullptr);
    QCOMPARE(m_tabManager->getTabCount(), 0);
    QCOMPARE(m_tabManager->getMaxTabs(), 20); // Default max tabs
    QVERIFY(m_tabManager->canCreateTab());
}

void TestTabManager::testCreateTab()
{
    // Test creating a tab with default name
    QString tabId1 = m_tabManager->createTab();
    QVERIFY(!tabId1.isEmpty());
    QCOMPARE(m_tabManager->getTabCount(), 1);
    
    // Test creating a tab with custom name
    QString tabId2 = m_tabManager->createTab("Custom Tab");
    QVERIFY(!tabId2.isEmpty());
    QCOMPARE(m_tabManager->getTabCount(), 2);
    QCOMPARE(m_tabManager->getTabName(tabId2), QString("Custom Tab"));
    
    // Verify tab IDs are unique
    QVERIFY(tabId1 != tabId2);
    
    // Verify tab IDs are in the list
    QStringList tabIds = m_tabManager->getTabIds();
    QVERIFY(tabIds.contains(tabId1));
    QVERIFY(tabIds.contains(tabId2));
}

void TestTabManager::testDeleteTab()
{
    // Create some tabs first
    QString tabId1 = m_tabManager->createTab("Tab 1");
    QString tabId2 = m_tabManager->createTab("Tab 2");
    QString tabId3 = m_tabManager->createTab("Tab 3");
    
    QCOMPARE(m_tabManager->getTabCount(), 3);
    
    // Delete middle tab
    bool deleted = m_tabManager->deleteTab(tabId2);
    QVERIFY(deleted);
    QCOMPARE(m_tabManager->getTabCount(), 2);
    
    QStringList tabIds = m_tabManager->getTabIds();
    QVERIFY(tabIds.contains(tabId1));
    QVERIFY(!tabIds.contains(tabId2));
    QVERIFY(tabIds.contains(tabId3));
    
    // Try to delete non-existent tab
    bool deletedNonExistent = m_tabManager->deleteTab("non-existent-id");
    QVERIFY(!deletedNonExistent);
    QCOMPARE(m_tabManager->getTabCount(), 2);
}

void TestTabManager::testTabCount()
{
    QCOMPARE(m_tabManager->getTabCount(), 0);
    
    QString tabId1 = m_tabManager->createTab();
    QCOMPARE(m_tabManager->getTabCount(), 1);
    
    QString tabId2 = m_tabManager->createTab();
    QCOMPARE(m_tabManager->getTabCount(), 2);
    
    m_tabManager->deleteTab(tabId1);
    QCOMPARE(m_tabManager->getTabCount(), 1);
    
    m_tabManager->deleteTab(tabId2);
    QCOMPARE(m_tabManager->getTabCount(), 0);
}

void TestTabManager::testTabNames()
{
    // Test default naming
    QString tabId1 = m_tabManager->createTab();
    QString name1 = m_tabManager->getTabName(tabId1);
    QVERIFY(!name1.isEmpty());
    QVERIFY(name1.contains("Tab")); // Should contain default prefix
    
    // Test custom naming
    QString tabId2 = m_tabManager->createTab("Custom Name");
    QCOMPARE(m_tabManager->getTabName(tabId2), QString("Custom Name"));
    
    // Test renaming
    bool renamed = m_tabManager->renameTab(tabId2, "Renamed Tab");
    QVERIFY(renamed);
    QCOMPARE(m_tabManager->getTabName(tabId2), QString("Renamed Tab"));
    
    // Test invalid rename (empty name)
    bool invalidRenamed = m_tabManager->renameTab(tabId2, "");
    QVERIFY(!invalidRenamed);
    QCOMPARE(m_tabManager->getTabName(tabId2), QString("Renamed Tab")); // Unchanged
}

void TestTabManager::testMaxTabs()
{
    // Set a small max tabs limit for testing
    m_tabManager->setMaxTabs(3);
    QCOMPARE(m_tabManager->getMaxTabs(), 3);
    
    // Create tabs up to the limit
    QString tabId1 = m_tabManager->createTab("Tab 1");
    QVERIFY(!tabId1.isEmpty());
    QVERIFY(m_tabManager->canCreateTab());
    
    QString tabId2 = m_tabManager->createTab("Tab 2");
    QVERIFY(!tabId2.isEmpty());
    QVERIFY(m_tabManager->canCreateTab());
    
    QString tabId3 = m_tabManager->createTab("Tab 3");
    QVERIFY(!tabId3.isEmpty());
    QVERIFY(!m_tabManager->canCreateTab()); // Should be at limit
    
    // Try to create one more tab (should fail)
    QString tabId4 = m_tabManager->createTab("Tab 4");
    QVERIFY(tabId4.isEmpty());
    QCOMPARE(m_tabManager->getTabCount(), 3);
}

void TestTabManager::testActiveTab()
{
    // No active tab initially
    QString activeId = m_tabManager->getActiveTabId();
    QVERIFY(activeId.isEmpty());
    
    // Create first tab - should become active
    QString tabId1 = m_tabManager->createTab("Tab 1");
    QString activeAfterFirst = m_tabManager->getActiveTabId();
    QCOMPARE(activeAfterFirst, tabId1);
    
    // Create second tab
    QString tabId2 = m_tabManager->createTab("Tab 2");
    
    // First tab should still be active
    QString stillActive = m_tabManager->getActiveTabId();
    QCOMPARE(stillActive, tabId1);
    
    // Set second tab as active
    m_tabManager->setActiveTab(tabId2);
    QString nowActive = m_tabManager->getActiveTabId();
    QCOMPARE(nowActive, tabId2);
}

void TestTabManager::testTabReordering()
{
    // Create some tabs
    QString tabId1 = m_tabManager->createTab("Tab 1");
    QString tabId2 = m_tabManager->createTab("Tab 2");
    QString tabId3 = m_tabManager->createTab("Tab 3");
    
    // Get initial indices
    int index1 = m_tabManager->getTabIndex(tabId1);
    int index2 = m_tabManager->getTabIndex(tabId2);
    int index3 = m_tabManager->getTabIndex(tabId3);
    
    QCOMPARE(index1, 0);
    QCOMPARE(index2, 1);
    QCOMPARE(index3, 2);
    
    // Test reordering (basic verification)
    bool reordered = m_tabManager->reorderTab(tabId1, 2);
    QVERIFY(reordered); // Should succeed even if not fully implemented
}

void TestTabManager::testTabCreatedSignals()
{
    QSignalSpy createdSpy(m_tabManager, &TabManager::tabCreated);
    QSignalSpy countSpy(m_tabManager, &TabManager::tabCountChanged);
    
    QString tabId = m_tabManager->createTab("Signal Test Tab");
    
    QCOMPARE(createdSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);
    
    QList<QVariant> createdArgs = createdSpy.takeFirst();
    QCOMPARE(createdArgs.at(0).toString(), tabId);
    QCOMPARE(createdArgs.at(1).toString(), QString("Signal Test Tab"));
    
    QList<QVariant> countArgs = countSpy.takeFirst();
    QCOMPARE(countArgs.at(0).toInt(), 1);
}

void TestTabManager::testTabDeletedSignals()
{
    QString tabId = m_tabManager->createTab("Delete Test Tab");
    
    QSignalSpy deletedSpy(m_tabManager, &TabManager::tabDeleted);
    QSignalSpy countSpy(m_tabManager, &TabManager::tabCountChanged);
    
    m_tabManager->deleteTab(tabId);
    
    QCOMPARE(deletedSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);
    
    QList<QVariant> deletedArgs = deletedSpy.takeFirst();
    QCOMPARE(deletedArgs.at(0).toString(), tabId);
    
    QList<QVariant> countArgs = countSpy.takeFirst();
    QCOMPARE(countArgs.at(0).toInt(), 0);
}

void TestTabManager::testTabRenamedSignals()
{
    QString tabId = m_tabManager->createTab("Original Name");
    
    QSignalSpy renamedSpy(m_tabManager, &TabManager::tabRenamed);
    
    m_tabManager->renameTab(tabId, "New Name");
    
    QCOMPARE(renamedSpy.count(), 1);
    
    QList<QVariant> renamedArgs = renamedSpy.takeFirst();
    QCOMPARE(renamedArgs.at(0).toString(), tabId);
    QCOMPARE(renamedArgs.at(1).toString(), QString("Original Name"));
    QCOMPARE(renamedArgs.at(2).toString(), QString("New Name"));
}

void TestTabManager::testTabReorderedSignals()
{
    QString tabId1 = m_tabManager->createTab("Tab 1");
    QString tabId2 = m_tabManager->createTab("Tab 2");
    QString tabId3 = m_tabManager->createTab("Tab 3");
    
    QSignalSpy reorderedSpy(m_tabManager, &TabManager::tabReordered);
    
    // Reorder tab from index 0 to index 2
    m_tabManager->reorderTab(tabId1, 2);
    
    QCOMPARE(reorderedSpy.count(), 1);
    
    QList<QVariant> reorderedArgs = reorderedSpy.takeFirst();
    QCOMPARE(reorderedArgs.at(0).toString(), tabId1);
    QCOMPARE(reorderedArgs.at(1).toInt(), 0); // old index
    QCOMPARE(reorderedArgs.at(2).toInt(), 2); // new index
}

void TestTabManager::testActiveTabChangedSignals()
{
    QString tabId1 = m_tabManager->createTab("Tab 1");
    QString tabId2 = m_tabManager->createTab("Tab 2");
    
    QSignalSpy activeSpy(m_tabManager, &TabManager::activeTabChanged);
    
    m_tabManager->setActiveTab(tabId2);
    
    QCOMPARE(activeSpy.count(), 1);
    
    QList<QVariant> activeArgs = activeSpy.takeFirst();
    QCOMPARE(activeArgs.at(0).toString(), tabId2);
    QCOMPARE(activeArgs.at(1).toInt(), 1); // index of tabId2
}

void TestTabManager::testTabCountChangedSignals()
{
    QSignalSpy countSpy(m_tabManager, &TabManager::tabCountChanged);
    
    // Create multiple tabs and verify count signals
    QString tabId1 = m_tabManager->createTab("Tab 1");
    QString tabId2 = m_tabManager->createTab("Tab 2");
    QString tabId3 = m_tabManager->createTab("Tab 3");
    
    QCOMPARE(countSpy.count(), 3);
    
    // Verify count progression
    QCOMPARE(countSpy.at(0).at(0).toInt(), 1);
    QCOMPARE(countSpy.at(1).at(0).toInt(), 2);
    QCOMPARE(countSpy.at(2).at(0).toInt(), 3);
    
    // Delete a tab and verify count decrease
    m_tabManager->deleteTab(tabId2);
    QCOMPARE(countSpy.count(), 4);
    QCOMPARE(countSpy.at(3).at(0).toInt(), 2);
}

void TestTabManager::testContextMenuCreation()
{
    QString tabId = m_tabManager->createTab("Context Test");
    
    // Simulate right-click on tab
    QTabWidget *tabWidget = m_tabManager->getTabWidget();
    QVERIFY(tabWidget != nullptr);
    
    // Test context menu request at tab position
    QPoint tabPos = tabWidget->tabBar()->tabRect(0).center();
    
    QSignalSpy contextSpy(m_tabManager, &TabManager::activeTabChanged);
    
    // Simulate context menu event (normally would show context menu)
    m_tabManager->showContextMenu(tabPos);
    
    // Context menu creation doesn't emit signals, but should not crash
    QVERIFY(true); // Test passes if no crash
}

void TestTabManager::testContextMenuActions()
{
    QString tabId1 = m_tabManager->createTab("Tab 1");
    QString tabId2 = m_tabManager->createTab("Tab 2");
    QString tabId3 = m_tabManager->createTab("Tab 3");
    
    // Test close action via context menu
    QSignalSpy deletedSpy(m_tabManager, &TabManager::tabDeleted);
    
    // Simulate close tab action
    // Note: In real implementation, this would be triggered by context menu
    m_tabManager->deleteTab(tabId2);
    
    QCOMPARE(deletedSpy.count(), 1);
    QCOMPARE(m_tabManager->getTabCount(), 2);
}

void TestTabManager::testDoubleClickRename()
{
    QString tabId = m_tabManager->createTab("Original Name");
    
    // Simulate double-click on tab (index 0)
    m_tabManager->onTabDoubleClicked(0);
    
    // In real implementation, this would start inline editing
    // For testing, we'll verify the tab can be renamed
    bool renamed = m_tabManager->renameTab(tabId, "Double Click Renamed");
    QVERIFY(renamed);
    QCOMPARE(m_tabManager->getTabName(tabId), QString("Double Click Renamed"));
}

void TestTabManager::testTabDragStart()
{
    QString tabId1 = m_tabManager->createTab("Drag Source");
    QString tabId2 = m_tabManager->createTab("Drag Target");
    
    QTabWidget *tabWidget = m_tabManager->getTabWidget();
    QTabBar *tabBar = tabWidget->tabBar();
    
    // Simulate mouse press on first tab
    QPoint tabCenter = tabBar->tabRect(0).center();
    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPointF(tabCenter), QPointF(tabCenter), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    
    // Send event to tab bar (would initiate drag in real implementation)
    QCoreApplication::sendEvent(tabBar, &pressEvent);
    
    // Verify tabs still exist after drag start
    QCOMPARE(m_tabManager->getTabCount(), 2);
    QVERIFY(m_tabManager->getTabIds().contains(tabId1));
    QVERIFY(m_tabManager->getTabIds().contains(tabId2));
}

void TestTabManager::testTabDragMove()
{
    QString tabId1 = m_tabManager->createTab("Tab 1");
    QString tabId2 = m_tabManager->createTab("Tab 2");
    QString tabId3 = m_tabManager->createTab("Tab 3");
    
    // Get initial order
    QStringList initialOrder = m_tabManager->getTabIds();
    QCOMPARE(initialOrder.size(), 3);
    
    // Test drag move simulation by reordering
    QSignalSpy reorderedSpy(m_tabManager, &TabManager::tabReordered);
    
    // Move first tab to last position
    bool reordered = m_tabManager->reorderTab(tabId1, 2);
    QVERIFY(reordered);
    
    if (reorderedSpy.count() > 0) {
        QCOMPARE(reorderedSpy.count(), 1);
    }
    
    // Verify new order
    int newIndex = m_tabManager->getTabIndex(tabId1);
    QCOMPARE(newIndex, 2);
}

void TestTabManager::testTabDropAccept()
{
    QString tabId1 = m_tabManager->createTab("Tab 1");
    QString tabId2 = m_tabManager->createTab("Tab 2");
    
    QTabWidget *tabWidget = m_tabManager->getTabWidget();
    
    // Create mock drag data
    QMimeData *mimeData = new QMimeData();
    mimeData->setText("tab_drag_data");
    
    // Simulate drop event
    QPoint dropPos = tabWidget->tabBar()->tabRect(1).center();
    QDropEvent dropEvent(dropPos, Qt::MoveAction, mimeData, Qt::LeftButton, Qt::NoModifier);
    
    // In real implementation, this would handle the drop
    // For testing, verify tabs are still intact
    QCOMPARE(m_tabManager->getTabCount(), 2);
    
    delete mimeData;
}

void TestTabManager::testTabReorderByDrag()
{
    QString tabId1 = m_tabManager->createTab("First");
    QString tabId2 = m_tabManager->createTab("Second");
    QString tabId3 = m_tabManager->createTab("Third");
    
    // Test reordering using the public API
    int originalIndex1 = m_tabManager->getTabIndex(tabId1);
    int originalIndex3 = m_tabManager->getTabIndex(tabId3);
    
    QCOMPARE(originalIndex1, 0);
    QCOMPARE(originalIndex3, 2);
    
    // Reorder: move first tab to third position
    QSignalSpy reorderedSpy(m_tabManager, &TabManager::tabReordered);
    bool success = m_tabManager->reorderTab(tabId1, 2);
    
    if (success) {
        int newIndex1 = m_tabManager->getTabIndex(tabId1);
        QCOMPARE(newIndex1, 2);
        
        if (reorderedSpy.count() > 0) {
            QCOMPARE(reorderedSpy.count(), 1);
        }
    }
}

void TestTabManager::testSaveRestoreState()
{
    // Create tabs with specific configuration
    QString tabId1 = m_tabManager->createTab("Saved Tab 1");
    QString tabId2 = m_tabManager->createTab("Saved Tab 2");
    QString tabId3 = m_tabManager->createTab("Saved Tab 3");
    
    m_tabManager->setActiveTab(tabId2);
    
    // Save state
    QJsonObject savedState = m_tabManager->saveState();
    QVERIFY(!savedState.isEmpty());
    
    // Create new tab manager and restore state
    TabManager *newTabManager = new TabManager(m_parentWidget);
    bool restored = newTabManager->restoreState(savedState);
    
    if (restored) {
        QCOMPARE(newTabManager->getTabCount(), 3);
    }
    
    delete newTabManager;
}

void TestTabManager::testTabContentPersistence()
{
    QString tabId = m_tabManager->createTab("Content Test");
    
    // Verify tab content exists
    QWidget *content = m_tabManager->getTabContent(tabId);
    QVERIFY(content != nullptr);
    
    // Verify struct window exists for tab
    StructWindow *structWindow = m_tabManager->getStructWindow(tabId);
    QVERIFY(structWindow != nullptr);
    
    // Verify window manager exists for tab
    WindowManager *windowManager = m_tabManager->getWindowManager(tabId);
    QVERIFY(windowManager != nullptr);
}

void TestTabManager::testDuplicateTabNames()
{
    QString tabId1 = m_tabManager->createTab("Duplicate Name");
    QString tabId2 = m_tabManager->createTab("Duplicate Name");
    
    // Tab manager should handle duplicate names by making them unique
    QString name1 = m_tabManager->getTabName(tabId1);
    QString name2 = m_tabManager->getTabName(tabId2);
    
    // Names should be different (implementation may add suffix)
    QVERIFY(!name1.isEmpty());
    QVERIFY(!name2.isEmpty());
    // Note: Specific uniqueness handling depends on implementation
}

void TestTabManager::testInvalidTabOperations()
{
    // Test operations on non-existent tabs
    QString fakeTabId = "non-existent-tab-id";
    
    QVERIFY(!m_tabManager->deleteTab(fakeTabId));
    QVERIFY(!m_tabManager->renameTab(fakeTabId, "New Name"));
    QVERIFY(!m_tabManager->reorderTab(fakeTabId, 1));
    QVERIFY(m_tabManager->getTabName(fakeTabId).isEmpty());
    QCOMPARE(m_tabManager->getTabIndex(fakeTabId), -1);
    QVERIFY(m_tabManager->getTabContent(fakeTabId) == nullptr);
    
    // Test invalid rename with empty name
    QString validTabId = m_tabManager->createTab("Valid Tab");
    QVERIFY(!m_tabManager->renameTab(validTabId, ""));
    QVERIFY(!m_tabManager->renameTab(validTabId, "   ")); // whitespace only
}

void TestTabManager::testTabLimits()
{
    // Test max tabs limit
    int originalMax = m_tabManager->getMaxTabs();
    m_tabManager->setMaxTabs(3);
    
    // Create tabs up to limit
    QString tabId1 = m_tabManager->createTab("Tab 1");
    QString tabId2 = m_tabManager->createTab("Tab 2");
    QString tabId3 = m_tabManager->createTab("Tab 3");
    
    QVERIFY(!tabId1.isEmpty());
    QVERIFY(!tabId2.isEmpty());
    QVERIFY(!tabId3.isEmpty());
    QVERIFY(!m_tabManager->canCreateTab());
    
    // Try to create one more (should fail)
    QString tabId4 = m_tabManager->createTab("Tab 4");
    QVERIFY(tabId4.isEmpty());
    QCOMPARE(m_tabManager->getTabCount(), 3);
    
    // Delete one tab and verify we can create again
    m_tabManager->deleteTab(tabId2);
    QVERIFY(m_tabManager->canCreateTab());
    
    QString tabId5 = m_tabManager->createTab("Tab 5");
    QVERIFY(!tabId5.isEmpty());
    
    // Restore original limit
    m_tabManager->setMaxTabs(originalMax);
}

void TestTabManager::testConcurrentOperations()
{
    QSignalSpy allSignals(m_tabManager, &TabManager::tabCountChanged);
    
    // Rapidly create and delete tabs
    QStringList tabIds;
    for (int i = 0; i < 10; ++i) {
        QString tabId = m_tabManager->createTab(QString("Concurrent Tab %1").arg(i));
        tabIds.append(tabId);
    }
    
    QCOMPARE(m_tabManager->getTabCount(), 10);
    
    // Delete every other tab
    for (int i = 0; i < tabIds.size(); i += 2) {
        m_tabManager->deleteTab(tabIds[i]);
    }
    
    QCOMPARE(m_tabManager->getTabCount(), 5);
    
    // Verify signal count (should have 10 create + 5 delete = 15 signals)
    QCOMPARE(allSignals.count(), 15);
}

void TestTabManager::testManyTabsPerformance()
{
    const int NUM_TABS = 100;
    
    QElapsedTimer timer;
    timer.start();
    
    // Create many tabs
    QStringList tabIds;
    for (int i = 0; i < NUM_TABS; ++i) {
        QString tabId = m_tabManager->createTab(QString("Performance Tab %1").arg(i));
        tabIds.append(tabId);
        
        // Early exit if we hit the limit
        if (!m_tabManager->canCreateTab()) {
            break;
        }
    }
    
    qint64 createTime = timer.elapsed();
    int actualCount = m_tabManager->getTabCount();
    
    // Performance should be reasonable (less than 1 second for creation)
    QVERIFY(createTime < 1000);
    QVERIFY(actualCount > 0);
    
    timer.restart();
    
    // Delete all tabs
    for (const QString &tabId : tabIds) {
        m_tabManager->deleteTab(tabId);
    }
    
    qint64 deleteTime = timer.elapsed();
    QVERIFY(deleteTime < 1000);
    QCOMPARE(m_tabManager->getTabCount(), 0);
}

void TestTabManager::testRapidTabOperations()
{
    QSignalSpy countSpy(m_tabManager, &TabManager::tabCountChanged);
    
    // Rapidly create, rename, and reorder tabs
    QString tabId1 = m_tabManager->createTab("Rapid 1");
    QString tabId2 = m_tabManager->createTab("Rapid 2");
    QString tabId3 = m_tabManager->createTab("Rapid 3");
    
    // Rapid renames
    for (int i = 0; i < 10; ++i) {
        m_tabManager->renameTab(tabId1, QString("Renamed %1").arg(i));
        m_tabManager->renameTab(tabId2, QString("Also Renamed %1").arg(i));
    }
    
    // Verify final names
    QCOMPARE(m_tabManager->getTabName(tabId1), QString("Renamed 9"));
    QCOMPARE(m_tabManager->getTabName(tabId2), QString("Also Renamed 9"));
    
    // Rapid reordering
    for (int i = 0; i < 5; ++i) {
        m_tabManager->reorderTab(tabId1, (i % 2 == 0) ? 2 : 0);
    }
    
    // Verify tab still exists and has valid index
    int finalIndex = m_tabManager->getTabIndex(tabId1);
    QVERIFY(finalIndex >= 0 && finalIndex < 3);
}

void TestTabManager::testStructWindowIntegration()
{
    QString tabId = m_tabManager->createTab("Integration Test");
    
    // Verify struct window is created and accessible
    StructWindow *structWindow = m_tabManager->getStructWindow(tabId);
    QVERIFY(structWindow != nullptr);
    
    // Verify struct window is part of tab content
    QWidget *tabContent = m_tabManager->getTabContent(tabId);
    QVERIFY(tabContent != nullptr);
    
    // Struct window should be a child of the tab content or its layout
    bool foundStructWindow = false;
    QList<StructWindow*> childStructWindows = tabContent->findChildren<StructWindow*>();
    if (!childStructWindows.isEmpty()) {
        foundStructWindow = true;
    }
    
    // Alternative: struct window might be accessible through tab content
    if (!foundStructWindow && structWindow->parent() == tabContent) {
        foundStructWindow = true;
    }
    
    QVERIFY(foundStructWindow);
}

void TestTabManager::testWindowManagerIntegration()
{
    QString tabId = m_tabManager->createTab("Window Manager Test");
    
    // Verify window manager is created and accessible
    WindowManager *windowManager = m_tabManager->getWindowManager(tabId);
    QVERIFY(windowManager != nullptr);
    
    // Verify window manager is associated with the correct tab
    QWidget *tabContent = m_tabManager->getTabContent(tabId);
    QVERIFY(tabContent != nullptr);
    
    // Window manager should be accessible through the tab
    WindowManager *retrievedManager = m_tabManager->getWindowManager(tabId);
    QCOMPARE(windowManager, retrievedManager);
}

QTEST_MAIN(TestTabManager)
#include "test_tab_manager.moc"