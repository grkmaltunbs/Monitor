#include <QtTest/QTest>
#include <QtCore/QCoreApplication>
#include "../src/logging/logger.h"
#include "../src/logging/memory_sink.h"
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    auto logger = Monitor::Logging::Logger::instance();
    logger->setAsynchronous(false);
    logger->clearSinks();
    
    auto sink = std::make_shared<Monitor::Logging::MemorySink>(100);
    logger->addSink(sink);
    
    std::cout << "Sink count before logging: " << sink->getEntryCount() << std::endl;
    
    logger->info("Test", "Test message");
    
    std::cout << "Sink count after logging: " << sink->getEntryCount() << std::endl;
    
    if (sink->getEntryCount() == 0) {
        std::cout << "ERROR: Sink didn't receive the message!" << std::endl;
        
        // Try with synchronous logging explicitly
        logger->setGlobalLogLevel(Monitor::Logging::LogLevel::Trace);
        logger->info("Test2", "Test message 2");
        std::cout << "After setting trace level: " << sink->getEntryCount() << std::endl;
    }
    
    return 0;
}