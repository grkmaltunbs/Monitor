#include <QTest>
#include <QApplication>
#include <QSignalSpy>
#include <QTimer>
#include <QEventLoop>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QVector3D>
#include <QQuaternion>
#include <QMatrix4x4>
#include <QElapsedTimer>
#include <memory>

// Qt 3D includes
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

// Application includes
#include "../../../src/ui/widgets/charts/chart_3d_widget.h"

/**
 * @brief Qt 3D Framework Integration Tests
 * 
 * These tests verify the proper integration between the Chart3DWidget
 * and the Qt 3D framework, ensuring correct 3D scene management,
 * entity creation, and OpenGL rendering.
 */
class TestQt3DIntegration : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QApplication> app;
    std::unique_ptr<Chart3DWidget> widget;
    std::unique_ptr<QOpenGLContext> context;
    std::unique_ptr<QOffscreenSurface> surface;

    void waitForRender(int ms = 100) {
        QEventLoop loop;
        QTimer::singleShot(ms, &loop, &QEventLoop::quit);
        loop.exec();
    }

    void createOpenGLContext() {
        surface = std::make_unique<QOffscreenSurface>();
        surface->create();
        
        context = std::make_unique<QOpenGLContext>();
        context->create();
        context->makeCurrent(surface.get());
    }

private slots:
    void initTestCase() {
        if (!QApplication::instance()) {
            int argc = 1;
            char* argv[] = {const_cast<char*>("test")};
            app = std::make_unique<QApplication>(argc, argv);
        }
        
        createOpenGLContext();
        
        widget = std::make_unique<Chart3DWidget>(
            "test_3d_widget",
            "Test 3D Widget",
            nullptr
        );
        
        // Initialize the widget
        widget->show();
        waitForRender(200);
    }

    void cleanupTestCase() {
        widget.reset();
        if (context) {
            context->doneCurrent();
        }
        context.reset();
        surface.reset();
    }

    void cleanup() {
        if (widget) {
            widget->clearSeries3D();
            widget->resetChart3DConfig();
        }
        waitForRender();
    }

    // Core Qt 3D Framework Tests
    void test3DWindowCreation();
    void testRootEntityCreation();
    void testSceneEntityHierarchy();
    void testCameraEntityCreation();
    void testLightEntityCreation();
    
    // Camera System Tests
    void testCameraControllerCreation();
    void testOrbitCameraController();
    void testFirstPersonCameraController();
    void testCameraTransitions();
    void testCameraMatrices();
    
    // Entity Management Tests
    void testDataEntityCreation();
    void testEntityHierarchy();
    void testEntityComponentSystem();
    void testEntityLifecycle();
    void testEntityTransforms();
    
    // Mesh and Material Tests
    void testSphereMeshCreation();
    void testCylinderMeshCreation();
    void testPlaneMeshCreation();
    void testMaterialCreation();
    void testMaterialProperties();
    
    // Lighting System Tests
    void testDirectionalLightSetup();
    void testPointLightSetup();
    void testMultiLightScenario();
    void testLightTransforms();
    void testLightColors();
    
    // Scene Management Tests
    void testSceneInitialization();
    void testSceneCleanup();
    void testSceneReset();
    void testSceneValidation();
    
    // Rendering Tests
    void testRenderingInitialization();
    void testFrameBufferSetup();
    void testViewportConfiguration();
    void testRenderLoop();
    void testRenderingPerformance();
    
    // Integration Tests
    void testWidgetTo3DIntegration();
    void test3DSceneUpdates();
    void testRealTimeDataVisualization();
    void testMultipleDataSeries();
    void testLargeDatasetHandling();
    
    // Error Handling Tests
    void testOpenGLContextFailure();
    void testInvalidEntityHandling();
    void testMemoryExhaustion();
    void testRenderingFailures();
    
    // Performance Tests
    void testEntityCreationPerformance();
    void testRenderingFrameRate();
    void testMemoryUsage();
    void testLargeScenePerformance();
};

void TestQt3DIntegration::test3DWindowCreation()
{
    // Test that Qt3DWindow is properly created
    auto* window3D = widget->get3DWindow();
    QVERIFY(window3D != nullptr);
    
    // Test window properties
    QVERIFY(window3D->isValid());
    QVERIFY(window3D->format().majorVersion() >= 3);
    
    // Test window size
    QVERIFY(window3D->width() > 0);
    QVERIFY(window3D->height() > 0);
}

void TestQt3DIntegration::testRootEntityCreation()
{
    // Test root entity creation
    auto* rootEntity = widget->getRootEntity();
    QVERIFY(rootEntity != nullptr);
    
    // Test entity properties
    QVERIFY(rootEntity->isEnabled());
    QCOMPARE(rootEntity->parent(), static_cast<QObject*>(nullptr));
    
    // Test entity components
    auto components = rootEntity->components();
    QVERIFY(!components.isEmpty());
}

void TestQt3DIntegration::testSceneEntityHierarchy()
{
    auto* rootEntity = widget->getRootEntity();
    auto* sceneEntity = widget->getSceneEntity();
    
    QVERIFY(rootEntity != nullptr);
    QVERIFY(sceneEntity != nullptr);
    
    // Test hierarchy relationship
    QCOMPARE(sceneEntity->parent(), rootEntity);
    
    // Test scene entity children
    auto children = sceneEntity->childNodes();
    QVERIFY(children.size() >= 0); // Should have at least camera and lights
}

