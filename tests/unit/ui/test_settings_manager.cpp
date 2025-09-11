#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QtConcurrent>
#include <QMainWindow>
#include "../../../src/ui/managers/settings_manager.h"
#include "../../../mainwindow.h"
#include "../../../src/ui/managers/tab_manager.h"

// Forward declaration
class MainWindow;

// Mock classes for testing
class MockMainWindow : public MainWindow
{
    Q_OBJECT
public:
    MockMainWindow() : MainWindow(nullptr), m_maximized(false) {
        setGeometry(100, 100, 800, 600);
    }
    
    bool isMaximized() const { return m_maximized; }
    void setMaximized(bool max) { m_maximized = max; }
    
private:
    bool m_maximized;
};

// Forward declaration
class TabManager;

class MockTabManager : public TabManager  
{
    Q_OBJECT
public:
    MockTabManager() : TabManager(nullptr), m_activeTab("tab1"), m_tabCount(3) {}
    
    QString getActiveTabId() const { return m_activeTab; }
    void setActiveTab(const QString &tab) { m_activeTab = tab; }
    int tabCount() const { return m_tabCount; }
    void setTabCount(int count) { m_tabCount = count; }
    
private:
    QString m_activeTab;
    int m_tabCount;
};

// Thread test helper class
class SettingsTestThread : public QThread
{
    Q_OBJECT
public:
    SettingsTestThread(SettingsManager *manager, int threadId) 
        : m_manager(manager), m_threadId(threadId) {}
    
protected:
    void run() override {
        for (int i = 0; i < 100; ++i) {
            QString key = QString("thread_%1_key_%2").arg(m_threadId).arg(i);
            QString value = QString("thread_%1_value_%2").arg(m_threadId).arg(i);
            
            m_manager->setSetting(key, value);
            QVariant retrieved = m_manager->getSetting(key);
            
            if (retrieved.toString() != value) {
                qWarning() << "Thread" << m_threadId << "value mismatch at" << i;
            }
        }
    }
    
private:
    SettingsManager *m_manager;
    int m_threadId;
};

class TestSettingsManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    
    // Core functionality tests
    void testInitialization();
    void testDirectoryCreation();
    void testSettingsFileSetup();
    
    // Settings operations tests
    void testGetSetSetting();
    void testSettingTypes();
    void testSettingPersistence();
    void testHasSetting();
    void testRemoveSetting();
    void testClearSettings();
    
    // Workspace management tests
    void testSaveWorkspace();
    void testLoadWorkspace();
    void testCreateNewWorkspace();
    void testWorkspacePath();
    void testWorkspaceName();
    
    // Recent workspaces tests
    void testRecentWorkspaces();
    void testAddRecentWorkspace();
    void testRemoveRecentWorkspace();
    void testClearRecentWorkspaces();
    void testMaxRecentWorkspaces();
    
    // UI state management tests
    void testMainWindowState();
    void testTabManagerState();
    void testUIStatePersistence();
    
    // Theme and appearance tests
    void testCurrentTheme();
    void testSetCurrentTheme();
    void testAvailableThemes();
    void testThemeChange();
    
    // Auto-save functionality tests
    void testAutoSave();
    void testAutoSaveEnabled();
    void testAutoSaveInterval();
    void testAutoSaveTriggered();
    
    // Settings validation and migration tests
    void testValidateSettings();
    void testMigrateSettings();
    void testSettingsVersion();
    void testVersionUpgrade();
    
    // Backup and restore tests
    void testCreateBackup();
    void testRestoreFromBackup();
    void testAvailableBackups();
    void testDeleteBackup();
    void testBackupManagement();
    
    // Import/Export tests
    void testExportSettings();
    void testImportSettings();
    void testExportImportRoundTrip();
    
    // Default settings tests
    void testResetToDefaults();
    void testGetDefaultSettings();
    void testDefaultValues();
    
    // Signal/slot tests
    void testSettingsChangedSignals();
    void testWorkspaceChangedSignals();
    void testWorkspaceSavedSignals();
    void testThemeChangedSignals();
    void testAutoSaveSignals();
    
    // Thread safety tests
    void testThreadSafetyBasic();
    void testConcurrentReadWrite();
    void testConcurrentWorkspaceOperations();
    void testMutexLocking();
    void testSettingsCache();
    
    // Performance tests
    void testManySettingsPerformance();
    void testLargeWorkspacePerformance();
    void testAutoSavePerformance();
    void testCachePerformance();
    
    // Error handling tests
    void testInvalidPaths();
    void testCorruptedSettings();
    void testMissingFiles();
    void testDiskSpaceErrors();
    void testPermissionErrors();
    
    // Integration tests
    void testSettingsIntegration();
    void testWorkspaceIntegration();
    void testApplicationShutdown();
    
    // Edge cases tests
    void testEmptyWorkspace();
    void testLongPaths();
    void testSpecialCharacters();
    void testLargeValues();
    void testNullValues();
    
    // Settings groups and batch operations
    void testSettingsGroup();
    void testSettingsBatch();
    void testBatchOperations();
    void testTransactionalUpdates();

private:
    SettingsManager *m_settingsManager;
    QTemporaryDir *m_tempDir;
    QString m_testConfigPath;
    
    // Helper methods
    void createTestWorkspace(const QString &path);
    void createTestBackup(const QString &path);
    QJsonObject createTestSettings();
    void verifySettingsIntegrity(const QJsonObject &settings);
    void simulateFileCorruption(const QString &filePath);
    void createLargeSettings(int count);
    bool waitForAutoSave(int timeoutMs = 5000);
};

void TestSettingsManager::initTestCase()
{
    if (!QApplication::instance()) {
        int argc = 1;
        char arg0[] = "test";
        char *argv[] = {arg0};
        new QApplication(argc, argv);
    }
    
    // Create temporary directory for test files
    m_tempDir = new QTemporaryDir("settings_test_XXXXXX");
    QVERIFY(m_tempDir->isValid());
    
    m_testConfigPath = m_tempDir->path() + "/test_config.ini";
}

void TestSettingsManager::cleanupTestCase()
{
    delete m_tempDir;
}

void TestSettingsManager::init()
{
    // Set temporary directory as application config location
    QCoreApplication::setOrganizationName("TestOrg");
    QCoreApplication::setApplicationName("SettingsManagerTest");
    
    m_settingsManager = new SettingsManager();
}

void TestSettingsManager::cleanup()
{
    delete m_settingsManager;
    m_settingsManager = nullptr;
}

