#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "src/ui/managers/tab_manager.h"
#include "src/ui/managers/settings_manager.h"
#include "src/ui/windows/struct_window.h"
#include "src/ui/windows/performance_dashboard.h"
#include "src/ui/test_framework/test_manager_window.h"
#include "src/test_framework/execution/test_runner.h"
#include "src/packet/sources/simulation_source.h"

#include <QApplication>
#include <QMessageBox>
#include <QCloseEvent>
#include <QShowEvent>
#include <QTimer>
#include <QStyle>
#include <QStyleFactory>
#include <QSplitter>
#include <QFrame>
#include <QSpacerItem>
#include <QAction>
#include <QtCore/QLoggingCategory>

Q_LOGGING_CATEGORY(mainWindow, "Monitor.MainWindow")

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_toolbar(nullptr)
    , m_tabManager(nullptr)
    , m_settingsManager(nullptr)
    , m_performanceDashboard(nullptr)
    , m_testManagerWindow(nullptr)
    , m_testRunner(nullptr)
    , m_simulationSource(nullptr)
    , m_currentMode(Mode::Ethernet)
    , m_playbackState(PlaybackState::Stopped)
    , m_simulationRunning(false)
    , m_testFrameworkEnabled(false)
    , m_currentPacketRate(0)
    , m_cpuUsage(0.0)
    , m_memoryUsage(0.0)
{
    ui->setupUi(this);
    
    // Initialize managers first
    m_settingsManager = new SettingsManager(this);
    m_tabManager = new TabManager(this);
    m_performanceDashboard = new PerformanceDashboard(this);
    
    // Initialize Test Framework components
    m_testRunner = std::make_shared<Monitor::TestFramework::TestRunner>();
    m_testManagerWindow = new Monitor::UI::TestFramework::TestManagerWindow(this);
    m_testManagerWindow->setTestRunner(m_testRunner);
    
    // Setup the complete UI
    setupUI();
    setupConnections();
    
    // Load settings and restore state
    loadSettings();
    
    // Set initial window properties
    setWindowTitle("Monitor Application v0.1.0");
    setMinimumSize(800, 600);
    
    qCInfo(mainWindow) << "MainWindow initialized successfully";
}

MainWindow::~MainWindow()
{
    // Save settings before destruction
    saveSettings();
    
    delete ui;
    
    qCInfo(mainWindow) << "MainWindow destroyed";
}

void MainWindow::setupUI()
{
    // Create the main components
    createToolbar();
    createCentralWidget();
    createStatusBar();
    createMenuBar();
    
    // Apply initial styling
    updateToolbarState();
    updateStatusBar();
    
    qCDebug(mainWindow) << "UI setup completed";
}