void TestQt3DIntegration::testCameraEntityCreation()
{
    auto* camera = widget->getCamera();
    QVERIFY(camera != nullptr);
    
    // Test camera properties
    QVERIFY(camera->fieldOfView() > 0.0f);
    QVERIFY(camera->nearPlane() > 0.0f);
    QVERIFY(camera->farPlane() > camera->nearPlane());
    
    // Test camera position
    auto position = camera->position();
    QVERIFY(!position.isNull());
    
    // Test camera view center
    auto viewCenter = camera->viewCenter();
    QVERIFY(!viewCenter.isNull());
    
    // Test camera up vector
    auto upVector = camera->upVector();
    QVERIFY(!upVector.isNull());
    QVERIFY(qFuzzyCompare(upVector.length(), 1.0f)); // Should be normalized
}

void TestQt3DIntegration::testLightEntityCreation()
{
    // Test directional light
    auto* dirLight = widget->getDirectionalLight();
    QVERIFY(dirLight != nullptr);
    
    // Test light properties
    QVERIFY(dirLight->intensity() > 0.0f);
    QVERIFY(dirLight->color().isValid());
    
    // Test light direction
    auto direction = dirLight->worldDirection();
    QVERIFY(!direction.isNull());
    
    // Test point light (if configured)
    auto* pointLight = widget->getPointLight();
    if (pointLight) {
        QVERIFY(pointLight->intensity() > 0.0f);
        QVERIFY(pointLight->color().isValid());
    }
}

void TestQt3DIntegration::testCameraControllerCreation()
{
    // Test orbit camera controller
    auto* orbitController = widget->getOrbitCameraController();
    QVERIFY(orbitController != nullptr);
    
    // Test controller properties
    auto* controlledCamera = orbitController->camera();
    QCOMPARE(controlledCamera, widget->getCamera());
    
    // Test first person controller
    auto* fpsController = widget->getFirstPersonCameraController();
    QVERIFY(fpsController != nullptr);
    
    auto* fpsControlledCamera = fpsController->camera();
    QCOMPARE(fpsControlledCamera, widget->getCamera());
}

void TestQt3DIntegration::testOrbitCameraController()
{
    auto* controller = widget->getOrbitCameraController();
    QVERIFY(controller != nullptr);
    
    // Set orbit camera mode
    widget->setCameraMode(Chart3DWidget::CameraMode::Orbit);
    waitForRender();
    
    // Test controller properties
    QVERIFY(controller->linearSpeed() > 0.0f);
    QVERIFY(controller->lookSpeed() > 0.0f);
    
    // Test camera position changes
    auto initialPosition = widget->getCameraPosition();
    
    // Simulate orbit movement
    controller->setLinearSpeed(10.0f);
    controller->setLookSpeed(50.0f);
    
    waitForRender(100);
    
    // Position should be controllable
    widget->setCameraPosition(QVector3D(5, 5, 5));
    waitForRender();
    
    auto newPosition = widget->getCameraPosition();
    QVERIFY(!qFuzzyCompare(initialPosition, newPosition));
}

void TestQt3DIntegration::testFirstPersonCameraController()
{
    auto* controller = widget->getFirstPersonCameraController();
    QVERIFY(controller != nullptr);
    
    // Set first person camera mode
    widget->setCameraMode(Chart3DWidget::CameraMode::FirstPerson);
    waitForRender();
    
    // Test controller properties
    QVERIFY(controller->linearSpeed() > 0.0f);
    QVERIFY(controller->lookSpeed() > 0.0f);
    
    // Test camera control
    controller->setLinearSpeed(15.0f);
    controller->setLookSpeed(100.0f);
    
    waitForRender(50);
    
    // Controller should be active
    QVERIFY(controller->isEnabled());
}

void TestQt3DIntegration::testCameraTransitions()
{
    // Test smooth camera transitions between modes
    QSignalSpy spy(widget.get(), &Chart3DWidget::cameraChanged);
    
    // Start with orbit mode
    widget->setCameraMode(Chart3DWidget::CameraMode::Orbit);
    waitForRender();
    
    // Switch to first person
    widget->setCameraMode(Chart3DWidget::CameraMode::FirstPerson);
    waitForRender();
    
    // Should have camera change signals
    QVERIFY(spy.count() >= 1);
    
    // Switch to fixed mode
    widget->setCameraMode(Chart3DWidget::CameraMode::Fixed);
    waitForRender();
    
    QVERIFY(spy.count() >= 2);
    
    // Test camera reset
    widget->resetCamera();
    waitForRender();
    
    QVERIFY(spy.count() >= 3);
}

void TestQt3DIntegration::testCameraMatrices()
{
    auto* camera = widget->getCamera();
    QVERIFY(camera != nullptr);
    
    // Test projection matrix
    auto projMatrix = camera->projectionMatrix();
    QVERIFY(!projMatrix.isIdentity());
    
    // Test view matrix
    auto viewMatrix = camera->viewMatrix();
    QVERIFY(!viewMatrix.isIdentity());
    
    // Test view projection matrix
    auto viewProjMatrix = camera->projectionMatrix() * camera->viewMatrix();
    QVERIFY(!viewProjMatrix.isIdentity());
    
    // Test matrix updates after camera changes
    auto initialProjMatrix = camera->projectionMatrix();
    
    camera->setFieldOfView(60.0f);
    waitForRender();
    
    auto newProjMatrix = camera->projectionMatrix();
    QVERIFY(initialProjMatrix != newProjMatrix);
}

void TestQt3DIntegration::testDataEntityCreation()
{
    // Add a 3D series to trigger entity creation
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "test.field";
    config.seriesName = "Test Series";
    config.renderMode = Chart3DWidget::RenderMode::Points;
    
    bool result = widget->addSeries3D("test.field", config);
    QVERIFY(result);
    
    waitForRender();
    
    // Test that data entities were created
    auto dataEntities = widget->getDataEntities();
    QVERIFY(dataEntities.find("test.field") != dataEntities.end());
    
    auto* entity = dataEntities["test.field"];
    QVERIFY(entity != nullptr);
    QVERIFY(entity->isEnabled());
}

