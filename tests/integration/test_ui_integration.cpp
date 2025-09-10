#include <QtTest/QtTest>
#include <QApplication>
#include "mainwindow.h"
#include "src/ui/managers/tab_manager.h"
#include "src/ui/managers/settings_manager.h"

class TestUIIntegration : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testMainWindowInitialization();
    void testTabManagerIntegration();
    void testSettingsIntegration();

private:
    MainWindow *m_mainWindow;
};

void TestUIIntegration::initTestCase()
{
    if (!QApplication::instance()) {
        int argc = 1;
        char *argv[] = {"test"};
        new QApplication(argc, argv);
    }
}

void TestUIIntegration::cleanupTestCase()
{
    if (m_mainWindow) {
        delete m_mainWindow;
    }
}

void TestUIIntegration::testMainWindowInitialization()
{
    m_mainWindow = new MainWindow();
    QVERIFY(m_mainWindow != nullptr);
    
    // Test that managers are created
    QVERIFY(m_mainWindow->getTabManager() != nullptr);
    QVERIFY(m_mainWindow->getSettingsManager() != nullptr);
}

void TestUIIntegration::testTabManagerIntegration()
{
    if (!m_mainWindow) {
        m_mainWindow = new MainWindow();
    }
    
    TabManager *tabManager = m_mainWindow->getTabManager();
    QVERIFY(tabManager != nullptr);
    
    // Test tab creation through main window
    QString tabId = tabManager->createTab("Integration Test Tab");
    QVERIFY(!tabId.isEmpty());
    QCOMPARE(tabManager->getTabCount(), 2); // Should have Main + new tab
}

void TestUIIntegration::testSettingsIntegration()
{
    if (!m_mainWindow) {
        m_mainWindow = new MainWindow();
    }
    
    SettingsManager *settingsManager = m_mainWindow->getSettingsManager();
    QVERIFY(settingsManager != nullptr);
    
    // Test settings persistence
    settingsManager->setSetting("integration_test", "test_value");
    QVariant value = settingsManager->getSetting("integration_test");
    QCOMPARE(value.toString(), QString("test_value"));
}

QTEST_MAIN(TestUIIntegration)
#include "test_ui_integration.moc"