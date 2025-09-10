#include "chart_3d_widget.h"
#include "../../../logging/logger.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <chrono>
#include <cmath>

Q_LOGGING_CATEGORY(chart3DWidget, "Monitor.UI.Chart3DWidget")

Chart3DWidget::Chart3DWidget(const QString& widgetId, const QString& windowTitle, QWidget* parent)
    : DisplayWidget(widgetId, windowTitle, parent)
    , m_3dWindow(nullptr)
    , m_rootEntity(nullptr)
    , m_sceneEntity(nullptr)
    , m_camera(nullptr)
    , m_fpsCameraController(nullptr)
    , m_orbitCameraController(nullptr)
    , m_lightEntity(nullptr)
    , m_directionalLight(nullptr)
    , m_pointLight(nullptr)
    , m_gridEntity(nullptr)
    , m_mainLayout(nullptr)
    , m_controlWidget(nullptr)
    , m_toolbar3D(nullptr)
    , m_animationTimer(new QTimer(this))
    , m_updateTimer(new QTimer(this))
    , m_isInitialized(false)
    , m_frameCount(0)
    , m_currentFPS(0.0)
    , m_currentPointCount(0)
    , m_resetCameraAction(nullptr)
    , m_toggleAxesAction(nullptr)
    , m_toggleGridAction(nullptr)
    , m_toggleLightingAction(nullptr)
    , m_export3DAction(nullptr)
    , m_settings3DAction(nullptr)
    , m_renderModeCombo(nullptr)
    , m_cameraModeCombo(nullptr)
    , m_rotationSpeedSlider(nullptr)
    , m_autoRotateCheckBox(nullptr)
{
    qCDebug(chart3DWidget) << "Creating Chart3DWidget with ID:" << widgetId;
    
    // Initialize axes entities
    for (int i = 0; i < 3; ++i) {
        m_axisEntities[i] = nullptr;
        m_labelEntities[i] = nullptr;
    }
    
    // Initialize axis configurations
    m_axisConfigs[0] = AxisConfig(); // X-axis
    m_axisConfigs[0].label = "X-Axis";
    m_axisConfigs[0].color = QColor(255, 0, 0);
    
    m_axisConfigs[1] = AxisConfig(); // Y-axis
    m_axisConfigs[1].label = "Y-Axis";  
    m_axisConfigs[1].color = QColor(0, 255, 0);
    
    m_axisConfigs[2] = AxisConfig(); // Z-axis
    m_axisConfigs[2].label = "Z-Axis";
    m_axisConfigs[2].color = QColor(0, 0, 255);
    
    // Setup timers
    connect(m_animationTimer, &QTimer::timeout, this, &Chart3DWidget::onAnimationTimerTimeout);
    connect(m_updateTimer, &QTimer::timeout, this, &Chart3DWidget::onUpdate3DTimer);
    
    m_animationTimer->setSingleShot(false);
    m_updateTimer->setSingleShot(false);
    
    // Initialize the widget
    initializeWidget();
    
    qCDebug(chart3DWidget) << "Chart3DWidget created successfully";
}

Chart3DWidget::~Chart3DWidget()
{
    qCDebug(chart3DWidget) << "Destroying Chart3DWidget";
    
    // Stop timers
    if (m_animationTimer) {
        m_animationTimer->stop();
    }
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    
    // Clean up 3D resources
    if (m_3dWindow) {
        m_3dWindow->setParent(nullptr);
        delete m_3dWindow;
        m_3dWindow = nullptr;
    }
    
    qCDebug(chart3DWidget) << "Chart3DWidget destroyed";
}

void Chart3DWidget::initializeWidget()
{
    qCDebug(chart3DWidget) << "Initializing Chart3DWidget";
    
    try {
        // Call parent initialization
        DisplayWidget::initializeWidget();
        
        // Setup main layout
        m_mainLayout = new QVBoxLayout(this);
        m_mainLayout->setContentsMargins(2, 2, 2, 2);
        m_mainLayout->setSpacing(1);
        
        // Setup toolbar
        setupToolbar3D();
        
        // Setup 3D window
        setup3DWindow();
        
        // Setup context menu
        setupContextMenu();
        
        // Initialize performance tracking
        initializePerformanceTracking3D();
        
        // Start update timer (30 FPS for 3D)
        m_updateTimer->start(33); // ~30 FPS
        
        m_isInitialized = true;
        qCDebug(chart3DWidget) << "Chart3DWidget initialized successfully";
        
    } catch (const std::exception& e) {
        qCCritical(chart3DWidget) << "Failed to initialize Chart3DWidget:" << e.what();
        throw;
    }
}