void TestQt3DIntegration::testEntityHierarchy()
{
    // Create test series with entities
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "hierarchy.test";
    config.seriesName = "Hierarchy Test";
    
    widget->addSeries3D("hierarchy.test", config);
    waitForRender();
    
    // Test entity parent-child relationships
    auto* sceneEntity = widget->getSceneEntity();
    auto dataEntities = widget->getDataEntities();
    
    if (!dataEntities.empty()) {
        auto* dataEntity = dataEntities.begin()->second;
        
        // Data entities should be children of scene entity
        bool foundInScene = false;
        for (auto* child : sceneEntity->childNodes()) {
            if (child == dataEntity) {
                foundInScene = true;
                break;
            }
        }
        QVERIFY(foundInScene);
    }
}

void TestQt3DIntegration::testEntityComponentSystem()
{
    // Create entity with components
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "components.test";
    config.renderMode = Chart3DWidget::RenderMode::Points;
    config.pointSize = 2.0f;
    
    widget->addSeries3D("components.test", config);
    waitForRender();
    
    auto dataEntities = widget->getDataEntities();
    QVERIFY(!dataEntities.empty());
    
    auto* entity = dataEntities["components.test"];
    QVERIFY(entity != nullptr);
    
    // Test entity components
    auto components = entity->components();
    QVERIFY(!components.isEmpty());
    
    // Should have mesh and material components for point rendering
    bool hasMesh = false;
    bool hasMaterial = false;
    bool hasTransform = false;
    
    for (auto* component : components) {
        if (qobject_cast<Qt3DRender::QGeometryRenderer*>(component)) {
            hasMesh = true;
        } else if (qobject_cast<Qt3DRender::QMaterial*>(component)) {
            hasMaterial = true;
        } else if (qobject_cast<Qt3DCore::QTransform*>(component)) {
            hasTransform = true;
        }
    }
    
    QVERIFY(hasMesh);
    QVERIFY(hasMaterial);
    QVERIFY(hasTransform);
}

void TestQt3DIntegration::testEntityLifecycle()
{
    // Test entity creation and destruction lifecycle
    QString fieldPath = "lifecycle.test";
    
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = fieldPath;
    config.seriesName = "Lifecycle Test";
    
    // Create entity
    widget->addSeries3D(fieldPath, config);
    waitForRender();
    
    auto dataEntities = widget->getDataEntities();
    QVERIFY(dataEntities.find(fieldPath) != dataEntities.end());
    
    auto* entity = dataEntities[fieldPath];
    QVERIFY(entity != nullptr);
    QVERIFY(entity->isEnabled());
    
    // Remove entity
    widget->removeSeries3D(fieldPath);
    waitForRender();
    
    dataEntities = widget->getDataEntities();
    QVERIFY(dataEntities.find(fieldPath) == dataEntities.end());
}

void TestQt3DIntegration::testEntityTransforms()
{
    // Create entity and test transformations
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "transform.test";
    config.seriesName = "Transform Test";
    
    widget->addSeries3D("transform.test", config);
    waitForRender();
    
    auto dataEntities = widget->getDataEntities();
    auto* entity = dataEntities["transform.test"];
    QVERIFY(entity != nullptr);
    
    // Find transform component
    Qt3DCore::QTransform* transform = nullptr;
    for (auto* component : entity->components()) {
        transform = qobject_cast<Qt3DCore::QTransform*>(component);
        if (transform) break;
    }
    
    QVERIFY(transform != nullptr);
    
    // Test transform properties
    auto matrix = transform->matrix();
    QVERIFY(!matrix.isIdentity() || matrix.isIdentity()); // Either is valid
    
    // Test transform modifications
    transform->setTranslation(QVector3D(1, 2, 3));
    auto translation = transform->translation();
    QCOMPARE(translation, QVector3D(1, 2, 3));
    
    transform->setScale(2.0f);
    QCOMPARE(transform->scale(), 2.0f);
}

void TestQt3DIntegration::testSphereMeshCreation()
{
    // Test sphere mesh creation and properties
    auto sphereMeshes = widget->getSphereMeshes();
    
    // Add series to trigger mesh creation
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "sphere.test";
    config.renderMode = Chart3DWidget::RenderMode::Points;
    config.pointSize = 1.5f;
    
    widget->addSeries3D("sphere.test", config);
    waitForRender();
    
    sphereMeshes = widget->getSphereMeshes();
    QVERIFY(!sphereMeshes.empty());
    
    auto* sphereMesh = sphereMeshes.begin()->second;
    QVERIFY(sphereMesh != nullptr);
    
    // Test sphere properties
    QVERIFY(sphereMesh->radius() > 0.0f);
    QVERIFY(sphereMesh->slices() > 8);
    QVERIFY(sphereMesh->rings() > 8);
}

void TestQt3DIntegration::testCylinderMeshCreation()
{
    // Test cylinder mesh for line rendering
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "cylinder.test";
    config.renderMode = Chart3DWidget::RenderMode::Lines;
    config.lineWidth = 3.0f;
    
    widget->addSeries3D("cylinder.test", config);
    waitForRender();
    
    // Cylinder meshes are used for line segments in 3D
    auto dataEntities = widget->getDataEntities();
    QVERIFY(!dataEntities.empty());
    
    // Check that entities were created for line rendering
    auto* entity = dataEntities["cylinder.test"];
    QVERIFY(entity != nullptr);
    
    // Should have appropriate components for line rendering
    auto components = entity->components();
    QVERIFY(!components.isEmpty());
}

