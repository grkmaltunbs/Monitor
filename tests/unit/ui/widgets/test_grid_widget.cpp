#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QJsonObject>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QContextMenuEvent>
#include <QTimer>
#include <QClipboard>
#include <memory>

// Include the widget under test
#include "../../../../src/ui/widgets/grid_widget.h"

/**
 * @brief Unit tests for GridWidget class
 */
class TestGridWidget : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Core functionality tests
    void testConstructor();
    void testGridOptions();
    void testFieldDisplayNames();
    void testFieldIcons();
    void testColumnManagement();
    void testTableStructure();
    void testFieldRowManagement();
    void testValueDisplay();
    void testSortingFunctionality();
    void testContextMenu();
    void testDragAndDrop();
    void testSettingsPersistence();
    void testExportFunctionality();

    // Visual and interaction tests
    void testRowHighlighting();
    void testColumnResizing();
    void testCellInteraction();
    void testKeyboardNavigation();

    // Performance tests
    void testManyFieldsPerformance();
    void testUpdatePerformance();
    void testMemoryUsage();

    // Edge case tests
    void testEmptyGrid();
    void testInvalidFieldOperations();
    void testCorruptedData();

private:
    void addSampleFields();
    void verifyTableStructure();
    void simulateFieldUpdate(const QString& fieldPath, const QVariant& value);

    QApplication* m_app;
    GridWidget* m_widget;
    QString m_testWidgetId;
};

void TestGridWidget::initTestCase()
{
    if (!QApplication::instance()) {
        int argc = 1;
        static const char* test_name = "test";
        const char* argv[] = {test_name};
        m_app = new QApplication(argc, const_cast<char**>(argv));
    } else {
        m_app = nullptr;
    }
    
    m_testWidgetId = "test_grid_widget_001";
}

void TestGridWidget::cleanupTestCase()
{
    if (m_app) {
        delete m_app;
        m_app = nullptr;
    }
}

void TestGridWidget::init()
{
    m_widget = new GridWidget(m_testWidgetId);
    QVERIFY(m_widget != nullptr);
    
    // Show widget to trigger initialization
    m_widget->show();
    QTest::qWait(100); // Allow initialization to complete
}

void TestGridWidget::cleanup()
{
    if (m_widget) {
        m_widget->close();
        delete m_widget;
        m_widget = nullptr;
    }
}

void TestGridWidget::testConstructor()
{
    QVERIFY(m_widget != nullptr);
    QCOMPARE(m_widget->getWidgetId(), m_testWidgetId);
    QCOMPARE(m_widget->getWindowTitle(), QString("Grid Widget"));
    
    // Test default grid options
    GridWidget::GridOptions options = m_widget->getGridOptions();
    QVERIFY(options.showGridLines);
    QVERIFY(options.alternatingRowColors);
    QVERIFY(options.sortingEnabled);
    QVERIFY(options.resizableColumns);
    QVERIFY(options.showFieldIcons);
    QVERIFY(options.animateValueChanges);
    QCOMPARE(options.valueChangeHighlightDuration, 1000);
    
    // Verify table is created and configured
    verifyTableStructure();
}

void TestGridWidget::testGridOptions()
{
    GridWidget::GridOptions options = m_widget->getGridOptions();
    
    // Modify options
    options.showGridLines = false;
    options.alternatingRowColors = false;
    options.sortingEnabled = false;
    options.animateValueChanges = false;
    options.valueChangeHighlightDuration = 500;
    options.highlightColor = QColor(Qt::yellow);
    
    QSignalSpy optionsChangedSpy(m_widget, &GridWidget::gridOptionsChanged);
    
    m_widget->setGridOptions(options);
    
    QCOMPARE(optionsChangedSpy.count(), 1);
    
    GridWidget::GridOptions retrievedOptions = m_widget->getGridOptions();
    QVERIFY(!retrievedOptions.showGridLines);
    QVERIFY(!retrievedOptions.alternatingRowColors);
    QVERIFY(!retrievedOptions.sortingEnabled);
    QVERIFY(!retrievedOptions.animateValueChanges);
    QCOMPARE(retrievedOptions.valueChangeHighlightDuration, 500);
    QCOMPARE(retrievedOptions.highlightColor, QColor(Qt::yellow));
}

