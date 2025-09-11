#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMimeData>
#include <QTimer>
#include <memory>

// Include the widget under test
#include "../../../../src/ui/widgets/base_widget.h"
#include "../../../../src/packet/core/packet.h"
#include "../../../../src/packet/routing/subscription_manager.h"
#include "../../../../src/packet/processing/field_extractor.h"
#include "../../../../src/core/application.h"

/**
 * @brief Mock implementation of BaseWidget for testing
 */
class MockBaseWidget : public BaseWidget
{
    Q_OBJECT

public:
    explicit MockBaseWidget(const QString& widgetId, QWidget* parent = nullptr)
        : BaseWidget(widgetId, "Mock Widget", parent)
        , m_initializeCalled(false)
        , m_updateDisplayCalled(false)
        , m_fieldAddedCalled(false)
        , m_fieldRemovedCalled(false)
        , m_fieldsClearedCalled(false)
    {
    }

    // Test accessors
    bool isInitializeCalled() const { return m_initializeCalled; }
    bool isUpdateDisplayCalled() const { return m_updateDisplayCalled; }
    bool isFieldAddedCalled() const { return m_fieldAddedCalled; }
    bool isFieldRemovedCalled() const { return m_fieldRemovedCalled; }
    bool isFieldsClearedCalled() const { return m_fieldsClearedCalled; }
    
    void resetFlags() {
        m_initializeCalled = false;
        m_updateDisplayCalled = false;
        m_fieldAddedCalled = false;
        m_fieldRemovedCalled = false;
        m_fieldsClearedCalled = false;
    }

protected:
    // BaseWidget template method implementations
    void initializeWidget() override {
        m_initializeCalled = true;
    }

    void updateDisplay() override {
        m_updateDisplayCalled = true;
    }

    void handleFieldAdded(const FieldAssignment& field) override {
        Q_UNUSED(field)
        m_fieldAddedCalled = true;
    }

    void handleFieldRemoved(const QString& fieldPath) override {
        Q_UNUSED(fieldPath)
        m_fieldRemovedCalled = true;
    }

    void handleFieldsCleared() override {
        m_fieldsClearedCalled = true;
    }

    // Settings implementation
    QJsonObject saveWidgetSpecificSettings() const override {
        QJsonObject settings;
        settings["mockData"] = "test";
        return settings;
    }

    bool restoreWidgetSpecificSettings(const QJsonObject& settings) override {
        return settings.contains("mockData");
    }

    // Context menu implementation
    void setupContextMenu() override {
        // Mock implementation
    }

public:
    // Public wrapper methods for testing protected members
    bool testCanAcceptDrop(const QMimeData* mimeData) {
        return canAcceptDrop(mimeData);
    }
    
    bool testProcessDrop(const QMimeData* mimeData) {
        return processDrop(mimeData);
    }
    
    QMenu* testGetContextMenu() {
        return getContextMenu();
    }

private:
    bool m_initializeCalled;
    bool m_updateDisplayCalled;
    bool m_fieldAddedCalled;
    bool m_fieldRemovedCalled;
    bool m_fieldsClearedCalled;
};

/**
 * @brief Unit tests for BaseWidget class
 */
class TestBaseWidget : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Core functionality tests
    void testConstructor();
    void testWidgetIdentity();
    void testWindowTitle();
    void testFieldManagement();
    void testFieldAssignmentValidation();
    void testDragAndDrop();
    void testSubscriptionManagement();
    void testUpdateThrottling();
    void testStatistics();
    void testSettingsPersistence();
    void testLifecycle();
    void testContextMenu();

    // Performance tests
    void testFieldAdditionPerformance();
    void testUpdatePerformance();
    void testMemoryUsage();

    // Error handling tests
    void testInvalidFieldHandling();
    void testNullPointerHandling();
    void testCorruptedSettings();

private:
    QApplication* m_app;
    MockBaseWidget* m_widget;
    QString m_testWidgetId;
};

void TestBaseWidget::initTestCase()
{
    // Initialize application if not already done
    if (!QApplication::instance()) {
        int argc = 1;
        const char* argv_str[] = {"test"};
        char** argv = const_cast<char**>(argv_str);
        m_app = new QApplication(argc, argv);
    } else {
        m_app = nullptr;
    }
    
    m_testWidgetId = "test_widget_001";
}