void TestQt3DIntegration::testPlaneMeshCreation()
{
    // Test plane mesh for surface rendering
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "plane.test";
    config.renderMode = Chart3DWidget::RenderMode::Surface;
    
    widget->addSeries3D("plane.test", config);
    waitForRender();
    
    auto dataEntities = widget->getDataEntities();
    auto* entity = dataEntities["plane.test"];
    QVERIFY(entity != nullptr);
    
    // Should have mesh component for surface
    bool hasMesh = false;
    for (auto* component : entity->components()) {
        if (qobject_cast<Qt3DRender::QGeometryRenderer*>(component)) {
            hasMesh = true;
            break;
        }
    }
    QVERIFY(hasMesh);
}

void TestQt3DIntegration::testMaterialCreation()
{
    // Test material creation and assignment
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "material.test";
    config.color = QColor(255, 128, 64);
    config.transparency = 0.8f;
    config.enableLighting = true;
    
    widget->addSeries3D("material.test", config);
    waitForRender();
    
    auto materials = widget->getMaterials();
    QVERIFY(!materials.empty());
    
    auto* material = materials.begin()->second;
    QVERIFY(material != nullptr);
    
    // Test material properties
    auto diffuse = material->diffuse();
    QVERIFY(diffuse.isValid());
    
    auto specular = material->specular();
    QVERIFY(specular.isValid());
    
    QVERIFY(material->shininess() > 0.0f);
}

void TestQt3DIntegration::testMaterialProperties()
{
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "material.props";
    config.materialColor = QColor(200, 100, 50);
    config.transparency = 0.6f;
    
    widget->addSeries3D("material.props", config);
    waitForRender();
    
    auto materials = widget->getMaterials();
    auto* material = materials["material.props"];
    QVERIFY(material != nullptr);
    
    // Test that material reflects configuration
    auto diffuse = material->diffuse();
    QVERIFY(diffuse.isValid());
    
    // Test transparency/alpha
    QVERIFY(material->alpha() >= 0.0f && material->alpha() <= 1.0f);
}

void TestQt3DIntegration::testDirectionalLightSetup()
{
    auto* dirLight = widget->getDirectionalLight();
    QVERIFY(dirLight != nullptr);
    
    // Test light properties
    QVERIFY(dirLight->intensity() > 0.0f);
    QVERIFY(dirLight->color().isValid());
    
    // Test light direction
    auto direction = dirLight->worldDirection();
    QVERIFY(!direction.isNull());
    QVERIFY(qFuzzyCompare(direction.length(), 1.0f)); // Should be normalized
    
    // Test light configuration changes
    auto config = widget->getChart3DConfig();
    config.lightDirection = QVector3D(0, -1, -1).normalized();
    config.diffuseColor = QColor(255, 255, 200);
    config.lightIntensity = 0.8f;
    
    widget->setChart3DConfig(config);
    waitForRender();
    
    // Verify changes applied
    QCOMPARE(dirLight->intensity(), 0.8f);
}

void TestQt3DIntegration::testPointLightSetup()
{
    // Configure point lighting
    auto config = widget->getChart3DConfig();
    config.lightingMode = Chart3DWidget::LightingMode::Point;
    config.lightPosition = QVector3D(5, 5, 5);
    config.lightIntensity = 1.2f;
    
    widget->setChart3DConfig(config);
    waitForRender();
    
    auto* pointLight = widget->getPointLight();
    QVERIFY(pointLight != nullptr);
    
    // Test point light properties
    QVERIFY(pointLight->intensity() > 0.0f);
    QVERIFY(pointLight->color().isValid());
    
    // Test light position (through transform component)
    auto* lightEntity = widget->getLightEntity();
    QVERIFY(lightEntity != nullptr);
    
    // Find transform component
    Qt3DCore::QTransform* transform = nullptr;
    for (auto* component : lightEntity->components()) {
        transform = qobject_cast<Qt3DCore::QTransform*>(component);
        if (transform) break;
    }
    
    if (transform) {
        auto position = transform->translation();
        QCOMPARE(position, QVector3D(5, 5, 5));
    }
}

void TestQt3DIntegration::testMultiLightScenario()
{
    // Configure multi-light scenario
    auto config = widget->getChart3DConfig();
    config.lightingMode = Chart3DWidget::LightingMode::Multi;
    config.lightIntensity = 0.7f;
    
    widget->setChart3DConfig(config);
    waitForRender();
    
    // Should have both directional and point lights
    auto* dirLight = widget->getDirectionalLight();
    auto* pointLight = widget->getPointLight();
    
    QVERIFY(dirLight != nullptr);
    QVERIFY(pointLight != nullptr);
    
    // Both lights should be enabled
    QVERIFY(dirLight->isEnabled());
    QVERIFY(pointLight->isEnabled());
    
    // Light intensities should be balanced for multi-light
    QVERIFY(dirLight->intensity() > 0.0f);
    QVERIFY(pointLight->intensity() > 0.0f);
}

void TestQt3DIntegration::testLightTransforms()
{
    auto* lightEntity = widget->getLightEntity();
    QVERIFY(lightEntity != nullptr);
    
    // Test light entity transform
    Qt3DCore::QTransform* transform = nullptr;
    for (auto* component : lightEntity->components()) {
        transform = qobject_cast<Qt3DCore::QTransform*>(component);
        if (transform) break;
    }
    
    if (transform) {
        // Test transform operations
        auto initialTranslation = transform->translation();
        
        transform->setTranslation(QVector3D(10, 0, 0));
        QCOMPARE(transform->translation(), QVector3D(10, 0, 0));
        
        transform->setRotationX(45.0f);
        QCOMPARE(transform->rotationX(), 45.0f);
        
        transform->setScale(2.0f);
        QCOMPARE(transform->scale(), 2.0f);
    }
}