void Chart3DWidget::setup3DWindow()
{
    qCDebug(chart3DWidget) << "Setting up 3D window";
    
    // Create Qt3D window
    m_3dWindow = new Qt3DExtras::Qt3DWindow();
    m_3dWindow->defaultFrameGraph()->setClearColor(QColor(QRgb(0x4d4d4f)));
    
    // Create container widget for the 3D window
    QWidget* container = QWidget::createWindowContainer(m_3dWindow, this);
    container->setMinimumSize(400, 300);
    container->setFocusPolicy(Qt::StrongFocus);
    
    // Create splitter to allow resizing
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    
    // Add 3D container to splitter
    splitter->addWidget(container);
    splitter->setSizes({800, 200});
    
    // Create control widget
    m_controlWidget = new QWidget(this);
    m_controlWidget->setMaximumWidth(200);
    m_controlWidget->setMinimumWidth(150);
    setupControlWidget();
    splitter->addWidget(m_controlWidget);
    
    // Add splitter to main layout
    m_mainLayout->addWidget(splitter);
    
    // Setup 3D scene
    setupScene();
    setupCamera();
    setupLighting();
    setupAxes();
    
    qCDebug(chart3DWidget) << "3D window setup complete";
}

void Chart3DWidget::setupScene()
{
    qCDebug(chart3DWidget) << "Setting up 3D scene";
    
    // Create root entity
    m_rootEntity = new Qt3DCore::QEntity();
    
    // Create scene entity for data
    m_sceneEntity = new Qt3DCore::QEntity(m_rootEntity);
    
    // Set root entity to window
    m_3dWindow->setRootEntity(m_rootEntity);
    
    qCDebug(chart3DWidget) << "3D scene setup complete";
}

void Chart3DWidget::setupCamera()
{
    qCDebug(chart3DWidget) << "Setting up 3D camera";
    
    // Get camera from window
    m_camera = m_3dWindow->camera();
    
    // Set initial camera position
    m_camera->lens()->setPerspectiveProjection(
        m_chart3DConfig.fieldOfView,
        16.0f/9.0f,
        m_chart3DConfig.nearPlane,
        m_chart3DConfig.farPlane
    );
    
    m_camera->setPosition(m_chart3DConfig.cameraPosition);
    m_camera->setViewCenter(m_chart3DConfig.cameraTarget);
    m_camera->setUpVector(m_chart3DConfig.cameraUp);
    
    // Setup camera controllers
    setupCameraController();
    
    qCDebug(chart3DWidget) << "3D camera setup complete";
}

void Chart3DWidget::setupCameraController()
{
    qCDebug(chart3DWidget) << "Setting up camera controller";
    
    // Remove existing controllers
    if (m_orbitCameraController) {
        delete m_orbitCameraController;
        m_orbitCameraController = nullptr;
    }
    if (m_fpsCameraController) {
        delete m_fpsCameraController;
        m_fpsCameraController = nullptr;
    }
    
    // Create appropriate controller based on mode
    switch (m_chart3DConfig.cameraMode) {
        case CameraMode::Orbit:
            m_orbitCameraController = new Qt3DExtras::QOrbitCameraController(m_rootEntity);
            m_orbitCameraController->setCamera(m_camera);
            m_orbitCameraController->setLinearSpeed(50.0f);
            m_orbitCameraController->setLookSpeed(180.0f);
            break;
            
        case CameraMode::FirstPerson:
            m_fpsCameraController = new Qt3DExtras::QFirstPersonCameraController(m_rootEntity);
            m_fpsCameraController->setCamera(m_camera);
            m_fpsCameraController->setLinearSpeed(5.0f);
            m_fpsCameraController->setLookSpeed(180.0f);
            break;
            
        case CameraMode::Fixed:
        case CameraMode::Animated:
        case CameraMode::Custom:
        default:
            // No controller for fixed/animated modes
            break;
    }
    
    qCDebug(chart3DWidget) << "Camera controller setup complete";
}

