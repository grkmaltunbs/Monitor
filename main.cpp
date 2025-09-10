#include "mainwindow.h"
#include "src/core/application.h"

#include <QApplication>
#include <QtCore/QLoggingCategory>

Q_LOGGING_CATEGORY(mainApp, "Monitor.Main")

int main(int argc, char *argv[])
{
    QApplication qtApp(argc, argv);
    
    // Set application properties
    QCoreApplication::setApplicationName("Monitor");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCoreApplication::setOrganizationName("Monitor Development");
    
    // Initialize the Monitor application
    Monitor::Core::Application* app = Monitor::Core::Application::instance();
    
    if (!app->initialize()) {
        qCritical(mainApp) << "Failed to initialize Monitor Application";
        return 1;
    }
    
    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();
    
    qCInfo(mainApp) << "Monitor Application started successfully";
    
    // Run the Qt application
    int result = qtApp.exec();
    
    // Shutdown the Monitor application
    app->shutdown();
    
    return result;
}