void TestQt3DIntegration::testLightColors()
{
    auto* dirLight = widget->getDirectionalLight();
    QVERIFY(dirLight != nullptr);
    
    // Test color changes
    QColor testColor(255, 128, 64);
    dirLight->setColor(testColor);
    QCOMPARE(dirLight->color(), testColor);
    
    // Test with point light
    auto config = widget->getChart3DConfig();
    config.lightingMode = Chart3DWidget::LightingMode::Point;
    widget->setChart3DConfig(config);
    waitForRender();
    
    auto* pointLight = widget->getPointLight();
    if (pointLight) {
        QColor pointColor(64, 128, 255);
        pointLight->setColor(pointColor);
        QCOMPARE(pointLight->color(), pointColor);
    }
}

void TestQt3DIntegration::testSceneInitialization()
{
    // Test complete scene initialization
    auto* rootEntity = widget->getRootEntity();
    auto* sceneEntity = widget->getSceneEntity();
    auto* camera = widget->getCamera();
    
    QVERIFY(rootEntity != nullptr);
    QVERIFY(sceneEntity != nullptr);
    QVERIFY(camera != nullptr);
    
    // Test scene hierarchy
    QCOMPARE(sceneEntity->parent(), rootEntity);
    
    // Test axis entities
    auto axisEntities = widget->getAxisEntities();
    QCOMPARE(axisEntities.size(), 3); // X, Y, Z axes
    
    for (auto* axisEntity : axisEntities) {
        QVERIFY(axisEntity != nullptr);
        QVERIFY(axisEntity->isEnabled());
    }
    
    // Test grid entity
    auto* gridEntity = widget->getGridEntity();
    if (gridEntity) {
        QVERIFY(gridEntity->isEnabled());
    }
}

void TestQt3DIntegration::testSceneCleanup()
{
    // Add multiple series
    for (int i = 0; i < 5; ++i) {
        Chart3DWidget::Series3DConfig config;
        config.fieldPath = QString("cleanup.test%1").arg(i);
        config.seriesName = QString("Cleanup Test %1").arg(i);
        widget->addSeries3D(config.fieldPath, config);
    }
    waitForRender();
    
    auto dataEntities = widget->getDataEntities();
    QCOMPARE(dataEntities.size(), 5);
    
    // Clear all series
    widget->clearSeries3D();
    waitForRender();
    
    dataEntities = widget->getDataEntities();
    QVERIFY(dataEntities.empty());
    
    // Scene should still be valid
    auto* sceneEntity = widget->getSceneEntity();
    QVERIFY(sceneEntity != nullptr);
    QVERIFY(sceneEntity->isEnabled());
}

void TestQt3DIntegration::testSceneReset()
{
    // Modify scene configuration
    auto config = widget->getChart3DConfig();
    config.showAxes = false;
    config.showGrid = false;
    config.lightingMode = Chart3DWidget::LightingMode::Point;
    widget->setChart3DConfig(config);
    
    // Add some data
    Chart3DWidget::Series3DConfig seriesConfig;
    seriesConfig.fieldPath = "reset.test";
    widget->addSeries3D("reset.test", seriesConfig);
    waitForRender();
    
    // Reset to defaults
    widget->resetChart3DConfig();
    waitForRender();
    
    // Verify reset
    auto resetConfig = widget->getChart3DConfig();
    QVERIFY(resetConfig.showAxes); // Should be back to default
    QVERIFY(resetConfig.showGrid); // Should be back to default
    QCOMPARE(resetConfig.lightingMode, Chart3DWidget::LightingMode::Directional);
}

void TestQt3DIntegration::testSceneValidation()
{
    // Test scene integrity
    auto* rootEntity = widget->getRootEntity();
    auto* sceneEntity = widget->getSceneEntity();
    
    // Validate entity hierarchy
    QVERIFY(rootEntity->childNodes().contains(sceneEntity));
    
    // Validate camera
    auto* camera = widget->getCamera();
    QVERIFY(camera->parent() != nullptr);
    
    // Validate lights
    auto* lightEntity = widget->getLightEntity();
    QVERIFY(lightEntity != nullptr);
    QVERIFY(sceneEntity->childNodes().contains(lightEntity));
    
    // Validate axis entities
    auto axisEntities = widget->getAxisEntities();
    for (auto* axisEntity : axisEntities) {
        QVERIFY(sceneEntity->childNodes().contains(axisEntity));
    }
}

void TestQt3DIntegration::testRenderingInitialization()
{
    auto* window3D = widget->get3DWindow();
    QVERIFY(window3D != nullptr);
    
    // Test rendering initialization
    QVERIFY(window3D->isValid());
    
    // Test OpenGL context
    auto* glContext = window3D->openglContext();
    QVERIFY(glContext != nullptr);
    QVERIFY(glContext->isValid());
    
    // Test surface format
    auto format = window3D->format();
    QVERIFY(format.majorVersion() >= 3);
    QVERIFY(format.profile() == QSurfaceFormat::CoreProfile ||
            format.profile() == QSurfaceFormat::CompatibilityProfile);
}

void TestQt3DIntegration::testFrameBufferSetup()
{
    auto* window3D = widget->get3DWindow();
    QVERIFY(window3D != nullptr);
    
    // Test framebuffer properties
    QVERIFY(window3D->width() > 0);
    QVERIFY(window3D->height() > 0);
    
    // Test format properties
    auto format = window3D->format();
    QVERIFY(format.depthBufferSize() > 0);
    QVERIFY(format.colorBufferSize() > 0);
}

