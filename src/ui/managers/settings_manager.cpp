#include "settings_manager.h"
#include "../../../mainwindow.h"
#include "tab_manager.h"

#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QtCore/QLoggingCategory>

Q_LOGGING_CATEGORY(settingsManager, "Monitor.SettingsManager")

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
    , m_applicationSettings(nullptr)
    , m_workspaceSettings(nullptr)
    , m_autoSaveTimer(nullptr)
    , m_autoSaveEnabled(true)
    , m_autoSaveInterval(300) // 5 minutes
    , m_settingsDirty(false)
    , m_maxRecentWorkspaces(10)
    , m_currentTheme("default")
    , m_settingsVersion(CURRENT_SETTINGS_VERSION)
    , m_maxBackups(5)
    , m_trackingEnabled(true)
    , m_cacheEnabled(true)
    , m_fileWatchTimer(nullptr)
{
    initializeSettings();
    setupAutoSave();
    
    // Connect to application quit signal
    connect(QApplication::instance(), &QApplication::aboutToQuit, 
            this, &SettingsManager::onApplicationAboutToQuit);
    
    qCInfo(settingsManager) << "SettingsManager initialized";
}

SettingsManager::~SettingsManager()
{
    saveSettings();
    qCInfo(settingsManager) << "SettingsManager destroyed";
}

void SettingsManager::initializeSettings()
{
    setupSettingsFiles();
    
    // Initialize QSettings objects
    m_applicationSettings = new QSettings(m_files.application, QSettings::IniFormat, this);
    
    // Load or create default settings
    if (!QFileInfo::exists(m_files.application)) {
        qCInfo(settingsManager) << "Creating default application settings";
        resetToDefaults();
    } else {
        qCInfo(settingsManager) << "Loading existing application settings";
        validateSettings();
    }
    
    // Load recent workspaces
    m_recentWorkspaces = m_applicationSettings->value("recentWorkspaces").toStringList();
    
    // Load current workspace path
    m_currentWorkspacePath = m_applicationSettings->value("lastWorkspace").toString();
    
    qCDebug(settingsManager) << "Settings initialized from" << getSettingsDirectory();
}

void SettingsManager::setupSettingsFiles()
{
    QString settingsDir = getSettingsDirectory();
    QString workspacesDir = getWorkspacesDirectory();
    QString backupsDir = getBackupsDirectory();
    QString themesDir = getThemesDirectory();
    
    // Ensure directories exist
    ensureDirectoryExists(settingsDir);
    ensureDirectoryExists(workspacesDir);
    ensureDirectoryExists(backupsDir);
    ensureDirectoryExists(themesDir);
    
    // Set up file paths
    m_files.application = QDir(settingsDir).filePath("application.ini");
    m_files.workspace = QDir(workspacesDir).filePath("current.json");
    m_files.recent = QDir(settingsDir).filePath("recent.ini");
    m_files.themes = QDir(themesDir).filePath("themes.json");
    m_files.backup = backupsDir;
    
    qCDebug(settingsManager) << "Settings files configured:";
    qCDebug(settingsManager) << "  Application:" << m_files.application;
    qCDebug(settingsManager) << "  Workspace:" << m_files.workspace;
    qCDebug(settingsManager) << "  Backups:" << m_files.backup;
}

void SettingsManager::setupAutoSave()
{
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(m_autoSaveInterval * 1000);
    m_autoSaveTimer->setSingleShot(false);
    
    connect(m_autoSaveTimer, &QTimer::timeout, this, &SettingsManager::onAutoSaveTriggered);
    
    if (m_autoSaveEnabled) {
        m_autoSaveTimer->start();
    }
}

QString SettingsManager::getSettingsDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString SettingsManager::getWorkspacesDirectory() const
{
    return QDir(getSettingsDirectory()).filePath("workspaces");
}

QString SettingsManager::getBackupsDirectory() const
{
    return QDir(getSettingsDirectory()).filePath("backups");
}

QString SettingsManager::getThemesDirectory() const
{
    return QDir(getSettingsDirectory()).filePath("themes");
}

bool SettingsManager::ensureDirectoryExists(const QString &path) const
{
    QDir dir;
    if (!dir.exists(path)) {
        bool created = dir.mkpath(path);
        if (created) {
            qCDebug(settingsManager) << "Created directory:" << path;
        } else {
            qCWarning(settingsManager) << "Failed to create directory:" << path;
        }
        return created;
    }
    return true;
}