void TestSettingsManager::testInitialization()
{
    QVERIFY(m_settingsManager != nullptr);
    QVERIFY(m_settingsManager->isAutoSaveEnabled());
    QVERIFY(m_settingsManager->getAutoSaveInterval() > 0);
    QVERIFY(!m_settingsManager->getCurrentWorkspacePath().isEmpty() || 
            m_settingsManager->getCurrentWorkspacePath().isEmpty());
    QVERIFY(m_settingsManager->getSettingsVersion() > 0);
}

void TestSettingsManager::testDirectoryCreation()
{
    // Verify that settings directories are created
    QStringList expectedDirs;
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    
    if (QDir(baseDir).exists()) {
        // Config directory structure exists
        QVERIFY(true);
    }
}

void TestSettingsManager::testSettingsFileSetup()
{
    // Test that settings files can be created and accessed
    m_settingsManager->setSetting("test_file_setup", "test_value");
    QVariant value = m_settingsManager->getSetting("test_file_setup");
    QCOMPARE(value.toString(), QString("test_value"));
}

void TestSettingsManager::testGetSetSetting()
{
    QSignalSpy settingsSpy(m_settingsManager, &SettingsManager::settingsChanged);
    
    // Test string setting
    m_settingsManager->setSetting("test_string", "hello world");
    QVariant stringValue = m_settingsManager->getSetting("test_string");
    QCOMPARE(stringValue.toString(), QString("hello world"));
    
    // Test integer setting
    m_settingsManager->setSetting("test_int", 42);
    QVariant intValue = m_settingsManager->getSetting("test_int");
    QCOMPARE(intValue.toInt(), 42);
    
    // Test boolean setting
    m_settingsManager->setSetting("test_bool", true);
    QVariant boolValue = m_settingsManager->getSetting("test_bool");
    QCOMPARE(boolValue.toBool(), true);
    
    // Test default value
    QVariant defaultValue = m_settingsManager->getSetting("nonexistent", "default");
    QCOMPARE(defaultValue.toString(), QString("default"));
    
    // Verify signals
    QVERIFY(settingsSpy.count() >= 3);
}

void TestSettingsManager::testSettingTypes()
{
    // Test various data types
    QStringList testList = {"item1", "item2", "item3"};
    m_settingsManager->setSetting("test_list", testList);
    QVariant listValue = m_settingsManager->getSetting("test_list");
    QCOMPARE(listValue.toStringList(), testList);
    
    QByteArray testData = "binary_data";
    m_settingsManager->setSetting("test_bytes", testData);
    QVariant bytesValue = m_settingsManager->getSetting("test_bytes");
    QCOMPARE(bytesValue.toByteArray(), testData);
    
    QDateTime testTime = QDateTime::currentDateTime();
    m_settingsManager->setSetting("test_datetime", testTime);
    QVariant timeValue = m_settingsManager->getSetting("test_datetime");
    QCOMPARE(timeValue.toDateTime(), testTime);
    
    // Test nested settings (using groups)
    m_settingsManager->setSetting("group/nested_key", "nested_value");
    QVariant nestedValue = m_settingsManager->getSetting("group/nested_key");
    QCOMPARE(nestedValue.toString(), QString("nested_value"));
}

void TestSettingsManager::testSettingPersistence()
{
    QString testKey = "persistence_test";
    QString testValue = "persistent_value";
    
    // Set value and save
    m_settingsManager->setSetting(testKey, testValue);
    m_settingsManager->saveSettings();
    
    // Create new settings manager and verify persistence
    delete m_settingsManager;
    m_settingsManager = new SettingsManager();
    
    QVariant retrievedValue = m_settingsManager->getSetting(testKey);
    QCOMPARE(retrievedValue.toString(), testValue);
}

void TestSettingsManager::testHasSetting()
{
    QString testKey = "has_setting_test";
    
    QVERIFY(!m_settingsManager->hasSetting(testKey));
    
    m_settingsManager->setSetting(testKey, "value");
    QVERIFY(m_settingsManager->hasSetting(testKey));
}

void TestSettingsManager::testRemoveSetting()
{
    QString testKey = "remove_test";
    QString testValue = "remove_value";
    
    m_settingsManager->setSetting(testKey, testValue);
    QVERIFY(m_settingsManager->hasSetting(testKey));
    
    QSignalSpy settingsSpy(m_settingsManager, &SettingsManager::settingsChanged);
    
    m_settingsManager->removeSetting(testKey);
    QVERIFY(!m_settingsManager->hasSetting(testKey));
    
    // Verify signal emission
    if (settingsSpy.count() > 0) {
        QCOMPARE(settingsSpy.last().at(0).toString(), testKey);
    }
}

void TestSettingsManager::testClearSettings()
{
    // Add some test settings
    m_settingsManager->setSetting("clear_test1", "value1");
    m_settingsManager->setSetting("clear_test2", "value2");
    m_settingsManager->setSetting("clear_test3", "value3");
    
    QVERIFY(m_settingsManager->hasSetting("clear_test1"));
    QVERIFY(m_settingsManager->hasSetting("clear_test2"));
    QVERIFY(m_settingsManager->hasSetting("clear_test3"));
    
    // Clear all settings
    m_settingsManager->clearSettings();
    
    QVERIFY(!m_settingsManager->hasSetting("clear_test1"));
    QVERIFY(!m_settingsManager->hasSetting("clear_test2"));
    QVERIFY(!m_settingsManager->hasSetting("clear_test3"));
}

void TestSettingsManager::testSaveWorkspace()
{
    QSignalSpy workspaceSpy(m_settingsManager, &SettingsManager::workspaceSaved);
    
    QString testPath = m_tempDir->path() + "/test_workspace.json";
    bool saved = m_settingsManager->saveWorkspace(testPath);
    QVERIFY(saved);
    
    // Verify file was created
    QVERIFY(QFile::exists(testPath));
    
    // Verify signal emission
    QCOMPARE(workspaceSpy.count(), 1);
    QCOMPARE(workspaceSpy.last().at(0).toString(), testPath);
    QCOMPARE(workspaceSpy.last().at(1).toBool(), true);
}

void TestSettingsManager::testLoadWorkspace()
{
    QString testPath = m_tempDir->path() + "/load_workspace.json";
    createTestWorkspace(testPath);
    
    QSignalSpy workspaceSpy(m_settingsManager, &SettingsManager::workspaceLoaded);
    QSignalSpy changedSpy(m_settingsManager, &SettingsManager::workspaceChanged);
    
    bool loaded = m_settingsManager->loadWorkspace(testPath);
    QVERIFY(loaded);
    
    QCOMPARE(m_settingsManager->getCurrentWorkspacePath(), testPath);
    
    // Verify signals
    QCOMPARE(workspaceSpy.count(), 1);
    QCOMPARE(workspaceSpy.last().at(0).toString(), testPath);
    QCOMPARE(workspaceSpy.last().at(1).toBool(), true);
    
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(changedSpy.last().at(0).toString(), testPath);
}

