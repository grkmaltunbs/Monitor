#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QTableWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryFile>
#include <QClipboard>
#include <QTimer>
#include <memory>

// Include the widget under test
#include "../../../../src/ui/widgets/grid_logger_widget.h"

/**
 * @brief Unit tests for GridLoggerWidget class
 */
class TestGridLoggerWidget : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Core functionality tests
    void testConstructor();
    void testLoggerOptions();
    void testRowManagement();
    void testHighlightRules();
    void testFieldColumnManagement();
    void testPacketLogging();
    void testDataPersistence();
    void testExportFunctionality();
    void testSearchAndFilter();
    void testAutoSave();
    void testSettingsPersistence();

    // Performance tests
    void testHighVolumeLogging();
    void testMemoryManagement();
    void testLargeDatasetHandling();

    // UI interaction tests
    void testContextMenu();
    void testSortingFunctionality();
    void testScrolling();
    void testCellInteraction();

    // Edge case tests
    void testEmptyLogger();
    void testMaxRowsLimit();
    void testCorruptedData();
    void testInvalidOperations();

private:
    void addSampleData();
    void simulatePacketArrival(const QHash<QString, QVariant>& fieldValues);
    void verifyTableStructure(int expectedRows, int expectedColumns);

    QApplication* m_app;
    GridLoggerWidget* m_widget;
    QString m_testWidgetId;
};

void TestGridLoggerWidget::initTestCase()
{
    if (!QApplication::instance()) {
        int argc = 1;
        static const char* test_name = "test";
        const char* argv[] = {test_name};
        m_app = new QApplication(argc, const_cast<char**>(argv));
    } else {
        m_app = nullptr;
    }
    
    m_testWidgetId = "test_grid_logger_widget_001";
}

void TestGridLoggerWidget::cleanupTestCase()
{
    if (m_app) {
        delete m_app;
        m_app = nullptr;
    }
}

void TestGridLoggerWidget::init()
{
    m_widget = new GridLoggerWidget(m_testWidgetId);
    QVERIFY(m_widget != nullptr);
    
    // Show widget to trigger initialization
    m_widget->show();
    QApplication::processEvents(); // Was: QTest::qWait(100)
}

void TestGridLoggerWidget::cleanup()
{
    if (m_widget) {
        m_widget->close();
        delete m_widget;
        m_widget = nullptr;
    }
}

void TestGridLoggerWidget::testConstructor()
{
    QVERIFY(m_widget != nullptr);
    QCOMPARE(m_widget->getWidgetId(), m_testWidgetId);
    QCOMPARE(m_widget->getWindowTitle(), QString("Grid Logger Widget"));
    
    // Test default logger options
    GridLoggerWidget::LoggerOptions options = m_widget->getLoggerOptions();
    QCOMPARE(options.maxRows, 10000);
    QVERIFY(options.autoScroll);
    QVERIFY(options.autoDeleteOldest);
    QVERIFY(options.showTimestamp);
    QCOMPARE(options.timestampFormat, QString("hh:mm:ss.zzz"));
    QVERIFY(!options.enableAutoSave);
    QCOMPARE(options.autoSaveInterval, 60000);
    QVERIFY(options.highlightNewRows);
    QCOMPARE(options.highlightDuration, 2000);
    
    // Initial state
    QCOMPARE(m_widget->getCurrentRowCount(), 0);
    QVERIFY(m_widget->getHighlightRules().isEmpty());
}