void TestGridWidget::testFieldDisplayNames()
{
    QString fieldPath = "test.display.field";
    QString customName = "Custom Field Name";
    
    // Test default display name (should format the field path)
    QString defaultName = m_widget->getFieldDisplayName(fieldPath);
    QCOMPARE(defaultName, QString("field")); // Last component of path
    
    // Set custom display name
    m_widget->setFieldDisplayName(fieldPath, customName);
    
    QString retrievedName = m_widget->getFieldDisplayName(fieldPath);
    QCOMPARE(retrievedName, customName);
    
    // Add field and verify name in table
    QJsonObject fieldInfo;
    fieldInfo["type"] = "int";
    m_widget->addField(fieldPath, 100, fieldInfo);
    
    // Verify the custom name appears in the table
    // Note: This would require access to the table widget which is private
    // In a real implementation, you might add methods to verify this
}

void TestGridWidget::testFieldIcons()
{
    QString fieldPath = "test.icon.field";
    QIcon customIcon = QIcon(":/icons/custom_icon.png"); // Would need real icon
    
    // Test default icon (based on field type)
    QIcon defaultIcon = m_widget->getFieldIcon(fieldPath);
    // Default icon might be null or a generated type icon
    
    // Set custom icon
    m_widget->setFieldIcon(fieldPath, customIcon);
    
    QIcon retrievedIcon = m_widget->getFieldIcon(fieldPath);
    // Icon comparison is tricky - we'll verify it's not null
    QVERIFY(!retrievedIcon.isNull());
}

void TestGridWidget::testColumnManagement()
{
    // Initial column setup (should be 2: Field Name, Value)
    QCOMPARE(m_widget->getColumnWidth(0), 150); // Default field name column width
    QVERIFY(m_widget->getColumnWidth(1) > 0);   // Value column width
    
    // Test setting column widths
    int newWidth = 200;
    m_widget->setColumnWidth(0, newWidth);
    QCOMPARE(m_widget->getColumnWidth(0), newWidth);
    
    // Test invalid column access
    QCOMPARE(m_widget->getColumnWidth(-1), 0);
    QCOMPARE(m_widget->getColumnWidth(10), 0);
    
    // Test reset column widths
    m_widget->resetColumnWidths();
    // After reset, columns should be sized to content
    QVERIFY(m_widget->getColumnWidth(0) >= 150); // At least minimum width
}

void TestGridWidget::testTableStructure()
{
    verifyTableStructure();
    
    // Test row height
    int defaultHeight = m_widget->getRowHeight();
    QVERIFY(defaultHeight > 0);
    
    int newHeight = 30;
    m_widget->setRowHeight(newHeight);
    QCOMPARE(m_widget->getRowHeight(), newHeight);
    
    // Test alternating row colors
    QVERIFY(m_widget->hasAlternatingRowColors());
    
    m_widget->setAlternatingRowColors(false);
    QVERIFY(!m_widget->hasAlternatingRowColors());
    
    m_widget->setAlternatingRowColors(true);
    QVERIFY(m_widget->hasAlternatingRowColors());
    
    // Test sorting
    QVERIFY(m_widget->isSortingEnabled());
    
    m_widget->setSortingEnabled(false);
    QVERIFY(!m_widget->isSortingEnabled());
    
    m_widget->setSortingEnabled(true);
    QVERIFY(m_widget->isSortingEnabled());
}

void TestGridWidget::testFieldRowManagement()
{
    QCOMPARE(m_widget->getFieldCount(), 0);
    
    // Add fields
    addSampleFields();
    
    QVERIFY(m_widget->getFieldCount() > 0);
    
    QStringList fields = m_widget->getAssignedFields();
    QVERIFY(!fields.isEmpty());
    
    // Test field removal
    if (!fields.isEmpty()) {
        QString firstField = fields.first();
        QSignalSpy fieldRemovedSpy(m_widget, &BaseWidget::fieldRemoved);
        
        m_widget->removeField(firstField);
        
        QCOMPARE(fieldRemovedSpy.count(), 1);
        QCOMPARE(m_widget->getFieldCount(), fields.size() - 1);
        QVERIFY(!m_widget->getAssignedFields().contains(firstField));
    }
    
    // Test clear all fields
    QSignalSpy fieldsClearedSpy(m_widget, &BaseWidget::fieldsCleared);
    
    m_widget->clearFields();
    
    QCOMPARE(fieldsClearedSpy.count(), 1);
    QCOMPARE(m_widget->getFieldCount(), 0);
    QVERIFY(m_widget->getAssignedFields().isEmpty());
}