// Core settings methods
QVariant SettingsManager::getSetting(const QString &key, const QVariant &defaultValue) const
{
    QMutexLocker locker(&m_settingsMutex);
    
    if (m_cacheEnabled && m_settingsCache.contains(key)) {
        return m_settingsCache[key];
    }
    
    QVariant value = m_applicationSettings->value(key, defaultValue);
    
    if (m_cacheEnabled) {
        m_settingsCache[key] = value;
    }
    
    return value;
}

void SettingsManager::setSetting(const QString &key, const QVariant &value)
{
    QMutexLocker locker(&m_settingsMutex);
    
    QVariant oldValue = m_applicationSettings->value(key);
    if (oldValue != value) {
        m_applicationSettings->setValue(key, value);
        
        if (m_cacheEnabled) {
            m_settingsCache[key] = value;
        }
        
        m_settingsDirty = true;
        
        // Track changes
        if (m_trackingEnabled) {
            m_previousValues[key] = oldValue;
        }
        
        emit settingsChanged(key, value);
        qCDebug(settingsManager) << "Setting changed:" << key << "=" << value;
    }
}

bool SettingsManager::hasSetting(const QString &key) const
{
    QMutexLocker locker(&m_settingsMutex);
    return m_applicationSettings->contains(key);
}

void SettingsManager::removeSetting(const QString &key)
{
    QMutexLocker locker(&m_settingsMutex);
    
    if (m_applicationSettings->contains(key)) {
        m_applicationSettings->remove(key);
        
        if (m_cacheEnabled) {
            m_settingsCache.remove(key);
        }
        
        m_settingsDirty = true;
        emit settingsChanged(key, QVariant());
        qCDebug(settingsManager) << "Setting removed:" << key;
    }
}

// Workspace methods (stubs)
bool SettingsManager::saveWorkspace(const QString &workspacePath)
{
    QString path = workspacePath.isEmpty() ? m_currentWorkspacePath : workspacePath;
    
    if (path.isEmpty()) {
        // Generate default workspace path
        path = QDir(getWorkspacesDirectory()).filePath("default.json");
    }
    
    QJsonObject workspaceData = createWorkspaceJson("Default Workspace");
    
    bool success = saveWorkspaceToFile(path, workspaceData);
    if (success) {
        m_currentWorkspacePath = path;
        addRecentWorkspace(path);
        emit workspaceSaved(path, true);
        qCInfo(settingsManager) << "Workspace saved to:" << path;
    } else {
        emit workspaceSaved(path, false);
        qCWarning(settingsManager) << "Failed to save workspace to:" << path;
    }
    
    return success;
}

bool SettingsManager::loadWorkspace(const QString &workspacePath)
{
    QString path = workspacePath.isEmpty() ? m_currentWorkspacePath : workspacePath;
    
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        qCWarning(settingsManager) << "Workspace file not found:" << path;
        emit workspaceLoaded(path, false);
        return false;
    }
    
    QJsonObject workspaceData = loadWorkspaceFromFile(path);
    if (workspaceData.isEmpty()) {
        qCWarning(settingsManager) << "Failed to load workspace from:" << path;
        emit workspaceLoaded(path, false);
        return false;
    }
    
    m_currentWorkspacePath = path;
    addRecentWorkspace(path);
    emit workspaceLoaded(path, true);
    emit workspaceChanged(path);
    
    qCInfo(settingsManager) << "Workspace loaded from:" << path;
    return true;
}

QJsonObject SettingsManager::createWorkspaceJson(const QString &name) const
{
    QJsonObject workspace;
    workspace["name"] = name;
    workspace["version"] = "1.0";
    workspace["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    workspace["modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Placeholder sections
    workspace["mainWindow"] = QJsonObject();
    workspace["tabs"] = QJsonObject();
    workspace["structures"] = QJsonObject();
    workspace["widgets"] = QJsonObject();
    workspace["testFramework"] = QJsonObject();
    workspace["globalSettings"] = QJsonObject();
    
    return workspace;
}

bool SettingsManager::saveWorkspaceToFile(const QString &filePath, const QJsonObject &data)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    QJsonDocument doc(data);
    qint64 written = file.write(doc.toJson());
    return written > 0;
}