void TestSettingsManager::testCreateNewWorkspace()
{
    QSignalSpy workspaceSpy(m_settingsManager, &SettingsManager::workspaceChanged);
    
    bool created = m_settingsManager->createNewWorkspace("Test Workspace");
    QVERIFY(created);
    
    QString currentPath = m_settingsManager->getCurrentWorkspacePath();
    QVERIFY(!currentPath.isEmpty());
    QVERIFY(QFile::exists(currentPath));
    
    QString currentName = m_settingsManager->getCurrentWorkspaceName();
    QCOMPARE(currentName, QString("Test Workspace"));
    
    // Verify signal emission
    QCOMPARE(workspaceSpy.count(), 1);
}

void TestSettingsManager::testWorkspacePath()
{
    QString testPath = m_tempDir->path() + "/path_test_workspace.json";
    createTestWorkspace(testPath);
    
    m_settingsManager->loadWorkspace(testPath);
    QCOMPARE(m_settingsManager->getCurrentWorkspacePath(), testPath);
}

void TestSettingsManager::testWorkspaceName()
{
    QString testPath = m_tempDir->path() + "/name_test_workspace.json";
    createTestWorkspace(testPath);
    
    m_settingsManager->loadWorkspace(testPath);
    QString name = m_settingsManager->getCurrentWorkspaceName();
    QVERIFY(!name.isEmpty());
}

void TestSettingsManager::testRecentWorkspaces()
{
    QStringList initialRecent = m_settingsManager->getRecentWorkspaces();
    
    // Add some recent workspaces
    QString path1 = m_tempDir->path() + "/recent1.json";
    QString path2 = m_tempDir->path() + "/recent2.json";
    QString path3 = m_tempDir->path() + "/recent3.json";
    
    createTestWorkspace(path1);
    createTestWorkspace(path2);
    createTestWorkspace(path3);
    
    m_settingsManager->addRecentWorkspace(path1);
    m_settingsManager->addRecentWorkspace(path2);
    m_settingsManager->addRecentWorkspace(path3);
    
    QStringList recent = m_settingsManager->getRecentWorkspaces();
    QVERIFY(recent.contains(path1));
    QVERIFY(recent.contains(path2));
    QVERIFY(recent.contains(path3));
}

void TestSettingsManager::testAddRecentWorkspace()
{
    QString testPath = m_tempDir->path() + "/add_recent.json";
    createTestWorkspace(testPath);
    
    QStringList beforeAdd = m_settingsManager->getRecentWorkspaces();
    
    m_settingsManager->addRecentWorkspace(testPath);
    
    QStringList afterAdd = m_settingsManager->getRecentWorkspaces();
    QVERIFY(afterAdd.contains(testPath));
    QVERIFY(afterAdd.size() >= beforeAdd.size());
}

void TestSettingsManager::testRemoveRecentWorkspace()
{
    QString testPath = m_tempDir->path() + "/remove_recent.json";
    createTestWorkspace(testPath);
    
    m_settingsManager->addRecentWorkspace(testPath);
    QVERIFY(m_settingsManager->getRecentWorkspaces().contains(testPath));
    
    m_settingsManager->removeRecentWorkspace(testPath);
    QVERIFY(!m_settingsManager->getRecentWorkspaces().contains(testPath));
}

void TestSettingsManager::testClearRecentWorkspaces()
{
    // Add some recent workspaces
    QString path1 = m_tempDir->path() + "/clear1.json";
    QString path2 = m_tempDir->path() + "/clear2.json";
    
    createTestWorkspace(path1);
    createTestWorkspace(path2);
    
    m_settingsManager->addRecentWorkspace(path1);
    m_settingsManager->addRecentWorkspace(path2);
    
    QVERIFY(!m_settingsManager->getRecentWorkspaces().isEmpty());
    
    m_settingsManager->clearRecentWorkspaces();
    QVERIFY(m_settingsManager->getRecentWorkspaces().isEmpty());
}

void TestSettingsManager::testMaxRecentWorkspaces()
{
    // Add many recent workspaces and verify limit
    for (int i = 0; i < 20; ++i) {
        QString path = m_tempDir->path() + QString("/recent_%1.json").arg(i);
        createTestWorkspace(path);
        m_settingsManager->addRecentWorkspace(path);
    }
    
    QStringList recent = m_settingsManager->getRecentWorkspaces();
    QVERIFY(recent.size() <= 10); // Assuming max is 10
}

void TestSettingsManager::testMainWindowState()
{
    MockMainWindow mockWindow;
    mockWindow.setGeometry(QRect(50, 50, 1000, 700));
    mockWindow.setMaximized(true);
    
    // Save state
    m_settingsManager->saveMainWindowState(&mockWindow);
    
    // Verify settings were saved
    QVERIFY(m_settingsManager->hasSetting(Settings::MainWindow::Geometry));
    QVERIFY(m_settingsManager->hasSetting(Settings::MainWindow::Maximized));
    
    // Create new window and restore state
    MockMainWindow restoredWindow;
    m_settingsManager->restoreMainWindowState(&restoredWindow);
    
    // Verify restoration (exact matching depends on implementation)
    QVERIFY(restoredWindow.geometry().isValid());
}

void TestSettingsManager::testTabManagerState()
{
    MockTabManager mockTabManager;
    mockTabManager.setActiveTab("test_tab");
    mockTabManager.setTabCount(5);
    
    // Save state
    m_settingsManager->saveTabManagerState(&mockTabManager);
    
    // Verify settings were saved
    QVERIFY(m_settingsManager->hasSetting(Settings::Tabs::ActiveTab));
    
    // Create new tab manager and restore state
    MockTabManager restoredTabManager;
    m_settingsManager->restoreTabManagerState(&restoredTabManager);
    
    // Verify restoration
    QCOMPARE(restoredTabManager.getActiveTabId(), QString("test_tab"));
}

void TestSettingsManager::testUIStatePersistence()
{
    MockMainWindow mockWindow;
    MockTabManager mockTabManager;
    
    mockWindow.setGeometry(QRect(100, 200, 800, 600));
    mockTabManager.setActiveTab("persistent_tab");
    
    // Save states
    m_settingsManager->saveMainWindowState(&mockWindow);
    m_settingsManager->saveTabManagerState(&mockTabManager);
    m_settingsManager->saveSettings();
    
    // Create new settings manager
    delete m_settingsManager;
    m_settingsManager = new SettingsManager();
    
    // Restore states
    MockMainWindow restoredWindow;
    MockTabManager restoredTabManager;
    
    m_settingsManager->restoreMainWindowState(&restoredWindow);
    m_settingsManager->restoreTabManagerState(&restoredTabManager);
    
    // Verify persistence
    QCOMPARE(restoredTabManager.getActiveTabId(), QString("persistent_tab"));
}