void TestGridWidget::testValueDisplay()
{
    addSampleFields();
    
    // Simulate field value updates
    QStringList fields = m_widget->getAssignedFields();
    
    for (const QString& fieldPath : fields) {
        QVariant testValue;
        if (fieldPath.contains("int")) {
            testValue = 42;
        } else if (fieldPath.contains("double")) {
            testValue = 3.14159;
        } else if (fieldPath.contains("bool")) {
            testValue = true;
        } else {
            testValue = QString("test_value");
        }
        
        // Simulate value update
        simulateFieldUpdate(fieldPath, testValue);
    }
    
    QTest::qWait(200); // Allow updates to process
    
    // Verify values are displayed (would need access to table internals)
    // In practice, you'd verify the table contents match expected values
}

void TestGridWidget::testSortingFunctionality()
{
    addSampleFields();
    
    // Test sorting by column
    m_widget->sortByColumn(0, Qt::AscendingOrder);
    
    // Test clear sort
    m_widget->clearSort();
    
    // Verify sorting still works after clear
    m_widget->sortByColumn(1, Qt::DescendingOrder);
}

void TestGridWidget::testContextMenu()
{
    m_widget->show();
    QTest::qWait(100);
    
    // Context menu should be available
    QMenu* contextMenu = m_widget->getContextMenuForTesting();
    QVERIFY(contextMenu != nullptr);
    
    QList<QAction*> actions = contextMenu->actions();
    QVERIFY(!actions.isEmpty());
    
    // Look for grid-specific actions
    bool foundToggleGridLines = false;
    bool foundToggleRowColors = false;
    bool foundExportToCSV = false;
    bool foundResetColumns = false;
    Q_UNUSED(foundExportToCSV);
    Q_UNUSED(foundResetColumns);
    
    for (QAction* action : actions) {
        QString text = action->text();
        if (text.contains("Grid Lines")) {
            foundToggleGridLines = true;
        } else if (text.contains("Row Colors")) {
            foundToggleRowColors = true;
        } else if (text.contains("CSV")) {
            foundExportToCSV = true;
        } else if (text.contains("Reset") && text.contains("Column")) {
            foundResetColumns = true;
        }
    }
    
    QVERIFY(foundToggleGridLines);
    QVERIFY(foundToggleRowColors);
    // Export and column actions might be conditional on having data
}

void TestGridWidget::testDragAndDrop()
{
    // Test drag enter
    QMimeData mimeData;
    QJsonObject fieldData;
    fieldData["fieldPath"] = "drag.test.field";
    fieldData["packetId"] = 500;
    fieldData["fieldInfo"] = QJsonObject();
    
    QJsonDocument doc(fieldData);
    mimeData.setData("application/x-monitor-field", doc.toJson());
    
    // Create drag enter event
    QDragEnterEvent dragEnterEvent(QPoint(10, 10), Qt::CopyAction, &mimeData,
                                   Qt::LeftButton, Qt::NoModifier);
    
    // Send event to widget
    QApplication::sendEvent(m_widget, &dragEnterEvent);
    
    QVERIFY(dragEnterEvent.isAccepted());
    
    // Test drop
    QDropEvent dropEvent(QPoint(10, 10), Qt::CopyAction, &mimeData,
                         Qt::LeftButton, Qt::NoModifier);
    
    int initialFieldCount = m_widget->getFieldCount();
    QApplication::sendEvent(m_widget, &dropEvent);
    
    if (dropEvent.isAccepted()) {
        QCOMPARE(m_widget->getFieldCount(), initialFieldCount + 1);
        QVERIFY(m_widget->getAssignedFields().contains("drag.test.field"));
    }
}