void MainWindow::createToolbar()
{
    m_toolbar = addToolBar("Main Toolbar");
    m_toolbar->setObjectName("MainToolbar");
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    
    // Structure management section
    m_addStructButton = new QPushButton("Add Struct", this);
    m_addStructButton->setToolTip("Open the Add Structure Window");
    m_addStructButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    m_toolbar->addWidget(m_addStructButton);
    
    m_toolbar->addSeparator();
    
    // Test framework section
    m_testFrameworkButton = new QPushButton("Test Framework", this);
    m_testFrameworkButton->setToolTip("Open Real-Time Test Manager");
    m_testFrameworkButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    m_toolbar->addWidget(m_testFrameworkButton);
    
    m_testStatusIndicator = new QLabel("●", this);
    m_testStatusIndicator->setToolTip("Test Status: Green=Passing, Red=Failing, Yellow=Paused");
    m_testStatusIndicator->setStyleSheet("QLabel { color: green; font-size: 14px; font-weight: bold; }");
    m_toolbar->addWidget(m_testStatusIndicator);
    
    m_toolbar->addSeparator();
    
    // Simulation controls section
    m_startSimulationButton = new QPushButton("Start Simulation", this);
    m_startSimulationButton->setToolTip("Start simulation mode for development");
    m_startSimulationButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_startSimulationButton->setCheckable(true);
    m_toolbar->addWidget(m_startSimulationButton);
    
    m_stopSimulationButton = new QPushButton("Stop Simulation", this);
    m_stopSimulationButton->setToolTip("Stop simulation mode");
    m_stopSimulationButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_stopSimulationButton->setCheckable(true);
    m_stopSimulationButton->setEnabled(false);
    m_toolbar->addWidget(m_stopSimulationButton);
    
    m_toolbar->addSeparator();
    
    // Mode selection section
    m_ethernetModeButton = new QPushButton("Ethernet", this);
    m_ethernetModeButton->setToolTip("Switch to Ethernet mode");
    m_ethernetModeButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    m_ethernetModeButton->setCheckable(true);
    m_ethernetModeButton->setChecked(true);
    m_toolbar->addWidget(m_ethernetModeButton);
    
    m_offlineModeButton = new QPushButton("Offline", this);
    m_offlineModeButton->setToolTip("Switch to offline playback mode");
    m_offlineModeButton->setIcon(style()->standardIcon(QStyle::SP_DriveHDIcon));
    m_offlineModeButton->setCheckable(true);
    m_toolbar->addWidget(m_offlineModeButton);
    
    m_portSettingsButton = new QPushButton("Port Settings", this);
    m_portSettingsButton->setToolTip("Configure Ethernet port settings");
    m_portSettingsButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_toolbar->addWidget(m_portSettingsButton);
    
    m_toolbar->addSeparator();
    
    // Create offline playback controls (initially hidden)
    createOfflinePlaybackControls();
    
    m_toolbar->addSeparator();
    
    // Widget creation section
    m_gridButton = new QPushButton("Grid", this);
    m_gridButton->setToolTip("Create Grid Widget");
    m_gridButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogListView));
    m_toolbar->addWidget(m_gridButton);
    
    m_gridLoggerButton = new QPushButton("GridLogger", this);
    m_gridLoggerButton->setToolTip("Create GridLogger Widget");
    m_gridLoggerButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    m_toolbar->addWidget(m_gridLoggerButton);
    
    m_lineChartButton = new QPushButton("Line Chart", this);
    m_lineChartButton->setToolTip("Create Line Chart Widget");
    m_lineChartButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    m_toolbar->addWidget(m_lineChartButton);
    
    m_pieChartButton = new QPushButton("Pie Chart", this);
    m_pieChartButton->setToolTip("Create Pie Chart Widget");
    m_pieChartButton->setIcon(style()->standardIcon(QStyle::SP_DialogHelpButton));
    m_toolbar->addWidget(m_pieChartButton);
    
    m_barChartButton = new QPushButton("Bar Chart", this);
    m_barChartButton->setToolTip("Create Bar Chart Widget");
    m_barChartButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    m_toolbar->addWidget(m_barChartButton);
    
    m_3dChartButton = new QPushButton("3D Chart", this);
    m_3dChartButton->setToolTip("Create 3D Chart Widget");
    m_3dChartButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    m_toolbar->addWidget(m_3dChartButton);
    
    m_toolbar->addSeparator();
    
    // Performance dashboard
    m_performanceDashboardButton = new QPushButton("Performance", this);
    m_performanceDashboardButton->setToolTip("Open Performance Dashboard");
    m_performanceDashboardButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    m_toolbar->addWidget(m_performanceDashboardButton);
    
    // Add stretch to push everything to the left
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);
}

