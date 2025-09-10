#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QToolBar>
#include <QTabWidget>
#include <QStatusBar>
#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// Forward declarations for UI components
class TabManager;
class StructWindow;
class SettingsManager;
class PerformanceDashboard;

// Forward declarations for Test Framework
namespace Monitor {
namespace UI {
namespace TestFramework {
class TestManagerWindow;
}
}
namespace TestFramework {
class TestRunner;
}
namespace Packet {
class SimulationSource;
}
}

/**
 * @brief Main application window for the Monitor Application
 * 
 * Provides the complete UI framework including:
 * - Comprehensive toolbar with all control buttons
 * - Dynamic tab management system
 * - Window management within tabs
 * - Settings persistence
 * - Status monitoring
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Public interface for external components
    TabManager* getTabManager() const { return m_tabManager; }
    SettingsManager* getSettingsManager() const { return m_settingsManager; }

public slots:
    void onConnectionStatusChanged(bool connected);
    void onPerformanceMetricsUpdated();
    void onPacketRateUpdated(int packetsPerSecond);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    // Structure management
    void onAddStructClicked();
    
    // Test framework
    void onTestFrameworkClicked();
    
    // Simulation mode
    void onStartSimulationClicked();
    void onStopSimulationClicked();
    
    // Mode switching
    void onEthernetModeSelected();
    void onOfflineModeSelected();
    void onPortSettingsClicked();
    
    // Offline playback controls
    void onPlayPauseClicked();
    void onStopPlaybackClicked();
    void onStepForwardClicked();
    void onStepBackwardClicked();
    void onJumpToTimeClicked();
    void onPlaybackSpeedChanged(int speed);
    
    // Widget creation
    void onCreateGridWidget();
    void onCreateGridLoggerWidget();
    void onCreateLineChartWidget();
    void onCreatePieChartWidget();
    void onCreateBarChartWidget();
    void onCreate3DChartWidget();
    
    // Performance dashboard
    void onPerformanceDashboardClicked();
    
    // Application events
    void onTabCountChanged(int count);
    void onActiveTabChanged(int index);
    
    // Test framework events
    void onTestResultsChanged(bool allPassing, int totalTests, int failedTests);

private:
    void setupUI();
    void createToolbar();
    void createOfflinePlaybackControls();
    void createCentralWidget();
    void createStatusBar();
    void createMenuBar();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void updateToolbarState();
    void updateStatusBar();

    // UI components
    Ui::MainWindow *ui;
    
    // Toolbar components
    QToolBar *m_toolbar;
    
    // Structure management
    QPushButton *m_addStructButton;
    
    // Test framework
    QPushButton *m_testFrameworkButton;
    QLabel *m_testStatusIndicator;
    
    // Simulation controls
    QPushButton *m_startSimulationButton;
    QPushButton *m_stopSimulationButton;
    QActionGroup *m_simulationGroup;
    
    // Mode selection
    QPushButton *m_ethernetModeButton;
    QPushButton *m_offlineModeButton;
    QPushButton *m_portSettingsButton;
    QActionGroup *m_modeGroup;
    
    // Offline playback controls
    QWidget *m_playbackControls;
    QPushButton *m_playPauseButton;
    QPushButton *m_stopPlaybackButton;
    QPushButton *m_stepForwardButton;
    QPushButton *m_stepBackwardButton;
    QPushButton *m_jumpToTimeButton;
    QSlider *m_playbackSpeedSlider;
    QProgressBar *m_playbackProgressBar;
    QLabel *m_playbackTimeLabel;
    
    // Widget creation buttons
    QPushButton *m_gridButton;
    QPushButton *m_gridLoggerButton;
    QPushButton *m_lineChartButton;
    QPushButton *m_pieChartButton;
    QPushButton *m_barChartButton;
    QPushButton *m_3dChartButton;
    
    // Performance dashboard
    QPushButton *m_performanceDashboardButton;
    
    // Central widget and managers
    TabManager *m_tabManager;
    SettingsManager *m_settingsManager;
    PerformanceDashboard *m_performanceDashboard;
    
    // Test Framework integration
    Monitor::UI::TestFramework::TestManagerWindow *m_testManagerWindow;
    std::shared_ptr<Monitor::TestFramework::TestRunner> m_testRunner;
    
    // Simulation Mode integration
    std::shared_ptr<Monitor::Packet::SimulationSource> m_simulationSource;
    
    // Status bar components
    QLabel *m_connectionStatusLabel;
    QLabel *m_packetRateLabel;
    QLabel *m_performanceLabel;
    QProgressBar *m_memoryUsageBar;
    
    // State tracking
    enum class Mode { Ethernet, Offline };
    enum class PlaybackState { Stopped, Playing, Paused };
    
    Mode m_currentMode;
    PlaybackState m_playbackState;
    bool m_simulationRunning;
    bool m_testFrameworkEnabled;
    
    // Performance metrics
    int m_currentPacketRate;
    double m_cpuUsage;
    double m_memoryUsage;
};

#endif // MAINWINDOW_H
