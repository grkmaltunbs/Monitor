#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <QObject>
#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>

// Forward declarations
class MainWindow;
class TabManager;

/**
 * @brief Manages all application settings and workspace persistence
 * 
 * The SettingsManager provides:
 * - Application settings (preferences, UI state)
 * - Workspace persistence (tabs, windows, layouts)
 * - Auto-save functionality
 * - Settings migration and validation
 * - Thread-safe settings access
 * - Backup and restore capabilities
 */
class SettingsManager : public QObject
{
    Q_OBJECT

public:
    explicit SettingsManager(QObject *parent = nullptr);
    ~SettingsManager();

    // Workspace management
    bool saveWorkspace(const QString &workspacePath = QString());
    bool loadWorkspace(const QString &workspacePath = QString());
    bool createNewWorkspace(const QString &name = QString());
    QString getCurrentWorkspacePath() const { return m_currentWorkspacePath; }
    QString getCurrentWorkspaceName() const;
    
    // Recent workspaces
    QStringList getRecentWorkspaces() const;
    void addRecentWorkspace(const QString &path);
    void removeRecentWorkspace(const QString &path);
    void clearRecentWorkspaces();
    
    // Application settings
    QVariant getSetting(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setSetting(const QString &key, const QVariant &value);
    bool hasSetting(const QString &key) const;
    void removeSetting(const QString &key);
    void clearSettings();
    
    // UI state management
    void saveMainWindowState(MainWindow *mainWindow);
    void restoreMainWindowState(MainWindow *mainWindow);
    void saveTabManagerState(TabManager *tabManager);
    void restoreTabManagerState(TabManager *tabManager);
    
    // Theme and appearance
    QString getCurrentTheme() const;
    void setCurrentTheme(const QString &themeName);
    QStringList getAvailableThemes() const;
    
    // Auto-save configuration
    void setAutoSaveEnabled(bool enabled);
    bool isAutoSaveEnabled() const { return m_autoSaveEnabled; }
    void setAutoSaveInterval(int seconds);
    int getAutoSaveInterval() const { return m_autoSaveInterval; }
    
    // Settings validation and migration
    bool validateSettings() const;
    bool migrateSettings(int fromVersion, int toVersion);
    int getSettingsVersion() const;
    
    // Backup and restore
    bool createBackup(const QString &backupPath = QString());
    bool restoreFromBackup(const QString &backupPath);
    QStringList getAvailableBackups() const;
    bool deleteBackup(const QString &backupPath);
    
    // Import/Export
    bool exportSettings(const QString &filePath) const;
    bool importSettings(const QString &filePath);
    
    // Default settings
    void resetToDefaults();
    QJsonObject getDefaultSettings() const;

public slots:
    void saveSettings();
    void reloadSettings();
    void onAutoSaveTriggered();
    void onApplicationAboutToQuit();

signals:
    void settingsChanged(const QString &key, const QVariant &value);
    void workspaceChanged(const QString &workspacePath);
    void workspaceSaved(const QString &workspacePath, bool success);
    void workspaceLoaded(const QString &workspacePath, bool success);
    void themeChanged(const QString &themeName);
    void autoSaveCompleted(bool success);
    void settingsValidationFailed(const QString &error);

private slots:
    void onFileSystemChanged(const QString &path);

private:
    // Settings file structure
    struct SettingsFiles {
        QString application;     // Application preferences
        QString workspace;       // Current workspace
        QString recent;         // Recent workspaces
        QString themes;         // Theme configurations
        QString backup;         // Backup directory
    };

    // Workspace structure
    struct WorkspaceData {
        QString name;
        QString version;
        QString created;
        QString modified;
        QJsonObject mainWindow;
        QJsonObject tabs;
        QJsonObject structures;
        QJsonObject widgets;
        QJsonObject testFramework;
        QJsonObject globalSettings;
    };

    // Settings initialization
    void initializeSettings();
    void setupSettingsFiles();
    void createDefaultDirectories();
    void setupAutoSave();
    
    // File operations
    QString getSettingsDirectory() const;
    QString getWorkspacesDirectory() const;
    QString getBackupsDirectory() const;
    QString getThemesDirectory() const;
    bool ensureDirectoryExists(const QString &path) const;
    
    // Workspace operations
    QJsonObject createWorkspaceJson(const QString &name) const;
    bool saveWorkspaceToFile(const QString &filePath, const QJsonObject &data);
    QJsonObject loadWorkspaceFromFile(const QString &filePath);
    bool isValidWorkspaceFile(const QString &filePath) const;
    QString generateWorkspaceName() const;
    
    // Settings validation
    bool validateWorkspaceData(const QJsonObject &data) const;
    bool validateApplicationSettings() const;
    QJsonObject sanitizeSettings(const QJsonObject &settings) const;
    
    // Migration helpers
    QJsonObject migrateWorkspaceV1ToV2(const QJsonObject &oldData) const;
    QJsonObject migrateApplicationSettingsV1ToV2(const QJsonObject &oldData) const;
    
    // Backup operations
    QString generateBackupPath() const;
    bool copyWorkspaceForBackup(const QString &sourcePath, const QString &backupPath);
    void cleanupOldBackups();
    
    // Thread safety
    mutable QMutex m_settingsMutex;
    void lockSettings() const { m_settingsMutex.lock(); }
    void unlockSettings() const { m_settingsMutex.unlock(); }
    QMutexLocker<QMutex> createSettingsLocker() const { return QMutexLocker<QMutex>(&m_settingsMutex); }
    
    // Core settings objects
    QSettings *m_applicationSettings;
    QSettings *m_workspaceSettings;
    
    // File system paths
    SettingsFiles m_files;
    QString m_currentWorkspacePath;
    
    // Auto-save functionality
    QTimer *m_autoSaveTimer;
    bool m_autoSaveEnabled;
    int m_autoSaveInterval;
    bool m_settingsDirty;
    
    // Recent workspaces management
    QStringList m_recentWorkspaces;
    int m_maxRecentWorkspaces;
    
    // Theme management
    QString m_currentTheme;
    QStringList m_availableThemes;
    QJsonObject m_themeSettings;
    
    // Settings validation and migration
    static const int CURRENT_SETTINGS_VERSION = 2;
    int m_settingsVersion;
    
    // Default settings
    QJsonObject m_defaultApplicationSettings;
    QJsonObject m_defaultWorkspaceSettings;
    
    // Backup management
    int m_maxBackups;
    QStringList m_backupList;
    
    // Change tracking
    QHash<QString, QVariant> m_previousValues;
    bool m_trackingEnabled;
    
    // Performance optimization
    mutable QHash<QString, QVariant> m_settingsCache;
    bool m_cacheEnabled;
    
    // File watchers
    QTimer *m_fileWatchTimer;
    QStringList m_watchedFiles;
};

/**
 * @brief Settings group for scoped access to related settings
 */
class SettingsGroup
{
public:
    explicit SettingsGroup(SettingsManager *manager, const QString &prefix);
    ~SettingsGroup();
    