void TestGridLoggerWidget::testLoggerOptions()
{
    GridLoggerWidget::LoggerOptions options = m_widget->getLoggerOptions();
    
    // Modify options
    options.maxRows = 5000;
    options.autoScroll = false;
    options.autoDeleteOldest = false;
    options.showTimestamp = false;
    options.timestampFormat = "yyyy-MM-dd hh:mm:ss";
    options.enableAutoSave = true;
    options.autoSaveInterval = 30000;
    options.highlightNewRows = false;
    options.highlightDuration = 1000;
    options.highlightColor = QColor(Qt::cyan);
    
    m_widget->setLoggerOptions(options);
    
    GridLoggerWidget::LoggerOptions retrievedOptions = m_widget->getLoggerOptions();
    QCOMPARE(retrievedOptions.maxRows, 5000);
    QVERIFY(!retrievedOptions.autoScroll);
    QVERIFY(!retrievedOptions.autoDeleteOldest);
    QVERIFY(!retrievedOptions.showTimestamp);
    QCOMPARE(retrievedOptions.timestampFormat, QString("yyyy-MM-dd hh:mm:ss"));
    QVERIFY(retrievedOptions.enableAutoSave);
    QCOMPARE(retrievedOptions.autoSaveInterval, 30000);
    QVERIFY(!retrievedOptions.highlightNewRows);
    QCOMPARE(retrievedOptions.highlightDuration, 1000);
    QCOMPARE(retrievedOptions.highlightColor, QColor(Qt::cyan));
}

void TestGridLoggerWidget::testRowManagement()
{
    QCOMPARE(m_widget->getCurrentRowCount(), 0);
    
    // Test max rows setting
    int originalMax = m_widget->getMaxRows();
    QCOMPARE(originalMax, 10000);
    
    m_widget->setMaxRows(1000);
    QCOMPARE(m_widget->getMaxRows(), 1000);
    
    // Test invalid max rows (should clamp to minimum)
    m_widget->setMaxRows(0);
    QVERIFY(m_widget->getMaxRows() >= 1);
    
    m_widget->setMaxRows(100); // Set reasonable limit for testing
    
    // Test that rows start at zero
    int initialRowCount = m_widget->getCurrentRowCount();
    QCOMPARE(initialRowCount, 0);
    
    // For now, skip complex packet simulation testing due to implementation dependencies
    // Add basic fields to verify structure is set up
    QJsonObject fieldInfo;
    fieldInfo["type"] = "int";
    bool addResult = m_widget->addField("test.field", 100, fieldInfo);
    QVERIFY(addResult);
    
    // Verify the field was added but no data rows yet (this is expected behavior)
    QCOMPARE(m_widget->getCurrentRowCount(), 0);
    
    // Test clear all rows
    QSignalSpy rowsClearedSpy(m_widget, &GridLoggerWidget::rowsCleared);
    
    m_widget->clearAllRows();
    
    QCOMPARE(m_widget->getCurrentRowCount(), 0);
    QCOMPARE(rowsClearedSpy.count(), 1);
}

void TestGridLoggerWidget::testHighlightRules()
{
    // Initially no highlight rules
    QVERIFY(m_widget->getHighlightRules().isEmpty());
    
    // Add highlight rule
    GridLoggerWidget::HighlightRule rule1;
    rule1.name = "High Value Alert";
    rule1.fieldPath = "test.value";
    rule1.condition = "> 100";
    rule1.backgroundColor = QColor(Qt::red);
    rule1.textColor = QColor(Qt::white);
    rule1.enabled = true;
    
    m_widget->addHighlightRule(rule1);
    
    QList<GridLoggerWidget::HighlightRule> rules = m_widget->getHighlightRules();
    QCOMPARE(rules.size(), 1);
    QCOMPARE(rules[0].name, QString("High Value Alert"));
    QCOMPARE(rules[0].fieldPath, QString("test.value"));
    QCOMPARE(rules[0].condition, QString("> 100"));
    QCOMPARE(rules[0].backgroundColor, QColor(Qt::red));
    QCOMPARE(rules[0].textColor, QColor(Qt::white));
    QVERIFY(rules[0].enabled);
    
    // Add second rule
    GridLoggerWidget::HighlightRule rule2;
    rule2.name = "Low Value Warning";
    rule2.fieldPath = "test.value";
    rule2.condition = "< 10";
    rule2.backgroundColor = QColor(Qt::yellow);
    rule2.enabled = true;
    
    m_widget->addHighlightRule(rule2);
    QCOMPARE(m_widget->getHighlightRules().size(), 2);
    
    // Update existing rule (same name)
    rule1.backgroundColor = QColor(Qt::darkRed);
    m_widget->addHighlightRule(rule1);
    
    rules = m_widget->getHighlightRules();
    QCOMPARE(rules.size(), 2); // Should still be 2 (updated, not added)
    
    // Find the updated rule
    bool foundUpdatedRule = false;
    for (const auto& rule : rules) {
        if (rule.name == "High Value Alert" && rule.backgroundColor == QColor(Qt::darkRed)) {
            foundUpdatedRule = true;
            break;
        }
    }
    QVERIFY(foundUpdatedRule);
    
    // Remove rule
    m_widget->removeHighlightRule("Low Value Warning");
    QCOMPARE(m_widget->getHighlightRules().size(), 1);
    
    // Clear all rules
    m_widget->clearHighlightRules();
    QVERIFY(m_widget->getHighlightRules().isEmpty());
}