void TestBaseWidget::cleanupTestCase()
{
    if (m_app) {
        delete m_app;
        m_app = nullptr;
    }
}

void TestBaseWidget::init()
{
    // Create fresh widget for each test
    m_widget = new MockBaseWidget(m_testWidgetId);
    QVERIFY(m_widget != nullptr);
}

void TestBaseWidget::cleanup()
{
    // Clean up after each test
    if (m_widget) {
        delete m_widget;
        m_widget = nullptr;
    }
}

void TestBaseWidget::testConstructor()
{
    QVERIFY(m_widget != nullptr);
    QCOMPARE(m_widget->getWidgetId(), m_testWidgetId);
    QCOMPARE(m_widget->getWindowTitle(), QString("Mock Widget"));
    QVERIFY(m_widget->getFieldCount() == 0);
    QVERIFY(m_widget->isUpdateEnabled());
    QCOMPARE(m_widget->getMaxUpdateRate(), 60); // Default 60 FPS
}

void TestBaseWidget::testWidgetIdentity()
{
    QString widgetId = "custom_widget_123";
    QString windowTitle = "Custom Test Widget";
    
    MockBaseWidget customWidget(widgetId, nullptr);
    customWidget.setWindowTitle(windowTitle);
    
    QCOMPARE(customWidget.getWidgetId(), widgetId);
    QCOMPARE(customWidget.getWindowTitle(), windowTitle);
}

void TestBaseWidget::testWindowTitle()
{
    QString originalTitle = m_widget->getWindowTitle();
    QString newTitle = "Updated Test Widget";
    
    QSignalSpy titleChangedSpy(m_widget, &QWidget::windowTitleChanged);
    
    m_widget->setWindowTitle(newTitle);
    
    QCOMPARE(m_widget->getWindowTitle(), newTitle);
    QCOMPARE(titleChangedSpy.count(), 1);
    
    // Test setting same title (should not emit signal)
    m_widget->setWindowTitle(newTitle);
    QCOMPARE(titleChangedSpy.count(), 1);
}

void TestBaseWidget::testFieldManagement()
{
    QCOMPARE(m_widget->getFieldCount(), 0);
    QVERIFY(m_widget->getAssignedFields().isEmpty());
    
    // Add field
    QString fieldPath = "test.field1";
    Monitor::Packet::PacketId packetId = 100;
    QJsonObject fieldInfo;
    fieldInfo["type"] = "int";
    fieldInfo["size"] = 4;
    
    QSignalSpy fieldAddedSpy(m_widget, &BaseWidget::fieldAdded);
    
    bool result = m_widget->addField(fieldPath, packetId, fieldInfo);
    
    QVERIFY(result);
    QCOMPARE(m_widget->getFieldCount(), 1);
    QVERIFY(m_widget->getAssignedFields().contains(fieldPath));
    QCOMPARE(fieldAddedSpy.count(), 1);
    QVERIFY(m_widget->isFieldAddedCalled());
    
    // Test duplicate field addition
    m_widget->resetFlags();
    result = m_widget->addField(fieldPath, packetId, fieldInfo);
    QVERIFY(!result); // Should fail for duplicate
    QCOMPARE(m_widget->getFieldCount(), 1);
    QVERIFY(!m_widget->isFieldAddedCalled());
    
    // Add second field
    QString fieldPath2 = "test.field2";
    result = m_widget->addField(fieldPath2, packetId + 1, fieldInfo);
    QVERIFY(result);
    QCOMPARE(m_widget->getFieldCount(), 2);
    
    // Remove field
    QSignalSpy fieldRemovedSpy(m_widget, &BaseWidget::fieldRemoved);
    
    m_widget->resetFlags();
    result = m_widget->removeField(fieldPath);
    
    QVERIFY(result);
    QCOMPARE(m_widget->getFieldCount(), 1);
    QVERIFY(!m_widget->getAssignedFields().contains(fieldPath));
    QCOMPARE(fieldRemovedSpy.count(), 1);
    QVERIFY(m_widget->isFieldRemovedCalled());
    
    // Clear all fields
    QSignalSpy fieldsClearedSpy(m_widget, &BaseWidget::fieldsCleared);
    
    m_widget->resetFlags();
    m_widget->clearFields();
    
    QCOMPARE(m_widget->getFieldCount(), 0);
    QVERIFY(m_widget->getAssignedFields().isEmpty());
    QCOMPARE(fieldsClearedSpy.count(), 1);
    QVERIFY(m_widget->isFieldsClearedCalled());
}