    QVariant get(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void set(const QString &key, const QVariant &value);
    bool has(const QString &key) const;
    void remove(const QString &key);
    QStringList keys() const;
    
private:
    SettingsManager *m_manager;
    QString m_prefix;
};

/**
 * @brief RAII class for batch settings operations
 */
class SettingsBatch
{
public:
    explicit SettingsBatch(SettingsManager *manager);
    ~SettingsBatch();
    
    void commit();
    void rollback();
    
private:
    SettingsManager *m_manager;
    bool m_committed;
    QHash<QString, QVariant> m_originalValues;
};

// Settings constants
namespace Settings {
    // Application settings keys
    namespace App {
        const QString Theme = "app/theme";
        const QString Language = "app/language";
        const QString AutoSave = "app/autoSave";
        const QString AutoSaveInterval = "app/autoSaveInterval";
        const QString MaxRecentWorkspaces = "app/maxRecentWorkspaces";
        const QString LastWorkspace = "app/lastWorkspace";
        const QString StartupBehavior = "app/startupBehavior";
    }
    
    // Main window settings
    namespace MainWindow {
        const QString Geometry = "mainWindow/geometry";
        const QString State = "mainWindow/state";
        const QString Maximized = "mainWindow/maximized";
        const QString ToolbarVisible = "mainWindow/toolbarVisible";
        const QString StatusBarVisible = "mainWindow/statusBarVisible";
        const QString MenuBarVisible = "mainWindow/menuBarVisible";
    }
    
    // Tab settings
    namespace Tabs {
        const QString ActiveTab = "tabs/activeTab";
        const QString TabOrder = "tabs/tabOrder";
        const QString MaxTabs = "tabs/maxTabs";
        const QString AllowReorder = "tabs/allowReorder";
        const QString ShowCloseButtons = "tabs/showCloseButtons";
    }
    
    // Performance settings
    namespace Performance {
        const QString EnableProfiling = "performance/enableProfiling";
        const QString MaxMemoryUsage = "performance/maxMemoryUsage";
        const QString ThreadPoolSize = "performance/threadPoolSize";
        const QString CacheEnabled = "performance/cacheEnabled";
    }
}

#endif // SETTINGS_MANAGER_H