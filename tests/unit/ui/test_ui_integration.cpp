#include <QtTest/QtTest>
#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QTreeWidget>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QSignalSpy>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QThread>
#include <QElapsedTimer>
#include <memory>

// Mock classes for integration testing
class MockMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MockMainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setupUI();
    }

    QTabWidget* getTabWidget() const { return tabWidget; }
    QTreeWidget* getStructWindow() const { return structWindow; }
    QMdiArea* getMdiArea() const { return mdiArea; }

signals:
    void tabCreated(const QString& name);
    void tabDeleted(int index);
    void windowCreated(const QString& type);
    void settingsChanged(const QString& key, const QVariant& value);

public slots:
    void onTabChanged(int index) {
        if (index >= 0 && index < tabWidget->count()) {
            emit tabCreated(tabWidget->tabText(index));
        }
    }

    void onWindowAdded() {
        emit windowCreated("TestWidget");
    }

    void onSettingChanged(const QString& key, const QVariant& value) {
        emit settingsChanged(key, value);
    }

private:
    void setupUI() {
        auto* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        tabWidget = new QTabWidget(centralWidget);
        structWindow = new QTreeWidget(centralWidget);
        mdiArea = new QMdiArea(centralWidget);

        // Create initial tab
        auto* tab1 = new QWidget();
        tabWidget->addTab(tab1, "Tab 1");

        // Setup struct window
        structWindow->setHeaderLabels({"Name", "Type"});
        auto* rootItem = new QTreeWidgetItem(structWindow);
        rootItem->setText(0, "Root");
        rootItem->setText(1, "struct");

        connect(tabWidget, QOverload<int>::of(&QTabWidget::currentChanged),
                this, &MockMainWindow::onTabChanged);
    }

    QTabWidget* tabWidget = nullptr;
    QTreeWidget* structWindow = nullptr;
    QMdiArea* mdiArea = nullptr;
};

class MockTabManager : public QObject {
    Q_OBJECT

public:
    explicit MockTabManager(QTabWidget* tabWidget, QObject* parent = nullptr)
        : QObject(parent), m_tabWidget(tabWidget) {}

    void createTab(const QString& name) {
        if (m_tabWidget) {
            auto* widget = new QWidget();
            int index = m_tabWidget->addTab(widget, name);
            emit tabCreated(name, index);
        }
    }

    void deleteTab(int index) {
        if (m_tabWidget && index >= 0 && index < m_tabWidget->count()) {
            QString name = m_tabWidget->tabText(index);
            m_tabWidget->removeTab(index);
            emit tabDeleted(name, index);
        }
    }

    void renameTab(int index, const QString& newName) {
        if (m_tabWidget && index >= 0 && index < m_tabWidget->count()) {
            QString oldName = m_tabWidget->tabText(index);
            m_tabWidget->setTabText(index, newName);
            emit tabRenamed(oldName, newName, index);
        }
    }

    int getTabCount() const {
        return m_tabWidget ? m_tabWidget->count() : 0;
    }

signals:
    void tabCreated(const QString& name, int index);
    void tabDeleted(const QString& name, int index);
    void tabRenamed(const QString& oldName, const QString& newName, int index);

private:
    QTabWidget* m_tabWidget = nullptr;
};

class MockStructWindow : public QObject {
    Q_OBJECT

public:
    explicit MockStructWindow(QTreeWidget* treeWidget, QObject* parent = nullptr)
        : QObject(parent), m_treeWidget(treeWidget) {}

    void addStructure(const QString& name, const QString& type) {
        if (m_treeWidget) {
            auto* item = new QTreeWidgetItem(m_treeWidget);
            item->setText(0, name);
            item->setText(1, type);
            emit structureAdded(name, type);
        }
    }