void MainWindow::createOfflinePlaybackControls()
{
    // Container for playback controls
    m_playbackControls = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(m_playbackControls);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    
    m_playPauseButton = new QPushButton(this);
    m_playPauseButton->setToolTip("Play/Pause playback");
    m_playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_playPauseButton->setMaximumSize(24, 24);
    layout->addWidget(m_playPauseButton);
    
    m_stopPlaybackButton = new QPushButton(this);
    m_stopPlaybackButton->setToolTip("Stop playback and return to beginning");
    m_stopPlaybackButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_stopPlaybackButton->setMaximumSize(24, 24);
    layout->addWidget(m_stopPlaybackButton);
    
    m_stepBackwardButton = new QPushButton(this);
    m_stepBackwardButton->setToolTip("Step backward one packet");
    m_stepBackwardButton->setIcon(style()->standardIcon(QStyle::SP_MediaSeekBackward));
    m_stepBackwardButton->setMaximumSize(24, 24);
    layout->addWidget(m_stepBackwardButton);
    
    m_stepForwardButton = new QPushButton(this);
    m_stepForwardButton->setToolTip("Step forward one packet");
    m_stepForwardButton->setIcon(style()->standardIcon(QStyle::SP_MediaSeekForward));
    m_stepForwardButton->setMaximumSize(24, 24);
    layout->addWidget(m_stepForwardButton);
    
    m_jumpToTimeButton = new QPushButton("Jump", this);
    m_jumpToTimeButton->setToolTip("Jump to specific time");
    m_jumpToTimeButton->setMaximumSize(40, 24);
    layout->addWidget(m_jumpToTimeButton);
    
    // Playback speed slider
    m_playbackSpeedSlider = new QSlider(Qt::Horizontal, this);
    m_playbackSpeedSlider->setToolTip("Playback speed (0.1x to 10x)");
    m_playbackSpeedSlider->setRange(1, 100); // 0.1x to 10x
    m_playbackSpeedSlider->setValue(10); // 1x speed
    m_playbackSpeedSlider->setMaximumWidth(100);
    layout->addWidget(m_playbackSpeedSlider);
    
    // Progress bar
    m_playbackProgressBar = new QProgressBar(this);
    m_playbackProgressBar->setToolTip("Playback progress");
    m_playbackProgressBar->setMaximumHeight(16);
    m_playbackProgressBar->setMaximumWidth(150);
    layout->addWidget(m_playbackProgressBar);
    
    // Time label
    m_playbackTimeLabel = new QLabel("00:00:00", this);
    m_playbackTimeLabel->setToolTip("Current playback time");
    m_playbackTimeLabel->setMinimumWidth(60);
    layout->addWidget(m_playbackTimeLabel);
    
    m_toolbar->addWidget(m_playbackControls);
    
    // Initially hide playback controls (shown only in offline mode)
    m_playbackControls->setVisible(false);
}

void MainWindow::createCentralWidget()
{
    // Replace the existing central widget with our tab widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);
    
    // Add the tab manager's widget to the central area
    layout->addWidget(m_tabManager->getTabWidget());
    
    // Create the first default tab
    m_tabManager->createTab("Main");
}

void MainWindow::createStatusBar()
{
    QStatusBar *statusBar = this->statusBar();
    
    // Connection status
    m_connectionStatusLabel = new QLabel("Disconnected", this);
    m_connectionStatusLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_connectionStatusLabel->setMinimumWidth(100);
    statusBar->addPermanentWidget(m_connectionStatusLabel);
    
    // Packet rate
    m_packetRateLabel = new QLabel("0 pps", this);
    m_packetRateLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_packetRateLabel->setMinimumWidth(80);
    m_packetRateLabel->setToolTip("Packets per second");
    statusBar->addPermanentWidget(m_packetRateLabel);
    
    // Performance metrics
    m_performanceLabel = new QLabel("CPU: 0% | Memory: 0%", this);
    m_performanceLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_performanceLabel->setMinimumWidth(150);
    statusBar->addPermanentWidget(m_performanceLabel);
    
    // Memory usage bar
    m_memoryUsageBar = new QProgressBar(this);
    m_memoryUsageBar->setMaximumHeight(16);
    m_memoryUsageBar->setMaximumWidth(100);
    m_memoryUsageBar->setRange(0, 100);
    m_memoryUsageBar->setValue(0);
    m_memoryUsageBar->setToolTip("Memory usage percentage");
    statusBar->addPermanentWidget(m_memoryUsageBar);
    
    // Default status message
    statusBar->showMessage("Monitor Application Ready", 2000);
}