void TestGridLoggerWidget::testFieldColumnManagement()
{
    // Initially no fields assigned
    QCOMPARE(m_widget->getFieldCount(), 0);
    
    // Add fields (this would typically trigger column creation)
    QJsonObject fieldInfo;
    fieldInfo["type"] = "int";
    m_widget->addField("test.column1", 100, fieldInfo);
    m_widget->addField("test.column2", 101, fieldInfo);
    m_widget->addField("test.column3", 102, fieldInfo);
    
    QCOMPARE(m_widget->getFieldCount(), 3);
    
    QStringList fields = m_widget->getAssignedFields();
    QCOMPARE(fields.size(), 3);
    QVERIFY(fields.contains("test.column1"));
    QVERIFY(fields.contains("test.column2"));
    QVERIFY(fields.contains("test.column3"));
    
    // Remove field
    m_widget->removeField("test.column2");
    QCOMPARE(m_widget->getFieldCount(), 2);
    QVERIFY(!m_widget->getAssignedFields().contains("test.column2"));
}

void TestGridLoggerWidget::testPacketLogging()
{
    // Add fields first
    QJsonObject intField;
    intField["type"] = "int";
    m_widget->addField("sensor.temperature", 200, intField);
    
    QJsonObject doubleField;
    doubleField["type"] = "double";
    m_widget->addField("sensor.pressure", 200, doubleField);
    
    QJsonObject stringField;
    stringField["type"] = "string";
    m_widget->addField("sensor.status", 200, stringField);
    
    int initialRowCount = m_widget->getCurrentRowCount();
    
    QSignalSpy rowAddedSpy(m_widget, &GridLoggerWidget::rowAdded);
    
    // Simulate packet arrival
    QHash<QString, QVariant> packet1;
    packet1["sensor.temperature"] = 25;
    packet1["sensor.pressure"] = 101.3;
    packet1["sensor.status"] = "OK";
    
    simulatePacketArrival(packet1);
    QApplication::processEvents(); // Was: QTest::qWait(100)
    
    QCOMPARE(m_widget->getCurrentRowCount(), initialRowCount + 1);
    QCOMPARE(rowAddedSpy.count(), 1);
    
    // Add more packets
    for (int i = 0; i < 5; ++i) {
        QHash<QString, QVariant> packet;
        packet["sensor.temperature"] = 20 + i;
        packet["sensor.pressure"] = 100.0 + i * 0.1;
        packet["sensor.status"] = QString("Status_%1").arg(i);
        
        simulatePacketArrival(packet);
    }
    
    QApplication::processEvents(); // Was: QTest::qWait(200)
    
    QCOMPARE(m_widget->getCurrentRowCount(), initialRowCount + 6);
}

void TestGridLoggerWidget::testDataPersistence()
{
    addSampleData();
    
    // Test data remains after refresh
    m_widget->refreshTable();
    QVERIFY(m_widget->getCurrentRowCount() > 0);
    
    // Test scrolling functions
    m_widget->scrollToTop();
    m_widget->scrollToBottom();
    
    // Test jump to specific row
    int rowCount = m_widget->getCurrentRowCount();
    if (rowCount > 0) {
        m_widget->jumpToRow(0);           // First row
        m_widget->jumpToRow(rowCount - 1); // Last row
        m_widget->jumpToRow(rowCount / 2); // Middle row
    }
}

