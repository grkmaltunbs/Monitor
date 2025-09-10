#ifndef CHART_3D_WIDGET_H
#define CHART_3D_WIDGET_H

#include "../display_widget.h"
#include "chart_common.h"

// Qt 3D includes for 3D rendering
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QDirectionalLight>
#include <Qt3DRender/QPointLight>
#include <Qt3DExtras/QFirstPersonCameraController>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QPlaneMesh>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QDiffuseSpecularMaterial>
// Note: Some advanced Qt3D geometry features will be implemented in future iterations
// #include <Qt3DRender/QMesh>
// #include <Qt3DRender/QBuffer>
// #include <Qt3DRender/QAttribute>
// #include <Qt3DRender/QGeometry>
// #include <Qt3DRender/QGeometryRenderer>

// Qt includes
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QGroupBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector3D>
#include <QQuaternion>
#include <QTimer>
#include <QWidget>
#include <memory>
#include <unordered_map>
#include <vector>

/**
 * @brief 3D Chart Widget for three-dimensional data visualization
 * 
 * Chart3DWidget provides advanced 3D visualization capabilities:
 * - 3-axis plotting with configurable field assignments
 * - Multiple rendering modes (points, lines, surfaces)
 * - Interactive camera controls (orbit, first-person)
 * - Lighting effects and material customization
 * - Real-time data updates with performance optimization
 * - Export functionality for 3D scenes
 * 
 * Key Features:
 * - X, Y, Z axis field assignment with independent scaling
 * - Point cloud, line strip, and surface rendering
 * - Dynamic lighting with directional and point lights
 * - Camera presets and custom positioning
 * - Animation support for data transitions
 * - GPU-accelerated rendering for performance
 * - Interactive tooltips in 3D space
 */
class Chart3DWidget : public DisplayWidget
{
    Q_OBJECT

public:
    /**
     * @brief 3D rendering modes
     */
    enum class RenderMode {
        Points,         // Individual data points as spheres
        Lines,          // Connected line strips
        Surface,        // Surface mesh from data
        PointCloud,     // High-density point cloud
        Wireframe,      // Wireframe surface representation
        Hybrid          // Combination of multiple modes
    };

    /**
     * @brief Camera control modes
     */
    enum class CameraMode {
        Orbit,          // Orbit around data center
        FirstPerson,    // First-person camera control
        Fixed,          // Fixed camera position
        Animated,       // Automatic rotation/movement
        Custom          // User-defined camera path
    };

    /**
     * @brief Lighting configuration
     */
    enum class LightingMode {
        Ambient,        // Ambient lighting only
        Directional,    // Single directional light
        Point,          // Point light source
        Multi,          // Multiple light sources
        Dynamic         // Dynamic lighting effects
    };

    /**
     * @brief 3D Chart configuration
     */
    struct Chart3DConfig {
        // Rendering settings
        RenderMode renderMode = RenderMode::Points;
        bool enableAntiAliasing = true;
        bool enableDepthTest = true;
        bool enableBlending = false;
        
        // Camera settings
        CameraMode cameraMode = CameraMode::Orbit;
        QVector3D cameraPosition = QVector3D(10, 10, 10);
        QVector3D cameraTarget = QVector3D(0, 0, 0);
        QVector3D cameraUp = QVector3D(0, 1, 0);
        float fieldOfView = 45.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        
        // Lighting settings
        LightingMode lightingMode = LightingMode::Directional;
        QVector3D lightDirection = QVector3D(-1, -1, -1);
        QVector3D lightPosition = QVector3D(10, 10, 10);
        QColor ambientColor = QColor(50, 50, 50);
        QColor diffuseColor = QColor(255, 255, 255);
        QColor specularColor = QColor(255, 255, 255);
        float lightIntensity = 1.0f;
        
        // Material settings
        QColor materialColor = QColor(100, 150, 200);
        float shininess = 80.0f;
        float transparency = 1.0f;
        bool useTextures = false;
        
        // Axis settings
        bool showAxes = true;
        bool showGrid = true;
        bool showLabels = true;
        QVector3D axisColors[3] = {
            QVector3D(1, 0, 0), // X-axis: Red
            QVector3D(0, 1, 0), // Y-axis: Green  
            QVector3D(0, 0, 1)  // Z-axis: Blue
        };
        
        // Animation settings
        bool enableAnimations = true;
        float animationSpeed = 1.0f;
        bool autoRotate = false;
        float rotationSpeed = 30.0f; // degrees per second
        