void MainWindow::createMenuBar()
{
    // File menu
    QMenu *fileMenu = menuBar()->addMenu("&File");
    auto newAction = fileMenu->addAction("&New Workspace");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::onAddStructClicked);
    
    auto openAction = fileMenu->addAction("&Open Workspace");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onAddStructClicked);
    
    auto saveAction = fileMenu->addAction("&Save Workspace");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::onAddStructClicked);
    
    fileMenu->addSeparator();
    
    auto exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    // Edit menu
    QMenu *editMenu = menuBar()->addMenu("&Edit");
    auto prefsAction = editMenu->addAction("&Preferences");
    prefsAction->setShortcut(QKeySequence::Preferences);
    connect(prefsAction, &QAction::triggered, this, &MainWindow::onAddStructClicked);
    
    // View menu
    QMenu *viewMenu = menuBar()->addMenu("&View");
    auto toolbarAction = viewMenu->addAction("&Toolbar");
    toolbarAction->setCheckable(true);
    connect(toolbarAction, &QAction::toggled, m_toolbar, &QToolBar::setVisible);
    
    auto statusbarAction = viewMenu->addAction("&Status Bar");
    statusbarAction->setCheckable(true);
    connect(statusbarAction, &QAction::toggled, statusBar(), &QStatusBar::setVisible);
    
    viewMenu->addSeparator();
    connect(viewMenu->addAction("&Performance Dashboard"), &QAction::triggered, 
            this, &MainWindow::onPerformanceDashboardClicked);
    
    // Tools menu
    QMenu *toolsMenu = menuBar()->addMenu("&Tools");
    connect(toolsMenu->addAction("&Test Framework"), &QAction::triggered, 
            this, &MainWindow::onTestFrameworkClicked);
    connect(toolsMenu->addAction("&Structure Manager"), &QAction::triggered, 
            this, &MainWindow::onAddStructClicked);
    toolsMenu->addSeparator();
    connect(toolsMenu->addAction("Start &Simulation"), &QAction::triggered, 
            this, &MainWindow::onStartSimulationClicked);
    connect(toolsMenu->addAction("Stop S&imulation"), &QAction::triggered, 
            this, &MainWindow::onStopSimulationClicked);
    
    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    connect(helpMenu->addAction("&About"), &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About Monitor Application", 
                          "Monitor Application v0.1.0\n\n"
                          "Real-time data visualization tool for packet monitoring.\n\n"
                          "Built with Qt6 and C++17");
    });
    connect(helpMenu->addAction("About &Qt"), &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::setupConnections()
{
    // Structure management
    connect(m_addStructButton, &QPushButton::clicked, this, &MainWindow::onAddStructClicked);
    
    // Test framework
    connect(m_testFrameworkButton, &QPushButton::clicked, this, &MainWindow::onTestFrameworkClicked);
    
    // Connect test manager window signals
    if (m_testManagerWindow) {
        connect(m_testManagerWindow, &Monitor::UI::TestFramework::TestManagerWindow::testCreated,
                this, [this](auto) { 
                    // Simulate test results update when tests are created
                    onTestResultsChanged(true, 1, 0);
                });
    }
    
    // Simulation controls
    connect(m_startSimulationButton, &QPushButton::clicked, this, &MainWindow::onStartSimulationClicked);
    connect(m_stopSimulationButton, &QPushButton::clicked, this, &MainWindow::onStopSimulationClicked);
    
    // Mode selection
    connect(m_ethernetModeButton, &QPushButton::clicked, this, &MainWindow::onEthernetModeSelected);
    connect(m_offlineModeButton, &QPushButton::clicked, this, &MainWindow::onOfflineModeSelected);
    connect(m_portSettingsButton, &QPushButton::clicked, this, &MainWindow::onPortSettingsClicked);
    
    // Offline playback controls
    connect(m_playPauseButton, &QPushButton::clicked, this, &MainWindow::onPlayPauseClicked);
    connect(m_stopPlaybackButton, &QPushButton::clicked, this, &MainWindow::onStopPlaybackClicked);
    connect(m_stepForwardButton, &QPushButton::clicked, this, &MainWindow::onStepForwardClicked);
    connect(m_stepBackwardButton, &QPushButton::clicked, this, &MainWindow::onStepBackwardClicked);
    connect(m_jumpToTimeButton, &QPushButton::clicked, this, &MainWindow::onJumpToTimeClicked);
    connect(m_playbackSpeedSlider, &QSlider::valueChanged, this, &MainWindow::onPlaybackSpeedChanged);
    
    // Widget creation
    connect(m_gridButton, &QPushButton::clicked, this, &MainWindow::onCreateGridWidget);
    connect(m_gridLoggerButton, &QPushButton::clicked, this, &MainWindow::onCreateGridLoggerWidget);
    connect(m_lineChartButton, &QPushButton::clicked, this, &MainWindow::onCreateLineChartWidget);
    connect(m_pieChartButton, &QPushButton::clicked, this, &MainWindow::onCreatePieChartWidget);
    connect(m_barChartButton, &QPushButton::clicked, this, &MainWindow::onCreateBarChartWidget);
    connect(m_3dChartButton, &QPushButton::clicked, this, &MainWindow::onCreate3DChartWidget);
    
    // Performance dashboard
    connect(m_performanceDashboardButton, &QPushButton::clicked, this, &MainWindow::onPerformanceDashboardClicked);
    
    // Tab manager events
    connect(m_tabManager, &TabManager::tabCountChanged, this, &MainWindow::onTabCountChanged);
    connect(m_tabManager, QOverload<const QString&, int>::of(&TabManager::activeTabChanged), 
            this, [this](const QString& tabId, int index) { 
                Q_UNUSED(tabId)
                onActiveTabChanged(index); 
            });
    
    // Settings manager
    connect(m_settingsManager, &SettingsManager::settingsChanged, this, [this](const QString &key, const QVariant &) {
        if (key.startsWith("mainWindow/")) {
            updateToolbarState();
            updateStatusBar();
        }
    });
}

void MainWindow::loadSettings()
{
    if (m_settingsManager) {
        m_settingsManager->restoreMainWindowState(this);
        m_settingsManager->restoreTabManagerState(m_tabManager);
    }
}

void MainWindow::saveSettings()
{
    if (m_settingsManager) {
        m_settingsManager->saveMainWindowState(this);
        m_settingsManager->saveTabManagerState(m_tabManager);
        m_settingsManager->saveSettings();
    }
}

void MainWindow::updateToolbarState()
{
    // Update mode buttons based on current mode
    m_ethernetModeButton->setChecked(m_currentMode == Mode::Ethernet);
    m_offlineModeButton->setChecked(m_currentMode == Mode::Offline);
    
    // Show/hide playback controls based on mode
    m_playbackControls->setVisible(m_currentMode == Mode::Offline);
    m_portSettingsButton->setVisible(m_currentMode == Mode::Ethernet);
    
    // Update simulation buttons
    m_startSimulationButton->setChecked(m_simulationRunning);
    m_stopSimulationButton->setChecked(!m_simulationRunning);
    m_startSimulationButton->setEnabled(!m_simulationRunning);
    m_stopSimulationButton->setEnabled(m_simulationRunning);
    
    // Update test framework indicator
    if (m_testFrameworkEnabled) {
        m_testStatusIndicator->setStyleSheet("QLabel { color: green; font-size: 14px; font-weight: bold; }");
        m_testStatusIndicator->setToolTip("Test Framework: Running - All tests passing");
    } else {
        m_testStatusIndicator->setStyleSheet("QLabel { color: gray; font-size: 14px; font-weight: bold; }");
        m_testStatusIndicator->setToolTip("Test Framework: Disabled");
    }
}

void MainWindow::updateStatusBar()
{
    // Update connection status
    if (m_currentMode == Mode::Ethernet) {
        m_connectionStatusLabel->setText("Ethernet");
        m_connectionStatusLabel->setStyleSheet("QLabel { color: green; }");
    } else {
        m_connectionStatusLabel->setText("Offline");
        m_connectionStatusLabel->setStyleSheet("QLabel { color: blue; }");
    }
    
    // Update packet rate
    m_packetRateLabel->setText(QString("%1 pps").arg(m_currentPacketRate));
    
    // Update performance metrics
    m_performanceLabel->setText(QString("CPU: %1% | Memory: %2%")
                               .arg(m_cpuUsage, 0, 'f', 1)
                               .arg(m_memoryUsage, 0, 'f', 1));
    
    // Update memory usage bar
    m_memoryUsageBar->setValue(static_cast<int>(m_memoryUsage));
}

// Event handlers
void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    
    // Perform any initialization that requires the window to be shown
    QTimer::singleShot(100, [this]() {
        updateToolbarState();
        updateStatusBar();
    });
}