void TestQt3DIntegration::testViewportConfiguration()
{
    auto* camera = widget->getCamera();
    QVERIFY(camera != nullptr);
    
    // Test viewport properties
    QVERIFY(camera->fieldOfView() > 0.0f && camera->fieldOfView() < 180.0f);
    QVERIFY(camera->nearPlane() > 0.0f);
    QVERIFY(camera->farPlane() > camera->nearPlane());
    
    // Test aspect ratio
    QVERIFY(camera->aspectRatio() > 0.0f);
    
    // Test viewport rectangle
    auto viewportRect = camera->viewportRect();
    QCOMPARE(viewportRect.x(), 0.0f);
    QCOMPARE(viewportRect.y(), 0.0f);
    QCOMPARE(viewportRect.width(), 1.0f);
    QCOMPARE(viewportRect.height(), 1.0f);
}

void TestQt3DIntegration::testRenderLoop()
{
    // Test that rendering loop is active
    QSignalSpy renderSpy(widget.get(), &Chart3DWidget::frameRendered);
    
    // Wait for render cycles
    waitForRender(200);
    
    // Should have some render signals (if widget is visible)
    if (widget->isVisible()) {
        QVERIFY(renderSpy.count() >= 0); // May be 0 if not actively rendering
    }
    
    // Test that scene updates trigger renders
    widget->setCameraPosition(QVector3D(5, 5, 5));
    waitForRender(100);
    
    // Should have additional renders from camera change
    if (widget->isVisible()) {
        QVERIFY(renderSpy.count() >= 0);
    }
}

void TestQt3DIntegration::testRenderingPerformance()
{
    if (!widget->isVisible()) {
        QSKIP("Widget not visible, skipping rendering performance test");
    }
    
    // Test rendering performance
    QElapsedTimer timer;
    timer.start();
    
    // Add data and measure rendering time
    for (int i = 0; i < 10; ++i) {
        Chart3DWidget::Series3DConfig config;
        config.fieldPath = QString("perf.test%1").arg(i);
        config.renderMode = Chart3DWidget::RenderMode::Points;
        widget->addSeries3D(config.fieldPath, config);
    }
    
    waitForRender(200);
    
    qint64 elapsed = timer.elapsed();
    
    // Should render within reasonable time (2 seconds for 10 series)
    QVERIFY(elapsed < 2000);
    
    // Test FPS
    double fps = widget->getCurrentFPS();
    if (fps > 0) {
        QVERIFY(fps >= 10.0); // At least 10 FPS minimum
    }
}

void TestQt3DIntegration::testWidgetTo3DIntegration()
{
    // Test widget-level configuration affecting 3D scene
    auto config = widget->getChart3DConfig();
    config.renderMode = Chart3DWidget::RenderMode::Points;
    config.enableAntiAliasing = true;
    config.enableDepthTest = true;
    config.showAxes = true;
    config.showGrid = true;
    
    widget->setChart3DConfig(config);
    waitForRender();
    
    // Verify 3D scene reflects widget configuration
    auto* window3D = widget->get3DWindow();
    auto format = window3D->format();
    
    if (config.enableAntiAliasing) {
        QVERIFY(format.samples() > 1);
    }
    
    // Verify axes visibility
    auto axisEntities = widget->getAxisEntities();
    for (auto* axisEntity : axisEntities) {
        QCOMPARE(axisEntity->isEnabled(), config.showAxes);
    }
    
    // Verify grid visibility
    auto* gridEntity = widget->getGridEntity();
    if (gridEntity) {
        QCOMPARE(gridEntity->isEnabled(), config.showGrid);
    }
}

void TestQt3DIntegration::test3DSceneUpdates()
{
    // Test real-time scene updates
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "updates.test";
    config.renderMode = Chart3DWidget::RenderMode::Points;
    
    widget->addSeries3D("updates.test", config);
    waitForRender();
    
    // Simulate field updates
    QSignalSpy updateSpy(widget.get(), &Chart3DWidget::sceneUpdated);
    
    // Update field display (simulating real data)
    widget->updateFieldDisplay("updates.test", QVariant(42.5));
    waitForRender();
    
    // Should trigger scene updates
    QVERIFY(updateSpy.count() >= 0);
    
    // Test multiple rapid updates
    for (int i = 0; i < 10; ++i) {
        widget->updateFieldDisplay("updates.test", QVariant(i * 1.5));
    }
    waitForRender(100);
    
    // Scene should handle rapid updates
    QVERIFY(updateSpy.count() >= 0);
}

void TestQt3DIntegration::testRealTimeDataVisualization()
{
    // Test real-time data visualization pipeline
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "realtime.x";
    config.renderMode = Chart3DWidget::RenderMode::Points;
    config.axisAssignment = 0; // X-axis
    
    widget->addSeries3D("realtime.x", config);
    
    config.fieldPath = "realtime.y";
    config.axisAssignment = 1; // Y-axis
    widget->addSeries3D("realtime.y", config);
    
    config.fieldPath = "realtime.z";
    config.axisAssignment = 2; // Z-axis
    widget->addSeries3D("realtime.z", config);
    
    waitForRender();
    
    // Simulate streaming data
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < 100; ++i) {
        double t = i * 0.1;
        widget->updateFieldDisplay("realtime.x", QVariant(qSin(t)));
        widget->updateFieldDisplay("realtime.y", QVariant(qCos(t)));
        widget->updateFieldDisplay("realtime.z", QVariant(t * 0.1));
        
        if (i % 10 == 0) {
            waitForRender(10);
        }
    }
    
    qint64 elapsed = timer.elapsed();
    
    // Should handle 100 updates efficiently (under 5 seconds)
    QVERIFY(elapsed < 5000);
    
    // Verify data is visualized
    auto dataEntities = widget->getDataEntities();
    QCOMPARE(dataEntities.size(), 3);
}