void Chart3DWidget::setupLighting()
{
    qCDebug(chart3DWidget) << "Setting up 3D lighting";
    
    // Remove existing lighting
    if (m_lightEntity) {
        delete m_lightEntity;
        m_lightEntity = nullptr;
    }
    
    // Create light entity
    m_lightEntity = new Qt3DCore::QEntity(m_rootEntity);
    
    // Setup lighting based on mode
    switch (m_chart3DConfig.lightingMode) {
        case LightingMode::Directional:
            m_directionalLight = new Qt3DRender::QDirectionalLight(m_lightEntity);
            m_directionalLight->setColor(m_chart3DConfig.diffuseColor);
            m_directionalLight->setIntensity(m_chart3DConfig.lightIntensity);
            m_directionalLight->setWorldDirection(m_chart3DConfig.lightDirection);
            m_lightEntity->addComponent(m_directionalLight);
            break;
            
        case LightingMode::Point:
            m_pointLight = new Qt3DRender::QPointLight(m_lightEntity);
            m_pointLight->setColor(m_chart3DConfig.diffuseColor);
            m_pointLight->setIntensity(m_chart3DConfig.lightIntensity);
            
            // Add transform for point light
            {
                auto* lightTransform = new Qt3DCore::QTransform(m_lightEntity);
                lightTransform->setTranslation(m_chart3DConfig.lightPosition);
                m_lightEntity->addComponent(lightTransform);
                m_lightEntity->addComponent(m_pointLight);
            }
            break;
            
        case LightingMode::Ambient:
        case LightingMode::Multi:
        case LightingMode::Dynamic:
        default:
            // Default to directional light
            m_directionalLight = new Qt3DRender::QDirectionalLight(m_lightEntity);
            m_directionalLight->setColor(m_chart3DConfig.diffuseColor);
            m_directionalLight->setIntensity(m_chart3DConfig.lightIntensity);
            m_directionalLight->setWorldDirection(m_chart3DConfig.lightDirection);
            m_lightEntity->addComponent(m_directionalLight);
            break;
    }
    
    qCDebug(chart3DWidget) << "3D lighting setup complete";
}

void Chart3DWidget::setupAxes()
{
    if (!m_chart3DConfig.showAxes) {
        return;
    }
    
    qCDebug(chart3DWidget) << "Setting up 3D axes";
    
    // Clean up existing axes
    for (int i = 0; i < 3; ++i) {
        if (m_axisEntities[i]) {
            delete m_axisEntities[i];
            m_axisEntities[i] = nullptr;
        }
        if (m_labelEntities[i]) {
            delete m_labelEntities[i];
            m_labelEntities[i] = nullptr;
        }
    }
    
    // Create axis entities
    createAxisEntities();
    
    qCDebug(chart3DWidget) << "3D axes setup complete";
}

void Chart3DWidget::createAxisEntities()
{
    qCDebug(chart3DWidget) << "Creating axis entities";
    
    // Define axis directions and colors
    const QVector3D axisDirections[3] = {
        QVector3D(10, 0, 0),  // X-axis
        QVector3D(0, 10, 0),  // Y-axis
        QVector3D(0, 0, 10)   // Z-axis
    };
    
    for (int i = 0; i < 3; ++i) {
        // Create axis line
        m_axisEntities[i] = createLineEntity(
            QVector3D(0, 0, 0),
            axisDirections[i],
            m_axisConfigs[i].color
        );
        
        if (m_axisEntities[i]) {
            m_axisEntities[i]->setParent(m_rootEntity);
        }
    }
    
    qCDebug(chart3DWidget) << "Axis entities created";
}