void TestBaseWidget::testFieldAssignmentValidation()
{
    // Test invalid field path
    bool result = m_widget->addField("", 100, QJsonObject());
    QVERIFY(!result);
    
    // Test invalid packet ID
    result = m_widget->addField("test.field", 0, QJsonObject());
    QVERIFY(!result);
    
    // Test valid field assignment
    QJsonObject fieldInfo;
    fieldInfo["type"] = "float";
    result = m_widget->addField("valid.field", 200, fieldInfo);
    QVERIFY(result);
}

void TestBaseWidget::testDragAndDrop()
{
    // Enable drops
    m_widget->setAcceptDrops(true);
    
    // Create mock MIME data
    QMimeData mimeData;
    
    // Test with invalid MIME data
    bool canAccept = m_widget->testCanAcceptDrop(&mimeData);
    QVERIFY(!canAccept);
    
    // Test with valid field MIME data
    QJsonObject fieldData;
    fieldData["fieldPath"] = "drag.test.field";
    fieldData["packetId"] = 300;
    fieldData["fieldInfo"] = QJsonObject();
    
    QJsonDocument doc(fieldData);
    mimeData.setData("application/x-monitor-field", doc.toJson());
    
    canAccept = m_widget->testCanAcceptDrop(&mimeData);
    QVERIFY(canAccept);
    
    // Test processing drop
    int initialCount = m_widget->getFieldCount();
    bool processed = m_widget->testProcessDrop(&mimeData);
    
    QVERIFY(processed);
    QCOMPARE(m_widget->getFieldCount(), initialCount + 1);
    QVERIFY(m_widget->getAssignedFields().contains("drag.test.field"));
}

void TestBaseWidget::testSubscriptionManagement()
{
    Monitor::Packet::PacketId packetId1 = 100;
    Monitor::Packet::PacketId packetId2 = 200;
    
    // Initially no subscriptions
    QVERIFY(m_widget->getSubscribedPackets().isEmpty());
    
    // Add field (should create subscription)
    QJsonObject fieldInfo;
    fieldInfo["type"] = "int";
    bool result = m_widget->addField("test.field1", packetId1, fieldInfo);
    QVERIFY(result);
    
    // Note: Actual subscription testing would require mocked SubscriptionManager
    // This tests the interface but not the full integration
    
    // Add field with different packet ID
    result = m_widget->addField("test.field2", packetId2, fieldInfo);
    QVERIFY(result);
    
    // Remove all fields (should clear subscriptions)
    m_widget->clearFields();
    QVERIFY(m_widget->getAssignedFields().isEmpty());
}

void TestBaseWidget::testUpdateThrottling()
{
    // Test update rate settings
    int originalRate = m_widget->getMaxUpdateRate();
    QCOMPARE(originalRate, 60);
    
    m_widget->setMaxUpdateRate(30);
    QCOMPARE(m_widget->getMaxUpdateRate(), 30);
    
    // Test rate clamping
    m_widget->setMaxUpdateRate(0); // Should clamp to minimum
    QVERIFY(m_widget->getMaxUpdateRate() >= 1);
    
    m_widget->setMaxUpdateRate(200); // Should clamp to maximum
    QVERIFY(m_widget->getMaxUpdateRate() <= 120);
    
    // Test enable/disable updates
    m_widget->setUpdateEnabled(false);
    QVERIFY(!m_widget->isUpdateEnabled());
    
    m_widget->setUpdateEnabled(true);
    QVERIFY(m_widget->isUpdateEnabled());
}

void TestBaseWidget::testStatistics()
{
    const BaseWidget::UpdateStatistics& stats = m_widget->getStatistics();
    
    // Initial statistics
    QCOMPARE(stats.packetsReceived.load(), static_cast<uint64_t>(0));
    QCOMPARE(stats.updatesSent.load(), static_cast<uint64_t>(0));
    
    // Reset statistics
    m_widget->resetStatistics();
    QCOMPARE(stats.packetsReceived.load(), static_cast<uint64_t>(0));
}