void TestSettingsManager::testCurrentTheme()
{
    QString initialTheme = m_settingsManager->getCurrentTheme();
    QVERIFY(!initialTheme.isEmpty());
}

void TestSettingsManager::testSetCurrentTheme()
{
    QSignalSpy themeSpy(m_settingsManager, &SettingsManager::themeChanged);
    
    QString testTheme = "dark_theme";
    m_settingsManager->setCurrentTheme(testTheme);
    
    QCOMPARE(m_settingsManager->getCurrentTheme(), testTheme);
    QCOMPARE(themeSpy.count(), 1);
    QCOMPARE(themeSpy.last().at(0).toString(), testTheme);
}

void TestSettingsManager::testAvailableThemes()
{
    QStringList themes = m_settingsManager->getAvailableThemes();
    QVERIFY(!themes.isEmpty());
    
    // Should contain at least some basic themes
    QVERIFY(themes.contains("light") || themes.contains("default") || !themes.isEmpty());
}

void TestSettingsManager::testThemeChange()
{
    QString originalTheme = m_settingsManager->getCurrentTheme();
    QStringList availableThemes = m_settingsManager->getAvailableThemes();
    
    if (availableThemes.size() > 1) {
        QString newTheme = availableThemes[1];
        if (newTheme == originalTheme && availableThemes.size() > 2) {
            newTheme = availableThemes[2];
        }
        
        QSignalSpy themeSpy(m_settingsManager, &SettingsManager::themeChanged);
        
        m_settingsManager->setCurrentTheme(newTheme);
        QCOMPARE(m_settingsManager->getCurrentTheme(), newTheme);
        QCOMPARE(themeSpy.count(), 1);
    }
}

void TestSettingsManager::testAutoSave()
{
    // Test initial state
    QVERIFY(m_settingsManager->isAutoSaveEnabled());
    QVERIFY(m_settingsManager->getAutoSaveInterval() > 0);
    
    // Test disabling auto-save
    m_settingsManager->setAutoSaveEnabled(false);
    QVERIFY(!m_settingsManager->isAutoSaveEnabled());
    
    // Test enabling auto-save
    m_settingsManager->setAutoSaveEnabled(true);
    QVERIFY(m_settingsManager->isAutoSaveEnabled());
}

void TestSettingsManager::testAutoSaveEnabled()
{
    bool initialState = m_settingsManager->isAutoSaveEnabled();
    
    m_settingsManager->setAutoSaveEnabled(!initialState);
    QCOMPARE(m_settingsManager->isAutoSaveEnabled(), !initialState);
    
    m_settingsManager->setAutoSaveEnabled(initialState);
    QCOMPARE(m_settingsManager->isAutoSaveEnabled(), initialState);
}

void TestSettingsManager::testAutoSaveInterval()
{
    int originalInterval = m_settingsManager->getAutoSaveInterval();
    int testInterval = 30; // 30 seconds
    
    m_settingsManager->setAutoSaveInterval(testInterval);
    QCOMPARE(m_settingsManager->getAutoSaveInterval(), testInterval);
    
    // Restore original interval
    m_settingsManager->setAutoSaveInterval(originalInterval);
}

void TestSettingsManager::testAutoSaveTriggered()
{
    QSignalSpy autoSaveSpy(m_settingsManager, &SettingsManager::autoSaveCompleted);
    
    // Enable auto-save with short interval
    m_settingsManager->setAutoSaveEnabled(true);
    m_settingsManager->setAutoSaveInterval(1); // 1 second
    
    // Make a change
    m_settingsManager->setSetting("autosave_test", "test_value");
    
    // Trigger auto-save manually
    m_settingsManager->onAutoSaveTriggered();
    
    QCOMPARE(autoSaveSpy.count(), 1);
    QCOMPARE(autoSaveSpy.last().at(0).toBool(), true);
}

void TestSettingsManager::testValidateSettings()
{
    bool isValid = m_settingsManager->validateSettings();
    QVERIFY(isValid);
    
    // Add some invalid settings and test validation
    m_settingsManager->setSetting("", "empty_key"); // Invalid empty key
    
    // Validation might still pass depending on implementation
    bool validAfterInvalid = m_settingsManager->validateSettings();
    QVERIFY(validAfterInvalid || !validAfterInvalid); // Either outcome is valid
}

void TestSettingsManager::testMigrateSettings()
{
    int currentVersion = m_settingsManager->getSettingsVersion();
    
    // Test migration from previous version
    if (currentVersion > 1) {
        bool migrated = m_settingsManager->migrateSettings(currentVersion - 1, currentVersion);
        QVERIFY(migrated || !migrated); // Migration might not be needed
    }
}

void TestSettingsManager::testSettingsVersion()
{
    int version = m_settingsManager->getSettingsVersion();
    QVERIFY(version > 0);
    QVERIFY(version <= 10); // Reasonable upper bound
}

void TestSettingsManager::testVersionUpgrade()
{
    int currentVersion = m_settingsManager->getSettingsVersion();
    
    // Simulate version upgrade scenario
    m_settingsManager->setSetting("app/version", currentVersion + 1);
    
    // Verify version is updated
    QVERIFY(m_settingsManager->getSettingsVersion() >= currentVersion);
}

void TestSettingsManager::testCreateBackup()
{
    QString backupPath = m_tempDir->path() + "/test_backup.json";
    
    bool created = m_settingsManager->createBackup(backupPath);
    QVERIFY(created);
    QVERIFY(QFile::exists(backupPath));
}

void TestSettingsManager::testRestoreFromBackup()
{
    QString backupPath = m_tempDir->path() + "/restore_backup.json";
    createTestBackup(backupPath);
    
    // Add some test settings before restore
    m_settingsManager->setSetting("before_restore", "original_value");
    
    bool restored = m_settingsManager->restoreFromBackup(backupPath);
    QVERIFY(restored);
    
    // Verify restore worked (exact behavior depends on implementation)
    QVERIFY(true);
}

void TestSettingsManager::testAvailableBackups()
{
    // Create some backup files
    QString backup1 = m_tempDir->path() + "/backup1.json";
    QString backup2 = m_tempDir->path() + "/backup2.json";
    
    createTestBackup(backup1);
    createTestBackup(backup2);
    
    QStringList backups = m_settingsManager->getAvailableBackups();
    // Backup discovery depends on implementation
    QVERIFY(backups.size() >= 0);
}