void Chart3DWidget::setupToolbar3D()
{
    qCDebug(chart3DWidget) << "Setting up 3D toolbar";
    
    m_toolbar3D = new QToolBar("3D Chart Controls", this);
    m_toolbar3D->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    m_toolbar3D->setIconSize(QSize(16, 16));
    
    // Reset camera action
    m_resetCameraAction = m_toolbar3D->addAction("Reset Camera");
    m_resetCameraAction->setToolTip("Reset camera to default position");
    connect(m_resetCameraAction, &QAction::triggered, this, &Chart3DWidget::onResetCamera);
    
    // Toggle axes action
    m_toggleAxesAction = m_toolbar3D->addAction("Toggle Axes");
    m_toggleAxesAction->setCheckable(true);
    m_toggleAxesAction->setChecked(m_chart3DConfig.showAxes);
    m_toggleAxesAction->setToolTip("Show/hide coordinate axes");
    connect(m_toggleAxesAction, &QAction::triggered, this, &Chart3DWidget::onToggleAxes);
    
    // Toggle grid action
    m_toggleGridAction = m_toolbar3D->addAction("Toggle Grid");
    m_toggleGridAction->setCheckable(true);
    m_toggleGridAction->setChecked(m_chart3DConfig.showGrid);
    m_toggleGridAction->setToolTip("Show/hide grid");
    connect(m_toggleGridAction, &QAction::triggered, this, &Chart3DWidget::onToggleGrid);
    
    // Toggle lighting action
    m_toggleLightingAction = m_toolbar3D->addAction("Toggle Lighting");
    m_toggleLightingAction->setCheckable(true);
    m_toggleLightingAction->setChecked(true);
    m_toggleLightingAction->setToolTip("Enable/disable lighting effects");
    connect(m_toggleLightingAction, &QAction::triggered, this, &Chart3DWidget::onToggleLighting);
    
    m_toolbar3D->addSeparator();
    
    // Export action
    m_export3DAction = m_toolbar3D->addAction("Export");
    m_export3DAction->setToolTip("Export 3D chart");
    connect(m_export3DAction, &QAction::triggered, this, &Chart3DWidget::onExport3DChart);
    
    // Settings action
    m_settings3DAction = m_toolbar3D->addAction("Settings");
    m_settings3DAction->setToolTip("Chart settings");
    connect(m_settings3DAction, &QAction::triggered, this, &Chart3DWidget::onShowChart3DSettings);
    
    // Add toolbar to layout
    m_mainLayout->insertWidget(0, m_toolbar3D);
    
    qCDebug(chart3DWidget) << "3D toolbar setup complete";
}

void Chart3DWidget::setupControlWidget()
{
    if (!m_controlWidget) {
        return;
    }
    
    QVBoxLayout* controlLayout = new QVBoxLayout(m_controlWidget);
    
    // Render mode group
    QGroupBox* renderGroup = new QGroupBox("Rendering", m_controlWidget);
    QVBoxLayout* renderLayout = new QVBoxLayout(renderGroup);
    
    m_renderModeCombo = new QComboBox(renderGroup);
    populateRenderModeCombo();
    renderLayout->addWidget(new QLabel("Render Mode:"));
    renderLayout->addWidget(m_renderModeCombo);
    connect(m_renderModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Chart3DWidget::onChangeRenderMode);
    
    controlLayout->addWidget(renderGroup);
    
    // Camera group
    QGroupBox* cameraGroup = new QGroupBox("Camera", m_controlWidget);
    QVBoxLayout* cameraLayout = new QVBoxLayout(cameraGroup);
    
    m_cameraModeCombo = new QComboBox(cameraGroup);
    populateCameraModeCombo();
    cameraLayout->addWidget(new QLabel("Camera Mode:"));
    cameraLayout->addWidget(m_cameraModeCombo);
    
    // Auto-rotate checkbox
    m_autoRotateCheckBox = new QCheckBox("Auto Rotate", cameraGroup);
    m_autoRotateCheckBox->setChecked(m_chart3DConfig.autoRotate);
    connect(m_autoRotateCheckBox, &QCheckBox::toggled, this, &Chart3DWidget::onToggleAutoRotate);
    cameraLayout->addWidget(m_autoRotateCheckBox);
    
    // Rotation speed slider
    cameraLayout->addWidget(new QLabel("Rotation Speed:"));
    m_rotationSpeedSlider = new QSlider(Qt::Horizontal, cameraGroup);
    m_rotationSpeedSlider->setRange(1, 100);
    m_rotationSpeedSlider->setValue(static_cast<int>(m_chart3DConfig.rotationSpeed));
    cameraLayout->addWidget(m_rotationSpeedSlider);
    
    controlLayout->addWidget(cameraGroup);
    
    // Add stretch at bottom
    controlLayout->addStretch();
}