void TestBaseWidget::testSettingsPersistence()
{
    // Configure widget
    m_widget->setWindowTitle("Configured Widget");
    m_widget->setMaxUpdateRate(45);
    m_widget->setUpdateEnabled(false);
    
    // Add some fields
    QJsonObject fieldInfo;
    fieldInfo["type"] = "double";
    m_widget->addField("save.test.field", 150, fieldInfo);
    
    // Save settings
    QJsonObject settings = m_widget->saveSettings();
    
    QVERIFY(!settings.isEmpty());
    QCOMPARE(settings.value("widgetId").toString(), m_testWidgetId);
    QCOMPARE(settings.value("windowTitle").toString(), QString("Configured Widget"));
    QCOMPARE(settings.value("maxUpdateRate").toInt(), 45);
    QCOMPARE(settings.value("updateEnabled").toBool(), false);
    
    // Verify fields are saved
    QJsonArray fieldsArray = settings.value("fields").toArray();
    QCOMPARE(fieldsArray.size(), 1);
    
    // Create new widget and restore settings
    MockBaseWidget restoredWidget("restored_widget");
    bool restored = restoredWidget.restoreSettings(settings);
    
    QVERIFY(restored);
    QCOMPARE(restoredWidget.getWindowTitle(), QString("Configured Widget"));
    QCOMPARE(restoredWidget.getMaxUpdateRate(), 45);
    QVERIFY(!restoredWidget.isUpdateEnabled());
    QCOMPARE(restoredWidget.getFieldCount(), 1);
    QVERIFY(restoredWidget.getAssignedFields().contains("save.test.field"));
}

void TestBaseWidget::testLifecycle()
{
    // Test show event
    m_widget->show();
    QApplication::processEvents(); // Was: QTest::qWait(100) // Allow event processing
    
    QVERIFY(m_widget->isVisible());
    QVERIFY(m_widget->isInitializeCalled());
    
    // Test hide event
    m_widget->hide();
    QVERIFY(!m_widget->isVisible());
    
    // Test reset to defaults
    m_widget->setMaxUpdateRate(30);
    m_widget->addField("lifecycle.test", 100, QJsonObject());
    
    m_widget->resetToDefaults();
    
    QCOMPARE(m_widget->getMaxUpdateRate(), 60);
    QCOMPARE(m_widget->getFieldCount(), 0);
    QVERIFY(m_widget->isUpdateEnabled());
}

void TestBaseWidget::testContextMenu()
{
    // Context menu is set up during initialization
    m_widget->show();
    QApplication::processEvents(); // Was: QTest::qWait(100)
    
    QMenu* contextMenu = m_widget->testGetContextMenu();
    QVERIFY(contextMenu != nullptr);
    
    // Test context menu actions exist
    QList<QAction*> actions = contextMenu->actions();
    QVERIFY(!actions.isEmpty());
    
    // Look for expected base actions
    bool foundSettings = false;
    bool foundClearFields = false;
    bool foundRefresh = false;
    
    for (QAction* action : actions) {
        if (action->text().contains("Settings")) {
            foundSettings = true;
        } else if (action->text().contains("Clear Fields")) {
            foundClearFields = true;
        } else if (action->text().contains("Refresh")) {
            foundRefresh = true;
        }
    }
    
    QVERIFY(foundSettings);
    QVERIFY(foundClearFields);
    QVERIFY(foundRefresh);
}

void TestBaseWidget::testFieldAdditionPerformance()
{
    const int numFields = 100;
    
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < numFields; ++i) {
        QString fieldPath = QString("performance.test.field%1").arg(i);
        QJsonObject fieldInfo;
        fieldInfo["type"] = "int";
        
        bool result = m_widget->addField(fieldPath, 1000 + i, fieldInfo);
        QVERIFY(result);
    }
    
    qint64 elapsed = timer.elapsed();
    
    QCOMPARE(m_widget->getFieldCount(), numFields);
    
    // Should complete in reasonable time (less than 1 second for 100 fields)
    QVERIFY(elapsed < 1000);
    
    qDebug() << "Added" << numFields << "fields in" << elapsed << "ms";
}

