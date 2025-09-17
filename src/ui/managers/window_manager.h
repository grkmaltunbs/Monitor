#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <QObject>
#include <QWidget>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QSplitter>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QHash>
#include <QList>
#include <QString>
#include <QUuid>
#include <QJsonObject>
#include <QJsonArray>
#include <QRect>
#include <QSize>
#include <QPoint>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QToolBar>
#include <QTimer>

// Forward declarations
class StructWindow;
class BaseWidget;
class GridWidget;
class GridLoggerWidget;
class LineChartWidget;
class PieChartWidget;
class BarChartWidget;
class Chart3DWidget;

/**
 * @brief Manages widget windows within a tab
 * 
 * The WindowManager provides:
 * - Multiple window arrangement modes (MDI, Tiled, Stacked)
 * - Window lifecycle management (create, resize, move, close)
 * - Layout persistence and restoration
 * - Window state management (minimized, maximized, normal)
 * - Drag-and-drop window arrangement
 * - Context menus and keyboard shortcuts
 * - Integration with drag-drop from StructWindow
 */
class WindowManager : public QObject
{
    Q_OBJECT

public:
    enum WindowMode {
        MDI_Mode,           // Multiple Document Interface with free-form windows
        Tiled_Mode,         // Grid-based tiling
        Tabbed_Mode,        // Tabbed interface for windows
        Splitter_Mode       // Splitter-based layout
    };

    enum WindowType {
        StructWindowType,
        GridWindowType,
        GridLoggerWindowType,
        LineChartWindowType,
        PieChartWindowType,
        BarChartWindowType,
        Chart3DWindowType,
        CustomWindowType
    };

    enum TileArrangement {
        Horizontal,
        Vertical,
        Grid,
        Cascade
    };

    explicit WindowManager(const QString &tabId, QWidget *parent = nullptr);
    ~WindowManager();

    // Core window operations
    QString createWindow(WindowType type, const QString &title = QString());
    bool closeWindow(const QString &windowId);
    bool minimizeWindow(const QString &windowId);
    bool maximizeWindow(const QString &windowId);
    bool restoreWindow(const QString &windowId);
    bool moveWindow(const QString &windowId, const QPoint &position);
    bool resizeWindow(const QString &windowId, const QSize &size);
    
    // Window access and information
    QWidget* getWindow(const QString &windowId) const;
    QWidget* getWindowContent(const QString &windowId) const;
    QWidget* getContainerWidget() const { return m_containerWidget; }
    QString getActiveWindowId() const;
    QStringList getWindowIds() const;
    QString getWindowTitle(const QString &windowId) const;
    WindowType getWindowType(const QString &windowId) const;
    QRect getWindowGeometry(const QString &windowId) const;
    bool isWindowVisible(const QString &windowId) const;
    bool isWindowMaximized(const QString &windowId) const;
    bool isWindowMinimized(const QString &windowId) const;
    
    // Layout and arrangement
    WindowMode getWindowMode() const { return m_windowMode; }
    void setWindowMode(WindowMode mode);
    void arrangeWindows(TileArrangement arrangement);
    void cascadeWindows();
    void tileWindows();
    void minimizeAllWindows();
    void restoreAllWindows();
    void closeAllWindows();
    
    // Special windows
    QString getStructWindowId() const { return m_structWindowId; }
    StructWindow* getStructWindow() const;
    
    // Settings and persistence
    QJsonObject saveState() const;
    bool restoreState(const QJsonObject &state);
    
    // Drop zone support
    void setDropZonesVisible(bool visible);
    bool areDropZonesVisible() const { return m_dropZonesVisible; }

public slots:
    void setActiveWindow(const QString &windowId);
    void showContextMenu(const QPoint &position);
    void onWindowActivated(const QString &windowId);
    void onWindowClosed(const QString &windowId);

signals:
    void windowCreated(const QString &windowId, WindowType type);
    void windowClosed(const QString &windowId);
    void windowActivated(const QString &windowId);
    void windowMoved(const QString &windowId, const QPoint &position);
    void windowResized(const QString &windowId, const QSize &size);
    void windowStateChanged(const QString &windowId, Qt::WindowState state);
    void activeWindowChanged(const QString &windowId);
    void windowModeChanged(WindowMode mode);
    void layoutChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    // MDI area events
    void onMdiSubWindowActivated(QMdiSubWindow *window);
    void onMdiWindowStateChanged(Qt::WindowStates oldState, Qt::WindowStates newState);
    
    // Context menu actions
    void onCreateGridWindow();
    void onCreateGridLoggerWindow();
    void onCreateLineChartWindow();
    void onCreatePieChartWindow();
    void onCreateBarChartWindow();
    void onCreate3DChartWindow();
    void onCloseActiveWindow();
    void onMinimizeActiveWindow();
    void onMaximizeActiveWindow();
    void onRestoreActiveWindow();
    void onTileHorizontally();
    void onTileVertically();
    void onCascadeWindows();
    void onArrangeGrid();
    
    // Layout mode actions
    void onMDIModeAction();
    void onTiledModeAction();
    void onTabbedModeAction();
    void onSplitterModeAction();

private:
    struct WindowInfo {
        QString id;
        QString title;
        WindowType type;
        QWidget *widget;          // The actual window container
        QWidget *content;         // The widget content
        QMdiSubWindow *mdiWindow; // MDI sub-window (if in MDI mode)
        QRect geometry;
        Qt::WindowState state;
        bool isCloseable;
        bool isMovable;
        bool isResizable;
        QJsonObject settings;
        qint64 created;
        qint64 lastActivated;
        