QJsonObject SettingsManager::loadWorkspaceFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }
    
    QByteArray data = file.readAll();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qCWarning(settingsManager) << "JSON parse error in workspace file:" << error.errorString();
        return QJsonObject();
    }
    
    return doc.object();
}

// Recent workspaces
QStringList SettingsManager::getRecentWorkspaces() const
{
    return m_recentWorkspaces;
}

void SettingsManager::addRecentWorkspace(const QString &path)
{
    m_recentWorkspaces.removeAll(path);
    m_recentWorkspaces.prepend(path);
    
    while (m_recentWorkspaces.size() > m_maxRecentWorkspaces) {
        m_recentWorkspaces.removeLast();
    }
    
    m_applicationSettings->setValue("recentWorkspaces", m_recentWorkspaces);
    m_settingsDirty = true;
}

// UI state management (stubs)
void SettingsManager::saveMainWindowState(MainWindow *mainWindow)
{
    if (!mainWindow) return;
    
    setSetting(Settings::MainWindow::Geometry, mainWindow->saveGeometry());
    setSetting(Settings::MainWindow::State, mainWindow->saveState());
    setSetting(Settings::MainWindow::Maximized, mainWindow->isMaximized());
    
    qCDebug(settingsManager) << "Main window state saved";
}

void SettingsManager::restoreMainWindowState(MainWindow *mainWindow)
{
    if (!mainWindow) return;
    
    QByteArray geometry = getSetting(Settings::MainWindow::Geometry).toByteArray();
    if (!geometry.isEmpty()) {
        mainWindow->restoreGeometry(geometry);
    }
    
    QByteArray state = getSetting(Settings::MainWindow::State).toByteArray();
    if (!state.isEmpty()) {
        mainWindow->restoreState(state);
    }
    
    bool maximized = getSetting(Settings::MainWindow::Maximized, false).toBool();
    if (maximized) {
        mainWindow->showMaximized();
    }
    
    qCDebug(settingsManager) << "Main window state restored";
}

void SettingsManager::saveTabManagerState(TabManager *tabManager)
{
    if (!tabManager) return;
    
    QJsonObject tabState = tabManager->saveState();
    // Convert to QVariant for storage
    setSetting(Settings::Tabs::ActiveTab, tabState["activeTab"].toString());
    
    qCDebug(settingsManager) << "Tab manager state saved";
}

void SettingsManager::restoreTabManagerState(TabManager *tabManager)
{
    if (!tabManager) return;
    
    // Create a simple state object for restoration
    QJsonObject tabState;
    tabState["activeTab"] = getSetting(Settings::Tabs::ActiveTab).toString();
    
    tabManager->restoreState(tabState);
    
    qCDebug(settingsManager) << "Tab manager state restored";
}

// Theme methods (stubs)
QString SettingsManager::getCurrentTheme() const
{
    return m_currentTheme;
}

void SettingsManager::setCurrentTheme(const QString &themeName)
{
    if (m_currentTheme != themeName) {
        m_currentTheme = themeName;
        setSetting(Settings::App::Theme, themeName);
        emit themeChanged(themeName);
        qCInfo(settingsManager) << "Theme changed to:" << themeName;
    }
}

QStringList SettingsManager::getAvailableThemes() const
{
    return m_availableThemes;
}

// Auto-save methods
void SettingsManager::setAutoSaveEnabled(bool enabled)
{
    m_autoSaveEnabled = enabled;
    if (enabled) {
        m_autoSaveTimer->start();
    } else {
        m_autoSaveTimer->stop();
    }
    setSetting(Settings::App::AutoSave, enabled);
}

void SettingsManager::setAutoSaveInterval(int seconds)
{
    m_autoSaveInterval = seconds;
    m_autoSaveTimer->setInterval(seconds * 1000);
    setSetting(Settings::App::AutoSaveInterval, seconds);
}

// Settings validation (stub)
bool SettingsManager::validateSettings() const
{
    // Basic validation
    if (!m_applicationSettings) {
        return false;
    }
    
    // Check settings version
    int version = m_applicationSettings->value("version", 1).toInt();
    if (version > CURRENT_SETTINGS_VERSION) {
        qCWarning(settingsManager) << "Settings version is newer than supported:" << version;
        return false;
    }
    
    return true;
}