void Chart3DWidget::populateRenderModeCombo()
{
    if (!m_renderModeCombo) return;
    
    m_renderModeCombo->addItem("Points", static_cast<int>(RenderMode::Points));
    m_renderModeCombo->addItem("Lines", static_cast<int>(RenderMode::Lines));
    m_renderModeCombo->addItem("Surface", static_cast<int>(RenderMode::Surface));
    m_renderModeCombo->addItem("Point Cloud", static_cast<int>(RenderMode::PointCloud));
    m_renderModeCombo->addItem("Wireframe", static_cast<int>(RenderMode::Wireframe));
    m_renderModeCombo->addItem("Hybrid", static_cast<int>(RenderMode::Hybrid));
    
    m_renderModeCombo->setCurrentIndex(static_cast<int>(m_chart3DConfig.renderMode));
}

void Chart3DWidget::populateCameraModeCombo()
{
    if (!m_cameraModeCombo) return;
    
    m_cameraModeCombo->addItem("Orbit", static_cast<int>(CameraMode::Orbit));
    m_cameraModeCombo->addItem("First Person", static_cast<int>(CameraMode::FirstPerson));
    m_cameraModeCombo->addItem("Fixed", static_cast<int>(CameraMode::Fixed));
    m_cameraModeCombo->addItem("Animated", static_cast<int>(CameraMode::Animated));
    m_cameraModeCombo->addItem("Custom", static_cast<int>(CameraMode::Custom));
    
    m_cameraModeCombo->setCurrentIndex(static_cast<int>(m_chart3DConfig.cameraMode));
}

Qt3DCore::QEntity* Chart3DWidget::createSphereEntity(const QVector3D& position, float radius, const QColor& color)
{
    // Create entity
    Qt3DCore::QEntity* entity = new Qt3DCore::QEntity(m_sceneEntity);
    
    // Create sphere mesh
    Qt3DExtras::QSphereMesh* mesh = new Qt3DExtras::QSphereMesh();
    mesh->setRadius(radius);
    mesh->setSlices(16);
    mesh->setRings(16);
    
    // Create material
    Qt3DExtras::QPhongMaterial* material = new Qt3DExtras::QPhongMaterial();
    material->setDiffuse(color);
    material->setSpecular(QColor(255, 255, 255));
    material->setShininess(m_chart3DConfig.shininess);
    
    // Create transform
    Qt3DCore::QTransform* transform = new Qt3DCore::QTransform();
    transform->setTranslation(position);
    
    // Add components
    entity->addComponent(mesh);
    entity->addComponent(material);
    entity->addComponent(transform);
    
    return entity;
}

Qt3DCore::QEntity* Chart3DWidget::createLineEntity(const QVector3D& start, const QVector3D& end, const QColor& color)
{
    Q_UNUSED(end)  // TODO: Implement actual line geometry
    // For now, return a simple sphere at start position
    // TODO: Implement actual line geometry
    return createSphereEntity(start, 0.1f, color);
}

void Chart3DWidget::updateDisplay()
{
    if (!m_isInitialized) {
        return;
    }
    
    qCDebug(chart3DWidget) << "Updating 3D display";
    
    try {
        // Update data points
        updateDataPoints();
        
        // Update axis ranges if auto-scale is enabled
        updateAxisRanges();
        
        // Update performance counters
        updateFPSCounter3D();
        
    } catch (const std::exception& e) {
        qCWarning(chart3DWidget) << "Error updating 3D display:" << e.what();
    }
}

void Chart3DWidget::updateDataPoints()
{
    // TODO: Implement data point updates based on current field assignments
    qCDebug(chart3DWidget) << "Updating data points - TODO: Implementation needed";
}

void Chart3DWidget::updateAxisRanges()
{
    // TODO: Implement auto-scaling for axes
    qCDebug(chart3DWidget) << "Updating axis ranges - TODO: Implementation needed";
}

void Chart3DWidget::initializePerformanceTracking3D()
{
    m_frameCount = 0;
    m_currentFPS = 0.0;
    m_currentPointCount = 0;
    m_lastFPSUpdate = std::chrono::steady_clock::now();
}