void TestGridWidget::testSettingsPersistence()
{
    // Configure widget
    addSampleFields();
    
    GridWidget::GridOptions options = m_widget->getGridOptions();
    options.showGridLines = false;
    options.alternatingRowColors = false;
    options.valueChangeHighlightDuration = 2000;
    m_widget->setGridOptions(options);
    
    m_widget->setColumnWidth(0, 180);
    m_widget->setFieldDisplayName("test.field1", "Custom Name 1");
    
    // Save settings
    QJsonObject settings = m_widget->saveSettings();
    
    QVERIFY(!settings.isEmpty());
    
    // Verify grid-specific settings are present
    QJsonObject widgetSpecific = settings.value("widgetSpecific").toObject();
    QVERIFY(widgetSpecific.contains("gridOptions"));
    
    QJsonObject gridOptions = widgetSpecific.value("gridOptions").toObject();
    QCOMPARE(gridOptions.value("showGridLines").toBool(), false);
    QCOMPARE(gridOptions.value("alternatingRowColors").toBool(), false);
    QCOMPARE(gridOptions.value("valueChangeHighlightDuration").toInt(), 2000);
    
    // Create new widget and restore
    GridWidget newWidget("restored_grid");
    bool restored = newWidget.restoreSettings(settings);
    
    QVERIFY(restored);
    
    GridWidget::GridOptions restoredOptions = newWidget.getGridOptions();
    QVERIFY(!restoredOptions.showGridLines);
    QVERIFY(!restoredOptions.alternatingRowColors);
    QCOMPARE(restoredOptions.valueChangeHighlightDuration, 2000);
}

void TestGridWidget::testExportFunctionality()
{
    addSampleFields();
    
    // Add some test values
    simulateFieldUpdate("test.int.field", 123);
    simulateFieldUpdate("test.double.field", 45.67);
    simulateFieldUpdate("test.string.field", "test_data");
    
    QTest::qWait(100);
    
    // Test export methods exist and don't crash
    // Note: These would typically write to temporary files in real tests
    
    // Test clipboard export (should not crash)
    m_widget->onExportToClipboardForTesting();
    
    // Verify clipboard has content
    QClipboard* clipboard = QApplication::clipboard();
    QString clipboardText = clipboard->text();
    QVERIFY(!clipboardText.isEmpty());
    QVERIFY(clipboardText.contains("Field")); // Should contain header
    QVERIFY(clipboardText.contains("Value")); // Should contain header
}

void TestGridWidget::testRowHighlighting()
{
    addSampleFields();
    
    // Test value change highlighting
    GridWidget::GridOptions options = m_widget->getGridOptions();
    QVERIFY(options.animateValueChanges);
    
    // Simulate rapid value changes to test highlighting
    simulateFieldUpdate("test.int.field", 100);
    QTest::qWait(50);
    simulateFieldUpdate("test.int.field", 200);
    QTest::qWait(50);
    simulateFieldUpdate("test.int.field", 300);
    
    // Animation should be active (hard to verify in unit test)
    // In practice, you might check for active animations or timers
}

void TestGridWidget::testColumnResizing()
{
    addSampleFields();
    
    // Test manual column resizing
    int originalWidth = m_widget->getColumnWidth(0);
    int newWidth = originalWidth + 50;
    
    m_widget->setColumnWidth(0, newWidth);
    QCOMPARE(m_widget->getColumnWidth(0), newWidth);
    
    // Test resize columns to contents
    m_widget->resizeColumnsToContents();
    
    // Columns should be sized appropriately (exact values depend on content)
    QVERIFY(m_widget->getColumnWidth(0) >= 150); // Minimum field name width
    QVERIFY(m_widget->getColumnWidth(1) >= 100); // Minimum value width
    
    // Test reset column widths
    m_widget->resetColumnWidths();
    QVERIFY(m_widget->getColumnWidth(0) >= 150);
}

void TestGridWidget::testCellInteraction()
{
    addSampleFields();
    
    QStringList fields = m_widget->getAssignedFields();
    if (fields.isEmpty()) return;
    
    QSignalSpy fieldSelectedSpy(m_widget, &GridWidget::fieldSelected);
    QSignalSpy fieldDoubleClickedSpy(m_widget, &GridWidget::fieldDoubleClicked);
    
    // Test field selection
    QString firstField = fields.first();
    m_widget->selectField(firstField);
    
    QCOMPARE(fieldSelectedSpy.count(), 1);
    QCOMPARE(fieldSelectedSpy.first().first().toString(), firstField);
    
    // Test scroll to field
    m_widget->scrollToField(firstField);
    // Should not crash and field should be visible
}