// Default settings
void SettingsManager::resetToDefaults()
{
    QMutexLocker locker(&m_settingsMutex);
    
    m_applicationSettings->clear();
    
    // Set default values
    m_applicationSettings->setValue("version", CURRENT_SETTINGS_VERSION);
    m_applicationSettings->setValue(Settings::App::Theme, "default");
    m_applicationSettings->setValue(Settings::App::AutoSave, true);
    m_applicationSettings->setValue(Settings::App::AutoSaveInterval, 300);
    m_applicationSettings->setValue(Settings::App::MaxRecentWorkspaces, 10);
    m_applicationSettings->setValue(Settings::MainWindow::ToolbarVisible, true);
    m_applicationSettings->setValue(Settings::MainWindow::StatusBarVisible, true);
    m_applicationSettings->setValue(Settings::Tabs::MaxTabs, 20);
    
    m_settingsDirty = true;
    
    qCInfo(settingsManager) << "Settings reset to defaults";
}

QJsonObject SettingsManager::getDefaultSettings() const
{
    QJsonObject defaults;
    defaults["version"] = CURRENT_SETTINGS_VERSION;
    defaults[Settings::App::Theme] = "default";
    defaults[Settings::App::AutoSave] = true;
    defaults[Settings::App::AutoSaveInterval] = 300;
    return defaults;
}

// Backup and restore (stubs)
bool SettingsManager::createBackup(const QString &backupPath)
{
    Q_UNUSED(backupPath)
    qCDebug(settingsManager) << "Create backup requested";
    return true;
}

// Public slots
void SettingsManager::saveSettings()
{
    if (m_settingsDirty && m_applicationSettings) {
        QMutexLocker locker(&m_settingsMutex);
        m_applicationSettings->sync();
        m_settingsDirty = false;
        qCDebug(settingsManager) << "Settings saved to disk";
    }
}

void SettingsManager::reloadSettings()
{
    QMutexLocker locker(&m_settingsMutex);
    if (m_applicationSettings) {
        m_applicationSettings->sync();
        m_settingsCache.clear();
        qCInfo(settingsManager) << "Settings reloaded from disk";
    }
}

void SettingsManager::onAutoSaveTriggered()
{
    saveSettings();
    emit autoSaveCompleted(true);
    qCDebug(settingsManager) << "Auto-save completed";
}

void SettingsManager::onApplicationAboutToQuit()
{
    saveSettings();
    qCInfo(settingsManager) << "Final settings save on application quit";
}

// Settings group helper classes
SettingsGroup::SettingsGroup(SettingsManager *manager, const QString &prefix)
    : m_manager(manager)
    , m_prefix(prefix)
{
    if (!m_prefix.endsWith('/')) {
        m_prefix += '/';
    }
}

SettingsGroup::~SettingsGroup() = default;

QVariant SettingsGroup::get(const QString &key, const QVariant &defaultValue) const
{
    return m_manager->getSetting(m_prefix + key, defaultValue);
}

void SettingsGroup::set(const QString &key, const QVariant &value)
{
    m_manager->setSetting(m_prefix + key, value);
}

bool SettingsGroup::has(const QString &key) const
{
    return m_manager->hasSetting(m_prefix + key);
}

void SettingsGroup::remove(const QString &key)
{
    m_manager->removeSetting(m_prefix + key);
}

// SettingsBatch implementation
SettingsBatch::SettingsBatch(SettingsManager *manager)
    : m_manager(manager)
    , m_committed(false)
{
    Q_UNUSED(m_manager) // Suppress warning - will be used when batch operations are implemented
    // TODO: Implement batch operations
}

SettingsBatch::~SettingsBatch()
{
    if (!m_committed) {
        rollback();
    }
}

void SettingsBatch::commit()
{
    m_committed = true;
    // TODO: Apply all batched changes
}

void SettingsBatch::rollback()
{
    // TODO: Restore original values
}

// Missing SettingsManager implementation
void SettingsManager::onFileSystemChanged(const QString &path)
{
    qCDebug(settingsManager) << "File system changed:" << path;
    // TODO: Handle file system changes for workspace monitoring
}