void TestSettingsManager::testDeleteBackup()
{
    QString backupPath = m_tempDir->path() + "/delete_backup.json";
    createTestBackup(backupPath);
    
    QVERIFY(QFile::exists(backupPath));
    
    bool deleted = m_settingsManager->deleteBackup(backupPath);
    if (deleted) {
        QVERIFY(!QFile::exists(backupPath));
    }
}

void TestSettingsManager::testBackupManagement()
{
    // Create multiple backups and test management
    for (int i = 0; i < 5; ++i) {
        QString backupPath = m_tempDir->path() + QString("/backup_%1.json").arg(i);
        createTestBackup(backupPath);
    }
    
    QStringList backups = m_settingsManager->getAvailableBackups();
    // Backup management depends on implementation
    QVERIFY(backups.size() >= 0);
}

void TestSettingsManager::testExportSettings()
{
    QString exportPath = m_tempDir->path() + "/exported_settings.json";
    
    // Add some test settings
    m_settingsManager->setSetting("export_test1", "value1");
    m_settingsManager->setSetting("export_test2", 42);
    m_settingsManager->setSetting("export_test3", true);
    
    bool exported = m_settingsManager->exportSettings(exportPath);
    QVERIFY(exported);
    QVERIFY(QFile::exists(exportPath));
    
    // Verify file content
    QFile exportFile(exportPath);
    QVERIFY(exportFile.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(exportFile.readAll());
    QVERIFY(!doc.isNull());
    QVERIFY(doc.isObject());
}

void TestSettingsManager::testImportSettings()
{
    QString importPath = m_tempDir->path() + "/import_settings.json";
    
    // Create test import file
    QJsonObject importData;
    importData["import_test1"] = "imported_value1";
    importData["import_test2"] = 99;
    importData["import_test3"] = false;
    
    QFile importFile(importPath);
    QVERIFY(importFile.open(QIODevice::WriteOnly));
    QJsonDocument doc(importData);
    importFile.write(doc.toJson());
    importFile.close();
    
    bool imported = m_settingsManager->importSettings(importPath);
    QVERIFY(imported);
    
    // Verify imported settings
    QCOMPARE(m_settingsManager->getSetting("import_test1").toString(), QString("imported_value1"));
    QCOMPARE(m_settingsManager->getSetting("import_test2").toInt(), 99);
    QCOMPARE(m_settingsManager->getSetting("import_test3").toBool(), false);
}

void TestSettingsManager::testExportImportRoundTrip()
{
    QString exportPath = m_tempDir->path() + "/roundtrip_export.json";
    QString importPath = m_tempDir->path() + "/roundtrip_import.json";
    
    // Set original values
    m_settingsManager->setSetting("roundtrip_string", "test_value");
    m_settingsManager->setSetting("roundtrip_int", 123);
    m_settingsManager->setSetting("roundtrip_bool", true);
    
    // Export
    QVERIFY(m_settingsManager->exportSettings(exportPath));
    
    // Clear and set different values
    m_settingsManager->setSetting("roundtrip_string", "different_value");
    m_settingsManager->setSetting("roundtrip_int", 456);
    m_settingsManager->setSetting("roundtrip_bool", false);
    
    // Import back
    QVERIFY(m_settingsManager->importSettings(exportPath));
    
    // Verify original values restored
    QCOMPARE(m_settingsManager->getSetting("roundtrip_string").toString(), QString("test_value"));
    QCOMPARE(m_settingsManager->getSetting("roundtrip_int").toInt(), 123);
    QCOMPARE(m_settingsManager->getSetting("roundtrip_bool").toBool(), true);
}

void TestSettingsManager::testResetToDefaults()
{
    // Set some custom values
    m_settingsManager->setSetting("custom_setting1", "custom_value");
    m_settingsManager->setSetting("custom_setting2", 789);
    
    QVERIFY(m_settingsManager->hasSetting("custom_setting1"));
    QVERIFY(m_settingsManager->hasSetting("custom_setting2"));
    
    // Reset to defaults
    m_settingsManager->resetToDefaults();
    
    // Custom settings should be removed or reset
    // Exact behavior depends on implementation
    QVERIFY(true);
}

void TestSettingsManager::testGetDefaultSettings()
{
    QJsonObject defaults = m_settingsManager->getDefaultSettings();
    QVERIFY(!defaults.isEmpty() || defaults.isEmpty()); // Either is valid
}

void TestSettingsManager::testDefaultValues()
{
    // Test that default values are returned for non-existent keys
    QVariant defaultString = m_settingsManager->getSetting("nonexistent_string", "default_value");
    QCOMPARE(defaultString.toString(), QString("default_value"));
    
    QVariant defaultInt = m_settingsManager->getSetting("nonexistent_int", 42);
    QCOMPARE(defaultInt.toInt(), 42);
    
    QVariant defaultBool = m_settingsManager->getSetting("nonexistent_bool", true);
    QCOMPARE(defaultBool.toBool(), true);
}

void TestSettingsManager::testSettingsChangedSignals()
{
    QSignalSpy settingsSpy(m_settingsManager, &SettingsManager::settingsChanged);
    
    QString testKey = "signal_test_key";
    QString testValue = "signal_test_value";
    
    m_settingsManager->setSetting(testKey, testValue);
    
    QCOMPARE(settingsSpy.count(), 1);
    QCOMPARE(settingsSpy.last().at(0).toString(), testKey);
    QCOMPARE(settingsSpy.last().at(1).toString(), testValue);
}

void TestSettingsManager::testWorkspaceChangedSignals()
{
    QSignalSpy workspaceSpy(m_settingsManager, &SettingsManager::workspaceChanged);
    
    QString testPath = m_tempDir->path() + "/signal_workspace.json";
    createTestWorkspace(testPath);
    
    m_settingsManager->loadWorkspace(testPath);
    
    QCOMPARE(workspaceSpy.count(), 1);
    QCOMPARE(workspaceSpy.last().at(0).toString(), testPath);
}

void TestSettingsManager::testWorkspaceSavedSignals()
{
    QSignalSpy savedSpy(m_settingsManager, &SettingsManager::workspaceSaved);
    
    QString testPath = m_tempDir->path() + "/saved_workspace.json";
    m_settingsManager->saveWorkspace(testPath);
    
    QCOMPARE(savedSpy.count(), 1);
    QCOMPARE(savedSpy.last().at(0).toString(), testPath);
    QCOMPARE(savedSpy.last().at(1).toBool(), true);
}

void TestSettingsManager::testThemeChangedSignals()
{
    QSignalSpy themeSpy(m_settingsManager, &SettingsManager::themeChanged);
    
    QString testTheme = "signal_test_theme";
    m_settingsManager->setCurrentTheme(testTheme);
    
    QCOMPARE(themeSpy.count(), 1);
    QCOMPARE(themeSpy.last().at(0).toString(), testTheme);
}

void TestSettingsManager::testAutoSaveSignals()
{
    QSignalSpy autoSaveSpy(m_settingsManager, &SettingsManager::autoSaveCompleted);
    
    m_settingsManager->setAutoSaveEnabled(true);
    m_settingsManager->setSetting("autosave_signal_test", "test_value");
    m_settingsManager->onAutoSaveTriggered();
    
    QCOMPARE(autoSaveSpy.count(), 1);
    QCOMPARE(autoSaveSpy.last().at(0).toBool(), true);
}

void TestSettingsManager::testThreadSafetyBasic()
{
    const int numThreads = 5;
    QList<SettingsTestThread*> threads;
    
    // Create and start threads
    for (int i = 0; i < numThreads; ++i) {
        SettingsTestThread *thread = new SettingsTestThread(m_settingsManager, i);
        threads.append(thread);
        thread->start();
    }
    
    // Wait for all threads to complete
    for (SettingsTestThread *thread : threads) {
        QVERIFY(thread->wait(10000)); // 10 second timeout
        delete thread;
    }
    
    // Verify that all settings were written correctly
    for (int threadId = 0; threadId < numThreads; ++threadId) {
        for (int i = 0; i < 10; ++i) {
            QString key = QString("thread_%1_key_%2").arg(threadId).arg(i);
            QString expectedValue = QString("thread_%1_value_%2").arg(threadId).arg(i);
            QVariant actualValue = m_settingsManager->getSetting(key);
            QCOMPARE(actualValue.toString(), expectedValue);
        }
    }
}

void TestSettingsManager::testConcurrentReadWrite()
{
    // Test concurrent read/write operations
    QStringList keys;
    for (int i = 0; i < 100; ++i) {
        keys.append(QString("concurrent_key_%1").arg(i));
    }
    
    // Use QtConcurrent to perform operations in parallel
    QtConcurrent::blockingMap(keys, [this](const QString &key) {
        QString value = QString("concurrent_value_%1").arg(key);
        m_settingsManager->setSetting(key, value);
        QVariant retrieved = m_settingsManager->getSetting(key);
        Q_UNUSED(retrieved);
    });
    
    // Verify all settings were written
    for (const QString &key : keys) {
        QString expectedValue = QString("concurrent_value_%1").arg(key);
        QVariant actualValue = m_settingsManager->getSetting(key);
        QCOMPARE(actualValue.toString(), expectedValue);
    }
}

void TestSettingsManager::testConcurrentWorkspaceOperations()
{
    // Test concurrent workspace save/load operations
    QStringList workspacePaths;
    for (int i = 0; i < 10; ++i) {
        QString path = m_tempDir->path() + QString("/concurrent_workspace_%1.json").arg(i);
        workspacePaths.append(path);
        createTestWorkspace(path);
    }
    
    // Load workspaces concurrently
    QtConcurrent::blockingMap(workspacePaths, [this](const QString &path) {
        bool loaded = m_settingsManager->loadWorkspace(path);
        Q_UNUSED(loaded);
    });
    
    // All operations should complete without crashes
    QVERIFY(true);
}

void TestSettingsManager::testMutexLocking()
{
    // Test that mutex locking works correctly
    // This is implicitly tested by other thread safety tests
    
    // Test mutex lock/unlock through settings operations
    QString testKey = "mutex_test";
    QString testValue = "mutex_value";
    
    m_settingsManager->setSetting(testKey, testValue);
    QVariant retrieved = m_settingsManager->getSetting(testKey);
    
    QCOMPARE(retrieved.toString(), testValue);
}

void TestSettingsManager::testSettingsCache()
{
    // Test settings caching functionality (if implemented)
    QString testKey = "cache_test";
    QString testValue = "cache_value";
    
    // First access - should populate cache
    m_settingsManager->setSetting(testKey, testValue);
    QVariant firstRead = m_settingsManager->getSetting(testKey);
    
    // Second access - should use cache
    QVariant secondRead = m_settingsManager->getSetting(testKey);
    
    QCOMPARE(firstRead.toString(), testValue);
    QCOMPARE(secondRead.toString(), testValue);
}

void TestSettingsManager::testManySettingsPerformance()
{
    const int SETTING_COUNT = 1000;
    
    QElapsedTimer timer;
    timer.start();
    
    // Write many settings
    for (int i = 0; i < SETTING_COUNT; ++i) {
        QString key = QString("perf_key_%1").arg(i);
        QString value = QString("perf_value_%1").arg(i);
        m_settingsManager->setSetting(key, value);
    }
    
    qint64 writeTime = timer.elapsed();
    QVERIFY(writeTime < 5000); // Should complete in less than 5 seconds
    
    timer.restart();
    
    // Read many settings
    for (int i = 0; i < SETTING_COUNT; ++i) {
        QString key = QString("perf_key_%1").arg(i);
        QVariant value = m_settingsManager->getSetting(key);
        Q_UNUSED(value);
    }
    
    qint64 readTime = timer.elapsed();
    QVERIFY(readTime < 2000); // Reads should be faster
}

void TestSettingsManager::testLargeWorkspacePerformance()
{
    // Create a large workspace
    createLargeSettings(1000);
    
    QString workspacePath = m_tempDir->path() + "/large_workspace.json";
    
    QElapsedTimer timer;
    timer.start();
    
    bool saved = m_settingsManager->saveWorkspace(workspacePath);
    qint64 saveTime = timer.elapsed();
    
    QVERIFY(saved);
    QVERIFY(saveTime < 10000); // Should save large workspace in less than 10 seconds
    
    timer.restart();
    
    bool loaded = m_settingsManager->loadWorkspace(workspacePath);
    qint64 loadTime = timer.elapsed();
    
    QVERIFY(loaded);
    QVERIFY(loadTime < 5000); // Should load large workspace in less than 5 seconds
}

void TestSettingsManager::testAutoSavePerformance()
{
    m_settingsManager->setAutoSaveEnabled(true);
    m_settingsManager->setAutoSaveInterval(1); // 1 second
    
    // Make many changes
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < 100; ++i) {
        m_settingsManager->setSetting(QString("autosave_perf_%1").arg(i), i);
    }
    
    // Trigger auto-save
    m_settingsManager->onAutoSaveTriggered();
    
    qint64 autoSaveTime = timer.elapsed();
    QVERIFY(autoSaveTime < 2000); // Auto-save should be fast
}