        // Performance settings
        int maxDataPoints = 100000;
        bool enableLevelOfDetail = true;
        float lodThreshold = 0.01f;
        bool enableCulling = true;
    };

    /**
     * @brief 3D Series configuration for field mapping
     */
    struct Series3DConfig {
        QString fieldPath;          // Field path for data
        QString seriesName;         // Display name
        QColor color;              // Series color
        bool visible = true;       // Series visibility
        
        // 3D-specific settings
        RenderMode renderMode = RenderMode::Points;
        float pointSize = 1.0f;
        float lineWidth = 2.0f;
        bool enableLighting = true;
        QColor materialColor;
        float transparency = 1.0f;
        
        // Axis assignment (0=X, 1=Y, 2=Z)
        int axisAssignment = 2; // Default to Z-axis
        
        Series3DConfig() : color(Monitor::Charts::ColorPalette::getColor(0)),
                          materialColor(Monitor::Charts::ColorPalette::getColor(0)) {}
        Series3DConfig(const QString& path, const QString& name) 
            : fieldPath(path), seriesName(name), 
              color(Monitor::Charts::ColorPalette::getColor(0)),
              materialColor(Monitor::Charts::ColorPalette::getColor(0)) {}
    };

    /**
     * @brief Axis configuration for 3D charts
     */
    struct AxisConfig {
        QString fieldPath;          // Field assigned to this axis
        QString label;             // Axis label
        double minValue = 0.0;     // Minimum value
        double maxValue = 100.0;   // Maximum value
        bool autoScale = true;     // Automatic scaling
        bool logarithmic = false;  // Logarithmic scale
        int tickCount = 10;        // Number of tick marks
        QColor color = QColor(255, 255, 255); // Axis color
    };

    explicit Chart3DWidget(const QString& widgetId, const QString& windowTitle, QWidget* parent = nullptr);
    ~Chart3DWidget() override;

    // Configuration management
    void setChart3DConfig(const Chart3DConfig& config);
    Chart3DConfig getChart3DConfig() const { return m_chart3DConfig; }
    void resetChart3DConfig();

    // Series management
    bool addSeries3D(const QString& fieldPath, const Series3DConfig& config);
    bool removeSeries3D(const QString& fieldPath);
    void clearSeries3D();
    QStringList getSeries3DList() const;
    Series3DConfig getSeries3DConfig(const QString& fieldPath) const;
    void setSeries3DConfig(const QString& fieldPath, const Series3DConfig& config);

    // Axis management
    void setAxisConfig(int axis, const AxisConfig& config); // 0=X, 1=Y, 2=Z
    AxisConfig getAxisConfig(int axis) const;
    void assignFieldToAxis(const QString& fieldPath, int axis);
    QString getAxisField(int axis) const;

    // Camera controls
    void setCameraPosition(const QVector3D& position);
    void setCameraTarget(const QVector3D& target);
    void resetCamera();
    void setCameraPreset(const QString& preset);
    QVector3D getCameraPosition() const;
    QVector3D getCameraTarget() const;

    // Rendering controls
    void setRenderMode(RenderMode mode);
    RenderMode getRenderMode() const { return m_chart3DConfig.renderMode; }
    void setLightingMode(LightingMode mode);
    LightingMode getLightingMode() const { return m_chart3DConfig.lightingMode; }

    // Export functionality
    bool export3DChart(const QString& filePath = QString());
    bool export3DChart(const QString& filePath, const QString& format, const QSize& size = QSize());

    // Performance monitoring
    double getCurrentFPS() const;
    int getCurrentPointCount() const;
    bool isGPUAccelerated() const;

public slots:
    // Camera control slots
    void onResetCamera();
    void onSetCameraPreset();
    void onToggleAutoRotate();
    
    // Rendering control slots
    void onChangeRenderMode();
    void onToggleLighting();
    void onToggleAxes();
    void onToggleGrid();
    
    // Export slots
    void onExport3DChart();
    void onShowChart3DSettings();

signals:
    // Interaction signals
    void chart3DClicked(QVector3D position);
    void chart3DDoubleClicked(QVector3D position);
    void pointHovered(QVector3D position, const QString& info);
    void cameraChanged(QVector3D position, QVector3D target);
    void renderModeChanged(RenderMode mode);

protected:
    // DisplayWidget implementation
    void initializeWidget() override;
    void updateDisplay() override;
    void handleFieldAdded(const FieldAssignment& field) override;
    void handleFieldRemoved(const QString& fieldPath) override;
    void handleFieldsCleared() override;