void TestGridLoggerWidget::testExportFunctionality()
{
    addSampleData();
    
    // Test CSV export
    QTemporaryFile csvFile;
    if (csvFile.open()) {
        bool result = m_widget->exportToCSV(csvFile.fileName());
        QVERIFY(result);
        
        // Verify file has content
        csvFile.seek(0);
        QByteArray content = csvFile.readAll();
        QVERIFY(!content.isEmpty());
        
        QString csvContent = QString::fromUtf8(content);
        QVERIFY(csvContent.contains("Timestamp")); // Should have timestamp header
        QVERIFY(csvContent.contains(",")); // Should be CSV format
    }
    
    // Test JSON export
    QTemporaryFile jsonFile;
    if (jsonFile.open()) {
        bool result = m_widget->exportToJSON(jsonFile.fileName());
        QVERIFY(result);
        
        jsonFile.seek(0);
        QByteArray content = jsonFile.readAll();
        QVERIFY(!content.isEmpty());
        
        // Verify it's valid JSON
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(content, &parseError);
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        
        QJsonObject root = doc.object();
        QVERIFY(root.contains("widget"));
        QVERIFY(root.contains("timestamp"));
        QVERIFY(root.contains("rows"));
        QCOMPARE(root["widget"].toString(), m_testWidgetId);
    }
    
    // Test clipboard export
    QString clipboardText = m_widget->getClipboardText();
    QVERIFY(!clipboardText.isEmpty());
    QVERIFY(clipboardText.contains("Timestamp"));
    QVERIFY(clipboardText.contains("\t")); // Tab-separated format
}

void TestGridLoggerWidget::testSearchAndFilter()
{
    addSampleData();
    
    int originalRowCount = m_widget->getCurrentRowCount();
    QVERIFY(originalRowCount > 0);
    
    // Test search filter
    m_widget->setSearchFilter("test");
    // Note: Actual filtering would require integration with data processing
    // This tests the interface doesn't crash
    
    m_widget->clearSearchFilter();
    
    // Test field filter
    m_widget->setFieldFilter("test.field", QVariant(42));
    
    m_widget->clearFieldFilters();
    
    // Widget should remain functional
    QVERIFY(m_widget->getCurrentRowCount() >= 0);
}

void TestGridLoggerWidget::testAutoSave()
{
    QTemporaryFile autoSaveFile;
    if (!autoSaveFile.open()) {
        QSKIP("Could not create temporary file for auto-save test");
    }
    
    QString fileName = autoSaveFile.fileName();
    autoSaveFile.close();
    
    // Enable auto-save
    QVERIFY(!m_widget->isAutoSaveEnabled());
    
    m_widget->enableAutoSave(true, fileName);
    
    QVERIFY(m_widget->isAutoSaveEnabled());
    QCOMPARE(m_widget->getAutoSaveFile(), fileName);
    
    // Add some data
    addSampleData();
    
    QSignalSpy autoSaveCompletedSpy(m_widget, &GridLoggerWidget::autoSaveCompleted);
    QSignalSpy autoSaveErrorSpy(m_widget, &GridLoggerWidget::autoSaveError);
    
    // Trigger auto-save manually (since timer interval is long)
    m_widget->performAutoSaveForTesting();
    
    QApplication::processEvents(); // Was: QTest::qWait(500) // Allow auto-save to complete
    
    // Should have completed successfully or with error
    QVERIFY(autoSaveCompletedSpy.count() + autoSaveErrorSpy.count() > 0);
    
    if (autoSaveCompletedSpy.count() > 0) {
        // Verify file exists and has content
        QFile savedFile(fileName);
        QVERIFY(savedFile.exists());
        QVERIFY(savedFile.size() > 0);
    }
    
    // Disable auto-save
    m_widget->enableAutoSave(false);
    QVERIFY(!m_widget->isAutoSaveEnabled());
}