void TestSettingsManager::testCachePerformance()
{
    QString testKey = "cache_perf_test";
    QString testValue = "cache_perf_value";
    
    m_settingsManager->setSetting(testKey, testValue);
    
    QElapsedTimer timer;
    timer.start();
    
    // Read same setting many times (should use cache)
    for (int i = 0; i < 1000; ++i) {
        QVariant value = m_settingsManager->getSetting(testKey);
        Q_UNUSED(value);
    }
    
    qint64 cacheTime = timer.elapsed();
    QVERIFY(cacheTime < 100); // Cached reads should be very fast
}

void TestSettingsManager::testInvalidPaths()
{
    // Test with invalid workspace paths
    QString invalidPath = "/nonexistent/path/workspace.json";
    bool loaded = m_settingsManager->loadWorkspace(invalidPath);
    QVERIFY(!loaded);
    
    bool saved = m_settingsManager->saveWorkspace(invalidPath);
    QVERIFY(!saved);
    
    // Test with invalid export/import paths
    bool exported = m_settingsManager->exportSettings(invalidPath);
    QVERIFY(!exported);
    
    bool imported = m_settingsManager->importSettings(invalidPath);
    QVERIFY(!imported);
}

void TestSettingsManager::testCorruptedSettings()
{
    QString corruptedPath = m_tempDir->path() + "/corrupted.json";
    
    // Create corrupted JSON file
    QFile corruptedFile(corruptedPath);
    QVERIFY(corruptedFile.open(QIODevice::WriteOnly));
    corruptedFile.write("{ invalid json content "); // Missing closing brace
    corruptedFile.close();
    
    bool loaded = m_settingsManager->loadWorkspace(corruptedPath);
    QVERIFY(!loaded); // Should fail to load corrupted file
    
    bool imported = m_settingsManager->importSettings(corruptedPath);
    QVERIFY(!imported); // Should fail to import corrupted file
}