    // Settings implementation
    QJsonObject saveWidgetSpecificSettings() const override;
    bool restoreWidgetSpecificSettings(const QJsonObject& settings) override;

    // Context menu implementation
    void setupContextMenu() override;

    // DisplayWidget pure virtual methods
    void updateFieldDisplay(const QString& fieldPath, const QVariant& value) override;
    void clearFieldDisplay(const QString& fieldPath) override;
    void refreshAllDisplays() override;

    // 3D-specific setup methods
    void setup3DWindow();
    void setupScene();
    void setupCamera();
    void setupLighting();
    void setupAxes();
    void setupToolbar3D();
    void setupControlWidget();

    // Data visualization methods
    void createDataPoints();
    void updateDataPoints();
    void createSurfaceMesh();
    void updateSurfaceMesh();
    void createAxisEntities();
    void updateAxisRanges();

    // Performance optimization
    void optimizeFor3DPerformance();
    void enableLevelOfDetail();
    void updateLOD();
    bool shouldDecimatePts() const;

    // Helper methods
    QVector3D transformDataPoint(const QVector3D& dataPoint) const;
    QVector3D worldToScreen(const QVector3D& worldPos) const;
    QVector3D screenToWorld(const QPoint& screenPos) const;
    void updateTooltip(const QVector3D& position);

protected:
    // 3D Scene components
    Qt3DExtras::Qt3DWindow* m_3dWindow;
    Qt3DCore::QEntity* m_rootEntity;
    Qt3DCore::QEntity* m_sceneEntity;
    Qt3DRender::QCamera* m_camera;
    Qt3DExtras::QFirstPersonCameraController* m_fpsCameraController;
    Qt3DExtras::QOrbitCameraController* m_orbitCameraController;

    // Lighting
    Qt3DCore::QEntity* m_lightEntity;
    Qt3DRender::QDirectionalLight* m_directionalLight;
    Qt3DRender::QPointLight* m_pointLight;

    // Axis entities
    Qt3DCore::QEntity* m_axisEntities[3]; // X, Y, Z axes
    Qt3DCore::QEntity* m_gridEntity;
    Qt3DCore::QEntity* m_labelEntities[3];

    // Data visualization entities
    std::unordered_map<QString, Qt3DCore::QEntity*> m_dataEntities;
    std::unordered_map<QString, Qt3DExtras::QSphereMesh*> m_sphereMeshes;
    std::unordered_map<QString, Qt3DExtras::QPhongMaterial*> m_materials;

    // Layout and UI
    QVBoxLayout* m_mainLayout;
    QWidget* m_controlWidget;
    QToolBar* m_toolbar3D;

    // Configuration
    Chart3DConfig m_chart3DConfig;
    std::unordered_map<QString, Series3DConfig> m_series3DConfigs;
    AxisConfig m_axisConfigs[3]; // X, Y, Z axis configurations

    // State management
    QTimer* m_animationTimer;
    QTimer* m_updateTimer;
    bool m_isInitialized;

    // Performance tracking
    int m_frameCount;
    std::chrono::steady_clock::time_point m_lastFPSUpdate;
    double m_currentFPS;
    int m_currentPointCount;

    // Toolbar actions
    QAction* m_resetCameraAction;
    QAction* m_toggleAxesAction;
    QAction* m_toggleGridAction;
    QAction* m_toggleLightingAction;
    QAction* m_export3DAction;
    QAction* m_settings3DAction;
    QComboBox* m_renderModeCombo;
    QComboBox* m_cameraModeCombo;
    QSlider* m_rotationSpeedSlider;
    QCheckBox* m_autoRotateCheckBox;

private slots:
    // Internal update management
    void onAnimationTimerTimeout();
    void onUpdate3DTimer();
    void onCameraChanged();

private:
    // Helper methods
    void initializePerformanceTracking3D();
    void updateFPSCounter3D();
    void populateRenderModeCombo();
    void populateCameraModeCombo();
    Qt3DCore::QEntity* createSphereEntity(const QVector3D& position, float radius, const QColor& color);
    Qt3DCore::QEntity* createLineEntity(const QVector3D& start, const QVector3D& end, const QColor& color);
    void updateMaterialProperties();
    void setupCameraController();
};

#endif // CHART_3D_WIDGET_H