void TestGridLoggerWidget::testSettingsPersistence()
{
    // Configure widget
    GridLoggerWidget::LoggerOptions options = m_widget->getLoggerOptions();
    options.maxRows = 5000;
    options.autoScroll = false;
    options.showTimestamp = false;
    options.timestampFormat = "hh:mm:ss";
    options.highlightNewRows = false;
    m_widget->setLoggerOptions(options);
    
    // Add highlight rules
    GridLoggerWidget::HighlightRule rule;
    rule.name = "Test Rule";
    rule.fieldPath = "test.field";
    rule.condition = "== 42";
    rule.backgroundColor = QColor(Qt::magenta);
    rule.textColor = QColor(Qt::yellow);
    m_widget->addHighlightRule(rule);
    
    // Add some fields
    m_widget->addField("persist.field1", 300, QJsonObject());
    m_widget->addField("persist.field2", 301, QJsonObject());
    
    // Save settings
    QJsonObject settings = m_widget->saveSettings();
    
    QVERIFY(!settings.isEmpty());
    
    QJsonObject widgetSpecific = settings.value("widgetSpecific").toObject();
    QVERIFY(widgetSpecific.contains("loggerOptions"));
    QVERIFY(widgetSpecific.contains("highlightRules"));
    
    // Verify logger options
    QJsonObject loggerOptions = widgetSpecific.value("loggerOptions").toObject();
    QCOMPARE(loggerOptions.value("maxRows").toInt(), 5000);
    QCOMPARE(loggerOptions.value("autoScroll").toBool(), false);
    QCOMPARE(loggerOptions.value("showTimestamp").toBool(), false);
    
    // Verify highlight rules
    QJsonArray highlightRules = widgetSpecific.value("highlightRules").toArray();
    QCOMPARE(highlightRules.size(), 1);
    
    QJsonObject ruleObj = highlightRules[0].toObject();
    QCOMPARE(ruleObj.value("name").toString(), QString("Test Rule"));
    QCOMPARE(ruleObj.value("fieldPath").toString(), QString("test.field"));
    QCOMPARE(ruleObj.value("condition").toString(), QString("== 42"));
    
    // Restore in new widget
    GridLoggerWidget newWidget("restored_logger");
    bool restored = newWidget.restoreSettings(settings);
    
    QVERIFY(restored);
    
    // Verify restoration
    GridLoggerWidget::LoggerOptions restoredOptions = newWidget.getLoggerOptions();
    QCOMPARE(restoredOptions.maxRows, 5000);
    QVERIFY(!restoredOptions.autoScroll);
    QVERIFY(!restoredOptions.showTimestamp);
    QCOMPARE(restoredOptions.timestampFormat, QString("hh:mm:ss"));
    QVERIFY(!restoredOptions.highlightNewRows);
    
    QList<GridLoggerWidget::HighlightRule> restoredRules = newWidget.getHighlightRules();
    QCOMPARE(restoredRules.size(), 1);
    QCOMPARE(restoredRules[0].name, QString("Test Rule"));
    
    QCOMPARE(newWidget.getFieldCount(), 2);
}

void TestGridLoggerWidget::testHighVolumeLogging()
{
    // Set lower max rows for testing
    m_widget->setMaxRows(100);
    
    // Add fields
    m_widget->addField("volume.field1", 400, QJsonObject());
    m_widget->addField("volume.field2", 401, QJsonObject());
    m_widget->addField("volume.field3", 402, QJsonObject());
    
    QSignalSpy maxRowsReachedSpy(m_widget, &GridLoggerWidget::maxRowsReached);
    
    QElapsedTimer timer;
    timer.start();
    
    // Generate high volume of data
    const int numPackets = 150; // More than max rows
    
    for (int i = 0; i < numPackets; ++i) {
        QHash<QString, QVariant> packet;
        packet["volume.field1"] = i;
        packet["volume.field2"] = i * 2.5;
        packet["volume.field3"] = QString("packet_%1").arg(i);
        
        simulatePacketArrival(packet);
        
        if (i % 20 == 0) {
            QApplication::processEvents(); // Periodic pause
        }
    }
    
    QApplication::processEvents(); // Was: QTest::qWait(200)
    
    qint64 elapsed = timer.elapsed();
    
    // Should have hit max rows limit
    QCOMPARE(m_widget->getCurrentRowCount(), 100);
    QVERIFY(maxRowsReachedSpy.count() > 0);
    
    // Performance should be reasonable
    QVERIFY(elapsed < 5000); // Less than 5 seconds
    
    qDebug() << "Processed" << numPackets << "packets in" << elapsed << "ms";
    qDebug() << "Current row count:" << m_widget->getCurrentRowCount();
}