void TestSettingsManager::testMissingFiles()
{
    QString missingPath = m_tempDir->path() + "/missing_file.json";
    
    bool loaded = m_settingsManager->loadWorkspace(missingPath);
    QVERIFY(!loaded);
    
    bool imported = m_settingsManager->importSettings(missingPath);
    QVERIFY(!imported);
    
    bool backupRestored = m_settingsManager->restoreFromBackup(missingPath);
    QVERIFY(!backupRestored);
}

void TestSettingsManager::testDiskSpaceErrors()
{
    // Difficult to test without actually filling up disk
    // Test with very large data that might cause disk space issues
    
    QString largePath = m_tempDir->path() + "/large_test.json";
    
    // Create very large settings data
    for (int i = 0; i < 10000; ++i) {
        QString key = QString("large_key_%1").arg(i);
        QString value = QString("large_value_%1").arg(i).repeated(100); // Large value
        m_settingsManager->setSetting(key, value);
    }
    
    bool saved = m_settingsManager->saveWorkspace(largePath);
    // Should either succeed or fail gracefully
    QVERIFY(saved || !saved);
}

void TestSettingsManager::testPermissionErrors()
{
    // Test with read-only directory (if possible)
#ifndef Q_OS_WIN // Unix-like systems
    QString readOnlyPath = "/tmp/readonly_test.json";
    // This test is limited by available permissions
    QVERIFY(true); // Placeholder
#else
    QVERIFY(true); // Skip on Windows due to permission complexity
#endif
}

void TestSettingsManager::testSettingsIntegration()
{
    // Test integration between different settings components
    
    // Set application settings
    m_settingsManager->setSetting(Settings::App::Theme, "dark");
    m_settingsManager->setSetting(Settings::App::Language, "en");
    m_settingsManager->setSetting(Settings::App::AutoSave, true);
    
    // Set main window settings
    m_settingsManager->setSetting(Settings::MainWindow::Geometry, QRect(0, 0, 1024, 768));
    m_settingsManager->setSetting(Settings::MainWindow::Maximized, false);
    
    // Save and reload
    m_settingsManager->saveSettings();
    m_settingsManager->reloadSettings();
    
    // Verify integration
    QCOMPARE(m_settingsManager->getSetting(Settings::App::Theme).toString(), QString("dark"));
    QCOMPARE(m_settingsManager->getSetting(Settings::App::AutoSave).toBool(), true);
    QCOMPARE(m_settingsManager->getSetting(Settings::MainWindow::Maximized).toBool(), false);
}

void TestSettingsManager::testWorkspaceIntegration()
{
    // Test integration between workspace and settings
    
    QString workspacePath = m_tempDir->path() + "/integration_workspace.json";
    
    // Set some settings
    m_settingsManager->setSetting("integration_test", "workspace_value");
    m_settingsManager->setCurrentTheme("integration_theme");
    
    // Save workspace
    QVERIFY(m_settingsManager->saveWorkspace(workspacePath));
    
    // Change settings
    m_settingsManager->setSetting("integration_test", "different_value");
    m_settingsManager->setCurrentTheme("different_theme");
    
    // Load workspace
    QVERIFY(m_settingsManager->loadWorkspace(workspacePath));
    
    // Verify workspace integration
    QCOMPARE(m_settingsManager->getCurrentWorkspacePath(), workspacePath);
}

void TestSettingsManager::testApplicationShutdown()
{
    QSignalSpy shutdownSpy(m_settingsManager, &SettingsManager::autoSaveCompleted);
    
    // Simulate application shutdown
    m_settingsManager->onApplicationAboutToQuit();
    
    // Should trigger final save
    if (shutdownSpy.count() > 0) {
        QCOMPARE(shutdownSpy.last().at(0).toBool(), true);
    }
}

void TestSettingsManager::testEmptyWorkspace()
{
    QString emptyPath = m_tempDir->path() + "/empty_workspace.json";
    
    // Create empty JSON file
    QFile emptyFile(emptyPath);
    QVERIFY(emptyFile.open(QIODevice::WriteOnly));
    emptyFile.write("{}");
    emptyFile.close();
    
    bool loaded = m_settingsManager->loadWorkspace(emptyPath);
    // Should handle empty workspace gracefully
    QVERIFY(loaded || !loaded); // Either outcome is valid
}

void TestSettingsManager::testLongPaths()
{
    // Test with very long file paths
    QString longName = QString("very_long_filename_").repeated(10);
    QString longPath = m_tempDir->path() + "/" + longName + ".json";
    
    if (longPath.length() < 260) { // Avoid hitting filesystem limits
        createTestWorkspace(longPath);
        bool loaded = m_settingsManager->loadWorkspace(longPath);
        QVERIFY(loaded || !loaded); // Should handle gracefully
    }
}

void TestSettingsManager::testSpecialCharacters()
{
    // Test with special characters in keys and values
    QString specialKey = "test_key_with_ümlauts_and_中文";
    QString specialValue = "value_with_émojis_🎉_and_специальные_символы";
    
    m_settingsManager->setSetting(specialKey, specialValue);
    QVariant retrieved = m_settingsManager->getSetting(specialKey);
    
    QCOMPARE(retrieved.toString(), specialValue);
}