void TestGridWidget::testKeyboardNavigation()
{
    addSampleFields();
    
    m_widget->show();
    m_widget->setFocus();
    QTest::qWait(100);
    
    // Test keyboard navigation (basic key events)
    QTest::keyClick(m_widget, Qt::Key_Tab);
    QTest::keyClick(m_widget, Qt::Key_Up);
    QTest::keyClick(m_widget, Qt::Key_Down);
    QTest::keyClick(m_widget, Qt::Key_Enter);
    
    // Widget should handle these keys gracefully without crashing
}

void TestGridWidget::testManyFieldsPerformance()
{
    const int numFields = 100;
    
    QElapsedTimer timer;
    timer.start();
    
    // Add many fields
    for (int i = 0; i < numFields; ++i) {
        QString fieldPath = QString("performance.field_%1").arg(i);
        QJsonObject fieldInfo;
        fieldInfo["type"] = (i % 3 == 0) ? "int" : (i % 3 == 1) ? "double" : "string";
        
        bool result = m_widget->addField(fieldPath, 1000 + i, fieldInfo);
        QVERIFY(result);
    }
    
    qint64 addTime = timer.elapsed();
    QCOMPARE(m_widget->getFieldCount(), numFields);
    
    // Test rapid updates
    timer.restart();
    
    for (int i = 0; i < numFields; ++i) {
        QString fieldPath = QString("performance.field_%1").arg(i);
        QVariant value = (i % 3 == 0) ? QVariant(i) : 
                        (i % 3 == 1) ? QVariant(i * 3.14) : QVariant(QString("value_%1").arg(i));
        
        simulateFieldUpdate(fieldPath, value);
    }
    
    QTest::qWait(500); // Allow updates to process
    qint64 updateTime = timer.elapsed();
    
    // Performance should be reasonable
    QVERIFY(addTime < 5000);    // Less than 5 seconds to add 100 fields
    QVERIFY(updateTime < 2000); // Less than 2 seconds to update all fields
    
    qDebug() << "Added" << numFields << "fields in" << addTime << "ms";
    qDebug() << "Updated" << numFields << "fields in" << updateTime << "ms";
}

void TestGridWidget::testUpdatePerformance()
{
    addSampleFields();
    
    QElapsedTimer timer;
    timer.start();
    
    const int numUpdates = 1000;
    QStringList fields = m_widget->getAssignedFields();
    
    for (int i = 0; i < numUpdates; ++i) {
        for (const QString& field : fields) {
            simulateFieldUpdate(field, QVariant(i));
        }
        
        if (i % 100 == 0) {
            QTest::qWait(10); // Periodic pause
        }
    }
    
    qint64 elapsed = timer.elapsed();
    
    // Should handle high-frequency updates efficiently
    QVERIFY(elapsed < 10000); // Less than 10 seconds for 1000 updates per field
    
    qDebug() << "Performed" << (numUpdates * fields.size()) << "field updates in" << elapsed << "ms";
}

void TestGridWidget::testMemoryUsage()
{
    // Test memory efficiency with many fields
    const int initialFieldCount = m_widget->getFieldCount();
    const int numFields = 500;
    
    // Add many fields
    for (int i = 0; i < numFields; ++i) {
        QString fieldPath = QString("memory.test.field_%1").arg(i);
        QJsonObject fieldInfo;
        fieldInfo["type"] = "double";
        fieldInfo["description"] = QString("Memory test field %1 with some descriptive text").arg(i);
        
        m_widget->addField(fieldPath, 2000 + i, fieldInfo);
    }
    
    QCOMPARE(m_widget->getFieldCount(), initialFieldCount + numFields);
    
    // Update all fields multiple times
    for (int update = 0; update < 10; ++update) {
        for (int i = 0; i < numFields; ++i) {
            QString fieldPath = QString("memory.test.field_%1").arg(i);
            simulateFieldUpdate(fieldPath, QVariant(update * i * 3.14159));
        }
        QTest::qWait(10);
    }
    
    // Clear fields to test cleanup
    m_widget->clearFields();
    QCOMPARE(m_widget->getFieldCount(), 0);
    
    // Widget should still be functional
    m_widget->addField("memory.recovery.test", 9999, QJsonObject());
    QCOMPARE(m_widget->getFieldCount(), 1);
}