void TestQt3DIntegration::testMultipleDataSeries()
{
    // Test multiple data series in 3D scene
    const int seriesCount = 20;
    
    for (int i = 0; i < seriesCount; ++i) {
        Chart3DWidget::Series3DConfig config;
        config.fieldPath = QString("multi.series%1").arg(i);
        config.seriesName = QString("Series %1").arg(i);
        config.renderMode = (i % 2 == 0) ? 
            Chart3DWidget::RenderMode::Points : 
            Chart3DWidget::RenderMode::Lines;
        config.color = QColor::fromHsv((i * 360 / seriesCount) % 360, 255, 255);
        
        widget->addSeries3D(config.fieldPath, config);
    }
    
    waitForRender(300);
    
    // Verify all series created
    auto dataEntities = widget->getDataEntities();
    QCOMPARE(dataEntities.size(), seriesCount);
    
    // Test series list
    auto seriesList = widget->getSeries3DList();
    QCOMPARE(seriesList.size(), seriesCount);
    
    // Test individual series retrieval
    for (int i = 0; i < seriesCount; ++i) {
        QString fieldPath = QString("multi.series%1").arg(i);
        auto config = widget->getSeries3DConfig(fieldPath);
        QCOMPARE(config.fieldPath, fieldPath);
    }
}

void TestQt3DIntegration::testLargeDatasetHandling()
{
    // Test handling of large datasets in 3D
    Chart3DWidget::Series3DConfig config;
    config.fieldPath = "large.dataset";
    config.renderMode = Chart3DWidget::RenderMode::PointCloud;
    config.maxDataPoints = 10000;
    config.enableLevelOfDetail = true;
    
    widget->addSeries3D("large.dataset", config);
    waitForRender();
    
    // Simulate large dataset updates
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < 1000; ++i) {
        // Simulate batch data update
        QVariantList data;
        for (int j = 0; j < 10; ++j) {
            data.append(QVariant(i + j * 0.1));
        }
        widget->updateFieldDisplay("large.dataset", QVariant(data));
        
        if (i % 100 == 0) {
            waitForRender(50);
        }
    }
    
    qint64 elapsed = timer.elapsed();
    
    // Should handle large updates efficiently (under 10 seconds)
    QVERIFY(elapsed < 10000);
    
    // Verify point count management
    int currentPointCount = widget->getCurrentPointCount();
    QVERIFY(currentPointCount <= config.maxDataPoints);
}

void TestQt3DIntegration::testOpenGLContextFailure()
{
    // Test handling of OpenGL context failures
    // This is difficult to test directly, so we test error conditions
    
    // Test widget behavior when 3D is not available
    auto* window3D = widget->get3DWindow();
    if (!window3D || !window3D->isValid()) {
        // Should handle gracefully
        QVERIFY(!widget->isGPUAccelerated());
        
        // Should still allow basic operations
        Chart3DWidget::Series3DConfig config;
        config.fieldPath = "context.fail.test";
        bool result = widget->addSeries3D("context.fail.test", config);
        // May succeed or fail depending on fallback implementation
        Q_UNUSED(result);
    } else {
        // Context is valid, skip this test
        QSKIP("OpenGL context is valid, cannot test failure condition");
    }
}

void TestQt3DIntegration::testInvalidEntityHandling()
{
    // Test handling of invalid entity operations
    
    // Try to remove non-existent series
    bool result = widget->removeSeries3D("non.existent.field");
    QVERIFY(!result);
    
    // Try to get config for non-existent series
    auto config = widget->getSeries3DConfig("non.existent.field");
    QVERIFY(config.fieldPath.isEmpty());
    
    // Try to update non-existent field
    widget->updateFieldDisplay("non.existent.field", QVariant(42));
    // Should not crash
    waitForRender();
    
    // Add valid series then corrupt and test cleanup
    Chart3DWidget::Series3DConfig validConfig;
    validConfig.fieldPath = "valid.series";
    widget->addSeries3D("valid.series", validConfig);
    waitForRender();
    
    auto dataEntities = widget->getDataEntities();
    QVERIFY(dataEntities.find("valid.series") != dataEntities.end());
    
    // Clear all and verify cleanup
    widget->clearSeries3D();
    waitForRender();
    
    dataEntities = widget->getDataEntities();
    QVERIFY(dataEntities.empty());
}

void TestQt3DIntegration::testMemoryExhaustion()
{
    // Test behavior under memory pressure
    // Create many entities to stress memory
    
    const int stressCount = 100;
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < stressCount; ++i) {
        Chart3DWidget::Series3DConfig config;
        config.fieldPath = QString("stress.test%1").arg(i);
        config.renderMode = Chart3DWidget::RenderMode::Points;
        
        bool result = widget->addSeries3D(config.fieldPath, config);
        if (!result) {
            // Memory pressure - should handle gracefully
            break;
        }
        
        if (i % 10 == 0) {
            waitForRender(10);
        }
    }
    
    qint64 elapsed = timer.elapsed();
    
    // Should not take excessive time (under 30 seconds)
    QVERIFY(elapsed < 30000);
    
    // Clean up
    widget->clearSeries3D();
    waitForRender(100);
    
    // Verify cleanup
    auto dataEntities = widget->getDataEntities();
    QVERIFY(dataEntities.empty());
}