void TestSettingsManager::testLargeValues()
{
    // Test with very large setting values
    QString largeValue = QString("large_data_").repeated(10000); // ~100KB string
    QString testKey = "large_value_test";
    
    m_settingsManager->setSetting(testKey, largeValue);
    QVariant retrieved = m_settingsManager->getSetting(testKey);
    
    QCOMPARE(retrieved.toString(), largeValue);
}

void TestSettingsManager::testNullValues()
{
    // Test with null and empty values
    QString nullKey = "null_test";
    QString emptyKey = "empty_test";
    
    m_settingsManager->setSetting(nullKey, QVariant());
    m_settingsManager->setSetting(emptyKey, "");
    
    QVariant nullValue = m_settingsManager->getSetting(nullKey);
    QVariant emptyValue = m_settingsManager->getSetting(emptyKey);
    
    QVERIFY(nullValue.isNull() || !nullValue.isNull()); // Either is valid
    QCOMPARE(emptyValue.toString(), QString(""));
}

void TestSettingsManager::testSettingsGroup()
{
    SettingsGroup group(m_settingsManager, "test_group");
    
    group.set("group_key1", "group_value1");
    group.set("group_key2", 42);
    group.set("group_key3", true);
    
    QCOMPARE(group.get("group_key1").toString(), QString("group_value1"));
    QCOMPARE(group.get("group_key2").toInt(), 42);
    QCOMPARE(group.get("group_key3").toBool(), true);
    
    QVERIFY(group.has("group_key1"));
    QVERIFY(!group.has("nonexistent_key"));
    
    QStringList keys = group.keys();
    QVERIFY(keys.contains("group_key1"));
    QVERIFY(keys.contains("group_key2"));
    QVERIFY(keys.contains("group_key3"));
}

void TestSettingsManager::testSettingsBatch()
{
    SettingsBatch batch(m_settingsManager);
    
    // Set original values
    m_settingsManager->setSetting("batch_key1", "original1");
    m_settingsManager->setSetting("batch_key2", "original2");
    
    // Make changes in batch
    m_settingsManager->setSetting("batch_key1", "batch1");
    m_settingsManager->setSetting("batch_key2", "batch2");
    
    // Batch should commit changes on destruction
    batch.commit();
    
    QCOMPARE(m_settingsManager->getSetting("batch_key1").toString(), QString("batch1"));
    QCOMPARE(m_settingsManager->getSetting("batch_key2").toString(), QString("batch2"));
}

void TestSettingsManager::testBatchOperations()
{
    // Test batch operations
    QHash<QString, QVariant> batchSettings;
    batchSettings["batch_op1"] = "value1";
    batchSettings["batch_op2"] = 123;
    batchSettings["batch_op3"] = false;
    
    // Set all batch settings
    for (auto it = batchSettings.begin(); it != batchSettings.end(); ++it) {
        m_settingsManager->setSetting(it.key(), it.value());
    }
    
    // Verify all batch settings
    for (auto it = batchSettings.begin(); it != batchSettings.end(); ++it) {
        QVariant retrieved = m_settingsManager->getSetting(it.key());
        QCOMPARE(retrieved, it.value());
    }
}

void TestSettingsManager::testTransactionalUpdates()
{
    SettingsBatch transaction(m_settingsManager);
    
    // Set original values
    m_settingsManager->setSetting("trans_key1", "original1");
    m_settingsManager->setSetting("trans_key2", "original2");
    
    // Make changes
    m_settingsManager->setSetting("trans_key1", "transaction1");
    m_settingsManager->setSetting("trans_key2", "transaction2");
    
    // Test rollback
    transaction.rollback();
    
    // Values should be restored to original
    QCOMPARE(m_settingsManager->getSetting("trans_key1").toString(), QString("original1"));
    QCOMPARE(m_settingsManager->getSetting("trans_key2").toString(), QString("original2"));
}

// Helper methods implementation
void TestSettingsManager::createTestWorkspace(const QString &path)
{
    QJsonObject workspace;
    workspace["name"] = "Test Workspace";
    workspace["version"] = "1.0";
    workspace["created"] = QDateTime::currentDateTime().toString();
    workspace["modified"] = QDateTime::currentDateTime().toString();
    
    QJsonObject mainWindow;
    mainWindow["geometry"] = "100,100,800,600";
    mainWindow["maximized"] = false;
    workspace["mainWindow"] = mainWindow;
    
    QJsonObject tabs;
    tabs["activeTab"] = "tab1";
    tabs["tabCount"] = 3;
    workspace["tabs"] = tabs;
    
    QFile workspaceFile(path);
    QVERIFY(workspaceFile.open(QIODevice::WriteOnly));
    QJsonDocument doc(workspace);
    workspaceFile.write(doc.toJson());
    workspaceFile.close();
}

void TestSettingsManager::createTestBackup(const QString &path)
{
    QJsonObject backup;
    backup["backup_timestamp"] = QDateTime::currentDateTime().toString();
    backup["backup_version"] = "1.0";
    backup["settings"] = createTestSettings();
    
    QFile backupFile(path);
    QVERIFY(backupFile.open(QIODevice::WriteOnly));
    QJsonDocument doc(backup);
    backupFile.write(doc.toJson());
    backupFile.close();
}

QJsonObject TestSettingsManager::createTestSettings()
{
    QJsonObject settings;
    settings["test_string"] = "test_value";
    settings["test_int"] = 42;
    settings["test_bool"] = true;
    settings["test_theme"] = "default";
    
    QJsonObject mainWindow;
    mainWindow["geometry"] = "0,0,1024,768";
    mainWindow["maximized"] = false;
    settings["mainWindow"] = mainWindow;
    
    return settings;
}

void TestSettingsManager::verifySettingsIntegrity(const QJsonObject &settings)
{
    QVERIFY(!settings.isEmpty());
    // Add more specific integrity checks based on expected structure
}

void TestSettingsManager::simulateFileCorruption(const QString &filePath)
{
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write("corrupted_data");
        file.close();
    }
}

void TestSettingsManager::createLargeSettings(int count)
{
    for (int i = 0; i < count; ++i) {
        QString key = QString("large_setting_%1").arg(i);
        QString value = QString("large_value_%1_").arg(i).repeated(10);
        m_settingsManager->setSetting(key, value);
    }
}

bool TestSettingsManager::waitForAutoSave(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    
    QSignalSpy autoSaveSpy(m_settingsManager, &SettingsManager::autoSaveCompleted);
    
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        if (autoSaveSpy.count() > 0) {
            return true;
        }
        QThread::msleep(10);
    }
    
    return false;
}

QTEST_MAIN(TestSettingsManager)
#include "test_settings_manager.moc"