    void selectStructure(const QString& name) {
        if (m_treeWidget) {
            for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
                auto* item = m_treeWidget->topLevelItem(i);
                if (item && item->text(0) == name) {
                    m_treeWidget->setCurrentItem(item);
                    emit structureSelected(name);
                    break;
                }
            }
        }
    }

    QStringList getStructureNames() const {
        QStringList names;
        if (m_treeWidget) {
            for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
                auto* item = m_treeWidget->topLevelItem(i);
                if (item) {
                    names << item->text(0);
                }
            }
        }
        return names;
    }

signals:
    void structureAdded(const QString& name, const QString& type);
    void structureSelected(const QString& name);

private:
    QTreeWidget* m_treeWidget = nullptr;
};

class MockWindowManager : public QObject {
    Q_OBJECT

public:
    explicit MockWindowManager(QMdiArea* mdiArea, QObject* parent = nullptr)
        : QObject(parent), m_mdiArea(mdiArea) {}

    void createWindow(const QString& type, const QString& title) {
        if (m_mdiArea) {
            auto* widget = new QWidget();
            widget->setWindowTitle(title);
            auto* subWindow = m_mdiArea->addSubWindow(widget);
            subWindow->show();
            emit windowCreated(type, title);
        }
    }

    void closeWindow(const QString& title) {
        if (m_mdiArea) {
            auto subWindows = m_mdiArea->subWindowList();
            for (auto* subWindow : subWindows) {
                if (subWindow && subWindow->windowTitle() == title) {
                    subWindow->close();
                    emit windowClosed(title);
                    break;
                }
            }
        }
    }

    int getWindowCount() const {
        if (m_mdiArea) {
            return m_mdiArea->subWindowList().count();
        }
        return 0;
    }

signals:
    void windowCreated(const QString& type, const QString& title);
    void windowClosed(const QString& title);

private:
    QMdiArea* m_mdiArea = nullptr;
};

class MockSettingsManager : public QObject {
    Q_OBJECT

public:
    explicit MockSettingsManager(QObject* parent = nullptr) : QObject(parent) {}

    void setValue(const QString& key, const QVariant& value) {
        m_settings[key] = value;
        emit settingChanged(key, value);
    }

    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const {
        return m_settings.value(key, defaultValue);
    }

    void saveWorkspace(const QString& filename) {
        QJsonObject workspace;
        for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
            workspace[it.key()] = QJsonValue::fromVariant(it.value());
        }

        QJsonDocument doc(workspace);
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            emit workspaceSaved(filename);
        }
    }

    void loadWorkspace(const QString& filename) {
        QFile file(filename);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            QJsonObject workspace = doc.object();
            
            for (auto it = workspace.begin(); it != workspace.end(); ++it) {
                m_settings[it.key()] = it.value().toVariant();
            }
            emit workspaceLoaded(filename);
        }
    }

    void clearSettings() {
        m_settings.clear();
        emit settingsCleared();
    }

signals:
    void settingChanged(const QString& key, const QVariant& value);
    void workspaceSaved(const QString& filename);
    void workspaceLoaded(const QString& filename);
    void settingsCleared();

private:
    QHash<QString, QVariant> m_settings;
};