        WindowInfo() : widget(nullptr), content(nullptr), mdiWindow(nullptr),
                      state(Qt::WindowNoState), isCloseable(true), isMovable(true),
                      isResizable(true), created(0), lastActivated(0) {}
    };

    // UI setup
    void setupContainerWidget();
    void setupMDIArea();
    void setupTiledLayout();
    void setupTabbedLayout();
    void setupSplitterLayout();
    void setupContextMenu();
    void setupToolbar();
    
    // Window creation helpers
    QWidget* createWindowContent(WindowType type, const QString &windowId);
    QString generateWindowId() const;
    QString generateDefaultTitle(WindowType type) const;
    void registerWindow(const QString &windowId, const WindowInfo &info);
    void unregisterWindow(const QString &windowId);
    
    // Layout mode switching
    void switchToMDIMode();
    void switchToTiledMode();
    void switchToTabbedMode();
    void switchToSplitterMode();
    void migrateWindowsBetweenModes(WindowMode fromMode, WindowMode toMode);
    
    // MDI operations
    QMdiSubWindow* createMDISubWindow(QWidget *content, const QString &title);
    void configureMDISubWindow(QMdiSubWindow *subWindow, const WindowInfo &info);
    
    // Tiled layout operations
    void updateTiledLayout();
    void calculateTilePositions(const QList<WindowInfo*> &windows, QRect availableArea);
    QRect calculateTileGeometry(int index, int totalWindows, const QRect &area, TileArrangement arrangement);
    
    // Tabbed layout operations
    void addToTabbedLayout(const QString &windowId, QWidget *content);
    void removeFromTabbedLayout(const QString &windowId);
    
    // Splitter layout operations
    void addToSplitterLayout(const QString &windowId, QWidget *content);
    void removeFromSplitterLayout(const QString &windowId);
    void updateSplitterSizes();
    
    // Window state management
    void saveWindowStates();
    void restoreWindowStates();
    void updateWindowState(const QString &windowId);
    
    // Special window handling
    void ensureStructWindow();
    void createStructWindow();
    void positionStructWindow();
    
    // Drop zones for drag-and-drop
    void createDropZones();
    void showDropZones();
    void hideDropZones();
    QWidget* createDropZone(const QString &text, const QRect &geometry);
    
    // Visual effects
    void animateWindowTransition(QWidget *window, const QRect &fromGeometry, const QRect &toGeometry);
    void applyWindowEffects();
    void updateWindowTitles();
    
    // Tab information
    QString m_tabId;
    
    // Container widgets for different modes
    QWidget *m_containerWidget;
    QMdiArea *m_mdiArea;
    QGridLayout *m_tiledLayout;
    QStackedWidget *m_tabbedWidget;
    QSplitter *m_splitterWidget;
    
    // Window management
    QHash<QString, WindowInfo> m_windows;
    QString m_activeWindowId;
    QString m_structWindowId;
    WindowMode m_windowMode;
    TileArrangement m_tileArrangement;
    
    // Context menu and toolbar
    QMenu *m_contextMenu;
    QToolBar *m_toolbar;
    QActionGroup *m_layoutModeGroup;
    
    // Window creation actions
    QAction *m_createGridAction;
    QAction *m_createGridLoggerAction;
    QAction *m_createLineChartAction;
    QAction *m_createPieChartAction;
    QAction *m_createBarChartAction;
    QAction *m_create3DChartAction;
    
    // Window management actions
    QAction *m_closeWindowAction;
    QAction *m_minimizeWindowAction;
    QAction *m_maximizeWindowAction;
    QAction *m_restoreWindowAction;
    
    // Layout actions
    QAction *m_tileHorizontalAction;
    QAction *m_tileVerticalAction;
    QAction *m_cascadeAction;
    QAction *m_arrangeGridAction;
    
    // Mode switching actions
    QAction *m_mdiModeAction;
    QAction *m_tiledModeAction;
    QAction *m_tabbedModeAction;
    QAction *m_splitterModeAction;
    
    // Drop zones for drag-drop
    bool m_dropZonesVisible;
    QList<QWidget*> m_dropZones;
    
    // Configuration
    int m_maxWindows;
    QSize m_defaultWindowSize;
    QPoint m_defaultWindowOffset;
    bool m_allowOverlapping;
    bool m_snapToGrid;
    int m_gridSize;
    
    // Animation and effects
    QTimer *m_animationTimer;
    bool m_animationsEnabled;
    int m_animationDuration;
    
    // Performance optimization
    bool m_batchUpdates;
    QTimer *m_updateTimer;
    bool m_layoutDirty;
};

/**
 * @brief Custom MDI area with enhanced features
 */
class CustomMdiArea : public QMdiArea
{
    Q_OBJECT

public:
    explicit CustomMdiArea(QWidget *parent = nullptr);

    void setDropZonesVisible(bool visible);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

signals:
    void windowDropped(const QPoint &position, const QString &data);

private:
    bool m_dropZonesVisible;
    QList<QRect> m_dropZones;
    int m_dragHighlightZone;
};

#endif // WINDOW_MANAGER_H