// Slot implementations (stubs for now - will be implemented in future phases)
void MainWindow::onConnectionStatusChanged(bool connected)
{
    Q_UNUSED(connected)
    updateStatusBar();
}

void MainWindow::onPerformanceMetricsUpdated()
{
    updateStatusBar();
}

void MainWindow::onPacketRateUpdated(int packetsPerSecond)
{
    m_currentPacketRate = packetsPerSecond;
    updateStatusBar();
}

void MainWindow::onAddStructClicked()
{
    qCDebug(mainWindow) << "Add Struct button clicked";
    // TODO: Implement in future phases
}

void MainWindow::onTestFrameworkClicked()
{
    qCDebug(mainWindow) << "Test Framework button clicked";
    
    if (!m_testManagerWindow) {
        qCWarning(mainWindow) << "Test Manager Window not initialized";
        return;
    }
    
    // Toggle Test Framework window visibility
    if (m_testManagerWindow->isWindowVisible()) {
        m_testManagerWindow->hideWindow();
        m_testFrameworkEnabled = false;
        m_testStatusIndicator->setText("Inactive");
        m_testStatusIndicator->setStyleSheet("color: gray;");
    } else {
        m_testManagerWindow->showWindow();
        m_testFrameworkEnabled = true;
        m_testStatusIndicator->setText("Active");
        m_testStatusIndicator->setStyleSheet("color: green;");
    }
    
    updateToolbarState();
}