// Integration test class
class UIIntegrationTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        // Create main window and components
        mainWindow = std::make_unique<MockMainWindow>();
        tabManager = std::make_unique<MockTabManager>(mainWindow->getTabWidget(), mainWindow.get());
        structWindow = std::make_unique<MockStructWindow>(mainWindow->getStructWindow(), mainWindow.get());
        windowManager = std::make_unique<MockWindowManager>(mainWindow->getMdiArea(), mainWindow.get());
        settingsManager = std::make_unique<MockSettingsManager>(mainWindow.get());

        // Setup connections between components
        setupConnections();

        mainWindow->show();
        bool exposed = QTest::qWaitForWindowExposed(mainWindow.get());
        Q_UNUSED(exposed)
    }

    void init() {
        // Reset state before each test
        if (settingsManager) {
            settingsManager->clearSettings();
        }
        
        // Reset window count
        if (windowManager) {
            // Close all windows
            auto subWindows = mainWindow->getMdiArea()->subWindowList();
            for (auto* subWindow : subWindows) {
                if (subWindow) {
                    subWindow->close();
                }
            }
        }
        
        // Reset tabs to initial state
        if (tabManager) {
            int tabCount = tabManager->getTabCount();
            // Remove all tabs except the first one
            for (int i = tabCount - 1; i > 0; --i) {
                tabManager->deleteTab(i);
            }
        }
        
        // Clear struct window
        if (structWindow && mainWindow->getStructWindow()) {
            mainWindow->getStructWindow()->clear();
            // Re-add root item
            auto* rootItem = new QTreeWidgetItem(mainWindow->getStructWindow());
            rootItem->setText(0, "Root");
            rootItem->setText(1, "struct");
        }
    }

    void cleanupTestCase() {
        // Reset in reverse order to avoid double deletion
        // Child objects will be deleted when mainWindow is deleted
        settingsManager.reset();
        windowManager.reset();
        structWindow.reset();
        tabManager.reset();
        
        if (mainWindow) {
            mainWindow->close();
        }
        mainWindow.reset();
    }

    void testBasicComponentIntegration() {
        // Test that all components are properly initialized
        QVERIFY(mainWindow.get() != nullptr);
        QVERIFY(tabManager.get() != nullptr);
        QVERIFY(structWindow.get() != nullptr);
        QVERIFY(windowManager.get() != nullptr);
        QVERIFY(settingsManager.get() != nullptr);

        // Test basic UI elements exist
        QVERIFY(mainWindow->getTabWidget() != nullptr);
        QVERIFY(mainWindow->getStructWindow() != nullptr);
        QVERIFY(mainWindow->getMdiArea() != nullptr);

        // Test initial state
        QVERIFY(tabManager->getTabCount() >= 1); // At least one tab should exist
        QCOMPARE(windowManager->getWindowCount(), 0); // No windows initially
    }

    void testTabSettingsIntegration() {
        QSignalSpy tabCreatedSpy(tabManager.get(), &MockTabManager::tabCreated);
        QSignalSpy settingsChangedSpy(settingsManager.get(), &MockSettingsManager::settingChanged);

        // Create new tabs and verify settings are updated
        tabManager->createTab("Test Tab 1");
        tabManager->createTab("Test Tab 2");

        QCOMPARE(tabCreatedSpy.count(), 2);
        
        // Settings should be updated for each tab
        QVERIFY(settingsChangedSpy.count() >= 2);

        // Verify settings contain tab information
        QVariant tab1Setting = settingsManager->getValue("tabs/1");
        QVariant tab2Setting = settingsManager->getValue("tabs/2");
        
        QCOMPARE(tab1Setting.toString(), QString("Test Tab 1"));
        QCOMPARE(tab2Setting.toString(), QString("Test Tab 2"));
    }

    void testStructureWindowIntegration() {
        QSignalSpy structureAddedSpy(structWindow.get(), &MockStructWindow::structureAdded);
        QSignalSpy structureSelectedSpy(structWindow.get(), &MockStructWindow::structureSelected);
        QSignalSpy settingsChangedSpy(settingsManager.get(), &MockSettingsManager::settingChanged);

        // Add structures
        structWindow->addStructure("TestStruct1", "struct");
        structWindow->addStructure("TestStruct2", "union");

        QCOMPARE(structureAddedSpy.count(), 2);

        // Select structure and verify settings update
        structWindow->selectStructure("TestStruct1");
        QCOMPARE(structureSelectedSpy.count(), 1);
        
        // Verify settings reflect selection
        QVariant selectedStruct = settingsManager->getValue("selected_structure");
        QCOMPARE(selectedStruct.toString(), QString("TestStruct1"));

        // Verify structure list
        QStringList structures = structWindow->getStructureNames();
        QCOMPARE(structures.size(), 3); // Including initial root item
        QVERIFY(structures.contains("TestStruct1"));
        QVERIFY(structures.contains("TestStruct2"));
    }

    void testWindowManagerIntegration() {
        QSignalSpy windowCreatedSpy(windowManager.get(), &MockWindowManager::windowCreated);
        QSignalSpy windowClosedSpy(windowManager.get(), &MockWindowManager::windowClosed);

        // Create windows
        windowManager->createWindow("GridWidget", "Grid Window 1");
        windowManager->createWindow("ChartWidget", "Chart Window 1");

        QCOMPARE(windowCreatedSpy.count(), 2);
        QCOMPARE(windowManager->getWindowCount(), 2);

        // Verify settings are updated
        QVariant gridWindowSetting = settingsManager->getValue("windows/Grid Window 1");
        QVariant chartWindowSetting = settingsManager->getValue("windows/Chart Window 1");
        
        QCOMPARE(gridWindowSetting.toString(), QString("GridWidget"));
        QCOMPARE(chartWindowSetting.toString(), QString("ChartWidget"));

        // Close a window
        windowManager->closeWindow("Grid Window 1");
        QCOMPARE(windowClosedSpy.count(), 1);
        QCOMPARE(windowManager->getWindowCount(), 1);
    }

    void testComplexWorkflowIntegration() {
        // Simulate a complete user workflow
        
        // Step 1: Add structures
        structWindow->addStructure("PacketHeader", "struct");
        structWindow->addStructure("DataPayload", "struct");
        
        // Step 2: Create tabs for different packet types
        tabManager->createTab("Header Analysis");
        tabManager->createTab("Payload Visualization");
        
        // Step 3: Create windows for visualization
        windowManager->createWindow("GridWidget", "Header Grid");
        windowManager->createWindow("ChartWidget", "Payload Chart");
        
        // Step 4: Select structure for current tab
        structWindow->selectStructure("PacketHeader");
        
        // Verify final state
        QCOMPARE(tabManager->getTabCount(), 3); // Original + 2 new
        QCOMPARE(windowManager->getWindowCount(), 2);
        QCOMPARE(structWindow->getStructureNames().size(), 3); // Root + 2 new
        
        // Verify settings reflect the workflow
        QVariant selectedStruct = settingsManager->getValue("selected_structure");
        QCOMPARE(selectedStruct.toString(), QString("PacketHeader"));
        
        QVariant headerTab = settingsManager->getValue("tabs/1");
        QVariant payloadTab = settingsManager->getValue("tabs/2");
        QCOMPARE(headerTab.toString(), QString("Header Analysis"));
        QCOMPARE(payloadTab.toString(), QString("Payload Visualization"));
    }

    void testWorkspacePersistenceIntegration() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString workspaceFile = tempDir.path() + "/test_workspace.json";
        
        // Setup initial state
        tabManager->createTab("Persistent Tab 1");
        tabManager->createTab("Persistent Tab 2");
        structWindow->addStructure("PersistentStruct", "struct");
        structWindow->selectStructure("PersistentStruct");
        windowManager->createWindow("PersistentWidget", "Persistent Window");
        
        // Additional settings
        settingsManager->setValue("theme", "dark");
        settingsManager->setValue("auto_save", true);
        
        QSignalSpy workspaceSavedSpy(settingsManager.get(), &MockSettingsManager::workspaceSaved);
        QSignalSpy workspaceLoadedSpy(settingsManager.get(), &MockSettingsManager::workspaceLoaded);
        
        // Save workspace
        settingsManager->saveWorkspace(workspaceFile);
        QCOMPARE(workspaceSavedSpy.count(), 1);
        QVERIFY(QFile::exists(workspaceFile));
        
        // Clear current state
        settingsManager->clearSettings();
        QVariant clearedTheme = settingsManager->getValue("theme");
        QVERIFY(!clearedTheme.isValid());
        
        // Load workspace
        settingsManager->loadWorkspace(workspaceFile);
        QCOMPARE(workspaceLoadedSpy.count(), 1);
        
        // Verify restored settings
        QVariant restoredTheme = settingsManager->getValue("theme");
        QVariant restoredAutoSave = settingsManager->getValue("auto_save");
        QVariant restoredStruct = settingsManager->getValue("selected_structure");
        
        QCOMPARE(restoredTheme.toString(), QString("dark"));
        QCOMPARE(restoredAutoSave.toBool(), true);
        QCOMPARE(restoredStruct.toString(), QString("PersistentStruct"));
    }

    void testErrorHandlingIntegration() {
        // Test deletion of non-existent tab
        int initialTabCount = tabManager->getTabCount();
        tabManager->deleteTab(999); // Invalid index
        QCOMPARE(tabManager->getTabCount(), initialTabCount);
        
        // Test closing non-existent window
        int initialWindowCount = windowManager->getWindowCount();
        windowManager->closeWindow("NonExistentWindow");
        QCOMPARE(windowManager->getWindowCount(), initialWindowCount);
        
        // Test selecting non-existent structure
        QSignalSpy structureSelectedSpy(structWindow.get(), &MockStructWindow::structureSelected);
        structWindow->selectStructure("NonExistentStruct");
        QCOMPARE(structureSelectedSpy.count(), 0); // Should not emit signal
        
        // Test loading invalid workspace file
        QSignalSpy workspaceLoadedSpy(settingsManager.get(), &MockSettingsManager::workspaceLoaded);
        settingsManager->loadWorkspace("/invalid/path/workspace.json");
        QCOMPARE(workspaceLoadedSpy.count(), 0); // Should not emit signal
    }

    void testUIResponsivenessUnderLoad() {
        QElapsedTimer timer;
        const int numOperations = 50; // Reduced for faster test execution
        
        // Measure UI responsiveness while performing many operations
        timer.start();
        
        for (int i = 0; i < numOperations; ++i) {
            tabManager->createTab(QString("Load Tab %1").arg(i));
            structWindow->addStructure(QString("LoadStruct%1").arg(i), "struct");
            windowManager->createWindow("LoadWidget", QString("Load Window %1").arg(i));
            
            // Process events to keep UI responsive
            QCoreApplication::processEvents();
        }
        
        qint64 totalTime = timer.elapsed();
        double avgTimePerOp = static_cast<double>(totalTime) / numOperations;
        
        // Verify UI remains responsive
        QVERIFY(avgTimePerOp < 100.0); // Average operation time should be reasonable
        QVERIFY(totalTime < 10000); // Total time should be reasonable
        
        qDebug() << "UI load test completed in" << totalTime << "ms";
        qDebug() << "Average time per operation:" << avgTimePerOp << "ms";
        
        // Verify final counts
        QCOMPARE(tabManager->getTabCount(), numOperations + 1);
        QCOMPARE(structWindow->getStructureNames().size(), numOperations + 1);
        QCOMPARE(windowManager->getWindowCount(), numOperations);
    }

private:
    void setupConnections() {
        // Connect tab manager to settings
        connect(tabManager.get(), &MockTabManager::tabCreated,
                [this](const QString& name, int index) {
                    settingsManager->setValue("tabs/" + QString::number(index), name);
                });

        connect(tabManager.get(), &MockTabManager::tabDeleted,
                [this](const QString& name, int index) {
                    Q_UNUSED(index)
                    settingsManager->setValue("tabs/deleted", name);
                });

        // Connect struct window to other components
        connect(structWindow.get(), &MockStructWindow::structureSelected,
                [this](const QString& name) {
                    settingsManager->setValue("selected_structure", name);
                });

        // Connect window manager to settings
        connect(windowManager.get(), &MockWindowManager::windowCreated,
                [this](const QString& type, const QString& title) {
                    settingsManager->setValue("windows/" + title, type);
                });
    }

private:
    std::unique_ptr<MockMainWindow> mainWindow;
    std::unique_ptr<MockTabManager> tabManager;
    std::unique_ptr<MockStructWindow> structWindow;
    std::unique_ptr<MockWindowManager> windowManager;
    std::unique_ptr<MockSettingsManager> settingsManager;
};

QTEST_MAIN(UIIntegrationTest)
#include "test_ui_integration.moc"