void Chart3DWidget::updateFPSCounter3D()
{
    m_frameCount++;
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFPSUpdate);
    
    if (duration.count() >= 1000) { // Update every second
        m_currentFPS = static_cast<double>(m_frameCount) * 1000.0 / duration.count();
        m_frameCount = 0;
        m_lastFPSUpdate = now;
        
        qCDebug(chart3DWidget) << "3D FPS:" << m_currentFPS;
    }
}

// Slot implementations
void Chart3DWidget::onResetCamera()
{
    resetCamera();
}

void Chart3DWidget::onSetCameraPreset()
{
    // TODO: Implement camera preset selection
    qCDebug(chart3DWidget) << "Set camera preset - TODO: Implementation needed";
}

void Chart3DWidget::onToggleAxes()
{
    m_chart3DConfig.showAxes = m_toggleAxesAction->isChecked();
    setupAxes();
}

void Chart3DWidget::onToggleGrid()
{
    m_chart3DConfig.showGrid = m_toggleGridAction->isChecked();
    // TODO: Implement grid toggle
}

void Chart3DWidget::onToggleLighting()
{
    if (m_toggleLightingAction->isChecked()) {
        setupLighting();
    } else {
        if (m_lightEntity) {
            delete m_lightEntity;
            m_lightEntity = nullptr;
        }
    }
}

void Chart3DWidget::onChangeRenderMode()
{
    if (!m_renderModeCombo) return;
    
    int modeIndex = m_renderModeCombo->currentData().toInt();
    m_chart3DConfig.renderMode = static_cast<RenderMode>(modeIndex);
    
    // TODO: Update rendering based on new mode
    qCDebug(chart3DWidget) << "Render mode changed to:" << modeIndex;
}

void Chart3DWidget::onToggleAutoRotate()
{
    m_chart3DConfig.autoRotate = m_autoRotateCheckBox->isChecked();
    
    if (m_chart3DConfig.autoRotate) {
        m_animationTimer->start(16); // ~60 FPS for smooth rotation
    } else {
        m_animationTimer->stop();
    }
}

void Chart3DWidget::onExport3DChart()
{
    export3DChart();
}

void Chart3DWidget::onShowChart3DSettings()
{
    // TODO: Implement settings dialog
    QMessageBox::information(this, "Settings", "3D Chart Settings dialog - TODO: Implementation needed");
}

void Chart3DWidget::onAnimationTimerTimeout()
{
    if (!m_chart3DConfig.autoRotate || !m_camera) {
        return;
    }
    
    // Simple auto-rotation around Y-axis
    static float rotationAngle = 0.0f;
    rotationAngle += m_chart3DConfig.rotationSpeed / 60.0f; // Degrees per frame at 60 FPS
    
    if (rotationAngle >= 360.0f) {
        rotationAngle -= 360.0f;
    }
    
    // Calculate new camera position
    float radius = m_chart3DConfig.cameraPosition.length();
    float radians = rotationAngle * M_PI / 180.0f;
    
    QVector3D newPosition(
        radius * cos(radians),
        m_chart3DConfig.cameraPosition.y(),
        radius * sin(radians)
    );
    
    m_camera->setPosition(newPosition);
}

void Chart3DWidget::onUpdate3DTimer()
{
    updateDisplay();
}

void Chart3DWidget::onCameraChanged()
{
    if (m_camera) {
        emit cameraChanged(m_camera->position(), m_camera->viewCenter());
    }
}

// Configuration methods
void Chart3DWidget::setChart3DConfig(const Chart3DConfig& config)
{
    m_chart3DConfig = config;
    
    if (m_isInitialized) {
        setupCamera();
        setupLighting();
        setupAxes();
    }
}

void Chart3DWidget::resetChart3DConfig()
{
    m_chart3DConfig = Chart3DConfig();
    setChart3DConfig(m_chart3DConfig);
}

void Chart3DWidget::resetCamera()
{
    if (m_camera) {
        m_camera->setPosition(m_chart3DConfig.cameraPosition);
        m_camera->setViewCenter(m_chart3DConfig.cameraTarget);
        m_camera->setUpVector(m_chart3DConfig.cameraUp);
    }
}

// Performance methods
double Chart3DWidget::getCurrentFPS() const
{
    return m_currentFPS;
}