void MainWindow::onTestResultsChanged(bool allPassing, int totalTests, int failedTests)
{
    Q_UNUSED(totalTests) // Avoid unused parameter warning
    
    if (!m_testFrameworkEnabled) {
        return;
    }
    
    // Update test status indicator based on results
    if (allPassing) {
        m_testStatusIndicator->setText("●");
        m_testStatusIndicator->setStyleSheet("QLabel { color: green; font-size: 14px; font-weight: bold; }");
        m_testStatusIndicator->setToolTip(QString("Test Framework: Running - All %1 tests passing").arg(totalTests));
    } else {
        m_testStatusIndicator->setText("●");
        m_testStatusIndicator->setStyleSheet("QLabel { color: red; font-size: 14px; font-weight: bold; }");
        m_testStatusIndicator->setToolTip(QString("Test Framework: Running - %1 of %2 tests failing").arg(failedTests).arg(totalTests));
    }
}

void MainWindow::onStartSimulationClicked()
{
    qCDebug(mainWindow) << "Start Simulation clicked";
    
    if (!m_simulationSource) {
        // Create simulation source with default configuration
        auto config = Monitor::Packet::SimulationSource::createDefaultConfig();
        m_simulationSource = std::make_shared<Monitor::Packet::SimulationSource>(config, this);
        
        // Connect simulation source signals for UI updates
        connect(m_simulationSource.get(), &Monitor::Packet::PacketSource::statisticsUpdated,
                this, [this]() {
                    auto stats = m_simulationSource->getStatistics();
                    onPacketRateUpdated(static_cast<int>(stats.packetsDelivered));
                });
    }
    
    // Start simulation
    if (m_simulationSource->start()) {
        m_simulationRunning = true;
        qCDebug(mainWindow) << "Simulation started successfully";
    } else {
        qCWarning(mainWindow) << "Failed to start simulation";
        m_simulationRunning = false;
    }
    
    updateToolbarState();
}