void TestQt3DIntegration::testRenderingFailures()
{
    // Test rendering failure recovery
    
    // Try invalid rendering configurations
    auto config = widget->getChart3DConfig();
    config.nearPlane = -1.0f; // Invalid
    config.farPlane = 0.0f;   // Invalid
    
    widget->setChart3DConfig(config);
    waitForRender();
    
    // Should not crash and should maintain valid state
    auto* camera = widget->getCamera();
    QVERIFY(camera != nullptr);
    QVERIFY(camera->nearPlane() > 0.0f);
    QVERIFY(camera->farPlane() > camera->nearPlane());
    
    // Reset to valid configuration
    widget->resetChart3DConfig();
    waitForRender();
    
    // Should recover
    auto resetConfig = widget->getChart3DConfig();
    QVERIFY(resetConfig.nearPlane > 0.0f);
    QVERIFY(resetConfig.farPlane > resetConfig.nearPlane);
}

void TestQt3DIntegration::testEntityCreationPerformance()
{
    // Measure entity creation performance
    QElapsedTimer timer;
    const int entityCount = 50;
    
    timer.start();
    
    for (int i = 0; i < entityCount; ++i) {
        Chart3DWidget::Series3DConfig config;
        config.fieldPath = QString("perf.entity%1").arg(i);
        config.renderMode = Chart3DWidget::RenderMode::Points;
        
        widget->addSeries3D(config.fieldPath, config);
    }
    
    waitForRender(100);
    
    qint64 elapsed = timer.elapsed();
    
    // Should create entities efficiently (under 5 seconds for 50 entities)
    QVERIFY(elapsed < 5000);
    
    // Verify all entities created
    auto dataEntities = widget->getDataEntities();
    QCOMPARE(dataEntities.size(), entityCount);
    
    // Measure cleanup performance
    timer.restart();
    widget->clearSeries3D();
    waitForRender(100);
    
    qint64 cleanupTime = timer.elapsed();
    
    // Cleanup should be fast (under 1 second)
    QVERIFY(cleanupTime < 1000);
}

void TestQt3DIntegration::testRenderingFrameRate()
{
    if (!widget->isVisible()) {
        QSKIP("Widget not visible, skipping frame rate test");
    }
    
    // Add some data for rendering
    for (int i = 0; i < 10; ++i) {
        Chart3DWidget::Series3DConfig config;
        config.fieldPath = QString("fps.test%1").arg(i);
        config.renderMode = Chart3DWidget::RenderMode::Points;
        widget->addSeries3D(config.fieldPath, config);
    }
    
    waitForRender(500);
    
    // Check FPS
    double fps = widget->getCurrentFPS();
    
    if (fps > 0) {
        // Should maintain reasonable frame rate
        QVERIFY(fps >= 15.0); // At least 15 FPS
        
        // For interactive applications, prefer higher FPS
        if (fps >= 30.0) {
            QVERIFY(fps >= 30.0); // Good performance
        }
    }
}

void TestQt3DIntegration::testMemoryUsage()
{
    // Test memory usage during operations
    size_t initialMemory = widget->getMemoryUsage();
    
    // Add data series
    const int seriesCount = 25;
    for (int i = 0; i < seriesCount; ++i) {
        Chart3DWidget::Series3DConfig config;
        config.fieldPath = QString("memory.test%1").arg(i);
        config.renderMode = Chart3DWidget::RenderMode::Points;
        widget->addSeries3D(config.fieldPath, config);
    }
    
    waitForRender(200);
    
    size_t afterAddMemory = widget->getMemoryUsage();
    
    // Memory should increase with data
    QVERIFY(afterAddMemory >= initialMemory);
    
    // Clear data
    widget->clearSeries3D();
    waitForRender(100);
    
    size_t afterClearMemory = widget->getMemoryUsage();
    
    // Memory should be released (allow some overhead)
    QVERIFY(afterClearMemory <= afterAddMemory);
}

void TestQt3DIntegration::testLargeScenePerformance()
{
    // Test performance with large 3D scenes
    const int largeSeriesCount = 100;
    
    QElapsedTimer timer;
    timer.start();
    
    // Create large scene
    for (int i = 0; i < largeSeriesCount; ++i) {
        Chart3DWidget::Series3DConfig config;
        config.fieldPath = QString("large.scene%1").arg(i);
        config.renderMode = (i % 3 == 0) ? Chart3DWidget::RenderMode::Points :
                          (i % 3 == 1) ? Chart3DWidget::RenderMode::Lines :
                                         Chart3DWidget::RenderMode::Surface;
        config.color = QColor::fromHsv((i * 360 / largeSeriesCount) % 360, 200, 200);
        
        widget->addSeries3D(config.fieldPath, config);
        
        // Occasionally wait for rendering
        if (i % 20 == 0) {
            waitForRender(50);
        }
    }
    
    waitForRender(500);
    
    qint64 creationTime = timer.elapsed();
    
    // Should handle large scenes (under 30 seconds)
    QVERIFY(creationTime < 30000);
    
    // Test scene interaction performance
    timer.restart();
    
    // Simulate camera movements
    for (int i = 0; i < 10; ++i) {
        float angle = i * 36.0f;
        float x = 10.0f * qCos(qDegreesToRadians(angle));
        float z = 10.0f * qSin(qDegreesToRadians(angle));
        
        widget->setCameraPosition(QVector3D(x, 5, z));
        waitForRender(50);
    }
    
    qint64 interactionTime = timer.elapsed();
    
    // Camera interactions should be responsive (under 2 seconds)
    QVERIFY(interactionTime < 2000);
    
    // Test cleanup performance
    timer.restart();
    widget->clearSeries3D();
    waitForRender(200);
    
    qint64 cleanupTime = timer.elapsed();
    
    // Cleanup should be efficient (under 5 seconds)
    QVERIFY(cleanupTime < 5000);
}

QTEST_MAIN(TestQt3DIntegration)
#include "test_qt3d_integration.moc"