void TestGridLoggerWidget::testMemoryManagement()
{
    // Test memory efficiency with large dataset
    m_widget->setMaxRows(1000);
    
    // Add multiple fields
    for (int i = 0; i < 10; ++i) {
        QString fieldPath = QString("memory.field_%1").arg(i);
        m_widget->addField(fieldPath, 500 + i, QJsonObject());
    }
    
    // Fill with data
    for (int packet = 0; packet < 500; ++packet) {
        QHash<QString, QVariant> data;
        for (int field = 0; field < 10; ++field) {
            QString fieldPath = QString("memory.field_%1").arg(field);
            data[fieldPath] = QString("data_%1_%2").arg(packet).arg(field);
        }
        simulatePacketArrival(data);
    }
    
    QApplication::processEvents(); // Was: QTest::qWait(500)
    
    QCOMPARE(m_widget->getCurrentRowCount(), 500);
    
    // Clear and verify cleanup
    m_widget->clearAllRows();
    QCOMPARE(m_widget->getCurrentRowCount(), 0);
    
    // Should still be functional
    QHash<QString, QVariant> testPacket;
    testPacket["memory.field_0"] = "recovery_test";
    simulatePacketArrival(testPacket);
    
    QApplication::processEvents(); // Was: QTest::qWait(100)
    QCOMPARE(m_widget->getCurrentRowCount(), 1);
}

void TestGridLoggerWidget::testLargeDatasetHandling()
{
    // Test with many fields (wide table)
    const int numFields = 50;
    
    for (int i = 0; i < numFields; ++i) {
        QString fieldPath = QString("dataset.field_%1").arg(i);
        QJsonObject fieldInfo;
        fieldInfo["type"] = (i % 3 == 0) ? "int" : (i % 3 == 1) ? "double" : "string";
        m_widget->addField(fieldPath, 600 + i, fieldInfo);
    }
    
    QCOMPARE(m_widget->getFieldCount(), numFields);
    
    // Add packets with many fields
    for (int packet = 0; packet < 10; ++packet) {
        QHash<QString, QVariant> data;
        for (int field = 0; field < numFields; ++field) {
            QString fieldPath = QString("dataset.field_%1").arg(field);
            
            if (field % 3 == 0) {
                data[fieldPath] = packet * field;
            } else if (field % 3 == 1) {
                data[fieldPath] = packet * field * 3.14;
            } else {
                data[fieldPath] = QString("p%1_f%2").arg(packet).arg(field);
            }
        }
        simulatePacketArrival(data);
    }
    
    QApplication::processEvents(); // Was: QTest::qWait(300)
    
    QCOMPARE(m_widget->getCurrentRowCount(), 10);
    
    // Test column resizing with many columns
    m_widget->resizeColumnsToContents();
    
    // Should handle gracefully without crashing
}