void MainWindow::onStopSimulationClicked()
{
    qCDebug(mainWindow) << "Stop Simulation clicked";
    
    if (m_simulationSource && m_simulationSource->isRunning()) {
        m_simulationSource->stop();
        qCDebug(mainWindow) << "Simulation stopped successfully";
    }
    
    m_simulationRunning = false;
    updateToolbarState();
}

void MainWindow::onEthernetModeSelected()
{
    qCDebug(mainWindow) << "Ethernet mode selected";
    m_currentMode = Mode::Ethernet;
    updateToolbarState();
}

void MainWindow::onOfflineModeSelected()
{
    qCDebug(mainWindow) << "Offline mode selected";
    m_currentMode = Mode::Offline;
    updateToolbarState();
}

void MainWindow::onPortSettingsClicked()
{
    qCDebug(mainWindow) << "Port Settings clicked";
    // TODO: Implement in Phase 9
}

void MainWindow::onPlayPauseClicked()
{
    qCDebug(mainWindow) << "Play/Pause clicked";
    if (m_playbackState == PlaybackState::Playing) {
        m_playbackState = PlaybackState::Paused;
        m_playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    } else {
        m_playbackState = PlaybackState::Playing;
        m_playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
}

void MainWindow::onStopPlaybackClicked()
{
    qCDebug(mainWindow) << "Stop Playback clicked";
    m_playbackState = PlaybackState::Stopped;
    m_playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
}

void MainWindow::onStepForwardClicked()
{
    qCDebug(mainWindow) << "Step Forward clicked";
    // TODO: Implement in Phase 9
}

void MainWindow::onStepBackwardClicked()
{
    qCDebug(mainWindow) << "Step Backward clicked";
    // TODO: Implement in Phase 9
}

void MainWindow::onJumpToTimeClicked()
{
    qCDebug(mainWindow) << "Jump to Time clicked";
    // TODO: Implement in Phase 9
}

void MainWindow::onPlaybackSpeedChanged(int speed)
{
    qCDebug(mainWindow) << "Playback speed changed to:" << speed;
    // TODO: Implement in Phase 9
}

void MainWindow::onCreateGridWidget()
{
    qCDebug(mainWindow) << "Create Grid Widget clicked";
    // TODO: Implement in Phase 6
}

void MainWindow::onCreateGridLoggerWidget()
{
    qCDebug(mainWindow) << "Create GridLogger Widget clicked";
    // TODO: Implement in Phase 6
}

void MainWindow::onCreateLineChartWidget()
{
    qCDebug(mainWindow) << "Create Line Chart Widget clicked";
    // TODO: Implement in Phase 7
}

void MainWindow::onCreatePieChartWidget()
{
    qCDebug(mainWindow) << "Create Pie Chart Widget clicked";
    // TODO: Implement in Phase 7
}

void MainWindow::onCreateBarChartWidget()
{
    qCDebug(mainWindow) << "Create Bar Chart Widget clicked";
    // TODO: Implement in Phase 7
}

void MainWindow::onCreate3DChartWidget()
{
    qCDebug(mainWindow) << "Create 3D Chart Widget clicked";
    // TODO: Implement in Phase 8
}

void MainWindow::onPerformanceDashboardClicked()
{
    qCDebug(mainWindow) << "Performance Dashboard clicked";
    
    if (m_performanceDashboard) {
        if (m_performanceDashboard->isVisible()) {
            m_performanceDashboard->raise();
            m_performanceDashboard->activateWindow();
        } else {
            m_performanceDashboard->show();
        }
    } else {
        qCWarning(mainWindow) << "Performance Dashboard not initialized";
    }
}

void MainWindow::onTabCountChanged(int count)
{
    qCDebug(mainWindow) << "Tab count changed to:" << count;
    // Update UI based on tab count
}

void MainWindow::onActiveTabChanged(int index)
{
    qCDebug(mainWindow) << "Active tab changed to index:" << index;
    // Update UI based on active tab
}