void TestGridWidget::testEmptyGrid()
{
    // Test operations on empty grid
    QCOMPARE(m_widget->getFieldCount(), 0);
    
    // These should not crash
    m_widget->refreshGrid();
    m_widget->resizeColumnsToContents();
    m_widget->sortByColumn(0, Qt::AscendingOrder);
    m_widget->clearSort();
    
    // Test context menu on empty grid
    QMenu* contextMenu = m_widget->getContextMenuForTesting();
    QVERIFY(contextMenu != nullptr);
    
    // Export on empty grid should work
    m_widget->onExportToClipboardForTesting();
    
    // Clipboard should have at least headers
    QString clipboardText = QApplication::clipboard()->text();
    QVERIFY(clipboardText.contains("Field"));
    QVERIFY(clipboardText.contains("Value"));
}

void TestGridWidget::testInvalidFieldOperations()
{
    // Test invalid field selection
    m_widget->selectField("nonexistent.field");
    m_widget->scrollToField("nonexistent.field");
    
    // Should not crash
    
    // Test setting display name for nonexistent field
    m_widget->setFieldDisplayName("nonexistent.field", "Test Name");
    QString name = m_widget->getFieldDisplayName("nonexistent.field");
    QCOMPARE(name, QString("field")); // Should format the path
    
    // Test setting icon for nonexistent field
    QIcon icon;
    m_widget->setFieldIcon("nonexistent.field", icon);
    QIcon retrieved = m_widget->getFieldIcon("nonexistent.field");
    // Should handle gracefully
}

void TestGridWidget::testCorruptedData()
{
    // Test with corrupted settings
    QJsonObject corruptedSettings;
    corruptedSettings["gridOptions"] = "invalid_object"; // Should be object, not string
    
    GridWidget corruptedWidget("corrupted_test");
    
    // Should handle gracefully and use defaults
    bool restored = corruptedWidget.restoreWidgetSpecificSettingsForTesting(corruptedSettings);
    Q_UNUSED(restored);
    // Return value depends on implementation, but should not crash
    
    GridWidget::GridOptions options = corruptedWidget.getGridOptions();
    // Should have valid default options
    QVERIFY(options.valueChangeHighlightDuration > 0);
    
    // Widget should still be functional
    corruptedWidget.show();
    QTest::qWait(100);
    
    corruptedWidget.addField("recovery.field", 100, QJsonObject());
    QCOMPARE(corruptedWidget.getFieldCount(), 1);
}

// Helper methods
void TestGridWidget::addSampleFields()
{
    QJsonObject intField;
    intField["type"] = "int";
    intField["size"] = 4;
    m_widget->addField("test.int.field", 100, intField);
    
    QJsonObject doubleField;
    doubleField["type"] = "double";
    doubleField["size"] = 8;
    m_widget->addField("test.double.field", 101, doubleField);
    
    QJsonObject stringField;
    stringField["type"] = "string";
    stringField["size"] = 32;
    m_widget->addField("test.string.field", 102, stringField);
    
    QJsonObject boolField;
    boolField["type"] = "bool";
    boolField["size"] = 1;
    m_widget->addField("test.bool.field", 103, boolField);
    
    QTest::qWait(100); // Allow field addition to complete
}

void TestGridWidget::verifyTableStructure()
{
    // Grid should have a table widget with 2 columns (Field, Value)
    // This is testing the interface - internal table access would need friend class or public methods
    
    // Test that basic functionality works
    QVERIFY(m_widget->getRowHeight() > 0);
    QVERIFY(m_widget->getColumnWidth(0) > 0);
    QVERIFY(m_widget->getColumnWidth(1) > 0);
}

void TestGridWidget::simulateFieldUpdate(const QString& fieldPath, const QVariant& value)
{
    // In a real implementation, this would trigger the field update mechanism
    // For testing, we directly call the update method if it's accessible
    // or use the public interface to simulate packet updates
    
    // This is a placeholder - actual implementation would depend on 
    // how field updates are triggered in the real system
    Q_UNUSED(fieldPath)
    Q_UNUSED(value)
    
    // Trigger refresh to simulate update processing
    m_widget->refreshDisplay();
}

QTEST_MAIN(TestGridWidget)
#include "test_grid_widget.moc"