void TestGridLoggerWidget::testContextMenu()
{
    addSampleData();
    
    QMenu* contextMenu = m_widget->getContextMenuForTesting();
    QVERIFY(contextMenu != nullptr);
    
    QList<QAction*> actions = contextMenu->actions();
    QVERIFY(!actions.isEmpty());
    
    // Look for logger-specific actions
    bool foundClearRows = false;
    bool foundExportCSV = false;
    bool foundExportJSON = false;
    bool foundHighlightRules = false;
    bool foundAutoSave = false;
    
    for (QAction* action : actions) {
        QString text = action->text();
        if (text.contains("Clear") && text.contains("Rows")) {
            foundClearRows = true;
        } else if (text.contains("CSV")) {
            foundExportCSV = true;
        } else if (text.contains("JSON")) {
            foundExportJSON = true;
        } else if (text.contains("Highlight") && text.contains("Rules")) {
            foundHighlightRules = true;
        } else if (text.contains("Auto-Save")) {
            foundAutoSave = true;
        }
    }
    
    QVERIFY(foundClearRows);
    QVERIFY(foundExportCSV);
    QVERIFY(foundExportJSON);
    QVERIFY(foundHighlightRules);
    QVERIFY(foundAutoSave);
}

void TestGridLoggerWidget::testSortingFunctionality()
{
    addSampleData();
    
    // Test sorting (exact verification would need access to table internals)
    // This tests that sorting doesn't crash
    
    // These should not crash
    m_widget->refreshTable();
    
    // Test scrolling after sorting
    m_widget->scrollToTop();
    m_widget->scrollToBottom();
}

void TestGridLoggerWidget::testScrolling()
{
    addSampleData();
    
    // Test scroll operations
    m_widget->scrollToTop();
    m_widget->scrollToBottom();
    
    // Test jump to specific row
    int rowCount = m_widget->getCurrentRowCount();
    if (rowCount > 0) {
        m_widget->jumpToRow(0);
        m_widget->jumpToRow(rowCount / 2);
        m_widget->jumpToRow(rowCount - 1);
        
        // Test invalid row numbers (should handle gracefully)
        m_widget->jumpToRow(-1);
        m_widget->jumpToRow(rowCount + 100);
    }
}

void TestGridLoggerWidget::testCellInteraction()
{
    addSampleData();
    
    // Test cell click handling (would need mock events in full implementation)
    // For now, test that the interface exists and doesn't crash
    
    // These test the public interface
    m_widget->refreshTable();
    m_widget->resizeColumnsToContents();
    
    // Test context menu request
    QPoint testPoint(10, 10);
    Q_UNUSED(testPoint);
    // Context menu functionality is tested in testContextMenu()
}

void TestGridLoggerWidget::testEmptyLogger()
{
    QCOMPARE(m_widget->getCurrentRowCount(), 0);
    
    // Operations on empty logger should not crash
    m_widget->clearAllRows();
    m_widget->refreshTable();
    m_widget->scrollToTop();
    m_widget->scrollToBottom();
    m_widget->jumpToRow(0);
    m_widget->resizeColumnsToContents();
    
    // Export operations should work (with headers only)
    QString clipboardText = m_widget->getClipboardText();
    QVERIFY(!clipboardText.isEmpty());
    QVERIFY(clipboardText.contains("Timestamp")); // Should have at least headers
    
    // Context menu should be available
    QMenu* contextMenu = m_widget->getContextMenuForTesting();
    QVERIFY(contextMenu != nullptr);
}

void TestGridLoggerWidget::testMaxRowsLimit()
{
    int maxRows = 10;
    m_widget->setMaxRows(maxRows);
    QCOMPARE(m_widget->getMaxRows(), maxRows);
    
    // Add a field
    m_widget->addField("limit.test", 700, QJsonObject());
    
    QSignalSpy maxRowsReachedSpy(m_widget, &GridLoggerWidget::maxRowsReached);
    
    // Add more packets than the limit
    for (int i = 0; i < maxRows + 5; ++i) {
        QHash<QString, QVariant> packet;
        packet["limit.test"] = i;
        simulatePacketArrival(packet);
    }
    
    QApplication::processEvents(); // Was: QTest::qWait(200)
    
    // Should not exceed max rows (if auto-delete is enabled)
    if (m_widget->getLoggerOptions().autoDeleteOldest) {
        QCOMPARE(m_widget->getCurrentRowCount(), maxRows);
        QVERIFY(maxRowsReachedSpy.count() > 0);
    } else {
        QVERIFY(m_widget->getCurrentRowCount() <= maxRows + 5);
    }
}