void TestBaseWidget::testUpdatePerformance()
{
    // Add some fields first
    for (int i = 0; i < 10; ++i) {
        QString fieldPath = QString("update.test.field%1").arg(i);
        m_widget->addField(fieldPath, 2000 + i, QJsonObject());
    }
    
    m_widget->show();
    QApplication::processEvents(); // Was: QTest::qWait(100)
    
    QElapsedTimer timer;
    timer.start();
    
    const int numUpdates = 100;
    for (int i = 0; i < numUpdates; ++i) {
        m_widget->refreshDisplay();
        QApplication::processEvents(); // Small delay between updates
    }
    
    qint64 elapsed = timer.elapsed();
    
    // Should handle updates efficiently
    QVERIFY(elapsed < 5000); // Less than 5 seconds for 100 updates
    
    qDebug() << "Performed" << numUpdates << "updates in" << elapsed << "ms";
}

void TestBaseWidget::testMemoryUsage()
{
    // This is a basic test - a full memory test would require external tools
    const int initialFieldCount = m_widget->getFieldCount();
    
    // Add many fields
    const int numFields = 1000;
    for (int i = 0; i < numFields; ++i) {
        QString fieldPath = QString("memory.test.field%1").arg(i);
        QJsonObject fieldInfo;
        fieldInfo["type"] = "double";
        fieldInfo["size"] = 8;
        
        m_widget->addField(fieldPath, 3000 + i, fieldInfo);
    }
    
    QCOMPARE(m_widget->getFieldCount(), initialFieldCount + numFields);
    
    // Clear fields
    m_widget->clearFields();
    QCOMPARE(m_widget->getFieldCount(), 0);
    
    // Widget should still be functional
    bool result = m_widget->addField("memory.final.test", 9999, QJsonObject());
    QVERIFY(result);
    QCOMPARE(m_widget->getFieldCount(), 1);
}

void TestBaseWidget::testInvalidFieldHandling()
{
    // Test null/empty field paths
    QVERIFY(!m_widget->addField("", 100, QJsonObject()));
    QVERIFY(!m_widget->addField(QString(), 100, QJsonObject()));
    
    // Test invalid packet IDs
    QVERIFY(!m_widget->addField("test.field", 0, QJsonObject()));
    
    // Test removing non-existent field
    QVERIFY(!m_widget->removeField("non.existent.field"));
    
    // Test with valid field first
    m_widget->addField("valid.field", 100, QJsonObject());
    QCOMPARE(m_widget->getFieldCount(), 1);
    
    // Test invalid operations don't affect valid fields
    m_widget->addField("", 200, QJsonObject());
    QCOMPARE(m_widget->getFieldCount(), 1); // Should remain unchanged
}

void TestBaseWidget::testNullPointerHandling()
{
    // Test with null MIME data
    bool canAccept = m_widget->testCanAcceptDrop(nullptr);
    QVERIFY(!canAccept);
    
    bool processed = m_widget->testProcessDrop(nullptr);
    QVERIFY(!processed);
    
    // Widget should remain stable after null pointer operations
    QVERIFY(m_widget->addField("stable.field", 100, QJsonObject()));
    QCOMPARE(m_widget->getFieldCount(), 1);
}

void TestBaseWidget::testCorruptedSettings()
{
    // Test empty settings
    bool restored = m_widget->restoreSettings(QJsonObject());
    Q_UNUSED(restored) // Should handle gracefully (return value depends on implementation)
    
    // Test corrupted field data
    QJsonObject corruptedSettings;
    QJsonArray corruptedFields;
    
    QJsonObject badField1; // Missing required fields
    badField1["fieldPath"] = ""; // Invalid path
    corruptedFields.append(badField1);
    
    QJsonObject badField2; // Invalid packet ID
    badField2["fieldPath"] = "corrupt.field";
    badField2["packetId"] = 0;
    corruptedFields.append(badField2);
    
    corruptedSettings["fields"] = corruptedFields;
    corruptedSettings["widgetSpecific"] = QJsonObject(); // Valid but empty
    
    int initialFieldCount = m_widget->getFieldCount();
    restored = m_widget->restoreSettings(corruptedSettings);
    
    // Widget should handle corruption gracefully
    // Field count should not increase due to invalid fields
    QCOMPARE(m_widget->getFieldCount(), initialFieldCount);
    
    // Widget should still be functional
    QVERIFY(m_widget->addField("recovery.field", 500, QJsonObject()));
}

// Include the moc file for the test class
#include "test_base_widget.moc"

QTEST_MAIN(TestBaseWidget)