int Chart3DWidget::getCurrentPointCount() const
{
    return m_currentPointCount;
}

bool Chart3DWidget::isGPUAccelerated() const
{
    return m_3dWindow != nullptr;
}

// Export functionality
bool Chart3DWidget::export3DChart(const QString& filePath)
{
    QString path = filePath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            this,
            "Export 3D Chart",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/chart3d.png",
            "PNG Images (*.png);;All Files (*)"
        );
        
        if (path.isEmpty()) {
            return false;
        }
    }
    
    // TODO: Implement 3D chart export
    qCDebug(chart3DWidget) << "Exporting 3D chart to:" << path << "- TODO: Implementation needed";
    
    return true;
}

bool Chart3DWidget::export3DChart(const QString& filePath, const QString& format, const QSize& size)
{
    Q_UNUSED(format)
    Q_UNUSED(size)
    return export3DChart(filePath);
}

// DisplayWidget interface implementation
void Chart3DWidget::handleFieldAdded(const FieldAssignment& field)
{
    Q_UNUSED(field)
    qCDebug(chart3DWidget) << "Field added to 3D chart - TODO: Implementation needed";
}

void Chart3DWidget::handleFieldRemoved(const QString& fieldPath)
{
    Q_UNUSED(fieldPath)
    qCDebug(chart3DWidget) << "Field removed from 3D chart - TODO: Implementation needed";
}

void Chart3DWidget::handleFieldsCleared()
{
    qCDebug(chart3DWidget) << "Fields cleared from 3D chart - TODO: Implementation needed";
}

// DisplayWidget pure virtual method implementations
void Chart3DWidget::updateFieldDisplay(const QString& fieldPath, const QVariant& value)
{
    Q_UNUSED(fieldPath)
    Q_UNUSED(value)
    qCDebug(chart3DWidget) << "Updating field display for 3D chart - TODO: Implementation needed";
}

void Chart3DWidget::clearFieldDisplay(const QString& fieldPath)
{
    Q_UNUSED(fieldPath)
    qCDebug(chart3DWidget) << "Clearing field display for 3D chart - TODO: Implementation needed";
}

void Chart3DWidget::refreshAllDisplays()
{
    qCDebug(chart3DWidget) << "Refreshing all displays for 3D chart - TODO: Implementation needed";
}

// Settings implementation
QJsonObject Chart3DWidget::saveWidgetSpecificSettings() const
{
    QJsonObject settings;
    
    // TODO: Implement settings serialization
    settings["chart3DConfig"] = QJsonObject{
        {"renderMode", static_cast<int>(m_chart3DConfig.renderMode)},
        {"cameraMode", static_cast<int>(m_chart3DConfig.cameraMode)},
        {"autoRotate", m_chart3DConfig.autoRotate},
        {"rotationSpeed", m_chart3DConfig.rotationSpeed}
    };
    
    return settings;
}

bool Chart3DWidget::restoreWidgetSpecificSettings(const QJsonObject& settings)
{
    Q_UNUSED(settings)
    // TODO: Implement settings deserialization
    qCDebug(chart3DWidget) << "Restoring 3D chart settings - TODO: Implementation needed";
    return true;
}

void Chart3DWidget::setupContextMenu()
{
    // TODO: Implement context menu for 3D chart
    qCDebug(chart3DWidget) << "Setting up 3D chart context menu - TODO: Implementation needed";
}

// Series management (temporary stubs)
bool Chart3DWidget::addSeries3D(const QString& fieldPath, const Series3DConfig& config)
{
    Q_UNUSED(fieldPath)
    Q_UNUSED(config)
    qCDebug(chart3DWidget) << "Adding 3D series - TODO: Implementation needed";
    return true;
}

bool Chart3DWidget::removeSeries3D(const QString& fieldPath)
{
    Q_UNUSED(fieldPath)
    qCDebug(chart3DWidget) << "Removing 3D series - TODO: Implementation needed";
    return true;
}

void Chart3DWidget::clearSeries3D()
{
    qCDebug(chart3DWidget) << "Clearing 3D series - TODO: Implementation needed";
}

QStringList Chart3DWidget::getSeries3DList() const
{
    qCDebug(chart3DWidget) << "Getting 3D series list - TODO: Implementation needed";
    return QStringList();
}