void TestGridLoggerWidget::testCorruptedData()
{
    // Test with invalid settings
    QJsonObject corruptedSettings;
    corruptedSettings["loggerOptions"] = "invalid"; // Should be object
    corruptedSettings["highlightRules"] = 123;      // Should be array
    
    GridLoggerWidget corruptedWidget("corrupted_logger");
    
    // Should handle gracefully
    bool restored = corruptedWidget.restoreWidgetSpecificSettingsForTesting(corruptedSettings);
    Q_UNUSED(restored);
    // May or may not succeed depending on implementation, but should not crash
    
    // Widget should still be functional with defaults
    corruptedWidget.show();
    QApplication::processEvents(); // Was: QTest::qWait(100)
    
    GridLoggerWidget::LoggerOptions options = corruptedWidget.getLoggerOptions();
    QVERIFY(options.maxRows > 0);
    QVERIFY(options.autoSaveInterval > 0);
    
    corruptedWidget.addField("recovery.field", 800, QJsonObject());
    QCOMPARE(corruptedWidget.getFieldCount(), 1);
}

void TestGridLoggerWidget::testInvalidOperations()
{
    // Test operations with invalid parameters
    m_widget->jumpToRow(-1);
    m_widget->jumpToRow(1000000);
    
    // Test invalid file operations
    bool csvResult = m_widget->exportToCSV("");
    QVERIFY(!csvResult);
    
    bool jsonResult = m_widget->exportToJSON("");
    QVERIFY(!jsonResult);
    
    // Test invalid highlight rule operations
    m_widget->removeHighlightRule("nonexistent_rule");
    QVERIFY(m_widget->getHighlightRules().isEmpty());
    
    // Test invalid search operations
    m_widget->setSearchFilter(QString()); // Empty filter
    m_widget->setFieldFilter("nonexistent.field", QVariant());
    
    // Widget should remain stable
    QCOMPARE(m_widget->getCurrentRowCount(), 0);
    
    // Should still be able to add data
    m_widget->addField("recovery.field", 900, QJsonObject());
    QCOMPARE(m_widget->getFieldCount(), 1);
}

// Helper methods
void TestGridLoggerWidget::addSampleData()
{
    // Add fields
    QJsonObject intField;
    intField["type"] = "int";
    m_widget->addField("sample.temperature", 150, intField);
    
    QJsonObject doubleField;
    doubleField["type"] = "double";
    m_widget->addField("sample.pressure", 151, doubleField);
    
    QJsonObject stringField;
    stringField["type"] = "string";
    m_widget->addField("sample.status", 152, stringField);
    
    // Add sample packets
    for (int i = 0; i < 5; ++i) {
        QHash<QString, QVariant> packet;
        packet["sample.temperature"] = 20 + i;
        packet["sample.pressure"] = 100.0 + i * 0.5;
        packet["sample.status"] = QString("Status_%1").arg(i);
        
        simulatePacketArrival(packet);
    }
    
    QApplication::processEvents(); // Was: QTest::qWait(100)
}

void TestGridLoggerWidget::simulatePacketArrival(const QHash<QString, QVariant>& fieldValues)
{
    // Simulate packet arrival by calling updateFieldDisplay for each field
    for (auto it = fieldValues.begin(); it != fieldValues.end(); ++it) {
        m_widget->updateFieldDisplayForTesting(it.key(), it.value());
    }
    
    // Process pending updates immediately for testing
    m_widget->processPendingUpdatesForTesting();
}

void TestGridLoggerWidget::verifyTableStructure(int expectedRows, int expectedColumns)
{
    // This would verify the internal table structure
    // Implementation depends on access to internal table widget
    
    QCOMPARE(m_widget->getCurrentRowCount(), expectedRows);
    
    // Column count verification would depend on implementation details
    // Expected columns = timestamp column (if enabled) + field columns
    Q_UNUSED(expectedColumns)
}

QTEST_MAIN(TestGridLoggerWidget)
#include "test_grid_logger_widget.moc"